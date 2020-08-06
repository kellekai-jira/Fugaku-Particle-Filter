for i in `seq 3 8`
do
    sbatch -N $i profiling-no-scorep.sbatch
    sleep 2
done
