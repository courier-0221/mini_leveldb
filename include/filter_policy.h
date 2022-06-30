#ifndef FILTER_POLICY_H_
#define FILTER_POLICY_H_

#include <string>

namespace leveldb
{

class Slice;

class FilterPolicy
{
public:
    virtual ~FilterPolicy() {};

    // 过滤策略的名字，用来唯一标识该 Filter 持久化、载入内存时的编码方法。
    virtual const char* Name() const = 0;

    // 给长度为 n 的 keys 集合（可能有重复）创建一个过滤策略，并将策略序列化为 string ，追加到 dst 最后。
    virtual void CreateFilter(const Slice* keys, int n, std::string* dst) const = 0;

    // “过滤” 函数。若调用 CreateFilter 时传入的集合为 keys，则如果 key 在 keys 中，则必须返回 true。
    // 若 key 不在 keys 中，可以返回 true，也可以返回 false，但最好大概率返回 false。
    virtual bool KeyMayMatch(const Slice& key, const Slice& filter) const = 0;
};

// Return a new filter policy that uses a bloom filter with approximately
// the specified number of bits per key.  A good value for bits_per_key
// is 10, which yields a filter with ~ 1% false positive rate.
//
// Callers must delete the result after any database that is using the
// result has been closed.
//
// Note: if you are using a custom comparator that ignores some parts
// of the keys being compared, you must not use NewBloomFilterPolicy()
// and must provide your own FilterPolicy that also ignores the
// corresponding parts of the keys.  For example, if the comparator
// ignores trailing spaces, it would be incorrect to use a
// FilterPolicy (like NewBloomFilterPolicy) that does not ignore
// trailing spaces in keys.
const FilterPolicy* NewBloomFilterPolicy(int bits_per_key);

}  // namespace leveldb

#endif  // FILTER_POLICY_H_
