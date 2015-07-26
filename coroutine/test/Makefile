CC=g++
CFLAGS=-std=c++11 -g -Wall
TARGET=simple.t nthread.t 1thread.t sleep.t hiredis.t s_hiredis.t
#OBJS=$(patsubst %.cpp, %.o, $(wildcard *.cpp))
OBJS=$(patsubst %, %.o, $(TARGET))

all : $(TARGET)

s_hiredis.t: hiredis.o
	@$(CC) $(CFLAGS) $^ -L.. -lhiredis -lcoroutine -o $@ -lboost_system -lboost_thread -ldl \
	   	-static -static-libgcc -static-libstdc++
	@echo "LD $(TARGET)"

hiredis.t: hiredis.o
	@$(CC) $(CFLAGS) $^ -L.. -lhiredis -lcoroutine -o $@ -lboost_system -lboost_thread -ldl
	@echo "LD $(TARGET)"

%.t: %.o
	@$(CC) $(CFLAGS) $^ -L.. -lcoroutine -o $@ -lboost_system -lboost_thread -ldl
	@echo "LD $(TARGET)"

.cpp.o:
	@$(CC) $(CFLAGS) -c $^ -I.. -o $@
	@echo "MK $@"

.PHONY: clean

clean:
	rm *.o $(TARGET) -f
