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
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "BLI_string_utils.h"

#include "MEM_guardedalloc.h"

#include "GPU_shader.h"

#include "eevee_private.h"

static const char *filter_defines = "#define HAMMERSLEY_SIZE " STRINGIFY(HAMMERSLEY_SIZE) "\n"
#if defined(IRRADIANCE_SH_L2)
                               "#define IRRADIANCE_SH_L2\n"
#elif defined(IRRADIANCE_CUBEMAP)
                               "#define IRRADIANCE_CUBEMAP\n"
#elif defined(IRRADIANCE_HL2)
                               "#define IRRADIANCE_HL2\n"
#endif
                               "#define NOISE_SIZE 64\n";

static struct {
  /* Probes */
  struct GPUShader *probe_default_sh;
  struct GPUShader *probe_default_studiolight_sh;
  struct GPUShader *probe_grid_display_sh;
  struct GPUShader *probe_cube_display_sh;
  struct GPUShader *probe_planar_display_sh;
  struct GPUShader *probe_filter_glossy_sh;
  struct GPUShader *probe_filter_diffuse_sh;
  struct GPUShader *probe_filter_visibility_sh;
  struct GPUShader *probe_grid_fill_sh;
  struct GPUShader *probe_planar_downsample_sh;

  /* Velocity Resolve */
  struct GPUShader *velocity_resolve_sh;

  /* Temporal Anti Aliasing */
  struct GPUShader *taa_resolve_sh;
  struct GPUShader *taa_resolve_reproject_sh;

} e_data = {NULL}; /* Engine data */

extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_bsdf_sampling_lib_glsl[];
extern char datatoc_common_uniforms_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];

extern char datatoc_background_vert_glsl[];
extern char datatoc_default_world_frag_glsl[];
extern char datatoc_lightprobe_geom_glsl[];
extern char datatoc_lightprobe_vert_glsl[];
extern char datatoc_lightprobe_cube_display_frag_glsl[];
extern char datatoc_lightprobe_cube_display_vert_glsl[];
extern char datatoc_lightprobe_filter_diffuse_frag_glsl[];
extern char datatoc_lightprobe_filter_glossy_frag_glsl[];
extern char datatoc_lightprobe_filter_visibility_frag_glsl[];
extern char datatoc_lightprobe_grid_display_frag_glsl[];
extern char datatoc_lightprobe_grid_display_vert_glsl[];
extern char datatoc_lightprobe_grid_fill_frag_glsl[];
extern char datatoc_lightprobe_planar_display_frag_glsl[];
extern char datatoc_lightprobe_planar_display_vert_glsl[];
extern char datatoc_lightprobe_planar_downsample_frag_glsl[];
extern char datatoc_lightprobe_planar_downsample_geom_glsl[];
extern char datatoc_lightprobe_planar_downsample_vert_glsl[];
extern char datatoc_irradiance_lib_glsl[];
extern char datatoc_lightprobe_lib_glsl[];
extern char datatoc_octahedron_lib_glsl[];

/* Velocity Resolve */
extern char datatoc_effect_velocity_resolve_frag_glsl[];

/* Temporal Sampling */
extern char datatoc_effect_temporal_aa_glsl[];

/* *********** FUNCTIONS *********** */

void EEVEE_shaders_lightprobe_shaders_init(void)
{
  BLI_assert(e_data.probe_filter_glossy_sh == NULL);
  char *shader_str = NULL;

  shader_str = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                datatoc_common_uniforms_lib_glsl,
                                datatoc_bsdf_common_lib_glsl,
                                datatoc_bsdf_sampling_lib_glsl,
                                datatoc_lightprobe_filter_glossy_frag_glsl);

  e_data.probe_filter_glossy_sh = DRW_shader_create(
      datatoc_lightprobe_vert_glsl, datatoc_lightprobe_geom_glsl, shader_str, filter_defines);

  e_data.probe_default_sh = DRW_shader_create_with_lib(datatoc_background_vert_glsl,
                                                       NULL,
                                                       datatoc_default_world_frag_glsl,
                                                       datatoc_common_view_lib_glsl,
                                                       NULL);

  MEM_freeN(shader_str);

  shader_str = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                datatoc_common_uniforms_lib_glsl,
                                datatoc_bsdf_common_lib_glsl,
                                datatoc_bsdf_sampling_lib_glsl,
                                datatoc_lightprobe_filter_diffuse_frag_glsl);

  e_data.probe_filter_diffuse_sh = DRW_shader_create_fullscreen(shader_str, filter_defines);

  MEM_freeN(shader_str);

  shader_str = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                datatoc_common_uniforms_lib_glsl,
                                datatoc_bsdf_common_lib_glsl,
                                datatoc_bsdf_sampling_lib_glsl,
                                datatoc_lightprobe_filter_visibility_frag_glsl);

  e_data.probe_filter_visibility_sh = DRW_shader_create_fullscreen(shader_str, filter_defines);

  MEM_freeN(shader_str);

  e_data.probe_grid_fill_sh = DRW_shader_create_fullscreen(datatoc_lightprobe_grid_fill_frag_glsl,
                                                           filter_defines);

  e_data.probe_planar_downsample_sh = DRW_shader_create(
      datatoc_lightprobe_planar_downsample_vert_glsl,
      datatoc_lightprobe_planar_downsample_geom_glsl,
      datatoc_lightprobe_planar_downsample_frag_glsl,
      NULL);
}

GPUShader *EEVEE_shaders_probe_filter_glossy_sh_get(void)
{
  return e_data.probe_filter_glossy_sh;
}

GPUShader *EEVEE_shaders_probe_default_sh_get(void)
{
  return e_data.probe_default_sh;
}

GPUShader *EEVEE_shaders_probe_filter_diffuse_sh_get(void)
{
  return e_data.probe_filter_diffuse_sh;
}

GPUShader *EEVEE_shaders_probe_filter_visibility_sh_get(void)
{
  return e_data.probe_filter_visibility_sh;
}

GPUShader *EEVEE_shaders_probe_grid_fill_sh_get(void)
{
  return e_data.probe_grid_fill_sh;
}

GPUShader *EEVEE_shaders_probe_planar_downsample_sh_get(void)
{
  return e_data.probe_planar_downsample_sh;
}

GPUShader *EEVEE_shaders_default_studiolight_sh_get(void)
{
  if (e_data.probe_default_studiolight_sh == NULL) {
    e_data.probe_default_studiolight_sh = DRW_shader_create_with_lib(
        datatoc_background_vert_glsl,
        NULL,
        datatoc_default_world_frag_glsl,
        datatoc_common_view_lib_glsl,
        "#define LOOKDEV\n");
  }
  return e_data.probe_default_studiolight_sh;
}

GPUShader *EEVEE_shaders_probe_cube_display_sh_get(void)
{
  if (e_data.probe_cube_display_sh == NULL) {
    char *shader_str = BLI_string_joinN(datatoc_octahedron_lib_glsl,
                                        datatoc_common_view_lib_glsl,
                                        datatoc_common_uniforms_lib_glsl,
                                        datatoc_bsdf_common_lib_glsl,
                                        datatoc_lightprobe_lib_glsl,
                                        datatoc_lightprobe_cube_display_frag_glsl);

    char *vert_str = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                      datatoc_lightprobe_cube_display_vert_glsl);

    e_data.probe_cube_display_sh = DRW_shader_create(vert_str, NULL, shader_str, SHADER_DEFINES);

    MEM_freeN(vert_str);
    MEM_freeN(shader_str);
  }
  return e_data.probe_cube_display_sh;
}

GPUShader *EEVEE_shaders_probe_grid_display_sh_get(void)
{
  if (e_data.probe_grid_display_sh == NULL) {
    char *shader_str = BLI_string_joinN(datatoc_octahedron_lib_glsl,
                                        datatoc_common_view_lib_glsl,
                                        datatoc_common_uniforms_lib_glsl,
                                        datatoc_bsdf_common_lib_glsl,
                                        datatoc_irradiance_lib_glsl,
                                        datatoc_lightprobe_lib_glsl,
                                        datatoc_lightprobe_grid_display_frag_glsl);

    char *vert_str = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                      datatoc_lightprobe_grid_display_vert_glsl);

    e_data.probe_grid_display_sh = DRW_shader_create(vert_str, NULL, shader_str, filter_defines);

    MEM_freeN(vert_str);
    MEM_freeN(shader_str);
  }
  return e_data.probe_grid_display_sh;
}

GPUShader *EEVEE_shaders_probe_planar_display_sh_get(void)
{
  if (e_data.probe_planar_display_sh == NULL) {
    char *vert_str = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                      datatoc_lightprobe_planar_display_vert_glsl);

    char *shader_str = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                        datatoc_lightprobe_planar_display_frag_glsl);

    e_data.probe_planar_display_sh = DRW_shader_create(vert_str, NULL, shader_str, NULL);

    MEM_freeN(vert_str);
    MEM_freeN(shader_str);
  }
  return e_data.probe_planar_display_sh;
}

GPUShader *EEVEE_shaders_velocity_resolve_sh_get(void)
{
  if (e_data.velocity_resolve_sh == NULL) {
    char *frag_str = BLI_string_joinN(datatoc_common_uniforms_lib_glsl,
                                      datatoc_common_view_lib_glsl,
                                      datatoc_bsdf_common_lib_glsl,
                                      datatoc_effect_velocity_resolve_frag_glsl);

    e_data.velocity_resolve_sh = DRW_shader_create_fullscreen(frag_str, NULL);

    MEM_freeN(frag_str);
  }
  return e_data.velocity_resolve_sh;
}

GPUShader *EEVEE_shaders_taa_resolve_sh_get(EEVEE_EffectsFlag enabled_effects)
{
  GPUShader **sh;
  const char *define = NULL;
  if (enabled_effects & EFFECT_TAA_REPROJECT) {
    sh = &e_data.taa_resolve_reproject_sh;
    define = "#define USE_REPROJECTION\n";
  }
  else {
    sh = &e_data.taa_resolve_sh;
  }
  if (*sh == NULL) {
    char *frag_str = BLI_string_joinN(datatoc_common_uniforms_lib_glsl,
                                      datatoc_common_view_lib_glsl,
                                      datatoc_bsdf_common_lib_glsl,
                                      datatoc_effect_temporal_aa_glsl);

    *sh = DRW_shader_create_fullscreen(frag_str, define);
    MEM_freeN(frag_str);
  }

  return *sh;
}

void EEVEE_shaders_free(void)
{
  DRW_SHADER_FREE_SAFE(e_data.probe_default_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_filter_glossy_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_filter_diffuse_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_filter_visibility_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_grid_fill_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_planar_downsample_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_default_studiolight_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_grid_display_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_cube_display_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_planar_display_sh);
  DRW_SHADER_FREE_SAFE(e_data.velocity_resolve_sh);
  DRW_SHADER_FREE_SAFE(e_data.taa_resolve_sh);
  DRW_SHADER_FREE_SAFE(e_data.taa_resolve_reproject_sh);
}
