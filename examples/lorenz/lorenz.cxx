#include <vector>
#include <algorithm>
#include <functional>
#include <iterator>
#include <random>
#include <mpi.h>
#include <iostream>
#include <cstdlib>
#include <cassert>
#include <sstream>
#include "../api/melissa_da_api.h"
#include <cmath>

#define GETENV( VAR, KEY ) do { \
  VAR = getenv( KEY ); \
  if ( VAR == NULL ) { \
    std::cerr << "environment variable '"<< VAR <<"' could not be found" << std::endl; \
    return -1; \
  } \
}while (0);

#define OUT( MSG ) std::cout << MSG << std::endl

int comm_rank, comm_size, mpi_left, mpi_right;
MPI_Comm comm;

double calculate_weight( int cycle );

//double calculate_weight() {
//   double lower_bound = 0.8;
//   double upper_bound = 1.0;
//   std::uniform_real_distribution<double> unif(lower_bound,upper_bound);
//   std::default_random_engine re;
//   double a_random_double = unif(re);
//}

const double F = 6;
const double dt = 0.01;
const double DT = 0.1;
const double PI = 3.141592653589793238463;

static int64_t NG;

int64_t nlt, nl, state_min_p, state_max_p;
std::vector<int64_t> nl_all;
const int MPI_MIN_BLK = 1;
  
std::vector<double> x_l;
std::vector<double> x_b1;
std::vector<double> x_b2;
std::vector<double> x_b3;

MPI_Fint fcomm_world;
MPI_Fint fcomm;


// for noise generation
const double mean = 0.0;
double stddev;

template<typename F>
void add_noise( std::vector<double>& data, F&& dist, std::mt19937& generator ) {
    for (auto& x : data) {
        x = x + dist(generator);
    }
}

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
    int64_t comm_int64_t = comm_size;
    int64_t nl_mod = NG%comm_int64_t;
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
    
    int64_t nl_off = 0;
    for(int i=0; i<comm_rank; i++) {
      nl_off = nl_off + nl_all[i];
    }
    state_min_p = nl_off;
    state_max_p = nl_off + nl - 1;

}

void RK_step( std::vector<double> & x, const std::vector<double> & k1, const std::vector<double> & k2, double w, double dt ) {
  for(int64_t i=0; i<x.size(); i++) {
    x[i] = k1[i] + dt * k2[i]/w;
  }
}

void exchange( std::vector<double> & x ) {
  int64_t nlt = x.size();
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
  int64_t N = x_in.size();
  for(int64_t i=2; i<N-1; i++) {
    x_out[i] = ( x_in[i+1] - x_in[i-2] ) * x_in[i-1] - x_in[i] + F;
  }
}

// integration using Runge-Kutta 4
void integrate( std::vector<double> & x, double F, double dt ) {
  x_b1 = x;
  x_b2 = x;
  d96( x_b2, x_b3, F );
  RK_step( x, x, x_b3, 6, dt );
  RK_step( x_b2, x_b1, x_b3, 2, dt );
  exchange(x_b2);
  d96(x_b2, x_b3, F);
  RK_step( x, x, x_b3, 3, dt );
  RK_step( x_b2, x_b1, x_b3, 2, dt );
  exchange(x_b2);
  d96(x_b2, x_b3, F);
  RK_step( x, x, x_b3, 3, dt );
  RK_step( x_b2, x_b1, x_b3, 1, dt );
  exchange(x_b2);
  d96(x_b2, x_b3, F);
  RK_step( x, x, x_b3, 6, dt );
  exchange(x);
}

double index_function( size_t idx ) {
    double norm = 1;
    double unit = 2*PI/NG;
    double freq = 100;
    return norm * cos( (double)(freq * unit * idx) );
}

void init_state() {
    x_l.resize(nlt);
    x_b1.resize(nlt);
    x_b2.resize(nlt);
    x_b3.resize(nlt);
    size_t idx = state_min_p;
    std::fill(x_l.begin(), x_l.end(), F);
    for(auto &x_i : x_l) {
        x_i += index_function( idx++ );
    }
    exchange( x_l );
}

int main() {

  assert( getenv("MELISSA_LORENZ_STATE_DIMENSION") != nullptr );
  std::istringstream NG_str(getenv("MELISSA_LORENZ_STATE_DIMENSION"));
  NG_str >> NG;

  init_parallel();

  stddev = sqrt(0.01/NG);

  std::mt19937 generator(std::random_device{}());
  auto dist = std::bind(std::normal_distribution<double>{mean, stddev},
                              std::mt19937(std::random_device{}()));

  int64_t zero = 0;
  int64_t nl_i = nl;
  int64_t lorenz_full_state_size = nl_i + 1;
  melissa_init_f("state1", &lorenz_full_state_size, &zero, &fcomm);
	melissa_register_weight_function(calculate_weight);	
 
  init_state();

  double wtime_T0 = MPI_Wtime();
  double wtime;
  static bool is_first_timestep = true;
  int nsteps = 1;
  do
  {
    if(comm_rank==0) printf("[DBG] --- START NEW ITERATION ---\n");
    for(int i=0; i<DT/dt; i++) {
      integrate( x_l, F, dt );
    }
    
    wtime = MPI_Wtime() - wtime_T0;
    int64_t expose_size = 1;
    int mode = static_cast<int>(MELISSA_MODE_EXPOSE);
    nsteps = melissa_expose_f("wtime", &wtime, &expose_size, &mode);
    expose_size = nl_i;
    mode = static_cast<int>(MELISSA_MODE_UPDATE);
    nsteps = melissa_expose_f("state1", &x_l[2], &expose_size, &mode);
    if(comm_rank==0) printf("[DBG] --- DONE EXPOSE [nsteps=%d] ---\n", nsteps);
    printf("calculating from timestep %d\n",
        melissa_get_current_step());
    if (nsteps > 0 && is_first_timestep)
    {
      printf("First time step to propagate: %d\n",
          melissa_get_current_step());
      is_first_timestep = false;
    }
    add_noise( x_l, dist, generator );
    exchange(x_l);

    // TODO does not work if we remove this for reasons.... (it will schedule many many things as simulation ranks finish too independently!
    MPI_Barrier(comm);
  } while (nsteps > 0);

  //int64_t off = 0;
  //for(int i=0; i<comm_size; i++) {
  //  if(comm_rank == i) {
  //    for(int j=0; j<nl; j++) {
  //      OUT( "x["<<off+j<<"]: " << x_l[2+j] );
  //    }
  //  }
  //  off += nl_all[i];
  //  MPI_Barrier(MPI_COMM_WORLD);
  //}

  MPI_Finalize();

}

double calculate_weight( int cycle )  {

  double share;
  int64_t OBS_BLOCK_SIZE, offset, index_tmp;
  std::string obs_dir;

  char* envvar;
  GETENV( envvar, "MELISSA_LORENZ_OBSERVATION_PERCENT" );
  share = atoi( envvar ) * 0.01;
  GETENV( envvar, "MELISSA_LORENZ_OBSERVATION_BLOCK_SIZE" );
  OBS_BLOCK_SIZE = atoi( envvar );
  GETENV( envvar, "MELISSA_LORENZ_OBSERVATION_DIR" );
  obs_dir = envvar;

  std::cout << "SHARE: " << share << " (cycle: '"<<cycle<<"')" << std::endl;
  std::cout << "BLOCK SIZE " << OBS_BLOCK_SIZE << " (cycle: '"<<cycle<<"')" << std::endl;
  std::cout << "OBSERVATION PATH " << obs_dir << " (cycle: '"<<cycle<<"')" << std::endl;
  fflush(stdout);

  // compute total number of observations
  int64_t dim_obs = std::floor(share * NG);
  if (dim_obs == 0) {
    dim_obs = 1;
  }

  // compute number of regions
  int64_t num_reg = dim_obs / OBS_BLOCK_SIZE;
  if ( dim_obs%OBS_BLOCK_SIZE != 0 ) {
    num_reg++;
  }
  
  std::cout << "num_reg: " << num_reg << " (cycle: '"<<cycle<<"')" << std::endl;
  fflush(stdout);

  // compute stride for regions
  int64_t stride = NG / num_reg;
  // determine number of obs in pe
  int64_t dim_obs_p = 0;
  int64_t cnt_obs = 0;
  for(int64_t i=0; i<num_reg; i++) {
    offset = i * stride;
    for(int64_t j=0; j<OBS_BLOCK_SIZE; j++) {
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
  
  std::cout << "dim_obs_p: " << dim_obs_p << " (cycle: '"<<cycle<<"')" << std::endl;
  fflush(stdout);

  std::vector<int64_t> idx_p(dim_obs_p);

  //assign indices to index array
  int64_t cnt_obs_p = 0;
  for(int64_t i=0; i<num_reg; i++) {
    offset = i * stride;
    for(int64_t j=0; j<OBS_BLOCK_SIZE; j++) {
      index_tmp = offset + j;
      if ( (index_tmp >= state_min_p) && (index_tmp <= state_max_p) ) {
        idx_p[cnt_obs_p] = index_tmp;
        cnt_obs_p++;
      }
      if ( cnt_obs_p == dim_obs_p ) break;
    }
    if ( cnt_obs_p == dim_obs_p ) break;
  }

  std::cout << "cnt_obs_p: " << cnt_obs_p << " (cycle: '"<<cycle<<"')" << std::endl;
  fflush(stdout);

  // write observations
  std::vector<int64_t> dim_obs_all(comm_size);
  
  std::cout << "cnt_obs_p: " << cnt_obs_p << " (cycle: '"<<cycle<<"')" << std::endl;
  fflush(stdout);
  
  MPI_Allgather( &dim_obs_p, 1, MPI_INT64_T, dim_obs_all.data(), 1, MPI_INT64_T, comm );

  int64_t disp_obs = 0;

  for(int64_t i=0; i<comm_rank; i++) {
    disp_obs += dim_obs_all[i] * sizeof(double);
  }


  std::vector<double> obs_p(dim_obs_p);

  std::string obs_path = obs_dir + "/iter-" + std::to_string(cycle) + ".obs";
  MPI_File fh;
  MPI_File_open(comm, obs_path.c_str(), MPI_MODE_RDONLY, MPI_INFO_NULL, &fh);
  MPI_File_set_view(fh, disp_obs, MPI_DOUBLE, MPI_DOUBLE, "native", MPI_INFO_NULL);
  MPI_File_read(fh, obs_p.data(), dim_obs_p, MPI_DOUBLE, MPI_STATUS_IGNORE);
  MPI_File_close(&fh);

  std::cout << "OBSERVATION PATH: " << obs_path << " (cycle: '"<<cycle<<"')" << std::endl;
  std::cout << "DIMENSION OBS: " << dim_obs_p << " (cycle: '"<<cycle<<"')" << std::endl;
  fflush(stdout);

  double sum_err = 0;
  for (int64_t i=0; i<dim_obs_p; i++) {
    sum_err = sum_err + (x_l[idx_p[i] - state_min_p + 2] - obs_p[i]) * (x_l[idx_p[i] - state_min_p + 2] - obs_p[i]);
  }

  std::cout << "SUM ERROR: " << sum_err << " (cycle: '"<<cycle<<"')" << std::endl;
  fflush(stdout);


  double sum_err_all;
  MPI_Allreduce( &sum_err, &sum_err_all, 1, MPI_DOUBLE, MPI_SUM, comm ); 
  double weight = exp( -1*sum_err_all );

  std::cout << "SUM ERROR ALL: " << sum_err_all << " (cycle: '"<<cycle<<"')" << std::endl;
  std::cout << "CALCULATED WEIGHT: " << weight << " (cycle: '"<<cycle<<"')" << std::endl;
  fflush(stdout);

  return weight;

}
