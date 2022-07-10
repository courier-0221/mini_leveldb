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
  // Create a writer that will append data to "*dest".
  // "*dest" must be initially empty.
  // "*dest" must remain live while this Writer is in use.
  explicit Writer(WritableFile* dest);

  // 创建一个写入器，将数据附加到“*dest”。
  // "*dest" 必须有初始长度 "dest_length"。
  // "*dest" 必须在此 Writer 正在使用时保持活动状态。
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
