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
 * Copyright 2020, Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.h"

#include "gpu_capabilities_private.hh"
#include "gpu_platform_private.hh"

#include "glew-mx.h"

#include "gl_debug.hh"

#include "gl_backend.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Platform
 * \{ */

void GLBackend::platform_init()
{
  BLI_assert(!GPG.initialized);
  GPG.initialized = true;

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
  GPG.create_key(GPG.support_level, vendor, renderer, version);
  GPG.create_gpu_name(vendor, renderer, version);
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

static bool detect_mip_render_workaround()
{
  int cube_size = 2;
  float clear_color[4] = {1.0f, 0.5f, 0.0f, 0.0f};
  float *source_pix = (float *)MEM_callocN(sizeof(float[4]) * cube_size * cube_size * 6, __func__);

  /* NOTE: Debug layers are not yet enabled. Force use of glGetError. */
  debug::check_gl_error("Cubemap Workaround Start");
  /* Not using GPU API since it is not yet fully initialized. */
  GLuint tex, fb;
  /* Create cubemap with 2 mip level. */
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
  for (int mip = 0; mip < 2; mip++) {
    for (int i = 0; i < 6; i++) {
      const int width = cube_size / (1 << mip);
      GLenum target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + i;
      glTexImage2D(target, mip, GL_RGBA16F, width, width, 0, GL_RGBA, GL_FLOAT, source_pix);
    }
  }
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 0);
  /* Attach and clear mip 1. */
  glGenFramebuffers(1, &fb);
  glBindFramebuffer(GL_FRAMEBUFFER, fb);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 1);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glClearColor(UNPACK4(clear_color));
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glClear(GL_COLOR_BUFFER_BIT);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  /* Read mip 1. If color is not the same as the clear_color, the rendering failed. */
  glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 1, GL_RGBA, GL_FLOAT, source_pix);
  bool enable_workaround = !equals_v4v4(clear_color, source_pix);
  MEM_freeN(source_pix);

  glDeleteFramebuffers(1, &fb);
  glDeleteTextures(1, &tex);

  debug::check_gl_error("Cubemap Workaround End9");

  return enable_workaround;
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
    GCaps.mip_render_workaround = true;
    GLContext::debug_layer_workaround = true;
    GLContext::unused_fb_slot_workaround = true;
    /* Turn off extensions. */
    GCaps.shader_image_load_store_support = false;
    GLContext::base_instance_support = false;
    GLContext::clear_texture_support = false;
    GLContext::copy_image_support = false;
    GLContext::debug_layer_support = false;
    GLContext::direct_state_access_support = false;
    GLContext::fixed_restart_index_support = false;
    GLContext::multi_bind_support = false;
    GLContext::multi_draw_indirect_support = false;
    GLContext::shader_draw_parameters_support = false;
    GLContext::texture_cube_map_array_support = false;
    GLContext::texture_filter_anisotropic_support = false;
    GLContext::texture_gather_support = false;
    GLContext::vertex_attrib_binding_support = false;
    return;
  }

  /* Some Intel drivers have issues with using mips as framebuffer targets if
   * GL_TEXTURE_MAX_LEVEL is higher than the target mip.
   * Only check at the end after all other workarounds because this uses the drawing code. */
  GCaps.mip_render_workaround = detect_mip_render_workaround();
  /* Limit support for GLEW_ARB_base_instance to OpenGL 4.0 and higher. NVIDIA Quadro FX 4800
   * (TeraScale) report that they support GLEW_ARB_base_instance, but the driver does not support
   * GLEW_ARB_draw_indirect as it has an OpenGL3 context what also matches the minimum needed
   * requirements.
   *
   * We use it as a target for glMapBuffer(Range) what is part of the OpenGL 4 API. So better
   * disable it when we don't have an OpenGL4 context (See T77657) */
  if (!GLEW_VERSION_4_0) {
    GLContext::base_instance_support = false;
  }
  /* The renderers include:
   *   Mobility Radeon HD 5000;
   *   Radeon HD 7500M;
   *   Radeon HD 7570M;
   *   Radeon HD 7600M;
   * And many others... */
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_WIN, GPU_DRIVER_OFFICIAL) &&
      (strstr(version, "4.5.13399") || strstr(version, "4.5.13417") ||
       strstr(version, "4.5.13422"))) {
    GLContext::unused_fb_slot_workaround = true;
    GCaps.shader_image_load_store_support = false;
    GCaps.broken_amd_driver = true;
  }
  /* We have issues with this specific renderer. (see T74024) */
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_UNIX, GPU_DRIVER_OPENSOURCE) &&
      strstr(renderer, "AMD VERDE")) {
    GLContext::unused_fb_slot_workaround = true;
    GCaps.shader_image_load_store_support = false;
    GCaps.broken_amd_driver = true;
  }
  /* Fix slowdown on this particular driver. (see T77641) */
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_UNIX, GPU_DRIVER_OPENSOURCE) &&
      strstr(version, "Mesa 19.3.4")) {
    GCaps.shader_image_load_store_support = false;
    GCaps.broken_amd_driver = true;
  }
  /* There is an issue with the #glBlitFramebuffer on MacOS with radeon pro graphics.
   * Blitting depth with#GL_DEPTH24_STENCIL8 is buggy so the workaround is to use
   * #GPU_DEPTH32F_STENCIL8. Then Blitting depth will work but blitting stencil will
   * still be broken. */
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_MAC, GPU_DRIVER_OFFICIAL)) {
    if (strstr(renderer, "AMD Radeon Pro") || strstr(renderer, "AMD Radeon R9") ||
        strstr(renderer, "AMD Radeon RX")) {
      GCaps.depth_blitting_workaround = true;
    }
  }
  /* Limit this fix to older hardware with GL < 4.5. This means Broadwell GPUs are
   * covered since they only support GL 4.4 on windows.
   * This fixes some issues with workbench anti-aliasing on Win + Intel GPU. (see T76273) */
  if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_WIN, GPU_DRIVER_OFFICIAL) && !GLEW_VERSION_4_5) {
    GLContext::copy_image_support = false;
  }
  /* Special fix for theses specific GPUs.
   * Without this workaround, blender crashes on startup. (see T72098) */
  if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_WIN, GPU_DRIVER_OFFICIAL) &&
      (strstr(renderer, "HD Graphics 620") || strstr(renderer, "HD Graphics 630"))) {
    GCaps.mip_render_workaround = true;
  }
  /* Intel Ivy Bridge GPU's seems to have buggy cube-map array support. (see T75943) */
  if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_WIN, GPU_DRIVER_OFFICIAL) &&
      (strstr(renderer, "HD Graphics 4000") || strstr(renderer, "HD Graphics 4400") ||
       strstr(renderer, "HD Graphics 2500"))) {
    GLContext::texture_cube_map_array_support = false;
  }
  /* Maybe not all of these drivers have problems with `GLEW_ARB_base_instance`.
   * But it's hard to test each case.
   * We get crashes from some crappy Intel drivers don't work well with shaders created in
   * different rendering contexts. */
  if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_WIN, GPU_DRIVER_ANY) &&
      (strstr(version, "Build 10.18.10.3") || strstr(version, "Build 10.18.10.4") ||
       strstr(version, "Build 10.18.10.5") || strstr(version, "Build 10.18.14.4") ||
       strstr(version, "Build 10.18.14.5"))) {
    GLContext::base_instance_support = false;
    GCaps.use_main_context_workaround = true;
  }
  /* Somehow fixes armature display issues (see T69743). */
  if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_WIN, GPU_DRIVER_ANY) &&
      (strstr(version, "Build 20.19.15.4285"))) {
    GCaps.use_main_context_workaround = true;
  }
  /* See T70187: merging vertices fail. This has been tested from 18.2.2 till 19.3.0~dev of the
   * Mesa driver */
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_UNIX, GPU_DRIVER_OPENSOURCE) &&
      (strstr(version, "Mesa 18.") || strstr(version, "Mesa 19.0") ||
       strstr(version, "Mesa 19.1") || strstr(version, "Mesa 19.2"))) {
    GLContext::unused_fb_slot_workaround = true;
  }
  /* There is a bug on older Nvidia GPU where GL_ARB_texture_gather
   * is reported to be supported but yield a compile error (see T55802). */
  if (GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_ANY) && !GLEW_VERSION_4_0) {
    GLContext::texture_gather_support = false;
  }

  /* dFdx/dFdy calculation factors, those are dependent on driver. */
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY) &&
      strstr(version, "3.3.10750")) {
    GLContext::derivative_signs[0] = 1.0;
    GLContext::derivative_signs[1] = -1.0;
  }
  else if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_WIN, GPU_DRIVER_ANY)) {
    if (strstr(version, "4.0.0 - Build 10.18.10.3308") ||
        strstr(version, "4.0.0 - Build 9.18.10.3186") ||
        strstr(version, "4.0.0 - Build 9.18.10.3165") ||
        strstr(version, "3.1.0 - Build 9.17.10.3347") ||
        strstr(version, "3.1.0 - Build 9.17.10.4101") ||
        strstr(version, "3.3.0 - Build 8.15.10.2618")) {
      GLContext::derivative_signs[0] = -1.0;
      GLContext::derivative_signs[1] = 1.0;
    }
  }

  /* Disable multidraw if the base instance cannot be read. */
  if (GLContext::shader_draw_parameters_support == false) {
    GLContext::multi_draw_indirect_support = false;
  }
  /* Enable our own incomplete debug layer if no other is available. */
  if (GLContext::debug_layer_support == false) {
    GLContext::debug_layer_workaround = true;
  }
}

/** Internal capabilities. */
GLint GLContext::max_cubemap_size = 0;
GLint GLContext::max_texture_3d_size = 0;
GLint GLContext::max_ubo_binds = 0;
GLint GLContext::max_ubo_size = 0;
/** Extensions. */
bool GLContext::base_instance_support = false;
bool GLContext::clear_texture_support = false;
bool GLContext::copy_image_support = false;
bool GLContext::debug_layer_support = false;
bool GLContext::direct_state_access_support = false;
bool GLContext::fixed_restart_index_support = false;
bool GLContext::multi_bind_support = false;
bool GLContext::multi_draw_indirect_support = false;
bool GLContext::shader_draw_parameters_support = false;
bool GLContext::texture_cube_map_array_support = false;
bool GLContext::texture_filter_anisotropic_support = false;
bool GLContext::texture_gather_support = false;
bool GLContext::vertex_attrib_binding_support = false;
/** Workarounds. */
bool GLContext::debug_layer_workaround = false;
bool GLContext::unused_fb_slot_workaround = false;
float GLContext::derivative_signs[2] = {1.0f, 1.0f};

void GLBackend::capabilities_init()
{
  BLI_assert(GLEW_VERSION_3_3);
  /* Common Capabilities. */
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &GCaps.max_texture_size);
  glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &GCaps.max_texture_layers);
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &GCaps.max_textures_frag);
  glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &GCaps.max_textures_vert);
  glGetIntegerv(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS, &GCaps.max_textures_geom);
  glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &GCaps.max_textures);
  GCaps.mem_stats_support = GLEW_NVX_gpu_memory_info || GLEW_ATI_meminfo;
  GCaps.shader_image_load_store_support = GLEW_ARB_shader_image_load_store;
  /* GL specific capabilities. */
  glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &GLContext::max_texture_3d_size);
  glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &GLContext::max_cubemap_size);
  glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, &GLContext::max_ubo_binds);
  glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &GLContext::max_ubo_size);
  GLContext::base_instance_support = GLEW_ARB_base_instance;
  GLContext::clear_texture_support = GLEW_ARB_clear_texture;
  GLContext::copy_image_support = GLEW_ARB_copy_image;
  GLContext::debug_layer_support = GLEW_VERSION_4_3 || GLEW_KHR_debug || GLEW_ARB_debug_output;
  GLContext::direct_state_access_support = GLEW_ARB_direct_state_access;
  GLContext::fixed_restart_index_support = GLEW_ARB_ES3_compatibility;
  GLContext::multi_bind_support = GLEW_ARB_multi_bind;
  GLContext::multi_draw_indirect_support = GLEW_ARB_multi_draw_indirect;
  GLContext::shader_draw_parameters_support = GLEW_ARB_shader_draw_parameters;
  GLContext::texture_cube_map_array_support = GLEW_ARB_texture_cube_map_array;
  GLContext::texture_filter_anisotropic_support = GLEW_EXT_texture_filter_anisotropic;
  GLContext::texture_gather_support = GLEW_ARB_texture_gather;
  GLContext::vertex_attrib_binding_support = GLEW_ARB_vertex_attrib_binding;

  detect_workarounds();

  /* Disable this feature entirely when not debugging. */
  if ((G.debug & G_DEBUG_GPU) == 0) {
    GLContext::debug_layer_support = false;
    GLContext::debug_layer_workaround = false;
  }
}

/** \} */

}  // namespace blender::gpu
