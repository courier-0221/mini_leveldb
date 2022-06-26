#ifndef CACHE_H_
#define CACHE_H_

#include <cstdint>
#include "slice.h"

namespace leveldb
{

/**********************
辅助函数
**********************/
void helpPrint(void* handle);

class Cache;

// 创建具有固定大小容量的缓存。缓存使用 least-recently-used 策略进行淘汰
Cache* NewLRUCache(size_t capacity);

class Cache
{
public:
    Cache() = default;

    Cache(const Cache&) = delete;
    Cache& operator=(const Cache&) = delete;

    // 通过调用 deleter 删除所有的 entry。
    virtual ~Cache();

    // Cache 中记录得每一项 entry ,这里定义为空结构体代表通用接口。
    struct Handle {};

    // 将一个对应 key-value 的 entry 插入缓存；
    // 返回 entry 对应的 handle，当不再需要返回的 handle，调用者必须调用 this->Release(handle)；
    // entry 被删除时，key 和 value 将会传递给 deleter。
    virtual Handle* Insert(const Slice& key, void* value, size_t charge,
                            void (*deleter)(const Slice& key, void* value)) = 0;

    // 如果当前缓存中没有对应 key 的 entry，返回 nullptr；
    // 否则返回 entry 对应的 handle，当不再需要返回的 handle 时，调用者必须调用 this->Release(handle)。
    virtual Handle* Lookup(const Slice& key) = 0;

    // 释放调用 this->Lookup 查找到的 handle：对应 entry 的引用计数减 1；
    // 若引用计数为 0，调用 deleter 删除该 entry。
    // 注意：handle 必须尚未释放。
    virtual void Release(Handle* handle) = 0;

    // 返回调用 this->Lookup(key) 查找到的 handle 中封装的值。
    // 注意：handle 必须尚未释放。
    virtual void* Value(Handle* handle) = 0;

    // 从缓存删除 key 对应的 entry。
    // 注意：entry 将一直保留，直到 entry 对应的所有 handle 都被 this->Release(handle) 释放。
    virtual void Erase(const Slice& key) = 0;

    // 返回一个新的数字 id。
    // 共享同一缓存的多个客户端可以使用该标识对保存 key 的空间进行分区。
    // 通常客户端将在启动时分配一个新的 id，并预先将该 id 添加到它保存 key 的缓存中。
    virtual uint64_t NewId() = 0;

    // 删除所有不活跃的 entry。
    // 内存受限的应用程序可能希望调用此方法以减少内存使用。
    virtual void Prune() {}

    // 返回缓存内存消耗的估计值。
    virtual size_t TotalCharge() const = 0;
};

}  // namespace leveldb

#endif  // CACHE_H_
