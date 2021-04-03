#include <vector>
#include <algorithm>
#include <functional>
#include <mpi.h>
#include <iostream>
#include <cmath>

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
    double share, size_t blk_size, int epoch );

int comm_rank, comm_size, mpi_left, mpi_right;
MPI_Comm comm;

const size_t NG = 40;
const double F = 5;
const double dt = 0.01;
const double DT = 10;
const int iter_max = 1;
const double PERCENT = 50;

size_t nlt, nl, state_min_p, state_max_p;
std::vector<int> nl_all;
const int MPI_MIN_BLK = 1;

int main() {

  init_parallel();
  int zero = 0;
  int nl_i = nl;
    
  std::vector<double> x_l(nlt);

  std::fill(x_l.begin(), x_l.end(), F);
  if( comm_rank == 0 ) x_l[2] += 0.01;
  exchange(x_l);
    
  for(int iter=0; iter<iter_max; iter++) 
  {
    
    for(int i=0; i<DT/dt; i++) {
      integrate( x_l, F, dt );
    }

    write_obs( "obs_t", x_l, 0.01*PERCENT, 4, iter );
  
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
    size_t comm_size_t = comm_size;
    size_t nl_mod = NG%comm_size_t;
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
    
    size_t nl_off = 0;
    for(int i=0; i<comm_rank; i++) {
      nl_off = nl_off + nl_all[i];
    }
    state_min_p = nl_off;
    state_max_p = nl_off + nl - 1;

}


void write_obs( const std::string file_name, std::vector<double> x, 
    double share, size_t blk_size, int epoch ) {
    
    size_t offset;
    size_t index_tmp;
    
    // compute total number of observations
    size_t dim_obs = std::floor(share * NG);
    if (dim_obs == 0) {
      dim_obs = 1;
    }
    OUT_SYNC( "dim_obs" << dim_obs );

    // compute number of regions
    size_t num_reg = dim_obs / blk_size;
    if ( dim_obs%blk_size != 0 ) {
      num_reg++;
    }

    OUT_SYNC( "num_reg: " << num_reg );
    OUT_SYNC( "state_min_p: " << state_min_p );
    OUT_SYNC( "state_max_p: " << state_max_p );

    // compute stride for regions
    size_t stride = NG / num_reg;

    OUT_SYNC( "stride: " << stride );

    // determine number of obs in pe
    size_t dim_obs_p = 0;
    size_t cnt_obs = 0;
    for(int i=0; i<num_reg; i++) {
      offset = i * stride;
      index_tmp;
      for(int j=0; j<blk_size; j++) {
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
    
    OUT_SYNC( "dim_obs_p: " << dim_obs_p );

    std::vector<double>obs_p(dim_obs_p);
    std::vector<double>obs_full_p(nl);

    std::fill(obs_full_p.begin(), obs_full_p.end(), -999.0);

    //assign indices to index array
    size_t cnt_obs_p = 0;
    for(int i=0; i<num_reg; i++) {
      offset = i * stride;
      for(int j=0; j<blk_size; j++) {
        index_tmp = offset + j;
        if ( (index_tmp >= state_min_p) && (index_tmp <= state_max_p) ) {
          obs_p[cnt_obs_p] = x[index_tmp - state_min_p];
          obs_full_p[index_tmp - state_min_p] = obs_p[cnt_obs_p];
          cnt_obs_p++;
          if(comm_rank==0) OUT( "["<<comm_rank<<"] index_l: " << index_tmp - state_min_p ); 
          if(comm_rank==0) OUT( "["<<comm_rank<<"] index_g: " << index_tmp ); 
        }
        if ( cnt_obs_p == dim_obs_p ) break;
      }
      if ( cnt_obs_p == dim_obs_p ) break;
    }
    
    // write observations
    //call mpi_allgather( dim_obs_p, 1, MPI_INTEGER, dim_obs_all, &
    //  1, MPI_INTEGER, comm_model, ierr )

    //disp = 0

    //do i = 1, mpi_rank
    //  disp = disp + dim_obs_all(i) * sizeof( obs_p(1) )
    //end do

    //call mpi_file_open(comm_model, file_name, & 
    //  MPI_MODE_WRONLY + MPI_MODE_CREATE, & 
    //  MPI_INFO_NULL, thefile, ierr)  

    //call mpi_file_set_view(thefile, disp, MPI_DOUBLE_PRECISION, & 
    //  MPI_DOUBLE_PRECISION, 'native', & 
    //  MPI_INFO_NULL, ierr)

    //call mpi_file_write(thefile, obs_p, dim_obs_p, MPI_DOUBLE_PRECISION, & 
    //  MPI_STATUS_IGNORE, ierr)

    //call mpi_file_close(thefile, ierr)
    //
    //DEALLOCATE( obs_p )
    //DEALLOCATE( obs_full_p )
    //!close(10)
}

void RK_step( std::vector<double> & x, const std::vector<double> & k1, const std::vector<double> & k2, double w, double dt ) {
  for(int i=0; i<x.size(); i++) {
    x[i] = k1[i] + dt * k2[i]/w; 
  }
}

void exchange( std::vector<double> & x ) {
  size_t nlt = x.size();
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
  size_t N = x_in.size();
  for(size_t i=2; i<N-1; i++) { 
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

