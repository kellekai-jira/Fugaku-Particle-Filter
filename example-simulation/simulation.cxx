#include <mpi.h>
#include <cassert>
#include <vector>
#include <algorithm>

#include <unistd.h>

#include "../api/melissa_api.h"

const int GLOBAL_VECT_SIZE = 40;

using namespace std;

int main(int argc, char * args[])
{

	int comm_size;
	int comm_rank;

	MPI_Init(NULL, NULL);
	MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &comm_rank);

	assert(GLOBAL_VECT_SIZE % comm_size == 0);  // need to use a good vect size!
	int local_vect_size = GLOBAL_VECT_SIZE / comm_size;

	melissa_init("variableX",
			local_vect_size,
			MPI_COMM_WORLD);  // do some crazy shit (dummy mpi implementation?) if we compile without mpi.
	bool timestepping = true;
	vector<double> state1(local_vect_size);
	fill(state1.begin(), state1.end(), comm_rank);

	while (timestepping)
	{
		for (auto it = state1.begin(); it != state1.end(); it++)
		{
			//*it += comm_size;
			*it += comm_rank;
		}

		// simulate some calculation
		usleep(100000);

		timestepping = melissa_expose("variableX", state1.data());

		// TODO: print output to see what happens!
	}
	int ret = MPI_Finalize();
	assert(ret == MPI_SUCCESS);
}


