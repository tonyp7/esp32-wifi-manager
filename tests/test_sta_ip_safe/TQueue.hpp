/**
 * @file TQueue.hpp
 * @author TheSomeMan
 * @date 2020-06-16
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef TQUEUE_H
#define TQUEUE_H

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

template<typename T>
class TQueue
{
public:
    bool
    is_empty()
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        return queue_.empty();
    }

    void
    clear()
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (!queue_.empty())
        {
            queue_.pop();
        }
    }

    T
    pop()
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (queue_.empty())
        {
            cond_.wait(mlock);
        }
        auto item = queue_.front();
        queue_.pop();
        return item;
    }

    void
    pop(T &item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (queue_.empty())
        {
            cond_.wait(mlock);
        }
        item = queue_.front();
        queue_.pop();
    }

    void
    push(const T &item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        handled_ = false;
        queue_.push(item);
        mlock.unlock();
        cond_.notify_one();
    }

    void
    push(T &&item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        handled_ = false;
        queue_.push(std::move(item));
        mlock.unlock();
        cond_.notify_one();
    }

    void
    push_and_wait(const T &item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        handled_ = false;
        queue_.push(item);
        mlock.unlock();
        cond_.notify_one();
        wait_until_handled();
    }

    void
    push_and_wait(T &&item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        handled_ = false;
        queue_.push(std::move(item));
        mlock.unlock();
        cond_.notify_one();
        wait_until_handled();
    }

    void
    notify_handled()
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        handled_ = true;
        mlock.unlock();
        cond_handled_.notify_one();
    }

    void
    wait_until_handled()
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (!handled_)
        {
            cond_handled_.wait(mlock);
        }
    }

private:
    std::queue<T>           queue_;
    std::mutex              mutex_;
    std::condition_variable cond_;
    bool                    handled_ {};
    std::condition_variable cond_handled_;
};

#endif // TQUEUE_H
