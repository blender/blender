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

#include "BLI_alloca.h"
#include "BLI_dynstr.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math_bits.h"
#include "BLI_rand.h"
#include "BLI_string_utils.h"

#include "BKE_paint.h"
#include "BKE_particle.h"

#include "DNA_hair_types.h"
#include "DNA_modifier_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

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
  char *vert_background_shader_str;
  char *vert_volume_shader_str;
  char *geom_volume_shader_str;
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

  uint sss_count;

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
extern char datatoc_cubemap_lib_glsl[];
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

#define DEFAULT_RENDER_PASS_FLAG 0xefffffff

/* Iterator for render passes. This iteration will only do the material based render passes. it
 * will ignore `EEVEE_RENDER_PASS_ENVIRONMENT`.
 *
 * parameters:
 * - `render_passes_` is a bitflag for render_passes that needs to be iterated over.
 * - `render_pass_index_` is a parameter name where the index of the render_pass will be available
 *   during iteration. This index can be used to select the right pass in the `psl`.
 * - `render_pass_` is the bitflag of the render_pass of the current iteration.
 *
 * The `render_pass_index_` parameter needs to be the same for the `RENDER_PASS_ITER_BEGIN` and
 * `RENDER_PASS_ITER_END`.
 */
#define RENDER_PASS_ITER_BEGIN(render_passes_, render_pass_index_, render_pass_) \
  const eViewLayerEEVEEPassType __filtered_##render_pass_index_ = render_passes_ & \
                                                                  EEVEE_RENDERPASSES_MATERIAL & \
                                                                  ~EEVEE_RENDER_PASS_ENVIRONMENT; \
  if (__filtered_##render_pass_index_ != 0) { \
    int render_pass_index_ = 1; \
    for (int bit_##render_pass_ = 0; bit_##render_pass_ < EEVEE_RENDER_PASS_MAX_BIT; \
         bit_##render_pass_++) { \
      eViewLayerEEVEEPassType render_pass_ = (1 << bit_##render_pass_); \
      if ((__filtered_##render_pass_index_ & render_pass_) != 0) {
#define RENDER_PASS_ITER_END(render_pass_index_) \
  render_pass_index_ += 1; \
  } \
  } \
  } \
  ((void)0)

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
  DRW_shgroup_call(grp, geom, NULL);

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
  DRW_shgroup_call(grp, geom, NULL);

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
      if (((i / 3) + 1) % 12 == 0) {
        fprintf(f, "\n\t\t");
      }
      else {
        fprintf(f, " ");
      }
    }
    fprintf(f, "\n\t},\n");
#  else
    for (int i = 0; i < w * h * 3; i += 3) {
      if (data[i] < 0.01) {
        printf(" ");
      }
      else if (data[i] < 0.3) {
        printf(".");
      }
      else if (data[i] < 0.6) {
        printf("+");
      }
      else if (data[i] < 0.9) {
        printf("%%");
      }
      else {
        printf("#");
      }
      if ((i / 3 + 1) % 64 == 0) {
        printf("\n");
      }
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

/* Get the default render pass ubo. This is a ubo that enables all bsdf render passes. */
struct GPUUniformBuffer *EEVEE_material_default_render_pass_ubo_get(EEVEE_ViewLayerData *sldata)
{
  return sldata->renderpass_ubo[0];
}

/* Get the render pass ubo for rendering the given render_pass. */
static struct GPUUniformBuffer *get_render_pass_ubo(EEVEE_ViewLayerData *sldata,
                                                    eViewLayerEEVEEPassType render_pass)
{
  int index;
  switch (render_pass) {
    case EEVEE_RENDER_PASS_DIFFUSE_COLOR:
      index = 1;
      break;
    case EEVEE_RENDER_PASS_DIFFUSE_LIGHT:
      index = 2;
      break;
    case EEVEE_RENDER_PASS_SPECULAR_COLOR:
      index = 3;
      break;
    case EEVEE_RENDER_PASS_SPECULAR_LIGHT:
      index = 4;
      break;
    case EEVEE_RENDER_PASS_EMIT:
      index = 5;
      break;
    default:
      index = 0;
      break;
  }
  return sldata->renderpass_ubo[index];
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
                                  bool use_alpha_blend,
                                  eViewLayerEEVEEPassType render_pass)
{
  LightCache *lcache = vedata->stl->g_data->light_cache;
  EEVEE_EffectsInfo *effects = vedata->stl->effects;

  DRW_shgroup_uniform_block(shgrp, "probe_block", sldata->probe_ubo);
  DRW_shgroup_uniform_block(shgrp, "grid_block", sldata->grid_ubo);
  DRW_shgroup_uniform_block(shgrp, "planar_block", sldata->planar_ubo);
  DRW_shgroup_uniform_block(shgrp, "light_block", sldata->light_ubo);
  DRW_shgroup_uniform_block(shgrp, "shadow_block", sldata->shadow_ubo);
  DRW_shgroup_uniform_block(shgrp, "common_block", sldata->common_ubo);
  DRW_shgroup_uniform_block(shgrp, "renderpass_block", get_render_pass_ubo(sldata, render_pass));

  DRW_shgroup_uniform_int_copy(shgrp, "outputSssId", 1);
  if (use_diffuse || use_glossy || use_refract) {
    DRW_shgroup_uniform_texture(shgrp, "utilTex", e_data.util_tex);
    DRW_shgroup_uniform_texture_ref(shgrp, "shadowCubeTexture", &sldata->shadow_cube_pool);
    DRW_shgroup_uniform_texture_ref(shgrp, "shadowCascadeTexture", &sldata->shadow_cascade_pool);
    DRW_shgroup_uniform_texture_ref(shgrp, "maxzBuffer", &vedata->txl->maxzbuffer);
  }
  if ((use_diffuse || use_glossy) && !use_ssrefraction) {
    DRW_shgroup_uniform_texture_ref(shgrp, "horizonBuffer", &effects->gtao_horizons);
  }
  if (use_diffuse) {
    DRW_shgroup_uniform_texture_ref(shgrp, "irradianceGrid", &lcache->grid_tx.tex);
  }
  if (use_glossy || use_refract) {
    DRW_shgroup_uniform_texture_ref(shgrp, "probeCubes", &lcache->cube_tx.tex);
  }
  if (use_glossy) {
    DRW_shgroup_uniform_texture_ref(shgrp, "probePlanars", &vedata->txl->planar_pool);
    DRW_shgroup_uniform_int_copy(shgrp, "outputSsrId", ssr_id ? *ssr_id : 0);
  }
  if (use_refract) {
    DRW_shgroup_uniform_float_copy(
        shgrp, "refractionDepth", (refract_depth) ? *refract_depth : 0.0);
    if (use_ssrefraction) {
      DRW_shgroup_uniform_texture_ref(shgrp, "colorBuffer", &vedata->txl->refract_color);
    }
  }
  if (use_alpha_blend) {
    DRW_shgroup_uniform_texture_ref(shgrp, "inScattering", &effects->volume_scatter);
    DRW_shgroup_uniform_texture_ref(shgrp, "inTransmittance", &effects->volume_transmit);
  }
}

/* Add the uniforms for the background shader to `shgrp`. */
static void add_background_uniforms(DRWShadingGroup *shgrp,
                                    EEVEE_ViewLayerData *sldata,
                                    EEVEE_Data *vedata)
{
  EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
  DRW_shgroup_uniform_float(shgrp, "backgroundAlpha", &stl->g_data->background_alpha, 1);
  /* TODO (fclem): remove those (need to clean the GLSL files). */
  DRW_shgroup_uniform_block(shgrp, "common_block", sldata->common_ubo);
  DRW_shgroup_uniform_block(shgrp, "grid_block", sldata->grid_ubo);
  DRW_shgroup_uniform_block(shgrp, "probe_block", sldata->probe_ubo);
  DRW_shgroup_uniform_block(shgrp, "planar_block", sldata->planar_ubo);
  DRW_shgroup_uniform_block(shgrp, "light_block", sldata->light_ubo);
  DRW_shgroup_uniform_block(shgrp, "shadow_block", sldata->shadow_ubo);
  DRW_shgroup_uniform_block(
      shgrp, "renderpass_block", EEVEE_material_default_render_pass_ubo_get(sldata));
}

static void create_default_shader(int options)
{
  char *frag_str = BLI_string_joinN(e_data.frag_shader_lib, datatoc_default_frag_glsl);

  char *defines = eevee_get_defines(options);

  e_data.default_lit[options] = DRW_shader_create(e_data.vert_shader_str, NULL, frag_str, defines);

  MEM_freeN(defines);
  MEM_freeN(frag_str);
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
  for (int j = 0; j < 16; j++) {
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
  const DRWContextState *draw_ctx = DRW_context_state_get();
  EEVEE_PrivateData *g_data = stl->g_data;

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
                                              datatoc_cubemap_lib_glsl,
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
                                                datatoc_cubemap_lib_glsl,
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

    e_data.vert_background_shader_str = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                                         datatoc_background_vert_glsl);

    e_data.vert_volume_shader_str = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                                     datatoc_volumetric_vert_glsl);

    e_data.geom_volume_shader_str = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                                     datatoc_volumetric_geom_glsl);

    e_data.default_background = DRW_shader_create_with_lib(datatoc_background_vert_glsl,
                                                           NULL,
                                                           datatoc_default_world_frag_glsl,
                                                           datatoc_common_view_lib_glsl,
                                                           NULL);

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
  }

  if (!DRW_state_is_image_render() && ((stl->effects->enabled_effects & EFFECT_TAA) == 0)) {
    sldata->common_data.alpha_hash_offset = 0.0f;
    sldata->common_data.alpha_hash_scale = 1.0f;
  }
  else {
    double r;
    BLI_halton_1d(5, 0.0, stl->effects->taa_current_sample - 1, &r);
    sldata->common_data.alpha_hash_offset = (float)r;
    sldata->common_data.alpha_hash_scale = 0.01f;
  }

  {
    /* Update view_vecs */
    float invproj[4][4], winmat[4][4];
    DRW_view_winmat_get(NULL, winmat, false);
    DRW_view_winmat_get(NULL, invproj, true);

    EEVEE_update_viewvecs(invproj, winmat, sldata->common_data.view_vecs);
  }

  {
    /* Update noise Framebuffer. */
    GPU_framebuffer_ensure_config(
        &fbl->update_noise_fb,
        {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE_LAYER(e_data.util_tex, 2)});
  }

  {
    /* Create RenderPass UBO */
    if (sldata->renderpass_ubo[0] == NULL) {
      /* EEVEE_RENDER_PASS_COMBINED */
      sldata->renderpass_data[0] = (const EEVEE_RenderPassData){
          true, true, true, true, true, false};
      /* EEVEE_RENDER_PASS_DIFFUSE_COLOR */
      sldata->renderpass_data[1] = (const EEVEE_RenderPassData){
          true, false, false, false, false, true};
      /* EEVEE_RENDER_PASS_DIFFUSE_LIGHT */
      sldata->renderpass_data[2] = (const EEVEE_RenderPassData){
          true, true, false, false, false, false};
      /* EEVEE_RENDER_PASS_SPECULAR_COLOR */
      sldata->renderpass_data[3] = (const EEVEE_RenderPassData){
          false, false, true, false, false, false};
      /* EEVEE_RENDER_PASS_SPECULAR_LIGHT */
      sldata->renderpass_data[4] = (const EEVEE_RenderPassData){
          false, false, true, true, false, false};
      /* EEVEE_RENDER_PASS_EMIT */
      sldata->renderpass_data[5] = (const EEVEE_RenderPassData){
          false, false, false, false, true, false};

      for (int i = 0; i < MAX_MATERIAL_RENDER_PASSES_UBO; i++) {
        sldata->renderpass_ubo[i] = DRW_uniformbuffer_create(sizeof(EEVEE_RenderPassData),
                                                             &sldata->renderpass_data[i]);
      }
    }

    /* HACK: EEVEE_material_world_background_get can create a new context. This can only be
     * done when there is no active framebuffer. We do this here otherwise
     * `EEVEE_renderpasses_output_init` will fail. It cannot be done in
     * `EEVEE_renderpasses_init` as the `e_data.vertcode` can be uninitialized.
     */
    if (g_data->render_passes & EEVEE_RENDER_PASS_ENVIRONMENT) {
      struct Scene *scene = draw_ctx->scene;
      struct World *wo = scene->world;
      if (wo && wo->use_nodes) {
        EEVEE_material_world_background_get(scene, wo);
      }
    }
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
                                      false,
                                      e_data.vert_background_shader_str,
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
                                      false,
                                      e_data.vert_background_shader_str,
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
                                     true,
                                     e_data.vert_volume_shader_str,
                                     e_data.geom_volume_shader_str,
                                     e_data.volume_shader_lib,
                                     defines,
                                     true);

  MEM_freeN(defines);

  return mat;
}

struct GPUMaterial *EEVEE_material_mesh_get(struct Scene *scene,
                                            Material *ma,
                                            EEVEE_Data *UNUSED(vedata),
                                            bool use_blend,
                                            bool use_refract)
{
  const void *engine = &DRW_engine_viewport_eevee_type;
  int options = VAR_MAT_MESH;

  SET_FLAG_FROM_TEST(options, use_blend, VAR_MAT_BLEND);
  SET_FLAG_FROM_TEST(options, use_refract, VAR_MAT_REFRACT);

  GPUMaterial *mat = DRW_shader_find_from_material(ma, engine, options, true);
  if (mat) {
    return mat;
  }

  char *defines = eevee_get_defines(options);

  mat = DRW_shader_create_from_material(scene,
                                        ma,
                                        engine,
                                        options,
                                        false,
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
                                        true,
                                        e_data.vert_volume_shader_str,
                                        e_data.geom_volume_shader_str,
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

  SET_FLAG_FROM_TEST(options, use_hashed_alpha, VAR_MAT_HASH);
  SET_FLAG_FROM_TEST(options, !use_hashed_alpha, VAR_MAT_CLIP);
  SET_FLAG_FROM_TEST(options, is_shadow, VAR_MAT_SHADOW);

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
                                        false,
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

static struct GPUMaterial *EEVEE_material_hair_depth_get(struct Scene *scene,
                                                         Material *ma,
                                                         bool use_hashed_alpha,
                                                         bool is_shadow)
{
  const void *engine = &DRW_engine_viewport_eevee_type;
  int options = VAR_MAT_MESH | VAR_MAT_HAIR;

  SET_FLAG_FROM_TEST(options, use_hashed_alpha, VAR_MAT_HASH);
  SET_FLAG_FROM_TEST(options, !use_hashed_alpha, VAR_MAT_CLIP);
  SET_FLAG_FROM_TEST(options, is_shadow, VAR_MAT_SHADOW);

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
                                        false,
                                        (is_shadow) ? e_data.vert_shadow_shader_str :
                                                      e_data.vert_shader_str,
                                        NULL,
                                        frag_str,
                                        defines,
                                        false);

  MEM_freeN(frag_str);
  MEM_freeN(defines);

  return mat;
}

struct GPUMaterial *EEVEE_material_hair_get(struct Scene *scene, Material *ma)
{
  const void *engine = &DRW_engine_viewport_eevee_type;
  int options = VAR_MAT_MESH | VAR_MAT_HAIR;

  GPUMaterial *mat = DRW_shader_find_from_material(ma, engine, options, true);
  if (mat) {
    return mat;
  }

  char *defines = eevee_get_defines(options);

  mat = DRW_shader_create_from_material(scene,
                                        ma,
                                        engine,
                                        options,
                                        false,
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
                                                                  bool use_ssr)
{
  static int ssr_id;
  ssr_id = (use_ssr) ? 1 : -1;
  int options = VAR_MAT_MESH;

  SET_FLAG_FROM_TEST(options, is_hair, VAR_MAT_HAIR);
  SET_FLAG_FROM_TEST(options, use_blend, VAR_MAT_BLEND);

  if (e_data.default_lit[options] == NULL) {
    create_default_shader(options);
  }

  DRWShadingGroup *shgrp = DRW_shgroup_create(e_data.default_lit[options], pass);
  add_standard_uniforms(shgrp,
                        sldata,
                        vedata,
                        &ssr_id,
                        NULL,
                        true,
                        true,
                        false,
                        false,
                        use_blend,
                        DEFAULT_RENDER_PASS_FLAG);

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
                                                               bool holdout,
                                                               bool use_ssr)
{
  static int ssr_id;
  ssr_id = (use_ssr) ? 1 : -1;
  int options = VAR_MAT_MESH;

  EEVEE_PassList *psl = vedata->psl;

  BLI_assert(!is_hair || (ob && ((psys && md) || ob->type == OB_HAIR)));

  SET_FLAG_FROM_TEST(options, is_hair, VAR_MAT_HAIR);
  SET_FLAG_FROM_TEST(options, holdout, VAR_MAT_HOLDOUT);

  if (e_data.default_lit[options] == NULL) {
    create_default_shader(options);
  }

  if (psl->default_pass[options] == NULL) {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_CLIP_PLANES;
    DRW_PASS_CREATE(psl->default_pass[options], state);

    /* XXX / WATCH: This creates non persistent binds for the ubos and textures.
     * But it's currently OK because the following shgroups does not add any bind.
     * EDIT: THIS IS NOT THE CASE FOR HAIRS !!! DUMMY!!! */
    if (!is_hair) {
      DRWShadingGroup *shgrp = DRW_shgroup_create(e_data.default_lit[options],
                                                  psl->default_pass[options]);
      add_standard_uniforms(shgrp,
                            sldata,
                            vedata,
                            &ssr_id,
                            NULL,
                            true,
                            true,
                            false,
                            false,
                            false,
                            DEFAULT_RENDER_PASS_FLAG);
    }
  }

  if (is_hair) {
    DRWShadingGroup *shgrp = DRW_shgroup_hair_create(
        ob, psys, md, vedata->psl->default_pass[options], e_data.default_lit[options]);
    add_standard_uniforms(shgrp,
                          sldata,
                          vedata,
                          &ssr_id,
                          NULL,
                          true,
                          true,
                          false,
                          false,
                          false,
                          DEFAULT_RENDER_PASS_FLAG);
    return shgrp;
  }
  else {
    return DRW_shgroup_create(e_data.default_lit[options], vedata->psl->default_pass[options]);
  }
}

static struct DRWShadingGroup *EEVEE_default_render_pass_shading_group_get(
    EEVEE_ViewLayerData *sldata,
    EEVEE_Data *vedata,
    bool holdout,
    bool use_ssr,
    DRWPass *pass,
    eViewLayerEEVEEPassType render_pass_flag)
{
  static int ssr_id;
  ssr_id = (use_ssr) ? 1 : -1;
  int options = VAR_MAT_MESH;

  SET_FLAG_FROM_TEST(options, holdout, VAR_MAT_HOLDOUT);

  if (e_data.default_lit[options] == NULL) {
    create_default_shader(options);
  }

  DRWShadingGroup *shgrp = DRW_shgroup_create(e_data.default_lit[options], pass);
  add_standard_uniforms(
      shgrp, sldata, vedata, &ssr_id, NULL, true, true, false, false, false, render_pass_flag);
  return shgrp;
}

static struct DRWShadingGroup *EEVEE_default_hair_render_pass_shading_group_get(
    EEVEE_ViewLayerData *sldata,
    EEVEE_Data *vedata,
    Object *ob,
    ParticleSystem *psys,
    ModifierData *md,
    bool holdout,
    bool use_ssr,
    DRWPass *pass,
    eViewLayerEEVEEPassType render_pass_flag)
{
  static int ssr_id;
  ssr_id = (use_ssr) ? 1 : -1;
  int options = VAR_MAT_MESH | VAR_MAT_HAIR;

  BLI_assert((ob && psys && md));

  SET_FLAG_FROM_TEST(options, holdout, VAR_MAT_HOLDOUT);

  if (e_data.default_lit[options] == NULL) {
    create_default_shader(options);
  }

  DRWShadingGroup *shgrp = DRW_shgroup_hair_create(
      ob, psys, md, pass, e_data.default_lit[options]);
  add_standard_uniforms(
      shgrp, sldata, vedata, &ssr_id, NULL, true, true, false, false, false, render_pass_flag);
  return shgrp;
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
    DRW_PASS_CREATE(psl->background_pass, DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL);

    struct GPUBatch *geom = DRW_cache_fullscreen_quad_get();
    DRWShadingGroup *grp = NULL;

    Scene *scene = draw_ctx->scene;
    World *wo = scene->world;

    const float *col = G_draw.block.colorBackground;

    EEVEE_lookdev_cache_init(vedata, sldata, &grp, psl->background_pass, wo, NULL);

    if (!grp && wo) {
      col = &wo->horr;

      if (wo->use_nodes && wo->nodetree) {
        static float error_col[3] = {1.0f, 0.0f, 1.0f};
        static float compile_col[3] = {0.5f, 0.5f, 0.5f};
        struct GPUMaterial *gpumat = EEVEE_material_world_background_get(scene, wo);

        switch (GPU_material_status(gpumat)) {
          case GPU_MAT_SUCCESS:
            grp = DRW_shgroup_material_create(gpumat, psl->background_pass);
            add_background_uniforms(grp, sldata, vedata);
            DRW_shgroup_call(grp, geom, NULL);
            break;
          case GPU_MAT_QUEUED:
            /* TODO Bypass probe compilation. */
            stl->g_data->queued_shaders_count++;
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
      DRW_shgroup_call(grp, geom, NULL);
    }
  }

  {
    DRWState state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    DRW_PASS_CREATE(psl->depth_pass, state);
    stl->g_data->depth_shgrp = DRW_shgroup_create(e_data.default_prepass_sh, psl->depth_pass);

    state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CULL_BACK;
    DRW_PASS_CREATE(psl->depth_pass_cull, state);
    stl->g_data->depth_shgrp_cull = DRW_shgroup_create(e_data.default_prepass_sh,
                                                       psl->depth_pass_cull);

    state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CLIP_PLANES;
    DRW_PASS_CREATE(psl->depth_pass_clip, state);
    stl->g_data->depth_shgrp_clip = DRW_shgroup_create(e_data.default_prepass_clip_sh,
                                                       psl->depth_pass_clip);

    state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CLIP_PLANES |
            DRW_STATE_CULL_BACK;
    DRW_PASS_CREATE(psl->depth_pass_clip_cull, state);
    stl->g_data->depth_shgrp_clip_cull = DRW_shgroup_create(e_data.default_prepass_clip_sh,
                                                            psl->depth_pass_clip_cull);
  }

  {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_CLIP_PLANES;
    DRW_PASS_CREATE(psl->material_pass, state);
    DRW_PASS_CREATE(psl->material_pass_cull, state | DRW_STATE_CULL_BACK);
  }

  {
    DRWState state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    DRW_PASS_CREATE(psl->refract_depth_pass, state);
    stl->g_data->refract_depth_shgrp = DRW_shgroup_create(e_data.default_prepass_sh,
                                                          psl->refract_depth_pass);

    state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CULL_BACK;
    DRW_PASS_CREATE(psl->refract_depth_pass_cull, state);
    stl->g_data->refract_depth_shgrp_cull = DRW_shgroup_create(e_data.default_prepass_sh,
                                                               psl->refract_depth_pass_cull);

    state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CLIP_PLANES;
    DRW_PASS_CREATE(psl->refract_depth_pass_clip, state);
    stl->g_data->refract_depth_shgrp_clip = DRW_shgroup_create(e_data.default_prepass_clip_sh,
                                                               psl->refract_depth_pass_clip);

    state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CLIP_PLANES |
            DRW_STATE_CULL_BACK;
    DRW_PASS_CREATE(psl->refract_depth_pass_clip_cull, state);
    stl->g_data->refract_depth_shgrp_clip_cull = DRW_shgroup_create(
        e_data.default_prepass_clip_sh, psl->refract_depth_pass_clip_cull);
  }

  {
    DRWState state = (DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_CLIP_PLANES);
    DRW_PASS_CREATE(psl->refract_pass, state);
  }

  {
    DRWState state = (DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_CLIP_PLANES |
                      DRW_STATE_WRITE_STENCIL | DRW_STATE_STENCIL_ALWAYS);
    DRW_PASS_CREATE(psl->sss_pass, state);
    DRW_PASS_CREATE(psl->sss_pass_cull, state | DRW_STATE_CULL_BACK);
    e_data.sss_count = 0;
  }

  {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CLIP_PLANES;
    DRW_PASS_CREATE(psl->transparent_pass, state);
  }

  {
    DRW_PASS_CREATE(psl->update_noise_pass, DRW_STATE_WRITE_COLOR);
    DRWShadingGroup *grp = DRW_shgroup_create(e_data.update_noise_sh, psl->update_noise_pass);
    DRW_shgroup_uniform_texture(grp, "blueNoise", e_data.noise_tex);
    DRW_shgroup_uniform_vec3(grp, "offsets", e_data.noise_offsets, 1);
    DRW_shgroup_call(grp, DRW_cache_fullscreen_quad_get(), NULL);
  }

  if (eevee_hdri_preview_overlay_enabled(draw_ctx->v3d)) {
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

    DRW_PASS_CREATE(psl->lookdev_diffuse_pass, state);
    shgrp = DRW_shgroup_create(e_data.default_lit[options], psl->lookdev_diffuse_pass);
    add_standard_uniforms(shgrp,
                          sldata,
                          vedata,
                          NULL,
                          NULL,
                          true,
                          true,
                          false,
                          false,
                          false,
                          DEFAULT_RENDER_PASS_FLAG);
    DRW_shgroup_uniform_vec3(shgrp, "basecol", color_diffuse, 1);
    DRW_shgroup_uniform_float_copy(shgrp, "metallic", 0.0f);
    DRW_shgroup_uniform_float_copy(shgrp, "specular", 0.5f);
    DRW_shgroup_uniform_float_copy(shgrp, "roughness", 1.0f);
    DRW_shgroup_call(shgrp, sphere, NULL);

    DRW_PASS_CREATE(psl->lookdev_glossy_pass, state);
    shgrp = DRW_shgroup_create(e_data.default_lit[options], psl->lookdev_glossy_pass);
    add_standard_uniforms(shgrp,
                          sldata,
                          vedata,
                          NULL,
                          NULL,
                          true,
                          true,
                          false,
                          false,
                          false,
                          DEFAULT_RENDER_PASS_FLAG);
    DRW_shgroup_uniform_vec3(shgrp, "basecol", color_chrome, 1);
    DRW_shgroup_uniform_float_copy(shgrp, "metallic", 1.0f);
    DRW_shgroup_uniform_float_copy(shgrp, "roughness", 0.0f);
    DRW_shgroup_call(shgrp, sphere, NULL);
  }

  {
    memset(psl->material_accum_pass, 0, sizeof(psl->material_accum_pass));
    for (int pass_index = 0; pass_index < stl->g_data->render_passes_material_count;
         pass_index++) {
      DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND_ADD_FULL;
      DRW_PASS_CREATE(psl->material_accum_pass[pass_index], state);
    }
  }
}

#define ADD_SHGROUP_CALL(shgrp, ob, geom, oedata) \
  do { \
    if (oedata) { \
      DRW_shgroup_call_with_callback(shgrp, geom, ob, oedata); \
    } \
    else { \
      DRW_shgroup_call(shgrp, geom, ob); \
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
  struct DRWShadingGroup *material_accum_grp[MAX_MATERIAL_RENDER_PASSES];
} EeveeMaterialShadingGroups;

static void material_opaque(Material *ma,
                            GHash *material_hash,
                            EEVEE_ViewLayerData *sldata,
                            EEVEE_Data *vedata,
                            struct GPUMaterial **gpumat,
                            struct GPUMaterial **gpumat_depth,
                            struct EeveeMaterialShadingGroups *shgrps,
                            bool holdout)
{
  EEVEE_EffectsInfo *effects = vedata->stl->effects;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
  EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
  bool use_diffuse, use_glossy, use_refract;
  bool store_material = true;
  float *color_p = &ma->r;
  float *metal_p = &ma->metallic;
  float *spec_p = &ma->spec;
  float *rough_p = &ma->roughness;

  const bool do_cull = (ma->blend_flag & MA_BL_CULL_BACKFACE) != 0;
  const bool use_gpumat = (ma->use_nodes && ma->nodetree && !holdout);
  const bool use_ssrefract = use_gpumat && ((ma->blend_flag & MA_BL_SS_REFRACTION) != 0) &&
                             ((effects->enabled_effects & EFFECT_REFRACT) != 0);
  const bool use_translucency = ((ma->blend_flag & MA_BL_TRANSLUCENCY) != 0);

  EeveeMaterialShadingGroups *emsg = BLI_ghash_lookup(material_hash, (const void *)ma);

  if (emsg) {
    memcpy(shgrps, emsg, sizeof(EeveeMaterialShadingGroups));

    /* This will have been created already, just perform a lookup. */
    *gpumat = (use_gpumat) ? EEVEE_material_mesh_get(scene, ma, vedata, false, use_ssrefract) :
                             NULL;
    *gpumat_depth = (use_gpumat) ? EEVEE_material_mesh_depth_get(
                                       scene, ma, (ma->blend_method == MA_BM_HASHED), false) :
                                   NULL;
    return;
  }

  emsg = MEM_callocN(sizeof(EeveeMaterialShadingGroups), "EeveeMaterialShadingGroups");
  if (use_gpumat) {
    static float error_col[3] = {1.0f, 0.0f, 1.0f};
    static float compile_col[3] = {0.5f, 0.5f, 0.5f};
    static float half = 0.5f;

    /* Shading */
    *gpumat = EEVEE_material_mesh_get(scene, ma, vedata, false, use_ssrefract);

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
        emsg->depth_grp = DRW_shgroup_material_create(
            *gpumat_depth, (do_cull) ? psl->refract_depth_pass_cull : psl->refract_depth_pass);
        emsg->depth_clip_grp = DRW_shgroup_material_create(
            *gpumat_depth,
            (do_cull) ? psl->refract_depth_pass_clip_cull : psl->refract_depth_pass_clip);
      }
      else {
        emsg->depth_grp = DRW_shgroup_material_create(
            *gpumat_depth, (do_cull) ? psl->depth_pass_cull : psl->depth_pass);
        emsg->depth_clip_grp = DRW_shgroup_material_create(
            *gpumat_depth, (do_cull) ? psl->depth_pass_clip_cull : psl->depth_pass_clip);
      }

      if (emsg->depth_grp != NULL) {
        use_diffuse = GPU_material_flag_get(*gpumat_depth, GPU_MATFLAG_DIFFUSE);
        use_glossy = GPU_material_flag_get(*gpumat_depth, GPU_MATFLAG_GLOSSY);
        use_refract = GPU_material_flag_get(*gpumat_depth, GPU_MATFLAG_REFRACT);

        add_standard_uniforms(emsg->depth_grp,
                              sldata,
                              vedata,
                              NULL,
                              NULL,
                              use_diffuse,
                              use_glossy,
                              use_refract,
                              false,
                              false,
                              DEFAULT_RENDER_PASS_FLAG);
        add_standard_uniforms(emsg->depth_clip_grp,
                              sldata,
                              vedata,
                              NULL,
                              NULL,
                              use_diffuse,
                              use_glossy,
                              use_refract,
                              false,
                              false,
                              DEFAULT_RENDER_PASS_FLAG);

        if (ma->blend_method == MA_BM_CLIP) {
          DRW_shgroup_uniform_float(emsg->depth_grp, "alphaThreshold", &ma->alpha_threshold, 1);
          DRW_shgroup_uniform_float(
              emsg->depth_clip_grp, "alphaThreshold", &ma->alpha_threshold, 1);
        }
      }
    }

    switch (status_mat_surface) {
      case GPU_MAT_SUCCESS: {
        static int no_ssr = 0;
        static int first_ssr = 1;
        int *ssr_id = (((effects->enabled_effects & EFFECT_SSR) != 0) && !use_ssrefract) ?
                          &first_ssr :
                          &no_ssr;
        const bool use_sss = GPU_material_flag_get(*gpumat, GPU_MATFLAG_SSS);
        use_diffuse = GPU_material_flag_get(*gpumat, GPU_MATFLAG_DIFFUSE);
        use_glossy = GPU_material_flag_get(*gpumat, GPU_MATFLAG_GLOSSY);
        use_refract = GPU_material_flag_get(*gpumat, GPU_MATFLAG_REFRACT);

        emsg->shading_grp = DRW_shgroup_material_create(
            *gpumat,
            (use_ssrefract) ?
                psl->refract_pass :
                (use_sss) ? ((do_cull) ? psl->sss_pass_cull : psl->sss_pass) :
                            ((do_cull) ? psl->material_pass_cull : psl->material_pass));

        add_standard_uniforms(emsg->shading_grp,
                              sldata,
                              vedata,
                              ssr_id,
                              &ma->refract_depth,
                              use_diffuse,
                              use_glossy,
                              use_refract,
                              use_ssrefract,
                              false,
                              DEFAULT_RENDER_PASS_FLAG);

        if (use_sss) {
          struct GPUTexture *sss_tex_profile = NULL;
          struct GPUUniformBuffer *sss_profile = GPU_material_sss_profile_get(
              *gpumat, stl->effects->sss_sample_count, &sss_tex_profile);

          if (sss_profile) {
            /* Limit of 8 bit stencil buffer. ID 255 is refraction. */
            if (e_data.sss_count < 254) {
              int sss_id = e_data.sss_count + 1;
              DRW_shgroup_stencil_mask(emsg->shading_grp, sss_id);
              EEVEE_subsurface_add_pass(sldata, vedata, sss_id, sss_profile);
              if (use_translucency) {
                EEVEE_subsurface_translucency_add_pass(
                    sldata, vedata, sss_id, sss_profile, sss_tex_profile);
              }
              e_data.sss_count++;
            }
            else {
              /* TODO : display message. */
              printf("Error: Too many different Subsurface shader in the scene.\n");
            }
          }
        }

        RENDER_PASS_ITER_BEGIN (stl->g_data->render_passes, render_pass_index, render_pass_flag) {
          emsg->material_accum_grp[render_pass_index] = DRW_shgroup_material_create(
              *gpumat, psl->material_accum_pass[render_pass_index]);
          add_standard_uniforms(emsg->material_accum_grp[render_pass_index],
                                sldata,
                                vedata,
                                ssr_id,
                                &ma->refract_depth,
                                use_diffuse,
                                use_glossy,
                                use_refract,
                                use_ssrefract,
                                false,
                                render_pass_flag);
        }
        RENDER_PASS_ITER_END(render_pass_index);

        break;
      }
      case GPU_MAT_QUEUED: {
        stl->g_data->queued_shaders_count++;
        color_p = compile_col;
        metal_p = spec_p = rough_p = &half;
        store_material = false;
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
  if (emsg->shading_grp == NULL) {
    bool use_ssr = ((effects->enabled_effects & EFFECT_SSR) != 0);
    emsg->shading_grp = EEVEE_default_shading_group_get(
        sldata, vedata, NULL, NULL, NULL, false, holdout, use_ssr);
    DRW_shgroup_uniform_vec3(emsg->shading_grp, "basecol", color_p, 1);
    DRW_shgroup_uniform_float(emsg->shading_grp, "metallic", metal_p, 1);
    DRW_shgroup_uniform_float(emsg->shading_grp, "specular", spec_p, 1);
    DRW_shgroup_uniform_float(emsg->shading_grp, "roughness", rough_p, 1);

    RENDER_PASS_ITER_BEGIN (stl->g_data->render_passes, render_pass_index, render_pass_flag) {
      DRWShadingGroup *shgrp = EEVEE_default_render_pass_shading_group_get(
          sldata,
          vedata,
          holdout,
          use_ssr,
          psl->material_accum_pass[render_pass_index],
          render_pass_flag);

      DRW_shgroup_uniform_vec3(shgrp, "basecol", color_p, 1);
      DRW_shgroup_uniform_float(shgrp, "metallic", metal_p, 1);
      DRW_shgroup_uniform_float(shgrp, "specular", spec_p, 1);
      DRW_shgroup_uniform_float(shgrp, "roughness", rough_p, 1);
      emsg->material_accum_grp[render_pass_index] = shgrp;
    }
    RENDER_PASS_ITER_END(render_pass_index);
  }

  /* Fallback default depth prepass */
  if (emsg->depth_grp == NULL) {
    if (use_ssrefract) {
      emsg->depth_grp = (do_cull) ? stl->g_data->refract_depth_shgrp_cull :
                                    stl->g_data->refract_depth_shgrp;
      emsg->depth_clip_grp = (do_cull) ? stl->g_data->refract_depth_shgrp_clip_cull :
                                         stl->g_data->refract_depth_shgrp_clip;
    }
    else {
      emsg->depth_grp = (do_cull) ? stl->g_data->depth_shgrp_cull : stl->g_data->depth_shgrp;
      emsg->depth_clip_grp = (do_cull) ? stl->g_data->depth_shgrp_clip_cull :
                                         stl->g_data->depth_shgrp_clip;
    }
  }

  memcpy(shgrps, emsg, sizeof(EeveeMaterialShadingGroups));
  if (store_material) {
    BLI_ghash_insert(material_hash, ma, emsg);
  }
  else {
    MEM_freeN(emsg);
  }
}

static void material_transparent(Material *ma,
                                 EEVEE_ViewLayerData *sldata,
                                 EEVEE_Data *vedata,
                                 struct GPUMaterial **gpumat,
                                 struct EeveeMaterialShadingGroups *shgrps)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
  EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;

  const bool do_cull = (ma->blend_flag & MA_BL_CULL_BACKFACE) != 0;
  const bool use_gpumat = ma->use_nodes && ma->nodetree;
  const bool use_ssrefract = use_gpumat && ((ma->blend_flag & MA_BL_SS_REFRACTION) != 0) &&
                             ((stl->effects->enabled_effects & EFFECT_REFRACT) != 0);
  const float *color_p = &ma->r;
  const float *metal_p = &ma->metallic;
  const float *spec_p = &ma->spec;
  const float *rough_p = &ma->roughness;

  const bool use_prepass = ((ma->blend_flag & MA_BL_HIDE_BACKFACE) != 0);

  DRWState cur_state;
  DRWState all_state = (DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR | DRW_STATE_CULL_BACK |
                        DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_DEPTH_EQUAL |
                        DRW_STATE_BLEND_CUSTOM);

  /* Depth prepass */
  if (use_prepass) {
    shgrps->depth_grp = DRW_shgroup_create(e_data.default_prepass_clip_sh, psl->transparent_pass);

    cur_state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    cur_state |= (do_cull) ? DRW_STATE_CULL_BACK : 0;

    DRW_shgroup_state_disable(shgrps->depth_grp, all_state);
    DRW_shgroup_state_enable(shgrps->depth_grp, cur_state);
  }

  if (use_gpumat) {
    static float error_col[3] = {1.0f, 0.0f, 1.0f};
    static float compile_col[3] = {0.5f, 0.5f, 0.5f};
    static float half = 0.5f;

    /* Shading */
    *gpumat = EEVEE_material_mesh_get(scene, ma, vedata, true, use_ssrefract);

    switch (GPU_material_status(*gpumat)) {
      case GPU_MAT_SUCCESS: {
        static int ssr_id = -1; /* TODO transparent SSR */

        shgrps->shading_grp = DRW_shgroup_material_create(*gpumat, psl->transparent_pass);

        bool use_blend = true;
        bool use_diffuse = GPU_material_flag_get(*gpumat, GPU_MATFLAG_DIFFUSE);
        bool use_glossy = GPU_material_flag_get(*gpumat, GPU_MATFLAG_GLOSSY);
        bool use_refract = GPU_material_flag_get(*gpumat, GPU_MATFLAG_REFRACT);

        add_standard_uniforms(shgrps->shading_grp,
                              sldata,
                              vedata,
                              &ssr_id,
                              &ma->refract_depth,
                              use_diffuse,
                              use_glossy,
                              use_refract,
                              use_ssrefract,
                              use_blend,
                              DEFAULT_RENDER_PASS_FLAG);
        break;
      }
      case GPU_MAT_QUEUED: {
        /* TODO Bypass probe compilation. */
        stl->g_data->queued_shaders_count++;
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
  if (shgrps->shading_grp == NULL) {
    shgrps->shading_grp = EEVEE_default_shading_group_create(
        sldata, vedata, psl->transparent_pass, false, true, false);
    DRW_shgroup_uniform_vec3(shgrps->shading_grp, "basecol", color_p, 1);
    DRW_shgroup_uniform_float(shgrps->shading_grp, "metallic", metal_p, 1);
    DRW_shgroup_uniform_float(shgrps->shading_grp, "specular", spec_p, 1);
    DRW_shgroup_uniform_float(shgrps->shading_grp, "roughness", rough_p, 1);
  }

  cur_state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM;
  cur_state |= (use_prepass) ? DRW_STATE_DEPTH_EQUAL : DRW_STATE_DEPTH_LESS_EQUAL;
  cur_state |= (do_cull) ? DRW_STATE_CULL_BACK : 0;

  /* Disable other blend modes and use the one we want. */
  DRW_shgroup_state_disable(shgrps->shading_grp, all_state);
  DRW_shgroup_state_enable(shgrps->shading_grp, cur_state);
}

/* Return correct material or empty default material if slot is empty. */
BLI_INLINE Material *eevee_object_material_get(Object *ob, int slot, bool holdout)
{
  if (holdout) {
    return BKE_material_default_holdout();
  }
  Material *ma = BKE_object_material_get(ob, slot + 1);
  if (ma == NULL) {
    if (ob->type == OB_VOLUME) {
      ma = BKE_material_default_volume();
    }
    else {
      ma = BKE_material_default_empty();
    }
  }
  return ma;
}

static void eevee_hair_cache_populate(EEVEE_Data *vedata,
                                      EEVEE_ViewLayerData *sldata,
                                      Object *ob,
                                      ParticleSystem *psys,
                                      ModifierData *md,
                                      int matnr,
                                      bool *cast_shadow)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  const bool holdout = (ob->base_flag & BASE_HOLDOUT) != 0;

  DRWShadingGroup *shgrp = NULL;
  Material *ma = eevee_object_material_get(ob, matnr - 1, holdout);
  const bool use_gpumat = ma->use_nodes && ma->nodetree && !holdout;
  const bool use_alpha_hash = (ma->blend_method == MA_BM_HASHED);
  const bool use_alpha_clip = (ma->blend_method == MA_BM_CLIP);
  const bool use_ssr = ((stl->effects->enabled_effects & EFFECT_SSR) != 0);

  GPUMaterial *gpumat = use_gpumat ? EEVEE_material_hair_get(scene, ma) : NULL;
  eGPUMaterialStatus status_mat_surface = gpumat ? GPU_material_status(gpumat) : GPU_MAT_SUCCESS;

  float *color_p = &ma->r;
  float *metal_p = &ma->metallic;
  float *spec_p = &ma->spec;
  float *rough_p = &ma->roughness;

  /* Depth prepass. */
  if (use_gpumat && (use_alpha_clip || use_alpha_hash)) {
    GPUMaterial *gpumat_depth = EEVEE_material_hair_depth_get(scene, ma, use_alpha_hash, false);

    eGPUMaterialStatus status_mat_depth = GPU_material_status(gpumat_depth);

    if (status_mat_depth != GPU_MAT_SUCCESS) {
      /* Mixing both flags. If depth shader fails, show it to the user by not using
       * the surface shader. */
      status_mat_surface = status_mat_depth;
    }
    else {
      const bool use_diffuse = GPU_material_flag_get(gpumat_depth, GPU_MATFLAG_DIFFUSE);
      const bool use_glossy = GPU_material_flag_get(gpumat_depth, GPU_MATFLAG_GLOSSY);
      const bool use_refract = GPU_material_flag_get(gpumat_depth, GPU_MATFLAG_REFRACT);

      for (int i = 0; i < 2; i++) {
        DRWPass *pass = (i == 0) ? psl->depth_pass : psl->depth_pass_clip;

        shgrp = DRW_shgroup_material_hair_create(ob, psys, md, pass, gpumat_depth);

        add_standard_uniforms(shgrp,
                              sldata,
                              vedata,
                              NULL,
                              NULL,
                              use_diffuse,
                              use_glossy,
                              use_refract,
                              false,
                              false,
                              DEFAULT_RENDER_PASS_FLAG);

        /* Unfortunately needed for correctness but not 99% of the time not needed.
         * TODO detect when needed? */
        DRW_shgroup_uniform_block(shgrp, "probe_block", sldata->probe_ubo);
        DRW_shgroup_uniform_block(shgrp, "grid_block", sldata->grid_ubo);
        DRW_shgroup_uniform_block(shgrp, "planar_block", sldata->planar_ubo);
        DRW_shgroup_uniform_block(shgrp, "light_block", sldata->light_ubo);
        DRW_shgroup_uniform_block(shgrp, "shadow_block", sldata->shadow_ubo);
        DRW_shgroup_uniform_block(
            shgrp, "renderpass_block", EEVEE_material_default_render_pass_ubo_get(sldata));
        DRW_shgroup_uniform_block(shgrp, "common_block", sldata->common_ubo);
        DRW_shgroup_uniform_texture(shgrp, "utilTex", e_data.util_tex);

        if (use_alpha_clip) {
          DRW_shgroup_uniform_float(shgrp, "alphaThreshold", &ma->alpha_threshold, 1);
        }
      }
    }
  }

  /* Fallback to default shader */
  if (shgrp == NULL) {
    for (int i = 0; i < 2; i++) {
      DRWPass *depth_pass = (i == 0) ? psl->depth_pass : psl->depth_pass_clip;
      struct GPUShader *depth_sh = (i == 0) ? e_data.default_hair_prepass_sh :
                                              e_data.default_hair_prepass_clip_sh;
      DRW_shgroup_hair_create(ob, psys, md, depth_pass, depth_sh);
    }
  }

  shgrp = NULL;

  if (gpumat) {
    static int ssr_id;
    ssr_id = (use_ssr) ? 1 : -1;
    static float half = 0.5f;
    static float error_col[3] = {1.0f, 0.0f, 1.0f};
    static float compile_col[3] = {0.5f, 0.5f, 0.5f};

    switch (status_mat_surface) {
      case GPU_MAT_SUCCESS: {
        bool use_diffuse = GPU_material_flag_get(gpumat, GPU_MATFLAG_DIFFUSE);
        bool use_glossy = GPU_material_flag_get(gpumat, GPU_MATFLAG_GLOSSY);
        bool use_refract = GPU_material_flag_get(gpumat, GPU_MATFLAG_REFRACT);

        shgrp = DRW_shgroup_material_hair_create(ob, psys, md, psl->material_pass, gpumat);

        if (!use_diffuse && !use_glossy && !use_refract) {
          /* HACK: Small hack to avoid issue when utilTex is needed for
           * world_normals_get and none of the bsdfs are present.
           * This binds utilTex even if not needed. */
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
                              false,
                              DEFAULT_RENDER_PASS_FLAG);

        /* Add the hair to all the render_passes that are enabled */
        RENDER_PASS_ITER_BEGIN (stl->g_data->render_passes, render_pass_index, render_pass_flag) {
          shgrp = DRW_shgroup_material_hair_create(
              ob, psys, md, psl->material_accum_pass[render_pass_index], gpumat);
          if (!use_diffuse && !use_glossy && !use_refract) {
            /* Small hack to avoid issue when utilTex is needed for
             * world_normals_get and none of the bsdfs that need it are present.
             * This binds `utilTex` even if not needed. */
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
                                false,
                                render_pass_flag);
        }
        RENDER_PASS_ITER_END(render_pass_index);

        break;
      }
      case GPU_MAT_QUEUED: {
        stl->g_data->queued_shaders_count++;
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
    shgrp = EEVEE_default_shading_group_get(sldata, vedata, ob, psys, md, true, holdout, use_ssr);
    DRW_shgroup_uniform_vec3(shgrp, "basecol", color_p, 1);
    DRW_shgroup_uniform_float(shgrp, "metallic", metal_p, 1);
    DRW_shgroup_uniform_float(shgrp, "specular", spec_p, 1);
    DRW_shgroup_uniform_float(shgrp, "roughness", rough_p, 1);

    RENDER_PASS_ITER_BEGIN (stl->g_data->render_passes, render_pass_index, render_pass_flag) {
      shgrp = EEVEE_default_hair_render_pass_shading_group_get(
          sldata,
          vedata,
          ob,
          psys,
          md,
          holdout,
          use_ssr,
          psl->material_accum_pass[render_pass_index],
          render_pass_flag);

      DRW_shgroup_uniform_vec3(shgrp, "basecol", color_p, 1);
      DRW_shgroup_uniform_float(shgrp, "metallic", metal_p, 1);
      DRW_shgroup_uniform_float(shgrp, "specular", spec_p, 1);
      DRW_shgroup_uniform_float(shgrp, "roughness", rough_p, 1);
    }
    RENDER_PASS_ITER_END(render_pass_index);
  }

  /* Shadows */
  char blend_shadow = use_gpumat ? ma->blend_shadow : MA_BS_SOLID;
  const bool shadow_alpha_hash = (blend_shadow == MA_BS_HASHED);
  switch (blend_shadow) {
    case MA_BS_SOLID:
      DRW_shgroup_hair_create(ob, psys, md, psl->shadow_pass, e_data.default_hair_prepass_sh);
      *cast_shadow = true;
      break;
    case MA_BS_CLIP:
    case MA_BS_HASHED:
      gpumat = EEVEE_material_hair_depth_get(scene, ma, shadow_alpha_hash, true);
      shgrp = DRW_shgroup_material_hair_create(ob, psys, md, psl->shadow_pass, gpumat);
      /* Unfortunately needed for correctness but not 99% of the time not needed.
       * TODO detect when needed? */
      DRW_shgroup_uniform_block(shgrp, "probe_block", sldata->probe_ubo);
      DRW_shgroup_uniform_block(shgrp, "grid_block", sldata->grid_ubo);
      DRW_shgroup_uniform_block(shgrp, "planar_block", sldata->planar_ubo);
      DRW_shgroup_uniform_block(shgrp, "light_block", sldata->light_ubo);
      DRW_shgroup_uniform_block(shgrp, "shadow_block", sldata->shadow_ubo);
      DRW_shgroup_uniform_block(
          shgrp, "renderpass_block", EEVEE_material_default_render_pass_ubo_get(sldata));
      DRW_shgroup_uniform_block(shgrp, "common_block", sldata->common_ubo);
      DRW_shgroup_uniform_texture(shgrp, "utilTex", e_data.util_tex);

      if (!shadow_alpha_hash) {
        DRW_shgroup_uniform_float(shgrp, "alphaThreshold", &ma->alpha_threshold, 1);
      }
      *cast_shadow = true;
      break;
    case MA_BS_NONE:
    default:
      break;
  }
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
  const bool holdout = (ob->base_flag & BASE_HOLDOUT) != 0;

  bool use_sculpt_pbvh = BKE_sculptsession_use_pbvh_draw(ob, draw_ctx->v3d) &&
                         !DRW_state_is_image_render();

  /* First get materials for this mesh. */
  if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL)) {
    const int materials_len = DRW_cache_object_material_count_get(ob);

    struct EeveeMaterialShadingGroups *shgrps_array = BLI_array_alloca(shgrps_array,
                                                                       materials_len);

    struct GPUMaterial **gpumat_array = BLI_array_alloca(gpumat_array, materials_len);
    struct GPUMaterial **gpumat_depth_array = BLI_array_alloca(gpumat_array, materials_len);
    struct Material **ma_array = BLI_array_alloca(ma_array, materials_len);

    for (int i = 0; i < materials_len; i++) {
      ma_array[i] = eevee_object_material_get(ob, i, holdout);
      memset(&shgrps_array[i], 0, sizeof(EeveeMaterialShadingGroups));
      gpumat_array[i] = NULL;
      gpumat_depth_array[i] = NULL;

      switch (ma_array[i]->blend_method) {
        case MA_BM_SOLID:
        case MA_BM_CLIP:
        case MA_BM_HASHED:
          material_opaque(ma_array[i],
                          material_hash,
                          sldata,
                          vedata,
                          &gpumat_array[i],
                          &gpumat_depth_array[i],
                          &shgrps_array[i],
                          holdout);
          break;
        case MA_BM_BLEND:
          material_transparent(ma_array[i], sldata, vedata, &gpumat_array[i], &shgrps_array[i]);
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
                                GPU_material_has_volume_output(gpumat_array[0]));

    if ((ob->dt >= OB_SOLID) || DRW_state_is_image_render()) {
      /* Get per-material split surface */
      struct GPUBatch **mat_geom = NULL;

      if (!use_sculpt_pbvh) {
        mat_geom = DRW_cache_object_surface_material_get(ob, gpumat_array, materials_len);
      }

      if (use_sculpt_pbvh) {
        /* Vcol is not supported in the modes that require PBVH drawing. */
        const bool use_vcol = false;
        struct DRWShadingGroup **sculpt_shgrps_array = BLI_array_alloca(sculpt_shgrps_array,
                                                                        materials_len);
        for (int i = 0; i < materials_len; i++) {
          sculpt_shgrps_array[i] = shgrps_array[i].shading_grp;
        }
        DRW_shgroup_call_sculpt_with_materials(sculpt_shgrps_array, materials_len, ob, use_vcol);

        for (int i = 0; i < materials_len; i++) {
          sculpt_shgrps_array[i] = shgrps_array[i].depth_grp;
        }
        DRW_shgroup_call_sculpt_with_materials(sculpt_shgrps_array, materials_len, ob, use_vcol);
        for (int i = 0; i < materials_len; i++) {
          sculpt_shgrps_array[i] = shgrps_array[i].depth_clip_grp;
        }
        DRW_shgroup_call_sculpt_with_materials(sculpt_shgrps_array, materials_len, ob, use_vcol);

        for (int renderpass_index = 0;
             renderpass_index < stl->g_data->render_passes_material_count;
             renderpass_index++) {
          for (int i = 0; i < materials_len; i++) {
            sculpt_shgrps_array[i] = shgrps_array[i].material_accum_grp[renderpass_index];
          }
          DRW_shgroup_call_sculpt_with_materials(sculpt_shgrps_array, materials_len, ob, use_vcol);
        }

        /* TODO(fclem): Support shadows in sculpt mode. */
      }
      else if (mat_geom) {
        for (int i = 0; i < materials_len; i++) {
          if (mat_geom[i] == NULL) {
            continue;
          }

          /* Do not render surface if we are rendering a volume object
           * and do not have a surface closure. */
          if (use_volume_material &&
              (gpumat_array[i] && !GPU_material_has_surface_output(gpumat_array[i]))) {
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
          EeveeMaterialShadingGroups *shgrps = &shgrps_array[i];
          ADD_SHGROUP_CALL(shgrps->shading_grp, ob, mat_geom[i], oedata);
          ADD_SHGROUP_CALL_SAFE(shgrps->depth_grp, ob, mat_geom[i], oedata);
          ADD_SHGROUP_CALL_SAFE(shgrps->depth_clip_grp, ob, mat_geom[i], oedata);
          for (int renderpass_index = 0;
               renderpass_index < stl->g_data->render_passes_material_count;
               renderpass_index++) {
            ADD_SHGROUP_CALL_SAFE(
                shgrps->material_accum_grp[renderpass_index], ob, mat_geom[i], oedata);
          }

          /* Shadow Pass */
          struct GPUMaterial *gpumat;
          const bool use_gpumat = (ma_array[i]->use_nodes && ma_array[i]->nodetree);
          char blend_shadow = use_gpumat ? ma_array[i]->blend_shadow : MA_BS_SOLID;
          switch (blend_shadow) {
            case MA_BS_SOLID:
              EEVEE_shadows_caster_add(sldata, stl, mat_geom[i], ob);
              *cast_shadow = true;
              break;
            case MA_BS_CLIP:
              gpumat = EEVEE_material_mesh_depth_get(scene, ma_array[i], false, true);
              EEVEE_shadows_caster_material_add(
                  sldata, psl, gpumat, mat_geom[i], ob, &ma_array[i]->alpha_threshold);
              *cast_shadow = true;
              break;
            case MA_BS_HASHED:
              gpumat = EEVEE_material_mesh_depth_get(scene, ma_array[i], true, true);
              EEVEE_shadows_caster_material_add(sldata, psl, gpumat, mat_geom[i], ob, NULL);
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
    if (use_volume_material) {
      EEVEE_volumes_cache_object_add(sldata, vedata, scene, ob);
    }
  }
}

void EEVEE_particle_hair_cache_populate(EEVEE_Data *vedata,
                                        EEVEE_ViewLayerData *sldata,
                                        Object *ob,
                                        bool *cast_shadow)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();

  if (ob->type == OB_MESH) {
    if (ob != draw_ctx->object_edit) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
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
        eevee_hair_cache_populate(vedata, sldata, ob, psys, md, part->omat, cast_shadow);
      }
    }
  }
}

void EEVEE_object_hair_cache_populate(EEVEE_Data *vedata,
                                      EEVEE_ViewLayerData *sldata,
                                      Object *ob,
                                      bool *cast_shadow)
{
  eevee_hair_cache_populate(vedata, sldata, ob, NULL, NULL, HAIR_MATERIAL_NR, cast_shadow);
}

void EEVEE_materials_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;

  BLI_ghash_free(stl->g_data->material_hash, NULL, MEM_freeN);

  SET_FLAG_FROM_TEST(stl->effects->enabled_effects, e_data.sss_count > 0, EFFECT_SSS);

  /* TODO(fclem) this is not really clean. Init should not be done in cache finish. */
  EEVEE_subsurface_draw_init(sldata, vedata);
}

void EEVEE_materials_free(void)
{
  for (int i = 0; i < VAR_MAT_MAX; i++) {
    DRW_SHADER_FREE_SAFE(e_data.default_lit[i]);
  }
  MEM_SAFE_FREE(e_data.frag_shader_lib);
  MEM_SAFE_FREE(e_data.vert_shader_str);
  MEM_SAFE_FREE(e_data.vert_shadow_shader_str);
  MEM_SAFE_FREE(e_data.vert_background_shader_str);
  MEM_SAFE_FREE(e_data.vert_volume_shader_str);
  MEM_SAFE_FREE(e_data.geom_volume_shader_str);
  MEM_SAFE_FREE(e_data.volume_shader_lib);
  DRW_SHADER_FREE_SAFE(e_data.default_hair_prepass_sh);
  DRW_SHADER_FREE_SAFE(e_data.default_hair_prepass_clip_sh);
  DRW_SHADER_FREE_SAFE(e_data.default_prepass_sh);
  DRW_SHADER_FREE_SAFE(e_data.default_prepass_clip_sh);
  DRW_SHADER_FREE_SAFE(e_data.default_background);
  DRW_SHADER_FREE_SAFE(e_data.update_noise_sh);
  DRW_TEXTURE_FREE_SAFE(e_data.util_tex);
  DRW_TEXTURE_FREE_SAFE(e_data.noise_tex);
}

void EEVEE_materials_draw_opaque(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_PassList *psl)
{
  for (int i = 0; i < VAR_MAT_MAX; i++) {
    if (psl->default_pass[i]) {
      DRW_draw_pass(psl->default_pass[i]);
    }
  }

  DRW_draw_pass(psl->material_pass);
  DRW_draw_pass(psl->material_pass_cull);
}

/* -------------------------------------------------------------------- */

/** \name Render Passes
 * \{ */

void EEVEE_material_renderpasses_init(EEVEE_Data *vedata)
{
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PrivateData *g_data = stl->g_data;

  /* For diffuse and glossy we calculate the final light + color buffer where we extract the
   * light from by dividing by the color buffer. When one the light is requested we also tag
   * the color buffer to do the extraction. */
  if (g_data->render_passes & EEVEE_RENDER_PASS_DIFFUSE_LIGHT) {
    g_data->render_passes |= EEVEE_RENDER_PASS_DIFFUSE_COLOR;
  }
  if (g_data->render_passes & EEVEE_RENDER_PASS_SPECULAR_LIGHT) {
    g_data->render_passes |= EEVEE_RENDER_PASS_SPECULAR_COLOR;
  }

  /* Calculate the number of material based render passes */
  uint num_render_passes = count_bits_i(stl->g_data->render_passes & EEVEE_RENDERPASSES_MATERIAL);
  if ((num_render_passes != 0 && stl->g_data->render_passes & EEVEE_RENDER_PASS_ENVIRONMENT) ==
      0) {
    num_render_passes += 1;
  }
  stl->g_data->render_passes_material_count = num_render_passes;
}

void EEVEE_material_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, uint tot_samples)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  EEVEE_FramebufferList *fbl = vedata->fbl;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_EffectsInfo *effects = stl->effects;

  float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  /* Create FrameBuffer. */

  /* Should be enough precision for many samples. */
  const eGPUTextureFormat texture_format_material_accum = (tot_samples > 128) ? GPU_RGBA32F :
                                                                                GPU_RGBA16F;
  const eViewLayerEEVEEPassType render_passes = stl->g_data->render_passes &
                                                EEVEE_RENDERPASSES_MATERIAL;
  if (render_passes != 0) {
    GPU_framebuffer_ensure_config(&fbl->material_accum_fb,
                                  {GPU_ATTACHMENT_TEXTURE(dtxl->depth), GPU_ATTACHMENT_LEAVE});
    int render_pass_index = ((render_passes & EEVEE_RENDER_PASS_ENVIRONMENT) != 0) ? 0 : 1;
    for (int bit = 0; bit < 32; bit++) {
      eViewLayerEEVEEPassType bitflag = (1 << bit);
      if ((render_passes & bitflag) != 0) {

        DRW_texture_ensure_fullscreen_2d(
            &txl->material_accum[render_pass_index], texture_format_material_accum, 0);

        /* Clear texture. */
        if (DRW_state_is_image_render() || effects->taa_current_sample == 1) {
          GPU_framebuffer_texture_attach(
              fbl->material_accum_fb, txl->material_accum[render_pass_index], 0, 0);
          GPU_framebuffer_bind(fbl->material_accum_fb);
          GPU_framebuffer_clear_color(fbl->material_accum_fb, clear);
          GPU_framebuffer_bind(fbl->main_fb);
          GPU_framebuffer_texture_detach(fbl->material_accum_fb,
                                         txl->material_accum[render_pass_index]);
        }
        render_pass_index++;
      }
    }

    if ((render_passes & EEVEE_RENDER_PASS_SPECULAR_LIGHT) &&
        (effects->enabled_effects & EFFECT_SSR)) {
      EEVEE_reflection_output_init(sldata, vedata, tot_samples);
    }

    if (render_passes & EEVEE_RENDER_PASS_ENVIRONMENT) {
      Scene *scene = draw_ctx->scene;
      World *wo = scene->world;

      if (wo && wo->use_nodes && wo->nodetree) {
        struct GPUMaterial *gpumat = EEVEE_material_world_background_get(scene, wo);
        if (GPU_material_status(gpumat) == GPU_MAT_SUCCESS) {
          DRWShadingGroup *grp = DRW_shgroup_material_create(gpumat, psl->material_accum_pass[0]);
          add_background_uniforms(grp, sldata, vedata);
          DRW_shgroup_call(grp, DRW_cache_fullscreen_quad_get(), NULL);
        }
      }
    }
  }
}

void EEVEE_material_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_TextureList *txl = vedata->txl;

  if (fbl->material_accum_fb != NULL) {
    for (int renderpass_index = 0; renderpass_index < stl->g_data->render_passes_material_count;
         renderpass_index++) {
      if (txl->material_accum[renderpass_index] != NULL) {
        GPU_framebuffer_texture_attach(
            fbl->material_accum_fb, txl->material_accum[renderpass_index], 0, 0);
        GPU_framebuffer_bind(fbl->material_accum_fb);
        DRW_draw_pass(psl->material_accum_pass[renderpass_index]);
        GPU_framebuffer_bind(fbl->main_fb);
        GPU_framebuffer_texture_detach(fbl->material_accum_fb,
                                       txl->material_accum[renderpass_index]);
      }
    }
    if ((stl->g_data->render_passes & EEVEE_RENDER_PASS_SPECULAR_LIGHT) &&
        (stl->effects->enabled_effects & EFFECT_SSR)) {
      EEVEE_reflection_output_accumulate(sldata, vedata);
    }
  }
}

int EEVEE_material_output_pass_index_get(EEVEE_ViewLayerData *UNUSED(sldata),
                                         EEVEE_Data *vedata,
                                         eViewLayerEEVEEPassType renderpass_type)
{
  EEVEE_StorageList *stl = vedata->stl;

  BLI_assert((stl->g_data->render_passes & EEVEE_RENDERPASSES_MATERIAL) != 0);
  BLI_assert((stl->g_data->render_passes & EEVEE_RENDERPASSES_MATERIAL & renderpass_type) != 0);

  /* pass_index 0 is reserved for the environment pass. */
  if ((stl->g_data->render_passes & EEVEE_RENDER_PASS_ENVIRONMENT & renderpass_type) != 0) {
    return 0;
  }

  /* pass_index 0 is reserved for the environment pass. Other passes start from index 1 */
  int index = 1;
  eViewLayerEEVEEPassType active_material_passes = stl->g_data->render_passes &
                                                   EEVEE_RENDERPASSES_MATERIAL &
                                                   ~EEVEE_RENDER_PASS_ENVIRONMENT;

  for (int bitshift = 0; bitshift < 32; bitshift++) {
    eViewLayerEEVEEPassType pass_flag = (1 << bitshift);
    if (pass_flag == renderpass_type) {
      break;
    }
    if (active_material_passes & pass_flag) {
      index++;
    }
  }

  return index;
}

/* Get the pass index that contains the color pass for the given renderpass_type. */
int EEVEE_material_output_color_pass_index_get(EEVEE_ViewLayerData *sldata,
                                               EEVEE_Data *vedata,
                                               eViewLayerEEVEEPassType renderpass_type)
{
  BLI_assert(
      ELEM(renderpass_type, EEVEE_RENDER_PASS_DIFFUSE_LIGHT, EEVEE_RENDER_PASS_SPECULAR_LIGHT));
  eViewLayerEEVEEPassType color_pass_type;
  switch (renderpass_type) {
    case EEVEE_RENDER_PASS_DIFFUSE_LIGHT:
      color_pass_type = EEVEE_RENDER_PASS_DIFFUSE_COLOR;
      break;
    case EEVEE_RENDER_PASS_SPECULAR_LIGHT:
      color_pass_type = EEVEE_RENDER_PASS_SPECULAR_COLOR;
      break;
    default:
      color_pass_type = 0;
      BLI_assert(false);
  }
  return EEVEE_material_output_pass_index_get(sldata, vedata, color_pass_type);
}
/* \} */
