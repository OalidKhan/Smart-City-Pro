#pragma once

#include "core/models/DeliveryPackage.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace smartcity::algorithms { class WeightedGraph; }

namespace smartcity::services {

class DeliveryScenario {
public:
    [[nodiscard]] int depotLocationId() const noexcept { return depotLocationId_; }
    [[nodiscard]] int vehicleCapacity() const noexcept { return vehicleCapacity_; }
    [[nodiscard]] int nextPackageId() const noexcept { return nextPackageId_; }
    [[nodiscard]] const std::unordered_map<int, core::DeliveryPackage>& packages() const noexcept
    { return packages_; }

    bool setDepotLocation(int locationId, const algorithms::WeightedGraph& graph,
                          std::string* error = nullptr);
    bool setVehicleCapacity(int capacity, std::string* error = nullptr);
    bool addPackage(const core::DeliveryPackage& package,
                    const algorithms::WeightedGraph& graph, std::string* error = nullptr);
    bool updatePackage(const core::DeliveryPackage& package,
                       const algorithms::WeightedGraph& graph, std::string* error = nullptr);
    bool removePackage(int packageId);
    bool replaceData(int depotLocationId, int vehicleCapacity,
                     const std::vector<core::DeliveryPackage>& packages,
                     const algorithms::WeightedGraph& graph, std::string* error = nullptr);
    [[nodiscard]] bool validateAgainstGraph(const algorithms::WeightedGraph& graph,
                                            std::string* error = nullptr) const;
    [[nodiscard]] bool referencesLocation(int locationId) const noexcept;

private:
    bool validatePackage(const core::DeliveryPackage& package,
                         const algorithms::WeightedGraph& graph, std::string* error) const;

    int depotLocationId_{-1};
    int vehicleCapacity_{10};
    int nextPackageId_{0};
    std::unordered_map<int, core::DeliveryPackage> packages_;
};

} // namespace smartcity::services
