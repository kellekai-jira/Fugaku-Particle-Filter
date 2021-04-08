#include <vector>
#include <algorithm>
#include <functional>
#include <mpi.h>
#include <iostream>
#include <cmath>
#include <fstream>
#include <cstdlib>
#include <cassert>
#include <sstream>

#ifdef __LORENZ_DEBUG__

#define OUT_SYNC( MSG ) do { \
  for(int i=0; i<comm_size; i++) { \
    if(comm_rank == i) { \
      std::cout << MSG << std::endl; \
    } \
    MPI_Barrier(MPI_COMM_WORLD); \
  } \
  MPI_Barrier(MPI_COMM_WORLD); \
} while(0)

#define OUT( MSG ) std::cout << MSG << std::endl

#else
#define OUT_SYNC( MSG )
#define OUT( MSG )
#endif // __LORENZ_DEBUG__

void exchange( std::vector<double> & x );
void d96( std::vector<double> & x_in, std::vector<double> & x_out, double F);
void RK_step( std::vector<double> & x, const std::vector<double> & k1,
    const std::vector<double> & k2, double w, double dt );
void integrate( std::vector<double> & x, double F, double dt );
void init_parallel();
void write_obs( const std::string file_name, std::vector<double> x,
    double share, int epoch );

int comm_rank, comm_size, mpi_left, mpi_right;
MPI_Comm comm;

const double F = 5;
const double dt = 0.01;
const double DT = 10;

static uint64_t NG;
static int ITER_MAX;
static double OBS_PERCENT;
static uint64_t OBS_BLOCK_SIZE;

uint64_t nlt, nl, state_min_p, state_max_p;
std::vector<int> nl_all;
const int MPI_MIN_BLK = 1;

int main() {

  assert( getenv("MELISSA_LORENZ_STATE_DIMENSION") != nullptr );
  assert( getenv("MELISSA_LORENZ_NUMBER_OF_EPOCHS") != nullptr );
  assert( getenv("MELISSA_LORENZ_OBSERVATION_PERCENT") != nullptr );
  assert( getenv("MELISSA_LORENZ_OBSERVATION_BLOCK_SIZE") != nullptr );
  assert( getenv("MELISSA_LORENZ_OBSERVATION_DIR") != nullptr );

  std::istringstream NG_str(getenv("MELISSA_LORENZ_STATE_DIMENSION"));
  std::istringstream ITER_MAX_str(getenv("MELISSA_LORENZ_NUMBER_OF_EPOCHS"));
  std::istringstream OBS_PERCENT_str(getenv("MELISSA_LORENZ_OBSERVATION_PERCENT"));
  std::istringstream OBS_BLOCK_SIZE_str(getenv("MELISSA_LORENZ_OBSERVATION_BLOCK_SIZE"));

  NG_str >> NG;
  ITER_MAX_str >> ITER_MAX;
  OBS_PERCENT_str >> OBS_PERCENT;
  OBS_BLOCK_SIZE_str >> OBS_BLOCK_SIZE;
  std::string obs_dir(getenv("MELISSA_LORENZ_OBSERVATION_DIR"));

  init_parallel();
  int zero = 0;
  int nl_i = nl;

  std::vector<double> x_l(nlt);

  std::fill(x_l.begin(), x_l.end(), F);
  if( comm_rank == 0 ) x_l[2] += 0.01;
  exchange(x_l);

  for(int iter=0; iter<=ITER_MAX; iter++)
  {


    for(int i=0; i<DT/dt; i++) {
      integrate( x_l, F, dt );
    }

    write_obs( obs_dir, x_l, 0.01*OBS_PERCENT, iter );

  }

  MPI_Finalize();

}

void init_parallel() {

    MPI_Init(NULL, NULL);

    comm = MPI_COMM_WORLD;

    MPI_Comm_size(comm, &comm_size);
    MPI_Comm_rank(comm, &comm_rank);

    nl_all.resize(comm_size);

    // middle ranks
    mpi_left    = comm_rank - 1;
    mpi_right   = comm_rank + 1;

    // first and last rank
    if (comm_rank == 0) {
      mpi_left = comm_size - 1;
    } else if (comm_rank == comm_size-1) {
      mpi_right = 0;
    }

    std::fill(nl_all.begin(), nl_all.end(), NG / comm_size);
    uint64_t comm_uint64_t = comm_size;
    uint64_t nl_mod = NG%comm_uint64_t;
    while (nl_mod > 0) {
      for(int i=0; i<comm_size; i++) {
        if (nl_mod > MPI_MIN_BLK) {
          nl_all[i] = nl_all[i] + MPI_MIN_BLK;
          nl_mod = nl_mod - MPI_MIN_BLK;
        } else {
          nl_all[i] = nl_all[i] + nl_mod;
          nl_mod = 0;
          break;
        }
      }
    }

    nl = nl_all[comm_rank];
    nlt = nl + 3;

    uint64_t nl_off = 0;
    for(int i=0; i<comm_rank; i++) {
      nl_off = nl_off + nl_all[i];
    }
    state_min_p = nl_off;
    state_max_p = nl_off + nl - 1;

}


void write_obs( const std::string file_path, std::vector<double> x,
    double share, int epoch ) {

    uint64_t offset;
    uint64_t index_tmp;

    // compute total number of observations
    uint64_t dim_obs = std::floor(share * NG);
    if (dim_obs == 0) {
      dim_obs = 1;
    }
    OUT_SYNC( "dim_obs" << dim_obs );

    // compute number of regions
    uint64_t num_reg = dim_obs / OBS_BLOCK_SIZE;
    if ( dim_obs%OBS_BLOCK_SIZE != 0 ) {
      num_reg++;
    }

    OUT_SYNC( "num_reg: " << num_reg );
    OUT_SYNC( "state_min_p: " << state_min_p );
    OUT_SYNC( "state_max_p: " << state_max_p );

    // compute stride for regions
    uint64_t stride = NG / num_reg;

    OUT_SYNC( "stride: " << stride );

    // determine number of obs in pe
    uint64_t dim_obs_p = 0;
    uint64_t cnt_obs = 0;
    for(int i=0; i<num_reg; i++) {
      offset = i * stride;
      index_tmp;
      for(int j=0; j<OBS_BLOCK_SIZE; j++) {
        index_tmp = offset + j;
        if ( (index_tmp >= state_min_p) && (index_tmp <= state_max_p) ) {
          dim_obs_p++;
        }
        cnt_obs++;
        if ( cnt_obs == dim_obs ) break;
        if ( index_tmp == state_max_p ) break;
      }
      if ( cnt_obs == dim_obs ) break;
      if ( index_tmp == state_max_p ) break;
    }

    OUT_SYNC( "rank: "<<comm_rank<<" dim_obs_p: " << dim_obs_p );

    std::vector<double> obs_p(dim_obs_p);
    std::vector<uint64_t> idx_p(dim_obs_p);

    //assign indices to index array
    uint64_t cnt_obs_p = 0;
    for(int i=0; i<num_reg; i++) {
      offset = i * stride;
      for(int j=0; j<OBS_BLOCK_SIZE; j++) {
        index_tmp = offset + j;
        if ( (index_tmp >= state_min_p) && (index_tmp <= state_max_p) ) {
          obs_p[cnt_obs_p] = x[index_tmp - state_min_p + 2];  // + 2 due to the halo
          idx_p[cnt_obs_p] = index_tmp;
          cnt_obs_p++;
          //if(comm_rank==0) OUT( "["<<comm_rank<<"] index_l: " << index_tmp - state_min_p );
          //if(comm_rank==0) OUT( "["<<comm_rank<<"] index_g: " << index_tmp );
        }
        if ( cnt_obs_p == dim_obs_p ) break;
      }
      if ( cnt_obs_p == dim_obs_p ) break;
    }


    // write observations
    uint64_t dim_obs_all[comm_size];
    MPI_Allgather( &dim_obs_p, 1, MPI_UINT64_T, dim_obs_all,
      1, MPI_UINT64_T, comm );

    uint64_t disp_obs = 0;

    for(int i=0; i<comm_rank; i++) {
      disp_obs += dim_obs_all[i] * sizeof(double);
    }

    std::string file_base = file_path + "/iter-" + std::to_string(epoch);
    //  "_timestep-" + std::to_string((epoch+1)*DT);
    std::string file_obs = file_base + ".obs";
    MPI_File fh;
    MPI_File_open(comm, file_obs.c_str(), MPI_MODE_WRONLY + MPI_MODE_CREATE, MPI_INFO_NULL, &fh);
    MPI_File_set_view(fh, disp_obs, MPI_DOUBLE, MPI_DOUBLE, "native", MPI_INFO_NULL);
    MPI_File_write(fh, &obs_p[0], dim_obs_p, MPI_DOUBLE, MPI_STATUS_IGNORE);
    MPI_File_close(&fh);
}

void RK_step( std::vector<double> & x, const std::vector<double> & k1, const std::vector<double> & k2, double w, double dt ) {
  for(int i=0; i<x.size(); i++) {
    x[i] = k1[i] + dt * k2[i]/w;
  }
}

void exchange( std::vector<double> & x ) {
  uint64_t nlt = x.size();
  if( comm_size == 1 ) {
    x[0] = x[nlt-3];
    x[1] = x[nlt-2];
    x[nlt-1] = x[2];
    return;
  }
  if ( comm_rank%2 == 0 ) {
    MPI_Send(&x[nlt-3], 2, MPI_DOUBLE, mpi_right, 42, comm);
    MPI_Recv(&x[0], 2, MPI_DOUBLE, mpi_left, 42, comm, MPI_STATUS_IGNORE);
    MPI_Send(&x[2], 1, MPI_DOUBLE, mpi_left, 42, comm);
    MPI_Recv(&x[nlt-1], 1, MPI_DOUBLE, mpi_right, 42, comm, MPI_STATUS_IGNORE);
  } else {
    MPI_Recv(&x[0], 2, MPI_DOUBLE, mpi_left, 42, comm, MPI_STATUS_IGNORE);
    MPI_Send(&x[nlt-3], 2, MPI_DOUBLE, mpi_right, 42, comm);
    MPI_Recv(&x[nlt-1], 1, MPI_DOUBLE, mpi_right, 42, comm, MPI_STATUS_IGNORE);
    MPI_Send(&x[2], 1, MPI_DOUBLE, mpi_left, 42, comm);
  }
}

void d96( std::vector<double> & x_in, std::vector<double> & x_out, double F) {
  uint64_t N = x_in.size();
  for(uint64_t i=2; i<N-1; i++) {
    x_out[i] = ( x_in[i+1] - x_in[i-2] ) * x_in[i-1] - x_in[i] + F;
  }
}

// integration using Runge-Kutta 4
void integrate( std::vector<double> & x, double F, double dt ) {
  std::vector<double> x_old(x);
  std::vector<double> ki(x);
  std::vector<double> kj(x.size());
  d96( ki, kj, F );
  RK_step( x, x, kj, 6, dt );
  RK_step( ki, x_old, kj, 2, dt );
  exchange(ki);
  d96(ki, kj, F);
  RK_step( x, x, kj, 3, dt );
  RK_step( ki, x_old, kj, 2, dt );
  exchange(ki);
  d96(ki, kj, F);
  RK_step( x, x, kj, 3, dt );
  RK_step( ki, x_old, kj, 1, dt );
  exchange(ki);
  d96(ki, kj, F);
  RK_step( x, x, kj, 6, dt );
  exchange(x);
}

