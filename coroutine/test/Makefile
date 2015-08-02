CC=g++
CFLAGS=-std=c++11 -g -Wall
TARGET=$(patsubst %.cpp, %.t, $(wildcard *.cpp)) pg_cpu.t
OBJS=$(patsubst %, %.o, $(TARGET))

all : $(TARGET)

pg_cpu.t: pg_cpu.o
	@$(CC) $(CFLAGS) -pg $^ -o $@ -lboost_thread -lboost_system -L.. -lcoroutine -ldl
	@echo "LD $@"

pg_cpu.o: cpu.cpp
	@$(CC) $(CFLAGS) -pg -c $^ -I.. -o $@
	@echo "MK $@"

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
	rm *.o *.t -f
