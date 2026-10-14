#pragma once

#include <string>

namespace smartcity::core {

enum class RoadDirection { OneWay, TwoWay };

struct Road {
    int id{-1};
    int fromLocationId{-1};
    int toLocationId{-1};
    double distance{0.0};
    double travelTime{0.0};
    double constructionCost{0.0};
    RoadDirection direction{RoadDirection::TwoWay};
    bool closed{false};

    [[nodiscard]] bool hasValidWeights() const noexcept;
};

[[nodiscard]] const char* directionName(RoadDirection direction) noexcept;
[[nodiscard]] bool directionFromName(const std::string& name,
                                     RoadDirection& direction) noexcept;

} // namespace smartcity::core
