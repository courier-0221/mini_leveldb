#include "log_writer.h"

#include <cstdint>

#include "env.h"
#include "coding.h"
#include "crc32c.h"

namespace leveldb {
namespace log {

// 计算crc值，存储到Writer中
static void InitTypeCrc(uint32_t* type_crc) {
  for (int i = 0; i <= kMaxRecordType; i++) {
    char t = static_cast<char>(i);
    type_crc[i] = crc32c::Value(&t, 1);
  }
}

Writer::Writer(WritableFile* dest) : dest_(dest), block_offset_(0) {
  InitTypeCrc(type_crc_);
}

Writer::Writer(WritableFile* dest, uint64_t dest_length)
    : dest_(dest), block_offset_(dest_length % kBlockSize) {
  InitTypeCrc(type_crc_);
}

Writer::~Writer() = default;

Status Writer::AddRecord(const Slice& slice) {
  const char* ptr = slice.data();
  size_t left = slice.size();

  Status s;
  // 表明这是第一条log record
  bool begin = true;
  do {
    // 如果当前 block 剩余空间不足容纳 record 的 header(7 字节) 则剩余空间作为 trailer 填充 0, 然后切换到新的 block.
    const int leftover = kBlockSize - block_offset_;
    assert(leftover >= 0);
    if (leftover < kHeaderSize) {
      if (leftover > 0) {
        static_assert(kHeaderSize == 7, "");
        dest_->Append(Slice("\x00\x00\x00\x00\x00\x00", leftover));
      }
      block_offset_ = 0;
    }

    // 到这一步, block 最终剩余字节必定大约等于 kHeaderSize
    assert(kBlockSize - block_offset_ - kHeaderSize >= 0);

    // 计算block剩余大小 avail，以及本次log record可写入数据长度 fragment_length
    const size_t avail = kBlockSize - block_offset_ - kHeaderSize;
    const size_t fragment_length = (left < avail) ? left : avail;

    // 根据两个值，判断log type
    RecordType type;
    // 判断是否将 record 剩余内容分片
    const bool end = (left == fragment_length);
    if (begin && end) {
      // 如果该 record 内容第一次写入文件, 而且, 
      // 如果 block 剩余空间可以容纳 record data 全部内容, 
      // 则写入一个 full 类型 record
      type = kFullType;
    } else if (begin) {
      // 如果该 record 内容第一写入文件, 而且, 
      // 如果 block 剩余空间无法容纳 record data 全部内容, 
      // 则写入一个 first 类型 record. 
      type = kFirstType;
    } else if (end) {
      // 如果这不是该 record 内容第一写入文件, 而且, 
      // 如果 block 剩余空间可以容纳 record data 剩余内容, 
      // 则写入一个 last 类型 record
      type = kLastType;
    } else {
      // 如果这不是该 record 内容第一写入文件, 而且, 
      // 如果 block 剩余空间无法容纳 record data 剩余内容, 
      // 则写入一个 middle 类型 record
      type = kMiddleType;
    }

    // 将类型为 type, data 长度为 fragment_length 的 record 写入 log 文件.并更新指针、剩余长度和begin标记
    s = EmitPhysicalRecord(type, ptr, fragment_length);
    ptr += fragment_length;
    left -= fragment_length;
    begin = false;
  } while (s.ok() && left > 0);
  return s;
}

Status Writer::EmitPhysicalRecord(RecordType t, const char* ptr,
                                  size_t length) {
  // data 大小必须能够被 16 位无符号整数表示, 因为 record 的 length 字段只有两字节                                  
  assert(length <= 0xffff);
  // 要写入的内容不能超过当前 block 剩余空间大小
  assert(block_offset_ + kHeaderSize + length <= kBlockSize);

  // buf 用于组装 record header，共7byte格式为：
  // | CRC32 (4 byte) | payload length lower + high (2 byte) |   type (1byte)|
  char buf[kHeaderSize];
  buf[4] = static_cast<char>(length & 0xff);
  buf[5] = static_cast<char>(length >> 8);
  buf[6] = static_cast<char>(t);

  // 计算 type 和 data 的 crc 并编码安排在最前面 4 个字节
  uint32_t crc = crc32c::Extend(type_crc_[t], ptr, length);
  crc = crc32c::Mask(crc);  // 空间调整
  // 将 crc 写入到 header 前四个字节
  EncodeFixed32(buf, crc);

  // 写入 header
  Status s = dest_->Append(Slice(buf, kHeaderSize));
  if (s.ok()) {
    // 写入 payload
    s = dest_->Append(Slice(ptr, length));
    if (s.ok()) {
      // 刷入文件
      s = dest_->Flush();
    }
  }
  block_offset_ += kHeaderSize + length;
  return s;
}

}  // namespace log
}  // namespace leveldb
