#include "gui/pages/AlgorithmLabPage.h"

#include "algorithms/WeightedGraph.h"
#include "gui/animation/AlgorithmAnimationController.h"
#include "services/CityRepository.h"

#include <QComboBox>
#include <QBrush>
#include <QColor>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QRegularExpression>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <functional>
#include <random>
#include <utility>

namespace {
using smartcity::algorithms::AlgorithmEvent;
using smartcity::algorithms::AlgorithmEventType;
using smartcity::algorithms::SortAlgorithm;
using smartcity::algorithms::SortOrder;

constexpr int MaximumInputSize = 2000;
constexpr int MaximumDrawnValues = 120;

QString compactNumber(double value)
{
    return QString::number(value, 'g', 6);
}
}

AlgorithmLabPage::AlgorithmLabPage(smartcity::services::CityRepository& repository,
                                   QWidget* parent)
    : QWidget(parent), repository_(repository), valuesEdit_(new QLineEdit(this)),
      datasetKindCombo_(new QComboBox(this)), sizeSpin_(new QSpinBox(this)),
      seedSpin_(new QSpinBox(this)), importCombo_(new QComboBox(this)),
      algorithmCombo_(new QComboBox(this)), orderCombo_(new QComboBox(this)),
      targetSpin_(new QDoubleSpinBox(this)), scene_(new QGraphicsScene(this)),
      view_(new QGraphicsView(scene_, this)), countersLabel_(new QLabel(this)),
      statusLabel_(new QLabel(this)), profileLabel_(new QLabel(this)),
      eventLabel_(new QLabel(this)), speedLabel_(new QLabel("Speed: 1.0×", this)),
      animation_(new AlgorithmAnimationController(this))
{
    auto* root = new QVBoxLayout(this); root->setContentsMargins(24, 20, 24, 20); root->setSpacing(9);
    auto* title = new QLabel("Sorting and Searching Lab", this); title->setObjectName("pageTitle");
    auto* description = new QLabel(
        "Animate manual sorting algorithms and Binary Search. Imported road and package values are copied; original records are never modified.", this);
    description->setObjectName("pageDescription"); description->setWordWrap(true);
    root->addWidget(title); root->addWidget(description);

    auto* dataPanel = new QFrame(this); dataPanel->setObjectName("routePanel");
    auto* dataGrid = new QGridLayout(dataPanel);
    valuesEdit_->setPlaceholderText("Example: 8, 3, 5, 3, -2, 10");
    dataGrid->addWidget(new QLabel("Numbers", dataPanel), 0, 0); dataGrid->addWidget(valuesEdit_, 0, 1, 1, 5);
    auto* useEntered = new QPushButton("Use entered numbers", dataPanel); useEntered->setObjectName("editorButton"); dataGrid->addWidget(useEntered, 0, 6);
    datasetKindCombo_->addItems({"Random", "Sorted", "Reverse-sorted", "Duplicate-heavy"});
    sizeSpin_->setRange(0, MaximumInputSize); sizeSpin_->setValue(20);
    seedSpin_->setRange(0, 999999999); seedSpin_->setValue(42);
    dataGrid->addWidget(new QLabel("Generate", dataPanel), 1, 0); dataGrid->addWidget(datasetKindCombo_, 1, 1);
    dataGrid->addWidget(new QLabel("Size", dataPanel), 1, 2); dataGrid->addWidget(sizeSpin_, 1, 3);
    dataGrid->addWidget(new QLabel("Seed", dataPanel), 1, 4); dataGrid->addWidget(seedSpin_, 1, 5);
    auto* generate = new QPushButton("Generate reproducibly", dataPanel); generate->setObjectName("editorButton"); dataGrid->addWidget(generate, 1, 6);
    importCombo_->addItems({"Road distances", "Road travel times", "Road construction costs",
                            "Package weights", "Package values", "Package priorities"});
    dataGrid->addWidget(new QLabel("Import copy", dataPanel), 2, 0); dataGrid->addWidget(importCombo_, 2, 1, 1, 5);
    auto* importButton = new QPushButton("Import values", dataPanel); importButton->setObjectName("editorButton"); dataGrid->addWidget(importButton, 2, 6);
    root->addWidget(dataPanel);

    auto* algorithmPanel = new QFrame(this); algorithmPanel->setObjectName("routePanel");
    auto* algorithmGrid = new QGridLayout(algorithmPanel);
    algorithmCombo_->addItem("Bubble Sort", static_cast<int>(SortAlgorithm::Bubble));
    algorithmCombo_->addItem("Selection Sort", static_cast<int>(SortAlgorithm::Selection));
    algorithmCombo_->addItem("Insertion Sort", static_cast<int>(SortAlgorithm::Insertion));
    algorithmCombo_->addItem("Merge Sort", static_cast<int>(SortAlgorithm::Merge));
    algorithmCombo_->addItem("Quick Sort", static_cast<int>(SortAlgorithm::Quick));
    algorithmCombo_->addItem("Heap Sort", static_cast<int>(SortAlgorithm::Heap));
    orderCombo_->addItem("Ascending", static_cast<int>(SortOrder::Ascending));
    orderCombo_->addItem("Descending", static_cast<int>(SortOrder::Descending));
    algorithmGrid->addWidget(new QLabel("Algorithm", algorithmPanel), 0, 0); algorithmGrid->addWidget(algorithmCombo_, 0, 1);
    algorithmGrid->addWidget(new QLabel("Order", algorithmPanel), 0, 2); algorithmGrid->addWidget(orderCombo_, 0, 3);
    auto* sortButton = new QPushButton("Prepare sort animation", algorithmPanel); sortButton->setObjectName("primaryButton"); algorithmGrid->addWidget(sortButton, 0, 4);
    targetSpin_->setRange(-1e12, 1e12); targetSpin_->setDecimals(4);
    algorithmGrid->addWidget(new QLabel("Binary Search target", algorithmPanel), 1, 0); algorithmGrid->addWidget(targetSpin_, 1, 1, 1, 2);
    auto* searchButton = new QPushButton("Search sorted result", algorithmPanel); searchButton->setObjectName("editorButton"); algorithmGrid->addWidget(searchButton, 1, 3, 1, 2);
    profileLabel_->setObjectName("algorithmCaveat"); profileLabel_->setWordWrap(true); algorithmGrid->addWidget(profileLabel_, 2, 0, 1, 5);
    root->addWidget(algorithmPanel);

    view_->setObjectName("algorithmLabView"); view_->setRenderHint(QPainter::Antialiasing); view_->setMinimumHeight(270);
    scene_->setBackgroundBrush(QColor("#f8fafc")); root->addWidget(view_, 1);
    countersLabel_->setObjectName("routeResult"); statusLabel_->setObjectName("routeResult");
    countersLabel_->setText("Comparisons: 0 · Swaps: 0 · Writes: 0"); statusLabel_->setWordWrap(true);
    root->addWidget(countersLabel_); root->addWidget(statusLabel_);

    auto* animationBar = new QHBoxLayout;
    const std::vector<std::pair<QString, std::function<void()>>> actions{
        {"Play", [this] { animation_->play(); }}, {"Pause", [this] { animation_->pause(); }},
        {"Step", [this] { animation_->step(); }}, {"Reset", [this] { animation_->reset(); }}};
    for (const auto& action : actions) {
        auto* button = new QPushButton(action.first, this); button->setObjectName("editorButton");
        connect(button, &QPushButton::clicked, this, action.second); animationBar->addWidget(button);
    }
    auto* speed = new QSlider(Qt::Horizontal, this); speed->setRange(1, 10); speed->setValue(5); speed->setMaximumWidth(150);
    animationBar->addWidget(speedLabel_); animationBar->addWidget(speed);
    eventLabel_->setObjectName("mapSummary"); animationBar->addWidget(eventLabel_, 1); root->addLayout(animationBar);

    connect(useEntered, &QPushButton::clicked, this, [this] { parseEnteredValues(); });
    connect(generate, &QPushButton::clicked, this, [this] { generateDataset(); });
    connect(importButton, &QPushButton::clicked, this, [this] { importDataset(); });
    connect(sortButton, &QPushButton::clicked, this, [this] { runSort(); });
    connect(searchButton, &QPushButton::clicked, this, [this] { runSearch(); });
    connect(algorithmCombo_, &QComboBox::currentIndexChanged, this, [this] { updateProfile(); });
    connect(speed, &QSlider::valueChanged, this, [this](int value) {
        speedLabel_->setText(QString("Speed: %1×").arg(value / 5.0, 0, 'f', 1));
        animation_->setIntervalMilliseconds(1050 - value * 90);
    });
    animation_->setCallbacks([this](const AlgorithmEvent& event) { applyEvent(event); },
                             [this] { resetAnimation(); },
                             [this] {
                                 if (!searchAnimation_) {
                                     displayed_ = sortedResult_;
                                     liveComparisons_ = finalSortCounters_.comparisons;
                                     liveSwaps_ = finalSortCounters_.swaps;
                                     liveWrites_ = finalSortCounters_.writes;
                                     countersLabel_->setText(QString("Comparisons: %1 · Swaps: %2 · Writes: %3")
                                         .arg(liveComparisons_).arg(liveSwaps_).arg(liveWrites_));
                                     redraw();
                                 }
                                 statusLabel_->setText(statusLabel_->text() + " Animation complete.");
                             });
    updateProfile(); generateDataset();
}

std::vector<double> AlgorithmLabPage::parsedValues(bool* valid) const
{
    std::vector<double> values; *valid = false;
    const auto parts = valuesEdit_->text().split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
    if (parts.size() > MaximumInputSize) return values;
    for (const auto& part : parts) {
        bool ok = false; const double value = part.toDouble(&ok);
        if (!ok || !std::isfinite(value)) return {};
        values.push_back(value);
    }
    *valid = true; return values;
}

void AlgorithmLabPage::parseEnteredValues()
{
    bool valid = false; auto values = parsedValues(&valid);
    if (!valid) { QMessageBox::warning(this, "Invalid numbers", QString("Enter at most %1 finite numbers separated by commas or spaces.").arg(MaximumInputSize)); return; }
    setDataset(std::move(values), "Entered dataset copied.");
}

void AlgorithmLabPage::generateDataset()
{
    std::mt19937 generator(static_cast<std::mt19937::result_type>(seedSpin_->value()));
    std::vector<double> values; values.reserve(static_cast<std::size_t>(sizeSpin_->value()));
    const int kind = datasetKindCombo_->currentIndex();
    std::uniform_int_distribution<int> randomValue(-99, 99); std::uniform_int_distribution<int> duplicateValue(-3, 3);
    for (int index = 0; index < sizeSpin_->value(); ++index) {
        if (kind == 1) values.push_back(index);
        else if (kind == 2) values.push_back(sizeSpin_->value() - index);
        else if (kind == 3) values.push_back(duplicateValue(generator));
        else values.push_back(randomValue(generator));
    }
    setDataset(std::move(values), QString("Generated with seed %1.").arg(seedSpin_->value()));
}

void AlgorithmLabPage::importDataset()
{
    std::vector<double> values;
    if (importCombo_->currentIndex() <= 2) {
        std::vector<int> ids; for (const auto& entry : repository_.city().roads()) ids.push_back(entry.first);
        std::sort(ids.begin(), ids.end());
        for (int id : ids) {
            const auto& road = *repository_.city().road(id);
            values.push_back(importCombo_->currentIndex() == 0 ? road.distance
                : importCombo_->currentIndex() == 1 ? road.travelTime : road.constructionCost);
        }
    } else {
        std::vector<int> ids; for (const auto& entry : repository_.deliveryScenario().packages()) ids.push_back(entry.first);
        std::sort(ids.begin(), ids.end());
        for (int id : ids) {
            const auto& package = repository_.deliveryScenario().packages().at(id);
            values.push_back(importCombo_->currentIndex() == 3 ? package.weight
                : importCombo_->currentIndex() == 4 ? package.value : static_cast<int>(package.priority));
        }
    }
    setDataset(std::move(values), "Imported a copy; source records were not changed.");
}

void AlgorithmLabPage::setDataset(std::vector<double> values, const QString& source)
{
    animation_->setEvents({}); dataset_ = std::move(values); animationBase_ = dataset_; displayed_ = dataset_;
    sortedResult_.clear(); hasSortedResult_ = false;
    QStringList text; for (double value : dataset_) text << compactNumber(value); valuesEdit_->setText(text.join(", "));
    statusLabel_->setText(source + QString(" %1 value(s) ready.").arg(dataset_.size()));
    countersLabel_->setText("Comparisons: 0 · Swaps: 0 · Writes: 0"); redraw();
}

void AlgorithmLabPage::runSort()
{
    const auto algorithm = static_cast<SortAlgorithm>(algorithmCombo_->currentData().toInt());
    const auto order = static_cast<SortOrder>(orderCombo_->currentData().toInt());
    auto result = smartcity::algorithms::SortingAlgorithms::sort(dataset_, algorithm, order);
    if (!result.validInput) { statusLabel_->setText("Sorting rejected non-finite input."); return; }
    sortedResult_ = result.values; sortedOrder_ = order; hasSortedResult_ = true; animationBase_ = dataset_;
    finalSortCounters_ = result.counters; searchAnimation_ = false;
    statusLabel_->setText(QString("Prepared %1 events. Final counters: %2 comparisons, %3 swaps, %4 writes.%5")
        .arg(result.events.size()).arg(result.counters.comparisons).arg(result.counters.swaps).arg(result.counters.writes)
        .arg(result.eventLimitReached ? " Event storage reached the 100,000-event cap; the computed result is still complete." : ""));
    animation_->setEvents(std::move(result.events));
}

void AlgorithmLabPage::runSearch()
{
    if (!hasSortedResult_) { QMessageBox::information(this, "Binary Search", "Prepare a sort first. Binary Search requires its dataset to be sorted in the selected direction."); return; }
    auto result = smartcity::algorithms::SortingAlgorithms::binarySearch(sortedResult_, targetSpin_->value(), sortedOrder_);
    if (!result.validInput) { statusLabel_->setText("Binary Search rejected a dataset that is not sorted in the recorded order."); return; }
    animationBase_ = sortedResult_;
    searchAnimation_ = true;
    statusLabel_->setText(result.found
        ? QString("Binary Search found %1 at index %2 after %3 comparison(s).").arg(targetSpin_->value()).arg(result.index).arg(result.comparisons)
        : QString("Binary Search did not find %1 after %2 comparison(s).").arg(targetSpin_->value()).arg(result.comparisons));
    animation_->setEvents(std::move(result.events));
}

void AlgorithmLabPage::resetAnimation()
{
    displayed_ = animationBase_; comparedA_ = comparedB_ = markedStart_ = markedEnd_ = foundIndex_ = -1;
    liveComparisons_ = liveSwaps_ = liveWrites_ = 0;
    countersLabel_->setText("Comparisons: 0 · Swaps: 0 · Writes: 0"); eventLabel_->setText("Animation reset."); redraw();
}

void AlgorithmLabPage::applyEvent(const AlgorithmEvent& event)
{
    comparedA_ = comparedB_ = -1;
    switch (event.type) {
    case AlgorithmEventType::ArrayCompared:
        comparedA_ = event.indexA; comparedB_ = event.indexB; ++liveComparisons_; eventLabel_->setText(QString("Compared indices %1 and %2").arg(event.indexA).arg(event.indexB)); break;
    case AlgorithmEventType::ArraySwapped:
        if (event.indexA >= 0 && event.indexB >= 0) std::swap(displayed_[static_cast<std::size_t>(event.indexA)], displayed_[static_cast<std::size_t>(event.indexB)]);
        ++liveSwaps_; liveWrites_ += 2; comparedA_ = event.indexA; comparedB_ = event.indexB; eventLabel_->setText(QString("Swapped indices %1 and %2").arg(event.indexA).arg(event.indexB)); break;
    case AlgorithmEventType::ArrayWritten:
        if (event.indexA >= 0) displayed_[static_cast<std::size_t>(event.indexA)] = event.value;
        ++liveWrites_; comparedA_ = event.indexA; eventLabel_->setText(QString("Wrote %1 at index %2").arg(compactNumber(event.value)).arg(event.indexA)); break;
    case AlgorithmEventType::ArrayRangeMarked:
        markedStart_ = event.rangeStart; markedEnd_ = event.rangeEnd; eventLabel_->setText(QString("Marked processed region [%1, %2]").arg(markedStart_).arg(markedEnd_)); break;
    case AlgorithmEventType::SearchProbe:
        ++liveComparisons_; comparedA_ = event.indexA; markedStart_ = event.rangeStart; markedEnd_ = event.rangeEnd; eventLabel_->setText(QString("Binary Search probed index %1 in [%2, %3]").arg(event.indexA).arg(event.rangeStart).arg(event.rangeEnd)); break;
    case AlgorithmEventType::SearchFound:
        foundIndex_ = event.indexA; eventLabel_->setText(QString("Found target at index %1").arg(event.indexA)); break;
    case AlgorithmEventType::Finished:
        markedStart_ = event.rangeStart; markedEnd_ = event.rangeEnd; eventLabel_->setText("Sort finished."); break;
    default: break;
    }
    countersLabel_->setText(QString("Comparisons: %1 · Swaps: %2 · Writes: %3")
        .arg(liveComparisons_).arg(liveSwaps_).arg(liveWrites_)); redraw();
}

void AlgorithmLabPage::redraw()
{
    scene_->clear();
    if (displayed_.empty()) { scene_->addText("Empty dataset"); return; }
    const int shown = std::min<int>(static_cast<int>(displayed_.size()), MaximumDrawnValues);
    double minimum = displayed_.front(), maximum = displayed_.front();
    for (int index = 0; index < shown; ++index) { minimum = std::min(minimum, displayed_[index]); maximum = std::max(maximum, displayed_[index]); }
    const double span = std::max(1e-12, maximum - minimum); const double width = std::max(7.0, 900.0 / shown);
    for (int index = 0; index < shown; ++index) {
        const double normalized = (displayed_[static_cast<std::size_t>(index)] - minimum) / span;
        const double height = 24.0 + normalized * 190.0;
        QColor color("#64748b");
        if (index >= markedStart_ && index <= markedEnd_) color = QColor("#22c55e");
        if (index == comparedA_ || index == comparedB_) color = QColor("#f59e0b");
        if (index == foundIndex_) color = QColor("#2563eb");
        scene_->addRect(index * width, 225.0 - height, width - 2.0, height, QPen(Qt::NoPen), QBrush(color));
        if (shown <= 45) {
            auto* value = scene_->addSimpleText(compactNumber(displayed_[static_cast<std::size_t>(index)]));
            value->setPos(index * width, 230.0); value->setRotation(-45);
        }
    }
    if (static_cast<int>(displayed_.size()) > shown)
        scene_->addText(QString("Showing first %1 of %2 values").arg(shown).arg(displayed_.size()))->setPos(0, 265);
    view_->fitInView(scene_->itemsBoundingRect().adjusted(-10, -10, 10, 10), Qt::KeepAspectRatio);
}

void AlgorithmLabPage::updateProfile()
{
    const auto algorithm = static_cast<SortAlgorithm>(algorithmCombo_->currentData().toInt());
    const auto profile = smartcity::algorithms::SortingAlgorithms::profile(algorithm);
    QString divideExplanation;
    if (algorithm == SortAlgorithm::Merge)
        divideExplanation = " Divide-and-conquer: recursively sort two halves, then merge them.";
    else if (algorithm == SortAlgorithm::Quick)
        divideExplanation = " Divide-and-conquer: partition around a pivot, then recursively sort the partitions.";
    profileLabel_->setText(QString("%1 — time: %2; auxiliary space: %3; %4.%5 Binary Search — time: O(log n), auxiliary space: O(1), requires sorted input. Counters: a comparison tests two values; a swap exchanges two positions and counts as two writes; a write assigns one array position.")
        .arg(QLatin1String(profile.name)).arg(QLatin1String(profile.timeComplexity)).arg(QLatin1String(profile.auxiliarySpace))
        .arg(profile.stable ? "stable" : "not stable")
        .arg(divideExplanation));
}
