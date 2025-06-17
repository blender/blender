/*
 * Copyright 2011-2021 Blender Foundation
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

#ifndef __HIPEW_H__
#define __HIPEW_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>


#define HIP_IPC_HANDLE_SIZE 64
#define hipHostMallocDefault 0x00
#define hipHostMallocPortable 0x01
#define hipHostMallocMapped 0x02
#define hipHostMallocWriteCombined 0x04
#define hipHostMallocNumaUser 0x20000000
#define hipHostMallocCoherent 0x40000000
#define hipHostMallocNonCoherent 0x80000000
#define hipHostRegisterPortable 0x01
#define hipHostRegisterMapped 0x02
#define hipHostRegisterIoMemory 0x04
#define hipCooperativeLaunchMultiDeviceNoPreSync 0x01
#define hipCooperativeLaunchMultiDeviceNoPostSync 0x02
#define hipArrayLayered 0x01
#define hipArraySurfaceLoadStore 0x02
#define hipArrayCubemap 0x04
#define hipArrayTextureGather 0x08
#define HIP_TRSA_OVERRIDE_FORMAT 0x01
#define HIP_TRSF_READ_AS_INTEGER 0x01
#define HIP_TRSF_NORMALIZED_COORDINATES 0x02
#define HIP_LAUNCH_PARAM_BUFFER_POINTER ((void*)0x01)
#define HIP_LAUNCH_PARAM_BUFFER_SIZE ((void*)0x02)
#define HIP_LAUNCH_PARAM_END ((void*)0x03)

/* Functions which changed 3.1 -> 3.2 for 64 bit stuff,
 * the cuda library has both the old ones for compatibility and new
 * ones with _v2 postfix,
 */
#define hipModuleGetGlobal hipModuleGetGlobal
#define hipMemGetInfo hipMemGetInfo
#define hipMemAllocPitch hipMemAllocPitch
#define hipMemGetAddressRange hipMemGetAddressRange
#define hipMemcpy hipMemcpy
#define hipMemcpyHtoD hipMemcpyHtoD
#define hipMemcpyDtoH hipMemcpyDtoH
#define hipMemcpyDtoD hipMemcpyDtoD
#define hipMemcpyHtoA hipMemcpyHtoA
#define hipMemcpyAtoH hipMemcpyAtoH
#define hipMemcpyHtoDAsync hipMemcpyHtoDAsync
#define hipMemcpyDtoHAsync hipMemcpyDtoHAsync
#define hipMemcpyDtoDAsync hipMemcpyDtoDAsync
#define hipMemsetD8 hipMemsetD8
#define hipMemsetD16 hipMemsetD16
#define hipMemsetD32 hipMemsetD32
#define hipArrayCreate hipArrayCreate
#define hipArray3DCreate hipArray3DCreate
#define hipPointerGetAttributes hipPointerGetAttributes
#define hipTexRefSetAddress hipTexRefSetAddress
#define hipTexRefGetAddress hipTexRefGetAddress
#define hipStreamDestroy hipStreamDestroy
#define hipEventDestroy hipEventDestroy
#define hipTexRefSetAddress2D hipTexRefSetAddress2D

/* Types. */
#ifdef _MSC_VER
typedef unsigned __int32 hipuint32_t;
typedef unsigned __int64 hipuint64_t;
#else
#include <stdint.h>
typedef uint32_t hipuint32_t;
typedef uint64_t hipuint64_t;
#endif

#if defined(__x86_64) || defined(AMD64) || defined(_M_AMD64) || defined (__aarch64__) || defined(_M_ARM64) || defined(__ppc64__) || defined(__PPC64__) || defined(__LP64__)
typedef unsigned long long hipDeviceptr_t;
#else
typedef unsigned int hipDeviceptr_t;
#endif


#ifdef _WIN32
#  define HIPAPI __stdcall
#  define HIP_CB __stdcall
#else
#  define HIPAPI
#  define HIP_CB
#endif

typedef int hipDevice_t;
typedef struct ihipCtx_t* hipCtx_t;
typedef struct ihipModule_t* hipModule_t;
typedef struct ihipModuleSymbol_t* hipFunction_t;
typedef struct hipArray* hArray;
typedef struct hipMipmappedArray_st* hipMipmappedArray_t;
typedef struct ihipEvent_t* hipEvent_t;
typedef struct ihipStream_t* hipStream_t;
typedef unsigned long long hipTextureObject_t;
typedef void* hipExternalMemory_t;

typedef struct HIPuuid_st {
  char bytes[16];
} HIPuuid;


typedef enum hipChannelFormatKind {
    hipChannelFormatKindSigned = 0,
    hipChannelFormatKindUnsigned = 1,
    hipChannelFormatKindFloat = 2,
    hipChannelFormatKindNone = 3,
}hipChannelFormatKind;

typedef struct hipChannelFormatDesc {
    int x;
    int y;
    int z;
    int w;
    enum hipChannelFormatKind f;
}hipChannelFormatDesc;

typedef enum hipTextureFilterMode {
  hipFilterModePoint = 0,
  hipFilterModeLinear = 1,
} hipTextureFilterMode;

typedef enum hipArray_Format {
  HIP_AD_FORMAT_UNSIGNED_INT8 = 0x01,
  HIP_AD_FORMAT_SIGNED_INT8 = 0x08,
  HIP_AD_FORMAT_UNSIGNED_INT16 = 0x02,
  HIP_AD_FORMAT_SIGNED_INT16 = 0x09,
  HIP_AD_FORMAT_UNSIGNED_INT32 = 0x03,
  HIP_AD_FORMAT_SIGNED_INT32 = 0x0a,
  HIP_AD_FORMAT_HALF = 0x10,
  HIP_AD_FORMAT_FLOAT = 0x20,
} hipArray_Format;

typedef enum hipTextureAddressMode {
  hipAddressModeWrap = 0,
  hipAddressModeClamp = 1,
  hipAddressModeMirror = 2,
  hipAddressModeBorder = 3,
} hipTextureAddressMode;

/**
 * hip texture reference
 */
typedef struct textureReference {
    int normalized;
    //enum hipTextureReadMode readMode;// used only for driver API's //why is this commentend out?
    enum hipTextureFilterMode filterMode;
    enum hipTextureAddressMode addressMode[3];  // Texture address mode for up to 3 dimensions
    struct hipChannelFormatDesc channelDesc;
    int sRGB;                    // Perform sRGB->linear conversion during texture read
    unsigned int maxAnisotropy;  // Limit to the anisotropy ratio
    enum hipTextureFilterMode mipmapFilterMode;
    float mipmapLevelBias;
    float minMipmapLevelClamp;
    float maxMipmapLevelClamp;

    hipTextureObject_t textureObject;
    int numChannels;
    enum hipArray_Format format;
}textureReference;

typedef textureReference* hipTexRef;

typedef struct ihipIpcEventHandle_t {
  char reserved[HIP_IPC_HANDLE_SIZE];
} ihipIpcEventHandle_t;

typedef struct hipIpcMemHandle_st {
  char reserved[HIP_IPC_HANDLE_SIZE];
} hipIpcMemHandle_t;

typedef enum HIPipcMem_flags_enum {
  hipIpcMemLazyEnablePeerAccess = 0x1,
} HIPipcMem_flags;

typedef enum HIPmemAttach_flags_enum {
  hipMemAttachGlobal = 0x1,
  hipMemAttachHost = 0x2,
  HIP_MEM_ATTACH_SINGLE = 0x4,
} HIPmemAttach_flags;

typedef enum HIPctx_flags_enum {
  hipDeviceScheduleAuto = 0x00,
  hipDeviceScheduleSpin = 0x01,
  hipDeviceScheduleYield = 0x02,
  hipDeviceScheduleBlockingSync = 0x04,
  hipDeviceScheduleMask = 0x07,
  hipDeviceMapHost = 0x08,
  hipDeviceLmemResizeToMax = 0x10,
} HIPctx_flags;

typedef enum HIPstream_flags_enum {
  hipStreamDefault = 0x0,
  hipStreamNonBlocking = 0x1,
} HIPstream_flags;

typedef enum HIPevent_flags_enum {
  hipEventDefault = 0x0,
  hipEventBlockingSync = 0x1,
  hipEventDisableTiming = 0x2,
  hipEventInterprocess = 0x4,
} HIPevent_flags;

typedef enum HIPstreamWaitValue_flags_enum {
  HIP_STREAM_WAIT_VALUE_GEQ = 0x0,
  HIP_STREAM_WAIT_VALUE_EQ = 0x1,
  HIP_STREAM_WAIT_VALUE_AND = 0x2,
  HIP_STREAM_WAIT_VALUE_NOR = 0x3,
  HIP_STREAM_WAIT_VALUE_FLUSH = (1 << 30),
} HIPstreamWaitValue_flags;

typedef enum HIPstreamWriteValue_flags_enum {
  HIP_STREAM_WRITE_VALUE_DEFAULT = 0x0,
  HIP_STREAM_WRITE_VALUE_NO_MEMORY_BARRIER = 0x1,
} HIPstreamWriteValue_flags;

typedef enum HIPstreamBatchMemOpType_enum {
  HIP_STREAM_MEM_OP_WAIT_VALUE_32 = 1,
  HIP_STREAM_MEM_OP_WRITE_VALUE_32 = 2,
  HIP_STREAM_MEM_OP_WAIT_VALUE_64 = 4,
  HIP_STREAM_MEM_OP_WRITE_VALUE_64 = 5,
  HIP_STREAM_MEM_OP_FLUSH_REMOTE_WRITES = 3,
} HIPstreamBatchMemOpType;


typedef union HIPstreamBatchMemOpParams_union {
  HIPstreamBatchMemOpType operation;
  struct HIPstreamMemOpWaitValueParams_st {
    HIPstreamBatchMemOpType operation;
    hipDeviceptr_t address;
    union {
      hipuint32_t value;
      hipuint64_t value64;
    };
    unsigned int flags;
    hipDeviceptr_t alias;
  } waitValue;
  struct HIPstreamMemOpWriteValueParams_st {
    HIPstreamBatchMemOpType operation;
    hipDeviceptr_t address;
    union {
      hipuint32_t value;
      hipuint64_t value64;
    };
    unsigned int flags;
    hipDeviceptr_t alias;
  } writeValue;
  struct HIPstreamMemOpFlushRemoteWritesParams_st {
    HIPstreamBatchMemOpType operation;
    unsigned int flags;
  } flushRemoteWrites;
  hipuint64_t pad[6];
} HIPstreamBatchMemOpParams;

typedef enum HIPoccupancy_flags_enum {
  hipOccupancyDefault = 0x0,
  HIP_OCCUPANCY_DISABLE_CACHING_OVERRIDE = 0x1,
} HIPoccupancy_flags;


typedef struct HIPdevprop_st {
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
} HIPdevprop;

typedef struct {
    // 32-bit Atomics
    unsigned hasGlobalInt32Atomics : 1;     ///< 32-bit integer atomics for global memory.
    unsigned hasGlobalFloatAtomicExch : 1;  ///< 32-bit float atomic exch for global memory.
    unsigned hasSharedInt32Atomics : 1;     ///< 32-bit integer atomics for shared memory.
    unsigned hasSharedFloatAtomicExch : 1;  ///< 32-bit float atomic exch for shared memory.
    unsigned hasFloatAtomicAdd : 1;  ///< 32-bit float atomic add in global and shared memory.

    // 64-bit Atomics
    unsigned hasGlobalInt64Atomics : 1;  ///< 64-bit integer atomics for global memory.
    unsigned hasSharedInt64Atomics : 1;  ///< 64-bit integer atomics for shared memory.

    // Doubles
    unsigned hasDoubles : 1;  ///< Double-precision floating point.

    // Warp cross-lane operations
    unsigned hasWarpVote : 1;     ///< Warp vote instructions (__any, __all).
    unsigned hasWarpBallot : 1;   ///< Warp ballot instructions (__ballot).
    unsigned hasWarpShuffle : 1;  ///< Warp shuffle operations. (__shfl_*).
    unsigned hasFunnelShift : 1;  ///< Funnel two words into one with shift&mask caps.

    // Sync
    unsigned hasThreadFenceSystem : 1;  ///< __threadfence_system.
    unsigned hasSyncThreadsExt : 1;     ///< __syncthreads_count, syncthreads_and, syncthreads_or.

    // Misc
    unsigned hasSurfaceFuncs : 1;        ///< Surface functions.
    unsigned has3dGrid : 1;              ///< Grid and group dims are 3D (rather than 2D).
    unsigned hasDynamicParallelism : 1;  ///< Dynamic parallelism.
} hipDeviceArch_t;


typedef enum hipFunction_attribute {
  HIP_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK = 0,
  HIP_FUNC_ATTRIBUTE_SHARED_SIZE_BYTES = 1,
  HIP_FUNC_ATTRIBUTE_CONST_SIZE_BYTES = 2,
  HIP_FUNC_ATTRIBUTE_LOCAL_SIZE_BYTES = 3,
  HIP_FUNC_ATTRIBUTE_NUM_REGS = 4,
  HIP_FUNC_ATTRIBUTE_PTX_VERSION = 5,
  HIP_FUNC_ATTRIBUTE_BINARY_VERSION = 6,
  HIP_FUNC_ATTRIBUTE_CACHE_MODE_CA = 7,
  HIP_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES = 8,
  HIP_FUNC_ATTRIBUTE_PREFERRED_SHARED_MEMORY_CARVEOUT = 9,
  HIP_FUNC_ATTRIBUTE_MAX,
} hipFunction_attribute;

typedef enum hipFuncCache_t {
  hipFuncCachePreferNone = 0x00,
  hipFuncCachePreferShared = 0x01,
  hipFuncCachePreferL1 = 0x02,
  hipFuncCachePreferEqual = 0x03,
} hipFuncCache_t;

typedef enum hipSharedMemConfig {
  hipSharedMemBankSizeDefault = 0x00,
  hipSharedMemBankSizeFourByte = 0x01,
  hipSharedMemBankSizeEightByte = 0x02,
} hipSharedMemConfig;

typedef enum HIPshared_carveout_enum {
  HIP_SHAREDMEM_CARVEOUT_DEFAULT,
  HIP_SHAREDMEM_CARVEOUT_MAX_SHARED = 100,
  HIP_SHAREDMEM_CARVEOUT_MAX_L1 = 0,
} HIPshared_carveout;

typedef enum HIPmem_advise_enum {
  HIP_MEM_ADVISE_SET_READ_MOSTLY = 1,
  HIP_MEM_ADVISE_UNSET_READ_MOSTLY = 2,
  HIP_MEM_ADVISE_SET_PREFERRED_LOCATION = 3,
  HIP_MEM_ADVISE_UNSET_PREFERRED_LOCATION = 4,
  HIP_MEM_ADVISE_SET_ACCESSED_BY = 5,
  HIP_MEM_ADVISE_UNSET_ACCESSED_BY = 6,
} HIPmem_advise;

typedef enum HIPmem_range_attribute_enum {
  HIP_MEM_RANGE_ATTRIBUTE_READ_MOSTLY = 1,
  HIP_MEM_RANGE_ATTRIBUTE_PREFERRED_LOCATION = 2,
  HIP_MEM_RANGE_ATTRIBUTE_ACCESSED_BY = 3,
  HIP_MEM_RANGE_ATTRIBUTE_LAST_PREFETCH_LOCATION = 4,
} HIPmem_range_attribute;

typedef enum hipJitOption {
  hipJitOptionMaxRegisters = 0,
  hipJitOptionThreadsPerBlock,
  hipJitOptionWallTime,
  hipJitOptionInfoLogBuffer,
  hipJitOptionInfoLogBufferSizeBytes,
  hipJitOptionErrorLogBuffer,
  hipJitOptionErrorLogBufferSizeBytes,
  hipJitOptionOptimizationLevel,
  hipJitOptionTargetFromContext,
  hipJitOptionTarget,
  hipJitOptionFallbackStrategy,
  hipJitOptionGenerateDebugInfo,
  hipJitOptionLogVerbose,
  hipJitOptionGenerateLineInfo,
  hipJitOptionCacheMode,
  hipJitOptionSm3xOpt,
  hipJitOptionFastCompile,
  hipJitOptionNumOptions,
} hipJitOption;

typedef enum HIPjit_target_enum {
  HIP_TARGET_COMPUTE_20 = 20,
  HIP_TARGET_COMPUTE_21 = 21,
  HIP_TARGET_COMPUTE_30 = 30,
  HIP_TARGET_COMPUTE_32 = 32,
  HIP_TARGET_COMPUTE_35 = 35,
  HIP_TARGET_COMPUTE_37 = 37,
  HIP_TARGET_COMPUTE_50 = 50,
  HIP_TARGET_COMPUTE_52 = 52,
  HIP_TARGET_COMPUTE_53 = 53,
  HIP_TARGET_COMPUTE_60 = 60,
  HIP_TARGET_COMPUTE_61 = 61,
  HIP_TARGET_COMPUTE_62 = 62,
  HIP_TARGET_COMPUTE_70 = 70,
  HIP_TARGET_COMPUTE_73 = 73,
  HIP_TARGET_COMPUTE_75 = 75,
} HIPjit_target;

typedef enum HIPjit_fallback_enum {
  HIP_PREFER_PTX = 0,
  HIP_PREFER_BINARY,
} HIPjit_fallback;

typedef enum HIPjit_cacheMode_enum {
  HIP_JIT_CACHE_OPTION_NONE = 0,
  HIP_JIT_CACHE_OPTION_CG,
  HIP_JIT_CACHE_OPTION_CA,
} HIPjit_cacheMode;

typedef enum HIPjitInputType_enum {
  HIP_JIT_INPUT_HIPBIN = 0,
  HIP_JIT_INPUT_PTX,
  HIP_JIT_INPUT_FATBINARY,
  HIP_JIT_INPUT_OBJECT,
  HIP_JIT_INPUT_LIBRARY,
  HIP_JIT_NUM_INPUT_TYPES,
} HIPjitInputType;

typedef struct HIPlinkState_st* HIPlinkState;

typedef enum hipGLDeviceList {
    hipGLDeviceListAll = 1,           ///< All hip devices used by current OpenGL context.
    hipGLDeviceListCurrentFrame = 2,  ///< Hip devices used by current OpenGL context in current
                                    ///< frame
    hipGLDeviceListNextFrame = 3      ///< Hip devices used by current OpenGL context in next
                                    ///< frame.
} hipGLDeviceList;

typedef enum hipGraphicsRegisterFlags {
    hipGraphicsRegisterFlagsNone = 0,
    hipGraphicsRegisterFlagsReadOnly = 1,  ///< HIP will not write to this registered resource
    hipGraphicsRegisterFlagsWriteDiscard =
        2,  ///< HIP will only write and will not read from this registered resource
    hipGraphicsRegisterFlagsSurfaceLoadStore = 4,  ///< HIP will bind this resource to a surface
    hipGraphicsRegisterFlagsTextureGather =
        8  ///< HIP will perform texture gather operations on this registered resource
} hipGraphicsRegisterFlags;

typedef enum HIPgraphicsRegisterFlags_enum {
  HIP_GRAPHICS_REGISTER_FLAGS_NONE = 0x00,
  HIP_GRAPHICS_REGISTER_FLAGS_READ_ONLY = 0x01,
  HIP_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD = 0x02,
  HIP_GRAPHICS_REGISTER_FLAGS_SURFACE_LDST = 0x04,
  HIP_GRAPHICS_REGISTER_FLAGS_TEXTURE_GATHER = 0x08,
} HIPgraphicsRegisterFlags;

typedef enum HIPgraphicsMapResourceFlags_enum {
  HIP_GRAPHICS_MAP_RESOURCE_FLAGS_NONE = 0x00,
  HIP_GRAPHICS_MAP_RESOURCE_FLAGS_READ_ONLY = 0x01,
  HIP_GRAPHICS_MAP_RESOURCE_FLAGS_WRITE_DISCARD = 0x02,
} HIPgraphicsMapResourceFlags;

typedef enum HIParray_cubemap_face_enum {
  HIP_HIPBEMAP_FACE_POSITIVE_X = 0x00,
  HIP_HIPBEMAP_FACE_NEGATIVE_X = 0x01,
  HIP_HIPBEMAP_FACE_POSITIVE_Y = 0x02,
  HIP_HIPBEMAP_FACE_NEGATIVE_Y = 0x03,
  HIP_HIPBEMAP_FACE_POSITIVE_Z = 0x04,
  HIP_HIPBEMAP_FACE_NEGATIVE_Z = 0x05,
} HIParray_cubemap_face;

typedef enum hipLimit_t {
  HIP_LIMIT_STACK_SIZE = 0x00,
  HIP_LIMIT_PRINTF_FIFO_SIZE = 0x01,
  hipLimitMallocHeapSize = 0x02,
  HIP_LIMIT_DEV_RUNTIME_SYNC_DEPTH = 0x03,
  HIP_LIMIT_DEV_RUNTIME_PENDING_LAUNCH_COUNT = 0x04,
  HIP_LIMIT_MAX,
} hipLimit_t;

typedef enum hipResourceType {
  hipResourceTypeArray = 0x00,
  hipResourceTypeMipmappedArray = 0x01,
  hipResourceTypeLinear = 0x02,
  hipResourceTypePitch2D = 0x03,
} hipResourceType;

typedef enum hipError_t {
  hipSuccess = 0,
  hipErrorInvalidValue = 1,
  hipErrorOutOfMemory = 2,
  hipErrorNotInitialized = 3,
  hipErrorDeinitialized = 4,
  hipErrorProfilerDisabled = 5,
  hipErrorProfilerNotInitialized = 6,
  hipErrorProfilerAlreadyStarted = 7,
  hipErrorProfilerAlreadyStopped = 8,
  hipErrorNoDevice = 100,
  hipErrorInvalidDevice = 101,
  hipErrorInvalidImage = 200,
  hipErrorInvalidContext = 201,
  hipErrorContextAlreadyCurrent = 202,
  hipErrorMapFailed = 205,
  hipErrorUnmapFailed = 206,
  hipErrorArrayIsMapped = 207,
  hipErrorAlreadyMapped = 208,
  hipErrorNoBinaryForGpu = 209,
  hipErrorAlreadyAcquired = 210,
  hipErrorNotMapped = 211,
  hipErrorNotMappedAsArray = 212,
  hipErrorNotMappedAsPointer = 213,
  hipErrorECCNotCorrectable = 214,
  hipErrorUnsupportedLimit = 215,
  hipErrorContextAlreadyInUse = 216,
  hipErrorPeerAccessUnsupported = 217,
  hipErrorInvalidKernelFile = 218,
  hipErrorInvalidGraphicsContext = 219,
  hipErrorInvalidSource = 300,
  hipErrorFileNotFound = 301,
  hipErrorSharedObjectSymbolNotFound = 302,
  hipErrorSharedObjectInitFailed = 303,
  hipErrorOperatingSystem = 304,
  hipErrorInvalidHandle = 400,
  hipErrorNotFound = 500,
  hipErrorNotReady = 600,
  hipErrorIllegalAddress = 700,
  hipErrorLaunchOutOfResources = 701,
  hipErrorLaunchTimeOut = 702,
  hipErrorPeerAccessAlreadyEnabled = 704,
  hipErrorPeerAccessNotEnabled = 705,
  hipErrorSetOnActiveProcess = 708,
  hipErrorAssert = 710,
  hipErrorHostMemoryAlreadyRegistered = 712,
  hipErrorHostMemoryNotRegistered = 713,
  hipErrorLaunchFailure = 719,
  hipErrorCooperativeLaunchTooLarge = 720,
  hipErrorNotSupported = 801,
  hipErrorUnknown = 999,
} hipError_t;

/**
 * Stream CallBack struct
 */
typedef void (*hipStreamCallback_t)(hipStream_t stream, hipError_t status, void* userData);

typedef enum HIPdevice_P2PAttribute_enum {
  HIP_DEVICE_P2P_ATTRIBUTE_PERFORMANCE_RANK = 0x01,
  HIP_DEVICE_P2P_ATTRIBUTE_ACCESS_SUPPORTED = 0x02,
  HIP_DEVICE_P2P_ATTRIBUTE_NATIVE_ATOMIC_SUPPORTED = 0x03,
  HIP_DEVICE_P2P_ATTRIBUTE_ARRAY_ACCESS_ACCESS_SUPPORTED = 0x04,
} HIPdevice_P2PAttribute;

typedef struct hipGraphicsResource_st* hipGraphicsResource;

typedef enum hipDeviceP2PAttr {
  hipDevP2PAttrPerformanceRank = 0,
  hipDevP2PAttrAccessSupported,
  hipDevP2PAttrNativeAtomicSupported,
  hipDevP2PAttrHipArrayAccessSupported
} hipDeviceP2PAttr;

typedef struct HIP_ARRAY_DESCRIPTOR {
  size_t Width;
  size_t Height;
  hipArray_Format Format;
  unsigned int NumChannels;
} HIP_ARRAY_DESCRIPTOR;

typedef struct HIP_ARRAY3D_DESCRIPTOR {
  size_t Width;
  size_t Height;
  size_t Depth;
  hipArray_Format Format;
  unsigned int NumChannels;
  unsigned int Flags;
} HIP_ARRAY3D_DESCRIPTOR;

typedef struct HIP_RESOURCE_DESC_st {
  hipResourceType resType;
  union {
    struct {
      hArray h_Array;
    } array;
    struct {
      hipMipmappedArray_t hMipmappedArray;
    } mipmap;
    struct {
      hipDeviceptr_t devPtr;
      hipArray_Format format;
      unsigned int numChannels;
      size_t sizeInBytes;
    } linear;
    struct {
      hipDeviceptr_t devPtr;
      hipArray_Format format;
      unsigned int numChannels;
      size_t width;
      size_t height;
      size_t pitchInBytes;
    } pitch2D;
    struct {
      int reserved[32];
    } reserved;
  } res;
  unsigned int flags;
} hipResourceDesc;

/**
 * hip texture resource view formats
 */
typedef enum hipResourceViewFormat {
    hipResViewFormatNone = 0x00,
    hipResViewFormatUnsignedChar1 = 0x01,
    hipResViewFormatUnsignedChar2 = 0x02,
    hipResViewFormatUnsignedChar4 = 0x03,
    hipResViewFormatSignedChar1 = 0x04,
    hipResViewFormatSignedChar2 = 0x05,
    hipResViewFormatSignedChar4 = 0x06,
    hipResViewFormatUnsignedShort1 = 0x07,
    hipResViewFormatUnsignedShort2 = 0x08,
    hipResViewFormatUnsignedShort4 = 0x09,
    hipResViewFormatSignedShort1 = 0x0a,
    hipResViewFormatSignedShort2 = 0x0b,
    hipResViewFormatSignedShort4 = 0x0c,
    hipResViewFormatUnsignedInt1 = 0x0d,
    hipResViewFormatUnsignedInt2 = 0x0e,
    hipResViewFormatUnsignedInt4 = 0x0f,
    hipResViewFormatSignedInt1 = 0x10,
    hipResViewFormatSignedInt2 = 0x11,
    hipResViewFormatSignedInt4 = 0x12,
    hipResViewFormatHalf1 = 0x13,
    hipResViewFormatHalf2 = 0x14,
    hipResViewFormatHalf4 = 0x15,
    hipResViewFormatFloat1 = 0x16,
    hipResViewFormatFloat2 = 0x17,
    hipResViewFormatFloat4 = 0x18,
    hipResViewFormatUnsignedBlockCompressed1 = 0x19,
    hipResViewFormatUnsignedBlockCompressed2 = 0x1a,
    hipResViewFormatUnsignedBlockCompressed3 = 0x1b,
    hipResViewFormatUnsignedBlockCompressed4 = 0x1c,
    hipResViewFormatSignedBlockCompressed4 = 0x1d,
    hipResViewFormatUnsignedBlockCompressed5 = 0x1e,
    hipResViewFormatSignedBlockCompressed5 = 0x1f,
    hipResViewFormatUnsignedBlockCompressed6H = 0x20,
    hipResViewFormatSignedBlockCompressed6H = 0x21,
    hipResViewFormatUnsignedBlockCompressed7 = 0x22
}hipResourceViewFormat;

typedef enum HIPresourceViewFormat_enum
{
    HIP_RES_VIEW_FORMAT_NONE          = 0x00, /**< No resource view format (use underlying resource format) */
    HIP_RES_VIEW_FORMAT_UINT_1X8      = 0x01, /**< 1 channel unsigned 8-bit integers */
    HIP_RES_VIEW_FORMAT_UINT_2X8      = 0x02, /**< 2 channel unsigned 8-bit integers */
    HIP_RES_VIEW_FORMAT_UINT_4X8      = 0x03, /**< 4 channel unsigned 8-bit integers */
    HIP_RES_VIEW_FORMAT_SINT_1X8      = 0x04, /**< 1 channel signed 8-bit integers */
    HIP_RES_VIEW_FORMAT_SINT_2X8      = 0x05, /**< 2 channel signed 8-bit integers */
    HIP_RES_VIEW_FORMAT_SINT_4X8      = 0x06, /**< 4 channel signed 8-bit integers */
    HIP_RES_VIEW_FORMAT_UINT_1X16     = 0x07, /**< 1 channel unsigned 16-bit integers */
    HIP_RES_VIEW_FORMAT_UINT_2X16     = 0x08, /**< 2 channel unsigned 16-bit integers */
    HIP_RES_VIEW_FORMAT_UINT_4X16     = 0x09, /**< 4 channel unsigned 16-bit integers */
    HIP_RES_VIEW_FORMAT_SINT_1X16     = 0x0a, /**< 1 channel signed 16-bit integers */
    HIP_RES_VIEW_FORMAT_SINT_2X16     = 0x0b, /**< 2 channel signed 16-bit integers */
    HIP_RES_VIEW_FORMAT_SINT_4X16     = 0x0c, /**< 4 channel signed 16-bit integers */
    HIP_RES_VIEW_FORMAT_UINT_1X32     = 0x0d, /**< 1 channel unsigned 32-bit integers */
    HIP_RES_VIEW_FORMAT_UINT_2X32     = 0x0e, /**< 2 channel unsigned 32-bit integers */
    HIP_RES_VIEW_FORMAT_UINT_4X32     = 0x0f, /**< 4 channel unsigned 32-bit integers */
    HIP_RES_VIEW_FORMAT_SINT_1X32     = 0x10, /**< 1 channel signed 32-bit integers */
    HIP_RES_VIEW_FORMAT_SINT_2X32     = 0x11, /**< 2 channel signed 32-bit integers */
    HIP_RES_VIEW_FORMAT_SINT_4X32     = 0x12, /**< 4 channel signed 32-bit integers */
    HIP_RES_VIEW_FORMAT_FLOAT_1X16    = 0x13, /**< 1 channel 16-bit floating point */
    HIP_RES_VIEW_FORMAT_FLOAT_2X16    = 0x14, /**< 2 channel 16-bit floating point */
    HIP_RES_VIEW_FORMAT_FLOAT_4X16    = 0x15, /**< 4 channel 16-bit floating point */
    HIP_RES_VIEW_FORMAT_FLOAT_1X32    = 0x16, /**< 1 channel 32-bit floating point */
    HIP_RES_VIEW_FORMAT_FLOAT_2X32    = 0x17, /**< 2 channel 32-bit floating point */
    HIP_RES_VIEW_FORMAT_FLOAT_4X32    = 0x18, /**< 4 channel 32-bit floating point */
    HIP_RES_VIEW_FORMAT_UNSIGNED_BC1  = 0x19, /**< Block compressed 1 */
    HIP_RES_VIEW_FORMAT_UNSIGNED_BC2  = 0x1a, /**< Block compressed 2 */
    HIP_RES_VIEW_FORMAT_UNSIGNED_BC3  = 0x1b, /**< Block compressed 3 */
    HIP_RES_VIEW_FORMAT_UNSIGNED_BC4  = 0x1c, /**< Block compressed 4 unsigned */
    HIP_RES_VIEW_FORMAT_SIGNED_BC4    = 0x1d, /**< Block compressed 4 signed */
    HIP_RES_VIEW_FORMAT_UNSIGNED_BC5  = 0x1e, /**< Block compressed 5 unsigned */
    HIP_RES_VIEW_FORMAT_SIGNED_BC5    = 0x1f, /**< Block compressed 5 signed */
    HIP_RES_VIEW_FORMAT_UNSIGNED_BC6H = 0x20, /**< Block compressed 6 unsigned half-float */
    HIP_RES_VIEW_FORMAT_SIGNED_BC6H   = 0x21, /**< Block compressed 6 signed half-float */
    HIP_RES_VIEW_FORMAT_UNSIGNED_BC7  = 0x22  /**< Block compressed 7 */
} HIPresourceViewFormat;

/**
 * hip resource view descriptor
 */
struct hipResourceViewDesc {
    enum hipResourceViewFormat format;
    size_t width;
    size_t height;
    size_t depth;
    unsigned int firstMipmapLevel;
    unsigned int lastMipmapLevel;
    unsigned int firstLayer;
    unsigned int lastLayer;
};


typedef struct hipTextureDesc_st {
  hipTextureAddressMode addressMode[3];
  hipTextureFilterMode filterMode;
  unsigned int flags;
  unsigned int maxAnisotropy;
  hipTextureFilterMode mipmapFilterMode;
  float mipmapLevelBias;
  float minMipmapLevelClamp;
  float maxMipmapLevelClamp;
  float borderColor[4];
  int reserved[12];
} hipTextureDesc;

/**
 * Resource view descriptor
 */
typedef struct HIP_RESOURCE_VIEW_DESC_st {
  hipResourceViewFormat format;
  size_t width;
  size_t height;
  size_t depth;
  unsigned int firstMipmapLevel;
  unsigned int lastMipmapLevel;
  unsigned int firstLayer;
  unsigned int lastLayer;
  unsigned int reserved[16];
} HIP_RESOURCE_VIEW_DESC;

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;

typedef enum HIPGLDeviceList_enum {
  HIP_GL_DEVICE_LIST_ALL = 0x01,
  HIP_GL_DEVICE_LIST_CURRENT_FRAME = 0x02,
  HIP_GL_DEVICE_LIST_NEXT_FRAME = 0x03,
} HIPGLDeviceList;

typedef enum HIPGLmap_flags_enum {
  HIP_GL_MAP_RESOURCE_FLAGS_NONE = 0x00,
  HIP_GL_MAP_RESOURCE_FLAGS_READ_ONLY = 0x01,
  HIP_GL_MAP_RESOURCE_FLAGS_WRITE_DISCARD = 0x02,
} HIPGLmap_flags;


/**
* hipRTC related
*/
typedef struct _hiprtcProgram* hiprtcProgram;

typedef enum hiprtcResult {
	  HIPRTC_SUCCESS = 0,
	  HIPRTC_ERROR_OUT_OF_MEMORY = 1,
	  HIPRTC_ERROR_PROGRAM_CREATION_FAILURE = 2,
	  HIPRTC_ERROR_INVALID_INPUT = 3,
	  HIPRTC_ERROR_INVALID_PROGRAM = 4,
	  HIPRTC_ERROR_INVALID_OPTION = 5,
	  HIPRTC_ERROR_COMPILATION = 6,
	  HIPRTC_ERROR_BUILTIN_OPERATION_FAILURE = 7,
	  HIPRTC_ERROR_NO_NAME_EXPRESSIONS_AFTER_COMPILATION = 8,
	  HIPRTC_ERROR_NO_LOWERED_NAMES_BEFORE_COMPILATION = 9,
	  HIPRTC_ERROR_NAME_EXPRESSION_NOT_VALID = 10,
	  HIPRTC_ERROR_INTERNAL_ERROR = 11,
	  HIPRTC_ERROR_LINKING = 100
} hiprtcResult;

typedef enum hiprtcJIT_option {
	HIPRTC_JIT_MAX_REGISTERS = 0,
	HIPRTC_JIT_THREADS_PER_BLOCK,
	HIPRTC_JIT_WALL_TIME,
	HIPRTC_JIT_INFO_LOG_BUFFER,
	HIPRTC_JIT_INFO_LOG_BUFFER_SIZE_BYTES,
	HIPRTC_JIT_ERROR_LOG_BUFFER,
	HIPRTC_JIT_ERROR_LOG_BUFFER_SIZE_BYTES,
	HIPRTC_JIT_OPTIMIZATION_LEVEL,
	HIPRTC_JIT_TARGET_FROM_HIPCONTEXT,
	HIPRTC_JIT_TARGET,
	HIPRTC_JIT_FALLBACK_STRATEGY,
	HIPRTC_JIT_GENERATE_DEBUG_INFO,
	HIPRTC_JIT_LOG_VERBOSE,
	HIPRTC_JIT_GENERATE_LINE_INFO,
	HIPRTC_JIT_CACHE_MODE,
	HIPRTC_JIT_NEW_SM3X_OPT,
	HIPRTC_JIT_FAST_COMPILE,
	HIPRTC_JIT_GLOBAL_SYMBOL_NAMES,
	HIPRTC_JIT_GLOBAL_SYMBOL_ADDRESS,
	HIPRTC_JIT_GLOBAL_SYMBOL_COUNT,
	HIPRTC_JIT_LTO,
	HIPRTC_JIT_FTZ,
	HIPRTC_JIT_PREC_DIV,
	HIPRTC_JIT_PREC_SQRT,
	HIPRTC_JIT_FMA,
	HIPRTC_JIT_NUM_OPTIONS,
} hiprtcJIT_option;

typedef enum hiprtcJITInputType {
	HIPRTC_JIT_INPUT_CUBIN = 0,
	HIPRTC_JIT_INPUT_PTX,
	HIPRTC_JIT_INPUT_FATBINARY,
	HIPRTC_JIT_INPUT_OBJECT,
	HIPRTC_JIT_INPUT_LIBRARY,
	HIPRTC_JIT_INPUT_NVVM,
	HIPRTC_JIT_NUM_LEGACY_INPUT_TYPES,
	HIPRTC_JIT_INPUT_LLVM_BITCODE = 100,
	HIPRTC_JIT_INPUT_LLVM_BUNDLED_BITCODE = 101,
	HIPRTC_JIT_INPUT_LLVM_ARCHIVES_OF_BUNDLED_BITCODE = 102,
	HIPRTC_JIT_NUM_INPUT_TYPES = ( HIPRTC_JIT_NUM_LEGACY_INPUT_TYPES + 3 )
} hiprtcJITInputType;

#ifdef WITH_HIP_SDK_5
#include "hipew5.h"
#else
#include "hipew6.h"
#endif

/**
 * Pointer attributes
 */
typedef struct hipPointerAttribute_t {
    enum hipMemoryType memoryType;
    int device;
    void* devicePointer;
    void* hostPointer;
    int isManaged;
    unsigned allocationFlags; /* flags specified when memory was allocated*/
    /* peers? */
} hipPointerAttribute_t;

typedef struct hip_Memcpy2D {
  size_t srcXInBytes;
  size_t srcY;
  hipMemoryType srcMemoryType;
  const void* srcHost;
  hipDeviceptr_t srcDevice;
  hArray * srcArray;
  size_t srcPitch;
  size_t dstXInBytes;
  size_t dstY;
  hipMemoryType dstMemoryType;
  void* dstHost;
  hipDeviceptr_t dstDevice;
  hArray * dstArray;
  size_t dstPitch;
  size_t WidthInBytes;
  size_t Height;
} hip_Memcpy2D;

typedef struct HIP_MEMCPY3D_PEER_st {
  size_t srcXInBytes;
  size_t srcY;
  size_t srcZ;
  size_t srcLOD;
  hipMemoryType srcMemoryType;
  const void* srcHost;
  hipDeviceptr_t srcDevice;
  hArray * srcArray;
  hipCtx_t srcContext;
  size_t srcPitch;
  size_t srcHeight;
  size_t dstXInBytes;
  size_t dstY;
  size_t dstZ;
  size_t dstLOD;
  hipMemoryType dstMemoryType;
  void* dstHost;
  hipDeviceptr_t dstDevice;
  hArray * dstArray;
  hipCtx_t dstContext;
  size_t dstPitch;
  size_t dstHeight;
  size_t WidthInBytes;
  size_t Height;
  size_t Depth;
} HIP_MEMCPY3D_PEER;

typedef struct ihiprtcLinkState* hiprtcLinkState;

/* Function types. */
typedef hipError_t HIPAPI thipGetErrorName(hipError_t error, const char** pStr);
typedef const char* HIPAPI thipGetErrorString(hipError_t error);
typedef hipError_t HIPAPI thipGetLastError(hipError_t error);
typedef hipError_t HIPAPI thipInit(unsigned int Flags);
typedef hipError_t HIPAPI thipDriverGetVersion(int* driverVersion);
typedef hipError_t HIPAPI thipRuntimeGetVersion(int* runtimeVersion);
typedef hipError_t HIPAPI thipGetDevice(int* device);
typedef hipError_t HIPAPI thipGetDeviceCount(int* count);
typedef hipError_t HIPAPI thipDeviceGet(hipDevice_t* device, int ordinal);
typedef hipError_t HIPAPI thipDeviceGetName(char* name, int len, hipDevice_t dev);
typedef hipError_t HIPAPI thipDeviceGetAttribute(int* pi, hipDeviceAttribute_t attrib, hipDevice_t dev);
typedef hipError_t HIPAPI thipDeviceGetLimit(size_t* pValue, enum hipLimit_t limit);
typedef hipError_t HIPAPI thipDeviceSetLimit(enum hipLimit_t limit, size_t value);
typedef hipError_t HIPAPI thipDeviceComputeCapability(int* major, int* minor, hipDevice_t dev);
typedef hipError_t HIPAPI thipDevicePrimaryCtxRetain(hipCtx_t* pctx, hipDevice_t dev);
typedef hipError_t HIPAPI thipDevicePrimaryCtxRelease(hipDevice_t dev);
typedef hipError_t HIPAPI thipDevicePrimaryCtxSetFlags(hipDevice_t dev, unsigned int flags);
typedef hipError_t HIPAPI thipDevicePrimaryCtxGetState(hipDevice_t dev, unsigned int* flags, int* active);
typedef hipError_t HIPAPI thipDevicePrimaryCtxReset(hipDevice_t dev);
typedef hipError_t HIPAPI thipCtxCreate(hipCtx_t* pctx, unsigned int flags, hipDevice_t dev);
typedef hipError_t HIPAPI thipCtxDestroy(hipCtx_t ctx);
typedef hipError_t HIPAPI thipCtxPushCurrent(hipCtx_t ctx);
typedef hipError_t HIPAPI thipCtxPopCurrent(hipCtx_t* pctx);
typedef hipError_t HIPAPI thipCtxSetCurrent(hipCtx_t ctx);
typedef hipError_t HIPAPI thipCtxGetCurrent(hipCtx_t* pctx);
typedef hipError_t HIPAPI thipCtxGetDevice(hipDevice_t* device);
typedef hipError_t HIPAPI thipCtxGetFlags(unsigned int* flags);
typedef hipError_t HIPAPI thipCtxSynchronize(void);
typedef hipError_t HIPAPI thipDeviceSynchronize(void);
typedef hipError_t HIPAPI thipCtxGetCacheConfig(hipFuncCache_t* pconfig);
typedef hipError_t HIPAPI thipCtxSetCacheConfig(hipFuncCache_t config);
typedef hipError_t HIPAPI thipCtxGetSharedMemConfig(hipSharedMemConfig* pConfig);
typedef hipError_t HIPAPI thipCtxSetSharedMemConfig(hipSharedMemConfig config);
typedef hipError_t HIPAPI thipCtxGetApiVersion(hipCtx_t ctx, unsigned int* version);
typedef hipError_t HIPAPI thipModuleLoad(hipModule_t* module, const char* fname);
typedef hipError_t HIPAPI thipModuleLoadData(hipModule_t* module, const void* image);
typedef hipError_t HIPAPI thipModuleLoadDataEx(hipModule_t* module, const void* image, unsigned int numOptions, hipJitOption* options, void** optionValues);
typedef hipError_t HIPAPI thipModuleUnload(hipModule_t hmod);
typedef hipError_t HIPAPI thipModuleGetFunction(hipFunction_t* hfunc, hipModule_t hmod, const char* name);
typedef hipError_t HIPAPI thipModuleGetGlobal(hipDeviceptr_t* dptr, size_t* bytes, hipModule_t hmod, const char* name);
typedef hipError_t HIPAPI thipModuleGetTexRef(textureReference** pTexRef, hipModule_t hmod, const char* name);
typedef hipError_t HIPAPI thipMemGetInfo(size_t* free, size_t* total);
typedef hipError_t HIPAPI thipMalloc(hipDeviceptr_t* dptr, size_t bytesize);
typedef hipError_t HIPAPI thipMemAllocPitch(hipDeviceptr_t* dptr, size_t* pPitch, size_t WidthInBytes, size_t Height, unsigned int ElementSizeBytes);
typedef hipError_t HIPAPI thipFree(hipDeviceptr_t dptr);
typedef hipError_t HIPAPI thipMemGetAddressRange(hipDeviceptr_t* pbase, size_t* psize, hipDeviceptr_t dptr);
typedef hipError_t HIPAPI thipHostMalloc(void** pp, size_t bytesize, unsigned int flags);
typedef hipError_t HIPAPI thipHostFree(void* p);
typedef hipError_t HIPAPI thipMemHostAlloc(void** pp, size_t bytesize, unsigned int Flags);
typedef hipError_t HIPAPI thipHostRegister(void* p, size_t bytesize, unsigned int Flags);
typedef hipError_t HIPAPI thipHostGetDevicePointer(hipDeviceptr_t* pdptr, void* p, unsigned int Flags);
typedef hipError_t HIPAPI thipHostGetFlags(unsigned int* pFlags, void* p);
typedef hipError_t HIPAPI thipMallocManaged(hipDeviceptr_t* dptr, size_t bytesize, unsigned int flags);
typedef hipError_t HIPAPI thipDeviceGetByPCIBusId(hipDevice_t* dev, const char* pciBusId);
typedef hipError_t HIPAPI thipDeviceGetPCIBusId(char* pciBusId, int len, hipDevice_t dev);
typedef hipError_t HIPAPI thipHostUnregister(void* p);
typedef hipError_t HIPAPI thipMemcpy(void* dst, const void* src, size_t ByteCount, hipMemcpyKind kind);
typedef hipError_t HIPAPI thipMemcpyPeer(hipDeviceptr_t dstDevice, hipCtx_t dstContext, hipDeviceptr_t srcDevice, hipCtx_t srcContext, size_t ByteCount);
typedef hipError_t HIPAPI thipMemcpyHtoD(hipDeviceptr_t dstDevice, void* srcHost, size_t ByteCount);
typedef hipError_t HIPAPI thipMemcpyDtoH(void* dstHost, hipDeviceptr_t srcDevice, size_t ByteCount);
typedef hipError_t HIPAPI thipMemcpyDtoD(hipDeviceptr_t dstDevice, hipDeviceptr_t srcDevice, size_t ByteCount);
typedef hipError_t HIPAPI thipDrvMemcpy2DUnaligned(const hip_Memcpy2D* pCopy);
typedef hipError_t HIPAPI thipMemcpyParam2D(const hip_Memcpy2D* pCopy);
typedef hipError_t HIPAPI thipDrvMemcpy3D(const HIP_MEMCPY3D* pCopy);
typedef hipError_t HIPAPI thipMemcpyHtoDAsync(hipDeviceptr_t dstDevice, const void* srcHost, size_t ByteCount, hipStream_t hStream);
typedef hipError_t HIPAPI thipMemcpyDtoHAsync(void* dstHost, hipDeviceptr_t srcDevice, size_t ByteCount, hipStream_t hStream);
typedef hipError_t HIPAPI thipMemcpyDtoDAsync(hipDeviceptr_t dstDevice, hipDeviceptr_t srcDevice, size_t ByteCount, hipStream_t hStream);
typedef hipError_t HIPAPI thipMemcpyParam2DAsync(const hip_Memcpy2D* pCopy, hipStream_t hStream);
typedef hipError_t HIPAPI thipDrvMemcpy3DAsync(const HIP_MEMCPY3D* pCopy, hipStream_t hStream);
typedef hipError_t HIPAPI thipMemset(void* dstDevice, int value, size_t sizeBytes);
typedef hipError_t HIPAPI thipMemsetD8(hipDeviceptr_t dstDevice, unsigned char uc, size_t N);
typedef hipError_t HIPAPI thipMemsetD16(hipDeviceptr_t dstDevice, unsigned short us, size_t N);
typedef hipError_t HIPAPI thipMemsetD32(hipDeviceptr_t dstDevice, unsigned int ui, size_t N);
typedef hipError_t HIPAPI thipMemsetD8Async(hipDeviceptr_t dstDevice, unsigned char uc, size_t N, hipStream_t hStream);
typedef hipError_t HIPAPI thipMemsetD16Async(hipDeviceptr_t dstDevice, unsigned short us, size_t N, hipStream_t hStream);
typedef hipError_t HIPAPI thipMemsetD32Async(hipDeviceptr_t dstDevice, unsigned int ui, size_t N, hipStream_t hStream);
typedef hipError_t HIPAPI thipMemsetD2D8Async(hipDeviceptr_t dstDevice, size_t dstPitch, unsigned char uc, size_t Width, size_t Height, hipStream_t hStream);
typedef hipError_t HIPAPI thipMemsetD2D16Async(hipDeviceptr_t dstDevice, size_t dstPitch, unsigned short us, size_t Width, size_t Height, hipStream_t hStream);
typedef hipError_t HIPAPI thipMemsetD2D32Async(hipDeviceptr_t dstDevice, size_t dstPitch, unsigned int ui, size_t Width, size_t Height, hipStream_t hStream);
typedef hipError_t HIPAPI thipArrayCreate(hArray ** pHandle, const HIP_ARRAY_DESCRIPTOR* pAllocateArray);
typedef hipError_t HIPAPI thipArrayDestroy(hArray hArray);
typedef hipError_t HIPAPI thipArray3DCreate(hArray * pHandle, const HIP_ARRAY3D_DESCRIPTOR* pAllocateArray);
typedef hipError_t HIPAPI thipPointerGetAttributes(hipPointerAttribute_t* attributes, const void* ptr);
typedef hipError_t HIPAPI thipStreamCreate(hipStream_t* phStream);
typedef hipError_t HIPAPI thipStreamCreateWithFlags(hipStream_t* phStream, unsigned int Flags);
typedef hipError_t HIPAPI thipStreamCreateWithPriority(hipStream_t* phStream, unsigned int flags, int priority);
typedef hipError_t HIPAPI thipStreamGetPriority(hipStream_t hStream, int* priority);
typedef hipError_t HIPAPI thipStreamGetFlags(hipStream_t hStream, unsigned int* flags);
typedef hipError_t HIPAPI thipStreamWaitEvent(hipStream_t hStream, hipEvent_t hEvent, unsigned int Flags);
typedef hipError_t HIPAPI thipStreamAddCallback(hipStream_t hStream, hipStreamCallback_t callback, void* userData, unsigned int flags);
typedef hipError_t HIPAPI thipStreamQuery(hipStream_t hStream);
typedef hipError_t HIPAPI thipStreamSynchronize(hipStream_t hStream);
typedef hipError_t HIPAPI thipStreamDestroy(hipStream_t hStream);
typedef hipError_t HIPAPI thipEventCreateWithFlags(hipEvent_t* phEvent, unsigned int Flags);
typedef hipError_t HIPAPI thipEventRecord(hipEvent_t hEvent, hipStream_t hStream);
typedef hipError_t HIPAPI thipEventQuery(hipEvent_t hEvent);
typedef hipError_t HIPAPI thipEventSynchronize(hipEvent_t hEvent);
typedef hipError_t HIPAPI thipEventDestroy(hipEvent_t hEvent);
typedef hipError_t HIPAPI thipEventElapsedTime(float* pMilliseconds, hipEvent_t hStart, hipEvent_t hEnd);
typedef hipError_t HIPAPI thipFuncGetAttribute(int* pi, hipFunction_attribute attrib, hipFunction_t hfunc);
typedef hipError_t HIPAPI thipFuncSetCacheConfig(hipFunction_t hfunc, hipFuncCache_t config);
typedef hipError_t HIPAPI thipModuleLaunchKernel(hipFunction_t f, unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ, unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ, unsigned int sharedMemBytes, hipStream_t hStream, void** kernelParams, void** extra);
typedef hipError_t HIPAPI thipDrvOccupancyMaxActiveBlocksPerMultiprocessor(int* numBlocks, hipFunction_t func, int blockSize, size_t dynamicSMemSize);
typedef hipError_t HIPAPI thipDrvOccupancyMaxActiveBlocksPerMultiprocessorWithFlags(int* numBlocks, hipFunction_t func, int blockSize, size_t dynamicSMemSize, unsigned int flags);
typedef hipError_t HIPAPI thipModuleOccupancyMaxPotentialBlockSize(int* minGridSize, int* blockSize, hipFunction_t func, size_t dynamicSMemSize, int blockSizeLimit);
typedef hipError_t HIPAPI thipTexRefSetArray(hipTexRef hTexRef, hArray * hArray, unsigned int Flags);
typedef hipError_t HIPAPI thipTexRefSetAddress(size_t* ByteOffset, hipTexRef hTexRef, hipDeviceptr_t dptr, size_t bytes);
typedef hipError_t HIPAPI thipTexRefSetAddress2D(hipTexRef hTexRef, const HIP_ARRAY_DESCRIPTOR* desc, hipDeviceptr_t dptr, size_t Pitch);
typedef hipError_t HIPAPI thipTexRefSetFormat(hipTexRef hTexRef, hipArray_Format fmt, int NumPackedComponents);
typedef hipError_t HIPAPI thipTexRefSetAddressMode(hipTexRef hTexRef, int dim, hipTextureAddressMode am);
typedef hipError_t HIPAPI thipTexRefSetFilterMode(hipTexRef hTexRef, hipTextureFilterMode fm);
typedef hipError_t HIPAPI thipTexRefSetFlags(hipTexRef hTexRef, unsigned int Flags);
typedef hipError_t HIPAPI thipTexRefGetAddress(hipDeviceptr_t* pdptr, hipTexRef hTexRef);
typedef hipError_t HIPAPI thipTexRefGetArray(hArray ** phArray, hipTexRef hTexRef);
typedef hipError_t HIPAPI thipTexRefGetAddressMode(hipTextureAddressMode* pam, hipTexRef hTexRef, int dim);
typedef hipError_t HIPAPI thipTexObjectCreate(hipTextureObject_t* pTexObject, const hipResourceDesc* pResDesc, const hipTextureDesc* pTexDesc, const HIP_RESOURCE_VIEW_DESC* pResViewDesc);
typedef hipError_t HIPAPI thipTexObjectDestroy(hipTextureObject_t texObject);
typedef hipError_t HIPAPI thipDeviceCanAccessPeer(int* canAccessPeer, hipDevice_t dev, hipDevice_t peerDev);
typedef hipError_t HIPAPI thipCtxEnablePeerAccess(hipCtx_t peerContext, unsigned int Flags);
typedef hipError_t HIPAPI thipCtxDisablePeerAccess(hipCtx_t peerContext);
typedef hipError_t HIPAPI thipDeviceGetP2PAttribute(int* value, hipDeviceP2PAttr attrib, hipDevice_t srcDevice, hipDevice_t dstDevice);
typedef hipError_t HIPAPI thipGraphicsUnregisterResource(hipGraphicsResource resource);
typedef hipError_t HIPAPI thipGraphicsResourceGetMappedMipmappedArray(hipMipmappedArray_t* pMipmappedArray, hipGraphicsResource resource);
typedef hipError_t HIPAPI thipGraphicsResourceGetMappedPointer(hipDeviceptr_t* pDevPtr, size_t* pSize, hipGraphicsResource resource);
typedef hipError_t HIPAPI thipGraphicsMapResources(unsigned int count, hipGraphicsResource* resources, hipStream_t hStream);
typedef hipError_t HIPAPI thipGraphicsUnmapResources(unsigned int count, hipGraphicsResource* resources, hipStream_t hStream);
typedef hipError_t HIPAPI thipGraphicsGLRegisterBuffer(hipGraphicsResource* pCudaResource, GLuint buffer, unsigned int Flags);
typedef hipError_t HIPAPI thipGLGetDevices(unsigned int* pHipDeviceCount, int* pHipDevices, unsigned int hipDeviceCount, hipGLDeviceList deviceList);
typedef hipError_t HIPAPI thipImportExternalMemory(hipExternalMemory_t* extMem_out, const hipExternalMemoryHandleDesc* memHandleDesc);
typedef hipError_t HIPAPI thipExternalMemoryGetMappedBuffer(void **devPtr, hipExternalMemory_t extMem, const hipExternalMemoryBufferDesc *bufferDesc);
typedef hipError_t HIPAPI thipDestroyExternalMemory(hipExternalMemory_t extMem);
typedef const char* HIPAPI thiprtcGetErrorString(hiprtcResult result);
typedef hiprtcResult HIPAPI thiprtcAddNameExpression(hiprtcProgram prog, const char* name_expression);
typedef hiprtcResult HIPAPI thiprtcCompileProgram(hiprtcProgram prog, int numOptions, const char** options);
typedef hiprtcResult HIPAPI thiprtcCreateProgram(hiprtcProgram* prog, const char* src, const char* name, int numHeaders, const char** headers, const char** includeNames);
typedef hiprtcResult HIPAPI thiprtcDestroyProgram(hiprtcProgram* prog);
typedef hiprtcResult HIPAPI thiprtcGetLoweredName(hiprtcProgram prog, const char* name_expression, const char** lowered_name);
typedef hiprtcResult HIPAPI thiprtcGetProgramLog(hiprtcProgram prog, char* log);
typedef hiprtcResult HIPAPI thiprtcGetProgramLogSize(hiprtcProgram prog, size_t* logSizeRet);
typedef hiprtcResult HIPAPI thiprtcGetBitcode( hiprtcProgram prog, char* bitcode );
typedef hiprtcResult HIPAPI thiprtcGetBitcodeSize( hiprtcProgram prog, size_t* bitcodeSizeRet );
typedef hiprtcResult HIPAPI thiprtcGetCode(hiprtcProgram prog, char* code);
typedef hiprtcResult HIPAPI thiprtcGetCodeSize(hiprtcProgram prog, size_t* codeSizeRet);
typedef hiprtcResult HIPAPI thiprtcLinkCreate( unsigned int num_options, hiprtcJIT_option* option_ptr, void** option_vals_pptr, hiprtcLinkState* hip_link_state_ptr );
typedef hiprtcResult HIPAPI thiprtcLinkAddFile( hiprtcLinkState hip_link_state, hiprtcJITInputType input_type, const char* file_path, unsigned int num_options, hiprtcJIT_option* options_ptr, void** option_values );
typedef hiprtcResult HIPAPI thiprtcLinkAddData( hiprtcLinkState hip_link_state, hiprtcJITInputType input_type, void* image, size_t image_size, const char* name, unsigned int num_options, hiprtcJIT_option* options_ptr, void** option_values );
typedef hiprtcResult HIPAPI thiprtcLinkComplete( hiprtcLinkState hip_link_state, void** bin_out, size_t* size_out );
typedef hiprtcResult HIPAPI thiprtcLinkDestroy( hiprtcLinkState hip_link_state );

/* Function declarations. */
extern thipGetErrorName *hipGetErrorName;
extern thipGetErrorString* hipGetErrorString;
extern thipGetLastError* hipGetLastError;
extern thipInit *hipInit;
extern thipDriverGetVersion *hipDriverGetVersion;
extern thipRuntimeGetVersion *hipRuntimeGetVersion;
extern thipGetDevice *hipGetDevice;
extern thipGetDeviceCount *hipGetDeviceCount;
extern thipDeviceGet *hipDeviceGet;
extern thipDeviceGetName *hipDeviceGetName;
extern thipDeviceGetAttribute *hipDeviceGetAttribute;
extern thipDeviceGetLimit *hipDeviceGetLimit;
extern thipDeviceSetLimit *hipDeviceSetLimit;
extern thipDeviceComputeCapability *hipDeviceComputeCapability;
extern thipDevicePrimaryCtxRetain *hipDevicePrimaryCtxRetain;
extern thipDevicePrimaryCtxRelease *hipDevicePrimaryCtxRelease;
extern thipDevicePrimaryCtxSetFlags *hipDevicePrimaryCtxSetFlags;
extern thipDevicePrimaryCtxGetState *hipDevicePrimaryCtxGetState;
extern thipDevicePrimaryCtxReset *hipDevicePrimaryCtxReset;
extern thipCtxCreate *hipCtxCreate;
extern thipCtxDestroy *hipCtxDestroy;
extern thipCtxPushCurrent *hipCtxPushCurrent;
extern thipCtxPopCurrent *hipCtxPopCurrent;
extern thipCtxSetCurrent *hipCtxSetCurrent;
extern thipCtxGetCurrent *hipCtxGetCurrent;
extern thipCtxGetDevice *hipCtxGetDevice;
extern thipCtxGetFlags *hipCtxGetFlags;
extern thipCtxSynchronize *hipCtxSynchronize;
extern thipDeviceSynchronize *hipDeviceSynchronize;
extern thipCtxGetCacheConfig *hipCtxGetCacheConfig;
extern thipCtxSetCacheConfig *hipCtxSetCacheConfig;
extern thipCtxGetSharedMemConfig *hipCtxGetSharedMemConfig;
extern thipCtxSetSharedMemConfig *hipCtxSetSharedMemConfig;
extern thipCtxGetApiVersion *hipCtxGetApiVersion;
extern thipModuleLoad *hipModuleLoad;
extern thipModuleLoadData *hipModuleLoadData;
extern thipModuleLoadDataEx *hipModuleLoadDataEx;
extern thipModuleUnload *hipModuleUnload;
extern thipModuleGetFunction *hipModuleGetFunction;
extern thipModuleGetGlobal *hipModuleGetGlobal;
extern thipModuleGetTexRef *hipModuleGetTexRef;
extern thipMemGetInfo *hipMemGetInfo;
extern thipMalloc *hipMalloc;
extern thipMemAllocPitch *hipMemAllocPitch;
extern thipFree *hipFree;
extern thipMemGetAddressRange *hipMemGetAddressRange;
extern thipHostMalloc *hipHostMalloc;
extern thipHostFree *hipHostFree;
extern thipHostRegister *hipHostRegister;
extern thipHostGetDevicePointer *hipHostGetDevicePointer;
extern thipHostGetFlags *hipHostGetFlags;
extern thipHostUnregister *hipHostUnregister;
extern thipMallocManaged *hipMallocManaged;
extern thipDeviceGetByPCIBusId *hipDeviceGetByPCIBusId;
extern thipDeviceGetPCIBusId *hipDeviceGetPCIBusId;
extern thipMemcpy *hipMemcpy;
extern thipMemcpyPeer *hipMemcpyPeer;
extern thipMemcpyHtoD *hipMemcpyHtoD;
extern thipMemcpyDtoH *hipMemcpyDtoH;
extern thipMemcpyDtoD *hipMemcpyDtoD;
extern thipDrvMemcpy2DUnaligned *hipDrvMemcpy2DUnaligned;
extern thipMemcpyParam2D *hipMemcpyParam2D;
extern thipDrvMemcpy3D *hipDrvMemcpy3D;
extern thipMemcpyHtoDAsync *hipMemcpyHtoDAsync;
extern thipMemcpyDtoHAsync *hipMemcpyDtoHAsync;
extern thipMemcpyDtoDAsync *hipMemcpyDtoDAsync;
extern thipMemcpyParam2DAsync *hipMemcpyParam2DAsync;
extern thipDrvMemcpy3DAsync *hipDrvMemcpy3DAsync;
extern thipMemset *hipMemset;
extern thipMemsetD8 *hipMemsetD8;
extern thipMemsetD16 *hipMemsetD16;
extern thipMemsetD32 *hipMemsetD32;
extern thipMemsetD8Async *hipMemsetD8Async;
extern thipMemsetD16Async *hipMemsetD16Async;
extern thipMemsetD32Async *hipMemsetD32Async;
extern thipArrayCreate *hipArrayCreate;
extern thipArrayDestroy *hipArrayDestroy;
extern thipArray3DCreate *hipArray3DCreate;
extern thipPointerGetAttributes *hipPointerGetAttributes;
extern thipStreamCreate* hipStreamCreate;
extern thipStreamCreateWithFlags *hipStreamCreateWithFlags;
extern thipStreamCreateWithPriority *hipStreamCreateWithPriority;
extern thipStreamGetPriority *hipStreamGetPriority;
extern thipStreamGetFlags *hipStreamGetFlags;
extern thipStreamWaitEvent *hipStreamWaitEvent;
extern thipStreamAddCallback *hipStreamAddCallback;
extern thipStreamQuery *hipStreamQuery;
extern thipStreamSynchronize *hipStreamSynchronize;
extern thipStreamDestroy *hipStreamDestroy;
extern thipEventCreateWithFlags *hipEventCreateWithFlags;
extern thipEventRecord *hipEventRecord;
extern thipEventQuery *hipEventQuery;
extern thipEventSynchronize *hipEventSynchronize;
extern thipEventDestroy *hipEventDestroy;
extern thipEventElapsedTime *hipEventElapsedTime;
extern thipFuncGetAttribute *hipFuncGetAttribute;
extern thipFuncSetCacheConfig *hipFuncSetCacheConfig;
extern thipModuleLaunchKernel *hipModuleLaunchKernel;
extern thipDrvOccupancyMaxActiveBlocksPerMultiprocessor *hipDrvOccupancyMaxActiveBlocksPerMultiprocessor;
extern thipDrvOccupancyMaxActiveBlocksPerMultiprocessorWithFlags *hipDrvOccupancyMaxActiveBlocksPerMultiprocessorWithFlags;
extern thipModuleOccupancyMaxPotentialBlockSize *hipModuleOccupancyMaxPotentialBlockSize;
extern thipTexRefSetArray *hipTexRefSetArray;
extern thipTexRefSetAddress *hipTexRefSetAddress;
extern thipTexRefSetAddress2D *hipTexRefSetAddress2D;
extern thipTexRefSetFormat *hipTexRefSetFormat;
extern thipTexRefSetAddressMode *hipTexRefSetAddressMode;
extern thipTexRefSetFilterMode *hipTexRefSetFilterMode;
extern thipTexRefSetFlags *hipTexRefSetFlags;
extern thipTexRefGetAddress *hipTexRefGetAddress;
extern thipTexRefGetArray *hipTexRefGetArray;
extern thipTexRefGetAddressMode *hipTexRefGetAddressMode;
extern thipTexObjectCreate *hipTexObjectCreate;
extern thipTexObjectDestroy *hipTexObjectDestroy;
extern thipDeviceCanAccessPeer *hipDeviceCanAccessPeer;
extern thipCtxEnablePeerAccess *hipCtxEnablePeerAccess;
extern thipCtxDisablePeerAccess *hipCtxDisablePeerAccess;
extern thipDeviceGetP2PAttribute *hipDeviceGetP2PAttribute;
extern thipGraphicsUnregisterResource *hipGraphicsUnregisterResource;
extern thipGraphicsResourceGetMappedMipmappedArray *hipGraphicsResourceGetMappedMipmappedArray;
extern thipGraphicsResourceGetMappedPointer *hipGraphicsResourceGetMappedPointer;
extern thipGraphicsMapResources *hipGraphicsMapResources;
extern thipGraphicsUnmapResources *hipGraphicsUnmapResources;

extern thipGraphicsGLRegisterBuffer *hipGraphicsGLRegisterBuffer;
extern thipGLGetDevices *hipGLGetDevices;
extern thipImportExternalMemory *hipImportExternalMemory;
extern thipExternalMemoryGetMappedBuffer *hipExternalMemoryGetMappedBuffer;
extern thipDestroyExternalMemory *hipDestroyExternalMemory;

extern thiprtcGetErrorString* hiprtcGetErrorString;
extern thiprtcAddNameExpression* hiprtcAddNameExpression;
extern thiprtcCompileProgram* hiprtcCompileProgram;
extern thiprtcCreateProgram* hiprtcCreateProgram;
extern thiprtcDestroyProgram* hiprtcDestroyProgram;
extern thiprtcGetLoweredName* hiprtcGetLoweredName;
extern thiprtcGetProgramLog* hiprtcGetProgramLog;
extern thiprtcGetProgramLogSize* hiprtcGetProgramLogSize;
extern thiprtcGetBitcode* hiprtcGetBitcode;
extern thiprtcGetBitcodeSize* hiprtcGetBitcodeSize;
extern thiprtcGetCode* hiprtcGetCode;
extern thiprtcGetCodeSize* hiprtcGetCodeSize;
extern thiprtcLinkCreate* hiprtcLinkCreate;
extern thiprtcLinkAddFile* hiprtcLinkAddFile;
extern thiprtcLinkAddData* hiprtcLinkAddData;
extern thiprtcLinkComplete* hiprtcLinkComplete;
extern thiprtcLinkDestroy* hiprtcLinkDestroy;

/* HIPEW API. */

enum {
  HIPEW_SUCCESS = 0,
  HIPEW_ERROR_OPEN_FAILED = -1,
  HIPEW_ERROR_ATEXIT_FAILED = -2,
  HIPEW_ERROR_OLD_DRIVER = -3,
};

enum {
	HIPEW_INIT_HIP = 1,
};

int hipewInit(hipuint32_t flags);
const char *hipewErrorString(hipError_t result);
const char *hipewCompilerPath(void);
int hipewCompilerVersion(void);

#ifdef __cplusplus
}
#endif

#endif  /* __HIPEW_H__ */
