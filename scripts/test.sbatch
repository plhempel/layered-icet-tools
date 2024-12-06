#!/bin/sh

#SBATCH -n            8
#SBATCH -t            5:00
#SBATCH --mem-per-cpu=1G
#SBATCH -o            out/test/log

module purge
. env/barnard.rc

make -f scripts/test.mk
