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

#include "BLI_dynstr.h"
#include "BLI_ghash.h"
#include "BLI_alloca.h"
#include "BLI_rand.h"
#include "BLI_string_utils.h"

#include "BKE_particle.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"

#include "DNA_world_types.h"
#include "DNA_modifier_types.h"
#include "DNA_view3d_types.h"

#include "GPU_material.h"

#include "DEG_depsgraph_query.h"

#include "eevee_engine.h"
#include "eevee_lut.h"
#include "eevee_private.h"

/* *********** STATIC *********** */
static struct {
  char *frag_shader_lib;
  char *vert_shader_str;
  char *vert_shadow_shader_str;
  char *volume_shader_lib;

  struct GPUShader *default_prepass_sh;
  struct GPUShader *default_prepass_clip_sh;
  struct GPUShader *default_hair_prepass_sh;
  struct GPUShader *default_hair_prepass_clip_sh;
  struct GPUShader *default_lit[VAR_MAT_MAX];
  struct GPUShader *default_background;
  struct GPUShader *update_noise_sh;

  /* 64*64 array texture containing all LUTs and other utilitarian arrays.
   * Packing enables us to same precious textures slots. */
  struct GPUTexture *util_tex;
  struct GPUTexture *noise_tex;

  struct GPUUniformBuffer *dummy_sss_profile;

  uint sss_count;

  float alpha_hash_offset;
  float alpha_hash_scale;
  float noise_offsets[3];
} e_data = {NULL}; /* Engine data */

extern char datatoc_lights_lib_glsl[];
extern char datatoc_lightprobe_lib_glsl[];
extern char datatoc_ambient_occlusion_lib_glsl[];
extern char datatoc_prepass_frag_glsl[];
extern char datatoc_prepass_vert_glsl[];
extern char datatoc_default_frag_glsl[];
extern char datatoc_default_world_frag_glsl[];
extern char datatoc_ltc_lib_glsl[];
extern char datatoc_bsdf_lut_frag_glsl[];
extern char datatoc_btdf_lut_frag_glsl[];
extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_bsdf_sampling_lib_glsl[];
extern char datatoc_common_uniforms_lib_glsl[];
extern char datatoc_common_hair_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];
extern char datatoc_irradiance_lib_glsl[];
extern char datatoc_octahedron_lib_glsl[];
extern char datatoc_lit_surface_frag_glsl[];
extern char datatoc_lit_surface_vert_glsl[];
extern char datatoc_raytrace_lib_glsl[];
extern char datatoc_ssr_lib_glsl[];
extern char datatoc_shadow_vert_glsl[];
extern char datatoc_lightprobe_geom_glsl[];
extern char datatoc_lightprobe_vert_glsl[];
extern char datatoc_background_vert_glsl[];
extern char datatoc_update_noise_frag_glsl[];
extern char datatoc_volumetric_vert_glsl[];
extern char datatoc_volumetric_geom_glsl[];
extern char datatoc_volumetric_frag_glsl[];
extern char datatoc_volumetric_lib_glsl[];

extern char datatoc_gpu_shader_uniform_color_frag_glsl[];

/* *********** FUNCTIONS *********** */

#if 0 /* Used only to generate the LUT values */
static struct GPUTexture *create_ggx_lut_texture(int UNUSED(w), int UNUSED(h))
{
  struct GPUTexture *tex;
  struct GPUFrameBuffer *fb = NULL;
  static float samples_len = 8192.0f;
  static float inv_samples_len = 1.0f / 8192.0f;

  char *lib_str = BLI_string_joinN(datatoc_bsdf_common_lib_glsl, datatoc_bsdf_sampling_lib_glsl);

  struct GPUShader *sh = DRW_shader_create_with_lib(datatoc_lightprobe_vert_glsl,
                                                    datatoc_lightprobe_geom_glsl,
                                                    datatoc_bsdf_lut_frag_glsl,
                                                    lib_str,
                                                    "#define HAMMERSLEY_SIZE 8192\n"
                                                    "#define BRDF_LUT_SIZE 64\n"
                                                    "#define NOISE_SIZE 64\n");

  DRWPass *pass = DRW_pass_create("LightProbe Filtering", DRW_STATE_WRITE_COLOR);
  DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
  DRW_shgroup_uniform_float(grp, "sampleCount", &samples_len, 1);
  DRW_shgroup_uniform_float(grp, "invSampleCount", &inv_samples_len, 1);
  DRW_shgroup_uniform_texture(grp, "texHammersley", e_data.hammersley);
  DRW_shgroup_uniform_texture(grp, "texJitter", e_data.jitter);

  struct GPUBatch *geom = DRW_cache_fullscreen_quad_get();
  DRW_shgroup_call_add(grp, geom, NULL);

  float *texels = MEM_mallocN(sizeof(float[2]) * w * h, "lut");

  tex = DRW_texture_create_2d(w, h, GPU_RG16F, DRW_TEX_FILTER, (float *)texels);

  DRWFboTexture tex_filter = {&tex, GPU_RG16F, DRW_TEX_FILTER};
  GPU_framebuffer_init(&fb, &draw_engine_eevee_type, w, h, &tex_filter, 1);

  GPU_framebuffer_bind(fb);
  DRW_draw_pass(pass);

  float *data = MEM_mallocN(sizeof(float[3]) * w * h, "lut");
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glReadPixels(0, 0, w, h, GL_RGB, GL_FLOAT, data);

  printf("{");
  for (int i = 0; i < w * h * 3; i += 3) {
    printf("%ff, %ff, ", data[i], data[i + 1]);
    i += 3;
    printf("%ff, %ff, ", data[i], data[i + 1]);
    i += 3;
    printf("%ff, %ff, ", data[i], data[i + 1]);
    i += 3;
    printf("%ff, %ff, \n", data[i], data[i + 1]);
  }
  printf("}");

  MEM_freeN(texels);
  MEM_freeN(data);

  return tex;
}

static struct GPUTexture *create_ggx_refraction_lut_texture(int w, int h)
{
  struct GPUTexture *tex;
  struct GPUTexture *hammersley = create_hammersley_sample_texture(8192);
  struct GPUFrameBuffer *fb = NULL;
  static float samples_len = 8192.0f;
  static float a2 = 0.0f;
  static float inv_samples_len = 1.0f / 8192.0f;

  char *frag_str = BLI_string_joinN(
      datatoc_bsdf_common_lib_glsl, datatoc_bsdf_sampling_lib_glsl, datatoc_btdf_lut_frag_glsl);

  struct GPUShader *sh = DRW_shader_create_fullscreen(frag_str,
                                                      "#define HAMMERSLEY_SIZE 8192\n"
                                                      "#define BRDF_LUT_SIZE 64\n"
                                                      "#define NOISE_SIZE 64\n"
                                                      "#define LUT_SIZE 64\n");

  MEM_freeN(frag_str);

  DRWPass *pass = DRW_pass_create("LightProbe Filtering", DRW_STATE_WRITE_COLOR);
  DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
  DRW_shgroup_uniform_float(grp, "a2", &a2, 1);
  DRW_shgroup_uniform_float(grp, "sampleCount", &samples_len, 1);
  DRW_shgroup_uniform_float(grp, "invSampleCount", &inv_samples_len, 1);
  DRW_shgroup_uniform_texture(grp, "texHammersley", hammersley);
  DRW_shgroup_uniform_texture(grp, "utilTex", e_data.util_tex);

  struct GPUBatch *geom = DRW_cache_fullscreen_quad_get();
  DRW_shgroup_call_add(grp, geom, NULL);

  float *texels = MEM_mallocN(sizeof(float[2]) * w * h, "lut");

  tex = DRW_texture_create_2d(w, h, GPU_R16F, DRW_TEX_FILTER, (float *)texels);

  DRWFboTexture tex_filter = {&tex, GPU_R16F, DRW_TEX_FILTER};
  GPU_framebuffer_init(&fb, &draw_engine_eevee_type, w, h, &tex_filter, 1);

  GPU_framebuffer_bind(fb);

  float *data = MEM_mallocN(sizeof(float[3]) * w * h, "lut");

  float inc = 1.0f / 31.0f;
  float roughness = 1e-8f - inc;
  FILE *f = BLI_fopen("btdf_split_sum_ggx.h", "w");
  fprintf(f, "static float btdf_split_sum_ggx[32][64 * 64] = {\n");
  do {
    roughness += inc;
    CLAMP(roughness, 1e-4f, 1.0f);
    a2 = powf(roughness, 4.0f);
    DRW_draw_pass(pass);

    GPU_framebuffer_read_data(0, 0, w, h, 3, 0, data);

#  if 1
    fprintf(f, "\t{\n\t\t");
    for (int i = 0; i < w * h * 3; i += 3) {
      fprintf(f, "%ff,", data[i]);
      if (((i / 3) + 1) % 12 == 0)
        fprintf(f, "\n\t\t");
      else
        fprintf(f, " ");
    }
    fprintf(f, "\n\t},\n");
#  else
    for (int i = 0; i < w * h * 3; i += 3) {
      if (data[i] < 0.01)
        printf(" ");
      else if (data[i] < 0.3)
        printf(".");
      else if (data[i] < 0.6)
        printf("+");
      else if (data[i] < 0.9)
        printf("%%");
      else
        printf("#");
      if ((i / 3 + 1) % 64 == 0)
        printf("\n");
    }
#  endif

  } while (roughness < 1.0f);
  fprintf(f, "\n};\n");

  fclose(f);

  MEM_freeN(texels);
  MEM_freeN(data);

  return tex;
}
#endif
/* XXX TODO define all shared resources in a shared place without duplication */
struct GPUTexture *EEVEE_materials_get_util_tex(void)
{
  return e_data.util_tex;
}

static int eevee_material_shadow_option(int shadow_method)
{
  switch (shadow_method) {
    case SHADOW_ESM:
      return VAR_MAT_ESM;
    case SHADOW_VSM:
      return VAR_MAT_VSM;
    default:
      BLI_assert(!"Incorrect Shadow Method");
      break;
  }

  return 0;
}

static char *eevee_get_defines(int options)
{
  char *str = NULL;

  DynStr *ds = BLI_dynstr_new();
  BLI_dynstr_append(ds, SHADER_DEFINES);

  if ((options & VAR_MAT_MESH) != 0) {
    BLI_dynstr_append(ds, "#define MESH_SHADER\n");
  }
  if ((options & VAR_MAT_HAIR) != 0) {
    BLI_dynstr_append(ds, "#define HAIR_SHADER\n");
  }
  if ((options & VAR_MAT_PROBE) != 0) {
    BLI_dynstr_append(ds, "#define PROBE_CAPTURE\n");
  }
  if ((options & VAR_MAT_CLIP) != 0) {
    BLI_dynstr_append(ds, "#define USE_ALPHA_CLIP\n");
  }
  if ((options & VAR_MAT_SHADOW) != 0) {
    BLI_dynstr_append(ds, "#define SHADOW_SHADER\n");
  }
  if ((options & VAR_MAT_HASH) != 0) {
    BLI_dynstr_append(ds, "#define USE_ALPHA_HASH\n");
  }
  if ((options & VAR_MAT_BLEND) != 0) {
    BLI_dynstr_append(ds, "#define USE_ALPHA_BLEND\n");
  }
  if ((options & VAR_MAT_MULT) != 0) {
    BLI_dynstr_append(ds, "#define USE_MULTIPLY\n");
  }
  if ((options & VAR_MAT_REFRACT) != 0) {
    BLI_dynstr_append(ds, "#define USE_REFRACTION\n");
  }
  if ((options & VAR_MAT_SSS) != 0) {
    BLI_dynstr_append(ds, "#define USE_SSS\n");
  }
  if ((options & VAR_MAT_SSSALBED) != 0) {
    BLI_dynstr_append(ds, "#define USE_SSS_ALBEDO\n");
  }
  if ((options & VAR_MAT_TRANSLUC) != 0) {
    BLI_dynstr_append(ds, "#define USE_TRANSLUCENCY\n");
  }
  if ((options & VAR_MAT_VSM) != 0) {
    BLI_dynstr_append(ds, "#define SHADOW_VSM\n");
  }
  if ((options & VAR_MAT_ESM) != 0) {
    BLI_dynstr_append(ds, "#define SHADOW_ESM\n");
  }
  if (((options & VAR_MAT_VOLUME) != 0) && ((options & VAR_MAT_BLEND) != 0)) {
    BLI_dynstr_append(ds, "#define USE_ALPHA_BLEND_VOLUMETRICS\n");
  }
  if ((options & VAR_MAT_LOOKDEV) != 0) {
    /* Auto config shadow method. Avoid more permutation. */
    BLI_assert((options & (VAR_MAT_VSM | VAR_MAT_ESM)) == 0);
    BLI_dynstr_append(ds, "#define LOOKDEV\n");
    BLI_dynstr_append(ds, "#define SHADOW_ESM\n");
  }

  str = BLI_dynstr_get_cstring(ds);
  BLI_dynstr_free(ds);

  return str;
}

static char *eevee_get_volume_defines(int options)
{
  char *str = NULL;

  DynStr *ds = BLI_dynstr_new();
  BLI_dynstr_append(ds, SHADER_DEFINES);
  BLI_dynstr_append(ds, "#define VOLUMETRICS\n");

  if ((options & VAR_MAT_VOLUME) != 0) {
    BLI_dynstr_append(ds, "#define MESH_SHADER\n");
  }

  str = BLI_dynstr_get_cstring(ds);
  BLI_dynstr_free(ds);

  return str;
}

/**
 * ssr_id can be null to disable ssr contribution.
 */
static void add_standard_uniforms(DRWShadingGroup *shgrp,
                                  EEVEE_ViewLayerData *sldata,
                                  EEVEE_Data *vedata,
                                  int *ssr_id,
                                  float *refract_depth,
                                  bool use_diffuse,
                                  bool use_glossy,
                                  bool use_refract,
                                  bool use_ssrefraction,
                                  bool use_alpha_blend)
{
  LightCache *lcache = vedata->stl->g_data->light_cache;

  if (ssr_id == NULL) {
    static int no_ssr = -1.0f;
    ssr_id = &no_ssr;
  }

  DRW_shgroup_uniform_block(shgrp, "probe_block", sldata->probe_ubo);
  DRW_shgroup_uniform_block(shgrp, "grid_block", sldata->grid_ubo);
  DRW_shgroup_uniform_block(shgrp, "planar_block", sldata->planar_ubo);
  DRW_shgroup_uniform_block(shgrp, "light_block", sldata->light_ubo);
  DRW_shgroup_uniform_block(shgrp, "shadow_block", sldata->shadow_ubo);
  DRW_shgroup_uniform_block(shgrp, "common_block", sldata->common_ubo);
  DRW_shgroup_uniform_block(shgrp, "clip_block", sldata->clip_ubo);

  if (use_diffuse || use_glossy || use_refract) {
    DRW_shgroup_uniform_texture(shgrp, "utilTex", e_data.util_tex);
    DRW_shgroup_uniform_texture_ref(shgrp, "shadowCubeTexture", &sldata->shadow_cube_pool);
    DRW_shgroup_uniform_texture_ref(shgrp, "shadowCascadeTexture", &sldata->shadow_cascade_pool);
    DRW_shgroup_uniform_texture_ref(shgrp, "maxzBuffer", &vedata->txl->maxzbuffer);
  }
  if ((use_diffuse || use_glossy) && !use_ssrefraction) {
    if ((vedata->stl->effects->enabled_effects & EFFECT_GTAO) != 0) {
      DRW_shgroup_uniform_texture_ref(
          shgrp, "horizonBuffer", &vedata->stl->effects->gtao_horizons);
    }
    else {
      /* Use maxzbuffer as fallback to avoid sampling problem on certain platform, see: T52593 */
      DRW_shgroup_uniform_texture_ref(shgrp, "horizonBuffer", &vedata->txl->maxzbuffer);
    }
  }
  if (use_diffuse) {
    DRW_shgroup_uniform_texture_ref(shgrp, "irradianceGrid", &lcache->grid_tx.tex);
  }
  if (use_glossy || use_refract) {
    DRW_shgroup_uniform_texture_ref(shgrp, "probeCubes", &lcache->cube_tx.tex);
  }
  if (use_glossy) {
    DRW_shgroup_uniform_texture_ref(shgrp, "probePlanars", &vedata->txl->planar_pool);
    DRW_shgroup_uniform_int(shgrp, "outputSsrId", ssr_id, 1);
  }
  if (use_refract) {
    DRW_shgroup_uniform_float_copy(
        shgrp, "refractionDepth", (refract_depth) ? *refract_depth : 0.0);
    if (use_ssrefraction) {
      DRW_shgroup_uniform_texture_ref(shgrp, "colorBuffer", &vedata->txl->refract_color);
    }
  }

  if ((vedata->stl->effects->enabled_effects & EFFECT_VOLUMETRIC) != 0 && use_alpha_blend) {
    /* Do not use history buffers as they already have been swapped */
    DRW_shgroup_uniform_texture_ref(shgrp, "inScattering", &vedata->txl->volume_scatter);
    DRW_shgroup_uniform_texture_ref(shgrp, "inTransmittance", &vedata->txl->volume_transmittance);
  }
}

static void create_default_shader(int options)
{
  char *frag_str = BLI_string_joinN(e_data.frag_shader_lib, datatoc_default_frag_glsl);

  char *defines = eevee_get_defines(options);

  e_data.default_lit[options] = DRW_shader_create(e_data.vert_shader_str, NULL, frag_str, defines);

  MEM_freeN(defines);
  MEM_freeN(frag_str);
}

static void eevee_init_dummys(void)
{
  e_data.dummy_sss_profile = GPU_material_create_sss_profile_ubo();
}

static void eevee_init_noise_texture(void)
{
  e_data.noise_tex = DRW_texture_create_2d(64, 64, GPU_RGBA16F, 0, (float *)blue_noise);
}

static void eevee_init_util_texture(void)
{
  const int layers = 4 + 16;
  float(*texels)[4] = MEM_mallocN(sizeof(float[4]) * 64 * 64 * layers, "utils texels");
  float(*texels_layer)[4] = texels;

  /* Copy ltc_mat_ggx into 1st layer */
  memcpy(texels_layer, ltc_mat_ggx, sizeof(float[4]) * 64 * 64);
  texels_layer += 64 * 64;

  /* Copy bsdf_split_sum_ggx into 2nd layer red and green channels.
   * Copy ltc_mag_ggx into 2nd layer blue and alpha channel. */
  for (int i = 0; i < 64 * 64; i++) {
    texels_layer[i][0] = bsdf_split_sum_ggx[i * 2 + 0];
    texels_layer[i][1] = bsdf_split_sum_ggx[i * 2 + 1];
    texels_layer[i][2] = ltc_mag_ggx[i * 2 + 0];
    texels_layer[i][3] = ltc_mag_ggx[i * 2 + 1];
  }
  texels_layer += 64 * 64;

  /* Copy blue noise in 3rd layer  */
  for (int i = 0; i < 64 * 64; i++) {
    texels_layer[i][0] = blue_noise[i][0];
    texels_layer[i][1] = blue_noise[i][2];
    texels_layer[i][2] = cosf(blue_noise[i][1] * 2.0f * M_PI);
    texels_layer[i][3] = sinf(blue_noise[i][1] * 2.0f * M_PI);
  }
  texels_layer += 64 * 64;

  /* Copy ltc_disk_integral in 4th layer  */
  for (int i = 0; i < 64 * 64; i++) {
    texels_layer[i][0] = ltc_disk_integral[i];
    texels_layer[i][1] = 0.0; /* UNUSED */
    texels_layer[i][2] = 0.0; /* UNUSED */
    texels_layer[i][3] = 0.0; /* UNUSED */
  }
  texels_layer += 64 * 64;

  /* Copy Refraction GGX LUT in layer 5 - 21 */
  for (int j = 0; j < 16; ++j) {
    for (int i = 0; i < 64 * 64; i++) {
      texels_layer[i][0] = btdf_split_sum_ggx[j * 2][i];
      texels_layer[i][1] = 0.0; /* UNUSED */
      texels_layer[i][2] = 0.0; /* UNUSED */
      texels_layer[i][3] = 0.0; /* UNUSED */
    }
    texels_layer += 64 * 64;
  }

  e_data.util_tex = DRW_texture_create_2d_array(
      64, 64, layers, GPU_RGBA16F, DRW_TEX_FILTER | DRW_TEX_WRAP, (float *)texels);

  MEM_freeN(texels);
}

void EEVEE_update_noise(EEVEE_PassList *psl, EEVEE_FramebufferList *fbl, const double offsets[3])
{
  e_data.noise_offsets[0] = offsets[0];
  e_data.noise_offsets[1] = offsets[1];
  e_data.noise_offsets[2] = offsets[2];

  /* Attach & detach because we don't currently support multiple FB per texture,
   * and this would be the case for multiple viewport. */
  GPU_framebuffer_bind(fbl->update_noise_fb);
  DRW_draw_pass(psl->update_noise_pass);
}

void EEVEE_update_viewvecs(float invproj[4][4], float winmat[4][4], float (*r_viewvecs)[4])
{
  /* view vectors for the corners of the view frustum.
   * Can be used to recreate the world space position easily */
  float view_vecs[4][4] = {
      {-1.0f, -1.0f, -1.0f, 1.0f},
      {1.0f, -1.0f, -1.0f, 1.0f},
      {-1.0f, 1.0f, -1.0f, 1.0f},
      {-1.0f, -1.0f, 1.0f, 1.0f},
  };

  /* convert the view vectors to view space */
  const bool is_persp = (winmat[3][3] == 0.0f);
  for (int i = 0; i < 4; i++) {
    mul_project_m4_v3(invproj, view_vecs[i]);
    /* normalized trick see:
     * http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
    if (is_persp) {
      /* Divide XY by Z. */
      mul_v2_fl(view_vecs[i], 1.0f / view_vecs[i][2]);
    }
  }

  /**
   * If ortho : view_vecs[0] is the near-bottom-left corner of the frustum and
   *            view_vecs[1] is the vector going from the near-bottom-left corner to
   *            the far-top-right corner.
   * If Persp : view_vecs[0].xy and view_vecs[1].xy are respectively the bottom-left corner
   *            when Z = 1, and top-left corner if Z = 1.
   *            view_vecs[0].z the near clip distance and view_vecs[1].z is the (signed)
   *            distance from the near plane to the far clip plane.
   */
  copy_v4_v4(r_viewvecs[0], view_vecs[0]);

  /* we need to store the differences */
  r_viewvecs[1][0] = view_vecs[1][0] - view_vecs[0][0];
  r_viewvecs[1][1] = view_vecs[2][1] - view_vecs[0][1];
  r_viewvecs[1][2] = view_vecs[3][2] - view_vecs[0][2];
}

void EEVEE_materials_init(EEVEE_ViewLayerData *sldata,
                          EEVEE_StorageList *stl,
                          EEVEE_FramebufferList *fbl)
{
  if (!e_data.frag_shader_lib) {
    /* Shaders */
    e_data.frag_shader_lib = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                              datatoc_common_uniforms_lib_glsl,
                                              datatoc_bsdf_common_lib_glsl,
                                              datatoc_bsdf_sampling_lib_glsl,
                                              datatoc_ambient_occlusion_lib_glsl,
                                              datatoc_raytrace_lib_glsl,
                                              datatoc_ssr_lib_glsl,
                                              datatoc_octahedron_lib_glsl,
                                              datatoc_irradiance_lib_glsl,
                                              datatoc_lightprobe_lib_glsl,
                                              datatoc_ltc_lib_glsl,
                                              datatoc_lights_lib_glsl,
                                              /* Add one for each Closure */
                                              datatoc_lit_surface_frag_glsl,
                                              datatoc_lit_surface_frag_glsl,
                                              datatoc_lit_surface_frag_glsl,
                                              datatoc_lit_surface_frag_glsl,
                                              datatoc_lit_surface_frag_glsl,
                                              datatoc_lit_surface_frag_glsl,
                                              datatoc_lit_surface_frag_glsl,
                                              datatoc_lit_surface_frag_glsl,
                                              datatoc_lit_surface_frag_glsl,
                                              datatoc_lit_surface_frag_glsl,
                                              datatoc_lit_surface_frag_glsl,
                                              datatoc_volumetric_lib_glsl);

    e_data.volume_shader_lib = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                                datatoc_common_uniforms_lib_glsl,
                                                datatoc_bsdf_common_lib_glsl,
                                                datatoc_ambient_occlusion_lib_glsl,
                                                datatoc_octahedron_lib_glsl,
                                                datatoc_irradiance_lib_glsl,
                                                datatoc_lightprobe_lib_glsl,
                                                datatoc_ltc_lib_glsl,
                                                datatoc_lights_lib_glsl,
                                                datatoc_volumetric_lib_glsl,
                                                datatoc_volumetric_frag_glsl);

    e_data.vert_shader_str = BLI_string_joinN(
        datatoc_common_view_lib_glsl, datatoc_common_hair_lib_glsl, datatoc_lit_surface_vert_glsl);

    e_data.vert_shadow_shader_str = BLI_string_joinN(
        datatoc_common_view_lib_glsl, datatoc_common_hair_lib_glsl, datatoc_shadow_vert_glsl);

    e_data.default_background = DRW_shader_create(
        datatoc_background_vert_glsl, NULL, datatoc_default_world_frag_glsl, NULL);

    char *vert_str = BLI_string_joinN(
        datatoc_common_view_lib_glsl, datatoc_common_hair_lib_glsl, datatoc_prepass_vert_glsl);

    e_data.default_prepass_sh = DRW_shader_create(vert_str, NULL, datatoc_prepass_frag_glsl, NULL);

    e_data.default_prepass_clip_sh = DRW_shader_create(
        vert_str, NULL, datatoc_prepass_frag_glsl, "#define CLIP_PLANES\n");

    e_data.default_hair_prepass_sh = DRW_shader_create(
        vert_str, NULL, datatoc_prepass_frag_glsl, "#define HAIR_SHADER\n");

    e_data.default_hair_prepass_clip_sh = DRW_shader_create(vert_str,
                                                            NULL,
                                                            datatoc_prepass_frag_glsl,
                                                            "#define HAIR_SHADER\n"
                                                            "#define CLIP_PLANES\n");

    MEM_freeN(vert_str);

    e_data.update_noise_sh = DRW_shader_create_fullscreen(datatoc_update_noise_frag_glsl, NULL);

    eevee_init_util_texture();
    eevee_init_noise_texture();
    eevee_init_dummys();
  }

  if (!DRW_state_is_image_render() && ((stl->effects->enabled_effects & EFFECT_TAA) == 0)) {
    e_data.alpha_hash_offset = 0.0f;
    e_data.alpha_hash_scale = 1.0f;
  }
  else {
    double r;
    BLI_halton_1d(5, 0.0, stl->effects->taa_current_sample - 1, &r);
    e_data.alpha_hash_offset = (float)r;
    e_data.alpha_hash_scale = 0.01f;
  }

  {
    /* Update view_vecs */
    float invproj[4][4], winmat[4][4];
    DRW_viewport_matrix_get(winmat, DRW_MAT_WIN);
    DRW_viewport_matrix_get(invproj, DRW_MAT_WININV);

    EEVEE_update_viewvecs(invproj, winmat, sldata->common_data.view_vecs);
  }

  {
    /* Update noise Framebuffer. */
    GPU_framebuffer_ensure_config(
        &fbl->update_noise_fb,
        {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE_LAYER(e_data.util_tex, 2)});
  }
}

struct GPUMaterial *EEVEE_material_world_lightprobe_get(struct Scene *scene, World *wo)
{
  const void *engine = &DRW_engine_viewport_eevee_type;
  const int options = VAR_WORLD_PROBE;

  GPUMaterial *mat = DRW_shader_find_from_world(wo, engine, options, false);
  if (mat != NULL) {
    return mat;
  }
  return DRW_shader_create_from_world(scene,
                                      wo,
                                      engine,
                                      options,
                                      datatoc_background_vert_glsl,
                                      NULL,
                                      e_data.frag_shader_lib,
                                      SHADER_DEFINES "#define PROBE_CAPTURE\n",
                                      false);
}

struct GPUMaterial *EEVEE_material_world_background_get(struct Scene *scene, World *wo)
{
  const void *engine = &DRW_engine_viewport_eevee_type;
  int options = VAR_WORLD_BACKGROUND;

  GPUMaterial *mat = DRW_shader_find_from_world(wo, engine, options, true);
  if (mat != NULL) {
    return mat;
  }
  return DRW_shader_create_from_world(scene,
                                      wo,
                                      engine,
                                      options,
                                      datatoc_background_vert_glsl,
                                      NULL,
                                      e_data.frag_shader_lib,
                                      SHADER_DEFINES "#define WORLD_BACKGROUND\n",
                                      true);
}

struct GPUMaterial *EEVEE_material_world_volume_get(struct Scene *scene, World *wo)
{
  const void *engine = &DRW_engine_viewport_eevee_type;
  int options = VAR_WORLD_VOLUME;

  GPUMaterial *mat = DRW_shader_find_from_world(wo, engine, options, true);
  if (mat != NULL) {
    return mat;
  }

  char *defines = eevee_get_volume_defines(options);

  mat = DRW_shader_create_from_world(scene,
                                     wo,
                                     engine,
                                     options,
                                     datatoc_volumetric_vert_glsl,
                                     datatoc_volumetric_geom_glsl,
                                     e_data.volume_shader_lib,
                                     defines,
                                     true);

  MEM_freeN(defines);

  return mat;
}

struct GPUMaterial *EEVEE_material_mesh_get(struct Scene *scene,
                                            Material *ma,
                                            EEVEE_Data *vedata,
                                            bool use_blend,
                                            bool use_multiply,
                                            bool use_refract,
                                            bool use_sss,
                                            bool use_translucency,
                                            int shadow_method)
{
  EEVEE_EffectsInfo *effects = vedata->stl->effects;
  const void *engine = &DRW_engine_viewport_eevee_type;
  int options = VAR_MAT_MESH;

  if (use_blend) {
    options |= VAR_MAT_BLEND;
  }
  if (use_multiply) {
    options |= VAR_MAT_MULT;
  }
  if (use_refract) {
    options |= VAR_MAT_REFRACT;
  }
  if (use_sss) {
    options |= VAR_MAT_SSS;
  }
  if (use_sss && effects->sss_separate_albedo) {
    options |= VAR_MAT_SSSALBED;
  }
  if (use_translucency) {
    options |= VAR_MAT_TRANSLUC;
  }
  if (((effects->enabled_effects & EFFECT_VOLUMETRIC) != 0) && use_blend) {
    options |= VAR_MAT_VOLUME;
  }

  options |= eevee_material_shadow_option(shadow_method);

  GPUMaterial *mat = DRW_shader_find_from_material(ma, engine, options, true);
  if (mat) {
    return mat;
  }

  char *defines = eevee_get_defines(options);

  mat = DRW_shader_create_from_material(scene,
                                        ma,
                                        engine,
                                        options,
                                        e_data.vert_shader_str,
                                        NULL,
                                        e_data.frag_shader_lib,
                                        defines,
                                        true);

  MEM_freeN(defines);

  return mat;
}

struct GPUMaterial *EEVEE_material_mesh_volume_get(struct Scene *scene, Material *ma)
{
  const void *engine = &DRW_engine_viewport_eevee_type;
  int options = VAR_MAT_VOLUME;

  GPUMaterial *mat = DRW_shader_find_from_material(ma, engine, options, true);
  if (mat != NULL) {
    return mat;
  }

  char *defines = eevee_get_volume_defines(options);

  mat = DRW_shader_create_from_material(scene,
                                        ma,
                                        engine,
                                        options,
                                        datatoc_volumetric_vert_glsl,
                                        datatoc_volumetric_geom_glsl,
                                        e_data.volume_shader_lib,
                                        defines,
                                        true);

  MEM_freeN(defines);

  return mat;
}

struct GPUMaterial *EEVEE_material_mesh_depth_get(struct Scene *scene,
                                                  Material *ma,
                                                  bool use_hashed_alpha,
                                                  bool is_shadow)
{
  const void *engine = &DRW_engine_viewport_eevee_type;
  int options = VAR_MAT_MESH;

  if (use_hashed_alpha) {
    options |= VAR_MAT_HASH;
  }
  else {
    options |= VAR_MAT_CLIP;
  }

  if (is_shadow) {
    options |= VAR_MAT_SHADOW;
  }

  GPUMaterial *mat = DRW_shader_find_from_material(ma, engine, options, true);
  if (mat) {
    return mat;
  }

  char *defines = eevee_get_defines(options);

  char *frag_str = BLI_string_joinN(e_data.frag_shader_lib, datatoc_prepass_frag_glsl);

  mat = DRW_shader_create_from_material(scene,
                                        ma,
                                        engine,
                                        options,
                                        (is_shadow) ? e_data.vert_shadow_shader_str :
                                                      e_data.vert_shader_str,
                                        NULL,
                                        frag_str,
                                        defines,
                                        true);

  MEM_freeN(frag_str);
  MEM_freeN(defines);

  return mat;
}

struct GPUMaterial *EEVEE_material_hair_get(struct Scene *scene, Material *ma, int shadow_method)
{
  const void *engine = &DRW_engine_viewport_eevee_type;
  int options = VAR_MAT_MESH | VAR_MAT_HAIR;

  options |= eevee_material_shadow_option(shadow_method);

  GPUMaterial *mat = DRW_shader_find_from_material(ma, engine, options, true);
  if (mat) {
    return mat;
  }

  char *defines = eevee_get_defines(options);

  mat = DRW_shader_create_from_material(scene,
                                        ma,
                                        engine,
                                        options,
                                        e_data.vert_shader_str,
                                        NULL,
                                        e_data.frag_shader_lib,
                                        defines,
                                        true);

  MEM_freeN(defines);

  return mat;
}

/**
 * Create a default shading group inside the given pass.
 */
static struct DRWShadingGroup *EEVEE_default_shading_group_create(EEVEE_ViewLayerData *sldata,
                                                                  EEVEE_Data *vedata,
                                                                  DRWPass *pass,
                                                                  bool is_hair,
                                                                  bool use_blend,
                                                                  bool use_ssr,
                                                                  int shadow_method)
{
  EEVEE_EffectsInfo *effects = vedata->stl->effects;
  static int ssr_id;
  ssr_id = (use_ssr) ? 1 : -1;
  int options = VAR_MAT_MESH;

  if (is_hair) {
    options |= VAR_MAT_HAIR;
  }
  if (use_blend) {
    options |= VAR_MAT_BLEND;
  }
  if (((effects->enabled_effects & EFFECT_VOLUMETRIC) != 0) && use_blend) {
    options |= VAR_MAT_VOLUME;
  }

  options |= eevee_material_shadow_option(shadow_method);

  if (e_data.default_lit[options] == NULL) {
    create_default_shader(options);
  }

  DRWShadingGroup *shgrp = DRW_shgroup_create(e_data.default_lit[options], pass);
  add_standard_uniforms(shgrp, sldata, vedata, &ssr_id, NULL, true, true, false, false, use_blend);

  return shgrp;
}

/**
 * Create a default shading group inside the default pass without standard uniforms.
 */
static struct DRWShadingGroup *EEVEE_default_shading_group_get(EEVEE_ViewLayerData *sldata,
                                                               EEVEE_Data *vedata,
                                                               Object *ob,
                                                               ParticleSystem *psys,
                                                               ModifierData *md,
                                                               bool is_hair,
                                                               bool use_ssr,
                                                               int shadow_method)
{
  static int ssr_id;
  ssr_id = (use_ssr) ? 1 : -1;
  int options = VAR_MAT_MESH;

  BLI_assert(!is_hair || (ob && psys && md));

  if (is_hair) {
    options |= VAR_MAT_HAIR;
  }

  options |= eevee_material_shadow_option(shadow_method);

  if (e_data.default_lit[options] == NULL) {
    create_default_shader(options);
  }

  if (vedata->psl->default_pass[options] == NULL) {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_CLIP_PLANES |
                     DRW_STATE_WIRE;
    vedata->psl->default_pass[options] = DRW_pass_create("Default Lit Pass", state);

    /* XXX / WATCH: This creates non persistent binds for the ubos and textures.
     * But it's currently OK because the following shgroups does not add any bind.
     * EDIT: THIS IS NOT THE CASE FOR HAIRS !!! DUMMY!!! */
    if (!is_hair) {
      DRWShadingGroup *shgrp = DRW_shgroup_create(e_data.default_lit[options],
                                                  vedata->psl->default_pass[options]);
      add_standard_uniforms(shgrp, sldata, vedata, &ssr_id, NULL, true, true, false, false, false);
    }
  }

  if (is_hair) {
    DRWShadingGroup *shgrp = DRW_shgroup_hair_create(
        ob, psys, md, vedata->psl->default_pass[options], e_data.default_lit[options]);
    add_standard_uniforms(shgrp, sldata, vedata, &ssr_id, NULL, true, true, false, false, false);
    return shgrp;
  }
  else {
    return DRW_shgroup_create(e_data.default_lit[options], vedata->psl->default_pass[options]);
  }
}

void EEVEE_materials_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
  EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();

  /* Create Material Ghash */
  {
    stl->g_data->material_hash = BLI_ghash_ptr_new("Eevee_material ghash");
  }

  {
    psl->background_pass = DRW_pass_create("Background Pass",
                                           DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL);

    struct GPUBatch *geom = DRW_cache_fullscreen_quad_get();
    DRWShadingGroup *grp = NULL;

    Scene *scene = draw_ctx->scene;
    World *wo = scene->world;

    const float *col = G_draw.block.colorBackground;

    EEVEE_lookdev_cache_init(
        vedata, &grp, psl->background_pass, stl->g_data->background_alpha, wo, NULL);

    if (!grp && wo) {
      col = &wo->horr;

      if (wo->use_nodes && wo->nodetree) {
        static float error_col[3] = {1.0f, 0.0f, 1.0f};
        static float compile_col[3] = {0.5f, 0.5f, 0.5f};
        struct GPUMaterial *gpumat = EEVEE_material_world_background_get(scene, wo);

        switch (GPU_material_status(gpumat)) {
          case GPU_MAT_SUCCESS:
            grp = DRW_shgroup_material_create(gpumat, psl->background_pass);
            DRW_shgroup_uniform_float(grp, "backgroundAlpha", &stl->g_data->background_alpha, 1);
            /* TODO (fclem): remove those (need to clean the GLSL files). */
            DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
            DRW_shgroup_uniform_block(grp, "grid_block", sldata->grid_ubo);
            DRW_shgroup_uniform_block(grp, "probe_block", sldata->probe_ubo);
            DRW_shgroup_uniform_block(grp, "planar_block", sldata->planar_ubo);
            DRW_shgroup_uniform_block(grp, "light_block", sldata->light_ubo);
            DRW_shgroup_uniform_block(grp, "shadow_block", sldata->shadow_ubo);
            DRW_shgroup_call_add(grp, geom, NULL);
            break;
          case GPU_MAT_QUEUED:
            /* TODO Bypass probe compilation. */
            col = compile_col;
            break;
          case GPU_MAT_FAILED:
          default:
            col = error_col;
            break;
        }
      }
    }

    /* Fallback if shader fails or if not using nodetree. */
    if (grp == NULL) {
      grp = DRW_shgroup_create(e_data.default_background, psl->background_pass);
      DRW_shgroup_uniform_vec3(grp, "color", col, 1);
      DRW_shgroup_uniform_float(grp, "backgroundAlpha", &stl->g_data->background_alpha, 1);
      DRW_shgroup_call_add(grp, geom, NULL);
    }
  }

  {
    DRWState state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_WIRE;
    psl->depth_pass = DRW_pass_create("Depth Pass", state);
    stl->g_data->depth_shgrp = DRW_shgroup_create(e_data.default_prepass_sh, psl->depth_pass);

    state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CULL_BACK;
    psl->depth_pass_cull = DRW_pass_create("Depth Pass Cull", state);
    stl->g_data->depth_shgrp_cull = DRW_shgroup_create(e_data.default_prepass_sh,
                                                       psl->depth_pass_cull);

    state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CLIP_PLANES |
            DRW_STATE_WIRE;
    psl->depth_pass_clip = DRW_pass_create("Depth Pass Clip", state);
    stl->g_data->depth_shgrp_clip = DRW_shgroup_create(e_data.default_prepass_clip_sh,
                                                       psl->depth_pass_clip);
    DRW_shgroup_uniform_block(stl->g_data->depth_shgrp_clip, "clip_block", sldata->clip_ubo);

    state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CLIP_PLANES |
            DRW_STATE_CULL_BACK;
    psl->depth_pass_clip_cull = DRW_pass_create("Depth Pass Cull Clip", state);
    stl->g_data->depth_shgrp_clip_cull = DRW_shgroup_create(e_data.default_prepass_clip_sh,
                                                            psl->depth_pass_clip_cull);
    DRW_shgroup_uniform_block(stl->g_data->depth_shgrp_clip_cull, "clip_block", sldata->clip_ubo);
  }

  {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_CLIP_PLANES |
                     DRW_STATE_WIRE;
    psl->material_pass = DRW_pass_create("Material Pass", state);
    psl->material_pass_cull = DRW_pass_create("Material Pass Cull", state | DRW_STATE_CULL_BACK);
  }

  {
    DRWState state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_WIRE;
    psl->refract_depth_pass = DRW_pass_create("Refract Depth Pass", state);
    stl->g_data->refract_depth_shgrp = DRW_shgroup_create(e_data.default_prepass_sh,
                                                          psl->refract_depth_pass);

    state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CULL_BACK;
    psl->refract_depth_pass_cull = DRW_pass_create("Refract Depth Pass Cull", state);
    stl->g_data->refract_depth_shgrp_cull = DRW_shgroup_create(e_data.default_prepass_sh,
                                                               psl->refract_depth_pass_cull);

    state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CLIP_PLANES |
            DRW_STATE_WIRE;
    psl->refract_depth_pass_clip = DRW_pass_create("Refract Depth Pass Clip", state);
    stl->g_data->refract_depth_shgrp_clip = DRW_shgroup_create(e_data.default_prepass_clip_sh,
                                                               psl->refract_depth_pass_clip);
    DRW_shgroup_uniform_block(
        stl->g_data->refract_depth_shgrp_clip, "clip_block", sldata->clip_ubo);

    state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CLIP_PLANES |
            DRW_STATE_CULL_BACK;
    psl->refract_depth_pass_clip_cull = DRW_pass_create("Refract Depth Pass Cull Clip", state);
    stl->g_data->refract_depth_shgrp_clip_cull = DRW_shgroup_create(
        e_data.default_prepass_clip_sh, psl->refract_depth_pass_clip_cull);
    DRW_shgroup_uniform_block(
        stl->g_data->refract_depth_shgrp_clip_cull, "clip_block", sldata->clip_ubo);
  }

  {
    DRWState state = (DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_CLIP_PLANES |
                      DRW_STATE_WIRE);
    psl->refract_pass = DRW_pass_create("Opaque Refraction Pass", state);
  }

  {
    DRWState state = (DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_CLIP_PLANES |
                      DRW_STATE_WIRE | DRW_STATE_WRITE_STENCIL);
    psl->sss_pass = DRW_pass_create("Subsurface Pass", state);
    psl->sss_pass_cull = DRW_pass_create("Subsurface Pass Cull", state | DRW_STATE_CULL_BACK);
    e_data.sss_count = 0;
  }

  {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CLIP_PLANES |
                     DRW_STATE_WIRE;
    psl->transparent_pass = DRW_pass_create("Material Transparent Pass", state);
  }

  {
    psl->update_noise_pass = DRW_pass_create("Update Noise Pass", DRW_STATE_WRITE_COLOR);
    DRWShadingGroup *grp = DRW_shgroup_create(e_data.update_noise_sh, psl->update_noise_pass);
    DRW_shgroup_uniform_texture(grp, "blueNoise", e_data.noise_tex);
    DRW_shgroup_uniform_vec3(grp, "offsets", e_data.noise_offsets, 1);
    DRW_shgroup_call_add(grp, DRW_cache_fullscreen_quad_get(), NULL);
  }

  if (LOOK_DEV_OVERLAY_ENABLED(draw_ctx->v3d)) {
    DRWShadingGroup *shgrp;

    struct GPUBatch *sphere = DRW_cache_sphere_get();
    static float color_chrome[3] = {1.0f, 1.0f, 1.0f};
    static float color_diffuse[3] = {0.8f, 0.8f, 0.8f};
    int options = VAR_MAT_MESH | VAR_MAT_LOOKDEV;

    if (e_data.default_lit[options] == NULL) {
      create_default_shader(options);
    }

    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS |
                     DRW_STATE_CULL_BACK;

    psl->lookdev_diffuse_pass = DRW_pass_create("LookDev Diffuse Pass", state);
    shgrp = DRW_shgroup_create(e_data.default_lit[options], psl->lookdev_diffuse_pass);
    add_standard_uniforms(shgrp, sldata, vedata, NULL, NULL, true, true, false, false, false);
    DRW_shgroup_uniform_vec3(shgrp, "basecol", color_diffuse, 1);
    DRW_shgroup_uniform_float_copy(shgrp, "metallic", 0.0f);
    DRW_shgroup_uniform_float_copy(shgrp, "specular", 0.5f);
    DRW_shgroup_uniform_float_copy(shgrp, "roughness", 1.0f);
    DRW_shgroup_call_add(shgrp, sphere, NULL);

    psl->lookdev_glossy_pass = DRW_pass_create("LookDev Glossy Pass", state);
    shgrp = DRW_shgroup_create(e_data.default_lit[options], psl->lookdev_glossy_pass);
    add_standard_uniforms(shgrp, sldata, vedata, NULL, NULL, true, true, false, false, false);
    DRW_shgroup_uniform_vec3(shgrp, "basecol", color_chrome, 1);
    DRW_shgroup_uniform_float_copy(shgrp, "metallic", 1.0f);
    DRW_shgroup_uniform_float_copy(shgrp, "roughness", 0.0f);
    DRW_shgroup_call_add(shgrp, sphere, NULL);
  }
}

#define ADD_SHGROUP_CALL(shgrp, ob, geom, oedata) \
  do { \
    if (oedata) { \
      DRW_shgroup_call_object_add_with_callback( \
          shgrp, geom, ob, EEVEE_lightprobes_obj_visibility_cb, oedata); \
    } \
    else { \
      DRW_shgroup_call_object_add_ex(shgrp, geom, ob, false); \
    } \
  } while (0)

#define ADD_SHGROUP_CALL_SAFE(shgrp, ob, geom, oedata) \
  do { \
    if (shgrp) { \
      ADD_SHGROUP_CALL(shgrp, ob, geom, oedata); \
    } \
  } while (0)

typedef struct EeveeMaterialShadingGroups {
  struct DRWShadingGroup *shading_grp;
  struct DRWShadingGroup *depth_grp;
  struct DRWShadingGroup *depth_clip_grp;
} EeveeMaterialShadingGroups;

static void material_opaque(Material *ma,
                            GHash *material_hash,
                            EEVEE_ViewLayerData *sldata,
                            EEVEE_Data *vedata,
                            bool do_cull,
                            struct GPUMaterial **gpumat,
                            struct GPUMaterial **gpumat_depth,
                            struct DRWShadingGroup **shgrp,
                            struct DRWShadingGroup **shgrp_depth,
                            struct DRWShadingGroup **shgrp_depth_clip)
{
  EEVEE_EffectsInfo *effects = vedata->stl->effects;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
  EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
  EEVEE_LightsInfo *linfo = sldata->lights;
  bool use_diffuse, use_glossy, use_refract;

  float *color_p = &ma->r;
  float *metal_p = &ma->metallic;
  float *spec_p = &ma->spec;
  float *rough_p = &ma->roughness;

  const bool use_gpumat = (ma->use_nodes && ma->nodetree);
  const bool use_ssrefract = ((ma->blend_flag & MA_BL_SS_REFRACTION) != 0) &&
                             ((effects->enabled_effects & EFFECT_REFRACT) != 0);
  bool use_sss = ((effects->enabled_effects & EFFECT_SSS) != 0);
  const bool use_translucency = use_sss && ((ma->blend_flag & MA_BL_TRANSLUCENCY) != 0);

  EeveeMaterialShadingGroups *emsg = BLI_ghash_lookup(material_hash, (const void *)ma);

  if (emsg) {
    *shgrp = emsg->shading_grp;
    *shgrp_depth = emsg->depth_grp;
    *shgrp_depth_clip = emsg->depth_clip_grp;

    /* This will have been created already, just perform a lookup. */
    *gpumat = (use_gpumat) ? EEVEE_material_mesh_get(scene,
                                                     ma,
                                                     vedata,
                                                     false,
                                                     false,
                                                     use_ssrefract,
                                                     use_sss,
                                                     use_translucency,
                                                     linfo->shadow_method) :
                             NULL;
    *gpumat_depth = (use_gpumat) ? EEVEE_material_mesh_depth_get(
                                       scene, ma, (ma->blend_method == MA_BM_HASHED), false) :
                                   NULL;
    return;
  }

  if (use_gpumat) {
    static float error_col[3] = {1.0f, 0.0f, 1.0f};
    static float compile_col[3] = {0.5f, 0.5f, 0.5f};
    static float half = 0.5f;

    /* Shading */
    *gpumat = EEVEE_material_mesh_get(scene,
                                      ma,
                                      vedata,
                                      false,
                                      false,
                                      use_ssrefract,
                                      use_sss,
                                      use_translucency,
                                      linfo->shadow_method);

    eGPUMaterialStatus status_mat_surface = GPU_material_status(*gpumat);

    /* Alpha CLipped : Discard pixel from depth pass, then
     * fail the depth test for shading. */
    if (ELEM(ma->blend_method, MA_BM_CLIP, MA_BM_HASHED)) {
      *gpumat_depth = EEVEE_material_mesh_depth_get(
          scene, ma, (ma->blend_method == MA_BM_HASHED), false);

      eGPUMaterialStatus status_mat_depth = GPU_material_status(*gpumat_depth);
      if (status_mat_depth != GPU_MAT_SUCCESS) {
        /* Mixing both flags. If depth shader fails, show it to the user by not using
         * the surface shader. */
        status_mat_surface = status_mat_depth;
      }
      else if (use_ssrefract) {
        *shgrp_depth = DRW_shgroup_material_create(
            *gpumat_depth, (do_cull) ? psl->refract_depth_pass_cull : psl->refract_depth_pass);
        *shgrp_depth_clip = DRW_shgroup_material_create(
            *gpumat_depth,
            (do_cull) ? psl->refract_depth_pass_clip_cull : psl->refract_depth_pass_clip);
      }
      else {
        *shgrp_depth = DRW_shgroup_material_create(
            *gpumat_depth, (do_cull) ? psl->depth_pass_cull : psl->depth_pass);
        *shgrp_depth_clip = DRW_shgroup_material_create(
            *gpumat_depth, (do_cull) ? psl->depth_pass_clip_cull : psl->depth_pass_clip);
      }

      if (*shgrp_depth != NULL) {
        use_diffuse = GPU_material_flag_get(*gpumat_depth, GPU_MATFLAG_DIFFUSE);
        use_glossy = GPU_material_flag_get(*gpumat_depth, GPU_MATFLAG_GLOSSY);
        use_refract = GPU_material_flag_get(*gpumat_depth, GPU_MATFLAG_REFRACT);

        add_standard_uniforms(*shgrp_depth,
                              sldata,
                              vedata,
                              NULL,
                              NULL,
                              use_diffuse,
                              use_glossy,
                              use_refract,
                              false,
                              false);
        add_standard_uniforms(*shgrp_depth_clip,
                              sldata,
                              vedata,
                              NULL,
                              NULL,
                              use_diffuse,
                              use_glossy,
                              use_refract,
                              false,
                              false);

        if (ma->blend_method == MA_BM_CLIP) {
          DRW_shgroup_uniform_float(*shgrp_depth, "alphaThreshold", &ma->alpha_threshold, 1);
          DRW_shgroup_uniform_float(*shgrp_depth_clip, "alphaThreshold", &ma->alpha_threshold, 1);
        }
        else if (ma->blend_method == MA_BM_HASHED) {
          DRW_shgroup_uniform_float(*shgrp_depth, "hashAlphaOffset", &e_data.alpha_hash_offset, 1);
          DRW_shgroup_uniform_float(
              *shgrp_depth_clip, "hashAlphaOffset", &e_data.alpha_hash_offset, 1);
          DRW_shgroup_uniform_float_copy(*shgrp_depth, "hashAlphaScale", e_data.alpha_hash_scale);
          DRW_shgroup_uniform_float_copy(
              *shgrp_depth_clip, "hashAlphaScale", e_data.alpha_hash_scale);
        }
      }
    }

    switch (status_mat_surface) {
      case GPU_MAT_SUCCESS: {
        static int no_ssr = -1;
        static int first_ssr = 1;
        int *ssr_id = (((effects->enabled_effects & EFFECT_SSR) != 0) && !use_ssrefract) ?
                          &first_ssr :
                          &no_ssr;
        use_diffuse = GPU_material_flag_get(*gpumat, GPU_MATFLAG_DIFFUSE);
        use_glossy = GPU_material_flag_get(*gpumat, GPU_MATFLAG_GLOSSY);
        use_refract = GPU_material_flag_get(*gpumat, GPU_MATFLAG_REFRACT);
        use_sss = use_sss && GPU_material_flag_get(*gpumat, GPU_MATFLAG_SSS);

        *shgrp = DRW_shgroup_material_create(
            *gpumat,
            (use_ssrefract) ?
                psl->refract_pass :
                (use_sss) ? ((do_cull) ? psl->sss_pass_cull : psl->sss_pass) :
                            ((do_cull) ? psl->material_pass_cull : psl->material_pass));

        add_standard_uniforms(*shgrp,
                              sldata,
                              vedata,
                              ssr_id,
                              &ma->refract_depth,
                              use_diffuse,
                              use_glossy,
                              use_refract,
                              use_ssrefract,
                              false);

        if (use_sss) {
          struct GPUTexture *sss_tex_profile = NULL;
          struct GPUUniformBuffer *sss_profile = GPU_material_sss_profile_get(
              *gpumat, stl->effects->sss_sample_count, &sss_tex_profile);

          if (sss_profile) {
            if (use_translucency) {
              DRW_shgroup_uniform_block(*shgrp, "sssProfile", sss_profile);
              DRW_shgroup_uniform_texture(*shgrp, "sssTexProfile", sss_tex_profile);
            }

            /* Limit of 8 bit stencil buffer. ID 255 is refraction. */
            if (e_data.sss_count < 254) {
              DRW_shgroup_stencil_mask(*shgrp, e_data.sss_count + 1);
              EEVEE_subsurface_add_pass(sldata, vedata, e_data.sss_count + 1, sss_profile);
              e_data.sss_count++;
            }
            else {
              /* TODO : display message. */
              printf("Error: Too many different Subsurface shader in the scene.\n");
            }
          }
          else {
            if (use_translucency) {
              /* NOTE: This is a nasty workaround, because the sss profile might not have been
               * generated but the UBO is still declared in this case even if not used.
               * But rendering without a bound UBO might result in crashes on certain platform. */
              DRW_shgroup_uniform_block(*shgrp, "sssProfile", e_data.dummy_sss_profile);
            }
          }
        }
        else {
          if (use_translucency) {
            DRW_shgroup_uniform_block(*shgrp, "sssProfile", e_data.dummy_sss_profile);
          }
        }
        break;
      }
      case GPU_MAT_QUEUED: {
        color_p = compile_col;
        metal_p = spec_p = rough_p = &half;
        break;
      }
      case GPU_MAT_FAILED:
      default:
        color_p = error_col;
        metal_p = spec_p = rough_p = &half;
        break;
    }
  }

  /* Fallback to default shader */
  if (*shgrp == NULL) {
    bool use_ssr = ((effects->enabled_effects & EFFECT_SSR) != 0);
    *shgrp = EEVEE_default_shading_group_get(
        sldata, vedata, NULL, NULL, NULL, false, use_ssr, linfo->shadow_method);
    DRW_shgroup_uniform_vec3(*shgrp, "basecol", color_p, 1);
    DRW_shgroup_uniform_float(*shgrp, "metallic", metal_p, 1);
    DRW_shgroup_uniform_float(*shgrp, "specular", spec_p, 1);
    DRW_shgroup_uniform_float(*shgrp, "roughness", rough_p, 1);
  }

  /* Fallback default depth prepass */
  if (*shgrp_depth == NULL) {
    if (use_ssrefract) {
      *shgrp_depth = (do_cull) ? stl->g_data->refract_depth_shgrp_cull :
                                 stl->g_data->refract_depth_shgrp;
      *shgrp_depth_clip = (do_cull) ? stl->g_data->refract_depth_shgrp_clip_cull :
                                      stl->g_data->refract_depth_shgrp_clip;
    }
    else {
      *shgrp_depth = (do_cull) ? stl->g_data->depth_shgrp_cull : stl->g_data->depth_shgrp;
      *shgrp_depth_clip = (do_cull) ? stl->g_data->depth_shgrp_clip_cull :
                                      stl->g_data->depth_shgrp_clip;
    }
  }

  emsg = MEM_mallocN(sizeof(EeveeMaterialShadingGroups), "EeveeMaterialShadingGroups");
  emsg->shading_grp = *shgrp;
  emsg->depth_grp = *shgrp_depth;
  emsg->depth_clip_grp = *shgrp_depth_clip;
  BLI_ghash_insert(material_hash, ma, emsg);
}

static void material_transparent(Material *ma,
                                 EEVEE_ViewLayerData *sldata,
                                 EEVEE_Data *vedata,
                                 bool do_cull,
                                 struct GPUMaterial **gpumat,
                                 struct DRWShadingGroup **shgrp,
                                 struct DRWShadingGroup **shgrp_depth)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
  EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
  EEVEE_LightsInfo *linfo = sldata->lights;

  const bool use_ssrefract = (((ma->blend_flag & MA_BL_SS_REFRACTION) != 0) &&
                              ((stl->effects->enabled_effects & EFFECT_REFRACT) != 0));
  float *color_p = &ma->r;
  float *metal_p = &ma->metallic;
  float *spec_p = &ma->spec;
  float *rough_p = &ma->roughness;

  if (ma->use_nodes && ma->nodetree) {
    static float error_col[3] = {1.0f, 0.0f, 1.0f};
    static float compile_col[3] = {0.5f, 0.5f, 0.5f};
    static float half = 0.5f;

    /* Shading */
    *gpumat = EEVEE_material_mesh_get(scene,
                                      ma,
                                      vedata,
                                      true,
                                      (ma->blend_method == MA_BM_MULTIPLY),
                                      use_ssrefract,
                                      false,
                                      false,
                                      linfo->shadow_method);

    switch (GPU_material_status(*gpumat)) {
      case GPU_MAT_SUCCESS: {
        static int ssr_id = -1; /* TODO transparent SSR */
        bool use_blend = (ma->blend_method & MA_BM_BLEND) != 0;

        *shgrp = DRW_shgroup_material_create(*gpumat, psl->transparent_pass);

        bool use_diffuse = GPU_material_flag_get(*gpumat, GPU_MATFLAG_DIFFUSE);
        bool use_glossy = GPU_material_flag_get(*gpumat, GPU_MATFLAG_GLOSSY);
        bool use_refract = GPU_material_flag_get(*gpumat, GPU_MATFLAG_REFRACT);

        add_standard_uniforms(*shgrp,
                              sldata,
                              vedata,
                              &ssr_id,
                              &ma->refract_depth,
                              use_diffuse,
                              use_glossy,
                              use_refract,
                              use_ssrefract,
                              use_blend);
        break;
      }
      case GPU_MAT_QUEUED: {
        /* TODO Bypass probe compilation. */
        color_p = compile_col;
        metal_p = spec_p = rough_p = &half;
        break;
      }
      case GPU_MAT_FAILED:
      default:
        color_p = error_col;
        metal_p = spec_p = rough_p = &half;
        break;
    }
  }

  /* Fallback to default shader */
  if (*shgrp == NULL) {
    *shgrp = EEVEE_default_shading_group_create(
        sldata, vedata, psl->transparent_pass, false, true, false, linfo->shadow_method);
    DRW_shgroup_uniform_vec3(*shgrp, "basecol", color_p, 1);
    DRW_shgroup_uniform_float(*shgrp, "metallic", metal_p, 1);
    DRW_shgroup_uniform_float(*shgrp, "specular", spec_p, 1);
    DRW_shgroup_uniform_float(*shgrp, "roughness", rough_p, 1);
  }

  const bool use_prepass = ((ma->blend_flag & MA_BL_HIDE_BACKFACE) != 0);

  DRWState all_state = (DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR | DRW_STATE_CULL_BACK |
                        DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND |
                        DRW_STATE_ADDITIVE | DRW_STATE_MULTIPLY);

  DRWState cur_state = DRW_STATE_WRITE_COLOR;
  cur_state |= (use_prepass) ? DRW_STATE_DEPTH_EQUAL : DRW_STATE_DEPTH_LESS_EQUAL;
  cur_state |= (do_cull) ? DRW_STATE_CULL_BACK : 0;

  switch (ma->blend_method) {
    case MA_BM_ADD:
      cur_state |= DRW_STATE_ADDITIVE;
      break;
    case MA_BM_MULTIPLY:
      cur_state |= DRW_STATE_MULTIPLY;
      break;
    case MA_BM_BLEND:
      cur_state |= DRW_STATE_BLEND;
      break;
    default:
      BLI_assert(0);
      break;
  }

  /* Disable other blend modes and use the one we want. */
  DRW_shgroup_state_disable(*shgrp, all_state);
  DRW_shgroup_state_enable(*shgrp, cur_state);

  /* Depth prepass */
  if (use_prepass) {
    *shgrp_depth = DRW_shgroup_create(e_data.default_prepass_clip_sh, psl->transparent_pass);
    DRW_shgroup_uniform_block(*shgrp_depth, "clip_block", sldata->clip_ubo);

    cur_state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    cur_state |= (do_cull) ? DRW_STATE_CULL_BACK : 0;

    DRW_shgroup_state_disable(*shgrp_depth, all_state);
    DRW_shgroup_state_enable(*shgrp_depth, cur_state);
  }
}

/* Return correct material or &defmaterial if slot is empty. */
BLI_INLINE Material *eevee_object_material_get(Object *ob, int slot)
{
  Material *ma = give_current_material(ob, slot + 1);
  if (ma == NULL) {
    ma = &defmaterial;
  }
  return ma;
}

void EEVEE_materials_cache_populate(EEVEE_Data *vedata,
                                    EEVEE_ViewLayerData *sldata,
                                    Object *ob,
                                    bool *cast_shadow)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  GHash *material_hash = stl->g_data->material_hash;

  const bool do_cull = (draw_ctx->v3d &&
                        (draw_ctx->v3d->shading.flag & V3D_SHADING_BACKFACE_CULLING));
  bool is_sculpt_mode = DRW_object_use_pbvh_drawing(ob);
  /* For now just force fully shaded with eevee when supported. */
  is_sculpt_mode = is_sculpt_mode &&
                   !(ob->sculpt->pbvh && BKE_pbvh_type(ob->sculpt->pbvh) == PBVH_FACES);

  /* First get materials for this mesh. */
  if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL)) {
    const int materials_len = MAX2(1, ob->totcol);

    struct DRWShadingGroup **shgrp_array = BLI_array_alloca(shgrp_array, materials_len);
    struct DRWShadingGroup **shgrp_depth_array = BLI_array_alloca(shgrp_depth_array,
                                                                  materials_len);
    struct DRWShadingGroup **shgrp_depth_clip_array = BLI_array_alloca(shgrp_depth_clip_array,
                                                                       materials_len);

    struct GPUMaterial **gpumat_array = BLI_array_alloca(gpumat_array, materials_len);
    struct GPUMaterial **gpumat_depth_array = BLI_array_alloca(gpumat_array, materials_len);
    struct Material **ma_array = BLI_array_alloca(ma_array, materials_len);

    for (int i = 0; i < materials_len; ++i) {
      ma_array[i] = eevee_object_material_get(ob, i);
      gpumat_array[i] = NULL;
      gpumat_depth_array[i] = NULL;
      shgrp_array[i] = NULL;
      shgrp_depth_array[i] = NULL;
      shgrp_depth_clip_array[i] = NULL;

      switch (ma_array[i]->blend_method) {
        case MA_BM_SOLID:
        case MA_BM_CLIP:
        case MA_BM_HASHED:
          material_opaque(ma_array[i],
                          material_hash,
                          sldata,
                          vedata,
                          do_cull,
                          &gpumat_array[i],
                          &gpumat_depth_array[i],
                          &shgrp_array[i],
                          &shgrp_depth_array[i],
                          &shgrp_depth_clip_array[i]);
          break;
        case MA_BM_ADD:
        case MA_BM_MULTIPLY:
        case MA_BM_BLEND:
          material_transparent(ma_array[i],
                               sldata,
                               vedata,
                               do_cull,
                               &gpumat_array[i],
                               &shgrp_array[i],
                               &shgrp_depth_array[i]);
          break;
        default:
          BLI_assert(0);
          break;
      }
    }

    /* Only support single volume material for now. */
    /* XXX We rely on the previously compiled surface shader
     * to know if the material has a "volume nodetree".
     */
    bool use_volume_material = (gpumat_array[0] &&
                                GPU_material_use_domain_volume(gpumat_array[0]));

    if ((ob->dt >= OB_SOLID) || DRW_state_is_image_render()) {
      /* Get per-material split surface */
      char *auto_layer_names;
      int *auto_layer_is_srgb;
      int auto_layer_count;
      struct GPUBatch **mat_geom = NULL;

      if (!is_sculpt_mode) {
        mat_geom = DRW_cache_object_surface_material_get(ob,
                                                         gpumat_array,
                                                         materials_len,
                                                         &auto_layer_names,
                                                         &auto_layer_is_srgb,
                                                         &auto_layer_count);
      }

      if (is_sculpt_mode) {
        /* Vcol is not supported in the modes that require PBVH drawing. */
        bool use_vcol = false;
        DRW_shgroup_call_sculpt_with_materials_add(shgrp_array, ob, use_vcol);
        DRW_shgroup_call_sculpt_with_materials_add(shgrp_depth_array, ob, use_vcol);
        DRW_shgroup_call_sculpt_with_materials_add(shgrp_depth_clip_array, ob, use_vcol);
        /* TODO(fclem): Support shadows in sculpt mode. */
      }
      else if (mat_geom) {
        for (int i = 0; i < materials_len; ++i) {
          if (mat_geom[i] == NULL) {
            continue;
          }

          /* Do not render surface if we are rendering a volume object
           * and do not have a surface closure. */
          if (use_volume_material &&
              (gpumat_array[i] && !GPU_material_use_domain_surface(gpumat_array[i]))) {
            continue;
          }

          /* XXX TODO rewrite this to include the dupli objects.
           * This means we cannot exclude dupli objects from reflections!!! */
          EEVEE_ObjectEngineData *oedata = NULL;
          if ((ob->base_flag & BASE_FROM_DUPLI) == 0) {
            oedata = EEVEE_object_data_ensure(ob);
            oedata->ob = ob;
            oedata->test_data = &sldata->probes->vis_data;
          }

          ADD_SHGROUP_CALL(shgrp_array[i], ob, mat_geom[i], oedata);
          ADD_SHGROUP_CALL_SAFE(shgrp_depth_array[i], ob, mat_geom[i], oedata);
          ADD_SHGROUP_CALL_SAFE(shgrp_depth_clip_array[i], ob, mat_geom[i], oedata);

          char *name = auto_layer_names;
          for (int j = 0; j < auto_layer_count; ++j) {
            /* TODO don't add these uniform when not needed (default pass shaders). */
            if (shgrp_array[i]) {
              DRW_shgroup_uniform_bool(shgrp_array[i], name, &auto_layer_is_srgb[j], 1);
            }
            if (shgrp_depth_array[i]) {
              DRW_shgroup_uniform_bool(shgrp_depth_array[i], name, &auto_layer_is_srgb[j], 1);
            }
            if (shgrp_depth_clip_array[i]) {
              DRW_shgroup_uniform_bool(shgrp_depth_clip_array[i], name, &auto_layer_is_srgb[j], 1);
            }
            /* Go to next layer name. */
            while (*name != '\0') {
              name++;
            }
            name += 1;
          }

          /* Shadow Pass */
          struct GPUMaterial *gpumat;
          switch (ma_array[i]->blend_shadow) {
            case MA_BS_SOLID:
              EEVEE_lights_cache_shcaster_add(sldata, stl, mat_geom[i], ob);
              *cast_shadow = true;
              break;
            case MA_BS_CLIP:
              gpumat = EEVEE_material_mesh_depth_get(scene, ma_array[i], false, true);
              EEVEE_lights_cache_shcaster_material_add(
                  sldata, psl, gpumat, mat_geom[i], ob, &ma_array[i]->alpha_threshold);
              *cast_shadow = true;
              break;
            case MA_BS_HASHED:
              gpumat = EEVEE_material_mesh_depth_get(scene, ma_array[i], true, true);
              EEVEE_lights_cache_shcaster_material_add(sldata, psl, gpumat, mat_geom[i], ob, NULL);
              *cast_shadow = true;
              break;
            case MA_BS_NONE:
            default:
              break;
          }
        }
      }
    }

    /* Volumetrics */
    if (((stl->effects->enabled_effects & EFFECT_VOLUMETRIC) != 0) && use_volume_material) {
      EEVEE_volumes_cache_object_add(sldata, vedata, scene, ob);
    }
  }
}

void EEVEE_hair_cache_populate(EEVEE_Data *vedata,
                               EEVEE_ViewLayerData *sldata,
                               Object *ob,
                               bool *cast_shadow)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;

  bool use_ssr = ((stl->effects->enabled_effects & EFFECT_SSR) != 0);

  if (ob->type == OB_MESH) {
    if (ob != draw_ctx->object_edit) {
      for (ModifierData *md = ob->modifiers.first; md; md = md->next) {
        if (md->type != eModifierType_ParticleSystem) {
          continue;
        }
        ParticleSystem *psys = ((ParticleSystemModifierData *)md)->psys;
        if (!DRW_object_is_visible_psys_in_active_context(ob, psys)) {
          continue;
        }
        ParticleSettings *part = psys->part;
        const int draw_as = (part->draw_as == PART_DRAW_REND) ? part->ren_as : part->draw_as;
        if (draw_as != PART_DRAW_PATH) {
          continue;
        }

        DRWShadingGroup *shgrp = NULL;
        Material *ma = eevee_object_material_get(ob, part->omat - 1);

        float *color_p = &ma->r;
        float *metal_p = &ma->metallic;
        float *spec_p = &ma->spec;
        float *rough_p = &ma->roughness;

        shgrp = DRW_shgroup_hair_create(
            ob, psys, md, psl->depth_pass, e_data.default_hair_prepass_sh);

        shgrp = DRW_shgroup_hair_create(
            ob, psys, md, psl->depth_pass_clip, e_data.default_hair_prepass_clip_sh);
        DRW_shgroup_uniform_block(shgrp, "clip_block", sldata->clip_ubo);

        shgrp = NULL;
        if (ma->use_nodes && ma->nodetree) {
          static int ssr_id;
          ssr_id = (use_ssr) ? 1 : -1;
          static float half = 0.5f;
          static float error_col[3] = {1.0f, 0.0f, 1.0f};
          static float compile_col[3] = {0.5f, 0.5f, 0.5f};
          struct GPUMaterial *gpumat = EEVEE_material_hair_get(
              scene, ma, sldata->lights->shadow_method);

          switch (GPU_material_status(gpumat)) {
            case GPU_MAT_SUCCESS: {
              bool use_diffuse = GPU_material_flag_get(gpumat, GPU_MATFLAG_DIFFUSE);
              bool use_glossy = GPU_material_flag_get(gpumat, GPU_MATFLAG_GLOSSY);
              bool use_refract = GPU_material_flag_get(gpumat, GPU_MATFLAG_REFRACT);

              shgrp = DRW_shgroup_material_hair_create(ob, psys, md, psl->material_pass, gpumat);

              if (!use_diffuse && !use_glossy && !use_refract) {
                /* FIXME: Small hack to avoid issue when utilTex is needed for
                 * world_normals_get and none of the bsdfs that need it are present.
                 * This can try to bind utilTex even if not needed. */
                DRW_shgroup_uniform_texture(shgrp, "utilTex", e_data.util_tex);
              }

              add_standard_uniforms(shgrp,
                                    sldata,
                                    vedata,
                                    &ssr_id,
                                    NULL,
                                    use_diffuse,
                                    use_glossy,
                                    use_refract,
                                    false,
                                    false);
              break;
            }
            case GPU_MAT_QUEUED: {
              color_p = compile_col;
              metal_p = spec_p = rough_p = &half;
              break;
            }
            case GPU_MAT_FAILED:
            default:
              color_p = error_col;
              metal_p = spec_p = rough_p = &half;
              break;
          }
        }

        /* Fallback to default shader */
        if (shgrp == NULL) {
          shgrp = EEVEE_default_shading_group_get(
              sldata, vedata, ob, psys, md, true, use_ssr, sldata->lights->shadow_method);
          DRW_shgroup_uniform_vec3(shgrp, "basecol", color_p, 1);
          DRW_shgroup_uniform_float(shgrp, "metallic", metal_p, 1);
          DRW_shgroup_uniform_float(shgrp, "specular", spec_p, 1);
          DRW_shgroup_uniform_float(shgrp, "roughness", rough_p, 1);
        }

        /* Shadows */
        DRW_shgroup_hair_create(ob, psys, md, psl->shadow_pass, e_data.default_hair_prepass_sh);
        *cast_shadow = true;
      }
    }
  }
}

void EEVEE_materials_cache_finish(EEVEE_Data *vedata)
{
  EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;

  BLI_ghash_free(stl->g_data->material_hash, NULL, MEM_freeN);
}

void EEVEE_materials_free(void)
{
  for (int i = 0; i < VAR_MAT_MAX; ++i) {
    DRW_SHADER_FREE_SAFE(e_data.default_lit[i]);
  }
  MEM_SAFE_FREE(e_data.frag_shader_lib);
  MEM_SAFE_FREE(e_data.vert_shader_str);
  MEM_SAFE_FREE(e_data.vert_shadow_shader_str);
  MEM_SAFE_FREE(e_data.volume_shader_lib);
  DRW_SHADER_FREE_SAFE(e_data.default_hair_prepass_sh);
  DRW_SHADER_FREE_SAFE(e_data.default_hair_prepass_clip_sh);
  DRW_SHADER_FREE_SAFE(e_data.default_prepass_sh);
  DRW_SHADER_FREE_SAFE(e_data.default_prepass_clip_sh);
  DRW_SHADER_FREE_SAFE(e_data.default_background);
  DRW_SHADER_FREE_SAFE(e_data.update_noise_sh);
  DRW_TEXTURE_FREE_SAFE(e_data.util_tex);
  DRW_TEXTURE_FREE_SAFE(e_data.noise_tex);
  DRW_UBO_FREE_SAFE(e_data.dummy_sss_profile);
}

void EEVEE_draw_default_passes(EEVEE_PassList *psl)
{
  for (int i = 0; i < VAR_MAT_MAX; ++i) {
    if (psl->default_pass[i]) {
      DRW_draw_pass(psl->default_pass[i]);
    }
  }
}
