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

#ifndef __HIPEW5_H__
#define __HIPEW5_H__

#define WIN_DRIVER "amdhip64.dll"

typedef enum hipMemcpyKind {
    hipMemcpyHostToHost = 0,
    hipMemcpyHostToDevice = 1,
    hipMemcpyDeviceToHost = 2,
    hipMemcpyDeviceToDevice = 3,
    hipMemcpyDefault = 4
} hipMemcpyKind;

typedef enum hipMemoryType {
  hipMemoryTypeHost    = 0,
  hipMemoryTypeDevice  = 1,
  hipMemoryTypeArray   = 2,
  hipMemoryTypeUnified = 3,
  hipMemoryTypeManaged = 4
} hipMemoryType;

typedef struct HIP_POINTER_ATTRIBUTE_P2P_TOKENS_st {
  unsigned long long p2pToken;
  unsigned int vaSpaceToken;
} HIP_POINTER_ATTRIBUTE_P2P_TOKENS;


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
  hipDeviceAttributeName,                             ///< Device name.
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
  hipDeviceAttributeUuid,                             ///< Cuda only. Unique ID in 16 byte.
  hipDeviceAttributeWarpSize,                         ///< Warp size in threads.
  hipDeviceAttributeCudaCompatibleEnd = 9999,
  hipDeviceAttributeAmdSpecificBegin = 10000,
  hipDeviceAttributeClockInstructionRate = hipDeviceAttributeAmdSpecificBegin,  ///< Frequency in khz of the timer used by the device-side "clock*"
  hipDeviceAttributeArch,                                     ///< Device architecture
  hipDeviceAttributeMaxSharedMemoryPerMultiprocessor,         ///< Maximum Shared Memory PerMultiprocessor.
  hipDeviceAttributeGcnArch,                                  ///< Device gcn architecture
  hipDeviceAttributeGcnArchName,                              ///< Device gcnArch name in 256 bytes
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

typedef struct hipDeviceProp_t {
    char name[256];            ///< Device name.
    size_t totalGlobalMem;     ///< Size of global memory region (in bytes).
    size_t sharedMemPerBlock;  ///< Size of shared memory region (in bytes).
    int regsPerBlock;          ///< Registers per block.
    int warpSize;              ///< Warp size.
    int maxThreadsPerBlock;    ///< Max work items per work group or workgroup max size.
    int maxThreadsDim[3];      ///< Max number of threads in each dimension (XYZ) of a block.
    int maxGridSize[3];        ///< Max grid dimensions (XYZ).
    int clockRate;             ///< Max clock frequency of the multiProcessors in khz.
    int memoryClockRate;       ///< Max global memory clock frequency in khz.
    int memoryBusWidth;        ///< Global memory bus width in bits.
    size_t totalConstMem;      ///< Size of shared memory region (in bytes).
    int major;  ///< Major compute capability.  On HCC, this is an approximation and features may
                ///< differ from CUDA CC.  See the arch feature flags for portable ways to query
                ///< feature caps.
    int minor;  ///< Minor compute capability.  On HCC, this is an approximation and features may
                ///< differ from CUDA CC.  See the arch feature flags for portable ways to query
                ///< feature caps.
    int multiProcessorCount;          ///< Number of multi-processors (compute units).
    int l2CacheSize;                  ///< L2 cache size.
    int maxThreadsPerMultiProcessor;  ///< Maximum resident threads per multi-processor.
    int computeMode;                  ///< Compute mode.
    int clockInstructionRate;  ///< Frequency in khz of the timer used by the device-side "clock*"
                               ///< instructions.  New for HIP.
    hipDeviceArch_t arch;      ///< Architectural feature flags.  New for HIP.
    int concurrentKernels;     ///< Device can possibly execute multiple kernels concurrently.
    int pciDomainID;           ///< PCI Domain ID
    int pciBusID;              ///< PCI Bus ID.
    int pciDeviceID;           ///< PCI Device ID.
    size_t maxSharedMemoryPerMultiProcessor;  ///< Maximum Shared Memory Per Multiprocessor.
    int isMultiGpuBoard;                      ///< 1 if device is on a multi-GPU board, 0 if not.
    int canMapHostMemory;                     ///< Check whether HIP can map host memory
    int gcnArch;                              ///< DEPRECATED: use gcnArchName instead
    char gcnArchName[256];                    ///< AMD GCN Arch Name.
    int integrated;            ///< APU vs dGPU
    int cooperativeLaunch;            ///< HIP device supports cooperative launch
    int cooperativeMultiDeviceLaunch; ///< HIP device supports cooperative launch on multiple devices
    int maxTexture1DLinear;    ///< Maximum size for 1D textures bound to linear memory
    int maxTexture1D;          ///< Maximum number of elements in 1D images
    int maxTexture2D[2];       ///< Maximum dimensions (width, height) of 2D images, in image elements
    int maxTexture3D[3];       ///< Maximum dimensions (width, height, depth) of 3D images, in image elements
    unsigned int* hdpMemFlushCntl;      ///< Addres of HDP_MEM_COHERENCY_FLUSH_CNTL register
    unsigned int* hdpRegFlushCntl;      ///< Addres of HDP_REG_COHERENCY_FLUSH_CNTL register
    size_t memPitch;                 ///<Maximum pitch in bytes allowed by memory copies
    size_t textureAlignment;         ///<Alignment requirement for textures
    size_t texturePitchAlignment;    ///<Pitch alignment requirement for texture references bound to pitched memory
    int kernelExecTimeoutEnabled;    ///<Run time limit for kernels executed on the device
    int ECCEnabled;                  ///<Device has ECC support enabled
    int tccDriver;                   ///< 1:If device is Tesla device using TCC driver, else 0
    int cooperativeMultiDeviceUnmatchedFunc;        ///< HIP device supports cooperative launch on multiple
                                                    ///devices with unmatched functions
    int cooperativeMultiDeviceUnmatchedGridDim;     ///< HIP device supports cooperative launch on multiple
                                                    ///devices with unmatched grid dimensions
    int cooperativeMultiDeviceUnmatchedBlockDim;    ///< HIP device supports cooperative launch on multiple
                                                    ///devices with unmatched block dimensions
    int cooperativeMultiDeviceUnmatchedSharedMem;   ///< HIP device supports cooperative launch on multiple
                                                    ///devices with unmatched shared memories
    int isLargeBar;                  ///< 1: if it is a large PCI bar device, else 0
    int asicRevision;                ///< Revision of the GPU in this device
    int managedMemory;               ///< Device supports allocating managed memory on this system
    int directManagedMemAccessFromHost; ///< Host can directly access managed memory on the device without migration
    int concurrentManagedAccess;     ///< Device can coherently access managed memory concurrently with the CPU
    int pageableMemoryAccess;        ///< Device supports coherently accessing pageable memory
                                     ///< without calling hipHostRegister on it
    int pageableMemoryAccessUsesHostPageTables; ///< Device accesses pageable memory via the host's page tables
} hipDeviceProp_t;

typedef enum HIPpointer_attribute_enum {
  HIP_POINTER_ATTRIBUTE_CONTEXT = 1,
  HIP_POINTER_ATTRIBUTE_MEMORY_TYPE = 2,
  HIP_POINTER_ATTRIBUTE_DEVICE_POINTER = 3,
  HIP_POINTER_ATTRIBUTE_HOST_POINTER = 4,
  HIP_POINTER_ATTRIBUTE_SYNC_MEMOPS = 6,
  HIP_POINTER_ATTRIBUTE_BUFFER_ID = 7,
  HIP_POINTER_ATTRIBUTE_IS_MANAGED = 8,
  HIP_POINTER_ATTRIBUTE_DEVICE_ORDINAL = 9,
} HIPpointer_attribute;


typedef enum hipComputeMode {
  hipComputeModeDefault = 0,
  hipComputeModeProhibited = 2,
  hipComputeModeExclusiveProcess = 3,
} hipComputeMode;

typedef struct HIP_MEMCPY3D {
  unsigned int srcXInBytes;
  unsigned int srcY;
  unsigned int srcZ;
  unsigned int srcLOD;
  hipMemoryType srcMemoryType;
  const void* srcHost;
  hipDeviceptr_t srcDevice;
  hArray srcArray;
  unsigned int srcPitch;
  unsigned int srcHeight;
  unsigned int dstXInBytes;
  unsigned int dstY;
  unsigned int dstZ;
  unsigned int dstLOD;
  hipMemoryType dstMemoryType;
  void* dstHost;
  hipDeviceptr_t dstDevice;
  hArray dstArray;
  unsigned int dstPitch;
  unsigned int dstHeight;
  unsigned int WidthInBytes;
  unsigned int Height;
  unsigned int Depth;
} HIP_MEMCPY3D;


typedef enum hipExternalMemoryHandleType_enum {
  hipExternalMemoryHandleTypeOpaqueFd = 1,
  hipExternalMemoryHandleTypeOpaqueWin32 = 2,
  hipExternalMemoryHandleTypeOpaqueWin32Kmt = 3,
  hipExternalMemoryHandleTypeD3D12Heap = 4,
  hipExternalMemoryHandleTypeD3D12Resource = 5,
  hipExternalMemoryHandleTypeD3D11Resource = 6,
  hipExternalMemoryHandleTypeD3D11ResourceKmt = 7
} hipExternalMemoryHandleType;

typedef struct hipExternalMemoryHandleDesc_st {
  hipExternalMemoryHandleType type;
  union {
    int fd;
    struct {
      void *handle;
      const void *name;
    } win32;
  } handle;
  unsigned long long size;
  unsigned int flags;
} hipExternalMemoryHandleDesc;

typedef struct hipExternalMemoryBufferDesc_st {
  unsigned long long offset;
  unsigned long long size;
  unsigned int flags;
} hipExternalMemoryBufferDesc;

typedef hipError_t HIPAPI thipGetDeviceProperties(hipDeviceProp_t* props, int deviceId);
extern thipGetDeviceProperties *hipGetDeviceProperties;


#endif  /* __HIPEW_H__ */
