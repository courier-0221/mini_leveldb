TARGET := logger_test

LEVELDB_UTIL_SRC := ../../util/
LEVELDB_UTIL_INC := ../../util/
LEVELDB_DB_SRC := ../../db/
LEVELDB_DB_INC := ../../db/
LEVELDBINC := ../../include/

CXX = g++
CPPFLAGS = -g -O2 -Wall -std=c++11
CPPFLAGS += $(addprefix -I,$(LEVELDBINC))
CPPFLAGS += $(addprefix -I,$(LEVELDB_UTIL_INC))
CPPFLAGS += $(addprefix -I,$(LEVELDB_DB_INC))
CPPFLAGS += $(addprefix -I,$(LEVELDB_DB_SRC))
LIB = -lpthread

FILETYPE = cc
OBJS := $(LEVELDB_UTIL_SRC)*.$(FILETYPE)

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CXX) $(CPPFLAGS) -o $(TARGET) logger_test.cc $(OBJS) $(LIB)

clean:
	-rm -f $(SRC)*.o $(TARGET)
	-rm -f LoggerTest.txt
