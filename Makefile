CXX=g++_910
CC=gcc_910

MPICC=mpicc

PLUGIN_FLAGS=-I`$(CC) -print-file-name=plugin`/include -g -Wall -fno-rtti -shared -fPIC

CFLAGS=-g -O3

all: test3.out

test3.out : plugin.so test3.c
	$(MPICC) test3.c $(CFLAGS) -fplugin=plugin.so -o $@

plugin.so: plugin.cpp
	$(CXX) plugin.cpp -shared $(PLUGIN_FLAGS) -o $@

clean:
	rm -rf *.out

clean_all: clean
	rm -rf libplugin*.so *.dot *.out
