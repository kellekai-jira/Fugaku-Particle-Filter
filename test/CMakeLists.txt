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

function(add_python_test test_name test_script)
    add_test(
        NAME ${test_name}
        COMMAND python3 -u ${test_script}
    )
    # TODO: call clean_up_test() within testcase and not during build
    set(PATH_LIST $ENV{PATH})
    list(APPEND PATH_LIST ${CMAKE_BINARY_DIR}/server)
    list(APPEND PATH_LIST ${CMAKE_BINARY_DIR}/examples/simulation1)
    list(APPEND PATH_LIST ${CMAKE_BINARY_DIR}/examples/simulation2-pdaf)
    list(APPEND PATH_LIST ${CMAKE_BINARY_DIR}/examples/simulation3-empty)
    list(APPEND PATH_LIST ${CMAKE_SOURCE_DIR}/test)
    string(REPLACE ";" ":" PATH_ "${PATH_LIST}")

    set(PYTHONPATH_LIST $ENV{PYTHONPATH})
    list(APPEND PYTHONPATH_LIST ${CMAKE_SOURCE_DIR}/test)
    list(APPEND PYTHONPATH_LIST ${CMAKE_SOURCE_DIR}/launcher)
    list(APPEND PYTHONPATH_LIST ${Melissa_PATH}/share/melissa)
    string(REPLACE ";" ":" PYTHONPATH "${PYTHONPATH_LIST}")


    set(LD_LIBRARY_PATH_LIST=$ENV{LD_LIBRARY_PATH})
    list(APPEND LD_LIBRARY_PATH_LIST ${ZeroMQ_INCLUDE_DIR}/../lib)
    list(APPEND LD_LIBRARY_PATH_LIST ${CMAKE_INSTALL_PREFIX}/lib)
    list(APPEND LD_LIBRARY_PATH_LIST ${FTI_PATH}/lib)
    string(REPLACE ";" ":" LD_LIBRARY_PATH_ "${LD_LIBRARY_PATH_LIST}")

    set_property(
        TEST ${test_name}
        APPEND PROPERTY ENVIRONMENT
            MELISSA_DA_PATH=${CMAKE_BINARY_DIR}
            MELISSA_DA_DATADIR=${CMAKE_SOURCE_DIR}/share
            MELISSA_DA_WITH_FTI=${WITH_FTI}
            MELISSA_DA_REPORT_TIMING=${REPORT_TIMING}
            MELISSA_DA_SOURCE_PATH=${CMAKE_SOURCE_DIR}
            MELISSA_DA_DATASET_PATH=${PDAF_PATH}/tutorial/inputs_online/
            MPIEXEC=${MPIEXEC}
            PYTHONPATH=${PYTHONPATH}
            PYTHONDONTWRITEBYTECODE=1
            PDAF_PATH=${PDAF_PATH}
            "LD_LIBRARY_PATH=${LD_LIBRARY_PATH_}"
            "PATH=${PATH_}"
            "MELISSA_DA_COMM4PY_PATH=${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libmelissa_comm4py.so"
    )
endfunction()


add_python_test("test-cmds"
    "${CMAKE_CURRENT_SOURCE_DIR}/test-cmds.py")
add_python_test("test-crashing-runner"
    "${CMAKE_CURRENT_SOURCE_DIR}/test-crashing-runner.py")
add_python_test("test-crashing-server1"
    "${CMAKE_CURRENT_SOURCE_DIR}/test-crashing-server1.py")
add_python_test("test-crashing-server2"
    "${CMAKE_CURRENT_SOURCE_DIR}/test-crashing-server2.py")
add_python_test("test-different-parallelism"
    "${CMAKE_CURRENT_SOURCE_DIR}/test-different-parallelism.py")
add_python_test("test-never-connecting-runner"
    "${CMAKE_CURRENT_SOURCE_DIR}/test-never-connecting-runner.py")
add_python_test("test-unresponsive-runner"
    "${CMAKE_CURRENT_SOURCE_DIR}/test-unresponsive-runner.py")
add_python_test("test-python-assimilator"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_python_assimilator.py")

# Should be the last test!
add_python_test("test-cleanup"
    "${CMAKE_CURRENT_SOURCE_DIR}/test-cleanup.py")

INSTALL(PROGRAMS melissa-da-cleanup-local.sh DESTINATION bin)
