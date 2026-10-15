#include "algorithms/KnapsackAlgorithms.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace smartcity::algorithms {
namespace {

bool validPackages(const std::vector<core::DeliveryPackage>& packages)
{
    std::unordered_set<int> ids;
    for (const auto& package : packages) {
        const int priority = static_cast<int>(package.priority);
        if (package.id < 0 || package.destinationLocationId < 0
            || !package.hasValidMetrics()
            || priority < static_cast<int>(core::PackagePriority::Low)
            || priority > static_cast<int>(core::PackagePriority::High)
            || !ids.insert(package.id).second) return false;
    }
    return true;
}

} // namespace

double KnapsackResult::tableValue(std::size_t row, std::size_t column) const
{
    if (row >= rowCount || column >= columnCount || visualizationTable.empty())
        throw std::out_of_range("Knapsack visualization cell is out of range");
    return visualizationTable[row * columnCount + column];
}

KnapsackResult KnapsackAlgorithms::solveZeroOne(
    std::vector<core::DeliveryPackage> packages, int capacity)
{
    KnapsackResult result;
    if (capacity < 0 || !validPackages(packages)) return result;
    std::sort(packages.begin(), packages.end(), [](const auto& left, const auto& right) {
        return left.id < right.id;
    });

    const std::size_t rows = packages.size() + 1;
    const std::size_t columns = static_cast<std::size_t>(capacity) + 1;
    if (columns != 0 && rows > MaximumDpCells / columns) {
        result.status = KnapsackStatus::TableTooLarge;
        return result;
    }
    const std::size_t cellCount = rows * columns;
    if (cellCount > MaximumDpCells) {
        result.status = KnapsackStatus::TableTooLarge;
        return result;
    }

    std::vector<double> table(cellCount, 0.0);
    const auto cell = [&](std::size_t row, std::size_t column) -> double& {
        return table[row * columns + column];
    };
    for (std::size_t row = 1; row < rows; ++row) {
        const auto& package = packages[row - 1];
        for (int currentCapacity = 0; currentCapacity <= capacity; ++currentCapacity) {
            double best = cell(row - 1, static_cast<std::size_t>(currentCapacity));
            if (package.weight <= currentCapacity) {
                const double included = package.value
                    + cell(row - 1, static_cast<std::size_t>(currentCapacity - package.weight));
                if (included > best) best = included;
            }
            cell(row, static_cast<std::size_t>(currentCapacity)) = best;
        }
    }

    int remaining = capacity;
    std::unordered_set<int> selected;
    constexpr double Epsilon = 1e-9;
    for (std::size_t row = packages.size(); row > 0; --row) {
        const auto& package = packages[row - 1];
        const double current = cell(row, static_cast<std::size_t>(remaining));
        const double excluded = cell(row - 1, static_cast<std::size_t>(remaining));
        if (package.weight <= remaining && current > excluded + Epsilon) {
            selected.insert(package.id);
            result.selectedPackageIds.push_back(package.id);
            result.totalWeight += package.weight;
            remaining -= package.weight;
        }
    }
    std::sort(result.selectedPackageIds.begin(), result.selectedPackageIds.end());
    for (const auto& package : packages) {
        if (selected.find(package.id) == selected.end())
            result.unselectedPackageIds.push_back(package.id);
    }
    result.remainingCapacity = capacity - result.totalWeight;
    result.totalValue = cell(packages.size(), static_cast<std::size_t>(capacity));
    result.status = KnapsackStatus::Solved;
    if (rows <= VisualizationMaximumRows && columns <= VisualizationMaximumColumns) {
        result.rowCount = rows;
        result.columnCount = columns;
        result.visualizationTable = std::move(table);
    }
    return result;
}

FractionalKnapsackResult KnapsackAlgorithms::solveFractional(
    std::vector<core::DeliveryPackage> packages, int capacity)
{
    FractionalKnapsackResult result;
    if (capacity < 0 || !validPackages(packages)) return result;
    std::sort(packages.begin(), packages.end(), [](const auto& left, const auto& right) {
        const double leftRatio = left.value / left.weight;
        const double rightRatio = right.value / right.weight;
        if (leftRatio != rightRatio) return leftRatio > rightRatio;
        if (left.priority != right.priority)
            return static_cast<int>(left.priority) > static_cast<int>(right.priority);
        return left.id < right.id;
    });

    double remaining = capacity;
    for (const auto& package : packages) {
        if (remaining <= 0.0) break;
        const double includedWeight = std::min<double>(package.weight, remaining);
        const double fraction = includedWeight / package.weight;
        result.selections.push_back(
            {package.id, fraction, includedWeight, package.value * fraction});
        result.totalWeight += includedWeight;
        result.totalValue += package.value * fraction;
        remaining -= includedWeight;
    }
    result.remainingCapacity = remaining;
    result.validInput = true;
    return result;
}

} // namespace smartcity::algorithms
