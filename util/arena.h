#ifndef ARENA_H_
#define ARENA_H_

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace leveldb
{
/*
* 申请内存时，将申请到的内存块放入std::vector blocks_中，在Arena的生命周期结束后，统一释放掉所有申请到的内存
* 策略：
*    1.bytes < 当前块剩余内存 => 直接在当前块分配。
*    2.当前块剩余内存 < bytes < 1024 KB (默认内存块大小的 1 / 4) => 直接申请一个默认大小为 4096 KB 的内存块，然后分配内存。
*    3.bytes > 当前块剩余内存 && bytes > 1024 KB => 直接申请一个新的大小为 bytes 的内存块，并分配内存。
* 相关文章：https://www.jianshu.com/p/f5eebf44dec9
*/

class Arena
{
public:
    Arena();
    ~Arena();

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    
    // 基本的内存分配函数
    char* Allocate(size_t bytes);
    // 按照字节对齐来分配内存
    char* AllocateAligned(size_t bytes);
    // 返回目前分配的总的内存
    size_t MemoryUsage() const
    {
        return memory_usage_.load(std::memory_order_relaxed);
    }

private:
    // 内存分配策略
    char* AllocateFallback(size_t bytes);
    // new一个指定大小的内存，并加入blocks_管理
    char* AllocateNewBlock(size_t block_bytes);

    char* alloc_ptr_;               // 当前 block 内还未使用的起始地址
    size_t alloc_bytes_remaining_;  // 当前 block 里还有多少字节可以使用
    std::vector<char*> blocks_;     // 已经申请了的所有的内存块
    std::atomic<size_t> memory_usage_;// 总的内存使用量统计，memory_order没有采用默认的，采用relaxed，用在无多核交互的场景下。
};

inline char* Arena::Allocate(size_t bytes)
{
    assert(bytes > 0);
    // 申请的内存小于剩余的内存，就直接在当前内存块上分配内存
    if (bytes <= alloc_bytes_remaining_)
    {
        char* result = alloc_ptr_;
        alloc_ptr_ += bytes;                // 从当前块中分配
        alloc_bytes_remaining_ -= bytes;    // 计算当前块的剩余字节数
        return result;
    }
    // 申请的内存的大于当前内存块剩余的内存，就用这个函数来重新申请内存
    return AllocateFallback(bytes);
}

}  // namespace leveldb

#endif  // ARENA_H_
