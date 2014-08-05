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

#ifdef _MSC_VER
#  define snprintf _snprintf
#  define popen _popen
#  define pclose _pclose
#  define _CRT_SECURE_NO_WARNINGS
#endif

#include <cuew.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define VC_EXTRALEAN
#  include <windows.h>

/* Utility macros. */

typedef HMODULE DynamicLibrary;

#  define dynamic_library_open(path)         LoadLibrary(path)
#  define dynamic_library_close(lib)         FreeLibrary(lib)
#  define dynamic_library_find(lib, symbol)  GetProcAddress(lib, symbol)
#else
#  include <dlfcn.h>

typedef void* DynamicLibrary;

#  define dynamic_library_open(path)         dlopen(path, RTLD_NOW)
#  define dynamic_library_close(lib)         dlclose(lib)
#  define dynamic_library_find(lib, symbol)  dlsym(lib, symbol)
#endif

#define CUDA_LIBRARY_FIND_CHECKED(name) \
        name = (t##name *)dynamic_library_find(lib, #name);

#define CUDA_LIBRARY_FIND(name) \
        name = (t##name *)dynamic_library_find(lib, #name); \
        assert(name);

static DynamicLibrary lib;

/* Function definitions. */
tcuGetErrorString *cuGetErrorString;
tcuGetErrorName *cuGetErrorName;
tcuInit *cuInit;
tcuDriverGetVersion *cuDriverGetVersion;
tcuDeviceGet *cuDeviceGet;
tcuDeviceGetCount *cuDeviceGetCount;
tcuDeviceGetName *cuDeviceGetName;
tcuDeviceTotalMem_v2 *cuDeviceTotalMem_v2;
tcuDeviceGetAttribute *cuDeviceGetAttribute;
tcuDeviceGetProperties *cuDeviceGetProperties;
tcuDeviceComputeCapability *cuDeviceComputeCapability;
tcuCtxCreate_v2 *cuCtxCreate_v2;
tcuCtxDestroy_v2 *cuCtxDestroy_v2;
tcuCtxPushCurrent_v2 *cuCtxPushCurrent_v2;
tcuCtxPopCurrent_v2 *cuCtxPopCurrent_v2;
tcuCtxSetCurrent *cuCtxSetCurrent;
tcuCtxGetCurrent *cuCtxGetCurrent;
tcuCtxGetDevice *cuCtxGetDevice;
tcuCtxSynchronize *cuCtxSynchronize;
tcuCtxSetLimit *cuCtxSetLimit;
tcuCtxGetLimit *cuCtxGetLimit;
tcuCtxGetCacheConfig *cuCtxGetCacheConfig;
tcuCtxSetCacheConfig *cuCtxSetCacheConfig;
tcuCtxGetSharedMemConfig *cuCtxGetSharedMemConfig;
tcuCtxSetSharedMemConfig *cuCtxSetSharedMemConfig;
tcuCtxGetApiVersion *cuCtxGetApiVersion;
tcuCtxGetStreamPriorityRange *cuCtxGetStreamPriorityRange;
tcuCtxAttach *cuCtxAttach;
tcuCtxDetach *cuCtxDetach;
tcuModuleLoad *cuModuleLoad;
tcuModuleLoadData *cuModuleLoadData;
tcuModuleLoadDataEx *cuModuleLoadDataEx;
tcuModuleLoadFatBinary *cuModuleLoadFatBinary;
tcuModuleUnload *cuModuleUnload;
tcuModuleGetFunction *cuModuleGetFunction;
tcuModuleGetGlobal_v2 *cuModuleGetGlobal_v2;
tcuModuleGetTexRef *cuModuleGetTexRef;
tcuModuleGetSurfRef *cuModuleGetSurfRef;
tcuLinkCreate *cuLinkCreate;
tcuLinkAddData *cuLinkAddData;
tcuLinkAddFile *cuLinkAddFile;
tcuLinkComplete *cuLinkComplete;
tcuLinkDestroy *cuLinkDestroy;
tcuMemGetInfo_v2 *cuMemGetInfo_v2;
tcuMemAlloc_v2 *cuMemAlloc_v2;
tcuMemAllocPitch_v2 *cuMemAllocPitch_v2;
tcuMemFree_v2 *cuMemFree_v2;
tcuMemGetAddressRange_v2 *cuMemGetAddressRange_v2;
tcuMemAllocHost_v2 *cuMemAllocHost_v2;
tcuMemFreeHost *cuMemFreeHost;
tcuMemHostAlloc *cuMemHostAlloc;
tcuMemHostGetDevicePointer_v2 *cuMemHostGetDevicePointer_v2;
tcuMemHostGetFlags *cuMemHostGetFlags;
tcuMemAllocManaged *cuMemAllocManaged;
tcuDeviceGetByPCIBusId *cuDeviceGetByPCIBusId;
tcuDeviceGetPCIBusId *cuDeviceGetPCIBusId;
tcuIpcGetEventHandle *cuIpcGetEventHandle;
tcuIpcOpenEventHandle *cuIpcOpenEventHandle;
tcuIpcGetMemHandle *cuIpcGetMemHandle;
tcuIpcOpenMemHandle *cuIpcOpenMemHandle;
tcuIpcCloseMemHandle *cuIpcCloseMemHandle;
tcuMemHostRegister *cuMemHostRegister;
tcuMemHostUnregister *cuMemHostUnregister;
tcuMemcpy *cuMemcpy;
tcuMemcpyPeer *cuMemcpyPeer;
tcuMemcpyHtoD_v2 *cuMemcpyHtoD_v2;
tcuMemcpyDtoH_v2 *cuMemcpyDtoH_v2;
tcuMemcpyDtoD_v2 *cuMemcpyDtoD_v2;
tcuMemcpyDtoA_v2 *cuMemcpyDtoA_v2;
tcuMemcpyAtoD_v2 *cuMemcpyAtoD_v2;
tcuMemcpyHtoA_v2 *cuMemcpyHtoA_v2;
tcuMemcpyAtoH_v2 *cuMemcpyAtoH_v2;
tcuMemcpyAtoA_v2 *cuMemcpyAtoA_v2;
tcuMemcpy2D_v2 *cuMemcpy2D_v2;
tcuMemcpy2DUnaligned_v2 *cuMemcpy2DUnaligned_v2;
tcuMemcpy3D_v2 *cuMemcpy3D_v2;
tcuMemcpy3DPeer *cuMemcpy3DPeer;
tcuMemcpyAsync *cuMemcpyAsync;
tcuMemcpyPeerAsync *cuMemcpyPeerAsync;
tcuMemcpyHtoDAsync_v2 *cuMemcpyHtoDAsync_v2;
tcuMemcpyDtoHAsync_v2 *cuMemcpyDtoHAsync_v2;
tcuMemcpyDtoDAsync_v2 *cuMemcpyDtoDAsync_v2;
tcuMemcpyHtoAAsync_v2 *cuMemcpyHtoAAsync_v2;
tcuMemcpyAtoHAsync_v2 *cuMemcpyAtoHAsync_v2;
tcuMemcpy2DAsync_v2 *cuMemcpy2DAsync_v2;
tcuMemcpy3DAsync_v2 *cuMemcpy3DAsync_v2;
tcuMemcpy3DPeerAsync *cuMemcpy3DPeerAsync;
tcuMemsetD8_v2 *cuMemsetD8_v2;
tcuMemsetD16_v2 *cuMemsetD16_v2;
tcuMemsetD32_v2 *cuMemsetD32_v2;
tcuMemsetD2D8_v2 *cuMemsetD2D8_v2;
tcuMemsetD2D16_v2 *cuMemsetD2D16_v2;
tcuMemsetD2D32_v2 *cuMemsetD2D32_v2;
tcuMemsetD8Async *cuMemsetD8Async;
tcuMemsetD16Async *cuMemsetD16Async;
tcuMemsetD32Async *cuMemsetD32Async;
tcuMemsetD2D8Async *cuMemsetD2D8Async;
tcuMemsetD2D16Async *cuMemsetD2D16Async;
tcuMemsetD2D32Async *cuMemsetD2D32Async;
tcuArrayCreate_v2 *cuArrayCreate_v2;
tcuArrayGetDescriptor_v2 *cuArrayGetDescriptor_v2;
tcuArrayDestroy *cuArrayDestroy;
tcuArray3DCreate_v2 *cuArray3DCreate_v2;
tcuArray3DGetDescriptor_v2 *cuArray3DGetDescriptor_v2;
tcuMipmappedArrayCreate *cuMipmappedArrayCreate;
tcuMipmappedArrayGetLevel *cuMipmappedArrayGetLevel;
tcuMipmappedArrayDestroy *cuMipmappedArrayDestroy;
tcuPointerGetAttribute *cuPointerGetAttribute;
tcuPointerSetAttribute *cuPointerSetAttribute;
tcuStreamCreate *cuStreamCreate;
tcuStreamCreateWithPriority *cuStreamCreateWithPriority;
tcuStreamGetPriority *cuStreamGetPriority;
tcuStreamGetFlags *cuStreamGetFlags;
tcuStreamWaitEvent *cuStreamWaitEvent;
tcuStreamAddCallback *cuStreamAddCallback;
tcuStreamAttachMemAsync *cuStreamAttachMemAsync;
tcuStreamQuery *cuStreamQuery;
tcuStreamSynchronize *cuStreamSynchronize;
tcuStreamDestroy_v2 *cuStreamDestroy_v2;
tcuEventCreate *cuEventCreate;
tcuEventRecord *cuEventRecord;
tcuEventQuery *cuEventQuery;
tcuEventSynchronize *cuEventSynchronize;
tcuEventDestroy_v2 *cuEventDestroy_v2;
tcuEventElapsedTime *cuEventElapsedTime;
tcuFuncGetAttribute *cuFuncGetAttribute;
tcuFuncSetCacheConfig *cuFuncSetCacheConfig;
tcuFuncSetSharedMemConfig *cuFuncSetSharedMemConfig;
tcuLaunchKernel *cuLaunchKernel;
tcuFuncSetBlockShape *cuFuncSetBlockShape;
tcuFuncSetSharedSize *cuFuncSetSharedSize;
tcuParamSetSize *cuParamSetSize;
tcuParamSeti *cuParamSeti;
tcuParamSetf *cuParamSetf;
tcuParamSetv *cuParamSetv;
tcuLaunch *cuLaunch;
tcuLaunchGrid *cuLaunchGrid;
tcuLaunchGridAsync *cuLaunchGridAsync;
tcuParamSetTexRef *cuParamSetTexRef;
tcuTexRefSetArray *cuTexRefSetArray;
tcuTexRefSetMipmappedArray *cuTexRefSetMipmappedArray;
tcuTexRefSetAddress_v2 *cuTexRefSetAddress_v2;
tcuTexRefSetAddress2D_v3 *cuTexRefSetAddress2D_v3;
tcuTexRefSetFormat *cuTexRefSetFormat;
tcuTexRefSetAddressMode *cuTexRefSetAddressMode;
tcuTexRefSetFilterMode *cuTexRefSetFilterMode;
tcuTexRefSetMipmapFilterMode *cuTexRefSetMipmapFilterMode;
tcuTexRefSetMipmapLevelBias *cuTexRefSetMipmapLevelBias;
tcuTexRefSetMipmapLevelClamp *cuTexRefSetMipmapLevelClamp;
tcuTexRefSetMaxAnisotropy *cuTexRefSetMaxAnisotropy;
tcuTexRefSetFlags *cuTexRefSetFlags;
tcuTexRefGetAddress_v2 *cuTexRefGetAddress_v2;
tcuTexRefGetArray *cuTexRefGetArray;
tcuTexRefGetMipmappedArray *cuTexRefGetMipmappedArray;
tcuTexRefGetAddressMode *cuTexRefGetAddressMode;
tcuTexRefGetFilterMode *cuTexRefGetFilterMode;
tcuTexRefGetFormat *cuTexRefGetFormat;
tcuTexRefGetMipmapFilterMode *cuTexRefGetMipmapFilterMode;
tcuTexRefGetMipmapLevelBias *cuTexRefGetMipmapLevelBias;
tcuTexRefGetMipmapLevelClamp *cuTexRefGetMipmapLevelClamp;
tcuTexRefGetMaxAnisotropy *cuTexRefGetMaxAnisotropy;
tcuTexRefGetFlags *cuTexRefGetFlags;
tcuTexRefCreate *cuTexRefCreate;
tcuTexRefDestroy *cuTexRefDestroy;
tcuSurfRefSetArray *cuSurfRefSetArray;
tcuSurfRefGetArray *cuSurfRefGetArray;
tcuTexObjectCreate *cuTexObjectCreate;
tcuTexObjectDestroy *cuTexObjectDestroy;
tcuTexObjectGetResourceDesc *cuTexObjectGetResourceDesc;
tcuTexObjectGetTextureDesc *cuTexObjectGetTextureDesc;
tcuTexObjectGetResourceViewDesc *cuTexObjectGetResourceViewDesc;
tcuSurfObjectCreate *cuSurfObjectCreate;
tcuSurfObjectDestroy *cuSurfObjectDestroy;
tcuSurfObjectGetResourceDesc *cuSurfObjectGetResourceDesc;
tcuDeviceCanAccessPeer *cuDeviceCanAccessPeer;
tcuCtxEnablePeerAccess *cuCtxEnablePeerAccess;
tcuCtxDisablePeerAccess *cuCtxDisablePeerAccess;
tcuGraphicsUnregisterResource *cuGraphicsUnregisterResource;
tcuGraphicsSubResourceGetMappedArray *cuGraphicsSubResourceGetMappedArray;
tcuGraphicsResourceGetMappedMipmappedArray *cuGraphicsResourceGetMappedMipmappedArray;
tcuGraphicsResourceGetMappedPointer_v2 *cuGraphicsResourceGetMappedPointer_v2;
tcuGraphicsResourceSetMapFlags *cuGraphicsResourceSetMapFlags;
tcuGraphicsMapResources *cuGraphicsMapResources;
tcuGraphicsUnmapResources *cuGraphicsUnmapResources;
tcuGetExportTable *cuGetExportTable;

tcuGraphicsGLRegisterBuffer *cuGraphicsGLRegisterBuffer;
tcuGraphicsGLRegisterImage *cuGraphicsGLRegisterImage;
tcuGLGetDevices *cuGLGetDevices;
tcuGLCtxCreate_v2 *cuGLCtxCreate_v2;
tcuGLInit *cuGLInit;
tcuGLRegisterBufferObject *cuGLRegisterBufferObject;
tcuGLMapBufferObject_v2 *cuGLMapBufferObject_v2;
tcuGLUnmapBufferObject *cuGLUnmapBufferObject;
tcuGLUnregisterBufferObject *cuGLUnregisterBufferObject;
tcuGLSetBufferObjectMapFlags *cuGLSetBufferObjectMapFlags;
tcuGLMapBufferObjectAsync_v2 *cuGLMapBufferObjectAsync_v2;
tcuGLUnmapBufferObjectAsync *cuGLUnmapBufferObjectAsync;


static void cuewExit(void) {
  if(lib != NULL) {
    /*  Ignore errors. */
    dynamic_library_close(lib);
    lib = NULL;
  }
}

/* Implementation function. */
int cuewInit(void) {
  /* Library paths. */
#ifdef _WIN32
  /* Expected in c:/windows/system or similar, no path needed. */
  const char *path = "nvcuda.dll";
#elif defined(__APPLE__)
  /* Default installation path. */
  const char *path = "/usr/local/cuda/lib/libcuda.dylib";
#else
  const char *path = "libcuda.so";
#endif
  static int initialized = 0;
  static int result = 0;
  int error, driver_version;

  if (initialized) {
    return result;
  }

  initialized = 1;

  error = atexit(cuewExit);
  if (error) {
    result = CUEW_ERROR_ATEXIT_FAILED;
    return result;
  }

  /* Load library. */
  lib = dynamic_library_open(path);

  if (lib == NULL) {
    result = CUEW_ERROR_OPEN_FAILED;
    return result;
  }

  /* Detect driver version. */
  driver_version = 1000;

  CUDA_LIBRARY_FIND_CHECKED(cuDriverGetVersion);
  if (cuDriverGetVersion) {
    cuDriverGetVersion(&driver_version);
  }

  /* We require version 4.0. */
  if (driver_version < 4000) {
    result = CUEW_ERROR_OPEN_FAILED;
    return result;
  }
  /* Fetch all function pointers. */
  CUDA_LIBRARY_FIND(cuGetErrorString);
  CUDA_LIBRARY_FIND(cuGetErrorName);
  CUDA_LIBRARY_FIND(cuInit);
  CUDA_LIBRARY_FIND(cuDriverGetVersion);
  CUDA_LIBRARY_FIND(cuDeviceGet);
  CUDA_LIBRARY_FIND(cuDeviceGetCount);
  CUDA_LIBRARY_FIND(cuDeviceGetName);
  CUDA_LIBRARY_FIND(cuDeviceTotalMem_v2);
  CUDA_LIBRARY_FIND(cuDeviceGetAttribute);
  CUDA_LIBRARY_FIND(cuDeviceGetProperties);
  CUDA_LIBRARY_FIND(cuDeviceComputeCapability);
  CUDA_LIBRARY_FIND(cuCtxCreate_v2);
  CUDA_LIBRARY_FIND(cuCtxDestroy_v2);
  CUDA_LIBRARY_FIND(cuCtxPushCurrent_v2);
  CUDA_LIBRARY_FIND(cuCtxPopCurrent_v2);
  CUDA_LIBRARY_FIND(cuCtxSetCurrent);
  CUDA_LIBRARY_FIND(cuCtxGetCurrent);
  CUDA_LIBRARY_FIND(cuCtxGetDevice);
  CUDA_LIBRARY_FIND(cuCtxSynchronize);
  CUDA_LIBRARY_FIND(cuCtxSetLimit);
  CUDA_LIBRARY_FIND(cuCtxGetLimit);
  CUDA_LIBRARY_FIND(cuCtxGetCacheConfig);
  CUDA_LIBRARY_FIND(cuCtxSetCacheConfig);
  CUDA_LIBRARY_FIND(cuCtxGetSharedMemConfig);
  CUDA_LIBRARY_FIND(cuCtxSetSharedMemConfig);
  CUDA_LIBRARY_FIND(cuCtxGetApiVersion);
  CUDA_LIBRARY_FIND(cuCtxGetStreamPriorityRange);
  CUDA_LIBRARY_FIND(cuCtxAttach);
  CUDA_LIBRARY_FIND(cuCtxDetach);
  CUDA_LIBRARY_FIND(cuModuleLoad);
  CUDA_LIBRARY_FIND(cuModuleLoadData);
  CUDA_LIBRARY_FIND(cuModuleLoadDataEx);
  CUDA_LIBRARY_FIND(cuModuleLoadFatBinary);
  CUDA_LIBRARY_FIND(cuModuleUnload);
  CUDA_LIBRARY_FIND(cuModuleGetFunction);
  CUDA_LIBRARY_FIND(cuModuleGetGlobal_v2);
  CUDA_LIBRARY_FIND(cuModuleGetTexRef);
  CUDA_LIBRARY_FIND(cuModuleGetSurfRef);
  CUDA_LIBRARY_FIND(cuLinkCreate);
  CUDA_LIBRARY_FIND(cuLinkAddData);
  CUDA_LIBRARY_FIND(cuLinkAddFile);
  CUDA_LIBRARY_FIND(cuLinkComplete);
  CUDA_LIBRARY_FIND(cuLinkDestroy);
  CUDA_LIBRARY_FIND(cuMemGetInfo_v2);
  CUDA_LIBRARY_FIND(cuMemAlloc_v2);
  CUDA_LIBRARY_FIND(cuMemAllocPitch_v2);
  CUDA_LIBRARY_FIND(cuMemFree_v2);
  CUDA_LIBRARY_FIND(cuMemGetAddressRange_v2);
  CUDA_LIBRARY_FIND(cuMemAllocHost_v2);
  CUDA_LIBRARY_FIND(cuMemFreeHost);
  CUDA_LIBRARY_FIND(cuMemHostAlloc);
  CUDA_LIBRARY_FIND(cuMemHostGetDevicePointer_v2);
  CUDA_LIBRARY_FIND(cuMemHostGetFlags);
  CUDA_LIBRARY_FIND(cuMemAllocManaged);
  CUDA_LIBRARY_FIND(cuDeviceGetByPCIBusId);
  CUDA_LIBRARY_FIND(cuDeviceGetPCIBusId);
  CUDA_LIBRARY_FIND(cuIpcGetEventHandle);
  CUDA_LIBRARY_FIND(cuIpcOpenEventHandle);
  CUDA_LIBRARY_FIND(cuIpcGetMemHandle);
  CUDA_LIBRARY_FIND(cuIpcOpenMemHandle);
  CUDA_LIBRARY_FIND(cuIpcCloseMemHandle);
  CUDA_LIBRARY_FIND(cuMemHostRegister);
  CUDA_LIBRARY_FIND(cuMemHostUnregister);
  CUDA_LIBRARY_FIND(cuMemcpy);
  CUDA_LIBRARY_FIND(cuMemcpyPeer);
  CUDA_LIBRARY_FIND(cuMemcpyHtoD_v2);
  CUDA_LIBRARY_FIND(cuMemcpyDtoH_v2);
  CUDA_LIBRARY_FIND(cuMemcpyDtoD_v2);
  CUDA_LIBRARY_FIND(cuMemcpyDtoA_v2);
  CUDA_LIBRARY_FIND(cuMemcpyAtoD_v2);
  CUDA_LIBRARY_FIND(cuMemcpyHtoA_v2);
  CUDA_LIBRARY_FIND(cuMemcpyAtoH_v2);
  CUDA_LIBRARY_FIND(cuMemcpyAtoA_v2);
  CUDA_LIBRARY_FIND(cuMemcpy2D_v2);
  CUDA_LIBRARY_FIND(cuMemcpy2DUnaligned_v2);
  CUDA_LIBRARY_FIND(cuMemcpy3D_v2);
  CUDA_LIBRARY_FIND(cuMemcpy3DPeer);
  CUDA_LIBRARY_FIND(cuMemcpyAsync);
  CUDA_LIBRARY_FIND(cuMemcpyPeerAsync);
  CUDA_LIBRARY_FIND(cuMemcpyHtoDAsync_v2);
  CUDA_LIBRARY_FIND(cuMemcpyDtoHAsync_v2);
  CUDA_LIBRARY_FIND(cuMemcpyDtoDAsync_v2);
  CUDA_LIBRARY_FIND(cuMemcpyHtoAAsync_v2);
  CUDA_LIBRARY_FIND(cuMemcpyAtoHAsync_v2);
  CUDA_LIBRARY_FIND(cuMemcpy2DAsync_v2);
  CUDA_LIBRARY_FIND(cuMemcpy3DAsync_v2);
  CUDA_LIBRARY_FIND(cuMemcpy3DPeerAsync);
  CUDA_LIBRARY_FIND(cuMemsetD8_v2);
  CUDA_LIBRARY_FIND(cuMemsetD16_v2);
  CUDA_LIBRARY_FIND(cuMemsetD32_v2);
  CUDA_LIBRARY_FIND(cuMemsetD2D8_v2);
  CUDA_LIBRARY_FIND(cuMemsetD2D16_v2);
  CUDA_LIBRARY_FIND(cuMemsetD2D32_v2);
  CUDA_LIBRARY_FIND(cuMemsetD8Async);
  CUDA_LIBRARY_FIND(cuMemsetD16Async);
  CUDA_LIBRARY_FIND(cuMemsetD32Async);
  CUDA_LIBRARY_FIND(cuMemsetD2D8Async);
  CUDA_LIBRARY_FIND(cuMemsetD2D16Async);
  CUDA_LIBRARY_FIND(cuMemsetD2D32Async);
  CUDA_LIBRARY_FIND(cuArrayCreate_v2);
  CUDA_LIBRARY_FIND(cuArrayGetDescriptor_v2);
  CUDA_LIBRARY_FIND(cuArrayDestroy);
  CUDA_LIBRARY_FIND(cuArray3DCreate_v2);
  CUDA_LIBRARY_FIND(cuArray3DGetDescriptor_v2);
  CUDA_LIBRARY_FIND(cuMipmappedArrayCreate);
  CUDA_LIBRARY_FIND(cuMipmappedArrayGetLevel);
  CUDA_LIBRARY_FIND(cuMipmappedArrayDestroy);
  CUDA_LIBRARY_FIND(cuPointerGetAttribute);
  CUDA_LIBRARY_FIND(cuPointerSetAttribute);
  CUDA_LIBRARY_FIND(cuStreamCreate);
  CUDA_LIBRARY_FIND(cuStreamCreateWithPriority);
  CUDA_LIBRARY_FIND(cuStreamGetPriority);
  CUDA_LIBRARY_FIND(cuStreamGetFlags);
  CUDA_LIBRARY_FIND(cuStreamWaitEvent);
  CUDA_LIBRARY_FIND(cuStreamAddCallback);
  CUDA_LIBRARY_FIND(cuStreamAttachMemAsync);
  CUDA_LIBRARY_FIND(cuStreamQuery);
  CUDA_LIBRARY_FIND(cuStreamSynchronize);
  CUDA_LIBRARY_FIND(cuStreamDestroy_v2);
  CUDA_LIBRARY_FIND(cuEventCreate);
  CUDA_LIBRARY_FIND(cuEventRecord);
  CUDA_LIBRARY_FIND(cuEventQuery);
  CUDA_LIBRARY_FIND(cuEventSynchronize);
  CUDA_LIBRARY_FIND(cuEventDestroy_v2);
  CUDA_LIBRARY_FIND(cuEventElapsedTime);
  CUDA_LIBRARY_FIND(cuFuncGetAttribute);
  CUDA_LIBRARY_FIND(cuFuncSetCacheConfig);
  CUDA_LIBRARY_FIND(cuFuncSetSharedMemConfig);
  CUDA_LIBRARY_FIND(cuLaunchKernel);
  CUDA_LIBRARY_FIND(cuFuncSetBlockShape);
  CUDA_LIBRARY_FIND(cuFuncSetSharedSize);
  CUDA_LIBRARY_FIND(cuParamSetSize);
  CUDA_LIBRARY_FIND(cuParamSeti);
  CUDA_LIBRARY_FIND(cuParamSetf);
  CUDA_LIBRARY_FIND(cuParamSetv);
  CUDA_LIBRARY_FIND(cuLaunch);
  CUDA_LIBRARY_FIND(cuLaunchGrid);
  CUDA_LIBRARY_FIND(cuLaunchGridAsync);
  CUDA_LIBRARY_FIND(cuParamSetTexRef);
  CUDA_LIBRARY_FIND(cuTexRefSetArray);
  CUDA_LIBRARY_FIND(cuTexRefSetMipmappedArray);
  CUDA_LIBRARY_FIND(cuTexRefSetAddress_v2);
  CUDA_LIBRARY_FIND(cuTexRefSetAddress2D_v3);
  CUDA_LIBRARY_FIND(cuTexRefSetFormat);
  CUDA_LIBRARY_FIND(cuTexRefSetAddressMode);
  CUDA_LIBRARY_FIND(cuTexRefSetFilterMode);
  CUDA_LIBRARY_FIND(cuTexRefSetMipmapFilterMode);
  CUDA_LIBRARY_FIND(cuTexRefSetMipmapLevelBias);
  CUDA_LIBRARY_FIND(cuTexRefSetMipmapLevelClamp);
  CUDA_LIBRARY_FIND(cuTexRefSetMaxAnisotropy);
  CUDA_LIBRARY_FIND(cuTexRefSetFlags);
  CUDA_LIBRARY_FIND(cuTexRefGetAddress_v2);
  CUDA_LIBRARY_FIND(cuTexRefGetArray);
  CUDA_LIBRARY_FIND(cuTexRefGetMipmappedArray);
  CUDA_LIBRARY_FIND(cuTexRefGetAddressMode);
  CUDA_LIBRARY_FIND(cuTexRefGetFilterMode);
  CUDA_LIBRARY_FIND(cuTexRefGetFormat);
  CUDA_LIBRARY_FIND(cuTexRefGetMipmapFilterMode);
  CUDA_LIBRARY_FIND(cuTexRefGetMipmapLevelBias);
  CUDA_LIBRARY_FIND(cuTexRefGetMipmapLevelClamp);
  CUDA_LIBRARY_FIND(cuTexRefGetMaxAnisotropy);
  CUDA_LIBRARY_FIND(cuTexRefGetFlags);
  CUDA_LIBRARY_FIND(cuTexRefCreate);
  CUDA_LIBRARY_FIND(cuTexRefDestroy);
  CUDA_LIBRARY_FIND(cuSurfRefSetArray);
  CUDA_LIBRARY_FIND(cuSurfRefGetArray);
  CUDA_LIBRARY_FIND(cuTexObjectCreate);
  CUDA_LIBRARY_FIND(cuTexObjectDestroy);
  CUDA_LIBRARY_FIND(cuTexObjectGetResourceDesc);
  CUDA_LIBRARY_FIND(cuTexObjectGetTextureDesc);
  CUDA_LIBRARY_FIND(cuTexObjectGetResourceViewDesc);
  CUDA_LIBRARY_FIND(cuSurfObjectCreate);
  CUDA_LIBRARY_FIND(cuSurfObjectDestroy);
  CUDA_LIBRARY_FIND(cuSurfObjectGetResourceDesc);
  CUDA_LIBRARY_FIND(cuDeviceCanAccessPeer);
  CUDA_LIBRARY_FIND(cuCtxEnablePeerAccess);
  CUDA_LIBRARY_FIND(cuCtxDisablePeerAccess);
  CUDA_LIBRARY_FIND(cuGraphicsUnregisterResource);
  CUDA_LIBRARY_FIND(cuGraphicsSubResourceGetMappedArray);
  CUDA_LIBRARY_FIND(cuGraphicsResourceGetMappedMipmappedArray);
  CUDA_LIBRARY_FIND(cuGraphicsResourceGetMappedPointer_v2);
  CUDA_LIBRARY_FIND(cuGraphicsResourceSetMapFlags);
  CUDA_LIBRARY_FIND(cuGraphicsMapResources);
  CUDA_LIBRARY_FIND(cuGraphicsUnmapResources);
  CUDA_LIBRARY_FIND(cuGetExportTable);

  CUDA_LIBRARY_FIND(cuGraphicsGLRegisterBuffer);
  CUDA_LIBRARY_FIND(cuGraphicsGLRegisterImage);
  CUDA_LIBRARY_FIND(cuGLGetDevices);
  CUDA_LIBRARY_FIND(cuGLCtxCreate_v2);
  CUDA_LIBRARY_FIND(cuGLInit);
  CUDA_LIBRARY_FIND(cuGLRegisterBufferObject);
  CUDA_LIBRARY_FIND(cuGLMapBufferObject_v2);
  CUDA_LIBRARY_FIND(cuGLUnmapBufferObject);
  CUDA_LIBRARY_FIND(cuGLUnregisterBufferObject);
  CUDA_LIBRARY_FIND(cuGLSetBufferObjectMapFlags);
  CUDA_LIBRARY_FIND(cuGLMapBufferObjectAsync_v2);
  CUDA_LIBRARY_FIND(cuGLUnmapBufferObjectAsync);


  result = CUEW_SUCCESS;
  return result;
}

const char *cuewErrorString(CUresult result) {
  switch(result) {
    case CUDA_SUCCESS: return "No errors";
    case CUDA_ERROR_INVALID_VALUE: return "Invalid value";
    case CUDA_ERROR_OUT_OF_MEMORY: return "Out of memory";
    case CUDA_ERROR_NOT_INITIALIZED: return "Driver not initialized";
    case CUDA_ERROR_DEINITIALIZED: return "Driver deinitialized";
    case CUDA_ERROR_PROFILER_DISABLED: return "PROFILER_DISABLED";
    case CUDA_ERROR_PROFILER_NOT_INITIALIZED: return "PROFILER_NOT_INITIALIZED";
    case CUDA_ERROR_PROFILER_ALREADY_STARTED: return "PROFILER_ALREADY_STARTED";
    case CUDA_ERROR_PROFILER_ALREADY_STOPPED: return "PROFILER_ALREADY_STOPPED";
    case CUDA_ERROR_NO_DEVICE: return "No CUDA-capable device available";
    case CUDA_ERROR_INVALID_DEVICE: return "Invalid device";
    case CUDA_ERROR_INVALID_IMAGE: return "Invalid kernel image";
    case CUDA_ERROR_INVALID_CONTEXT: return "Invalid context";
    case CUDA_ERROR_CONTEXT_ALREADY_CURRENT: return "Context already current";
    case CUDA_ERROR_MAP_FAILED: return "Map failed";
    case CUDA_ERROR_UNMAP_FAILED: return "Unmap failed";
    case CUDA_ERROR_ARRAY_IS_MAPPED: return "Array is mapped";
    case CUDA_ERROR_ALREADY_MAPPED: return "Already mapped";
    case CUDA_ERROR_NO_BINARY_FOR_GPU: return "No binary for GPU";
    case CUDA_ERROR_ALREADY_ACQUIRED: return "Already acquired";
    case CUDA_ERROR_NOT_MAPPED: return "Not mapped";
    case CUDA_ERROR_NOT_MAPPED_AS_ARRAY: return "Mapped resource not available for access as an array";
    case CUDA_ERROR_NOT_MAPPED_AS_POINTER: return "Mapped resource not available for access as a pointer";
    case CUDA_ERROR_ECC_UNCORRECTABLE: return "Uncorrectable ECC error detected";
    case CUDA_ERROR_UNSUPPORTED_LIMIT: return "CUlimit not supported by device";
    case CUDA_ERROR_CONTEXT_ALREADY_IN_USE: return "CONTEXT_ALREADY_IN_USE";
    case CUDA_ERROR_PEER_ACCESS_UNSUPPORTED: return "PEER_ACCESS_UNSUPPORTED";
    case CUDA_ERROR_INVALID_PTX: return "INVALID_PTX";
    case CUDA_ERROR_INVALID_SOURCE: return "Invalid source";
    case CUDA_ERROR_FILE_NOT_FOUND: return "File not found";
    case CUDA_ERROR_SHARED_OBJECT_SYMBOL_NOT_FOUND: return "Link to a shared object failed to resolve";
    case CUDA_ERROR_SHARED_OBJECT_INIT_FAILED: return "Shared object initialization failed";
    case CUDA_ERROR_OPERATING_SYSTEM: return "OPERATING_SYSTEM";
    case CUDA_ERROR_INVALID_HANDLE: return "Invalid handle";
    case CUDA_ERROR_NOT_FOUND: return "Not found";
    case CUDA_ERROR_NOT_READY: return "CUDA not ready";
    case CUDA_ERROR_ILLEGAL_ADDRESS: return "ILLEGAL_ADDRESS";
    case CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES: return "Launch exceeded resources";
    case CUDA_ERROR_LAUNCH_TIMEOUT: return "Launch exceeded timeout";
    case CUDA_ERROR_LAUNCH_INCOMPATIBLE_TEXTURING: return "Launch with incompatible texturing";
    case CUDA_ERROR_PEER_ACCESS_ALREADY_ENABLED: return "PEER_ACCESS_ALREADY_ENABLED";
    case CUDA_ERROR_PEER_ACCESS_NOT_ENABLED: return "PEER_ACCESS_NOT_ENABLED";
    case CUDA_ERROR_PRIMARY_CONTEXT_ACTIVE: return "PRIMARY_CONTEXT_ACTIVE";
    case CUDA_ERROR_CONTEXT_IS_DESTROYED: return "CONTEXT_IS_DESTROYED";
    case CUDA_ERROR_ASSERT: return "ASSERT";
    case CUDA_ERROR_TOO_MANY_PEERS: return "TOO_MANY_PEERS";
    case CUDA_ERROR_HOST_MEMORY_ALREADY_REGISTERED: return "HOST_MEMORY_ALREADY_REGISTERED";
    case CUDA_ERROR_HOST_MEMORY_NOT_REGISTERED: return "HOST_MEMORY_NOT_REGISTERED";
    case CUDA_ERROR_HARDWARE_STACK_ERROR: return "HARDWARE_STACK_ERROR";
    case CUDA_ERROR_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
    case CUDA_ERROR_MISALIGNED_ADDRESS: return "MISALIGNED_ADDRESS";
    case CUDA_ERROR_INVALID_ADDRESS_SPACE: return "INVALID_ADDRESS_SPACE";
    case CUDA_ERROR_INVALID_PC: return "INVALID_PC";
    case CUDA_ERROR_LAUNCH_FAILED: return "Launch failed";
    case CUDA_ERROR_NOT_PERMITTED: return "NOT_PERMITTED";
    case CUDA_ERROR_NOT_SUPPORTED: return "NOT_SUPPORTED";
    case CUDA_ERROR_UNKNOWN: return "Unknown error";
    default: return "Unknown CUDA error value";
  }
}

static void path_join(const char *path1,
                      const char *path2,
                      int maxlen,
                      char *result) {
#if defined(WIN32) || defined(_WIN32)
  const char separator = '\\';
#else
  const char separator = '/';
#endif
  int n = snprintf(result, maxlen, "%s%c%s", path1, separator, path2);
  if (n != -1 && n < maxlen) {
    result[n] = '\0';
  }
  else {
    result[maxlen - 1] = '\0';
  }
}

static int path_exists(const char *path) {
  struct stat st;
  if (stat(path, &st)) {
    return 0;
  }
  return 1;
}

const char *cuewCompilerPath(void) {
#ifdef _WIN32
  const char *defaultpaths[] = {"C:/CUDA/bin", NULL};
  const char *executable = "nvcc.exe";
#else
  const char *defaultpaths[] = {
    "/Developer/NVIDIA/CUDA-5.0/bin",
    "/usr/local/cuda-5.0/bin",
    "/usr/local/cuda/bin",
    "/Developer/NVIDIA/CUDA-6.0/bin",
    "/usr/local/cuda-6.0/bin",
    "/Developer/NVIDIA/CUDA-5.5/bin",
    "/usr/local/cuda-5.5/bin",
    NULL};
  const char *executable = "nvcc";
#endif
  int i;

  const char *binpath = getenv("CUDA_BIN_PATH");

  static char nvcc[65536];

  if (binpath) {
    path_join(binpath, executable, sizeof(nvcc), nvcc);
    if (path_exists(nvcc))
      return nvcc;
  }

  for (i = 0; defaultpaths[i]; ++i) {
    path_join(defaultpaths[i], executable, sizeof(nvcc), nvcc);
    if (path_exists(nvcc))
      return nvcc;
  }

#ifndef _WIN32
  {
    FILE *handle = popen("which nvcc", "r");
    if (handle) {
      char buffer[4096] = {0};
      int len = fread(buffer, 1, sizeof(buffer) - 1, handle);
      buffer[len] = '\0';
      pclose(handle);

      if (buffer[0])
        return "nvcc";
    }
  }
#endif

  return NULL;
}

int cuewCompilerVersion(void) {
  const char *path = cuewCompilerPath();
  const char *marker = "Cuda compilation tools, release ";
  FILE *pipe;
  int major, minor;
  char *versionstr;
  char buf[128];
  char output[65536] = "\0";
  char command[65536] = "\0";

  if (path == NULL)
    return 0;

  /* get --version output */
  strncpy(command, path, sizeof(command));
  strncat(command, " --version", sizeof(command) - strlen(path));
  pipe = popen(command, "r");
  if (!pipe) {
    fprintf(stderr, "CUDA: failed to run compiler to retrieve version");
    return 0;
  }

  while (!feof(pipe)) {
    if (fgets(buf, sizeof(buf), pipe) != NULL) {
      strncat(output, buf, sizeof(output) - strlen(output));
    }
  }

  pclose(pipe);

  /* parse version number */
  versionstr = strstr(output, marker);
  if (versionstr == NULL) {
    fprintf(stderr, "CUDA: failed to find version number in:\n\n%s\n", output);
    return 0;
  }
  versionstr += strlen(marker);

  if (sscanf(versionstr, "%d.%d", &major, &minor) < 2) {
    fprintf(stderr, "CUDA: failed to parse version number from:\n\n%s\n", output);
    return 0;
  }

  return 10 * major + minor;
}

