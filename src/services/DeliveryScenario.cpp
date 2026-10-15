#include "services/DeliveryScenario.h"

#include "algorithms/WeightedGraph.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace smartcity::services {
namespace {
bool fail(std::string* error, const std::string& message)
{
    if (error) *error = message;
    return false;
}
}

bool DeliveryScenario::setDepotLocation(int locationId, const algorithms::WeightedGraph& graph,
                                        std::string* error)
{
    if (!graph.containsVertex(locationId)) return fail(error, "The depot must be an existing city location.");
    depotLocationId_ = locationId;
    return true;
}

bool DeliveryScenario::setVehicleCapacity(int capacity, std::string* error)
{
    if (capacity <= 0) return fail(error, "Vehicle capacity must be a positive integer.");
    vehicleCapacity_ = capacity;
    return true;
}

bool DeliveryScenario::validatePackage(const core::DeliveryPackage& package,
                                       const algorithms::WeightedGraph& graph,
                                       std::string* error) const
{
    if (package.id < 0) return fail(error, "Package ID must be nonnegative.");
    if (!graph.containsVertex(package.destinationLocationId))
        return fail(error, "Package destination must be an existing city location.");
    if (!package.hasValidMetrics())
        return fail(error, "Weight must be positive and value must be finite and nonnegative.");
    const int priority = static_cast<int>(package.priority);
    if (priority < 1 || priority > 3) return fail(error, "Package priority is invalid.");
    return true;
}

bool DeliveryScenario::addPackage(const core::DeliveryPackage& package,
                                  const algorithms::WeightedGraph& graph, std::string* error)
{
    if (!validatePackage(package, graph, error)) return false;
    if (packages_.count(package.id)) return fail(error, "Package ID already exists.");
    packages_.emplace(package.id, package);
    nextPackageId_ = std::max(nextPackageId_, package.id + 1);
    return true;
}

bool DeliveryScenario::updatePackage(const core::DeliveryPackage& package,
                                     const algorithms::WeightedGraph& graph, std::string* error)
{
    if (!validatePackage(package, graph, error)) return false;
    const auto it = packages_.find(package.id);
    if (it == packages_.end()) return fail(error, "Package ID does not exist.");
    it->second = package;
    return true;
}

bool DeliveryScenario::removePackage(int packageId)
{
    return packages_.erase(packageId) != 0;
}

bool DeliveryScenario::replaceData(int depotLocationId, int vehicleCapacity,
                                   const std::vector<core::DeliveryPackage>& packages,
                                   const algorithms::WeightedGraph& graph, std::string* error)
{
    DeliveryScenario replacement;
    if (!replacement.setDepotLocation(depotLocationId, graph, error)
        || !replacement.setVehicleCapacity(vehicleCapacity, error)) return false;
    for (const auto& package : packages)
        if (!replacement.addPackage(package, graph, error)) return false;
    *this = std::move(replacement);
    return true;
}

bool DeliveryScenario::validateAgainstGraph(const algorithms::WeightedGraph& graph,
                                            std::string* error) const
{
    if (!graph.containsVertex(depotLocationId_))
        return fail(error, "The delivery depot no longer exists in the city graph.");
    for (const auto& entry : packages_)
        if (!validatePackage(entry.second, graph, error)) return false;
    return true;
}

bool DeliveryScenario::referencesLocation(int locationId) const noexcept
{
    if (depotLocationId_ == locationId) return true;
    for (const auto& entry : packages_)
        if (entry.second.destinationLocationId == locationId) return true;
    return false;
}

} // namespace smartcity::services
