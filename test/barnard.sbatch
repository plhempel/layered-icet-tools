#!/bin/sh

#SBATCH -N 8
#SBATCH --ntasks-per-node=1
#SBATCH --mem=1G
#SBATCH -t 5:00
#SBATCH -o test/out/barnard/log

module purge
. env/barnard.rc

cmake --build $BUILD -j 2
make -f test/Makefile
