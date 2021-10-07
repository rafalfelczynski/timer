#include "Timer.hpp"

namespace timer
{
ConstSleepingTimeBetweenTicks::ConstSleepingTimeBetweenTicks(unsigned intervalMillis, std::function<void()> callback)
    : callback_(std::move(callback))
    , intervalMillis_(intervalMillis)
{
}
void ConstSleepingTimeBetweenTicks::operator()()
{
    auto sleepingTime = std::chrono::microseconds(1000 * intervalMillis_);
    std::this_thread::sleep_for(sleepingTime);
    callback_();
}

ConstTimeBetweenTicks::ConstTimeBetweenTicks(unsigned intervalMillis, std::function<void()> callback)
    : callback_(std::move(callback))
    , intervalMillis_(intervalMillis)
    , sleepingTime(1000 * intervalMillis_)
{
}

void ConstTimeBetweenTicks::operator()()
{
    std::this_thread::sleep_for(std::chrono::microseconds(sleepingTime));
    const auto wakeupTime = std::chrono::high_resolution_clock::now();
    (callback_)();
    const auto processingTime =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - wakeupTime)
            .count();
    const long long intervalMicros = 1000 * intervalMillis_;
    sleepingTime = processingTime >= intervalMicros ? 0 : intervalMicros - processingTime;
}

Timer::Timer(unsigned intervalMillis, std::function<void()> callback)
    : tickBehaviour_(std::make_unique<ConstSleepingTimeBetweenTicks>(intervalMillis, std::move(callback)))
{
}

Timer::Timer(std::unique_ptr<TickBehaviour> tickBehaviour)
    : tickBehaviour_(std::move(tickBehaviour))
{
}

Timer::~Timer()
{
    isRunning_ = false;
    worker_.join();
}

void Timer::start()
{
    std::lock_guard lock(mutex_);
    if (isRunning_)
    {
        return;
    }
    isRunning_ = true;
    worker_ = std::thread([this] {
        while (isRunning_)
        {
            (*tickBehaviour_)();
        }
    });
}

void Timer::reset()
{
}

void Timer::stop()
{
    std::lock_guard lock(mutex_);
    isRunning_ = false;
}

void SingleShotTimer::setOnExitThreadBehaviour(OnExitThreadBehaviour behaviour)
{
    auto& timer = instance();
    timer.threadManager_.setOnExitThreadBehaviour(behaviour);
}

void SingleShotTimer::call(unsigned waitForMillis, std::function<void()> callback)
{
    auto& timer = SingleShotTimer::instance();
    std::lock_guard lock(timer.mutex_);
    auto callbackId = timer.nextId_++;

    std::thread thread{[waitForMillis, callbackId, callback = std::move(callback)] {
        std::this_thread::sleep_for(std::chrono::milliseconds(waitForMillis));
        callback();
        SingleShotTimer::instance().threadManager_.removeThread(callbackId);
    }};

    timer.threadManager_.addThread(callbackId, std::move(thread));
}

SingleShotTimer& SingleShotTimer::instance()
{
    static SingleShotTimer instance;
    return instance;
}

SingleShotTimer::ThreadManager::ThreadManager()
    : deleter_(std::thread([this] { runDeleter(); }))
{
}

SingleShotTimer::ThreadManager::~ThreadManager()
{
    isRunning_ = false;
    monitor_.notify_all();
    deleter_.join();
    onExitThreadBehaviour_ == OnExitThreadBehaviour::Join ? joinThreads() : detatchThreads();
}

void SingleShotTimer::ThreadManager::setOnExitThreadBehaviour(OnExitThreadBehaviour behaviour)
{
    onExitThreadBehaviour_ = behaviour;
}

unsigned SingleShotTimer::ThreadManager::addThread(unsigned threadId, std::thread&& thread)
{
    std::lock_guard lock(mutex_);
    threads_.insert({threadId, std::move(thread)});
    monitor_.notify_all();
    return threadId;
}

void SingleShotTimer::ThreadManager::removeThread(unsigned id)
{
    std::lock_guard lock(mutex_);
    idsOfThreadsToRemove_.push_back(id);
    monitor_.notify_all();
}

void SingleShotTimer::ThreadManager::removePendingThreads()
{
    for (const auto& id : idsOfThreadsToRemove_)
    {
        auto iter = threads_.find(id);
        if (iter != threads_.end())
        {
            iter->second.detach();
            threads_.erase(iter);
        }
    }
    idsOfThreadsToRemove_.clear();
}

void SingleShotTimer::ThreadManager::runDeleter()
{
    while (isRunning_)
    {
        std::unique_lock lock(mutex_);
        removePendingThreads();
        monitor_.wait(lock);
    }
}

void SingleShotTimer::ThreadManager::joinThreads()
{
    for (auto& thr : threads_)
    {
        thr.second.join();
    }
}

void SingleShotTimer::ThreadManager::detatchThreads()
{
    for (auto& thr : threads_)
    {
        thr.second.detach();
    }
}
}  // namespace timer
