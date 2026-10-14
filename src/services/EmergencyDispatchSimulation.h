#pragma once

#include "core/models/EmergencyModels.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace smartcity::algorithms { class WeightedGraph; }

namespace smartcity::services {

enum class DispatchResultCode {
    Assigned,
    NoQueuedRequest,
    NoAvailableAmbulance,
    PatientUnreachable,
    NoSuitableHospital
};

struct DispatchResult {
    DispatchResultCode code{DispatchResultCode::NoQueuedRequest};
    int requestId{-1};
    int ambulanceId{-1};
    int hospitalLocationId{-1};
    double ambulanceTravelMinutes{0.0};
    double hospitalTravelMinutes{0.0};
    std::string message;
};

enum class SimulationStepCode {
    Moved,
    Rerouted,
    PatientReached,
    HospitalReached,
    Requeued,
    Failed,
    InvalidRequest
};

struct SimulationStepResult {
    SimulationStepCode code{SimulationStepCode::InvalidRequest};
    int requestId{-1};
    int ambulanceId{-1};
    int fromLocationId{-1};
    int toLocationId{-1};
    int roadId{-1};
    double elapsedMinutes{0.0};
    std::string message;
};

struct PlannedJourney {
    std::vector<int> locationIds;
    std::vector<int> roadIds;
    std::size_t nextRoadIndex{0};
};

class EmergencyDispatchSimulation {
public:
    [[nodiscard]] double clockMinutes() const noexcept;
    void advanceClock(double minutes);
    [[nodiscard]] int nextAmbulanceId() noexcept;
    [[nodiscard]] int nextRequestId() noexcept;

    bool addAmbulance(const core::Ambulance& ambulance,
                      const algorithms::WeightedGraph& graph);
    bool updateAmbulance(const core::Ambulance& ambulance,
                         const algorithms::WeightedGraph& graph);
    bool removeAmbulance(int ambulanceId);
    bool addHospital(const core::HospitalResource& hospital,
                     const algorithms::WeightedGraph& graph);
    bool updateHospital(const core::HospitalResource& hospital,
                        const algorithms::WeightedGraph& graph);
    bool removeHospital(int locationId);
    bool addRequest(const core::EmergencyRequest& request,
                    const algorithms::WeightedGraph& graph);
    bool cancelRequest(int requestId);
    bool removeRequest(int requestId);

    [[nodiscard]] DispatchResult dispatchNext(const algorithms::WeightedGraph& graph);
    [[nodiscard]] SimulationStepResult advanceOneNode(
        const algorithms::WeightedGraph& graph, int requestId);

    [[nodiscard]] const std::unordered_map<int, core::Ambulance>& ambulances() const noexcept;
    [[nodiscard]] const std::unordered_map<int, core::HospitalResource>& hospitals() const noexcept;
    [[nodiscard]] const std::unordered_map<int, core::EmergencyRequest>& requests() const noexcept;
    [[nodiscard]] const PlannedJourney* plannedJourney(int requestId) const noexcept;
    [[nodiscard]] std::vector<int> activeRequestIds() const;
    [[nodiscard]] bool referencesLocation(int locationId) const noexcept;
    [[nodiscard]] bool validateAgainstGraph(const algorithms::WeightedGraph& graph,
                                            std::string* errorMessage = nullptr) const;

    bool replaceData(double clockMinutes,
                     const std::vector<core::Ambulance>& ambulances,
                     const std::vector<core::HospitalResource>& hospitals,
                     const std::vector<core::EmergencyRequest>& requests,
                     const algorithms::WeightedGraph& graph,
                     std::string* errorMessage = nullptr);

private:
    struct ActiveJourney {
        PlannedJourney route;
        std::uint64_t graphRevision{static_cast<std::uint64_t>(-1)};
    };

    [[nodiscard]] int nextQueuedRequestId() const;
    bool rebuildJourney(const algorithms::WeightedGraph& graph, int requestId,
                        SimulationStepResult* result);
    void releaseAssignment(core::EmergencyRequest& request, bool restoreBed);
    SimulationStepResult arriveAtTarget(core::EmergencyRequest& request,
                                        core::Ambulance& ambulance,
                                        const algorithms::WeightedGraph& graph);

    double clockMinutes_{0.0};
    int nextAmbulanceId_{0};
    int nextRequestId_{0};
    std::unordered_map<int, core::Ambulance> ambulances_;
    std::unordered_map<int, core::HospitalResource> hospitals_;
    std::unordered_map<int, core::EmergencyRequest> requests_;
    std::unordered_map<int, ActiveJourney> activeJourneys_;
};

} // namespace smartcity::services
