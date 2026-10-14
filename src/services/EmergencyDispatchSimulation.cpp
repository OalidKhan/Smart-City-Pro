#include "services/EmergencyDispatchSimulation.h"

#include "algorithms/RouteAlgorithms.h"
#include "algorithms/WeightedGraph.h"
#include "core/models/CityLocation.h"
#include "core/models/Road.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <tuple>
#include <unordered_set>
#include <utility>

namespace smartcity::services {
namespace {

bool routeFound(const algorithms::RouteResult& route)
{
    return route.status == algorithms::RouteStatus::Found
        || route.status == algorithms::RouteStatus::Trivial;
}

bool lowerWeight(double candidate, int candidateId, double best, int bestId)
{
    constexpr double Epsilon = 1e-9;
    if (!std::isfinite(best)) return true;
    const double tolerance = Epsilon * std::max({1.0, std::abs(candidate), std::abs(best)});
    if (candidate < best - tolerance) return true;
    return std::abs(candidate - best) <= tolerance && candidateId < bestId;
}

bool servicesValid(const std::vector<core::EmergencyService>& services)
{
    if (services.empty()) return false;
    std::unordered_set<int> unique;
    for (const auto service : services) {
        const int value = static_cast<int>(service);
        if (value < static_cast<int>(core::EmergencyService::General)
            || value > static_cast<int>(core::EmergencyService::Pediatric)
            || !unique.insert(value).second) return false;
    }
    return true;
}

bool requestEnumsValid(const core::EmergencyRequest& request)
{
    const int priority = static_cast<int>(request.priority);
    const int service = static_cast<int>(request.requiredService);
    const int status = static_cast<int>(request.status);
    return priority >= static_cast<int>(core::SimulatedPriority::Low)
        && priority <= static_cast<int>(core::SimulatedPriority::Critical)
        && service >= static_cast<int>(core::EmergencyService::General)
        && service <= static_cast<int>(core::EmergencyService::Pediatric)
        && status >= static_cast<int>(core::RequestStatus::Queued)
        && status <= static_cast<int>(core::RequestStatus::Failed);
}

} // namespace

double EmergencyDispatchSimulation::clockMinutes() const noexcept { return clockMinutes_; }

void EmergencyDispatchSimulation::advanceClock(double minutes)
{
    if (std::isfinite(minutes) && minutes >= 0.0
        && minutes <= std::numeric_limits<double>::max() - clockMinutes_) clockMinutes_ += minutes;
}

int EmergencyDispatchSimulation::nextAmbulanceId() noexcept
{
    while (ambulances_.find(nextAmbulanceId_) != ambulances_.end()) ++nextAmbulanceId_;
    return nextAmbulanceId_++;
}

int EmergencyDispatchSimulation::nextRequestId() noexcept
{
    while (requests_.find(nextRequestId_) != requests_.end()) ++nextRequestId_;
    return nextRequestId_++;
}

bool EmergencyDispatchSimulation::addAmbulance(
    const core::Ambulance& ambulance, const algorithms::WeightedGraph& graph)
{
    if (ambulance.id < 0 || ambulance.id == std::numeric_limits<int>::max()
        || !graph.containsVertex(ambulance.currentLocationId)
        || ambulance.assignedRequestId != -1 || ambulances_.find(ambulance.id) != ambulances_.end()) {
        return false;
    }
    ambulances_.emplace(ambulance.id, ambulance);
    nextAmbulanceId_ = std::max(nextAmbulanceId_, ambulance.id + 1);
    return true;
}

bool EmergencyDispatchSimulation::updateAmbulance(
    const core::Ambulance& ambulance, const algorithms::WeightedGraph& graph)
{
    auto found = ambulances_.find(ambulance.id);
    if (found == ambulances_.end() || found->second.assignedRequestId != -1
        || ambulance.assignedRequestId != -1 || !graph.containsVertex(ambulance.currentLocationId)) {
        return false;
    }
    found->second = ambulance;
    return true;
}

bool EmergencyDispatchSimulation::removeAmbulance(int ambulanceId)
{
    const auto found = ambulances_.find(ambulanceId);
    if (found == ambulances_.end() || found->second.assignedRequestId != -1) return false;
    ambulances_.erase(found);
    return true;
}

bool EmergencyDispatchSimulation::addHospital(
    const core::HospitalResource& hospital, const algorithms::WeightedGraph& graph)
{
    const auto* location = graph.location(hospital.locationId);
    if (!location || location->category != core::LocationCategory::Hospital
        || hospital.availableBeds < 0 || !servicesValid(hospital.supportedServices)
        || hospitals_.find(hospital.locationId) != hospitals_.end()) return false;
    hospitals_.emplace(hospital.locationId, hospital);
    return true;
}

bool EmergencyDispatchSimulation::updateHospital(
    const core::HospitalResource& hospital, const algorithms::WeightedGraph& graph)
{
    auto found = hospitals_.find(hospital.locationId);
    const auto* location = graph.location(hospital.locationId);
    if (found == hospitals_.end() || !location
        || location->category != core::LocationCategory::Hospital
        || hospital.availableBeds < 0 || !servicesValid(hospital.supportedServices)) return false;
    for (const auto& entry : requests_) {
        const auto& request = entry.second;
        if ((request.status == core::RequestStatus::ToPatient
             || request.status == core::RequestStatus::ToHospital)
            && request.assignedHospitalLocationId == hospital.locationId) return false;
    }
    found->second = hospital;
    return true;
}

bool EmergencyDispatchSimulation::removeHospital(int locationId)
{
    for (const auto& entry : requests_) {
        const auto& request = entry.second;
        if ((request.status == core::RequestStatus::ToPatient
             || request.status == core::RequestStatus::ToHospital)
            && request.assignedHospitalLocationId == locationId) return false;
    }
    return hospitals_.erase(locationId) > 0;
}

bool EmergencyDispatchSimulation::addRequest(
    const core::EmergencyRequest& request, const algorithms::WeightedGraph& graph)
{
    if (request.id < 0 || request.id == std::numeric_limits<int>::max()
        || !graph.containsVertex(request.locationId)
        || !std::isfinite(request.creationTimeMinutes) || request.creationTimeMinutes < 0.0
        || request.creationTimeMinutes > clockMinutes_
        || !requestEnumsValid(request)
        || request.status != core::RequestStatus::Queued
        || request.assignedAmbulanceId != -1 || request.assignedHospitalLocationId != -1
        || requests_.find(request.id) != requests_.end()) return false;
    requests_.emplace(request.id, request);
    nextRequestId_ = std::max(nextRequestId_, request.id + 1);
    return true;
}

bool EmergencyDispatchSimulation::cancelRequest(int requestId)
{
    auto found = requests_.find(requestId);
    if (found == requests_.end()
        || (found->second.status != core::RequestStatus::Queued
            && found->second.status != core::RequestStatus::ToPatient
            && found->second.status != core::RequestStatus::ToHospital)) return false;
    auto& request = found->second;
    const bool active = request.status == core::RequestStatus::ToPatient
        || request.status == core::RequestStatus::ToHospital;
    if (active) releaseAssignment(request, true);
    activeJourneys_.erase(requestId);
    request.status = core::RequestStatus::Cancelled;
    request.note = "Cancelled by the simulation user.";
    return true;
}

bool EmergencyDispatchSimulation::removeRequest(int requestId)
{
    const auto found = requests_.find(requestId);
    if (found == requests_.end()
        || found->second.status == core::RequestStatus::Queued
        || found->second.status == core::RequestStatus::ToPatient
        || found->second.status == core::RequestStatus::ToHospital) return false;
    activeJourneys_.erase(requestId);
    requests_.erase(found);
    return true;
}

int EmergencyDispatchSimulation::nextQueuedRequestId() const
{
    struct QueueEntry {
        int priority;
        double creationTime;
        int requestId;
    };
    const auto lowerPriority = [](const QueueEntry& left, const QueueEntry& right) {
        if (left.priority != right.priority) return left.priority < right.priority;
        if (left.creationTime != right.creationTime) return left.creationTime > right.creationTime;
        return left.requestId > right.requestId;
    };
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, decltype(lowerPriority)> queue(lowerPriority);
    for (const auto& entry : requests_) {
        const auto& request = entry.second;
        if (request.status == core::RequestStatus::Queued) {
            queue.push({static_cast<int>(request.priority), request.creationTimeMinutes, request.id});
        }
    }
    return queue.empty() ? -1 : queue.top().requestId;
}

DispatchResult EmergencyDispatchSimulation::dispatchNext(const algorithms::WeightedGraph& graph)
{
    DispatchResult result;
    result.requestId = nextQueuedRequestId();
    if (result.requestId < 0) {
        result.message = "No queued simulated request.";
        return result;
    }
    auto& request = requests_.at(result.requestId);

    std::vector<int> ambulanceIds;
    for (const auto& entry : ambulances_)
        if (entry.second.available) ambulanceIds.push_back(entry.first);
    std::sort(ambulanceIds.begin(), ambulanceIds.end());
    if (ambulanceIds.empty()) {
        result.code = DispatchResultCode::NoAvailableAmbulance;
        result.message = "No ambulance is currently available; the request remains queued.";
        request.note = result.message;
        return result;
    }

    int bestAmbulance = -1;
    double bestAmbulanceTime = std::numeric_limits<double>::infinity();
    algorithms::RouteResult bestPatientRoute;
    for (const int ambulanceId : ambulanceIds) {
        const auto route = algorithms::RouteAlgorithms::dijkstra(
            graph, ambulances_.at(ambulanceId).currentLocationId,
            request.locationId, algorithms::RouteMetric::TravelTime);
        if (routeFound(route)
            && lowerWeight(route.totalWeight, ambulanceId, bestAmbulanceTime, bestAmbulance)) {
            bestAmbulance = ambulanceId;
            bestAmbulanceTime = route.totalWeight;
            bestPatientRoute = route;
        }
    }
    if (bestAmbulance < 0) {
        result.code = DispatchResultCode::PatientUnreachable;
        result.message = "No available ambulance can reach the patient using open roads.";
        request.note = result.message;
        return result;
    }

    std::vector<int> hospitalIds;
    for (const auto& entry : hospitals_) hospitalIds.push_back(entry.first);
    std::sort(hospitalIds.begin(), hospitalIds.end());
    int bestHospital = -1;
    double bestHospitalTime = std::numeric_limits<double>::infinity();
    for (const int locationId : hospitalIds) {
        const auto& hospital = hospitals_.at(locationId);
        if (hospital.availableBeds <= 0 || !hospital.supports(request.requiredService)) continue;
        const auto route = algorithms::RouteAlgorithms::dijkstra(
            graph, request.locationId, locationId, algorithms::RouteMetric::TravelTime);
        if (routeFound(route)
            && lowerWeight(route.totalWeight, locationId, bestHospitalTime, bestHospital)) {
            bestHospital = locationId;
            bestHospitalTime = route.totalWeight;
        }
    }
    if (bestHospital < 0) {
        result.code = DispatchResultCode::NoSuitableHospital;
        result.message = "No reachable hospital has the required service and an available simulated bed.";
        request.note = result.message;
        return result;
    }

    auto& ambulance = ambulances_.at(bestAmbulance);
    auto& hospital = hospitals_.at(bestHospital);
    ambulance.available = false;
    ambulance.assignedRequestId = request.id;
    --hospital.availableBeds;
    request.status = core::RequestStatus::ToPatient;
    request.assignedAmbulanceId = bestAmbulance;
    request.assignedHospitalLocationId = bestHospital;
    request.note = "Ambulance and hospital bed reserved.";
    activeJourneys_[request.id] = {
        {bestPatientRoute.locationIds, bestPatientRoute.roadIds, 0}, graph.revision()};

    result.code = DispatchResultCode::Assigned;
    result.ambulanceId = bestAmbulance;
    result.hospitalLocationId = bestHospital;
    result.ambulanceTravelMinutes = bestAmbulanceTime;
    result.hospitalTravelMinutes = bestHospitalTime;
    result.message = "Resources reserved and ambulance dispatched.";
    return result;
}

void EmergencyDispatchSimulation::releaseAssignment(
    core::EmergencyRequest& request, bool restoreBed)
{
    const auto ambulance = ambulances_.find(request.assignedAmbulanceId);
    if (ambulance != ambulances_.end()) {
        ambulance->second.available = true;
        ambulance->second.assignedRequestId = -1;
    }
    if (restoreBed) {
        const auto hospital = hospitals_.find(request.assignedHospitalLocationId);
        if (hospital != hospitals_.end()) ++hospital->second.availableBeds;
    }
    request.assignedAmbulanceId = -1;
    request.assignedHospitalLocationId = -1;
}

bool EmergencyDispatchSimulation::rebuildJourney(
    const algorithms::WeightedGraph& graph, int requestId, SimulationStepResult* result)
{
    auto& request = requests_.at(requestId);
    auto& ambulance = ambulances_.at(request.assignedAmbulanceId);
    const int target = request.status == core::RequestStatus::ToPatient
        ? request.locationId : request.assignedHospitalLocationId;
    const auto route = algorithms::RouteAlgorithms::dijkstra(
        graph, ambulance.currentLocationId, target, algorithms::RouteMetric::TravelTime);
    if (!routeFound(route)) {
        const bool beforePatient = request.status == core::RequestStatus::ToPatient;
        releaseAssignment(request, true);
        activeJourneys_.erase(requestId);
        request.status = beforePatient ? core::RequestStatus::Queued : core::RequestStatus::Failed;
        request.note = beforePatient
            ? "Road changes made the patient unreachable; resources released and request requeued."
            : "Road changes left no route to the reserved hospital; simulation journey failed.";
        if (result) {
            result->code = beforePatient ? SimulationStepCode::Requeued : SimulationStepCode::Failed;
            result->message = request.note;
        }
        return false;
    }
    activeJourneys_[requestId] = {{route.locationIds, route.roadIds, 0}, graph.revision()};
    if (result) {
        result->code = SimulationStepCode::Rerouted;
        result->message = "Road data changed; remaining journey rerouted from the current node.";
    }
    return true;
}

SimulationStepResult EmergencyDispatchSimulation::arriveAtTarget(
    core::EmergencyRequest& request, core::Ambulance& ambulance,
    const algorithms::WeightedGraph& graph)
{
    SimulationStepResult result;
    result.requestId = request.id;
    result.ambulanceId = ambulance.id;
    if (request.status == core::RequestStatus::ToPatient) {
        request.responseTimeMinutes = clockMinutes_ - request.creationTimeMinutes;
        request.status = core::RequestStatus::ToHospital;
        request.note = "Patient reached; transport to the reserved hospital started.";
        result.code = SimulationStepCode::PatientReached;
        result.message = request.note;
        if (!rebuildJourney(graph, request.id, nullptr)) {
            result.code = SimulationStepCode::Failed;
            result.message = request.note;
        }
        return result;
    }

    request.transportTimeMinutes = request.accumulatedTransportMinutes;
    request.status = core::RequestStatus::Completed;
    request.note = "Patient arrived at the simulated hospital.";
    ambulance.available = true;
    ambulance.assignedRequestId = -1;
    activeJourneys_.erase(request.id);
    result.code = SimulationStepCode::HospitalReached;
    result.message = request.note;
    return result;
}

SimulationStepResult EmergencyDispatchSimulation::advanceOneNode(
    const algorithms::WeightedGraph& graph, int requestId)
{
    SimulationStepResult result;
    result.requestId = requestId;
    auto requestIterator = requests_.find(requestId);
    if (requestIterator == requests_.end()
        || (requestIterator->second.status != core::RequestStatus::ToPatient
            && requestIterator->second.status != core::RequestStatus::ToHospital)) {
        result.message = "The request has no active journey.";
        return result;
    }
    auto& request = requestIterator->second;
    auto ambulanceIterator = ambulances_.find(request.assignedAmbulanceId);
    if (ambulanceIterator == ambulances_.end()) {
        request.status = core::RequestStatus::Failed;
        request.note = "Assigned ambulance is missing.";
        result.code = SimulationStepCode::Failed;
        result.message = request.note;
        return result;
    }
    auto& ambulance = ambulanceIterator->second;
    result.ambulanceId = ambulance.id;

    auto journey = activeJourneys_.find(requestId);
    if (journey == activeJourneys_.end() || journey->second.graphRevision != graph.revision()) {
        if (!rebuildJourney(graph, requestId, &result)) return result;
        return result;
    }
    auto& route = journey->second.route;
    if (route.nextRoadIndex >= route.roadIds.size()) {
        return arriveAtTarget(request, ambulance, graph);
    }

    const int roadId = route.roadIds[route.nextRoadIndex];
    const auto* road = graph.road(roadId);
    if (!road || road->closed || route.nextRoadIndex + 1 >= route.locationIds.size()
        || route.locationIds[route.nextRoadIndex] != ambulance.currentLocationId) {
        journey->second.graphRevision = static_cast<std::uint64_t>(-1);
        if (!rebuildJourney(graph, requestId, &result)) return result;
        return result;
    }

    const int from = ambulance.currentLocationId;
    const int to = route.locationIds[route.nextRoadIndex + 1];
    const bool allowedDirection = road->fromLocationId == from && road->toLocationId == to;
    const bool allowedReverse = road->direction == core::RoadDirection::TwoWay
        && road->toLocationId == from && road->fromLocationId == to;
    if (!allowedDirection && !allowedReverse) {
        journey->second.graphRevision = static_cast<std::uint64_t>(-1);
        if (!rebuildJourney(graph, requestId, &result)) return result;
        return result;
    }

    ambulance.currentLocationId = to;
    clockMinutes_ += road->travelTime;
    if (request.status == core::RequestStatus::ToHospital)
        request.accumulatedTransportMinutes += road->travelTime;
    ++route.nextRoadIndex;
    result.code = SimulationStepCode::Moved;
    result.fromLocationId = from;
    result.toLocationId = to;
    result.roadId = roadId;
    result.elapsedMinutes = road->travelTime;
    result.message = "Ambulance advanced to the next graph node.";

    if (route.nextRoadIndex >= route.roadIds.size()) {
        auto arrival = arriveAtTarget(request, ambulance, graph);
        arrival.fromLocationId = from;
        arrival.toLocationId = to;
        arrival.roadId = roadId;
        arrival.elapsedMinutes = road->travelTime;
        return arrival;
    }
    return result;
}

const std::unordered_map<int, core::Ambulance>&
EmergencyDispatchSimulation::ambulances() const noexcept { return ambulances_; }

const std::unordered_map<int, core::HospitalResource>&
EmergencyDispatchSimulation::hospitals() const noexcept { return hospitals_; }

const std::unordered_map<int, core::EmergencyRequest>&
EmergencyDispatchSimulation::requests() const noexcept { return requests_; }

const PlannedJourney* EmergencyDispatchSimulation::plannedJourney(int requestId) const noexcept
{
    const auto found = activeJourneys_.find(requestId);
    return found == activeJourneys_.end() ? nullptr : &found->second.route;
}

std::vector<int> EmergencyDispatchSimulation::activeRequestIds() const
{
    std::vector<int> ids;
    for (const auto& entry : requests_) {
        if (entry.second.status == core::RequestStatus::ToPatient
            || entry.second.status == core::RequestStatus::ToHospital) ids.push_back(entry.first);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool EmergencyDispatchSimulation::referencesLocation(int locationId) const noexcept
{
    for (const auto& entry : ambulances_)
        if (entry.second.currentLocationId == locationId) return true;
    if (hospitals_.find(locationId) != hospitals_.end()) return true;
    for (const auto& entry : requests_) {
        if (entry.second.locationId == locationId
            || entry.second.assignedHospitalLocationId == locationId) return true;
    }
    return false;
}

bool EmergencyDispatchSimulation::validateAgainstGraph(
    const algorithms::WeightedGraph& graph, std::string* errorMessage) const
{
    for (const auto& entry : ambulances_) {
        if (!graph.containsVertex(entry.second.currentLocationId)) {
            if (errorMessage) *errorMessage = "An ambulance references a missing location.";
            return false;
        }
    }
    for (const auto& entry : hospitals_) {
        const auto* location = graph.location(entry.first);
        if (!location || location->category != core::LocationCategory::Hospital) {
            if (errorMessage) *errorMessage = "A simulated hospital requires a matching hospital-category location.";
            return false;
        }
    }
    for (const auto& entry : requests_) {
        if (!graph.containsVertex(entry.second.locationId)) {
            if (errorMessage) *errorMessage = "A request references a missing patient location.";
            return false;
        }
        if (entry.second.assignedHospitalLocationId >= 0
            && !graph.containsVertex(entry.second.assignedHospitalLocationId)) {
            if (errorMessage) *errorMessage = "A request references a missing hospital location.";
            return false;
        }
    }
    return true;
}

bool EmergencyDispatchSimulation::replaceData(
    double clockMinutes, const std::vector<core::Ambulance>& ambulances,
    const std::vector<core::HospitalResource>& hospitals,
    const std::vector<core::EmergencyRequest>& requests,
    const algorithms::WeightedGraph& graph, std::string* errorMessage)
{
    if (!std::isfinite(clockMinutes) || clockMinutes < 0.0) {
        if (errorMessage) *errorMessage = "Invalid simulation clock.";
        return false;
    }
    EmergencyDispatchSimulation candidate;
    candidate.clockMinutes_ = clockMinutes;
    for (const auto& hospital : hospitals) {
        if (!candidate.addHospital(hospital, graph)) {
            if (errorMessage) *errorMessage = "Invalid or duplicate hospital resource.";
            return false;
        }
    }
    for (const auto& ambulance : ambulances) {
        if (ambulance.id < 0 || ambulance.id == std::numeric_limits<int>::max()
            || !graph.containsVertex(ambulance.currentLocationId)
            || (ambulance.assignedRequestId != -1 && ambulance.available)
            || candidate.ambulances_.find(ambulance.id) != candidate.ambulances_.end()) {
            if (errorMessage) *errorMessage = "Invalid or duplicate ambulance.";
            return false;
        }
        candidate.ambulances_.emplace(ambulance.id, ambulance);
        candidate.nextAmbulanceId_ = std::max(candidate.nextAmbulanceId_, ambulance.id + 1);
    }
    for (const auto& request : requests) {
        if (request.id < 0 || request.id == std::numeric_limits<int>::max()
            || !graph.containsVertex(request.locationId)
            || !std::isfinite(request.creationTimeMinutes) || request.creationTimeMinutes < 0.0
            || request.creationTimeMinutes > clockMinutes
            || !requestEnumsValid(request)
            || !std::isfinite(request.responseTimeMinutes)
            || !std::isfinite(request.transportTimeMinutes)
            || !std::isfinite(request.accumulatedTransportMinutes)
            || request.responseTimeMinutes < -1.0
            || request.transportTimeMinutes < -1.0
            || request.accumulatedTransportMinutes < 0.0
            || (request.assignedHospitalLocationId >= 0
                && !graph.containsVertex(request.assignedHospitalLocationId))
            || candidate.requests_.find(request.id) != candidate.requests_.end()) {
            if (errorMessage) *errorMessage = "Invalid or duplicate request.";
            return false;
        }
        const bool active = request.status == core::RequestStatus::ToPatient
            || request.status == core::RequestStatus::ToHospital;
        if ((request.status == core::RequestStatus::ToHospital
             && request.responseTimeMinutes < 0.0)
            || (request.status == core::RequestStatus::Completed
                && (request.responseTimeMinutes < 0.0
                    || request.transportTimeMinutes < 0.0
                    || request.assignedAmbulanceId < 0
                    || request.assignedHospitalLocationId < 0))) {
            if (errorMessage) *errorMessage = "Request timing does not match its state.";
            return false;
        }
        if (active) {
            const auto ambulance = candidate.ambulances_.find(request.assignedAmbulanceId);
            const auto hospital = candidate.hospitals_.find(request.assignedHospitalLocationId);
            if (ambulance == candidate.ambulances_.end() || hospital == candidate.hospitals_.end()
                || ambulance->second.available
                || ambulance->second.assignedRequestId != request.id
                || !hospital->second.supports(request.requiredService)) {
                if (errorMessage) *errorMessage = "Active request has inconsistent reservations.";
                return false;
            }
            candidate.activeJourneys_.emplace(request.id, ActiveJourney{});
        } else if ((request.status == core::RequestStatus::Queued
                    || request.status == core::RequestStatus::Cancelled
                    || request.status == core::RequestStatus::Failed)
                   && (request.assignedAmbulanceId != -1
                       || request.assignedHospitalLocationId != -1)) {
            if (errorMessage) *errorMessage = "Inactive request retains a reserved resource.";
            return false;
        }
        candidate.requests_.emplace(request.id, request);
        candidate.nextRequestId_ = std::max(candidate.nextRequestId_, request.id + 1);
    }
    for (const auto& entry : candidate.ambulances_) {
        if (entry.second.assignedRequestId != -1) {
            const auto request = candidate.requests_.find(entry.second.assignedRequestId);
            if (request == candidate.requests_.end()
                || (request->second.status != core::RequestStatus::ToPatient
                    && request->second.status != core::RequestStatus::ToHospital)) {
                if (errorMessage) *errorMessage = "Reserved ambulance has no active request.";
                return false;
            }
        }
    }
    *this = std::move(candidate);
    return true;
}

} // namespace smartcity::services
