#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <utility>
#include <list>
#include <iostream>

namespace timer
{
struct TickBehaviour
{
    virtual void operator()() = 0;
};

class ConstSleepingTimeBetweenTicks : public TickBehaviour
{
public:
    template<typename Functor>
    ConstSleepingTimeBetweenTicks(unsigned intervalMillis, Functor&& callback)
        : callback_(callback)
        , intervalMillis_(intervalMillis)
    {
    }

    void operator()() override
    {
        auto sleepingTime = std::chrono::microseconds(1000 * intervalMillis_);
        std::this_thread::sleep_for(sleepingTime);
        callback_();
    }

private:
    std::function<void()> callback_;
    unsigned intervalMillis_;
};

class ConstTimeBetweenTicks : public TickBehaviour
{
public:
    template<typename Functor>
    ConstTimeBetweenTicks(unsigned intervalMillis, Functor&& callback)
        : callback_(std::forward<Functor>(callback))
        , intervalMillis_(intervalMillis)
        , sleepingTime(1000 * intervalMillis_)
    {
    }

    void operator()() override
    {
        std::this_thread::sleep_for(std::chrono::microseconds(sleepingTime));
        const auto wakeupTime = std::chrono::high_resolution_clock::now();
        (callback_)();
        const auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(
                                        std::chrono::high_resolution_clock::now() - wakeupTime)
                                        .count();
        const long long intervalMicros = 1000 * intervalMillis_;
        sleepingTime = processingTime >= intervalMicros ? 0 : intervalMicros - processingTime;
    }

private:
    std::function<void()> callback_;
    unsigned intervalMillis_;
    long long sleepingTime;
};

class Timer
{
    friend struct TickBehaviour;

public:
    template<typename Functor>
    Timer(unsigned intervalMillis, Functor&& callback)
        : tickBehaviour_(std::make_unique<ConstSleepingTimeBetweenTicks>(&callback_, &intervalMillis_))
    {
    }

    template<typename Functor>
    Timer(std::unique_ptr<TickBehaviour> tickBehaviour)
        : tickBehaviour_(std::move(tickBehaviour))
    {
    }

    ~Timer()
    {
        isRunning_ = false;
        worker_.join();
    }

    inline void start()
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

    inline void reset()
    {
    }

    inline void stop()
    {
        std::lock_guard lock(mutex_);
        isRunning_ = false;
    }

private:
    std::unique_ptr<TickBehaviour> tickBehaviour_;
    bool isRunning_;
    std::mutex mutex_;
    std::thread worker_;
};

class SingleShotTimer
{
public:
    template<typename Functor>
    static void call(long long waitForMillis, Functor&& callback)
    {
        static SingleShotTimer instance;
        auto currentMillis = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::high_resolution_clock::now().time_since_epoch())
                                 .count();
        instance.callbacks_.insert({currentMillis + waitForMillis, {std::forward<Functor>(callback)}});
        instance.wakeUp();
    }

private:
    SingleShotTimer(){}
    void wakeUp()
    {
        monitor_.notify_all();
    }

    void sleep()
    {
        std::unique_lock lock(mutex_);
        monitor_.wait(lock);
    }

    void sleepFor(long long millis)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(millis));
    }

    bool shouldCallbacksBeCalled()
    {
        auto element = callbacks_.begin();
        if (element != callbacks_.end())
        {
            auto currentMillis = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::high_resolution_clock::now().time_since_epoch())
                                     .count();
            return (*element).first <= currentMillis;
        }
        return false;
    }

    void processCurrentCalbacks()
    {
        for (const auto& callback : callbacks_.begin()->second)
        {
            callback();
        }
        callbacks_.erase(callbacks_.begin());
    }

    void run()
    {
        while (isRunning_)
        {
            std::cout << "running" << std::endl;
            if (shouldCallbacksBeCalled())
            {
                processCurrentCalbacks();
            }
            if (callbacks_.empty())
            {
                sleep();
            }
            else
            {
                auto currentMillis = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::high_resolution_clock::now().time_since_epoch())
                                     .count();
                auto timeToNextCallback = callbacks_.begin()->first - currentMillis;
                sleepFor(timeToNextCallback);
            }
        }
    }

    std::map<long long, std::list<std::function<void()>>> callbacks_;
    std::condition_variable monitor_;
    std::mutex mutex_;
    std::thread worker_{[this] { run(); }};
    unsigned currentTime_;
    bool isRunning_ = true;
};
}  // namespace timer
