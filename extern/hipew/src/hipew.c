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
#ifdef _MSC_VER
#  if _MSC_VER < 1900
#    define snprintf _snprintf
#  endif
#  define popen _popen
#  define pclose _pclose
#  define _CRT_SECURE_NO_WARNINGS
#endif

#include <hipew.h>
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

#  define dynamic_library_open(path)         LoadLibraryA(path)
#  define dynamic_library_close(lib)         FreeLibrary(lib)
#  define dynamic_library_find(lib, symbol)  GetProcAddress(lib, symbol)
#else
#  include <dlfcn.h>

typedef void* DynamicLibrary;

#  define dynamic_library_open(path)         dlopen(path, RTLD_NOW)
#  define dynamic_library_close(lib)         dlclose(lib)
#  define dynamic_library_find(lib, symbol)  dlsym(lib, symbol)
#endif

#define _LIBRARY_FIND_CHECKED(lib, name) \
        name = (t##name *)dynamic_library_find(lib, #name); \
        assert(name);

#define _LIBRARY_FIND(lib, name) \
        name = (t##name *)dynamic_library_find(lib, #name);

#define HIP_LIBRARY_FIND_CHECKED(name) \
        _LIBRARY_FIND_CHECKED(hip_lib, name)
#define HIP_LIBRARY_FIND(name) _LIBRARY_FIND(hip_lib, name)


static DynamicLibrary hip_lib;

/* Function definitions. */
thipGetErrorName *hipGetErrorName;
thipInit *hipInit;
thipDriverGetVersion *hipDriverGetVersion;
thipGetDevice *hipGetDevice;
thipGetDeviceCount *hipGetDeviceCount;
thipGetDeviceProperties *hipGetDeviceProperties;
thipDeviceGet* hipDeviceGet;
thipDeviceGetName *hipDeviceGetName;
thipDeviceGetAttribute *hipDeviceGetAttribute;
thipDeviceComputeCapability *hipDeviceComputeCapability;
thipDevicePrimaryCtxRetain *hipDevicePrimaryCtxRetain;
thipDevicePrimaryCtxRelease *hipDevicePrimaryCtxRelease;
thipDevicePrimaryCtxSetFlags *hipDevicePrimaryCtxSetFlags;
thipDevicePrimaryCtxGetState *hipDevicePrimaryCtxGetState;
thipDevicePrimaryCtxReset *hipDevicePrimaryCtxReset;
thipCtxCreate *hipCtxCreate;
thipCtxDestroy *hipCtxDestroy;
thipCtxPushCurrent *hipCtxPushCurrent;
thipCtxPopCurrent *hipCtxPopCurrent;
thipCtxSetCurrent *hipCtxSetCurrent;
thipCtxGetCurrent *hipCtxGetCurrent;
thipCtxGetDevice *hipCtxGetDevice;
thipCtxGetFlags *hipCtxGetFlags;
thipCtxSynchronize *hipCtxSynchronize;
thipDeviceSynchronize *hipDeviceSynchronize;
thipCtxGetCacheConfig *hipCtxGetCacheConfig;
thipCtxSetCacheConfig *hipCtxSetCacheConfig;
thipCtxGetSharedMemConfig *hipCtxGetSharedMemConfig;
thipCtxSetSharedMemConfig *hipCtxSetSharedMemConfig;
thipCtxGetApiVersion *hipCtxGetApiVersion;
thipModuleLoad *hipModuleLoad;
thipModuleLoadData *hipModuleLoadData;
thipModuleLoadDataEx *hipModuleLoadDataEx;
thipModuleUnload *hipModuleUnload;
thipModuleGetFunction *hipModuleGetFunction;
thipModuleGetGlobal *hipModuleGetGlobal;
thipModuleGetTexRef *hipModuleGetTexRef;
thipMemGetInfo *hipMemGetInfo;
thipMalloc *hipMalloc;
thipMemAllocPitch *hipMemAllocPitch;
thipFree *hipFree;
thipMemGetAddressRange *hipMemGetAddressRange;
thipHostMalloc *hipHostMalloc;
thipHostFree *hipHostFree;
thipHostGetDevicePointer *hipHostGetDevicePointer;
thipHostGetFlags *hipHostGetFlags;
thipMallocManaged *hipMallocManaged;
thipDeviceGetByPCIBusId *hipDeviceGetByPCIBusId;
thipDeviceGetPCIBusId *hipDeviceGetPCIBusId;
thipMemcpyPeer *hipMemcpyPeer;
thipMemcpyHtoD *hipMemcpyHtoD;
thipMemcpyDtoH *hipMemcpyDtoH;
thipMemcpyDtoD *hipMemcpyDtoD;
thipDrvMemcpy2DUnaligned *hipDrvMemcpy2DUnaligned;
thipMemcpyParam2D *hipMemcpyParam2D;
thipDrvMemcpy3D *hipDrvMemcpy3D;
thipMemcpyHtoDAsync *hipMemcpyHtoDAsync;
thipMemcpyDtoHAsync *hipMemcpyDtoHAsync;
thipMemcpyParam2DAsync *hipMemcpyParam2DAsync;
thipDrvMemcpy3DAsync *hipDrvMemcpy3DAsync;
thipMemsetD8 *hipMemsetD8;
thipMemsetD16 *hipMemsetD16;
thipMemsetD32 *hipMemsetD32;
thipMemsetD8Async *hipMemsetD8Async;
thipMemsetD16Async *hipMemsetD16Async;
thipMemsetD32Async *hipMemsetD32Async;
thipArrayCreate *hipArrayCreate;
thipArrayDestroy *hipArrayDestroy;
thipArray3DCreate *hipArray3DCreate;
thipStreamCreateWithFlags *hipStreamCreateWithFlags;
thipStreamCreateWithPriority *hipStreamCreateWithPriority;
thipStreamGetPriority *hipStreamGetPriority;
thipStreamGetFlags *hipStreamGetFlags;
thipStreamWaitEvent *hipStreamWaitEvent;
thipStreamAddCallback *hipStreamAddCallback;
thipStreamQuery *hipStreamQuery;
thipStreamSynchronize *hipStreamSynchronize;
thipStreamDestroy *hipStreamDestroy;
thipEventCreateWithFlags *hipEventCreateWithFlags;
thipEventRecord *hipEventRecord;
thipEventQuery *hipEventQuery;
thipEventSynchronize *hipEventSynchronize;
thipEventDestroy *hipEventDestroy;
thipEventElapsedTime *hipEventElapsedTime;
thipFuncGetAttribute *hipFuncGetAttribute;
thipFuncSetCacheConfig *hipFuncSetCacheConfig;
thipModuleLaunchKernel *hipModuleLaunchKernel;
thipDrvOccupancyMaxActiveBlocksPerMultiprocessor *hipDrvOccupancyMaxActiveBlocksPerMultiprocessor;
thipDrvOccupancyMaxActiveBlocksPerMultiprocessorWithFlags *hipDrvOccupancyMaxActiveBlocksPerMultiprocessorWithFlags;
thipModuleOccupancyMaxPotentialBlockSize *hipModuleOccupancyMaxPotentialBlockSize;
thipTexRefSetArray *hipTexRefSetArray;
thipTexRefSetAddress *hipTexRefSetAddress;
thipTexRefSetAddress2D *hipTexRefSetAddress2D;
thipTexRefSetFormat *hipTexRefSetFormat;
thipTexRefSetAddressMode *hipTexRefSetAddressMode;
thipTexRefSetFilterMode *hipTexRefSetFilterMode;
thipTexRefSetFlags *hipTexRefSetFlags;
thipTexRefGetAddress *hipTexRefGetAddress;
thipTexRefGetArray *hipTexRefGetArray;
thipTexRefGetAddressMode *hipTexRefGetAddressMode;
thipTexObjectCreate *hipTexObjectCreate;
thipTexObjectDestroy *hipTexObjectDestroy;
thipDeviceCanAccessPeer *hipDeviceCanAccessPeer;

thipCtxEnablePeerAccess *hipCtxEnablePeerAccess;
thipCtxDisablePeerAccess *hipCtxDisablePeerAccess;
thipDeviceGetP2PAttribute *hipDeviceGetP2PAttribute;
thipGraphicsUnregisterResource *hipGraphicsUnregisterResource;
thipGraphicsMapResources *hipGraphicsMapResources;
thipGraphicsUnmapResources *hipGraphicsUnmapResources;
thipGraphicsResourceGetMappedPointer *hipGraphicsResourceGetMappedPointer;

thipGraphicsGLRegisterBuffer *hipGraphicsGLRegisterBuffer;
thipGLGetDevices *hipGLGetDevices;

thiprtcGetErrorString* hiprtcGetErrorString;
thiprtcAddNameExpression* hiprtcAddNameExpression;
thiprtcCompileProgram* hiprtcCompileProgram;
thiprtcCreateProgram* hiprtcCreateProgram;
thiprtcDestroyProgram* hiprtcDestroyProgram;
thiprtcGetLoweredName* hiprtcGetLoweredName;
thiprtcGetProgramLog* hiprtcGetProgramLog;
thiprtcGetProgramLogSize* hiprtcGetProgramLogSize;
thiprtcGetCode* hiprtcGetCode;
thiprtcGetCodeSize* hiprtcGetCodeSize;



static DynamicLibrary dynamic_library_open_find(const char **paths) {
  int i = 0;
  while (paths[i] != NULL) {
      DynamicLibrary lib = dynamic_library_open(paths[i]);
      if (lib != NULL) {
        return lib;
      }
      ++i;
  }
  return NULL;
}

/* Implementation function. */
static void hipewHipExit(void) {
  if (hip_lib != NULL) {
    /*  Ignore errors. */
    dynamic_library_close(hip_lib);
    hip_lib = NULL;
  }
}

#ifdef _WIN32
static int hipewHasOldDriver(const char *hip_path) {
  DWORD verHandle = 0;
  DWORD verSize = GetFileVersionInfoSize(hip_path, &verHandle);
  int old_driver = 0;
  if (verSize != 0) {
    LPSTR verData = (LPSTR)malloc(verSize);
    if (GetFileVersionInfo(hip_path, verHandle, verSize, verData)) {
      LPBYTE lpBuffer = NULL;
      UINT size = 0;
      if (VerQueryValue(verData, "\\", (VOID FAR * FAR *)&lpBuffer, &size)) {
        if (size) {
          VS_FIXEDFILEINFO *verInfo = (VS_FIXEDFILEINFO *)lpBuffer;
          /* Magic value from
           * https://docs.microsoft.com/en-us/windows/win32/api/verrsrc/ns-verrsrc-vs_fixedfileinfo */
          if (verInfo->dwSignature == 0xfeef04bd) {
            unsigned int fileVersionLS0 = (verInfo->dwFileVersionLS >> 16) & 0xffff;
            unsigned int fileversionLS1 = (verInfo->dwFileVersionLS >> 0) & 0xffff;
            /* Corresponds to versions older than AMD Radeon Pro 21.Q4. */
            old_driver = ((fileVersionLS0 < 3354) || (fileVersionLS0 == 3354 && fileversionLS1 < 13));
          }
        }
      }
    }
    free(verData);
  }
  return old_driver;
}
#endif

static int hipewHipInit(void) {
  /* Library paths. */
#ifdef _WIN32
  /* Expected in c:/windows/system or similar, no path needed. */
  const char *hip_paths[] = {"amdhip64.dll", NULL};
#elif defined(__APPLE__)
  /* Default installation path. */
  const char *hip_paths[] = {"", NULL};
#else
  const char *hip_paths[] = {"libamdhip64.so.5",
                             "/opt/rocm/hip/lib/libamdhip64.so.5",
                             "libamdhip64.so",
                             "/opt/rocm/hip/lib/libamdhip64.so", NULL};
#endif
  static int initialized = 0;
  static int result = 0;
  int error;

  if (initialized) {
    return result;
  }

  initialized = 1;

  error = atexit(hipewHipExit);
  if (error) {
    result = HIPEW_ERROR_ATEXIT_FAILED;
    return result;
  }

#ifdef _WIN32
  /* Test for driver version. */
  if(hipewHasOldDriver(hip_paths[0])) {
     result = HIPEW_ERROR_OLD_DRIVER;
     return result;
  }
#endif

  /* Load library. */
  hip_lib = dynamic_library_open_find(hip_paths);

  if (hip_lib == NULL) {
    result = HIPEW_ERROR_OPEN_FAILED;
    return result;
  }

  /* Fetch all function pointers. */
  HIP_LIBRARY_FIND_CHECKED(hipGetErrorName);
  HIP_LIBRARY_FIND_CHECKED(hipInit);
  HIP_LIBRARY_FIND_CHECKED(hipDriverGetVersion);
  HIP_LIBRARY_FIND_CHECKED(hipGetDevice);
  HIP_LIBRARY_FIND_CHECKED(hipGetDeviceCount);
  HIP_LIBRARY_FIND_CHECKED(hipGetDeviceProperties);
  HIP_LIBRARY_FIND_CHECKED(hipDeviceGet);
  HIP_LIBRARY_FIND_CHECKED(hipDeviceGetName);
  HIP_LIBRARY_FIND_CHECKED(hipDeviceGetAttribute);
  HIP_LIBRARY_FIND_CHECKED(hipDeviceComputeCapability);
  HIP_LIBRARY_FIND_CHECKED(hipDevicePrimaryCtxRetain);
  HIP_LIBRARY_FIND_CHECKED(hipDevicePrimaryCtxRelease);
  HIP_LIBRARY_FIND_CHECKED(hipDevicePrimaryCtxSetFlags);
  HIP_LIBRARY_FIND_CHECKED(hipDevicePrimaryCtxGetState);
  HIP_LIBRARY_FIND_CHECKED(hipDevicePrimaryCtxReset);
  HIP_LIBRARY_FIND_CHECKED(hipCtxCreate);
  HIP_LIBRARY_FIND_CHECKED(hipCtxDestroy);
  HIP_LIBRARY_FIND_CHECKED(hipCtxPushCurrent);
  HIP_LIBRARY_FIND_CHECKED(hipCtxPopCurrent);
  HIP_LIBRARY_FIND_CHECKED(hipCtxSetCurrent);
  HIP_LIBRARY_FIND_CHECKED(hipCtxGetCurrent);
  HIP_LIBRARY_FIND_CHECKED(hipCtxGetDevice);
  HIP_LIBRARY_FIND_CHECKED(hipCtxGetFlags);
  HIP_LIBRARY_FIND_CHECKED(hipCtxSynchronize);
  HIP_LIBRARY_FIND_CHECKED(hipDeviceSynchronize);
  HIP_LIBRARY_FIND_CHECKED(hipCtxGetCacheConfig);
  HIP_LIBRARY_FIND_CHECKED(hipCtxSetCacheConfig);
  HIP_LIBRARY_FIND_CHECKED(hipCtxGetSharedMemConfig);
  HIP_LIBRARY_FIND_CHECKED(hipCtxSetSharedMemConfig);
  HIP_LIBRARY_FIND_CHECKED(hipCtxGetApiVersion);
  HIP_LIBRARY_FIND_CHECKED(hipModuleLoad);
  HIP_LIBRARY_FIND_CHECKED(hipModuleLoadData);
  HIP_LIBRARY_FIND_CHECKED(hipModuleLoadDataEx);
  HIP_LIBRARY_FIND_CHECKED(hipModuleUnload);
  HIP_LIBRARY_FIND_CHECKED(hipModuleGetFunction);
  HIP_LIBRARY_FIND_CHECKED(hipModuleGetGlobal);
  HIP_LIBRARY_FIND_CHECKED(hipModuleGetTexRef);
  HIP_LIBRARY_FIND_CHECKED(hipMemGetInfo);
  HIP_LIBRARY_FIND_CHECKED(hipMalloc);
  HIP_LIBRARY_FIND_CHECKED(hipMemAllocPitch);
  HIP_LIBRARY_FIND_CHECKED(hipFree);
  HIP_LIBRARY_FIND_CHECKED(hipMemGetAddressRange);
  HIP_LIBRARY_FIND_CHECKED(hipHostMalloc);
  HIP_LIBRARY_FIND_CHECKED(hipHostFree);
  HIP_LIBRARY_FIND_CHECKED(hipHostGetDevicePointer);
  HIP_LIBRARY_FIND_CHECKED(hipHostGetFlags);
  HIP_LIBRARY_FIND_CHECKED(hipMallocManaged);
  HIP_LIBRARY_FIND_CHECKED(hipDeviceGetByPCIBusId);
  HIP_LIBRARY_FIND_CHECKED(hipDeviceGetPCIBusId);
  HIP_LIBRARY_FIND_CHECKED(hipMemcpyPeer);
  HIP_LIBRARY_FIND_CHECKED(hipMemcpyHtoD);
  HIP_LIBRARY_FIND_CHECKED(hipMemcpyDtoH);
  HIP_LIBRARY_FIND_CHECKED(hipMemcpyDtoD);
  HIP_LIBRARY_FIND_CHECKED(hipMemcpyParam2D);
  HIP_LIBRARY_FIND_CHECKED(hipDrvMemcpy3D);
  HIP_LIBRARY_FIND_CHECKED(hipMemcpyHtoDAsync);
  HIP_LIBRARY_FIND_CHECKED(hipMemcpyDtoHAsync);
  HIP_LIBRARY_FIND_CHECKED(hipDrvMemcpy2DUnaligned);
  HIP_LIBRARY_FIND_CHECKED(hipMemcpyParam2DAsync);
  HIP_LIBRARY_FIND_CHECKED(hipDrvMemcpy3DAsync);
  HIP_LIBRARY_FIND_CHECKED(hipMemsetD8);
  HIP_LIBRARY_FIND_CHECKED(hipMemsetD16);
  HIP_LIBRARY_FIND_CHECKED(hipMemsetD32);
  HIP_LIBRARY_FIND_CHECKED(hipMemsetD8Async);
  HIP_LIBRARY_FIND_CHECKED(hipMemsetD16Async);
  HIP_LIBRARY_FIND_CHECKED(hipMemsetD32Async);
  HIP_LIBRARY_FIND_CHECKED(hipArrayCreate);
  HIP_LIBRARY_FIND_CHECKED(hipArrayDestroy);
  HIP_LIBRARY_FIND_CHECKED(hipArray3DCreate);
  HIP_LIBRARY_FIND_CHECKED(hipStreamCreateWithFlags);
  HIP_LIBRARY_FIND_CHECKED(hipStreamCreateWithPriority);
  HIP_LIBRARY_FIND_CHECKED(hipStreamGetPriority);
  HIP_LIBRARY_FIND_CHECKED(hipStreamGetFlags);
  HIP_LIBRARY_FIND_CHECKED(hipStreamWaitEvent);
  HIP_LIBRARY_FIND_CHECKED(hipStreamAddCallback);
  HIP_LIBRARY_FIND_CHECKED(hipStreamQuery);
  HIP_LIBRARY_FIND_CHECKED(hipStreamSynchronize);
  HIP_LIBRARY_FIND_CHECKED(hipStreamDestroy);
  HIP_LIBRARY_FIND_CHECKED(hipEventCreateWithFlags);
  HIP_LIBRARY_FIND_CHECKED(hipEventRecord);
  HIP_LIBRARY_FIND_CHECKED(hipEventQuery);
  HIP_LIBRARY_FIND_CHECKED(hipEventSynchronize);
  HIP_LIBRARY_FIND_CHECKED(hipEventDestroy);
  HIP_LIBRARY_FIND_CHECKED(hipEventElapsedTime);
  HIP_LIBRARY_FIND_CHECKED(hipFuncGetAttribute);
  HIP_LIBRARY_FIND_CHECKED(hipFuncSetCacheConfig);
  HIP_LIBRARY_FIND_CHECKED(hipModuleLaunchKernel);
  HIP_LIBRARY_FIND_CHECKED(hipModuleOccupancyMaxPotentialBlockSize);
  HIP_LIBRARY_FIND_CHECKED(hipTexRefSetArray);
  HIP_LIBRARY_FIND_CHECKED(hipTexRefSetAddress);
  HIP_LIBRARY_FIND_CHECKED(hipTexRefSetAddress2D);
  HIP_LIBRARY_FIND_CHECKED(hipTexRefSetFormat);
  HIP_LIBRARY_FIND_CHECKED(hipTexRefSetAddressMode);
  HIP_LIBRARY_FIND_CHECKED(hipTexRefSetFilterMode);
  HIP_LIBRARY_FIND_CHECKED(hipTexRefSetFlags);
  HIP_LIBRARY_FIND_CHECKED(hipTexRefGetAddress);
  HIP_LIBRARY_FIND_CHECKED(hipTexRefGetAddressMode);
  HIP_LIBRARY_FIND_CHECKED(hipTexObjectCreate);
  HIP_LIBRARY_FIND_CHECKED(hipTexObjectDestroy);
  HIP_LIBRARY_FIND_CHECKED(hipDeviceCanAccessPeer);
  HIP_LIBRARY_FIND_CHECKED(hipCtxEnablePeerAccess);
  HIP_LIBRARY_FIND_CHECKED(hipCtxDisablePeerAccess);
  HIP_LIBRARY_FIND_CHECKED(hipDeviceGetP2PAttribute);
#ifdef _WIN32
  HIP_LIBRARY_FIND_CHECKED(hipGraphicsUnregisterResource);
  HIP_LIBRARY_FIND_CHECKED(hipGraphicsMapResources);
  HIP_LIBRARY_FIND_CHECKED(hipGraphicsUnmapResources);
  HIP_LIBRARY_FIND_CHECKED(hipGraphicsResourceGetMappedPointer);
  HIP_LIBRARY_FIND_CHECKED(hipGraphicsGLRegisterBuffer);
  HIP_LIBRARY_FIND_CHECKED(hipGLGetDevices);
#endif
  HIP_LIBRARY_FIND_CHECKED(hiprtcGetErrorString);
  HIP_LIBRARY_FIND_CHECKED(hiprtcAddNameExpression);
  HIP_LIBRARY_FIND_CHECKED(hiprtcCompileProgram);
  HIP_LIBRARY_FIND_CHECKED(hiprtcCreateProgram);
  HIP_LIBRARY_FIND_CHECKED(hiprtcDestroyProgram);
  HIP_LIBRARY_FIND_CHECKED(hiprtcGetLoweredName);
  HIP_LIBRARY_FIND_CHECKED(hiprtcGetProgramLog);
  HIP_LIBRARY_FIND_CHECKED(hiprtcGetProgramLogSize);
  HIP_LIBRARY_FIND_CHECKED(hiprtcGetCode);
  HIP_LIBRARY_FIND_CHECKED(hiprtcGetCodeSize);
  result = HIPEW_SUCCESS;
  return result;
}



int hipewInit(hipuint32_t flags) {
  int result = HIPEW_SUCCESS;

  if (flags & HIPEW_INIT_HIP) {
    result = hipewHipInit();
    if (result != HIPEW_SUCCESS) {
      return result;
    }
  }

  return result;
}


const char *hipewErrorString(hipError_t result) {
  switch (result) {
    case hipSuccess: return "No errors";
    case hipErrorInvalidValue: return "Invalid value";
    case hipErrorOutOfMemory: return "Out of memory";
    case hipErrorNotInitialized: return "Driver not initialized";
    case hipErrorDeinitialized: return "Driver deinitialized";
    case hipErrorProfilerDisabled: return "Profiler disabled";
    case hipErrorProfilerNotInitialized: return "Profiler not initialized";
    case hipErrorProfilerAlreadyStarted: return "Profiler already started";
    case hipErrorProfilerAlreadyStopped: return "Profiler already stopped";
    case hipErrorNoDevice: return "No HIP-capable device available";
    case hipErrorInvalidDevice: return "Invalid device";
    case hipErrorInvalidImage: return "Invalid kernel image";
    case hipErrorInvalidContext: return "Invalid context";
    case hipErrorContextAlreadyCurrent: return "Context already current";
    case hipErrorMapFailed: return "Map failed";
    case hipErrorUnmapFailed: return "Unmap failed";
    case hipErrorArrayIsMapped: return "Array is mapped";
    case hipErrorAlreadyMapped: return "Already mapped";
    case hipErrorNoBinaryForGpu: return "No binary for GPU";
    case hipErrorAlreadyAcquired: return "Already acquired";
    case hipErrorNotMapped: return "Not mapped";
    case hipErrorNotMappedAsArray: return "Mapped resource not available for access as an array";
    case hipErrorNotMappedAsPointer: return "Mapped resource not available for access as a pointer";
    case hipErrorECCNotCorrectable: return "Uncorrectable ECC error detected";
    case hipErrorUnsupportedLimit: return "hipLimit_t not supported by device";
    case hipErrorContextAlreadyInUse: return "Context already in use";
    case hipErrorPeerAccessUnsupported: return "Peer access unsupported";
    case hipErrorInvalidKernelFile: return "Invalid ptx";
    case hipErrorInvalidGraphicsContext: return "Invalid graphics context";
    case hipErrorInvalidSource: return "Invalid source";
    case hipErrorFileNotFound: return "File not found";
    case hipErrorSharedObjectSymbolNotFound: return "Link to a shared object failed to resolve";
    case hipErrorSharedObjectInitFailed: return "Shared object initialization failed";
    case hipErrorOperatingSystem: return "Operating system";
    case hipErrorInvalidHandle: return "Invalid handle";
    case hipErrorNotFound: return "Not found";
    case hipErrorNotReady: return "HIP not ready";
    case hipErrorIllegalAddress: return "Illegal address";
    case hipErrorLaunchOutOfResources: return "Launch exceeded resources";
    case hipErrorLaunchTimeOut: return "Launch exceeded timeout";
    case hipErrorPeerAccessAlreadyEnabled: return "Peer access already enabled";
    case hipErrorPeerAccessNotEnabled: return "Peer access not enabled";
    case hipErrorSetOnActiveProcess: return "Primary context active";
    case hipErrorAssert: return "Assert";
    case hipErrorHostMemoryAlreadyRegistered: return "Host memory already registered";
    case hipErrorHostMemoryNotRegistered: return "Host memory not registered";
    case hipErrorLaunchFailure: return "Launch failed";
    case hipErrorCooperativeLaunchTooLarge: return "Cooperative launch too large";
    case hipErrorNotSupported: return "Not supported";
    case hipErrorUnknown: return "Unknown error";
    default: return "Unknown HIP error value";
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

const char *hipewCompilerPath(void) {
    #ifdef _WIN32
    const char *hipPath = getenv("HIP_ROCCLR_HOME");
    const char *windowsCommand = "perl ";
    const char *executable = "bin/hipcc";

    static char hipcc[65536];
    static char finalCommand[65536];
    if(hipPath) {
      path_join(hipPath, executable, sizeof(hipcc), hipcc);
      if(path_exists(hipcc)) {
        snprintf(finalCommand, sizeof(hipcc), "%s %s", windowsCommand, hipcc);
        return finalCommand;
      } else {
        printf("Could not find hipcc. Make sure HIP_ROCCLR_HOME points to the directory holding /bin/hipcc");
      }
    }
    #else
    const char *hipPath =  "opt/rocm/hip/bin";
    const char *executable = "hipcc";

    static char hipcc[65536];
    if(hipPath) {
      path_join(hipPath, executable, sizeof(hipcc), hipcc);
      if(path_exists(hipcc)){
        return hipcc;
      }
    }
    #endif

  {
#ifdef _WIN32
    FILE *handle = popen("where hipcc", "r");
#else
    FILE *handle = popen("which hipcc", "r");
#endif
    if (handle) {
      char buffer[4096] = {0};
      int len = fread(buffer, 1, sizeof(buffer) - 1, handle);
      buffer[len] = '\0';
      pclose(handle);
      if (buffer[0]) {
        return "hipcc";
      }
    }
  }

  return NULL;
}

int hipewCompilerVersion(void) {
  const char *path = hipewCompilerPath();
  const char *marker = "Hip compilation tools, release ";
  FILE *pipe;
  char buf[128];
  char output[65536] = "\0";
  char command[65536] = "\0";

  if (path == NULL) {
    return 0;
  }

  /* get --version output */
  strcat(command, "\"");
  strncat(command, path, sizeof(command) - 1);
  strncat(command, "\" --version", sizeof(command) - strlen(path) - 1);
  pipe = popen(command, "r");
  if (!pipe) {
    fprintf(stderr, "HIP: failed to run compiler to retrieve version");
    return 0;
  }

  while (!feof(pipe)) {
    if (fgets(buf, sizeof(buf), pipe) != NULL) {
      strncat(output, buf, sizeof(output) - strlen(output) - 1);
    }
  }

  pclose(pipe);
  return 40;
}
