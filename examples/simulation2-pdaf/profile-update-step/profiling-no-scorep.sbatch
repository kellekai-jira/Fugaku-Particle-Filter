#!/bin/bash
#SBATCH --job-name=melissa-da-profiling-no-scorep
#SBATCH --time=0:30:00
#SBATCH --output=big-%j.log
#SBATCH -A prcoe03
##SBATCH --partition devel
source ~/bin/ms
python3 script-juwels-profiling-server.py

