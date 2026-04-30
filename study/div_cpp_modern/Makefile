CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g

SRC = main.cpp vm.cpp
OBJ = $(SRC:.cpp=.o)

all: div_modern

div_modern: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) div_modern

.PHONY: all clean
