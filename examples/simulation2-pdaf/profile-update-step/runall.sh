for i in 47
do
    sbatch -N $i profiling-no-scorep.sbatch
    sleep 2
done
