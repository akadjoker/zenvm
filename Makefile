# zen — Makefile
# g++ -std=c++11, all .cpp in src/

CXX      = g++
CXXFLAGS = -std=c++11 -O2 -Wall -Wextra -Wno-unused-parameter
SRCDIR   = src
BUILDDIR = build
TARGET   = zen

# Core sources: everything except test_*.cpp and main entry points
CORE_SRCS = $(filter-out $(SRCDIR)/test_%.cpp $(SRCDIR)/main.cpp $(SRCDIR)/zen_main.cpp, $(wildcard $(SRCDIR)/*.cpp))
CORE_OBJS = $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(CORE_SRCS))

# CLI binary (zen_main.cpp)
CLI_OBJS = $(CORE_OBJS) $(BUILDDIR)/zen_main.o

# VM test binary (main.cpp — handcoded bytecode tests)
TEST_VM_OBJS = $(CORE_OBJS) $(BUILDDIR)/main.o

.PHONY: all clean run debug test_vm

all: $(TARGET)

$(TARGET): $(CLI_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Old handcoded VM tests
test_vm: $(TEST_VM_OBJS)
	$(CXX) $(CXXFLAGS) -o zen_test_vm $^
	./zen_test_vm

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR) $(TARGET) zen_test_vm

run: $(TARGET)
	./$(TARGET)

debug: CXXFLAGS = -std=c++11 -g -O0 -Wall -Wextra -Wno-unused-parameter -DZEN_DEBUG_TRACE_EXEC
debug: clean $(TARGET)

# Stress test for collections (separate binary)
TEST_COLL_SRCS = $(SRCDIR)/test_collections.cpp $(SRCDIR)/memory.cpp
bench: $(TEST_COLL_SRCS)
	$(CXX) $(CXXFLAGS) -flto -I$(SRCDIR) $^ -o test_collections
	./test_collections

# Edge-case correctness tests (with sanitizers)
SANITIZE = -fsanitize=address,undefined -fno-omit-frame-pointer
TEST_EDGE_SRCS = $(SRCDIR)/test_edge_cases.cpp $(SRCDIR)/memory.cpp
test_edge: $(TEST_EDGE_SRCS)
	$(CXX) -std=c++11 -O1 -g -Wall -Wextra -Wno-unused-parameter $(SANITIZE) -I$(SRCDIR) $^ -o test_edge_cases
	ASAN_OPTIONS=detect_leaks=0 ./test_edge_cases
