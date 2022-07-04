#ifndef STORAGE_LEVELDB_DB_MEMTABLE_H_
#define STORAGE_LEVELDB_DB_MEMTABLE_H_

#include <string>
#include "skiplist.h"
#include "arena.h"
#include "dbformat.h"

namespace leveldb {

class InternalKeyComparator;
class MemTableIterator;

class MemTable {
 public:
  // MemTables are reference counted.  The initial reference count
  // is zero and the caller must call Ref() at least once.
  explicit MemTable(const InternalKeyComparator& comparator);

  MemTable(const MemTable&) = delete;
  MemTable& operator=(const MemTable&) = delete;

  // Increase reference count.
  void Ref() { ++refs_; }

  // Drop reference count.  Delete if no more references exist.
  void Unref() {
    --refs_;
    assert(refs_ >= 0);
    if (refs_ <= 0) {
      delete this;
    }
  }

  // 返回该 Memtable 非精确的内存使用量
  size_t ApproximateMemoryUsage();

  // AppendInternalKey db/format.{h,cc} module.
  // 返回 Memtable 的迭代器，用来进行顺序访问以及 seek 操作
  // 因为底层数据结构使用的是skiplist 所以和skiplist的迭代器功能一样
  Iterator* NewIterator();

  // 向memtable中添加一条entry，将键映射到值指定的序列号和指定的类型。
  // 如果 type==kTypeDeletion，通常 value 将为空。
  void Add(SequenceNumber seq, ValueType type, const Slice& key,
           const Slice& value);

  // 如果 memtable 包含 key 的 value，则将其存储在 *value 中并返回 true。
  // 如果 memtable 包含 key 的 detetion，则存储 NotFound() 错误在 *status 中并返回 true。
  // 否则，返回 false。
  bool Get(const LookupKey& key, std::string* value, Status* s);

 private:
  friend class MemTableIterator;
  friend class MemTableBackwardIterator;

  struct KeyComparator {
    const InternalKeyComparator comparator;
    explicit KeyComparator(const InternalKeyComparator& c) : comparator(c) {}
    int operator()(const char* a, const char* b) const;
  };

  typedef SkipList<const char*, KeyComparator> Table;

  ~MemTable();  // Private since only Unref() should be used to delete it
  
  KeyComparator comparator_;    // key值比较模块，提供给skiplist
  int refs_;
  Arena arena_; // 内存分配模块，提供给skiplist
  Table table_;
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_MEMTABLE_H_
