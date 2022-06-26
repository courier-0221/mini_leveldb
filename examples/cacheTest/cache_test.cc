#include "cache.h"
#include <vector>
#include "coding.h"

using namespace leveldb;

struct LRUHandle;

static std::string EncodeKey(int k)
{
    std::string result;
    PutFixed32(&result, k);
    return result;
}
static int DecodeKey(const Slice& k)
{
    assert(k.size() == 4);
    return DecodeFixed32(k.data());
}
static void* EncodeValue(uintptr_t v) { return reinterpret_cast<void*>(v); }
static int DecodeValue(void* v) { return reinterpret_cast<uintptr_t>(v); }

class CacheTest
{
public:
    // lru节点引用计数为0时的回调函数
    static void Deleter(const Slice& key, void* v)
    {
        current_->deleted_keys_.push_back(DecodeKey(key));
        current_->deleted_values_.push_back(DecodeValue(v));
    }

    CacheTest() : cache_(NewLRUCache(kCacheSize)) { current_ = this; }

    ~CacheTest() { delete cache_; }

    int Lookup(int key)
    {
        Cache::Handle* handle = cache_->Lookup(EncodeKey(key));
        const int r = (handle == nullptr) ? -1 : DecodeValue(cache_->Value(handle));
        if (handle != nullptr)
        {
            cache_->Release(handle);
        }
        return r;
    }

    void Insert(int key, int value, int charge = 1)
    {
        cache_->Release(cache_->Insert(EncodeKey(key), EncodeValue(value), charge,
                                    &CacheTest::Deleter));
    }

    Cache::Handle* InsertAndReturnHandle(int key, int value, int charge = 1)
    {
        return cache_->Insert(EncodeKey(key), EncodeValue(value), charge,
                            &CacheTest::Deleter);
    }

    void Erase(int key) { cache_->Erase(EncodeKey(key)); }

public:
    static constexpr int kCacheSize = 1000;
    std::vector<int> deleted_keys_; // lrucache中清空引用计数为0节点时需要用到的结构，起记录作用
    std::vector<int> deleted_values_;// 同上
    Cache* cache_;                  // 指向 ShardedLRUCache
    static CacheTest* current_;     // 指向当前 CacheTest，使用为传入引用计数为0时得辅助删除
};

CacheTest* CacheTest::current_;

void CacheTest_HitAndMiss(void)
{
    CacheTest ct;
    printf("lookup 100 ret=%d\n",ct.Lookup(100));
    printf("\n");
    
    ct.Insert(100, 101);
    printf("lookup 100 ret=%d\n",ct.Lookup(100));
    printf("lookup 200 ret=%d\n",ct.Lookup(200));
    printf("lookup 300 ret=%d\n",ct.Lookup(300));
    printf("\n");

    ct.Insert(200, 201);
    printf("lookup 100 ret=%d\n",ct.Lookup(100));
    printf("lookup 200 ret=%d\n",ct.Lookup(200));
    printf("lookup 300 ret=%d\n",ct.Lookup(300));
    printf("\n");

    // update
    ct.Insert(100, 102);
    printf("lookup 100 ret=%d\n",ct.Lookup(100));
    printf("lookup 200 ret=%d\n",ct.Lookup(200));
    printf("lookup 300 ret=%d\n",ct.Lookup(300));

    printf("deleted_keys_.size()=%lu\n",ct.deleted_keys_.size());
    printf("deleted_keys_[0]=%d\n",ct.deleted_keys_[0]);
    printf("deleted_values_[0]=%d\n",ct.deleted_values_[0]);
    printf("\n");
}

void CacheTest_Erase(void)
{
    CacheTest ct;
    ct.Erase(200);

    ct.Insert(100, 101);
    ct.Insert(200, 201);

    printf("lookup 100 ret=%d\n",ct.Lookup(100));
    printf("lookup 200 ret=%d\n",ct.Lookup(200));
    printf("\n");

    ct.Erase(100);
    printf("lookup 100 ret=%d\n",ct.Lookup(100));
    printf("lookup 200 ret=%d\n",ct.Lookup(200));

    printf("deleted_keys_.size()=%lu\n",ct.deleted_keys_.size());
    printf("deleted_keys_[0]=%d\n",ct.deleted_keys_[0]);
    printf("deleted_values_[0]=%d\n",ct.deleted_values_[0]);
    printf("\n");
}

void CacheTest_EntriesArePinned(void)
{
    printf("EncodeKey(100).%s\n", EncodeKey(100).data());
    // 直接使用CacheTest中的cache_成员操作，看release作用
    CacheTest ct;
    ct.Insert(100, 101);
    Cache::Handle* h1 = ct.cache_->Lookup(EncodeKey(100));
    printf("lookup 100 ret=%d\n",DecodeValue(ct.cache_->Value(h1)));
    helpPrint(h1);
    printf("\n");
    
    ct.Insert(100, 102);
    Cache::Handle* h2 = ct.cache_->Lookup(EncodeKey(100));
    printf("lookup 100 ret=%d\n",DecodeValue(ct.cache_->Value(h2)));
    printf("deleted_keys_.size()=%lu\n",ct.deleted_keys_.size());
    helpPrint(h1);  //旧元素h1会从cache中删除，refs会减一，此时refs==1 in_cache=false，但是没有触发删除回调，内存没有释放
    helpPrint(h2);
    printf("\n");

    ct.cache_->Release(h1); // 此刻才会真正的删除
    helpPrint(h1);
    printf("deleted_keys_.size()=%lu\n",ct.deleted_keys_.size());
    printf("deleted_keys_[0]=%d\n",ct.deleted_keys_[0]);
    printf("deleted_values_[0]=%d\n",ct.deleted_values_[0]);
    printf("\n");

    ct.Erase(100);

    ct.cache_->Release(h2);
    helpPrint(h2);
    printf("deleted_keys_.size()=%lu\n",ct.deleted_keys_.size());
    printf("deleted_keys_[0]=%d\n",ct.deleted_keys_[0]);
    printf("deleted_values_[0]=%d\n",ct.deleted_values_[0]);
    printf("\n");
}

void CacheTest_Prune(void)
{
    CacheTest ct;
    ct.Insert(1, 100);
    ct.Insert(2, 200);
    
    Cache::Handle* handle = ct.cache_->Lookup(EncodeKey(1));
    ct.cache_->Prune();
    ct.cache_->Release(handle);

    printf("lookup 100 ret=%d\n",ct.Lookup(1));
    printf("lookup 200 ret=%d\n",ct.Lookup(2));
}

int main(void)
{
    //CacheTest_HitAndMiss();
    //CacheTest_Erase();
    //CacheTest_EntriesArePinned();
    CacheTest_Prune();
    return 0;
}
