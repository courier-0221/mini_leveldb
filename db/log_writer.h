#ifndef STORAGE_LEVELDB_DB_LOG_WRITER_H_
#define STORAGE_LEVELDB_DB_LOG_WRITER_H_

#include <cstdint>

#include "log_format.h"
#include "slice.h"
#include "status.h"

namespace leveldb {

class WritableFile;

namespace log {

class Writer {
 public:
  // 创建一个 writer 用于追加数据到 dest 指向的文件.
  // dest 指向的文件初始必须为空文件; dest 生命期不能短于 writer.
  explicit Writer(WritableFile* dest);

  // 创建一个 writer 用于追加数据到 dest 指向的文件.
  // dest 指向文件初始长度必须为 dest_length; dest 生命期不能短于 writer.
  Writer(WritableFile* dest, uint64_t dest_length);

  Writer(const Writer&) = delete;
  Writer& operator=(const Writer&) = delete;

  ~Writer();

  // 写入
  Status AddRecord(const Slice& slice);

 private:
  Status EmitPhysicalRecord(RecordType type, const char* ptr, size_t length);

  WritableFile* dest_; // 顺序写文件
  int block_offset_;  // Current offset in block

  // crc的值，预先计算出来，以减少计算开销
  uint32_t type_crc_[kMaxRecordType + 1];
};

}  // namespace log
}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_LOG_WRITER_H_
