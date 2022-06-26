#include "cache.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "hash.h"
#include "mutex.h"

namespace leveldb
{

Cache::~Cache() {}

// LRUHandle 是一个双向循环链表，存储缓存中的 entry(记录)，它的功能有:
// 1.存储 key-value 数据；
// 2.作为 LRU 链表，是一个可变长度的堆分配结构；
// 3.记录引用计数并负责清理。
struct LRUHandle
{
    // 存储的 value 对象的指针，可以存储任意类型的值
    void* value;
    //删除器。当refs == 0时，调用deleter完成value对象释放。
    void (*deleter)(const Slice&, void* value);
    
    // HashTable中使用：HashTable中的节点使用该字段进行串联链表
    LRUHandle* next_hash;
    
    // LRU 链表双向指针
    LRUHandle* next;
    LRUHandle* prev;
    
    size_t charge;      // 用户指定占用缓存的大小
    
    size_t key_length;  // key 的字节数
    bool in_cache;      // 是否在LRUCache in_use_ 链表
    uint32_t refs;      // 引用计数
    uint32_t hash;      // key()的哈希值; 用于快速分片和比较

    // 存储 key 的开始地址，结合 `key_length` 获取真正的 key 
    // GCC 支持 C99 标准，允许定义 char key_data[] 这样的柔性数组（Flexible Array)。
    // C++ 标准并不支持柔性数组的实现，这里定义为 key_data[1]，这也是 c++ 中的标准做法。
    char key_data[1];

    Slice key() const {
        // 只有当当前节点是空链表头时，`next` 才会等于 `this`
        // 链表是循环双向链表，空链表的头节点 next 和 prev 都指向自己构成环
        // 表头不会存在有意义的 key，只利用它的 next 和 prev
        assert(next != this);
        return Slice(key_data, key_length);
    }
};

// 作者实现的 hashtable 比一些内置的 哈希表实现快
// 每一个 LRUHandle 链表是 HashTable 中的一个哈希桶，低位相同哈希值的 entry 通过 next_hash 连成链表
class HandleTable
{
public:
    HandleTable() : length_(0), elems_(0), list_(nullptr) { Resize(); }
    ~HandleTable() { delete[] list_; }

    LRUHandle* Lookup(const Slice& key, uint32_t hash)
    {
        return *FindPointer(key, hash);
    }

    LRUHandle* Insert(LRUHandle* h)
    {
        LRUHandle** ptr = FindPointer(h->key(), h->hash);
        LRUHandle* old = *ptr;
        h->next_hash = (old == nullptr ? nullptr : old->next_hash);
        *ptr = h;
        if (old == nullptr)
        {
            ++elems_;
            if (elems_ > length_)
            {
                Resize();
            }
        }
        return old;
    }

    LRUHandle* Remove(const Slice& key, uint32_t hash)
    {
        LRUHandle** ptr = FindPointer(key, hash);
        LRUHandle* result = *ptr;
        if (result != nullptr)
        {
            *ptr = result->next_hash;
            --elems_;
        }
        return result;
    }

private:

    // 如果某个 LRUHandle* 存在相同的 hash/key 值，则返回该 LRUHandle* 的二级指针，即指向上一个LRUHandle*的next_hash的二级指针.
    // 如果不存在这样的 LRUHandle*，则返回指向该bucket的最后一个LRUHandle*的next_hash的二级指针，其值为nullptr.
    // 返回next_hash地址的作用是可以直接修改该值，因此起到修改链表的作用
    LRUHandle** FindPointer(const Slice& key, uint32_t hash)
    {
        // list_[hash & (length_ - 1)] 中存储着 next_hash 的地址
        // ptr 等于 next_hash 的地址
        // *ptr 等于其指向的 next_hash，即指向 next_hash 所连成的链表
        // **ptr 等于 next_hash 所指向的 LRUHandle，即指向具体的 entry

        // 定位到是哪个桶
        LRUHandle** ptr = &list_[hash & (length_ - 1)];
        while (*ptr != nullptr && ((*ptr)->hash != hash || key != (*ptr)->key()))
        {
            ptr = &(*ptr)->next_hash;
        }
        return ptr;
    }

    void Resize()
    {
        // 初始化时，哈希桶的个数为 4
        uint32_t new_length = 4;

        // 随着不同哈希值的增加动态扩展，每次扩展为原容量的两倍
        while (new_length < elems_)
        {
            new_length *= 2;
        }

        // 按照新容量申请内存，分配给新list，并将原数据复制过去
        LRUHandle** new_list = new LRUHandle*[new_length];
        memset(new_list, 0, sizeof(new_list[0]) * new_length);
        uint32_t count = 0;
        // 容量发生变化时，将旧list上每一层的每一个 entry 放到新链表上
        for (uint32_t i = 0; i < length_; i++)
        {
            LRUHandle* h = list_[i];
            while (h != nullptr)
            {
                LRUHandle* next = h->next_hash;
                uint32_t hash = h->hash;
                // 旧list中的 entry 放到新 list 上的哪一层的依据为 &操作，区间在[0，new_length - 1]
                LRUHandle** ptr = &new_list[hash & (new_length - 1)];
                h->next_hash = *ptr;
                *ptr = h;
                h = next;
                count++;
            }
        }
        assert(elems_ == count);
        delete[] list_;
        list_ = new_list;
        length_ = new_length;
    }

private:
    // 指针数组的大小，即哈希桶的个数
    uint32_t length_;
    // 存储的 entry 总个数，当 elems > length 时，扩展 list 数组并重新 hash
    uint32_t elems_;
    // 存储 entry 的指针数组，每一层为一个哈希桶，初始大小为 4层，动态扩展，成倍增长
    LRUHandle** list_;
};

// The cache 内部维护了两条链表，一条in_use，一条lru。所有的item只能位于一条链表上。如果item被删了，但是客户端仍然引用，则不位于任何一条链上。
// in-use: 保存了经常被引用的items，无序存放。
// LRU: 保存了不经常被引用的items，LRU顺序存放。
class LRUCache
{
public:
    LRUCache();
    ~LRUCache();

    //与构造函数分离，以便调用方可以轻松地生成LRUCache数组
    void SetCapacity(size_t capacity) { capacity_ = capacity; }

    // Like Cache methods, but with an extra "hash" parameter.
    Cache::Handle* Insert(const Slice& key, uint32_t hash, void* value,
                        size_t charge,
                        void (*deleter)(const Slice& key, void* value));
    Cache::Handle* Lookup(const Slice& key, uint32_t hash);
    void Release(Cache::Handle* handle);
    void Erase(const Slice& key, uint32_t hash);
    void Prune();
    size_t TotalCharge() const
    {
        MutexLock l(&mutex_);
        return usage_;
    }

private:
    // 双向链表的节点的添加与删除
    void LRU_Remove(LRUHandle* e);
    void LRU_Append(LRUHandle* list, LRUHandle* e);
    // entry 引用计数的增加与减少
    void Ref(LRUHandle* e);
    void Unref(LRUHandle* e);   // ref==0 调用deleter / ref==1&&in_cache==TRUE 将e从in_use_挪到lru_中
    // 配合table_使用(insert/lookup/remove)，将内存归还给 usage_ && Unref
    bool FinishErase(LRUHandle* e);

private:
    // 缓存容量
    size_t capacity_;   

    // 互斥锁，保护下列数据
    mutable Mutex mutex_;
    
    // 当前lrucache已经使用的内存
    // usage_ 必须小于 capacity
    size_t usage_ ;

    // 缓存中的节点
    // lru.next 指向最旧的 entry，lru.prev 指向最新的 entry
    // 当缓存满了时，先从 lru.next 开始淘汰 entry
    // lru 保存 refs==1，并且 in_cache==true 的 entry
    LRUHandle lru_ ;

    // 当前正在被使用的缓存项链表
    // entries 被客户使用 并且 refs>=2 and in_cache==true
    LRUHandle in_use_ ;

    // 保存所有 entry 的哈希表，用于快速查找数据
    HandleTable table_ ;
};

LRUCache::LRUCache() : capacity_(0), usage_(0)
{
    // lru 和 in_use 都是循环双向链表
    // 空链表的头节点 next 和 prev 都指向自己构成环，链表头冗余
    lru_.next = &lru_;
    lru_.prev = &lru_;
    in_use_.next = &in_use_;
    in_use_.prev = &in_use_;
}

LRUCache::~LRUCache()
{
    // in_use 链表不能为空
    assert(in_use_.next == &in_use_);
    for (LRUHandle* e = lru_.next; e != &lru_;)
    {
        LRUHandle* next = e->next;
        assert(e->in_cache);
        e->in_cache = false;
        // entry 的引用必须为1
        assert(e->refs == 1);
        Unref(e);
        e = next;
    }
}

void LRUCache::Ref(LRUHandle* e)
{
    if (e->refs == 1 && e->in_cache)    // 如果当前在lru_里，移动到in_use_里
    {
        LRU_Remove(e);                  // 先从链表中移除
        LRU_Append(&in_use_, e);        // 插入到in_use_
    }
    e->refs++;
}

void LRUCache::Unref(LRUHandle* e)
{
    assert(e->refs > 0);
    e->refs--;

    if (e->refs == 0)
    {
        assert(!e->in_cache);
        (*e->deleter)(e->key(), e->value);
        free(e);
    }
    else if (e->in_cache && e->refs == 1)
    {
        // 重新移动到lru_里
        LRU_Remove(e);
        LRU_Append(&lru_, e);
    }
}

void LRUCache::LRU_Remove(LRUHandle* e)
{
    e->next->prev = e->prev;
    e->prev->next = e->next;
}

void LRUCache::LRU_Append(LRUHandle* list, LRUHandle* e)
{
    // 头插法，使 e 成为最新entry
    e->next = list;
    e->prev = list->prev;
    e->prev->next = e;
    e->next->prev = e;
}

Cache::Handle* LRUCache::Lookup(const Slice& key, uint32_t hash)
{
    MutexLock l(&mutex_);
    // 先查找table_，然后更新lru_ in_use链表
    LRUHandle* e = table_.Lookup(key, hash);
    if (e != nullptr)
    {
        Ref(e);
    }
    return reinterpret_cast<Cache::Handle*>(e);
}

void LRUCache::Release(Cache::Handle* handle)
{
    // 对 entry 解引用
    MutexLock l(&mutex_);
    Unref(reinterpret_cast<LRUHandle*>(handle));
}

Cache::Handle* LRUCache::Insert(const Slice& key, uint32_t hash, void* value,
                                size_t charge,
                                void (*deleter)(const Slice& key,
                                                void* value))
{
    MutexLock l(&mutex_);

    // 申请动态大小的LRUHandle内存，初始化该结构体
    LRUHandle* e = reinterpret_cast<LRUHandle*>(malloc(sizeof(LRUHandle) - 1 + key.size()));
    e->value = value;
    e->deleter = deleter;
    e->charge = charge;
    e->key_length = key.size();
    e->hash = hash;
    e->in_cache = false; 
    e->refs = 1;  // 返回handle，引用计数+1
    ::memcpy(e->key_data, key.data(), key.size());

    if (capacity_ > 0)
    {
        e->refs++;  // 加入缓存，引用计数+1
        e->in_cache = true;
        // 暂时理解cache就是in_use_链表,in_cache使能时需要向lru_cache中添加消耗usage_同时将entry加入到in_use_链表中
        LRU_Append(&in_use_, e);
        usage_ += charge;
        FinishErase(table_.Insert(e));
        //printf("refs.%d in_cache.%d\n", e->refs, e->in_cache);
    }
    else 
    {
        // 不缓存（支持 capacity==0，这表示关闭缓存
        // next 会影响 `key()` 中的断言，因此必须对其进行初始化
        e->next = nullptr;
    }

    // 如果超过了容量限制，根据lru_按照lru策略淘汰
    while (usage_ > capacity_ && lru_.next != &lru_)
    {
        // lru_.next是最老的节点，首先淘汰
        LRUHandle* old = lru_.next;
        assert(old->refs == 1);
        bool erased = FinishErase(table_.Remove(old->key(), old->hash));
        if (!erased)
        {  // to avoid unused variable when compiled NDEBUG
            assert(erased);
        }
    }

    return reinterpret_cast<Cache::Handle*>(e);
}

bool LRUCache::FinishErase(LRUHandle* e)
{
    if (e != nullptr)
    {
        assert(e->in_cache);
        LRU_Remove(e);
        e->in_cache = false;
        usage_ -= e->charge;
        //printf("key.%s value.%u refs.%d\n", e->key().data(), reinterpret_cast<uintptr_t>(e->value), e->refs);
        Unref(e);
    }
    return e != nullptr;
}

void LRUCache::Erase(const Slice& key, uint32_t hash)
{
    MutexLock l(&mutex_);
    FinishErase(table_.Remove(key, hash));
}

void LRUCache::Prune()
{
    MutexLock l(&mutex_);
    while (lru_.next != &lru_)
    {
        LRUHandle* e = lru_.next;
        assert(e->refs == 1);
        bool erased = FinishErase(table_.Remove(e->key(), e->hash));
        if (!erased)   // to avoid unused variable when compiled NDEBUG
        {
            assert(erased);
        }
    }
}





static const int kNumShardBits = 4;
static const int kNumShards = 1 << kNumShardBits;

class ShardedLRUCache : public Cache
{
public:
    explicit ShardedLRUCache(size_t capacity) : last_id_(0)
    {
        const size_t per_shard = (capacity + (kNumShards - 1)) / kNumShards;
        for (int s = 0; s < kNumShards; s++)
        {
            // 给 lru_cache 设置容量
            shard_[s].SetCapacity(per_shard);
        }
    }
    ~ShardedLRUCache() override {}

    // ShardedLRUCache 的接口实现非常简单，它自己只进行哈希值的计算
    // 然后选择相应的 LRUCache，调用相应 LRUCache 的相关接口实现。
    Handle* Insert(const Slice& key, void* value, size_t charge,
                    void (*deleter)(const Slice& key, void* value)) override
    {
        const uint32_t hash = HashSlice(key);
        return shard_[Shard(hash)].Insert(key, hash, value, charge, deleter);
    }
    Handle* Lookup(const Slice& key) override
    {
        const uint32_t hash = HashSlice(key);
        return shard_[Shard(hash)].Lookup(key, hash);
    }
    void Release(Handle* handle) override
    {
        LRUHandle* h = reinterpret_cast<LRUHandle*>(handle);
        shard_[Shard(h->hash)].Release(handle);
    }
    void Erase(const Slice& key) override
    {
        const uint32_t hash = HashSlice(key);
        shard_[Shard(hash)].Erase(key, hash);
    }
    void* Value(Handle* handle) override
    {
        return reinterpret_cast<LRUHandle*>(handle)->value;
    }
    uint64_t NewId() override
    {
        MutexLock l(&id_mutex_);
        return ++(last_id_);
    }
    void Prune() override
    {
        for (int s = 0; s < kNumShards; s++)
        {
            shard_[s].Prune();
        }
    }
    size_t TotalCharge() const override
    {
        size_t total = 0;
        for (int s = 0; s < kNumShards; s++)
        {
            total += shard_[s].TotalCharge();
        }
        return total;
    }

private:
    // 计算hash值
    static inline uint32_t HashSlice(const Slice& s)
    {
        return Hash(s.data(), s.size(), 0);
    }
    // 得到hash值得最高4位 作为数组下标，进行插入或者查询时使用该值判断当前应该在哪个 LRUCache 中操作
    static uint32_t Shard(uint32_t hash) { return hash >> (32 - kNumShardBits); }

private:
    LRUCache shard_[kNumShards];    // 16个LRUCache
    Mutex id_mutex_;
    uint64_t last_id_;
};

Cache* NewLRUCache(size_t capacity) { return new ShardedLRUCache(capacity); }

void helpPrint(void* handle)
{
    printf("refs.%d in_cache.%d\n", reinterpret_cast<LRUHandle*>(handle)->refs, reinterpret_cast<LRUHandle*>(handle)->in_cache);
}

}  // namespace leveldb
