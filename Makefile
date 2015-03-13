CXX=g++
CXXFLAGS=-g -std=c++11 -Wall -pedantic -O0 -fPIC
LINKFLAGS=-lm -lpthread
BIN=smu
LIB=smu.a

SYS := $(shell gcc -dumpmachine)
ifneq (, $(findstring linux, $(SYS)))
    LINKFLAGS+=$(shell pkg-config --libs libusb-1.0)
    CXXFLAGS+=$(shell pkg-config --cflags libusb-1.0)
else ifneq(, $(findstring mingw, $(SYS)))
    LINKFLAGS+=-L"C:\libusb\MinGW32\static\libusb-1.0.a"
	CXXFLAGS+=-I"C:\libusb\include\libusb-1.0"
endif

SRC=session.cpp device_cee.cpp device_m1000.cpp cli.cpp
OBJ=$(SRC:%.cpp=%.o)

$(BIN): cli.o $(LIB)
	$(CXX) -o $(BIN) $^ $(LINKFLAGS)

$(LIB): $(OBJ)
	ar crf $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -o $@ -c $<

-include $(OBJ:%.o=%.d)

clean:
	rm -f $(OBJ) $(BIN) $(OBJ:%.o=%.d) pysmu.so

python: $(LIB)
	$(CXX) $(CXXFLAGS) -I/usr/include/python2.7 -o pysmu.o -c pysmu.cpp
	$(CXX) $(CXXFLAGS) -shared pysmu.o $(LIB) $(LINKFLAGS) -lpython2.7 -o pysmu.so
