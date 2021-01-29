# Melissa-DA


At the moment the code is in a rather living state with many parts being under heavy
development. Many parts of dead code and unnecessary comments are still in there.

Feel free to create refactoring merge requests ;)

## TLDR how to run a DA Example:
1. [install Melissa-DA & dependencies](#1.-Install)
2. [instrument and link your model against Melissa-DA](TODO) (or use one of the example models to
start)
3. [Configure your assimilator by writing a new assimilator or writing a new
pdaf-wrapper library to be preloaded at runtime](TODO) (or use one of the existing assimilators
for the beginning)
4. launch your simulation from within a simple python script:
```python
from melissa_da_study import *

run_melissa_da_study(
        runner_cmd='simulation1',               # which model code to use
        total_steps=3,                          # how many assimilation cycles to run
        ensemble_size=3,                        # Ensemble size
        assimilator_type=ASSIMILATOR_DUMMY,     # which assimilator to chose during DA update phase Further options must be specified using environment variables passed to the server (see additional_server_env)
        cluster=LocalCluster(),                 # on which cluster to execute, LocalClsuter will run on localhost, default: empty. it will try to select the cluster automatically
        procs_server=2,                         # server paralelism
        procs_runner=3,                         # model paralelism
        n_runners=2)                            # how many runners
```

have a look into [melissa-da/launcher/melissa-da/launcher/melissa_da_study.py](https://gitlab.inria.fr/melissa/melissa-da/-/blob/master/launcher/melissa_da_study.py) to check the arguments that `run_melissa_da_study` supports.

further examples can be found in the [examples/](examples/) directory


## 1. Install
- install dependencies. There are multiple resources to figure out which dependencies
are necessary on your system and how to set them up. Examples which modules to load on
the Juwels and the jean-zay supercomputer can be found in the
[`arch/`](https://gitlab.inria.fr/melissa/melissa-da/-/tree/master/arch) directory

On Ubuntu this can be done like this:
```sh
apt install \
    autoconf \
    build-essential \
    cmake \
    gcc \
    gfortran \
    g++ \
    bc \
    psmisc
    git \
    libhdf5-openmpi-dev \
    libopenblas-dev \
    libopenmpi-dev \
    libpython3.7-dev \
    libssl-dev \
    libzmq5-dev \
    make \
    pkg-config \
    python \
    python-numpy \
    python3.7
sudo python3.7 -m pip install numpy pandas mpi4py
```

- download PDAF V1.15 (you need to give your mail on their [website](http://pdaf.awi.de/download/index.php?id=ab341070863ac82737b9e4613c72f997) to get a download link)

```
tar -xvf PDAF-D_V1.15.tar.gz
cd PDAF-D_V1.15
export PDAF_PATH=$PWD
```

- clone Melissa-DA (To get access ask
  `sebastian [dot] friedemann [at] indria [dot] fr` for permissions):
```
cd <where you want to clone melissa-da>
git clone git@gitlab.inria.fr:melissa/melissa-da.git
```

- after cloning do not forget to init the submodules:
```
git submodule update --recursive --init
```

- install Melissa-SA (a dependency from Melissa-DA) within the submodules folder:
```
cd melissa
mkdir build
cd build
cmake ..
make install
cd ../..
```

- compile and install it (see `compile.sh` and `.gitlab-ci.yml` for more information)
```
mkdir build
cd build
cmake .. -DPDAF_PATH=$PDAF_PATH -DCMAKE_INSTALL_PREFIX=install
make install
```
- this will install it into build/install which is rather convenient for development and testing

- if you get some dependency problems as some paths are not found. Go to `build/` and fix them using `ccmake ..`

- **Congratulations!** you just installed Melissa-DA

*The following steps are optional:*

### Run one of the existing examples
```
cd examples/<example-dir>
source ../../build/install/bin/melissa-da_set_env.sh
python3 script.py
```

### Test
```
cd build
ctest
```


### Install with FTI
To enable server checkpointing which is needed by some test cases install hdf5
(`apt install libhdf5-openmpi-dev`, checkpoints are stored in hdf5 file format) and use the following cmake line:

```
cmake .. -DPDAF_PATH=$PDAF_PATH -DINSTALL_FTI=ON -DWITH_FTI=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install -DHDF5_ROOT=/usr/lib/x86_64-linux-gnu/hdf5/openmpi \
         -DWITH_FTI_THREADS=ON
```
`HDF5_ROOT` needs to be specified as cmake does not find the parallel hdf5 version if
working with Ubuntu bionic

## 2. Instrument and link a model against Melissa-DA

## Concept

To permit load balancing through online state migration (the way how Melissa-DA works), the model must be transformed into a runner. Thus the model must expose all its state (the full state theoretically necessary for a model restart, containing the assimilated part of the state vector, but not necessarily containing the part of the state vector that is the same on all members and constant over time) at the right place where Melissa-DA will intercept and change all this state to the state according to the ensemble member it wants to propagate next.

A simple model

```python
x = Model_Init()
for t < t_end:
    Integrate(x)
    Write_Output(x)
Model_Finalize(x)
```

Performs the following algorithm after instrumentation:

```python
x = Model_Init()
melissa_init(...)
while melissa_expose(x) != 0:
    Integrate(x)
    # optional: Write_Output(x)
Model_Finalize(x)
```

## Melissa-API

The Melissa-API exports functions to C/C++ and Fortran.

To instrument your model simply the following 2 functions must be inserted in your model code to transform it into a *runner* that can take work from the *melissa_server*.

```c
void melissa_init(const char *field_name,
                  const size_t local_vect_size,
                  const size_t local_hidden_vect_size,
                  const int bytes_per_element,
                  const int bytes_per_element_hidden,
                  MPI_Comm comm_
                  );

int melissa_expose(const char *field_name, VEC_T *values,
                   VEC_T *hidden_values);
```

- The `field_name` parameter defines the name of the field that is exposed through melissa. For the moment in Melissa-DA only one field is allowed. Thus `field_name` must be the same in each `melissa_init` and `melissa_expose` call for Melissa-DA.

- `local_vect_size` is the size in bytes of the assimilated state

- `local_vect_size_hidden` is the size in bytes of the hidden state (the piece of the state variable that is needed for state migration/restart not counting the assimilated state. It also does not necessarily contain the part of the state vector that is the same on all members and constant over time as mentioned in the section above.

- `bytes_per_element` How many bytes each element of the assimilated measures. This is important as the melissa_server will never split elements in the middle. Thus if the assimilated state vector consists e.g. of doubles a double will never be split into 2 to distribute to multiple server notes

- `bytes_per_element_hidden` The same for the hidden state vector, see the point above

- `comm_` The  MPI-Communicator of which each rank will build a connection to the melissa_server. In the most cases this can be set to `MPI_COMM_WORLD`

- `values`, `hidden_values` pointers to the raw data buffer saving the assimilated state and hidden state respectively. These variables are in-out, meaning that `melissa_expose` will change them inplace to avoid memory copies.

The Fortran API is quite similar. For more detail have a look into [api/melissa_api.i.f90](api/melissa_api.i.f90) or the `build_prefix/include/melissa_api.f90` which is created during building.

There are also different other API functions considering index maps (to map multiple variables in the assimilated or hidden state) or to expose multiple chunks of data stored on different places in the memory. As they are not vital for simple study runs and their API is not completely fixed yet they are only documented in the source code ([api/melissa_api.h](api/melissa_api.h)).

### Linking against the Melissa-API
If you are using CMake it is as simple as

```cmake
project(Model LANGUAGES ...)
find_package(Melissa)
add_executable(Model.exe ...)
target_include_directories(Model.exe PUBLIC ${MELISSA_INCLUDE_DIR})
target_link_libraries(Model.exe ${MELISSA_LIBRARY})
```


### Examples
Examples how to instrument models and how to link against the Melissa-DA Api (using CMake) can be found in the [examples/](examples/) directory.



## 3. Configure your assimilator by writing a new assimilator or writing a new

### The Python Assimilator

The easiest way of doing so is using the Python Assimilator interface:

```python
run_melissa_da_study(
            ...
            assimilator_type=ASSIMILATOR_PYTHON,
            ...)
```

 This exposes the whole ensemble of state variables to the user:

```python
import mpi4py
mpi4py.rc(initialize=False, finalize=False)
from mpi4py import MPI
import numpy as np

def callback(t, ensemble_list_background, ensemble_list_analysis,
        ensemble_list_hidden_inout, assimilated_index, assimilated_varid):

    assert(ENSEMBLE_SIZE == len(ensemble_list_analysis) == len(ensemble_list_background) == len(ensemble_list_hidden_inout))

    rank = MPI.COMM_WORLD.rank

    print('my rank:', rank)
    print("now doing DA update for t=%d..." % t)

    # Bytes To Double
    def btd(arr):
        # 8 bytes per double
        assert arr.shape[0] % 8 == 0
        return np.frombuffer(arr, dtype='float64', offset=0, count=arr.shape[0] // 8)

    # transform member 0's to double:
    print(btd(ensemble_list_background[0]))

    # don't do any assimilation, just write back the same state
    for b, a in zip(ensemble_list_background, ensemble_list_analysis):
        np.copyto(a, b)  # copy background into analysis state
```

It is the responsibility of the user to transform the transfered lists of numpy byte arrays into the correct format and to write back a correct analysis state for each ensemble member.

It is probable that this API will still change in the future to e.g. expose the hidden state too.

For an example please refer to [test/test_python_assimilator.py](test/test_python_assimilator.py).

### Different Methods

Alternative ways are inheriting [server/Assimilator.h](server/Assimilator.h) to define a new assimilation update step in C++.

Another approach permitting [PDAF](http://pdaf.awi.de/) based DA is to use the `LD_PRELOAD` functionality to inject a bunch of  user defined functions for analysis, postprocessing, observation loading ... (see [http://pdaf.awi.de/trac/wiki/ImplementationGuide](http://pdaf.awi.de/trac/wiki/ImplementationGuide))


## Dependencies

| Library | License |
| -- | -- |
| [ØMQ](https://zeromq.org/) (ZeroMQ) | [GNU Lesser General Public License version 3 with static linking exception](http://wiki.zeromq.org/area:licensing) |
| [Parallel Data Assimilation Framework](http://pdaf.awi.de/trac/wiki) (PDAF) | [GNU Lesser General Public License version 3](https://www.gnu.org/licenses/lgpl-3.0.en.html) |
| [Fault Tolerance Interface](https://github.com/leobago/fti) (FTI) | [3-clause BSD](https://github.com/leobago/fti/blob/master/LICENSE) |
| [repex scripts](https://gitlab.inria.fr/sfriedem/repex) | [MIT License](https://gitlab.inria.fr/sfriedem/repex/-/blob/master/LICENSE) |

Copies of the licenses can be found in the folder [`licenses`](licenses).

## More in depth documentation
For more in depth documentation we refer to [doc/implementation.md](doc/implementation.md) and to [this](https://hal.archives-ouvertes.fr/hal-03017033v2).
## TODO
- Handle Timing for parflow...
- better interface to zerocopy add structured data.
- void pointer to add hidden state variables, state variables important to restart a timestep but which are not assimilated.
- refactor global variables in server.cxx. Do we really need ENSEMBLE_SIZE for example?
- in code todos






## Comparison with Melissa-SA: a distant branch of Melissa-SA...
This is a rather distant branch of Melissa (https://melissa-sa.github.io/) for data assimilation:
The biggest part is written in C++ as:
- to interact with Jobs and ensemble members we depend on different containers (maps, vectors...) implemented in the C++ stl,
  reimplementing them in C would be a hassle
- `melissa_send` is becoming 2-way - now called `melissa_expose`
- it is hard to define how different fields should interact in a DA cycle. e.g. melissa first sends all fields, then the update steps are performed on then? Why not just having one big field that contains all variables? Data assimilation is only well defined on one such field (Other wise one must define what to assimilate in which order and so on....)
  - thus we only allow one at the moment
- In the vanilla melissa C code are some optimizations done that would have to be reversed to stay compatible with Melissa-DA's data flow
- Melissa-DA needs much more synchronism also on the server side

...

### But
  - launcher interface and the api base as well as many dev-ops things are common between
    the different melissa versions
