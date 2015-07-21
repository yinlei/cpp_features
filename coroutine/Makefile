CC=g++
CFLAGS=-std=c++11 -g -Wall
TARGET=libcoroutine.a
OBJS=$(patsubst %.cpp, %.o, $(wildcard *.cpp))

all : $(TARGET)

$(TARGET) : $(OBJS)
	ar cr $@ $^
	echo "LD $(TARGET)"

.cpp.o:
	$(CC) $(CFLAGS) -c $^ -o $@
	echo "MK $@"

.PHONY: clean

clean:
	rm *.o *.a -f
