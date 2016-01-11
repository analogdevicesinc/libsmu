CXX=g++
CXXFLAGS=-g -std=c++11 -Wall -pedantic -O0 -fPIC
LINKFLAGS=-lm -lpthread
BIN=smu
LIB=smu.a

SYS := $(shell gcc -dumpmachine)
ifneq (, $(findstring linux, $(SYS)) $(findstring darwin, $(SYS))) 
	LINKFLAGS+=$(shell pkg-config --libs libusb-1.0)
	CXXFLAGS+=$(shell pkg-config --cflags libusb-1.0)
	PYCXXFLAGS=$(shell pkg-config --cflags python-2.7)
	PYLINKFLAGS=$(shell pkg-config --libs python-2.7)
	SHARE=libsmu.so
	PYSHARE=libpysmu.so
else
		CXXFLAGS += -v -static -static-libgcc -static-libstdc++ -g
		LINKFLAGS+="/usr/local/lib/libusb-1.0.a"
		CXXFLAGS+=-I"C:\libusb\include\libusb-1.0"
		PYCXXFLAGS=-I"C:\Python27\include"
		PYLINKFLAGS="C:\Python27\libs\libpython27.a"
		SHARE=libsmu.dll
		PYSHARE=libpysmu.pyd
endif


SRC=session.cpp device_cee.cpp device_m1000.cpp cli.cpp
OBJ=$(SRC:%.cpp=%.o)

all: $(LIB) $(BIN) $(SHARE)

$(LIB): $(OBJ)
	ar cr $@ $^

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
