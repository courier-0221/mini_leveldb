#include <iostream>
#include "status.h"
#include "slice.h"
#include "gtest/gtest.h"

namespace leveldb
{

TEST(Status, MoveConstructor)
{
    {
        Status ok = Status::OK();
        Status ok2 = std::move(ok);
        //std::cout << "Status(Status&& rhs) ok2: " << std::boolalpha << ok2.IsOk() << std::endl;

        ASSERT_TRUE(ok2.IsOk());
    }

    {
        Status status = Status::NotFound("custom NotFound status message");
        Status status2 = std::move(status);

        //std::cout << "Status(Status&& rhs) status2: " << std::boolalpha << status2.IsNotFound() << std::endl;
        //printf("Status(Status&& rhs) status2.str %s\n", status2.ToString().c_str());

        ASSERT_TRUE(status2.IsNotFound());
        ASSERT_EQ("NotFound: custom NotFound status message", status2.ToString());
    }

    {
        Status self_moved = Status::IOError("custom IOError status message");

        Status& self_moved_reference = self_moved;
        self_moved_reference = std::move(self_moved);
        //printf("operator=(Status&& rhs) self_moved_reference.str %s\n", self_moved_reference.ToString().c_str());
    }
}

}  // namespace leveldb
