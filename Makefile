# hyperspace: make, then ./hyperspace (or make install for ~/.local/bin/hyperspace)
CXXFLAGS ?= -O2 -Wall
PKGS = sdl3 epoxy libpulse-simple
PKG_CFLAGS = $(shell pkg-config --cflags $(PKGS))
PKG_LIBS = $(shell pkg-config --libs $(PKGS))

# vendored Dear ImGui (the settings sidebar) — compiled once into objects so
# rebuilding main.cpp stays quick
IMGUI_DIR = imgui
IMGUI_SRC = imgui.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp \
            imgui_impl_sdl3.cpp imgui_impl_opengl3.cpp
IMGUI_OBJ = $(addprefix $(IMGUI_DIR)/, $(IMGUI_SRC:.cpp=.o))
IMGUI_CFLAGS = -I$(IMGUI_DIR) $(shell pkg-config --cflags sdl3)

HEADERS = FractalLogo.hpp Loading.hpp Ineffable.hpp Ineffable.glsl Audio.hpp Shaders.hpp Wire.hpp Geometry.hpp LookShader.hpp Looks.hpp Media.hpp FracDive.hpp

hyperspace: main.cpp $(HEADERS) $(IMGUI_OBJ)
	$(CXX) -std=c++17 $(CXXFLAGS) -I$(IMGUI_DIR) main.cpp $(IMGUI_OBJ) -o $@ $(PKG_CFLAGS) $(PKG_LIBS) -lpthread -ldl

# ImGui objects: built with -O2 only (its own code, not ours to -Wall)
$(IMGUI_DIR)/%.o: $(IMGUI_DIR)/%.cpp
	$(CXX) -std=c++17 -O2 $(IMGUI_CFLAGS) -c $< -o $@

install: hyperspace
	ln -sf $(CURDIR)/hyperspace $(HOME)/.local/bin/hyperspace
clean:
	rm -f hyperspace $(IMGUI_OBJ)
.PHONY: install clean
