TARGET := statusTest

LEVELDBSRC := ../../util/
LEVELDBINC := ../../include/

CXX = g++
CPPFLAGS = -g -O2 -Wall -std=c++11
CPPFLAGS += $(addprefix -I,$(LEVELDBINC))

FILETYPE = cc
OBJS := $(LEVELDBSRC)*.$(FILETYPE)

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CXX) $(CPPFLAGS) -o $(TARGET) statusTest.cc $(OBJS)

clean:
	-rm -f $(SRC)*.o $(TARGET)
