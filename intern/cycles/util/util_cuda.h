/*
 * Copyright 2011-2013 Blender Foundation
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
 * limitations under the License
 */

#ifndef __UTIL_CUDA_H__
#define __UTIL_CUDA_H__

#include <stdlib.h>
#include "util_opengl.h"
#include "util_string.h"

CCL_NAMESPACE_BEGIN

/* CUDA is linked in dynamically at runtime, so we can start the application
 * without requiring a CUDA installation. Code adapted from the example
 * matrixMulDynlinkJIT in the CUDA SDK. */

bool cuLibraryInit();
bool cuHavePrecompiledKernels();
string cuCompilerPath();
int cuCompilerVersion();

CCL_NAMESPACE_END

/* defines, structs, enums */

#define CUDA_VERSION 3020

#if defined(__x86_64) || defined(AMD64) || defined(_M_AMD64) || defined(__LP64__)
typedef unsigned long long CUdeviceptr;
#else
typedef unsigned int CUdeviceptr;
#endif

typedef int CUdevice;
typedef struct CUctx_st *CUcontext;
typedef struct CUmod_st *CUmodule;
typedef struct CUfunc_st *CUfunction;
typedef struct CUarray_st *CUarray;
typedef struct CUtexref_st *CUtexref;
typedef struct CUsurfref_st *CUsurfref;
typedef struct CUevent_st *CUevent;
typedef struct CUstream_st *CUstream;
typedef struct CUgraphicsResource_st *CUgraphicsResource;

typedef struct CUuuid_st {
	char bytes[16];
} CUuuid;

typedef enum CUctx_flags_enum {
	CU_CTX_SCHED_AUTO  = 0,
	CU_CTX_SCHED_SPIN  = 1,
	CU_CTX_SCHED_YIELD = 2,
	CU_CTX_SCHED_MASK  = 0x3,
	CU_CTX_BLOCKING_SYNC = 4,
	CU_CTX_MAP_HOST = 8,
	CU_CTX_LMEM_RESIZE_TO_MAX = 16,
	CU_CTX_FLAGS_MASK  = 0x1f
} CUctx_flags;

typedef enum CUevent_flags_enum {
	CU_EVENT_DEFAULT        = 0,
	CU_EVENT_BLOCKING_SYNC  = 1,
	CU_EVENT_DISABLE_TIMING = 2
} CUevent_flags;

typedef enum CUarray_format_enum {
	CU_AD_FORMAT_UNSIGNED_INT8  = 0x01,
	CU_AD_FORMAT_UNSIGNED_INT16 = 0x02,
	CU_AD_FORMAT_UNSIGNED_INT32 = 0x03,
	CU_AD_FORMAT_SIGNED_INT8    = 0x08,
	CU_AD_FORMAT_SIGNED_INT16   = 0x09,
	CU_AD_FORMAT_SIGNED_INT32   = 0x0a,
	CU_AD_FORMAT_HALF           = 0x10,
	CU_AD_FORMAT_FLOAT          = 0x20
} CUarray_format;

typedef enum CUaddress_mode_enum {
	CU_TR_ADDRESS_MODE_WRAP   = 0,
	CU_TR_ADDRESS_MODE_CLAMP  = 1,
	CU_TR_ADDRESS_MODE_MIRROR = 2,
	CU_TR_ADDRESS_MODE_BORDER = 3
} CUaddress_mode;

typedef enum CUfilter_mode_enum {
	CU_TR_FILTER_MODE_POINT  = 0,
	CU_TR_FILTER_MODE_LINEAR = 1
} CUfilter_mode;

typedef enum CUdevice_attribute_enum {
	CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK = 1,
	CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_X = 2,
	CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Y = 3,
	CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Z = 4,
	CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_X = 5,
	CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Y = 6,
	CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Z = 7,
	CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK = 8,
	CU_DEVICE_ATTRIBUTE_SHARED_MEMORY_PER_BLOCK = 8,
	CU_DEVICE_ATTRIBUTE_TOTAL_CONSTANT_MEMORY = 9,
	CU_DEVICE_ATTRIBUTE_WARP_SIZE = 10,
	CU_DEVICE_ATTRIBUTE_MAX_PITCH = 11,
	CU_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_BLOCK = 12,
	CU_DEVICE_ATTRIBUTE_REGISTERS_PER_BLOCK = 12,
	CU_DEVICE_ATTRIBUTE_CLOCK_RATE = 13,
	CU_DEVICE_ATTRIBUTE_TEXTURE_ALIGNMENT = 14,
	CU_DEVICE_ATTRIBUTE_GPU_OVERLAP = 15,
	CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT = 16,
	CU_DEVICE_ATTRIBUTE_KERNEL_EXEC_TIMEOUT = 17,
	CU_DEVICE_ATTRIBUTE_INTEGRATED = 18,
	CU_DEVICE_ATTRIBUTE_CAN_MAP_HOST_MEMORY = 19,
	CU_DEVICE_ATTRIBUTE_COMPUTE_MODE = 20,
	CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_WIDTH = 21,
	CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_WIDTH = 22,
	CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_HEIGHT = 23,
	CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_WIDTH = 24,
	CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_HEIGHT = 25,
	CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_DEPTH = 26,
	CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_ARRAY_WIDTH = 27,
	CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_ARRAY_HEIGHT = 28,
	CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_ARRAY_NUMSLICES = 29,
	CU_DEVICE_ATTRIBUTE_SURFACE_ALIGNMENT = 30,
	CU_DEVICE_ATTRIBUTE_CONCURRENT_KERNELS = 31,
	CU_DEVICE_ATTRIBUTE_ECC_ENABLED = 32,
	CU_DEVICE_ATTRIBUTE_PCI_BUS_ID = 33,
	CU_DEVICE_ATTRIBUTE_PCI_DEVICE_ID = 34,
	CU_DEVICE_ATTRIBUTE_TCC_DRIVER = 35
} CUdevice_attribute;

typedef struct CUdevprop_st {
	int maxThreadsPerBlock;
	int maxThreadsDim[3];
	int maxGridSize[3];
	int sharedMemPerBlock;
	int totalConstantMemory;
	int SIMDWidth;
	int memPitch;
	int regsPerBlock;
	int clockRate;
	int textureAlign;
} CUdevprop;

typedef enum CUfunction_attribute_enum {
	CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK = 0,
	CU_FUNC_ATTRIBUTE_SHARED_SIZE_BYTES = 1,
	CU_FUNC_ATTRIBUTE_CONST_SIZE_BYTES = 2,
	CU_FUNC_ATTRIBUTE_LOCAL_SIZE_BYTES = 3,
	CU_FUNC_ATTRIBUTE_NUM_REGS = 4,
	CU_FUNC_ATTRIBUTE_PTX_VERSION = 5,
	CU_FUNC_ATTRIBUTE_BINARY_VERSION = 6,
	CU_FUNC_ATTRIBUTE_MAX
} CUfunction_attribute;

typedef enum CUfunc_cache_enum {
	CU_FUNC_CACHE_PREFER_NONE    = 0x00,
	CU_FUNC_CACHE_PREFER_SHARED  = 0x01,
	CU_FUNC_CACHE_PREFER_L1      = 0x02
} CUfunc_cache;

typedef enum CUmemorytype_enum {
	CU_MEMORYTYPE_HOST   = 0x01,
	CU_MEMORYTYPE_DEVICE = 0x02,
	CU_MEMORYTYPE_ARRAY  = 0x03
} CUmemorytype;

typedef enum CUcomputemode_enum {
	CU_COMPUTEMODE_DEFAULT    = 0,
	CU_COMPUTEMODE_EXCLUSIVE  = 1,
	CU_COMPUTEMODE_PROHIBITED = 2
} CUcomputemode;

typedef enum CUjit_option_enum
{
	CU_JIT_MAX_REGISTERS = 0,
	CU_JIT_THREADS_PER_BLOCK,
	CU_JIT_WALL_TIME,
	CU_JIT_INFO_LOG_BUFFER,
	CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES,
	CU_JIT_ERROR_LOG_BUFFER,
	CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES,
	CU_JIT_OPTIMIZATION_LEVEL,
	CU_JIT_TARGET_FROM_CUCONTEXT,
	CU_JIT_TARGET,
	CU_JIT_FALLBACK_STRATEGY

} CUjit_option;

typedef enum CUjit_target_enum
{
	CU_TARGET_COMPUTE_10 = 0,
	CU_TARGET_COMPUTE_11,
	CU_TARGET_COMPUTE_12,
	CU_TARGET_COMPUTE_13,
	CU_TARGET_COMPUTE_20,
	CU_TARGET_COMPUTE_21,
	CU_TARGET_COMPUTE_30,
	CU_TARGET_COMPUTE_35,
	CU_TARGET_COMPUTE_50
} CUjit_target;

typedef enum CUjit_fallback_enum
{
	CU_PREFER_PTX = 0,
	CU_PREFER_BINARY

} CUjit_fallback;

typedef enum CUgraphicsRegisterFlags_enum {
	CU_GRAPHICS_REGISTER_FLAGS_NONE  = 0x00
} CUgraphicsRegisterFlags;

typedef enum CUgraphicsMapResourceFlags_enum {
	CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE          = 0x00,
	CU_GRAPHICS_MAP_RESOURCE_FLAGS_READ_ONLY     = 0x01,
	CU_GRAPHICS_MAP_RESOURCE_FLAGS_WRITE_DISCARD = 0x02
} CUgraphicsMapResourceFlags;

typedef enum CUarray_cubemap_face_enum {
	CU_CUBEMAP_FACE_POSITIVE_X  = 0x00,
	CU_CUBEMAP_FACE_NEGATIVE_X  = 0x01,
	CU_CUBEMAP_FACE_POSITIVE_Y  = 0x02,
	CU_CUBEMAP_FACE_NEGATIVE_Y  = 0x03,
	CU_CUBEMAP_FACE_POSITIVE_Z  = 0x04,
	CU_CUBEMAP_FACE_NEGATIVE_Z  = 0x05
} CUarray_cubemap_face;

typedef enum CUlimit_enum {
	CU_LIMIT_STACK_SIZE        = 0x00,
	CU_LIMIT_PRINTF_FIFO_SIZE  = 0x01,
	CU_LIMIT_MALLOC_HEAP_SIZE  = 0x02
} CUlimit;

typedef enum cudaError_enum {
	CUDA_SUCCESS                              = 0,
	CUDA_ERROR_INVALID_VALUE                  = 1,
	CUDA_ERROR_OUT_OF_MEMORY                  = 2,
	CUDA_ERROR_NOT_INITIALIZED                = 3,
	CUDA_ERROR_DEINITIALIZED                  = 4,
	CUDA_ERROR_NO_DEVICE                      = 100,
	CUDA_ERROR_INVALID_DEVICE                 = 101,
	CUDA_ERROR_INVALID_IMAGE                  = 200,
	CUDA_ERROR_INVALID_CONTEXT                = 201,
	CUDA_ERROR_MAP_FAILED                     = 205,
	CUDA_ERROR_UNMAP_FAILED                   = 206,
	CUDA_ERROR_ARRAY_IS_MAPPED                = 207,
	CUDA_ERROR_ALREADY_MAPPED                 = 208,
	CUDA_ERROR_NO_BINARY_FOR_GPU              = 209,
	CUDA_ERROR_ALREADY_ACQUIRED               = 210,
	CUDA_ERROR_NOT_MAPPED                     = 211,
	CUDA_ERROR_NOT_MAPPED_AS_ARRAY            = 212,
	CUDA_ERROR_NOT_MAPPED_AS_POINTER          = 213,
	CUDA_ERROR_ECC_UNCORRECTABLE              = 214,
	CUDA_ERROR_UNSUPPORTED_LIMIT              = 215,
	CUDA_ERROR_CONTEXT_ALREADY_IN_USE         = 216,
	CUDA_ERROR_PEER_ACCESS_UNSUPPORTED        = 217,
	CUDA_ERROR_INVALID_PTX                    = 218,
	CUDA_ERROR_INVALID_SOURCE                 = 300,
	CUDA_ERROR_FILE_NOT_FOUND                 = 301,
	CUDA_ERROR_SHARED_OBJECT_SYMBOL_NOT_FOUND = 302,
	CUDA_ERROR_SHARED_OBJECT_INIT_FAILED      = 303,
	CUDA_ERROR_OPERATING_SYSTEM               = 304,
	CUDA_ERROR_INVALID_HANDLE                 = 400,
	CUDA_ERROR_NOT_FOUND                      = 500,
	CUDA_ERROR_NOT_READY                      = 600,
	CUDA_ERROR_ILLEGAL_ADDRESS                = 700,
	CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES        = 701,
	CUDA_ERROR_LAUNCH_TIMEOUT                 = 702,
	CUDA_ERROR_LAUNCH_INCOMPATIBLE_TEXTURING  = 703,
	CUDA_ERROR_HARDWARE_STACK_ERROR           = 714,
	CUDA_ERROR_ILLEGAL_INSTRUCTION            = 715,
	CUDA_ERROR_MISALIGNED_ADDRESS             = 716,
	CUDA_ERROR_INVALID_ADDRESS_SPACE          = 717,
	CUDA_ERROR_INVALID_PC                     = 718,
	CUDA_ERROR_LAUNCH_FAILED                  = 719,
	CUDA_ERROR_NOT_PERMITTED                  = 800,
	CUDA_ERROR_NOT_SUPPORTED                  = 801,
	CUDA_ERROR_UNKNOWN                        = 999
} CUresult;

#define CU_MEMHOSTALLOC_PORTABLE        0x01
#define CU_MEMHOSTALLOC_DEVICEMAP       0x02
#define CU_MEMHOSTALLOC_WRITECOMBINED   0x04

typedef struct CUDA_MEMCPY2D_st {
	size_t srcXInBytes;
	size_t srcY;

	CUmemorytype srcMemoryType;
	const void *srcHost;
	CUdeviceptr srcDevice;
	CUarray srcArray;
	size_t srcPitch;

	size_t dstXInBytes;
	size_t dstY;

	CUmemorytype dstMemoryType;
	void *dstHost;
	CUdeviceptr dstDevice;
	CUarray dstArray;
	size_t dstPitch;

	size_t WidthInBytes;
	size_t Height;
} CUDA_MEMCPY2D;

typedef struct CUDA_MEMCPY3D_st {
	size_t srcXInBytes;
	size_t srcY;
	size_t srcZ;
	size_t srcLOD;
	CUmemorytype srcMemoryType;
	const void *srcHost;
	CUdeviceptr srcDevice;
	CUarray srcArray;
	void *reserved0;
	size_t srcPitch;
	size_t srcHeight;

	size_t dstXInBytes;
	size_t dstY;
	size_t dstZ;
	size_t dstLOD;
	CUmemorytype dstMemoryType;
	void *dstHost;
	CUdeviceptr dstDevice;
	CUarray dstArray;
	void *reserved1;
	size_t dstPitch;
	size_t dstHeight;

	size_t WidthInBytes;
	size_t Height;
	size_t Depth;
} CUDA_MEMCPY3D;

typedef struct CUDA_ARRAY_DESCRIPTOR_st
{
	size_t Width;
	size_t Height;

	CUarray_format Format;
	unsigned int NumChannels;
} CUDA_ARRAY_DESCRIPTOR;

typedef struct CUDA_ARRAY3D_DESCRIPTOR_st
{
	size_t Width;
	size_t Height;
	size_t Depth;

	CUarray_format Format;
	unsigned int NumChannels;
	unsigned int Flags;
} CUDA_ARRAY3D_DESCRIPTOR;

#define CUDA_ARRAY3D_2DARRAY        0x01
#define CUDA_ARRAY3D_SURFACE_LDST   0x02
#define CU_TRSA_OVERRIDE_FORMAT 0x01
#define CU_TRSF_READ_AS_INTEGER         0x01
#define CU_TRSF_NORMALIZED_COORDINATES  0x02
#define CU_TRSF_SRGB  0x10
#define CU_PARAM_TR_DEFAULT -1

#ifdef _WIN32
#define CUDAAPI __stdcall
#else
#define CUDAAPI
#endif

/* function types */

typedef CUresult CUDAAPI tcuInit(unsigned int Flags);
typedef CUresult CUDAAPI tcuDriverGetVersion(int *driverVersion);
typedef CUresult CUDAAPI tcuDeviceGet(CUdevice *device, int ordinal);
typedef CUresult CUDAAPI tcuDeviceGetCount(int *count);
typedef CUresult CUDAAPI tcuDeviceGetName(char *name, int len, CUdevice dev);
typedef CUresult CUDAAPI tcuDeviceComputeCapability(int *major, int *minor, CUdevice dev);
typedef CUresult CUDAAPI tcuDeviceTotalMem(size_t *bytes, CUdevice dev);
typedef CUresult CUDAAPI tcuDeviceGetProperties(CUdevprop *prop, CUdevice dev);
typedef CUresult CUDAAPI tcuDeviceGetAttribute(int *pi, CUdevice_attribute attrib, CUdevice dev);
typedef CUresult CUDAAPI tcuCtxCreate(CUcontext *pctx, unsigned int flags, CUdevice dev);
typedef CUresult CUDAAPI tcuCtxDestroy(CUcontext ctx);
typedef CUresult CUDAAPI tcuCtxAttach(CUcontext *pctx, unsigned int flags);
typedef CUresult CUDAAPI tcuCtxDetach(CUcontext ctx);
typedef CUresult CUDAAPI tcuCtxPushCurrent(CUcontext ctx );
typedef CUresult CUDAAPI tcuCtxPopCurrent(CUcontext *pctx);
typedef CUresult CUDAAPI tcuCtxGetDevice(CUdevice *device);
typedef CUresult CUDAAPI tcuCtxSynchronize(void);
typedef CUresult CUDAAPI tcuCtxSetLimit(CUlimit limit, size_t value);
typedef CUresult CUDAAPI tcuCtxGetLimit(size_t *pvalue, CUlimit limit);
typedef CUresult CUDAAPI tcuCtxGetCacheConfig(CUfunc_cache *pconfig);
typedef CUresult CUDAAPI tcuCtxSetCacheConfig(CUfunc_cache config);
typedef CUresult CUDAAPI tcuCtxGetApiVersion(CUcontext ctx, unsigned int *version);
typedef CUresult CUDAAPI tcuModuleLoad(CUmodule *module, const char *fname);
typedef CUresult CUDAAPI tcuModuleLoadData(CUmodule *module, const void *image);
typedef CUresult CUDAAPI tcuModuleLoadDataEx(CUmodule *module, const void *image, unsigned int numOptions, CUjit_option *options, void **optionValues);
typedef CUresult CUDAAPI tcuModuleLoadFatBinary(CUmodule *module, const void *fatCubin);
typedef CUresult CUDAAPI tcuModuleUnload(CUmodule hmod);
typedef CUresult CUDAAPI tcuModuleGetFunction(CUfunction *hfunc, CUmodule hmod, const char *name);
typedef CUresult CUDAAPI tcuModuleGetGlobal(CUdeviceptr *dptr, size_t *bytes, CUmodule hmod, const char *name);
typedef CUresult CUDAAPI tcuModuleGetTexRef(CUtexref *pTexRef, CUmodule hmod, const char *name);
typedef CUresult CUDAAPI tcuModuleGetSurfRef(CUsurfref *pSurfRef, CUmodule hmod, const char *name);
typedef CUresult CUDAAPI tcuMemGetInfo(size_t *free, size_t *total);
typedef CUresult CUDAAPI tcuMemAlloc(CUdeviceptr *dptr, size_t bytesize);
typedef CUresult CUDAAPI tcuMemAllocPitch(CUdeviceptr *dptr, size_t *pPitch, size_t WidthInBytes, size_t Height, unsigned int ElementSizeBytes);
typedef CUresult CUDAAPI tcuMemFree(CUdeviceptr dptr);
typedef CUresult CUDAAPI tcuMemGetAddressRange(CUdeviceptr *pbase, size_t *psize, CUdeviceptr dptr);
typedef CUresult CUDAAPI tcuMemAllocHost(void **pp, size_t bytesize);
typedef CUresult CUDAAPI tcuMemFreeHost(void *p);
typedef CUresult CUDAAPI tcuMemHostAlloc(void **pp, size_t bytesize, unsigned int Flags);
typedef CUresult CUDAAPI tcuMemHostGetDevicePointer(CUdeviceptr *pdptr, void *p, unsigned int Flags);
typedef CUresult CUDAAPI tcuMemHostGetFlags(unsigned int *pFlags, void *p);
typedef CUresult CUDAAPI tcuMemcpyHtoD(CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount);
typedef CUresult CUDAAPI tcuMemcpyDtoH(void *dstHost, CUdeviceptr srcDevice, size_t ByteCount);
typedef CUresult CUDAAPI tcuMemcpyDtoD(CUdeviceptr dstDevice, CUdeviceptr srcDevice, size_t ByteCount);
typedef CUresult CUDAAPI tcuMemcpyDtoA(CUarray dstArray, size_t dstOffset, CUdeviceptr srcDevice, size_t ByteCount);
typedef CUresult CUDAAPI tcuMemcpyAtoD(CUdeviceptr dstDevice, CUarray srcArray, size_t srcOffset, size_t ByteCount);
typedef CUresult CUDAAPI tcuMemcpyHtoA(CUarray dstArray, size_t dstOffset, const void *srcHost, size_t ByteCount);
typedef CUresult CUDAAPI tcuMemcpyAtoH(void *dstHost, CUarray srcArray, size_t srcOffset, size_t ByteCount);
typedef CUresult CUDAAPI tcuMemcpyAtoA(CUarray dstArray, size_t dstOffset, CUarray srcArray, size_t srcOffset, size_t ByteCount);
typedef CUresult CUDAAPI tcuMemcpy2D(const CUDA_MEMCPY2D *pCopy);
typedef CUresult CUDAAPI tcuMemcpy2DUnaligned(const CUDA_MEMCPY2D *pCopy);
typedef CUresult CUDAAPI tcuMemcpy3D(const CUDA_MEMCPY3D *pCopy);
typedef CUresult CUDAAPI tcuMemcpyHtoDAsync(CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount, CUstream hStream);
typedef CUresult CUDAAPI tcuMemcpyDtoHAsync(void *dstHost, CUdeviceptr srcDevice, size_t ByteCount, CUstream hStream);
typedef CUresult CUDAAPI tcuMemcpyDtoDAsync(CUdeviceptr dstDevice, CUdeviceptr srcDevice, size_t ByteCount, CUstream hStream);
typedef CUresult CUDAAPI tcuMemcpyHtoAAsync(CUarray dstArray, size_t dstOffset, const void *srcHost, size_t ByteCount, CUstream hStream);
typedef CUresult CUDAAPI tcuMemcpyAtoHAsync(void *dstHost, CUarray srcArray, size_t srcOffset, size_t ByteCount, CUstream hStream);
typedef CUresult CUDAAPI tcuMemcpy2DAsync(const CUDA_MEMCPY2D *pCopy, CUstream hStream);
typedef CUresult CUDAAPI tcuMemcpy3DAsync(const CUDA_MEMCPY3D *pCopy, CUstream hStream);
typedef CUresult CUDAAPI tcuMemsetD8(CUdeviceptr dstDevice, unsigned char uc, size_t N);
typedef CUresult CUDAAPI tcuMemsetD16(CUdeviceptr dstDevice, unsigned short us, size_t N);
typedef CUresult CUDAAPI tcuMemsetD32(CUdeviceptr dstDevice, unsigned int ui, size_t N);
typedef CUresult CUDAAPI tcuMemsetD2D8(CUdeviceptr dstDevice, size_t dstPitch, unsigned char uc, size_t Width, size_t Height);
typedef CUresult CUDAAPI tcuMemsetD2D16(CUdeviceptr dstDevice, size_t dstPitch, unsigned short us, size_t Width, size_t Height);
typedef CUresult CUDAAPI tcuMemsetD2D32(CUdeviceptr dstDevice, size_t dstPitch, unsigned int ui, size_t Width, size_t Height);
typedef CUresult CUDAAPI tcuMemsetD8Async(CUdeviceptr dstDevice, unsigned char uc, size_t N, CUstream hStream);
typedef CUresult CUDAAPI tcuMemsetD16Async(CUdeviceptr dstDevice, unsigned short us, size_t N, CUstream hStream);
typedef CUresult CUDAAPI tcuMemsetD32Async(CUdeviceptr dstDevice, unsigned int ui, size_t N, CUstream hStream);
typedef CUresult CUDAAPI tcuMemsetD2D8Async(CUdeviceptr dstDevice, size_t dstPitch, unsigned char uc, size_t Width, size_t Height, CUstream hStream);
typedef CUresult CUDAAPI tcuMemsetD2D16Async(CUdeviceptr dstDevice, size_t dstPitch, unsigned short us, size_t Width, size_t Height, CUstream hStream);
typedef CUresult CUDAAPI tcuMemsetD2D32Async(CUdeviceptr dstDevice, size_t dstPitch, unsigned int ui, size_t Width, size_t Height, CUstream hStream);
typedef CUresult CUDAAPI tcuArrayCreate(CUarray *pHandle, const CUDA_ARRAY_DESCRIPTOR *pAllocateArray);
typedef CUresult CUDAAPI tcuArrayGetDescriptor(CUDA_ARRAY_DESCRIPTOR *pArrayDescriptor, CUarray hArray);
typedef CUresult CUDAAPI tcuArrayDestroy(CUarray hArray);
typedef CUresult CUDAAPI tcuArray3DCreate(CUarray *pHandle, const CUDA_ARRAY3D_DESCRIPTOR *pAllocateArray);
typedef CUresult CUDAAPI tcuArray3DGetDescriptor(CUDA_ARRAY3D_DESCRIPTOR *pArrayDescriptor, CUarray hArray);
typedef CUresult CUDAAPI tcuStreamCreate(CUstream *phStream, unsigned int Flags);
typedef CUresult CUDAAPI tcuStreamWaitEvent(CUstream hStream, CUevent hEvent, unsigned int Flags);
typedef CUresult CUDAAPI tcuStreamQuery(CUstream hStream);
typedef CUresult CUDAAPI tcuStreamSynchronize(CUstream hStream);
typedef CUresult CUDAAPI tcuStreamDestroy(CUstream hStream);
typedef CUresult CUDAAPI tcuEventCreate(CUevent *phEvent, unsigned int Flags);
typedef CUresult CUDAAPI tcuEventRecord(CUevent hEvent, CUstream hStream);
typedef CUresult CUDAAPI tcuEventQuery(CUevent hEvent);
typedef CUresult CUDAAPI tcuEventSynchronize(CUevent hEvent);
typedef CUresult CUDAAPI tcuEventDestroy(CUevent hEvent);
typedef CUresult CUDAAPI tcuEventElapsedTime(float *pMilliseconds, CUevent hStart, CUevent hEnd);
typedef CUresult CUDAAPI tcuFuncSetBlockShape(CUfunction hfunc, int x, int y, int z);
typedef CUresult CUDAAPI tcuFuncSetSharedSize(CUfunction hfunc, unsigned int bytes);
typedef CUresult CUDAAPI tcuFuncGetAttribute(int *pi, CUfunction_attribute attrib, CUfunction hfunc);
typedef CUresult CUDAAPI tcuFuncSetCacheConfig(CUfunction hfunc, CUfunc_cache config);
typedef CUresult CUDAAPI tcuParamSetSize(CUfunction hfunc, unsigned int numbytes);
typedef CUresult CUDAAPI tcuParamSeti(CUfunction hfunc, int offset, unsigned int value);
typedef CUresult CUDAAPI tcuParamSetf(CUfunction hfunc, int offset, float value);
typedef CUresult CUDAAPI tcuParamSetv(CUfunction hfunc, int offset, void *ptr, unsigned int numbytes);
typedef CUresult CUDAAPI tcuLaunch(CUfunction f);
typedef CUresult CUDAAPI tcuLaunchGrid(CUfunction f, int grid_width, int grid_height);
typedef CUresult CUDAAPI tcuLaunchGridAsync(CUfunction f, int grid_width, int grid_height, CUstream hStream);
typedef CUresult CUDAAPI tcuParamSetTexRef(CUfunction hfunc, int texunit, CUtexref hTexRef);
typedef CUresult CUDAAPI tcuTexRefSetArray(CUtexref hTexRef, CUarray hArray, unsigned int Flags);
typedef CUresult CUDAAPI tcuTexRefSetAddress(size_t *ByteOffset, CUtexref hTexRef, CUdeviceptr dptr, size_t bytes);
typedef CUresult CUDAAPI tcuTexRefSetAddress2D(CUtexref hTexRef, const CUDA_ARRAY_DESCRIPTOR *desc, CUdeviceptr dptr, size_t Pitch);
typedef CUresult CUDAAPI tcuTexRefSetFormat(CUtexref hTexRef, CUarray_format fmt, int NumPackedComponents);
typedef CUresult CUDAAPI tcuTexRefSetAddressMode(CUtexref hTexRef, int dim, CUaddress_mode am);
typedef CUresult CUDAAPI tcuTexRefSetFilterMode(CUtexref hTexRef, CUfilter_mode fm);
typedef CUresult CUDAAPI tcuTexRefSetFlags(CUtexref hTexRef, unsigned int Flags);
typedef CUresult CUDAAPI tcuTexRefGetAddress(CUdeviceptr *pdptr, CUtexref hTexRef);
typedef CUresult CUDAAPI tcuTexRefGetArray(CUarray *phArray, CUtexref hTexRef);
typedef CUresult CUDAAPI tcuTexRefGetAddressMode(CUaddress_mode *pam, CUtexref hTexRef, int dim);
typedef CUresult CUDAAPI tcuTexRefGetFilterMode(CUfilter_mode *pfm, CUtexref hTexRef);
typedef CUresult CUDAAPI tcuTexRefGetFormat(CUarray_format *pFormat, int *pNumChannels, CUtexref hTexRef);
typedef CUresult CUDAAPI tcuTexRefGetFlags(unsigned int *pFlags, CUtexref hTexRef);
typedef CUresult CUDAAPI tcuTexRefCreate(CUtexref *pTexRef);
typedef CUresult CUDAAPI tcuTexRefDestroy(CUtexref hTexRef);
typedef CUresult CUDAAPI tcuSurfRefSetArray(CUsurfref hSurfRef, CUarray hArray, unsigned int Flags);
typedef CUresult CUDAAPI tcuSurfRefGetArray(CUarray *phArray, CUsurfref hSurfRef);
typedef CUresult CUDAAPI tcuGraphicsUnregisterResource(CUgraphicsResource resource);
typedef CUresult CUDAAPI tcuGraphicsSubResourceGetMappedArray(CUarray *pArray, CUgraphicsResource resource, unsigned int arrayIndex, unsigned int mipLevel);
typedef CUresult CUDAAPI tcuGraphicsResourceGetMappedPointer(CUdeviceptr *pDevPtr, size_t *pSize, CUgraphicsResource resource);
typedef CUresult CUDAAPI tcuGraphicsResourceSetMapFlags(CUgraphicsResource resource, unsigned int flags);
typedef CUresult CUDAAPI tcuGraphicsMapResources(unsigned int count, CUgraphicsResource *resources, CUstream hStream);
typedef CUresult CUDAAPI tcuGraphicsUnmapResources(unsigned int count, CUgraphicsResource *resources, CUstream hStream);
typedef CUresult CUDAAPI tcuGetExportTable(const void **ppExportTable, const CUuuid *pExportTableId);
typedef CUresult CUDAAPI tcuGLCtxCreate(CUcontext *pCtx, unsigned int Flags, CUdevice device );
typedef CUresult CUDAAPI tcuGraphicsGLRegisterBuffer(CUgraphicsResource *pCudaResource, GLuint buffer, unsigned int Flags);
typedef CUresult CUDAAPI tcuGraphicsGLRegisterImage(CUgraphicsResource *pCudaResource, GLuint image, GLenum target, unsigned int Flags);
typedef CUresult CUDAAPI tcuCtxSetCurrent(CUcontext ctx);
typedef CUresult CUDAAPI tcuLaunchKernel(CUfunction f, unsigned gridDimX, unsigned gridDimY, unsigned gridDimZ, unsigned blockDimX, unsigned blockDimY, unsigned blockDimZ, unsigned sharedMemBytes, CUstream hStream, void* kernelParams, void* extra);

/* function declarations */

extern tcuInit *cuInit;
extern tcuDriverGetVersion *cuDriverGetVersion;
extern tcuDeviceGet *cuDeviceGet;
extern tcuDeviceGetCount *cuDeviceGetCount;
extern tcuDeviceGetName *cuDeviceGetName;
extern tcuDeviceComputeCapability *cuDeviceComputeCapability;
extern tcuDeviceTotalMem *cuDeviceTotalMem;
extern tcuDeviceGetProperties *cuDeviceGetProperties;
extern tcuDeviceGetAttribute *cuDeviceGetAttribute;
extern tcuCtxCreate *cuCtxCreate;
extern tcuCtxDestroy *cuCtxDestroy;
extern tcuCtxAttach *cuCtxAttach;
extern tcuCtxDetach *cuCtxDetach;
extern tcuCtxPushCurrent *cuCtxPushCurrent;
extern tcuCtxPopCurrent *cuCtxPopCurrent;
extern tcuCtxGetDevice *cuCtxGetDevice;
extern tcuCtxSynchronize *cuCtxSynchronize;
extern tcuModuleLoad *cuModuleLoad;
extern tcuModuleLoadData *cuModuleLoadData;
extern tcuModuleLoadDataEx *cuModuleLoadDataEx;
extern tcuModuleLoadFatBinary *cuModuleLoadFatBinary;
extern tcuModuleUnload *cuModuleUnload;
extern tcuModuleGetFunction *cuModuleGetFunction;
extern tcuModuleGetGlobal *cuModuleGetGlobal;
extern tcuModuleGetTexRef *cuModuleGetTexRef;
extern tcuModuleGetSurfRef *cuModuleGetSurfRef;
extern tcuMemGetInfo *cuMemGetInfo;
extern tcuMemAlloc *cuMemAlloc;
extern tcuMemAllocPitch *cuMemAllocPitch;
extern tcuMemFree *cuMemFree;
extern tcuMemGetAddressRange *cuMemGetAddressRange;
extern tcuMemAllocHost *cuMemAllocHost;
extern tcuMemFreeHost *cuMemFreeHost;
extern tcuMemHostAlloc *cuMemHostAlloc;
extern tcuMemHostGetDevicePointer *cuMemHostGetDevicePointer;
extern tcuMemHostGetFlags *cuMemHostGetFlags;
extern tcuMemcpyHtoD *cuMemcpyHtoD;
extern tcuMemcpyDtoH *cuMemcpyDtoH;
extern tcuMemcpyDtoD *cuMemcpyDtoD;
extern tcuMemcpyDtoA *cuMemcpyDtoA;
extern tcuMemcpyAtoD *cuMemcpyAtoD;
extern tcuMemcpyHtoA *cuMemcpyHtoA;
extern tcuMemcpyAtoH *cuMemcpyAtoH;
extern tcuMemcpyAtoA *cuMemcpyAtoA;
extern tcuMemcpy2D *cuMemcpy2D;
extern tcuMemcpy2DUnaligned *cuMemcpy2DUnaligned;
extern tcuMemcpy3D *cuMemcpy3D;
extern tcuMemcpyHtoDAsync *cuMemcpyHtoDAsync;
extern tcuMemcpyDtoHAsync *cuMemcpyDtoHAsync;
extern tcuMemcpyDtoDAsync *cuMemcpyDtoDAsync;
extern tcuMemcpyHtoAAsync *cuMemcpyHtoAAsync;
extern tcuMemcpyAtoHAsync *cuMemcpyAtoHAsync;
extern tcuMemcpy2DAsync *cuMemcpy2DAsync;
extern tcuMemcpy3DAsync *cuMemcpy3DAsync;
extern tcuMemsetD8 *cuMemsetD8;
extern tcuMemsetD16 *cuMemsetD16;
extern tcuMemsetD32 *cuMemsetD32;
extern tcuMemsetD2D8 *cuMemsetD2D8;
extern tcuMemsetD2D16 *cuMemsetD2D16;
extern tcuMemsetD2D32 *cuMemsetD2D32;
extern tcuFuncSetBlockShape *cuFuncSetBlockShape;
extern tcuFuncSetSharedSize *cuFuncSetSharedSize;
extern tcuFuncGetAttribute *cuFuncGetAttribute;
extern tcuFuncSetCacheConfig *cuFuncSetCacheConfig;
extern tcuArrayCreate *cuArrayCreate;
extern tcuArrayGetDescriptor *cuArrayGetDescriptor;
extern tcuArrayDestroy *cuArrayDestroy;
extern tcuArray3DCreate *cuArray3DCreate;
extern tcuArray3DGetDescriptor *cuArray3DGetDescriptor;
extern tcuTexRefCreate *cuTexRefCreate;
extern tcuTexRefDestroy *cuTexRefDestroy;
extern tcuTexRefSetArray *cuTexRefSetArray;
extern tcuTexRefSetAddress *cuTexRefSetAddress;
extern tcuTexRefSetAddress2D *cuTexRefSetAddress2D;
extern tcuTexRefSetFormat *cuTexRefSetFormat;
extern tcuTexRefSetAddressMode *cuTexRefSetAddressMode;
extern tcuTexRefSetFilterMode *cuTexRefSetFilterMode;
extern tcuTexRefSetFlags *cuTexRefSetFlags;
extern tcuTexRefGetAddress *cuTexRefGetAddress;
extern tcuTexRefGetArray *cuTexRefGetArray;
extern tcuTexRefGetAddressMode *cuTexRefGetAddressMode;
extern tcuTexRefGetFilterMode *cuTexRefGetFilterMode;
extern tcuTexRefGetFormat *cuTexRefGetFormat;
extern tcuTexRefGetFlags *cuTexRefGetFlags;
extern tcuSurfRefSetArray *cuSurfRefSetArray;
extern tcuSurfRefGetArray *cuSurfRefGetArray;
extern tcuParamSetSize *cuParamSetSize;
extern tcuParamSeti *cuParamSeti;
extern tcuParamSetf *cuParamSetf;
extern tcuParamSetv *cuParamSetv;
extern tcuParamSetTexRef *cuParamSetTexRef;
extern tcuLaunch *cuLaunch;
extern tcuLaunchGrid *cuLaunchGrid;
extern tcuLaunchGridAsync *cuLaunchGridAsync;
extern tcuEventCreate *cuEventCreate;
extern tcuEventRecord *cuEventRecord;
extern tcuEventQuery *cuEventQuery;
extern tcuEventSynchronize *cuEventSynchronize;
extern tcuEventDestroy *cuEventDestroy;
extern tcuEventElapsedTime *cuEventElapsedTime;
extern tcuStreamCreate *cuStreamCreate;
extern tcuStreamQuery *cuStreamQuery;
extern tcuStreamSynchronize *cuStreamSynchronize;
extern tcuStreamDestroy *cuStreamDestroy;
extern tcuGraphicsUnregisterResource *cuGraphicsUnregisterResource;
extern tcuGraphicsSubResourceGetMappedArray *cuGraphicsSubResourceGetMappedArray;
extern tcuGraphicsResourceGetMappedPointer *cuGraphicsResourceGetMappedPointer;
extern tcuGraphicsResourceSetMapFlags *cuGraphicsResourceSetMapFlags;
extern tcuGraphicsMapResources *cuGraphicsMapResources;
extern tcuGraphicsUnmapResources *cuGraphicsUnmapResources;
extern tcuGetExportTable *cuGetExportTable;
extern tcuCtxSetLimit *cuCtxSetLimit;
extern tcuCtxGetLimit *cuCtxGetLimit;
extern tcuGLCtxCreate *cuGLCtxCreate;
extern tcuGraphicsGLRegisterBuffer *cuGraphicsGLRegisterBuffer;
extern tcuGraphicsGLRegisterImage *cuGraphicsGLRegisterImage;
extern tcuCtxSetCurrent *cuCtxSetCurrent;
extern tcuLaunchKernel *cuLaunchKernel;

#endif /* __UTIL_CUDA_H__ */

