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

#include "BKE_lib_id.h"
#include "BKE_node.h"

#include "BLI_dynstr.h"
#include "BLI_string_utils.h"

#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "GPU_material.h"
#include "GPU_shader.h"

#include "NOD_shader.h"

#include "eevee_engine.h"
#include "eevee_private.h"

static const char *filter_defines = "#define HAMMERSLEY_SIZE " STRINGIFY(HAMMERSLEY_SIZE) "\n"
#if defined(IRRADIANCE_SH_L2)
                               "#define IRRADIANCE_SH_L2\n";
#elif defined(IRRADIANCE_HL2)
                               "#define IRRADIANCE_HL2\n";
#endif

static struct {
  /* Lookdev */
  struct GPUShader *studiolight_probe_sh;
  struct GPUShader *studiolight_background_sh;

  /* Probes */
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

  /* General purpose Shaders. */
  struct GPUShader *lookdev_background;
  struct GPUShader *update_noise_sh;

  /* Shader strings */
  char *closure_lit_lib;
  char *surface_lit_frag;
  char *surface_prepass_frag;
  char *surface_geom_barycentric;

  DRWShaderLibrary *lib;

  /* LookDev Materials */
  Material *glossy_mat;
  Material *diffuse_mat;

  Material *error_mat;

  World *default_world;

  /* Default Material */
  struct {
    bNodeTree *ntree;
    bNodeSocketValueRGBA *color_socket;
    bNodeSocketValueFloat *metallic_socket;
    bNodeSocketValueFloat *roughness_socket;
    bNodeSocketValueFloat *specular_socket;
  } surface;

  struct {
    bNodeTree *ntree;
    bNodeSocketValueRGBA *color_socket;
  } world;
} e_data = {NULL}; /* Engine data */

extern char datatoc_common_hair_lib_glsl[];
extern char datatoc_common_math_lib_glsl[];
extern char datatoc_common_math_geom_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];
extern char datatoc_gpu_shader_common_obinfos_lib_glsl[];

extern char datatoc_ambient_occlusion_lib_glsl[];
extern char datatoc_background_vert_glsl[];
extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_bsdf_lut_frag_glsl[];
extern char datatoc_bsdf_sampling_lib_glsl[];
extern char datatoc_btdf_lut_frag_glsl[];
extern char datatoc_closure_lib_glsl[];
extern char datatoc_common_uniforms_lib_glsl[];
extern char datatoc_common_utiltex_lib_glsl[];
extern char datatoc_cubemap_lib_glsl[];
extern char datatoc_default_frag_glsl[];
extern char datatoc_lookdev_world_frag_glsl[];
extern char datatoc_effect_bloom_frag_glsl[];
extern char datatoc_effect_dof_frag_glsl[];
extern char datatoc_effect_dof_vert_glsl[];
extern char datatoc_effect_downsample_cube_frag_glsl[];
extern char datatoc_effect_downsample_frag_glsl[];
extern char datatoc_effect_gtao_frag_glsl[];
extern char datatoc_effect_minmaxz_frag_glsl[];
extern char datatoc_effect_mist_frag_glsl[];
extern char datatoc_effect_motion_blur_frag_glsl[];
extern char datatoc_effect_ssr_frag_glsl[];
extern char datatoc_effect_subsurface_frag_glsl[];
extern char datatoc_effect_temporal_aa_glsl[];
extern char datatoc_effect_translucency_frag_glsl[];
extern char datatoc_effect_velocity_resolve_frag_glsl[];
extern char datatoc_effect_velocity_tile_frag_glsl[];
extern char datatoc_irradiance_lib_glsl[];
extern char datatoc_lightprobe_cube_display_frag_glsl[];
extern char datatoc_lightprobe_cube_display_vert_glsl[];
extern char datatoc_lightprobe_filter_diffuse_frag_glsl[];
extern char datatoc_lightprobe_filter_glossy_frag_glsl[];
extern char datatoc_lightprobe_filter_visibility_frag_glsl[];
extern char datatoc_lightprobe_geom_glsl[];
extern char datatoc_lightprobe_grid_display_frag_glsl[];
extern char datatoc_lightprobe_grid_display_vert_glsl[];
extern char datatoc_lightprobe_grid_fill_frag_glsl[];
extern char datatoc_lightprobe_lib_glsl[];
extern char datatoc_lightprobe_planar_display_frag_glsl[];
extern char datatoc_lightprobe_planar_display_vert_glsl[];
extern char datatoc_lightprobe_planar_downsample_frag_glsl[];
extern char datatoc_lightprobe_planar_downsample_geom_glsl[];
extern char datatoc_lightprobe_planar_downsample_vert_glsl[];
extern char datatoc_lightprobe_vert_glsl[];
extern char datatoc_lights_lib_glsl[];
extern char datatoc_closure_lit_lib_glsl[];
extern char datatoc_ltc_lib_glsl[];
extern char datatoc_object_motion_frag_glsl[];
extern char datatoc_object_motion_vert_glsl[];
extern char datatoc_octahedron_lib_glsl[];
extern char datatoc_prepass_frag_glsl[];
extern char datatoc_prepass_vert_glsl[];
extern char datatoc_raytrace_lib_glsl[];
extern char datatoc_renderpass_lib_glsl[];
extern char datatoc_renderpass_postprocess_frag_glsl[];
extern char datatoc_shadow_accum_frag_glsl[];
extern char datatoc_shadow_frag_glsl[];
extern char datatoc_shadow_vert_glsl[];
extern char datatoc_ssr_lib_glsl[];
extern char datatoc_surface_frag_glsl[];
extern char datatoc_surface_geom_glsl[];
extern char datatoc_surface_lib_glsl[];
extern char datatoc_surface_vert_glsl[];
extern char datatoc_update_noise_frag_glsl[];
extern char datatoc_volumetric_accum_frag_glsl[];
extern char datatoc_volumetric_frag_glsl[];
extern char datatoc_volumetric_geom_glsl[];
extern char datatoc_volumetric_integration_frag_glsl[];
extern char datatoc_volumetric_lib_glsl[];
extern char datatoc_volumetric_resolve_frag_glsl[];
extern char datatoc_volumetric_scatter_frag_glsl[];
extern char datatoc_volumetric_vert_glsl[];

/* *********** FUNCTIONS *********** */

static void eevee_shader_library_ensure(void)
{
  if (e_data.lib == NULL) {
    e_data.lib = DRW_shader_library_create();
    /* NOTE: Theses needs to be ordered by dependencies. */
    DRW_SHADER_LIB_ADD(e_data.lib, common_math_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, common_math_geom_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, common_hair_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, common_view_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, common_uniforms_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, gpu_shader_common_obinfos_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, renderpass_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, bsdf_common_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, common_utiltex_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, bsdf_sampling_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, cubemap_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, raytrace_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, ambient_occlusion_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, octahedron_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, irradiance_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, lightprobe_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, ltc_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, lights_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, surface_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, volumetric_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, closure_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, ssr_lib);

    /* Add one for each Closure */
    e_data.closure_lit_lib = BLI_string_joinN(datatoc_closure_lit_lib_glsl,
                                              datatoc_closure_lit_lib_glsl,
                                              datatoc_closure_lit_lib_glsl,
                                              datatoc_closure_lit_lib_glsl,
                                              datatoc_closure_lit_lib_glsl,
                                              datatoc_closure_lit_lib_glsl,
                                              datatoc_closure_lit_lib_glsl,
                                              datatoc_closure_lit_lib_glsl,
                                              datatoc_closure_lit_lib_glsl,
                                              datatoc_closure_lit_lib_glsl,
                                              datatoc_closure_lit_lib_glsl);

    DRW_shader_library_add_file(e_data.lib, e_data.closure_lit_lib, "closure_lit_lib.glsl");

    e_data.surface_lit_frag = DRW_shader_library_create_shader_string(e_data.lib,
                                                                      datatoc_surface_frag_glsl);

    e_data.surface_prepass_frag = DRW_shader_library_create_shader_string(
        e_data.lib, datatoc_prepass_frag_glsl);

    e_data.surface_geom_barycentric = DRW_shader_library_create_shader_string(
        e_data.lib, datatoc_surface_geom_glsl);
  }
}

void EEVEE_shaders_lightprobe_shaders_init(void)
{
  BLI_assert(e_data.probe_filter_glossy_sh == NULL);

  eevee_shader_library_ensure();

  e_data.probe_filter_glossy_sh = DRW_shader_create_with_shaderlib(
      datatoc_lightprobe_vert_glsl,
      datatoc_lightprobe_geom_glsl,
      datatoc_lightprobe_filter_glossy_frag_glsl,
      e_data.lib,
      filter_defines);

  e_data.probe_filter_diffuse_sh = DRW_shader_create_fullscreen_with_shaderlib(
      datatoc_lightprobe_filter_diffuse_frag_glsl, e_data.lib, filter_defines);

  e_data.probe_filter_visibility_sh = DRW_shader_create_fullscreen_with_shaderlib(
      datatoc_lightprobe_filter_visibility_frag_glsl, e_data.lib, filter_defines);

  e_data.probe_grid_fill_sh = DRW_shader_create_fullscreen_with_shaderlib(
      datatoc_lightprobe_grid_fill_frag_glsl, e_data.lib, filter_defines);

  e_data.probe_planar_downsample_sh = DRW_shader_create(
      datatoc_lightprobe_planar_downsample_vert_glsl,
      datatoc_lightprobe_planar_downsample_geom_glsl,
      datatoc_lightprobe_planar_downsample_frag_glsl,
      NULL);
}

void EEVEE_shaders_material_shaders_init(void)
{
  eevee_shader_library_ensure();
}

DRWShaderLibrary *EEVEE_shader_lib_get(void)
{
  eevee_shader_library_ensure();
  return e_data.lib;
}

GPUShader *EEVEE_shaders_probe_filter_glossy_sh_get(void)
{
  return e_data.probe_filter_glossy_sh;
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

GPUShader *EEVEE_shaders_studiolight_probe_sh_get(void)
{
  if (e_data.studiolight_probe_sh == NULL) {
    e_data.studiolight_probe_sh = DRW_shader_create_with_shaderlib(datatoc_background_vert_glsl,
                                                                   NULL,
                                                                   datatoc_lookdev_world_frag_glsl,
                                                                   e_data.lib,
                                                                   SHADER_DEFINES);
  }
  return e_data.studiolight_probe_sh;
}

GPUShader *EEVEE_shaders_studiolight_background_sh_get(void)
{
  if (e_data.studiolight_background_sh == NULL) {
    e_data.studiolight_background_sh = DRW_shader_create_with_shaderlib(
        datatoc_background_vert_glsl,
        NULL,
        datatoc_lookdev_world_frag_glsl,
        e_data.lib,
        "#define LOOKDEV_BG\n" SHADER_DEFINES);
  }
  return e_data.studiolight_background_sh;
}

GPUShader *EEVEE_shaders_probe_cube_display_sh_get(void)
{
  if (e_data.probe_cube_display_sh == NULL) {
    e_data.probe_cube_display_sh = DRW_shader_create_with_shaderlib(
        datatoc_lightprobe_cube_display_vert_glsl,
        NULL,
        datatoc_lightprobe_cube_display_frag_glsl,
        e_data.lib,
        SHADER_DEFINES);
  }
  return e_data.probe_cube_display_sh;
}

GPUShader *EEVEE_shaders_probe_grid_display_sh_get(void)
{
  if (e_data.probe_grid_display_sh == NULL) {
    e_data.probe_grid_display_sh = DRW_shader_create_with_shaderlib(
        datatoc_lightprobe_grid_display_vert_glsl,
        NULL,
        datatoc_lightprobe_grid_display_frag_glsl,
        e_data.lib,
        filter_defines);
  }
  return e_data.probe_grid_display_sh;
}

GPUShader *EEVEE_shaders_probe_planar_display_sh_get(void)
{
  if (e_data.probe_planar_display_sh == NULL) {
    e_data.probe_planar_display_sh = DRW_shader_create_with_shaderlib(
        datatoc_lightprobe_planar_display_vert_glsl,
        NULL,
        datatoc_lightprobe_planar_display_frag_glsl,
        e_data.lib,
        NULL);
  }
  return e_data.probe_planar_display_sh;
}

GPUShader *EEVEE_shaders_velocity_resolve_sh_get(void)
{
  if (e_data.velocity_resolve_sh == NULL) {
    e_data.velocity_resolve_sh = DRW_shader_create_fullscreen_with_shaderlib(
        datatoc_effect_velocity_resolve_frag_glsl, e_data.lib, NULL);
  }
  return e_data.velocity_resolve_sh;
}

GPUShader *EEVEE_shaders_update_noise_sh_get(void)
{
  if (e_data.update_noise_sh == NULL) {
    e_data.update_noise_sh = DRW_shader_create_fullscreen_with_shaderlib(
        datatoc_update_noise_frag_glsl, e_data.lib, NULL);
  }
  return e_data.update_noise_sh;
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
    *sh = DRW_shader_create_fullscreen_with_shaderlib(
        datatoc_effect_temporal_aa_glsl, e_data.lib, define);
  }

  return *sh;
}

Material *EEVEE_material_default_diffuse_get(void)
{
  if (!e_data.diffuse_mat) {
    Material *ma = BKE_id_new_nomain(ID_MA, "EEVEEE default diffuse");

    bNodeTree *ntree = ntreeAddTree(NULL, "Shader Nodetree", ntreeType_Shader->idname);
    ma->nodetree = ntree;
    ma->use_nodes = true;

    bNode *bsdf = nodeAddStaticNode(NULL, ntree, SH_NODE_BSDF_DIFFUSE);
    bNodeSocket *base_color = nodeFindSocket(bsdf, SOCK_IN, "Color");
    copy_v3_fl(((bNodeSocketValueRGBA *)base_color->default_value)->value, 0.8f);

    bNode *output = nodeAddStaticNode(NULL, ntree, SH_NODE_OUTPUT_MATERIAL);

    nodeAddLink(ntree,
                bsdf,
                nodeFindSocket(bsdf, SOCK_OUT, "BSDF"),
                output,
                nodeFindSocket(output, SOCK_IN, "Surface"));

    nodeSetActive(ntree, output);
    e_data.diffuse_mat = ma;
  }
  return e_data.diffuse_mat;
}

Material *EEVEE_material_default_glossy_get(void)
{
  if (!e_data.glossy_mat) {
    Material *ma = BKE_id_new_nomain(ID_MA, "EEVEEE default metal");

    bNodeTree *ntree = ntreeAddTree(NULL, "Shader Nodetree", ntreeType_Shader->idname);
    ma->nodetree = ntree;
    ma->use_nodes = true;

    bNode *bsdf = nodeAddStaticNode(NULL, ntree, SH_NODE_BSDF_GLOSSY);
    bNodeSocket *base_color = nodeFindSocket(bsdf, SOCK_IN, "Color");
    copy_v3_fl(((bNodeSocketValueRGBA *)base_color->default_value)->value, 1.0f);
    bNodeSocket *roughness = nodeFindSocket(bsdf, SOCK_IN, "Roughness");
    ((bNodeSocketValueFloat *)roughness->default_value)->value = 0.0f;

    bNode *output = nodeAddStaticNode(NULL, ntree, SH_NODE_OUTPUT_MATERIAL);

    nodeAddLink(ntree,
                bsdf,
                nodeFindSocket(bsdf, SOCK_OUT, "BSDF"),
                output,
                nodeFindSocket(output, SOCK_IN, "Surface"));

    nodeSetActive(ntree, output);
    e_data.glossy_mat = ma;
  }
  return e_data.glossy_mat;
}

Material *EEVEE_material_default_error_get(void)
{
  if (!e_data.error_mat) {
    Material *ma = BKE_id_new_nomain(ID_MA, "EEVEEE default metal");

    bNodeTree *ntree = ntreeAddTree(NULL, "Shader Nodetree", ntreeType_Shader->idname);
    ma->nodetree = ntree;
    ma->use_nodes = true;

    /* Use emission and output material to be compatible with both World and Material. */
    bNode *bsdf = nodeAddStaticNode(NULL, ntree, SH_NODE_EMISSION);
    bNodeSocket *color = nodeFindSocket(bsdf, SOCK_IN, "Color");
    copy_v3_fl3(((bNodeSocketValueRGBA *)color->default_value)->value, 1.0f, 0.0f, 1.0f);

    bNode *output = nodeAddStaticNode(NULL, ntree, SH_NODE_OUTPUT_MATERIAL);

    nodeAddLink(ntree,
                bsdf,
                nodeFindSocket(bsdf, SOCK_OUT, "Emission"),
                output,
                nodeFindSocket(output, SOCK_IN, "Surface"));

    nodeSetActive(ntree, output);
    e_data.error_mat = ma;
  }
  return e_data.error_mat;
}

/* Configure a default nodetree with the given material.  */
struct bNodeTree *EEVEE_shader_default_surface_nodetree(Material *ma)
{
  /* WARNING: This function is not threadsafe. Which is not a problem for the moment. */
  if (!e_data.surface.ntree) {
    bNodeTree *ntree = ntreeAddTree(NULL, "Shader Nodetree", ntreeType_Shader->idname);
    bNode *bsdf = nodeAddStaticNode(NULL, ntree, SH_NODE_BSDF_PRINCIPLED);
    bNode *output = nodeAddStaticNode(NULL, ntree, SH_NODE_OUTPUT_MATERIAL);
    bNodeSocket *bsdf_out = nodeFindSocket(bsdf, SOCK_OUT, "BSDF");
    bNodeSocket *output_in = nodeFindSocket(output, SOCK_IN, "Surface");
    nodeAddLink(ntree, bsdf, bsdf_out, output, output_in);
    nodeSetActive(ntree, output);

    e_data.surface.color_socket = nodeFindSocket(bsdf, SOCK_IN, "Base Color")->default_value;
    e_data.surface.metallic_socket = nodeFindSocket(bsdf, SOCK_IN, "Metallic")->default_value;
    e_data.surface.roughness_socket = nodeFindSocket(bsdf, SOCK_IN, "Roughness")->default_value;
    e_data.surface.specular_socket = nodeFindSocket(bsdf, SOCK_IN, "Specular")->default_value;
    e_data.surface.ntree = ntree;
  }
  /* Update */
  copy_v3_fl3(e_data.surface.color_socket->value, ma->r, ma->g, ma->b);
  e_data.surface.metallic_socket->value = ma->metallic;
  e_data.surface.roughness_socket->value = ma->roughness;
  e_data.surface.specular_socket->value = ma->spec;

  return e_data.surface.ntree;
}

/* Configure a default nodetree with the given world.  */
struct bNodeTree *EEVEE_shader_default_world_nodetree(World *wo)
{
  /* WARNING: This function is not threadsafe. Which is not a problem for the moment. */
  if (!e_data.world.ntree) {
    bNodeTree *ntree = ntreeAddTree(NULL, "Shader Nodetree", ntreeType_Shader->idname);
    bNode *bg = nodeAddStaticNode(NULL, ntree, SH_NODE_BACKGROUND);
    bNode *output = nodeAddStaticNode(NULL, ntree, SH_NODE_OUTPUT_WORLD);
    bNodeSocket *bg_out = nodeFindSocket(bg, SOCK_OUT, "Background");
    bNodeSocket *output_in = nodeFindSocket(output, SOCK_IN, "Surface");
    nodeAddLink(ntree, bg, bg_out, output, output_in);
    nodeSetActive(ntree, output);

    e_data.world.color_socket = nodeFindSocket(bg, SOCK_IN, "Color")->default_value;
    e_data.world.ntree = ntree;
  }

  copy_v3_fl3(e_data.world.color_socket->value, wo->horr, wo->horg, wo->horb);

  return e_data.world.ntree;
}

World *EEVEE_world_default_get(void)
{
  if (e_data.default_world == NULL) {
    e_data.default_world = BKE_id_new_nomain(ID_WO, "EEVEEE default world");
    copy_v3_fl(&e_data.default_world->horr, 0.0f);
    e_data.default_world->use_nodes = 0;
    e_data.default_world->nodetree = NULL;
    BLI_listbase_clear(&e_data.default_world->gpumaterial);
  }
  return e_data.default_world;
}

static char *eevee_get_defines(int options)
{
  char *str = NULL;

  DynStr *ds = BLI_dynstr_new();
  BLI_dynstr_append(ds, SHADER_DEFINES);

  if ((options & VAR_WORLD_BACKGROUND) != 0) {
    BLI_dynstr_append(ds, "#define WORLD_BACKGROUND\n");
  }
  if ((options & VAR_MAT_VOLUME) != 0) {
    BLI_dynstr_append(ds, "#define VOLUMETRICS\n");
  }
  if ((options & VAR_MAT_MESH) != 0) {
    BLI_dynstr_append(ds, "#define MESH_SHADER\n");
  }
  if ((options & VAR_MAT_DEPTH) != 0) {
    BLI_dynstr_append(ds, "#define DEPTH_SHADER\n");
  }
  if ((options & VAR_MAT_HAIR) != 0) {
    BLI_dynstr_append(ds, "#define HAIR_SHADER\n");
  }
  if ((options & VAR_WORLD_PROBE) != 0) {
    BLI_dynstr_append(ds, "#define PROBE_CAPTURE\n");
  }
  if ((options & VAR_MAT_HASH) != 0) {
    BLI_dynstr_append(ds, "#define USE_ALPHA_HASH\n");
  }
  if ((options & VAR_MAT_BLEND) != 0) {
    BLI_dynstr_append(ds, "#define USE_ALPHA_BLEND\n");
  }
  if ((options & VAR_MAT_REFRACT) != 0) {
    BLI_dynstr_append(ds, "#define USE_REFRACTION\n");
  }
  if ((options & VAR_MAT_LOOKDEV) != 0) {
    BLI_dynstr_append(ds, "#define LOOKDEV\n");
  }
  if ((options & VAR_MAT_HOLDOUT) != 0) {
    BLI_dynstr_append(ds, "#define HOLDOUT\n");
  }

  str = BLI_dynstr_get_cstring(ds);
  BLI_dynstr_free(ds);

  return str;
}

static char *eevee_get_vert(int options)
{
  char *str = NULL;

  if ((options & VAR_MAT_VOLUME) != 0) {
    str = DRW_shader_library_create_shader_string(e_data.lib, datatoc_volumetric_vert_glsl);
  }
  else if ((options & (VAR_WORLD_PROBE | VAR_WORLD_BACKGROUND)) != 0) {
    str = DRW_shader_library_create_shader_string(e_data.lib, datatoc_background_vert_glsl);
  }
  else {
    str = DRW_shader_library_create_shader_string(e_data.lib, datatoc_surface_vert_glsl);
  }

  return str;
}

static char *eevee_get_geom(int options)
{
  char *str = NULL;

  if ((options & VAR_MAT_VOLUME) != 0) {
    str = DRW_shader_library_create_shader_string(e_data.lib, datatoc_volumetric_geom_glsl);
  }

  return str;
}

static char *eevee_get_frag(int options)
{
  char *str = NULL;

  if ((options & VAR_MAT_VOLUME) != 0) {
    str = DRW_shader_library_create_shader_string(e_data.lib, datatoc_volumetric_frag_glsl);
  }
  else if ((options & VAR_MAT_DEPTH) != 0) {
    str = BLI_strdup(e_data.surface_prepass_frag);
  }
  else {
    str = BLI_strdup(e_data.surface_lit_frag);
  }

  return str;
}

static void eevee_material_post_eval(GPUMaterial *mat,
                                     int options,
                                     const char **UNUSED(vert_code),
                                     const char **geom_code,
                                     const char **UNUSED(frag_lib),
                                     const char **UNUSED(defines))
{
  const bool is_hair = (options & VAR_MAT_HAIR) != 0;
  const bool is_mesh = (options & VAR_MAT_MESH) != 0;

  /* Force geometry usage if GPU_BARYCENTRIC_DIST or GPU_BARYCENTRIC_TEXCO are used.
   * Note: GPU_BARYCENTRIC_TEXCO only requires it if the shader is not drawing hairs. */
  if (!is_hair && is_mesh && GPU_material_flag_get(mat, GPU_MATFLAG_BARYCENTRIC) &&
      *geom_code == NULL) {
    *geom_code = e_data.surface_geom_barycentric;
  }
}

static struct GPUMaterial *eevee_material_get_ex(
    struct Scene *scene, Material *ma, World *wo, int options, bool deferred)
{
  BLI_assert(ma || wo);
  const bool is_volume = (options & VAR_MAT_VOLUME) != 0;
  const bool is_default = (options & VAR_DEFAULT) != 0;
  const void *engine = &DRW_engine_viewport_eevee_type;

  GPUMaterial *mat = NULL;

  if (ma) {
    mat = DRW_shader_find_from_material(ma, engine, options, deferred);
  }
  else {
    mat = DRW_shader_find_from_world(wo, engine, options, deferred);
  }

  if (mat) {
    return mat;
  }

  char *defines = eevee_get_defines(options);
  char *vert = eevee_get_vert(options);
  char *geom = eevee_get_geom(options);
  char *frag = eevee_get_frag(options);

  if (ma) {
    GPUMaterialEvalCallbackFn cbfn = &eevee_material_post_eval;

    bNodeTree *ntree = !is_default ? ma->nodetree : EEVEE_shader_default_surface_nodetree(ma);
    mat = DRW_shader_create_from_material(
        scene, ma, ntree, engine, options, is_volume, vert, geom, frag, defines, deferred, cbfn);
  }
  else {
    bNodeTree *ntree = !is_default ? wo->nodetree : EEVEE_shader_default_world_nodetree(wo);
    mat = DRW_shader_create_from_world(
        scene, wo, ntree, engine, options, is_volume, vert, geom, frag, defines, deferred, NULL);
  }

  MEM_SAFE_FREE(defines);
  MEM_SAFE_FREE(vert);
  MEM_SAFE_FREE(geom);
  MEM_SAFE_FREE(frag);

  return mat;
}

/* Note: Compilation is not deferred. */
struct GPUMaterial *EEVEE_material_default_get(struct Scene *scene, Material *ma, int options)
{
  Material *def_ma = (ma && (options & VAR_MAT_VOLUME)) ? BKE_material_default_volume() :
                                                          BKE_material_default_surface();
  BLI_assert(def_ma->use_nodes && def_ma->nodetree);

  return eevee_material_get_ex(scene, def_ma, NULL, options, false);
}

struct GPUMaterial *EEVEE_material_get(
    EEVEE_Data *vedata, struct Scene *scene, Material *ma, World *wo, int options)
{
  if ((ma && (!ma->use_nodes || !ma->nodetree)) || (wo && (!wo->use_nodes || !wo->nodetree))) {
    options |= VAR_DEFAULT;
  }

  /* Meh, implicit option. World probe cannot be deferred because they need
   * to be rendered immediately. */
  const bool deferred = (options & VAR_WORLD_PROBE) == 0;

  GPUMaterial *mat = eevee_material_get_ex(scene, ma, wo, options, deferred);

  int status = GPU_material_status(mat);
  switch (status) {
    case GPU_MAT_SUCCESS:
      break;
    case GPU_MAT_QUEUED:
      vedata->stl->g_data->queued_shaders_count++;
      mat = EEVEE_material_default_get(scene, ma, options);
      break;
    case GPU_MAT_FAILED:
    default:
      ma = EEVEE_material_default_error_get();
      mat = eevee_material_get_ex(scene, ma, NULL, options, false);
      break;
  }
  /* Returned material should be ready to be drawn. */
  BLI_assert(GPU_material_status(mat) == GPU_MAT_SUCCESS);
  return mat;
}

void EEVEE_shaders_free(void)
{
  MEM_SAFE_FREE(e_data.closure_lit_lib);
  MEM_SAFE_FREE(e_data.surface_prepass_frag);
  MEM_SAFE_FREE(e_data.surface_lit_frag);
  MEM_SAFE_FREE(e_data.surface_geom_barycentric);
  DRW_SHADER_FREE_SAFE(e_data.lookdev_background);
  DRW_SHADER_FREE_SAFE(e_data.update_noise_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_filter_glossy_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_filter_diffuse_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_filter_visibility_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_grid_fill_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_planar_downsample_sh);
  DRW_SHADER_FREE_SAFE(e_data.studiolight_probe_sh);
  DRW_SHADER_FREE_SAFE(e_data.studiolight_background_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_grid_display_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_cube_display_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_planar_display_sh);
  DRW_SHADER_FREE_SAFE(e_data.velocity_resolve_sh);
  DRW_SHADER_FREE_SAFE(e_data.taa_resolve_sh);
  DRW_SHADER_FREE_SAFE(e_data.taa_resolve_reproject_sh);
  DRW_SHADER_LIB_FREE_SAFE(e_data.lib);

  if (e_data.default_world) {
    BKE_id_free(NULL, e_data.default_world);
    e_data.default_world = NULL;
  }
  if (e_data.glossy_mat) {
    BKE_id_free(NULL, e_data.glossy_mat);
    e_data.glossy_mat = NULL;
  }
  if (e_data.diffuse_mat) {
    BKE_id_free(NULL, e_data.diffuse_mat);
    e_data.diffuse_mat = NULL;
  }
  if (e_data.error_mat) {
    BKE_id_free(NULL, e_data.error_mat);
    e_data.error_mat = NULL;
  }
  if (e_data.surface.ntree) {
    ntreeFreeEmbeddedTree(e_data.surface.ntree);
    MEM_freeN(e_data.surface.ntree);
    e_data.surface.ntree = NULL;
  }
  if (e_data.world.ntree) {
    ntreeFreeEmbeddedTree(e_data.world.ntree);
    MEM_freeN(e_data.world.ntree);
    e_data.world.ntree = NULL;
  }
}
