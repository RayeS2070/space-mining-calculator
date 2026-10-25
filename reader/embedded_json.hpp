#ifndef SMC_READER_EMBEDDED_JSON_HPP
#define SMC_READER_EMBEDDED_JSON_HPP

#include <string_view>

// Internal to smc_reader: not for its users.

namespace reader {

// The text of data/asteroids.json; defined in a TU that cmake/embed.cmake
// generates at build time.
auto embeddedJson() -> std::string_view;

}  // namespace reader

#endif  // SMC_READER_EMBEDDED_JSON_HPP
