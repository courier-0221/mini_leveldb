#ifndef STORAGE_LEVELDB_UTIL_POSIX_LOGGER_H_
#define STORAGE_LEVELDB_UTIL_POSIX_LOGGER_H_

#include <sys/time.h>

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <sstream>
#include <thread>

#include "env.h"

namespace leveldb {

class PosixLogger final : public Logger {
 public:
  // 创建一个写入给定文件的记录器。
  explicit PosixLogger(std::FILE* fp) : fp_(fp) { assert(fp != nullptr); }

  ~PosixLogger() override { std::fclose(fp_); }

  void Logv(const char* format, std::va_list arguments) override {
    // 记录时间
    struct ::timeval now_timeval;
    ::gettimeofday(&now_timeval, nullptr);
    const std::time_t now_seconds = now_timeval.tv_sec;
    struct std::tm now_components;
    ::localtime_r(&now_seconds, &now_components);

    // 记录线程id
    constexpr const int kMaxThreadIdSize = 32;
    std::ostringstream thread_stream;
    thread_stream << std::this_thread::get_id();
    std::string thread_id = thread_stream.str();
    if (thread_id.size() > kMaxThreadIdSize) {
      thread_id.resize(kMaxThreadIdSize);
    }

    // 如果第一次分的栈内存不够，会第二次分配更大的堆内存。
    // 如果第二次分的内存还不够，只能对存放内容做截断处理了。
    constexpr const int kStackBufferSize = 512;
    char stack_buffer[kStackBufferSize];
    static_assert(sizeof(stack_buffer) == static_cast<size_t>(kStackBufferSize),
                  "sizeof(char) is expected to be 1 in C++");

    int dynamic_buffer_size = 0;  // Computed in the first iteration.
    for (int iteration = 0; iteration < 2; ++iteration) {
      const int buffer_size =
          (iteration == 0) ? kStackBufferSize : dynamic_buffer_size;
      char* const buffer =
          (iteration == 0) ? stack_buffer : new char[dynamic_buffer_size];
      
      // 将标头打印到缓冲区中 时间+线程id。
      int buffer_offset = std::snprintf(
          buffer, buffer_size, "%04d/%02d/%02d-%02d:%02d:%02d.%06d %s ",
          now_components.tm_year + 1900, now_components.tm_mon + 1,
          now_components.tm_mday, now_components.tm_hour, now_components.tm_min,
          now_components.tm_sec, static_cast<int>(now_timeval.tv_usec),
          thread_id.c_str());

      // 表头最多可以有 28 个字符（10 日期 + 15 时间 + 3 个分隔符）加上线程 ID
      assert(buffer_offset <= 28 + kMaxThreadIdSize);
      static_assert(28 + kMaxThreadIdSize < kStackBufferSize,
                    "stack-allocated buffer may not fit the message header");
      assert(buffer_offset < buffer_size);

      // Print the message into the buffer. 
      std::va_list arguments_copy;
      va_copy(arguments_copy, arguments);
      buffer_offset +=
          std::vsnprintf(buffer + buffer_offset, buffer_size - buffer_offset,
                         format, arguments_copy);
      va_end(arguments_copy);

      // 需要在缓冲区的末尾附加一个换行符，所以需要一个额外字符的空间。
      if (buffer_offset >= buffer_size - 1) {
        // 第一次操作不加入换行符，需要申请内存来处理
        if (iteration == 0) {
          // 重新运行循环并使用动态分配的缓冲区。缓冲区将足够大以容纳日志消息、额外的换行符和空终止符。
          dynamic_buffer_size = buffer_offset + 2;
          continue;
        }

        // 动态分配的缓冲区大小不正确。 假设 std::(v)snprintf 的正确实现，这不应该发生。
        // 测试失败，通过截断生产中的日志消息来恢复。
        assert(false);
        buffer_offset = buffer_size - 1;
      }

      // Add a newline if necessary.
      if (buffer[buffer_offset - 1] != '\n') {
        buffer[buffer_offset] = '\n';
        ++buffer_offset;
      }

      assert(buffer_offset <= buffer_size);
      std::fwrite(buffer, 1, buffer_offset, fp_);
      std::fflush(fp_);

      if (iteration != 0) {
        delete[] buffer;
      }
      break;
    }
  }

 private:
  std::FILE* const fp_;
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_UTIL_POSIX_LOGGER_H_
