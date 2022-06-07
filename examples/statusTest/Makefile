TARGET := statusTest

LEVELDBSRC := ../../util/
LEVELDBINC := ../../include/

GTESTINC := ../../third_party/googletest/googletest/include/
GTESTINC += ../../third_party/googletest/googlemock/include/

CXX = g++
CPPFLAGS = -g -O2 -Wall -std=c++11
CPPFLAGS += $(addprefix -I,$(LEVELDBINC))
CPPFLAGS += $(addprefix -I,$(GTESTINC))
CPPFLAGS += -L../../third_party/lib/

LIB = -lgtest -lgtest_main -lpthread

FILETYPE = cc
OBJS := $(LEVELDBSRC)*.$(FILETYPE)

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CXX) $(CPPFLAGS) -o $(TARGET) status_test.cc $(OBJS) $(LIB)

clean:
	-rm -f $(SRC)*.o $(TARGET)
