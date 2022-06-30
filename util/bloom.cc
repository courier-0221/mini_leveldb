#include "filter_policy.h"
#include "slice.h"
#include "hash.h"

/*
其误判率应该和以下参数有关：
1.哈希函数的个数 k
2.底层位数组的长度 m
3.数据集大小 n
当 k = ln2 * (m/n) 时，Bloom Filter 获取最优的准确率。m/n 即 bits per key（集合中每个 key 平均分到的 bit 数）。
bits_per_key = m / n
*/

namespace leveldb
{

static uint32_t BloomHash(const Slice& key)
{
    return Hash(key.data(), key.size(), 0xbc9f1d34);
}

class BloomFilterPolicy : public FilterPolicy
{
public:
    explicit BloomFilterPolicy(int bits_per_key) : bits_per_key_(bits_per_key)
    {
        // k = ln2 * (m/n)，获取哈希函数的个数 k。
        // bits_per_key = m / n
        // [1, 30]个hash函数
        k_ = static_cast<size_t>(bits_per_key * 0.69);  // 0.69 =~ ln(2)
        if (k_ < 1) k_ = 1;
        if (k_ > 30) k_ = 30;
    }

    const char* Name() const override { return "leveldb.BuiltinBloomFilter2"; }

    void CreateFilter(const Slice* keys, int n, std::string* dst) const override
    {
        // 计算 bloom filter 的 bit 数组长度 n，会除以 8 向上取整，因为 bit 数组最后会用 char 数组表示
        size_t bits = n * bits_per_key_;

        // 如果数组太短，会有很高的误判率，因此这里增加了一个最小长度限定。
        if (bits < 64) bits = 64;

        //计算需要的bytes数，最少8bytes
        size_t bytes = (bits + 7) / 8;
        bits = bytes * 8;

        //printf("bytes.%d bits.%d\n", bytes, bits);

        // dst[init_size : init_size + bytes]写入过滤器内容，默认全为0
        // dst[init_size + bytes]写入hash函数个数
        const size_t init_size = dst->size();
        //printf("init_size.%d\n", init_size);

        dst->resize(init_size + bytes, 0);
        dst->push_back(static_cast<char>(k_));  // 记下哈希函数的个数
        char* array = &(*dst)[init_size];       //更新array <-> dst[init_size : init_size + bytes]
        for (int i = 0; i < n; i++) {
            // 使用 double-hashing 方法，仅使用一个 hash 函数来生成 k 个 hash 值，近似等价于使用 k 个哈希函数的效果
            uint32_t h = BloomHash(keys[i]);
            //printf("BloomHash.%u keys.%s\n", h, keys[i].data());
            const uint32_t delta = (h >> 17) | (h << 15);  // Rotate right 17 bits
            //printf("delta.%u\n", delta);
            for (size_t j = 0; j < k_; j++) {
                const uint32_t bitpos = h % bits;
                //printf("bitpos.%u\n", bitpos);
                array[bitpos / 8] |= (1 << (bitpos % 8));
                h += delta;
            }
        }
    }

    bool KeyMayMatch(const Slice& key, const Slice& bloom_filter) const override
    {
        const size_t len = bloom_filter.size();
        if (len < 2) return false;

        const char* array = bloom_filter.data();
        const size_t bits = (len - 1) * 8;

        // Use the encoded k so that we can read filters generated by
        // bloom filters created using different parameters.
        const size_t k = array[len - 1];
        if (k > 30) {
            // Reserved for potentially new encodings for short bloom filters.
            // Consider it a match.
            return true;
        }

        uint32_t h = BloomHash(key);
        const uint32_t delta = (h >> 17) | (h << 15);  // Rotate right 17 bits
        for (size_t j = 0; j < k; j++) {
            const uint32_t bitpos = h % bits;
            if ((array[bitpos / 8] & (1 << (bitpos % 8))) == 0) return false;
            h += delta;
        }
        return true;
    }

private:
    size_t bits_per_key_;
    size_t k_;
};

const FilterPolicy* NewBloomFilterPolicy(int bits_per_key)
{
    return new BloomFilterPolicy(bits_per_key);
}

}  // namespace leveldb
