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
 * \ingroup DNA
 */

#ifndef __EEVEE_PRIVATE_H__
#define __EEVEE_PRIVATE_H__

#include "DNA_lightprobe_types.h"

struct EEVEE_BoundSphere;
struct EEVEE_ShadowCasterBuffer;
struct GPUFrameBuffer;
struct Object;
struct RenderLayer;
struct RenderResult;

extern struct DrawEngineType draw_engine_eevee_type;

/* Minimum UBO is 16384 bytes */
#define MAX_PROBE 128 /* TODO : find size by dividing UBO max size by probe data size */
#define MAX_GRID 64   /* TODO : find size by dividing UBO max size by grid data size */
#define MAX_PLANAR 16 /* TODO : find size by dividing UBO max size by grid data size */
#define MAX_LIGHT 128 /* TODO : find size by dividing UBO max size by light data size */
#define MAX_CASCADE_NUM 4
#define MAX_SHADOW 256 /* TODO : Make this depends on GL_MAX_ARRAY_TEXTURE_LAYERS */
#define MAX_SHADOW_CASCADE 8
#define MAX_SHADOW_CUBE (MAX_SHADOW - MAX_CASCADE_NUM * MAX_SHADOW_CASCADE)
#define MAX_BLOOM_STEP 16

/* Only define one of these. */
// #define IRRADIANCE_SH_L2
// #define IRRADIANCE_CUBEMAP
#define IRRADIANCE_HL2
#define HAMMERSLEY_SIZE 1024

#if defined(IRRADIANCE_SH_L2)
#  define SHADER_IRRADIANCE "#define IRRADIANCE_SH_L2\n"
#elif defined(IRRADIANCE_CUBEMAP)
#  define SHADER_IRRADIANCE "#define IRRADIANCE_CUBEMAP\n"
#elif defined(IRRADIANCE_HL2)
#  define SHADER_IRRADIANCE "#define IRRADIANCE_HL2\n"
#endif

/* Macro causes over indentation. */
/* clang-format off */
#define SHADER_DEFINES \
  "#define EEVEE_ENGINE\n" \
  "#define MAX_PROBE " STRINGIFY(MAX_PROBE) "\n" \
  "#define MAX_GRID " STRINGIFY(MAX_GRID) "\n" \
  "#define MAX_PLANAR " STRINGIFY(MAX_PLANAR) "\n" \
  "#define MAX_LIGHT " STRINGIFY(MAX_LIGHT) "\n" \
  "#define MAX_SHADOW " STRINGIFY(MAX_SHADOW) "\n" \
  "#define MAX_SHADOW_CUBE " STRINGIFY(MAX_SHADOW_CUBE) "\n" \
  "#define MAX_SHADOW_CASCADE " STRINGIFY(MAX_SHADOW_CASCADE) "\n" \
  "#define MAX_CASCADE_NUM " STRINGIFY(MAX_CASCADE_NUM) "\n" \
  SHADER_IRRADIANCE
/* clang-format on */

#define SWAP_DOUBLE_BUFFERS() \
  { \
    if (effects->swap_double_buffer) { \
      SWAP(struct GPUFrameBuffer *, fbl->main_fb, fbl->double_buffer_fb); \
      SWAP(struct GPUFrameBuffer *, fbl->main_color_fb, fbl->double_buffer_color_fb); \
      SWAP(GPUTexture *, txl->color, txl->color_double_buffer); \
      effects->swap_double_buffer = false; \
    } \
  } \
  ((void)0)

#define SWAP_BUFFERS() \
  { \
    if (effects->target_buffer == fbl->effect_color_fb) { \
      SWAP_DOUBLE_BUFFERS(); \
      effects->source_buffer = txl->color_post; \
      effects->target_buffer = fbl->main_color_fb; \
    } \
    else { \
      SWAP_DOUBLE_BUFFERS(); \
      effects->source_buffer = txl->color; \
      effects->target_buffer = fbl->effect_color_fb; \
    } \
  } \
  ((void)0)

#define SWAP_BUFFERS_TAA() \
  { \
    if (effects->target_buffer == fbl->effect_color_fb) { \
      SWAP(struct GPUFrameBuffer *, fbl->effect_fb, fbl->taa_history_fb); \
      SWAP(struct GPUFrameBuffer *, fbl->effect_color_fb, fbl->taa_history_color_fb); \
      SWAP(GPUTexture *, txl->color_post, txl->taa_history); \
      effects->source_buffer = txl->taa_history; \
      effects->target_buffer = fbl->effect_color_fb; \
    } \
    else { \
      SWAP(struct GPUFrameBuffer *, fbl->main_fb, fbl->taa_history_fb); \
      SWAP(struct GPUFrameBuffer *, fbl->main_color_fb, fbl->taa_history_color_fb); \
      SWAP(GPUTexture *, txl->color, txl->taa_history); \
      effects->source_buffer = txl->taa_history; \
      effects->target_buffer = fbl->main_color_fb; \
    } \
  } \
  ((void)0)

#define OVERLAY_ENABLED(v3d) ((v3d) && (v3d->flag2 & V3D_HIDE_OVERLAYS) == 0)
#define LOOK_DEV_MODE_ENABLED(v3d) ((v3d) && (v3d->shading.type == OB_MATERIAL))
#define LOOK_DEV_OVERLAY_ENABLED(v3d) \
  (LOOK_DEV_MODE_ENABLED(v3d) && OVERLAY_ENABLED(v3d) && \
   (v3d->overlay.flag & V3D_OVERLAY_LOOK_DEV))
#define USE_SCENE_LIGHT(v3d) \
  ((!v3d) || (!LOOK_DEV_MODE_ENABLED(v3d)) || \
   ((LOOK_DEV_MODE_ENABLED(v3d) && (v3d->shading.flag & V3D_SHADING_SCENE_LIGHTS))))
#define LOOK_DEV_STUDIO_LIGHT_ENABLED(v3d) \
  (LOOK_DEV_MODE_ENABLED(v3d) && !(v3d->shading.flag & V3D_SHADING_SCENE_WORLD))

#define OCTAHEDRAL_SIZE_FROM_CUBESIZE(cube_size) \
  ((int)ceilf(sqrtf((cube_size * cube_size) * 6.0f)))
#define MIN_CUBE_LOD_LEVEL 3
#define MAX_PLANAR_LOD_LEVEL 9

/* World shader variations */
enum {
  VAR_WORLD_BACKGROUND = 0,
  VAR_WORLD_PROBE = 1,
  VAR_WORLD_VOLUME = 2,
};

/* Material shader variations */
enum {
  VAR_MAT_MESH = (1 << 0),
  VAR_MAT_PROBE = (1 << 1),
  VAR_MAT_HAIR = (1 << 2),
  VAR_MAT_BLEND = (1 << 3),
  VAR_MAT_VSM = (1 << 4),
  VAR_MAT_ESM = (1 << 5),
  VAR_MAT_VOLUME = (1 << 6),
  VAR_MAT_LOOKDEV = (1 << 7),
  /* Max number of variation */
  /* IMPORTANT : Leave it last and set
   * it's value accordingly. */
  VAR_MAT_MAX = (1 << 8),
  /* These are options that are not counted in VAR_MAT_MAX
   * because they are not cumulative with the others above. */
  VAR_MAT_CLIP = (1 << 9),
  VAR_MAT_HASH = (1 << 10),
  VAR_MAT_MULT = (1 << 11),
  VAR_MAT_SHADOW = (1 << 12),
  VAR_MAT_REFRACT = (1 << 13),
  VAR_MAT_TRANSLUC = (1 << 15),
  VAR_MAT_SSSALBED = (1 << 16),
};

/* ************ PROBE UBO ************* */

/* They are the same struct as their Cache siblings.
 * typedef'ing just to keep the naming consistent with
 * other eevee types. */
typedef LightProbeCache EEVEE_LightProbe;
typedef LightGridCache EEVEE_LightGrid;

typedef struct EEVEE_PlanarReflection {
  float plane_equation[4];
  float clip_vec_x[3], attenuation_scale;
  float clip_vec_y[3], attenuation_bias;
  float clip_edge_x_pos, clip_edge_x_neg;
  float clip_edge_y_pos, clip_edge_y_neg;
  float facing_scale, facing_bias, clipsta, pad;
  float reflectionmat[4][4]; /* Used for sampling the texture. */
  float mtx[4][4];           /* Not used in shader. TODO move elsewhere. */
} EEVEE_PlanarReflection;

/* --------------------------------------- */

typedef struct EEVEE_BoundSphere {
  float center[3], radius;
} EEVEE_BoundSphere;

typedef struct EEVEE_BoundBox {
  float center[3], halfdim[3];
} EEVEE_BoundBox;

typedef struct EEVEE_PassList {
  /* Shadows */
  struct DRWPass *shadow_pass;
  struct DRWPass *shadow_cube_copy_pass;
  struct DRWPass *shadow_cube_store_pass;
  struct DRWPass *shadow_cube_store_high_pass;
  struct DRWPass *shadow_cascade_copy_pass;
  struct DRWPass *shadow_cascade_store_pass;
  struct DRWPass *shadow_cascade_store_high_pass;

  /* Probes */
  struct DRWPass *probe_background;
  struct DRWPass *probe_glossy_compute;
  struct DRWPass *probe_diffuse_compute;
  struct DRWPass *probe_visibility_compute;
  struct DRWPass *probe_grid_fill;
  struct DRWPass *probe_display;
  struct DRWPass *probe_planar_downsample_ps;

  /* Effects */
  struct DRWPass *ao_horizon_search;
  struct DRWPass *ao_horizon_search_layer;
  struct DRWPass *ao_horizon_debug;
  struct DRWPass *ao_accum_ps;
  struct DRWPass *mist_accum_ps;
  struct DRWPass *motion_blur;
  struct DRWPass *bloom_blit;
  struct DRWPass *bloom_downsample_first;
  struct DRWPass *bloom_downsample;
  struct DRWPass *bloom_upsample;
  struct DRWPass *bloom_resolve;
  struct DRWPass *dof_down;
  struct DRWPass *dof_scatter;
  struct DRWPass *dof_resolve;
  struct DRWPass *volumetric_world_ps;
  struct DRWPass *volumetric_objects_ps;
  struct DRWPass *volumetric_scatter_ps;
  struct DRWPass *volumetric_integration_ps;
  struct DRWPass *volumetric_resolve_ps;
  struct DRWPass *ssr_raytrace;
  struct DRWPass *ssr_resolve;
  struct DRWPass *sss_blur_ps;
  struct DRWPass *sss_resolve_ps;
  struct DRWPass *sss_accum_ps;
  struct DRWPass *color_downsample_ps;
  struct DRWPass *color_downsample_cube_ps;
  struct DRWPass *velocity_resolve;
  struct DRWPass *taa_resolve;
  struct DRWPass *alpha_checker;

  /* HiZ */
  struct DRWPass *minz_downlevel_ps;
  struct DRWPass *maxz_downlevel_ps;
  struct DRWPass *minz_downdepth_ps;
  struct DRWPass *maxz_downdepth_ps;
  struct DRWPass *minz_downdepth_layer_ps;
  struct DRWPass *maxz_downdepth_layer_ps;
  struct DRWPass *minz_copydepth_ps;
  struct DRWPass *maxz_copydepth_ps;
  struct DRWPass *maxz_copydepth_layer_ps;

  struct DRWPass *depth_pass;
  struct DRWPass *depth_pass_cull;
  struct DRWPass *depth_pass_clip;
  struct DRWPass *depth_pass_clip_cull;
  struct DRWPass *refract_depth_pass;
  struct DRWPass *refract_depth_pass_cull;
  struct DRWPass *refract_depth_pass_clip;
  struct DRWPass *refract_depth_pass_clip_cull;
  struct DRWPass *default_pass[VAR_MAT_MAX];
  struct DRWPass *sss_pass;
  struct DRWPass *sss_pass_cull;
  struct DRWPass *material_pass;
  struct DRWPass *material_pass_cull;
  struct DRWPass *refract_pass;
  struct DRWPass *transparent_pass;
  struct DRWPass *background_pass;
  struct DRWPass *update_noise_pass;
  struct DRWPass *lookdev_glossy_pass;
  struct DRWPass *lookdev_diffuse_pass;
} EEVEE_PassList;

typedef struct EEVEE_FramebufferList {
  /* Effects */
  struct GPUFrameBuffer *gtao_fb;
  struct GPUFrameBuffer *gtao_debug_fb;
  struct GPUFrameBuffer *downsample_fb;
  struct GPUFrameBuffer *bloom_blit_fb;
  struct GPUFrameBuffer *bloom_down_fb[MAX_BLOOM_STEP];
  struct GPUFrameBuffer *bloom_accum_fb[MAX_BLOOM_STEP - 1];
  struct GPUFrameBuffer *sss_blur_fb;
  struct GPUFrameBuffer *sss_blit_fb;
  struct GPUFrameBuffer *sss_resolve_fb;
  struct GPUFrameBuffer *sss_clear_fb;
  struct GPUFrameBuffer *sss_accum_fb;
  struct GPUFrameBuffer *dof_down_fb;
  struct GPUFrameBuffer *dof_scatter_fb;
  struct GPUFrameBuffer *volumetric_fb;
  struct GPUFrameBuffer *volumetric_scat_fb;
  struct GPUFrameBuffer *volumetric_integ_fb;
  struct GPUFrameBuffer *screen_tracing_fb;
  struct GPUFrameBuffer *refract_fb;
  struct GPUFrameBuffer *mist_accum_fb;
  struct GPUFrameBuffer *ao_accum_fb;
  struct GPUFrameBuffer *velocity_resolve_fb;

  struct GPUFrameBuffer *update_noise_fb;

  struct GPUFrameBuffer *planarref_fb;
  struct GPUFrameBuffer *planar_downsample_fb;

  struct GPUFrameBuffer *main_fb;
  struct GPUFrameBuffer *main_color_fb;
  struct GPUFrameBuffer *effect_fb;
  struct GPUFrameBuffer *effect_color_fb;
  struct GPUFrameBuffer *double_buffer_fb;
  struct GPUFrameBuffer *double_buffer_color_fb;
  struct GPUFrameBuffer *double_buffer_depth_fb;
  struct GPUFrameBuffer *taa_history_fb;
  struct GPUFrameBuffer *taa_history_color_fb;
} EEVEE_FramebufferList;

typedef struct EEVEE_TextureList {
  /* Effects */
  struct GPUTexture *color_post; /* R16_G16_B16 */
  struct GPUTexture *mist_accum;
  struct GPUTexture *ao_accum;
  struct GPUTexture *sss_dir_accum;
  struct GPUTexture *sss_col_accum;
  struct GPUTexture *refract_color;
  struct GPUTexture *taa_history;

  struct GPUTexture *volume_prop_scattering;
  struct GPUTexture *volume_prop_extinction;
  struct GPUTexture *volume_prop_emission;
  struct GPUTexture *volume_prop_phase;
  struct GPUTexture *volume_scatter;
  struct GPUTexture *volume_transmit;
  struct GPUTexture *volume_scatter_history;
  struct GPUTexture *volume_transmit_history;

  struct GPUTexture *lookdev_grid_tx;
  struct GPUTexture *lookdev_cube_tx;

  struct GPUTexture *planar_pool;
  struct GPUTexture *planar_depth;

  struct GPUTexture *maxzbuffer;

  struct GPUTexture *color; /* R16_G16_B16 */
  struct GPUTexture *color_double_buffer;
  struct GPUTexture *depth_double_buffer;
} EEVEE_TextureList;

typedef struct EEVEE_StorageList {
  /* Effects */
  struct EEVEE_EffectsInfo *effects;

  struct EEVEE_PrivateData *g_data;

  struct LightCache *lookdev_lightcache;
  EEVEE_LightProbe *lookdev_cube_data;
  EEVEE_LightGrid *lookdev_grid_data;
  LightCacheTexture *lookdev_cube_mips;
} EEVEE_StorageList;

/* ************ LIGHT UBO ************* */
typedef struct EEVEE_Light {
  float position[3], invsqrdist;
  float color[3], spec;
  float spotsize, spotblend, radius, shadowid;
  float rightvec[3], sizex;
  float upvec[3], sizey;
  float forwardvec[3], light_type;
} EEVEE_Light;

/* Special type for elliptic area lights, matches lamps_lib.glsl */
#define LAMPTYPE_AREA_ELLIPSE 100.0f

typedef struct EEVEE_Shadow {
  float near, far, bias, exp;
  float shadow_start, data_start, multi_shadow_count, shadow_blur;
  float contact_dist, contact_bias, contact_spread, contact_thickness;
} EEVEE_Shadow;

typedef struct EEVEE_ShadowCube {
  float position[3], pad;
} EEVEE_ShadowCube;

typedef struct EEVEE_ShadowCascade {
  /* World->Light->NDC->Tex : used for sampling the shadow map. */
  float shadowmat[MAX_CASCADE_NUM][4][4];
  float split_start[4];
  float split_end[4];
} EEVEE_ShadowCascade;

typedef struct EEVEE_ShadowRender {
  float position[3], pad;
  float cube_texel_size;
  float stored_texel_size;
  float clip_near;
  float clip_far;
  int shadow_samples_len;
  float shadow_samples_len_inv;
  float exponent;
} EEVEE_ShadowRender;

/* This is just a really long bitflag with special function to access it. */
#define MAX_LIGHTBITS_FIELDS (MAX_LIGHT / 8)
typedef struct EEVEE_LightBits {
  uchar fields[MAX_LIGHTBITS_FIELDS];
} EEVEE_LightBits;

typedef struct EEVEE_ShadowCaster {
  struct EEVEE_LightBits bits;
  struct EEVEE_BoundBox bbox;
} EEVEE_ShadowCaster;

typedef struct EEVEE_ShadowCasterBuffer {
  struct EEVEE_ShadowCaster *shadow_casters;
  char *flags;
  uint alloc_count;
  uint count;
} EEVEE_ShadowCasterBuffer;

/* ************ LIGHT DATA ************* */
typedef struct EEVEE_LightsInfo {
  int num_light, cache_num_light;
  int num_cube_layer, cache_num_cube_layer;
  int num_cascade_layer, cache_num_cascade_layer;
  int gpu_cube_len, gpu_cascade_len, gpu_shadow_len;
  int cpu_cube_len, cpu_cascade_len;
  int update_flag;
  int shadow_cube_size, shadow_cascade_size, shadow_method;
  bool shadow_high_bitdepth, soft_shadows;
  int shadow_cube_store_size;
  int current_shadow_cascade;
  int current_shadow_face;
  uint shadow_instance_count;
  float filter_size;
  /* List of lights in the scene. */
  /* XXX This is fragile, can get out of sync quickly. */
  struct Object *light_ref[MAX_LIGHT];
  struct Object *shadow_cube_ref[MAX_SHADOW_CUBE];
  struct Object *shadow_cascade_ref[MAX_SHADOW_CASCADE];
  /* UBO Storage : data used by UBO */
  struct EEVEE_Light light_data[MAX_LIGHT];
  struct EEVEE_ShadowRender shadow_render_data;
  struct EEVEE_Shadow shadow_data[MAX_SHADOW];
  struct EEVEE_ShadowCube shadow_cube_data[MAX_SHADOW_CUBE];
  struct EEVEE_ShadowCascade shadow_cascade_data[MAX_SHADOW_CASCADE];
  /* Lights tracking */
  int new_shadow_id[MAX_LIGHT]; /* To be able to convert old bitfield to new bitfield */
  struct EEVEE_BoundSphere shadow_bounds[MAX_LIGHT]; /* Tightly packed light bounds  */
  /* Pointers only. */
  struct EEVEE_ShadowCasterBuffer *shcaster_frontbuffer;
  struct EEVEE_ShadowCasterBuffer *shcaster_backbuffer;
} EEVEE_LightsInfo;

/* EEVEE_LightsInfo->shadow_casters_flag */
enum {
  SHADOW_CASTER_PRUNED = (1 << 0),
  SHADOW_CASTER_UPDATED = (1 << 1),
};

/* EEVEE_LightsInfo->update_flag */
enum {
  LIGHT_UPDATE_SHADOW_CUBE = (1 << 0),
};

/* ************ PROBE DATA ************* */
typedef struct EEVEE_LightProbeVisTest {
  struct Collection *collection; /* Skip test if NULL */
  bool invert;
  bool cached; /* Reuse last test results */
} EEVEE_LightProbeVisTest;

typedef struct EEVEE_LightProbesInfo {
  int num_cube, cache_num_cube;
  int num_grid, cache_num_grid;
  int num_planar, cache_num_planar;
  int total_irradiance_samples; /* Total for all grids */
  int cache_irradiance_size[3];
  int update_flag;
  int updated_bounce;
  int num_bounce;
  int cubemap_res;
  /* Update */
  bool do_cube_update;
  bool do_grid_update;
  /* For rendering probes */
  float probemat[6][4][4];
  int layer;
  float texel_size;
  float padding_size;
  float samples_len;
  float samples_len_inv;
  float near_clip;
  float far_clip;
  float roughness;
  float firefly_fac;
  float lodfactor;
  float lod_rt_max, lod_cube_max, lod_planar_max;
  float visibility_range;
  float visibility_blur;
  float intensity_fac;
  int shres;
  EEVEE_LightProbeVisTest planar_vis_tests[MAX_PLANAR];
  /* UBO Storage : data used by UBO */
  EEVEE_LightProbe probe_data[MAX_PROBE];
  EEVEE_LightGrid grid_data[MAX_GRID];
  EEVEE_PlanarReflection planar_data[MAX_PLANAR];
  /* Probe Visibility Collection */
  EEVEE_LightProbeVisTest vis_data;
} EEVEE_LightProbesInfo;

/* EEVEE_LightProbesInfo->update_flag */
enum {
  PROBE_UPDATE_CUBE = (1 << 0),
  PROBE_UPDATE_GRID = (1 << 1),
  PROBE_UPDATE_ALL = 0xFFFFFF,
};

/* ************ EFFECTS DATA ************* */

typedef enum EEVEE_EffectsFlag {
  EFFECT_MOTION_BLUR = (1 << 0),
  EFFECT_BLOOM = (1 << 1),
  EFFECT_DOF = (1 << 2),
  EFFECT_VOLUMETRIC = (1 << 3),
  EFFECT_SSR = (1 << 4),
  EFFECT_DOUBLE_BUFFER = (1 << 5), /* Not really an effect but a feature */
  EFFECT_REFRACT = (1 << 6),
  EFFECT_GTAO = (1 << 7),
  EFFECT_TAA = (1 << 8),
  EFFECT_POST_BUFFER = (1 << 9),    /* Not really an effect but a feature */
  EFFECT_NORMAL_BUFFER = (1 << 10), /* Not really an effect but a feature */
  EFFECT_SSS = (1 << 11),
  EFFECT_VELOCITY_BUFFER = (1 << 12),     /* Not really an effect but a feature */
  EFFECT_TAA_REPROJECT = (1 << 13),       /* should be mutually exclusive with EFFECT_TAA */
  EFFECT_DEPTH_DOUBLE_BUFFER = (1 << 14), /* Not really an effect but a feature */
  EFFECT_ALPHA_CHECKER = (1 << 15),       /* Not really an effect but a feature */
} EEVEE_EffectsFlag;

typedef struct EEVEE_EffectsInfo {
  EEVEE_EffectsFlag enabled_effects;
  bool swap_double_buffer;
  /* SSSS */
  int sss_sample_count;
  bool sss_separate_albedo;
  struct GPUTexture *sss_data; /* Textures from pool */
  struct GPUTexture *sss_albedo;
  struct GPUTexture *sss_blur;
  struct GPUTexture *sss_stencil;
  /* Volumetrics */
  int volume_current_sample;
  struct GPUTexture *volume_scatter;
  struct GPUTexture *volume_transmit;
  /* SSR */
  bool reflection_trace_full;
  bool ssr_was_persp;
  int ssr_neighbor_ofs;
  int ssr_halfres_ofs[2];
  struct GPUTexture *ssr_normal_input; /* Textures from pool */
  struct GPUTexture *ssr_specrough_input;
  struct GPUTexture *ssr_hit_output;
  struct GPUTexture *ssr_pdf_output;
  /* Temporal Anti Aliasing */
  int taa_reproject_sample;
  int taa_current_sample;
  int taa_render_sample;
  int taa_total_sample;
  float taa_alpha;
  bool prev_drw_support;
  float prev_drw_persmat[4][4];
  struct DRWView *taa_view;
  /* Ambient Occlusion */
  int ao_depth_layer;
  struct GPUTexture *ao_src_depth;  /* pointer copy */
  struct GPUTexture *gtao_horizons; /* Textures from pool */
  struct GPUTexture *gtao_horizons_debug;
  /* Motion Blur */
  float current_world_to_ndc[4][4];
  float current_ndc_to_world[4][4];
  float past_world_to_ndc[4][4];
  int motion_blur_samples;
  bool motion_blur_mat_cached;
  /* Velocity Pass */
  float velocity_curr_persinv[4][4];
  float velocity_past_persmat[4][4];
  struct GPUTexture *velocity_tx; /* Texture from pool */
  /* Depth Of Field */
  float dof_near_far[2];
  float dof_params[2];
  float dof_bokeh[4];
  float dof_bokeh_sides[4];
  int dof_target_size[2];
  struct GPUTexture *dof_down_near; /* Textures from pool */
  struct GPUTexture *dof_down_far;
  struct GPUTexture *dof_coc;
  struct GPUTexture *dof_blur;
  struct GPUTexture *dof_blur_alpha;
  /* Alpha Checker */
  float color_checker_dark[4];
  float color_checker_light[4];
  struct DRWView *checker_view;
  /* Other */
  float prev_persmat[4][4];
  /* Lookdev */
  int sphere_size;
  int anchor[2];
  struct DRWView *lookdev_view;
  /* Bloom */
  int bloom_iteration_len;
  float source_texel_size[2];
  float blit_texel_size[2];
  float downsamp_texel_size[MAX_BLOOM_STEP][2];
  float bloom_color[3];
  float bloom_clamp;
  float bloom_sample_scale;
  float bloom_curve_threshold[4];
  float unf_source_texel_size[2];
  struct GPUTexture *bloom_blit; /* Textures from pool */
  struct GPUTexture *bloom_downsample[MAX_BLOOM_STEP];
  struct GPUTexture *bloom_upsample[MAX_BLOOM_STEP - 1];
  struct GPUTexture *unf_source_buffer; /* pointer copy */
  struct GPUTexture *unf_base_buffer;   /* pointer copy */
  /* Not alloced, just a copy of a *GPUtexture in EEVEE_TextureList. */
  struct GPUTexture *source_buffer;     /* latest updated texture */
  struct GPUFrameBuffer *target_buffer; /* next target to render to */
  struct GPUTexture *final_tx;          /* Final color to transform to display color space. */
  struct GPUFrameBuffer *final_fb;      /* Framebuffer with final_tx as attachement. */
} EEVEE_EffectsInfo;

/* ***************** COMMON DATA **************** */

/* Common uniform buffer containing all "constant" data over the whole drawing pipeline. */
/* !! CAUTION !!
 * - [i]vec3 need to be padded to [i]vec4 (even in ubo declaration).
 * - Make sure that [i]vec4 start at a multiple of 16 bytes.
 * - Arrays of vec2/vec3 are padded as arrays of vec4.
 * - sizeof(bool) == sizeof(int) in GLSL so use int in C */
typedef struct EEVEE_CommonUniformBuffer {
  float prev_persmat[4][4]; /* mat4 */
  float view_vecs[2][4];    /* vec4[2] */
  float mip_ratio[10][4];   /* vec2[10] */
  /* Ambient Occlusion */
  /* -- 16 byte aligned -- */
  float ao_dist, pad1, ao_factor, pad2;                    /* vec4 */
  float ao_offset, ao_bounce_fac, ao_quality, ao_settings; /* vec4 */
  /* Volumetric */
  /* -- 16 byte aligned -- */
  int vol_tex_size[3], pad3;         /* ivec3 */
  float vol_depth_param[3], pad4;    /* vec3 */
  float vol_inv_tex_size[3], pad5;   /* vec3 */
  float vol_jitter[3], pad6;         /* vec3 */
  float vol_coord_scale[2], pad7[2]; /* vec2 */
  /* -- 16 byte aligned -- */
  float vol_history_alpha; /* float */
  float vol_light_clamp;   /* float */
  float vol_shadow_steps;  /* float */
  int vol_use_lights;      /* bool */
  /* Screen Space Reflections */
  /* -- 16 byte aligned -- */
  float ssr_quality, ssr_thickness, ssr_pixelsize[2]; /* vec4 */
  float ssr_border_fac;                               /* float */
  float ssr_max_roughness;                            /* float */
  float ssr_firefly_fac;                              /* float */
  float ssr_brdf_bias;                                /* float */
  int ssr_toggle;                                     /* bool */
  /* SubSurface Scattering */
  float sss_jitter_threshold; /* float */
  int sss_toggle;             /* bool */
  /* Specular */
  int spec_toggle; /* bool */
  /* Lights */
  int la_num_light; /* int */
  /* Probes */
  int prb_num_planar;          /* int */
  int prb_num_render_cube;     /* int */
  int prb_num_render_grid;     /* int */
  int prb_irradiance_vis_size; /* int */
  float prb_irradiance_smooth; /* float */
  float prb_lod_cube_max;      /* float */
  float prb_lod_planar_max;    /* float */
  /* Misc */
  int hiz_mip_offset; /* int */
  int ray_type;       /* int */
  float ray_depth;    /* float */
} EEVEE_CommonUniformBuffer;

/* ray_type (keep in sync with rayType) */
#define EEVEE_RAY_CAMERA 0
#define EEVEE_RAY_SHADOW 1
#define EEVEE_RAY_DIFFUSE 2
#define EEVEE_RAY_GLOSSY 3

/* ************** SCENE LAYER DATA ************** */
typedef struct EEVEE_ViewLayerData {
  /* Lights */
  struct EEVEE_LightsInfo *lights;

  struct GPUUniformBuffer *light_ubo;
  struct GPUUniformBuffer *shadow_ubo;
  struct GPUUniformBuffer *shadow_render_ubo;
  struct GPUUniformBuffer *shadow_samples_ubo;

  struct GPUFrameBuffer *shadow_cube_target_fb;
  struct GPUFrameBuffer *shadow_cube_store_fb;
  struct GPUFrameBuffer *shadow_cascade_target_fb;
  struct GPUFrameBuffer *shadow_cascade_store_fb;

  struct GPUTexture *shadow_cube_target;
  struct GPUTexture *shadow_cube_blur;
  struct GPUTexture *shadow_cascade_target;
  struct GPUTexture *shadow_cascade_blur;
  struct GPUTexture *shadow_cube_pool;
  struct GPUTexture *shadow_cascade_pool;

  struct EEVEE_ShadowCasterBuffer shcasters_buffers[2];

  /* Probes */
  struct EEVEE_LightProbesInfo *probes;

  struct GPUUniformBuffer *probe_ubo;
  struct GPUUniformBuffer *grid_ubo;
  struct GPUUniformBuffer *planar_ubo;

  /* Common Uniform Buffer */
  struct EEVEE_CommonUniformBuffer common_data;
  struct GPUUniformBuffer *common_ubo;

  struct LightCache *fallback_lightcache;
} EEVEE_ViewLayerData;

/* ************ OBJECT DATA ************ */

typedef struct EEVEE_LightData {
  short light_id, shadow_id;
} EEVEE_LightData;

typedef struct EEVEE_ShadowCubeData {
  short light_id, shadow_id, cube_id, layer_id;
} EEVEE_ShadowCubeData;

typedef struct EEVEE_ShadowCascadeData {
  short light_id, shadow_id, cascade_id, layer_id;
  /* World->Light->NDC : used for rendering the shadow map. */
  float viewprojmat[MAX_CASCADE_NUM][4][4]; /* Could be removed. */
  float projmat[MAX_CASCADE_NUM][4][4];
  float viewmat[4][4], viewinv[4][4];
  float radius[MAX_CASCADE_NUM];
} EEVEE_ShadowCascadeData;

/* Theses are the structs stored inside Objects.
 * It works with even if the object is in multiple layers
 * because we don't get the same "Object *" for each layer. */
typedef struct EEVEE_LightEngineData {
  DrawData dd;

  bool need_update;
  /* This needs to be out of the union to avoid undefined behavior. */
  short prev_cube_shadow_id;
  union {
    struct EEVEE_LightData ld;
    struct EEVEE_ShadowCubeData scd;
    struct EEVEE_ShadowCascadeData scad;
  } data;
} EEVEE_LightEngineData;

typedef struct EEVEE_LightProbeEngineData {
  DrawData dd;

  bool need_update;
} EEVEE_LightProbeEngineData;

typedef struct EEVEE_ObjectEngineData {
  DrawData dd;

  Object *ob; /* self reference */
  EEVEE_LightProbeVisTest *test_data;
  bool ob_vis, ob_vis_dirty;

  bool need_update;
  uint shadow_caster_id;
} EEVEE_ObjectEngineData;

typedef struct EEVEE_WorldEngineData {
  DrawData dd;
} EEVEE_WorldEngineData;

/* *********************************** */

typedef struct EEVEE_Data {
  void *engine_type;
  EEVEE_FramebufferList *fbl;
  EEVEE_TextureList *txl;
  EEVEE_PassList *psl;
  EEVEE_StorageList *stl;
} EEVEE_Data;

typedef struct EEVEE_PrivateData {
  struct DRWShadingGroup *shadow_shgrp;
  struct DRWShadingGroup *depth_shgrp;
  struct DRWShadingGroup *depth_shgrp_cull;
  struct DRWShadingGroup *depth_shgrp_clip;
  struct DRWShadingGroup *depth_shgrp_clip_cull;
  struct DRWShadingGroup *refract_depth_shgrp;
  struct DRWShadingGroup *refract_depth_shgrp_cull;
  struct DRWShadingGroup *refract_depth_shgrp_clip;
  struct DRWShadingGroup *refract_depth_shgrp_clip_cull;
  struct DRWCallBuffer *planar_display_shgrp;
  struct GHash *material_hash;
  float background_alpha; /* TODO find a better place for this. */
  /* Chosen lightcache: can come from Lookdev or the viewlayer. */
  struct LightCache *light_cache;
  /* For planar probes */
  float planar_texel_size[2];
  /* For double buffering */
  bool view_updated;
  bool valid_double_buffer;
  bool valid_taa_history;
  /* Render Matrices */
  float studiolight_matrix[3][3];
  float overscan, overscan_pixels;
  float size_orig[2];

  /* Mist Settings */
  float mist_start, mist_inv_dist, mist_falloff;

  /* Color Management */
  bool use_color_render_settings;

  /* LookDev Settings */
  int studiolight_index;
  float studiolight_rot_z;
  int studiolight_cubemap_res;
  float studiolight_glossy_clamp;
  float studiolight_filter_quality;

  /** For rendering probes and shadows. */
  struct DRWView *cube_views[6];
  struct DRWView *planar_views[MAX_PLANAR];
} EEVEE_PrivateData; /* Transient data */

/* eevee_data.c */
void EEVEE_view_layer_data_free(void *sldata);
EEVEE_ViewLayerData *EEVEE_view_layer_data_get(void);
EEVEE_ViewLayerData *EEVEE_view_layer_data_ensure_ex(struct ViewLayer *view_layer);
EEVEE_ViewLayerData *EEVEE_view_layer_data_ensure(void);
EEVEE_ObjectEngineData *EEVEE_object_data_get(Object *ob);
EEVEE_ObjectEngineData *EEVEE_object_data_ensure(Object *ob);
EEVEE_LightProbeEngineData *EEVEE_lightprobe_data_get(Object *ob);
EEVEE_LightProbeEngineData *EEVEE_lightprobe_data_ensure(Object *ob);
EEVEE_LightEngineData *EEVEE_light_data_get(Object *ob);
EEVEE_LightEngineData *EEVEE_light_data_ensure(Object *ob);
EEVEE_WorldEngineData *EEVEE_world_data_get(World *wo);
EEVEE_WorldEngineData *EEVEE_world_data_ensure(World *wo);

/* eevee_materials.c */
struct GPUTexture *EEVEE_materials_get_util_tex(void); /* XXX */
void EEVEE_materials_init(EEVEE_ViewLayerData *sldata,
                          EEVEE_StorageList *stl,
                          EEVEE_FramebufferList *fbl);
void EEVEE_materials_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_materials_cache_populate(EEVEE_Data *vedata,
                                    EEVEE_ViewLayerData *sldata,
                                    Object *ob,
                                    bool *cast_shadow);
void EEVEE_hair_cache_populate(EEVEE_Data *vedata,
                               EEVEE_ViewLayerData *sldata,
                               Object *ob,
                               bool *cast_shadow);
void EEVEE_materials_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
struct GPUMaterial *EEVEE_material_world_lightprobe_get(struct Scene *scene, struct World *wo);
struct GPUMaterial *EEVEE_material_world_background_get(struct Scene *scene, struct World *wo);
struct GPUMaterial *EEVEE_material_world_volume_get(struct Scene *scene, struct World *wo);
struct GPUMaterial *EEVEE_material_mesh_get(struct Scene *scene,
                                            Material *ma,
                                            EEVEE_Data *vedata,
                                            bool use_blend,
                                            bool use_multiply,
                                            bool use_refract,
                                            bool use_translucency,
                                            int shadow_method);
struct GPUMaterial *EEVEE_material_mesh_volume_get(struct Scene *scene, Material *ma);
struct GPUMaterial *EEVEE_material_mesh_depth_get(struct Scene *scene,
                                                  Material *ma,
                                                  bool use_hashed_alpha,
                                                  bool is_shadow);
struct GPUMaterial *EEVEE_material_hair_get(struct Scene *scene, Material *ma, int shadow_method);
void EEVEE_materials_free(void);
void EEVEE_draw_default_passes(EEVEE_PassList *psl);
void EEVEE_update_noise(EEVEE_PassList *psl, EEVEE_FramebufferList *fbl, const double offsets[3]);
void EEVEE_update_viewvecs(float invproj[4][4], float winmat[4][4], float (*r_viewvecs)[4]);

/* eevee_lights.c */
void EEVEE_lights_init(EEVEE_ViewLayerData *sldata);
void EEVEE_lights_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_lights_cache_add(EEVEE_ViewLayerData *sldata, struct Object *ob);
void EEVEE_lights_cache_shcaster_add(EEVEE_ViewLayerData *sldata,
                                     EEVEE_StorageList *stl,
                                     struct GPUBatch *geom,
                                     Object *ob);
void EEVEE_lights_cache_shcaster_material_add(EEVEE_ViewLayerData *sldata,
                                              EEVEE_PassList *psl,
                                              struct GPUMaterial *gpumat,
                                              struct GPUBatch *geom,
                                              struct Object *ob,
                                              float *alpha_threshold);
void EEVEE_lights_cache_shcaster_object_add(EEVEE_ViewLayerData *sldata, struct Object *ob);
void EEVEE_lights_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_lights_update(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_draw_shadows(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, struct DRWView *view);
void EEVEE_lights_free(void);

/* eevee_shaders.c */
void EEVEE_shaders_lightprobe_shaders_init(void);
struct GPUShader *EEVEE_shaders_probe_filter_glossy_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_default_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_filter_diffuse_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_filter_visibility_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_grid_fill_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_planar_downsample_sh_get(void);
struct GPUShader *EEVEE_shaders_default_studiolight_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_cube_display_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_grid_display_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_planar_display_sh_get(void);
struct GPUShader *EEVEE_shaders_velocity_resolve_sh_get(void);
struct GPUShader *EEVEE_shaders_taa_resolve_sh_get(EEVEE_EffectsFlag enabled_effects);
void EEVEE_shaders_free(void);

/* eevee_lightprobes.c */
bool EEVEE_lightprobes_obj_visibility_cb(bool vis_in, void *user_data);
void EEVEE_lightprobes_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_lightprobes_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_lightprobes_cache_add(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, Object *ob);
void EEVEE_lightprobes_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_lightprobes_refresh(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_lightprobes_refresh_planar(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_lightprobes_free(void);

void EEVEE_lightbake_cache_init(EEVEE_ViewLayerData *sldata,
                                EEVEE_Data *vedata,
                                GPUTexture *rt_color,
                                GPUTexture *rt_depth);
void EEVEE_lightbake_render_world(EEVEE_ViewLayerData *sldata,
                                  EEVEE_Data *vedata,
                                  struct GPUFrameBuffer *face_fb[6]);
void EEVEE_lightbake_render_scene(EEVEE_ViewLayerData *sldata,
                                  EEVEE_Data *vedata,
                                  struct GPUFrameBuffer *face_fb[6],
                                  const float pos[3],
                                  float near_clip,
                                  float far_clip);
void EEVEE_lightbake_filter_glossy(EEVEE_ViewLayerData *sldata,
                                   EEVEE_Data *vedata,
                                   struct GPUTexture *rt_color,
                                   struct GPUFrameBuffer *fb,
                                   int probe_idx,
                                   float intensity,
                                   int maxlevel,
                                   float filter_quality,
                                   float firefly_fac);
void EEVEE_lightbake_filter_diffuse(EEVEE_ViewLayerData *sldata,
                                    EEVEE_Data *vedata,
                                    struct GPUTexture *rt_color,
                                    struct GPUFrameBuffer *fb,
                                    int grid_offset,
                                    float intensity);
void EEVEE_lightbake_filter_visibility(EEVEE_ViewLayerData *sldata,
                                       EEVEE_Data *vedata,
                                       struct GPUTexture *rt_depth,
                                       struct GPUFrameBuffer *fb,
                                       int grid_offset,
                                       float clipsta,
                                       float clipend,
                                       float vis_range,
                                       float vis_blur,
                                       int vis_size);

void EEVEE_lightprobes_grid_data_from_object(Object *ob, EEVEE_LightGrid *prb_data, int *offset);
void EEVEE_lightprobes_cube_data_from_object(Object *ob, EEVEE_LightProbe *prb_data);
void EEVEE_lightprobes_planar_data_from_object(Object *ob,
                                               EEVEE_PlanarReflection *eplanar,
                                               EEVEE_LightProbeVisTest *vis_test);

/* eevee_depth_of_field.c */
int EEVEE_depth_of_field_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, Object *camera);
void EEVEE_depth_of_field_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_depth_of_field_draw(EEVEE_Data *vedata);
void EEVEE_depth_of_field_free(void);

/* eevee_bloom.c */
int EEVEE_bloom_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_bloom_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_bloom_draw(EEVEE_Data *vedata);
void EEVEE_bloom_free(void);

/* eevee_occlusion.c */
int EEVEE_occlusion_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_occlusion_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_occlusion_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_occlusion_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_occlusion_compute(EEVEE_ViewLayerData *sldata,
                             EEVEE_Data *vedata,
                             struct GPUTexture *depth_src,
                             int layer);
void EEVEE_occlusion_draw_debug(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_occlusion_free(void);

/* eevee_screen_raytrace.c */
int EEVEE_screen_raytrace_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_screen_raytrace_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_refraction_compute(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_reflection_compute(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_screen_raytrace_free(void);

/* eevee_subsurface.c */
void EEVEE_subsurface_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_draw_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_add_pass(EEVEE_ViewLayerData *sldata,
                               EEVEE_Data *vedata,
                               uint sss_id,
                               struct GPUUniformBuffer *sss_profile);
void EEVEE_subsurface_data_render(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_compute(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_free(void);

/* eevee_motion_blur.c */
int EEVEE_motion_blur_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, Object *camera);
void EEVEE_motion_blur_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_motion_blur_draw(EEVEE_Data *vedata);
void EEVEE_motion_blur_free(void);

/* eevee_mist.c */
void EEVEE_mist_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_mist_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_mist_free(void);

/* eevee_temporal_sampling.c */
void EEVEE_temporal_sampling_reset(EEVEE_Data *vedata);
int EEVEE_temporal_sampling_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_temporal_sampling_offset_calc(const double ht_point[2],
                                         const float filter_size,
                                         float r_offset[2]);
void EEVEE_temporal_sampling_matrices_calc(EEVEE_EffectsInfo *effects, const double ht_point[2]);
void EEVEE_temporal_sampling_update_matrices(EEVEE_Data *vedata);
void EEVEE_temporal_sampling_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_temporal_sampling_draw(EEVEE_Data *vedata);

/* eevee_volumes.c */
void EEVEE_volumes_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_volumes_set_jitter(EEVEE_ViewLayerData *sldata, uint current_sample);
void EEVEE_volumes_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_volumes_cache_object_add(EEVEE_ViewLayerData *sldata,
                                    EEVEE_Data *vedata,
                                    struct Scene *scene,
                                    Object *ob);
void EEVEE_volumes_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_volumes_draw_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_volumes_compute(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_volumes_resolve(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_volumes_free_smoke_textures(void);
void EEVEE_volumes_free(void);

/* eevee_effects.c */
void EEVEE_effects_init(EEVEE_ViewLayerData *sldata,
                        EEVEE_Data *vedata,
                        Object *camera,
                        const bool minimal);
void EEVEE_effects_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_effects_draw_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_create_minmax_buffer(EEVEE_Data *vedata, struct GPUTexture *depth_src, int layer);
void EEVEE_downsample_buffer(EEVEE_Data *vedata, struct GPUTexture *texture_src, int level);
void EEVEE_downsample_cube_buffer(EEVEE_Data *vedata, struct GPUTexture *texture_src, int level);
void EEVEE_draw_alpha_checker(EEVEE_Data *vedata);
void EEVEE_draw_effects(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_effects_free(void);

/* eevee_render.c */
void EEVEE_render_init(EEVEE_Data *vedata,
                       struct RenderEngine *engine,
                       struct Depsgraph *depsgraph);
void EEVEE_render_cache(void *vedata,
                        struct Object *ob,
                        struct RenderEngine *engine,
                        struct Depsgraph *depsgraph);
void EEVEE_render_draw(EEVEE_Data *vedata,
                       struct RenderEngine *engine,
                       struct RenderLayer *render_layer,
                       const struct rcti *rect);
void EEVEE_render_update_passes(struct RenderEngine *engine,
                                struct Scene *scene,
                                struct ViewLayer *view_layer);

/** eevee_lookdev.c */
void EEVEE_lookdev_cache_init(EEVEE_Data *vedata,
                              DRWShadingGroup **grp,
                              DRWPass *pass,
                              float background_alpha,
                              struct World *world,
                              EEVEE_LightProbesInfo *pinfo);
void EEVEE_lookdev_draw(EEVEE_Data *vedata);

/** eevee_engine.c */
void EEVEE_cache_populate(void *vedata, Object *ob);

/* Shadow Matrix */
static const float texcomat[4][4] = {
    /* From NDC to TexCo */
    {0.5f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.5f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.5f, 0.0f},
    {0.5f, 0.5f, 0.5f, 1.0f},
};

/* Cubemap Matrices */
static const float cubefacemat[6][4][4] = {
    /* Pos X */
    {{0.0f, 0.0f, -1.0f, 0.0f},
     {0.0f, -1.0f, 0.0f, 0.0f},
     {-1.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 1.0f}},
    /* Neg X */
    {{0.0f, 0.0f, 1.0f, 0.0f},
     {0.0f, -1.0f, 0.0f, 0.0f},
     {1.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 1.0f}},
    /* Pos Y */
    {{1.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, -1.0f, 0.0f},
     {0.0f, 1.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 1.0f}},
    /* Neg Y */
    {{1.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 1.0f, 0.0f},
     {0.0f, -1.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 1.0f}},
    /* Pos Z */
    {{1.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, -1.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, -1.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 1.0f}},
    /* Neg Z */
    {{-1.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, -1.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 1.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 1.0f}},
};

#endif /* __EEVEE_PRIVATE_H__ */
