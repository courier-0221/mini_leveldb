#include "arena.h"

namespace leveldb
{

static const int kBlockSize = 4096;

Arena::Arena()
    : alloc_ptr_(nullptr), alloc_bytes_remaining_(0), memory_usage_(0)
{}

Arena::~Arena()
{
    for (size_t i = 0; i < blocks_.size(); i++)
    {
        delete[] blocks_[i];
    }
}

char* Arena::AllocateFallback(size_t bytes)
{
    // 申请的内存大于当前块剩余内存，并且大于默认内存块大小的 1 / 4，直接申请一个需要的的 bytes 大小的内存块。
    if (bytes > kBlockSize / 4) 
    {
        char* result = AllocateNewBlock(bytes);
        return result;
    }

    // 当前块剩余内存 < 申请的内存 < 默认内存块大小的 1 / 4 (4096 KB / 4 = 1024 KB)，重新申请一个默认大小的内存块 (4096 KB)。
    alloc_ptr_ = AllocateNewBlock(kBlockSize);
    alloc_bytes_remaining_ = kBlockSize;

    //申请完了进行分配
    char* result = alloc_ptr_;
    alloc_ptr_ += bytes;
    alloc_bytes_remaining_ -= bytes;
    return result;
}

char* Arena::AllocateAligned(size_t bytes)
{
    // 设置要对齐的字节数，最多 8 字节对齐，否则就按照当前机器的 void* 的大小来对齐
    const int align = (sizeof(void*) > 8) ? sizeof(void*) : 8;

    // 字节对齐必须是 2 的次幂, 例如 align = 8
    // 8 & (8 - 1) = 1000 & 0111 = 0, 表示 8 是字节对齐的
    static_assert((align & (align - 1)) == 0, "Pointer size should be a power of 2");

    // 了解一个公式：A & (B - 1) = A % B
    // 所以，这句话的意思是将 alloc_ptr_ % align 的值强制转换成 uintptr_t 类型
    // 这个 uintptr_t 类型代表了当前机器的指针大小
    size_t current_mod = reinterpret_cast<uintptr_t>(alloc_ptr_) & (align - 1);

    // 如果上面的代码返回 0 代表当前 alloc_ptr_ 已经是字节对齐了
    // 否则就计算出对齐的偏差
    // 例如 current_mod = 2, 则还需要 8 - 2 = 6 个字节才能使得 alloc_ptr 按照 8 字节对齐
    size_t slop = (current_mod == 0 ? 0 : align - current_mod);

    // 分配的字节数加上对齐偏差就是最后需要分配的内存字节总量
    size_t needed = bytes + slop;
    char* result;

    // 分配逻辑同 Allocate 
    if (needed <= alloc_bytes_remaining_)
    {
        result = alloc_ptr_ + slop;
        alloc_ptr_ += needed;
        alloc_bytes_remaining_ -= needed;
    }
    else
    {
        result = AllocateFallback(bytes);
    }
    assert((reinterpret_cast<uintptr_t>(result) & (align - 1)) == 0);
    return result;
}

char* Arena::AllocateNewBlock(size_t block_bytes)
{
    char* result = new char[block_bytes];
    blocks_.push_back(result);
    memory_usage_.fetch_add(block_bytes + sizeof(char*), std::memory_order_relaxed);
    return result;
}

}  // namespace leveldb
