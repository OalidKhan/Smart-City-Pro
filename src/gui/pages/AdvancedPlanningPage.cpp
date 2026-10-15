#include "gui/pages/AdvancedPlanningPage.h"

#include "algorithms/NumberTheoryAlgorithms.h"
#include "algorithms/WeightedGraph.h"
#include "core/models/Road.h"
#include "services/CityRepository.h"

#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QFrame>
#include <QFutureWatcher>
#include <QGraphicsEllipseItem>
#include <QGraphicsItem>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QShowEvent>
#include <QSplitter>
#include <QStringList>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <algorithm>
#include <vector>

namespace {
using smartcity::algorithms::DeliveryTourResult;
using smartcity::algorithms::DeliveryTourStatus;

QString locationLabel(const smartcity::algorithms::WeightedGraph& graph, int id)
{
    const auto* location = graph.location(id);
    return location ? QString("L%1 — %2").arg(id).arg(QString::fromStdString(location->name))
                    : QString("L%1").arg(id);
}
}

AdvancedPlanningPage::AdvancedPlanningPage(smartcity::services::CityRepository& repository,
                                           QWidget* parent)
    : QWidget(parent), repository_(repository), intervalsEdit_(new QLineEdit(this)),
      maintenanceResultLabel_(new QLabel(this)), maintenanceSteps_(new QTextEdit(this)),
      depotCombo_(new QComboBox(this)), stopsList_(new QListWidget(this)),
      computeButton_(new QPushButton("Compute minimum tour", this)),
      progressBar_(new QProgressBar(this)), tourStatusLabel_(new QLabel(this)),
      tourOrderLabel_(new QLabel(this)), scene_(new QGraphicsScene(this)),
      view_(new QGraphicsView(scene_, this)),
      watcher_(new QFutureWatcher<DeliveryTourResult>(this))
{
    auto* root = new QVBoxLayout(this); root->setContentsMargins(24, 20, 24, 20); root->setSpacing(9);
    auto* title = new QLabel("Number Theory and Advanced Delivery Planning", this); title->setObjectName("pageTitle");
    auto* description = new QLabel(
        "Plan coincident maintenance cycles and solve a small exact multi-stop delivery tour over the live directed road graph.", this);
    description->setObjectName("pageDescription"); description->setWordWrap(true);
    root->addWidget(title); root->addWidget(description);

    auto* tabs = new QTabWidget(this);
    auto* maintenance = new QWidget(tabs); auto* maintenanceLayout = new QVBoxLayout(maintenance);
    auto* maintenancePanel = new QFrame(maintenance); maintenancePanel->setObjectName("routePanel");
    auto* maintenanceGrid = new QGridLayout(maintenancePanel);
    intervalsEdit_->setPlaceholderText("Example: 6, 8, 12"); intervalsEdit_->setText("6, 8, 12");
    auto* calculateButton = new QPushButton("Calculate cycles", maintenancePanel); calculateButton->setObjectName("primaryButton");
    maintenanceGrid->addWidget(new QLabel("Positive integer intervals", maintenancePanel), 0, 0);
    maintenanceGrid->addWidget(intervalsEdit_, 0, 1); maintenanceGrid->addWidget(calculateButton, 0, 2);
    auto* assumption = new QLabel(
        "Assumption: every maintenance cycle begins at the same simulation time. The LCM is the first positive offset when all cycles coincide again.", maintenancePanel);
    assumption->setObjectName("algorithmCaveat"); assumption->setWordWrap(true);
    maintenanceGrid->addWidget(assumption, 1, 0, 1, 3);
    maintenanceResultLabel_->setObjectName("routeResult"); maintenanceResultLabel_->setWordWrap(true);
    maintenanceSteps_->setReadOnly(true); maintenanceSteps_->setPlaceholderText("Euclid and LCM calculation steps appear here.");
    maintenanceLayout->addWidget(maintenancePanel); maintenanceLayout->addWidget(maintenanceResultLabel_);
    maintenanceLayout->addWidget(maintenanceSteps_, 1);
    tabs->addTab(maintenance, "Maintenance Cycles");

    auto* tour = new QWidget(tabs); auto* tourLayout = new QVBoxLayout(tour);
    auto* tourControls = new QFrame(tour); tourControls->setObjectName("routePanel");
    auto* tourGrid = new QGridLayout(tourControls);
    progressBar_->setRange(0, 0); progressBar_->setMaximumWidth(120); progressBar_->hide();
    computeButton_->setObjectName("primaryButton");
    tourGrid->addWidget(new QLabel("Depot", tourControls), 0, 0); tourGrid->addWidget(depotCombo_, 0, 1);
    tourGrid->addWidget(new QLabel("Delivery stops (maximum 12)", tourControls), 1, 0);
    tourGrid->addWidget(stopsList_, 1, 1, 2, 2); tourGrid->addWidget(computeButton_, 0, 2); tourGrid->addWidget(progressBar_, 0, 3);
    auto* transitNote = new QLabel(
        "The service order is optimized by travel time. Passing through another selected delivery location while travelling between two scheduled stops does not automatically service it; only arrival as the current DP stop counts.", tourControls);
    transitNote->setObjectName("algorithmCaveat"); transitNote->setWordWrap(true); tourGrid->addWidget(transitNote, 3, 0, 1, 4);
    auto* complexity = new QLabel(
        "Held–Karp DP: O(k² × 2^k) time and O(k × 2^k) space for k delivery stops, plus pairwise Dijkstra route construction.", tourControls);
    complexity->setObjectName("mapSummary"); complexity->setWordWrap(true); tourGrid->addWidget(complexity, 4, 0, 1, 4);
    tourLayout->addWidget(tourControls);

    auto* splitter = new QSplitter(Qt::Horizontal, tour);
    auto* resultPanel = new QFrame(splitter); resultPanel->setObjectName("routePanel"); resultPanel->setMinimumWidth(285);
    auto* resultLayout = new QVBoxLayout(resultPanel);
    auto* resultTitle = new QLabel("Exact tour result", resultPanel); resultTitle->setObjectName("sectionTitle");
    tourStatusLabel_->setObjectName("routeResult"); tourStatusLabel_->setWordWrap(true);
    tourOrderLabel_->setObjectName("routeResult"); tourOrderLabel_->setWordWrap(true);
    resultLayout->addWidget(resultTitle); resultLayout->addWidget(tourStatusLabel_); resultLayout->addWidget(tourOrderLabel_); resultLayout->addStretch();
    view_->setObjectName("advancedPlanningView"); view_->setRenderHint(QPainter::Antialiasing); view_->setMinimumHeight(300);
    splitter->addWidget(resultPanel); splitter->addWidget(view_); splitter->setStretchFactor(0, 1); splitter->setStretchFactor(1, 3);
    tourLayout->addWidget(splitter, 1); tabs->addTab(tour, "Multi-Stop Delivery Tour");
    root->addWidget(tabs, 1);

    connect(calculateButton, &QPushButton::clicked, this, [this] { calculateMaintenance(); });
    connect(depotCombo_, &QComboBox::currentIndexChanged, this, [this] { updateDepotChoice(); });
    connect(computeButton_, &QPushButton::clicked, this, [this] { computeTour(); });
    connect(watcher_, &QFutureWatcher<DeliveryTourResult>::finished, this, [this] { tourFinished(); });
    calculateMaintenance(); refreshGraph(true);
}

void AdvancedPlanningPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event); refreshGraph();
}

void AdvancedPlanningPage::calculateMaintenance()
{
    std::vector<std::uint64_t> intervals;
    const auto parts = intervalsEdit_->text().split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        maintenanceResultLabel_->setText("Enter at least one positive integer interval."); maintenanceSteps_->clear(); return;
    }
    for (const auto& part : parts) {
        bool ok = false;
        const bool decimalDigitsOnly = QRegularExpression("^[0-9]+$").match(part).hasMatch();
        const auto value = part.toULongLong(&ok);
        if (!decimalDigitsOnly || !ok || value == 0) {
            maintenanceResultLabel_->setText("Invalid interval: every entry must be a positive integer representable by 64 bits.");
            maintenanceSteps_->clear(); return;
        }
        intervals.push_back(value);
    }
    const auto result = smartcity::algorithms::NumberTheoryAlgorithms::maintenanceCycles(intervals);
    if (!result.validInput) { maintenanceResultLabel_->setText("Invalid maintenance intervals."); return; }
    QStringList lines;
    if (result.steps.empty())
        lines << QString("Single interval: gcd = lcm = %1.")
            .arg(static_cast<qulonglong>(intervals.front()));
    if (!result.gcdSteps.empty()) lines << "Overall GCD using Euclid's algorithm:";
    for (const auto& group : result.gcdSteps) {
        lines << QString("gcd(%1, %2):")
            .arg(static_cast<qulonglong>(group.accumulated))
            .arg(static_cast<qulonglong>(group.nextInterval));
        for (const auto& euclid : group.euclidSteps)
            lines << QString("  %1 = %2 × %3 + %4")
                .arg(static_cast<qulonglong>(euclid.dividend))
                .arg(static_cast<qulonglong>(euclid.divisor))
                .arg(static_cast<qulonglong>(euclid.dividend / euclid.divisor))
                .arg(static_cast<qulonglong>(euclid.remainder));
        lines << QString("  result = %1").arg(static_cast<qulonglong>(group.result));
    }
    if (!result.steps.empty()) lines << "Incremental LCM using division before multiplication:";
    for (std::size_t index = 0; index < result.steps.size(); ++index) {
        const auto& step = result.steps[index];
        lines << QString("Combine %1 and %2:")
            .arg(static_cast<qulonglong>(step.accumulated))
            .arg(static_cast<qulonglong>(step.nextInterval));
        for (const auto& euclid : step.euclidSteps)
            lines << QString("  %1 = %2 × %3 + %4")
                .arg(static_cast<qulonglong>(euclid.dividend))
                .arg(static_cast<qulonglong>(euclid.divisor))
                .arg(static_cast<qulonglong>(euclid.dividend / euclid.divisor))
                .arg(static_cast<qulonglong>(euclid.remainder));
        lines << QString("  gcd = %1").arg(static_cast<qulonglong>(step.gcd));
        lines << QString("  lcm = (%1 ÷ %2) × %3 = %4 × %3%5")
            .arg(static_cast<qulonglong>(step.accumulated))
            .arg(static_cast<qulonglong>(step.gcd))
            .arg(static_cast<qulonglong>(step.nextInterval))
            .arg(static_cast<qulonglong>(step.dividedBeforeMultiply))
            .arg(step.overflow ? " → overflow" : QString(" = %1").arg(static_cast<qulonglong>(step.result)));
    }
    maintenanceSteps_->setPlainText(lines.join('\n'));
    if (result.overflow) {
        maintenanceResultLabel_->setText(
            "LCM overflow detected before multiplication. No representable 64-bit coincidence time is reported.");
    } else {
        maintenanceResultLabel_->setText(QString(
            "Overall GCD: %1. The cycles next coincide %2 simulation-time unit(s) after their common start.")
            .arg(static_cast<qulonglong>(result.greatestCommonDivisor))
            .arg(static_cast<qulonglong>(result.nextCoincidence)));
    }
}

int AdvancedPlanningPage::depotId() const
{
    return depotCombo_->currentIndex() < 0 ? -1 : depotCombo_->currentData().toInt();
}

void AdvancedPlanningPage::refreshGraph(bool force)
{
    const auto& graph = repository_.city();
    if (!force && observedRevision_ == graph.revision()) return;
    const int previousDepot = depotId(); observedRevision_ = graph.revision();
    depotCombo_->clear(); stopsList_->clear();
    std::vector<int> ids; for (const auto& entry : graph.locations()) ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end());
    for (int id : ids) {
        const QString label = locationLabel(graph, id); depotCombo_->addItem(label, id);
        auto* item = new QListWidgetItem(label, stopsList_); item->setData(Qt::UserRole, id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable); item->setCheckState(Qt::Unchecked);
    }
    const int depotIndex = depotCombo_->findData(previousDepot);
    if (depotIndex >= 0) depotCombo_->setCurrentIndex(depotIndex);
    updateDepotChoice(); rebuildMap();
    tourStatusLabel_->setText(QString("Graph revision %1 loaded. Choose up to 12 delivery stops.")
        .arg(static_cast<qulonglong>(observedRevision_)));
    tourOrderLabel_->clear();
}

void AdvancedPlanningPage::updateDepotChoice()
{
    const int depot = depotId();
    for (int row = 0; row < stopsList_->count(); ++row) {
        auto* item = stopsList_->item(row); const bool isDepot = item->data(Qt::UserRole).toInt() == depot;
        if (isDepot) item->setCheckState(Qt::Unchecked);
        auto flags = item->flags();
        if (isDepot) flags &= ~Qt::ItemIsEnabled;
        else flags |= Qt::ItemIsEnabled;
        item->setFlags(flags);
    }
    rebuildMap();
}

void AdvancedPlanningPage::computeTour()
{
    refreshGraph();
    if (watcher_->isRunning()) { tourStatusLabel_->setText("A tour computation is already running."); return; }
    if (depotId() < 0) { tourStatusLabel_->setText("The city has no depot location."); return; }
    std::vector<int> stops;
    for (int row = 0; row < stopsList_->count(); ++row) {
        const auto* item = stopsList_->item(row);
        if (item->checkState() == Qt::Checked) stops.push_back(item->data(Qt::UserRole).toInt());
    }
    if (stops.size() > smartcity::algorithms::DeliveryTourPlanner::MaximumDeliveryStops) {
        QMessageBox::warning(this, "Too many stops", "Select at most 12 delivery stops for exact Held–Karp planning."); return;
    }
    const auto snapshot = repository_.city(); const int depot = depotId();
    computeButton_->setEnabled(false); progressBar_->show();
    tourStatusLabel_->setText(QString("Computing pairwise travel-time routes and Held–Karp DP from immutable graph revision %1…")
        .arg(static_cast<qulonglong>(snapshot.revision())));
    watcher_->setFuture(QtConcurrent::run([snapshot, depot, stops] {
        return smartcity::algorithms::DeliveryTourPlanner::compute(snapshot, depot, stops);
    }));
}

void AdvancedPlanningPage::tourFinished()
{
    computeButton_->setEnabled(true); progressBar_->hide();
    const auto result = watcher_->result();
    if (repository_.city().revision() != result.graphRevision) {
        tourStatusLabel_->setText("Discarded the stale tour because the city graph changed during computation.");
        rebuildMap(); return;
    }
    if (result.status == DeliveryTourStatus::TooManyStops) {
        tourStatusLabel_->setText("The exact planner accepts at most 12 delivery stops."); return;
    }
    if (result.status == DeliveryTourStatus::InvalidInput) {
        tourStatusLabel_->setText("Invalid depot or duplicate/missing delivery stop."); return;
    }
    if (result.status == DeliveryTourStatus::Unreachable) {
        QStringList unreachable; for (int id : result.unreachableStopIds) unreachable << locationLabel(repository_.city(), id);
        tourStatusLabel_->setText(unreachable.isEmpty()
            ? "No closed tour exists using open roads in their permitted direction."
            : "No closed tour exists. Unreachable from/to the depot: " + unreachable.join(", "));
        tourOrderLabel_->clear(); rebuildMap(); return;
    }
    QStringList serviceOrder; for (int id : result.serviceOrder) serviceOrder << locationLabel(repository_.city(), id);
    QStringList expanded; for (int id : result.expandedLocationIds) expanded << QString("L%1").arg(id);
    tourStatusLabel_->setText(result.status == DeliveryTourStatus::Trivial
        ? "No delivery stops selected; the depot-only tour costs 0 minutes."
        : QString("Minimum closed tour travel time: %1 minutes across %2 selected stop(s).")
            .arg(result.totalTravelTime, 0, 'f', 2).arg(result.serviceOrder.size() - 2));
    tourOrderLabel_->setText("Service order: " + serviceOrder.join(QString::fromUtf8(" → "))
        + "\nExpanded road path: " + expanded.join(QString::fromUtf8(" → ")));
    rebuildMap(&result);
}

void AdvancedPlanningPage::rebuildMap(const DeliveryTourResult* result)
{
    scene_->clear(); roadItems_.clear(); nodeItems_.clear(); const auto& graph = repository_.city();
    for (const auto& entry : graph.roads()) {
        const auto& road = entry.second; const auto* from = graph.location(road.fromLocationId); const auto* to = graph.location(road.toLocationId);
        if (!from || !to) continue;
        auto* line = scene_->addLine(from->x, from->y, to->x, to->y,
            QPen(road.closed ? QColor("#ef4444") : QColor("#cbd5e1"), 4.0,
                 road.closed ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap));
        roadItems_.emplace(road.id, line);
    }
    for (const auto& entry : graph.locations()) {
        const auto& location = entry.second;
        auto* node = scene_->addEllipse(location.x - 10, location.y - 10, 20, 20,
            QPen(Qt::white, 3), QBrush(location.id == depotId() ? QColor("#16a34a") : QColor("#475569")));
        nodeItems_.emplace(location.id, node); node->setZValue(2);
        auto* label = scene_->addSimpleText(QString("%1 [L%2]%3")
            .arg(QString::fromStdString(location.name)).arg(location.id)
            .arg(location.id == depotId() ? " DEPOT" : ""));
        label->setPos(location.x + 13, location.y - 14); label->setFlag(QGraphicsItem::ItemIgnoresTransformations); label->setZValue(3);
    }
    if (result) {
        for (int roadId : result->roadIds) {
            const auto found = roadItems_.find(roadId);
            if (found != roadItems_.end()) found->second->setPen(QPen(QColor("#2563eb"), 7.0, Qt::SolidLine, Qt::RoundCap));
        }
        for (int locationId : result->expandedLocationIds) {
            const auto found = nodeItems_.find(locationId);
            if (found != nodeItems_.end()) found->second->setBrush(QBrush(QColor("#2563eb")));
        }
        const auto depot = nodeItems_.find(depotId());
        if (depot != nodeItems_.end()) depot->second->setBrush(QBrush(QColor("#16a34a")));
    }
    if (!scene_->items().isEmpty()) view_->fitInView(scene_->itemsBoundingRect().adjusted(-45,-45,45,45), Qt::KeepAspectRatio);
}
