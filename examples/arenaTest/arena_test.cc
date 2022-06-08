#include "arena.h"
#include "random.h"
#include "gtest/gtest.h"

namespace leveldb
{

TEST(ArenaTest, Empty)
{ 
    Arena arena;
}

TEST(ArenaTest, Simple)
{
    // 记录已经申请的内存 <size, ptr>
    std::vector<std::pair<size_t, char*>> allocated;
    Arena arena;
    const int N = 100000;
    size_t bytes = 0;
    Random rnd(301);
    for (int i = 0; i < N; i++)
    {
        // 对申请内存的size大小整活
        size_t s;
        if (i % (N / 10) == 0)
        {
            s = i;
        }
        else
        {
            s = rnd.OneIn(4000) ? rnd.Uniform(6000)
                : (rnd.OneIn(10) ? rnd.Uniform(100) : rnd.Uniform(20));
        }
        if (s == 0)
        {
            // Our arena disallows size 0 allocations.
            s = 1;
        }

        // 申请
        char* r;
        if (rnd.OneIn(10))  // size是10的倍数走对齐申请
        {
            r = arena.AllocateAligned(s);
        }
        else    // 否则走常规申请
        {
            r = arena.Allocate(s);
        }

        // 对申请下来的内存进行填充数据
        for (size_t b = 0; b < s; b++)
        {
            // Fill the "i"th allocation with a known bit pattern
            r[b] = i % 256;
        }
        bytes += s;
        // 本地记录
        allocated.push_back(std::make_pair(s, r));
        // 比较 这里由于arena会有内存浪费的情况，所以 MemoryUsage() 肯定大于 bytes
        ASSERT_GE(arena.MemoryUsage(), bytes);
        if (i > N / 10)
        {
            ASSERT_LE(arena.MemoryUsage(), bytes * 1.10);
        }
    }

    // 读取数据正确性对比
    for (size_t i = 0; i < allocated.size(); i++)
    {
        size_t num_bytes = allocated[i].first;
        const char* p = allocated[i].second;
        for (size_t b = 0; b < num_bytes; b++)
        {
            // Check the "i"th allocation for the known bit pattern
            ASSERT_EQ(int(p[b]) & 0xff, i % 256);
        }
    }
}

}  // namespace leveldb
