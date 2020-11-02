# Copyright (c) 2020, Inria (https://www.inria.fr/)
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

find_package(Melissa CONFIG REQUIRED)
find_package(MPI REQUIRED)
find_package(ZeroMQ REQUIRED)

function(add_python_test test_name test_script)
    add_test(
        NAME ${test_name}
        COMMAND python3 -u ${test_script}
    )

    set(PYTHONPATH_LIST $ENV{PYTHONPATH})
    list(APPEND PYTHONPATH_LIST ${CMAKE_BINARY_DIR}/test)
    string(REPLACE ";" ":" PYTHONPATH "${PYTHONPATH_LIST}")

    set_property(
        TEST ${test_name}
        APPEND PROPERTY ENVIRONMENT
            LD_PRELOAD=${CMAKE_BINARY_DIR}/test/libmelissa_da_test.so
            MELISSA_DA_PATH=${CMAKE_BINARY_DIR}
            MELISSA_DA_WITH_FTI=FALSE
            MELISSA_INSTALL_PREFIX=${Melissa_PATH}
            MPIEXEC=${MPIEXEC}
            PYTHONPATH=${PYTHONPATH}
            PYTHONDONTWRITEBYTECODE=1
    )
endfunction()

add_library(melissa_da_test SHARED preload.cxx)
target_compile_options(melissa_da_test PRIVATE -Wextra -Wall -pedantic)
target_include_directories(melissa_da_test PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(melissa_da_test)

target_link_libraries(melissa_da_test melissa_api -ldl)


