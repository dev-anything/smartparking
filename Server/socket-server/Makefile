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
	@rm -rf build/*