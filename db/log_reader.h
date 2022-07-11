#ifndef STORAGE_LEVELDB_DB_LOG_READER_H_
#define STORAGE_LEVELDB_DB_LOG_READER_H_

#include <cstdint>

#include "log_format.h"
#include "slice.h"
#include "status.h"

namespace leveldb {

class SequentialFile;

namespace log {

class Reader {
 public:
  // 上报错误接口
  class Reporter {
   public:
    virtual ~Reporter();

    // 检测到一些损坏。 “bytes”是由于损坏而丢弃的大约字节数。
    virtual void Corruption(size_t bytes, const Status& status) = 0;
  };

  // 创建一个 Reader 来从 file 中读取和解析 records, 
  // 读取的第一个 record 的起始位置位于文件 initial_offset 或其之后的物理地址. 
  // 如果 reporter 不为空, 则在检测到数据损坏时汇报要丢弃的数据估计大小. 
  // 如果 checksum 为 true, 则在可行的条件比对校验和. 
  // 注意, file 和 reporter 的生命期不能短于 Reader 对象. 
  Reader(SequentialFile* file, Reporter* reporter, bool checksum,
         uint64_t initial_offset);

  Reader(const Reader&) = delete;
  Reader& operator=(const Reader&) = delete;

  ~Reader();

  // 从文件读取下一个 record 到 *record 中. 如果读取成功, 返回 true; 遇到文件尾返回 false. 
  // 如果当前读取的 record 没有被分片, 那就用不到 *scratch 参数来为 *record 做底层存储了;
  // 其它情况需要借助 *scratch 来拼装分片的 record data 部分, 最后封装为一个 Slice 赋值给 *record.
  bool ReadRecord(Slice* record, std::string* scratch);

  // Returns the physical offset of the last record returned by ReadRecord.
  //
  // Undefined before the first call to ReadRecord.
  uint64_t LastRecordOffset();

 private:
  // Extend record types with the following special values
  enum {
    kEof = kMaxRecordType + 1,
    // Returned whenever we find an invalid physical record.
    // Currently there are three situations in which this happens:
    // * The record has an invalid CRC (ReadPhysicalRecord reports a drop)
    // * The record is a 0-length record (No drop is reported)
    // * The record is below constructor's initial_offset (No drop is reported)
    kBadRecord = kMaxRecordType + 2
  };

  // Skips all blocks that are completely before "initial_offset_".
  //
  // Returns true on success. Handles reporting.
  bool SkipToInitialBlock();

  // Return type, or one of the preceding special values
  unsigned int ReadPhysicalRecord(Slice* result);

  // Reports dropped bytes to the reporter.
  // buffer_ must be updated to remove the dropped bytes prior to invocation.
  void ReportCorruption(uint64_t bytes, const char* reason);
  void ReportDrop(uint64_t bytes, const Status& reason);

  SequentialFile* const file_;
  Reporter* const reporter_;
  bool const checksum_;
  char* const backing_store_;
  Slice buffer_;    // 读取的内容
  bool eof_;        // 上次Read()返回长度< kBlockSize，暗示到了文件结尾EOF

  // 表示上一次调用 ReadRecord 方法返回的 record 的起始偏移量
  uint64_t last_record_offset_;
  // 当前的读取偏移
  uint64_t end_of_buffer_offset_;
  // 表示用户创建 Reader 时指定的在文件中寻找第一个 record 的起始地址.
  uint64_t const initial_offset_;

  // resyncing_ 用于跳过起始地址不符合 initial_offset_ 的 record,
  // 如果为 true 表示目前还在定位第一个满足条件的逻辑 record 中.
  bool resyncing_;
};

}  // namespace log
}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_LOG_READER_H_
