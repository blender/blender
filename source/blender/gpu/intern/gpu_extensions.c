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

#include "BLI_math_base.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "MEM_guardedalloc.h"

#include "GPU_extensions.h"
#include "GPU_framebuffer.h"
#include "GPU_glew.h"
#include "GPU_platform.h"
#include "GPU_texture.h"

#include "intern/gpu_private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

/* Extensions support */

/* -- extension: version of GL that absorbs it
 * EXT_gpu_shader4: 3.0
 * ARB_framebuffer object: 3.0
 * EXT_framebuffer_multisample_blit_scaled: ???
 * ARB_draw_instanced: 3.1
 * ARB_texture_multisample: 3.2
 * ARB_texture_query_lod: 4.0
 */

static struct GPUGlobal {
  GLint maxtexsize;
  GLint maxtexlayers;
  GLint maxcubemapsize;
  GLint maxtextures;
  GLint maxtexturesfrag;
  GLint maxtexturesgeom;
  GLint maxtexturesvert;
  GLint maxubosize;
  GLint maxubobinds;
  int samples_color_texture_max;
  float line_width_range[2];
  /* workaround for different calculation of dfdy factors on GPUs. Some GPUs/drivers
   * calculate dfdy in shader differently when drawing to an offscreen buffer. First
   * number is factor on screen and second is off-screen */
  float dfdyfactors[2];
  float max_anisotropy;
  /* Some Intel drivers have limited support for `GLEW_ARB_base_instance` so in
   * these cases it is best to indicate that it is not supported. See T67951 */
  bool glew_arb_base_instance_is_supported;
  /* Cubemap Array support. */
  bool glew_arb_texture_cube_map_array_is_supported;
  /* Some Intel drivers have issues with using mips as framebuffer targets if
   * GL_TEXTURE_MAX_LEVEL is higher than the target mip.
   * We need a workaround in this cases. */
  bool mip_render_workaround;
  /* There is an issue with the glBlitFramebuffer on MacOS with radeon pro graphics.
   * Blitting depth with GL_DEPTH24_STENCIL8 is buggy so the workaround is to use
   * GPU_DEPTH32F_STENCIL8. Then Blitting depth will work but blitting stencil will
   * still be broken. */
  bool depth_blitting_workaround;
  /* Crappy driver don't know how to map framebuffer slot to output vars...
   * We need to have no "holes" in the output buffer slots. */
  bool unused_fb_slot_workaround;
  bool broken_amd_driver;
  /* Some crappy Intel drivers don't work well with shaders created in different
   * rendering contexts. */
  bool context_local_shaders_workaround;
} GG = {1, 0};

static void gpu_detect_mip_render_workaround(void)
{
  int cube_size = 2;
  float *source_pix = MEM_callocN(sizeof(float) * 4 * 6 * cube_size * cube_size, __func__);
  float clear_color[4] = {1.0f, 0.5f, 0.0f, 0.0f};

  GPUTexture *tex = GPU_texture_create_cube(cube_size, GPU_RGBA16F, source_pix, NULL);
  MEM_freeN(source_pix);

  GPU_texture_bind(tex, 0);
  GPU_texture_generate_mipmap(tex);
  glTexParameteri(GPU_texture_target(tex), GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GPU_texture_target(tex), GL_TEXTURE_MAX_LEVEL, 0);
  GPU_texture_unbind(tex);

  GPUFrameBuffer *fb = GPU_framebuffer_create();
  GPU_framebuffer_texture_attach(fb, tex, 0, 1);
  GPU_framebuffer_bind(fb);
  GPU_framebuffer_clear_color(fb, clear_color);
  GPU_framebuffer_restore();
  GPU_framebuffer_free(fb);

  float *data = GPU_texture_read(tex, GPU_DATA_FLOAT, 1);
  GG.mip_render_workaround = !equals_v4v4(clear_color, data);

  MEM_freeN(data);
  GPU_texture_free(tex);
}

/* GPU Extensions */

int GPU_max_texture_size(void)
{
  return GG.maxtexsize;
}

int GPU_max_texture_layers(void)
{
  return GG.maxtexlayers;
}

int GPU_max_textures(void)
{
  return GG.maxtextures;
}

int GPU_max_textures_frag(void)
{
  return GG.maxtexturesfrag;
}

int GPU_max_textures_geom(void)
{
  return GG.maxtexturesgeom;
}

int GPU_max_textures_vert(void)
{
  return GG.maxtexturesvert;
}

float GPU_max_texture_anisotropy(void)
{
  return GG.max_anisotropy;
}

int GPU_max_color_texture_samples(void)
{
  return GG.samples_color_texture_max;
}

int GPU_max_cube_map_size(void)
{
  return GG.maxcubemapsize;
}

int GPU_max_ubo_binds(void)
{
  return GG.maxubobinds;
}

int GPU_max_ubo_size(void)
{
  return GG.maxubosize;
}

float GPU_max_line_width(void)
{
  return GG.line_width_range[1];
}

void GPU_get_dfdy_factors(float fac[2])
{
  copy_v2_v2(fac, GG.dfdyfactors);
}

bool GPU_arb_base_instance_is_supported(void)
{
  return GG.glew_arb_base_instance_is_supported;
}

bool GPU_arb_texture_cube_map_array_is_supported(void)
{
  return GG.glew_arb_texture_cube_map_array_is_supported;
}

bool GPU_mip_render_workaround(void)
{
  return GG.mip_render_workaround;
}

bool GPU_depth_blitting_workaround(void)
{
  return GG.depth_blitting_workaround;
}

bool GPU_unused_fb_slot_workaround(void)
{
  return GG.unused_fb_slot_workaround;
}

bool GPU_context_local_shaders_workaround(void)
{
  return GG.context_local_shaders_workaround;
}

bool GPU_crappy_amd_driver(void)
{
  /* Currently are the same drivers with the `unused_fb_slot` problem. */
  return GG.broken_amd_driver;
}

void gpu_extensions_init(void)
{
  /* during 2.8 development each platform has its own OpenGL minimum requirements
   * final 2.8 release will be unified on OpenGL 3.3 core profile, no required extensions
   * see developer.blender.org/T49012 for details
   */
  BLI_assert(GLEW_VERSION_3_3);

  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &GG.maxtexturesfrag);
  glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &GG.maxtexturesvert);
  glGetIntegerv(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS, &GG.maxtexturesgeom);
  glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &GG.maxtextures);

  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &GG.maxtexsize);
  glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &GG.maxtexlayers);
  glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &GG.maxcubemapsize);

  if (GLEW_EXT_texture_filter_anisotropic) {
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &GG.max_anisotropy);
  }
  else {
    GG.max_anisotropy = 1.0f;
  }

  glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, &GG.maxubobinds);
  glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &GG.maxubosize);

  glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, GG.line_width_range);

  glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &GG.samples_color_texture_max);

  const char *vendor = (const char *)glGetString(GL_VENDOR);
  const char *renderer = (const char *)glGetString(GL_RENDERER);
  const char *version = (const char *)glGetString(GL_VERSION);

  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_WIN, GPU_DRIVER_OFFICIAL)) {
    if (strstr(version, "4.5.13399") || strstr(version, "4.5.13417") ||
        strstr(version, "4.5.13422")) {
      /* The renderers include:
       *   Mobility Radeon HD 5000;
       *   Radeon HD 7500M;
       *   Radeon HD 7570M;
       *   Radeon HD 7600M;
       * And many others... */

      GG.unused_fb_slot_workaround = true;
      GG.broken_amd_driver = true;
    }
  }

  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_MAC, GPU_DRIVER_OFFICIAL)) {
    if (strstr(renderer, "AMD Radeon Pro") || strstr(renderer, "AMD Radeon R9") ||
        strstr(renderer, "AMD Radeon RX")) {
      GG.depth_blitting_workaround = true;
    }
  }

  GG.glew_arb_base_instance_is_supported = GLEW_ARB_base_instance;
  GG.glew_arb_texture_cube_map_array_is_supported = GLEW_ARB_texture_cube_map_array;
  gpu_detect_mip_render_workaround();

  if (G.debug & G_DEBUG_GPU_FORCE_WORKAROUNDS) {
    printf("\n");
    printf("GPU: Bypassing workaround detection.\n");
    printf("GPU: OpenGL identification strings\n");
    printf("GPU: vendor: %s\n", vendor);
    printf("GPU: renderer: %s\n", renderer);
    printf("GPU: version: %s\n\n", version);
    GG.mip_render_workaround = true;
    GG.depth_blitting_workaround = true;
    GG.unused_fb_slot_workaround = true;
    GG.context_local_shaders_workaround = GLEW_ARB_get_program_binary;
  }

  /* Special fix for theses specific GPUs. Without thoses workaround, blender crashes on strartup.
   * (see T72098) */
  if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_WIN, GPU_DRIVER_OFFICIAL) &&
      (strstr(renderer, "HD Graphics 620") || strstr(renderer, "HD Graphics 630"))) {
    GG.mip_render_workaround = true;
    GG.context_local_shaders_workaround = GLEW_ARB_get_program_binary;
  }

  /* df/dy calculation factors, those are dependent on driver */
  GG.dfdyfactors[0] = 1.0;
  GG.dfdyfactors[1] = 1.0;

  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY) &&
      strstr(version, "3.3.10750")) {
    GG.dfdyfactors[0] = 1.0;
    GG.dfdyfactors[1] = -1.0;
  }
  else if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_WIN, GPU_DRIVER_ANY)) {
    if (strstr(version, "4.0.0 - Build 10.18.10.3308") ||
        strstr(version, "4.0.0 - Build 9.18.10.3186") ||
        strstr(version, "4.0.0 - Build 9.18.10.3165") ||
        strstr(version, "3.1.0 - Build 9.17.10.3347") ||
        strstr(version, "3.1.0 - Build 9.17.10.4101") ||
        strstr(version, "3.3.0 - Build 8.15.10.2618")) {
      GG.dfdyfactors[0] = -1.0;
      GG.dfdyfactors[1] = 1.0;
    }

    if (strstr(version, "Build 10.18.10.3") || strstr(version, "Build 10.18.10.4") ||
        strstr(version, "Build 10.18.10.5") || strstr(version, "Build 10.18.14.4") ||
        strstr(version, "Build 10.18.14.5")) {
      /* Maybe not all of these drivers have problems with `GLEW_ARB_base_instance`.
       * But it's hard to test each case. */
      GG.glew_arb_base_instance_is_supported = false;
      GG.context_local_shaders_workaround = true;
    }

    if (strstr(version, "Build 20.19.15.4285")) {
      /* Somehow fixes armature display issues (see T69743). */
      GG.context_local_shaders_workaround = true;
    }
  }
  else if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_UNIX, GPU_DRIVER_OPENSOURCE) &&
           (strstr(version, "Mesa 18.") || strstr(version, "Mesa 19.0") ||
            strstr(version, "Mesa 19.1") || strstr(version, "Mesa 19.2"))) {
    /* See T70187: merging vertices fail. This has been tested from 18.2.2 till 19.3.0~dev of the
     * Mesa driver */
    GG.unused_fb_slot_workaround = true;
  }

  GPU_invalid_tex_init();
  GPU_samplers_init();
}

void gpu_extensions_exit(void)
{
  GPU_invalid_tex_free();
  GPU_samplers_free();
}

bool GPU_mem_stats_supported(void)
{
#ifndef GPU_STANDALONE
  return (GLEW_NVX_gpu_memory_info || GLEW_ATI_meminfo) && (G.debug & G_DEBUG_GPU_MEM);
#else
  return false;
#endif
}

void GPU_mem_stats_get(int *totalmem, int *freemem)
{
  /* TODO(merwin): use Apple's platform API to get this info */

  if (GLEW_NVX_gpu_memory_info) {
    /* returned value in Kb */
    glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, totalmem);

    glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, freemem);
  }
  else if (GLEW_ATI_meminfo) {
    int stats[4];

    glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, stats);
    *freemem = stats[0];
    *totalmem = 0;
  }
  else {
    *totalmem = 0;
    *freemem = 0;
  }
}
