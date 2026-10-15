#pragma once

#include "algorithms/AlgorithmEvent.h"
#include "algorithms/SortingAlgorithms.h"

#include <QWidget>

#include <vector>

class AlgorithmAnimationController;
class QComboBox;
class QDoubleSpinBox;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QLineEdit;
class QSlider;
class QSpinBox;

namespace smartcity::services { class CityRepository; }

class AlgorithmLabPage final : public QWidget {
public:
    explicit AlgorithmLabPage(smartcity::services::CityRepository& repository,
                              QWidget* parent = nullptr);

private:
    void parseEnteredValues();
    void generateDataset();
    void importDataset();
    void runSort();
    void runSearch();
    void applyEvent(const smartcity::algorithms::AlgorithmEvent& event);
    void resetAnimation();
    void redraw();
    void updateProfile();
    void setDataset(std::vector<double> values, const QString& source);
    [[nodiscard]] std::vector<double> parsedValues(bool* valid) const;

    smartcity::services::CityRepository& repository_;
    QLineEdit* valuesEdit_;
    QComboBox* datasetKindCombo_;
    QSpinBox* sizeSpin_;
    QSpinBox* seedSpin_;
    QComboBox* importCombo_;
    QComboBox* algorithmCombo_;
    QComboBox* orderCombo_;
    QDoubleSpinBox* targetSpin_;
    QGraphicsScene* scene_;
    QGraphicsView* view_;
    QLabel* countersLabel_;
    QLabel* statusLabel_;
    QLabel* profileLabel_;
    QLabel* eventLabel_;
    QLabel* speedLabel_;
    AlgorithmAnimationController* animation_;
    std::vector<double> dataset_;
    std::vector<double> animationBase_;
    std::vector<double> displayed_;
    std::vector<double> sortedResult_;
    smartcity::algorithms::SortOrder sortedOrder_{smartcity::algorithms::SortOrder::Ascending};
    bool hasSortedResult_{false};
    int comparedA_{-1};
    int comparedB_{-1};
    int markedStart_{-1};
    int markedEnd_{-1};
    int foundIndex_{-1};
    std::size_t liveComparisons_{0};
    std::size_t liveSwaps_{0};
    std::size_t liveWrites_{0};
    smartcity::algorithms::OperationCounters finalSortCounters_;
    bool searchAnimation_{false};
};
