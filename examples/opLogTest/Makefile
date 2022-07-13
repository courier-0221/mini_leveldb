TARGET := oplog_test

LEVELDB_UTIL_SRC := ../../util/
LEVELDB_UTIL_INC := ../../util/
LEVELDB_DB_SRC := ../../db/
LEVELDB_DB_INC := ../../db/
LEVELDB_TBALE_SRC := ../../table/
LEVELDB_TBALE_INC := ../../table/
LEVELDBINC := ../../include/

GTESTINC := ../../third_party/googletest/googletest/include/
GTESTINC += ../../third_party/googletest/googlemock/include/

CXX = g++
CPPFLAGS = -g -O2 -Wall -std=c++11
CPPFLAGS += $(addprefix -I,$(LEVELDBINC))
CPPFLAGS += $(addprefix -I,$(LEVELDB_UTIL_INC))
CPPFLAGS += $(addprefix -I,$(LEVELDB_DB_INC))
CPPFLAGS += $(addprefix -I,$(LEVELDB_TBALE_INC))
CPPFLAGS += $(addprefix -I,$(GTESTINC))
CPPFLAGS += -L../../third_party/lib/

LIB = -lgtest -lgtest_main -lpthread

FILETYPE = cc
OBJS := $(LEVELDB_UTIL_SRC)*.$(FILETYPE)
OBJS += $(LEVELDB_DB_SRC)*.$(FILETYPE)
OBJS += $(LEVELDB_TBALE_SRC)*.$(FILETYPE)

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CXX) $(CPPFLAGS) -o $(TARGET) oplog_test.cc $(OBJS) $(LIB)

clean:
	-rm -f $(SRC)*.o $(TARGET)
