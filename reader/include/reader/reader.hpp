#ifndef SMC_READER_READER_HPP
#define SMC_READER_READER_HPP

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

#include "core/model.hpp"

// Reads data/asteroids.json into core::Catalog. Kept out of core: core does
// no I/O and does not see Boost.JSON.

namespace reader {

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

#endif  // SMC_READER_READER_HPP
