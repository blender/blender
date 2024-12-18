/*
 * Copyright 2011-2024 Blender Foundation
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

#ifndef __HIPEW6_H__
#define __HIPEW6_H__

#define WIN_DRIVER "amdhip64_6.dll"

#define hipIpcMemLazyEnablePeerAccess 0x01

#define hipMemAttachGlobal  0x01
#define hipMemAttachHost    0x02

#define hipDeviceScheduleAuto 0x0
#define hipDeviceScheduleSpin  0x1

#define hipDeviceScheduleYield  0x2
#define hipDeviceScheduleBlockingSync 0x4
#define hipDeviceScheduleMask 0x7
#define hipDeviceMapHost 0x8
#define hipDeviceLmemResizeToMax 0x10

#define hipStreamDefault  0x00
#define hipStreamNonBlocking 0x01

#define hipEventDefault 0x0
#define hipEventBlockingSync 0x1
#define hipEventDisableTiming  0x2
#define hipEventInterprocess 0x4

#define hipOccupancyDefault 0x00


typedef enum hipMemcpyKind {
    hipMemcpyHostToHost = 0,
    hipMemcpyHostToDevice = 1,
    hipMemcpyDeviceToHost = 2,
    hipMemcpyDeviceToDevice = 3,
    hipMemcpyDefault = 4,
    hipMemcpyDeviceToDeviceNoCU = 1024

} hipMemcpyKind;


typedef enum hipMemoryType {  
    hipMemoryTypeUnregistered = 0,
    hipMemoryTypeHost         = 1,
    hipMemoryTypeDevice       = 2,
    hipMemoryTypeManaged      = 3,
    hipMemoryTypeArray        = 10,
    hipMemoryTypeUnified      = 11
} hipMemoryType;

typedef enum hipDeviceAttribute_t {
  hipDeviceAttributeCudaCompatibleBegin = 0,
  hipDeviceAttributeEccEnabled = hipDeviceAttributeCudaCompatibleBegin, ///< Whether ECC support is enabled.
  hipDeviceAttributeAccessPolicyMaxWindowSize,        ///< Cuda only. The maximum size of the window policy in bytes.
  hipDeviceAttributeAsyncEngineCount,                 ///< Cuda only. Asynchronous engines number.
  hipDeviceAttributeCanMapHostMemory,                 ///< Whether host memory can be mapped into device address space
  hipDeviceAttributeCanUseHostPointerForRegisteredMem,///< Cuda only. Device can access host registered memory
                                                      ///< at the same virtual address as the CPU
  hipDeviceAttributeClockRate,                        ///< Peak clock frequency in kilohertz.
  hipDeviceAttributeComputeMode,                      ///< Compute mode that device is currently in.
  hipDeviceAttributeComputePreemptionSupported,       ///< Cuda only. Device supports Compute Preemption.
  hipDeviceAttributeConcurrentKernels,                ///< Device can possibly execute multiple kernels concurrently.
  hipDeviceAttributeConcurrentManagedAccess,          ///< Device can coherently access managed memory concurrently with the CPU
  hipDeviceAttributeCooperativeLaunch,                ///< Support cooperative launch
  hipDeviceAttributeCooperativeMultiDeviceLaunch,     ///< Support cooperative launch on multiple devices
  hipDeviceAttributeDeviceOverlap,               ///< Cuda only. Device can concurrently copy memory and execute a kernel.  
                                                      ///< Deprecated. Use instead asyncEngineCount.
  hipDeviceAttributeDirectManagedMemAccessFromHost,   ///< Host can directly access managed memory on
                                                      ///< the device without migration
  hipDeviceAttributeGlobalL1CacheSupported,           ///< Cuda only. Device supports caching globals in L1
  hipDeviceAttributeHostNativeAtomicSupported,        ///< Cuda only. Link between the device and the host supports native atomic operations
  hipDeviceAttributeIntegrated,                       ///< Device is integrated GPU
  hipDeviceAttributeIsMultiGpuBoard,                  ///< Multiple GPU devices.
  hipDeviceAttributeKernelExecTimeout,                ///< Run time limit for kernels executed on the device
  hipDeviceAttributeL2CacheSize,                      ///< Size of L2 cache in bytes. 0 if the device doesn't have L2 cache.
  hipDeviceAttributeLocalL1CacheSupported,            ///< caching locals in L1 is supported
  hipDeviceAttributeLuid,                             ///< Cuda only. 8-byte locally unique identifier in 8 bytes. Undefined on TCC and non-Windows platforms
  hipDeviceAttributeLuidDeviceNodeMask,               ///< Cuda only. Luid device node mask. Undefined on TCC and non-Windows platforms
  hipDeviceAttributeComputeCapabilityMajor,           ///< Major compute capability version number.
  hipDeviceAttributeManagedMemory,                    ///< Device supports allocating managed memory on this system
  hipDeviceAttributeMaxBlocksPerMultiProcessor,       ///< Cuda only. Max block size per multiprocessor
  hipDeviceAttributeMaxBlockDimX,                     ///< Max block size in width.
  hipDeviceAttributeMaxBlockDimY,                     ///< Max block size in height.
  hipDeviceAttributeMaxBlockDimZ,                     ///< Max block size in depth.
  hipDeviceAttributeMaxGridDimX,                      ///< Max grid size  in width.
  hipDeviceAttributeMaxGridDimY,                      ///< Max grid size  in height.
  hipDeviceAttributeMaxGridDimZ,                      ///< Max grid size  in depth.
  hipDeviceAttributeMaxSurface1D,                     ///< Maximum size of 1D surface.
  hipDeviceAttributeMaxSurface1DLayered,              ///< Cuda only. Maximum dimensions of 1D layered surface.
  hipDeviceAttributeMaxSurface2D,                     ///< Maximum dimension (width, height) of 2D surface.
  hipDeviceAttributeMaxSurface2DLayered,              ///< Cuda only. Maximum dimensions of 2D layered surface.
  hipDeviceAttributeMaxSurface3D,                     ///< Maximum dimension (width, height, depth) of 3D surface.
  hipDeviceAttributeMaxSurfaceCubemap,                ///< Cuda only. Maximum dimensions of Cubemap surface.
  hipDeviceAttributeMaxSurfaceCubemapLayered,         ///< Cuda only. Maximum dimension of Cubemap layered surface.
  hipDeviceAttributeMaxTexture1DWidth,                ///< Maximum size of 1D texture.
  hipDeviceAttributeMaxTexture1DLayered,              ///< Cuda only. Maximum dimensions of 1D layered texture.
  hipDeviceAttributeMaxTexture1DLinear,               ///< Maximum number of elements allocatable in a 1D linear texture.
                                                      ///< Use cudaDeviceGetTexture1DLinearMaxWidth() instead on Cuda.
  hipDeviceAttributeMaxTexture1DMipmap,               ///< Cuda only. Maximum size of 1D mipmapped texture.
  hipDeviceAttributeMaxTexture2DWidth,                ///< Maximum dimension width of 2D texture.
  hipDeviceAttributeMaxTexture2DHeight,               ///< Maximum dimension hight of 2D texture.
  hipDeviceAttributeMaxTexture2DGather,               ///< Cuda only. Maximum dimensions of 2D texture if gather operations  performed.
  hipDeviceAttributeMaxTexture2DLayered,              ///< Cuda only. Maximum dimensions of 2D layered texture.
  hipDeviceAttributeMaxTexture2DLinear,               ///< Cuda only. Maximum dimensions (width, height, pitch) of 2D textures bound to pitched memory.
  hipDeviceAttributeMaxTexture2DMipmap,               ///< Cuda only. Maximum dimensions of 2D mipmapped texture.
  hipDeviceAttributeMaxTexture3DWidth,                ///< Maximum dimension width of 3D texture.
  hipDeviceAttributeMaxTexture3DHeight,               ///< Maximum dimension height of 3D texture.
  hipDeviceAttributeMaxTexture3DDepth,                ///< Maximum dimension depth of 3D texture.
  hipDeviceAttributeMaxTexture3DAlt,                  ///< Cuda only. Maximum dimensions of alternate 3D texture.
  hipDeviceAttributeMaxTextureCubemap,                ///< Cuda only. Maximum dimensions of Cubemap texture
  hipDeviceAttributeMaxTextureCubemapLayered,         ///< Cuda only. Maximum dimensions of Cubemap layered texture.
  hipDeviceAttributeMaxThreadsDim,                    ///< Maximum dimension of a block
  hipDeviceAttributeMaxThreadsPerBlock,               ///< Maximum number of threads per block.
  hipDeviceAttributeMaxThreadsPerMultiProcessor,      ///< Maximum resident threads per multiprocessor.
  hipDeviceAttributeMaxPitch,                         ///< Maximum pitch in bytes allowed by memory copies
  hipDeviceAttributeMemoryBusWidth,                   ///< Global memory bus width in bits.
  hipDeviceAttributeMemoryClockRate,                  ///< Peak memory clock frequency in kilohertz.
  hipDeviceAttributeComputeCapabilityMinor,           ///< Minor compute capability version number.
  hipDeviceAttributeMultiGpuBoardGroupID,             ///< Cuda only. Unique ID of device group on the same multi-GPU board
  hipDeviceAttributeMultiprocessorCount,              ///< Number of multiprocessors on the device.
  hipDeviceAttributeUnused1,                          ///< Previously hipDeviceAttributeName
  hipDeviceAttributePageableMemoryAccess,             ///< Device supports coherently accessing pageable memory
                                                      ///< without calling hipHostRegister on it
  hipDeviceAttributePageableMemoryAccessUsesHostPageTables, ///< Device accesses pageable memory via the host's page tables
  hipDeviceAttributePciBusId,                         ///< PCI Bus ID.
  hipDeviceAttributePciDeviceId,                      ///< PCI Device ID.
  hipDeviceAttributePciDomainID,                      ///< PCI Domain ID.
  hipDeviceAttributePersistingL2CacheMaxSize,         ///< Cuda11 only. Maximum l2 persisting lines capacity in bytes
  hipDeviceAttributeMaxRegistersPerBlock,             ///< 32-bit registers available to a thread block. This number is shared
                                                      ///< by all thread blocks simultaneously resident on a multiprocessor.
  hipDeviceAttributeMaxRegistersPerMultiprocessor,    ///< 32-bit registers available per block.
  hipDeviceAttributeReservedSharedMemPerBlock,        ///< Cuda11 only. Shared memory reserved by CUDA driver per block.
  hipDeviceAttributeMaxSharedMemoryPerBlock,          ///< Maximum shared memory available per block in bytes.
  hipDeviceAttributeSharedMemPerBlockOptin,           ///< Cuda only. Maximum shared memory per block usable by special opt in.
  hipDeviceAttributeSharedMemPerMultiprocessor,       ///< Cuda only. Shared memory available per multiprocessor.
  hipDeviceAttributeSingleToDoublePrecisionPerfRatio, ///< Cuda only. Performance ratio of single precision to double precision.
  hipDeviceAttributeStreamPrioritiesSupported,        ///< Cuda only. Whether to support stream priorities.
  hipDeviceAttributeSurfaceAlignment,                 ///< Cuda only. Alignment requirement for surfaces
  hipDeviceAttributeTccDriver,                        ///< Cuda only. Whether device is a Tesla device using TCC driver
  hipDeviceAttributeTextureAlignment,                 ///< Alignment requirement for textures
  hipDeviceAttributeTexturePitchAlignment,            ///< Pitch alignment requirement for 2D texture references bound to pitched memory;
  hipDeviceAttributeTotalConstantMemory,              ///< Constant memory size in bytes.
  hipDeviceAttributeTotalGlobalMem,                   ///< Global memory available on devicice.
  hipDeviceAttributeUnifiedAddressing,                ///< Cuda only. An unified address space shared with the host.
  hipDeviceAttributeUnused2,                          ///< Previously hipDeviceAttributeUuid
  hipDeviceAttributeWarpSize,                         ///< Warp size in threads.
  hipDeviceAttributeMemoryPoolsSupported,             ///< Device supports HIP Stream Ordered Memory Allocator
  hipDeviceAttributeVirtualMemoryManagementSupported, ///< Device supports HIP virtual memory management
  hipDeviceAttributeHostRegisterSupported,            ///< Can device support host memory registration via hipHostRegister
  hipDeviceAttributeMemoryPoolSupportedHandleTypes,   ///< Supported handle mask for HIP Stream Ordered Memory Allocator
  hipDeviceAttributeCudaCompatibleEnd = 9999,
  hipDeviceAttributeAmdSpecificBegin = 10000,
  hipDeviceAttributeClockInstructionRate = hipDeviceAttributeAmdSpecificBegin,  ///< Frequency in khz of the timer used by the device-side "clock*"
  hipDeviceAttributeUnused3,                                  ///< Previously hipDeviceAttributeArch
  hipDeviceAttributeMaxSharedMemoryPerMultiprocessor,         ///< Maximum Shared Memory PerMultiprocessor.
  hipDeviceAttributeUnused4,                                  ///< Previously hipDeviceAttributeGcnArch
  hipDeviceAttributeUnused5,                                  ///< Previously hipDeviceAttributeGcnArchName
  hipDeviceAttributeHdpMemFlushCntl,                          ///< Address of the HDP_MEM_COHERENCY_FLUSH_CNTL register
  hipDeviceAttributeHdpRegFlushCntl,                          ///< Address of the HDP_REG_COHERENCY_FLUSH_CNTL register
  hipDeviceAttributeCooperativeMultiDeviceUnmatchedFunc,      ///< Supports cooperative launch on multiple
                                                              ///< devices with unmatched functions
  hipDeviceAttributeCooperativeMultiDeviceUnmatchedGridDim,   ///< Supports cooperative launch on multiple
                                                              ///< devices with unmatched grid dimensions
  hipDeviceAttributeCooperativeMultiDeviceUnmatchedBlockDim,  ///< Supports cooperative launch on multiple
                                                              ///< devices with unmatched block dimensions
  hipDeviceAttributeCooperativeMultiDeviceUnmatchedSharedMem, ///< Supports cooperative launch on multiple
                                                              ///< devices with unmatched shared memories
  hipDeviceAttributeIsLargeBar,                               ///< Whether it is LargeBar
  hipDeviceAttributeAsicRevision,                             ///< Revision of the GPU in this device
  hipDeviceAttributeCanUseStreamWaitValue,                    ///< '1' if Device supports hipStreamWaitValue32() and
                                                              ///< hipStreamWaitValue64() , '0' otherwise.
  hipDeviceAttributeAmdSpecificEnd = 19999,
  hipDeviceAttributeVendorSpecificBegin = 20000,
  hipDeviceAttribute
  // Extended attributes for vendors
} hipDeviceAttribute_t;

typedef struct hipUUID_t {
    char bytes[16];
} hipUUID;


typedef struct hipDeviceProp_t {
    char name[256];                   ///< Device name.
    hipUUID uuid;                     ///< UUID of a device
    char luid[8];                     ///< 8-byte unique identifier. Only valid on windows
    unsigned int luidDeviceNodeMask;  ///< LUID node mask
    size_t totalGlobalMem;            ///< Size of global memory region (in bytes).
    size_t sharedMemPerBlock;         ///< Size of shared memory per block (in bytes).
    int regsPerBlock;                 ///< Registers per block.
    int warpSize;                     ///< Warp size.
    size_t memPitch;                  ///< Maximum pitch in bytes allowed by memory copies
                                      ///< pitched memory
    int maxThreadsPerBlock;           ///< Max work items per work group or workgroup max size.
    int maxThreadsDim[3];             ///< Max number of threads in each dimension (XYZ) of a block.
    int maxGridSize[3];               ///< Max grid dimensions (XYZ).
    int clockRate;                    ///< Max clock frequency of the multiProcessors in khz.
    size_t totalConstMem;             ///< Size of shared constant memory region on the device
                                      ///< (in bytes).
    int major;  ///< Major compute capability.  On HCC, this is an approximation and features may
                ///< differ from CUDA CC.  See the arch feature flags for portable ways to query
                ///< feature caps.
    int minor;  ///< Minor compute capability.  On HCC, this is an approximation and features may
                ///< differ from CUDA CC.  See the arch feature flags for portable ways to query
                ///< feature caps.
    size_t textureAlignment;       ///< Alignment requirement for textures
    size_t texturePitchAlignment;  ///< Pitch alignment requirement for texture references bound to
    int deviceOverlap;             ///< Deprecated. Use asyncEngineCount instead
    int multiProcessorCount;       ///< Number of multi-processors (compute units).
    int kernelExecTimeoutEnabled;  ///< Run time limit for kernels executed on the device
    int integrated;                ///< APU vs dGPU
    int canMapHostMemory;          ///< Check whether HIP can map host memory
    int computeMode;               ///< Compute mode.
    int maxTexture1D;              ///< Maximum number of elements in 1D images
    int maxTexture1DMipmap;        ///< Maximum 1D mipmap texture size
    int maxTexture1DLinear;        ///< Maximum size for 1D textures bound to linear memory
    int maxTexture2D[2];  ///< Maximum dimensions (width, height) of 2D images, in image elements
    int maxTexture2DMipmap[2];  ///< Maximum number of elements in 2D array mipmap of images
    int maxTexture2DLinear[3];  ///< Maximum 2D tex dimensions if tex are bound to pitched memory
    int maxTexture2DGather[2];  ///< Maximum 2D tex dimensions if gather has to be performed
    int maxTexture3D[3];  ///< Maximum dimensions (width, height, depth) of 3D images, in image
                          ///< elements
    int maxTexture3DAlt[3];           ///< Maximum alternate 3D texture dims
    int maxTextureCubemap;            ///< Maximum cubemap texture dims
    int maxTexture1DLayered[2];       ///< Maximum number of elements in 1D array images
    int maxTexture2DLayered[3];       ///< Maximum number of elements in 2D array images
    int maxTextureCubemapLayered[2];  ///< Maximum cubemaps layered texture dims
    int maxSurface1D;                 ///< Maximum 1D surface size
    int maxSurface2D[2];              ///< Maximum 2D surface size
    int maxSurface3D[3];              ///< Maximum 3D surface size
    int maxSurface1DLayered[2];       ///< Maximum 1D layered surface size
    int maxSurface2DLayered[3];       ///< Maximum 2D layared surface size
    int maxSurfaceCubemap;            ///< Maximum cubemap surface size
    int maxSurfaceCubemapLayered[2];  ///< Maximum cubemap layered surface size
    size_t surfaceAlignment;          ///< Alignment requirement for surface
    int concurrentKernels;         ///< Device can possibly execute multiple kernels concurrently.
    int ECCEnabled;                ///< Device has ECC support enabled
    int pciBusID;                  ///< PCI Bus ID.
    int pciDeviceID;               ///< PCI Device ID.
    int pciDomainID;               ///< PCI Domain ID
    int tccDriver;                 ///< 1:If device is Tesla device using TCC driver, else 0
    int asyncEngineCount;          ///< Number of async engines
    int unifiedAddressing;         ///< Does device and host share unified address space
    int memoryClockRate;           ///< Max global memory clock frequency in khz.
    int memoryBusWidth;            ///< Global memory bus width in bits.
    int l2CacheSize;               ///< L2 cache size.
    int persistingL2CacheMaxSize;  ///< Device's max L2 persisting lines in bytes
    int maxThreadsPerMultiProcessor;    ///< Maximum resident threads per multi-processor.
    int streamPrioritiesSupported;      ///< Device supports stream priority
    int globalL1CacheSupported;         ///< Indicates globals are cached in L1
    int localL1CacheSupported;          ///< Locals are cahced in L1
    size_t sharedMemPerMultiprocessor;  ///< Amount of shared memory available per multiprocessor.
    int regsPerMultiprocessor;          ///< registers available per multiprocessor
    int managedMemory;         ///< Device supports allocating managed memory on this system
    int isMultiGpuBoard;       ///< 1 if device is on a multi-GPU board, 0 if not.
    int multiGpuBoardGroupID;  ///< Unique identifier for a group of devices on same multiboard GPU
    int hostNativeAtomicSupported;         ///< Link between host and device supports native atomics
    int singleToDoublePrecisionPerfRatio;  ///< Deprecated. CUDA only.
    int pageableMemoryAccess;              ///< Device supports coherently accessing pageable memory
                                           ///< without calling hipHostRegister on it
    int concurrentManagedAccess;  ///< Device can coherently access managed memory concurrently with
                                  ///< the CPU
    int computePreemptionSupported;         ///< Is compute preemption supported on the device
    int canUseHostPointerForRegisteredMem;  ///< Device can access host registered memory with same
                                            ///< address as the host
    int cooperativeLaunch;                  ///< HIP device supports cooperative launch
    int cooperativeMultiDeviceLaunch;       ///< HIP device supports cooperative launch on multiple
                                            ///< devices
    size_t
        sharedMemPerBlockOptin;  ///< Per device m ax shared mem per block usable by special opt in
    int pageableMemoryAccessUsesHostPageTables;  ///< Device accesses pageable memory via the host's
                                                 ///< page tables
    int directManagedMemAccessFromHost;  ///< Host can directly access managed memory on the device
                                         ///< without migration
    int maxBlocksPerMultiProcessor;      ///< Max number of blocks on CU
    int accessPolicyMaxWindowSize;       ///< Max value of access policy window
    size_t reservedSharedMemPerBlock;    ///< Shared memory reserved by driver per block
    int hostRegisterSupported;           ///< Device supports hipHostRegister
    int sparseHipArraySupported;         ///< Indicates if device supports sparse hip arrays
    int hostRegisterReadOnlySupported;   ///< Device supports using the hipHostRegisterReadOnly flag
                                         ///< with hipHostRegistger
    int timelineSemaphoreInteropSupported;  ///< Indicates external timeline semaphore support
    int memoryPoolsSupported;  ///< Indicates if device supports hipMallocAsync and hipMemPool APIs
    int gpuDirectRDMASupported;                    ///< Indicates device support of RDMA APIs
    unsigned int gpuDirectRDMAFlushWritesOptions;  ///< Bitmask to be interpreted according to
                                                   ///< hipFlushGPUDirectRDMAWritesOptions
    int gpuDirectRDMAWritesOrdering;               ///< value of hipGPUDirectRDMAWritesOrdering
    unsigned int
        memoryPoolSupportedHandleTypes;  ///< Bitmask of handle types support with mempool based IPC
    int deferredMappingHipArraySupported;  ///< Device supports deferred mapping HIP arrays and HIP
                                           ///< mipmapped arrays
    int ipcEventSupported;                 ///< Device supports IPC events
    int clusterLaunch;                     ///< Device supports cluster launch
    int unifiedFunctionPointers;           ///< Indicates device supports unified function pointers
    int reserved[63];                      ///< CUDA Reserved.

    int hipReserved[32];  ///< Reserved for adding new entries for HIP/CUDA.

    /* HIP Only struct members */
    char gcnArchName[256];                    ///< AMD GCN Arch Name. HIP Only.
    size_t maxSharedMemoryPerMultiProcessor;  ///< Maximum Shared Memory Per CU. HIP Only.
    int clockInstructionRate;  ///< Frequency in khz of the timer used by the device-side "clock*"
                               ///< instructions.  New for HIP.
    hipDeviceArch_t arch;      ///< Architectural feature flags.  New for HIP.
    unsigned int* hdpMemFlushCntl;            ///< Addres of HDP_MEM_COHERENCY_FLUSH_CNTL register
    unsigned int* hdpRegFlushCntl;            ///< Addres of HDP_REG_COHERENCY_FLUSH_CNTL register
    int cooperativeMultiDeviceUnmatchedFunc;  ///< HIP device supports cooperative launch on
                                              ///< multiple
                                              /// devices with unmatched functions
    int cooperativeMultiDeviceUnmatchedGridDim;    ///< HIP device supports cooperative launch on
                                                   ///< multiple
                                                   /// devices with unmatched grid dimensions
    int cooperativeMultiDeviceUnmatchedBlockDim;   ///< HIP device supports cooperative launch on
                                                   ///< multiple
                                                   /// devices with unmatched block dimensions
    int cooperativeMultiDeviceUnmatchedSharedMem;  ///< HIP device supports cooperative launch on
                                                   ///< multiple
                                                   /// devices with unmatched shared memories
    int isLargeBar;                                ///< 1: if it is a large PCI bar device, else 0
    int asicRevision;                              ///< Revision of the GPU in this device
} hipDeviceProp_t;

typedef enum HIPpointer_attribute_enum {
  HIP_POINTER_ATTRIBUTE_CONTEXT = 1,   ///< The context on which a pointer was allocated
                                         ///< @warning - not supported in HIP
    HIP_POINTER_ATTRIBUTE_MEMORY_TYPE,   ///< memory type describing location of a pointer
    HIP_POINTER_ATTRIBUTE_DEVICE_POINTER,///< address at which the pointer is allocated on device
    HIP_POINTER_ATTRIBUTE_HOST_POINTER,  ///< address at which the pointer is allocated on host
    HIP_POINTER_ATTRIBUTE_P2P_TOKENS,    ///< A pair of tokens for use with linux kernel interface
                                         ///< @warning - not supported in HIP
    HIP_POINTER_ATTRIBUTE_SYNC_MEMOPS,   ///< Synchronize every synchronous memory operation
                                         ///< initiated on this region
    HIP_POINTER_ATTRIBUTE_BUFFER_ID,     ///< Unique ID for an allocated memory region
    HIP_POINTER_ATTRIBUTE_IS_MANAGED,    ///< Indicates if the pointer points to managed memory
    HIP_POINTER_ATTRIBUTE_DEVICE_ORDINAL,///< device ordinal of a device on which a pointer
                                         ///< was allocated or registered
    HIP_POINTER_ATTRIBUTE_IS_LEGACY_HIP_IPC_CAPABLE, ///< if this pointer maps to an allocation
                                                     ///< that is suitable for hipIpcGetMemHandle
                                                     ///< @warning - not supported in HIP
    HIP_POINTER_ATTRIBUTE_RANGE_START_ADDR,///< Starting address for this requested pointer
    HIP_POINTER_ATTRIBUTE_RANGE_SIZE,      ///< Size of the address range for this requested pointer
    HIP_POINTER_ATTRIBUTE_MAPPED,          ///< tells if this pointer is in a valid address range
                                           ///< that is mapped to a backing allocation
    HIP_POINTER_ATTRIBUTE_ALLOWED_HANDLE_TYPES,///< Bitmask of allowed hipmemAllocationHandleType
                                           ///< for this allocation @warning - not supported in HIP
    HIP_POINTER_ATTRIBUTE_IS_GPU_DIRECT_RDMA_CAPABLE, ///< returns if the memory referenced by
                                           ///< this pointer can be used with the GPUDirect RDMA API
                                           ///< @warning - not supported in HIP
    HIP_POINTER_ATTRIBUTE_ACCESS_FLAGS,    ///< Returns the access flags the device associated with
                                           ///< for the corresponding memory referenced by the ptr
    HIP_POINTER_ATTRIBUTE_MEMPOOL_HANDLE   ///< Returns the mempool handle for the allocation if
                                           ///< it was allocated from a mempool
                                           ///< @warning - not supported in HIP
} HIPpointer_attribute;

typedef enum hipComputeMode {
  hipComputeModeDefault = 0,
  hipComputeModeExclusive = 1,
  hipComputeModeProhibited = 2,
  hipComputeModeExclusiveProcess = 3,
} hipComputeMode;

typedef struct HIP_MEMCPY3D {
  size_t srcXInBytes;
  size_t srcY;
  size_t srcZ;
  size_t srcLOD;
  hipMemoryType srcMemoryType;
  const void* srcHost;
  hipDeviceptr_t srcDevice;
  hArray srcArray;
  size_t srcPitch;
  size_t srcHeight;
  size_t dstXInBytes;
  size_t dstY;
  size_t dstZ;
  size_t dstLOD;
  hipMemoryType dstMemoryType;
  void* dstHost;
  hipDeviceptr_t dstDevice;
  hArray dstArray;
  size_t dstPitch;
  size_t dstHeight;
  size_t WidthInBytes;
  size_t Height;
  size_t Depth;
} HIP_MEMCPY3D;

#if 0 //version 5 and 6 are the same but the structure already in hipew use doesn't match the structure in either version! but matches CUDA ew
//need to check further
typedef struct hipTextureDesc {
    enum hipTextureAddressMode addressMode[3];  // Texture address mode for up to 3 dimensions
    enum hipTextureFilterMode filterMode;
    enum hipTextureReadMode readMode;
    int sRGB;  // Perform sRGB->linear conversion during texture read
    float borderColor[4];
    int normalizedCoords;
    unsigned int maxAnisotropy;
    enum hipTextureFilterMode mipmapFilterMode;
    float mipmapLevelBias;
    float minMipmapLevelClamp;
    float maxMipmapLevelClamp;
}hipTextureDesc;
#endif

typedef enum hipExternalMemoryHandleType_enum {
  hipExternalMemoryHandleTypeOpaqueFd = 1,
  hipExternalMemoryHandleTypeOpaqueWin32 = 2,
  hipExternalMemoryHandleTypeOpaqueWin32Kmt = 3,
  hipExternalMemoryHandleTypeD3D12Heap = 4,
  hipExternalMemoryHandleTypeD3D12Resource = 5,
  hipExternalMemoryHandleTypeD3D11Resource = 6,
  hipExternalMemoryHandleTypeD3D11ResourceKmt = 7,
  hipExternalMemoryHandleTypeNvSciBuf         = 8
} hipExternalMemoryHandleType;

typedef struct hipExternalMemoryHandleDesc_st {
  hipExternalMemoryHandleType type;
  union {
    int fd;
    struct {
      void *handle;
      const void *name;
    } win32;
    const void *nvSciBufObject;
  } handle;
  unsigned long long size;
  unsigned int flags;
  unsigned int reserved[16];
} hipExternalMemoryHandleDesc;

typedef struct hipExternalMemoryBufferDesc_st {
  unsigned long long offset;
  unsigned long long size;
  unsigned int flags;
  unsigned int reserved[16];
} hipExternalMemoryBufferDesc;


typedef hipError_t HIPAPI thipGetDevicePropertiesR0600(hipDeviceProp_t* props, int deviceId);
extern thipGetDevicePropertiesR0600 *hipGetDevicePropertiesR0600;

#define hipGetDeviceProperties hipGetDevicePropertiesR0600


#endif  /* __HIPEW_H__ */
