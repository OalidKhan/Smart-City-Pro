#pragma once

#include "core/models/DeliveryPackage.h"

#include <cstddef>
#include <vector>

namespace smartcity::algorithms {

enum class KnapsackStatus { Solved, InvalidInput, TableTooLarge };

struct KnapsackResult {
    KnapsackStatus status{KnapsackStatus::InvalidInput};
    std::vector<int> selectedPackageIds;
    std::vector<int> unselectedPackageIds;
    int totalWeight{0};
    double totalValue{0.0};
    int remainingCapacity{0};
    std::size_t rowCount{0};
    std::size_t columnCount{0};
    std::vector<double> visualizationTable;

    [[nodiscard]] double tableValue(std::size_t row, std::size_t column) const;
};

struct FractionalSelection {
    int packageId{-1};
    double fraction{0.0};
    double includedWeight{0.0};
    double includedValue{0.0};
};

struct FractionalKnapsackResult {
    bool validInput{false};
    std::vector<FractionalSelection> selections;
    double totalWeight{0.0};
    double totalValue{0.0};
    double remainingCapacity{0.0};
};

class KnapsackAlgorithms {
public:
    static constexpr std::size_t MaximumDpCells = 2'000'000;
    static constexpr std::size_t VisualizationMaximumRows = 21;
    static constexpr std::size_t VisualizationMaximumColumns = 101;

    [[nodiscard]] static KnapsackResult solveZeroOne(
        std::vector<core::DeliveryPackage> packages, int capacity);
    [[nodiscard]] static FractionalKnapsackResult solveFractional(
        std::vector<core::DeliveryPackage> packages, int capacity);
};

} // namespace smartcity::algorithms
