CXX=clang++
CXXFLAGS=-g -std=c++11 -Wall -pedantic -O3
LINKFLAGS=-lusb-1.0 -lm
BIN=smu

SRC=cli.cpp session.cpp signal.cpp device_cee.cpp
OBJ=$(SRC:%.cpp=%.o)

$(BIN): $(OBJ)
	$(CXX) -o $(BIN) $^ $(LINKFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -o $@ -c $<

-include $(OBJ:%.o=%.d)

clean:
	rm -f $(OBJ) $(BIN) $(OBJ:%.o=%.d)
