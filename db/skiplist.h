#ifndef SKIPLIST_H_
#define SKIPLIST_H_

#include <atomic>
#include <cassert>
#include <cstdlib>

#include "arena.h"
#include "random.h"

namespace leveldb
{

/*
* LevelDB 的 Skip List 对外提供的接口主要有三个，分别是插入、查询和遍历。
* Insert、Contains、Iterator
* 使用时传入跳表的类型和key比较的类，需要重载"()"实现仿函数
*/

template <typename Key, class Comparator>
class SkipList
{
private:
    // 跳表中的节点
    struct Node;

public:
    explicit SkipList(Comparator cmp, Arena* arena);

    SkipList(const SkipList&) = delete;
    SkipList& operator=(const SkipList&) = delete;

    // Insert key into the list. 插入前确保list中无要插入的key
    void Insert(const Key& key);
    // Returns true iff an entry that compares equal to key is in the list.
    bool Contains(const Key& key) const;

    // 跳表迭代器，用于遍历
    class Iterator
    {
    public:
        explicit Iterator(const SkipList* list);
        bool Valid() const;     // 是否指向了一个有效的节点
        const Key& key() const; // 返回指向当前节点的key
        void Next();            // 下一个节点
        void Prev();            // 前一个节点
        void Seek(const Key& target);   // 将node_调整到跳表第0层大于等于key的节点
        void SeekToFirst();             // 将node_调整到跳表第0层的头部节点
        void SeekToLast();              // 将node_调整到跳表第0层的尾部节点
    private:
        const SkipList* list_;
        Node* node_;    //当前指向的节点
    };

private:
    enum { kMaxHeight = 12 };   // 可以增长到的最大高度

    inline int GetMaxHeight() const
    {
        return max_height_.load(std::memory_order_relaxed);
    }

    Node* NewNode(const Key& key, int height);
    // 高度随机增加，增加多少不确定
    int RandomHeight();
    // key值相等返回true
    bool Equal(const Key& a, const Key& b) const { return (compare_(a, b) == 0); }
    // Return true if key is greater than the data stored in "n"，不包括等于
    bool KeyIsAfterNode(const Key& key, Node* n) const;
    // prev参数说明：如果查找操作，则指定 prev = nullptr 即可；若要插入数据，则需传入一个合适尺寸的 prev 参数
    // 找到第0层大于等于key的节点，没有返回nullptr
    Node* FindGreaterOrEqual(const Key& key, Node** prev) const;
    // 返回跳表第0层最后一个小于key的节点
    Node* FindLessThan(const Key& key) const;
    // 返回跳表第0层的尾部节点
    Node* FindLast() const;

private:
    Comparator const compare_;      // 比较
    Arena* const arena_;            // Arena used for allocations of nodes
    Node* const head_;              // 头节点
    std::atomic<int> max_height_;   // 当前跳表最大高度
    Random rnd_;
};

// 表示一个key -> value
// Next & SetNext 为带有内存屏障，NoBarrier_Next & NoBarrier_SetNext 为不带内存屏障 
template <typename Key, class Comparator>
struct SkipList<Key, Comparator>::Node
{
    explicit Node(const Key& k) : key(k) {}

    // 当前节点的Key值
    Key const key;

	// 参数n代表level，可以横向获取下一个节点也可以纵向获取下一个节点，纵向获取时传不同的n来获取
    Node* Next(int n)
    {
        assert(n >= 0);
        // 保证同线程中该 load 之后的对相关内存读写语句不会被重排到 load 之前
        // 并且其他线程中对同样内存用了 store release 都对其可见。
        return next_[n].load(std::memory_order_acquire);
    }
	// 横向设置每一层节点指向的下一个节点，通过n来切换不同层，n代表level
    void SetNext(int n, Node* x)
    {
        assert(n >= 0);
        // 保证同线程中该 store 之后的对相关内存的读写语句不会被重排到 store 之前
        // 并且该线程的所有修改对用了 load acquire 的其他线程都可见。
        next_[n].store(x, std::memory_order_release);
    }

    // 使用上稍有限制
    Node* NoBarrier_Next(int n)
    {
        assert(n >= 0);
        return next_[n].load(std::memory_order_relaxed);
    }
    void NoBarrier_SetNext(int n, Node* x)
    {
        assert(n >= 0);
        next_[n].store(x, std::memory_order_relaxed);
    }

private:
	// 横向：存储下一个节点位置
	// 纵向：NewNode 分配内存时会申请从 kMaxHeight 到当前 height 的差值个 Node* 内存，用于存储纵向内容
    std::atomic<Node*> next_[1];    
};

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node* SkipList<Key, Comparator>::NewNode(
    const Key& key, int height)
{
    // node数据结构的大小+（height-1）个所需存储的指向下一个Node的空间（每一层都需要指向下一个结点）
    char* const node_memory = arena_->AllocateAligned(
        sizeof(Node) + sizeof(std::atomic<Node*>) * (height - 1));
    return new (node_memory) Node(key); //使用new 的步骤2，placement new，来调用node的构造函数。
}

template <typename Key, class Comparator>
inline SkipList<Key, Comparator>::Iterator::Iterator(const SkipList* list)
{
    list_ = list;
    node_ = nullptr;
}

template <typename Key, class Comparator>
inline bool SkipList<Key, Comparator>::Iterator::Valid() const
{
    return node_ != nullptr;
}

template <typename Key, class Comparator>
inline const Key& SkipList<Key, Comparator>::Iterator::key() const
{
    assert(Valid());
    return node_->key;
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::Next()
{
    assert(Valid());
    node_ = node_->Next(0);
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::Prev()
{
    assert(Valid());
    node_ = list_->FindLessThan(node_->key);    //找到node的前一个节点
    if (node_ == list_->head_)
    {
        node_ = nullptr;
    }
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::Seek(const Key& target)
{
    node_ = list_->FindGreaterOrEqual(target, nullptr);
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::SeekToFirst()
{
    node_ = list_->head_->Next(0);
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::SeekToLast()
{
    node_ = list_->FindLast();
    if (node_ == list_->head_)
    {
        node_ = nullptr;
    }
}

template <typename Key, class Comparator>
int SkipList<Key, Comparator>::RandomHeight()
{
    static const unsigned int kBranching = 4;
    int height = 1;
    // 每次以 1/4 的概率增加层数
    // 得到一个随机值，如果随机值是 4 的倍数就返回 height，否则 keight 就加 1，为什么要这么做？
    // 如果我们得到一个随机值，直接对 kMaxHeight 取模加 1，然后赋值给 height，那么 height 在 [1~12] 之前出现的概率一样的
    // 如果节点个数为 n，那么有 12 层的节点有 n/12 个，11 层的有 n/12+n/12(需要把12层的也加上)，
    // 节点太多，最上层平均前进一次才右移 12 个节点，下面层就更不用说了，效率低；
    // 作者的方法是每一层会按照4的倍数减少，出现4层的概率只有出现3层概率的1/4，这样查询起来效率是不是大大提升了呢
    while (height < kMaxHeight && rnd_.OneIn(kBranching))
    {
        height++;
    }
    assert(height > 0);
    assert(height <= kMaxHeight);
    return height;
}

template <typename Key, class Comparator>
bool SkipList<Key, Comparator>::KeyIsAfterNode(const Key& key, Node* n) const
{
    return (n != nullptr) && (compare_(n->key, key) < 0);
}

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node*
    SkipList<Key, Comparator>::FindGreaterOrEqual(const Key& key, Node** prev) const
{
    Node* x = head_;
    int level = GetMaxHeight() - 1;
    while (true)
    {
        Node* next = x->Next(level);    // 这里next表示两种情况，(水平方向)该层中下一个节点 或者 (垂直方向)该节点的下一层中的位置
        if (KeyIsAfterNode(key, next))  // 待查找 key 比 next 大(不包括等于)，则在该层继续查找
        {
            x = next;
        }
        else
        {
			//插入时使用prev 当跨层时记录所在层的前一个节点，查找时传入nullptr无需关心
            if (prev != nullptr) prev[level] = x;
            if (level == 0)             // 待查找 key 不大于 next，则到底返回
            {
                return next;
            }
            else                        // 待查找 key 不大于 next，且没到底，则往下查找
            {
                level--;
            }
        }
    }
}

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node*
    SkipList<Key, Comparator>::FindLessThan(const Key& key) const
{
    Node* x = head_;
    int level = GetMaxHeight() - 1;
    while (true)
    {
        assert(x == head_ || compare_(x->key, key) < 0);
        Node* next = x->Next(level);
        if (next == nullptr || compare_(next->key, key) >= 0)
        {
            if (level == 0)
            {
                return x;
            }
            else
            {
                level--;
            }
        }
        else
        {
            x = next;
        }
    }
}

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node* SkipList<Key, Comparator>::FindLast() const
{
    Node* x = head_;
    int level = GetMaxHeight() - 1;
    while (true)
    {
        Node* next = x->Next(level);
        if (next == nullptr)
        {
            if (level == 0)
            {
                return x;
            }
            else
            {
                // Switch to next list
                level--;
            }
        }
        else
        {
            x = next;
        }
    }
}

template <typename Key, class Comparator>
SkipList<Key, Comparator>::SkipList(Comparator cmp, Arena* arena)
    : compare_(cmp),
    arena_(arena),
    head_(NewNode(0, kMaxHeight)),
    max_height_(1),
    rnd_(0xdeadbeef)
{
    for (int i = 0; i < kMaxHeight; i++)
    {
        head_->SetNext(i, nullptr);
    }
}

template <typename Key, class Comparator>
void SkipList<Key, Comparator>::Insert(const Key& key)
{
    // 待做(opt): 由于插入要求外部加锁，因此可以使用 NoBarrier_Next 的 FindGreaterOrEqual 以提高性能
    Node* prev[kMaxHeight]; // 长度设定简单粗暴，直接取最大值
    Node* x = FindGreaterOrEqual(key, prev);

    // 不允许插入重复key
    assert(x == nullptr || !Equal(key, x->key));

    int height = RandomHeight();    // 随机取一个level值
    if (height > GetMaxHeight())
    {
        // [maxheight, height]
        for (int i = GetMaxHeight(); i < height; i++)
        {
            prev[i] = head_;
        }
        // 此处不用为并发读加锁。因为并发读在（在另外线程中通过 FindGreaterOrEqual 中的 GetMaxHeight）
        // 读取到更新后跳表层数，但该节点尚未插入时也无妨。因为这意味着它会读到 nullptr，而在 LevelDB
        // 的 Comparator 设定中，nullptr 比所有 key 都大。因此，FindGreaterOrEqual 会继续往下找，符合预期。
        max_height_.store(height, std::memory_order_relaxed);
    }

    x = NewNode(key, height);
    for (int i = 0; i < height; i++)
    {
        // 此句 NoBarrier_SetNext() 版本就够用了，因为后续 prev[i]->SetNext(i, x) 语句会进行强制同步。
        // 并且为了保证并发读的正确性，一定要先设置本节点指针，再设置原条表中节点（prev）指针
        x->NoBarrier_SetNext(i, prev[i]->NoBarrier_Next(i));
        prev[i]->SetNext(i, x);
    }
}

template <typename Key, class Comparator>
bool SkipList<Key, Comparator>::Contains(const Key& key) const
{
    Node* x = FindGreaterOrEqual(key, nullptr);
    if (x != nullptr && Equal(key, x->key))
    {
        return true;
    }
    else
    {
        return false;
    }
}

}  // namespace leveldb

#endif  // SKIPLIST_H_
