#ifndef SLICE_H
#define SLICE_H

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <string>

namespace leveldb
{
    
class Slice
{
public:
    Slice() : data_(""), size_(0) {}
    Slice(const char* d, size_t n) : data_(d), size_(n) {}
    Slice(const std::string& s) : data_(s.data()), size_(s.size()) {}
    Slice(const char* s) : data_(s), size_(strlen(s)) {}

    Slice(const Slice&) = default;
    Slice& operator=(const Slice&) = default;

    const char* data() const { return data_; }
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    char operator[](size_t n) const
    {
        assert(n < size());
        return data_[n];
    }

    void clear()
    {
        data_ = "";
        size_ = 0;
    }

    void remove_prefix(size_t n)
    {
        assert(n <= size());
        data_ += n;
        size_ -= n;
    }

    bool starts_with(const Slice& x) const
    {
        return size_ >= x.size() && ::memcmp(data_, x.data(), x.size()) == 0;
    }

    std::string ToString() const { return std::string(data_, size_); }

    //类似C中memcmp函数
    int compare(const Slice& b) const;

private:
    const char* data_;
    std::size_t size_;
};

inline bool operator==(const Slice& x, const Slice& y)
{
    return x.size() == y.size() && ::memcmp(x.data(), y.data(), x.size()) == 0;
}

inline bool operator!=(const Slice& x, const Slice& y)
{
    return !operator==(x, y);
}

inline int Slice::compare(const Slice& b) const
{
    const size_t min_size = size_ > b.size() ? b.size() : size_;
    int r = ::memcmp(data_, b.data_, min_size);
    if (r == 0)
    {
        (size_ < b.size_) ? (r = -1) : (r = +1);
    }
    return r;
}

}  // namespace leveldb

#endif  // SLICE_H