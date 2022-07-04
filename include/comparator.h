#ifndef STORAGE_LEVELDB_INCLUDE_COMPARATOR_H_
#define STORAGE_LEVELDB_INCLUDE_COMPARATOR_H_

#include <string>

namespace leveldb {

class Slice;

// 比较器提供sstable或者database中key的排序方法。
// 一个比较器的实现必须是线程安全的，因为leveldb可能会同时从多个线程调用比较器的方法。
class Comparator {
 public:
  virtual ~Comparator();

  // Three-way comparison.  Returns value:
  //   < 0 iff "a" < "b",
  //   == 0 iff "a" == "b",
  //   > 0 iff "a" > "b"
  virtual int Compare(const Slice& a, const Slice& b) const = 0;

  // 比较器的名字，用来检查比较器能不能match上（比如数据库创建时使用的名字和后来使用的不一致）
  // 如果比较器实的实现改变了并且会造成任何多个key的相关顺序改变，那客户端就应该换一个新名字。
  // 以'level.'开头的名字被保留，客户端不要使用
  virtual const char* Name() const = 0;

  // 高级方法：它们用于减少索引块等内部数据结构的空间需求。

  // 如果*start < limit，改变*start变成一个短字符串[start,limit)
  virtual void FindShortestSeparator(std::string* start,
                                     const Slice& limit) const = 0;
  // 将*key变成一个比原*key大的短字符串，并赋值给*key返回。
  virtual void FindShortSuccessor(std::string* key) const = 0;
};

// 返回一个内置的使用字典序的比较器
// 返回结果仍然是这个模块的属性，不能删除。
const Comparator* BytewiseComparator();

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_INCLUDE_COMPARATOR_H_
