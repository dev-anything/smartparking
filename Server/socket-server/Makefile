CC = gcc
FLAGS = -g -Wall
LIBS = -lmysqlclient -lcurl -ljson-c -lmicrohttpd -lpthread

OUTPUT_DIR = build

TARGET = server
OBJS = server.o
RAW = server.c

.PHONY: compile out run clean build



compile:
	$(CC) -c $(FLAGS) $(RAW) -o $(OUTPUT_DIR)/$(OBJS)

out:
	$(CC) -o $(OUTPUT_DIR)/$(TARGET) $(OUTPUT_DIR)/$(OBJS) $(LIBS)

run:
	./$(OUTPUT_DIR)/$(TARGET)

clean:
	rm -f $(OUTPUT_DIR)/*

build:
	$(MAKE) clean
	$(MAKE) compile
	$(MAKE) out
	$(MAKE) run