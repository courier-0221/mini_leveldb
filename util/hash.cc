#include "hash.h"
#include "coding.h"
#include <cstring>

// The FALLTHROUGH_INTENDED macro can be used to annotate implicit fall-through
// between switch labels. The real definition should be provided externally.
// This one is a fallback version for unsupported compilers.
#ifndef FALLTHROUGH_INTENDED
#define FALLTHROUGH_INTENDED \
  do {                       \
  } while (0)
#endif

namespace leveldb
{
/*
*参数：
*    data-数组地址
*    n-数组大小
*    seed-随机数种子
*返回值：计算后的hash值，长度固定8字节大小
*/
uint32_t Hash(const char* data, size_t n, uint32_t seed)
{
    // Similar to murmur hash
    const uint32_t m = 0xc6a4a793;
    const uint32_t r = 24;
    const char* limit = data + n;   //指向最后一个元素的下一个位置
    uint32_t h = seed ^ (n * m);

    // 处理4字节
    while (data + 4 <= limit)
    {
        uint32_t w = DecodeFixed32(data);
        data += 4;
        h += w;
        h *= m;
        h ^= (h >> 16);
    }

    // 不足4字节
    switch (limit - data)
    {
    case 3:
        h += static_cast<uint8_t>(data[2]) << 16;
        FALLTHROUGH_INTENDED;
    case 2:
        h += static_cast<uint8_t>(data[1]) << 8;
        FALLTHROUGH_INTENDED;
    case 1:
        h += static_cast<uint8_t>(data[0]);
        h *= m;
        h ^= (h >> r);
        break;
    }
    return h;
}

}  // namespace leveldb
