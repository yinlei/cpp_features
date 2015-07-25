CC=g++
CFLAGS=-std=c++11 -g -Wall
TARGET=hook.t
#OBJS=$(patsubst %.cpp, %.o, $(wildcard *.cpp))
OBJS=$(patsubst %, %.o, $(TARGET))

all : $(TARGET)

%.t: %.o
	$(CC) $(CFLAGS) $^ -L.. -lcoroutine -o $@ -lboost_system -lboost_thread -ldl
	echo "LD $(TARGET)"

.cpp.o:
	$(CC) $(CFLAGS) -c $^ -I.. -o $@
	echo "MK $@"

.PHONY: clean

clean:
	rm *.o $(TARGET) -f
