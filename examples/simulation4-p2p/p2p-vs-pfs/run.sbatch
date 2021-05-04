#!/bin/bash
#SBATCH -N 50
#SBATCH -A igf@cpu
#SBATCH --time=0:20:0

source $HOME/setenv_melissa-p2p.sh

python3 -u script.py
