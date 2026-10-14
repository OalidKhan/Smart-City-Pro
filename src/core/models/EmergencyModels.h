#pragma once

#include <string>
#include <vector>

namespace smartcity::core {

enum class EmergencyService { General, Trauma, Cardiac, Pediatric };
enum class SimulatedPriority { Low = 1, Medium = 2, Critical = 3 };
enum class RequestStatus { Queued, ToPatient, ToHospital, Completed, Cancelled, Failed };

struct Ambulance {
    int id{-1};
    int currentLocationId{-1};
    bool available{true};
    int assignedRequestId{-1};
};

struct HospitalResource {
    int locationId{-1};
    int availableBeds{0};
    std::vector<EmergencyService> supportedServices;

    [[nodiscard]] bool supports(EmergencyService service) const noexcept;
};

struct EmergencyRequest {
    int id{-1};
    int locationId{-1};
    SimulatedPriority priority{SimulatedPriority::Medium};
    double creationTimeMinutes{0.0};
    EmergencyService requiredService{EmergencyService::General};
    RequestStatus status{RequestStatus::Queued};
    int assignedAmbulanceId{-1};
    int assignedHospitalLocationId{-1};
    double responseTimeMinutes{-1.0};
    double transportTimeMinutes{-1.0};
    double accumulatedTransportMinutes{0.0};
    std::string note;
};

[[nodiscard]] const char* emergencyServiceName(EmergencyService service) noexcept;
[[nodiscard]] bool emergencyServiceFromName(const std::string& name,
                                            EmergencyService& service) noexcept;
[[nodiscard]] const char* priorityName(SimulatedPriority priority) noexcept;
[[nodiscard]] bool priorityFromName(const std::string& name,
                                    SimulatedPriority& priority) noexcept;
[[nodiscard]] const char* requestStatusName(RequestStatus status) noexcept;
[[nodiscard]] bool requestStatusFromName(const std::string& name,
                                         RequestStatus& status) noexcept;

} // namespace smartcity::core
