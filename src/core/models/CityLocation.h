#pragma once

#include <string>

namespace smartcity::core {

enum class LocationCategory {
    Intersection,
    Hospital,
    University,
    Market,
    ResidentialArea,
    AmbulanceStation
};

struct CityLocation {
    int id{-1};
    std::string name;
    double x{0.0};
    double y{0.0};
    LocationCategory category{LocationCategory::Intersection};

    [[nodiscard]] bool isValid() const noexcept;
};

[[nodiscard]] const char* categoryName(LocationCategory category) noexcept;
[[nodiscard]] bool categoryFromName(const std::string& name,
                                    LocationCategory& category) noexcept;

} // namespace smartcity::core

