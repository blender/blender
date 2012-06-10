/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <stdlib.h>

#include "util_cuda.h"
#include "util_debug.h"
#include "util_dynlib.h"
#include "util_path.h"
#include "util_string.h"

/* function defininitions */

tcuInit *cuInit;
tcuDriverGetVersion *cuDriverGetVersion;
tcuDeviceGet *cuDeviceGet;
tcuDeviceGetCount *cuDeviceGetCount;
tcuDeviceGetName *cuDeviceGetName;
tcuDeviceComputeCapability *cuDeviceComputeCapability;
tcuDeviceTotalMem *cuDeviceTotalMem;
tcuDeviceGetProperties *cuDeviceGetProperties;
tcuDeviceGetAttribute *cuDeviceGetAttribute;
tcuCtxCreate *cuCtxCreate;
tcuCtxDestroy *cuCtxDestroy;
tcuCtxAttach *cuCtxAttach;
tcuCtxDetach *cuCtxDetach;
tcuCtxPushCurrent *cuCtxPushCurrent;
tcuCtxPopCurrent *cuCtxPopCurrent;
tcuCtxGetDevice *cuCtxGetDevice;
tcuCtxSynchronize *cuCtxSynchronize;
tcuModuleLoad *cuModuleLoad;
tcuModuleLoadData *cuModuleLoadData;
tcuModuleLoadDataEx *cuModuleLoadDataEx;
tcuModuleLoadFatBinary *cuModuleLoadFatBinary;
tcuModuleUnload *cuModuleUnload;
tcuModuleGetFunction *cuModuleGetFunction;
tcuModuleGetGlobal *cuModuleGetGlobal;
tcuModuleGetTexRef *cuModuleGetTexRef;
tcuModuleGetSurfRef *cuModuleGetSurfRef;
tcuMemGetInfo *cuMemGetInfo;
tcuMemAlloc *cuMemAlloc;
tcuMemAllocPitch *cuMemAllocPitch;
tcuMemFree *cuMemFree;
tcuMemGetAddressRange *cuMemGetAddressRange;
tcuMemAllocHost *cuMemAllocHost;
tcuMemFreeHost *cuMemFreeHost;
tcuMemHostAlloc *cuMemHostAlloc;
tcuMemHostGetDevicePointer *cuMemHostGetDevicePointer;
tcuMemHostGetFlags *cuMemHostGetFlags;
tcuMemcpyHtoD *cuMemcpyHtoD;
tcuMemcpyDtoH *cuMemcpyDtoH;
tcuMemcpyDtoD *cuMemcpyDtoD;
tcuMemcpyDtoA *cuMemcpyDtoA;
tcuMemcpyAtoD *cuMemcpyAtoD;
tcuMemcpyHtoA *cuMemcpyHtoA;
tcuMemcpyAtoH *cuMemcpyAtoH;
tcuMemcpyAtoA *cuMemcpyAtoA;
tcuMemcpy2D *cuMemcpy2D;
tcuMemcpy2DUnaligned *cuMemcpy2DUnaligned;
tcuMemcpy3D *cuMemcpy3D;
tcuMemcpyHtoDAsync *cuMemcpyHtoDAsync;
tcuMemcpyDtoHAsync *cuMemcpyDtoHAsync;
tcuMemcpyDtoDAsync *cuMemcpyDtoDAsync;
tcuMemcpyHtoAAsync *cuMemcpyHtoAAsync;
tcuMemcpyAtoHAsync *cuMemcpyAtoHAsync;
tcuMemcpy2DAsync *cuMemcpy2DAsync;
tcuMemcpy3DAsync *cuMemcpy3DAsync;
tcuMemsetD8 *cuMemsetD8;
tcuMemsetD16 *cuMemsetD16;
tcuMemsetD32 *cuMemsetD32;
tcuMemsetD2D8 *cuMemsetD2D8;
tcuMemsetD2D16 *cuMemsetD2D16;
tcuMemsetD2D32 *cuMemsetD2D32;
tcuFuncSetBlockShape *cuFuncSetBlockShape;
tcuFuncSetSharedSize *cuFuncSetSharedSize;
tcuFuncGetAttribute *cuFuncGetAttribute;
tcuFuncSetCacheConfig *cuFuncSetCacheConfig;
tcuArrayCreate *cuArrayCreate;
tcuArrayGetDescriptor *cuArrayGetDescriptor;
tcuArrayDestroy *cuArrayDestroy;
tcuArray3DCreate *cuArray3DCreate;
tcuArray3DGetDescriptor *cuArray3DGetDescriptor;
tcuTexRefCreate *cuTexRefCreate;
tcuTexRefDestroy *cuTexRefDestroy;
tcuTexRefSetArray *cuTexRefSetArray;
tcuTexRefSetAddress *cuTexRefSetAddress;
tcuTexRefSetAddress2D *cuTexRefSetAddress2D;
tcuTexRefSetFormat *cuTexRefSetFormat;
tcuTexRefSetAddressMode *cuTexRefSetAddressMode;
tcuTexRefSetFilterMode *cuTexRefSetFilterMode;
tcuTexRefSetFlags *cuTexRefSetFlags;
tcuTexRefGetAddress *cuTexRefGetAddress;
tcuTexRefGetArray *cuTexRefGetArray;
tcuTexRefGetAddressMode *cuTexRefGetAddressMode;
tcuTexRefGetFilterMode *cuTexRefGetFilterMode;
tcuTexRefGetFormat *cuTexRefGetFormat;
tcuTexRefGetFlags *cuTexRefGetFlags;
tcuSurfRefSetArray *cuSurfRefSetArray;
tcuSurfRefGetArray *cuSurfRefGetArray;
tcuParamSetSize *cuParamSetSize;
tcuParamSeti *cuParamSeti;
tcuParamSetf *cuParamSetf;
tcuParamSetv *cuParamSetv;
tcuParamSetTexRef *cuParamSetTexRef;
tcuLaunch *cuLaunch;
tcuLaunchGrid *cuLaunchGrid;
tcuLaunchGridAsync *cuLaunchGridAsync;
tcuEventCreate *cuEventCreate;
tcuEventRecord *cuEventRecord;
tcuEventQuery *cuEventQuery;
tcuEventSynchronize *cuEventSynchronize;
tcuEventDestroy *cuEventDestroy;
tcuEventElapsedTime *cuEventElapsedTime;
tcuStreamCreate *cuStreamCreate;
tcuStreamQuery *cuStreamQuery;
tcuStreamSynchronize *cuStreamSynchronize;
tcuStreamDestroy *cuStreamDestroy;
tcuGraphicsUnregisterResource *cuGraphicsUnregisterResource;
tcuGraphicsSubResourceGetMappedArray *cuGraphicsSubResourceGetMappedArray;
tcuGraphicsResourceGetMappedPointer *cuGraphicsResourceGetMappedPointer;
tcuGraphicsResourceSetMapFlags *cuGraphicsResourceSetMapFlags;
tcuGraphicsMapResources *cuGraphicsMapResources;
tcuGraphicsUnmapResources *cuGraphicsUnmapResources;
tcuGetExportTable *cuGetExportTable;
tcuCtxSetLimit *cuCtxSetLimit;
tcuCtxGetLimit *cuCtxGetLimit;
tcuGLCtxCreate *cuGLCtxCreate;
tcuGraphicsGLRegisterBuffer *cuGraphicsGLRegisterBuffer;
tcuGraphicsGLRegisterImage *cuGraphicsGLRegisterImage;
tcuCtxSetCurrent *cuCtxSetCurrent;

CCL_NAMESPACE_BEGIN

/* utility macros */
#define CUDA_LIBRARY_FIND_CHECKED(name) \
	name = (t##name*)dynamic_library_find(lib, #name);

#define CUDA_LIBRARY_FIND(name) \
	name = (t##name*)dynamic_library_find(lib, #name); \
	assert(name);

#define CUDA_LIBRARY_FIND_V2(name) \
	name = (t##name*)dynamic_library_find(lib, #name "_v2"); \
	assert(name);

/* initialization function */

bool cuLibraryInit()
{
	static bool initialized = false;
	static bool result = false;

	if(initialized)
		return result;
	
	initialized = true;

	/* library paths */
#ifdef _WIN32
	/* expected in c:/windows/system or similar, no path needed */
	const char *path = "nvcuda.dll";
#elif defined(__APPLE__)
	/* default installation path */
	const char *path = "/usr/local/cuda/lib/libcuda.dylib";
#else
	const char *path = "libcuda.so";
#endif

	/* load library */
	DynamicLibrary *lib = dynamic_library_open(path);

	if(lib == NULL)
		return false;

	/* detect driver version */
	int driver_version = 1000;

	CUDA_LIBRARY_FIND_CHECKED(cuDriverGetVersion);
	if(cuDriverGetVersion)
		cuDriverGetVersion(&driver_version);

	/* we require version 4.0 */
	if(driver_version < 4000)
		return false;

	/* fetch all function pointers */
	CUDA_LIBRARY_FIND(cuInit);
	CUDA_LIBRARY_FIND(cuDeviceGet);
	CUDA_LIBRARY_FIND(cuDeviceGetCount);
	CUDA_LIBRARY_FIND(cuDeviceGetName);
	CUDA_LIBRARY_FIND(cuDeviceComputeCapability);
	CUDA_LIBRARY_FIND(cuDeviceTotalMem);
	CUDA_LIBRARY_FIND(cuDeviceGetProperties);
	CUDA_LIBRARY_FIND(cuDeviceGetAttribute);
	CUDA_LIBRARY_FIND(cuCtxCreate);
	CUDA_LIBRARY_FIND(cuCtxDestroy);
	CUDA_LIBRARY_FIND(cuCtxAttach);
	CUDA_LIBRARY_FIND(cuCtxDetach);
	CUDA_LIBRARY_FIND(cuCtxPushCurrent);
	CUDA_LIBRARY_FIND(cuCtxPopCurrent);
	CUDA_LIBRARY_FIND(cuCtxGetDevice);
	CUDA_LIBRARY_FIND(cuCtxSynchronize);
	CUDA_LIBRARY_FIND(cuModuleLoad);
	CUDA_LIBRARY_FIND(cuModuleLoadData);
	CUDA_LIBRARY_FIND(cuModuleUnload);
	CUDA_LIBRARY_FIND(cuModuleGetFunction);
	CUDA_LIBRARY_FIND(cuModuleGetGlobal);
	CUDA_LIBRARY_FIND(cuModuleGetTexRef);
	CUDA_LIBRARY_FIND(cuMemGetInfo);
	CUDA_LIBRARY_FIND(cuMemAlloc);
	CUDA_LIBRARY_FIND(cuMemAllocPitch);
	CUDA_LIBRARY_FIND(cuMemFree);
	CUDA_LIBRARY_FIND(cuMemGetAddressRange);
	CUDA_LIBRARY_FIND(cuMemAllocHost);
	CUDA_LIBRARY_FIND(cuMemFreeHost);
	CUDA_LIBRARY_FIND(cuMemHostAlloc);
	CUDA_LIBRARY_FIND(cuMemHostGetDevicePointer);
	CUDA_LIBRARY_FIND(cuMemcpyHtoD);
	CUDA_LIBRARY_FIND(cuMemcpyDtoH);
	CUDA_LIBRARY_FIND(cuMemcpyDtoD);
	CUDA_LIBRARY_FIND(cuMemcpyDtoA);
	CUDA_LIBRARY_FIND(cuMemcpyAtoD);
	CUDA_LIBRARY_FIND(cuMemcpyHtoA);
	CUDA_LIBRARY_FIND(cuMemcpyAtoH);
	CUDA_LIBRARY_FIND(cuMemcpyAtoA);
	CUDA_LIBRARY_FIND(cuMemcpy2D);
	CUDA_LIBRARY_FIND(cuMemcpy2DUnaligned);
	CUDA_LIBRARY_FIND(cuMemcpy3D);
	CUDA_LIBRARY_FIND(cuMemcpyHtoDAsync);
	CUDA_LIBRARY_FIND(cuMemcpyDtoHAsync);
	CUDA_LIBRARY_FIND(cuMemcpyHtoAAsync);
	CUDA_LIBRARY_FIND(cuMemcpyAtoHAsync);
	CUDA_LIBRARY_FIND(cuMemcpy2DAsync);
	CUDA_LIBRARY_FIND(cuMemcpy3DAsync);
	CUDA_LIBRARY_FIND(cuMemsetD8);
	CUDA_LIBRARY_FIND(cuMemsetD16);
	CUDA_LIBRARY_FIND(cuMemsetD32);
	CUDA_LIBRARY_FIND(cuMemsetD2D8);
	CUDA_LIBRARY_FIND(cuMemsetD2D16);
	CUDA_LIBRARY_FIND(cuMemsetD2D32);
	CUDA_LIBRARY_FIND(cuFuncSetBlockShape);
	CUDA_LIBRARY_FIND(cuFuncSetSharedSize);
	CUDA_LIBRARY_FIND(cuFuncGetAttribute);
	CUDA_LIBRARY_FIND(cuArrayCreate);
	CUDA_LIBRARY_FIND(cuArrayGetDescriptor);
	CUDA_LIBRARY_FIND(cuArrayDestroy);
	CUDA_LIBRARY_FIND(cuArray3DCreate);
	CUDA_LIBRARY_FIND(cuArray3DGetDescriptor);
	CUDA_LIBRARY_FIND(cuTexRefCreate);
	CUDA_LIBRARY_FIND(cuTexRefDestroy);
	CUDA_LIBRARY_FIND(cuTexRefSetArray);
	CUDA_LIBRARY_FIND(cuTexRefSetAddress);
	CUDA_LIBRARY_FIND(cuTexRefSetAddress2D);
	CUDA_LIBRARY_FIND(cuTexRefSetFormat);
	CUDA_LIBRARY_FIND(cuTexRefSetAddressMode);
	CUDA_LIBRARY_FIND(cuTexRefSetFilterMode);
	CUDA_LIBRARY_FIND(cuTexRefSetFlags);
	CUDA_LIBRARY_FIND(cuTexRefGetAddress);
	CUDA_LIBRARY_FIND(cuTexRefGetArray);
	CUDA_LIBRARY_FIND(cuTexRefGetAddressMode);
	CUDA_LIBRARY_FIND(cuTexRefGetFilterMode);
	CUDA_LIBRARY_FIND(cuTexRefGetFormat);
	CUDA_LIBRARY_FIND(cuTexRefGetFlags);
	CUDA_LIBRARY_FIND(cuParamSetSize);
	CUDA_LIBRARY_FIND(cuParamSeti);
	CUDA_LIBRARY_FIND(cuParamSetf);
	CUDA_LIBRARY_FIND(cuParamSetv);
	CUDA_LIBRARY_FIND(cuParamSetTexRef);
	CUDA_LIBRARY_FIND(cuLaunch);
	CUDA_LIBRARY_FIND(cuLaunchGrid);
	CUDA_LIBRARY_FIND(cuLaunchGridAsync);
	CUDA_LIBRARY_FIND(cuEventCreate);
	CUDA_LIBRARY_FIND(cuEventRecord);
	CUDA_LIBRARY_FIND(cuEventQuery);
	CUDA_LIBRARY_FIND(cuEventSynchronize);
	CUDA_LIBRARY_FIND(cuEventDestroy);
	CUDA_LIBRARY_FIND(cuEventElapsedTime);
	CUDA_LIBRARY_FIND(cuStreamCreate);
	CUDA_LIBRARY_FIND(cuStreamQuery);
	CUDA_LIBRARY_FIND(cuStreamSynchronize);
	CUDA_LIBRARY_FIND(cuStreamDestroy);

	/* cuda 2.1 */
	CUDA_LIBRARY_FIND(cuModuleLoadDataEx);
	CUDA_LIBRARY_FIND(cuModuleLoadFatBinary);
	CUDA_LIBRARY_FIND(cuGLCtxCreate);
	CUDA_LIBRARY_FIND(cuGraphicsGLRegisterBuffer);
	CUDA_LIBRARY_FIND(cuGraphicsGLRegisterImage);

	/* cuda 2.3 */
	CUDA_LIBRARY_FIND(cuMemHostGetFlags);
	CUDA_LIBRARY_FIND(cuGraphicsGLRegisterBuffer);
	CUDA_LIBRARY_FIND(cuGraphicsGLRegisterImage);

	/* cuda 3.0 */
	CUDA_LIBRARY_FIND(cuMemcpyDtoDAsync);
	CUDA_LIBRARY_FIND(cuFuncSetCacheConfig);
	CUDA_LIBRARY_FIND(cuGraphicsUnregisterResource);
	CUDA_LIBRARY_FIND(cuGraphicsSubResourceGetMappedArray);
	CUDA_LIBRARY_FIND(cuGraphicsResourceGetMappedPointer);
	CUDA_LIBRARY_FIND(cuGraphicsResourceSetMapFlags);
	CUDA_LIBRARY_FIND(cuGraphicsMapResources);
	CUDA_LIBRARY_FIND(cuGraphicsUnmapResources);
	CUDA_LIBRARY_FIND(cuGetExportTable);

	/* cuda 3.1 */
	CUDA_LIBRARY_FIND(cuModuleGetSurfRef);
	CUDA_LIBRARY_FIND(cuSurfRefSetArray);
	CUDA_LIBRARY_FIND(cuSurfRefGetArray);
	CUDA_LIBRARY_FIND(cuCtxSetLimit);
	CUDA_LIBRARY_FIND(cuCtxGetLimit);

	/* functions which changed 3.1 -> 3.2 for 64 bit stuff, the cuda library
	 * has both the old ones for compatibility and new ones with _v2 postfix,
	 * we load the _v2 ones here. */
	CUDA_LIBRARY_FIND_V2(cuDeviceTotalMem);
	CUDA_LIBRARY_FIND_V2(cuCtxCreate);
	CUDA_LIBRARY_FIND_V2(cuModuleGetGlobal);
	CUDA_LIBRARY_FIND_V2(cuMemGetInfo);
	CUDA_LIBRARY_FIND_V2(cuMemAlloc);
	CUDA_LIBRARY_FIND_V2(cuMemAllocPitch);
	CUDA_LIBRARY_FIND_V2(cuMemFree);
	CUDA_LIBRARY_FIND_V2(cuMemGetAddressRange);
	CUDA_LIBRARY_FIND_V2(cuMemAllocHost);
	CUDA_LIBRARY_FIND_V2(cuMemHostGetDevicePointer);
	CUDA_LIBRARY_FIND_V2(cuMemcpyHtoD);
	CUDA_LIBRARY_FIND_V2(cuMemcpyDtoH);
	CUDA_LIBRARY_FIND_V2(cuMemcpyDtoD);
	CUDA_LIBRARY_FIND_V2(cuMemcpyDtoA);
	CUDA_LIBRARY_FIND_V2(cuMemcpyAtoD);
	CUDA_LIBRARY_FIND_V2(cuMemcpyHtoA);
	CUDA_LIBRARY_FIND_V2(cuMemcpyAtoH);
	CUDA_LIBRARY_FIND_V2(cuMemcpyAtoA);
	CUDA_LIBRARY_FIND_V2(cuMemcpyHtoAAsync);
	CUDA_LIBRARY_FIND_V2(cuMemcpyAtoHAsync);
	CUDA_LIBRARY_FIND_V2(cuMemcpy2D);
	CUDA_LIBRARY_FIND_V2(cuMemcpy2DUnaligned);
	CUDA_LIBRARY_FIND_V2(cuMemcpy3D);
	CUDA_LIBRARY_FIND_V2(cuMemcpyHtoDAsync);
	CUDA_LIBRARY_FIND_V2(cuMemcpyDtoHAsync);
	CUDA_LIBRARY_FIND_V2(cuMemcpyDtoDAsync);
	CUDA_LIBRARY_FIND_V2(cuMemcpy2DAsync);
	CUDA_LIBRARY_FIND_V2(cuMemcpy3DAsync);
	CUDA_LIBRARY_FIND_V2(cuMemsetD8);
	CUDA_LIBRARY_FIND_V2(cuMemsetD16);
	CUDA_LIBRARY_FIND_V2(cuMemsetD32);
	CUDA_LIBRARY_FIND_V2(cuMemsetD2D8);
	CUDA_LIBRARY_FIND_V2(cuMemsetD2D16);
	CUDA_LIBRARY_FIND_V2(cuMemsetD2D32);
	CUDA_LIBRARY_FIND_V2(cuArrayCreate);
	CUDA_LIBRARY_FIND_V2(cuArrayGetDescriptor);
	CUDA_LIBRARY_FIND_V2(cuArray3DCreate);
	CUDA_LIBRARY_FIND_V2(cuArray3DGetDescriptor);
	CUDA_LIBRARY_FIND_V2(cuTexRefSetAddress);
	CUDA_LIBRARY_FIND_V2(cuTexRefSetAddress2D);
	CUDA_LIBRARY_FIND_V2(cuTexRefGetAddress);
	CUDA_LIBRARY_FIND_V2(cuGraphicsResourceGetMappedPointer);
	CUDA_LIBRARY_FIND_V2(cuGLCtxCreate);

	/* cuda 4.0 */
	CUDA_LIBRARY_FIND(cuCtxSetCurrent);

#ifndef WITH_CUDA_BINARIES
#ifdef _WIN32
	return false; /* runtime build doesn't work at the moment */
#else
	if(cuCompilerPath() == "")
		return false;
#endif
#endif

	/* success */
	result = true;

	return result;
}

string cuCompilerPath()
{
#ifdef _WIN32
	const char *defaultpath = "C:/CUDA/bin";
	const char *executable = "nvcc.exe";
#else
	const char *defaultpath = "/usr/local/cuda/bin";
	const char *executable = "nvcc";
#endif

	const char *binpath = getenv("CUDA_BIN_PATH");

	string nvcc;

	if(binpath)
		nvcc = path_join(binpath, executable);
	else
		nvcc = path_join(defaultpath, executable);

	if(path_exists(nvcc))
		return nvcc;

#ifndef _WIN32
	if(system("which nvcc") == 0)
		return "nvcc";
#endif

	return "";
}

CCL_NAMESPACE_END

