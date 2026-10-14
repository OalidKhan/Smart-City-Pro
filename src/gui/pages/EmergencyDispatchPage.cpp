#include "gui/pages/EmergencyDispatchPage.h"

#include "algorithms/WeightedGraph.h"
#include "core/models/CityLocation.h"
#include "core/models/EmergencyModels.h"
#include "core/models/Road.h"
#include "services/CityRepository.h"
#include "services/DispatchJsonSerializer.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QGridLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QShowEvent>
#include <QSlider>
#include <QSpinBox>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <vector>

namespace {

using smartcity::core::EmergencyService;
using smartcity::core::RequestStatus;
using smartcity::core::SimulatedPriority;

QString locationText(const smartcity::algorithms::WeightedGraph& graph, int id)
{
    const auto* location = graph.location(id);
    return location ? QString("L%1 — %2").arg(id).arg(QString::fromStdString(location->name))
                    : QString("L%1 (missing)").arg(id);
}

QString servicesText(const smartcity::core::HospitalResource& hospital)
{
    QStringList values;
    for (const auto service : hospital.supportedServices)
        values.push_back(QLatin1String(smartcity::core::emergencyServiceName(service)));
    return values.join(", ");
}

void configureTable(QTableWidget* table, const QStringList& headers)
{
    table->setObjectName("dispatchTable");
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setAlternatingRowColors(true);
}

void populateLocations(QComboBox* combo, const smartcity::algorithms::WeightedGraph& graph,
                       bool hospitalsOnly = false)
{
    std::vector<int> ids;
    for (const auto& entry : graph.locations()) {
        if (!hospitalsOnly || entry.second.category == smartcity::core::LocationCategory::Hospital)
            ids.push_back(entry.first);
    }
    std::sort(ids.begin(), ids.end());
    for (const int id : ids) combo->addItem(locationText(graph, id), id);
}

void populateServices(QComboBox* combo)
{
    for (const auto service : {EmergencyService::General, EmergencyService::Trauma,
                               EmergencyService::Cardiac, EmergencyService::Pediatric}) {
        combo->addItem(QLatin1String(smartcity::core::emergencyServiceName(service)),
                       static_cast<int>(service));
    }
}

} // namespace

EmergencyDispatchPage::EmergencyDispatchPage(
    smartcity::services::CityRepository& repository, QWidget* parent)
    : QWidget(parent), repository_(repository), scene_(new QGraphicsScene(this)),
      view_(new QGraphicsView(scene_, this)), ambulanceTable_(new QTableWidget(this)),
      hospitalTable_(new QTableWidget(this)), requestTable_(new QTableWidget(this)),
      activeRequestCombo_(new QComboBox(this)), clockLabel_(new QLabel(this)),
      statusLabel_(new QLabel("Create a simulated request or dispatch the next queued request.", this)),
      speedLabel_(new QLabel("Speed: 1.0×", this)), timer_(new QTimer(this))
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(8);
    auto* title = new QLabel("Emergency Ambulance Dispatch Simulator", this);
    title->setObjectName("pageTitle");
    auto* description = new QLabel(
        "Educational simulation only — no real emergency-service calls are made. Greedy dispatch chooses the best current assignment; it is not globally optimal across multiple requests.", this);
    description->setObjectName("pageDescription");
    description->setWordWrap(true);
    root->addWidget(title);
    root->addWidget(description);

    auto* editorBar = new QFrame(this);
    editorBar->setObjectName("routePanel");
    auto* editorLayout = new QGridLayout(editorBar);
    const std::vector<std::pair<QString, std::function<void()>>> editorActions = {
        {"+ Ambulance", [this] { addAmbulance(); }}, {"Edit ambulance", [this] { editAmbulance(); }},
        {"Delete ambulance", [this] { deleteAmbulance(); }}, {"+ Hospital", [this] { addHospital(); }},
        {"Edit hospital", [this] { editHospital(); }}, {"Delete hospital", [this] { deleteHospital(); }},
        {"+ Request", [this] { createRequest(); }}, {"Cancel request", [this] { cancelRequest(); }},
        {"Delete record", [this] { deleteRequestRecord(); }}
    };
    int editorIndex = 0;
    for (const auto& action : editorActions) {
        auto* button = new QPushButton(action.first, editorBar);
        button->setObjectName("editorButton");
        connect(button, &QPushButton::clicked, this, action.second);
        editorLayout->addWidget(button, editorIndex / 5, editorIndex % 5);
        ++editorIndex;
    }
    root->addWidget(editorBar);

    auto* content = new QHBoxLayout;
    view_->setObjectName("dispatchView");
    view_->setRenderHint(QPainter::Antialiasing);
    view_->setDragMode(QGraphicsView::ScrollHandDrag);
    view_->setMinimumHeight(315);
    scene_->setBackgroundBrush(QColor("#f8fafc"));
    content->addWidget(view_, 1);

    auto* tabs = new QTabWidget(this);
    tabs->setMinimumWidth(500);
    configureTable(ambulanceTable_, {"ID", "Location", "Availability", "Request"});
    configureTable(hospitalTable_, {"Location", "Beds", "Services"});
    configureTable(requestTable_, {"ID", "Priority", "Created", "Location", "Service", "Status",
                                   "Amb.", "Hospital", "Response", "Transport"});
    tabs->addTab(requestTable_, "Requests");
    tabs->addTab(ambulanceTable_, "Ambulances");
    tabs->addTab(hospitalTable_, "Hospitals");
    content->addWidget(tabs);
    root->addLayout(content, 1);

    auto* simulationBar = new QFrame(this);
    simulationBar->setObjectName("routePanel");
    auto* simulationLayout = new QHBoxLayout(simulationBar);
    auto* dispatchButton = new QPushButton("Dispatch next", simulationBar);
    dispatchButton->setObjectName("primaryButton");
    auto* playButton = new QPushButton("Play", simulationBar);
    auto* pauseButton = new QPushButton("Pause", simulationBar);
    auto* stepButton = new QPushButton("Step to next node", simulationBar);
    auto* clockButton = new QPushButton("+1 clock minute", simulationBar);
    auto* saveButton = new QPushButton("Save simulation", simulationBar);
    auto* loadButton = new QPushButton("Load simulation", simulationBar);
    for (auto* button : {playButton, pauseButton, stepButton, clockButton, saveButton, loadButton})
        button->setObjectName("editorButton");
    simulationLayout->addWidget(dispatchButton);
    simulationLayout->addWidget(new QLabel("Active request", simulationBar));
    simulationLayout->addWidget(activeRequestCombo_);
    simulationLayout->addWidget(playButton);
    simulationLayout->addWidget(pauseButton);
    simulationLayout->addWidget(stepButton);
    simulationLayout->addWidget(clockButton);
    simulationLayout->addStretch();
    simulationLayout->addWidget(saveButton);
    simulationLayout->addWidget(loadButton);
    root->addWidget(simulationBar);

    auto* infoBar = new QHBoxLayout;
    clockLabel_->setObjectName("sectionTitle");
    statusLabel_->setObjectName("mapSummary");
    statusLabel_->setWordWrap(true);
    auto* speed = new QSlider(Qt::Horizontal, this);
    speed->setRange(1, 10);
    speed->setValue(5);
    speed->setMaximumWidth(130);
    infoBar->addWidget(clockLabel_);
    infoBar->addSpacing(12);
    infoBar->addWidget(speedLabel_);
    infoBar->addWidget(speed);
    infoBar->addSpacing(12);
    infoBar->addWidget(statusLabel_, 1);
    root->addLayout(infoBar);

    auto* policy = new QLabel(
        "Road-change policy: an ambulance finishes its current edge, then detects the graph revision at the next node and reroutes before entering another edge. If no route remains, reservations are safely released.", this);
    policy->setObjectName("algorithmCaveat");
    policy->setWordWrap(true);
    root->addWidget(policy);

    timer_->setInterval(500);
    connect(timer_, &QTimer::timeout, this, [this] { stepJourney(); });
    connect(dispatchButton, &QPushButton::clicked, this, [this] { dispatchNext(); });
    connect(playButton, &QPushButton::clicked, this, [this] {
        if (activeRequestCombo_->count() > 0) timer_->start();
    });
    connect(pauseButton, &QPushButton::clicked, timer_, &QTimer::stop);
    connect(stepButton, &QPushButton::clicked, this, [this] { stepJourney(); });
    connect(clockButton, &QPushButton::clicked, this, [this] {
        repository_.dispatchSimulation().advanceClock(1.0);
        refreshAll(false);
        statusLabel_->setText("Simulation clock advanced manually by one minute.");
    });
    connect(saveButton, &QPushButton::clicked, this, [this] { saveSimulation(); });
    connect(loadButton, &QPushButton::clicked, this, [this] { loadSimulation(); });
    connect(speed, &QSlider::valueChanged, this, [this](int value) {
        speedLabel_->setText(QString("Speed: %1×").arg(value / 5.0, 0, 'f', 1));
        timer_->setInterval(1050 - value * 90);
    });
    refreshAll();
}

void EmergencyDispatchPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (observedGraphRevision_ != repository_.city().revision()) {
        refreshAll();
        statusLabel_->setText("Travel graph changed. Active journeys will reroute at their current node on the next step.");
    } else {
        refreshAll(false);
    }
}

void EmergencyDispatchPage::hideEvent(QHideEvent* event)
{
    timer_->stop();
    QWidget::hideEvent(event);
}

int EmergencyDispatchPage::selectedId(const QTableWidget* table) const
{
    const int row = table->currentRow();
    return row < 0 || !table->item(row, 0) ? -1 : table->item(row, 0)->data(Qt::UserRole).toInt();
}

void EmergencyDispatchPage::refreshAll(bool rebuild)
{
    refreshTables();
    refreshActiveRequests();
    clockLabel_->setText(QString("Clock: %1 min")
        .arg(repository_.dispatchSimulation().clockMinutes(), 0, 'f', 2));
    if (rebuild || observedGraphRevision_ != repository_.city().revision()) rebuildMap();
    observedGraphRevision_ = repository_.city().revision();
}

void EmergencyDispatchPage::refreshTables()
{
    const auto& simulation = repository_.dispatchSimulation();
    const auto& graph = repository_.city();
    ambulanceTable_->setRowCount(0);
    std::vector<int> ambulanceIds;
    for (const auto& entry : simulation.ambulances()) ambulanceIds.push_back(entry.first);
    std::sort(ambulanceIds.begin(), ambulanceIds.end());
    for (const int id : ambulanceIds) {
        const auto& ambulance = simulation.ambulances().at(id);
        const int row = ambulanceTable_->rowCount();
        ambulanceTable_->insertRow(row);
        ambulanceTable_->setItem(row, 0, new QTableWidgetItem(QString("A%1").arg(id)));
        ambulanceTable_->item(row, 0)->setData(Qt::UserRole, id);
        ambulanceTable_->setItem(row, 1, new QTableWidgetItem(locationText(graph, ambulance.currentLocationId)));
        const QString availability = ambulance.assignedRequestId >= 0
            ? "Reserved / moving" : (ambulance.available ? "Available" : "Unavailable");
        ambulanceTable_->setItem(row, 2, new QTableWidgetItem(availability));
        ambulanceTable_->setItem(row, 3, new QTableWidgetItem(
            ambulance.assignedRequestId < 0 ? "—" : QString("Q%1").arg(ambulance.assignedRequestId)));
    }
    hospitalTable_->setRowCount(0);
    std::vector<int> hospitalIds;
    for (const auto& entry : simulation.hospitals()) hospitalIds.push_back(entry.first);
    std::sort(hospitalIds.begin(), hospitalIds.end());
    for (const int id : hospitalIds) {
        const auto& hospital = simulation.hospitals().at(id);
        const int row = hospitalTable_->rowCount();
        hospitalTable_->insertRow(row);
        hospitalTable_->setItem(row, 0, new QTableWidgetItem(locationText(graph, id)));
        hospitalTable_->item(row, 0)->setData(Qt::UserRole, id);
        hospitalTable_->setItem(row, 1, new QTableWidgetItem(QString::number(hospital.availableBeds)));
        hospitalTable_->setItem(row, 2, new QTableWidgetItem(servicesText(hospital)));
    }
    requestTable_->setRowCount(0);
    std::vector<int> requestIds;
    for (const auto& entry : simulation.requests()) requestIds.push_back(entry.first);
    std::sort(requestIds.begin(), requestIds.end());
    for (const int id : requestIds) {
        const auto& request = simulation.requests().at(id);
        const int row = requestTable_->rowCount();
        requestTable_->insertRow(row);
        const QStringList values = {
            QString("Q%1").arg(id), QLatin1String(smartcity::core::priorityName(request.priority)),
            QString::number(request.creationTimeMinutes, 'f', 2), locationText(graph, request.locationId),
            QLatin1String(smartcity::core::emergencyServiceName(request.requiredService)),
            QLatin1String(smartcity::core::requestStatusName(request.status)),
            request.assignedAmbulanceId < 0 ? "—" : QString("A%1").arg(request.assignedAmbulanceId),
            request.assignedHospitalLocationId < 0 ? "—" : QString("L%1").arg(request.assignedHospitalLocationId),
            request.responseTimeMinutes < 0.0 ? "—" : QString::number(request.responseTimeMinutes, 'f', 2),
            request.transportTimeMinutes < 0.0 ? "—" : QString::number(request.transportTimeMinutes, 'f', 2)};
        for (int column = 0; column < values.size(); ++column)
            requestTable_->setItem(row, column, new QTableWidgetItem(values[column]));
        requestTable_->item(row, 0)->setData(Qt::UserRole, id);
        requestTable_->item(row, 5)->setToolTip(QString::fromStdString(request.note));
    }
    for (auto* table : {ambulanceTable_, hospitalTable_, requestTable_}) table->resizeColumnsToContents();
}

void EmergencyDispatchPage::refreshActiveRequests()
{
    const int oldId = activeRequestCombo_->currentData().toInt();
    activeRequestCombo_->clear();
    for (const int id : repository_.dispatchSimulation().activeRequestIds())
        activeRequestCombo_->addItem(QString("Q%1").arg(id), id);
    const int oldIndex = activeRequestCombo_->findData(oldId);
    if (oldIndex >= 0) activeRequestCombo_->setCurrentIndex(oldIndex);
    if (activeRequestCombo_->count() == 0) timer_->stop();
}

void EmergencyDispatchPage::rebuildMap()
{
    scene_->clear(); roadItems_.clear(); nodeItems_.clear();
    const auto& graph = repository_.city();
    for (const auto& entry : graph.roads()) {
        const auto& road = entry.second;
        const auto* from = graph.location(road.fromLocationId);
        const auto* to = graph.location(road.toLocationId);
        if (!from || !to) continue;
        auto* line = scene_->addLine(from->x, from->y, to->x, to->y,
            QPen(road.closed ? QColor("#ef4444") : QColor("#cbd5e1"), 4.0,
                 road.closed ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap));
        line->setZValue(0); roadItems_.emplace(road.id, line);
        auto* label = scene_->addSimpleText(QString("R%1 · %2 min")
            .arg(road.id).arg(road.travelTime, 0, 'f', 1));
        label->setBrush(QBrush(QColor("#64748b")));
        label->setPos((from->x + to->x) / 2.0 + 4.0, (from->y + to->y) / 2.0 - 18.0);
        label->setFlag(QGraphicsItem::ItemIgnoresTransformations);
    }
    for (const auto& entry : graph.locations()) {
        const auto& location = entry.second;
        auto* node = scene_->addEllipse(location.x - 10.0, location.y - 10.0, 20.0, 20.0,
            QPen(Qt::white, 3.0), QBrush(location.category == smartcity::core::LocationCategory::Hospital
                ? QColor("#16a34a") : QColor("#475569")));
        node->setZValue(2); nodeItems_.emplace(location.id, node);
        auto* label = scene_->addSimpleText(QString("%1 [L%2]")
            .arg(QString::fromStdString(location.name)).arg(location.id));
        label->setBrush(QBrush(QColor("#0f172a")));
        label->setPos(location.x + 13.0, location.y - 14.0);
        label->setFlag(QGraphicsItem::ItemIgnoresTransformations); label->setZValue(3);
    }
    std::unordered_map<int, int> offsetAtLocation;
    for (const auto& entry : repository_.dispatchSimulation().ambulances()) {
        const auto& ambulance = entry.second;
        const auto* location = graph.location(ambulance.currentLocationId);
        if (!location) continue;
        const int offset = offsetAtLocation[ambulance.currentLocationId]++ * 13;
        const QColor ambulanceColor = ambulance.assignedRequestId >= 0
            ? QColor("#f59e0b") : (ambulance.available ? QColor("#0ea5e9") : QColor("#94a3b8"));
        auto* marker = scene_->addEllipse(location->x - 18.0 + offset, location->y + 13.0,
            13.0, 13.0, QPen(Qt::white, 2.0),
            QBrush(ambulanceColor));
        marker->setZValue(4);
        auto* label = scene_->addSimpleText(QString("A%1").arg(ambulance.id));
        label->setBrush(QBrush(QColor("#0369a1")));
        label->setPos(location->x - 20.0 + offset, location->y + 27.0);
        label->setFlag(QGraphicsItem::ItemIgnoresTransformations); label->setZValue(5);
    }
    if (!scene_->items().isEmpty())
        view_->fitInView(scene_->itemsBoundingRect().adjusted(-45, -45, 45, 45), Qt::KeepAspectRatio);
}

void EmergencyDispatchPage::addAmbulance()
{
    if (repository_.city().vertexCount() == 0) return;
    QDialog dialog(this); dialog.setWindowTitle("Add simulated ambulance");
    auto* form = new QFormLayout(&dialog);
    auto* location = new QComboBox(&dialog); populateLocations(location, repository_.city());
    auto* available = new QCheckBox("Available for dispatch", &dialog); available->setChecked(true);
    form->addRow("Current location", location);
    form->addRow(available);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    auto& simulation = repository_.dispatchSimulation();
    if (!simulation.addAmbulance({simulation.nextAmbulanceId(), location->currentData().toInt(), available->isChecked(), -1},
                                 repository_.city()))
        QMessageBox::warning(this, "Invalid ambulance", "The ambulance could not be added.");
    refreshAll();
}

void EmergencyDispatchPage::editAmbulance()
{
    const int id = selectedId(ambulanceTable_);
    const auto found = repository_.dispatchSimulation().ambulances().find(id);
    if (found == repository_.dispatchSimulation().ambulances().end()) {
        QMessageBox::information(this, "Select an ambulance", "Select an ambulance row first."); return;
    }
    if (found->second.assignedRequestId >= 0) {
        QMessageBox::information(this, "Ambulance reserved", "A moving or reserved ambulance cannot be edited."); return;
    }
    QDialog dialog(this); dialog.setWindowTitle(QString("Edit ambulance A%1").arg(id));
    auto* form = new QFormLayout(&dialog);
    auto* location = new QComboBox(&dialog); populateLocations(location, repository_.city());
    location->setCurrentIndex(location->findData(found->second.currentLocationId));
    auto* available = new QCheckBox("Available for dispatch", &dialog);
    available->setChecked(found->second.available);
    form->addRow("Current location", location);
    form->addRow(available);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    repository_.dispatchSimulation().updateAmbulance(
        {id, location->currentData().toInt(), available->isChecked(), -1}, repository_.city());
    refreshAll();
}

void EmergencyDispatchPage::deleteAmbulance()
{
    const int id = selectedId(ambulanceTable_);
    if (!repository_.dispatchSimulation().removeAmbulance(id)) {
        QMessageBox::information(this, "Cannot delete", "Select an unassigned ambulance. Reserved ambulances cannot be deleted.");
        return;
    }
    refreshAll();
}

void EmergencyDispatchPage::addHospital()
{
    QDialog dialog(this); dialog.setWindowTitle("Add simulated hospital resources");
    auto* form = new QFormLayout(&dialog);
    auto* location = new QComboBox(&dialog); populateLocations(location, repository_.city(), true);
    if (location->count() == 0) {
        QMessageBox::information(this, "No hospital locations", "Add a city location with the hospital category first."); return;
    }
    auto* beds = new QSpinBox(&dialog); beds->setRange(0, 100000); beds->setValue(5);
    std::vector<QCheckBox*> checks;
    for (const auto service : {EmergencyService::General, EmergencyService::Trauma,
                               EmergencyService::Cardiac, EmergencyService::Pediatric}) {
        auto* check = new QCheckBox(QLatin1String(smartcity::core::emergencyServiceName(service)), &dialog);
        check->setProperty("service", static_cast<int>(service)); check->setChecked(true); checks.push_back(check);
        form->addRow(check);
    }
    form->insertRow(0, "Hospital location", location); form->insertRow(1, "Available simulated beds", beds);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    smartcity::core::HospitalResource hospital{location->currentData().toInt(), beds->value(), {}};
    for (auto* check : checks) if (check->isChecked())
        hospital.supportedServices.push_back(static_cast<EmergencyService>(check->property("service").toInt()));
    if (!repository_.dispatchSimulation().addHospital(hospital, repository_.city()))
        QMessageBox::warning(this, "Invalid hospital", "Choose an unused hospital location and at least one service.");
    refreshAll();
}

void EmergencyDispatchPage::editHospital()
{
    const int id = selectedId(hospitalTable_);
    const auto found = repository_.dispatchSimulation().hospitals().find(id);
    if (found == repository_.dispatchSimulation().hospitals().end()) {
        QMessageBox::information(this, "Select a hospital", "Select a hospital row first."); return;
    }
    QDialog dialog(this); dialog.setWindowTitle(QString("Edit resources at L%1").arg(id));
    auto* form = new QFormLayout(&dialog);
    form->addRow("Hospital location", new QLabel(locationText(repository_.city(), id), &dialog));
    auto* beds = new QSpinBox(&dialog); beds->setRange(0, 100000); beds->setValue(found->second.availableBeds);
    form->addRow("Available simulated beds", beds);
    std::vector<QCheckBox*> checks;
    for (const auto service : {EmergencyService::General, EmergencyService::Trauma,
                               EmergencyService::Cardiac, EmergencyService::Pediatric}) {
        auto* check = new QCheckBox(QLatin1String(smartcity::core::emergencyServiceName(service)), &dialog);
        check->setProperty("service", static_cast<int>(service)); check->setChecked(found->second.supports(service));
        checks.push_back(check); form->addRow(check);
    }
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    smartcity::core::HospitalResource changed{id, beds->value(), {}};
    for (auto* check : checks) if (check->isChecked())
        changed.supportedServices.push_back(static_cast<EmergencyService>(check->property("service").toInt()));
    if (!repository_.dispatchSimulation().updateHospital(changed, repository_.city()))
        QMessageBox::warning(this, "Cannot edit hospital", "Reserved hospitals cannot be edited; at least one service is required.");
    refreshAll();
}

void EmergencyDispatchPage::deleteHospital()
{
    const int id = selectedId(hospitalTable_);
    if (!repository_.dispatchSimulation().removeHospital(id)) {
        QMessageBox::information(this, "Cannot delete", "Select an unreserved hospital resource row."); return;
    }
    refreshAll();
}

void EmergencyDispatchPage::createRequest()
{
    if (repository_.city().vertexCount() == 0) return;
    QDialog dialog(this); dialog.setWindowTitle("Create simulated emergency request");
    auto* form = new QFormLayout(&dialog);
    auto* location = new QComboBox(&dialog); populateLocations(location, repository_.city());
    auto* priority = new QComboBox(&dialog);
    priority->addItem("Critical", static_cast<int>(SimulatedPriority::Critical));
    priority->addItem("Medium", static_cast<int>(SimulatedPriority::Medium));
    priority->addItem("Low", static_cast<int>(SimulatedPriority::Low));
    auto* service = new QComboBox(&dialog); populateServices(service);
    form->addRow("Patient location", location); form->addRow("Simulated priority", priority);
    form->addRow("Required service", service);
    form->addRow("Creation time", new QLabel(QString("%1 simulation minutes")
        .arg(repository_.dispatchSimulation().clockMinutes(), 0, 'f', 2), &dialog));
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    auto& simulation = repository_.dispatchSimulation();
    smartcity::core::EmergencyRequest request;
    request.id = simulation.nextRequestId(); request.locationId = location->currentData().toInt();
    request.priority = static_cast<SimulatedPriority>(priority->currentData().toInt());
    request.creationTimeMinutes = simulation.clockMinutes();
    request.requiredService = static_cast<EmergencyService>(service->currentData().toInt());
    if (!simulation.addRequest(request, repository_.city()))
        QMessageBox::warning(this, "Invalid request", "The simulated request could not be created.");
    refreshAll(false);
}

void EmergencyDispatchPage::cancelRequest()
{
    const int id = selectedId(requestTable_);
    if (!repository_.dispatchSimulation().cancelRequest(id)) {
        QMessageBox::information(this, "Cannot cancel", "Select a queued or active request."); return;
    }
    refreshAll(); statusLabel_->setText(QString("Q%1 cancelled; any reservations were released.").arg(id));
}

void EmergencyDispatchPage::deleteRequestRecord()
{
    const int id = selectedId(requestTable_);
    if (!repository_.dispatchSimulation().removeRequest(id)) {
        QMessageBox::information(this, "Cannot delete record",
            "Only completed, cancelled, or failed request records can be deleted.");
        return;
    }
    refreshAll(false);
    statusLabel_->setText(QString("Q%1 history record deleted.").arg(id));
}

void EmergencyDispatchPage::dispatchNext()
{
    const auto result = repository_.dispatchSimulation().dispatchNext(repository_.city());
    refreshAll();
    if (result.code == smartcity::services::DispatchResultCode::Assigned) {
        statusLabel_->setText(QString("Q%1 → A%2, hospital L%3. Planned legs: %4 + %5 minutes.")
            .arg(result.requestId).arg(result.ambulanceId).arg(result.hospitalLocationId)
            .arg(result.ambulanceTravelMinutes, 0, 'f', 2).arg(result.hospitalTravelMinutes, 0, 'f', 2));
    } else statusLabel_->setText(QString::fromStdString(result.message));
}

void EmergencyDispatchPage::stepJourney()
{
    if (activeRequestCombo_->count() == 0) {
        timer_->stop(); statusLabel_->setText("There is no active journey to animate."); return;
    }
    const int requestId = activeRequestCombo_->currentData().toInt();
    const auto result = repository_.dispatchSimulation().advanceOneNode(repository_.city(), requestId);
    showStep(result);
}

void EmergencyDispatchPage::showStep(const smartcity::services::SimulationStepResult& result)
{
    refreshAll();
    if (result.roadId >= 0) {
        const auto line = roadItems_.find(result.roadId);
        const auto request = repository_.dispatchSimulation().requests().find(result.requestId);
        const bool transportLeg = result.code == smartcity::services::SimulationStepCode::HospitalReached
            || (result.code == smartcity::services::SimulationStepCode::Moved
                && request != repository_.dispatchSimulation().requests().end()
                && request->second.status == RequestStatus::ToHospital);
        if (line != roadItems_.end()) line->second->setPen(QPen(
            transportLeg ? QColor("#16a34a") : QColor("#2563eb"),
            7.0, Qt::SolidLine, Qt::RoundCap));
    }
    statusLabel_->setText(QString("Q%1: %2%3")
        .arg(result.requestId).arg(QString::fromStdString(result.message))
        .arg(result.elapsedMinutes > 0.0
            ? QString(" (+%1 min)").arg(result.elapsedMinutes, 0, 'f', 2) : QString()));
}

void EmergencyDispatchPage::saveSimulation()
{
    const QString fileName = QFileDialog::getSaveFileName(
        this, "Save dispatch simulation", "smartcity-dispatch.json", "JSON files (*.json)");
    if (fileName.isEmpty()) return;
    QString error;
    if (!smartcity::services::DispatchJsonSerializer::save(
            fileName, repository_.dispatchSimulation(), &error))
        QMessageBox::critical(this, "Save failed", error);
    else statusLabel_->setText("Simulation saved to " + fileName);
}

void EmergencyDispatchPage::loadSimulation()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this, "Load dispatch simulation", {}, "JSON files (*.json)");
    if (fileName.isEmpty()) return;
    timer_->stop();
    QString error;
    if (!smartcity::services::DispatchJsonSerializer::load(
            fileName, repository_.city(), repository_.dispatchSimulation(), &error)) {
        QMessageBox::critical(this, "Load failed", error + "\n\nThe current simulation was not changed.");
        return;
    }
    refreshAll(); statusLabel_->setText("Simulation loaded. Active journeys will rebuild their route on the next step.");
}
