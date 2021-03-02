CFLAGS = -std=c++17 -Wall -Wextra
LDFLAGS = `pkg-config --static --libs glfw3` -lvulkan
CXX = g++
GLSLS = $(foreach glsl,$(shell ls src/shaders),spir-v/$(notdir $(glsl)).spv)
.SUFFIXES: .vert .frag
.PHONY: spir-v/%.spv test clean

spir-v/%.vert.spv: src/shaders/%.vert
	mkdir -p spir-v
	glslc $< -o $@

spir-v/%.frag.spv: src/shaders/%.frag
	mkdir -p spir-v
	glslc $< -o $@

release: src/*.cpp src/*.hpp src/*.h $(GLSLS)
	mkdir -p bin
	$(CXX) $(CFLAGS) -o bin/VulkanApp src/main.cpp $(LDFLAGS) -DNDEBUG

debug: src/main.cpp $(GLSLS)
	$(CXX) $(CFLAGS) src/main.cpp $(LDFLAGS)

test: debug
	./a.out

clean:
	rm -f bin/VulkanApp
	rm -f a.out
