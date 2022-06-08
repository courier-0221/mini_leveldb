#ifndef RANDOM_H_
#define RANDOM_H_

#include <cstdint>

namespace leveldb
{
/*
* 随机数生成算法： seed=(seed∗A+C)%M
* 其中 A,C,M 都是常数（一般会取质数）。当 C=0 时，叫做乘同余法。
* Random 类中，A=16807, M=2147483647, C=0
*/
class Random
{
private:
    uint32_t seed_;

public:
    //初始化时给个种子
    explicit Random(uint32_t s) : seed_(s & 0x7fffffffu)
    {
        if (seed_ == 0 || seed_ == 2147483647L) {
            seed_ = 1;
        }
    }

    //种子滚动，下一次生成随机值
    uint32_t Next()
    {
        static const uint32_t M = 2147483647L;  // 2^31-1
        static const uint64_t A = 16807;        // bits 14, 8, 7, 5, 2, 1, 0
        // We are computing
        //       seed_ = (seed_ * A) % M,    where M = 2^31-1
        //
        // seed_ must not be zero or M, or else all subsequent computed values
        // will be zero or M respectively.  For all other values, seed_ will end
        // up cycling through every number in [1,M-1]
        uint64_t product = seed_ * A;

        // Compute (product % M) using the fact that ((x << 31) % M) == x.
        seed_ = static_cast<uint32_t>((product >> 31) + (product & M));
        // The first reduction may overflow by 1 bit, so we may need to
        // repeat.  mod == M is not possible; using > allows the faster
        // sign-bit-based test.
        if (seed_ > M) {
            seed_ -= M;
        }
        return seed_;
    }
    // Returns a uniformly distributed value in the range [0..n-1]
    // REQUIRES: n > 0
    uint32_t Uniform(int n) { return Next() % n; }

    // Randomly returns true ~"1/n" of the time, and false otherwise.
    // REQUIRES: n > 0
    bool OneIn(int n) { return (Next() % n) == 0; }

    // Skewed: pick "base" uniformly from range [0,max_log] and then
    // return "base" random bits.  The effect is to pick a number in the
    // range [0,2^max_log-1] with exponential bias towards smaller numbers.
    uint32_t Skewed(int max_log) { return Uniform(1 << Uniform(max_log + 1)); }
};

}  // namespace leveldb

#endif  // RANDOM_H_
