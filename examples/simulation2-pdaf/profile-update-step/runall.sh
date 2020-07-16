for i in 7 8 9 10 11 12
do
    sbatch -N $i profiling-no-scorep.sbatch
    sleep 2
done
