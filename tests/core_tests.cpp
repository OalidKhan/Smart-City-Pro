#include "algorithms/WeightedGraph.h"
#include "core/models/CityLocation.h"
#include "core/models/Road.h"
#include "services/CityRepository.h"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
} // namespace

int main()
{
    using smartcity::algorithms::WeightedGraph;
    using smartcity::core::LocationCategory;
    using smartcity::core::RoadDirection;

    WeightedGraph graph;
    require(graph.addLocation({10, "Alpha", 10.0, 20.0, LocationCategory::Intersection}),
            "first location should be added");
    require(graph.addLocation({20, "Beta", 30.0, 40.0, LocationCategory::Hospital}),
            "second location should be added");
    require(!graph.addLocation({10, "Duplicate", 0.0, 0.0, LocationCategory::Market}),
            "duplicate location id should be rejected");
    require(graph.addRoad({4, 10, 20, 2.5, 5.0, 1000.0, RoadDirection::OneWay, false}),
            "valid directed road should be added");
    require(!graph.addRoad({5, 10, 20, -1.0, 5.0, 1000.0, RoadDirection::TwoWay, false}),
            "negative distance should be rejected");
    require(graph.neighbors(10).size() == 1, "one-way road should create a forward edge");
    require(graph.neighbors(20).empty(), "one-way road should not create a reverse edge");
    auto editedRoad = *graph.road(4);
    editedRoad.direction = RoadDirection::TwoWay;
    editedRoad.closed = true;
    const auto beforeRoadEdit = graph.revision();
    require(graph.updateRoad(editedRoad), "road should be editable");
    require(graph.neighbors(20).size() == 1, "two-way road should create a reverse edge");
    require(graph.neighbors(20).front().closed, "closed status should reach adjacency entries");
    require(graph.revision() > beforeRoadEdit, "road edits should update the graph revision");

    const auto beforeMove = graph.revision();
    require(graph.moveLocation(10, 15.0, 25.0), "location should move");
    require(graph.revision() > beforeMove, "moving should update the graph revision");
    require(graph.removeLocation(10), "location should be removed");
    require(graph.roadCount() == 0, "connected roads should be removed safely");

    const smartcity::services::CityRepository repository;
    require(repository.city().vertexCount() == 6, "sample city should contain six locations");
    require(repository.city().roadCount() == 7, "sample city should contain seven roads");
    require(repository.city().road(6)->closed, "sample should contain a closed road");

    std::cout << "All SmartCity core tests passed.\n";
    return 0;
}
