# Portable build for the Nostalgia Manager Simulation console game.
# (The repo also ships a Visual Studio solution: NostalgiaManager.sln)

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
SRCDIR    = NostalgiaManager/src
SOURCES   = $(shell find $(SRCDIR) -name '*.cpp')
BUILDDIR  = build
TARGET    = $(BUILDDIR)/NostalgiaManager

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SOURCES)
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

run: all
	./$(TARGET) NostalgiaManager/data

clean:
	rm -rf $(BUILDDIR)
