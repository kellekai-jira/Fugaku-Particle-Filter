# A docker file that installs all dependencies and can be used as gitlab runner for CI
# at the same time.

FROM registry.hub.docker.com/gitlab/gitlab-runner
RUN apt-get -qq update


# install dependencies:
RUN apt-get install -y \
 psmisc \
 bc \
 gfortran \
 git \
 autoconf \
 openmpi-bin libopenmpi-dev openmpi-common \
 libhdf5-openmpi-dev \
 build-essential gcc g++ make cmake \
 python3 python3-numpy python3-pandas \
 libzmq5-dev pkg-config \
 libblas-dev liblapack-dev

# Pandas seems to be a huge dependency... maybe we can get rid of it?



# Do not run testcases as root as mpirun does not like this...
ARG userid=1000
RUN export uid=$userid gid=$userid && \
    mkdir -p /home/docker && \
    echo "docker:x:${uid}:${gid}:Docker,,,:/home/docker:/bin/bash" >> /etc/passwd && \
    echo "docker:x:${uid}:" >> /etc/group

RUN echo "docker:docker" | chpasswd

RUN chown -R docker:docker /home/docker

USER docker

# Fix OMPI in docker:
ENV OMPI_MCA_btl "^vader"

VOLUME /home/docker/.gitlab-runner
WORKDIR /home/docker

# install FTI:
RUN cd && git clone https://github.com/leobago/fti.git && cd FTI && mkdir build && cd build && cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/FTI -DENABLE_TESTS=0 -DENABLE_EXAMPLES=0 -DENABLE_HDF5=1 && make install  # lets hope that it automatically finds HDF5... not sure though


WARNING: this docker file might be outdated. Especially the FTI support was not tested for a while...


WARNING: don't forget to copy PDAF-D_V1.15 in /docker/workspace !!
