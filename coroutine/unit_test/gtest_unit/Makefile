CC=g++
CFLAGS=-std=c++11 -g -Wall
INCLUDE=-Igtest/include -I/usr/include/coroutine
LINK=-Lgtest/lib/.libs -lcoroutine -ldl -lgtest_main -lgtest -lpthread \
	 -lboost_thread -lboost_system -lpthread
STATIC=-static -static-libgcc -static-libstdc++
TARGET=$(patsubst %.cpp, %.t, $(wildcard *.cpp)) $(patsubst %.cpp, s_%.t, $(wildcard *.cpp))

all : /usr/lib/libgtest.a $(TARGET)

/usr/lib/libgtest.a:
	cd gtest && ./configure && make
	sudo cp gtest/lib/.libs/* /usr/lib

s_%.t: %.o
	@echo "LD $@"
	@$(CC) $(CFLAGS) $^ -o $@ $(INCLUDE) $(LINK) $(STATIC)

%.t: %.o
	@echo "LD $@"
	@$(CC) $(CFLAGS) $^ -o $@ $(INCLUDE) $(LINK)

.cpp.o:
	@echo "CC $@"
	@$(CC) $(CFLAGS) -c $^ -o $@ $(INCLUDE)

.PHONY: clean

clean:
	rm *.o $(TARGET) -f
