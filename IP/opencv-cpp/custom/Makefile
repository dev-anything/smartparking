# C++ - OpenCV Makefile

CXX = g++

ORT_DIR = $(HOME)/onnxruntime_gpu
SRC     = $(wildcard src/*.cpp)
OBJ     = $(SRC:src/%.cpp=build/%.o)
HEADERS = $(wildcard include/*.h)
TARGET  = build/main

CFLAGS  = -g -Wall -std=c++11 -Iinclude $(shell pkg-config --cflags opencv4) \
          -I$(ORT_DIR)/include
LDFLAGS = -lmysqlclient -lpthread $(shell pkg-config --libs opencv4) \
          -L$(ORT_DIR)/lib -lonnxruntime -Wl,-rpath,$(ORT_DIR)/lib

.PHONY: build run clean delete

build: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

# 모든 .o는 모든 헤더에 의존: 헤더가 하나라도 바뀌면 전체 재컴파일
$(OBJ): $(HEADERS)

build/%.o: src/%.cpp
	@mkdir -p build
	$(CXX) $(CFLAGS) -c $< -o $@

run: build
	./$(TARGET)

clean:
	rm -rf build/*

delete:
	rm -f images/*

# 순서 작성해보기
# 1. 모든 build/%.o 파일은 src/%.c 파일에 의존한다.(% => 같은 이름)
# 2. 모든 OBJ 파일(build/%.o)은 HEADERS(include/*.h)에 의존한다.
# 3. TARGET(build/main)은 OBJ에 의존한다.
# 4. build 명령은 TARGET에 의존한다.
# 5. run 명령은 TARGET을 실행한다.
# 6. build, run, clean은 파일이 아니라 명령이다.