#include "core/models/DeliveryPackage.h"

#include <cmath>
#include <cstring>

namespace smartcity::core {

bool DeliveryPackage::hasValidMetrics() const noexcept
{
    return weight > 0 && std::isfinite(value) && value >= 0.0;
}

const char* packagePriorityName(PackagePriority priority) noexcept
{
    switch (priority) {
    case PackagePriority::Low: return "low";
    case PackagePriority::Medium: return "medium";
    case PackagePriority::High: return "high";
    }
    return "medium";
}

bool packagePriorityFromName(const char* name, PackagePriority& priority) noexcept
{
    if (!name) return false;
    if (std::strcmp(name, "low") == 0 || std::strcmp(name, "Low") == 0)
        priority = PackagePriority::Low;
    else if (std::strcmp(name, "medium") == 0 || std::strcmp(name, "Medium") == 0)
        priority = PackagePriority::Medium;
    else if (std::strcmp(name, "high") == 0 || std::strcmp(name, "High") == 0)
        priority = PackagePriority::High;
    else return false;
    return true;
}

} // namespace smartcity::core
