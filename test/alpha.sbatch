#!/bin/sh

#SBATCH -N 8
#SBATCH --ntasks-per-node=1
#SBATCH --gpus-per-node=1
#SBATCH --mem=1G
#SBATCH -t 5:00
#SBATCH -o test/out/alpha/log

module purge
. env/alpha.rc

cmake --build $BUILD -j 2
make -f test/Makefile
