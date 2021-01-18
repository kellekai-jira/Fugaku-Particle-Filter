module purge

# ESIAS/WRF:
module load intel-para
module load ParaStationMPI
module load netCDF-Fortran/4.5.3

module load imkl
module load netCDF-C++4
module load HDF5
module load GDB/10.1
module load CMake/3.18.0

ulimit -s unlimited
ulimit -c 4000000
