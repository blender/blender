/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "device/hip/device.h"

#include "util/log.h"

#ifdef WITH_HIP
#  include "device/device.h"
#  include "device/hip/device_impl.h"

#  include "util/string.h"
#  include "util/windows.h"
#endif /* WITH_HIP */

#ifdef WITH_HIPRT
#  include "device/hiprt/device_impl.h"
#endif

CCL_NAMESPACE_BEGIN

bool device_hip_init()
{
#if !defined(WITH_HIP)
  return false;
#elif defined(WITH_HIP_DYNLOAD)
  static bool initialized = false;
  static bool result = false;

  if (initialized)
    return result;

  initialized = true;
  int hipew_result = hipewInit(HIPEW_INIT_HIP);
  if (hipew_result == HIPEW_SUCCESS) {
    VLOG_INFO << "HIPEW initialization succeeded";
    if (HIPDevice::have_precompiled_kernels()) {
      VLOG_INFO << "Found precompiled kernels";
      result = true;
    }
    else if (hipewCompilerPath() != NULL) {
      VLOG_INFO << "Found HIPCC " << hipewCompilerPath();
      result = true;
    }
    else {
      VLOG_INFO << "Neither precompiled kernels nor HIPCC was found,"
                << " unable to use HIP";
    }
  }
  else {
    if (hipew_result == HIPEW_ERROR_ATEXIT_FAILED) {
      VLOG_WARNING << "HIPEW initialization failed: Error setting up atexit() handler";
    }
    else if (hipew_result == HIPEW_ERROR_OLD_DRIVER) {
      VLOG_WARNING
          << "HIPEW initialization failed: Driver version too old, requires AMD Radeon Pro "
             "21.Q4 driver or newer";
    }
    else {
      VLOG_WARNING << "HIPEW initialization failed: Error opening HIP dynamic library";
    }
  }

  return result;
#else  /* WITH_HIP_DYNLOAD */
  return true;
#endif /* WITH_HIP_DYNLOAD */
}

Device *device_hip_create(const DeviceInfo &info, Stats &stats, Profiler &profiler)
{
#ifdef WITH_HIPRT
  if (info.use_hardware_raytracing)
    return new HIPRTDevice(info, stats, profiler);
  else
    return new HIPDevice(info, stats, profiler);
#elif defined(WITH_HIP)
  return new HIPDevice(info, stats, profiler);
#else
  (void)info;
  (void)stats;
  (void)profiler;

  LOG(FATAL) << "Request to create HIP device without compiled-in support. Should never happen.";

  return nullptr;
#endif
}

#ifdef WITH_HIP
static hipError_t device_hip_safe_init()
{
#  ifdef _WIN32
  __try
  {
    return hipInit(0);
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    /* Ignore crashes inside the HIP driver and hope we can
     * survive even with corrupted HIP installs. */
    fprintf(stderr, "Cycles HIP: driver crashed, continuing without HIP.\n");
  }

  return hipErrorNoDevice;
#  else
  return hipInit(0);
#  endif
}
#endif /* WITH_HIP */

void device_hip_info(vector<DeviceInfo> &devices)
{
#ifdef WITH_HIP
  hipError_t result = device_hip_safe_init();
  if (result != hipSuccess) {
    if (result != hipErrorNoDevice)
      fprintf(stderr, "HIP hipInit: %s\n", hipewErrorString(result));
    return;
  }

  int count = 0;
  result = hipGetDeviceCount(&count);
  if (result != hipSuccess) {
    fprintf(stderr, "HIP hipGetDeviceCount: %s\n", hipewErrorString(result));
    return;
  }

#  ifdef WITH_HIPRT
  const bool has_hardware_raytracing = hiprtewInit();
#  else
  const bool has_hardware_raytracing = false;
#  endif

  vector<DeviceInfo> display_devices;

  for (int num = 0; num < count; num++) {
    char name[256];

    result = hipDeviceGetName(name, 256, num);
    if (result != hipSuccess) {
      fprintf(stderr, "HIP :hipDeviceGetName: %s\n", hipewErrorString(result));
      continue;
    }

    if (!hipSupportsDevice(num)) {
      continue;
    }

    DeviceInfo info;

    info.type = DEVICE_HIP;
    info.description = string(name);
    info.num = num;

    info.has_nanovdb = true;
    info.has_light_tree = true;
    info.has_mnee = true;
    info.denoisers = 0;

    info.has_gpu_queue = true;
    /* Check if the device has P2P access to any other device in the system. */
    for (int peer_num = 0; peer_num < count && !info.has_peer_memory; peer_num++) {
      if (num != peer_num) {
        int can_access = 0;
        hipDeviceCanAccessPeer(&can_access, num, peer_num);
        info.has_peer_memory = (can_access != 0);
      }
    }

    info.use_hardware_raytracing = has_hardware_raytracing;

    int pci_location[3] = {0, 0, 0};
    hipDeviceGetAttribute(&pci_location[0], hipDeviceAttributePciDomainID, num);
    hipDeviceGetAttribute(&pci_location[1], hipDeviceAttributePciBusId, num);
    hipDeviceGetAttribute(&pci_location[2], hipDeviceAttributePciDeviceId, num);
    info.id = string_printf("HIP_%s_%04x:%02x:%02x",
                            name,
                            (unsigned int)pci_location[0],
                            (unsigned int)pci_location[1],
                            (unsigned int)pci_location[2]);

    /* If device has a kernel timeout and no compute preemption, we assume
     * it is connected to a display and will freeze the display while doing
     * computations. */
    int timeout_attr = 0;
    hipDeviceGetAttribute(&timeout_attr, hipDeviceAttributeKernelExecTimeout, num);

    if (timeout_attr) {
      VLOG_INFO << "Device is recognized as display.";
      info.description += " (Display)";
      info.display_device = true;
      display_devices.push_back(info);
    }
    else {
      VLOG_INFO << "Device has compute preemption or is not used for display.";
      devices.push_back(info);
    }

    VLOG_INFO << "Added device \"" << name << "\" with id \"" << info.id << "\".";
  }

  if (!display_devices.empty())
    devices.insert(devices.end(), display_devices.begin(), display_devices.end());
#else  /* WITH_HIP */
  (void)devices;
#endif /* WITH_HIP */
}

string device_hip_capabilities()
{
#ifdef WITH_HIP
  hipError_t result = device_hip_safe_init();
  if (result != hipSuccess) {
    if (result != hipErrorNoDevice) {
      return string("Error initializing HIP: ") + hipewErrorString(result);
    }
    return "No HIP device found\n";
  }

  int count;
  result = hipGetDeviceCount(&count);
  if (result != hipSuccess) {
    return string("Error getting devices: ") + hipewErrorString(result);
  }

  string capabilities = "";
  for (int num = 0; num < count; num++) {
    char name[256];
    if (hipDeviceGetName(name, 256, num) != hipSuccess) {
      continue;
    }
    capabilities += string("\t") + name + "\n";
    int value;
#  define GET_ATTR(attr) \
    { \
      if (hipDeviceGetAttribute(&value, hipDeviceAttribute##attr, num) == hipSuccess) { \
        capabilities += string_printf("\t\thipDeviceAttribute" #attr "\t\t\t%d\n", value); \
      } \
    } \
    (void)0
    /* TODO(sergey): Strip all attributes which are not useful for us
     * or does not depend on the driver.
     */
    GET_ATTR(MaxThreadsPerBlock);
    GET_ATTR(MaxBlockDimX);
    GET_ATTR(MaxBlockDimY);
    GET_ATTR(MaxBlockDimZ);
    GET_ATTR(MaxGridDimX);
    GET_ATTR(MaxGridDimY);
    GET_ATTR(MaxGridDimZ);
    GET_ATTR(MaxSharedMemoryPerBlock);
    GET_ATTR(TotalConstantMemory);
    GET_ATTR(WarpSize);
    GET_ATTR(MaxPitch);
    GET_ATTR(MaxRegistersPerBlock);
    GET_ATTR(ClockRate);
    GET_ATTR(TextureAlignment);
    GET_ATTR(MultiprocessorCount);
    GET_ATTR(KernelExecTimeout);
    GET_ATTR(Integrated);
    GET_ATTR(CanMapHostMemory);
    GET_ATTR(ComputeMode);
    GET_ATTR(MaxTexture1DWidth);
    GET_ATTR(MaxTexture2DWidth);
    GET_ATTR(MaxTexture2DHeight);
    GET_ATTR(MaxTexture3DWidth);
    GET_ATTR(MaxTexture3DHeight);
    GET_ATTR(MaxTexture3DDepth);
    GET_ATTR(ConcurrentKernels);
    GET_ATTR(EccEnabled);
    GET_ATTR(MemoryClockRate);
    GET_ATTR(MemoryBusWidth);
    GET_ATTR(L2CacheSize);
    GET_ATTR(MaxThreadsPerMultiProcessor);
    GET_ATTR(ComputeCapabilityMajor);
    GET_ATTR(ComputeCapabilityMinor);
    GET_ATTR(MaxSharedMemoryPerMultiprocessor);
    GET_ATTR(ManagedMemory);
    GET_ATTR(IsMultiGpuBoard);
#  undef GET_ATTR
    capabilities += "\n";
  }

  return capabilities;

#else  /* WITH_HIP */
  return "";
#endif /* WITH_HIP */
}

CCL_NAMESPACE_END
