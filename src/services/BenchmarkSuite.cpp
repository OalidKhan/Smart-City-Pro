#include "services/BenchmarkSuite.h"

#include "algorithms/FloydWarshall.h"
#include "algorithms/InfrastructureAlgorithms.h"
#include "algorithms/RouteAlgorithms.h"
#include "algorithms/SortingAlgorithms.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <random>
#include <stdexcept>

namespace smartcity::services {
namespace {
using Clock = std::chrono::steady_clock;

template<typename Function>
double medianMilliseconds(int warmups, int repetitions, Function&& function,
                          double& checksum)
{
    for (int run = 0; run < warmups; ++run) checksum += function();
    std::vector<Clock::duration> durations;
    durations.reserve(static_cast<std::size_t>(repetitions));
    for (int run = 0; run < repetitions; ++run) {
        const auto start = Clock::now();
        const double observation = function();
        const auto finish = Clock::now();
        checksum += observation;
        durations.push_back(finish - start);
    }
    std::sort(durations.begin(), durations.end());
    const auto middle = durations.size() / 2;
    double nanoseconds = static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(durations[middle]).count());
    if (durations.size() % 2 == 0) {
        nanoseconds += static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(durations[middle - 1]).count());
        nanoseconds /= 2.0;
    }
    return nanoseconds / 1'000'000.0;
}

std::string compilerName()
{
#if defined(__clang__)
    return "Clang " __clang_version__;
#elif defined(__GNUC__)
    return "GCC " __VERSION__;
#elif defined(_MSC_VER)
    return "MSVC " + std::to_string(_MSC_VER);
#else
    return "Unknown compiler";
#endif
}

std::string buildMode()
{
#ifdef NDEBUG
    return "Release (NDEBUG)";
#else
    return "Debug";
#endif
}

std::vector<int> sortedLocationIds(const algorithms::WeightedGraph& graph)
{
    std::vector<int> ids;
    ids.reserve(graph.vertexCount());
    for (const auto& entry : graph.locations()) ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}
}

BenchmarkReport BenchmarkSuite::run(const algorithms::WeightedGraph& graph,
                                    const BenchmarkConfig& config)
{
    if (config.sortingDatasetSize == 0 || config.warmupRuns < 0 || config.measuredRuns < 1)
        throw std::invalid_argument("Invalid benchmark configuration");
    BenchmarkReport report;
    report.config = config;
    report.graphRevision = graph.revision();
    report.locationCount = graph.vertexCount();
    report.roadCount = graph.roadCount();
    report.constructionCandidateCount = graph.constructionCandidateCount();
    report.buildMode = buildMode();
    report.compiler = compilerName();

    std::mt19937 generator(config.randomSeed);
    std::uniform_int_distribution<int> distribution(-1'000'000, 1'000'000);
    std::vector<double> sortingDataset(config.sortingDatasetSize);
    for (double& value : sortingDataset) value = distribution(generator);
    auto sortingReference = sortingDataset;
    std::sort(sortingReference.begin(), sortingReference.end());

    const std::vector<algorithms::SortAlgorithm> sortingAlgorithms{
        algorithms::SortAlgorithm::Bubble, algorithms::SortAlgorithm::Selection,
        algorithms::SortAlgorithm::Insertion, algorithms::SortAlgorithm::Merge,
        algorithms::SortAlgorithm::Quick, algorithms::SortAlgorithm::Heap};
    report.sortingAgreement = true;
    for (const auto algorithm : sortingAlgorithms) {
        const auto profile = algorithms::SortingAlgorithms::profile(algorithm);
        double localChecksum = 0.0;
        const double median = medianMilliseconds(config.warmupRuns, config.measuredRuns, [&] {
            const auto result = algorithms::SortingAlgorithms::sort(
                sortingDataset, algorithm, algorithms::SortOrder::Ascending, false);
            return result.values.empty() ? 0.0 : result.values.front() + result.values.back();
        }, localChecksum);
        const auto verification = algorithms::SortingAlgorithms::sort(
            sortingDataset, algorithm, algorithms::SortOrder::Ascending, false);
        report.sortingAgreement = report.sortingAgreement
            && verification.validInput && verification.values == sortingReference;
        report.checksum += localChecksum;
        report.records.push_back({"Sorting", profile.name, config.sortingDatasetSize,
            "identical seeded copied values", median, profile.timeComplexity,
            profile.auxiliarySpace});
    }

    const auto primCheck = algorithms::InfrastructureAlgorithms::prim(graph, false);
    const auto kruskalCheck = algorithms::InfrastructureAlgorithms::kruskal(graph, false);
    report.mstAgreement = primCheck.componentCount == kruskalCheck.componentCount
        && std::abs(primCheck.totalCost - kruskalCheck.totalCost) < 1e-9;
    for (const auto algorithm : {algorithms::InfrastructureAlgorithm::Prim,
                                 algorithms::InfrastructureAlgorithm::Kruskal}) {
        double localChecksum = 0.0;
        const double median = medianMilliseconds(config.warmupRuns, config.measuredRuns, [&] {
            const auto result = algorithms::InfrastructureAlgorithms::minimumSpanningForest(
                graph, algorithm, false);
            return result.totalCost + result.componentCount;
        }, localChecksum);
        report.checksum += localChecksum;
        report.records.push_back({"Infrastructure",
            algorithm == algorithms::InfrastructureAlgorithm::Prim ? "Prim" : "Kruskal",
            graph.constructionCandidateCount(), "same construction-candidate network", median,
            algorithm == algorithms::InfrastructureAlgorithm::Prim
                ? "O((V + C) log C)" : "O(C log C)",
            "O(V + C)"});
    }

    const auto locationIds = sortedLocationIds(graph);
    const auto repeatedDijkstra = [&] {
        double sum = 0.0;
        for (int source : locationIds) for (int destination : locationIds) {
            const auto route = algorithms::RouteAlgorithms::dijkstra(
                graph, source, destination, algorithms::RouteMetric::TravelTime, false);
            if (route.status == algorithms::RouteStatus::Found
                || route.status == algorithms::RouteStatus::Trivial) sum += route.totalWeight;
        }
        return sum;
    };
    const auto floydWarshall = [&] {
        double sum = 0.0;
        const auto result = algorithms::FloydWarshall::compute(
            graph, algorithms::RouteMetric::TravelTime);
        for (const auto& row : result.weights) for (double value : row)
            if (std::isfinite(value)) sum += value;
        return sum;
    };
    const auto agreementMatrix = algorithms::FloydWarshall::compute(
        graph, algorithms::RouteMetric::TravelTime);
    report.shortestPathAgreement = true;
    for (int source : locationIds) for (int destination : locationIds) {
        const auto route = algorithms::RouteAlgorithms::dijkstra(
            graph, source, destination, algorithms::RouteMetric::TravelTime, false);
        const auto matrixPath = agreementMatrix.reconstructPath(source, destination);
        const bool routeReachable = route.status == algorithms::RouteStatus::Found
            || route.status == algorithms::RouteStatus::Trivial;
        report.shortestPathAgreement = report.shortestPathAgreement
            && routeReachable == matrixPath.reachable
            && (!routeReachable || std::abs(route.totalWeight - matrixPath.totalWeight) < 1e-9);
    }
    double timedDijkstraChecksum = 0.0;
    const double dijkstraMedian = medianMilliseconds(config.warmupRuns, config.measuredRuns,
        repeatedDijkstra, timedDijkstraChecksum);
    double timedFloydChecksum = 0.0;
    const double floydMedian = medianMilliseconds(config.warmupRuns, config.measuredRuns,
        floydWarshall, timedFloydChecksum);
    report.checksum += timedDijkstraChecksum + timedFloydChecksum;
    const std::size_t pairCount = locationIds.size() * locationIds.size();
    report.records.push_back({"All-pairs travel time", "Repeated Dijkstra", pairCount,
        "all ordered source/destination pairs", dijkstraMedian,
        "O(V(V + E) log V)", "O(V + E) per sequential run"});
    report.records.push_back({"All-pairs travel time", "Floyd-Warshall", pairCount,
        "same ordered source/destination pairs", floydMedian,
        "O(V³)", "O(V²)"});
    return report;
}

} // namespace smartcity::services
