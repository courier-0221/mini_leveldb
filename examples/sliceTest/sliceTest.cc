#include <iostream>
#include "slice.h"

int main()
{
    //初始化操作
    leveldb::Slice s1 = "s1-123456789";
    printf("s1.size=%ld s1.data=%s\n", s1.size(), s1.data());

    leveldb::Slice s2("s2-123456789");
    printf("s2.size=%ld s2.data=%s\n", s2.size(), s2.data());

    std::string str("s3-123456789");
    leveldb::Slice s3(str);
    printf("s3.size=%ld s3.data=%s\n", s3.size(), s3.data());

    //starts_with
    std::cout << "starts_with: " << std::boolalpha << s1.starts_with("s1") << std::endl;

    //remove_prefix
    s2.remove_prefix(3);
    printf("remove_prefix s2.size=%ld s2.data=%s\n", s2.size(), s2.data());

    //compare
    if (0 == s3.compare(s2))
    {
        printf("compare s2 = s3\n");
    }
    else
    {
        printf("compare s2 != s3\n");
    }

    //operator==
    s3.remove_prefix(3);
    if (s2 == s3)
    {
        printf("operator== s2 = s3\n");
    }
    else
    {
        printf("operator== s2 != s3\n");
    }

    //clear && empty
    s3.clear();
    if (s3.empty())
    {
        printf("clear && empty s3 size is 0\n");
    }
    else
    {
        printf("clear && empty s3 size is not 0\n");
    }

    return 0;
}
