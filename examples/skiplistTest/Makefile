TARGET := skiplist_test

LEVELDB_UTIL_SRC := ../../util/
LEVELDB_UTIL_INC := ../../util/
LEVELDB_DB_INC := ../../db/
LEVELDBINC := ../../include/

CXX = g++
CPPFLAGS = -g -O2 -Wall -std=c++11
CPPFLAGS += $(addprefix -I,$(LEVELDBINC))
CPPFLAGS += $(addprefix -I,$(LEVELDB_UTIL_INC))
CPPFLAGS += $(addprefix -I,$(LEVELDB_DB_INC))

FILETYPE = cc
OBJS := $(LEVELDB_UTIL_SRC)*.$(FILETYPE)

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CXX) $(CPPFLAGS) -o $(TARGET) skiplist_test.cc $(OBJS)

clean:
	-rm -f $(SRC)*.o $(TARGET)
