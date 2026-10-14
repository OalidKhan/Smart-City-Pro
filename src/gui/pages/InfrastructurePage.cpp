#include "gui/pages/InfrastructurePage.h"

#include "algorithms/AlgorithmEvent.h"
#include "algorithms/WeightedGraph.h"
#include "core/models/ConstructionCandidate.h"
#include "gui/animation/AlgorithmAnimationController.h"
#include "services/CityRepository.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QShowEvent>
#include <QSlider>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace {

using smartcity::algorithms::AlgorithmEvent;
using smartcity::algorithms::AlgorithmEventType;
using smartcity::algorithms::InfrastructureAlgorithm;

QString locationText(const smartcity::algorithms::WeightedGraph& graph, int id)
{
    const auto* location = graph.location(id);
    return location ? QString("L%1 — %2").arg(id).arg(QString::fromStdString(location->name))
                    : QString("L%1").arg(id);
}

QString eventDescription(const AlgorithmEvent& event)
{
    switch (event.type) {
    case AlgorithmEventType::Started: return "Spanning-forest algorithm started";
    case AlgorithmEventType::ComponentStarted:
        return QString("Started component at L%1").arg(event.locationId);
    case AlgorithmEventType::NodeVisited: return QString("Added L%1 to the forest").arg(event.locationId);
    case AlgorithmEventType::CandidateConsidered:
        return QString("Considering C%1 (cost %2)").arg(event.candidateId).arg(event.value, 0, 'f', 2);
    case AlgorithmEventType::CandidateAccepted:
        return QString("Accepted C%1").arg(event.candidateId);
    case AlgorithmEventType::CandidateRejected:
        return QString("Rejected C%1 because it would not improve the forest").arg(event.candidateId);
    case AlgorithmEventType::Finished: return "Animation complete";
    default: return "Algorithm event";
    }
}

QString candidateList(const std::vector<int>& candidateIds)
{
    QStringList labels;
    for (const int id : candidateIds) labels.push_back(QString("C%1").arg(id));
    return labels.isEmpty() ? QString("none") : labels.join(", ");
}

} // namespace

InfrastructurePage::InfrastructurePage(smartcity::services::CityRepository& repository,
                                       QWidget* parent)
    : QWidget(parent), repository_(repository), algorithmCombo_(new QComboBox(this)),
      candidateTable_(new QTableWidget(this)), scene_(new QGraphicsScene(this)),
      view_(new QGraphicsView(scene_, this)), resultLabel_(new QLabel(this)),
      eventLabel_(new QLabel("Choose an algorithm to begin.", this)),
      speedLabel_(new QLabel("Speed: 1.0×", this)),
      animation_(new AlgorithmAnimationController(this))
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(10);
    auto* title = new QLabel("Infrastructure Planner", this);
    title->setObjectName("pageTitle");
    auto* description = new QLabel(
        "Plan an undirected construction network with Prim or Kruskal. Candidates are explicit and are never inferred from travel roads.", this);
    description->setObjectName("pageDescription");
    description->setWordWrap(true);
    root->addWidget(title);
    root->addWidget(description);

    auto* controls = new QFrame(this);
    controls->setObjectName("routePanel");
    auto* controlsLayout = new QHBoxLayout(controls);
    auto* addButton = new QPushButton("+ Candidate", controls);
    auto* editButton = new QPushButton("Edit selected", controls);
    auto* deleteButton = new QPushButton("Delete selected", controls);
    for (auto* button : {addButton, editButton, deleteButton}) button->setObjectName("editorButton");
    algorithmCombo_->addItem("Prim", static_cast<int>(InfrastructureAlgorithm::Prim));
    algorithmCombo_->addItem("Kruskal", static_cast<int>(InfrastructureAlgorithm::Kruskal));
    auto* runButton = new QPushButton("Build minimum forest", controls);
    runButton->setObjectName("primaryButton");
    auto* compareButton = new QPushButton("Compare both", controls);
    compareButton->setObjectName("editorButton");
    controlsLayout->addWidget(addButton);
    controlsLayout->addWidget(editButton);
    controlsLayout->addWidget(deleteButton);
    controlsLayout->addStretch();
    controlsLayout->addWidget(new QLabel("Algorithm", controls));
    controlsLayout->addWidget(algorithmCombo_);
    controlsLayout->addWidget(runButton);
    controlsLayout->addWidget(compareButton);
    root->addWidget(controls);

    auto* content = new QHBoxLayout;
    view_->setObjectName("infrastructureView");
    view_->setRenderHint(QPainter::Antialiasing);
    view_->setDragMode(QGraphicsView::ScrollHandDrag);
    view_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    view_->setMinimumHeight(330);
    scene_->setBackgroundBrush(QColor("#f8fafc"));
    content->addWidget(view_, 1);

    auto* panel = new QFrame(this);
    panel->setObjectName("routePanel");
    panel->setMinimumWidth(370);
    auto* panelLayout = new QVBoxLayout(panel);
    auto* candidateTitle = new QLabel("Construction candidates", panel);
    candidateTitle->setObjectName("sectionTitle");
    candidateTable_->setColumnCount(4);
    candidateTable_->setHorizontalHeaderLabels({"ID", "First", "Second", "Cost"});
    candidateTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    candidateTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    candidateTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    candidateTable_->verticalHeader()->setVisible(false);
    candidateTable_->horizontalHeader()->setStretchLastSection(true);
    candidateTable_->setObjectName("candidateTable");
    resultLabel_->setWordWrap(true);
    resultLabel_->setObjectName("routeResult");
    panelLayout->addWidget(candidateTitle);
    panelLayout->addWidget(candidateTable_, 1);
    panelLayout->addWidget(resultLabel_);
    auto* persistence = new QLabel(
        "City Map → Save/Load JSON includes these candidates.", panel);
    persistence->setObjectName("mapSummary");
    persistence->setWordWrap(true);
    panelLayout->addWidget(persistence);
    content->addWidget(panel);
    root->addLayout(content, 1);

    auto* animationBar = new QHBoxLayout;
    const std::vector<std::pair<QString, std::function<void()>>> actions = {
        {"Play", [this] { animation_->play(); }}, {"Pause", [this] { animation_->pause(); }},
        {"Step", [this] { animation_->step(); }}, {"Reset", [this] { animation_->reset(); }}
    };
    for (const auto& action : actions) {
        auto* button = new QPushButton(action.first, this);
        button->setObjectName("editorButton");
        connect(button, &QPushButton::clicked, this, action.second);
        animationBar->addWidget(button);
    }
    auto* speed = new QSlider(Qt::Horizontal, this);
    speed->setRange(1, 10);
    speed->setValue(5);
    speed->setMaximumWidth(150);
    animationBar->addSpacing(10);
    animationBar->addWidget(speedLabel_);
    animationBar->addWidget(speed);
    eventLabel_->setObjectName("mapSummary");
    animationBar->addSpacing(12);
    animationBar->addWidget(eventLabel_, 1);
    root->addLayout(animationBar);

    connect(addButton, &QPushButton::clicked, this, [this] { addCandidate(); });
    connect(editButton, &QPushButton::clicked, this, [this] { editCandidate(); });
    connect(deleteButton, &QPushButton::clicked, this, [this] { deleteCandidate(); });
    connect(runButton, &QPushButton::clicked, this, [this] { runSelectedAlgorithm(); });
    connect(compareButton, &QPushButton::clicked, this, [this] { compareAlgorithms(); });
    connect(candidateTable_, &QTableWidget::cellDoubleClicked, this,
            [this](int, int) { editCandidate(); });
    connect(speed, &QSlider::valueChanged, this, [this](int value) {
        const double multiplier = value / 5.0;
        speedLabel_->setText(QString("Speed: %1×").arg(multiplier, 0, 'f', 1));
        animation_->setIntervalMilliseconds(1050 - value * 90);
    });
    animation_->setCallbacks(
        [this](const AlgorithmEvent& event) { applyEvent(event); },
        [this] { resetVisualization(); },
        [this] { eventLabel_->setText("Animation complete."); });
    refreshFromGraph(true);
}

void InfrastructurePage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refreshFromGraph();
}

void InfrastructurePage::refreshFromGraph(bool force)
{
    const auto& graph = repository_.city();
    if (!force && observedRevision_ == graph.revision()) return;
    animation_->setEvents({});
    candidateTable_->setRowCount(0);
    std::vector<int> ids;
    for (const auto& entry : graph.constructionCandidates()) ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end());
    for (const int id : ids) {
        const auto& candidate = graph.constructionCandidates().at(id);
        const int row = candidateTable_->rowCount();
        candidateTable_->insertRow(row);
        candidateTable_->setItem(row, 0, new QTableWidgetItem(QString("C%1").arg(id)));
        candidateTable_->item(row, 0)->setData(Qt::UserRole, id);
        candidateTable_->setItem(row, 1, new QTableWidgetItem(locationText(graph, candidate.firstLocationId)));
        candidateTable_->setItem(row, 2, new QTableWidgetItem(locationText(graph, candidate.secondLocationId)));
        candidateTable_->setItem(row, 3, new QTableWidgetItem(QString::number(candidate.constructionCost, 'f', 2)));
    }
    candidateTable_->resizeColumnsToContents();
    observedRevision_ = graph.revision();
    resultLabel_->setText(QString("%1 explicit candidates. Run an algorithm to build a minimum spanning forest.")
        .arg(static_cast<qulonglong>(graph.constructionCandidateCount())));
    eventLabel_->setText(QString("Graph revision %1 loaded.").arg(static_cast<qulonglong>(observedRevision_)));
    rebuildMap();
}

void InfrastructurePage::rebuildMap()
{
    scene_->clear();
    nodeItems_.clear();
    candidateItems_.clear();
    const auto& graph = repository_.city();
    std::vector<int> candidateIds;
    for (const auto& entry : graph.constructionCandidates()) candidateIds.push_back(entry.first);
    std::sort(candidateIds.begin(), candidateIds.end());
    for (const int id : candidateIds) {
        const auto& candidate = graph.constructionCandidates().at(id);
        const auto* first = graph.location(candidate.firstLocationId);
        const auto* second = graph.location(candidate.secondLocationId);
        if (!first || !second) continue;
        auto* line = scene_->addLine(first->x, first->y, second->x, second->y,
            QPen(QColor("#94a3b8"), 4.0, Qt::SolidLine, Qt::RoundCap));
        line->setZValue(0);
        candidateItems_.emplace(id, line);
        auto* label = scene_->addSimpleText(QString("C%1 · %2").arg(id).arg(candidate.constructionCost, 0, 'f', 0));
        label->setBrush(QBrush(QColor("#475569")));
        label->setPos((first->x + second->x) / 2.0 + 5.0,
                      (first->y + second->y) / 2.0 - 18.0);
        label->setFlag(QGraphicsItem::ItemIgnoresTransformations);
        label->setZValue(1);
    }
    for (const auto& entry : graph.locations()) {
        const auto& location = entry.second;
        auto* node = scene_->addEllipse(location.x - 10.0, location.y - 10.0, 20.0, 20.0,
            QPen(Qt::white, 3.0), QBrush(QColor("#475569")));
        node->setZValue(2);
        nodeItems_.emplace(location.id, node);
        auto* label = scene_->addSimpleText(QString("%1 [L%2]")
            .arg(QString::fromStdString(location.name)).arg(location.id));
        label->setBrush(QBrush(QColor("#0f172a")));
        label->setPos(location.x + 13.0, location.y - 14.0);
        label->setFlag(QGraphicsItem::ItemIgnoresTransformations);
        label->setZValue(3);
    }
    if (!scene_->items().isEmpty())
        view_->fitInView(scene_->itemsBoundingRect().adjusted(-45, -45, 45, 45), Qt::KeepAspectRatio);
}

void InfrastructurePage::resetVisualization()
{
    rebuildMap();
    eventLabel_->setText("Animation reset. Press Play or Step.");
}

void InfrastructurePage::applyEvent(const AlgorithmEvent& event)
{
    eventLabel_->setText(eventDescription(event));
    if (event.type == AlgorithmEventType::NodeVisited
        || event.type == AlgorithmEventType::ComponentStarted) {
        const auto item = nodeItems_.find(event.locationId);
        if (item != nodeItems_.end()) item->second->setBrush(QBrush(
            event.type == AlgorithmEventType::ComponentStarted ? QColor("#8b5cf6") : QColor("#2563eb")));
    }
    const auto candidate = candidateItems_.find(event.candidateId);
    if (candidate == candidateItems_.end()) return;
    if (event.type == AlgorithmEventType::CandidateConsidered)
        candidate->second->setPen(QPen(QColor("#f59e0b"), 6.0, Qt::SolidLine, Qt::RoundCap));
    else if (event.type == AlgorithmEventType::CandidateAccepted)
        candidate->second->setPen(QPen(QColor("#16a34a"), 7.0, Qt::SolidLine, Qt::RoundCap));
    else if (event.type == AlgorithmEventType::CandidateRejected)
        candidate->second->setPen(QPen(QColor("#cbd5e1"), 3.0, Qt::DashLine, Qt::RoundCap));
}

int InfrastructurePage::selectedCandidateId() const
{
    const int row = candidateTable_->currentRow();
    return row < 0 || !candidateTable_->item(row, 0)
        ? -1 : candidateTable_->item(row, 0)->data(Qt::UserRole).toInt();
}

void InfrastructurePage::addCandidate()
{
    const auto& graph = repository_.city();
    if (graph.vertexCount() < 2) {
        QMessageBox::information(this, "Candidate unavailable", "Add at least two locations first.");
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle("Add construction candidate");
    auto* form = new QFormLayout(&dialog);
    auto* first = new QComboBox(&dialog);
    auto* second = new QComboBox(&dialog);
    std::vector<int> ids;
    for (const auto& entry : graph.locations()) ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end());
    for (const int id : ids) {
        first->addItem(locationText(graph, id), id);
        second->addItem(locationText(graph, id), id);
    }
    second->setCurrentIndex(1);
    auto* cost = new QDoubleSpinBox(&dialog);
    cost->setRange(0.0, 1.0e12);
    cost->setDecimals(2);
    cost->setValue(100000.0);
    form->addRow("First location", first);
    form->addRow("Second location", second);
    form->addRow("Construction cost", cost);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    if (first->currentData().toInt() == second->currentData().toInt()) {
        QMessageBox::warning(this, "Invalid candidate", "The two endpoints must be different.");
        return;
    }
    auto& city = repository_.city();
    const smartcity::core::ConstructionCandidate candidate{
        city.nextCandidateId(), first->currentData().toInt(), second->currentData().toInt(), cost->value()};
    if (!city.addConstructionCandidate(candidate)) {
        QMessageBox::warning(this, "Invalid candidate", "The construction candidate could not be added.");
        return;
    }
    refreshFromGraph(true);
}

void InfrastructurePage::editCandidate()
{
    const int id = selectedCandidateId();
    const auto* existing = repository_.city().constructionCandidate(id);
    if (!existing) {
        QMessageBox::information(this, "Select a candidate", "Select a construction candidate to edit.");
        return;
    }
    const auto original = *existing;
    QDialog dialog(this);
    dialog.setWindowTitle(QString("Edit candidate C%1").arg(id));
    auto* form = new QFormLayout(&dialog);
    auto* first = new QComboBox(&dialog);
    auto* second = new QComboBox(&dialog);
    std::vector<int> ids;
    for (const auto& entry : repository_.city().locations()) ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end());
    for (const int locationId : ids) {
        const QString text = locationText(repository_.city(), locationId);
        first->addItem(text, locationId);
        second->addItem(text, locationId);
    }
    first->setCurrentIndex(first->findData(original.firstLocationId));
    second->setCurrentIndex(second->findData(original.secondLocationId));
    auto* cost = new QDoubleSpinBox(&dialog);
    cost->setRange(0.0, 1.0e12);
    cost->setDecimals(2);
    cost->setValue(original.constructionCost);
    form->addRow("Stable ID", new QLabel(QString("C%1").arg(id), &dialog));
    form->addRow("First location", first);
    form->addRow("Second location", second);
    form->addRow("Construction cost", cost);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    if (first->currentData().toInt() == second->currentData().toInt()) {
        QMessageBox::warning(this, "Invalid candidate", "The two endpoints must be different.");
        return;
    }
    const smartcity::core::ConstructionCandidate changed{
        id, first->currentData().toInt(), second->currentData().toInt(), cost->value()};
    if (!repository_.city().updateConstructionCandidate(changed)) {
        QMessageBox::warning(this, "Invalid candidate", "The construction candidate could not be updated.");
        return;
    }
    refreshFromGraph(true);
}

void InfrastructurePage::deleteCandidate()
{
    const int id = selectedCandidateId();
    if (id < 0) {
        QMessageBox::information(this, "Select a candidate", "Select a construction candidate to delete.");
        return;
    }
    if (QMessageBox::question(this, "Delete candidate", QString("Delete construction candidate C%1?").arg(id))
        != QMessageBox::Yes) return;
    repository_.city().removeConstructionCandidate(id);
    refreshFromGraph(true);
}

void InfrastructurePage::runSelectedAlgorithm()
{
    refreshFromGraph();
    const auto algorithm = static_cast<InfrastructureAlgorithm>(algorithmCombo_->currentData().toInt());
    const auto result = smartcity::algorithms::InfrastructureAlgorithms::minimumSpanningForest(
        repository_.city(), algorithm);
    showResult(result, algorithm == InfrastructureAlgorithm::Prim ? "Prim" : "Kruskal");
    animation_->setEvents(result.events);
    animation_->play();
}

void InfrastructurePage::showResult(
    const smartcity::algorithms::SpanningForestResult& result, const QString& algorithmName)
{
    QString text = QString("%1 selected %2 edge(s). Total construction cost: %3. Components: %4.")
        .arg(algorithmName)
        .arg(static_cast<qulonglong>(result.candidateIds.size()))
        .arg(result.totalCost, 0, 'f', 2)
        .arg(result.componentCount);
    text += " Selected candidates: " + candidateList(result.candidateIds) + ".";
    if (result.componentCount > 1) {
        text += " This is a minimum spanning forest; one connected network is impossible with the available candidates.";
    } else if (result.componentCount == 1) {
        text += " A single connected minimum spanning tree is available.";
    } else {
        text += " The city has no locations.";
    }
    resultLabel_->setText(text);
}

void InfrastructurePage::compareAlgorithms()
{
    refreshFromGraph();
    const auto prim = smartcity::algorithms::InfrastructureAlgorithms::prim(repository_.city());
    const auto kruskal = smartcity::algorithms::InfrastructureAlgorithms::kruskal(repository_.city());
    const double costScale = std::max({1.0, std::abs(prim.totalCost), std::abs(kruskal.totalCost)});
    const bool same = prim.componentCount == kruskal.componentCount
        && std::abs(prim.totalCost - kruskal.totalCost) <= 1e-9 * costScale;
    resultLabel_->setText(QString(
        "Identical input comparison — Prim: %1 (%2 components; %3). "
        "Kruskal: %4 (%5 components; %6). %7 "
        "Equal-cost ties may produce different valid edge sets.")
        .arg(prim.totalCost, 0, 'f', 2).arg(prim.componentCount).arg(candidateList(prim.candidateIds))
        .arg(kruskal.totalCost, 0, 'f', 2).arg(kruskal.componentCount).arg(candidateList(kruskal.candidateIds))
        .arg(same ? "Totals agree." : "Totals differ: check the candidate data or implementation."));
    animation_->setEvents({});
    resetVisualization();
}
