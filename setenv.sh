#!/bin/sh

echo "Loading GCC 9.1.0 environment"

export LD_LIBRARY_PATH=/home/patrick.carribault/LOCAL/GCC/gcc910_install/lib64:/home/patrick.carribault/LOCAL/MPC/mpc110_install/lib:/home/patrick.carribault/LOCAL/MPFR/mpfr401_install/lib:/home/patrick.carribault/LOCAL/GMP/gmp612_install/lib:$LD_LIBRARY_PATH
export PATH=/home/patrick.carribault/LOCAL/GCC/gcc910_install/bin:$PATH
export CPATH=/home/patrick.carribault/LOCAL/GMP/gmp612_install/include:$CPATH

echo "Loading MPI environment"
module load mpi
export OMPI_MPICXX=g++_910
export OMPI_MPICC=gcc_910

