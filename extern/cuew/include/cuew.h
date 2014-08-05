/*
 * Copyright 2011-2014 Blender Foundation
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

#ifndef __CUEW_H__
#define __CUEW_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

/* Defines. */
#define CUEW_VERSION_MAJOR 1
#define CUEW_VERSION_MINOR 2

#define CUDA_VERSION 6000
#define CU_IPC_HANDLE_SIZE 64
#define CU_MEMHOSTALLOC_PORTABLE 0x01
#define CU_MEMHOSTALLOC_DEVICEMAP 0x02
#define CU_MEMHOSTALLOC_WRITECOMBINED 0x04
#define CU_MEMHOSTREGISTER_PORTABLE 0x01
#define CU_MEMHOSTREGISTER_DEVICEMAP 0x02
#define CUDA_ARRAY3D_LAYERED 0x01
#define CUDA_ARRAY3D_2DARRAY 0x01
#define CUDA_ARRAY3D_SURFACE_LDST 0x02
#define CUDA_ARRAY3D_CUBEMAP 0x04
#define CUDA_ARRAY3D_TEXTURE_GATHER 0x08
#define CUDA_ARRAY3D_DEPTH_TEXTURE 0x10
#define CU_TRSA_OVERRIDE_FORMAT 0x01
#define CU_TRSF_READ_AS_INTEGER 0x01
#define CU_TRSF_NORMALIZED_COORDINATES 0x02
#define CU_TRSF_SRGB 0x10
#define CU_LAUNCH_PARAM_END ((void*)0x00)
#define CU_LAUNCH_PARAM_BUFFER_POINTER ((void*)0x01)
#define CU_LAUNCH_PARAM_BUFFER_SIZE ((void*)0x02)
#define CU_PARAM_TR_DEFAULT -1
#define CUDAGL_H

/* Functions which changed 3.1 -> 3.2 for 64 bit stuff,
 * the cuda library has both the old ones for compatibility and new
 * ones with _v2 postfix,
 */
#define cuDeviceTotalMem cuDeviceTotalMem_v2
#define cuCtxCreate cuCtxCreate_v2
#define cuModuleGetGlobal cuModuleGetGlobal_v2
#define cuMemGetInfo cuMemGetInfo_v2
#define cuMemAlloc cuMemAlloc_v2
#define cuMemAllocPitch cuMemAllocPitch_v2
#define cuMemFree cuMemFree_v2
#define cuMemGetAddressRange cuMemGetAddressRange_v2
#define cuMemAllocHost cuMemAllocHost_v2
#define cuMemHostGetDevicePointer cuMemHostGetDevicePointer_v2
#define cuMemcpyHtoD cuMemcpyHtoD_v2
#define cuMemcpyDtoH cuMemcpyDtoH_v2
#define cuMemcpyDtoD cuMemcpyDtoD_v2
#define cuMemcpyDtoA cuMemcpyDtoA_v2
#define cuMemcpyAtoD cuMemcpyAtoD_v2
#define cuMemcpyHtoA cuMemcpyHtoA_v2
#define cuMemcpyAtoH cuMemcpyAtoH_v2
#define cuMemcpyAtoA cuMemcpyAtoA_v2
#define cuMemcpyHtoAAsync cuMemcpyHtoAAsync_v2
#define cuMemcpyAtoHAsync cuMemcpyAtoHAsync_v2
#define cuMemcpy2D cuMemcpy2D_v2
#define cuMemcpy2DUnaligned cuMemcpy2DUnaligned_v2
#define cuMemcpy3D cuMemcpy3D_v2
#define cuMemcpyHtoDAsync cuMemcpyHtoDAsync_v2
#define cuMemcpyDtoHAsync cuMemcpyDtoHAsync_v2
#define cuMemcpyDtoDAsync cuMemcpyDtoDAsync_v2
#define cuMemcpy2DAsync cuMemcpy2DAsync_v2
#define cuMemcpy3DAsync cuMemcpy3DAsync_v2
#define cuMemsetD8 cuMemsetD8_v2
#define cuMemsetD16 cuMemsetD16_v2
#define cuMemsetD32 cuMemsetD32_v2
#define cuMemsetD2D8 cuMemsetD2D8_v2
#define cuMemsetD2D16 cuMemsetD2D16_v2
#define cuMemsetD2D32 cuMemsetD2D32_v2
#define cuArrayCreate cuArrayCreate_v2
#define cuArrayGetDescriptor cuArrayGetDescriptor_v2
#define cuArray3DCreate cuArray3DCreate_v2
#define cuArray3DGetDescriptor cuArray3DGetDescriptor_v2
#define cuTexRefSetAddress cuTexRefSetAddress_v2
#define cuTexRefGetAddress cuTexRefGetAddress_v2
#define cuGraphicsResourceGetMappedPointer cuGraphicsResourceGetMappedPointer_v2
#define cuCtxDestroy cuCtxDestroy_v2
#define cuCtxPopCurrent cuCtxPopCurrent_v2
#define cuCtxPushCurrent cuCtxPushCurrent_v2
#define cuStreamDestroy cuStreamDestroy_v2
#define cuEventDestroy cuEventDestroy_v2
#define cuTexRefSetAddress2D cuTexRefSetAddress2D_v2
#define cuGLCtxCreate cuGLCtxCreate_v2
#define cuGLMapBufferObject cuGLMapBufferObject_v2
#define cuGLMapBufferObjectAsync cuGLMapBufferObjectAsync_v2

/* Types. */
#if defined(__x86_64) || defined(AMD64) || defined(_M_AMD64)
typedef unsigned long long CUdeviceptr;
#else
typedef unsigned int CUdeviceptr;
#endif

typedef int CUdevice;
typedef struct CUctx_st* CUcontext;
typedef struct CUmod_st* CUmodule;
typedef struct CUfunc_st* CUfunction;
typedef struct CUarray_st* CUarray;
typedef struct CUmipmappedArray_st* CUmipmappedArray;
typedef struct CUtexref_st* CUtexref;
typedef struct CUsurfref_st* CUsurfref;
typedef struct CUevent_st* CUevent;
typedef struct CUstream_st* CUstream;
typedef struct CUgraphicsResource_st* CUgraphicsResource;
typedef unsigned CUtexObject;
typedef unsigned CUsurfObject;

typedef struct CUuuid_st {
  char bytes[16];
} CUuuid;

typedef struct CUipcEventHandle_st {
  char reserved[CU_IPC_HANDLE_SIZE];
} CUipcEventHandle;

typedef struct CUipcMemHandle_st {
  char reserved[CU_IPC_HANDLE_SIZE];
} CUipcMemHandle;

typedef enum CUipcMem_flags_enum {
  CU_IPC_MEM_LAZY_ENABLE_PEER_ACCESS = 0x1,
} CUipcMem_flags;

typedef enum CUmemAttach_flags_enum {
  CU_MEM_ATTACH_GLOBAL = 0x1,
  CU_MEM_ATTACH_HOST = 0x2,
  CU_MEM_ATTACH_SINGLE = 0x4,
} CUmemAttach_flags;

typedef enum CUctx_flags_enum {
  CU_CTX_SCHED_AUTO = 0x00,
  CU_CTX_SCHED_SPIN = 0x01,
  CU_CTX_SCHED_YIELD = 0x02,
  CU_CTX_SCHED_BLOCKING_SYNC = 0x04,
  CU_CTX_BLOCKING_SYNC = 0x04,
  CU_CTX_SCHED_MASK = 0x07,
  CU_CTX_MAP_HOST = 0x08,
  CU_CTX_LMEM_RESIZE_TO_MAX = 0x10,
  CU_CTX_FLAGS_MASK = 0x1f,
} CUctx_flags;

typedef enum CUstream_flags_enum {
  CU_STREAM_DEFAULT = 0x0,
  CU_STREAM_NON_BLOCKING = 0x1,
} CUstream_flags;

typedef enum CUevent_flags_enum {
  CU_EVENT_DEFAULT = 0x0,
  CU_EVENT_BLOCKING_SYNC = 0x1,
  CU_EVENT_DISABLE_TIMING = 0x2,
  CU_EVENT_INTERPROCESS = 0x4,
} CUevent_flags;

typedef enum CUarray_format_enum {
  CU_AD_FORMAT_UNSIGNED_INT8 = 0x01,
  CU_AD_FORMAT_UNSIGNED_INT16 = 0x02,
  CU_AD_FORMAT_UNSIGNED_INT32 = 0x03,
  CU_AD_FORMAT_SIGNED_INT8 = 0x08,
  CU_AD_FORMAT_SIGNED_INT16 = 0x09,
  CU_AD_FORMAT_SIGNED_INT32 = 0x0a,
  CU_AD_FORMAT_HALF = 0x10,
  CU_AD_FORMAT_FLOAT = 0x20,
} CUarray_format;

typedef enum CUaddress_mode_enum {
  CU_TR_ADDRESS_MODE_WRAP = 0,
  CU_TR_ADDRESS_MODE_CLAMP = 1,
  CU_TR_ADDRESS_MODE_MIRROR = 2,
  CU_TR_ADDRESS_MODE_BORDER = 3,
} CUaddress_mode;

typedef enum CUfilter_mode_enum {
  CU_TR_FILTER_MODE_POINT = 0,
  CU_TR_FILTER_MODE_LINEAR = 1,
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
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_WIDTH = 27,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_HEIGHT = 28,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_LAYERS = 29,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_ARRAY_WIDTH = 27,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_ARRAY_HEIGHT = 28,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_ARRAY_NUMSLICES = 29,
  CU_DEVICE_ATTRIBUTE_SURFACE_ALIGNMENT = 30,
  CU_DEVICE_ATTRIBUTE_CONCURRENT_KERNELS = 31,
  CU_DEVICE_ATTRIBUTE_ECC_ENABLED = 32,
  CU_DEVICE_ATTRIBUTE_PCI_BUS_ID = 33,
  CU_DEVICE_ATTRIBUTE_PCI_DEVICE_ID = 34,
  CU_DEVICE_ATTRIBUTE_TCC_DRIVER = 35,
  CU_DEVICE_ATTRIBUTE_MEMORY_CLOCK_RATE = 36,
  CU_DEVICE_ATTRIBUTE_GLOBAL_MEMORY_BUS_WIDTH = 37,
  CU_DEVICE_ATTRIBUTE_L2_CACHE_SIZE = 38,
  CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR = 39,
  CU_DEVICE_ATTRIBUTE_ASYNC_ENGINE_COUNT = 40,
  CU_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING = 41,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_LAYERED_WIDTH = 42,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_LAYERED_LAYERS = 43,
  CU_DEVICE_ATTRIBUTE_CAN_TEX2D_GATHER = 44,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_GATHER_WIDTH = 45,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_GATHER_HEIGHT = 46,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_WIDTH_ALTERNATE = 47,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_HEIGHT_ALTERNATE = 48,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_DEPTH_ALTERNATE = 49,
  CU_DEVICE_ATTRIBUTE_PCI_DOMAIN_ID = 50,
  CU_DEVICE_ATTRIBUTE_TEXTURE_PITCH_ALIGNMENT = 51,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_WIDTH = 52,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_LAYERED_WIDTH = 53,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_LAYERED_LAYERS = 54,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE1D_WIDTH = 55,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_WIDTH = 56,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_HEIGHT = 57,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE3D_WIDTH = 58,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE3D_HEIGHT = 59,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE3D_DEPTH = 60,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE1D_LAYERED_WIDTH = 61,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE1D_LAYERED_LAYERS = 62,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_WIDTH = 63,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_HEIGHT = 64,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_LAYERS = 65,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_WIDTH = 66,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_LAYERED_WIDTH = 67,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_LAYERED_LAYERS = 68,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_LINEAR_WIDTH = 69,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_WIDTH = 70,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_HEIGHT = 71,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_PITCH = 72,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_MIPMAPPED_WIDTH = 73,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_MIPMAPPED_HEIGHT = 74,
  CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR = 75,
  CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR = 76,
  CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_MIPMAPPED_WIDTH = 77,
  CU_DEVICE_ATTRIBUTE_STREAM_PRIORITIES_SUPPORTED = 78,
  CU_DEVICE_ATTRIBUTE_GLOBAL_L1_CACHE_SUPPORTED = 79,
  CU_DEVICE_ATTRIBUTE_LOCAL_L1_CACHE_SUPPORTED = 80,
  CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_MULTIPROCESSOR = 81,
  CU_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_MULTIPROCESSOR = 82,
  CU_DEVICE_ATTRIBUTE_MANAGED_MEMORY = 83,
  CU_DEVICE_ATTRIBUTE_MULTI_GPU_BOARD = 84,
  CU_DEVICE_ATTRIBUTE_MULTI_GPU_BOARD_GROUP_ID = 85,
  CU_DEVICE_ATTRIBUTE_MAX,
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

typedef enum CUpointer_attribute_enum {
  CU_POINTER_ATTRIBUTE_CONTEXT = 1,
  CU_POINTER_ATTRIBUTE_MEMORY_TYPE = 2,
  CU_POINTER_ATTRIBUTE_DEVICE_POINTER = 3,
  CU_POINTER_ATTRIBUTE_HOST_POINTER = 4,
  CU_POINTER_ATTRIBUTE_P2P_TOKENS = 5,
  CU_POINTER_ATTRIBUTE_SYNC_MEMOPS = 6,
  CU_POINTER_ATTRIBUTE_BUFFER_ID = 7,
  CU_POINTER_ATTRIBUTE_IS_MANAGED = 8,
} CUpointer_attribute;

typedef enum CUfunction_attribute_enum {
  CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK = 0,
  CU_FUNC_ATTRIBUTE_SHARED_SIZE_BYTES = 1,
  CU_FUNC_ATTRIBUTE_CONST_SIZE_BYTES = 2,
  CU_FUNC_ATTRIBUTE_LOCAL_SIZE_BYTES = 3,
  CU_FUNC_ATTRIBUTE_NUM_REGS = 4,
  CU_FUNC_ATTRIBUTE_PTX_VERSION = 5,
  CU_FUNC_ATTRIBUTE_BINARY_VERSION = 6,
  CU_FUNC_ATTRIBUTE_CACHE_MODE_CA = 7,
  CU_FUNC_ATTRIBUTE_MAX,
} CUfunction_attribute;

typedef enum CUfunc_cache_enum {
  CU_FUNC_CACHE_PREFER_NONE = 0x00,
  CU_FUNC_CACHE_PREFER_SHARED = 0x01,
  CU_FUNC_CACHE_PREFER_L1 = 0x02,
  CU_FUNC_CACHE_PREFER_EQUAL = 0x03,
} CUfunc_cache;

typedef enum CUsharedconfig_enum {
  CU_SHARED_MEM_CONFIG_DEFAULT_BANK_SIZE = 0x00,
  CU_SHARED_MEM_CONFIG_FOUR_BYTE_BANK_SIZE = 0x01,
  CU_SHARED_MEM_CONFIG_EIGHT_BYTE_BANK_SIZE = 0x02,
} CUsharedconfig;

typedef enum CUmemorytype_enum {
  CU_MEMORYTYPE_HOST = 0x01,
  CU_MEMORYTYPE_DEVICE = 0x02,
  CU_MEMORYTYPE_ARRAY = 0x03,
  CU_MEMORYTYPE_UNIFIED = 0x04,
} CUmemorytype;

typedef enum CUcomputemode_enum {
  CU_COMPUTEMODE_DEFAULT = 0,
  CU_COMPUTEMODE_EXCLUSIVE = 1,
  CU_COMPUTEMODE_PROHIBITED = 2,
  CU_COMPUTEMODE_EXCLUSIVE_PROCESS = 3,
} CUcomputemode;

typedef enum CUjit_option_enum {
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
  CU_JIT_FALLBACK_STRATEGY,
  CU_JIT_GENERATE_DEBUG_INFO,
  CU_JIT_LOG_VERBOSE,
  CU_JIT_GENERATE_LINE_INFO,
  CU_JIT_CACHE_MODE,
  CU_JIT_NUM_OPTIONS,
} CUjit_option;

typedef enum CUjit_target_enum {
  CU_TARGET_COMPUTE_10 = 10,
  CU_TARGET_COMPUTE_11 = 11,
  CU_TARGET_COMPUTE_12 = 12,
  CU_TARGET_COMPUTE_13 = 13,
  CU_TARGET_COMPUTE_20 = 20,
  CU_TARGET_COMPUTE_21 = 21,
  CU_TARGET_COMPUTE_30 = 30,
  CU_TARGET_COMPUTE_32 = 32,
  CU_TARGET_COMPUTE_35 = 35,
  CU_TARGET_COMPUTE_50 = 50,
} CUjit_target;

typedef enum CUjit_fallback_enum {
  CU_PREFER_PTX = 0,
  CU_PREFER_BINARY,
} CUjit_fallback;

typedef enum CUjit_cacheMode_enum {
  CU_JIT_CACHE_OPTION_NONE = 0,
  CU_JIT_CACHE_OPTION_CG,
  CU_JIT_CACHE_OPTION_CA,
} CUjit_cacheMode;

typedef enum CUjitInputType_enum {
  CU_JIT_INPUT_CUBIN = 0,
  CU_JIT_INPUT_PTX,
  CU_JIT_INPUT_FATBINARY,
  CU_JIT_INPUT_OBJECT,
  CU_JIT_INPUT_LIBRARY,
  CU_JIT_NUM_INPUT_TYPES,
} CUjitInputType;

typedef struct CUlinkState_st* CUlinkState;

typedef enum CUgraphicsRegisterFlags_enum {
  CU_GRAPHICS_REGISTER_FLAGS_NONE = 0x00,
  CU_GRAPHICS_REGISTER_FLAGS_READ_ONLY = 0x01,
  CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD = 0x02,
  CU_GRAPHICS_REGISTER_FLAGS_SURFACE_LDST = 0x04,
  CU_GRAPHICS_REGISTER_FLAGS_TEXTURE_GATHER = 0x08,
} CUgraphicsRegisterFlags;

typedef enum CUgraphicsMapResourceFlags_enum {
  CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE = 0x00,
  CU_GRAPHICS_MAP_RESOURCE_FLAGS_READ_ONLY = 0x01,
  CU_GRAPHICS_MAP_RESOURCE_FLAGS_WRITE_DISCARD = 0x02,
} CUgraphicsMapResourceFlags;

typedef enum CUarray_cubemap_face_enum {
  CU_CUBEMAP_FACE_POSITIVE_X = 0x00,
  CU_CUBEMAP_FACE_NEGATIVE_X = 0x01,
  CU_CUBEMAP_FACE_POSITIVE_Y = 0x02,
  CU_CUBEMAP_FACE_NEGATIVE_Y = 0x03,
  CU_CUBEMAP_FACE_POSITIVE_Z = 0x04,
  CU_CUBEMAP_FACE_NEGATIVE_Z = 0x05,
} CUarray_cubemap_face;

typedef enum CUlimit_enum {
  CU_LIMIT_STACK_SIZE = 0x00,
  CU_LIMIT_PRINTF_FIFO_SIZE = 0x01,
  CU_LIMIT_MALLOC_HEAP_SIZE = 0x02,
  CU_LIMIT_DEV_RUNTIME_SYNC_DEPTH = 0x03,
  CU_LIMIT_DEV_RUNTIME_PENDING_LAUNCH_COUNT = 0x04,
  CU_LIMIT_MAX,
} CUlimit;

typedef enum CUresourcetype_enum {
  CU_RESOURCE_TYPE_ARRAY = 0x00,
  CU_RESOURCE_TYPE_MIPMAPPED_ARRAY = 0x01,
  CU_RESOURCE_TYPE_LINEAR = 0x02,
  CU_RESOURCE_TYPE_PITCH2D = 0x03,
} CUresourcetype;

typedef enum cudaError_enum {
  CUDA_SUCCESS = 0,
  CUDA_ERROR_INVALID_VALUE = 1,
  CUDA_ERROR_OUT_OF_MEMORY = 2,
  CUDA_ERROR_NOT_INITIALIZED = 3,
  CUDA_ERROR_DEINITIALIZED = 4,
  CUDA_ERROR_PROFILER_DISABLED = 5,
  CUDA_ERROR_PROFILER_NOT_INITIALIZED = 6,
  CUDA_ERROR_PROFILER_ALREADY_STARTED = 7,
  CUDA_ERROR_PROFILER_ALREADY_STOPPED = 8,
  CUDA_ERROR_NO_DEVICE = 100,
  CUDA_ERROR_INVALID_DEVICE = 101,
  CUDA_ERROR_INVALID_IMAGE = 200,
  CUDA_ERROR_INVALID_CONTEXT = 201,
  CUDA_ERROR_CONTEXT_ALREADY_CURRENT = 202,
  CUDA_ERROR_MAP_FAILED = 205,
  CUDA_ERROR_UNMAP_FAILED = 206,
  CUDA_ERROR_ARRAY_IS_MAPPED = 207,
  CUDA_ERROR_ALREADY_MAPPED = 208,
  CUDA_ERROR_NO_BINARY_FOR_GPU = 209,
  CUDA_ERROR_ALREADY_ACQUIRED = 210,
  CUDA_ERROR_NOT_MAPPED = 211,
  CUDA_ERROR_NOT_MAPPED_AS_ARRAY = 212,
  CUDA_ERROR_NOT_MAPPED_AS_POINTER = 213,
  CUDA_ERROR_ECC_UNCORRECTABLE = 214,
  CUDA_ERROR_UNSUPPORTED_LIMIT = 215,
  CUDA_ERROR_CONTEXT_ALREADY_IN_USE = 216,
  CUDA_ERROR_PEER_ACCESS_UNSUPPORTED = 217,
  CUDA_ERROR_INVALID_PTX = 218,
  CUDA_ERROR_INVALID_SOURCE = 300,
  CUDA_ERROR_FILE_NOT_FOUND = 301,
  CUDA_ERROR_SHARED_OBJECT_SYMBOL_NOT_FOUND = 302,
  CUDA_ERROR_SHARED_OBJECT_INIT_FAILED = 303,
  CUDA_ERROR_OPERATING_SYSTEM = 304,
  CUDA_ERROR_INVALID_HANDLE = 400,
  CUDA_ERROR_NOT_FOUND = 500,
  CUDA_ERROR_NOT_READY = 600,
  CUDA_ERROR_ILLEGAL_ADDRESS = 700,
  CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES = 701,
  CUDA_ERROR_LAUNCH_TIMEOUT = 702,
  CUDA_ERROR_LAUNCH_INCOMPATIBLE_TEXTURING = 703,
  CUDA_ERROR_PEER_ACCESS_ALREADY_ENABLED = 704,
  CUDA_ERROR_PEER_ACCESS_NOT_ENABLED = 705,
  CUDA_ERROR_PRIMARY_CONTEXT_ACTIVE = 708,
  CUDA_ERROR_CONTEXT_IS_DESTROYED = 709,
  CUDA_ERROR_ASSERT = 710,
  CUDA_ERROR_TOO_MANY_PEERS = 711,
  CUDA_ERROR_HOST_MEMORY_ALREADY_REGISTERED = 712,
  CUDA_ERROR_HOST_MEMORY_NOT_REGISTERED = 713,
  CUDA_ERROR_HARDWARE_STACK_ERROR = 714,
  CUDA_ERROR_ILLEGAL_INSTRUCTION = 715,
  CUDA_ERROR_MISALIGNED_ADDRESS = 716,
  CUDA_ERROR_INVALID_ADDRESS_SPACE = 717,
  CUDA_ERROR_INVALID_PC = 718,
  CUDA_ERROR_LAUNCH_FAILED = 719,
  CUDA_ERROR_NOT_PERMITTED = 800,
  CUDA_ERROR_NOT_SUPPORTED = 801,
  CUDA_ERROR_UNKNOWN = 999,
} CUresult;

typedef void* CUstreamCallback;

typedef struct CUDA_MEMCPY2D_st {
  size_t srcXInBytes;
  size_t srcY;
  CUmemorytype srcMemoryType;
  const void* srcHost;
  CUdeviceptr srcDevice;
  CUarray srcArray;
  size_t srcPitch;
  size_t dstXInBytes;
  size_t dstY;
  CUmemorytype dstMemoryType;
  void* dstHost;
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
  const void* srcHost;
  CUdeviceptr srcDevice;
  CUarray srcArray;
  void* reserved0;
  size_t srcPitch;
  size_t srcHeight;
  size_t dstXInBytes;
  size_t dstY;
  size_t dstZ;
  size_t dstLOD;
  CUmemorytype dstMemoryType;
  void* dstHost;
  CUdeviceptr dstDevice;
  CUarray dstArray;
  void* reserved1;
  size_t dstPitch;
  size_t dstHeight;
  size_t WidthInBytes;
  size_t Height;
  size_t Depth;
} CUDA_MEMCPY3D;

typedef struct CUDA_MEMCPY3D_PEER_st {
  size_t srcXInBytes;
  size_t srcY;
  size_t srcZ;
  size_t srcLOD;
  CUmemorytype srcMemoryType;
  const void* srcHost;
  CUdeviceptr srcDevice;
  CUarray srcArray;
  CUcontext srcContext;
  size_t srcPitch;
  size_t srcHeight;
  size_t dstXInBytes;
  size_t dstY;
  size_t dstZ;
  size_t dstLOD;
  CUmemorytype dstMemoryType;
  void* dstHost;
  CUdeviceptr dstDevice;
  CUarray dstArray;
  CUcontext dstContext;
  size_t dstPitch;
  size_t dstHeight;
  size_t WidthInBytes;
  size_t Height;
  size_t Depth;
} CUDA_MEMCPY3D_PEER;

typedef struct CUDA_ARRAY_DESCRIPTOR_st {
  size_t Width;
  size_t Height;
  CUarray_format Format;
  unsigned NumChannels;
} CUDA_ARRAY_DESCRIPTOR;

typedef struct CUDA_ARRAY3D_DESCRIPTOR_st {
  size_t Width;
  size_t Height;
  size_t Depth;
  CUarray_format Format;
  unsigned NumChannels;
  unsigned Flags;
} CUDA_ARRAY3D_DESCRIPTOR;

typedef struct CUDA_RESOURCE_DESC_st {
  CUresourcetype resType;
  union {
    struct {
      CUarray hArray;
    } array;
    struct {
      CUmipmappedArray hMipmappedArray;
    } mipmap;
    struct {
      CUdeviceptr devPtr;
      CUarray_format format;
      unsigned numChannels;
      size_t sizeInBytes;
    } linear;
    struct {
      CUdeviceptr devPtr;
      CUarray_format format;
      unsigned numChannels;
      size_t width;
      size_t height;
      size_t pitchInBytes;
    } pitch2D;
    struct {
      int reserved[32];
    } reserved;
  } res;
  unsigned flags;
} CUDA_RESOURCE_DESC;

typedef struct CUDA_TEXTURE_DESC_st {
  CUaddress_mode addressMode[3];
  CUfilter_mode filterMode;
  unsigned flags;
  unsigned maxAnisotropy;
  CUfilter_mode mipmapFilterMode;
  float mipmapLevelBias;
  float minMipmapLevelClamp;
  float maxMipmapLevelClamp;
  int reserved[16];
} CUDA_TEXTURE_DESC;

typedef enum CUresourceViewFormat_enum {
  CU_RES_VIEW_FORMAT_NONE = 0x00,
  CU_RES_VIEW_FORMAT_UINT_1X8 = 0x01,
  CU_RES_VIEW_FORMAT_UINT_2X8 = 0x02,
  CU_RES_VIEW_FORMAT_UINT_4X8 = 0x03,
  CU_RES_VIEW_FORMAT_SINT_1X8 = 0x04,
  CU_RES_VIEW_FORMAT_SINT_2X8 = 0x05,
  CU_RES_VIEW_FORMAT_SINT_4X8 = 0x06,
  CU_RES_VIEW_FORMAT_UINT_1X16 = 0x07,
  CU_RES_VIEW_FORMAT_UINT_2X16 = 0x08,
  CU_RES_VIEW_FORMAT_UINT_4X16 = 0x09,
  CU_RES_VIEW_FORMAT_SINT_1X16 = 0x0a,
  CU_RES_VIEW_FORMAT_SINT_2X16 = 0x0b,
  CU_RES_VIEW_FORMAT_SINT_4X16 = 0x0c,
  CU_RES_VIEW_FORMAT_UINT_1X32 = 0x0d,
  CU_RES_VIEW_FORMAT_UINT_2X32 = 0x0e,
  CU_RES_VIEW_FORMAT_UINT_4X32 = 0x0f,
  CU_RES_VIEW_FORMAT_SINT_1X32 = 0x10,
  CU_RES_VIEW_FORMAT_SINT_2X32 = 0x11,
  CU_RES_VIEW_FORMAT_SINT_4X32 = 0x12,
  CU_RES_VIEW_FORMAT_FLOAT_1X16 = 0x13,
  CU_RES_VIEW_FORMAT_FLOAT_2X16 = 0x14,
  CU_RES_VIEW_FORMAT_FLOAT_4X16 = 0x15,
  CU_RES_VIEW_FORMAT_FLOAT_1X32 = 0x16,
  CU_RES_VIEW_FORMAT_FLOAT_2X32 = 0x17,
  CU_RES_VIEW_FORMAT_FLOAT_4X32 = 0x18,
  CU_RES_VIEW_FORMAT_UNSIGNED_BC1 = 0x19,
  CU_RES_VIEW_FORMAT_UNSIGNED_BC2 = 0x1a,
  CU_RES_VIEW_FORMAT_UNSIGNED_BC3 = 0x1b,
  CU_RES_VIEW_FORMAT_UNSIGNED_BC4 = 0x1c,
  CU_RES_VIEW_FORMAT_SIGNED_BC4 = 0x1d,
  CU_RES_VIEW_FORMAT_UNSIGNED_BC5 = 0x1e,
  CU_RES_VIEW_FORMAT_SIGNED_BC5 = 0x1f,
  CU_RES_VIEW_FORMAT_UNSIGNED_BC6H = 0x20,
  CU_RES_VIEW_FORMAT_SIGNED_BC6H = 0x21,
  CU_RES_VIEW_FORMAT_UNSIGNED_BC7 = 0x22,
} CUresourceViewFormat;

typedef struct CUDA_RESOURCE_VIEW_DESC_st {
  CUresourceViewFormat format;
  size_t width;
  size_t height;
  size_t depth;
  unsigned firstMipmapLevel;
  unsigned lastMipmapLevel;
  unsigned firstLayer;
  unsigned lastLayer;
  unsigned reserved[16];
} CUDA_RESOURCE_VIEW_DESC;

typedef struct CUDA_POINTER_ATTRIBUTE_P2P_TOKENS_st {
  unsigned p2pToken;
  unsigned vaSpaceToken;
} CUDA_POINTER_ATTRIBUTE_P2P_TOKENS;
typedef unsigned GLenum;
typedef unsigned GLuint;
typedef int GLint;

typedef enum CUGLDeviceList_enum {
  CU_GL_DEVICE_LIST_ALL = 0x01,
  CU_GL_DEVICE_LIST_CURRENT_FRAME = 0x02,
  CU_GL_DEVICE_LIST_NEXT_FRAME = 0x03,
} CUGLDeviceList;

typedef enum CUGLmap_flags_enum {
  CU_GL_MAP_RESOURCE_FLAGS_NONE = 0x00,
  CU_GL_MAP_RESOURCE_FLAGS_READ_ONLY = 0x01,
  CU_GL_MAP_RESOURCE_FLAGS_WRITE_DISCARD = 0x02,
} CUGLmap_flags;

#ifdef _WIN32
#  define CUDAAPI __stdcall
#  define CUDA_CB __stdcall
#else
#  define CUDAAPI
#  define CUDA_CB
#endif

/* Function types. */
typedef CUresult CUDAAPI tcuGetErrorString(CUresult error, const char* pStr);
typedef CUresult CUDAAPI tcuGetErrorName(CUresult error, const char* pStr);
typedef CUresult CUDAAPI tcuInit(unsigned Flags);
typedef CUresult CUDAAPI tcuDriverGetVersion(int* driverVersion);
typedef CUresult CUDAAPI tcuDeviceGet(CUdevice* device, int ordinal);
typedef CUresult CUDAAPI tcuDeviceGetCount(int* count);
typedef CUresult CUDAAPI tcuDeviceGetName(char* name, int len, CUdevice dev);
typedef CUresult CUDAAPI tcuDeviceTotalMem_v2(size_t* bytes, CUdevice dev);
typedef CUresult CUDAAPI tcuDeviceGetAttribute(int* pi, CUdevice_attribute attrib, CUdevice dev);
typedef CUresult CUDAAPI tcuDeviceGetProperties(CUdevprop* prop, CUdevice dev);
typedef CUresult CUDAAPI tcuDeviceComputeCapability(int* major, int* minor, CUdevice dev);
typedef CUresult CUDAAPI tcuCtxCreate_v2(CUcontext* pctx, unsigned flags, CUdevice dev);
typedef CUresult CUDAAPI tcuCtxDestroy_v2(CUcontext ctx);
typedef CUresult CUDAAPI tcuCtxPushCurrent_v2(CUcontext ctx);
typedef CUresult CUDAAPI tcuCtxPopCurrent_v2(CUcontext* pctx);
typedef CUresult CUDAAPI tcuCtxSetCurrent(CUcontext ctx);
typedef CUresult CUDAAPI tcuCtxGetCurrent(CUcontext* pctx);
typedef CUresult CUDAAPI tcuCtxGetDevice(CUdevice* device);
typedef CUresult CUDAAPI tcuCtxSynchronize(void);
typedef CUresult CUDAAPI tcuCtxSetLimit(CUlimit limit, size_t value);
typedef CUresult CUDAAPI tcuCtxGetLimit(size_t* pvalue, CUlimit limit);
typedef CUresult CUDAAPI tcuCtxGetCacheConfig(CUfunc_cache* pconfig);
typedef CUresult CUDAAPI tcuCtxSetCacheConfig(CUfunc_cache config);
typedef CUresult CUDAAPI tcuCtxGetSharedMemConfig(CUsharedconfig* pConfig);
typedef CUresult CUDAAPI tcuCtxSetSharedMemConfig(CUsharedconfig config);
typedef CUresult CUDAAPI tcuCtxGetApiVersion(CUcontext ctx, unsigned* version);
typedef CUresult CUDAAPI tcuCtxGetStreamPriorityRange(int* leastPriority, int* greatestPriority);
typedef CUresult CUDAAPI tcuCtxAttach(CUcontext* pctx, unsigned flags);
typedef CUresult CUDAAPI tcuCtxDetach(CUcontext ctx);
typedef CUresult CUDAAPI tcuModuleLoad(CUmodule* module, const char* fname);
typedef CUresult CUDAAPI tcuModuleLoadData(CUmodule* module, const void* image);
typedef CUresult CUDAAPI tcuModuleLoadDataEx(CUmodule* module, const void* image, unsigned numOptions, CUjit_option* options, void* optionValues);
typedef CUresult CUDAAPI tcuModuleLoadFatBinary(CUmodule* module, const void* fatCubin);
typedef CUresult CUDAAPI tcuModuleUnload(CUmodule hmod);
typedef CUresult CUDAAPI tcuModuleGetFunction(CUfunction* hfunc, CUmodule hmod, const char* name);
typedef CUresult CUDAAPI tcuModuleGetGlobal_v2(CUdeviceptr* dptr, size_t* bytes, CUmodule hmod, const char* name);
typedef CUresult CUDAAPI tcuModuleGetTexRef(CUtexref* pTexRef, CUmodule hmod, const char* name);
typedef CUresult CUDAAPI tcuModuleGetSurfRef(CUsurfref* pSurfRef, CUmodule hmod, const char* name);
typedef CUresult CUDAAPI tcuLinkCreate(unsigned numOptions, CUjit_option* options, void* optionValues, CUlinkState* stateOut);
typedef CUresult CUDAAPI tcuLinkAddData(CUlinkState state, CUjitInputType type, void* data, size_t size, const char* name, unsigned numOptions, CUjit_option* options, void* optionValues);
typedef CUresult CUDAAPI tcuLinkAddFile(CUlinkState state, CUjitInputType type, const char* path, unsigned numOptions, CUjit_option* options, void* optionValues);
typedef CUresult CUDAAPI tcuLinkComplete(CUlinkState state, void* cubinOut, size_t* sizeOut);
typedef CUresult CUDAAPI tcuLinkDestroy(CUlinkState state);
typedef CUresult CUDAAPI tcuMemGetInfo_v2(size_t* free, size_t* total);
typedef CUresult CUDAAPI tcuMemAlloc_v2(CUdeviceptr* dptr, size_t bytesize);
typedef CUresult CUDAAPI tcuMemAllocPitch_v2(CUdeviceptr* dptr, size_t* pPitch, size_t WidthInBytes, size_t Height, unsigned ElementSizeBytes);
typedef CUresult CUDAAPI tcuMemFree_v2(CUdeviceptr dptr);
typedef CUresult CUDAAPI tcuMemGetAddressRange_v2(CUdeviceptr* pbase, size_t* psize, CUdeviceptr dptr);
typedef CUresult CUDAAPI tcuMemAllocHost_v2(void* pp, size_t bytesize);
typedef CUresult CUDAAPI tcuMemFreeHost(void* p);
typedef CUresult CUDAAPI tcuMemHostAlloc(void* pp, size_t bytesize, unsigned Flags);
typedef CUresult CUDAAPI tcuMemHostGetDevicePointer_v2(CUdeviceptr* pdptr, void* p, unsigned Flags);
typedef CUresult CUDAAPI tcuMemHostGetFlags(unsigned* pFlags, void* p);
typedef CUresult CUDAAPI tcuMemAllocManaged(CUdeviceptr* dptr, size_t bytesize, unsigned flags);
typedef CUresult CUDAAPI tcuDeviceGetByPCIBusId(CUdevice* dev, const char* pciBusId);
typedef CUresult CUDAAPI tcuDeviceGetPCIBusId(char* pciBusId, int len, CUdevice dev);
typedef CUresult CUDAAPI tcuIpcGetEventHandle(CUipcEventHandle* pHandle, CUevent event);
typedef CUresult CUDAAPI tcuIpcOpenEventHandle(CUevent* phEvent, CUipcEventHandle handle);
typedef CUresult CUDAAPI tcuIpcGetMemHandle(CUipcMemHandle* pHandle, CUdeviceptr dptr);
typedef CUresult CUDAAPI tcuIpcOpenMemHandle(CUdeviceptr* pdptr, CUipcMemHandle handle, unsigned Flags);
typedef CUresult CUDAAPI tcuIpcCloseMemHandle(CUdeviceptr dptr);
typedef CUresult CUDAAPI tcuMemHostRegister(void* p, size_t bytesize, unsigned Flags);
typedef CUresult CUDAAPI tcuMemHostUnregister(void* p);
typedef CUresult CUDAAPI tcuMemcpy(CUdeviceptr dst, CUdeviceptr src, size_t ByteCount);
typedef CUresult CUDAAPI tcuMemcpyPeer(CUdeviceptr dstDevice, CUcontext dstContext, CUdeviceptr srcDevice, CUcontext srcContext, size_t ByteCount);
typedef CUresult CUDAAPI tcuMemcpyHtoD_v2(CUdeviceptr dstDevice, const void* srcHost, size_t ByteCount);
typedef CUresult CUDAAPI tcuMemcpyDtoH_v2(void* dstHost, CUdeviceptr srcDevice, size_t ByteCount);
typedef CUresult CUDAAPI tcuMemcpyDtoD_v2(CUdeviceptr dstDevice, CUdeviceptr srcDevice, size_t ByteCount);
typedef CUresult CUDAAPI tcuMemcpyDtoA_v2(CUarray dstArray, size_t dstOffset, CUdeviceptr srcDevice, size_t ByteCount);
typedef CUresult CUDAAPI tcuMemcpyAtoD_v2(CUdeviceptr dstDevice, CUarray srcArray, size_t srcOffset, size_t ByteCount);
typedef CUresult CUDAAPI tcuMemcpyHtoA_v2(CUarray dstArray, size_t dstOffset, const void* srcHost, size_t ByteCount);
typedef CUresult CUDAAPI tcuMemcpyAtoH_v2(void* dstHost, CUarray srcArray, size_t srcOffset, size_t ByteCount);
typedef CUresult CUDAAPI tcuMemcpyAtoA_v2(CUarray dstArray, size_t dstOffset, CUarray srcArray, size_t srcOffset, size_t ByteCount);
typedef CUresult CUDAAPI tcuMemcpy2D_v2(const CUDA_MEMCPY2D* pCopy);
typedef CUresult CUDAAPI tcuMemcpy2DUnaligned_v2(const CUDA_MEMCPY2D* pCopy);
typedef CUresult CUDAAPI tcuMemcpy3D_v2(const CUDA_MEMCPY3D* pCopy);
typedef CUresult CUDAAPI tcuMemcpy3DPeer(const CUDA_MEMCPY3D_PEER* pCopy);
typedef CUresult CUDAAPI tcuMemcpyAsync(CUdeviceptr dst, CUdeviceptr src, size_t ByteCount, CUstream hStream);
typedef CUresult CUDAAPI tcuMemcpyPeerAsync(CUdeviceptr dstDevice, CUcontext dstContext, CUdeviceptr srcDevice, CUcontext srcContext, size_t ByteCount, CUstream hStream);
typedef CUresult CUDAAPI tcuMemcpyHtoDAsync_v2(CUdeviceptr dstDevice, const void* srcHost, size_t ByteCount, CUstream hStream);
typedef CUresult CUDAAPI tcuMemcpyDtoHAsync_v2(void* dstHost, CUdeviceptr srcDevice, size_t ByteCount, CUstream hStream);
typedef CUresult CUDAAPI tcuMemcpyDtoDAsync_v2(CUdeviceptr dstDevice, CUdeviceptr srcDevice, size_t ByteCount, CUstream hStream);
typedef CUresult CUDAAPI tcuMemcpyHtoAAsync_v2(CUarray dstArray, size_t dstOffset, const void* srcHost, size_t ByteCount, CUstream hStream);
typedef CUresult CUDAAPI tcuMemcpyAtoHAsync_v2(void* dstHost, CUarray srcArray, size_t srcOffset, size_t ByteCount, CUstream hStream);
typedef CUresult CUDAAPI tcuMemcpy2DAsync_v2(const CUDA_MEMCPY2D* pCopy, CUstream hStream);
typedef CUresult CUDAAPI tcuMemcpy3DAsync_v2(const CUDA_MEMCPY3D* pCopy, CUstream hStream);
typedef CUresult CUDAAPI tcuMemcpy3DPeerAsync(const CUDA_MEMCPY3D_PEER* pCopy, CUstream hStream);
typedef CUresult CUDAAPI tcuMemsetD8_v2(CUdeviceptr dstDevice, unsigned uc, size_t N);
typedef CUresult CUDAAPI tcuMemsetD16_v2(CUdeviceptr dstDevice, unsigned us, size_t N);
typedef CUresult CUDAAPI tcuMemsetD32_v2(CUdeviceptr dstDevice, unsigned ui, size_t N);
typedef CUresult CUDAAPI tcuMemsetD2D8_v2(CUdeviceptr dstDevice, size_t dstPitch, unsigned uc, size_t Width, size_t Height);
typedef CUresult CUDAAPI tcuMemsetD2D16_v2(CUdeviceptr dstDevice, size_t dstPitch, unsigned us, size_t Width, size_t Height);
typedef CUresult CUDAAPI tcuMemsetD2D32_v2(CUdeviceptr dstDevice, size_t dstPitch, unsigned ui, size_t Width, size_t Height);
typedef CUresult CUDAAPI tcuMemsetD8Async(CUdeviceptr dstDevice, unsigned uc, size_t N, CUstream hStream);
typedef CUresult CUDAAPI tcuMemsetD16Async(CUdeviceptr dstDevice, unsigned us, size_t N, CUstream hStream);
typedef CUresult CUDAAPI tcuMemsetD32Async(CUdeviceptr dstDevice, unsigned ui, size_t N, CUstream hStream);
typedef CUresult CUDAAPI tcuMemsetD2D8Async(CUdeviceptr dstDevice, size_t dstPitch, unsigned uc, size_t Width, size_t Height, CUstream hStream);
typedef CUresult CUDAAPI tcuMemsetD2D16Async(CUdeviceptr dstDevice, size_t dstPitch, unsigned us, size_t Width, size_t Height, CUstream hStream);
typedef CUresult CUDAAPI tcuMemsetD2D32Async(CUdeviceptr dstDevice, size_t dstPitch, unsigned ui, size_t Width, size_t Height, CUstream hStream);
typedef CUresult CUDAAPI tcuArrayCreate_v2(CUarray* pHandle, const CUDA_ARRAY_DESCRIPTOR* pAllocateArray);
typedef CUresult CUDAAPI tcuArrayGetDescriptor_v2(CUDA_ARRAY_DESCRIPTOR* pArrayDescriptor, CUarray hArray);
typedef CUresult CUDAAPI tcuArrayDestroy(CUarray hArray);
typedef CUresult CUDAAPI tcuArray3DCreate_v2(CUarray* pHandle, const CUDA_ARRAY3D_DESCRIPTOR* pAllocateArray);
typedef CUresult CUDAAPI tcuArray3DGetDescriptor_v2(CUDA_ARRAY3D_DESCRIPTOR* pArrayDescriptor, CUarray hArray);
typedef CUresult CUDAAPI tcuMipmappedArrayCreate(CUmipmappedArray* pHandle, const CUDA_ARRAY3D_DESCRIPTOR* pMipmappedArrayDesc, unsigned numMipmapLevels);
typedef CUresult CUDAAPI tcuMipmappedArrayGetLevel(CUarray* pLevelArray, CUmipmappedArray hMipmappedArray, unsigned level);
typedef CUresult CUDAAPI tcuMipmappedArrayDestroy(CUmipmappedArray hMipmappedArray);
typedef CUresult CUDAAPI tcuPointerGetAttribute(void* data, CUpointer_attribute attribute, CUdeviceptr ptr);
typedef CUresult CUDAAPI tcuPointerSetAttribute(const void* value, CUpointer_attribute attribute, CUdeviceptr ptr);
typedef CUresult CUDAAPI tcuStreamCreate(CUstream* phStream, unsigned Flags);
typedef CUresult CUDAAPI tcuStreamCreateWithPriority(CUstream* phStream, unsigned flags, int priority);
typedef CUresult CUDAAPI tcuStreamGetPriority(CUstream hStream, int* priority);
typedef CUresult CUDAAPI tcuStreamGetFlags(CUstream hStream, unsigned* flags);
typedef CUresult CUDAAPI tcuStreamWaitEvent(CUstream hStream, CUevent hEvent, unsigned Flags);
typedef CUresult CUDAAPI tcuStreamAddCallback(CUstream hStream, CUstreamCallback callback, void* userData, unsigned flags);
typedef CUresult CUDAAPI tcuStreamAttachMemAsync(CUstream hStream, CUdeviceptr dptr, size_t length, unsigned flags);
typedef CUresult CUDAAPI tcuStreamQuery(CUstream hStream);
typedef CUresult CUDAAPI tcuStreamSynchronize(CUstream hStream);
typedef CUresult CUDAAPI tcuStreamDestroy_v2(CUstream hStream);
typedef CUresult CUDAAPI tcuEventCreate(CUevent* phEvent, unsigned Flags);
typedef CUresult CUDAAPI tcuEventRecord(CUevent hEvent, CUstream hStream);
typedef CUresult CUDAAPI tcuEventQuery(CUevent hEvent);
typedef CUresult CUDAAPI tcuEventSynchronize(CUevent hEvent);
typedef CUresult CUDAAPI tcuEventDestroy_v2(CUevent hEvent);
typedef CUresult CUDAAPI tcuEventElapsedTime(float* pMilliseconds, CUevent hStart, CUevent hEnd);
typedef CUresult CUDAAPI tcuFuncGetAttribute(int* pi, CUfunction_attribute attrib, CUfunction hfunc);
typedef CUresult CUDAAPI tcuFuncSetCacheConfig(CUfunction hfunc, CUfunc_cache config);
typedef CUresult CUDAAPI tcuFuncSetSharedMemConfig(CUfunction hfunc, CUsharedconfig config);
typedef CUresult CUDAAPI tcuLaunchKernel(CUfunction f, unsigned gridDimX, unsigned gridDimY, unsigned gridDimZ, unsigned blockDimX, unsigned blockDimY, unsigned blockDimZ, unsigned sharedMemBytes, CUstream hStream, void* kernelParams, void* extra);
typedef CUresult CUDAAPI tcuFuncSetBlockShape(CUfunction hfunc, int x, int y, int z);
typedef CUresult CUDAAPI tcuFuncSetSharedSize(CUfunction hfunc, unsigned bytes);
typedef CUresult CUDAAPI tcuParamSetSize(CUfunction hfunc, unsigned numbytes);
typedef CUresult CUDAAPI tcuParamSeti(CUfunction hfunc, int offset, unsigned value);
typedef CUresult CUDAAPI tcuParamSetf(CUfunction hfunc, int offset, float value);
typedef CUresult CUDAAPI tcuParamSetv(CUfunction hfunc, int offset, void* ptr, unsigned numbytes);
typedef CUresult CUDAAPI tcuLaunch(CUfunction f);
typedef CUresult CUDAAPI tcuLaunchGrid(CUfunction f, int grid_width, int grid_height);
typedef CUresult CUDAAPI tcuLaunchGridAsync(CUfunction f, int grid_width, int grid_height, CUstream hStream);
typedef CUresult CUDAAPI tcuParamSetTexRef(CUfunction hfunc, int texunit, CUtexref hTexRef);
typedef CUresult CUDAAPI tcuTexRefSetArray(CUtexref hTexRef, CUarray hArray, unsigned Flags);
typedef CUresult CUDAAPI tcuTexRefSetMipmappedArray(CUtexref hTexRef, CUmipmappedArray hMipmappedArray, unsigned Flags);
typedef CUresult CUDAAPI tcuTexRefSetAddress_v2(size_t* ByteOffset, CUtexref hTexRef, CUdeviceptr dptr, size_t bytes);
typedef CUresult CUDAAPI tcuTexRefSetAddress2D_v3(CUtexref hTexRef, const CUDA_ARRAY_DESCRIPTOR* desc, CUdeviceptr dptr, size_t Pitch);
typedef CUresult CUDAAPI tcuTexRefSetFormat(CUtexref hTexRef, CUarray_format fmt, int NumPackedComponents);
typedef CUresult CUDAAPI tcuTexRefSetAddressMode(CUtexref hTexRef, int dim, CUaddress_mode am);
typedef CUresult CUDAAPI tcuTexRefSetFilterMode(CUtexref hTexRef, CUfilter_mode fm);
typedef CUresult CUDAAPI tcuTexRefSetMipmapFilterMode(CUtexref hTexRef, CUfilter_mode fm);
typedef CUresult CUDAAPI tcuTexRefSetMipmapLevelBias(CUtexref hTexRef, float bias);
typedef CUresult CUDAAPI tcuTexRefSetMipmapLevelClamp(CUtexref hTexRef, float minMipmapLevelClamp, float maxMipmapLevelClamp);
typedef CUresult CUDAAPI tcuTexRefSetMaxAnisotropy(CUtexref hTexRef, unsigned maxAniso);
typedef CUresult CUDAAPI tcuTexRefSetFlags(CUtexref hTexRef, unsigned Flags);
typedef CUresult CUDAAPI tcuTexRefGetAddress_v2(CUdeviceptr* pdptr, CUtexref hTexRef);
typedef CUresult CUDAAPI tcuTexRefGetArray(CUarray* phArray, CUtexref hTexRef);
typedef CUresult CUDAAPI tcuTexRefGetMipmappedArray(CUmipmappedArray* phMipmappedArray, CUtexref hTexRef);
typedef CUresult CUDAAPI tcuTexRefGetAddressMode(CUaddress_mode* pam, CUtexref hTexRef, int dim);
typedef CUresult CUDAAPI tcuTexRefGetFilterMode(CUfilter_mode* pfm, CUtexref hTexRef);
typedef CUresult CUDAAPI tcuTexRefGetFormat(CUarray_format* pFormat, int* pNumChannels, CUtexref hTexRef);
typedef CUresult CUDAAPI tcuTexRefGetMipmapFilterMode(CUfilter_mode* pfm, CUtexref hTexRef);
typedef CUresult CUDAAPI tcuTexRefGetMipmapLevelBias(float* pbias, CUtexref hTexRef);
typedef CUresult CUDAAPI tcuTexRefGetMipmapLevelClamp(float* pminMipmapLevelClamp, float* pmaxMipmapLevelClamp, CUtexref hTexRef);
typedef CUresult CUDAAPI tcuTexRefGetMaxAnisotropy(int* pmaxAniso, CUtexref hTexRef);
typedef CUresult CUDAAPI tcuTexRefGetFlags(unsigned* pFlags, CUtexref hTexRef);
typedef CUresult CUDAAPI tcuTexRefCreate(CUtexref* pTexRef);
typedef CUresult CUDAAPI tcuTexRefDestroy(CUtexref hTexRef);
typedef CUresult CUDAAPI tcuSurfRefSetArray(CUsurfref hSurfRef, CUarray hArray, unsigned Flags);
typedef CUresult CUDAAPI tcuSurfRefGetArray(CUarray* phArray, CUsurfref hSurfRef);
typedef CUresult CUDAAPI tcuTexObjectCreate(CUtexObject* pTexObject, const CUDA_RESOURCE_DESC* pResDesc, const CUDA_TEXTURE_DESC* pTexDesc, const CUDA_RESOURCE_VIEW_DESC* pResViewDesc);
typedef CUresult CUDAAPI tcuTexObjectDestroy(CUtexObject texObject);
typedef CUresult CUDAAPI tcuTexObjectGetResourceDesc(CUDA_RESOURCE_DESC* pResDesc, CUtexObject texObject);
typedef CUresult CUDAAPI tcuTexObjectGetTextureDesc(CUDA_TEXTURE_DESC* pTexDesc, CUtexObject texObject);
typedef CUresult CUDAAPI tcuTexObjectGetResourceViewDesc(CUDA_RESOURCE_VIEW_DESC* pResViewDesc, CUtexObject texObject);
typedef CUresult CUDAAPI tcuSurfObjectCreate(CUsurfObject* pSurfObject, const CUDA_RESOURCE_DESC* pResDesc);
typedef CUresult CUDAAPI tcuSurfObjectDestroy(CUsurfObject surfObject);
typedef CUresult CUDAAPI tcuSurfObjectGetResourceDesc(CUDA_RESOURCE_DESC* pResDesc, CUsurfObject surfObject);
typedef CUresult CUDAAPI tcuDeviceCanAccessPeer(int* canAccessPeer, CUdevice dev, CUdevice peerDev);
typedef CUresult CUDAAPI tcuCtxEnablePeerAccess(CUcontext peerContext, unsigned Flags);
typedef CUresult CUDAAPI tcuCtxDisablePeerAccess(CUcontext peerContext);
typedef CUresult CUDAAPI tcuGraphicsUnregisterResource(CUgraphicsResource resource);
typedef CUresult CUDAAPI tcuGraphicsSubResourceGetMappedArray(CUarray* pArray, CUgraphicsResource resource, unsigned arrayIndex, unsigned mipLevel);
typedef CUresult CUDAAPI tcuGraphicsResourceGetMappedMipmappedArray(CUmipmappedArray* pMipmappedArray, CUgraphicsResource resource);
typedef CUresult CUDAAPI tcuGraphicsResourceGetMappedPointer_v2(CUdeviceptr* pDevPtr, size_t* pSize, CUgraphicsResource resource);
typedef CUresult CUDAAPI tcuGraphicsResourceSetMapFlags(CUgraphicsResource resource, unsigned flags);
typedef CUresult CUDAAPI tcuGraphicsMapResources(unsigned count, CUgraphicsResource* resources, CUstream hStream);
typedef CUresult CUDAAPI tcuGraphicsUnmapResources(unsigned count, CUgraphicsResource* resources, CUstream hStream);
typedef CUresult CUDAAPI tcuGetExportTable(const void* ppExportTable, const CUuuid* pExportTableId);

typedef CUresult CUDAAPI tcuGraphicsGLRegisterBuffer(CUgraphicsResource* pCudaResource, GLuint buffer, unsigned Flags);
typedef CUresult CUDAAPI tcuGraphicsGLRegisterImage(CUgraphicsResource* pCudaResource, GLuint image, GLenum target, unsigned Flags);
typedef CUresult CUDAAPI tcuGLGetDevices(unsigned* pCudaDeviceCount, CUdevice* pCudaDevices, unsigned cudaDeviceCount, CUGLDeviceList deviceList);
typedef CUresult CUDAAPI tcuGLCtxCreate_v2(CUcontext* pCtx, unsigned Flags, CUdevice device);
typedef CUresult CUDAAPI tcuGLInit(void);
typedef CUresult CUDAAPI tcuGLRegisterBufferObject(GLuint buffer);
typedef CUresult CUDAAPI tcuGLMapBufferObject_v2(CUdeviceptr* dptr, size_t* size, GLuint buffer);
typedef CUresult CUDAAPI tcuGLUnmapBufferObject(GLuint buffer);
typedef CUresult CUDAAPI tcuGLUnregisterBufferObject(GLuint buffer);
typedef CUresult CUDAAPI tcuGLSetBufferObjectMapFlags(GLuint buffer, unsigned Flags);
typedef CUresult CUDAAPI tcuGLMapBufferObjectAsync_v2(CUdeviceptr* dptr, size_t* size, GLuint buffer, CUstream hStream);
typedef CUresult CUDAAPI tcuGLUnmapBufferObjectAsync(GLuint buffer, CUstream hStream);


/* Function declarations. */
extern tcuGetErrorString *cuGetErrorString;
extern tcuGetErrorName *cuGetErrorName;
extern tcuInit *cuInit;
extern tcuDriverGetVersion *cuDriverGetVersion;
extern tcuDeviceGet *cuDeviceGet;
extern tcuDeviceGetCount *cuDeviceGetCount;
extern tcuDeviceGetName *cuDeviceGetName;
extern tcuDeviceTotalMem_v2 *cuDeviceTotalMem_v2;
extern tcuDeviceGetAttribute *cuDeviceGetAttribute;
extern tcuDeviceGetProperties *cuDeviceGetProperties;
extern tcuDeviceComputeCapability *cuDeviceComputeCapability;
extern tcuCtxCreate_v2 *cuCtxCreate_v2;
extern tcuCtxDestroy_v2 *cuCtxDestroy_v2;
extern tcuCtxPushCurrent_v2 *cuCtxPushCurrent_v2;
extern tcuCtxPopCurrent_v2 *cuCtxPopCurrent_v2;
extern tcuCtxSetCurrent *cuCtxSetCurrent;
extern tcuCtxGetCurrent *cuCtxGetCurrent;
extern tcuCtxGetDevice *cuCtxGetDevice;
extern tcuCtxSynchronize *cuCtxSynchronize;
extern tcuCtxSetLimit *cuCtxSetLimit;
extern tcuCtxGetLimit *cuCtxGetLimit;
extern tcuCtxGetCacheConfig *cuCtxGetCacheConfig;
extern tcuCtxSetCacheConfig *cuCtxSetCacheConfig;
extern tcuCtxGetSharedMemConfig *cuCtxGetSharedMemConfig;
extern tcuCtxSetSharedMemConfig *cuCtxSetSharedMemConfig;
extern tcuCtxGetApiVersion *cuCtxGetApiVersion;
extern tcuCtxGetStreamPriorityRange *cuCtxGetStreamPriorityRange;
extern tcuCtxAttach *cuCtxAttach;
extern tcuCtxDetach *cuCtxDetach;
extern tcuModuleLoad *cuModuleLoad;
extern tcuModuleLoadData *cuModuleLoadData;
extern tcuModuleLoadDataEx *cuModuleLoadDataEx;
extern tcuModuleLoadFatBinary *cuModuleLoadFatBinary;
extern tcuModuleUnload *cuModuleUnload;
extern tcuModuleGetFunction *cuModuleGetFunction;
extern tcuModuleGetGlobal_v2 *cuModuleGetGlobal_v2;
extern tcuModuleGetTexRef *cuModuleGetTexRef;
extern tcuModuleGetSurfRef *cuModuleGetSurfRef;
extern tcuLinkCreate *cuLinkCreate;
extern tcuLinkAddData *cuLinkAddData;
extern tcuLinkAddFile *cuLinkAddFile;
extern tcuLinkComplete *cuLinkComplete;
extern tcuLinkDestroy *cuLinkDestroy;
extern tcuMemGetInfo_v2 *cuMemGetInfo_v2;
extern tcuMemAlloc_v2 *cuMemAlloc_v2;
extern tcuMemAllocPitch_v2 *cuMemAllocPitch_v2;
extern tcuMemFree_v2 *cuMemFree_v2;
extern tcuMemGetAddressRange_v2 *cuMemGetAddressRange_v2;
extern tcuMemAllocHost_v2 *cuMemAllocHost_v2;
extern tcuMemFreeHost *cuMemFreeHost;
extern tcuMemHostAlloc *cuMemHostAlloc;
extern tcuMemHostGetDevicePointer_v2 *cuMemHostGetDevicePointer_v2;
extern tcuMemHostGetFlags *cuMemHostGetFlags;
extern tcuMemAllocManaged *cuMemAllocManaged;
extern tcuDeviceGetByPCIBusId *cuDeviceGetByPCIBusId;
extern tcuDeviceGetPCIBusId *cuDeviceGetPCIBusId;
extern tcuIpcGetEventHandle *cuIpcGetEventHandle;
extern tcuIpcOpenEventHandle *cuIpcOpenEventHandle;
extern tcuIpcGetMemHandle *cuIpcGetMemHandle;
extern tcuIpcOpenMemHandle *cuIpcOpenMemHandle;
extern tcuIpcCloseMemHandle *cuIpcCloseMemHandle;
extern tcuMemHostRegister *cuMemHostRegister;
extern tcuMemHostUnregister *cuMemHostUnregister;
extern tcuMemcpy *cuMemcpy;
extern tcuMemcpyPeer *cuMemcpyPeer;
extern tcuMemcpyHtoD_v2 *cuMemcpyHtoD_v2;
extern tcuMemcpyDtoH_v2 *cuMemcpyDtoH_v2;
extern tcuMemcpyDtoD_v2 *cuMemcpyDtoD_v2;
extern tcuMemcpyDtoA_v2 *cuMemcpyDtoA_v2;
extern tcuMemcpyAtoD_v2 *cuMemcpyAtoD_v2;
extern tcuMemcpyHtoA_v2 *cuMemcpyHtoA_v2;
extern tcuMemcpyAtoH_v2 *cuMemcpyAtoH_v2;
extern tcuMemcpyAtoA_v2 *cuMemcpyAtoA_v2;
extern tcuMemcpy2D_v2 *cuMemcpy2D_v2;
extern tcuMemcpy2DUnaligned_v2 *cuMemcpy2DUnaligned_v2;
extern tcuMemcpy3D_v2 *cuMemcpy3D_v2;
extern tcuMemcpy3DPeer *cuMemcpy3DPeer;
extern tcuMemcpyAsync *cuMemcpyAsync;
extern tcuMemcpyPeerAsync *cuMemcpyPeerAsync;
extern tcuMemcpyHtoDAsync_v2 *cuMemcpyHtoDAsync_v2;
extern tcuMemcpyDtoHAsync_v2 *cuMemcpyDtoHAsync_v2;
extern tcuMemcpyDtoDAsync_v2 *cuMemcpyDtoDAsync_v2;
extern tcuMemcpyHtoAAsync_v2 *cuMemcpyHtoAAsync_v2;
extern tcuMemcpyAtoHAsync_v2 *cuMemcpyAtoHAsync_v2;
extern tcuMemcpy2DAsync_v2 *cuMemcpy2DAsync_v2;
extern tcuMemcpy3DAsync_v2 *cuMemcpy3DAsync_v2;
extern tcuMemcpy3DPeerAsync *cuMemcpy3DPeerAsync;
extern tcuMemsetD8_v2 *cuMemsetD8_v2;
extern tcuMemsetD16_v2 *cuMemsetD16_v2;
extern tcuMemsetD32_v2 *cuMemsetD32_v2;
extern tcuMemsetD2D8_v2 *cuMemsetD2D8_v2;
extern tcuMemsetD2D16_v2 *cuMemsetD2D16_v2;
extern tcuMemsetD2D32_v2 *cuMemsetD2D32_v2;
extern tcuMemsetD8Async *cuMemsetD8Async;
extern tcuMemsetD16Async *cuMemsetD16Async;
extern tcuMemsetD32Async *cuMemsetD32Async;
extern tcuMemsetD2D8Async *cuMemsetD2D8Async;
extern tcuMemsetD2D16Async *cuMemsetD2D16Async;
extern tcuMemsetD2D32Async *cuMemsetD2D32Async;
extern tcuArrayCreate_v2 *cuArrayCreate_v2;
extern tcuArrayGetDescriptor_v2 *cuArrayGetDescriptor_v2;
extern tcuArrayDestroy *cuArrayDestroy;
extern tcuArray3DCreate_v2 *cuArray3DCreate_v2;
extern tcuArray3DGetDescriptor_v2 *cuArray3DGetDescriptor_v2;
extern tcuMipmappedArrayCreate *cuMipmappedArrayCreate;
extern tcuMipmappedArrayGetLevel *cuMipmappedArrayGetLevel;
extern tcuMipmappedArrayDestroy *cuMipmappedArrayDestroy;
extern tcuPointerGetAttribute *cuPointerGetAttribute;
extern tcuPointerSetAttribute *cuPointerSetAttribute;
extern tcuStreamCreate *cuStreamCreate;
extern tcuStreamCreateWithPriority *cuStreamCreateWithPriority;
extern tcuStreamGetPriority *cuStreamGetPriority;
extern tcuStreamGetFlags *cuStreamGetFlags;
extern tcuStreamWaitEvent *cuStreamWaitEvent;
extern tcuStreamAddCallback *cuStreamAddCallback;
extern tcuStreamAttachMemAsync *cuStreamAttachMemAsync;
extern tcuStreamQuery *cuStreamQuery;
extern tcuStreamSynchronize *cuStreamSynchronize;
extern tcuStreamDestroy_v2 *cuStreamDestroy_v2;
extern tcuEventCreate *cuEventCreate;
extern tcuEventRecord *cuEventRecord;
extern tcuEventQuery *cuEventQuery;
extern tcuEventSynchronize *cuEventSynchronize;
extern tcuEventDestroy_v2 *cuEventDestroy_v2;
extern tcuEventElapsedTime *cuEventElapsedTime;
extern tcuFuncGetAttribute *cuFuncGetAttribute;
extern tcuFuncSetCacheConfig *cuFuncSetCacheConfig;
extern tcuFuncSetSharedMemConfig *cuFuncSetSharedMemConfig;
extern tcuLaunchKernel *cuLaunchKernel;
extern tcuFuncSetBlockShape *cuFuncSetBlockShape;
extern tcuFuncSetSharedSize *cuFuncSetSharedSize;
extern tcuParamSetSize *cuParamSetSize;
extern tcuParamSeti *cuParamSeti;
extern tcuParamSetf *cuParamSetf;
extern tcuParamSetv *cuParamSetv;
extern tcuLaunch *cuLaunch;
extern tcuLaunchGrid *cuLaunchGrid;
extern tcuLaunchGridAsync *cuLaunchGridAsync;
extern tcuParamSetTexRef *cuParamSetTexRef;
extern tcuTexRefSetArray *cuTexRefSetArray;
extern tcuTexRefSetMipmappedArray *cuTexRefSetMipmappedArray;
extern tcuTexRefSetAddress_v2 *cuTexRefSetAddress_v2;
extern tcuTexRefSetAddress2D_v3 *cuTexRefSetAddress2D_v3;
extern tcuTexRefSetFormat *cuTexRefSetFormat;
extern tcuTexRefSetAddressMode *cuTexRefSetAddressMode;
extern tcuTexRefSetFilterMode *cuTexRefSetFilterMode;
extern tcuTexRefSetMipmapFilterMode *cuTexRefSetMipmapFilterMode;
extern tcuTexRefSetMipmapLevelBias *cuTexRefSetMipmapLevelBias;
extern tcuTexRefSetMipmapLevelClamp *cuTexRefSetMipmapLevelClamp;
extern tcuTexRefSetMaxAnisotropy *cuTexRefSetMaxAnisotropy;
extern tcuTexRefSetFlags *cuTexRefSetFlags;
extern tcuTexRefGetAddress_v2 *cuTexRefGetAddress_v2;
extern tcuTexRefGetArray *cuTexRefGetArray;
extern tcuTexRefGetMipmappedArray *cuTexRefGetMipmappedArray;
extern tcuTexRefGetAddressMode *cuTexRefGetAddressMode;
extern tcuTexRefGetFilterMode *cuTexRefGetFilterMode;
extern tcuTexRefGetFormat *cuTexRefGetFormat;
extern tcuTexRefGetMipmapFilterMode *cuTexRefGetMipmapFilterMode;
extern tcuTexRefGetMipmapLevelBias *cuTexRefGetMipmapLevelBias;
extern tcuTexRefGetMipmapLevelClamp *cuTexRefGetMipmapLevelClamp;
extern tcuTexRefGetMaxAnisotropy *cuTexRefGetMaxAnisotropy;
extern tcuTexRefGetFlags *cuTexRefGetFlags;
extern tcuTexRefCreate *cuTexRefCreate;
extern tcuTexRefDestroy *cuTexRefDestroy;
extern tcuSurfRefSetArray *cuSurfRefSetArray;
extern tcuSurfRefGetArray *cuSurfRefGetArray;
extern tcuTexObjectCreate *cuTexObjectCreate;
extern tcuTexObjectDestroy *cuTexObjectDestroy;
extern tcuTexObjectGetResourceDesc *cuTexObjectGetResourceDesc;
extern tcuTexObjectGetTextureDesc *cuTexObjectGetTextureDesc;
extern tcuTexObjectGetResourceViewDesc *cuTexObjectGetResourceViewDesc;
extern tcuSurfObjectCreate *cuSurfObjectCreate;
extern tcuSurfObjectDestroy *cuSurfObjectDestroy;
extern tcuSurfObjectGetResourceDesc *cuSurfObjectGetResourceDesc;
extern tcuDeviceCanAccessPeer *cuDeviceCanAccessPeer;
extern tcuCtxEnablePeerAccess *cuCtxEnablePeerAccess;
extern tcuCtxDisablePeerAccess *cuCtxDisablePeerAccess;
extern tcuGraphicsUnregisterResource *cuGraphicsUnregisterResource;
extern tcuGraphicsSubResourceGetMappedArray *cuGraphicsSubResourceGetMappedArray;
extern tcuGraphicsResourceGetMappedMipmappedArray *cuGraphicsResourceGetMappedMipmappedArray;
extern tcuGraphicsResourceGetMappedPointer_v2 *cuGraphicsResourceGetMappedPointer_v2;
extern tcuGraphicsResourceSetMapFlags *cuGraphicsResourceSetMapFlags;
extern tcuGraphicsMapResources *cuGraphicsMapResources;
extern tcuGraphicsUnmapResources *cuGraphicsUnmapResources;
extern tcuGetExportTable *cuGetExportTable;

extern tcuGraphicsGLRegisterBuffer *cuGraphicsGLRegisterBuffer;
extern tcuGraphicsGLRegisterImage *cuGraphicsGLRegisterImage;
extern tcuGLGetDevices *cuGLGetDevices;
extern tcuGLCtxCreate_v2 *cuGLCtxCreate_v2;
extern tcuGLInit *cuGLInit;
extern tcuGLRegisterBufferObject *cuGLRegisterBufferObject;
extern tcuGLMapBufferObject_v2 *cuGLMapBufferObject_v2;
extern tcuGLUnmapBufferObject *cuGLUnmapBufferObject;
extern tcuGLUnregisterBufferObject *cuGLUnregisterBufferObject;
extern tcuGLSetBufferObjectMapFlags *cuGLSetBufferObjectMapFlags;
extern tcuGLMapBufferObjectAsync_v2 *cuGLMapBufferObjectAsync_v2;
extern tcuGLUnmapBufferObjectAsync *cuGLUnmapBufferObjectAsync;


enum {
  CUEW_SUCCESS = 0,
  CUEW_ERROR_OPEN_FAILED = -1,
  CUEW_ERROR_ATEXIT_FAILED = -2,
};

int cuewInit(void);
const char *cuewErrorString(CUresult result);
const char *cuewCompilerPath(void);
int cuewCompilerVersion(void);

#ifdef __cplusplus
}
#endif

#endif  /* __CUEW_H__ */
