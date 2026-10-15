#pragma once

#include "algorithms/WeightedGraph.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace smartcity::services {

struct BenchmarkConfig {
    std::size_t sortingDatasetSize{250};
    std::uint32_t randomSeed{42};
    int warmupRuns{1};
    int measuredRuns{7};
};

struct BenchmarkRecord {
    std::string category;
    std::string algorithm;
    std::size_t workloadSize{0};
    std::string workloadDescription;
    double medianMilliseconds{0.0};
    std::string timeComplexity;
    std::string auxiliarySpace;
};

struct BenchmarkReport {
    BenchmarkConfig config;
    std::uint64_t graphRevision{0};
    std::size_t locationCount{0};
    std::size_t roadCount{0};
    std::size_t constructionCandidateCount{0};
    std::string buildMode;
    std::string compiler;
    std::vector<BenchmarkRecord> records;
    bool sortingAgreement{false};
    bool mstAgreement{false};
    bool shortestPathAgreement{false};
    double checksum{0.0};
};

class BenchmarkSuite {
public:
    [[nodiscard]] static BenchmarkReport run(const algorithms::WeightedGraph& graphSnapshot,
                                             const BenchmarkConfig& config);
};

} // namespace smartcity::services
