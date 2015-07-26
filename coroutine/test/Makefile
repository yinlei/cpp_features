CC=g++
CFLAGS=-std=c++11 -g -Wall
TARGET=simple.t nthread.t 1thread.t sleep.t hiredis.t s_hiredis.t
#OBJS=$(patsubst %.cpp, %.o, $(wildcard *.cpp))
OBJS=$(patsubst %, %.o, $(TARGET))

all : $(TARGET)

s_hiredis.t: hiredis.o
	@$(CC) $(CFLAGS) $^ -o $@ -L.. -lhiredis -lcoroutine -static -static-libgcc -static-libstdc++
	@echo "LD $@"

hiredis.t: hiredis.o
	@$(CC) $(CFLAGS) $^ -o $@ -L.. -lhiredis -lcoroutine -ldl
	@echo "LD $@"

%.t: %.o
	@$(CC) $(CFLAGS) $^ -o $@ -lboost_thread -lboost_system -L.. -lcoroutine -ldl
	@echo "LD $@"

.cpp.o:
	@$(CC) $(CFLAGS) -c $^ -I.. -o $@
	@echo "MK $@"

.PHONY: clean

clean:
	rm *.o $(TARGET) -f
