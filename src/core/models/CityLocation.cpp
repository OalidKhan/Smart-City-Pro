#include "core/models/CityLocation.h"

#include <cmath>

namespace smartcity::core {

bool CityLocation::isValid() const noexcept
{
    return id >= 0 && !name.empty() && std::isfinite(x) && std::isfinite(y);
}

const char* categoryName(LocationCategory category) noexcept
{
    switch (category) {
    case LocationCategory::Intersection: return "intersection";
    case LocationCategory::Hospital: return "hospital";
    case LocationCategory::University: return "university";
    case LocationCategory::Market: return "market";
    case LocationCategory::ResidentialArea: return "residential area";
    case LocationCategory::AmbulanceStation: return "ambulance station";
    }
    return "intersection";
}

bool categoryFromName(const std::string& name, LocationCategory& category) noexcept
{
    if (name == "intersection") category = LocationCategory::Intersection;
    else if (name == "hospital") category = LocationCategory::Hospital;
    else if (name == "university") category = LocationCategory::University;
    else if (name == "market") category = LocationCategory::Market;
    else if (name == "residential area") category = LocationCategory::ResidentialArea;
    else if (name == "ambulance station") category = LocationCategory::AmbulanceStation;
    else return false;
    return true;
}

} // namespace smartcity::core

