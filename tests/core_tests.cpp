#include "algorithms/WeightedGraph.h"
#include "algorithms/RouteAlgorithms.h"
#include "algorithms/FloydWarshall.h"
#include "algorithms/DisjointSetUnion.h"
#include "algorithms/InfrastructureAlgorithms.h"
#include "algorithms/KnapsackAlgorithms.h"
#include "algorithms/SortingAlgorithms.h"
#include "algorithms/NumberTheoryAlgorithms.h"
#include "algorithms/DeliveryTourPlanner.h"
#include "core/models/CityLocation.h"
#include "core/models/EmergencyModels.h"
#include "core/models/Road.h"
#include "services/CityRepository.h"
#include "services/EmergencyDispatchSimulation.h"
#include "services/DeliveryScenario.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
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
    const auto beforeCandidate = graph.revision();
    require(graph.addConstructionCandidate({8, 10, 20, 400.0}),
            "valid construction candidate should be added");
    require(graph.revision() > beforeCandidate,
            "construction candidate changes should update graph revision");
    require(!graph.addConstructionCandidate({9, 10, 20, -1.0}),
            "negative construction cost should be rejected");
    require(graph.moveLocation(10, 15.0, 25.0), "location should move");
    require(graph.revision() > beforeMove, "moving should update the graph revision");
    require(graph.removeLocation(10), "location should be removed");
    require(graph.roadCount() == 0, "connected roads should be removed safely");
    require(graph.constructionCandidateCount() == 0,
            "connected construction candidates should be removed safely");

    smartcity::algorithms::DisjointSetUnion disjointSets(4);
    require(disjointSets.unite(0, 1), "different disjoint sets should unite");
    require(disjointSets.unite(2, 3), "second pair should unite");
    require(disjointSets.unite(1, 2), "the two trees should unite");
    require(disjointSets.find(0) == disjointSets.find(3),
            "path-compressed find should report the merged root");
    require(!disjointSets.unite(0, 3), "uniting an existing component should do nothing");
    require(disjointSets.componentCount() == 1, "DSU should track component count");

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

    const auto allDistances = smartcity::algorithms::FloydWarshall::compute(
        routes, RouteMetric::Distance);
    const auto allTimes = smartcity::algorithms::FloydWarshall::compute(
        routes, RouteMetric::TravelTime);
    const auto reconstructedDistance = allDistances.reconstructPath(0, 2);
    require(reconstructedDistance.reachable && reconstructedDistance.totalWeight == 2.0,
            "Floyd-Warshall should find the known distance answer");
    require(reconstructedDistance.locationIds == distanceRoute.locationIds
                && reconstructedDistance.roadIds == distanceRoute.roadIds,
            "Floyd-Warshall should reconstruct the same known path as Dijkstra");
    require(allTimes.reconstructPath(0, 2).totalWeight == timeRoute.totalWeight,
            "Floyd-Warshall travel time should match Dijkstra");
    require(allDistances.reconstructPath(1, 1).reachable
                && allDistances.reconstructPath(1, 1).totalWeight == 0.0,
            "all-pairs diagonal entries should be zero");
    require(allDistances.weights.at(1).at(1) == 0.0
                && std::isinf(allDistances.weights.at(0).at(3)),
            "matrix should use zero diagonals and infinity for unreachable pairs");
    require(!allDistances.isReachable(0, 3) && !allDistances.isReachable(0, 5),
            "closed-only and disconnected pairs should remain unreachable");
    require(!allDistances.isReachable(4, 2),
            "all-pairs routing should respect one-way roads");

    auto cacheRevisionGraph = routes;
    const auto cachedRoutes = smartcity::algorithms::FloydWarshall::compute(
        cacheRevisionGraph, RouteMetric::Distance);
    auto cacheChangingRoad = *cacheRevisionGraph.road(1);
    cacheChangingRoad.distance += 1.0;
    require(cacheRevisionGraph.updateRoad(cacheChangingRoad)
                && cachedRoutes.graphRevision != cacheRevisionGraph.revision(),
            "a graph edit should make a revision-keyed route cache stale");

    for (const auto metric : {RouteMetric::Distance, RouteMetric::TravelTime}) {
        const auto allPairs = smartcity::algorithms::FloydWarshall::compute(routes, metric);
        for (int source = 0; source <= 5; ++source) {
            for (int destination = 0; destination <= 5; ++destination) {
                const auto singleSource = RouteAlgorithms::dijkstra(routes, source, destination, metric);
                const auto allPairsPath = allPairs.reconstructPath(source, destination);
                const bool dijkstraReachable = singleSource.status == RouteStatus::Found
                    || singleSource.status == RouteStatus::Trivial;
                require(allPairsPath.reachable == dijkstraReachable,
                        "Floyd-Warshall and Dijkstra reachability should agree");
                if (dijkstraReachable) {
                    require(std::abs(allPairsPath.totalWeight - singleSource.totalWeight) < 1e-9,
                            "Floyd-Warshall and Dijkstra weights should agree");
                }
            }
        }
    }

    WeightedGraph overflowGraph;
    require(overflowGraph.addLocation({0, "A", 0.0, 0.0, LocationCategory::Intersection}),
            "overflow test location A should be added");
    require(overflowGraph.addLocation({1, "B", 1.0, 0.0, LocationCategory::Intersection}),
            "overflow test location B should be added");
    require(overflowGraph.addLocation({2, "C", 2.0, 0.0, LocationCategory::Intersection}),
            "overflow test location C should be added");
    const double hugeWeight = std::numeric_limits<double>::max() * 0.75;
    require(overflowGraph.addRoad(
                {0, 0, 1, hugeWeight, hugeWeight, 0.0, RoadDirection::OneWay, false}),
            "first huge road should be added");
    require(overflowGraph.addRoad(
                {1, 1, 2, hugeWeight, hugeWeight, 0.0, RoadDirection::OneWay, false}),
            "second huge road should be added");
    require(!smartcity::algorithms::FloydWarshall::compute(overflowGraph, RouteMetric::Distance)
                 .isReachable(0, 2),
            "overflowing path sums should remain infinity rather than wrap or corrupt the matrix");

    WeightedGraph infrastructure;
    for (int id = 0; id < 4; ++id) {
        require(infrastructure.addLocation({id, "Site " + std::to_string(id), id * 10.0, 0.0,
                                            LocationCategory::Intersection}),
                "infrastructure test location should be added");
    }
    require(infrastructure.addConstructionCandidate({0, 0, 1, 1.0}),
            "first explicit candidate should be added");
    require(infrastructure.addConstructionCandidate({1, 1, 2, 2.0}),
            "second explicit candidate should be added");
    require(infrastructure.addConstructionCandidate({2, 0, 2, 5.0}),
            "cycle candidate should be added");
    require(infrastructure.addRoad({40, 2, 3, 1.0, 1.0, 0.01,
                                    RoadDirection::TwoWay, false}),
            "travel road should coexist with infrastructure candidates");
    const auto primForest = smartcity::algorithms::InfrastructureAlgorithms::prim(infrastructure);
    const auto kruskalForest = smartcity::algorithms::InfrastructureAlgorithms::kruskal(infrastructure);
    require(primForest.totalCost == 3.0 && kruskalForest.totalCost == 3.0,
            "Prim and Kruskal should produce the known minimum cost");
    require(primForest.candidateIds.size() == 2 && kruskalForest.candidateIds.size() == 2,
            "both forests should select two construction candidates");
    require(primForest.componentCount == 2 && kruskalForest.componentCount == 2,
            "the isolated site should produce a two-component forest");
    require(std::find(primForest.candidateIds.begin(), primForest.candidateIds.end(), 40)
                == primForest.candidateIds.end(),
            "travel road IDs must never become construction candidates");

    WeightedGraph ties;
    for (int id = 0; id < 3; ++id)
        require(ties.addLocation({id, "Tie", id * 5.0, 0.0, LocationCategory::Intersection}),
                "tie location should be added");
    require(ties.addConstructionCandidate({0, 0, 1, 1.0})
                && ties.addConstructionCandidate({1, 1, 2, 1.0})
                && ties.addConstructionCandidate({2, 0, 2, 1.0}),
            "equal-cost candidates should be accepted");
    const auto primTies = smartcity::algorithms::InfrastructureAlgorithms::prim(ties);
    const auto kruskalTies = smartcity::algorithms::InfrastructureAlgorithms::kruskal(ties);
    require(primTies.componentCount == 1 && kruskalTies.componentCount == 1
                && std::abs(primTies.totalCost - kruskalTies.totalCost) < 1e-9,
            "Prim and Kruskal totals should agree even when tied edge choices may differ");

    WeightedGraph dispatchGraph;
    require(dispatchGraph.addLocation({0, "Station West", 0.0, 0.0,
                                       LocationCategory::AmbulanceStation})
                && dispatchGraph.addLocation({1, "Patient", 10.0, 0.0,
                                              LocationCategory::ResidentialArea})
                && dispatchGraph.addLocation({2, "Hospital", 20.0, 0.0,
                                              LocationCategory::Hospital})
                && dispatchGraph.addLocation({3, "Station East", 10.0, 10.0,
                                              LocationCategory::AmbulanceStation})
                && dispatchGraph.addLocation({4, "Isolated", 100.0, 100.0,
                                              LocationCategory::ResidentialArea})
                && dispatchGraph.addLocation({5, "Second Hospital", 20.0, 10.0,
                                              LocationCategory::Hospital}),
            "dispatch locations should be added");
    require(dispatchGraph.addRoad({0, 0, 1, 1.0, 5.0, 0.0,
                                   RoadDirection::TwoWay, false})
                && dispatchGraph.addRoad({1, 3, 1, 1.0, 5.0, 0.0,
                                          RoadDirection::TwoWay, false})
                && dispatchGraph.addRoad({2, 1, 2, 1.0, 4.0, 0.0,
                                          RoadDirection::TwoWay, false})
                && dispatchGraph.addRoad({3, 1, 5, 1.0, 4.0, 0.0,
                                          RoadDirection::TwoWay, false}),
            "dispatch travel roads should be added");
    smartcity::services::EmergencyDispatchSimulation dispatch;
    require(dispatch.addAmbulance({0, 0, true, -1}, dispatchGraph)
                && dispatch.addAmbulance({1, 3, true, -1}, dispatchGraph),
            "sample ambulances should be added");
    require(dispatch.addHospital({2, 1, {smartcity::core::EmergencyService::Trauma}},
                                 dispatchGraph)
                && dispatch.addHospital({5, 1, {smartcity::core::EmergencyService::Trauma}},
                                        dispatchGraph),
            "sample hospital resources should be added");
    smartcity::core::EmergencyRequest lowPriority;
    lowPriority.id = 0;
    lowPriority.locationId = 1;
    lowPriority.priority = smartcity::core::SimulatedPriority::Low;
    lowPriority.requiredService = smartcity::core::EmergencyService::Trauma;
    require(dispatch.addRequest(lowPriority, dispatchGraph),
            "low-priority request should be queued");
    dispatch.advanceClock(1.0);
    smartcity::core::EmergencyRequest criticalRequest = lowPriority;
    criticalRequest.id = 1;
    criticalRequest.priority = smartcity::core::SimulatedPriority::Critical;
    criticalRequest.creationTimeMinutes = 1.0;
    require(dispatch.addRequest(criticalRequest, dispatchGraph),
            "critical request should be queued");
    const auto assigned = dispatch.dispatchNext(dispatchGraph);
    require(assigned.code == smartcity::services::DispatchResultCode::Assigned
                && assigned.requestId == 1,
            "priority should precede creation time in the dispatch queue");
    require(assigned.ambulanceId == 0 && assigned.hospitalLocationId == 2,
            "equal travel times should use the lower ambulance and hospital IDs");
    require(dispatch.hospitals().at(2).availableBeds == 0
                && !dispatch.ambulances().at(0).available,
            "dispatch should reserve the bed and ambulance atomically");
    require(dispatch.removeHospital(5), "unused tie-test hospital should be removable");
    const auto blockedByBed = dispatch.dispatchNext(dispatchGraph);
    require(blockedByBed.code == smartcity::services::DispatchResultCode::NoSuitableHospital
                && dispatch.requests().at(0).status == smartcity::core::RequestStatus::Queued,
            "request should remain queued when no suitable bed is available");
    const auto patientArrival = dispatch.advanceOneNode(dispatchGraph, 1);
    require(patientArrival.code == smartcity::services::SimulationStepCode::PatientReached
                && dispatch.requests().at(1).responseTimeMinutes == 5.0,
            "first journey leg should record response time");
    const auto hospitalArrival = dispatch.advanceOneNode(dispatchGraph, 1);
    require(hospitalArrival.code == smartcity::services::SimulationStepCode::HospitalReached
                && dispatch.requests().at(1).transportTimeMinutes == 4.0
                && dispatch.ambulances().at(0).available,
            "second journey leg should complete and release the ambulance");
    smartcity::core::EmergencyRequest unreachableRequest = lowPriority;
    unreachableRequest.id = 2;
    unreachableRequest.locationId = 4;
    unreachableRequest.priority = smartcity::core::SimulatedPriority::Critical;
    unreachableRequest.creationTimeMinutes = dispatch.clockMinutes();
    require(dispatch.addRequest(unreachableRequest, dispatchGraph),
            "unreachable request should still enter the queue");
    require(dispatch.dispatchNext(dispatchGraph).code
                == smartcity::services::DispatchResultCode::PatientUnreachable
                && dispatch.requests().at(2).status == smartcity::core::RequestStatus::Queued,
            "an unreachable patient should remain queued without a reservation");

    smartcity::services::EmergencyDispatchSimulation noAmbulanceDispatch;
    smartcity::core::EmergencyRequest noAmbulanceRequest;
    noAmbulanceRequest.id = 0;
    noAmbulanceRequest.locationId = 1;
    require(noAmbulanceDispatch.addRequest(noAmbulanceRequest, dispatchGraph)
                && noAmbulanceDispatch.dispatchNext(dispatchGraph).code
                    == smartcity::services::DispatchResultCode::NoAvailableAmbulance,
            "a request should remain queued when no ambulance exists");

    smartcity::services::EmergencyDispatchSimulation cancellationDispatch;
    require(cancellationDispatch.addAmbulance({0, 0, true, -1}, dispatchGraph)
                && cancellationDispatch.addHospital(
                    {2, 1, {smartcity::core::EmergencyService::General}}, dispatchGraph)
                && cancellationDispatch.addRequest(noAmbulanceRequest, dispatchGraph)
                && cancellationDispatch.dispatchNext(dispatchGraph).code
                    == smartcity::services::DispatchResultCode::Assigned,
            "cancellation scenario should reserve resources");
    require(cancellationDispatch.cancelRequest(0)
                && cancellationDispatch.ambulances().at(0).available
                && cancellationDispatch.hospitals().at(2).availableBeds == 1,
            "active cancellation should release ambulance and bed reservations");

    smartcity::services::EmergencyDispatchSimulation roadChangeDispatch;
    require(roadChangeDispatch.addAmbulance({0, 0, true, -1}, dispatchGraph)
                && roadChangeDispatch.addHospital(
                    {2, 1, {smartcity::core::EmergencyService::General}}, dispatchGraph),
            "road-change resources should be added");
    smartcity::core::EmergencyRequest roadChangeRequest;
    roadChangeRequest.id = 0;
    roadChangeRequest.locationId = 1;
    roadChangeRequest.requiredService = smartcity::core::EmergencyService::General;
    require(roadChangeDispatch.addRequest(roadChangeRequest, dispatchGraph)
                && roadChangeDispatch.dispatchNext(dispatchGraph).code
                    == smartcity::services::DispatchResultCode::Assigned,
            "road-change request should initially dispatch");
    auto closedApproach = *dispatchGraph.road(0);
    closedApproach.closed = true;
    require(dispatchGraph.updateRoad(closedApproach), "approach road should close");
    const auto rerouteFailure = roadChangeDispatch.advanceOneNode(dispatchGraph, 0);
    require(rerouteFailure.code == smartcity::services::SimulationStepCode::Requeued
                && roadChangeDispatch.requests().at(0).status
                    == smartcity::core::RequestStatus::Queued
                && roadChangeDispatch.ambulances().at(0).available
                && roadChangeDispatch.hospitals().at(2).availableBeds == 1,
            "an impossible node-level reroute should safely release resources and requeue");
    require(roadChangeDispatch.cancelRequest(0)
                && roadChangeDispatch.requests().at(0).status
                    == smartcity::core::RequestStatus::Cancelled,
            "queued simulated requests should support cancellation");

    using smartcity::core::DeliveryPackage;
    using smartcity::core::PackagePriority;
    const std::vector<DeliveryPackage> knapsackPackages{
        {0, 0, 10, 60.0, PackagePriority::Medium},
        {1, 1, 20, 100.0, PackagePriority::Low},
        {2, 2, 30, 120.0, PackagePriority::High}};
    const auto zeroOne = smartcity::algorithms::KnapsackAlgorithms::solveZeroOne(
        knapsackPackages, 50);
    require(zeroOne.status == smartcity::algorithms::KnapsackStatus::Solved
                && zeroOne.selectedPackageIds == std::vector<int>({1, 2})
                && zeroOne.unselectedPackageIds == std::vector<int>({0})
                && zeroOne.totalWeight == 50
                && std::abs(zeroOne.totalValue - 220.0) < 1e-9
                && zeroOne.remainingCapacity == 0,
            "0/1 Knapsack should reconstruct the known optimal package set");
    require(zeroOne.totalValue > 160.0,
            "0/1 dynamic programming should beat greedy ratio selection on the classic counterexample");
    double bruteKnapsackValue = 0.0;
    for (std::size_t mask = 0; mask < (std::size_t{1} << knapsackPackages.size()); ++mask) {
        int weight = 0; double value = 0.0;
        for (std::size_t index = 0; index < knapsackPackages.size(); ++index) {
            if ((mask & (std::size_t{1} << index)) == 0) continue;
            weight += knapsackPackages[index].weight; value += knapsackPackages[index].value;
        }
        if (weight <= 50) bruteKnapsackValue = std::max(bruteKnapsackValue, value);
    }
    require(std::abs(zeroOne.totalValue - bruteKnapsackValue) < 1e-9,
            "0/1 Knapsack should agree with brute-force subsets on a tiny input");
    require(zeroOne.rowCount == 4 && zeroOne.columnCount == 51
                && std::abs(zeroOne.tableValue(3, 50) - 220.0) < 1e-9,
            "small 0/1 inputs should expose a correct DP visualization table");
    const auto fractional = smartcity::algorithms::KnapsackAlgorithms::solveFractional(
        knapsackPackages, 50);
    require(fractional.validInput && std::abs(fractional.totalValue - 240.0) < 1e-9
                && fractional.selections.size() == 3
                && std::abs(fractional.selections.back().fraction - 2.0 / 3.0) < 1e-9,
            "Fractional Knapsack should independently produce the known divisible-cargo answer");
    auto invalidPackages = knapsackPackages;
    invalidPackages.front().weight = 0;
    require(smartcity::algorithms::KnapsackAlgorithms::solveZeroOne(invalidPackages, 50).status
                == smartcity::algorithms::KnapsackStatus::InvalidInput,
            "zero package weight should be rejected");
    require(smartcity::algorithms::KnapsackAlgorithms::solveZeroOne(
                knapsackPackages, 1'000'000).status
                == smartcity::algorithms::KnapsackStatus::TableTooLarge,
            "oversized DP tables should be rejected before allocation");

    smartcity::services::DeliveryScenario delivery;
    require(delivery.setDepotLocation(0, routes)
                && delivery.setVehicleCapacity(50)
                && delivery.addPackage(knapsackPackages.at(0), routes),
            "delivery scenario should accept valid graph references and positive capacity");
    require(!delivery.addPackage({3, 99, 1, 1.0, PackagePriority::Low}, routes),
            "delivery scenario should reject a missing destination");
    require(delivery.referencesLocation(0),
            "delivery scenario should report depot and package location references");

    const std::vector<double> sortingInput{5.0, -1.0, 5.0, 0.0, 3.5, -7.0, 3.5};
    auto ascendingReference = sortingInput;
    std::sort(ascendingReference.begin(), ascendingReference.end());
    auto descendingReference = sortingInput;
    std::sort(descendingReference.begin(), descendingReference.end(), std::greater<double>());
    using smartcity::algorithms::SortAlgorithm;
    using smartcity::algorithms::SortOrder;
    for (const auto algorithm : {SortAlgorithm::Bubble, SortAlgorithm::Selection,
                                 SortAlgorithm::Insertion, SortAlgorithm::Merge,
                                 SortAlgorithm::Quick, SortAlgorithm::Heap}) {
        const auto ascending = smartcity::algorithms::SortingAlgorithms::sort(
            sortingInput, algorithm, SortOrder::Ascending);
        const auto descending = smartcity::algorithms::SortingAlgorithms::sort(
            sortingInput, algorithm, SortOrder::Descending);
        require(ascending.validInput && ascending.values == ascendingReference,
                "manual ascending sort should match std::sort reference output");
        require(descending.validInput && descending.values == descendingReference,
                "manual descending sort should match std::sort reference output");
        auto replayed = sortingInput;
        for (const auto& event : ascending.events) {
            if (event.type == smartcity::algorithms::AlgorithmEventType::ArraySwapped)
                std::swap(replayed.at(static_cast<std::size_t>(event.indexA)),
                          replayed.at(static_cast<std::size_t>(event.indexB)));
            else if (event.type == smartcity::algorithms::AlgorithmEventType::ArrayWritten)
                replayed.at(static_cast<std::size_t>(event.indexA)) = event.value;
        }
        require(replayed == ascendingReference,
                "uncapped sorting events should replay to the computed result");
    }
    const auto alreadySortedBubble = smartcity::algorithms::SortingAlgorithms::sort(
        ascendingReference, SortAlgorithm::Bubble, SortOrder::Ascending);
    require(alreadySortedBubble.counters.comparisons == ascendingReference.size() - 1
                && alreadySortedBubble.counters.swaps == 0,
            "Bubble Sort should stop after one unchanged pass");
    const auto foundSearch = smartcity::algorithms::SortingAlgorithms::binarySearch(
        ascendingReference, 3.5, SortOrder::Ascending);
    require(foundSearch.validInput && foundSearch.found
                && ascendingReference.at(static_cast<std::size_t>(foundSearch.index)) == 3.5,
            "Binary Search should find a duplicate target in ascending data");
    const auto descendingSearch = smartcity::algorithms::SortingAlgorithms::binarySearch(
        descendingReference, -7.0, SortOrder::Descending);
    require(descendingSearch.validInput && descendingSearch.found,
            "Binary Search should support descending data");
    require(!smartcity::algorithms::SortingAlgorithms::binarySearch(
                sortingInput, 5.0, SortOrder::Ascending).validInput,
            "Binary Search should reject unsorted input");
    std::vector<double> largeReverse(500);
    for (std::size_t index = 0; index < largeReverse.size(); ++index)
        largeReverse[index] = static_cast<double>(largeReverse.size() - index);
    auto largeReference = largeReverse;
    std::sort(largeReference.begin(), largeReference.end());
    const auto cappedEvents = smartcity::algorithms::SortingAlgorithms::sort(
        largeReverse, SortAlgorithm::Bubble, SortOrder::Ascending);
    require(cappedEvents.values == largeReference
                && cappedEvents.eventLimitReached
                && cappedEvents.events.size()
                    == smartcity::algorithms::SortingAlgorithms::MaximumStoredEvents,
            "large sorts should complete correctly while capping stored animation events");
    const auto mergeProfile = smartcity::algorithms::SortingAlgorithms::profile(SortAlgorithm::Merge);
    const auto quickProfile = smartcity::algorithms::SortingAlgorithms::profile(SortAlgorithm::Quick);
    require(smartcity::algorithms::SortingAlgorithms::profile(SortAlgorithm::Bubble).stable
                && !smartcity::algorithms::SortingAlgorithms::profile(SortAlgorithm::Selection).stable
                && smartcity::algorithms::SortingAlgorithms::profile(SortAlgorithm::Insertion).stable
                && mergeProfile.stable && mergeProfile.divideAndConquer
                && !quickProfile.stable && quickProfile.divideAndConquer
                && !smartcity::algorithms::SortingAlgorithms::profile(SortAlgorithm::Heap).stable,
            "algorithm profiles should distinguish stability and divide-and-conquer sorts");

    std::vector<smartcity::algorithms::EuclidStep> euclidSteps;
    require(smartcity::algorithms::NumberTheoryAlgorithms::gcd(1071, 462, &euclidSteps) == 21
                && euclidSteps.size() == 3
                && euclidSteps.front().remainder == 147
                && euclidSteps.back().remainder == 0,
            "Euclid's algorithm should produce the known GCD and division steps");
    const auto maintenance = smartcity::algorithms::NumberTheoryAlgorithms::maintenanceCycles(
        {6, 8, 12});
    require(maintenance.validInput && !maintenance.overflow
                && maintenance.greatestCommonDivisor == 2
                && maintenance.nextCoincidence == 24
                && maintenance.gcdSteps.size() == 2
                && !maintenance.gcdSteps.front().euclidSteps.empty(),
            "maintenance cycles should produce the known combined GCD and LCM");
    require(!smartcity::algorithms::NumberTheoryAlgorithms::maintenanceCycles({5, 0, 10}).validInput,
            "zero maintenance intervals should be rejected");
    const auto maintenanceOverflow =
        smartcity::algorithms::NumberTheoryAlgorithms::maintenanceCycles(
            {std::numeric_limits<std::uint64_t>::max(), 2});
    require(maintenanceOverflow.validInput && maintenanceOverflow.overflow
                && maintenanceOverflow.steps.back().overflow,
            "LCM should detect 64-bit overflow before multiplication");
    const auto safeLargeMaintenance =
        smartcity::algorithms::NumberTheoryAlgorithms::maintenanceCycles(
            {std::numeric_limits<std::uint64_t>::max() - 1, 2});
    require(!safeLargeMaintenance.overflow
                && safeLargeMaintenance.nextCoincidence
                    == std::numeric_limits<std::uint64_t>::max() - 1,
            "LCM division before multiplication should preserve a large representable result");

    WeightedGraph tourGraph;
    for (int id = 0; id < 4; ++id)
        require(tourGraph.addLocation({id, "Tour " + std::to_string(id), id * 10.0, 0.0,
                                       LocationCategory::Intersection}),
                "tour test location should be added");
    require(tourGraph.addRoad({0, 0, 1, 1.0, 1.0, 0.0, RoadDirection::OneWay, false})
                && tourGraph.addRoad({1, 1, 2, 1.0, 1.0, 0.0, RoadDirection::OneWay, false})
                && tourGraph.addRoad({2, 2, 3, 1.0, 1.0, 0.0, RoadDirection::OneWay, false})
                && tourGraph.addRoad({3, 3, 0, 1.0, 1.0, 0.0, RoadDirection::OneWay, false}),
            "directed tour cycle should be added");
    const auto tour = smartcity::algorithms::DeliveryTourPlanner::compute(tourGraph, 0, {1, 2, 3});
    require(tour.status == smartcity::algorithms::DeliveryTourStatus::Solved
                && std::abs(tour.totalTravelTime - 4.0) < 1e-9
                && tour.serviceOrder == std::vector<int>({0, 1, 2, 3, 0})
                && tour.expandedLocationIds == std::vector<int>({0, 1, 2, 3, 0})
                && tour.roadIds == std::vector<int>({0, 1, 2, 3}),
            "Held-Karp should reconstruct and expand the known directed minimum tour");
    std::vector<int> bruteStops{1, 2, 3};
    double bruteTourCost = std::numeric_limits<double>::infinity();
    do {
        int current = 0; double cost = 0.0; bool reachable = true;
        for (int stop : bruteStops) {
            const auto leg = RouteAlgorithms::dijkstra(
                tourGraph, current, stop, RouteMetric::TravelTime, false);
            if (leg.status != RouteStatus::Found && leg.status != RouteStatus::Trivial) {
                reachable = false; break;
            }
            cost += leg.totalWeight; current = stop;
        }
        const auto returnLeg = RouteAlgorithms::dijkstra(
            tourGraph, current, 0, RouteMetric::TravelTime, false);
        if (returnLeg.status != RouteStatus::Found && returnLeg.status != RouteStatus::Trivial)
            reachable = false;
        else cost += returnLeg.totalWeight;
        if (reachable) bruteTourCost = std::min(bruteTourCost, cost);
    } while (std::next_permutation(bruteStops.begin(), bruteStops.end()));
    require(std::abs(tour.totalTravelTime - bruteTourCost) < 1e-9,
            "Held-Karp should agree with brute-force permutations on a tiny input");
    require(smartcity::algorithms::DeliveryTourPlanner::compute(tourGraph, 0, {}).status
                == smartcity::algorithms::DeliveryTourStatus::Trivial,
            "a depot with no selected stops should produce a trivial zero-cost tour");
    auto closedReturn = *tourGraph.road(3); closedReturn.closed = true;
    require(tourGraph.updateRoad(closedReturn), "tour return road should close");
    const auto unreachableTour = smartcity::algorithms::DeliveryTourPlanner::compute(
        tourGraph, 0, {1, 2, 3});
    require(unreachableTour.status == smartcity::algorithms::DeliveryTourStatus::Unreachable
                && !unreachableTour.unreachableStopIds.empty(),
            "tour planning should respect road direction and closures");
    WeightedGraph transitGraph;
    for (int id = 0; id < 3; ++id)
        require(transitGraph.addLocation({id, "Transit", id * 5.0, 0.0,
                                          LocationCategory::Intersection}),
                "transit-semantics location should be added");
    require(transitGraph.addRoad({0, 0, 1, 1.0, 1.0, 0.0, RoadDirection::OneWay, false})
                && transitGraph.addRoad({1, 1, 2, 1.0, 1.0, 0.0,
                                         RoadDirection::OneWay, false})
                && transitGraph.addRoad({2, 2, 1, 1.0, 1.0, 0.0,
                                         RoadDirection::OneWay, false})
                && transitGraph.addRoad({3, 1, 0, 10.0, 10.0, 0.0,
                                         RoadDirection::OneWay, false})
                && transitGraph.addRoad({4, 2, 0, 100.0, 100.0, 0.0,
                                         RoadDirection::OneWay, false}),
            "transit-semantics roads should be added");
    const auto transitTour = smartcity::algorithms::DeliveryTourPlanner::compute(
        transitGraph, 0, {1, 2});
    require(transitTour.status == smartcity::algorithms::DeliveryTourStatus::Solved
                && transitTour.serviceOrder == std::vector<int>({0, 2, 1, 0})
                && transitTour.expandedLocationIds == std::vector<int>({0, 1, 2, 1, 0}),
            "passing through a selected stop should not service it before its scheduled DP arrival");
    WeightedGraph oversizedTourGraph;
    std::vector<int> thirteenStops;
    require(oversizedTourGraph.addLocation({0, "Depot", 0.0, 0.0,
                                            LocationCategory::Intersection}),
            "oversized tour depot should be added");
    for (int id = 1; id <= 13; ++id) {
        require(oversizedTourGraph.addLocation({id, "Stop", id * 1.0, 0.0,
                                                LocationCategory::ResidentialArea}),
                "oversized tour stop should be added");
        thirteenStops.push_back(id);
    }
    require(smartcity::algorithms::DeliveryTourPlanner::compute(
                oversizedTourGraph, 0, thirteenStops).status
                == smartcity::algorithms::DeliveryTourStatus::TooManyStops,
            "Held-Karp should enforce the 12-stop limit before allocating DP state");

    const smartcity::services::CityRepository repository;
    require(repository.city().vertexCount() == 6, "sample city should contain six locations");
    require(repository.city().roadCount() == 7, "sample city should contain seven roads");
    require(repository.city().constructionCandidateCount() == 8,
            "sample city should contain eight explicit construction candidates");
    require(repository.dispatchSimulation().ambulances().size() == 2
                && repository.dispatchSimulation().hospitals().size() == 1,
            "sample city should include simulated dispatch resources");
    require(repository.deliveryScenario().packages().size() == 4
                && repository.deliveryScenario().vehicleCapacity() == 7,
            "sample city should include a delivery scenario");
    require(repository.city().road(6)->closed, "sample should contain a closed road");

    std::cout << "All SmartCity core tests passed.\n";
    return 0;
}
