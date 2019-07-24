#include <vector>

struct n_to_m {  // TODO: rename datatype into Part
  int rank_simu;
  int local_offset_simu;

  int rank_server;
  int local_offset_server;

  int send_count;
};

std::vector<n_to_m> calculate_n_to_m(int ranks_server, int ranks_simu, int local_vect_sizes_simu[])
{
  std::vector <n_to_m> parts;
  int local_vect_sizes_server[ranks_server];
  int global_vect_size = 0;
  for (int i = 0; i < ranks_simu; ++i)
  {
    global_vect_size += local_vect_sizes_simu[i];
  }

  for (int i = 0; i < ranks_server; ++i)
  {
    // every server rank gets the same amount
    local_vect_sizes_server[i] = global_vect_size / ranks_server;

    // let n be the rest of this division
    // the first n server ranks get one more to split the rest fair up...
    int n_rest = global_vect_size - int(global_vect_size / ranks_server) * ranks_server;
    if (i < n_rest)
    {
      local_vect_sizes_server[i]++;
    }
  }

  parts.push_back({0, 0, 0, 0, 0});
  int index_in_simu = 0;
  int index_in_server = 0;
  n_to_m * last = &parts[0];

  for (int i = 0; i < global_vect_size; i++)
  {
    bool added_part = false;

    if (index_in_simu > local_vect_sizes_simu[last->rank_simu]-1)
    {
      // new part as we cross simulation domain border:
      parts.push_back(*last);
      last = &parts[parts.size()-1];

      last->rank_simu++;
      last->local_offset_simu = 0;
      index_in_simu = 0;

      last->local_offset_server = index_in_server;
      last->send_count = 0;

      added_part = true;
    }
    if (index_in_server > local_vect_sizes_server[last->rank_server]-1)
    {
      if (!added_part)
      {
        // if server and simulation domain borders are at the same time...
        parts.push_back(*last);
        last = &parts[parts.size()-1];
      }
      // new part as we cross server domain border:
      last->rank_server++;
      last->local_offset_server = 0;
      index_in_server = 0;

      last->local_offset_simu = index_in_simu;
      last->send_count = 0;
    }

    last->send_count++;

    index_in_simu++;
    index_in_server++;
  }

  return parts;
}