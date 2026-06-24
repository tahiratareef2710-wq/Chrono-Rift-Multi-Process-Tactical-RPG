

CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -pthread

LIBS = -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-network -lsfml-system -lrt
# LIBS = $(shell sdl2-config --libs) -lrt
# LIBS = -lglfw -lGL -lrt
# LIBS = -lncurses -lrt

TARGETS = arbiter_out hip_out hip2_out asp_out render_out menu_out

all: clean $(TARGETS)
	@echo Build complete.

arbiter_out: arbiter/arbiter.cpp
	$(CXX) $(CXXFLAGS) arbiter/*.cpp -o arbiter_out $(LIBS)

hip_out: hip/hip.cpp
	$(CXX) $(CXXFLAGS) hip/*.cpp -o hip_out $(LIBS)
	
hip2_out: hip2/hip2.cpp
	$(CXX) $(CXXFLAGS) hip2/*.cpp -o hip2_out $(LIBS)

asp_out: asp/asp.cpp
	$(CXX) $(CXXFLAGS) asp/*.cpp -o asp_out $(LIBS)

render_out: render/render.cpp
	$(CXX) $(CXXFLAGS) render/*.cpp -o render_out $(LIBS)
	
menu_out: main_menu.cpp
	$(CXX) $(CXXFLAGS) main_menu.cpp -o menu_out

clean:
	rm -f arbiter_out hip_out hip2_out asp_out render_out menu_out

.PHONY: all clean
