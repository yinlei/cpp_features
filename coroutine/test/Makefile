CC=g++
CFLAGS=-std=c++11 -g -Wall
TARGET=test_coroutine
OBJS=$(patsubst %.cpp, %.o, $(wildcard *.cpp))

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $^ -L.. -lcoroutine -o $@ -Wl,--no-as-needed -lpthread
	echo "LD $(TARGET)"

.cpp.o:
	$(CC) $(CFLAGS) -c $^ -I.. -o $@
	echo "MK $@"

.PHONY: clean

clean:
	rm *.o $(TARGET) -f
