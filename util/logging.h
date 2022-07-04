#ifndef STORAGE_LEVELDB_UTIL_LOGGING_H_
#define STORAGE_LEVELDB_UTIL_LOGGING_H_

#include <cstdint>
#include <cstdio>
#include <string>

namespace leveldb {

class Slice;

// uint64_t ==> string
void AppendNumberTo(std::string* str, uint64_t num);

// uint64_t ==> string
std::string NumberToString(uint64_t num);

// Slice ==> uint64_t
bool ConsumeDecimalNumber(Slice* in, uint64_t* val);

// ASCII字符集由95个可打印字符（0x20-0x7E）(' ' - '~') 和 33个控制字符（0x00-0x1F，0x7F）组成。
// Slice ==> string，如果Slice中存在不可以打印的字符则转义下
void AppendEscapedStringTo(std::string* str, const Slice& value);

// Slice ==> string    call AppendEscapedStringTo
std::string EscapeString(const Slice& value);

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_UTIL_LOGGING_H_
