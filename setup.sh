#!/bin/bash

export OMPI_MPICC=~/GCC/gcc-9.1.0/MYBUILD/bin/gcc
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:`pwd`
echo ${LD_LIBRARY_PATH}
echo ${OMPI_MPICC}
