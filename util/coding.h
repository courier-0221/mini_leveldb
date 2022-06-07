#ifndef CODING_H_
#define CODING_H_

#include <cstdint>
#include "slice.h"

/*
*编码方式分为FixedInt定长和VarInt变长编码两种
*变长编码目的是为了减少空间占用，其基本思想是：每一个Byte最高位bit用0/1表示该整数是否结束，用剩余7bit表示实际的数值。
*/

namespace leveldb
{

// int/slice ==> std::string
// int32/int64
void PutFixed32(std::string* dst, uint32_t value);
void PutFixed64(std::string* dst, uint64_t value);
void PutVarint32(std::string* dst, uint32_t value);
void PutVarint64(std::string* dst, uint64_t value);
// slice.size(encode)&slice.data 
void PutLengthPrefixedSlice(std::string* dst, const Slice& value);

// slice ==> int/slice
bool GetVarint32(Slice* input, uint32_t* value);
bool GetVarint64(Slice* input, uint64_t* value);
bool GetLengthPrefixedSlice(Slice* input, Slice* result);

// 有效位
int VarintLength(uint64_t v);

/* 变长 编码 & 解码 */
// 编码
char* EncodeVarint32(char* dst, uint32_t value);
char* EncodeVarint64(char* dst, uint64_t value);
// 解码 
//参数 p:开始 limit:结束 解析完成返回nullptr
const char* GetVarint64Ptr(const char* p, const char* limit, uint64_t* v);
const char* GetVarint32PtrFallback(const char* p, const char* limit, uint32_t* value);
inline const char* GetVarint32Ptr(const char* p, const char* limit,  uint32_t* value)
{
    if (p < limit)
    {
        uint32_t result = *(reinterpret_cast<const uint8_t*>(p));
        // 第8位为0说明该整数已到末尾，返回这7位计算出的值
        if ((result & 128) == 0) {
            *value = result;
            return p + 1;
        }
    }
    // 第8位不为0则继续
    return GetVarint32PtrFallback(p, limit, value);
}


/* 定长 编码 & 解码 */
inline void EncodeFixed32(char* dst, uint32_t value)
{
    uint8_t* const buffer = reinterpret_cast<uint8_t*>(dst);
    buffer[0] = static_cast<uint8_t>(value);
    buffer[1] = static_cast<uint8_t>(value >> 8);
    buffer[2] = static_cast<uint8_t>(value >> 16);
    buffer[3] = static_cast<uint8_t>(value >> 24);
}

inline void EncodeFixed64(char* dst, uint64_t value)
{
    uint8_t* const buffer = reinterpret_cast<uint8_t*>(dst);
    buffer[0] = static_cast<uint8_t>(value);
    buffer[1] = static_cast<uint8_t>(value >> 8);
    buffer[2] = static_cast<uint8_t>(value >> 16);
    buffer[3] = static_cast<uint8_t>(value >> 24);
    buffer[4] = static_cast<uint8_t>(value >> 32);
    buffer[5] = static_cast<uint8_t>(value >> 40);
    buffer[6] = static_cast<uint8_t>(value >> 48);
    buffer[7] = static_cast<uint8_t>(value >> 56);
}

inline uint32_t DecodeFixed32(const char* ptr)
{
    const uint8_t* const buffer = reinterpret_cast<const uint8_t*>(ptr);
    return (static_cast<uint32_t>(buffer[0])) |
            (static_cast<uint32_t>(buffer[1]) << 8) |
            (static_cast<uint32_t>(buffer[2]) << 16) |
            (static_cast<uint32_t>(buffer[3]) << 24);
}

inline uint64_t DecodeFixed64(const char* ptr)
{
    const uint8_t* const buffer = reinterpret_cast<const uint8_t*>(ptr);
    return (static_cast<uint64_t>(buffer[0])) |
            (static_cast<uint64_t>(buffer[1]) << 8) |
            (static_cast<uint64_t>(buffer[2]) << 16) |
            (static_cast<uint64_t>(buffer[3]) << 24) |
            (static_cast<uint64_t>(buffer[4]) << 32) |
            (static_cast<uint64_t>(buffer[5]) << 40) |
            (static_cast<uint64_t>(buffer[6]) << 48) |
            (static_cast<uint64_t>(buffer[7]) << 56);
}

}  // namespace leveldb

#endif  // CODING_H_
