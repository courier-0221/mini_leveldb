#include <iostream>
#include <set>
#include "skiplist.h"

using namespace leveldb;
using namespace std;

typedef uint64_t Key;

struct Comparator {
    int operator()(const Key& a, const Key& b) const {
        if (a < b) {
            return -1;
        }
        else if (a > b) {
            return +1;
        }
        else {
            return 0;
        }
    }
};

void SkipTestEmpty()
{
    Arena arena;
    Comparator cmp;
    SkipList<Key, Comparator> list(cmp, &arena);
    cout << std::boolalpha << list.Contains(10) << endl;

    SkipList<Key, Comparator>::Iterator iter(&list);
    cout << std::boolalpha << iter.Valid() << endl;
    iter.SeekToFirst();
	cout << std::boolalpha << iter.Valid() << endl;
    iter.Seek(100);
	cout << std::boolalpha << iter.Valid() << endl;
    iter.SeekToLast();
	cout << std::boolalpha << iter.Valid() << endl;
}

void SkipTest_InsertAndLookup()
{
	const int N = 20;
	const int R = 50;
	Random rnd(10);
	std::set<Key> keys;
	Arena arena;
	Comparator cmp;
	SkipList<Key, Comparator> list(cmp, &arena);
	
	// 插入
	for (int i = 0; i < N; i++)
	{
		Key key = rnd.Next() % R;
		printf("key.%llu\t", key);
		//set插入成功在考虑插入list && 同时判定list中是否存在该key，不允许有重复节点出现
		if (keys.insert(key).second && !list.Contains(key))
		{
			list.Insert(key);
		}
	}

	// 查找
	for (int i = 0; i < R; i++)
	{
		if (list.Contains(i))
		{
			printf("find key.%d ==> size.%d\n", i, keys.count(i));
		}
	}

	// 遍历
	{
		SkipList<Key, Comparator>::Iterator iter(&list);
		iter.SeekToFirst();
		while (iter.Valid())
		{
			printf("iterator key:%llu\n", iter.key());
			iter.Next();
		}
	}

	 // Simple iterator tests
	{
		SkipList<Key, Comparator>::Iterator iter(&list);
		cout << std::boolalpha << iter.Valid() << endl;

		// 找到比0大的key的位置
		iter.Seek(0);
		cout << std::boolalpha << iter.Valid() << endl;
		printf("keys.begin=%llu iter.key=%llu\n", *(keys.begin()), iter.key());

		// 调整到第一个元素位置
		iter.SeekToFirst();
		cout << std::boolalpha << iter.Valid() << endl;
		printf("keys.begin=%llu iter.key=%llu\n", *(keys.begin()), iter.key());

		// 调整到最后一个元素位置
		iter.SeekToLast();
		cout << std::boolalpha << iter.Valid() << endl;
		printf("keys.begin=%llu iter.key=%llu\n", *(keys.rbegin()), iter.key());
	}


}

int main()
{
    //SkipTestEmpty();
	SkipTest_InsertAndLookup();


	system("pause");
    return 0;
}