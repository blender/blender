/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "device/cuda/device.h"

#include "util/log.h"

#ifdef WITH_CUDA
#  include "device/cuda/device_impl.h"
#  include "device/device.h"

#  include "integrator/denoiser_oidn_gpu.h"

#  include "util/string.h"
#  include "util/windows.h"
#endif /* WITH_CUDA */

CCL_NAMESPACE_BEGIN

bool device_cuda_init()
{
#if !defined(WITH_CUDA)
  return false;
#elif defined(WITH_CUDA_DYNLOAD)
  static bool initialized = false;
  static bool result = false;

  if (initialized) {
    return result;
  }

  initialized = true;
  int cuew_result = cuewInit(CUEW_INIT_CUDA);
  if (cuew_result == CUEW_SUCCESS) {
    VLOG_INFO << "CUEW initialization succeeded";
    if (CUDADevice::have_precompiled_kernels()) {
      VLOG_INFO << "Found precompiled kernels";
      result = true;
    }
    else if (cuewCompilerPath() != NULL) {
      VLOG_INFO << "Found CUDA compiler " << cuewCompilerPath();
      result = true;
    }
    else {
      VLOG_INFO << "Neither precompiled kernels nor CUDA compiler was found,"
                << " unable to use CUDA";
    }
  }
  else {
    VLOG_WARNING << "CUEW initialization failed: "
                 << ((cuew_result == CUEW_ERROR_ATEXIT_FAILED) ?
                         "Error setting up atexit() handler" :
                         "Error opening the library");
  }

  return result;
#else  /* WITH_CUDA_DYNLOAD */
  return true;
#endif /* WITH_CUDA_DYNLOAD */
}

Device *device_cuda_create(const DeviceInfo &info, Stats &stats, Profiler &profiler)
{
#ifdef WITH_CUDA
  return new CUDADevice(info, stats, profiler);
#else
  (void)info;
  (void)stats;
  (void)profiler;

  LOG(FATAL) << "Request to create CUDA device without compiled-in support. Should never happen.";

  return nullptr;
#endif
}

#ifdef WITH_CUDA
static CUresult device_cuda_safe_init()
{
#  ifdef _WIN32
  __try
  {
    return cuInit(0);
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    /* Ignore crashes inside the CUDA driver and hope we can
     * survive even with corrupted CUDA installs. */
    fprintf(stderr, "Cycles CUDA: driver crashed, continuing without CUDA.\n");
  }

  return CUDA_ERROR_NO_DEVICE;
#  else
  return cuInit(0);
#  endif
}
#endif /* WITH_CUDA */

void device_cuda_info(vector<DeviceInfo> &devices)
{
#ifdef WITH_CUDA
  CUresult result = device_cuda_safe_init();
  if (result != CUDA_SUCCESS) {
    if (result != CUDA_ERROR_NO_DEVICE) {
      fprintf(stderr, "CUDA cuInit: %s\n", cuewErrorString(result));
    }
    return;
  }

  int count = 0;
  result = cuDeviceGetCount(&count);
  if (result != CUDA_SUCCESS) {
    fprintf(stderr, "CUDA cuDeviceGetCount: %s\n", cuewErrorString(result));
    return;
  }

  vector<DeviceInfo> display_devices;

  for (int num = 0; num < count; num++) {
    char name[256];

    result = cuDeviceGetName(name, 256, num);
    if (result != CUDA_SUCCESS) {
      fprintf(stderr, "CUDA cuDeviceGetName: %s\n", cuewErrorString(result));
      continue;
    }

    int major;
    cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, num);
    if (major < 3) {
      VLOG_INFO << "Ignoring device \"" << name
                << "\", this graphics card is no longer supported.";
      continue;
    }

    DeviceInfo info;

    info.type = DEVICE_CUDA;
    info.description = string(name);
    info.num = num;

    info.has_nanovdb = true;
    info.denoisers = 0;

    info.has_gpu_queue = true;

    /* Check if the device has P2P access to any other device in the system. */
    for (int peer_num = 0; peer_num < count && !info.has_peer_memory; peer_num++) {
      if (num != peer_num) {
        int can_access = 0;
        cuDeviceCanAccessPeer(&can_access, num, peer_num);
        info.has_peer_memory = (can_access != 0);
      }
    }

    int pci_location[3] = {0, 0, 0};
    cuDeviceGetAttribute(&pci_location[0], CU_DEVICE_ATTRIBUTE_PCI_DOMAIN_ID, num);
    cuDeviceGetAttribute(&pci_location[1], CU_DEVICE_ATTRIBUTE_PCI_BUS_ID, num);
    cuDeviceGetAttribute(&pci_location[2], CU_DEVICE_ATTRIBUTE_PCI_DEVICE_ID, num);
    info.id = string_printf("CUDA_%s_%04x:%02x:%02x",
                            name,
                            (unsigned int)pci_location[0],
                            (unsigned int)pci_location[1],
                            (unsigned int)pci_location[2]);

#  if defined(WITH_OPENIMAGEDENOISE)
#    if OIDN_VERSION >= 20300
    if (oidnIsCUDADeviceSupported(num)) {
#    else
    if (OIDNDenoiserGPU::is_device_supported(info)) {
#    endif
      info.denoisers |= DENOISER_OPENIMAGEDENOISE;
    }
#  endif

    /* If device has a kernel timeout and no compute preemption, we assume
     * it is connected to a display and will freeze the display while doing
     * computations. */
    int timeout_attr = 0, preempt_attr = 0;
    cuDeviceGetAttribute(&timeout_attr, CU_DEVICE_ATTRIBUTE_KERNEL_EXEC_TIMEOUT, num);
    cuDeviceGetAttribute(&preempt_attr, CU_DEVICE_ATTRIBUTE_COMPUTE_PREEMPTION_SUPPORTED, num);

    /* The CUDA driver reports compute preemption as not being available on
     * Windows 10 even when it is, due to an issue in application profiles.
     * Detect case where we expect it to be available and override. */
    if (preempt_attr == 0 && (major >= 6) && system_windows_version_at_least(10, 17134)) {
      VLOG_INFO << "Assuming device has compute preemption on Windows 10.";
      preempt_attr = 1;
    }

    if (timeout_attr && !preempt_attr) {
      VLOG_INFO << "Device is recognized as display.";
      info.description += " (Display)";
      info.display_device = true;
      display_devices.push_back(info);
    }
    else {
      VLOG_INFO << "Device has compute preemption or is not used for display.";
      devices.push_back(info);
    }
    VLOG_INFO << "Added device \"" << info.description << "\" with id \"" << info.id << "\".";

    if (info.denoisers & DENOISER_OPENIMAGEDENOISE)
      VLOG_INFO << "Device with id \"" << info.id << "\" supports "
                << denoiserTypeToHumanReadable(DENOISER_OPENIMAGEDENOISE) << ".";
  }

  if (!display_devices.empty()) {
    devices.insert(devices.end(), display_devices.begin(), display_devices.end());
  }
#else  /* WITH_CUDA */
  (void)devices;
#endif /* WITH_CUDA */
}

string device_cuda_capabilities()
{
#ifdef WITH_CUDA
  CUresult result = device_cuda_safe_init();
  if (result != CUDA_SUCCESS) {
    if (result != CUDA_ERROR_NO_DEVICE) {
      return string("Error initializing CUDA: ") + cuewErrorString(result);
    }
    return "No CUDA device found\n";
  }

  int count;
  result = cuDeviceGetCount(&count);
  if (result != CUDA_SUCCESS) {
    return string("Error getting devices: ") + cuewErrorString(result);
  }

  string capabilities = "";
  for (int num = 0; num < count; num++) {
    char name[256];
    if (cuDeviceGetName(name, 256, num) != CUDA_SUCCESS) {
      continue;
    }
    capabilities += string("\t") + name + "\n";
    int value;
#  define GET_ATTR(attr) \
    { \
      if (cuDeviceGetAttribute(&value, CU_DEVICE_ATTRIBUTE_##attr, num) == CUDA_SUCCESS) { \
        capabilities += string_printf("\t\tCU_DEVICE_ATTRIBUTE_" #attr "\t\t\t%d\n", value); \
      } \
    } \
    (void)0
    /* TODO(sergey): Strip all attributes which are not useful for us
     * or does not depend on the driver.
     */
    GET_ATTR(MAX_THREADS_PER_BLOCK);
    GET_ATTR(MAX_BLOCK_DIM_X);
    GET_ATTR(MAX_BLOCK_DIM_Y);
    GET_ATTR(MAX_BLOCK_DIM_Z);
    GET_ATTR(MAX_GRID_DIM_X);
    GET_ATTR(MAX_GRID_DIM_Y);
    GET_ATTR(MAX_GRID_DIM_Z);
    GET_ATTR(MAX_SHARED_MEMORY_PER_BLOCK);
    GET_ATTR(SHARED_MEMORY_PER_BLOCK);
    GET_ATTR(TOTAL_CONSTANT_MEMORY);
    GET_ATTR(WARP_SIZE);
    GET_ATTR(MAX_PITCH);
    GET_ATTR(MAX_REGISTERS_PER_BLOCK);
    GET_ATTR(REGISTERS_PER_BLOCK);
    GET_ATTR(CLOCK_RATE);
    GET_ATTR(TEXTURE_ALIGNMENT);
    GET_ATTR(GPU_OVERLAP);
    GET_ATTR(MULTIPROCESSOR_COUNT);
    GET_ATTR(KERNEL_EXEC_TIMEOUT);
    GET_ATTR(INTEGRATED);
    GET_ATTR(CAN_MAP_HOST_MEMORY);
    GET_ATTR(COMPUTE_MODE);
    GET_ATTR(MAXIMUM_TEXTURE1D_WIDTH);
    GET_ATTR(MAXIMUM_TEXTURE2D_WIDTH);
    GET_ATTR(MAXIMUM_TEXTURE2D_HEIGHT);
    GET_ATTR(MAXIMUM_TEXTURE3D_WIDTH);
    GET_ATTR(MAXIMUM_TEXTURE3D_HEIGHT);
    GET_ATTR(MAXIMUM_TEXTURE3D_DEPTH);
    GET_ATTR(MAXIMUM_TEXTURE2D_LAYERED_WIDTH);
    GET_ATTR(MAXIMUM_TEXTURE2D_LAYERED_HEIGHT);
    GET_ATTR(MAXIMUM_TEXTURE2D_LAYERED_LAYERS);
    GET_ATTR(MAXIMUM_TEXTURE2D_ARRAY_WIDTH);
    GET_ATTR(MAXIMUM_TEXTURE2D_ARRAY_HEIGHT);
    GET_ATTR(MAXIMUM_TEXTURE2D_ARRAY_NUMSLICES);
    GET_ATTR(SURFACE_ALIGNMENT);
    GET_ATTR(CONCURRENT_KERNELS);
    GET_ATTR(ECC_ENABLED);
    GET_ATTR(TCC_DRIVER);
    GET_ATTR(MEMORY_CLOCK_RATE);
    GET_ATTR(GLOBAL_MEMORY_BUS_WIDTH);
    GET_ATTR(L2_CACHE_SIZE);
    GET_ATTR(MAX_THREADS_PER_MULTIPROCESSOR);
    GET_ATTR(ASYNC_ENGINE_COUNT);
    GET_ATTR(UNIFIED_ADDRESSING);
    GET_ATTR(MAXIMUM_TEXTURE1D_LAYERED_WIDTH);
    GET_ATTR(MAXIMUM_TEXTURE1D_LAYERED_LAYERS);
    GET_ATTR(CAN_TEX2D_GATHER);
    GET_ATTR(MAXIMUM_TEXTURE2D_GATHER_WIDTH);
    GET_ATTR(MAXIMUM_TEXTURE2D_GATHER_HEIGHT);
    GET_ATTR(MAXIMUM_TEXTURE3D_WIDTH_ALTERNATE);
    GET_ATTR(MAXIMUM_TEXTURE3D_HEIGHT_ALTERNATE);
    GET_ATTR(MAXIMUM_TEXTURE3D_DEPTH_ALTERNATE);
    GET_ATTR(TEXTURE_PITCH_ALIGNMENT);
    GET_ATTR(MAXIMUM_TEXTURECUBEMAP_WIDTH);
    GET_ATTR(MAXIMUM_TEXTURECUBEMAP_LAYERED_WIDTH);
    GET_ATTR(MAXIMUM_TEXTURECUBEMAP_LAYERED_LAYERS);
    GET_ATTR(MAXIMUM_SURFACE1D_WIDTH);
    GET_ATTR(MAXIMUM_SURFACE2D_WIDTH);
    GET_ATTR(MAXIMUM_SURFACE2D_HEIGHT);
    GET_ATTR(MAXIMUM_SURFACE3D_WIDTH);
    GET_ATTR(MAXIMUM_SURFACE3D_HEIGHT);
    GET_ATTR(MAXIMUM_SURFACE3D_DEPTH);
    GET_ATTR(MAXIMUM_SURFACE1D_LAYERED_WIDTH);
    GET_ATTR(MAXIMUM_SURFACE1D_LAYERED_LAYERS);
    GET_ATTR(MAXIMUM_SURFACE2D_LAYERED_WIDTH);
    GET_ATTR(MAXIMUM_SURFACE2D_LAYERED_HEIGHT);
    GET_ATTR(MAXIMUM_SURFACE2D_LAYERED_LAYERS);
    GET_ATTR(MAXIMUM_SURFACECUBEMAP_WIDTH);
    GET_ATTR(MAXIMUM_SURFACECUBEMAP_LAYERED_WIDTH);
    GET_ATTR(MAXIMUM_SURFACECUBEMAP_LAYERED_LAYERS);
    GET_ATTR(MAXIMUM_TEXTURE1D_LINEAR_WIDTH);
    GET_ATTR(MAXIMUM_TEXTURE2D_LINEAR_WIDTH);
    GET_ATTR(MAXIMUM_TEXTURE2D_LINEAR_HEIGHT);
    GET_ATTR(MAXIMUM_TEXTURE2D_LINEAR_PITCH);
    GET_ATTR(MAXIMUM_TEXTURE2D_MIPMAPPED_WIDTH);
    GET_ATTR(MAXIMUM_TEXTURE2D_MIPMAPPED_HEIGHT);
    GET_ATTR(COMPUTE_CAPABILITY_MAJOR);
    GET_ATTR(COMPUTE_CAPABILITY_MINOR);
    GET_ATTR(MAXIMUM_TEXTURE1D_MIPMAPPED_WIDTH);
    GET_ATTR(STREAM_PRIORITIES_SUPPORTED);
    GET_ATTR(GLOBAL_L1_CACHE_SUPPORTED);
    GET_ATTR(LOCAL_L1_CACHE_SUPPORTED);
    GET_ATTR(MAX_SHARED_MEMORY_PER_MULTIPROCESSOR);
    GET_ATTR(MAX_REGISTERS_PER_MULTIPROCESSOR);
    GET_ATTR(MANAGED_MEMORY);
    GET_ATTR(MULTI_GPU_BOARD);
    GET_ATTR(MULTI_GPU_BOARD_GROUP_ID);
#  undef GET_ATTR
    capabilities += "\n";
  }

  return capabilities;

#else  /* WITH_CUDA */
  return "";
#endif /* WITH_CUDA */
}

CCL_NAMESPACE_END
