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
 * Copyright 2017, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

#ifndef __GPENCIL_ENGINE_H__
#define __GPENCIL_ENGINE_H__

#include "DNA_gpencil_types.h"

#include "BLI_bitmap.h"

#include "GPU_batch.h"

extern DrawEngineType draw_engine_gpencil_type;

struct GPENCIL_Data;
struct GPENCIL_StorageList;
struct GPUBatch;
struct GPUVertBuf;
struct GPUVertFormat;
struct GpencilBatchCache;
struct MaterialGPencilStyle;
struct Object;
struct RenderEngine;
struct RenderLayer;
struct View3D;
struct bGPDstroke;

/* used to convert pixel scale. */
#define GPENCIL_PIXEL_FACTOR 2000.0f

/* used to expand VBOs. Size has a big impact in the speed */
#define GPENCIL_VBO_BLOCK_SIZE 128

#define GP_MAX_MASKBITS 256

/* UBO structure. Watch out for padding. Must match GLSL declaration. */
typedef struct gpMaterial {
  float stroke_color[4];
  float fill_color[4];
  float fill_mix_color[4];
  float fill_uv_transform[3][2], _pad0[2];
  float stroke_texture_mix;
  float stroke_u_scale;
  float fill_texture_mix;
  int flag;
} gpMaterial;

/* gpMaterial->flag */
/* WATCH Keep in sync with GLSL declaration. */
#define GP_STROKE_ALIGNMENT_STROKE 1
#define GP_STROKE_ALIGNMENT_OBJECT 2
#define GP_STROKE_ALIGNMENT_FIXED 3
#define GP_STROKE_ALIGNMENT 0x3
#define GP_STROKE_OVERLAP (1 << 2)
#define GP_STROKE_TEXTURE_USE (1 << 3)
#define GP_STROKE_TEXTURE_STENCIL (1 << 4)
#define GP_STROKE_TEXTURE_PREMUL (1 << 5)
#define GP_STROKE_DOTS (1 << 6)
#define GP_FILL_TEXTURE_USE (1 << 10)
#define GP_FILL_TEXTURE_PREMUL (1 << 11)
#define GP_FILL_TEXTURE_CLIP (1 << 12)
#define GP_FILL_GRADIENT_USE (1 << 13)
#define GP_FILL_GRADIENT_RADIAL (1 << 14)

#define GPENCIL_LIGHT_BUFFER_LEN 128

/* UBO structure. Watch out for padding. Must match GLSL declaration. */
typedef struct gpLight {
  float color[3], type;
  float right[3], spotsize;
  float up[3], spotblend;
  float forward[4];
  float position[4];
} gpLight;

/* gpLight->type */
/* WATCH Keep in sync with GLSL declaration. */
#define GP_LIGHT_TYPE_POINT 0.0
#define GP_LIGHT_TYPE_SPOT 1.0
#define GP_LIGHT_TYPE_SUN 2.0
#define GP_LIGHT_TYPE_AMBIENT 3.0

BLI_STATIC_ASSERT_ALIGN(gpMaterial, 16)
BLI_STATIC_ASSERT_ALIGN(gpLight, 16)

/* *********** Draw Datas *********** */
typedef struct GPENCIL_MaterialPool {
  /* Linklist. */
  struct GPENCIL_MaterialPool *next;
  /* GPU representatin of materials. */
  gpMaterial mat_data[GP_MATERIAL_BUFFER_LEN];
  /* Matching ubo. */
  struct GPUUniformBuffer *ubo;
  /* Texture per material. NULL means none. */
  struct GPUTexture *tex_fill[GP_MATERIAL_BUFFER_LEN];
  struct GPUTexture *tex_stroke[GP_MATERIAL_BUFFER_LEN];
  /* Number of material used in this pool. */
  int used_count;
} GPENCIL_MaterialPool;

typedef struct GPENCIL_LightPool {
  /* GPU representatin of materials. */
  gpLight light_data[GPENCIL_LIGHT_BUFFER_LEN];
  /* Matching ubo. */
  struct GPUUniformBuffer *ubo;
  /* Number of light in the pool. */
  int light_used;
} GPENCIL_LightPool;

typedef struct GPENCIL_ViewLayerData {
  /* GPENCIL_tObject */
  struct BLI_memblock *gp_object_pool;
  /* GPENCIL_tLayer */
  struct BLI_memblock *gp_layer_pool;
  /* GPENCIL_tVfx */
  struct BLI_memblock *gp_vfx_pool;
  /* GPENCIL_MaterialPool */
  struct BLI_memblock *gp_material_pool;
  /* GPENCIL_LightPool */
  struct BLI_memblock *gp_light_pool;
  /* BLI_bitmap */
  struct BLI_memblock *gp_maskbit_pool;
} GPENCIL_ViewLayerData;

/* *********** GPencil  *********** */

typedef struct GPENCIL_tVfx {
  /** Linklist */
  struct GPENCIL_tVfx *next;
  DRWPass *vfx_ps;
  /* Framebuffer reference since it may not be allocated yet. */
  GPUFrameBuffer **target_fb;
} GPENCIL_tVfx;

typedef struct GPENCIL_tLayer {
  /** Linklist */
  struct GPENCIL_tLayer *next;
  /** Geometry pass (draw all strokes). */
  DRWPass *geom_ps;
  /** Blend pass to composite onto the target buffer (blends modes). NULL if not needed. */
  DRWPass *blend_ps;
  /** First shading group created for this layer. Contains all uniforms. */
  DRWShadingGroup *base_shgrp;
  /** Layer id of the mask. */
  BLI_bitmap *mask_bits;
  BLI_bitmap *mask_invert_bits;
  /** Index in the layer list. Used as id for masking. */
  int layer_id;
} GPENCIL_tLayer;

typedef struct GPENCIL_tObject {
  /** Linklist */
  struct GPENCIL_tObject *next;

  struct {
    GPENCIL_tLayer *first, *last;
  } layers;

  struct {
    GPENCIL_tVfx *first, *last;
  } vfx;

  /* Distance to camera. Used for sorting. */
  float camera_z;
  /* Used for stroke thickness scaling. */
  float object_scale;
  /* Normal used for shading. Based on view angle. */
  float plane_normal[3];
  /* Used for drawing depth merge pass. */
  float plane_mat[4][4];

  bool is_drawmode3d;
} GPENCIL_tObject;

/* *********** LISTS *********** */
typedef struct GPENCIL_StorageList {
  struct GPENCIL_PrivateData *pd;
} GPENCIL_StorageList;

typedef struct GPENCIL_PassList {
  /* Composite the main GPencil buffer onto the rendered image. */
  struct DRWPass *composite_ps;
  /* Composite the object depth to the default depth buffer to occlude overlays. */
  struct DRWPass *merge_depth_ps;
  /* Invert mask buffer content. */
  struct DRWPass *mask_invert_ps;
  /* Anti-Aliasing. */
  struct DRWPass *smaa_edge_ps;
  struct DRWPass *smaa_weight_ps;
  struct DRWPass *smaa_resolve_ps;
} GPENCIL_PassList;

typedef struct GPENCIL_FramebufferList {
  struct GPUFrameBuffer *render_fb;
  struct GPUFrameBuffer *gpencil_fb;
  struct GPUFrameBuffer *snapshot_fb;
  struct GPUFrameBuffer *layer_fb;
  struct GPUFrameBuffer *object_fb;
  struct GPUFrameBuffer *mask_fb;
  struct GPUFrameBuffer *smaa_edge_fb;
  struct GPUFrameBuffer *smaa_weight_fb;
} GPENCIL_FramebufferList;

typedef struct GPENCIL_TextureList {
  /* Dummy texture to avoid errors cause by empty sampler. */
  struct GPUTexture *dummy_texture;
  /* Snapshot for smoother drawing. */
  struct GPUTexture *snapshot_depth_tx;
  struct GPUTexture *snapshot_color_tx;
  struct GPUTexture *snapshot_reveal_tx;
  /* Textures used by Antialiasing. */
  struct GPUTexture *smaa_area_tx;
  struct GPUTexture *smaa_search_tx;
  /* Textures used during render. Containing underlying rendered scene. */
  struct GPUTexture *render_depth_tx;
  struct GPUTexture *render_color_tx;
} GPENCIL_TextureList;

typedef struct GPENCIL_Data {
  void *engine_type; /* Required */
  struct GPENCIL_FramebufferList *fbl;
  struct GPENCIL_TextureList *txl;
  struct GPENCIL_PassList *psl;
  struct GPENCIL_StorageList *stl;
} GPENCIL_Data;

/* *********** STATIC *********** */
typedef struct GPENCIL_PrivateData {
  /* Pointers copied from GPENCIL_ViewLayerData. */
  struct BLI_memblock *gp_object_pool;
  struct BLI_memblock *gp_layer_pool;
  struct BLI_memblock *gp_vfx_pool;
  struct BLI_memblock *gp_material_pool;
  struct BLI_memblock *gp_light_pool;
  struct BLI_memblock *gp_maskbit_pool;
  /* Last used material pool. */
  GPENCIL_MaterialPool *last_material_pool;
  /* Last used light pool. */
  GPENCIL_LightPool *last_light_pool;
  /* Common lightpool containing all lights in the scene. */
  GPENCIL_LightPool *global_light_pool;
  /* Common lightpool containing one ambient white light. */
  GPENCIL_LightPool *shadeless_light_pool;
  /* Linked list of tObjects. */
  struct {
    GPENCIL_tObject *first, *last;
  } tobjects, tobjects_infront;
  /* Temp Textures (shared with other engines). */
  GPUTexture *depth_tx;
  GPUTexture *color_tx;
  GPUTexture *color_layer_tx;
  GPUTexture *color_object_tx;
  /* Revealage is 1 - alpha */
  GPUTexture *reveal_tx;
  GPUTexture *reveal_layer_tx;
  GPUTexture *reveal_object_tx;
  /* Mask texture */
  GPUTexture *mask_tx;
  /* Anti-Aliasing. */
  GPUTexture *smaa_edge_tx;
  GPUTexture *smaa_weight_tx;
  /* Pointer to dtxl->depth */
  GPUTexture *scene_depth_tx;
  GPUFrameBuffer *scene_fb;
  /* Copy of txl->dummy_tx */
  GPUTexture *dummy_tx;
  /* Copy of v3d->shading.single_color. */
  float v3d_single_color[3];
  /* Copy of v3d->shading.color_type or -1 to ignore. */
  int v3d_color_type;
  /* Current frame */
  int cfra;
  /* If we are rendering for final render (F12). */
  bool is_render;
  /* If we are in viewport display (used for VFX). */
  bool is_viewport;
  /* True in selection and auto_depth drawing */
  bool draw_depth_only;
  /* Is shading set to wireframe. */
  bool draw_wireframe;
  /* Used by the depth merge step. */
  int is_stroke_order_3d;
  float object_bound_mat[4][4];
  /* Used for computing object distance to camera. */
  float camera_z_axis[3], camera_z_offset;
  float camera_pos[3];
  /* Pseudo depth of field parameter. Used to scale blur radius. */
  float dof_params[2];
  /* Used for DoF Setup. */
  Object *camera;
  /* Copy of draw_ctx->view_layer for convenience. */
  struct ViewLayer *view_layer;
  /* Copy of draw_ctx->scene for convenience. */
  struct Scene *scene;
  /* Copy of draw_ctx->vie3d for convenience. */
  struct View3D *v3d;

  /* Active object. */
  Object *obact;
  /* Object being in draw mode. */
  struct bGPdata *sbuffer_gpd;
  /* Layer to append the temp stroke to. */
  struct bGPDlayer *sbuffer_layer;
  /* Temporary stroke currently being drawn. */
  struct bGPDstroke *sbuffer_stroke;
  /* List of temp objects containing the stroke. */
  struct {
    GPENCIL_tObject *first, *last;
  } sbuffer_tobjects;
  /* Batches containing the temp stroke. */
  GPUBatch *stroke_batch;
  GPUBatch *fill_batch;
  bool do_fast_drawing;
  bool snapshot_buffer_dirty;

  /* Display onion skinning */
  bool do_onion;
  /* simplify settings */
  bool simplify_fill;
  bool simplify_fx;
  bool simplify_antialias;
  /* Use scene lighting or flat shading (global setting). */
  bool use_lighting;
  /* Use physical lights or just ambient lighting. */
  bool use_lights;
  /* Do we need additional framebuffers? */
  bool use_layer_fb;
  bool use_object_fb;
  bool use_mask_fb;
  /* Some blend mode needs to add negative values.
   * This is only supported if target texture is signed. */
  bool use_signed_fb;
  /* Use only lines for multiedit and not active frame. */
  bool use_multiedit_lines_only;
  /* Layer opacity for fading. */
  float fade_layer_opacity;
  /* Opacity for fading gpencil objects. */
  float fade_gp_object_opacity;
  /* Opacity for fading 3D objects. */
  float fade_3d_object_opacity;
  /* Mask opacity uniform. */
  float mask_opacity;
  /* Xray transparency in solid mode. */
  float xray_alpha;
  /* Mask invert uniform. */
  int mask_invert;
} GPENCIL_PrivateData;

/* geometry batch cache functions */
struct GpencilBatchCache *gpencil_batch_cache_get(struct Object *ob, int cfra);

GPENCIL_tObject *gpencil_object_cache_add(GPENCIL_PrivateData *pd, Object *ob);
void gpencil_object_cache_sort(GPENCIL_PrivateData *pd);

GPENCIL_tLayer *gpencil_layer_cache_add(GPENCIL_PrivateData *pd,
                                        const Object *ob,
                                        const bGPDlayer *gpl,
                                        const bGPDframe *gpf,
                                        GPENCIL_tObject *tgp_ob);
GPENCIL_tLayer *gpencil_layer_cache_get(GPENCIL_tObject *tgp_ob, int number);

GPENCIL_MaterialPool *gpencil_material_pool_create(GPENCIL_PrivateData *pd, Object *ob, int *ofs);
void gpencil_material_resources_get(GPENCIL_MaterialPool *first_pool,
                                    int mat_id,
                                    struct GPUTexture **r_tex_stroke,
                                    struct GPUTexture **r_tex_fill,
                                    struct GPUUniformBuffer **r_ubo_mat);

void gpencil_light_ambient_add(GPENCIL_LightPool *lightpool, const float color[3]);
void gpencil_light_pool_populate(GPENCIL_LightPool *matpool, Object *ob);
GPENCIL_LightPool *gpencil_light_pool_add(GPENCIL_PrivateData *pd);
GPENCIL_LightPool *gpencil_light_pool_create(GPENCIL_PrivateData *pd, Object *ob);

/* effects */
void gpencil_vfx_cache_populate(GPENCIL_Data *vedata, Object *ob, GPENCIL_tObject *tgp_ob);

/* Shaders */
struct GPUShader *GPENCIL_shader_antialiasing(int stage);
struct GPUShader *GPENCIL_shader_geometry_get(void);
struct GPUShader *GPENCIL_shader_composite_get(void);
struct GPUShader *GPENCIL_shader_layer_blend_get(void);
struct GPUShader *GPENCIL_shader_mask_invert_get(void);
struct GPUShader *GPENCIL_shader_depth_merge_get(void);
struct GPUShader *GPENCIL_shader_fx_blur_get(void);
struct GPUShader *GPENCIL_shader_fx_colorize_get(void);
struct GPUShader *GPENCIL_shader_fx_composite_get(void);
struct GPUShader *GPENCIL_shader_fx_transform_get(void);
struct GPUShader *GPENCIL_shader_fx_glow_get(void);
struct GPUShader *GPENCIL_shader_fx_pixelize_get(void);
struct GPUShader *GPENCIL_shader_fx_rim_get(void);
struct GPUShader *GPENCIL_shader_fx_shadow_get(void);

void GPENCIL_shader_free(void);

/* Antialiasing */
void GPENCIL_antialiasing_init(struct GPENCIL_Data *vedata);
void GPENCIL_antialiasing_draw(struct GPENCIL_Data *vedata);

/* main functions */
void GPENCIL_engine_init(void *vedata);
void GPENCIL_cache_init(void *vedata);
void GPENCIL_cache_populate(void *vedata, struct Object *ob);
void GPENCIL_cache_finish(void *vedata);
void GPENCIL_draw_scene(void *vedata);

/* render */
void GPENCIL_render_init(struct GPENCIL_Data *ved,
                         struct RenderEngine *engine,
                         struct RenderLayer *render_layer,
                         const struct Depsgraph *depsgraph,
                         const rcti *rect);
void GPENCIL_render_to_image(void *vedata,
                             struct RenderEngine *engine,
                             struct RenderLayer *render_layer,
                             const rcti *rect);

/* Draw Data. */
void gpencil_light_pool_free(void *storage);
void gpencil_material_pool_free(void *storage);
GPENCIL_ViewLayerData *GPENCIL_view_layer_data_ensure(void);

#endif /* __GPENCIL_ENGINE_H__ */
