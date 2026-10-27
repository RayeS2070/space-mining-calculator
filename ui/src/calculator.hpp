#ifndef SMC_UI_CALCULATOR_HPP
#define SMC_UI_CALCULATOR_HPP

#include <imgui.h>

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <vector>

#include "core/model.hpp"

// The calculator's window: the settings and the best distance on the left,
// the asteroid table on the right. What the command line tool does through
// its options, here through widgets; the texts come from the same report
// library.
class Calculator {
 public:
  explicit Calculator(core::Catalog catalog);

  // Draws one frame over the whole viewport.
  auto draw() -> void;

 private:
  // The answer for the current targets and settings.
  struct Answer {
    // The best distances as text, or why there are none.
    std::expected<std::string, std::string> distances;
    std::string commonDrones;
  };

  auto drawSettings() -> void;
  auto drawTargets() -> void;
  auto drawAnswer() -> void;
  auto drawAsteroids() -> void;

  auto isTarget(std::size_t asteroid) const -> bool;
  auto toggleTarget(std::size_t asteroid) -> void;
  // The asteroids that pass the table filters, in catalog order.
  auto visibleRows() const -> std::vector<std::size_t>;
  auto miner() const -> core::MinerModule;
  auto drones() const -> std::optional<core::DroneSet>;
  auto computeAnswer() const -> std::optional<Answer>;

  core::Catalog catalog_;
  // Per asteroid: its outputs as one line, for the table and the filter.
  std::vector<std::string> outputs_;

  int moduleTier_ = core::kModuleTierCount;
  bool roughEstimate_ = false;  // ignore the drones, as the CLI without them
  core::DroneSet drones_;
  std::vector<std::size_t> targets_;  // in the order they were picked

  ImGuiTextFilter filter_;  // on names and outputs
  bool atDistanceOnly_ = false;
  int distance_ = 1;
  bool minableOnly_ = false;  // only what the module tier allows

  // Recomputed when an input changes, not every frame.
  std::optional<Answer> answer_;
  bool answerStale_ = true;
};

#endif  // SMC_UI_CALCULATOR_HPP
