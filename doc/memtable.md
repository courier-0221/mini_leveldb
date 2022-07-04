# key的不同类型

leveldb有5个key

![memtable-key](/Users/kuilong/Desktop/memtable-key.png)

## 1.user_kry

用户层面传入的 key，使用 Slice 格式。

## 2.ParsedInternalKey (db/dbformat.h)

db 内部操作的 key。

未编码前，或者说是一个解码后的Internal Key结构，它由user_key、sequence和type三个字段组成。

```cpp
struct ParsedInternalKey {
	Slice user_key; 
  SequenceNumber sequence; 
  ValueType type;
};
```

## 3.InternalKey (db/dbformat.h)

Db 内部，包装易用的结构

InternalKey是由User key + SequenceNumber + ValueType组合而成的

InternalKey的格式为：
User key (string) | sequence number (7 bytes) | value type (1 byte) 



### 3.1.ValueType  (db/dbformat.h)

leveldb 更新(put/delete)某个 key 时不会操控到 db 中的数据，每次操作都是直接新插入一份 kv 数据，具体的数据合并和清除由后台的 compact 完成。所以，每次 put，db 中就会新加入一份 KV 数据， 即使该 key 已经存在;而 delete 等同于 put 空的 value。为了区分真实 kv 数据和删除操作的 mock 数据，使用 ValueType 来标识:

```cpp
enum ValueType { 
  kTypeDeletion = 0x0, 
  kTypeValue = 0x1
};
```

### 3.2.SequnceNnumber  (db/dbformat.h)

leveldb 中的每次更新(put/delete)操作都拥有一个版本，由 SequnceNumber 来标识，整个 db 有一个 全局值保存着当前使用到的 SequnceNumber。SequnceNumber 在 leveldb 有重要的地位，key 的排序， compact 以及 snapshot 都依赖于它。
typedef uint64_t SequenceNumber;

存储时，SequnceNumber 只占用 56 bits, ValueType 占用 8 bits，二者共同占用 64bits(uint64_t).

## 4.LookupKey & Memtable Key (db/dbformat.h)

Memtable_key是LookupKey的字符串版本，类型为slice

Memtable的查询接口传入的是LookupKey，它也是由User Key和Sequence Number组合而成的.

LookupKey的格式为：

Size (int32变长)| User key (string) | sequence number (7 bytes) | value type (1 byte)

![memtable-lookupformat](/Users/kuilong/Desktop/memtable-lookupformat.png)

```cpp
class LookupKey {
...
public:
  Slice memtable_key() const { return Slice(start_, end_ - start_); }
  Slice internal_key() const { return Slice(kstart_, end_ - kstart_); }
  Slice user_key() const { return Slice(kstart_, end_ - kstart_ - 8); }

private:
	const char* start_; 
  const char* kstart_; // 指向user_key开始的位置
  const char* end_;
};
```

对 memtable 进行 lookup 时使用 [start,end], 对 sstable lookup 时使用[kstart, end]。
