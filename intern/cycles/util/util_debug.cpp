/*
 * Copyright 2011-2016 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "util/util_debug.h"

#include <stdlib.h>

#include "util/util_logging.h"
#include "util/util_string.h"

CCL_NAMESPACE_BEGIN

DebugFlags::CPU::CPU()
  : avx2(true),
    avx(true),
    sse41(true),
    sse3(true),
    sse2(true),
    qbvh(true),
    split_kernel(false)
{
	reset();
}

void DebugFlags::CPU::reset()
{
#define STRINGIFY(x) #x
#define CHECK_CPU_FLAGS(flag, env) \
	do { \
		flag = (getenv(env) == NULL); \
		if(!flag) { \
			VLOG(1) << "Disabling " << STRINGIFY(flag) << " instruction set."; \
		} \
	} while(0)

	CHECK_CPU_FLAGS(avx2, "CYCLES_CPU_NO_AVX2");
	CHECK_CPU_FLAGS(avx, "CYCLES_CPU_NO_AVX");
	CHECK_CPU_FLAGS(sse41, "CYCLES_CPU_NO_SSE41");
	CHECK_CPU_FLAGS(sse3, "CYCLES_CPU_NO_SSE3");
	CHECK_CPU_FLAGS(sse2, "CYCLES_CPU_NO_SSE2");

#undef STRINGIFY
#undef CHECK_CPU_FLAGS

	qbvh = true;
	split_kernel = false;
}

DebugFlags::CUDA::CUDA()
  : adaptive_compile(false),
    split_kernel(false)
{
	reset();
}

void DebugFlags::CUDA::reset()
{
	if(getenv("CYCLES_CUDA_ADAPTIVE_COMPILE") != NULL)
		adaptive_compile = true;

	split_kernel = false;
}

DebugFlags::OpenCL::OpenCL()
  : device_type(DebugFlags::OpenCL::DEVICE_ALL),
    kernel_type(DebugFlags::OpenCL::KERNEL_DEFAULT),
    debug(false),
    single_program(false)
{
	reset();
}

void DebugFlags::OpenCL::reset()
{
	/* Initialize device type from environment variables. */
	device_type = DebugFlags::OpenCL::DEVICE_ALL;
	char *device = getenv("CYCLES_OPENCL_TEST");
	if(device) {
		if(strcmp(device, "NONE") == 0) {
			device_type = DebugFlags::OpenCL::DEVICE_NONE;
		}
		else if(strcmp(device, "ALL") == 0) {
			device_type = DebugFlags::OpenCL::DEVICE_ALL;
		}
		else if(strcmp(device, "DEFAULT") == 0) {
			device_type = DebugFlags::OpenCL::DEVICE_DEFAULT;
		}
		else if(strcmp(device, "CPU") == 0) {
			device_type = DebugFlags::OpenCL::DEVICE_CPU;
		}
		else if(strcmp(device, "GPU") == 0) {
			device_type = DebugFlags::OpenCL::DEVICE_GPU;
		}
		else if(strcmp(device, "ACCELERATOR") == 0) {
			device_type = DebugFlags::OpenCL::DEVICE_ACCELERATOR;
		}
	}
	/* Initialize kernel type from environment variables. */
	kernel_type = DebugFlags::OpenCL::KERNEL_DEFAULT;
	if(getenv("CYCLES_OPENCL_MEGA_KERNEL_TEST") != NULL) {
		kernel_type = DebugFlags::OpenCL::KERNEL_MEGA;
	}
	else if(getenv("CYCLES_OPENCL_SPLIT_KERNEL_TEST") != NULL) {
		kernel_type = DebugFlags::OpenCL::KERNEL_SPLIT;
	}
	/* Initialize other flags from environment variables. */
	debug = (getenv("CYCLES_OPENCL_DEBUG") != NULL);
	single_program = (getenv("CYCLES_OPENCL_MULTI_PROGRAM") == NULL);
}

DebugFlags::DebugFlags()
{
	/* Nothing for now. */
}

void DebugFlags::reset()
{
	cpu.reset();
	opencl.reset();
}

std::ostream& operator <<(std::ostream &os,
                          DebugFlagsConstRef debug_flags)
{
	os << "CPU flags:\n"
	   << "  AVX2   : " << string_from_bool(debug_flags.cpu.avx2)  << "\n"
	   << "  AVX    : " << string_from_bool(debug_flags.cpu.avx)   << "\n"
	   << "  SSE4.1 : " << string_from_bool(debug_flags.cpu.sse41) << "\n"
	   << "  SSE3   : " << string_from_bool(debug_flags.cpu.sse3)  << "\n"
	   << "  SSE2   : " << string_from_bool(debug_flags.cpu.sse2)  << "\n"
	   << "  QBVH   : " << string_from_bool(debug_flags.cpu.qbvh)  << "\n"
	   << "  Split  : " << string_from_bool(debug_flags.cpu.split_kernel) << "\n";

	os << "CUDA flags:\n"
	   << " Adaptive Compile: " << string_from_bool(debug_flags.cuda.adaptive_compile) << "\n";

	const char *opencl_device_type,
	           *opencl_kernel_type;
	switch(debug_flags.opencl.device_type) {
		case DebugFlags::OpenCL::DEVICE_NONE:
			opencl_device_type = "NONE";
			break;
		case DebugFlags::OpenCL::DEVICE_ALL:
			opencl_device_type = "ALL";
			break;
		case DebugFlags::OpenCL::DEVICE_DEFAULT:
			opencl_device_type = "DEFAULT";
			break;
		case DebugFlags::OpenCL::DEVICE_CPU:
			opencl_device_type = "CPU";
			break;
		case DebugFlags::OpenCL::DEVICE_GPU:
			opencl_device_type = "GPU";
			break;
		case DebugFlags::OpenCL::DEVICE_ACCELERATOR:
			opencl_device_type = "ACCELERATOR";
			break;
	}
	switch(debug_flags.opencl.kernel_type) {
		case DebugFlags::OpenCL::KERNEL_DEFAULT:
			opencl_kernel_type = "DEFAULT";
			break;
		case DebugFlags::OpenCL::KERNEL_MEGA:
			opencl_kernel_type = "MEGA";
			break;
		case DebugFlags::OpenCL::KERNEL_SPLIT:
			opencl_kernel_type = "SPLIT";
			break;
	}
	os << "OpenCL flags:\n"
	   << "  Device type    : " << opencl_device_type << "\n"
	   << "  Kernel type    : " << opencl_kernel_type << "\n"
	   << "  Debug          : " << string_from_bool(debug_flags.opencl.debug) << "\n"
	   << "  Single program : " << string_from_bool(debug_flags.opencl.single_program) << "\n"
	   << "  Memory limit   : " << string_human_readable_size(debug_flags.opencl.mem_limit) << "\n";
	return os;
}

CCL_NAMESPACE_END
