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

#pragma once

#include "DRW_render.h"

#include "BLI_bitmap.h"

#include "DNA_lightprobe_types.h"

#include "GPU_viewport.h"

#include "BKE_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

struct EEVEE_ShadowCasterBuffer;
struct GPUFrameBuffer;
struct Object;
struct RenderLayer;

extern struct DrawEngineType draw_engine_eevee_type;

/* Minimum UBO is 16384 bytes */
#define MAX_PROBE 128 /* TODO : find size by dividing UBO max size by probe data size */
#define MAX_GRID 64   /* TODO : find size by dividing UBO max size by grid data size */
#define MAX_PLANAR 16 /* TODO : find size by dividing UBO max size by grid data size */
#define MAX_LIGHT 128 /* TODO : find size by dividing UBO max size by light data size */
#define MAX_CASCADE_NUM 4
#define MAX_SHADOW 128 /* TODO : Make this depends on GL_MAX_ARRAY_TEXTURE_LAYERS */
#define MAX_SHADOW_CASCADE 8
#define MAX_SHADOW_CUBE (MAX_SHADOW - MAX_CASCADE_NUM * MAX_SHADOW_CASCADE)
#define MAX_BLOOM_STEP 16
#define MAX_AOVS 64

/* Special value chosen to not be altered by depth of field sample count. */
#define TAA_MAX_SAMPLE 10000926

// #define DEBUG_SHADOW_DISTRIBUTION

/* Only define one of these. */
// #define IRRADIANCE_SH_L2
#define IRRADIANCE_HL2
#define HAMMERSLEY_SIZE 1024

#if defined(IRRADIANCE_SH_L2)
#  define SHADER_IRRADIANCE "#define IRRADIANCE_SH_L2\n"
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

#define EEVEE_PROBE_MAX min_ii(MAX_PROBE, GPU_max_texture_layers() / 6)
#define EEVEE_VELOCITY_TILE_SIZE 32
#define USE_VOLUME_OPTI (GPU_shader_image_load_store_support())

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

BLI_INLINE bool eevee_hdri_preview_overlay_enabled(const View3D *v3d)
{
  /* Only show the HDRI Preview in Shading Preview in the Viewport. */
  if (v3d == NULL || v3d->shading.type != OB_MATERIAL) {
    return false;
  }

  /* Only show the HDRI Preview when viewing the Combined render pass */
  if (v3d->shading.render_pass != SCE_PASS_COMBINED) {
    return false;
  }

  return ((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0) && (v3d->overlay.flag & V3D_OVERLAY_LOOK_DEV);
}

#define USE_SCENE_LIGHT(v3d) \
  ((!v3d) || \
   ((v3d->shading.type == OB_MATERIAL) && (v3d->shading.flag & V3D_SHADING_SCENE_LIGHTS)) || \
   ((v3d->shading.type == OB_RENDER) && (v3d->shading.flag & V3D_SHADING_SCENE_LIGHTS_RENDER)))
#define LOOK_DEV_STUDIO_LIGHT_ENABLED(v3d) \
  ((v3d) && (((v3d->shading.type == OB_MATERIAL) && \
              ((v3d->shading.flag & V3D_SHADING_SCENE_WORLD) == 0)) || \
             ((v3d->shading.type == OB_RENDER) && \
              ((v3d->shading.flag & V3D_SHADING_SCENE_WORLD_RENDER) == 0))))

#define MIN_CUBE_LOD_LEVEL 3
#define MAX_SCREEN_BUFFERS_LOD_LEVEL 6

/* All the renderpasses that use the GPUMaterial for accumulation */
#define EEVEE_RENDERPASSES_MATERIAL \
  (EEVEE_RENDER_PASS_EMIT | EEVEE_RENDER_PASS_DIFFUSE_COLOR | EEVEE_RENDER_PASS_DIFFUSE_LIGHT | \
   EEVEE_RENDER_PASS_SPECULAR_COLOR | EEVEE_RENDER_PASS_SPECULAR_LIGHT | \
   EEVEE_RENDER_PASS_ENVIRONMENT | EEVEE_RENDER_PASS_AOV)
#define EEVEE_AOV_HASH_ALL -1
#define EEVEE_AOV_HASH_COLOR_TYPE_MASK 1
#define MAX_CRYPTOMATTE_LAYERS 3

/* Material shader variations */
enum {
  VAR_MAT_MESH = (1 << 0),
  VAR_MAT_VOLUME = (1 << 1),
  VAR_MAT_HAIR = (1 << 2),
  /* VAR_MAT_PROBE = (1 << 3), UNUSED */
  VAR_MAT_BLEND = (1 << 4),
  VAR_MAT_LOOKDEV = (1 << 5),
  VAR_MAT_HOLDOUT = (1 << 6),
  VAR_MAT_HASH = (1 << 7),
  VAR_MAT_DEPTH = (1 << 8),
  VAR_MAT_REFRACT = (1 << 9),
  VAR_WORLD_BACKGROUND = (1 << 10),
  VAR_WORLD_PROBE = (1 << 11),
  VAR_WORLD_VOLUME = (1 << 12),
  VAR_DEFAULT = (1 << 13),
};

/* Material shader cache keys */
enum {
  /* HACK: This assumes the struct GPUShader will never be smaller than our variations.
   * This allow us to only keep one ghash and avoid bigger keys comparisons/hashing.
   * We combine the GPUShader pointer with the key. */
  KEY_CULL = (1 << 0),
  KEY_REFRACT = (1 << 1),
  KEY_HAIR = (1 << 2),
  KEY_SHADOW = (1 << 3),
};

/* SSR shader variations */
typedef enum EEVEE_SSRShaderOptions {
  SSR_RESOLVE = (1 << 0),
  SSR_FULL_TRACE = (1 << 1),
  SSR_MAX_SHADER = (1 << 2),
} EEVEE_SSRShaderOptions;

/* DOF Gather pass shader variations */
typedef enum EEVEE_DofGatherPass {
  DOF_GATHER_FOREGROUND = 0,
  DOF_GATHER_BACKGROUND = 1,
  DOF_GATHER_HOLEFILL = 2,

  DOF_GATHER_MAX_PASS,
} EEVEE_DofGatherPass;

#define DOF_TILE_DIVISOR 16
#define DOF_BOKEH_LUT_SIZE 32
#define DOF_GATHER_RING_COUNT 5
#define DOF_DILATE_RING_COUNT 3
#define DOF_FAST_GATHER_COC_ERROR 0.05

#define DOF_SHADER_DEFINES \
  "#define DOF_TILE_DIVISOR " STRINGIFY(DOF_TILE_DIVISOR) "\n" \
  "#define DOF_BOKEH_LUT_SIZE " STRINGIFY(DOF_BOKEH_LUT_SIZE) "\n" \
  "#define DOF_GATHER_RING_COUNT " STRINGIFY(DOF_GATHER_RING_COUNT) "\n" \
  "#define DOF_DILATE_RING_COUNT " STRINGIFY(DOF_DILATE_RING_COUNT) "\n" \
  "#define DOF_FAST_GATHER_COC_ERROR " STRINGIFY(DOF_FAST_GATHER_COC_ERROR) "\n"

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

typedef struct EEVEE_BoundBox {
  float center[3], halfdim[3];
} EEVEE_BoundBox;

typedef struct EEVEE_PassList {
  /* Shadows */
  struct DRWPass *shadow_pass;
  struct DRWPass *shadow_accum_pass;

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
  struct DRWPass *bloom_accum_ps;
  struct DRWPass *dof_setup;
  struct DRWPass *dof_flatten_tiles;
  struct DRWPass *dof_dilate_tiles_minmax;
  struct DRWPass *dof_dilate_tiles_minabs;
  struct DRWPass *dof_reduce_copy;
  struct DRWPass *dof_downsample;
  struct DRWPass *dof_reduce;
  struct DRWPass *dof_bokeh;
  struct DRWPass *dof_gather_fg;
  struct DRWPass *dof_gather_fg_holefill;
  struct DRWPass *dof_gather_bg;
  struct DRWPass *dof_scatter_fg;
  struct DRWPass *dof_scatter_bg;
  struct DRWPass *dof_filter;
  struct DRWPass *dof_resolve;
  struct DRWPass *volumetric_world_ps;
  struct DRWPass *volumetric_objects_ps;
  struct DRWPass *volumetric_scatter_ps;
  struct DRWPass *volumetric_integration_ps;
  struct DRWPass *volumetric_resolve_ps;
  struct DRWPass *volumetric_accum_ps;
  struct DRWPass *ssr_raytrace;
  struct DRWPass *ssr_resolve;
  struct DRWPass *sss_blur_ps;
  struct DRWPass *sss_resolve_ps;
  struct DRWPass *sss_translucency_ps;
  struct DRWPass *color_copy_ps;
  struct DRWPass *color_downsample_ps;
  struct DRWPass *color_downsample_cube_ps;
  struct DRWPass *velocity_object;
  struct DRWPass *velocity_hair;
  struct DRWPass *velocity_resolve;
  struct DRWPass *velocity_tiles_x;
  struct DRWPass *velocity_tiles;
  struct DRWPass *velocity_tiles_expand[2];
  struct DRWPass *taa_resolve;
  struct DRWPass *alpha_checker;

  /* HiZ */
  struct DRWPass *maxz_downlevel_ps;
  struct DRWPass *maxz_copydepth_ps;
  struct DRWPass *maxz_copydepth_layer_ps;

  /* Renderpass Accumulation. */
  struct DRWPass *material_accum_ps;
  struct DRWPass *background_accum_ps;
  struct DRWPass *cryptomatte_ps;

  struct DRWPass *depth_ps;
  struct DRWPass *depth_cull_ps;
  struct DRWPass *depth_clip_ps;
  struct DRWPass *depth_clip_cull_ps;
  struct DRWPass *depth_refract_ps;
  struct DRWPass *depth_refract_cull_ps;
  struct DRWPass *depth_refract_clip_ps;
  struct DRWPass *depth_refract_clip_cull_ps;
  struct DRWPass *material_ps;
  struct DRWPass *material_cull_ps;
  struct DRWPass *material_refract_ps;
  struct DRWPass *material_refract_cull_ps;
  struct DRWPass *material_sss_ps;
  struct DRWPass *material_sss_cull_ps;
  struct DRWPass *transparent_pass;
  struct DRWPass *background_ps;
  struct DRWPass *update_noise_pass;
  struct DRWPass *lookdev_glossy_pass;
  struct DRWPass *lookdev_diffuse_pass;
  struct DRWPass *renderpass_pass;
} EEVEE_PassList;

typedef struct EEVEE_FramebufferList {
  /* Effects */
  struct GPUFrameBuffer *gtao_fb;
  struct GPUFrameBuffer *gtao_debug_fb;
  struct GPUFrameBuffer *downsample_fb;
  struct GPUFrameBuffer *maxzbuffer_fb;
  struct GPUFrameBuffer *bloom_blit_fb;
  struct GPUFrameBuffer *bloom_down_fb[MAX_BLOOM_STEP];
  struct GPUFrameBuffer *bloom_accum_fb[MAX_BLOOM_STEP - 1];
  struct GPUFrameBuffer *bloom_pass_accum_fb;
  struct GPUFrameBuffer *cryptomatte_fb;
  struct GPUFrameBuffer *shadow_accum_fb;
  struct GPUFrameBuffer *ssr_accum_fb;
  struct GPUFrameBuffer *sss_blur_fb;
  struct GPUFrameBuffer *sss_blit_fb;
  struct GPUFrameBuffer *sss_resolve_fb;
  struct GPUFrameBuffer *sss_clear_fb;
  struct GPUFrameBuffer *sss_translucency_fb;
  struct GPUFrameBuffer *sss_accum_fb;
  struct GPUFrameBuffer *dof_setup_fb;
  struct GPUFrameBuffer *dof_flatten_tiles_fb;
  struct GPUFrameBuffer *dof_dilate_tiles_fb;
  struct GPUFrameBuffer *dof_downsample_fb;
  struct GPUFrameBuffer *dof_reduce_fb;
  struct GPUFrameBuffer *dof_reduce_copy_fb;
  struct GPUFrameBuffer *dof_bokeh_fb;
  struct GPUFrameBuffer *dof_gather_fg_fb;
  struct GPUFrameBuffer *dof_filter_fg_fb;
  struct GPUFrameBuffer *dof_gather_fg_holefill_fb;
  struct GPUFrameBuffer *dof_gather_bg_fb;
  struct GPUFrameBuffer *dof_filter_bg_fb;
  struct GPUFrameBuffer *dof_scatter_fg_fb;
  struct GPUFrameBuffer *dof_scatter_bg_fb;
  struct GPUFrameBuffer *volumetric_fb;
  struct GPUFrameBuffer *volumetric_scat_fb;
  struct GPUFrameBuffer *volumetric_integ_fb;
  struct GPUFrameBuffer *volumetric_accum_fb;
  struct GPUFrameBuffer *screen_tracing_fb;
  struct GPUFrameBuffer *mist_accum_fb;
  struct GPUFrameBuffer *material_accum_fb;
  struct GPUFrameBuffer *renderpass_fb;
  struct GPUFrameBuffer *ao_accum_fb;
  struct GPUFrameBuffer *velocity_resolve_fb;
  struct GPUFrameBuffer *velocity_fb;
  struct GPUFrameBuffer *velocity_tiles_fb[2];

  struct GPUFrameBuffer *update_noise_fb;

  struct GPUFrameBuffer *planarref_fb;
  struct GPUFrameBuffer *planar_downsample_fb;

  struct GPUFrameBuffer *main_fb;
  struct GPUFrameBuffer *main_color_fb;
  struct GPUFrameBuffer *effect_fb;
  struct GPUFrameBuffer *effect_color_fb;
  struct GPUFrameBuffer *radiance_filtered_fb;
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
  struct GPUTexture *sss_accum;
  struct GPUTexture *env_accum;
  struct GPUTexture *diff_color_accum;
  struct GPUTexture *diff_light_accum;
  struct GPUTexture *spec_color_accum;
  struct GPUTexture *spec_light_accum;
  struct GPUTexture *aov_surface_accum[MAX_AOVS];
  struct GPUTexture *emit_accum;
  struct GPUTexture *bloom_accum;
  struct GPUTexture *ssr_accum;
  struct GPUTexture *shadow_accum;
  struct GPUTexture *cryptomatte;
  struct GPUTexture *taa_history;
  /* Could not be pool texture because of mipmapping. */
  struct GPUTexture *dof_reduced_color;
  struct GPUTexture *dof_reduced_coc;

  struct GPUTexture *volume_prop_scattering;
  struct GPUTexture *volume_prop_extinction;
  struct GPUTexture *volume_prop_emission;
  struct GPUTexture *volume_prop_phase;
  struct GPUTexture *volume_scatter;
  struct GPUTexture *volume_transmit;
  struct GPUTexture *volume_scatter_history;
  struct GPUTexture *volume_transmit_history;
  struct GPUTexture *volume_scatter_accum;
  struct GPUTexture *volume_transmittance_accum;

  struct GPUTexture *lookdev_grid_tx;
  struct GPUTexture *lookdev_cube_tx;

  struct GPUTexture *planar_pool;
  struct GPUTexture *planar_depth;

  struct GPUTexture *maxzbuffer;
  struct GPUTexture *filtered_radiance;

  struct GPUTexture *renderpass;

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

/* ************ RENDERPASS UBO ************* */
typedef struct EEVEE_RenderPassData {
  int renderPassDiffuse;
  int renderPassDiffuseLight;
  int renderPassGlossy;
  int renderPassGlossyLight;
  int renderPassEmit;
  int renderPassSSSColor;
  int renderPassEnvironment;
  int renderPassAOV;
  int renderPassAOVActive;
  int _pad[3];
} EEVEE_RenderPassData;

/* ************ LIGHT UBO ************* */
typedef struct EEVEE_Light {
  float position[3], invsqrdist;
  float color[3], spec;
  float spotsize, spotblend, radius, shadow_id;
  float rightvec[3], sizex;
  float upvec[3], sizey;
  float forwardvec[3], light_type;
} EEVEE_Light;

/* Special type for elliptic area lights, matches lamps_lib.glsl */
#define LAMPTYPE_AREA_ELLIPSE 100.0f

typedef struct EEVEE_Shadow {
  float near, far, bias, type_data_id;
  float contact_dist, contact_bias, contact_spread, contact_thickness;
} EEVEE_Shadow;

typedef struct EEVEE_ShadowCube {
  float shadowmat[4][4];
  float position[3], _pad0[1];
} EEVEE_ShadowCube;

typedef struct EEVEE_ShadowCascade {
  /* World->Light->NDC->Tex : used for sampling the shadow map. */
  float shadowmat[MAX_CASCADE_NUM][4][4];
  float split_start[4];
  float split_end[4];
  float shadow_vec[3], tex_id;
} EEVEE_ShadowCascade;

typedef struct EEVEE_ShadowCascadeRender {
  /* World->Light->NDC : used for rendering the shadow map. */
  float projmat[MAX_CASCADE_NUM][4][4];
  float viewmat[4][4], viewinv[4][4];
  float radius[MAX_CASCADE_NUM];
  float original_bias;
  float cascade_max_dist;
  float cascade_exponent;
  float cascade_fade;
  int cascade_count;
} EEVEE_ShadowCascadeRender;

BLI_STATIC_ASSERT_ALIGN(EEVEE_Light, 16)
BLI_STATIC_ASSERT_ALIGN(EEVEE_Shadow, 16)
BLI_STATIC_ASSERT_ALIGN(EEVEE_ShadowCube, 16)
BLI_STATIC_ASSERT_ALIGN(EEVEE_ShadowCascade, 16)
BLI_STATIC_ASSERT_ALIGN(EEVEE_RenderPassData, 16)

BLI_STATIC_ASSERT(sizeof(EEVEE_Shadow) * MAX_SHADOW +
                          sizeof(EEVEE_ShadowCascade) * MAX_SHADOW_CASCADE +
                          sizeof(EEVEE_ShadowCube) * MAX_SHADOW_CUBE <
                      16384,
                  "Shadow UBO is too big!!!")

typedef struct EEVEE_ShadowCasterBuffer {
  struct EEVEE_BoundBox *bbox;
  BLI_bitmap *update;
  uint alloc_count;
  uint count;
} EEVEE_ShadowCasterBuffer;

/* ************ LIGHT DATA ************* */
typedef struct EEVEE_LightsInfo {
  int num_light, cache_num_light;
  int num_cube_layer, cache_num_cube_layer;
  int num_cascade_layer, cache_num_cascade_layer;
  int cube_len, cascade_len, shadow_len;
  int shadow_cube_size, shadow_cascade_size;
  bool shadow_high_bitdepth, soft_shadows;
  /* UBO Storage : data used by UBO */
  struct EEVEE_Light light_data[MAX_LIGHT];
  struct EEVEE_Shadow shadow_data[MAX_SHADOW];
  struct EEVEE_ShadowCube shadow_cube_data[MAX_SHADOW_CUBE];
  struct EEVEE_ShadowCascade shadow_cascade_data[MAX_SHADOW_CASCADE];
  /* Additional rendering info for cascade. */
  struct EEVEE_ShadowCascadeRender shadow_cascade_render[MAX_SHADOW_CASCADE];
  /* Back index in light_data. */
  uchar shadow_cube_light_indices[MAX_SHADOW_CUBE];
  uchar shadow_cascade_light_indices[MAX_SHADOW_CASCADE];
  /* Update bitmap. */
  BLI_bitmap sh_cube_update[BLI_BITMAP_SIZE(MAX_SHADOW_CUBE)];
  /* Lights tracking */
  struct BoundSphere shadow_bounds[MAX_LIGHT]; /* Tightly packed light bounds  */
  /* List of bbox and update bitmap. Double buffered. */
  struct EEVEE_ShadowCasterBuffer *shcaster_frontbuffer, *shcaster_backbuffer;
  /* AABB of all shadow casters combined. */
  struct {
    float min[3], max[3];
  } shcaster_aabb;
} EEVEE_LightsInfo;

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
  float lod_rt_max, lod_cube_max;
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

/* ************** MOTION BLUR ************ */

#define MB_PREV 0
#define MB_NEXT 1
#define MB_CURR 2

typedef struct EEVEE_MotionBlurData {
  struct GHash *object;
  struct GHash *geom;
  struct {
    float viewmat[4][4];
    float persmat[4][4];
    float persinv[4][4];
  } camera[3];
  DRWShadingGroup *hair_grp;
} EEVEE_MotionBlurData;

typedef struct EEVEE_ObjectKey {
  /** Object or source object for duplis */
  struct Object *ob;
  /** Parent object for duplis */
  struct Object *parent;
  /** Dupli objects recursive unique identifier */
  int id[8]; /* MAX_DUPLI_RECUR */
} EEVEE_ObjectKey;

typedef struct EEVEE_ObjectMotionData {
  float obmat[3][4][4];
} EEVEE_ObjectMotionData;

typedef enum eEEVEEMotionData {
  EEVEE_MOTION_DATA_MESH = 0,
  EEVEE_MOTION_DATA_HAIR,
} eEEVEEMotionData;

typedef struct EEVEE_HairMotionData {
  /** Needs to be first to ensure casting. */
  eEEVEEMotionData type;
  int use_deform;
  /** Allocator will alloc enough slot for all particle systems. Or 1 if it's a hair object. */
  int psys_len;
  struct {
    struct GPUVertBuf *hair_pos[2];    /* Position buffer for time = t +/- step. */
    struct GPUTexture *hair_pos_tx[2]; /* Buffer Texture of the corresponding VBO. */
  } psys[0];
} EEVEE_HairMotionData;

typedef struct EEVEE_GeometryMotionData {
  /** Needs to be first to ensure casting. */
  eEEVEEMotionData type;
  /** To disable deform mb if vertcount mismatch. */
  int use_deform;

  struct GPUBatch *batch;    /* Batch for time = t. */
  struct GPUVertBuf *vbo[2]; /* Vbo for time = t +/- step. */
} EEVEE_GeometryMotionData;

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
  EFFECT_POST_BUFFER = (1 << 9),      /* Not really an effect but a feature */
  EFFECT_NORMAL_BUFFER = (1 << 10),   /* Not really an effect but a feature */
  EFFECT_RADIANCE_BUFFER = (1 << 10), /* Not really an effect but a feature */
  EFFECT_SSS = (1 << 11),
  EFFECT_VELOCITY_BUFFER = (1 << 12),     /* Not really an effect but a feature */
  EFFECT_TAA_REPROJECT = (1 << 13),       /* should be mutually exclusive with EFFECT_TAA */
  EFFECT_DEPTH_DOUBLE_BUFFER = (1 << 14), /* Not really an effect but a feature */
} EEVEE_EffectsFlag;

typedef struct EEVEE_EffectsInfo {
  EEVEE_EffectsFlag enabled_effects;
  bool swap_double_buffer;
  /* SSSS */
  int sss_sample_count;
  int sss_surface_count;
  struct GPUTexture *sss_irradiance; /* Textures from pool */
  struct GPUTexture *sss_radius;
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
  bool ssr_was_valid_double_buffer;
  int ssr_neighbor_ofs;
  int ssr_halfres_ofs[2];
  struct GPUTexture *ssr_normal_input; /* Textures from pool */
  struct GPUTexture *ssr_specrough_input;
  struct GPUTexture *ssr_hit_output;
  struct GPUTexture *ssr_hit_depth;
  /* Temporal Anti Aliasing */
  int taa_reproject_sample;
  int taa_current_sample;
  int taa_render_sample;
  int taa_total_sample;
  float taa_alpha;
  bool bypass_drawing;
  bool prev_drw_support;
  bool prev_is_navigating;
  float prev_drw_persmat[4][4]; /* Used for checking view validity and reprojection. */
  struct DRWView *taa_view;
  /* Ambient Occlusion */
  int ao_depth_layer;
  struct GPUTexture *ao_src_depth;             /* pointer copy */
  struct GPUTexture *gtao_horizons;            /* Textures from pool */
  struct GPUTexture *gtao_horizons_renderpass; /* Texture when rendering render pass */
  struct GPUTexture *gtao_horizons_debug;
  /* Motion Blur */
  float current_ndc_to_world[4][4];
  float current_world_to_ndc[4][4];
  float current_world_to_view[4][4];
  float past_world_to_ndc[4][4];
  float past_world_to_view[4][4];
  CameraParams past_cam_params;
  CameraParams current_cam_params;
  char motion_blur_step;         /* Which step we are evaluating. */
  int motion_blur_max;           /* Maximum distance in pixels a motion-blurred pixel can cover. */
  float motion_blur_near_far[2]; /* Camera near/far clip distances (positive). */
  bool cam_params_init;
  /* TODO(fclem): Only used in render mode for now.
   * This is because we are missing a per scene persistent place to hold this. */
  struct EEVEE_MotionBlurData motion_blur;
  /* Velocity Pass */
  struct GPUTexture *velocity_tx; /* Texture from pool */
  struct GPUTexture *velocity_tiles_x_tx;
  struct GPUTexture *velocity_tiles_tx;
  /* Depth Of Field */
  float dof_jitter_radius;
  float dof_jitter_blades;
  float dof_jitter_focus;
  int dof_jitter_ring_count;
  float dof_coc_params[2], dof_coc_near_dist, dof_coc_far_dist;
  float dof_bokeh_blades, dof_bokeh_rotation, dof_bokeh_aniso[2], dof_bokeh_max_size;
  float dof_bokeh_aniso_inv[2];
  float dof_scatter_color_threshold;
  float dof_scatter_coc_threshold;
  float dof_scatter_neighbor_max_color;
  float dof_fx_max_coc;
  float dof_denoise_factor;
  int dof_dilate_slight_focus;
  int dof_dilate_ring_count;
  int dof_dilate_ring_width_multiplier;
  int dof_reduce_steps;
  bool dof_hq_slight_focus;
  eGPUTextureFormat dof_color_format;
  struct GPUTexture *dof_bg_color_tx; /* All textures from pool... */
  struct GPUTexture *dof_bg_occlusion_tx;
  struct GPUTexture *dof_bg_weight_tx;
  struct GPUTexture *dof_bokeh_gather_lut_tx;
  struct GPUTexture *dof_bokeh_scatter_lut_tx;
  struct GPUTexture *dof_bokeh_resolve_lut_tx;
  struct GPUTexture *dof_coc_dilated_tiles_bg_tx;
  struct GPUTexture *dof_coc_dilated_tiles_fg_tx;
  struct GPUTexture *dof_coc_tiles_bg_tx;
  struct GPUTexture *dof_coc_tiles_fg_tx;
  struct GPUTexture *dof_downsample_tx;
  struct GPUTexture *dof_fg_color_tx;
  struct GPUTexture *dof_fg_occlusion_tx;
  struct GPUTexture *dof_fg_weight_tx;
  struct GPUTexture *dof_fg_holefill_color_tx;
  struct GPUTexture *dof_fg_holefill_weight_tx;
  struct GPUTexture *dof_half_res_coc_tx;
  struct GPUTexture *dof_half_res_color_tx;
  struct GPUTexture *dof_scatter_src_tx;
  struct GPUTexture *dof_reduce_input_coc_tx; /* Just references to actual textures. */
  struct GPUTexture *dof_reduce_input_color_tx;
  /* Other */
  float prev_persmat[4][4];
  /* Size used by all fullscreen buffers using mipmaps. */
  int hiz_size[2];
  /* Lookdev */
  int sphere_size;
  eDRWLevelOfDetail sphere_lod;
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
  struct GPUFrameBuffer *final_fb;      /* Frame-buffer with final_tx as attachment. */
} EEVEE_EffectsInfo;

/* ***************** COMMON DATA **************** */

/* Common uniform buffer containing all "constant" data over the whole drawing pipeline. */
/* !! CAUTION !!
 * - [i]vec3 need to be padded to [i]vec4 (even in ubo declaration).
 * - Make sure that [i]vec4 start at a multiple of 16 bytes.
 * - Arrays of vec2/vec3 are padded as arrays of vec4.
 * - sizeof(bool) == sizeof(int) in GLSL so use int in C */
typedef struct EEVEE_CommonUniformBuffer {
  float prev_persmat[4][4];               /* mat4 */
  float hiz_uv_scale[2], ssr_uv_scale[2]; /* vec4 */
  /* Ambient Occlusion */
  /* -- 16 byte aligned -- */
  float ao_dist, pad1, ao_factor, pad2;                    /* vec4 */
  float ao_offset, ao_bounce_fac, ao_quality, ao_settings; /* vec4 */
  /* Volumetric */
  /* -- 16 byte aligned -- */
  int vol_tex_size[3], pad3;       /* ivec3 */
  float vol_depth_param[3], pad4;  /* vec3 */
  float vol_inv_tex_size[3], pad5; /* vec3 */
  float vol_jitter[3], pad6;       /* vec3 */
  float vol_coord_scale[4];        /* vec4 */
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
  int ssrefract_toggle;                               /* bool */
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
  /* Misc */
  int ray_type;            /* int */
  float ray_depth;         /* float */
  float alpha_hash_offset; /* float */
  float alpha_hash_scale;  /* float */
  float pad7;              /* float */
  float pad8;              /* float */
  float pad9;              /* float */
  float pad10;             /* float */
} EEVEE_CommonUniformBuffer;

BLI_STATIC_ASSERT_ALIGN(EEVEE_CommonUniformBuffer, 16)

/* ray_type (keep in sync with rayType) */
#define EEVEE_RAY_CAMERA 0
#define EEVEE_RAY_SHADOW 1
#define EEVEE_RAY_DIFFUSE 2
#define EEVEE_RAY_GLOSSY 3

/* ************** SCENE LAYER DATA ************** */
typedef struct EEVEE_ViewLayerData {
  /* Lights */
  struct EEVEE_LightsInfo *lights;

  struct GPUUniformBuf *light_ubo;
  struct GPUUniformBuf *shadow_ubo;
  struct GPUUniformBuf *shadow_samples_ubo;

  struct GPUFrameBuffer *shadow_fb;

  struct GPUTexture *shadow_cube_pool;
  struct GPUTexture *shadow_cascade_pool;

  struct EEVEE_ShadowCasterBuffer shcasters_buffers[2];

  /* Probes */
  struct EEVEE_LightProbesInfo *probes;

  struct GPUUniformBuf *probe_ubo;
  struct GPUUniformBuf *grid_ubo;
  struct GPUUniformBuf *planar_ubo;

  /* Material Render passes */
  struct {
    struct GPUUniformBuf *combined;
    struct GPUUniformBuf *environment;
    struct GPUUniformBuf *diff_color;
    struct GPUUniformBuf *diff_light;
    struct GPUUniformBuf *spec_color;
    struct GPUUniformBuf *spec_light;
    struct GPUUniformBuf *emit;
    struct GPUUniformBuf *aovs[MAX_AOVS];
  } renderpass_ubo;

  /* Common Uniform Buffer */
  struct EEVEE_CommonUniformBuffer common_data;
  struct GPUUniformBuf *common_ubo;

  struct LightCache *fallback_lightcache;

  struct BLI_memblock *material_cache;
} EEVEE_ViewLayerData;

/* ************ OBJECT DATA ************ */

/* These are the structs stored inside Objects.
 * It works even if the object is in multiple layers
 * because we don't get the same "Object *" for each layer. */
typedef struct EEVEE_LightEngineData {
  DrawData dd;

  bool need_update;
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
  bool geom_update;
  uint shadow_caster_id;
} EEVEE_ObjectEngineData;

typedef struct EEVEE_WorldEngineData {
  DrawData dd;
} EEVEE_WorldEngineData;

typedef struct EEVEE_CryptomatteSample {
  float hash;
  float weight;
} EEVEE_CryptomatteSample;

/* *********************************** */

typedef struct EEVEE_Data {
  void *engine_type;
  EEVEE_FramebufferList *fbl;
  EEVEE_TextureList *txl;
  EEVEE_PassList *psl;
  EEVEE_StorageList *stl;
  char info[GPU_INFO_SIZE];
} EEVEE_Data;

typedef struct EEVEE_PrivateData {
  struct DRWShadingGroup *shadow_shgrp;
  struct DRWShadingGroup *shadow_accum_shgrp;
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
  float camtexcofac[4];
  float size_orig[2];

  /* Cached original camera when rendering for motion blur (see T79637). */
  struct Object *cam_original_ob;

  /* Mist Settings */
  float mist_start, mist_inv_dist, mist_falloff;

  /* Color Management */
  bool use_color_render_settings;

  /* Compiling shaders count. This is to track if a shader has finished compiling. */
  int queued_shaders_count;
  int queued_shaders_count_prev;

  /* LookDev Settings */
  int studiolight_index;
  float studiolight_rot_z;
  float studiolight_intensity;
  int studiolight_cubemap_res;
  float studiolight_glossy_clamp;
  float studiolight_filter_quality;

  /* Renderpasses */
  /* Bitmask containing the active render_passes */
  eViewLayerEEVEEPassType render_passes;
  int aov_hash;
  int num_aovs_used;
  struct CryptomatteSession *cryptomatte_session;
  bool cryptomatte_accurate_mode;
  EEVEE_CryptomatteSample *cryptomatte_accum_buffer;
  float *cryptomatte_download_buffer;

  /* Uniform references that are referenced inside the `renderpass_pass`. They are updated
   * to reuse the drawing pass and the shading group. */
  int renderpass_type;
  int renderpass_postprocess;
  int renderpass_current_sample;
  GPUTexture *renderpass_input;
  GPUTexture *renderpass_col_input;
  GPUTexture *renderpass_light_input;
  GPUTexture *renderpass_transmittance_input;
  /* Renderpass ubo reference used by material pass. */
  struct GPUUniformBuf *renderpass_ubo;
  /** For rendering shadows. */
  struct DRWView *cube_views[6];
  /** For rendering probes. */
  struct DRWView *bake_views[6];
  /** Same as bake_views but does not generate culling infos. */
  struct DRWView *world_views[6];
  /** For rendering planar reflections. */
  struct DRWView *planar_views[MAX_PLANAR];

  int render_timesteps;
  int render_sample_count_per_timestep;
} EEVEE_PrivateData; /* Transient data */

/* eevee_data.c */
void EEVEE_motion_blur_data_init(EEVEE_MotionBlurData *mb);
void EEVEE_motion_blur_data_free(EEVEE_MotionBlurData *mb);
void EEVEE_view_layer_data_free(void *storage);
EEVEE_ViewLayerData *EEVEE_view_layer_data_get(void);
EEVEE_ViewLayerData *EEVEE_view_layer_data_ensure_ex(struct ViewLayer *view_layer);
EEVEE_ViewLayerData *EEVEE_view_layer_data_ensure(void);
EEVEE_ObjectEngineData *EEVEE_object_data_get(Object *ob);
EEVEE_ObjectEngineData *EEVEE_object_data_ensure(Object *ob);
EEVEE_ObjectMotionData *EEVEE_motion_blur_object_data_get(EEVEE_MotionBlurData *mb,
                                                          Object *ob,
                                                          bool hair);
EEVEE_GeometryMotionData *EEVEE_motion_blur_geometry_data_get(EEVEE_MotionBlurData *mb,
                                                              Object *ob);
EEVEE_HairMotionData *EEVEE_motion_blur_hair_data_get(EEVEE_MotionBlurData *mb, Object *ob);
EEVEE_LightProbeEngineData *EEVEE_lightprobe_data_get(Object *ob);
EEVEE_LightProbeEngineData *EEVEE_lightprobe_data_ensure(Object *ob);
EEVEE_LightEngineData *EEVEE_light_data_get(Object *ob);
EEVEE_LightEngineData *EEVEE_light_data_ensure(Object *ob);
EEVEE_WorldEngineData *EEVEE_world_data_get(World *wo);
EEVEE_WorldEngineData *EEVEE_world_data_ensure(World *wo);

void eevee_id_update(void *vedata, ID *id);

/* eevee_materials.c */
struct GPUTexture *EEVEE_materials_get_util_tex(void); /* XXX */
void EEVEE_materials_init(EEVEE_ViewLayerData *sldata,
                          EEVEE_Data *vedata,
                          EEVEE_StorageList *stl,
                          EEVEE_FramebufferList *fbl);
void EEVEE_materials_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_materials_cache_populate(EEVEE_Data *vedata,
                                    EEVEE_ViewLayerData *sldata,
                                    Object *ob,
                                    bool *cast_shadow);
void EEVEE_particle_hair_cache_populate(EEVEE_Data *vedata,
                                        EEVEE_ViewLayerData *sldata,
                                        Object *ob,
                                        bool *cast_shadow);
void EEVEE_object_hair_cache_populate(EEVEE_Data *vedata,
                                      EEVEE_ViewLayerData *sldata,
                                      Object *ob,
                                      bool *cast_shadow);
void EEVEE_materials_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_materials_free(void);
void EEVEE_update_noise(EEVEE_PassList *psl, EEVEE_FramebufferList *fbl, const double offsets[3]);
void EEVEE_material_renderpasses_init(EEVEE_Data *vedata);
void EEVEE_material_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, uint tot_samples);
void EEVEE_material_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_material_bind_resources(DRWShadingGroup *shgrp,
                                   struct GPUMaterial *gpumat,
                                   EEVEE_ViewLayerData *sldata,
                                   EEVEE_Data *vedata,
                                   const int *ssr_id,
                                   const float *refract_depth,
                                   bool use_ssrefraction,
                                   bool use_alpha_blend);
/* eevee_lights.c */
void eevee_light_matrix_get(const EEVEE_Light *evli, float r_mat[4][4]);
void EEVEE_lights_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_lights_cache_add(EEVEE_ViewLayerData *sldata, struct Object *ob);
void EEVEE_lights_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);

/* eevee_shadows.c */
void eevee_contact_shadow_setup(const Light *la, EEVEE_Shadow *evsh);
void EEVEE_shadows_init(EEVEE_ViewLayerData *sldata);
void EEVEE_shadows_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_shadows_caster_register(EEVEE_ViewLayerData *sldata, struct Object *ob);
void EEVEE_shadows_update(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_shadows_cube_add(EEVEE_LightsInfo *linfo, EEVEE_Light *evli, struct Object *ob);
bool EEVEE_shadows_cube_setup(EEVEE_LightsInfo *linfo, const EEVEE_Light *evli, int sample_ofs);
void EEVEE_shadows_cascade_add(EEVEE_LightsInfo *linfo, EEVEE_Light *evli, struct Object *ob);
void EEVEE_shadows_draw(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, struct DRWView *view);
void EEVEE_shadows_draw_cubemap(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, int cube_index);
void EEVEE_shadows_draw_cascades(EEVEE_ViewLayerData *sldata,
                                 EEVEE_Data *vedata,
                                 DRWView *view,
                                 int cascade_index);
void EEVEE_shadow_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, uint tot_samples);
void EEVEE_shadow_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);

/* eevee_sampling.c */
void EEVEE_sample_ball(int sample_ofs, float radius, float rsample[3]);
void EEVEE_sample_rectangle(int sample_ofs,
                            const float x_axis[3],
                            const float y_axis[3],
                            float size_x,
                            float size_y,
                            float rsample[3]);
void EEVEE_sample_ellipse(int sample_ofs,
                          const float x_axis[3],
                          const float y_axis[3],
                          float size_x,
                          float size_y,
                          float rsample[3]);
void EEVEE_random_rotation_m4(int sample_ofs, float scale, float r_mat[4][4]);

/* eevee_shaders.c */
void EEVEE_shaders_lightprobe_shaders_init(void);
void EEVEE_shaders_material_shaders_init(void);
struct DRWShaderLibrary *EEVEE_shader_lib_get(void);
struct GPUShader *EEVEE_shaders_bloom_blit_get(bool high_quality);
struct GPUShader *EEVEE_shaders_bloom_downsample_get(bool high_quality);
struct GPUShader *EEVEE_shaders_bloom_upsample_get(bool high_quality);
struct GPUShader *EEVEE_shaders_bloom_resolve_get(bool high_quality);
struct GPUShader *EEVEE_shaders_depth_of_field_bokeh_get(void);
struct GPUShader *EEVEE_shaders_depth_of_field_setup_get(void);
struct GPUShader *EEVEE_shaders_depth_of_field_flatten_tiles_get(void);
struct GPUShader *EEVEE_shaders_depth_of_field_dilate_tiles_get(bool pass);
struct GPUShader *EEVEE_shaders_depth_of_field_downsample_get(void);
struct GPUShader *EEVEE_shaders_depth_of_field_reduce_get(bool is_copy_pass);
struct GPUShader *EEVEE_shaders_depth_of_field_gather_get(EEVEE_DofGatherPass pass, bool bokeh_tx);
struct GPUShader *EEVEE_shaders_depth_of_field_filter_get(void);
struct GPUShader *EEVEE_shaders_depth_of_field_scatter_get(bool is_foreground, bool bokeh_tx);
struct GPUShader *EEVEE_shaders_depth_of_field_resolve_get(bool use_bokeh_tx, bool use_hq_gather);
struct GPUShader *EEVEE_shaders_effect_color_copy_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_downsample_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_downsample_cube_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_minz_downlevel_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_maxz_downlevel_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_minz_downdepth_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_maxz_downdepth_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_minz_downdepth_layer_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_maxz_downdepth_layer_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_maxz_copydepth_layer_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_minz_copydepth_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_maxz_copydepth_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_mist_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_motion_blur_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_motion_blur_object_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_motion_blur_hair_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_motion_blur_velocity_tiles_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_motion_blur_velocity_tiles_expand_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_ambient_occlusion_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_ambient_occlusion_layer_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_ambient_occlusion_debug_sh_get(void);
struct GPUShader *EEVEE_shaders_effect_screen_raytrace_sh_get(EEVEE_SSRShaderOptions options);
struct GPUShader *EEVEE_shaders_renderpasses_post_process_sh_get(void);
struct GPUShader *EEVEE_shaders_cryptomatte_sh_get(bool is_hair);
struct GPUShader *EEVEE_shaders_shadow_sh_get(void);
struct GPUShader *EEVEE_shaders_shadow_accum_sh_get(void);
struct GPUShader *EEVEE_shaders_subsurface_first_pass_sh_get(void);
struct GPUShader *EEVEE_shaders_subsurface_second_pass_sh_get(void);
struct GPUShader *EEVEE_shaders_subsurface_translucency_sh_get(void);
struct GPUShader *EEVEE_shaders_volumes_clear_sh_get(void);
struct GPUShader *EEVEE_shaders_volumes_scatter_sh_get(void);
struct GPUShader *EEVEE_shaders_volumes_scatter_with_lights_sh_get(void);
struct GPUShader *EEVEE_shaders_volumes_integration_sh_get(void);
struct GPUShader *EEVEE_shaders_volumes_resolve_sh_get(bool accum);
struct GPUShader *EEVEE_shaders_volumes_accum_sh_get(void);
struct GPUShader *EEVEE_shaders_ggx_lut_sh_get(void);
struct GPUShader *EEVEE_shaders_ggx_refraction_lut_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_filter_glossy_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_filter_diffuse_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_filter_visibility_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_grid_fill_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_planar_downsample_sh_get(void);
struct GPUShader *EEVEE_shaders_studiolight_probe_sh_get(void);
struct GPUShader *EEVEE_shaders_studiolight_background_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_cube_display_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_grid_display_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_planar_display_sh_get(void);
struct GPUShader *EEVEE_shaders_update_noise_sh_get(void);
struct GPUShader *EEVEE_shaders_velocity_resolve_sh_get(void);
struct GPUShader *EEVEE_shaders_taa_resolve_sh_get(EEVEE_EffectsFlag enabled_effects);
struct bNodeTree *EEVEE_shader_default_surface_nodetree(Material *ma);
struct bNodeTree *EEVEE_shader_default_world_nodetree(World *wo);
Material *EEVEE_material_default_diffuse_get(void);
Material *EEVEE_material_default_glossy_get(void);
Material *EEVEE_material_default_error_get(void);
World *EEVEE_world_default_get(void);
struct GPUMaterial *EEVEE_material_default_get(struct Scene *scene, Material *ma, int options);
struct GPUMaterial *EEVEE_material_get(
    EEVEE_Data *vedata, struct Scene *scene, Material *ma, World *wo, int options);
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

void EEVEE_lightprobes_grid_data_from_object(Object *ob, EEVEE_LightGrid *egrid, int *offset);
void EEVEE_lightprobes_cube_data_from_object(Object *ob, EEVEE_LightProbe *eprobe);
void EEVEE_lightprobes_planar_data_from_object(Object *ob,
                                               EEVEE_PlanarReflection *eplanar,
                                               EEVEE_LightProbeVisTest *vis_test);

/* eevee_depth_of_field.c */
int EEVEE_depth_of_field_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, Object *camera);
void EEVEE_depth_of_field_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_depth_of_field_draw(EEVEE_Data *vedata);
bool EEVEE_depth_of_field_jitter_get(EEVEE_EffectsInfo *effects,
                                     float r_jitter[2],
                                     float *r_focus_distance);
int EEVEE_depth_of_field_sample_count_get(EEVEE_EffectsInfo *effects,
                                          int sample_count,
                                          int *r_ring_count);

/* eevee_bloom.c */
int EEVEE_bloom_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_bloom_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_bloom_draw(EEVEE_Data *vedata);
void EEVEE_bloom_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, uint tot_samples);
void EEVEE_bloom_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);

/* eevee_cryptomatte.c */
void EEVEE_cryptomatte_renderpasses_init(EEVEE_Data *vedata);
void EEVEE_cryptomatte_output_init(EEVEE_ViewLayerData *sldata,
                                   EEVEE_Data *vedata,
                                   int tot_samples);
void EEVEE_cryptomatte_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_cryptomatte_cache_populate(EEVEE_Data *vedata, EEVEE_ViewLayerData *sldata, Object *ob);
void EEVEE_cryptomatte_particle_hair_cache_populate(EEVEE_Data *vedata,
                                                    EEVEE_ViewLayerData *sldata,
                                                    Object *ob);
void EEVEE_cryptomatte_object_hair_cache_populate(EEVEE_Data *vedata,
                                                  EEVEE_ViewLayerData *sldata,
                                                  Object *ob);
void EEVEE_cryptomatte_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_cryptomatte_update_passes(struct RenderEngine *engine,
                                     struct Scene *scene,
                                     struct ViewLayer *view_layer);
void EEVEE_cryptomatte_render_result(struct RenderLayer *rl,
                                     const char *viewname,
                                     const rcti *rect,
                                     EEVEE_Data *vedata,
                                     EEVEE_ViewLayerData *sldata);
void EEVEE_cryptomatte_store_metadata(EEVEE_Data *vedata, struct RenderResult *render_result);
void EEVEE_cryptomatte_free(EEVEE_Data *vedata);

/* eevee_occlusion.c */
int EEVEE_occlusion_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_occlusion_output_init(EEVEE_ViewLayerData *sldata,
                                 EEVEE_Data *vedata,
                                 uint tot_samples);
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
void EEVEE_reflection_output_init(EEVEE_ViewLayerData *sldata,
                                  EEVEE_Data *vedata,
                                  uint tot_samples);
void EEVEE_reflection_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);

/* eevee_subsurface.c */
void EEVEE_subsurface_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_draw_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_output_init(EEVEE_ViewLayerData *sldata,
                                  EEVEE_Data *vedata,
                                  uint tot_samples);
void EEVEE_subsurface_add_pass(EEVEE_ViewLayerData *sldata,
                               EEVEE_Data *vedata,
                               Material *ma,
                               DRWShadingGroup *shgrp,
                               struct GPUMaterial *gpumat);
void EEVEE_subsurface_data_render(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_compute(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);

/* eevee_motion_blur.c */
int EEVEE_motion_blur_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_motion_blur_step_set(EEVEE_Data *vedata, int step);
void EEVEE_motion_blur_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_motion_blur_cache_populate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, Object *ob);
void EEVEE_motion_blur_hair_cache_populate(EEVEE_ViewLayerData *sldata,
                                           EEVEE_Data *vedata,
                                           Object *ob,
                                           struct ParticleSystem *psys,
                                           struct ModifierData *md);
void EEVEE_motion_blur_swap_data(EEVEE_Data *vedata);
void EEVEE_motion_blur_cache_finish(EEVEE_Data *vedata);
void EEVEE_motion_blur_draw(EEVEE_Data *vedata);

/* eevee_mist.c */
void EEVEE_mist_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_mist_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);

/* eevee_renderpasses.c */
void EEVEE_renderpasses_init(EEVEE_Data *vedata);
void EEVEE_renderpasses_output_init(EEVEE_ViewLayerData *sldata,
                                    EEVEE_Data *vedata,
                                    uint tot_samples);
void EEVEE_renderpasses_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_renderpasses_output_accumulate(EEVEE_ViewLayerData *sldata,
                                          EEVEE_Data *vedata,
                                          bool post_effect);
void EEVEE_renderpasses_postprocess(EEVEE_ViewLayerData *sldata,
                                    EEVEE_Data *vedata,
                                    eViewLayerEEVEEPassType renderpass_type,
                                    int aov_index);
void EEVEE_renderpasses_draw(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_renderpasses_draw_debug(EEVEE_Data *vedata);
bool EEVEE_renderpasses_only_first_sample_pass_active(EEVEE_Data *vedata);
int EEVEE_renderpasses_aov_hash(const ViewLayerAOV *aov);

/* eevee_temporal_sampling.c */
void EEVEE_temporal_sampling_reset(EEVEE_Data *vedata);
void EEVEE_temporal_sampling_create_view(EEVEE_Data *vedata);
int EEVEE_temporal_sampling_sample_count_get(const Scene *scene, const EEVEE_StorageList *stl);
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
void EEVEE_volumes_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, uint tot_samples);
void EEVEE_volumes_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_volumes_free_smoke_textures(void);
void EEVEE_volumes_free(void);

/* eevee_effects.c */
void EEVEE_effects_init(EEVEE_ViewLayerData *sldata,
                        EEVEE_Data *vedata,
                        Object *camera,
                        const bool minimal);
void EEVEE_effects_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_effects_draw_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_effects_downsample_radiance_buffer(EEVEE_Data *vedata, struct GPUTexture *texture_src);
void EEVEE_create_minmax_buffer(EEVEE_Data *vedata, struct GPUTexture *depth_src, int layer);
void EEVEE_downsample_cube_buffer(EEVEE_Data *vedata, struct GPUTexture *texture_src, int level);
void EEVEE_draw_effects(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);

/* eevee_render.c */
bool EEVEE_render_init(EEVEE_Data *vedata,
                       struct RenderEngine *engine,
                       struct Depsgraph *depsgraph);
void EEVEE_render_view_sync(EEVEE_Data *vedata,
                            struct RenderEngine *engine,
                            struct Depsgraph *depsgraph);
void EEVEE_render_modules_init(EEVEE_Data *vedata,
                               struct RenderEngine *engine,
                               struct Depsgraph *depsgraph);
void EEVEE_render_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_render_cache(void *vedata,
                        struct Object *ob,
                        struct RenderEngine *engine,
                        struct Depsgraph *depsgraph);
void EEVEE_render_draw(EEVEE_Data *vedata,
                       struct RenderEngine *engine,
                       struct RenderLayer *rl,
                       const struct rcti *rect);
void EEVEE_render_read_result(EEVEE_Data *vedata,
                              struct RenderEngine *engine,
                              struct RenderLayer *rl,
                              const rcti *rect);
void EEVEE_render_update_passes(struct RenderEngine *engine,
                                struct Scene *scene,
                                struct ViewLayer *view_layer);

/** eevee_lookdev.c */
void EEVEE_lookdev_init(EEVEE_Data *vedata);
void EEVEE_lookdev_cache_init(EEVEE_Data *vedata,
                              EEVEE_ViewLayerData *sldata,
                              DRWPass *pass,
                              EEVEE_LightProbesInfo *pinfo,
                              DRWShadingGroup **r_shgrp);
void EEVEE_lookdev_draw(EEVEE_Data *vedata);

/** eevee_engine.c */
void EEVEE_cache_populate(void *vedata, Object *ob);

/** eevee_lut_gen.c */
float *EEVEE_lut_update_ggx_brdf(int lut_size);
float *EEVEE_lut_update_ggx_btdf(int lut_size, int lut_depth);

/* Shadow Matrix */
static const float texcomat[4][4] = {
    /* From NDC to TexCo */
    {0.5f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.5f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.5f, 0.0f},
    {0.5f, 0.5f, 0.5f, 1.0f},
};

/* Cube-map Matrices */
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

#ifdef __cplusplus
}
#endif
