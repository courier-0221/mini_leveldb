#include "log_reader.h"

#include <cstdio>

#include "env.h"
#include "coding.h"
#include "crc32c.h"

namespace leveldb {
namespace log {

Reader::Reporter::~Reporter() = default;

Reader::Reader(SequentialFile* file, Reporter* reporter, bool checksum,
               uint64_t initial_offset)
    : file_(file),
      reporter_(reporter),
      checksum_(checksum),
      backing_store_(new char[kBlockSize]),
      buffer_(),
      eof_(false),
      last_record_offset_(0),
      end_of_buffer_offset_(0),
      initial_offset_(initial_offset),
      resyncing_(initial_offset > 0) {}

Reader::~Reader() { delete[] backing_store_; }

bool Reader::SkipToInitialBlock() {
  const size_t offset_in_block = initial_offset_ % kBlockSize;
  uint64_t block_start_location = initial_offset_ - offset_in_block;

  // Don't search a block if we'd be in the trailer
  if (offset_in_block > kBlockSize - 6) {
    block_start_location += kBlockSize;
  }

  end_of_buffer_offset_ = block_start_location;

  // Skip to start of first block that can contain the initial record
  if (block_start_location > 0) {
    Status skip_status = file_->Skip(block_start_location);
    if (!skip_status.ok()) {
      ReportDrop(block_start_location, skip_status);
      return false;
    }
  }

  return true;
}

bool Reader::ReadRecord(Slice* record, std::string* scratch) {
  // 如果条件成立, 表示当前方法是首次被调用，跳到我们要读取的第一个 block 起始位置
  if (last_record_offset_ < initial_offset_) {
    if (!SkipToInitialBlock()) {
      return false;
    }
  }

  scratch->clear();
  record->clear();
  // 指示正在处理的 record 是否被分片了, 
  // 除非逻辑 record 对应的物理 record 类型是 full, 否则就是被分片了.
  bool in_fragmented_record = false;
  uint64_t prospective_record_offset = 0;

  Slice fragment;
  while (true) {
    // 从文件读取一个物理 record 并将其 data 部分保存到 fragment, 
    // 同时返回该 record 的 type.
    const unsigned int record_type = ReadPhysicalRecord(&fragment);

    // 计算返回的当前 record 在 log file 中的起始地址
    //    = 当前文件待读取位置
    //      - buffer 剩余字节数
    //      - 刚读取的 record 头大小
    //      - 刚读取 record 数据部分大小
    // end_of_buffer_offset_ 表示 log file 待读取字节位置
    // buffer_ 表示是对一整个 block 数据的封装, 底层存储为 backing_store_, 
    //    每次执行 ReadPhysicalRecord 时会移动 buffer_ 指针.
    uint64_t physical_record_offset =
        end_of_buffer_offset_ - buffer_.size() - kHeaderSize - fragment.size();

    if (resyncing_) {
      if (record_type == kMiddleType) {
        continue;
      } else if (record_type == kLastType) {
        resyncing_ = false;
        continue;
      } else {
        resyncing_ = false;
      }
    }

    // 根据记录的类型来判断是否需要将当前记录附加到scratch后并继续读取
    switch (record_type) {
      // 类型为kFullType则说明当前是完整的记录，直接赋值给record后返回
      case kFullType:
        if (in_fragmented_record) {
          if (!scratch->empty()) {
            ReportCorruption(scratch->size(), "partial record without end(1)");
          }
        }
        prospective_record_offset = physical_record_offset;
        scratch->clear();
        *record = fragment;
        last_record_offset_ = prospective_record_offset;
        return true;

      // 类型为kFirstType则说明当前是第一部分，先将记录复制到scratch后继续读取
      case kFirstType:
        if (in_fragmented_record) {
          if (!scratch->empty()) {
            ReportCorruption(scratch->size(), "partial record without end(2)");
          }
        }
        prospective_record_offset = physical_record_offset;
        scratch->assign(fragment.data(), fragment.size());
        in_fragmented_record = true;
        break;

      // 类型为kMiddleType则说明当前是中间部分，先将记录追加到scratch后继续读取
      case kMiddleType:
        if (!in_fragmented_record) {
          ReportCorruption(fragment.size(),
                           "missing start of fragmented record(1)");
        } else {
          scratch->append(fragment.data(), fragment.size());
        }
        break;

      // 类型为kLastType则说明当前为最后，继续追加到scratch，并将scratch赋值给record并返回
      case kLastType:
        if (!in_fragmented_record) {
          ReportCorruption(fragment.size(),
                           "missing start of fragmented record(2)");
        } else {
          scratch->append(fragment.data(), fragment.size());
          *record = Slice(*scratch);
          last_record_offset_ = prospective_record_offset;
          return true;
        }
        break;

      // 如果状态为kEof、kBadRecord时说明日志损坏，此时清空scratch并返回false
      case kEof:
        if (in_fragmented_record) {
          // This can be caused by the writer dying immediately after
          // writing a physical record but before completing the next; don't
          // treat it as a corruption, just ignore the entire logical record.
          scratch->clear();
        }
        return false;

      case kBadRecord:
        if (in_fragmented_record) {
          ReportCorruption(scratch->size(), "error in middle of record");
          in_fragmented_record = false;
          scratch->clear();
        }
        break;

      // 未定义的类型，输出日志，剩余同上处理
      default: {
        char buf[40];
        std::snprintf(buf, sizeof(buf), "unknown record type %u", record_type);
        ReportCorruption(
            (fragment.size() + (in_fragmented_record ? scratch->size() : 0)),
            buf);
        in_fragmented_record = false;
        scratch->clear();
        break;
      }
    }
  }
  return false;
}

uint64_t Reader::LastRecordOffset() { return last_record_offset_; }

void Reader::ReportCorruption(uint64_t bytes, const char* reason) {
  ReportDrop(bytes, Status::Corruption(reason));
}

void Reader::ReportDrop(uint64_t bytes, const Status& reason) {
  if (reporter_ != nullptr &&
      end_of_buffer_offset_ - buffer_.size() - bytes >= initial_offset_) {
    reporter_->Corruption(static_cast<size_t>(bytes), reason);
  }
}

unsigned int Reader::ReadPhysicalRecord(Slice* result) {
  while (true) {
    // 如果buffer_小于block header大小kHeaderSize，进入如下的几个分支
    if (buffer_.size() < kHeaderSize) {
      if (!eof_) {
        // 如果eof_为false，表明还没有到文件结尾，清空buffer，并读取数据。
        buffer_.clear();
        Status status = file_->Read(kBlockSize, &buffer_, backing_store_);
        end_of_buffer_offset_ += buffer_.size();
        if (!status.ok()) {
          buffer_.clear();
          ReportDrop(kBlockSize, status);
          eof_ = true;
          return kEof;
        } else if (buffer_.size() < kBlockSize) {
          eof_ = true;
        }
        continue;
      } else {
        // 如果buffer_非空，我们在文件末尾有一个截断的头部，这可能是由于写入器在写入头部的过程中崩溃造成的。 
        // 这不是一个错误，只需报告 EOF。
        buffer_.clear();
        return kEof;
      }
    }

    // 解析头部
    const char* header = buffer_.data();
    const uint32_t a = static_cast<uint32_t>(header[4]) & 0xff;
    const uint32_t b = static_cast<uint32_t>(header[5]) & 0xff;
    const unsigned int type = header[6];
    const uint32_t length = a | (b << 8);
    // 长度超出了，汇报错误
    if (kHeaderSize + length > buffer_.size()) {
      size_t drop_size = buffer_.size();
      buffer_.clear();
      if (!eof_) {
        ReportCorruption(drop_size, "bad record length");
        return kBadRecord;
      }
      // 如果没有读满 header 中记录的 length 就读到了文件末尾，有可能是写入端在写记录过程中死亡，不需要报告异常
      return kEof;
    }

    if (type == kZeroType && length == 0) {
      // 对于Zero Type类型，不汇报错误
      buffer_.clear();
      return kBadRecord;
    }

    // 校验CRC32，如果校验出错，则汇报错误，并返回kBadRecord。
    if (checksum_) {
      uint32_t expected_crc = crc32c::Unmask(DecodeFixed32(header));
      // crc 是基于 type 和 data 来计算的
      uint32_t actual_crc = crc32c::Value(header + 6, 1 + length);
      if (actual_crc != expected_crc) {
        size_t drop_size = buffer_.size();
        buffer_.clear();
        ReportCorruption(drop_size, "checksum mismatch");
        return kBadRecord;
      }
    }

    // header 解析完毕, 将当前 record 从 buffer_ 中移除(通过向前移动 buffer_ 底层存储指针实现)
    buffer_.remove_prefix(kHeaderSize + length);

    // 如果record的开始位置在initial offset之前，则跳过，并返回kBadRecord，否则返回record数据和type。
    if (end_of_buffer_offset_ - buffer_.size() - kHeaderSize - length <
        initial_offset_) {
      result->clear();
      return kBadRecord;
    }

    *result = Slice(header + kHeaderSize, length);
    return type;
  }
}

}  // namespace log
}  // namespace leveldb
