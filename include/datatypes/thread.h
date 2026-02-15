#pragma once

#ifndef THREAD_H
#define THREAD_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <vector>
#include <queue>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <pthread.h>
#endif // _WIN32


#ifdef DATATYPES_USE_INDEPENDENT_NAMESP
#define DATATYPES_NAMESPACE_BEGIN namespace datatypes {
#define DATATYPES_NAMESPACE_END }
#else
#define DATATYPES_NAMESPACE_BEGIN
#define DATATYPES_NAMESPACE_END
#endif // DATATYPES_USE_INDEPENDENT_NAMESP

DATATYPES_NAMESPACE_BEGIN

// 线程状态枚举
enum class ThreadState {
    IDLE,       // 空闲
    RUNNING,    // 运行中
    STOPPING,   // 正在停止
    STOPPED,    // 已停止
    TERMINATED  // 已终止
};



// 线程类。
class Thread {
private:
    std::thread worker;                                 // 线程对象
    std::atomic<ThreadState> state;                     // 线程状态
    std::chrono::steady_clock::time_point startTime;    // 线程启动时间   
    std::chrono::steady_clock::time_point endTime;      // 线程结束时间
    std::function<void()> task = nullptr;               // 线程任务
    mutable std::mutex taskMutex;                       // 任务互斥锁 (添加 mutable)
    std::condition_variable cv;                         // 条件变量
    std::atomic<bool> stopRequested;                    // 停止请求标志
#ifdef _WIN32
    DWORD threadId = 0;                                     // Windows线程ID
#else
    pthread_t nativeHandle;                              // POSIX线程句柄
#endif

    // 线程执行函数
    void run() {
        {
            std::lock_guard<std::mutex> lock(taskMutex);
            startTime = std::chrono::steady_clock::now();
            state = ThreadState::RUNNING;
        }
#ifdef _WIN32
        threadId = GetCurrentThreadId();
#else
        nativeHandle = pthread_self();
#endif

        try {
            if (task) task();
        } catch (const std::exception& e) {
            fprintf(stderr, "Exception from Thread(0x{%16x}), threadId=%d:\n", 
                    (uint64_t) this, getId());
            fprintf(stderr, "    %s\n", e.what());
        } catch (...) {
            fprintf(stderr, "Exception from Thread(0x{%16x}), threadId=%d:\n", 
                    (uint64_t) this, getId());
            fprintf(stderr, "    Unknown exception\n");
        }

        {
            std::lock_guard<std::mutex> lock(taskMutex);
            endTime = std::chrono::steady_clock::now();
            state = stopRequested ? ThreadState::STOPPED : ThreadState::IDLE;
        }
    }

public:
    // 错误处理回调函数类型
    using ErrorCallback = std::function<void(Thread*, const std::string&)>;
    
    // 实例级别的错误处理回调函数
    ErrorCallback onStartingRunningThread;    // 试图启动一个已经在运行的线程
    ErrorCallback onStoppingStoppedThread;    // 试图停止一个已经停止的线程
    ErrorCallback onTerminatingStoppedThread; // 试图强制终止一个已经停止的线程
    ErrorCallback onStartingNullTask;         // 试图启动一个没有任务的线程
    ErrorCallback onForceTerminate;           // 强制终止线程
    ErrorCallback onGetIdFailed;              // 获取线程ID失败
    
    /**
     * 构造函数
     */
    Thread() : state(ThreadState::IDLE), stopRequested(false) {
        // 设置默认的错误处理回调
        setDefaultErrorCallbacks();
    }
    
    /**
     * 设置默认的错误处理回调
     */
    void setDefaultErrorCallbacks() {
        onStartingRunningThread = [](Thread* pt, const std::string& msg) {
            fprintf(stderr, "Warning: Attempt to start a running thread Thread(%p) - %s\n", 
                    pt, msg.c_str());
        };
        
        onStoppingStoppedThread = [](Thread* pt, const std::string& msg) {
            fprintf(stderr, "Warning: Attempt to stop a stopped thread Thread(%p) - %s\n", 
                    pt, msg.c_str());
        };
        
        onTerminatingStoppedThread = [](Thread* pt, const std::string& msg) {
            fprintf(stderr, "Warning: Attempt to terminate a stopped thread Thread(%p) - %s\n", 
                    pt, msg.c_str());
        };
        
        onStartingNullTask = [](Thread* pt, const std::string& msg) {
            fprintf(stderr, "Warning: Attempt to start a thread with null task Thread(%p) - %s\n", 
                    pt, msg.c_str());
        };
        
        onForceTerminate = [](Thread* pt, const std::string& msg) {
            fprintf(stderr, "Warning: Force terminating thread Thread(%p) - %s\n", 
                    pt, msg.c_str());
        };
        
        onGetIdFailed = [](Thread* pt, const std::string& msg) {
            fprintf(stderr, "Warning: Thread(%p) getId() failed - %s\n", 
                    pt, msg.c_str());
        };
    }

    /**
     * 构造函数。
     * @param func 线程任务
     */
    Thread(const std::function<void()>& func)
        : state(ThreadState::IDLE), task(func), stopRequested(false) {}

    /**
     * 构造函数。
     * @param func 线程任务的指针
     */
    Thread(const std::function<void()>* func)
        : state(ThreadState::IDLE), stopRequested(false) {
        if (func) {
            task = *func;
        }
    }

    // 析构函数。
    // 会自动终止线程。
    ~Thread() {
        if (state == ThreadState::RUNNING) {
            stop();
        }
    }

    // 禁止拷贝
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    // 允许移动
    Thread(Thread&& other) noexcept {
        *this = std::move(other);
    }

    // 移动赋值。
    // 终止当前线程（如果在运行），然后接管另一个线程的资源。
    Thread& operator=(Thread&& other) noexcept {
        if (this != &other) {
            terminate();
            worker = std::move(other.worker);
            state = other.state.load();
            startTime = other.startTime;
            endTime = other.endTime;
            task = std::move(other.task);
            stopRequested = other.stopRequested.load();
        }
        return *this;
    }

    /**
     * 启动线程。
     * @param func 线程任务。如果提供了该参数，则使用该任务，否则使用之前设置的任务。
     * @return 是否成功启动
     */
    bool start(std::function<void()> func = nullptr) {
        std::lock_guard<std::mutex> lock(taskMutex);

        ThreadState currentState = state;
        if (currentState == ThreadState::RUNNING) {
            if (onStartingRunningThread) {
                onStartingRunningThread(this, "Thread is already running");
            }
            return false;
        }

        if (func) task = std::move(func);

        if (!task) {
            if (onStartingNullTask) 
                onStartingNullTask(this, "No task provided for thread");
            return false;
        }

        stopRequested = false;
        state = ThreadState::RUNNING;

        worker = std::thread(&Thread::run, this);
        return true;
    }

    /**
     * 等待线程完成
     * @param timeoutMs 超时时间（毫秒），0表示无限等待
     * @return 是否成功等待完成
     */
    bool join(unsigned long timeout = 0) {
        if (state == ThreadState::IDLE || state == ThreadState::STOPPED) {
            return true;
        }

        // stopRequested = true;

        if (worker.joinable()) {
            if (timeout == 0) {
                worker.join();
                return true;
            } else {
                // 带超时的等待
                auto timeoutTime = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeout);
                while (state == ThreadState::RUNNING && std::chrono::steady_clock::now() < timeoutTime)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));

                if (state != ThreadState::RUNNING) {
                    worker.join();
                    return true;
                }
                return false; // 超时
            }
        }
        return true;
    }


    /**
     * 使用平台特定的API强制杀死线程。
     * 警告：可能导致资源泄漏和数据不一致。
     */
    bool terminate() {
        std::lock_guard<std::mutex> lock(taskMutex);
        
        if (state == ThreadState::IDLE || state == ThreadState::STOPPED) {
            if (onTerminatingStoppedThread) {
                onTerminatingStoppedThread(this, "Thread is already stopped");
            }
            return false;
        }

        if (!worker.joinable()) {
            return false;
        }

        bool success = false;
        
#ifdef _WIN32
        if (threadId != 0) {
            HANDLE hThread = OpenThread(THREAD_TERMINATE, FALSE, threadId);
            if (hThread) {
                success = TerminateThread(hThread, 0);
                CloseHandle(hThread);
                
                if (success) {
                    state = ThreadState::TERMINATED;
                    endTime = std::chrono::steady_clock::now();
                    worker.detach(); // 分离已终止的线程
                }
            }
        }
#else
        success = (pthread_cancel(nativeHandle) == 0);
        if (success) {
            state = ThreadState::TERMINATED;
            endTime = std::chrono::steady_clock::now();
            worker.detach(); // 分离已终止的线程
        }
#endif

        if (onForceTerminate)
            onForceTerminate(this, success ? "Thread force terminated successfully" : "Thread force termination failed");
        
        return success;
    }

    // 安全停止线程。
    // 注意：线程任务需要定期检查shouldStop()来决定是否提前退出。
    void stop() {
        stopRequested = true;
        join();
    }

    // 将线程从当前主线程分离。
    void detach() {
        std::lock_guard<std::mutex> lock(taskMutex);
        if (worker.joinable()) {
            worker.detach();
            state = ThreadState::STOPPED;
        }
    }

    // 获取线程状态。
    ThreadState getState() const {
        return state;
    }

    // 检查线程是否正在运行。
    bool isRunning() const {
        return state == ThreadState::RUNNING;
    }

    //  获取线程ID。
    std::thread::id getId() const {
        if (worker.joinable()) {
            return worker.get_id();
        }
        if (onGetIdFailed) {
            // 这里需要去掉const，但是具体为啥我也不知道（可能是const？）
            onGetIdFailed(const_cast<Thread*>(this), "Thread is not running or already stopped");
        }
        return std::thread::id();
    }

    // 获取任务开始时间。
    std::chrono::steady_clock::time_point getStartTime() const {
        std::lock_guard<std::mutex> lock(taskMutex);
        return startTime;
    }

    // 获取任务结束时间。
    std::chrono::steady_clock::time_point getEndTime() const {
        std::lock_guard<std::mutex> lock(taskMutex);
        return endTime;
    }

    // 获取任务运行时长（毫秒）。
    uint64_t duration() const {
        std::lock_guard<std::mutex> lock(taskMutex);
        if (state == ThreadState::RUNNING) {
            auto now = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                now - startTime).count();
        } else if (state == ThreadState::STOPPED || state == ThreadState::TERMINATED) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                endTime - startTime).count();
        }
        return 0;
    }

    // 设置线程任务
    void setTask(const std::function<void()>& func) {
        std::lock_guard<std::mutex> lock(taskMutex);
        task = func;
    }

    /**
     * 检查是否有停止请求。
     * 任务可以在执行过程中调用此函数来检查是否应该提前退出。
     */
    bool shouldStop() const {
        return stopRequested;
    }
};


DATATYPES_NAMESPACE_END

#endif // THREAD_H