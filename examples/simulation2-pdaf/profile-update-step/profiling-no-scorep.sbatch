#!/bin/bash
#SBATCH --job-name=melissa-da-profiling-no-scorep    # Job name
#SBATCH --time=0:30:00               # Time limit hrs:min:sec
#SBATCH --output=big-%j.log   # Standard output and error log
#SBATCH -A prcoe03
#SBATCH --partition devel
source ~/bin/ms
python3 script-juwels-profiling-server.py

