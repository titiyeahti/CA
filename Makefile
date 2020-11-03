CXX=g++_910
CC=gcc_910

MPICC=mpicc

SRC=src/

PLUGIN_FLAGS=-I`$(CC) -print-file-name=plugin`/include -g -Wall -fno-rtti -shared -fPIC

CFLAGS=-g -O3

all: test3.out test2.out test4.out test5.out

test%.out : $(SRC)test%.c plugin.so
	$(MPICC) $< $(CFLAGS) -fplugin=plugin.so -o $@

plugin.so: $(SRC)plugin.cpp
	$(CXX) $^ -shared $(PLUGIN_FLAGS) -o $@

%.o : $(SRC)%.c $(SRC)%.h
	$(CXX) -c $< -o $@ $(PLUGIN_FLAGS)

clean:
	rm -rf *.out *so

clean_all: clean
	rm -rf *.so *.dot *.out
