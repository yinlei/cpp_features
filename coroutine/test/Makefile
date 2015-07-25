CC=g++
CFLAGS=-std=c++11 -g -Wall
TARGET=simple.t nthread.t 1thread.t sleep.t
#OBJS=$(patsubst %.cpp, %.o, $(wildcard *.cpp))
OBJS=$(patsubst %, %.o, $(TARGET))

all : $(TARGET)

%.t: %.o
	$(CC) $(CFLAGS) $^ -L.. -lcoroutine -o $@ -lboost_system -lboost_thread
	echo "LD $(TARGET)"

.cpp.o:
	$(CC) $(CFLAGS) -c $^ -I.. -o $@
	echo "MK $@"

.PHONY: clean

clean:
	rm *.o $(TARGET) -f
