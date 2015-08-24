CC=g++
CFLAGS=-std=c++11 -g -Wall -fPIC
STATIC_LIB=libwin_patch.a
DYNAMIC_LIB=libwin_patch.so
OBJS=$(patsubst %.cpp, %.o, $(wildcard *.cpp))

all : $(STATIC_LIB) $(DYNAMIC_LIB)

$(STATIC_LIB) : $(OBJS)
	@ar cr $@ $^
	@echo "LD $(STATIC_LIB)"

$(DYNAMIC_LIB) : $(OBJS)
	@$(CC) $(CFLAGS) -shared $^ -o $@
	@echo "LD $(DYNAMIC_LIB)"

.cpp.o:
	@echo "CC $@"
	@$(CC) $(CFLAGS) -c $^ -o $@

.PHONY: clean

clean:
	rm *.o $(STATIC_LIB) $(DYNAMIC_LIB) -f
