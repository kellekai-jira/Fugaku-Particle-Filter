# Melissa-DA


At the moment the code is in a rather living state with many parts being under heavy
development. Many parts of dead code and unnecessary comments are still in there.

Feel free to create refactoring merge requests ;)

## TLDR how to run a da example:
1. [install melissa-da & dependencies](#1.-Install)
2. [instrument and link your model against melissa-da](TODO) (or use one of the example models to
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
        cluster=LocalCluster(),                 # on which cluster to execute, LocalClsuter will run on localhost
        procs_server=2,                         # server paralelism
        procs_runner=3,                         # model paralelism
        n_runners=2)                            # how many runners
```




## 1. Install
- install dependencies. There are multiple resources to figure out which dependencies
are necessary on your system and how to set them up. Examples which modules to load on
the Juwels and the jean-zay supercomputer can be found in the
[`arch/`](https://gitlab.inria.fr/melissa/melissa-da/-/tree/master/arch) directory

On ubuntu this can be done like this:
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
  libpython3-dev-all \
  libssl-dev \
	libzmq5-dev \
  make \
	pkg-config \
  python \
  python-numpy \
	python3 \
  python3-mpi4py \
  python3-numpy \
  python3-pandas
```

- download PDAF-D V1.15
  (you need to give your mail on their
  [website](http://pdaf.awi.de/download/index.php?id=ab341070863ac82737b9e4613c72f997)
  to get a download link)

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

- install melissa-sa (a dependency from melissa-da) within the submodules folder:
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

- **Congratulations!** you just installed melissa-da

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
(`apt install libhdf5-openmpi-dev`)
(checkpoints are stored in hdf5 file format) and use the following cmake line:
```
cmake .. -DPDAF_PATH=$PDAF_PATH -DINSTALL_FTI=ON -DWITH_FTI=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install -DHDF5_ROOT=/usr/lib/x86_64-linux-gnu/hdf5/openmpi \
         -DWITH_FTI_THREADS=ON
```
`HDF5_ROOT` needs to be specified as cmake does not find the parallel hdf5 version if
working with ubuntu bionic

## 2. Instrument and link a model against Melissa-DA
for this wie simply refer to ....

## 3. Configure your assimilator by writing a new assimilator or writing a new
pdaf-wrapper library to be preloaded at runtime (or use one of the existing assimilators
for the beginning)

## 4. launch your simulation from within a simple python script:
```python
from melissa_da_study import *

run_melissa_da_study(
        runner_cmd='simulation1',               # which model code to use
        total_steps=3,                          # how many assimilation cycles to run
        ensemble_size=3,                        # Ensemble size
        assimilator_type=ASSIMILATOR_DUMMY,     # which assimilator to chose during DA update phase Further options must be specified using environment variables passed to the server (see additional_server_env)
        cluster=LocalCluster(),                 # on which cluster to execute, LocalClsuter will run on localhost
        procs_server=2,                         # server paralelism
        procs_runner=3,                         # model paralelism
        n_runners=2)                            # how many runners
```
- have a look into [melissa-da/launcher/melissa-da/launcher/melissa_da_study.py](https://gitlab.inria.fr/melissa/melissa-da/-/blob/master/launcher/melissa_da_study.py) to check the arguments that `run_melissa_da_study` supports.


## Dependencies

| Library | License |
| -- | -- |
| [ØMQ](https://zeromq.org/) (ZeroMQ) | [GNU Lesser General Public License version 3 with static linking exception](http://wiki.zeromq.org/area:licensing) |
| [Parallel Data Assimilation Framework](http://pdaf.awi.de/trac/wiki) (PDAF) | [GNU Lesser General Public License version 3](https://www.gnu.org/licenses/lgpl-3.0.en.html) |
| [Fault Tolerance Interface](https://github.com/leobago/fti) (FTI) | [3-clause BSD](https://github.com/leobago/fti/blob/master/LICENSE) |
| [repex scripts](https://gitlab.inria.fr/sfriedem/repex) | [MIT License](https://gitlab.inria.fr/sfriedem/repex/-/blob/master/LICENSE) |

Copies of the licenses can be found in the folder [`licenses`](licenses).


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
