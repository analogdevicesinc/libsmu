CXX=clang++
CXXFLAGS=-g -std=c++11 -Wall -pedantic -O0 -fPIC
LINKFLAGS=-lusb-1.0 -lm
BIN=smu
LIB=smu.a

SRC=session.cpp signal.cpp device_cee.cpp
OBJ=$(SRC:%.cpp=%.o)

$(BIN): cli.o $(LIB)
	$(CXX) -o $(BIN) $^ $(LINKFLAGS)

$(LIB): $(OBJ)
	ar crf $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -o $@ -c $<

-include $(OBJ:%.o=%.d)

clean:
	rm -f $(OBJ) $(BIN) $(OBJ:%.o=%.d)
