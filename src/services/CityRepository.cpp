#include "services/CityRepository.h"

#include "core/models/CityLocation.h"
#include "core/models/ConstructionCandidate.h"
#include "core/models/Road.h"

namespace smartcity::services {

CityRepository::CityRepository()
{
    using core::LocationCategory;
    using core::RoadDirection;

    city_.addLocation({0, "Civic Square", 160.0, 130.0, LocationCategory::Intersection});
    city_.addLocation({1, "Northside Hospital", 475.0, 105.0, LocationCategory::Hospital});
    city_.addLocation({2, "Riverside University", 390.0, 335.0, LocationCategory::University});
    city_.addLocation({3, "Green Market", 105.0, 360.0, LocationCategory::Market});
    city_.addLocation({4, "Lakeview Homes", 650.0, 300.0, LocationCategory::ResidentialArea});
    city_.addLocation({5, "Central Ambulance", 660.0, 120.0, LocationCategory::AmbulanceStation});

    city_.addRoad({0, 0, 1, 4.2, 7.5, 125000.0, RoadDirection::TwoWay, false});
    city_.addRoad({1, 0, 2, 3.8, 6.8, 98000.0, RoadDirection::TwoWay, false});
    city_.addRoad({2, 0, 3, 3.1, 5.4, 87000.0, RoadDirection::OneWay, false});
    city_.addRoad({3, 1, 2, 2.6, 4.9, 72000.0, RoadDirection::TwoWay, false});
    city_.addRoad({4, 1, 5, 2.1, 3.6, 64000.0, RoadDirection::TwoWay, false});
    city_.addRoad({5, 2, 4, 3.0, 5.2, 110000.0, RoadDirection::OneWay, false});
    city_.addRoad({6, 4, 5, 2.4, 4.0, 76000.0, RoadDirection::TwoWay, true});

    // Infrastructure candidates are deliberately explicit and undirected. They are
    // not inferred from the directed/open/closed travel-road network above.
    city_.addConstructionCandidate({0, 0, 1, 130000.0});
    city_.addConstructionCandidate({1, 0, 2, 95000.0});
    city_.addConstructionCandidate({2, 0, 3, 82000.0});
    city_.addConstructionCandidate({3, 1, 2, 70000.0});
    city_.addConstructionCandidate({4, 1, 5, 60000.0});
    city_.addConstructionCandidate({5, 2, 4, 105000.0});
    city_.addConstructionCandidate({6, 2, 3, 68000.0});
    city_.addConstructionCandidate({7, 4, 5, 74000.0});
}

algorithms::WeightedGraph& CityRepository::city() noexcept { return city_; }
const algorithms::WeightedGraph& CityRepository::city() const noexcept { return city_; }

} // namespace smartcity::services
