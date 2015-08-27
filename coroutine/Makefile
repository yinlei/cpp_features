CC=g++
CFLAGS=-std=c++11 -g -O3 -Wall -fPIC
STATIC_LIB=libcoroutine.a
DYNAMIC_LIB=libcoroutine.so
INCLUDE=-Ilinux
OBJS=$(patsubst %.cpp, %.o, $(wildcard *.cpp)) $(patsubst %.cpp, %.o, $(wildcard linux/*.cpp))

all : $(STATIC_LIB) $(DYNAMIC_LIB)

$(STATIC_LIB) : $(OBJS)
	@ar cr $@ $^
	@echo "LD $(STATIC_LIB)"

$(DYNAMIC_LIB) : $(OBJS)
	@echo "CC dynamic-link hook module."
	@rm linux_glibc_hook.o -f
	@$(CC) $(CFLAGS) $(INCLUDE) -DCO_DYNAMIC_LINK -c linux_glibc_hook.cpp -o linux_glibc_hook.o
	@$(CC) $(CFLAGS) $(INCLUDE) -shared $^ -o $@
	@echo "LD $(DYNAMIC_LIB)"
	@rm linux_glibc_hook.o -f

.cpp.o:
	@echo "CC $@"
	@$(CC) $(CFLAGS) $(INCLUDE) -c $^ -o $@

.PHONY: clean install

clean:
	rm *.o linux/*.o $(STATIC_LIB) $(DYNAMIC_LIB) -f

install:
	install -m 0664 $(STATIC_LIB) /usr/lib
	install -m 0775 $(DYNAMIC_LIB) /usr/lib
	mkdir -p /usr/include/coroutine
	install -m 0664 *.h /usr/include/coroutine
	install -m 0664 linux/*.h /usr/include/coroutine
