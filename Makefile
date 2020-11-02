HOST_GCC=~/GCC/gcc-9.1.0/MYBUILD/bin/g++
TARGET_GCC=mpicc
PLUGIN_SOURCE_FILES= plugin.cpp
GCCPLUGINS_DIR:= $(shell $(TARGET_GCC) -print-file-name=plugin)
CXXFLAGS+= -I$(GCCPLUGINS_DIR)/include -fPIC -fno-rtti -g 

all: test3

test: plugin.so test.c
	$(TARGET_GCC) -fplugin=plugin.so test.c

test2: plugin.so test2.c
	$(TARGET_GCC) -fplugin=plugin.so test2.c

test3: plugin.so test3.c
	$(TARGET_GCC) -fplugin=plugin.so test3.c

plugin.so: $(PLUGIN_SOURCE_FILES)
	$(HOST_GCC) -shared $(CXXFLAGS) $^ -o $@

clean:
	rm -rf *.so 


