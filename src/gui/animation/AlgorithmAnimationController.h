#pragma once

#include "algorithms/AlgorithmEvent.h"

#include <QObject>

#include <cstddef>
#include <functional>
#include <vector>

class QTimer;

class AlgorithmAnimationController final : public QObject {
public:
    using EventCallback = std::function<void(const smartcity::algorithms::AlgorithmEvent&)>;
    using SimpleCallback = std::function<void()>;

    explicit AlgorithmAnimationController(QObject* parent = nullptr);

    void setCallbacks(EventCallback eventCallback,
                      SimpleCallback resetCallback,
                      SimpleCallback finishedCallback);
    void setEvents(std::vector<smartcity::algorithms::AlgorithmEvent> events);
    void play();
    void pause();
    void step();
    void reset();
    void setIntervalMilliseconds(int milliseconds);

    [[nodiscard]] bool isPlaying() const;
    [[nodiscard]] std::size_t currentIndex() const noexcept;
    [[nodiscard]] std::size_t eventCount() const noexcept;

private:
    QTimer* timer_;
    std::vector<smartcity::algorithms::AlgorithmEvent> events_;
    std::size_t currentIndex_{0};
    EventCallback eventCallback_;
    SimpleCallback resetCallback_;
    SimpleCallback finishedCallback_;
};
