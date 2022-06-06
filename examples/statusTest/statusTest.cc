#include <iostream>
#include "status.h"

int main()
{
    // MoveConstructor
    {
        // OK() 是static方法， 这里没有调用copy constructor
        leveldb::Status ok = leveldb::Status::OK();
        leveldb::Status ok2 = std::move(ok);

        std::cout << "Status(Status&& rhs) ok2: " << std::boolalpha << ok2.IsOk() << std::endl;
        leveldb::Status s2;
        leveldb::Status s3 = s2;
    }

    {
        leveldb::Status status = leveldb::Status::NotFound("custom NotFound status message");
        leveldb::Status status2 = std::move(status);

        std::cout << "Status(Status&& rhs) status2: " << std::boolalpha << status2.IsNotFound() << std::endl;
        printf("Status(Status&& rhs) status2.str %s\n", status2.ToString().c_str());
    }

    {
        leveldb::Status self_moved = leveldb::Status::IOError("custom IOError status message");
        leveldb::Status& self_moved_reference = self_moved;
        self_moved_reference = std::move(self_moved);
        printf("operator=(Status&& rhs) self_moved_reference.str %s\n", self_moved_reference.ToString().c_str());
    }

    return 0;
}