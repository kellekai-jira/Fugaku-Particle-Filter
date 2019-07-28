#include <mpi.h>
#include <cassert>
#include <vector>
#include <algorithm>

#include <unistd.h>

#include <iostream>
#include <fstream>

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
	fill(state1.begin(), state1.end(), 0);


	while (timestepping)
	{
		int i = 0;
		for (auto it = state1.begin(); it != state1.end(); it++)
		{
			//*it += comm_size;
			*it += comm_rank * GLOBAL_VECT_SIZE / comm_size + i;

			i++;
		}

		// simulate some calculation
		usleep(10000);

		timestepping = melissa_expose("variableX", state1.data());

		// file output of allways ensemble member 0
		// TODO: maybe move this functionality into ap?
		if (timestepping && melissa_get_current_state_id() == 0)
		{
			if (comm_rank == 0)
			{
				ofstream myfile;
				// 1 is the smallest timestamp.
				if (melissa_get_current_timestamp() == 1) {
					// remove old file...
					myfile.open ("output.txt", ios::trunc);
				}
				else
				{
					myfile.open ("output.txt", ios::app);
				}
				vector<double> full_state(GLOBAL_VECT_SIZE);

				MPI_Gather(state1.data(), state1.size(), MPI_DOUBLE,
						full_state.data(), state1.size(), MPI_DOUBLE, 0, MPI_COMM_WORLD);
				for (auto it = full_state.begin(); it != full_state.end(); it++)
				{
					myfile << *it << ",";
				}
				myfile << "\n";
				myfile.close();
			} else {
				MPI_Gather(state1.data(), state1.size(), MPI_DOUBLE,
						NULL, state1.size(), MPI_DOUBLE, 0, MPI_COMM_WORLD);
			}

		}



		// TODO: print output to see what happens!
	}
	int ret = MPI_Finalize();
	assert(ret == MPI_SUCCESS);
}


