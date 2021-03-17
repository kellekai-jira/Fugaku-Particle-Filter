module purge

#Currently Loaded Modulefiles:
#module load tcl/8.6.8
module load intel-compilers/19.0.4
module load intel-all/2019.4
module load zeromq/4.2.5
module load intel-mkl/2019.4
module load intel-mpi/2019.4
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



module load python/3.7.5  # don't have this while compiling!
# (it will hide intels libmpi with the anaconda libmpi :/ )

# anyway we overwrite python3.7.5 with our conda env. python 3.7.5 must still be loaded
# to have the conda command
conda activate /gpfsscratch/rech/moy/rkop006/conda_envs


# let the server find lib  python:  (don't know why I need to do this manually...)
#export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/gpfsscratch/rech/moy/rkop006/conda_envs/lib

# packages in environment at /gpfsscratch/rech/moy/rkop006/conda_envs:
#
# Name                    Version                   Build  Channel
#_libgcc_mutex             0.1                        main
#blas                      1.0                         mkl
#ca-certificates           2021.1.19            h06a4308_1
#certifi                   2020.12.5        py37h06a4308_0
#intel-openmp              2020.2                      254
#ld_impl_linux-64          2.33.1               h53a641e_7
#libedit                   3.1.20191231         h14c3975_1
#libffi                    3.3                  he6710b0_2
#libgcc-ng                 9.1.0                hdf63c60_0
#libgfortran-ng            7.3.0                hdf63c60_0
#libstdcxx-ng              9.1.0                hdf63c60_0
#mkl                       2020.2                      256
#mkl-service               2.3.0            py37he8ac12f_0
#mkl_fft                   1.2.0            py37h23d657b_0
#mkl_random                1.1.1            py37h0573a6f_0
#mpi                       1.0                       mpich
#mpi4py                    3.0.3            py37hf046da1_1
#mpich                     3.3.2                hc856adb_0
#ncurses                   6.2                  he6710b0_1
#numpy                     1.14.6           py37h3b04361_5
#numpy-base                1.14.6           py37hde5b4d6_5
#openssl                   1.1.1j               h27cfd23_0
#pandas                    1.0.5            py37h0573a6f_0
#pip                       21.0.1           py37h06a4308_0
#python                    3.7.10               hdb3f193_0
#python-dateutil           2.8.1              pyhd3eb1b0_0
#pytz                      2021.1             pyhd3eb1b0_0
#readline                  8.1                  h27cfd23_0
#setuptools                52.0.0           py37h06a4308_0
#six                       1.15.0           py37h06a4308_0
#sqlite                    3.33.0               h62c20be_0
#tk                        8.6.10               hbc83047_0
#wheel                     0.36.2             pyhd3eb1b0_0
#xz                        5.2.5                h7b6447c_0
#zlib                      1.2.11               h7b6447c_3



ulimit -s unlimited
ulimit -c 4000000
