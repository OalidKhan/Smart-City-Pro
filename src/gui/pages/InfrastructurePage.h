#pragma once

#include "algorithms/InfrastructureAlgorithms.h"

#include <QWidget>

#include <cstdint>
#include <unordered_map>

class AlgorithmAnimationController;
class QComboBox;
class QGraphicsEllipseItem;
class QGraphicsLineItem;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QShowEvent;
class QString;
class QTableWidget;

namespace smartcity::services { class CityRepository; }

class InfrastructurePage final : public QWidget {
public:
    explicit InfrastructurePage(smartcity::services::CityRepository& repository,
                                QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void refreshFromGraph(bool force = false);
    void rebuildMap();
    void resetVisualization();
    void applyEvent(const smartcity::algorithms::AlgorithmEvent& event);
    void addCandidate();
    void editCandidate();
    void deleteCandidate();
    void runSelectedAlgorithm();
    void compareAlgorithms();
    int selectedCandidateId() const;
    void showResult(const smartcity::algorithms::SpanningForestResult& result,
                    const QString& algorithmName);

    smartcity::services::CityRepository& repository_;
    QComboBox* algorithmCombo_;
    QTableWidget* candidateTable_;
    QGraphicsScene* scene_;
    QGraphicsView* view_;
    QLabel* resultLabel_;
    QLabel* eventLabel_;
    QLabel* speedLabel_;
    AlgorithmAnimationController* animation_;
    std::uint64_t observedRevision_{static_cast<std::uint64_t>(-1)};
    std::unordered_map<int, QGraphicsEllipseItem*> nodeItems_;
    std::unordered_map<int, QGraphicsLineItem*> candidateItems_;
};
