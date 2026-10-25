module;

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

export module reader;

export import core;

// Reads data/asteroids.json into core::Catalog. Kept out of core: core does
// no I/O and does not see Boost.JSON.

export namespace reader {

// Parses and checks the data. The error names the place, e.g.
// "$.asteroids[3].outputs[0].chance: expected an integer".
auto parseCatalog(std::string_view json)
    -> std::expected<core::Catalog, std::string>;

auto readCatalog(const std::filesystem::path& file)
    -> std::expected<core::Catalog, std::string>;

// The data/asteroids.json the program was built with: the same on every
// platform, the browser included.
auto embeddedCatalog() -> std::expected<core::Catalog, std::string>;

}  // namespace reader

namespace reader {

// The text of data/asteroids.json; defined in a TU that cmake/embed.cmake
// generates at build time.
auto embeddedJson() -> std::string_view;

}  // namespace reader
