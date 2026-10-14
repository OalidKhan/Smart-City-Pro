#include "algorithms/WeightedGraph.h"
#include "algorithms/RouteAlgorithms.h"
#include "core/models/CityLocation.h"
#include "core/models/Road.h"
#include "services/CityRepository.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

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

    WeightedGraph routes;
    for (int id = 0; id <= 5; ++id) {
        require(routes.addLocation({id, "Node " + std::to_string(id), id * 10.0, 0.0,
                                    LocationCategory::Intersection}),
                "known-answer location should be added");
    }
    require(routes.addRoad({0, 0, 1, 0.0, 5.0, 0.0, RoadDirection::TwoWay, false}),
            "zero-weight road should be accepted");
    require(routes.addRoad({1, 1, 2, 2.0, 2.0, 0.0, RoadDirection::TwoWay, false}),
            "second road should be added");
    require(routes.addRoad({2, 0, 2, 5.0, 1.0, 0.0, RoadDirection::TwoWay, false}),
            "direct road should be added");
    require(routes.addRoad({3, 2, 4, 1.0, 1.0, 0.0, RoadDirection::OneWay, false}),
            "one-way road should be added");
    require(routes.addRoad({4, 0, 3, 0.0, 0.0, 0.0, RoadDirection::TwoWay, true}),
            "closed shortcut should be represented");

    using smartcity::algorithms::RouteAlgorithms;
    using smartcity::algorithms::RouteMetric;
    using smartcity::algorithms::RouteStatus;
    using smartcity::algorithms::TraversalKind;
    const auto distanceRoute = RouteAlgorithms::dijkstra(routes, 0, 2, RouteMetric::Distance);
    require(distanceRoute.status == RouteStatus::Found, "distance route should be found");
    require(distanceRoute.totalWeight == 2.0, "distance route should use the zero-weight edge");
    require(distanceRoute.locationIds == std::vector<int>({0, 1, 2}),
            "distance location path should be reconstructed");
    require(distanceRoute.roadIds == std::vector<int>({0, 1}),
            "distance road path should be reconstructed");
    require(std::any_of(distanceRoute.events.begin(), distanceRoute.events.end(),
                        [](const auto& event) {
                            return event.type == smartcity::algorithms::AlgorithmEventType::FinalRouteEdge
                                && event.roadId == 0;
                        }),
            "event stream should identify final route edges");

    const auto timeRoute = RouteAlgorithms::dijkstra(routes, 0, 2, RouteMetric::TravelTime);
    require(timeRoute.totalWeight == 1.0 && timeRoute.roadIds == std::vector<int>({2}),
            "travel-time route should use the direct road");
    require(RouteAlgorithms::dijkstra(routes, 0, 3, RouteMetric::Distance).status
                == RouteStatus::Unreachable,
            "a destination connected only by a closed road should be unreachable");
    require(RouteAlgorithms::dijkstra(routes, 0, 5, RouteMetric::Distance).status
                == RouteStatus::Unreachable,
            "an isolated destination in a disconnected graph should be unreachable");
    require(RouteAlgorithms::dijkstra(routes, 4, 2, RouteMetric::Distance).status
                == RouteStatus::Unreachable,
            "one-way roads should not be traversed backwards");
    require(RouteAlgorithms::dijkstra(routes, 99, 2, RouteMetric::Distance).status
                == RouteStatus::InvalidInput,
            "unknown endpoints should be rejected");
    const auto trivial = RouteAlgorithms::dijkstra(routes, 1, 1, RouteMetric::Distance);
    require(trivial.status == RouteStatus::Trivial && trivial.totalWeight == 0.0
                && trivial.locationIds == std::vector<int>({1}),
            "start equal to destination should return a zero route");

    const auto bfs = RouteAlgorithms::traverse(routes, 0, 4, TraversalKind::BreadthFirst);
    const auto dfs = RouteAlgorithms::traverse(routes, 0, 4, TraversalKind::DepthFirst);
    require(bfs.destinationReachable && dfs.destinationReachable,
            "BFS and DFS should respect forward reachability");
    require(bfs.visitOrder == std::vector<int>({0, 1, 2, 4}),
            "BFS should visit the known graph level by level");
    require(!RouteAlgorithms::traverse(routes, 0, 3, TraversalKind::DepthFirst).destinationReachable,
            "DFS should ignore closed roads to disconnected locations");
    require(!RouteAlgorithms::traverse(routes, 4, 0, TraversalKind::BreadthFirst).destinationReachable,
            "BFS should respect one-way roads");

    const smartcity::services::CityRepository repository;
    require(repository.city().vertexCount() == 6, "sample city should contain six locations");
    require(repository.city().roadCount() == 7, "sample city should contain seven roads");
    require(repository.city().road(6)->closed, "sample should contain a closed road");

    std::cout << "All SmartCity core tests passed.\n";
    return 0;
}
