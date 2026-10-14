#include "core/models/EmergencyModels.h"

#include <algorithm>

namespace smartcity::core {

bool HospitalResource::supports(EmergencyService service) const noexcept
{
    return std::find(supportedServices.begin(), supportedServices.end(), service)
        != supportedServices.end();
}

const char* emergencyServiceName(EmergencyService service) noexcept
{
    switch (service) {
    case EmergencyService::General: return "general";
    case EmergencyService::Trauma: return "trauma";
    case EmergencyService::Cardiac: return "cardiac";
    case EmergencyService::Pediatric: return "pediatric";
    }
    return "general";
}

bool emergencyServiceFromName(const std::string& name, EmergencyService& service) noexcept
{
    if (name == "general") service = EmergencyService::General;
    else if (name == "trauma") service = EmergencyService::Trauma;
    else if (name == "cardiac") service = EmergencyService::Cardiac;
    else if (name == "pediatric") service = EmergencyService::Pediatric;
    else return false;
    return true;
}

const char* priorityName(SimulatedPriority priority) noexcept
{
    switch (priority) {
    case SimulatedPriority::Low: return "low";
    case SimulatedPriority::Medium: return "medium";
    case SimulatedPriority::Critical: return "critical";
    }
    return "medium";
}

bool priorityFromName(const std::string& name, SimulatedPriority& priority) noexcept
{
    if (name == "low") priority = SimulatedPriority::Low;
    else if (name == "medium") priority = SimulatedPriority::Medium;
    else if (name == "critical") priority = SimulatedPriority::Critical;
    else return false;
    return true;
}

const char* requestStatusName(RequestStatus status) noexcept
{
    switch (status) {
    case RequestStatus::Queued: return "queued";
    case RequestStatus::ToPatient: return "to-patient";
    case RequestStatus::ToHospital: return "to-hospital";
    case RequestStatus::Completed: return "completed";
    case RequestStatus::Cancelled: return "cancelled";
    case RequestStatus::Failed: return "failed";
    }
    return "failed";
}

bool requestStatusFromName(const std::string& name, RequestStatus& status) noexcept
{
    if (name == "queued") status = RequestStatus::Queued;
    else if (name == "to-patient") status = RequestStatus::ToPatient;
    else if (name == "to-hospital") status = RequestStatus::ToHospital;
    else if (name == "completed") status = RequestStatus::Completed;
    else if (name == "cancelled") status = RequestStatus::Cancelled;
    else if (name == "failed") status = RequestStatus::Failed;
    else return false;
    return true;
}

} // namespace smartcity::core
