/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "BKE_global.hh"
#if defined(WIN32)
#  include "BLI_winstuff.h"
#endif
#include "BLI_array.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_subprocess.hh"
#include "BLI_threads.h"
#include "BLI_vector.hh"

#include "DNA_userdef_types.h"

#include "gpu_capabilities_private.hh"
#include "gpu_platform_private.hh"

#include "gl_debug.hh"

#include "gl_backend.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Platform
 * \{ */

static bool match_renderer(StringRef renderer, const Vector<std::string> &items)
{
  for (const std::string &item : items) {
    const std::string wrapped = " " + item + " ";
    if (renderer.endswith(item) || renderer.find(wrapped) != StringRef::not_found) {
      return true;
    }
  }
  return false;
}

static bool parse_version(const std::string &version,
                          const std::string &format,
                          Vector<int> &r_version)
{
  int f = 0;
  std::string subversion;
  for (int v : IndexRange(version.size())) {
    bool match = false;
    if (format[f] == '0') {
      if (std::isdigit(version[v])) {
        match = true;
        subversion.push_back(version[v]);
      }
    }
    else {
      match = version[v] == format[f];
      if (!subversion.empty()) {
        r_version.append(std::stoi(subversion));
        subversion.clear();
      }
    }

    if (!match) {
      f = 0;
      subversion.clear();
      r_version.clear();
      continue;
    }

    f++;

    if (f == format.size()) {
      return true;
    }
  }

  return false;
}

/** Try to check if the driver is older than 22.6.1, preferring false positives. */
static bool is_bad_AMD_driver(const char *version_cstr)
{
  std::string version_str = version_cstr;
  /* Allow matches when the version number is at the string end. */
  version_str.push_back(' ');

  Vector<int> version;

  if (parse_version(version_str, " 00.00.00.00 ", version) ||
      parse_version(version_str, " 00.00.000000 ", version) ||
      parse_version(version_str, " 00.00.00 ", version) ||
      parse_version(version_str, " 00.00.0 ", version) ||
      parse_version(version_str, " 00.0.00 ", version) ||
      parse_version(version_str, " 00.Q0.", version))
  {
    return version[0] < 23;
  }
  /* Some drivers only expose the Windows version https://gpuopen.com/version-table/ */
  if (parse_version(version_str, " 00.00.00000.00000 ", version) ||
      parse_version(version_str, " 00.00.00000.0000 ", version) ||
      parse_version(version_str, " 00.00.0000.00000 ", version))
  {
    return version[0] < 31 || (version[0] == 31 && version[2] < 21001);
  }

  /* Unknown version, assume it's a bad one. */
  return true;
}

void GLBackend::platform_init()
{
  BLI_assert(!GPG.initialized);

  const char *vendor = (const char *)glGetString(GL_VENDOR);
  const char *renderer = (const char *)glGetString(GL_RENDERER);
  const char *version = (const char *)glGetString(GL_VERSION);
  GPUDeviceType device = GPU_DEVICE_ANY;
  GPUOSType os = GPU_OS_ANY;
  GPUDriverType driver = GPU_DRIVER_ANY;
  GPUSupportLevel support_level = GPU_SUPPORT_LEVEL_SUPPORTED;

#ifdef _WIN32
  os = GPU_OS_WIN;
#else
  os = GPU_OS_UNIX;
#endif

  if (!vendor) {
    printf("Warning: No OpenGL vendor detected.\n");
    device = GPU_DEVICE_UNKNOWN;
    driver = GPU_DRIVER_ANY;
  }
  else if (strstr(renderer, "Mesa DRI R") ||
           (strstr(renderer, "Radeon") && (strstr(vendor, "X.Org") || strstr(version, "Mesa"))) ||
           (strstr(renderer, "AMD") && (strstr(vendor, "X.Org") || strstr(version, "Mesa"))) ||
           (strstr(renderer, "Gallium ") && strstr(renderer, " on ATI ")) ||
           (strstr(renderer, "Gallium ") && strstr(renderer, " on AMD ")))
  {
    device = GPU_DEVICE_ATI;
    driver = GPU_DRIVER_OPENSOURCE;
  }
  else if (strstr(vendor, "ATI") || strstr(vendor, "AMD")) {
    device = GPU_DEVICE_ATI;
    driver = GPU_DRIVER_OFFICIAL;
  }
  else if (strstr(vendor, "NVIDIA")) {
    device = GPU_DEVICE_NVIDIA;
    driver = GPU_DRIVER_OFFICIAL;
  }
  else if (strstr(vendor, "Intel") ||
           /* src/mesa/drivers/dri/intel/intel_context.c */
           strstr(renderer, "Mesa DRI Intel") || strstr(renderer, "Mesa DRI Mobile Intel"))
  {
    device = GPU_DEVICE_INTEL;
    driver = GPU_DRIVER_OFFICIAL;

    if (strstr(renderer, "UHD Graphics") ||
        /* Not UHD but affected by the same bugs. */
        strstr(renderer, "HD Graphics 530") || strstr(renderer, "Kaby Lake GT2") ||
        strstr(renderer, "Whiskey Lake"))
    {
      device |= GPU_DEVICE_INTEL_UHD;
    }
  }
  else if (strstr(renderer, "Nouveau") || strstr(vendor, "nouveau")) {
    device = GPU_DEVICE_NVIDIA;
    driver = GPU_DRIVER_OPENSOURCE;
  }
  else if (strstr(vendor, "Mesa")) {
    device = GPU_DEVICE_SOFTWARE;
    driver = GPU_DRIVER_SOFTWARE;
  }
  else if (strstr(vendor, "Microsoft")) {
    /* Qualcomm devices use Mesa's GLOn12, which claims to be vended by Microsoft */
    if (strstr(renderer, "Qualcomm")) {
      device = GPU_DEVICE_QUALCOMM;
      driver = GPU_DRIVER_OFFICIAL;
    }
    else {
      device = GPU_DEVICE_SOFTWARE;
      driver = GPU_DRIVER_SOFTWARE;
    }
  }
  else if (strstr(vendor, "Apple")) {
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
  else {
    printf("Warning: Could not find a matching GPU name. Things may not behave as expected.\n");
    printf("Detected OpenGL configuration:\n");
    printf("Vendor: %s\n", vendor);
    printf("Renderer: %s\n", renderer);
  }

  /* Detect support level */
  if (!(epoxy_gl_version() >= 43)) {
    support_level = GPU_SUPPORT_LEVEL_UNSUPPORTED;
  }
  else {
#if defined(WIN32)
    long long driverVersion = 0;
    if (device & GPU_DEVICE_QUALCOMM) {
      if (BLI_windows_get_directx_driver_version(L"Qualcomm(R) Adreno(TM)", &driverVersion)) {
        /* Parse out the driver version in format x.x.x.x */
        WORD ver0 = (driverVersion >> 48) & 0xffff;
        WORD ver1 = (driverVersion >> 32) & 0xffff;
        WORD ver2 = (driverVersion >> 16) & 0xffff;

        /* Any Qualcomm driver older than 30.x.x.x will never capable of running blender >= 4.0
         * As due to an issue in D3D typed UAV load capabilities, Compute Shaders are not available
         * 30.0.3820.x and above are capable of running blender >=4.0, but these drivers
         * are only available on 8cx gen3 devices or newer */
        if (ver0 < 30 || (ver0 == 30 && ver1 == 0 && ver2 < 3820)) {
          std::cout
              << "=====================================\n"
              << "Qualcomm drivers older than 30.0.3820.x cannot run Blender 4.0 or later.\n"
              << "If your device is older than an 8cx Gen3, you must use a 3.x LTS release.\n"
              << "If you have an 8cx Gen3 or newer device, a driver update may be available.\n"
              << "=====================================\n";
          support_level = GPU_SUPPORT_LEVEL_UNSUPPORTED;
        }
      }
    }
#endif
    if ((device & GPU_DEVICE_INTEL) && (os & GPU_OS_WIN)) {
      /* Old Intel drivers with known bugs that cause material properties to crash.
       * Version Build 10.18.14.5067 is the latest available and appears to be working
       * ok with our workarounds, so excluded from this list. */
      if (strstr(version, "Build 7.14") || strstr(version, "Build 7.15") ||
          strstr(version, "Build 8.15") || strstr(version, "Build 9.17") ||
          strstr(version, "Build 9.18") || strstr(version, "Build 10.18.10.3") ||
          strstr(version, "Build 10.18.10.4") || strstr(version, "Build 10.18.10.5") ||
          strstr(version, "Build 10.18.14.4"))
      {
        support_level = GPU_SUPPORT_LEVEL_LIMITED;
      }
      /* A rare GPU that has z-fighting issues in edit mode. (see #128179) */
      if (strstr(renderer, "HD Graphics 405")) {
        support_level = GPU_SUPPORT_LEVEL_LIMITED;
      }
      /* Latest Intel driver have bugs that won't allow Blender to start.
       * Users must install different version of the driver.
       * See #113124 for more information. */
      if (strstr(version, "Build 20.19.15.51")) {
        support_level = GPU_SUPPORT_LEVEL_UNSUPPORTED;
      }
    }
    if ((device & GPU_DEVICE_ATI) && (os & GPU_OS_UNIX)) {
      /* Platform seems to work when SB backend is disabled. This can be done
       * by adding the environment variable `R600_DEBUG=nosb`. */
      if (strstr(renderer, "AMD CEDAR")) {
        support_level = GPU_SUPPORT_LEVEL_LIMITED;
      }
    }
    if ((device & GPU_DEVICE_QUALCOMM) && (os & GPU_OS_WIN)) {
      if (strstr(version, "Mesa 20.") || strstr(version, "Mesa 21.") ||
          strstr(version, "Mesa 22.") || strstr(version, "Mesa 23."))
      {
        std::cerr << "Unsupported driver. Requires at least Mesa 24.0.0." << std::endl;
        support_level = GPU_SUPPORT_LEVEL_UNSUPPORTED;
      }
    }

    /* Check SSBO bindings requirement. */
    GLint max_ssbo_binds_vertex;
    GLint max_ssbo_binds_fragment;
    GLint max_ssbo_binds_compute;
    glGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, &max_ssbo_binds_vertex);
    glGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &max_ssbo_binds_fragment);
    glGetIntegerv(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS, &max_ssbo_binds_compute);
    GLint max_ssbo_binds = min_iii(
        max_ssbo_binds_vertex, max_ssbo_binds_fragment, max_ssbo_binds_compute);
    if (max_ssbo_binds < 12) {
      std::cout << "Warning: Unsupported platform as it supports max " << max_ssbo_binds
                << " SSBO binding locations\n";
      support_level = GPU_SUPPORT_LEVEL_UNSUPPORTED;
    }

    if (!epoxy_has_gl_extension("GL_ARB_shader_draw_parameters")) {
      std::cout << "Error: The OpenGL implementation doesn't support ARB_shader_draw_parameters\n";
      support_level = GPU_SUPPORT_LEVEL_UNSUPPORTED;
    }

    if (!epoxy_has_gl_extension("GL_ARB_clip_control")) {
      std::cout << "Error: The OpenGL implementation doesn't support ARB_clip_control\n";
      support_level = GPU_SUPPORT_LEVEL_UNSUPPORTED;
    }
  }

  /* Compute shaders have some issues with those versions (see #94936). */
  if ((device & GPU_DEVICE_ATI) && (driver & GPU_DRIVER_OFFICIAL) &&
      (strstr(version, "4.5.14831") || strstr(version, "4.5.14760")))
  {
    support_level = GPU_SUPPORT_LEVEL_UNSUPPORTED;
  }

  GPG.init(device,
           os,
           driver,
           support_level,
           GPU_BACKEND_OPENGL,
           vendor,
           renderer,
           version,
           GPU_ARCHITECTURE_IMR);

  GPG.device_uuid.reinitialize(0);
  GPG.device_luid.reinitialize(0);
  GPG.device_luid_node_mask = 0;

  if (epoxy_has_gl_extension("GL_EXT_memory_object")) {
    GLint number_of_devices = 0;
    glGetIntegerv(GL_NUM_DEVICE_UUIDS_EXT, &number_of_devices);
    /* Multiple devices could be used by the context if certain extensions like multi-cast is used.
     * But this is not used by Blender, so this should always be 1. */
    BLI_assert(number_of_devices == 1);

    GLubyte device_uuid[GL_UUID_SIZE_EXT] = {0};
    glGetUnsignedBytei_vEXT(GL_DEVICE_UUID_EXT, 0, device_uuid);
    GPG.device_uuid = Array<uint8_t, 16>(Span<uint8_t>(device_uuid, GL_UUID_SIZE_EXT));

    /* LUID is only supported on Windows. */
    if (epoxy_has_gl_extension("GL_EXT_memory_object_win32") && (os & GPU_OS_WIN)) {
      GLubyte device_luid[GL_LUID_SIZE_EXT] = {0};
      glGetUnsignedBytevEXT(GL_DEVICE_LUID_EXT, device_luid);
      GPG.device_luid = Array<uint8_t, 8>(Span<uint8_t>(device_luid, GL_LUID_SIZE_EXT));

      GLint node_mask = 0;
      glGetIntegerv(GL_DEVICE_NODE_MASK_EXT, &node_mask);
      GPG.device_luid_node_mask = uint32_t(node_mask);
    }
  }
}

void GLBackend::platform_exit()
{
  BLI_assert(GPG.initialized);
  GPG.clear();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Capabilities
 * \{ */

static const char *gl_extension_get(int i)
{
  return (char *)glGetStringi(GL_EXTENSIONS, i);
}

static void detect_workarounds()
{
  const char *vendor = (const char *)glGetString(GL_VENDOR);
  const char *renderer = (const char *)glGetString(GL_RENDERER);
  const char *version = (const char *)glGetString(GL_VERSION);

  if (G.debug & G_DEBUG_GPU_FORCE_WORKAROUNDS) {
    printf("\n");
    printf("GL: Forcing workaround usage and disabling extensions.\n");
    printf("    OpenGL identification strings\n");
    printf("    vendor: %s\n", vendor);
    printf("    renderer: %s\n", renderer);
    printf("    version: %s\n\n", version);
    GCaps.depth_blitting_workaround = true;
    GCaps.stencil_clasify_buffer_workaround = true;
    GLContext::debug_layer_workaround = true;
    /* Turn off Blender features. */
    GCaps.hdr_viewport_support = false;
    /* Turn off OpenGL 4.4 features. */
    GLContext::multi_bind_support = false;
    GLContext::multi_bind_image_support = false;
    /* Turn off OpenGL 4.5 features. */
    GLContext::direct_state_access_support = false;
    /* Turn off OpenGL 4.6 features. */
    GLContext::texture_filter_anisotropic_support = false;
    /* Turn off extensions. */
    GLContext::layered_rendering_support = false;
    /* Turn off vendor specific extensions. */
    GLContext::native_barycentric_support = false;
    GLContext::framebuffer_fetch_support = false;
    GLContext::texture_barrier_support = false;
    GCaps.stencil_export_support = false;

#if 0
    /* Do not alter OpenGL 4.3 features.
     * These code paths should be removed. */
    GLContext::debug_layer_support = false;
#endif

    return;
  }

  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_WIN, GPU_DRIVER_OFFICIAL) &&
      (strstr(version, "4.5.13399") || strstr(version, "4.5.13417") ||
       strstr(version, "4.5.13422") || strstr(version, "4.5.13467")))
  {
    /* The renderers include:
     *   Radeon HD 5000;
     *   Radeon HD 7500M;
     *   Radeon HD 7570M;
     *   Radeon HD 7600M;
     *   Radeon R5 Graphics;
     * And others... */
    GLContext::unused_fb_slot_workaround = true;
  }
  /* We have issues with this specific renderer. (see #74024) */
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_UNIX, GPU_DRIVER_OPENSOURCE) &&
      (strstr(renderer, "AMD VERDE") || strstr(renderer, "AMD KAVERI") ||
       strstr(renderer, "AMD TAHITI")))
  {
    GLContext::unused_fb_slot_workaround = true;
  }
  /* See #82856: AMD drivers since 20.11 running on a polaris architecture doesn't support the
   * `GL_INT_2_10_10_10_REV` data type correctly. This data type is used to pack normals and flags.
   * The work around uses `TextureFormat::SINT_16_16_16_16`. In 22.?.? drivers this
   * has been fixed for polaris platform. Keeping legacy platforms around just in case.
   */
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_OFFICIAL)) {
    /* Check for AMD legacy driver. Assuming that when these drivers are used this bug is present.
     */
    if (is_bad_AMD_driver(version)) {
      GCaps.use_hq_normals_workaround = true;
    }
    const Vector<std::string> matches = {
        "RX550/550", "(TM) 520", "(TM) 530", "(TM) 535", "R5", "R7", "R9", "HD"};

    if (match_renderer(renderer, matches)) {
      GCaps.use_hq_normals_workaround = true;
    }
  }

  /* Maybe not all of these drivers have problems with `GL_ARB_base_instance`.
   * But it's hard to test each case.
   * We get crashes from some crappy Intel drivers don't work well with shaders created in
   * different rendering contexts. */
  if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_WIN, GPU_DRIVER_ANY) &&
      (strstr(version, "Build 10.18.10.3") || strstr(version, "Build 10.18.10.4") ||
       strstr(version, "Build 10.18.10.5") || strstr(version, "Build 10.18.14.4") ||
       strstr(version, "Build 10.18.14.5")))
  {
    GCaps.use_main_context_workaround = true;
  }
  /* Somehow fixes armature display issues (see #69743). */
  if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_WIN, GPU_DRIVER_ANY) &&
      strstr(version, "Build 20.19.15.4285"))
  {
    GCaps.use_main_context_workaround = true;
  }
  /* Needed to avoid driver hangs on legacy AMD drivers (see #139939). */
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_OFFICIAL) &&
      is_bad_AMD_driver(version))
  {
    GCaps.use_main_context_workaround = true;
  }
  /* See #70187: merging vertices fail. This has been tested from `18.2.2` till `19.3.0~dev`
   * of the Mesa driver */
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_UNIX, GPU_DRIVER_OPENSOURCE) &&
      (strstr(version, "Mesa 18.") || strstr(version, "Mesa 19.0") ||
       strstr(version, "Mesa 19.1") || strstr(version, "Mesa 19.2")))
  {
    GLContext::unused_fb_slot_workaround = true;
  }

/* Snapdragon X Elite devices currently have a driver bug that results in
 * eevee rendering a black cube with anything except an emission shader
 * if shader draw parameters are enabled (#122837) */
#if defined(WIN32)
  long long driverVersion = 0;
  if (GPU_type_matches(GPU_DEVICE_QUALCOMM, GPU_OS_WIN, GPU_DRIVER_ANY)) {
    if (BLI_windows_get_directx_driver_version(L"Qualcomm(R) Adreno(TM)", &driverVersion)) {
      /* Parse out the driver version */
      WORD ver0 = (driverVersion >> 48) & 0xffff;

      /* X Elite devices have GPU driver version 31, and currently no known release version of the
       * GPU driver renders the cube correctly. This will be changed when a working driver version
       * is released to commercial devices to only enable this flags on older drivers. */
      if (ver0 == 31) {
        GCaps.stencil_clasify_buffer_workaround = true;
      }
    }
  }
#endif

  /* Enable our own incomplete debug layer if no other is available. */
  if (GLContext::debug_layer_support == false) {
    GLContext::debug_layer_workaround = true;
  }

  /* There is an issue in AMD official driver where we cannot use multi bind when using images. AMD
   * is aware of the issue, but hasn't released a fix. */
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_OFFICIAL)) {
    GLContext::multi_bind_image_support = false;
  }

  /* #107642, #120273 Windows Intel iGPU (multiple generations) incorrectly report that
   * they support image binding. But when used it results into `GL_INVALID_OPERATION` with
   * `internal format of texture N is not supported`. */
  if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_WIN, GPU_DRIVER_OFFICIAL)) {
    GLContext::multi_bind_image_support = false;
  }

  /* Metal-related Workarounds. */

  /* Minimum Per-Vertex stride is 1 byte for OpenGL. */
  GCaps.minimum_per_vertex_stride = 1;
}

/** Internal capabilities. */

GLint GLContext::max_cubemap_size = 0;
GLint GLContext::max_ubo_binds = 0;
GLint GLContext::max_ssbo_binds = 0;

/** Extensions. */

bool GLContext::debug_layer_support = false;
bool GLContext::direct_state_access_support = false;
bool GLContext::explicit_location_support = false;
bool GLContext::framebuffer_fetch_support = false;
bool GLContext::layered_rendering_support = false;
bool GLContext::native_barycentric_support = false;
bool GLContext::multi_bind_support = false;
bool GLContext::multi_bind_image_support = false;
bool GLContext::stencil_texturing_support = false;
bool GLContext::texture_barrier_support = false;
bool GLContext::texture_filter_anisotropic_support = false;

/** Workarounds. */

bool GLContext::debug_layer_workaround = false;
bool GLContext::unused_fb_slot_workaround = false;
bool GLContext::generate_mipmap_workaround = false;

void GLBackend::capabilities_init()
{
  BLI_assert(epoxy_gl_version() >= 33);
  /* Common Capabilities. */
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &GCaps.max_texture_size);
  glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &GCaps.max_texture_layers);
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &GCaps.max_textures_frag);
  glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &GCaps.max_textures_vert);
  glGetIntegerv(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS, &GCaps.max_textures_geom);
  glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &GCaps.max_textures);
  glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &GCaps.max_uniforms_vert);
  glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &GCaps.max_uniforms_frag);
  glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &GCaps.max_batch_indices);
  glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &GCaps.max_batch_vertices);
  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &GCaps.max_vertex_attribs);
  glGetIntegerv(GL_MAX_VARYING_FLOATS, &GCaps.max_varying_floats);
  glGetIntegerv(GL_MAX_IMAGE_UNITS, &GCaps.max_images);

  glGetIntegerv(GL_NUM_EXTENSIONS, &GCaps.extensions_len);
  GCaps.extension_get = gl_extension_get;

  GCaps.max_samplers = GCaps.max_textures;
  GCaps.mem_stats_support = epoxy_has_gl_extension("GL_NVX_gpu_memory_info") ||
                            epoxy_has_gl_extension("GL_ATI_meminfo");
  GCaps.geometry_shader_support = true;
  GCaps.max_samplers = GCaps.max_textures;
  GCaps.hdr_viewport_support = false;

  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &GCaps.max_work_group_count[0]);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &GCaps.max_work_group_count[1]);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &GCaps.max_work_group_count[2]);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &GCaps.max_work_group_size[0]);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &GCaps.max_work_group_size[1]);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &GCaps.max_work_group_size[2]);
  glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &GCaps.max_shader_storage_buffer_bindings);
  glGetIntegerv(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS, &GCaps.max_compute_shader_storage_blocks);
  int64_t max_ssbo_size, max_ubo_size;
  glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &max_ubo_size);
  GCaps.max_uniform_buffer_size = size_t(max_ubo_size);
  glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &max_ssbo_size);
  GCaps.max_storage_buffer_size = size_t(max_ssbo_size);
  GLint ssbo_alignment;
  glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &ssbo_alignment);
  GCaps.storage_buffer_alignment = size_t(ssbo_alignment);

  GCaps.stencil_export_support = epoxy_has_gl_extension("GL_ARB_shader_stencil_export");

  /* GL specific capabilities. */
  glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &GCaps.max_texture_3d_size);
  glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE,
                reinterpret_cast<int *>(&GCaps.max_buffer_texture_size));
  glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &GLContext::max_cubemap_size);
  glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, &GLContext::max_ubo_binds);
  GLint max_ssbo_binds;
  GLContext::max_ssbo_binds = 999999;
  glGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, &max_ssbo_binds);
  GLContext::max_ssbo_binds = min_ii(GLContext::max_ssbo_binds, max_ssbo_binds);
  glGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &max_ssbo_binds);
  GLContext::max_ssbo_binds = min_ii(GLContext::max_ssbo_binds, max_ssbo_binds);
  glGetIntegerv(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS, &max_ssbo_binds);
  GLContext::max_ssbo_binds = min_ii(GLContext::max_ssbo_binds, max_ssbo_binds);
  GLContext::debug_layer_support = epoxy_gl_version() >= 43 ||
                                   epoxy_has_gl_extension("GL_KHR_debug") ||
                                   epoxy_has_gl_extension("GL_ARB_debug_output");
  GLContext::direct_state_access_support = epoxy_has_gl_extension("GL_ARB_direct_state_access");
  GLContext::explicit_location_support = epoxy_gl_version() >= 43;
  GLContext::framebuffer_fetch_support = epoxy_has_gl_extension("GL_EXT_shader_framebuffer_fetch");
  GLContext::texture_barrier_support = epoxy_has_gl_extension("GL_ARB_texture_barrier");
  GLContext::layered_rendering_support = epoxy_has_gl_extension(
      "GL_ARB_shader_viewport_layer_array");
  GLContext::native_barycentric_support = epoxy_has_gl_extension(
      "GL_AMD_shader_explicit_vertex_parameter");
  GLContext::multi_bind_support = GLContext::multi_bind_image_support = epoxy_has_gl_extension(
      "GL_ARB_multi_bind");
  GLContext::stencil_texturing_support = epoxy_gl_version() >= 43;
  GLContext::texture_filter_anisotropic_support = epoxy_has_gl_extension(
      "GL_EXT_texture_filter_anisotropic");

  /* Disabled until it is proven to work. */
  GLContext::framebuffer_fetch_support = false;

  detect_workarounds();

#if BLI_SUBPROCESS_SUPPORT
  GCaps.use_subprocess_shader_compilations = U.shader_compilation_method ==
                                             USER_SHADER_COMPILE_SUBPROCESS;
#else
  GCaps.use_subprocess_shader_compilations = false;
#endif
  if (G.debug & G_DEBUG_GPU_RENDERDOC) {
    /* Avoid crashes on RenderDoc sessions. */
    GCaps.use_subprocess_shader_compilations = false;
  }

  int thread_count = U.gpu_shader_workers;

  if (thread_count == 0) {
    /* Good default based on measurements. */

    /* Always have at least 1 worker. */
    thread_count = 1;

    if (GCaps.use_subprocess_shader_compilations) {
      /* Use reasonable number of worker by default when there are known gains. */
      if (GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_OFFICIAL) ||
          GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_OFFICIAL) ||
          GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_WIN, GPU_DRIVER_ANY))
      {
        /* Subprocess is too costly in memory (>150MB per worker) to have better defaults. */
        thread_count = std::max(1, std::min(4, BLI_system_thread_count() / 2));
      }
    }
    else if (GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_OFFICIAL)) {
      /* Best middle ground between memory usage and speedup as Nvidia context memory footprint
       * is quite heavy (~25MB). Moreover we have diminishing return after this because of PSO
       * compilation blocking the main thread.
       * Can be revisited if we find a way to delete the worker thread context after finishing
       * compilation, and fix the scheduling bubbles (#139775). */
      thread_count = 4;
    }
    else if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_OPENSOURCE) ||
             GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_UNIX, GPU_DRIVER_ANY))
    {
      /* Mesa has very good compilation time and doesn't block the main thread.
       * The memory footprint of the worker context is rather small (<10MB).
       * Shader compilation gets much slower as the number of threads increases. */
      thread_count = 8;
    }
    else if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_OFFICIAL)) {
      /* AMD proprietary driver's context have huge memory footprint (~45MB).
       * There is also not much gain from parallelization. */
      thread_count = 1;
    }
    else if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_WIN, GPU_DRIVER_ANY)) {
      /* Intel windows driver offer almost no speedup with parallel compilation. */
      thread_count = 1;
    }
  }

  /* Allow thread count override option to limit the number of workers and avoid allocating more
   * workers than needed. Also ensures that there is always 1 thread available for the UI. */
  int max_thread_count = std::max(1, BLI_system_thread_count() - 1);

  GCaps.max_parallel_compilations = std::min(thread_count, max_thread_count);

  /* Disable this feature entirely when not debugging. */
  if ((G.debug & G_DEBUG_GPU) == 0) {
    GLContext::debug_layer_support = false;
    GLContext::debug_layer_workaround = false;
  }
}

/** \} */

}  // namespace blender::gpu
