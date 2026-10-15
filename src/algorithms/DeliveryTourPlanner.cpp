#include "algorithms/DeliveryTourPlanner.h"

#include "algorithms/RouteAlgorithms.h"
#include "algorithms/WeightedGraph.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace smartcity::algorithms {
namespace {
struct PairPath {
    double cost{std::numeric_limits<double>::infinity()};
    std::vector<int> locations;
    std::vector<int> roads;
};

bool safeAdd(double first, double second, double& result)
{
    if (!std::isfinite(first) || !std::isfinite(second)
        || first > std::numeric_limits<double>::max() - second) return false;
    result = first + second;
    return std::isfinite(result);
}
}

DeliveryTourResult DeliveryTourPlanner::compute(const WeightedGraph& graph,
                                                int depotLocationId,
                                                const std::vector<int>& stopLocationIds)
{
    DeliveryTourResult result;
    result.graphRevision = graph.revision();
    if (!graph.containsVertex(depotLocationId)) return result;
    if (stopLocationIds.size() > MaximumDeliveryStops) {
        result.status = DeliveryTourStatus::TooManyStops;
        return result;
    }
    std::unordered_set<int> uniqueStops;
    for (const int stop : stopLocationIds) {
        if (stop == depotLocationId || !graph.containsVertex(stop)
            || !uniqueStops.insert(stop).second) return result;
    }
    if (stopLocationIds.empty()) {
        result.status = DeliveryTourStatus::Trivial;
        result.serviceOrder = {depotLocationId};
        result.expandedLocationIds = {depotLocationId};
        return result;
    }

    std::vector<int> required{depotLocationId};
    required.insert(required.end(), stopLocationIds.begin(), stopLocationIds.end());
    const std::size_t requiredCount = required.size();
    std::vector<std::vector<PairPath>> paths(requiredCount,
                                             std::vector<PairPath>(requiredCount));
    for (std::size_t from = 0; from < requiredCount; ++from) {
        paths[from][from] = {0.0, {required[from]}, {}};
        for (std::size_t to = 0; to < requiredCount; ++to) {
            if (from == to) continue;
            auto route = RouteAlgorithms::dijkstra(
                graph, required[from], required[to], RouteMetric::TravelTime);
            if (route.status == RouteStatus::Found || route.status == RouteStatus::Trivial)
                paths[from][to] = {route.totalWeight, std::move(route.locationIds),
                                   std::move(route.roadIds)};
        }
    }
    for (std::size_t stop = 1; stop < requiredCount; ++stop) {
        if (!std::isfinite(paths[0][stop].cost) || !std::isfinite(paths[stop][0].cost))
            result.unreachableStopIds.push_back(required[stop]);
    }

    const std::size_t stopCount = stopLocationIds.size();
    const std::size_t maskCount = std::size_t{1} << stopCount;
    const double infinity = std::numeric_limits<double>::infinity();
    std::vector<double> dp(maskCount * stopCount, infinity);
    std::vector<int> parent(maskCount * stopCount, -1);
    const auto index = [stopCount](std::size_t mask, std::size_t end) {
        return mask * stopCount + end;
    };
    for (std::size_t stop = 0; stop < stopCount; ++stop)
        dp[index(std::size_t{1} << stop, stop)] = paths[0][stop + 1].cost;

    constexpr double epsilon = 1e-12;
    for (std::size_t mask = 1; mask < maskCount; ++mask) {
        for (std::size_t end = 0; end < stopCount; ++end) {
            if ((mask & (std::size_t{1} << end)) == 0) continue;
            const std::size_t previousMask = mask ^ (std::size_t{1} << end);
            if (previousMask == 0) continue;
            for (std::size_t previous = 0; previous < stopCount; ++previous) {
                if ((previousMask & (std::size_t{1} << previous)) == 0) continue;
                double candidate = infinity;
                if (!safeAdd(dp[index(previousMask, previous)],
                             paths[previous + 1][end + 1].cost, candidate)) continue;
                const auto state = index(mask, end);
                if (candidate + epsilon < dp[state]
                    || (std::abs(candidate - dp[state]) <= epsilon
                        && (parent[state] < 0 || static_cast<int>(previous) < parent[state]))) {
                    dp[state] = candidate;
                    parent[state] = static_cast<int>(previous);
                }
            }
        }
    }

    const std::size_t fullMask = maskCount - 1;
    double best = infinity;
    int bestEnd = -1;
    for (std::size_t end = 0; end < stopCount; ++end) {
        double candidate = infinity;
        if (!safeAdd(dp[index(fullMask, end)], paths[end + 1][0].cost, candidate)) continue;
        if (candidate + epsilon < best
            || (std::abs(candidate - best) <= epsilon
                && (bestEnd < 0 || static_cast<int>(end) < bestEnd))) {
            best = candidate;
            bestEnd = static_cast<int>(end);
        }
    }
    if (bestEnd < 0) {
        result.status = DeliveryTourStatus::Unreachable;
        return result;
    }

    std::vector<int> reversedStops;
    std::size_t mask = fullMask;
    int end = bestEnd;
    while (end >= 0) {
        reversedStops.push_back(end);
        const int previous = parent[index(mask, static_cast<std::size_t>(end))];
        mask ^= std::size_t{1} << static_cast<std::size_t>(end);
        end = previous;
    }
    std::reverse(reversedStops.begin(), reversedStops.end());
    result.serviceOrder.push_back(depotLocationId);
    for (const int stopIndex : reversedStops)
        result.serviceOrder.push_back(stopLocationIds[static_cast<std::size_t>(stopIndex)]);
    result.serviceOrder.push_back(depotLocationId);
    result.totalTravelTime = best;

    for (std::size_t legIndex = 0; legIndex + 1 < result.serviceOrder.size(); ++legIndex) {
        const int fromId = result.serviceOrder[legIndex];
        const int toId = result.serviceOrder[legIndex + 1];
        const auto fromFound = std::find(required.begin(), required.end(), fromId);
        const auto toFound = std::find(required.begin(), required.end(), toId);
        const auto& path = paths[static_cast<std::size_t>(fromFound - required.begin())]
                                [static_cast<std::size_t>(toFound - required.begin())];
        result.legs.push_back({fromId, toId, path.cost, path.locations, path.roads});
        if (result.expandedLocationIds.empty()) result.expandedLocationIds = path.locations;
        else result.expandedLocationIds.insert(result.expandedLocationIds.end(),
                                                path.locations.begin() + 1, path.locations.end());
        result.roadIds.insert(result.roadIds.end(), path.roads.begin(), path.roads.end());
    }
    result.status = DeliveryTourStatus::Solved;
    return result;
}

} // namespace smartcity::algorithms
