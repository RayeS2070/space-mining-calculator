#include "calculator.hpp"

#include <imgui.h>

#include <algorithm>
#include <compare>
#include <cstddef>
#include <expected>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "core/distance.hpp"
#include "core/model.hpp"
#include "report/report.hpp"

namespace {

// Table columns; the value is the ImGui column user id the sort specs carry.
enum class Column : ImGuiID {
  target,
  name,
  distance,
  drones,
  module,
  weight,
  size,
  ticks,
  eut,
  mod,  // the mod the asteroid requires
  outputs,
  count,
};

constexpr auto kColumnCount = static_cast<int>(Column::count);
constexpr ImVec4 kErrorColor{1.0f, 0.45f, 0.4f, 1.0f};

auto text(std::string_view s) -> void {
  ImGui::TextUnformatted(s.data(), s.data() + s.size());
}

auto wrappedText(std::string_view s) -> void {
  ImGui::PushTextWrapPos(0.0f);
  text(s);
  ImGui::PopTextWrapPos();
}

auto disabledText(std::string_view s) -> void {
  ImGui::PushStyleColor(ImGuiCol_Text,
                        ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  wrappedText(s);
  ImGui::PopStyleColor();
}

// A tooltip for the last item; ImGui::SetItemTooltip takes a printf format.
auto tooltip(std::string_view s) -> void {
  if (ImGui::BeginItemTooltip()) {
    // A fixed width: wrapping at the edge of a window that fits its content
    // squeezes the text into a column.
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
    text(s);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

auto errorText(std::string_view s) -> void {
  ImGui::PushStyleColor(ImGuiCol_Text, kErrorColor);
  wrappedText(s);
  ImGui::PopStyleColor();
}

auto compare(const core::Asteroid& a, const core::Asteroid& b, Column column)
    -> std::strong_ordering {
  switch (column) {
    case Column::name:
      return a.name <=> b.name;
    case Column::distance:
      return std::tie(a.minDistance, a.maxDistance) <=>
             std::tie(b.minDistance, b.maxDistance);
    case Column::drones:
      return std::tie(a.firstDroneTier, a.lastDroneTier) <=>
             std::tie(b.firstDroneTier, b.lastDroneTier);
    case Column::module:
      return a.minModuleTier <=> b.minModuleTier;
    case Column::weight:
      return a.weight <=> b.weight;
    case Column::size:
      return std::tie(a.minSize, a.maxSize) <=> std::tie(b.minSize, b.maxSize);
    case Column::ticks:
      return a.duration <=> b.duration;
    case Column::eut:
      return a.eut <=> b.eut;
    case Column::mod:
      return a.requiresMod <=> b.requiresMod;
    case Column::target:  // sorted by the caller, which knows the targets
    case Column::outputs:
    case Column::count:
      break;
  }
  return std::strong_ordering::equal;
}

}  // namespace

Calculator::Calculator(core::Catalog catalog) : catalog_(std::move(catalog)) {
  outputs_.reserve(catalog_.asteroids.size());
  for (const core::Asteroid& a : catalog_.asteroids) {
    outputs_.push_back(report::join(report::formatOutputs(a)));
  }
}

auto Calculator::draw() -> void {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::Begin("Space mining calculator", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);

  ImGui::BeginChild("side", ImVec2(ImGui::GetFontSize() * 22.0f, 0.0f),
                    ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
  drawSettings();
  drawTargets();
  drawAnswer();
  ImGui::Spacing();
  disabledText(std::format("{} asteroids from GT5U {}.",
                           catalog_.asteroids.size(), catalog_.gt5uVersion));
  ImGui::EndChild();

  ImGui::SameLine();
  ImGui::BeginChild("asteroids");
  drawAsteroids();
  ImGui::EndChild();

  ImGui::End();
}

auto Calculator::drawSettings() -> void {
  ImGui::SeparatorText("Mining module");
  for (int tier = 1; tier <= core::kModuleTierCount; ++tier) {
    if (tier > 1) ImGui::SameLine();
    if (ImGui::RadioButton(std::format("T{}", tier).c_str(), &moduleTier_,
                           tier)) {
      answerStale_ = true;
    }
  }

  ImGui::SeparatorText("Drones");
  if (ImGui::Checkbox("Rough estimate, without drones", &roughEstimate_)) {
    answerStale_ = true;
  }
  tooltip(
      "Each asteroid counts once with its base duration, as no real drone "
      "set gives: the answer can be far off.");

  ImGui::BeginDisabled(roughEstimate_);
  if (ImGui::BeginTable("drone tiers", 4)) {
    for (std::size_t tier = 0; tier < core::kDroneTierNames.size(); ++tier) {
      ImGui::TableNextColumn();
      bool on = drones_.test(tier);
      if (ImGui::Checkbox(std::string(core::kDroneTierNames[tier]).c_str(),
                          &on)) {
        drones_.set(tier, on);
        answerStale_ = true;
      }
    }
    ImGui::EndTable();
  }
  ImGui::BeginDisabled(targets_.empty());
  if (ImGui::Button("The targets' tiers")) {
    drones_ = core::commonDroneTiers(catalog_.asteroids, targets_);
    answerStale_ = true;
  }
  tooltip("Every drone tier that reaches all the targets.");
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button("No drones")) {
    drones_.reset();
    answerStale_ = true;
  }
  ImGui::EndDisabled();
}

auto Calculator::drawTargets() -> void {
  ImGui::SeparatorText("Targets");
  if (targets_.empty()) {
    disabledText("Tick the asteroids to mine in the table.");
    return;
  }
  // A copy: a click removes from targets_ while it is walked.
  for (const std::size_t i : std::vector(targets_)) {
    ImGui::PushID(static_cast<int>(i));
    if (ImGui::SmallButton("x")) toggleTarget(i);
    ImGui::PopID();
    ImGui::SameLine();
    text(catalog_.asteroids[i].name);
  }
  if (ImGui::Button("Clear targets")) {
    targets_.clear();
    answerStale_ = true;
  }
}

auto Calculator::drawAnswer() -> void {
  ImGui::SeparatorText("Best distance");
  if (answerStale_) {
    answer_ = computeAnswer();
    answerStale_ = false;
  }
  if (!answer_) {
    disabledText("Pick targets to see where to mine them.");
    return;
  }
  if (answer_->distances) {
    text(*answer_->distances);
    if (roughEstimate_) disabledText("A rough estimate: pick the drones.");
  } else {
    errorText(answer_->distances.error());
  }
  wrappedText(
      std::format("Drones for all the targets: {}", answer_->commonDrones));
}

auto Calculator::drawAsteroids() -> void {
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
  filter_.Draw("Filter: name or output");
  ImGui::SameLine();
  ImGui::Checkbox("At distance", &atDistanceOnly_);
  ImGui::SameLine();
  ImGui::BeginDisabled(!atDistanceOnly_);
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7.0f);
  if (ImGui::InputInt("##distance", &distance_)) {
    distance_ = std::max(distance_, 1);
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::Checkbox(std::format("Only for T{}", moduleTier_).c_str(),
                  &minableOnly_);
  tooltip("Hide the asteroids the module tier does not allow.");

  constexpr ImGuiTableFlags kFlags =
      ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
      ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable |
      ImGuiTableFlags_SortMulti | ImGuiTableFlags_RowBg |
      ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH |
      ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
  if (!ImGui::BeginTable("asteroid table", kColumnCount, kFlags)) return;

  const auto column = [](const char* label, Column id,
                         ImGuiTableColumnFlags flags = 0) {
    ImGui::TableSetupColumn(label, flags, 0.0f, static_cast<ImGuiID>(id));
  };
  column("Target", Column::target, ImGuiTableColumnFlags_NoHide);
  column("Asteroid", Column::name,
         ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_NoHide);
  column("Distance", Column::distance);
  column("Drones", Column::drones);
  column("Module", Column::module);
  column("Weight", Column::weight, ImGuiTableColumnFlags_PreferSortDescending);
  column("Size", Column::size, ImGuiTableColumnFlags_PreferSortDescending);
  column("Ticks", Column::ticks);
  column("EU/t", Column::eut);
  column("Requires", Column::mod);
  column("Outputs", Column::outputs,
         ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupScrollFreeze(2, 1);
  ImGui::TableHeadersRow();

  std::vector<std::size_t> rows = visibleRows();
  // A few dozen rows: sorted every frame, no cache to keep in step.
  if (const ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs()) {
    std::ranges::stable_sort(rows, [&](std::size_t x, std::size_t y) {
      for (int n = 0; n < specs->SpecsCount; ++n) {
        const ImGuiTableColumnSortSpecs& spec = specs->Specs[n];
        const auto id = static_cast<Column>(spec.ColumnUserID);
        const std::strong_ordering order =
            id == Column::target
                ? isTarget(y) <=> isTarget(x)  // targets first
                : compare(catalog_.asteroids[x], catalog_.asteroids[y], id);
        if (order != 0) {
          return spec.SortDirection == ImGuiSortDirection_Descending
                     ? order > 0
                     : order < 0;
        }
      }
      return false;
    });
  }

  const core::MinerModule miner = this->miner();
  for (const std::size_t i : rows) {
    const core::Asteroid& a = catalog_.asteroids[i];
    ImGui::TableNextRow();
    ImGui::PushID(static_cast<int>(i));

    ImGui::TableNextColumn();
    bool target = isTarget(i);
    if (ImGui::Checkbox("##target", &target)) toggleTarget(i);

    // An asteroid the module tier does not allow is greyed out.
    const auto unminable = report::whyUnminable(a, miner, std::nullopt);
    if (unminable) {
      ImGui::PushStyleColor(ImGuiCol_Text,
                            ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    }
    ImGui::TableNextColumn();
    text(a.name);
    if (unminable) tooltip(*unminable);
    ImGui::TableNextColumn();
    text(report::formatDistances(a));
    ImGui::TableNextColumn();
    text(report::formatDroneRange(a));
    ImGui::TableNextColumn();
    text(std::format("T{}", a.minModuleTier));
    ImGui::TableNextColumn();
    text(std::format("{}", a.weight));
    ImGui::TableNextColumn();
    text(std::format("{}-{}", a.minSize, a.maxSize));
    ImGui::TableNextColumn();
    text(std::format("{}", a.duration));
    ImGui::TableNextColumn();
    text(std::format("{}", a.eut));
    ImGui::TableNextColumn();
    text(a.requiresMod.value_or(""));
    ImGui::TableNextColumn();
    text(outputs_[i]);
    if (ImGui::BeginItemTooltip()) {
      for (const std::string& output : report::formatOutputs(a)) text(output);
      ImGui::EndTooltip();
    }
    if (unminable) ImGui::PopStyleColor();

    ImGui::PopID();
  }
  ImGui::EndTable();
}

auto Calculator::isTarget(std::size_t asteroid) const -> bool {
  return std::ranges::contains(targets_, asteroid);
}

auto Calculator::toggleTarget(std::size_t asteroid) -> void {
  if (const auto it = std::ranges::find(targets_, asteroid);
      it != targets_.end()) {
    targets_.erase(it);
  } else {
    targets_.push_back(asteroid);
  }
  answerStale_ = true;
}

auto Calculator::visibleRows() const -> std::vector<std::size_t> {
  const core::MinerModule miner = this->miner();
  std::vector<std::size_t> rows;
  for (std::size_t i = 0; i < catalog_.asteroids.size(); ++i) {
    const core::Asteroid& a = catalog_.asteroids[i];
    if (!filter_.PassFilter(a.name.c_str()) &&
        !filter_.PassFilter(outputs_[i].c_str())) {
      continue;
    }
    if (atDistanceOnly_ && !core::isReachable(a, distance_)) continue;
    if (minableOnly_ && !core::fitsModule(a, miner)) continue;
    rows.push_back(i);
  }
  return rows;
}

auto Calculator::miner() const -> core::MinerModule {
  return {.tier = moduleTier_};
}

auto Calculator::drones() const -> std::optional<core::DroneSet> {
  if (roughEstimate_) return std::nullopt;
  return drones_;
}

auto Calculator::computeAnswer() const -> std::optional<Answer> {
  if (targets_.empty()) return std::nullopt;
  const core::MinerModule miner = this->miner();
  const std::optional<core::DroneSet> drones = this->drones();
  const auto best =
      drones ? core::bestDistances(catalog_.asteroids, targets_, miner,
                                   {.drones = *drones})
             : core::bestDistances(catalog_.asteroids, targets_, miner);

  Answer answer;
  if (best) {
    answer.distances =
        report::join(*best | std::views::transform(report::formatRange));
  } else {
    answer.distances = std::unexpected(
        report::describe(best.error(), catalog_, targets_, miner, drones));
  }
  const core::DroneSet common =
      core::commonDroneTiers(catalog_.asteroids, targets_);
  answer.commonDrones = common.any() ? report::formatDrones(common)
                                     : "none, one drone each is needed";
  return answer;
}
