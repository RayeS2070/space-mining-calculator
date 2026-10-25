#include "reader/reader.hpp"

#include <algorithm>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "embedded_json.hpp"

namespace json = boost::json;

namespace reader {
namespace {

using Result = std::expected<void, std::string>;

auto fail(std::string_view path, std::string_view what) -> Result {
  return std::unexpected(std::format("{}: {}", path, what));
}

// The top-level "source" member.
struct Source {
  std::string gt5u;
};

// One overload per type found in the data. All are declared before
// ObjectReader and the vector overload, which call them from templates.
auto convert(const json::value& v, std::string_view path, bool& out) -> Result;
auto convert(const json::value& v, std::string_view path, int& out) -> Result;
auto convert(const json::value& v, std::string_view path, std::int64_t& out)
    -> Result;
auto convert(const json::value& v, std::string_view path, std::string& out)
    -> Result;
auto convert(const json::value& v, std::string_view path, core::Output& out)
    -> Result;
auto convert(const json::value& v, std::string_view path, core::Asteroid& out)
    -> Result;
auto convert(const json::value& v, std::string_view path, Source& out)
    -> Result;

template <class T>
auto convert(const json::value& v, std::string_view path, std::vector<T>& out)
    -> Result {
  const json::array* array = v.if_array();
  if (!array) return fail(path, "expected an array");
  out.clear();
  out.reserve(array->size());
  for (std::size_t i = 0; i < array->size(); ++i) {
    if (auto ok = convert((*array)[i], std::format("{}[{}]", path, i),
                          out.emplace_back());
        !ok) {
      return ok;
    }
  }
  return {};
}

// Reads the members of one object into fields. Stops at the first error, so
// the reads go one after another without a check each; finish() reports it,
// or a member that nothing read: a new field in the data must not go unseen.
class ObjectReader {
 public:
  ObjectReader(const json::value& v, std::string_view path)
      : object_(v.if_object()), path_(path) {
    if (!object_) error_ = std::format("{}: expected an object", path_);
  }

  template <class T>
  auto read(std::string_view key, T& out) -> ObjectReader& {
    if (const json::value* v = member(key)) {
      convertMember(*v, key, out);
    } else if (!error_) {
      error_ = std::format("{}: no \"{}\"", path_, key);
    }
    return *this;
  }

  template <class T>
  auto readOptional(std::string_view key, std::optional<T>& out)
      -> ObjectReader& {
    out.reset();
    if (const json::value* v = member(key))
      convertMember(*v, key, out.emplace());
    return *this;
  }

  // A member that is known but not needed.
  auto skip(std::string_view key) -> ObjectReader& {
    member(key);
    return *this;
  }

  auto finish() const -> Result {
    if (error_) return std::unexpected(*error_);
    for (const json::key_value_pair& kv : *object_) {
      if (!std::ranges::contains(seen_, std::string_view(kv.key()))) {
        return fail(path_, std::format("unknown member \"{}\"",
                                       std::string_view(kv.key())));
      }
    }
    return {};
  }

 private:
  // The member, or null if it is missing or an error already happened.
  auto member(std::string_view key) -> const json::value* {
    if (error_) return nullptr;
    seen_.push_back(key);
    return object_->if_contains(key);
  }

  template <class T>
  auto convertMember(const json::value& v, std::string_view key, T& out)
      -> void {
    if (auto ok = convert(v, std::format("{}.{}", path_, key), out); !ok) {
      error_ = std::move(ok.error());
    }
  }

  const json::object* object_;
  std::string path_;
  std::vector<std::string_view> seen_;
  std::optional<std::string> error_;
};

auto convert(const json::value& v, std::string_view path, bool& out) -> Result {
  const bool* b = v.if_bool();
  if (!b) return fail(path, "expected true or false");
  out = *b;
  return {};
}

auto convert(const json::value& v, std::string_view path, std::int64_t& out)
    -> Result {
  const std::int64_t* i = v.if_int64();
  if (!i) return fail(path, "expected an integer");
  out = *i;
  return {};
}

auto convert(const json::value& v, std::string_view path, int& out) -> Result {
  std::int64_t wide = 0;
  if (auto ok = convert(v, path, wide); !ok) return ok;
  if (wide < std::numeric_limits<int>::min() ||
      wide > std::numeric_limits<int>::max()) {
    return fail(path, "integer out of range");
  }
  out = static_cast<int>(wide);
  return {};
}

auto convert(const json::value& v, std::string_view path, std::string& out)
    -> Result {
  const json::string* s = v.if_string();
  if (!s) return fail(path, "expected a string");
  out.assign(s->data(), s->size());
  return {};
}

auto convert(const json::value& v, std::string_view path, core::Output& out)
    -> Result {
  return ObjectReader(v, path)
      .read("name", out.name)
      .readOptional("prefix", out.prefix)
      .read("amount", out.amount)
      .read("chance", out.chance)
      .finish();
}

auto droneTierIndex(std::string_view name) -> std::optional<int> {
  const auto it = std::ranges::find(core::kDroneTierNames, name);
  if (it == core::kDroneTierNames.end()) return std::nullopt;
  return static_cast<int>(it - core::kDroneTierNames.begin());
}

// The ranges the model relies on.
auto check(const core::Asteroid& a, std::string_view path) -> Result {
  if (a.outputs.empty()) return fail(path, "no outputs");
  int totalChance = 0;
  for (const core::Output& o : a.outputs) {
    if (o.amount <= 0 || o.chance < 0) {
      return fail(path, std::format("output {}: bad amount or chance", o.name));
    }
    totalChance += o.chance;
  }
  if (totalChance <= 0) return fail(path, "all output chances are 0");
  if (a.minSize < 0 || a.minSize > a.maxSize) return fail(path, "bad sizes");
  if (a.minDistance < 1 || a.minDistance > a.maxDistance) {
    return fail(path, "bad distances");
  }
  if (a.minModuleTier < 1 || a.minModuleTier > 3) {
    return fail(path, "bad minModuleTier");
  }
  if (a.computation < 0 || a.duration <= 0 || a.eut <= 0 || a.weight <= 0) {
    return fail(path, "computation, duration, eut or weight out of range");
  }
  if (a.firstDroneTier > a.lastDroneTier) {
    return fail(path, "droneTiers out of order");
  }
  return {};
}

auto convert(const json::value& v, std::string_view path, core::Asteroid& out)
    -> Result {
  std::vector<std::string> droneTiers;
  if (auto ok = ObjectReader(v, path)
                    .read("name", out.name)
                    .read("outputs", out.outputs)
                    .read("minSize", out.minSize)
                    .read("maxSize", out.maxSize)
                    .read("minDistance", out.minDistance)
                    .read("maxDistance", out.maxDistance)
                    .read("computation", out.computation)
                    .read("minModuleTier", out.minModuleTier)
                    .read("duration", out.duration)
                    .read("eut", out.eut)
                    .read("scaleEUt", out.scaleEUt)
                    .read("droneTiers", droneTiers)
                    .read("weight", out.weight)
                    .readOptional("requiresMod", out.requiresMod)
                    .finish();
      !ok) {
    return ok;
  }

  const std::string tiersPath = std::format("{}.droneTiers", path);
  if (droneTiers.size() != 2) return fail(tiersPath, "expected 2 tiers");
  const auto first = droneTierIndex(droneTiers[0]);
  const auto last = droneTierIndex(droneTiers[1]);
  if (!first || !last) return fail(tiersPath, "unknown drone tier");
  out.firstDroneTier = *first;
  out.lastDroneTier = *last;
  return check(out, path);
}

auto convert(const json::value& v, std::string_view path, Source& out)
    -> Result {
  return ObjectReader(v, path).skip("file").read("gt5u", out.gt5u).finish();
}

auto toCatalog(const json::value& root)
    -> std::expected<core::Catalog, std::string> {
  core::Catalog catalog;
  Source source;
  std::vector<std::string> droneTiers;
  if (auto ok = ObjectReader(root, "$")
                    .read("source", source)
                    .read("droneTiers", droneTiers)
                    .read("asteroids", catalog.asteroids)
                    .finish();
      !ok) {
    return std::unexpected(std::move(ok.error()));
  }
  // Asteroids name their drone tiers; the model knows them by index.
  if (!std::ranges::equal(droneTiers, core::kDroneTierNames)) {
    return std::unexpected("$.droneTiers: not the tiers the model knows");
  }
  // Asteroids are looked up by name: core::findAsteroid.
  for (std::size_t i = 0; i < catalog.asteroids.size(); ++i) {
    if (core::findAsteroid(catalog.asteroids, catalog.asteroids[i].name) != i) {
      return std::unexpected(std::format("$.asteroids[{}]: name {} repeated", i,
                                         catalog.asteroids[i].name));
    }
  }
  catalog.gt5uVersion = std::move(source.gt5u);
  return catalog;
}

}  // namespace

auto parseCatalog(std::string_view text)
    -> std::expected<core::Catalog, std::string> {
  boost::system::error_code ec;
  const json::value root = json::parse(text, ec);
  if (ec) return std::unexpected(std::format("not JSON: {}", ec.message()));
  return toCatalog(root);
}

auto readCatalog(const std::filesystem::path& file)
    -> std::expected<core::Catalog, std::string> {
  std::ifstream in(file, std::ios::binary);
  if (!in)
    return std::unexpected(std::format("{}: cannot open", file.string()));
  const std::string text{std::istreambuf_iterator<char>(in),
                         std::istreambuf_iterator<char>()};
  if (in.bad()) {
    return std::unexpected(std::format("{}: cannot read", file.string()));
  }
  return parseCatalog(text).transform_error([&](const std::string& e) {
    return std::format("{}: {}", file.string(), e);
  });
}

auto embeddedCatalog() -> std::expected<core::Catalog, std::string> {
  return parseCatalog(embeddedJson()).transform_error([](const std::string& e) {
    return std::format("embedded asteroids.json: {}", e);
  });
}

}  // namespace reader
