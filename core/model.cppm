module;

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module core:model;

// What the asteroids are, what the player has built and what they set. The
// mechanics that work on these are in :mechanics.

export namespace core {

constexpr std::string_view kName = "space-mining-calc";

// Drone tiers, in the order of the droneTiers list of data/asteroids.json.
constexpr std::array<std::string_view, 14> kDroneTierNames = {
    "LV", "MV",  "HV",  "EV",  "IV",  "LuV", "ZPM",
    "UV", "UHV", "UEV", "UIV", "UMV", "UXV", "MAX"};
constexpr int kDroneTierCount = static_cast<int>(kDroneTierNames.size());
using DroneSet = std::bitset<kDroneTierNames.size()>;

// Plasma tiers 1..5: helium, bismuth, radon, technetium, plutonium-241.
constexpr int kPlasmaTierCount = 5;
// Plasma per parallel, mB, indexed by plasma tier - 1.
constexpr std::array<int, kPlasmaTierCount> kPlasmaPerParallel = {825, 550, 375,
                                                                  250, 150};

// One possible result of a draw.
struct Output {
  std::string name;                   // material or item id
  std::optional<std::string> prefix;  // ore prefix; none for item outputs
  int amount = 0;                     // per draw
  int chance = 0;  // weight among the outputs, nominally out of 10000
};

// An asteroid as data/asteroids.json stores it: before the per drone tier
// expansion.
struct Asteroid {
  std::string name;
  std::vector<Output> outputs;
  int minSize = 0;
  int maxSize = 0;
  int minDistance = 0;
  int maxDistance = 0;
  int computation = 0;  // per second, per parallel
  int minModuleTier = 1;
  int duration = 0;  // ticks
  std::int64_t eut = 0;
  bool scaleEUt = false;
  int firstDroneTier = 0;  // index in kDroneTierNames
  int lastDroneTier = 0;
  int weight = 0;
  std::optional<std::string> requiresMod;
};

// All asteroids of data/asteroids.json.
struct Catalog {
  std::string gt5uVersion;  // GT5U version the data comes from
  std::vector<Asteroid> asteroids;
};

// One recipe: an asteroid with one drone tier. The fields that do not depend
// on the drone tier are copied from the asteroid, so a recipe is checked
// without a lookup.
struct Recipe {
  std::size_t asteroid = 0;  // index in Catalog::asteroids
  int droneTier = 0;
  int minSize = 0;
  int maxSize = 0;
  int duration = 0;  // ticks, before plasma and overdrive
  std::int64_t eut = 0;
  int minDistance = 0;
  int maxDistance = 0;
  int computation = 0;
  int minModuleTier = 1;
  int weight = 0;
};

// Mining module tiers 1..3.
constexpr int kModuleTierCount = 3;
// Voltage of each module tier, EU/t, indexed by tier - 1. The module has no
// energy hatch of its own: getMaxInputVoltage() is V[tTier] with tTier fixed
// per module, UV / UHV / UEV.
constexpr std::array<std::int64_t, kModuleTierCount> kModuleVoltage = {
    524'288, 2'097'152, 8'388'608};

// What the player has built: fixed while the settings are searched.
struct MinerModule {
  int tier = 1;  // 1..kModuleTierCount
};

// What the player sets on the module: the search space.
struct Settings {
  int distance = 1;
  DroneSet drones;
  int plasmaTier = 1;      // 1..kPlasmaTierCount
  double overdrive = 1.0;  // (0, 2]
};

}  // namespace core
