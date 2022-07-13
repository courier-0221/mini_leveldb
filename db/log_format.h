#ifndef STORAGE_LEVELDB_DB_LOG_FORMAT_H_
#define STORAGE_LEVELDB_DB_LOG_FORMAT_H_

namespace leveldb {
namespace log {

// Log 文件以块为基本单位，一条记录可能全部写到一个块上，也可能跨几个块。
enum RecordType {
  kZeroType = 0,    // 为预分配的文件保留。

  kFullType = 1,    // 表示一条记录完整地写到了一个块上。

  // For fragments
  kFirstType = 2,   // 说明是user record的第一条log record
  kMiddleType = 3,  // 说明是user record中间的log record
  kLastType = 4     // 说明是user record最后的一条log record
};
static const int kMaxRecordType = kLastType;

static const int kBlockSize = 32768;

// Header is checksum (4 bytes), length (2 bytes), type (1 byte).
static const int kHeaderSize = 4 + 2 + 1;

}  // namespace log
}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_LOG_FORMAT_H_
