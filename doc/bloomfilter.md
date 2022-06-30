# 简介

Bloom filter 是一个数据结构，它可以用来判断某个元素是否在集合内，具有运行快速，内存占用小的特点。
而高效插入和查询的代价就是，Bloom Filter 是一个**基于概率的数据结构**：它只能告诉我们一个元素**绝对不**在集合内或**可能**在集合内

# 理论知识

先介绍下 bloom filter 的几个组成：
1.n 个 key
2.m bits 的空间 v，全部初始化为0
3.k 个无关的 hash 函数：h1, h2, ..., hk，hash 结果为{1, 2, ..., m} or {0, 1, ..., m-1})

当 key 越来越多，v 里置为 1 的 bits 越来越多。对于某个不存在的 key，k 个 hash 函数对应的 bit 可能正好为1，此时就概率发生误判，更专业的术语称为 **false positive**，或者 **false drop**.
因此，我们称 bloom filter 是一种概率型的数据结构，当返回某个 key 存在时，只是说明可能存在。
m 越大，k 越大， n 越小，那么 **false positive**越小。
结论：hash 函数 k 的最优个数为 ln2 * (m/n).

# 代码实现

对应类BloomFilterPolicy，主要有三个接口

1.构造函数

2.CreateFilter：根据传入的 keys，计算对应的 hash area.

3.KeyMayMatch：查找传入的 key 是否存在

可以看到，bloom filter 仅支持插入和查找，不支持删除操作(导致误删除)。

## 1.构造函数

构造函数主要是根据 m/n 计算 k 的个数，不过最大不超过 30 个:

```cpp
 explicit BloomFilterPolicy(int bits_per_key) : bits_per_key_(bits_per_key)
 {
     // k = ln2 * (m/n)，获取哈希函数的个数 k。
     // bits_per_key = m / n
     // [1, 30]个hash函数
     k_ = static_cast<size_t>(bits_per_key * 0.69);  // 0.69 =~ ln(2)
     if (k_ < 1) k_ = 1;
     if (k_ > 30) k_ = 30;
 }
```

## 2.CreateFilter

CreateFilter计算传入的 n 个 key，最终结果存储到dst(对应到理论介绍里的 m)

首先计算需要的总字节数，然后依次调用 k 次 hash 函数，存储到dst，并在最后追加 k 本身。

virtual void CreateFilter(const Slice* keys, int n, std::string* dst) const

```cpp
void CreateFilter(const Slice* keys, int n, std::string* dst) const override
{
    // 计算 bloom filter 的 bit 数组长度 n，会除以 8 向上取整，因为 bit 数组最后会用 char 数组表示
    size_t bits = n * bits_per_key_;

    // 如果数组太短，会有很高的误判率，因此这里增加了一个最小长度限定。
    if (bits < 64) bits = 64;

    //计算需要的bytes数，最少8bytes
    size_t bytes = (bits + 7) / 8;
    bits = bytes * 8;

    // dst[init_size : init_size + bytes]写入过滤器内容，默认全为0
    // dst[init_size + bytes]写入hash函数个数
    const size_t init_size = dst->size();

    dst->resize(init_size + bytes, 0);
    dst->push_back(static_cast<char>(k_));  // 记下哈希函数的个数
    char* array = &(*dst)[init_size];       //更新array <-> dst[init_size : init_size + bytes]
    for (int i = 0; i < n; i++) {
        // 使用 double-hashing 方法，仅使用一个 hash 函数来生成 k 个 hash 值，近似等价于使用 k 个哈希函数的效果
        uint32_t h = BloomHash(keys[i]);
        const uint32_t delta = (h >> 17) | (h << 15);  // Rotate right 17 bits
        for (size_t j = 0; j < k_; j++) {
            const uint32_t bitpos = h % bits;
            array[bitpos / 8] |= (1 << (bitpos % 8));
            h += delta;
        }
    }
}
```

## 3.KeyMayMatch

是逆过程

## 4.hash函数

```cpp
leveldb只定义了一个hash函数
虽然多次调用，函数间仍然保持independent，因此仍然满足前面的公式
static uint32_t BloomHash(const Slice& key) {
	return Hash(key.data(), key.size(), 0xbc9f1d34);
}
```





