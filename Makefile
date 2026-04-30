# zen — Makefile
# g++ -std=c++11, all .cpp in src/

CXX      = g++
CXXFLAGS = -std=c++11 -O2 -Wall -Wextra -Wno-unused-parameter
SRCDIR   = src
BUILDDIR = build
TARGET   = zen

SRCS = $(filter-out $(SRCDIR)/test_%.cpp, $(wildcard $(SRCDIR)/*.cpp))
OBJS = $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRCS))

.PHONY: all clean run debug

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR) $(TARGET)

run: $(TARGET)
	./$(TARGET)

debug: CXXFLAGS = -std=c++11 -g -O0 -Wall -Wextra -Wno-unused-parameter -DZEN_DEBUG_TRACE_EXEC
debug: clean $(TARGET)

# Stress test for collections (separate binary)
TEST_COLL_SRCS = $(SRCDIR)/test_collections.cpp $(SRCDIR)/memory.cpp
bench: $(TEST_COLL_SRCS)
	$(CXX) $(CXXFLAGS) -flto -I$(SRCDIR) $^ -o test_collections
	./test_collections
