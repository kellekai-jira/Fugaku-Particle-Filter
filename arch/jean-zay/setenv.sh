
module load python/3.7.5                        # don't have this while compiling! (it will hide intels libmpi with the anaconda libmpi :/ )

#Currently Loaded Modulefiles:
#module load tcl/8.6.8
module load intel-compilers/19.0.4
module load intel-all/2019.4
module load zeromq/4.2.5
module load intel-mkl/2019.4
module load intel-mpi/2019.4
#module load python/3.7.5                        # don't have this while compiling! (it will hide intels libmpi with the anaconda libmpi :/ )
module load intel-advisor/2019.4
module load cmake/3.14.4
module load intel-tbb/2019.6
module load intel-itac/2019.4
module load netcdf/4.7.2-mpi
module load netcdf-fortran/4.5.2-mpi
module load hypre/2.18.2-mpi
module load hdf5/1.10.5-mpi
export PDAF_ARCH=linux_ifort


export PYTHONPATH=$PYTHONPATH:$HOME/workspace/repex


ulimit -s unlimited
ulimit -c 4000000
