#include <vector>
#include <algorithm>
#include <functional>
#include <mpi.h>
#include <iostream>
#include "../api/melissa_da_api.h"

#define OUT( MSG ) std::cout << MSG << std::endl

int comm_rank, comm_size, mpi_left, mpi_right;
MPI_Comm comm;

const size_t NG = 40;
const double F = 5;
const double dt = 0.01;


size_t nlt, nl, state_min_p, state_max_p;
std::vector<int> nl_all;
const int MPI_MIN_BLK = 1;

MPI_Fint fcomm_world;
MPI_Fint fcomm;

void init_parallel() {
    
    MPI_Init(NULL, NULL);

    fcomm_world = MPI_Comm_c2f(MPI_COMM_WORLD);
    fcomm = melissa_comm_init_f(&fcomm_world);
    comm = MPI_Comm_f2c(fcomm);
    
    //comm = MPI_COMM_WORLD;

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
    state_min_p = nl_off + 1;
    state_max_p = nl_off + nl;

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

int main() {

  init_parallel();
  int zero = 0;
  int nl_i = nl;
  melissa_init_f("state1", &nl_i, &zero, &fcomm);
    
  std::vector<double> x_l(nlt);

  std::fill(x_l.begin(), x_l.end(), F);
  if( comm_rank == 0 ) x_l[2] += 0.01;
  exchange(x_l);
    
  static bool is_first_timestep = true;
  int nsteps = 1;
  do
  {
    for(int i=0; i<10/dt; i++) {
      integrate( x_l, F, dt );
    }


    nsteps = melissa_expose_f("state1", x_l.data());
    printf("calculating from timestep %d\n",
        melissa_get_current_step());

    if (nsteps > 0 && is_first_timestep)
    {
      printf("First time step to propagate: %d\n",
          melissa_get_current_step());
      is_first_timestep = false;
    }

    // TODO does not work if we remove this for reasons.... (it will schedule many many things as simulation ranks finish too independently!
    MPI_Barrier(comm);
  } while (nsteps > 0);

  size_t off = 0;
  for(int i=0; i<comm_size; i++) {
    if(comm_rank == i) {
      for(int j=0; j<nl; j++) {
        OUT( "x["<<off+j<<"]: " << x_l[2+j] );
      }
    }
    off += nl_all[i];
    MPI_Barrier(MPI_COMM_WORLD);
  }

  MPI_Finalize();

}
