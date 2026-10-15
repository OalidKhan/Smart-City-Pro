#include "gui/pages/PerformanceDashboardPage.h"

#include "services/CityRepository.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QFileDialog>
#include <QFrame>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QSpinBox>
#include <QSplitter>
#include <QStringList>
#include <QSysInfo>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <array>
#include <exception>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
QString csvField(QString value)
{
    value.replace('"', "\"\"");
    return "\"" + value + "\"";
}
}

PerformanceDashboardPage::PerformanceDashboardPage(
    smartcity::services::CityRepository& repository, QWidget* parent)
    : QWidget(parent), repository_(repository), datasetSizeSpin_(new QSpinBox(this)),
      seedSpin_(new QSpinBox(this)), warmupSpin_(new QSpinBox(this)),
      repetitionsSpin_(new QSpinBox(this)), runButton_(new QPushButton("Run actual benchmarks", this)),
      exportButton_(new QPushButton("Export measurements CSV", this)),
      progressBar_(new QProgressBar(this)), metadataLabel_(new QLabel(this)),
      validationLabel_(new QLabel(this)), methodologyLabel_(new QLabel(this)),
      measuredTable_(new QTableWidget(this)), theoryTable_(new QTableWidget(this)),
      watcher_(new QFutureWatcher<smartcity::services::BenchmarkReport>(this))
{
    auto* root = new QVBoxLayout(this); root->setContentsMargins(24, 20, 24, 20); root->setSpacing(9);
    auto* title = new QLabel("Performance Dashboard", this); title->setObjectName("pageTitle");
    auto* description = new QLabel(
        "Measure algorithms on identical copied workloads. Theoretical complexity is shown separately from observed median runtime.", this);
    description->setObjectName("pageDescription"); description->setWordWrap(true);
    root->addWidget(title); root->addWidget(description);

    auto* controls = new QFrame(this); controls->setObjectName("routePanel");
    auto* grid = new QGridLayout(controls);
    datasetSizeSpin_->setRange(10, 2000); datasetSizeSpin_->setValue(250);
    seedSpin_->setRange(0, 999999999); seedSpin_->setValue(42);
    warmupSpin_->setRange(1, 5); warmupSpin_->setValue(1);
    repetitionsSpin_->setRange(3, 21); repetitionsSpin_->setSingleStep(2); repetitionsSpin_->setValue(7);
    runButton_->setObjectName("primaryButton"); exportButton_->setObjectName("editorButton"); exportButton_->setEnabled(false);
    progressBar_->setRange(0, 0); progressBar_->setMaximumWidth(120); progressBar_->hide();
    grid->addWidget(new QLabel("Sorting values", controls), 0, 0); grid->addWidget(datasetSizeSpin_, 0, 1);
    grid->addWidget(new QLabel("Random seed", controls), 0, 2); grid->addWidget(seedSpin_, 0, 3);
    grid->addWidget(new QLabel("Warm-ups", controls), 0, 4); grid->addWidget(warmupSpin_, 0, 5);
    grid->addWidget(new QLabel("Measured runs", controls), 0, 6); grid->addWidget(repetitionsSpin_, 0, 7);
    grid->addWidget(runButton_, 1, 0, 1, 2); grid->addWidget(exportButton_, 1, 2, 1, 2); grid->addWidget(progressBar_, 1, 4);
    methodologyLabel_->setObjectName("algorithmCaveat"); methodologyLabel_->setWordWrap(true);
    methodologyLabel_->setText(
        "Method: steady_clock, one or more warm-up runs, repeated measurements, median reporting. Dataset generation, rendering, animation delays, and algorithm-event storage are outside timed regions. Times are observations, not complexity claims.");
    grid->addWidget(methodologyLabel_, 2, 0, 1, 8); root->addWidget(controls);

    metadataLabel_->setObjectName("mapSummary"); metadataLabel_->setWordWrap(true);
    validationLabel_->setObjectName("routeResult"); validationLabel_->setWordWrap(true);
    metadataLabel_->setText("No benchmark has run. The table contains no sample or fabricated measurements.");
    root->addWidget(metadataLabel_); root->addWidget(validationLabel_);

    auto* splitter = new QSplitter(Qt::Vertical, this);
    measuredTable_->setObjectName("performanceTable"); measuredTable_->setColumnCount(5);
    measuredTable_->setHorizontalHeaderLabels({"Category", "Algorithm", "Workload", "Median (ms)", "Description"});
    measuredTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    measuredTable_->horizontalHeader()->setStretchLastSection(true);
    measuredTable_->setEditTriggers(QAbstractItemView::NoEditTriggers); measuredTable_->setAlternatingRowColors(true);
    theoryTable_->setObjectName("performanceTable"); theoryTable_->setColumnCount(4);
    theoryTable_->setHorizontalHeaderLabels({"Algorithm", "Time complexity", "Auxiliary space", "Scope"});
    theoryTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    theoryTable_->setEditTriggers(QAbstractItemView::NoEditTriggers); theoryTable_->setAlternatingRowColors(true);
    splitter->addWidget(measuredTable_); splitter->addWidget(theoryTable_); splitter->setStretchFactor(0, 2); splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1); populateTheoryTable();

    connect(runButton_, &QPushButton::clicked, this, [this] { runBenchmarks(); });
    connect(exportButton_, &QPushButton::clicked, this, [this] { exportCsv(); });
    connect(watcher_, &QFutureWatcher<smartcity::services::BenchmarkReport>::finished,
            this, [this] { benchmarkFinished(); });
}

void PerformanceDashboardPage::populateTheoryTable()
{
    const std::vector<std::array<QString, 4>> rows{
        {"Bubble Sort", "O(n²), best O(n)", "O(1)", "n sorting values"},
        {"Selection Sort", "O(n²)", "O(1)", "n sorting values"},
        {"Insertion Sort", "O(n²), best O(n)", "O(1)", "n sorting values"},
        {"Merge Sort", "O(n log n)", "O(n)", "n sorting values"},
        {"Quick Sort", "average O(n log n), worst O(n²)", "average O(log n), worst O(n)", "n sorting values"},
        {"Heap Sort", "O(n log n)", "O(1)", "n sorting values"},
        {"Prim", "O((V + C) log C)", "O(V + C)", "construction candidates"},
        {"Kruskal", "O(C log C)", "O(V + C)", "construction candidates"},
        {"Repeated Dijkstra", "O(V(V + E) log V)", "O(V + E) per run", "all ordered pairs"},
        {"Floyd-Warshall", "O(V³)", "O(V²)", "all ordered pairs"}};
    theoryTable_->setRowCount(static_cast<int>(rows.size()));
    for (int row = 0; row < static_cast<int>(rows.size()); ++row)
        for (int column = 0; column < 4; ++column)
            theoryTable_->setItem(row, column, new QTableWidgetItem(rows[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)]));
}

void PerformanceDashboardPage::runBenchmarks()
{
    if (watcher_->isRunning()) return;
    smartcity::services::BenchmarkConfig config;
    config.sortingDatasetSize = static_cast<std::size_t>(datasetSizeSpin_->value());
    config.randomSeed = static_cast<std::uint32_t>(seedSpin_->value());
    config.warmupRuns = warmupSpin_->value(); config.measuredRuns = repetitionsSpin_->value();
    const auto snapshot = repository_.city();
    runButton_->setEnabled(false); exportButton_->setEnabled(false); progressBar_->show();
    metadataLabel_->setText(QString("Benchmarking immutable graph revision %1 in a worker thread…")
        .arg(static_cast<qulonglong>(snapshot.revision())));
    validationLabel_->clear(); measuredTable_->setRowCount(0);
    watcher_->setFuture(QtConcurrent::run([snapshot, config] {
        return smartcity::services::BenchmarkSuite::run(snapshot, config);
    }));
}

void PerformanceDashboardPage::benchmarkFinished()
{
    runButton_->setEnabled(true); progressBar_->hide();
    try {
        report_ = watcher_->result(); measuredAt_ = QDateTime::currentDateTime().toString(Qt::ISODate);
        exportButton_->setEnabled(true); displayReport();
    } catch (const std::exception& error) {
        report_.reset(); exportButton_->setEnabled(false);
        metadataLabel_->setText("Benchmark failed: " + QString::fromUtf8(error.what()));
    }
}

void PerformanceDashboardPage::displayReport()
{
    if (!report_) return;
    const auto& report = *report_;
    const QString environment = QString("%1; %2 %3; CPU architecture %4; Qt %5")
        .arg(QSysInfo::prettyProductName()).arg(QSysInfo::kernelType())
        .arg(QSysInfo::kernelVersion()).arg(QSysInfo::currentCpuArchitecture())
        .arg(QLatin1String(QT_VERSION_STR));
    metadataLabel_->setText(QString(
        "Measured %1 · seed %2 · sorting size %3 · warm-ups %4 · measured runs %5 · build %6 · compiler %7 · graph revision %8 (%9 locations, %10 roads, %11 candidates) · %12")
        .arg(measuredAt_).arg(static_cast<qulonglong>(report.config.randomSeed))
        .arg(static_cast<qulonglong>(report.config.sortingDatasetSize))
        .arg(report.config.warmupRuns).arg(report.config.measuredRuns)
        .arg(QString::fromStdString(report.buildMode))
        .arg(QString::fromStdString(report.compiler))
        .arg(static_cast<qulonglong>(report.graphRevision))
        .arg(static_cast<qulonglong>(report.locationCount))
        .arg(static_cast<qulonglong>(report.roadCount))
        .arg(static_cast<qulonglong>(report.constructionCandidateCount)).arg(environment));
    const bool stale = repository_.city().revision() != report.graphRevision;
    validationLabel_->setText(QString("Reference checks — sorting: %1; Prim/Kruskal totals: %2; repeated Dijkstra/Floyd-Warshall: %3.%4")
        .arg(report.sortingAgreement ? "agree" : "MISMATCH")
        .arg(report.mstAgreement ? "agree" : "MISMATCH")
        .arg(report.shortestPathAgreement ? "agree" : "MISMATCH")
        .arg(stale ? " The live graph changed; these measurements remain labeled for the older snapshot." : ""));
    measuredTable_->setRowCount(static_cast<int>(report.records.size()));
    for (int row = 0; row < static_cast<int>(report.records.size()); ++row) {
        const auto& record = report.records[static_cast<std::size_t>(row)];
        const QStringList values{QString::fromStdString(record.category), QString::fromStdString(record.algorithm),
            QString::number(static_cast<qulonglong>(record.workloadSize)),
            QString::number(record.medianMilliseconds, 'f', 6),
            QString::fromStdString(record.workloadDescription)};
        for (int column = 0; column < values.size(); ++column)
            measuredTable_->setItem(row, column, new QTableWidgetItem(values.at(column)));
    }
}

void PerformanceDashboardPage::exportCsv()
{
    if (!report_) return;
    const QString fileName = QFileDialog::getSaveFileName(this, "Export actual benchmark measurements",
                                                          "smartcity-benchmarks.csv", "CSV files (*.csv)");
    if (fileName.isEmpty()) return;
    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Export failed", file.errorString()); return;
    }
    QTextStream stream(&file);
    stream << "measured_at,category,algorithm,workload_size,workload_description,median_ms,seed,warmups,measured_runs,build_mode,compiler,os,kernel_type,kernel_version,cpu_architecture,qt_version,graph_revision,locations,roads,candidates,time_complexity,auxiliary_space\n";
    for (const auto& record : report_->records) {
        stream << csvField(measuredAt_) << ',' << csvField(QString::fromStdString(record.category)) << ','
               << csvField(QString::fromStdString(record.algorithm)) << ','
               << static_cast<qulonglong>(record.workloadSize) << ','
               << csvField(QString::fromStdString(record.workloadDescription)) << ','
               << QString::number(record.medianMilliseconds, 'f', 9) << ','
               << static_cast<qulonglong>(report_->config.randomSeed) << ','
               << report_->config.warmupRuns << ',' << report_->config.measuredRuns << ','
               << csvField(QString::fromStdString(report_->buildMode)) << ','
               << csvField(QString::fromStdString(report_->compiler)) << ','
               << csvField(QSysInfo::prettyProductName()) << ','
               << csvField(QSysInfo::kernelType()) << ','
               << csvField(QSysInfo::kernelVersion()) << ','
               << csvField(QSysInfo::currentCpuArchitecture()) << ','
               << csvField(QLatin1String(QT_VERSION_STR)) << ','
               << static_cast<qulonglong>(report_->graphRevision) << ','
               << static_cast<qulonglong>(report_->locationCount) << ','
               << static_cast<qulonglong>(report_->roadCount) << ','
               << static_cast<qulonglong>(report_->constructionCandidateCount) << ','
               << csvField(QString::fromStdString(record.timeComplexity)) << ','
               << csvField(QString::fromStdString(record.auxiliarySpace)) << '\n';
    }
    if (!file.commit()) QMessageBox::critical(this, "Export failed", file.errorString());
    else QMessageBox::information(this, "Export complete", "Actual measurements were exported to CSV.");
}
