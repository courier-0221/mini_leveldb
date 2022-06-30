TARGET := bloom_test

LEVELDB_UTIL_SRC := ../../util/
LEVELDB_UTIL_INC := ../../util/
LEVELDBINC := ../../include/

GTESTINC := ../../third_party/googletest/googletest/include/
GTESTINC += ../../third_party/googletest/googlemock/include/

CXX = g++
CPPFLAGS = -g -O2 -Wall -std=c++11
CPPFLAGS += $(addprefix -I,$(LEVELDBINC))
CPPFLAGS += $(addprefix -I,$(LEVELDB_UTIL_INC))
CPPFLAGS += $(addprefix -I,$(GTESTINC))
CPPFLAGS += -L../../third_party/lib/

LIB = -lgtest -lgtest_main -lpthread

FILETYPE = cc
OBJS := $(LEVELDB_UTIL_SRC)*.$(FILETYPE)

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CXX) $(CPPFLAGS) -o $(TARGET) bloom_test.cc $(OBJS) $(LIB)

clean:
	-rm -f $(SRC)*.o $(TARGET)
