#include "core/models/Road.h"

#include <cmath>

namespace smartcity::core {

bool Road::hasValidWeights() const noexcept
{
    return std::isfinite(distance) && std::isfinite(travelTime)
        && std::isfinite(constructionCost) && distance >= 0.0
        && travelTime >= 0.0 && constructionCost >= 0.0;
}

const char* directionName(RoadDirection direction) noexcept
{
    return direction == RoadDirection::OneWay ? "one-way" : "two-way";
}

bool directionFromName(const std::string& name, RoadDirection& direction) noexcept
{
    if (name == "one-way") {
        direction = RoadDirection::OneWay;
        return true;
    }
    if (name == "two-way") {
        direction = RoadDirection::TwoWay;
        return true;
    }
    return false;
}

} // namespace smartcity::core
