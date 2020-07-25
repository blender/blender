/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Wrap OpenGL features such as textures, shaders and GLSL
 * with checks for drivers and GPU support.
 */
#include "GPU_platform.h"
#include "GPU_glew.h"
#include "gpu_private.h"

#include <string.h>

#include "BLI_dynstr.h"
#include "BLI_string.h"

#include "MEM_guardedalloc.h"

static struct GPUPlatformGlobal {
  bool initialized;
  eGPUDeviceType device;
  eGPUOSType os;
  eGPUDriverType driver;
  eGPUSupportLevel support_level;
  char *support_key;
  char *gpu_name;
} GPG = {false};

/* Remove this? */
#if 0
typedef struct GPUPlatformSupportTest {
  eGPUSupportLevel support_level;
  eGPUDeviceType device;
  eGPUOSType os;
  eGPUDriverType driver;
  const char *vendor;
  const char *renderer;
  const char *version;
} GPUPlatformSupportTest;
#endif

eGPUSupportLevel GPU_platform_support_level(void)
{
  return GPG.support_level;
}

const char *GPU_platform_support_level_key(void)
{
  return GPG.support_key;
}

const char *GPU_platform_gpu_name(void)
{
  return GPG.gpu_name;
}

/* GPU Types */
bool GPU_type_matches(eGPUDeviceType device, eGPUOSType os, eGPUDriverType driver)
{
  return (GPG.device & device) && (GPG.os & os) && (GPG.driver & driver);
}

static char *gpu_platform_create_key(eGPUSupportLevel support_level,
                                     const char *vendor,
                                     const char *renderer,
                                     const char *version)
{
  DynStr *ds = BLI_dynstr_new();
  BLI_dynstr_append(ds, "{");
  BLI_dynstr_append(ds, vendor);
  BLI_dynstr_append(ds, "/");
  BLI_dynstr_append(ds, renderer);
  BLI_dynstr_append(ds, "/");
  BLI_dynstr_append(ds, version);
  BLI_dynstr_append(ds, "}");
  BLI_dynstr_append(ds, "=");
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
  BLI_str_replace_char(support_key, '\n', ' ');
  BLI_str_replace_char(support_key, '\r', ' ');
  return support_key;
}

static char *gpu_platform_create_gpu_name(const char *vendor,
                                          const char *renderer,
                                          const char *version)
{
  DynStr *ds = BLI_dynstr_new();
  BLI_dynstr_append(ds, vendor);
  BLI_dynstr_append(ds, " ");
  BLI_dynstr_append(ds, renderer);
  BLI_dynstr_append(ds, " ");
  BLI_dynstr_append(ds, version);

  char *gpu_name = BLI_dynstr_get_cstring(ds);
  BLI_dynstr_free(ds);
  BLI_str_replace_char(gpu_name, '\n', ' ');
  BLI_str_replace_char(gpu_name, '\r', ' ');
  return gpu_name;
}

void gpu_platform_init(void)
{
  if (GPG.initialized) {
    return;
  }

#ifdef _WIN32
  GPG.os = GPU_OS_WIN;
#elif defined(__APPLE__)
  GPG.os = GPU_OS_MAC;
#else
  GPG.os = GPU_OS_UNIX;
#endif

  const char *vendor = (const char *)glGetString(GL_VENDOR);
  const char *renderer = (const char *)glGetString(GL_RENDERER);
  const char *version = (const char *)glGetString(GL_VERSION);

  if (strstr(vendor, "ATI") || strstr(vendor, "AMD")) {
    GPG.device = GPU_DEVICE_ATI;
    GPG.driver = GPU_DRIVER_OFFICIAL;
  }
  else if (strstr(vendor, "NVIDIA")) {
    GPG.device = GPU_DEVICE_NVIDIA;
    GPG.driver = GPU_DRIVER_OFFICIAL;
  }
  else if (strstr(vendor, "Intel") ||
           /* src/mesa/drivers/dri/intel/intel_context.c */
           strstr(renderer, "Mesa DRI Intel") || strstr(renderer, "Mesa DRI Mobile Intel")) {
    GPG.device = GPU_DEVICE_INTEL;
    GPG.driver = GPU_DRIVER_OFFICIAL;

    if (strstr(renderer, "UHD Graphics") ||
        /* Not UHD but affected by the same bugs. */
        strstr(renderer, "HD Graphics 530") || strstr(renderer, "Kaby Lake GT2") ||
        strstr(renderer, "Whiskey Lake")) {
      GPG.device |= GPU_DEVICE_INTEL_UHD;
    }
  }
  else if ((strstr(renderer, "Mesa DRI R")) ||
           (strstr(renderer, "Radeon") && strstr(vendor, "X.Org")) ||
           (strstr(renderer, "AMD") && strstr(vendor, "X.Org")) ||
           (strstr(renderer, "Gallium ") && strstr(renderer, " on ATI ")) ||
           (strstr(renderer, "Gallium ") && strstr(renderer, " on AMD "))) {
    GPG.device = GPU_DEVICE_ATI;
    GPG.driver = GPU_DRIVER_OPENSOURCE;
  }
  else if (strstr(renderer, "Nouveau") || strstr(vendor, "nouveau")) {
    GPG.device = GPU_DEVICE_NVIDIA;
    GPG.driver = GPU_DRIVER_OPENSOURCE;
  }
  else if (strstr(vendor, "Mesa")) {
    GPG.device = GPU_DEVICE_SOFTWARE;
    GPG.driver = GPU_DRIVER_SOFTWARE;
  }
  else if (strstr(vendor, "Microsoft")) {
    GPG.device = GPU_DEVICE_SOFTWARE;
    GPG.driver = GPU_DRIVER_SOFTWARE;
  }
  else if (strstr(renderer, "Apple Software Renderer")) {
    GPG.device = GPU_DEVICE_SOFTWARE;
    GPG.driver = GPU_DRIVER_SOFTWARE;
  }
  else if (strstr(renderer, "llvmpipe") || strstr(renderer, "softpipe")) {
    GPG.device = GPU_DEVICE_SOFTWARE;
    GPG.driver = GPU_DRIVER_SOFTWARE;
  }
  else {
    printf("Warning: Could not find a matching GPU name. Things may not behave as expected.\n");
    printf("Detected OpenGL configuration:\n");
    printf("Vendor: %s\n", vendor);
    printf("Renderer: %s\n", renderer);
    GPG.device = GPU_DEVICE_ANY;
    GPG.driver = GPU_DRIVER_ANY;
  }

  /* Detect support level */
  if (!GLEW_VERSION_3_3) {
    GPG.support_level = GPU_SUPPORT_LEVEL_UNSUPPORTED;
  }
  else {
    if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_WIN, GPU_DRIVER_ANY)) {
      /* Old Intel drivers with known bugs that cause material properties to crash.
       * Version Build 10.18.14.5067 is the latest available and appears to be working
       * ok with our workarounds, so excluded from this list. */
      if (strstr(version, "Build 7.14") || strstr(version, "Build 7.15") ||
          strstr(version, "Build 8.15") || strstr(version, "Build 9.17") ||
          strstr(version, "Build 9.18") || strstr(version, "Build 10.18.10.3") ||
          strstr(version, "Build 10.18.10.4") || strstr(version, "Build 10.18.10.5") ||
          strstr(version, "Build 10.18.14.4")) {
        GPG.support_level = GPU_SUPPORT_LEVEL_LIMITED;
      }
    }
  }
  GPG.support_key = gpu_platform_create_key(GPG.support_level, vendor, renderer, version);
  GPG.gpu_name = gpu_platform_create_gpu_name(vendor, renderer, version);
  GPG.initialized = true;
}

void gpu_platform_exit(void)
{
  MEM_SAFE_FREE(GPG.support_key);
  MEM_SAFE_FREE(GPG.gpu_name);
}
