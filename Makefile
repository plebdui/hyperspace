# hyperspace: make, then ./hyperspace (or make install for ~/.local/bin/hyperspace)
CXXFLAGS ?= -O2 -Wall
PKGS = sdl3 epoxy libpulse-simple
hyperspace: main.cpp Audio.hpp Shaders.hpp Wire.hpp Geometry.hpp LookShader.hpp Looks.hpp Media.hpp
	$(CXX) -std=c++17 $(CXXFLAGS) main.cpp -o $@ $(shell pkg-config --cflags --libs $(PKGS)) -lpthread
install: hyperspace
	ln -sf $(CURDIR)/hyperspace $(HOME)/.local/bin/hyperspace
clean:
	rm -f hyperspace
.PHONY: install clean
