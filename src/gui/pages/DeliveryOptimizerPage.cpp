#include "gui/pages/DeliveryOptimizerPage.h"

#include "algorithms/KnapsackAlgorithms.h"
#include "algorithms/RouteAlgorithms.h"
#include "algorithms/WeightedGraph.h"
#include "core/models/DeliveryPackage.h"
#include "core/models/Road.h"
#include "services/CityRepository.h"
#include "services/DeliveryJsonSerializer.h"

#include <QCheckBox>
#include <QAbstractItemView>
#include <QBrush>
#include <QComboBox>
#include <QFileDialog>
#include <QFrame>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QStringList>

#include <algorithm>
#include <unordered_set>

namespace {
QString locationText(const smartcity::algorithms::WeightedGraph& graph, int id)
{
    const auto* location = graph.location(id);
    return location ? QString("L%1 — %2").arg(id).arg(QString::fromStdString(location->name))
                    : QString("L%1").arg(id);
}

std::vector<smartcity::core::DeliveryPackage> sortedPackages(
    const smartcity::services::DeliveryScenario& scenario)
{
    std::vector<smartcity::core::DeliveryPackage> packages;
    for (const auto& entry : scenario.packages()) packages.push_back(entry.second);
    std::sort(packages.begin(), packages.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    return packages;
}
}

DeliveryOptimizerPage::DeliveryOptimizerPage(smartcity::services::CityRepository& repository,
                                             QWidget* parent)
    : QWidget(parent), repository_(repository), depotCombo_(new QComboBox(this)),
      capacitySpin_(new QSpinBox(this)), metricCombo_(new QComboBox(this)),
      showDpCheck_(new QCheckBox("Show DP table when small", this)),
      packageTable_(new QTableWidget(this)), dpTable_(new QTableWidget(this)),
      scene_(new QGraphicsScene(this)), view_(new QGraphicsView(scene_, this)),
      totalsLabel_(new QLabel("Run an optimizer to see totals.", this)),
      routesLabel_(new QLabel(this)), explanationLabel_(new QLabel(this))
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20); root->setSpacing(9);
    auto* title = new QLabel("Delivery Optimizer", this); title->setObjectName("pageTitle");
    auto* description = new QLabel(
        "Choose indivisible packages with 0/1 dynamic programming, inspect a separate divisible-cargo demonstration, and calculate independent depot routes.", this);
    description->setObjectName("pageDescription"); description->setWordWrap(true);
    root->addWidget(title); root->addWidget(description);

    auto* controls = new QFrame(this); controls->setObjectName("routePanel");
    auto* grid = new QGridLayout(controls);
    grid->addWidget(new QLabel("Depot", controls), 0, 0); grid->addWidget(depotCombo_, 0, 1);
    capacitySpin_->setRange(1, 100000); capacitySpin_->setValue(7);
    grid->addWidget(new QLabel("Vehicle capacity", controls), 0, 2); grid->addWidget(capacitySpin_, 0, 3);
    metricCombo_->addItem("Routes by distance", static_cast<int>(smartcity::algorithms::RouteMetric::Distance));
    metricCombo_->addItem("Routes by travel time", static_cast<int>(smartcity::algorithms::RouteMetric::TravelTime));
    grid->addWidget(metricCombo_, 0, 4);
    auto* apply = new QPushButton("Apply settings", controls); apply->setObjectName("editorButton"); grid->addWidget(apply, 0, 5);
    auto* optimize = new QPushButton("Optimize 0/1 packages", controls); optimize->setObjectName("primaryButton");
    auto* fractional = new QPushButton("Fractional cargo demo", controls); fractional->setObjectName("editorButton");
    grid->addWidget(optimize, 1, 0, 1, 2); grid->addWidget(fractional, 1, 2, 1, 2); grid->addWidget(showDpCheck_, 1, 4, 1, 2);
    root->addWidget(controls);

    auto* content = new QGridLayout;
    packageTable_->setObjectName("deliveryTable"); packageTable_->setColumnCount(6);
    packageTable_->setHorizontalHeaderLabels({"ID", "Destination", "Weight", "Value", "Priority", "Result"});
    packageTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    packageTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    packageTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    packageTable_->setAlternatingRowColors(true);
    content->addWidget(packageTable_, 0, 0);
    view_->setObjectName("deliveryView"); view_->setRenderHint(QPainter::Antialiasing); view_->setMinimumHeight(260);
    content->addWidget(view_, 0, 1);
    content->setColumnStretch(0, 1); content->setColumnStretch(1, 1);
    root->addLayout(content, 1);

    auto* actions = new QHBoxLayout;
    const QStringList actionLabels{"Add package", "Edit package", "Delete package", "Save delivery JSON", "Load delivery JSON"};
    for (const auto& label : actionLabels) {
        auto* button = new QPushButton(label, this); button->setObjectName("editorButton"); actions->addWidget(button);
        if (label == QString("Add package")) connect(button, &QPushButton::clicked, this, [this] { addPackage(); });
        else if (label == QString("Edit package")) connect(button, &QPushButton::clicked, this, [this] { editPackage(); });
        else if (label == QString("Delete package")) connect(button, &QPushButton::clicked, this, [this] { deletePackage(); });
        else if (label == QString("Save delivery JSON")) connect(button, &QPushButton::clicked, this, [this] { saveScenario(); });
        else connect(button, &QPushButton::clicked, this, [this] { loadScenario(); });
    }
    actions->addStretch(); root->addLayout(actions);
    totalsLabel_->setObjectName("routeResult"); totalsLabel_->setWordWrap(true);
    routesLabel_->setObjectName("routeResult"); routesLabel_->setWordWrap(true);
    explanationLabel_->setObjectName("algorithmCaveat"); explanationLabel_->setWordWrap(true);
    explanationLabel_->setText("0/1 Knapsack uses O(nW) time and O(nW) memory here; W is numeric capacity, so this is pseudo-polynomial. Greedy value/weight selection can miss the optimal indivisible set. Fractional Knapsack is a separate demonstration for divisible cargo.");
    root->addWidget(totalsLabel_); root->addWidget(routesLabel_); root->addWidget(explanationLabel_);
    dpTable_->setObjectName("deliveryTable"); dpTable_->setVisible(false); dpTable_->setMaximumHeight(210);
    root->addWidget(dpTable_);

    connect(apply, &QPushButton::clicked, this, [this] { applySettings(); });
    connect(optimize, &QPushButton::clicked, this, [this] { runZeroOne(); });
    connect(fractional, &QPushButton::clicked, this, [this] { runFractional(); });
    connect(showDpCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        dpTable_->setVisible(checked && dpTable_->rowCount() > 0);
    });
    refreshFromModels(true);
}

void DeliveryOptimizerPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event); refreshFromModels();
}

void DeliveryOptimizerPage::refreshFromModels(bool force)
{
    const auto& graph = repository_.city();
    if (!force && observedGraphRevision_ == graph.revision()) { refreshPackages(); return; }
    const int oldDepot = repository_.deliveryScenario().depotLocationId();
    depotCombo_->clear();
    std::vector<int> ids; for (const auto& entry : graph.locations()) ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end());
    for (int id : ids) depotCombo_->addItem(locationText(graph, id), id);
    const int depotIndex = depotCombo_->findData(oldDepot);
    if (depotIndex >= 0) depotCombo_->setCurrentIndex(depotIndex);
    capacitySpin_->setValue(repository_.deliveryScenario().vehicleCapacity());
    observedGraphRevision_ = graph.revision(); refreshPackages(); rebuildMap();
}

void DeliveryOptimizerPage::refreshPackages()
{
    const auto packages = sortedPackages(repository_.deliveryScenario());
    packageTable_->setRowCount(static_cast<int>(packages.size()));
    for (int row = 0; row < static_cast<int>(packages.size()); ++row) {
        const auto& p = packages[static_cast<std::size_t>(row)];
        const QStringList values{QString::number(p.id), locationText(repository_.city(), p.destinationLocationId),
                                 QString::number(p.weight), QString::number(p.value, 'f', 2),
                                 QLatin1String(smartcity::core::packagePriorityName(p.priority)), "Not evaluated"};
        for (int column = 0; column < values.size(); ++column)
            packageTable_->setItem(row, column, new QTableWidgetItem(values.at(column)));
    }
}

void DeliveryOptimizerPage::rebuildMap(const std::vector<int>& highlightedRoads)
{
    scene_->clear(); const auto& graph = repository_.city();
    const std::unordered_set<int> highlights(highlightedRoads.begin(), highlightedRoads.end());
    for (const auto& entry : graph.roads()) {
        const auto& road = entry.second; const auto* a = graph.location(road.fromLocationId); const auto* b = graph.location(road.toLocationId);
        if (!a || !b) continue;
        const bool highlighted = highlights.count(road.id) != 0;
        scene_->addLine(a->x, a->y, b->x, b->y, QPen(highlighted ? QColor("#2563eb") : (road.closed ? QColor("#ef4444") : QColor("#cbd5e1")), highlighted ? 7.0 : 3.0, road.closed && !highlighted ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap));
    }
    const int depot = repository_.deliveryScenario().depotLocationId();
    for (const auto& entry : graph.locations()) {
        const auto& location = entry.second; const bool isDepot = location.id == depot;
        scene_->addEllipse(location.x - 9, location.y - 9, 18, 18, QPen(Qt::white, 2), QBrush(isDepot ? QColor("#16a34a") : QColor("#475569")));
        auto* label = scene_->addSimpleText(QString("%1%2").arg(QString::fromStdString(location.name), isDepot ? " [DEPOT]" : ""));
        label->setPos(location.x + 12, location.y - 14); label->setFlag(QGraphicsItem::ItemIgnoresTransformations);
    }
    if (!scene_->items().isEmpty()) view_->fitInView(scene_->itemsBoundingRect().adjusted(-35,-35,35,35), Qt::KeepAspectRatio);
}

int DeliveryOptimizerPage::selectedPackageId() const
{
    const int row = packageTable_->currentRow();
    return row < 0 || !packageTable_->item(row, 0) ? -1 : packageTable_->item(row, 0)->text().toInt();
}

void DeliveryOptimizerPage::addPackage()
{
    if (repository_.city().vertexCount() == 0) return;
    const auto& scenario = repository_.deliveryScenario(); bool ok = false;
    const int id = QInputDialog::getInt(this, "Add package", "Stable package ID", scenario.nextPackageId(), 0, 1000000, 1, &ok); if (!ok) return;
    QStringList locations; std::vector<int> ids;
    for (const auto& entry : repository_.city().locations()) ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end()); for (int location : ids) locations << locationText(repository_.city(), location);
    const QString destination = QInputDialog::getItem(this, "Add package", "Destination", locations, 0, false, &ok); if (!ok) return;
    const int weight = QInputDialog::getInt(this, "Add package", "Positive integer weight", 1, 1, 100000, 1, &ok); if (!ok) return;
    const double value = QInputDialog::getDouble(this, "Add package", "Nonnegative value", 1.0, 0.0, 1e12, 2, &ok); if (!ok) return;
    const QStringList priorities{"Low", "Medium", "High"}; const QString priorityText = QInputDialog::getItem(this, "Add package", "Priority", priorities, 1, false, &ok); if (!ok) return;
    smartcity::core::PackagePriority priority; smartcity::core::packagePriorityFromName(priorityText.toStdString().c_str(), priority);
    std::string error;
    if (!repository_.deliveryScenario().addPackage({id, ids.at(static_cast<std::size_t>(locations.indexOf(destination))), weight, value, priority}, repository_.city(), &error))
        QMessageBox::warning(this, "Package not added", QString::fromStdString(error));
    refreshPackages();
}

void DeliveryOptimizerPage::editPackage()
{
    const int id = selectedPackageId(); auto& scenario = repository_.deliveryScenario();
    const auto it = scenario.packages().find(id); if (it == scenario.packages().end()) { QMessageBox::information(this, "Edit package", "Select a package row first."); return; }
    auto package = it->second; bool ok = false;
    QStringList locations; std::vector<int> ids;
    for (const auto& entry : repository_.city().locations()) ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end());
    int currentDestinationIndex = 0;
    for (int location : ids) {
        if (location == package.destinationLocationId) currentDestinationIndex = locations.size();
        locations << locationText(repository_.city(), location);
    }
    const QString destination = QInputDialog::getItem(this, "Edit package", "Destination", locations, currentDestinationIndex, false, &ok); if (!ok) return;
    package.destinationLocationId = ids.at(static_cast<std::size_t>(locations.indexOf(destination)));
    package.weight = QInputDialog::getInt(this, "Edit package", "Positive integer weight", package.weight, 1, 100000, 1, &ok); if (!ok) return;
    package.value = QInputDialog::getDouble(this, "Edit package", "Nonnegative value", package.value, 0.0, 1e12, 2, &ok); if (!ok) return;
    const QStringList priorities{"Low", "Medium", "High"}; const QString priorityText = QInputDialog::getItem(this, "Edit package", "Priority", priorities, static_cast<int>(package.priority)-1, false, &ok); if (!ok) return;
    smartcity::core::packagePriorityFromName(priorityText.toStdString().c_str(), package.priority);
    std::string error; if (!scenario.updatePackage(package, repository_.city(), &error)) QMessageBox::warning(this, "Package not edited", QString::fromStdString(error));
    refreshPackages();
}

void DeliveryOptimizerPage::deletePackage()
{
    const int id = selectedPackageId(); if (id < 0) { QMessageBox::information(this, "Delete package", "Select a package row first."); return; }
    repository_.deliveryScenario().removePackage(id); refreshPackages();
}

void DeliveryOptimizerPage::applySettings()
{
    if (depotCombo_->currentIndex() < 0) return;
    std::string error; auto& scenario = repository_.deliveryScenario();
    if (!scenario.setDepotLocation(depotCombo_->currentData().toInt(), repository_.city(), &error)
        || !scenario.setVehicleCapacity(capacitySpin_->value(), &error)) {
        QMessageBox::warning(this, "Invalid settings", QString::fromStdString(error)); return;
    }
    rebuildMap(); totalsLabel_->setText("Settings applied. Run an optimizer.");
}

void DeliveryOptimizerPage::runZeroOne()
{
    applySettings(); const auto packages = sortedPackages(repository_.deliveryScenario());
    const auto result = smartcity::algorithms::KnapsackAlgorithms::solveZeroOne(packages, capacitySpin_->value());
    if (result.status == smartcity::algorithms::KnapsackStatus::TableTooLarge) {
        totalsLabel_->setText(QString("Input rejected: DP would exceed %1 cells. Reduce package count or capacity.").arg(smartcity::algorithms::KnapsackAlgorithms::MaximumDpCells)); return;
    }
    if (result.status != smartcity::algorithms::KnapsackStatus::Solved) { totalsLabel_->setText("Invalid package input."); return; }
    const std::unordered_set<int> selected(result.selectedPackageIds.begin(), result.selectedPackageIds.end());
    for (int row = 0; row < packageTable_->rowCount(); ++row) {
        const int id = packageTable_->item(row, 0)->text().toInt(); packageTable_->item(row, 5)->setText(selected.count(id) ? "Selected" : "Unselected");
    }
    totalsLabel_->setText(QString("0/1 result — selected: %1; unselected: %2; total weight: %3; total value: %4; remaining capacity: %5.")
        .arg(static_cast<qulonglong>(result.selectedPackageIds.size())).arg(static_cast<qulonglong>(result.unselectedPackageIds.size()))
        .arg(result.totalWeight).arg(result.totalValue, 0, 'f', 2).arg(result.remainingCapacity));
    std::vector<int> highlightedRoads; QStringList routes;
    const auto metric = static_cast<smartcity::algorithms::RouteMetric>(metricCombo_->currentData().toInt());
    for (int packageId : result.selectedPackageIds) {
        const auto& p = repository_.deliveryScenario().packages().at(packageId);
        const auto route = smartcity::algorithms::RouteAlgorithms::dijkstra(repository_.city(), repository_.deliveryScenario().depotLocationId(), p.destinationLocationId, metric);
        if (route.status == smartcity::algorithms::RouteStatus::Unreachable || route.status == smartcity::algorithms::RouteStatus::InvalidInput)
            routes << QString("P%1 → %2: Unreachable").arg(packageId).arg(locationText(repository_.city(), p.destinationLocationId));
        else {
            highlightedRoads.insert(highlightedRoads.end(), route.roadIds.begin(), route.roadIds.end());
            routes << QString("P%1 → %2: %3 %4").arg(packageId).arg(locationText(repository_.city(), p.destinationLocationId)).arg(route.totalWeight, 0, 'f', 2).arg(metric == smartcity::algorithms::RouteMetric::Distance ? "km" : "minutes");
        }
    }
    routesLabel_->setText("Independent depot-to-destination routes (not an optimized multi-stop tour): " + (routes.empty() ? QString("No selected packages.") : routes.join(" | ")));
    rebuildMap(highlightedRoads);
    showDpTable(result.visualizationTable, result.rowCount, result.columnCount);
}

void DeliveryOptimizerPage::runFractional()
{
    applySettings(); const auto result = smartcity::algorithms::KnapsackAlgorithms::solveFractional(sortedPackages(repository_.deliveryScenario()), capacitySpin_->value());
    if (!result.validInput) { totalsLabel_->setText("Invalid cargo input."); return; }
    for (int row = 0; row < packageTable_->rowCount(); ++row)
        packageTable_->item(row, 5)->setText("0% included");
    QStringList parts;
    for (const auto& selection : result.selections) {
        parts << QString("P%1: %2%").arg(selection.packageId).arg(selection.fraction * 100.0, 0, 'f', 1);
        for (int row = 0; row < packageTable_->rowCount(); ++row) {
            if (packageTable_->item(row, 0)->text().toInt() == selection.packageId)
                packageTable_->item(row, 5)->setText(QString("%1% included").arg(selection.fraction * 100.0, 0, 'f', 1));
        }
    }
    totalsLabel_->setText(QString("Fractional demonstration (divisible cargo only) — %1; weight: %2; value: %3; remaining: %4.")
        .arg(parts.join(", ")).arg(result.totalWeight, 0, 'f', 2).arg(result.totalValue, 0, 'f', 2).arg(result.remainingCapacity, 0, 'f', 2));
    routesLabel_->setText("No package-delivery routes are inferred from fractional cargo; this demonstration permits splitting items.");
    dpTable_->setRowCount(0); dpTable_->setVisible(false); rebuildMap();
}

void DeliveryOptimizerPage::showDpTable(const std::vector<double>& values, std::size_t rows, std::size_t columns)
{
    dpTable_->clear();
    if (values.empty()) { dpTable_->setRowCount(0); dpTable_->setVisible(false); if (showDpCheck_->isChecked()) routesLabel_->setText(routesLabel_->text() + " DP visualization hidden because it exceeds 20 packages or capacity 100."); return; }
    dpTable_->setRowCount(static_cast<int>(rows)); dpTable_->setColumnCount(static_cast<int>(columns));
    QStringList headers; for (std::size_t c=0;c<columns;++c) headers << QString::number(c); dpTable_->setHorizontalHeaderLabels(headers);
    for (std::size_t r=0;r<rows;++r) for (std::size_t c=0;c<columns;++c)
        dpTable_->setItem(static_cast<int>(r), static_cast<int>(c), new QTableWidgetItem(QString::number(values[r*columns+c], 'f', 1)));
    dpTable_->setVisible(showDpCheck_->isChecked());
}

void DeliveryOptimizerPage::saveScenario()
{
    const QString file = QFileDialog::getSaveFileName(this, "Save delivery scenario", "sample-delivery.json", "JSON files (*.json)"); if (file.isEmpty()) return;
    QString error; if (!smartcity::services::DeliveryJsonSerializer::save(file, repository_.deliveryScenario(), &error)) QMessageBox::warning(this, "Save failed", error);
    else QMessageBox::information(this, "Saved", "Delivery scenario saved.");
}

void DeliveryOptimizerPage::loadScenario()
{
    const QString file = QFileDialog::getOpenFileName(this, "Load delivery scenario", {}, "JSON files (*.json)"); if (file.isEmpty()) return;
    QString error; if (!smartcity::services::DeliveryJsonSerializer::load(file, repository_.city(), repository_.deliveryScenario(), &error)) { QMessageBox::warning(this, "Load failed", error); return; }
    refreshFromModels(true); totalsLabel_->setText("Delivery scenario loaded safely.");
}
