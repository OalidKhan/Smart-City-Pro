#include "gui/animation/AlgorithmAnimationController.h"

#include <QTimer>

#include <algorithm>
#include <utility>

AlgorithmAnimationController::AlgorithmAnimationController(QObject* parent)
    : QObject(parent), timer_(new QTimer(this))
{
    timer_->setInterval(450);
    connect(timer_, &QTimer::timeout, this, [this] { step(); });
}

void AlgorithmAnimationController::setCallbacks(EventCallback eventCallback,
                                                 SimpleCallback resetCallback,
                                                 SimpleCallback finishedCallback)
{
    eventCallback_ = std::move(eventCallback);
    resetCallback_ = std::move(resetCallback);
    finishedCallback_ = std::move(finishedCallback);
}

void AlgorithmAnimationController::setEvents(
    std::vector<smartcity::algorithms::AlgorithmEvent> events)
{
    pause();
    events_ = std::move(events);
    currentIndex_ = 0;
    if (resetCallback_) resetCallback_();
}

void AlgorithmAnimationController::play()
{
    if (currentIndex_ < events_.size()) timer_->start();
}

void AlgorithmAnimationController::pause() { timer_->stop(); }

void AlgorithmAnimationController::step()
{
    if (currentIndex_ >= events_.size()) {
        pause();
        return;
    }
    if (eventCallback_) eventCallback_(events_[currentIndex_]);
    ++currentIndex_;
    if (currentIndex_ == events_.size()) {
        pause();
        if (finishedCallback_) finishedCallback_();
    }
}

void AlgorithmAnimationController::reset()
{
    pause();
    currentIndex_ = 0;
    if (resetCallback_) resetCallback_();
}

void AlgorithmAnimationController::setIntervalMilliseconds(int milliseconds)
{
    timer_->setInterval(std::clamp(milliseconds, 40, 2000));
}

bool AlgorithmAnimationController::isPlaying() const { return timer_->isActive(); }
std::size_t AlgorithmAnimationController::currentIndex() const noexcept { return currentIndex_; }
std::size_t AlgorithmAnimationController::eventCount() const noexcept { return events_.size(); }
