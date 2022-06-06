TARGET := sliceTest

LEVELDBSRC := ../../
LEVELDBINC := ../../include/

CXX = g++
CPPFLAGS = -g -O2 -Wall -std=c++11
CPPFLAGS += $(addprefix -I,$(LEVELDBINC))

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CXX) $(CPPFLAGS) -o $(TARGET) sliceTest.cc

clean:
	-rm -f $(SRC)*.o $(TARGET)
