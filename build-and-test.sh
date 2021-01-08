#!/bin/bash

# Copyright (c) 2020, Institut National de Recherche en Informatique et en Automatique (https://www.inria.fr/)
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# * Neither the name of the copyright holder nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


set -e
set -o pipefail
set -u

export CFLAGS='-fno-common'
export CXXFLAGS="$CFLAGS"

cwd="$(pwd -P)"
num_jobs="$(getconf _NPROCESSORS_ONLN)"
build_type='Debug'

raw_melissa_da_source_dir="${1:?path to Melissa DA source directory missing}"
raw_pdaf_path="${2:?path to PDAF 1.15 needed}"
raw_fti_path="${3:?path to FTI needed}"

canonicalize_path() {
	readlink --verbose --canonicalize-existing "${1:?}"
}

melissa_da_source_dir="$(canonicalize_path "$raw_melissa_da_source_dir")"
pdaf_path="$(canonicalize_path "$raw_pdaf_path")"

melissa_da_binary_dir="$cwd/build.melissa-da"
melissa_da_prefix_dir="$cwd/prefix.melissa-da"
melissa_sa_source_dir="$melissa_da_source_dir/melissa"
melissa_sa_binary_dir="$cwd/build.melissa-sa"
melissa_sa_prefix_dir="$cwd/prefix.melissa-sa"


# this information may be buried somewhere in the logs when using continuous
# integration so just print it here
cmake --version


echo 'building Melissa SA'
mkdir -- "$melissa_sa_binary_dir"
cd -- "$melissa_sa_binary_dir"
cmake \
	-DCMAKE_BUILD_TYPE="$build_type" \
	-DCMAKE_INSTALL_PREFIX="$melissa_sa_prefix_dir" \
	-- "$melissa_sa_source_dir"
cmake --build . -- --jobs="$num_jobs"
cmake --build . --target install


echo 'building FTI'

fti_source_dir="$(canonicalize_path "$raw_fti_path")"
fti_binary_dir="$cwd/build.fti"
fti_prefix_dir="$cwd/prefix.fti"

mkdir -- "$fti_binary_dir"
cd -- "$fti_binary_dir"

cmake \
	-DENABLE_TESTS=OFF \
	-DENABLE_EXAMPLES=OFF \
	-DENABLE_HDF5=ON -DHDF5_ROOT=/usr/lib/x86_64-linux-gnu/hdf5/openmpi/ \
	-DCMAKE_BUILD_TYPE="$build_type" \
	-DCMAKE_INSTALL_PREFIX="$fti_prefix_dir" \
	-- "$fti_source_dir"
cmake --build . -- --jobs="$num_jobs"
cmake --build . --target install


echo 'building Melissa DA'
mkdir -- "$melissa_da_binary_dir"
cd -- "$melissa_da_binary_dir"
cmake \
	-DCMAKE_BUILD_TYPE="$build_type" \
	-DCMAKE_INSTALL_PREFIX="$melissa_da_prefix_dir" \
	-DPDAF_PATH="$pdaf_path" \
	-DWITH_FTI=ON -DWITH_FTI_THREADS=ON -DINSTALL_FTI=OFF \
	-DFTI_PATH="$fti_prefix_dir" \
	-DHDF5_ROOT=/usr/lib/x86_64-linux-gnu/hdf5/openmpi \
    -DMelissa_DIR="$melissa_sa_prefix_dir/share/cmake/Melissa" \
	-- "$melissa_da_source_dir"
cmake --build . -- --jobs="$num_jobs"
cmake --build . --target install
ctest --stop-on-failure --output-on-failure --timeout 300
