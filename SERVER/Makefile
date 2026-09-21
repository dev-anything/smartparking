CC = gcc
FLAGS = -g -Wall
LIBS = -lmysqlclient -lcurl -ljson-c

TARGET = llmserver
OBJS = llmserver.o
RAW = llmserver.c

.PHONY: compile out run clean build



compile:
	$(CC) -c $(FLAGS) $(RAW)

out:
	$(CC) -o $(TARGET) $(OBJS) $(LIBS)

run:
	./$(TARGET)

clean:
	rm -f $(TARGET) *.o

build:
	$(MAKE) clean
	$(MAKE) compile
	$(MAKE) out