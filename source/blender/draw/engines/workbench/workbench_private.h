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

#ifndef __WORKBENCH_PRIVATE_H__
#define __WORKBENCH_PRIVATE_H__

#include "BKE_studiolight.h"

#include "DNA_image_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "DRW_render.h"

#include "workbench_engine.h"

extern struct DrawEngineType draw_engine_workbench;

#define WORKBENCH_ENGINE "BLENDER_WORKBENCH"

#define MAX_MATERIAL (1 << 12)

#define DEBUG_SHADOW_VOLUME 0

#define STUDIOLIGHT_ENABLED(wpd) (wpd->shading.light == V3D_LIGHTING_STUDIO)
#define MATCAP_ENABLED(wpd) (wpd->shading.light == V3D_LIGHTING_MATCAP)
#define USE_WORLD_ORIENTATION(wpd) ((wpd->shading.flag & V3D_SHADING_WORLD_ORIENTATION) != 0)
#define STUDIOLIGHT_TYPE_WORLD_ENABLED(wpd) \
  (STUDIOLIGHT_ENABLED(wpd) && (wpd->studio_light->flag & STUDIOLIGHT_TYPE_WORLD))
#define STUDIOLIGHT_TYPE_STUDIO_ENABLED(wpd) \
  (STUDIOLIGHT_ENABLED(wpd) && (wpd->studio_light->flag & STUDIOLIGHT_TYPE_STUDIO))
#define STUDIOLIGHT_TYPE_MATCAP_ENABLED(wpd) \
  (MATCAP_ENABLED(wpd) && (wpd->studio_light->flag & STUDIOLIGHT_TYPE_MATCAP))
#define SSAO_ENABLED(wpd) \
  ((wpd->shading.flag & V3D_SHADING_CAVITY) && \
   ((wpd->shading.cavity_type == V3D_SHADING_CAVITY_SSAO) || \
    (wpd->shading.cavity_type == V3D_SHADING_CAVITY_BOTH)))
#define CURVATURE_ENABLED(wpd) \
  ((wpd->shading.flag & V3D_SHADING_CAVITY) && \
   ((wpd->shading.cavity_type == V3D_SHADING_CAVITY_CURVATURE) || \
    (wpd->shading.cavity_type == V3D_SHADING_CAVITY_BOTH)))
#define CAVITY_ENABLED(wpd) (CURVATURE_ENABLED(wpd) || SSAO_ENABLED(wpd))
#define SHADOW_ENABLED(wpd) (wpd->shading.flag & V3D_SHADING_SHADOW)
#define CULL_BACKFACE_ENABLED(wpd) ((wpd->shading.flag & V3D_SHADING_BACKFACE_CULLING) != 0)

#define OBJECT_OUTLINE_ENABLED(wpd) (wpd->shading.flag & V3D_SHADING_OBJECT_OUTLINE)
#define OBJECT_ID_PASS_ENABLED(wpd) (OBJECT_OUTLINE_ENABLED(wpd) || CURVATURE_ENABLED(wpd))
#define NORMAL_ENCODING_ENABLED() (true)

struct RenderEngine;
struct RenderLayer;
struct rcti;

typedef struct WORKBENCH_FramebufferList {
  struct GPUFrameBuffer *opaque_fb;
  struct GPUFrameBuffer *opaque_infront_fb;

  struct GPUFrameBuffer *transp_accum_fb;
  struct GPUFrameBuffer *transp_accum_infront_fb;

  struct GPUFrameBuffer *id_clear_fb;

  struct GPUFrameBuffer *dof_downsample_fb;
  struct GPUFrameBuffer *dof_coc_tile_h_fb;
  struct GPUFrameBuffer *dof_coc_tile_v_fb;
  struct GPUFrameBuffer *dof_coc_dilate_fb;
  struct GPUFrameBuffer *dof_blur1_fb;
  struct GPUFrameBuffer *dof_blur2_fb;

  struct GPUFrameBuffer *antialiasing_fb;
  struct GPUFrameBuffer *antialiasing_in_front_fb;
  struct GPUFrameBuffer *smaa_edge_fb;
  struct GPUFrameBuffer *smaa_weight_fb;
} WORKBENCH_FramebufferList;

typedef struct WORKBENCH_TextureList {
  struct GPUTexture *dof_source_tx;
  struct GPUTexture *coc_halfres_tx;
  struct GPUTexture *history_buffer_tx;
  struct GPUTexture *depth_buffer_tx;
  struct GPUTexture *depth_buffer_in_front_tx;
  struct GPUTexture *smaa_search_tx;
  struct GPUTexture *smaa_area_tx;
  struct GPUTexture *dummy_image_tx;
  struct GPUTexture *dummy_volume_tx;
  struct GPUTexture *dummy_shadow_tx;
  struct GPUTexture *dummy_coba_tx;
} WORKBENCH_TextureList;

typedef struct WORKBENCH_StorageList {
  struct WORKBENCH_PrivateData *wpd;
  float *dof_ubo_data;
} WORKBENCH_StorageList;

typedef struct WORKBENCH_PassList {
  struct DRWPass *opaque_ps;
  struct DRWPass *opaque_infront_ps;

  struct DRWPass *transp_resolve_ps;
  struct DRWPass *transp_accum_ps;
  struct DRWPass *transp_accum_infront_ps;

  struct DRWPass *shadow_ps[2];

  struct DRWPass *merge_infront_ps;

  struct DRWPass *cavity_ps;
  struct DRWPass *outline_ps;

  struct DRWPass *composite_ps;

  struct DRWPass *dof_down_ps;
  struct DRWPass *dof_down2_ps;
  struct DRWPass *dof_flatten_v_ps;
  struct DRWPass *dof_flatten_h_ps;
  struct DRWPass *dof_dilate_h_ps;
  struct DRWPass *dof_dilate_v_ps;
  struct DRWPass *dof_blur1_ps;
  struct DRWPass *dof_blur2_ps;
  struct DRWPass *dof_resolve_ps;

  struct DRWPass *volume_ps;

  struct DRWPass *aa_accum_ps;
  struct DRWPass *aa_edge_ps;
  struct DRWPass *aa_weight_ps;
  struct DRWPass *aa_resolve_ps;
} WORKBENCH_PassList;

typedef struct WORKBENCH_Data {
  void *engine_type;
  WORKBENCH_FramebufferList *fbl;
  WORKBENCH_TextureList *txl;
  WORKBENCH_PassList *psl;
  WORKBENCH_StorageList *stl;
} WORKBENCH_Data;

typedef struct WORKBENCH_UBO_Light {
  float light_direction[4];
  float specular_color[3], pad;
  float diffuse_color[3], wrapped;
} WORKBENCH_UBO_Light;

typedef struct WORKBENCH_UBO_Material {
  float base_color[3];
  /* Packed data into a int. Decoded in the shader. */
  uint32_t packed_data;
} WORKBENCH_UBO_Material;

typedef struct WORKBENCH_UBO_World {
  float viewvecs[3][4];
  float viewport_size[2], viewport_size_inv[2];
  float object_outline_color[4];
  float shadow_direction_vs[4];
  float shadow_focus, shadow_shift, shadow_mul, shadow_add;
  WORKBENCH_UBO_Light lights[4];
  float ambient_color[4];

  int cavity_sample_start;
  int cavity_sample_end;
  float cavity_sample_count_inv;
  float cavity_jitter_scale;

  float cavity_valley_factor;
  float cavity_ridge_factor;
  float cavity_attenuation;
  float cavity_distance;

  float curvature_ridge;
  float curvature_valley;
  float ui_scale;
  float _pad0;

  int matcap_orientation;
  int use_specular; /* Bools are 32bit ints in GLSL. */
  int _pad1;
  int _pad2;
} WORKBENCH_UBO_World;

BLI_STATIC_ASSERT_ALIGN(WORKBENCH_UBO_World, 16)
BLI_STATIC_ASSERT_ALIGN(WORKBENCH_UBO_Light, 16)
BLI_STATIC_ASSERT_ALIGN(WORKBENCH_UBO_Material, 16)

typedef struct WORKBENCH_Prepass {
  /** Hash storing shading group for each Material or GPUTexture to reduce state changes. */
  struct GHash *material_hash;
  /** First common (non-vcol and non-image colored) shading group to created subgroups. */
  struct DRWShadingGroup *common_shgrp;
  /** First Vertex Color shading group to created subgroups. */
  struct DRWShadingGroup *vcol_shgrp;
  /** First Image shading group to created subgroups. */
  struct DRWShadingGroup *image_shgrp;
  /** First UDIM (tiled image) shading group to created subgroups. */
  struct DRWShadingGroup *image_tiled_shgrp;
} WORKBENCH_Prepass;

typedef struct WORKBENCH_PrivateData {
  /** ViewLayerData for faster access. */
  struct WORKBENCH_ViewLayerData *vldata;
  /** Copy of draw_ctx->sh_cfg for faster access. */
  eGPUShaderConfig sh_cfg;
  /** Global clip and cull states. */
  DRWState clip_state, cull_state;
  /** Copy of scene->display.shading or v3d->shading for viewport. */
  View3DShading shading;
  /** Chosen studiolight or matcap. */
  StudioLight *studio_light;
  /** Copy of ctx_draw->scene for faster access. */
  struct Scene *scene;
  /** Shorthand version of U global for user preferences. */
  const UserDef *preferences;
  /** Copy of context mode for faster access. */
  eContextObjectMode ctx_mode;
  /** Shorthand for wpd->vldata->world_ubo. */
  struct GPUUniformBuffer *world_ubo;
  /** Background color to clear the color buffer with. */
  float background_color[4];

  /* Shadow */
  /** Previous shadow direction to test if shadow has changed. */
  float shadow_cached_direction[3];
  /** Current shadow direction in world space. */
  float shadow_direction_ws[3];
  /** Shadow precomputed matrices. */
  float shadow_mat[4][4];
  float shadow_inv[4][4];
  /** Far plane of the view frustum. Used for shadow volume extrusion. */
  float shadow_far_plane[4];
  /** Min and max of shadow_near_corners. Speed up culling test. */
  float shadow_near_min[3];
  float shadow_near_max[3];
  /** This is a parallelogram, so only 2 normal and distance to the edges. */
  float shadow_near_sides[2][4];
  /* Shadow shading groups. First array elem is for non-manifold geom and second for manifold. */
  struct DRWShadingGroup *shadow_pass_grp[2];
  struct DRWShadingGroup *shadow_fail_grp[2];
  struct DRWShadingGroup *shadow_fail_caps_grp[2];
  /** If the shadow has changed direction and ob bboxes needs to be updated. */
  bool shadow_changed;

  /* Temporal Antialiasing */
  /** Total number of samples to after which TAA stops accumulating samples. */
  int taa_sample_len;
  /** Total number of samples of the previous TAA. When changed TAA will be reset. */
  int taa_sample_len_previous;
  /** Current TAA sample index in [0..taa_sample_len[ range. */
  int taa_sample;
  /** Inverse of taa_sample to divide the accumulation buffer. */
  float taa_sample_inv;
  /** If the view has been updated and TAA needs to be reset. */
  bool view_updated;
  /** True if the history buffer contains relevant data and false if it could contain garbage. */
  bool valid_history;
  /** View */
  struct DRWView *view;
  /** Last projection matrix to see if view is still valid. */
  float last_mat[4][4];

  /* Smart Morphological Anti-Aliasing */
  /** Temp buffers to store edges and weights. */
  struct GPUTexture *smaa_edge_tx;
  struct GPUTexture *smaa_weight_tx;
  /** Weight of the smaa pass. */
  float smaa_mix_factor;

  /** Opaque pipeline buffers. */
  struct GPUTexture *material_buffer_tx;
  struct GPUTexture *composite_buffer_tx;
  struct GPUTexture *normal_buffer_tx;
  /** Transparent pipeline buffers. */
  struct GPUTexture *accum_buffer_tx;
  struct GPUTexture *reveal_buffer_tx;
  /** Object IDs buffer for curvature & outline. */
  struct GPUTexture *object_id_tx;

  /** Prepass infos for each draw types [transparent][infront][hair]. */
  WORKBENCH_Prepass prepass[2][2][2];

  /* Materials */
  /** Copy of vldata->material_ubo for faster access. */
  struct BLI_memblock *material_ubo;
  /** Copy of vldata->material_ubo_data for faster access. */
  struct BLI_memblock *material_ubo_data;
  /** Current material chunk being filled by workbench_material_setup_ex(). */
  WORKBENCH_UBO_Material *material_ubo_data_curr;
  struct GPUUniformBuffer *material_ubo_curr;
  /** Copy of txl->dummy_image_tx for faster access. */
  struct GPUTexture *dummy_image_tx;
  /** Total number of used material chunk. */
  int material_chunk_count;
  /** Index of current material chunk. */
  int material_chunk_curr;
  /** Index of current material inside the material chunk. Only for material coloring mode. */
  int material_index;

  /* Volumes */
  /** List of smoke domain textures to free after drawing. */
  ListBase smoke_domains;

  /* Depth of Field */
  /** Depth of field temp buffers. */
  struct GPUTexture *dof_blur_tx;
  struct GPUTexture *coc_temp_tx;
  struct GPUTexture *coc_tiles_tx[2];
  /** Depth of field parameters. */
  float dof_aperturesize;
  float dof_distance;
  float dof_invsensorsize;
  float dof_near_far[2];
  float dof_blades;
  float dof_rotation;
  float dof_ratio;

  /** True if any volume needs to be rendered. */
  bool volumes_do;
  /** Convenience boolean. */
  bool dof_enabled;
  bool is_playback;
  bool is_navigating;
  bool reset_next_sample;
} WORKBENCH_PrivateData; /* Transient data */

typedef struct WORKBENCH_ObjectData {
  DrawData dd;

  /* Shadow direction in local object space. */
  float shadow_dir[3], shadow_depth;
  /* Min, max in shadow space */
  float shadow_min[3], shadow_max[3];
  BoundBox shadow_bbox;
  bool shadow_bbox_dirty;
} WORKBENCH_ObjectData;

typedef struct WORKBENCH_ViewLayerData {
  /** Depth of field sample location array.*/
  struct GPUUniformBuffer *dof_sample_ubo;
  /** All constant data used for a render loop.*/
  struct GPUUniformBuffer *world_ubo;
  /** Cavity sample location array.*/
  struct GPUUniformBuffer *cavity_sample_ubo;
  /** Blue noise texture used to randomize the sampling of some effects.*/
  struct GPUTexture *cavity_jitter_tx;
  /** Materials ubos allocated in a memblock for easy bookeeping. */
  struct BLI_memblock *material_ubo;
  struct BLI_memblock *material_ubo_data;
  /** Number of samples for which cavity_sample_ubo is valid. */
  int cavity_sample_count;
} WORKBENCH_ViewLayerData;

/* inline helper functions */
BLI_INLINE bool workbench_is_specular_highlight_enabled(WORKBENCH_PrivateData *wpd)
{
  if ((wpd->shading.flag & V3D_SHADING_SPECULAR_HIGHLIGHT)) {
    if (STUDIOLIGHT_ENABLED(wpd) || MATCAP_ENABLED(wpd)) {
      return (wpd->studio_light->flag & STUDIOLIGHT_SPECULAR_HIGHLIGHT_PASS) != 0;
    }
  }
  return false;
}

/* workbench_opaque.c */
void workbench_opaque_engine_init(WORKBENCH_Data *data);
void workbench_opaque_cache_init(WORKBENCH_Data *data);

/* workbench_transparent.c */
void workbench_transparent_engine_init(WORKBENCH_Data *data);
void workbench_transparent_cache_init(WORKBENCH_Data *data);
void workbench_transparent_draw_depth_pass(WORKBENCH_Data *data);

/* workbench_shadow.c */
void workbench_shadow_data_update(WORKBENCH_PrivateData *wpd, WORKBENCH_UBO_World *wd);
void workbench_shadow_cache_init(WORKBENCH_Data *data);
void workbench_shadow_cache_populate(WORKBENCH_Data *data, Object *ob, const bool has_transp_mat);

/* workbench_shader.c */
GPUShader *workbench_shader_opaque_get(WORKBENCH_PrivateData *wpd, bool hair);
GPUShader *workbench_shader_opaque_image_get(WORKBENCH_PrivateData *wpd, bool hair, bool tiled);
GPUShader *workbench_shader_composite_get(WORKBENCH_PrivateData *wpd);
GPUShader *workbench_shader_merge_infront_get(WORKBENCH_PrivateData *wpd);

GPUShader *workbench_shader_transparent_get(WORKBENCH_PrivateData *wpd, bool hair);
GPUShader *workbench_shader_transparent_image_get(WORKBENCH_PrivateData *wpd,
                                                  bool hair,
                                                  bool tiled);
GPUShader *workbench_shader_transparent_resolve_get(WORKBENCH_PrivateData *wpd);

GPUShader *workbench_shader_shadow_pass_get(bool manifold);
GPUShader *workbench_shader_shadow_fail_get(bool manifold, bool cap);

GPUShader *workbench_shader_cavity_get(bool cavity, bool curvature);
GPUShader *workbench_shader_outline_get(void);

GPUShader *workbench_shader_antialiasing_accumulation_get(void);
GPUShader *workbench_shader_antialiasing_get(int stage);

GPUShader *workbench_shader_volume_get(bool slice, bool coba, bool cubic, bool smoke);

void workbench_shader_depth_of_field_get(GPUShader **prepare_sh,
                                         GPUShader **downsample_sh,
                                         GPUShader **blur1_sh,
                                         GPUShader **blur2_sh,
                                         GPUShader **resolve_sh);

void workbench_shader_library_ensure(void);
void workbench_shader_free(void);

/* workbench_effect_antialiasing.c */
int workbench_antialiasing_sample_count_get(WORKBENCH_PrivateData *wpd);
void workbench_antialiasing_engine_init(WORKBENCH_Data *vedata);
void workbench_antialiasing_cache_init(WORKBENCH_Data *vedata);
void workbench_antialiasing_view_updated(WORKBENCH_Data *vedata);
bool workbench_antialiasing_setup(WORKBENCH_Data *vedata);
void workbench_antialiasing_draw_pass(WORKBENCH_Data *vedata);

/* workbench_effect_cavity.c */
void workbench_cavity_data_update(WORKBENCH_PrivateData *wpd, WORKBENCH_UBO_World *wd);
void workbench_cavity_samples_ubo_ensure(WORKBENCH_PrivateData *wpd);
void workbench_cavity_cache_init(WORKBENCH_Data *data);

/* workbench_effect_outline.c */
void workbench_outline_cache_init(WORKBENCH_Data *data);
/* workbench_effect_dof.c */
void workbench_dof_engine_init(WORKBENCH_Data *vedata);
void workbench_dof_cache_init(WORKBENCH_Data *vedata);
void workbench_dof_draw_pass(WORKBENCH_Data *vedata);

/* workbench_materials.c */
void workbench_material_ubo_data(WORKBENCH_PrivateData *wpd,
                                 Object *ob,
                                 Material *mat,
                                 WORKBENCH_UBO_Material *data,
                                 eV3DShadingColorType color_type);

DRWShadingGroup *workbench_material_setup_ex(WORKBENCH_PrivateData *wpd,
                                             Object *ob,
                                             int mat_nr,
                                             eV3DShadingColorType color_type,
                                             bool hair,
                                             bool *r_transp);
DRWShadingGroup *workbench_image_setup_ex(WORKBENCH_PrivateData *wpd,
                                          Object *ob,
                                          int mat_nr,
                                          Image *ima,
                                          ImageUser *iuser,
                                          eGPUSamplerState sampler,
                                          bool hair);

#define workbench_material_setup(wpd, ob, mat_nr, color_type, r_transp) \
  workbench_material_setup_ex(wpd, ob, mat_nr, color_type, false, r_transp)
#define workbench_image_setup(wpd, ob, mat_nr, ima, iuser, interp) \
  workbench_image_setup_ex(wpd, ob, mat_nr, ima, iuser, interp, false)

#define workbench_material_hair_setup(wpd, ob, mat_nr, color_type) \
  workbench_material_setup_ex(wpd, ob, mat_nr, color_type, true, 0)
#define workbench_image_hair_setup(wpd, ob, mat_nr, ima, iuser, interp) \
  workbench_image_setup_ex(wpd, ob, mat_nr, ima, iuser, interp, true)

/* workbench_data.c */
void workbench_private_data_init(WORKBENCH_PrivateData *wpd);
void workbench_update_world_ubo(WORKBENCH_PrivateData *wpd);
void workbench_update_material_ubos(WORKBENCH_PrivateData *wpd);
struct GPUUniformBuffer *workbench_material_ubo_alloc(WORKBENCH_PrivateData *wpd);

/* workbench_volume.c */
void workbench_volume_engine_init(WORKBENCH_Data *vedata);
void workbench_volume_cache_init(WORKBENCH_Data *vedata);
void workbench_volume_cache_populate(WORKBENCH_Data *vedata,
                                     struct Scene *UNUSED(scene),
                                     struct Object *ob,
                                     struct ModifierData *md,
                                     eV3DShadingColorType color_type);
void workbench_volume_draw_pass(WORKBENCH_Data *vedata);
void workbench_volume_draw_finish(WORKBENCH_Data *vedata);

/* workbench_engine.c */
void workbench_engine_init(void *ved);
void workbench_cache_init(void *ved);
void workbench_cache_populate(void *ved, Object *ob);
void workbench_cache_finish(void *ved);
void workbench_draw_sample(void *ved);
void workbench_draw_finish(void *ved);

/* workbench_render.c */
void workbench_render(void *ved,
                      struct RenderEngine *engine,
                      struct RenderLayer *render_layer,
                      const struct rcti *rect);
void workbench_render_update_passes(struct RenderEngine *engine,
                                    struct Scene *scene,
                                    struct ViewLayer *view_layer);

#endif
