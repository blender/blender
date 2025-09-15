/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Wrap OpenGL features such as textures, shaders and GLSL
 * with checks for drivers and GPU support.
 */

#include <cstdint>

#include "MEM_guardedalloc.h"

#include "BLI_dynstr.h"
#include "BLI_string.h"
#include "BLI_string_utils.hh"
#include "BLI_vector.hh"

#include "GPU_platform.hh"

#include "gpu_platform_private.hh"

/* -------------------------------------------------------------------- */
/** \name GPUPlatformGlobal
 * \{ */

namespace blender::gpu {

GPUPlatformGlobal GPG;

static char *create_key(GPUSupportLevel support_level,
                        const char *vendor,
                        const char *renderer,
                        const char *version)
{
  DynStr *ds = BLI_dynstr_new();
  BLI_dynstr_appendf(ds, "{%s/%s/%s}=", vendor, renderer, version);
  if (support_level == GPU_SUPPORT_LEVEL_SUPPORTED) {
    BLI_dynstr_append(ds, "SUPPORTED");
  }
  else if (support_level == GPU_SUPPORT_LEVEL_LIMITED) {
    BLI_dynstr_append(ds, "LIMITED");
  }
  else {
    BLI_dynstr_append(ds, "UNSUPPORTED");
  }

  char *support_key = BLI_dynstr_get_cstring(ds);
  BLI_dynstr_free(ds);
  BLI_string_replace_char(support_key, '\n', ' ');
  BLI_string_replace_char(support_key, '\r', ' ');
  return support_key;
}

static char *create_gpu_name(const char *vendor, const char *renderer, const char *version)
{
  DynStr *ds = BLI_dynstr_new();
  BLI_dynstr_appendf(ds, "%s %s %s", vendor, renderer, version);

  char *gpu_name = BLI_dynstr_get_cstring(ds);
  BLI_dynstr_free(ds);
  BLI_string_replace_char(gpu_name, '\n', ' ');
  BLI_string_replace_char(gpu_name, '\r', ' ');
  return gpu_name;
}

void GPUPlatformGlobal::init(GPUDeviceType gpu_device,
                             GPUOSType os_type,
                             GPUDriverType driver_type,
                             GPUSupportLevel gpu_support_level,
                             GPUBackendType backend,
                             const char *vendor_str,
                             const char *renderer_str,
                             const char *version_str,
                             GPUArchitectureType arch_type)
{
  this->clear();

  this->initialized = true;

  this->device = gpu_device;
  this->os = os_type;
  this->driver = driver_type;
  this->support_level = gpu_support_level;

  const char *vendor = vendor_str ? vendor_str : "UNKNOWN";
  const char *renderer = renderer_str ? renderer_str : "UNKNOWN";
  const char *version = version_str ? version_str : "UNKNOWN";

  this->vendor = BLI_strdup(vendor);
  this->renderer = BLI_strdup(renderer);
  this->version = BLI_strdup(version);
  this->support_key = create_key(gpu_support_level, vendor, renderer, version);
  this->gpu_name = create_gpu_name(vendor, renderer, version);
  this->backend = backend;
  this->architecture_type = arch_type;
}

void GPUPlatformGlobal::clear()
{
  MEM_SAFE_FREE(vendor);
  MEM_SAFE_FREE(renderer);
  MEM_SAFE_FREE(version);
  MEM_SAFE_FREE(support_key);
  MEM_SAFE_FREE(gpu_name);
  devices.clear_and_shrink();
  device_uuid.reinitialize(0);
  device_luid.reinitialize(0);
  device_luid_node_mask = 0;
  initialized = false;
}

}  // namespace blender::gpu

/** \} */

/* -------------------------------------------------------------------- */
/** \name C-API
 * \{ */

using namespace blender::gpu;

GPUSupportLevel GPU_platform_support_level()
{
  BLI_assert(GPG.initialized);
  return GPG.support_level;
}

const char *GPU_platform_vendor()
{
  BLI_assert(GPG.initialized);
  return GPG.vendor;
}

const char *GPU_platform_renderer()
{
  BLI_assert(GPG.initialized);
  return GPG.renderer;
}

const char *GPU_platform_version()
{
  BLI_assert(GPG.initialized);
  return GPG.version;
}

const char *GPU_platform_support_level_key()
{
  BLI_assert(GPG.initialized);
  return GPG.support_key;
}

const char *GPU_platform_gpu_name()
{
  BLI_assert(GPG.initialized);
  return GPG.gpu_name;
}

GPUArchitectureType GPU_platform_architecture()
{
  BLI_assert(GPG.initialized);
  return GPG.architecture_type;
}

bool GPU_type_matches(GPUDeviceType device, GPUOSType os, GPUDriverType driver)
{
  return GPU_type_matches_ex(device, os, driver, GPU_BACKEND_ANY);
}

bool GPU_type_matches_ex(GPUDeviceType device,
                         GPUOSType os,
                         GPUDriverType driver,
                         GPUBackendType backend)
{
  BLI_assert(GPG.initialized);
  return (GPG.device & device) && (GPG.os & os) && (GPG.driver & driver) &&
         (GPG.backend & backend);
}

blender::Span<GPUDevice> GPU_platform_devices_list()
{
  return GPG.devices.as_span();
}

blender::Span<uint8_t> GPU_platform_uuid()
{
  return GPG.device_uuid.as_span();
}

blender::Span<uint8_t> GPU_platform_luid()
{
  return GPG.device_luid.as_span();
}

uint32_t GPU_platform_luid_node_mask()
{
  return GPG.device_luid_node_mask;
}

/** \} */
