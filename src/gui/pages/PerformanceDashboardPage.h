#pragma once

#include "services/BenchmarkSuite.h"

#include <QWidget>
#include <QString>

#include <optional>

class QLabel;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableWidget;
template<typename T> class QFutureWatcher;

namespace smartcity::services { class CityRepository; }

class PerformanceDashboardPage final : public QWidget {
public:
    explicit PerformanceDashboardPage(smartcity::services::CityRepository& repository,
                                      QWidget* parent = nullptr);

private:
    void runBenchmarks();
    void benchmarkFinished();
    void displayReport();
    void exportCsv();
    void populateTheoryTable();

    smartcity::services::CityRepository& repository_;
    QSpinBox* datasetSizeSpin_;
    QSpinBox* seedSpin_;
    QSpinBox* warmupSpin_;
    QSpinBox* repetitionsSpin_;
    QPushButton* runButton_;
    QPushButton* exportButton_;
    QProgressBar* progressBar_;
    QLabel* metadataLabel_;
    QLabel* validationLabel_;
    QLabel* methodologyLabel_;
    QTableWidget* measuredTable_;
    QTableWidget* theoryTable_;
    QFutureWatcher<smartcity::services::BenchmarkReport>* watcher_;
    std::optional<smartcity::services::BenchmarkReport> report_;
    QString measuredAt_;
};
