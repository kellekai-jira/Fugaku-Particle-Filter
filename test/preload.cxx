// Copyright (c) 2020, Inria (https://www.inria.fr/)
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

//#include <api/melissa_api.h>

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "TimingEvent.h"
#include <string>

#include <fstream>

enum Identity { UNKNOWN = 0, SERVER = 1, CLIENT = 2};


namespace {
	auto g_identity = Identity::UNKNOWN;
    std::ofstream g_os;
    bool g_first_time = true;
}



void die(const char* fn) {
	std::perror(fn);
	std::exit(EXIT_FAILURE);
}



void* get_function_ptr(const char* symbol) {
	void* fn = dlsym(RTLD_NEXT, symbol);

	if(!fn) {
		auto error = dlerror();
		std::fprintf(
            stderr,
            "error: dlsym(RTLD_NEXT, %s): %s\n",
            symbol, error
        );
		std::exit(EXIT_FAILURE);
	}

    return fn;
}

template<typename Ret, typename ...Args>
Ret (*get_function(const char* symbol, Ret, Args...))(Args...) {
    using Fn = Ret(*)(Args...);
	return reinterpret_cast<Fn>(get_function_ptr(symbol));
}

#define MELISSA_TEST_GET_FN(...) get_function(__func__, __VA_ARGS__)



void init() __attribute__((constructor));
void init() {
	constexpr auto num_bytes = std::size_t{_POSIX_PATH_MAX};
	char executable_path[num_bytes+1] = { 0 };

	auto pid = std::intmax_t{getpid()};
	auto ret = readlink("/proc/self/exe", executable_path, num_bytes);

	if(ret < 0) {
		std::perror("readlink");
		std::exit(EXIT_FAILURE);
	}

	std::fprintf(stderr, "pid=%jd exe='%s'\n", pid, executable_path);

	constexpr auto path_separator = '/';
	auto last_separator = std::strrchr(executable_path, path_separator);
	auto executable = (last_separator == nullptr)
		? executable_path
		: last_separator + 1
	;


	// sanity checks
	assert(executable < executable_path + ret);
	assert(strnlen(executable, _POSIX_PATH_MAX) < static_cast<std::size_t>(ret));


	g_identity =
		(std::strcmp(executable, "melissa_server") == 0) ? Identity::SERVER :
		(std::strcmp(executable, "simulation1") == 0) ? Identity::CLIENT :
														Identity::UNKNOWN
	;




	std::fprintf(
		stderr, "executable='%s' %u\n",
		executable, static_cast<unsigned>(g_identity)
	);


}



void deinit() __attribute__((destructor));
void deinit() {
	auto pid = std::intmax_t{getpid()};

	std::fprintf(
		stderr, "pid=%jd \n", pid
	);

    if (g_identity != Identity::UNKNOWN && !g_first_time)
    {
        g_os.close();
    }
}

void Timing::trigger_event(TimingEventType type, const int parameter) {
    if (g_first_time) {
        if (g_identity != Identity::UNKNOWN)
        {
            // now connect to the domain socket
            const char * fifo_file = getenv("MELISSA_DA_TEST_FIFO");
            assert(fifo_file != nullptr);
            g_os = std::ofstream(fifo_file, std::ofstream::out | std::ofstream::app);
            std::fprintf(
                    stderr, "connected to fifo %s",
                    fifo_file
                    );
        }
        g_first_time = false;
    }

    if (g_identity == SERVER) {
        std::cerr << "trigger event " << type << parameter << std::endl;
    }

    // TODO: this will probablhy only get called on rank0 since only he does timing...
    //assert(g_identity == CLIENT);
    //g_os << static_cast<unsigned>(g_identity) << "," << type << "," << parameter << std::endl;
    //g_os.flush();


    void* fn = dlsym(RTLD_NEXT, "_ZN6Timing13trigger_eventE15TimingEventTypei");

	if(!fn) {
		auto error = dlerror();
		std::fprintf(stderr, "dlsym error: %s\n", error);
		std::exit(EXIT_FAILURE);
	}

    using Fn = void(*)(Timing *, TimingEventType , const int );
    auto trigger_event_fn = reinterpret_cast<Fn>(fn);

	trigger_event_fn(this, type, parameter);
}
