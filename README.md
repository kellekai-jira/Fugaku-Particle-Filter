# Melissa-DA
Melissa for data assimilation - this is quite different from the vanilla Melissa
as the server requests the propagation of every single timestep.


At the moment the code is in a rather living state with many parts being under heavy
development. Many parts of dead code and unnecessary comments are still in there.

Feel free to create refactoring merge requests ;)




## Install
- install dependencies (see `Dockerfile` for a more up to date list).
  On ubuntu this can be done like this:
```
 apt install gfortran \
 git \
 openmpi-bin libopenmpi-dev openmpi-common \
 libhdf5-openmpi-dev \
 build-essential gcc g++ make cmake \
 python3 python3-numpy python3-pandas \
 libzmq5-dev pkg-config \
 libblas-dev liblapack-dev
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

- install melissa:
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

## Run an example
```
cd examples/<example-dir>
source ../../build/install/bin/melissa-da_set_env.sh
python3 script.py
```

## Test
```
cd build
ctest
```


## Install with FTI
To enable server checkpointing which is needed by some test cases install hdf5
(`apt install libhdf5-openmpi-dev`)
(checkpoints are stored in hdf5 file format) and use the following cmake line:
```
cmake .. -DPDAF_PATH=$PDAF_PATH -DINSTALL_FTI=ON -DWITH_FTI=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install -DHDF5_ROOT=/usr/lib/x86_64-linux-gnu/hdf5/openmpi \
         -DWITH_FTI_THREADS=ON
```
`HDF5_ROOT` needs to be specified as cmake does not find the parallel hdf5 version if
working with ubuntu bionic

## Credits
- for Launcher-Server-Client communictaion we depend on [libzmq](wiki.zeromq.org/)
- data assimilation can be powerd using [PDAF](http://pdaf.awi.de/trac/wiki)
- Server checkpointing depends on [FTI](https://github.com/leobago/fti)
- large scale experiments use the [repex scripts](https://gitlab.inria.fr/sfriedem/repex)


## TODO
- Handle Timing for parflow...
- better interface to zerocopy add structured data.
- void pointer to add hidden state variables, state variables important to restart a timestep but which are not assimilated.
- refactor global variables in server.cxx. Do we really need ENSEMBLE_SIZE for example?
- in code todos






## Comparison with Melissa-SA: a distant branch of Melissa-SA...
This is a rather distant branch of Melissa (https://melissa-sa.github.io/) for data assimilation:
The biggest part is written in C++ as:
- to interact with Jobs, ensemble members we depend on different containers (maps, vectors...) implemented in the C++ stl,
  reimplementing them in C would be a hassle
- melissa_send is becoming 2-way - now called melissa_expose
- it is hard to define how different fields should interact in a DA cycle. e.g. melissa first sends all fields, then the update steps are performed on then? Why not just having one big field that contains all variables? Data assimilation is only well defined on one such field (Other wise one must define what to assimilate in which order and so on....)
  - thus we only allow one at the moment
- In the vanilla melissa C code are some optimizations done that would have to be reversed to stay compatible with Melissa-DA's data flow
- Melissa-DA needs much more synchronism also on the server side

...

### But
  - launcher interface and the api base as well as many dev-ops things are common between
  the different melissa version
