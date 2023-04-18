#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <list>
#include <map>
#include <mutex>
#include <thread>
#include <utility>

namespace timer
{
struct TickBehaviour
{
    virtual void operator()() = 0;
};

class ConstSleepingTimeBetweenTicks : public TickBehaviour
{
public:
    ConstSleepingTimeBetweenTicks(unsigned intervalMillis, std::function<void()> callback);
    void operator()() override;

private:
    std::function<void()> callback_;
    unsigned intervalMillis_;
};

class ConstTimeBetweenTicks : public TickBehaviour
{
public:
    ConstTimeBetweenTicks(unsigned intervalMillis, std::function<void()> callback);
    void operator()() override;

private:
    std::function<void()> callback_;
    unsigned intervalMillis_;
    long long sleepingTime;
};

class Timer
{
public:
    Timer(unsigned intervalMillis, std::function<void()> callback);
    Timer(std::unique_ptr<TickBehaviour> tickBehaviour);
    ~Timer();
    void start();
    void reset();
    void stop();

private:
    std::unique_ptr<TickBehaviour> tickBehaviour_;
    bool isRunning_;
    std::mutex mutex_;
    std::thread worker_;
};

class SingleShotTimer
{
    enum class OnExitThreadBehaviour
    {
        Join,
        Detatch
    };
    class ThreadManager
    {
    public:
        ThreadManager();
        ~ThreadManager();
        void setOnExitThreadBehaviour(OnExitThreadBehaviour behaviour);
        unsigned addThread(unsigned threadId, std::thread&& thread);
        void removeThread(unsigned id);

    private:
        void removePendingThreads();
        void runDeleter();
        void joinThreads();
        void detatchThreads();

        std::vector<unsigned> idsOfThreadsToRemove_;
        std::map<unsigned, std::thread> threads_;
        std::thread deleter_;
        std::mutex mutex_;
        std::condition_variable monitor_;
        bool isRunning_ = true;
        OnExitThreadBehaviour onExitThreadBehaviour_ = OnExitThreadBehaviour::Detatch;
    };

public:
    static void call(unsigned waitForMillis, std::function<void()> callback);
    static void setOnExitThreadBehaviour(OnExitThreadBehaviour behaviour);

private:
    static SingleShotTimer& instance();

    unsigned nextId_ = 0;
    std::mutex mutex_;
    ThreadManager threadManager_;
};
}  // namespace timer
