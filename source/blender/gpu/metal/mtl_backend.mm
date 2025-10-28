/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include <cstring>

#include "BLI_threads.h"

#include "BKE_global.hh"

#include "gpu_backend.hh"
#include "mtl_backend.hh"
#include "mtl_batch.hh"
#include "mtl_context.hh"
#include "mtl_framebuffer.hh"
#include "mtl_immediate.hh"
#include "mtl_index_buffer.hh"
#include "mtl_query.hh"
#include "mtl_shader.hh"
#include "mtl_storage_buffer.hh"
#include "mtl_uniform_buffer.hh"
#include "mtl_vertex_buffer.hh"

#include "gpu_capabilities_private.hh"
#include "gpu_platform_private.hh"

#include <Cocoa/Cocoa.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>
#include <sys/sysctl.h>

namespace blender::gpu {

/* Global per-thread AutoReleasePool. */
thread_local NSAutoreleasePool *g_autoreleasepool = nil;
thread_local int g_autoreleasepool_depth = 0;

/* -------------------------------------------------------------------- */
/** \name Metal Backend
 * \{ */

void MTLBackend::init_resources()
{
  compiler_ = MEM_new<MTLShaderCompiler>(__func__);
}

void MTLBackend::delete_resources()
{
  MEM_delete(compiler_);
}

void MTLBackend::samplers_update() {
  /* Placeholder -- Handled in MTLContext. */
};

Context *MTLBackend::context_alloc(void *ghost_window, void *ghost_context)
{
  return new MTLContext(ghost_window, ghost_context);
};

Batch *MTLBackend::batch_alloc()
{
  return new MTLBatch();
};

Fence *MTLBackend::fence_alloc()
{
  return new MTLFence();
};

FrameBuffer *MTLBackend::framebuffer_alloc(const char *name)
{
  return new MTLFrameBuffer(MTLContext::get(), name);
};

IndexBuf *MTLBackend::indexbuf_alloc()
{
  return new MTLIndexBuf();
};

PixelBuffer *MTLBackend::pixelbuf_alloc(size_t size)
{
  return new MTLPixelBuffer(size);
};

QueryPool *MTLBackend::querypool_alloc()
{
  return new MTLQueryPool();
};

Shader *MTLBackend::shader_alloc(const char *name)
{
  return new MTLShader(MTLContext::get(), name);
};

Texture *MTLBackend::texture_alloc(const char *name)
{
  return new gpu::MTLTexture(name);
}

UniformBuf *MTLBackend::uniformbuf_alloc(size_t size, const char *name)
{
  return new MTLUniformBuf(size, name);
};

StorageBuf *MTLBackend::storagebuf_alloc(size_t size, GPUUsageType usage, const char *name)
{
  return new MTLStorageBuf(size, usage, name);
}

VertBuf *MTLBackend::vertbuf_alloc()
{
  return new MTLVertBuf();
}

void MTLBackend::render_begin()
{
  /* All Rendering must occur within a render boundary */
  /* Track a call-count for nested calls, used to ensure we are inside an
   * autoreleasepool from all rendering path. */
  BLI_assert(g_autoreleasepool_depth >= 0);

  if (g_autoreleasepool == nil) {
    g_autoreleasepool = [[NSAutoreleasePool alloc] init];
  }
  g_autoreleasepool_depth++;
  BLI_assert(g_autoreleasepool_depth > 0);
}

void MTLBackend::render_end()
{
  /* If call-count reaches zero, drain auto release pool.
   * Ensures temporary objects are freed within a frame's lifetime. */
  BLI_assert(g_autoreleasepool != nil);
  g_autoreleasepool_depth--;
  BLI_assert(g_autoreleasepool_depth >= 0);

  if (g_autoreleasepool_depth == 0) {
    [g_autoreleasepool drain];
    g_autoreleasepool = nil;
  }
}

void MTLBackend::render_step(bool force_resource_release)
{
  /* NOTE(Metal): Primarily called from main thread, but below data-structures
   * and operations are thread-safe, and GPUContext rendering coordination
   * is also thread-safe. */

  /* Flush any MTLSafeFreeLists which have previously been released by any MTLContext. */
  MTLContext::get_global_memory_manager()->update_memory_pools();

  /* End existing MTLSafeFreeList and begin new list --
   * Buffers wont `free` until all associated in-flight command buffers have completed.
   * Decrement final reference count for ensuring the previous list is certainly
   * released. */
  MTLSafeFreeList *cmd_free_buffer_list =
      MTLContext::get_global_memory_manager()->get_current_safe_list();
  if (cmd_free_buffer_list->should_flush()) {
    MTLContext::get_global_memory_manager()->begin_new_safe_list();
  }

  if (force_resource_release && g_autoreleasepool) {
    [g_autoreleasepool drain];
    g_autoreleasepool = [[NSAutoreleasePool alloc] init];
  }
}

bool MTLBackend::is_inside_render_boundary()
{
  return (g_autoreleasepool != nil);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Platform
 * \{ */

/* For Metal, platform_init needs to be called after MTLContext initialization. */
void MTLBackend::platform_init(MTLContext *ctx)
{
  if (GPG.initialized) {
    return;
  }

  GPUDeviceType device = GPU_DEVICE_UNKNOWN;
  GPUOSType os = GPU_OS_MAC;
  GPUDriverType driver = GPU_DRIVER_ANY;
  GPUSupportLevel support_level = GPU_SUPPORT_LEVEL_SUPPORTED;

  BLI_assert(ctx);
  id<MTLDevice> mtl_device = ctx->device;
  BLI_assert(device);

  NSString *gpu_name = [mtl_device name];
  const char *vendor = [gpu_name UTF8String];
  const char *renderer = "Metal API";
  const char *version = "1.2";
  if (G.debug & G_DEBUG_GPU) {
    printf("METAL API - DETECTED GPU: %s\n", vendor);
  }

  /* macOS is the only supported platform, but check to ensure we are not building with Metal
   * enablement on another platform. */
  BLI_assert_msg(os == GPU_OS_MAC, "Platform must be macOS");

  /* Determine Vendor from name. */
  if (strstr(vendor, "ATI") || strstr(vendor, "AMD")) {
    device = GPU_DEVICE_ATI;
    driver = GPU_DRIVER_OFFICIAL;
  }
  else if (strstr(vendor, "NVIDIA")) {
    device = GPU_DEVICE_NVIDIA;
    driver = GPU_DRIVER_OFFICIAL;
  }
  else if (strstr(vendor, "Intel")) {
    device = GPU_DEVICE_INTEL;
    driver = GPU_DRIVER_OFFICIAL;
    support_level = GPU_SUPPORT_LEVEL_LIMITED;
  }
  else if (strstr(vendor, "Apple") || strstr(vendor, "APPLE")) {
    /* Apple Silicon. */
    device = GPU_DEVICE_APPLE;
    driver = GPU_DRIVER_OFFICIAL;
  }
  else if (strstr(renderer, "Apple Software Renderer")) {
    device = GPU_DEVICE_SOFTWARE;
    driver = GPU_DRIVER_SOFTWARE;
  }
  else if (strstr(renderer, "llvmpipe") || strstr(renderer, "softpipe")) {
    device = GPU_DEVICE_SOFTWARE;
    driver = GPU_DRIVER_SOFTWARE;
  }
  else if (G.debug & G_DEBUG_GPU) {
    printf("Warning: Could not find a matching GPU name. Things may not behave as expected.\n");
    printf("Detected configuration:\n");
    printf("Vendor: %s\n", vendor);
    printf("Renderer: %s\n", renderer);
  }

  GPUArchitectureType architecture_type = (mtl_device.hasUnifiedMemory &&
                                           device == GPU_DEVICE_APPLE) ?
                                              GPU_ARCHITECTURE_TBDR :
                                              GPU_ARCHITECTURE_IMR;

  GPG.init(device,
           os,
           driver,
           support_level,
           GPU_BACKEND_METAL,
           vendor,
           renderer,
           version,
           architecture_type);

  /* UUID is not supported on Metal. */
  GPG.device_uuid.reinitialize(0);

  /* LUID is registryID on Metal, or at least this is what libraries like OIDN expects. */
  const uint64_t luid = mtl_device.registryID;
  GPG.device_luid.reinitialize(sizeof(luid));
  std::memcpy(GPG.device_luid.data(), &luid, sizeof(luid));

  /* Metal only has one device per LUID, so only the first bit will always be active.. */
  GPG.device_luid_node_mask = 1;
}

void MTLBackend::platform_exit()
{
  BLI_assert(GPG.initialized);
  GPG.clear();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Capabilities
 * \{ */
MTLCapabilities MTLBackend::capabilities = {};

static const char *mtl_extensions_get_null(int /*i*/)
{
  return nullptr;
}

bool supports_barycentric_whitelist(id<MTLDevice> device)
{
  NSString *gpu_name = [device name];
  BLI_assert([gpu_name length]);
  const char *vendor = [gpu_name UTF8String];

  /* Verify GPU support. */
  bool supported_gpu = [device supportsFamily:MTLGPUFamilyMac2];
  bool should_support_barycentrics = false;

  /* Known good configs. */
  if (strstr(vendor, "AMD") || strstr(vendor, "Apple") || strstr(vendor, "APPLE")) {
    should_support_barycentrics = true;
  }

  /* Explicit support for Intel-based platforms. */
  if ((strstr(vendor, "Intel") || strstr(vendor, "INTEL"))) {
    should_support_barycentrics = true;
  }
  return supported_gpu && should_support_barycentrics;
}

bool is_apple_sillicon(id<MTLDevice> device)
{
  NSString *gpu_name = [device name];
  BLI_assert([gpu_name length]);

  const char *vendor = [gpu_name UTF8String];

  /* Known good configs. */
  return (strstr(vendor, "Apple") || strstr(vendor, "APPLE"));
}

static int get_num_performance_cpu_cores(id<MTLDevice> device)
{
  const int SYSCTL_BUF_LENGTH = 16;
  int num_performance_cores = -1;
  unsigned char sysctl_buffer[SYSCTL_BUF_LENGTH];
  size_t sysctl_buffer_length = SYSCTL_BUF_LENGTH;

  if (is_apple_sillicon(device)) {
    /* On Apple Silicon query the number of performance cores */
    if (sysctlbyname(
            "hw.perflevel0.logicalcpu", &sysctl_buffer, &sysctl_buffer_length, nullptr, 0) == 0)
    {
      num_performance_cores = sysctl_buffer[0];
    }
  }
  else {
    /* On Intel just return the logical core count */
    if (sysctlbyname("hw.logicalcpu", &sysctl_buffer, &sysctl_buffer_length, nullptr, 0) == 0) {
      num_performance_cores = sysctl_buffer[0];
    }
  }
  BLI_assert(num_performance_cores != -1);
  return num_performance_cores;
}

static int get_num_efficiency_cpu_cores(id<MTLDevice> device)
{
  if (is_apple_sillicon(device)) {
    /* On Apple Silicon query the number of efficiency cores */
    const int SYSCTL_BUF_LENGTH = 16;
    int num_efficiency_cores = -1;
    unsigned char sysctl_buffer[SYSCTL_BUF_LENGTH];
    size_t sysctl_buffer_length = SYSCTL_BUF_LENGTH;
    if (sysctlbyname(
            "hw.perflevel1.logicalcpu", &sysctl_buffer, &sysctl_buffer_length, nullptr, 0) == 0)
    {
      num_efficiency_cores = sysctl_buffer[0];
    }

    BLI_assert(num_efficiency_cores != -1);
    return num_efficiency_cores;
  }
  return 0;
}

bool MTLBackend::metal_is_supported()
{
  /* Device compatibility information using Metal Feature-set tables.
   * See: https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf */

  NSOperatingSystemVersion version = [[NSProcessInfo processInfo] operatingSystemVersion];

  /* Metal Viewport requires macOS Version 10.15 onward. */
  bool supported_os_version = version.majorVersion >= 11 ||
                              (version.majorVersion == 10 ? version.minorVersion >= 15 : false);
  if (!supported_os_version) {
    printf(
        "OS Version too low to run minimum required metal version. Required at least 10.15, got "
        "%ld.%ld \n",
        (long)version.majorVersion,
        (long)version.minorVersion);
    return false;
  }

  id<MTLDevice> device = MTLCreateSystemDefaultDevice();

  /* Debug: Enable low power GPU with Environment Var: METAL_FORCE_INTEL. */
  static const char *forceIntelStr = getenv("METAL_FORCE_INTEL");
  bool forceIntel = forceIntelStr ? (atoi(forceIntelStr) != 0) : false;

  if (forceIntel) {
    NSArray<id<MTLDevice>> *allDevices = MTLCopyAllDevices();
    for (id<MTLDevice> _device in allDevices) {
      if (_device.lowPower) {
        device = _device;
      }
    }
  }

  /* Metal Viewport requires argument buffer tier-2 support and Barycentric Coordinates.
   * These are available on most hardware configurations supporting Metal 2.2. */
  bool supports_argument_buffers_tier2 = ([device argumentBuffersSupport] ==
                                          MTLArgumentBuffersTier2);
  bool supports_barycentrics = [device supportsShaderBarycentricCoordinates] ||
                               supports_barycentric_whitelist(device);
  bool supported_metal_version = [device supportsFamily:MTLGPUFamilyMac2];

  bool result = supports_argument_buffers_tier2 && supports_barycentrics && supported_os_version &&
                supported_metal_version;

  if (G.debug & G_DEBUG_GPU) {
    if (!supports_argument_buffers_tier2) {
      printf("[Metal] Device does not support argument buffers tier 2\n");
    }
    if (!supports_barycentrics) {
      printf("[Metal] Device does not support barycentrics coordinates\n");
    }
    if (!supported_metal_version) {
      printf("[Metal] Device does not support metal 2.2 or higher\n");
    }

    if (result) {
      printf("Device with name %s supports metal minimum requirements\n",
             [[device name] UTF8String]);
    }
  }

  return result;
}

void MTLBackend::capabilities_init(MTLContext *ctx)
{
  BLI_assert(ctx);
  id<MTLDevice> device = ctx->device;
  BLI_assert(device);

  /* Initialize Capabilities. */
  MTLBackend::capabilities.supports_argument_buffers_tier2 = ([device argumentBuffersSupport] ==
                                                              MTLArgumentBuffersTier2);
  MTLBackend::capabilities.supports_family_mac1 = [device supportsFamily:MTLGPUFamilyMac1];
  MTLBackend::capabilities.supports_family_mac2 = [device supportsFamily:MTLGPUFamilyMac2];
  MTLBackend::capabilities.supports_family_mac_catalyst1 = [device
      supportsFamily:MTLGPUFamilyMacCatalyst1];
  MTLBackend::capabilities.supports_family_mac_catalyst2 = [device
      supportsFamily:MTLGPUFamilyMacCatalyst2];
  /* NOTE(Metal): Texture gather is supported on AMD, but results are non consistent
   * with Apple Silicon GPUs. Disabling for now to avoid erroneous rendering. */
  MTLBackend::capabilities.supports_texture_gather = [device hasUnifiedMemory];

  /* GPU Type. */
  const char *gpu_name = [device.name UTF8String];
  if (strstr(gpu_name, "M1")) {
    MTLBackend::capabilities.gpu = APPLE_GPU_M1;
  }
  else if (strstr(gpu_name, "M2")) {
    MTLBackend::capabilities.gpu = APPLE_GPU_M2;
  }
  else if (strstr(gpu_name, "M3")) {
    MTLBackend::capabilities.gpu = APPLE_GPU_M3;
  }
  else {
    MTLBackend::capabilities.gpu = APPLE_GPU_UNKNOWN;
  }

  /* Texture atomics supported in Metal 3.1. */
  MTLBackend::capabilities.supports_texture_atomics = false;
#if defined(MAC_OS_VERSION_14_0)
  if (@available(macOS 14.0, *)) {
    MTLBackend::capabilities.supports_texture_atomics = true;
  }
#endif

  /** Identify support for tile inputs. */
  const bool is_tile_based_arch = (GPU_platform_architecture() == GPU_ARCHITECTURE_TBDR);
  if (is_tile_based_arch) {
    MTLBackend::capabilities.supports_native_tile_inputs = true;
  }
  else {
    /* NOTE: If emulating tile input reads, we must ensure we also expose position data. */
    MTLBackend::capabilities.supports_native_tile_inputs = false;
  }

  /* CPU Info */
  MTLBackend::capabilities.num_performance_cores = get_num_performance_cpu_cores(ctx->device);
  MTLBackend::capabilities.num_efficiency_cores = get_num_efficiency_cpu_cores(ctx->device);

  /* Common Global Capabilities. */
  GCaps.max_texture_size = ([device supportsFamily:MTLGPUFamilyApple3] ||
                            MTLBackend::capabilities.supports_family_mac1) ?
                               16384 :
                               8192;
  GCaps.max_texture_3d_size = 2048;
  GCaps.max_buffer_texture_size = UINT_MAX;
  GCaps.max_texture_layers = 2048;
  GCaps.max_textures = (MTLBackend::capabilities.supports_family_mac1) ?
                           128 :
                           (([device supportsFamily:MTLGPUFamilyApple4]) ? 96 : 31);
  if (GCaps.max_textures <= 32) {
    BLI_assert(false);
  }
  GCaps.max_samplers = (MTLBackend::capabilities.supports_argument_buffers_tier2) ? 1024 : 16;

  GCaps.max_textures_vert = GCaps.max_textures;
  GCaps.max_textures_geom = 0; /* N/A geometry shaders not supported. */
  GCaps.max_textures_frag = GCaps.max_textures;

  GCaps.max_images = GCaps.max_textures;

  /* Conservative uniform data limit is 4KB per-stage -- This is the limit of setBytes.
   * MTLBuffer path is also supported but not as efficient. */
  GCaps.max_uniforms_vert = 1024;
  GCaps.max_uniforms_frag = 1024;

  GCaps.max_batch_indices = 1 << 31;
  GCaps.max_batch_vertices = 1 << 31;
  GCaps.max_vertex_attribs = 31;
  GCaps.max_varying_floats = 60;

  /* Feature support */
  GCaps.mem_stats_support = false;
  GCaps.hdr_viewport_support = true;

  GCaps.geometry_shader_support = false;

  /* Compile shaders on performance cores but leave one free so UI is still responsive.
   * Also respect command line option to reduce number of threads. */
  GCaps.max_parallel_compilations = std::min(BLI_system_thread_count(),
                                             MTLBackend::capabilities.num_performance_cores - 1);

  /* Maximum buffer bindings: 31. Consider required slot for uniforms/UBOs/Vertex attributes.
   * Can use argument buffers if a higher limit is required. */
  GCaps.max_shader_storage_buffer_bindings = 14;
  GCaps.max_compute_shader_storage_blocks = 14;
  GCaps.max_storage_buffer_size = size_t(ctx->device.maxBufferLength);
  GCaps.max_uniform_buffer_size = size_t(ctx->device.maxBufferLength);
  GCaps.storage_buffer_alignment = 256; /* TODO(fclem): But also unused. */

  GCaps.max_work_group_count[0] = 65535;
  GCaps.max_work_group_count[1] = 65535;
  GCaps.max_work_group_count[2] = 65535;
  /* In Metal, total_thread_count is 512 or 1024, such that
   * threadgroup `width*height*depth <= total_thread_count` */
  uint max_threads_per_threadgroup_per_dim = ([device supportsFamily:MTLGPUFamilyApple4] ||
                                              MTLBackend::capabilities.supports_family_mac1) ?
                                                 1024 :
                                                 512;
  GCaps.max_work_group_size[0] = max_threads_per_threadgroup_per_dim;
  GCaps.max_work_group_size[1] = max_threads_per_threadgroup_per_dim;
  GCaps.max_work_group_size[2] = max_threads_per_threadgroup_per_dim;

  GCaps.stencil_export_support = true;

  /* OPENGL Related workarounds -- none needed for Metal. */
  GCaps.extensions_len = 0;
  GCaps.extension_get = mtl_extensions_get_null;
  GCaps.depth_blitting_workaround = false;
  GCaps.use_main_context_workaround = false;

  /* Metal related workarounds. */
  /* Minimum per-vertex stride is 4 bytes in Metal.
   * A bound vertex buffer must contribute at least 4 bytes per vertex. */
  GCaps.minimum_per_vertex_stride = 4;

  /* Force workarounds when starting blender with `--debug-gpu-force-workarounds`.
   *
   * Not all workarounds are listed here as some capabilities are currently assumed to be present
   * on all devices. */
  if (G.debug & G_DEBUG_GPU_FORCE_WORKAROUNDS) {
    /* Texture gather is supported on AMD, but results are non consistent with Apple Silicon GPUs
     * and can be disabled. */
    MTLBackend::capabilities.supports_texture_gather = false;
    MTLBackend::capabilities.supports_texture_atomics = false;
    MTLBackend::capabilities.supports_native_tile_inputs = false;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Compute dispatch.
 * \{ */

void MTLBackend::compute_dispatch(int groups_x_len, int groups_y_len, int groups_z_len)
{
  /* Fetch Context.
   * With Metal, workload submission and resource management occurs within the context.
   * Call compute dispatch on valid context. */
  MTLContext *ctx = MTLContext::get();
  BLI_assert(ctx != nullptr);
  if (ctx) {
    ctx->compute_dispatch(groups_x_len, groups_y_len, groups_z_len);
  }
}

void MTLBackend::compute_dispatch_indirect(StorageBuf *indirect_buf)
{
  /* Fetch Context.
   * With Metal, workload submission and resource management occurs within the context.
   * Call compute dispatch on valid context. */
  MTLContext *ctx = MTLContext::get();
  BLI_assert(ctx != nullptr);
  if (ctx) {
    ctx->compute_dispatch_indirect(indirect_buf);
  }
}

/** \} */

}  // namespace blender::gpu
