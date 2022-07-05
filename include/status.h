#ifndef STATUS_H_
#define STATUS_H_

#include <algorithm>
#include "slice.h"

namespace leveldb
{

class Status
{
public:
    Status() noexcept : state_(nullptr) {}
    ~Status() { delete[] state_; }

    Status(const Status& rhs);
    Status& operator=(const Status& rhs);

    Status(Status&& rhs) noexcept : state_(rhs.state_) { rhs.state_ = nullptr; }
    Status& operator=(Status&& rhs) noexcept;

    // 构造Status 
    static Status OK() { return Status(); }
    static Status NotFound(const Slice& msg, const Slice& msg2 = Slice()) {
        return Status(kNotFound, msg, msg2);
    }
    static Status Corruption(const Slice& msg, const Slice& msg2 = Slice()) {
        return Status(kCorruption, msg, msg2);
    }
    static Status NotSupported(const Slice& msg, const Slice& msg2 = Slice()) {
        return Status(kNotSupported, msg, msg2);
    }
    static Status InvalidArgument(const Slice& msg, const Slice& msg2 = Slice()) {
        return Status(kInvalidArgument, msg, msg2);
    }
    static Status IOError(const Slice& msg, const Slice& msg2 = Slice()) {
        return Status(kIOError, msg, msg2);
    }
    
    // 错误类型判定
    bool ok() const { return (state_ == nullptr); }
    bool IsNotFound() const { return code() == kNotFound; }
    bool IsCorruption() const { return code() == kCorruption; }
    bool IsIOError() const { return code() == kIOError; }
    bool IsNotSupportedError() const { return code() == kNotSupported; }
    bool IsInvalidArgument() const { return code() == kInvalidArgument; }

    // 将_status内容转换成字符串
    std::string ToString() const;

private:
    // 错误类型
    enum Code
    {
        kOk = 0,
        kNotFound = 1,
        kCorruption = 2,
        kNotSupported = 3,
        kInvalidArgument = 4,
        kIOError = 5
    };
    // 获取code类型
    Code code() const
    {
        return (state_ == nullptr) ? kOk : static_cast<Code>(state_[4]);
    }

    // 构造函数
    Status(Code code, const Slice& msg, const Slice& msg2);
    //拷贝一份 state_
    static const char* CopyState(const char* s);

    // 成功状态OK state_ 是 NULL，否则 state_ 是一个包含如下信息的数组
    //    state_[0..3] == length of message
    //    state_[4]    == code
    //    state_[5..]  == message
    const char* state_;
};

inline Status::Status(const Status& rhs)
{
    state_ = (rhs.state_ == nullptr) ? nullptr : CopyState(rhs.state_);
}
inline Status& Status::operator=(const Status& rhs)
{
    if (state_ != rhs.state_)
    {
        delete[] state_;
        state_ = (rhs.state_ == nullptr) ? nullptr : CopyState(rhs.state_);
    }
    return *this;
}
inline Status& Status::operator=(Status&& rhs) noexcept
{
    std::swap(state_, rhs.state_);
    return *this;
}

}  // namespace leveldb

#endif  // STATUS_H_
