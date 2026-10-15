#pragma once

namespace smartcity::core {

enum class PackagePriority { Low = 1, Medium = 2, High = 3 };

struct DeliveryPackage {
    int id{-1};
    int destinationLocationId{-1};
    int weight{1};
    double value{0.0};
    PackagePriority priority{PackagePriority::Medium};

    [[nodiscard]] bool hasValidMetrics() const noexcept;
};

[[nodiscard]] const char* packagePriorityName(PackagePriority priority) noexcept;
[[nodiscard]] bool packagePriorityFromName(const char* name,
                                           PackagePriority& priority) noexcept;

} // namespace smartcity::core
