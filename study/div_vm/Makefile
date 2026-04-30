CXX            = g++
CPPFLAGS       =
BASE_CXXFLAGS  = -std=c++17 -Wall -Wextra
DEBUG_CXXFLAGS = -O0 -g
RELEASE_CXXFLAGS = -O3 -DNDEBUG

CONFIG ?= debug

CORE_SRC  = vm.cpp compiler.cpp
MAIN_SRC  = main.cpp

ifeq ($(CONFIG),release)
  BUILD_DIR = build/release
  CXXFLAGS  = $(BASE_CXXFLAGS) $(RELEASE_CXXFLAGS)
else
  BUILD_DIR = build/debug
  CXXFLAGS  = $(BASE_CXXFLAGS) $(DEBUG_CXXFLAGS)
endif

OBJDIR = $(BUILD_DIR)/obj
BINDIR = $(BUILD_DIR)/bin
TARGET = $(BINDIR)/div_vm
CORE_OBJ  = $(CORE_SRC:%.cpp=$(OBJDIR)/%.o)
MAIN_OBJ  = $(MAIN_SRC:%.cpp=$(OBJDIR)/%.o)

all: $(TARGET)

debug:
	$(MAKE) CONFIG=debug all

release:
	$(MAKE) CONFIG=release all

bench:
	$(MAKE) CONFIG=$(CONFIG) all

bench-release:
	$(MAKE) CONFIG=release bench

$(TARGET): $(CORE_OBJ) $(MAIN_OBJ) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $^

$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $@

$(BINDIR):
	mkdir -p $@

clean:
	rm -rf build
	rm -f *.o div_vm div_vm_fib_bench

.PHONY: all debug release bench bench-release clean
