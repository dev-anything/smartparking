CC      = gcc
CFLAGS  = -g -Wall -Iinclude
LDFLAGS = -lmysqlclient -lpthread

SRC     = $(wildcard src/*.c)
OBJ     = $(SRC:src/%.c=build/%.o)
HEADERS = $(wildcard include/*.h)
TARGET  = build/main

.PHONY: build run clean

build: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

# 모든 .o는 모든 헤더에 의존: 헤더가 하나라도 바뀌면 전체 재컴파일
$(OBJ): $(HEADERS)

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

run: build
	./$(TARGET)

clean:
	rm -rf build/*

# 순서 작성해보기
# 1. 모든 build/%.o 파일은 src/%.c 파일에 의존한다.(% => 같은 이름)
# 2. 모든 OBJ 파일(build/%.o)은 HEADERS(include/*.h)에 의존한다.
# 3. TARGET(build/main)은 OBJ에 의존한다.
# 4. build 명령은 TARGET에 의존한다.
# 5. run 명령은 TARGET을 실행한다.
# 6. build, run, clean은 파일이 아니라 명령이다.