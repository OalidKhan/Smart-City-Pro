#pragma once

#include "algorithms/FloydWarshall.h"

#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>

class QComboBox;
class QGraphicsEllipseItem;
class QGraphicsLineItem;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QProgressBar;
class QPushButton;
class QShowEvent;
class QTableWidget;

template<typename T> class QFutureWatcher;

namespace smartcity::services { class CityRepository; }

class AllPairsRoutesPage final : public QWidget {
public:
    explicit AllPairsRoutesPage(smartcity::services::CityRepository& repository,
                                QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    using CacheKey = std::pair<std::uint64_t, int>;
    static constexpr std::size_t MatrixVisualizationLimit = 40;
    static constexpr std::size_t ComputationLimit = 250;

    void refreshFromGraph(bool force = false);
    void requestComputation();
    void computationFinished();
    void activateCachedResult();
    void displayResult(const smartcity::algorithms::AllPairsResult& result);
    void displayMatrix(const smartcity::algorithms::AllPairsResult& result);
    void updateSelectedRoute();
    void rebuildMap();
    const smartcity::algorithms::AllPairsResult* currentResult() const;
    int selectedLocationId(const QComboBox* combo) const;

    smartcity::services::CityRepository& repository_;
    QComboBox* modeCombo_;
    QComboBox* sourceCombo_;
    QComboBox* destinationCombo_;
    QPushButton* computeButton_;
    QProgressBar* progressBar_;
    QTableWidget* matrixTable_;
    QGraphicsScene* scene_;
    QGraphicsView* view_;
    QLabel* statusLabel_;
    QLabel* routeLabel_;
    QFutureWatcher<smartcity::algorithms::AllPairsResult>* watcher_;
    std::uint64_t observedRevision_{static_cast<std::uint64_t>(-1)};
    std::map<CacheKey, smartcity::algorithms::AllPairsResult> cache_;
    std::optional<CacheKey> currentKey_;
    std::unordered_map<int, QGraphicsEllipseItem*> nodeItems_;
    std::unordered_map<int, QGraphicsLineItem*> roadItems_;
};
