#!/bin/bash
#SBATCH -N 18
#SBATCH -A igf@cpu
#SBATCH --time=0:25:0

source $HOME/setenv_melissa-p2p.sh

python3 -u script.py
