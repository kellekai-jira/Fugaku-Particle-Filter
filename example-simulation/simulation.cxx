#include <mpi.h>
#include <cassert>
#include <vector>
#include <algorithm>

#include <unistd.h>

#include <iostream>
#include <fstream>

#include "../api/melissa_api.h"

#include <csignal>


const int GLOBAL_VECT_SIZE = 40;

using namespace std;

int main(int argc, char * args[])
{

	int comm_size;
	int comm_rank;

	MPI_Init(NULL, NULL);
	MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &comm_rank);

	int local_vect_size = GLOBAL_VECT_SIZE / comm_size;
	// do I have one more?
	if (GLOBAL_VECT_SIZE % comm_size != 0 && comm_rank < GLOBAL_VECT_SIZE % comm_size)
	{
		local_vect_size += 1;
	}

	// how many pixels are left of me?
	vector<int> offsets(comm_size);
	vector<int> counts(comm_size);
	fill(offsets.begin(), offsets.end(), 0);
	fill(counts.begin(), counts.end(), 0);
	int next_offset = 0;
	for (int rank = 0; rank < comm_size; rank++)
	{
		offsets[rank] = next_offset;

		counts[rank] = GLOBAL_VECT_SIZE / comm_size;
		// add one for all who have one more.
		if (GLOBAL_VECT_SIZE % comm_size != 0 && rank < GLOBAL_VECT_SIZE % comm_size)
		{
			counts[rank] += 1;
		}

		next_offset += counts[rank];
	}

	melissa_init("variableX",
			local_vect_size,
			MPI_COMM_WORLD);  // do some crazy shit (dummy mpi implementation?) if we compile without mpi.
	bool timestepping = true;
	vector<double> state1(local_vect_size);
	fill(state1.begin(), state1.end(), 0);
	printf("offset %d on rank %d \n", offsets[comm_rank], comm_rank);


	static bool is_first_timestep = true;
	while (timestepping)
	{
		int i = 0;
		for (auto it = state1.begin(); it != state1.end(); it++)
		{
			*it += offsets[comm_rank] + i;

			i++;
		}

		// simulate some calculation
		// If the simulations are too fast our testcase will not use all model task runners (Assimilation stopped before they could register...)
		usleep(10000);

		timestepping = melissa_expose("variableX", state1.data());

		if (timestepping && is_first_timestep)
		{
			printf("First timestep to propagate: %d\n", melissa_get_current_timestamp());
			is_first_timestep = false;
		}

		// file output of allways ensemble member 0
		// TODO: maybe move this functionality into ap?
		if (timestepping && melissa_get_current_state_id() == 1)
		{
			//raise(SIGINT);
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

				MPI_Gatherv(state1.data(), state1.size(), MPI_DOUBLE,
				                full_state.data(), counts.data(), offsets.data(),
				                MPI_DOUBLE, 0, MPI_COMM_WORLD);
				for (auto it = full_state.begin(); it != full_state.end(); it++)
				{
					myfile << *it << ",";
				}
				myfile << "\n";
				myfile.close();
			} else {
				MPI_Gatherv(state1.data(), state1.size(), MPI_DOUBLE,
				                NULL, NULL, NULL,
				                MPI_DOUBLE, 0, MPI_COMM_WORLD);
			}

		}



		// TODO: print output to see what happens!
	}
	int ret = MPI_Finalize();
	assert(ret == MPI_SUCCESS);
}


