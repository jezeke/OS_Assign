CC = gcc
CFLAGS = -Wall -lpthread -lrt -g
TARGET = scheduler

all: $(TARGET)

$(TARGET) : scheduler.o
	$(CC) $(CFLAGS) scheduler.c -o $(TARGET)

scheduler.o : scheduler.c scheduler.h
	$(CC) $(CFLAGS) -c scheduler.c

clean :
	rm $(TARGET) scheduler.o
