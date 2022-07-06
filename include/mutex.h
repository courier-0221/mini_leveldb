#ifndef MUTEX_H_
#define MUTEX_H_

#include <condition_variable>
#include <mutex>
#include <string>
#include <assert.h>

namespace leveldb 
{

class CondVar;

class Mutex
{
public:
    Mutex() = default;
    ~Mutex() = default;

    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    void Lock() { mu_.lock(); }
    void Unlock() { mu_.unlock(); }

private:
    // 声明友元类，CondVar可以访问私有数据
    friend class CondVar;
    std::mutex mu_;
};

// Thinly wraps std::condition_variable.
class CondVar
{
public:
    explicit CondVar(Mutex* mu) : mu_(mu) { assert(mu != nullptr); }
    ~CondVar() = default;

    CondVar(const CondVar&) = delete;
    CondVar& operator=(const CondVar&) = delete;

    void Wait() {
        // unique_lock 具备 lock_guard (RAII)的所有功能，在使用条件变量时应当使用unique_lock
        // adopt_lock 假设当前线程已经获得互斥对象的所有权，所以不再请求锁
        std::unique_lock<std::mutex> lock(mu_->mu_, std::adopt_lock);
        cv_.wait(lock);
        lock.release(); // 返回所管理的mutex对象的指针，并释放所有权，但不改变mutex对象的状态
    }
    void Signal() { cv_.notify_one(); }
    void SignalAll() { cv_.notify_all(); }

private:
    std::condition_variable cv_;
    Mutex* const mu_;
};

// RAII 封装
class MutexLock
{
public:
    explicit MutexLock(Mutex* mu) : mu_(mu) {
        this->mu_->Lock();
    }
    ~MutexLock() { this->mu_->Unlock(); }

    MutexLock(const MutexLock&) = delete;
    MutexLock& operator=(const MutexLock&) = delete;
private:
    Mutex* const mu_;
};

}  // namespace leveldb

#endif  // MUTEX_H_
