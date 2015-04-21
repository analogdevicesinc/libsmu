CXX=g++
CXXFLAGS=-g -std=c++11 -Wall -pedantic -O0 -fPIC
LINKFLAGS=-lm -lpthread
BIN=smu
LIB=smu.a

SYS := $(shell gcc -dumpmachine)
ifneq (, $(findstring linux, $(SYS)))
	LINKFLAGS+=$(shell pkg-config --libs libusb-1.0)
	CXXFLAGS+=$(shell pkg-config --cflags libusb-1.0)
	PYCXXFLAGS=$(shell pkg-config --cflags python)
	PYLINKFLAGS=$(shell pkg-config --libs python)
	SHARE=libsmu.so
	PYSHARE=libpysmu.so
else
	LINKFLAGS+="C:\libusb\MinGW32\static\libusb-1.0.a"
	CXXFLAGS+=-I"C:\libusb\include\libusb-1.0"
	PYCXXFLAGS=-I"C:\Python27\include"
	PYLINKFLAGS="C:\Python\libs\libpython27.a"
	SHARE=libsmu.dll
	PYSHARE=libpysmu.dll
endif

SRC=session.cpp device_cee.cpp device_m1000.cpp cli.cpp
OBJ=$(SRC:%.cpp=%.o)

all: $(LIB) $(BIN) $(SHARE)

$(LIB): $(OBJ)
	ar crf $@ $^

$(BIN): cli.o $(LIB)
	$(CXX) -o $(BIN) $^ $(LINKFLAGS)

$(SHARE): $(LIB)
	$(CXX) -o $(SHARE) -shared $(OBJ) $(LINKFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -o $@ -c $<

-include $(OBJ:%.o=%.d)

clean:
	rm -f $(OBJ) $(BIN) $(OBJ:%.o=%.d) $(LIB) $(SHARE)

python: $(LIB)
	$(CXX) $(CXXFLAGS) $(PYCXXFLAGS) -o libpysmu.o -c pysmu.cpp
	$(CXX) $(CXXFLAGS) -shared libpysmu.o $(LIB) $(LINKFLAGS) $(PYLINKFLAGS) -o $(PYSHARE)
