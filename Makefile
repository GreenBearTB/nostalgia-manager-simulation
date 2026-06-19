# Portable build for Nostalgia Manager Simulation.
# (The repo also ships a Visual Studio solution: NostalgiaManager.sln, which
#  builds the Dear ImGui GUI with the Win32 + Direct3D 11 backend.)
#
#   make            -> builds both the console tools and the GUI
#   make console    -> build/NostalgiaManager      (text build; --bench/--trace)
#   make gui        -> build/nm-gui                 (Dear ImGui window, GLFW/GL3)
#
# The GUI target needs GLFW + OpenGL dev packages:
#   sudo apt-get install libglfw3-dev libgl1-mesa-dev

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
SRCDIR    = NostalgiaManager/src
BUILDDIR  = build

# Shared engine / data / core sources (no main()).
CORE = $(SRCDIR)/core/Team.cpp $(SRCDIR)/data/Database.cpp \
       $(SRCDIR)/engine/Config.cpp $(SRCDIR)/engine/MatchEngine.cpp

# Console build (the original text front-end + dev utilities).
CONSOLE_SRC = $(CORE) $(SRCDIR)/ui/Game.cpp $(SRCDIR)/main.cpp
CONSOLE_BIN = $(BUILDDIR)/NostalgiaManager

# GUI build.
IMGUI_DIR = third_party/imgui
IMGUI_SRC = $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_draw.cpp \
            $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp \
            $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp \
            $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp
GUI_SRC = $(CORE) $(SRCDIR)/ui/App.cpp $(SRCDIR)/platform/main_glfw.cpp $(IMGUI_SRC)
GUI_BIN = $(BUILDDIR)/nm-gui
GUI_INC = -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends
GUI_LIBS = -lglfw -lGL -ldl -lpthread

.PHONY: all console gui run clean

all: console gui

console: $(CONSOLE_BIN)
$(CONSOLE_BIN): $(CONSOLE_SRC)
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(CONSOLE_SRC) -o $(CONSOLE_BIN)

gui: $(GUI_BIN)
$(GUI_BIN): $(GUI_SRC)
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(GUI_INC) $(GUI_SRC) -o $(GUI_BIN) $(GUI_LIBS)

run: gui
	./$(GUI_BIN) NostalgiaManager/data

clean:
	rm -rf $(BUILDDIR)
