/*
 * Adopted from OpenSubdiv with the following license:
 *
 *   Copyright 2015 Pixar
 *
 *   Licensed under the Apache License, Version 2.0 (the "Apache License")
 *   with the following modification; you may not use this file except in
 *   compliance with the Apache License and the following modification to it:
 *   Section 6. Trademarks. is deleted and replaced with:
 *
 *   6. Trademarks. This License does not grant permission to use the trade
 *      names, trademarks, service marks, or product names of the Licensor
 *      and its affiliates, except as required to comply with Section 4(c) of
 *      the License and to reproduce the content of the NOTICE file.
 *
 *   You may obtain a copy of the Apache License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the Apache License with the above modification is
 *   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *   KIND, either express or implied. See the Apache License for the specific
 *   language governing permissions and limitations under the Apache License.
 */

#ifdef OPENSUBDIV_HAS_CUDA

#ifdef _MSC_VER
#  include "iso646.h"
#endif

#include "opensubdiv_device_context_cuda.h"

#if defined(_WIN32)
#  include <windows.h>
#elif defined(__APPLE__)
#  include <OpenGL/OpenGL.h>
#else
#  include <X11/Xlib.h>
#  include <GL/glx.h>
#endif

#include <cstdio>
#include <algorithm>
#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cuda_gl_interop.h>

#define message(fmt, ...)
//#define message(fmt, ...)  fprintf(stderr, fmt, __VA_ARGS__)
#define error(fmt, ...)  fprintf(stderr, fmt, __VA_ARGS__)

static int _GetCudaDeviceForCurrentGLContext()
{
	// Find and use the CUDA device for the current GL context
	unsigned int interopDeviceCount = 0;
	int interopDevices[1];
	cudaError_t status = cudaGLGetDevices(&interopDeviceCount, interopDevices,
	                                      1,  cudaGLDeviceListCurrentFrame);
	if (status == cudaErrorNoDevice or interopDeviceCount != 1) {
		message("CUDA no interop devices found.\n");
		return 0;
	}
	int device = interopDevices[0];

#if defined(_WIN32)
	return device;

#elif defined(__APPLE__)
	return device;

#else  // X11
	Display * display = glXGetCurrentDisplay();
	int screen = DefaultScreen(display);
	if (device != screen) {
		error("The CUDA interop device (%d) does not match "
		      "the screen used by the current GL context (%d), "
		      "which may cause slow performance on systems "
		      "with multiple GPU devices.", device, screen);
	}
	message("CUDA init using device for current GL context: %d\n", device);
	return device;
#endif
}

/* From "NVIDIA GPU Computing SDK 4.2/C/common/inc/cutil_inline_runtime.h": */

/* Beginning of GPU Architecture definitions */
inline int _ConvertSMVer2Cores_local(int major, int minor)
{
	/* Defines for GPU Architecture types (using the SM version to determine
	 * the # of cores per SM
	 */
	typedef struct {
		int SM; /* 0xMm (hexidecimal notation),
		         * M = SM Major version,
		         * and m = SM minor version
		         */
		int Cores;
	} sSMtoCores;

	sSMtoCores nGpuArchCoresPerSM[] =
		{ { 0x10,  8 },  /* Tesla Generation (SM 1.0) G80 class */
		  { 0x11,  8 },  /* Tesla Generation (SM 1.1) G8x class */
		  { 0x12,  8 },  /* Tesla Generation (SM 1.2) G9x class */
		  { 0x13,  8 },  /* Tesla Generation (SM 1.3) GT200 class */
		  { 0x20, 32 },  /* Fermi Generation (SM 2.0) GF100 class */
		  { 0x21, 48 },  /* Fermi Generation (SM 2.1) GF10x class */
		  { 0x30, 192},  /* Fermi Generation (SM 3.0) GK10x class */
		  {   -1, -1 }
		};

	int index = 0;
	while (nGpuArchCoresPerSM[index].SM != -1) {
		if (nGpuArchCoresPerSM[index].SM == ((major << 4) + minor)) {
			return nGpuArchCoresPerSM[index].Cores;
		}
		index++;
	}
	printf("MapSMtoCores undefined SMversion %d.%d!\n", major, minor);
	return -1;
}
/* End of GPU Architecture definitions. */

/* This function returns the best GPU (with maximum GFLOPS) */
inline int cutGetMaxGflopsDeviceId()
{
	int current_device   = 0, sm_per_multiproc = 0;
	int max_compute_perf = 0, max_perf_device  = -1;
	int device_count     = 0, best_SM_arch     = 0;
	int compat_major, compat_minor;

	cuDeviceGetCount(&device_count);
	/* Find the best major SM Architecture GPU device. */
	while (current_device < device_count) {
		cuDeviceComputeCapability(&compat_major, &compat_minor, current_device);
		if (compat_major > 0 && compat_major < 9999) {
			best_SM_arch = std::max(best_SM_arch, compat_major);
		}
		current_device++;
	}

	/* Find the best CUDA capable GPU device. */
	current_device = 0;
	while (current_device < device_count) {
		cuDeviceComputeCapability(&compat_major, &compat_minor, current_device);
		if (compat_major == 9999 && compat_minor == 9999) {
			sm_per_multiproc = 1;
		} else {
			sm_per_multiproc = _ConvertSMVer2Cores_local(compat_major,
			                                             compat_minor);
		}
		int multi_processor_count;
		cuDeviceGetAttribute(&multi_processor_count,
		                     CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT,
		                     current_device);
		int clock_rate;
		cuDeviceGetAttribute(&clock_rate,
		                     CU_DEVICE_ATTRIBUTE_CLOCK_RATE,
		                     current_device);
		int compute_perf = multi_processor_count * sm_per_multiproc * clock_rate;
		if (compute_perf > max_compute_perf) {
			/* If we find GPU with SM major > 2, search only these */
			if (best_SM_arch > 2) {
				/* If our device==dest_SM_arch, choose this, or else pass. */
				if (compat_major == best_SM_arch) {
					max_compute_perf = compute_perf;
					max_perf_device = current_device;
				}
			} else {
				max_compute_perf = compute_perf;
				max_perf_device = current_device;
			}
		}
		++current_device;
	}
	return max_perf_device;
}

bool CudaDeviceContext::HAS_CUDA_VERSION_4_0()
{
#ifdef OPENSUBDIV_HAS_CUDA
	static bool cudaInitialized = false;
	static bool cudaLoadSuccess = true;
	if (!cudaInitialized) {
		cudaInitialized = true;

#  ifdef OPENSUBDIV_HAS_CUEW
		cudaLoadSuccess = cuewInit(CUEW_INIT_CUDA) == CUEW_SUCCESS;
		if (!cudaLoadSuccess) {
			fprintf(stderr, "Loading CUDA failed.\n");
		}
#  endif
		// Need to initialize CUDA here so getting device
		// with the maximum FPLOS works fine.
		if (cuInit(0) == CUDA_SUCCESS) {
			// This is to deal with cases like NVidia Optimus,
			// when there might be CUDA library installed but
			// NVidia card is not being active.
			if (cutGetMaxGflopsDeviceId() < 0) {
				cudaLoadSuccess = false;
			}
		}
		else {
			cudaLoadSuccess = false;
		}
	}
	return cudaLoadSuccess;
#else
	return false;
#endif
}

CudaDeviceContext::CudaDeviceContext()
    : _initialized(false) {
}

CudaDeviceContext::~CudaDeviceContext() {
	cudaDeviceReset();
}

bool CudaDeviceContext::Initialize()
{
	/* See if any cuda device is available. */
	int deviceCount = 0;
	cudaGetDeviceCount(&deviceCount);
	message("CUDA device count: %d\n", deviceCount);
	if (deviceCount <= 0) {
		return false;
	}
	cudaGLSetGLDevice(_GetCudaDeviceForCurrentGLContext());
	_initialized = true;
	return true;
}

#endif  /* OPENSUBDIV_HAS_CUDA */
