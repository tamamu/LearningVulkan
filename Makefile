CFLAGS = -std=c++17 -Wall -Wextra
LDFLAGS = `pkg-config --static --libs glfw3` -lvulkan
CXX = clang++

.PHONY: test clean

release: src/*.cpp src/*.hpp src/*.h
	mkdir -p bin
	$(CXX) $(CFLAGS) -o bin/VulkanApp src/main.cpp $(LDFLAGS)

debug: src/main.cpp
	$(CXX) $(CFLAGS) src/main.cpp $(LDFLAGS)

test: debug
	./a.out

clean:
	rm -f bin/VulkanApp
	rm -f a.out