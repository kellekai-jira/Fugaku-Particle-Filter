Instructions how to setup architectures to run melissa-da go into this folder.


All instructions in the cluster folders (jean-zay and juwels) are rather points to start
off. As the moduling systems on some super comuters changes rather fast, version numbers
may need to be adapted.

To compile locally on e.g.\ ubuntu please have a look into our ci setup:
https://gitlab.inria.fr/melissa/melissa-ci/-/tree/master


To install Boost (we rely on a somehow older version for now, tested with 1.64:
```
wget https://dl.bintray.com/boostorg/release/1.64.0/source/boost_1_64_0.tar.gz
tar -xvf boost_1_64_0.tar.gz
```
and then tell cmake where to find it, e.g., like this:
```
cmake ..... -DBOOST_ROOT=/p/project/prcoe03/sebastian/wrf2/boost/boost_1_64_0
```

