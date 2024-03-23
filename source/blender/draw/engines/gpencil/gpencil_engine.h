/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "DNA_gpencil_legacy_types.h"

#include "DRW_render.hh"

#include "BLI_bitmap.h"
#include "BLI_bounds.hh"

#include "BKE_grease_pencil.hh"

#include "GPU_batch.hh"

#ifdef __cplusplus
extern "C" {
#endif

#define GP_LIGHT

#include "gpencil_defines.h"
#include "gpencil_shader_shared.h"

extern DrawEngineType draw_engine_gpencil_type;

struct GPENCIL_Data;
struct GPENCIL_StorageList;
struct GPUBatch;
struct GpencilBatchCache;
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

/* *********** Draw Data *********** */
typedef struct GPENCIL_MaterialPool {
  /* Single linked-list. */
  struct GPENCIL_MaterialPool *next;
  /* GPU representation of materials. */
  gpMaterial mat_data[GPENCIL_MATERIAL_BUFFER_LEN];
  /* Matching ubo. */
  struct GPUUniformBuf *ubo;
  /* Texture per material. NULL means none. */
  struct GPUTexture *tex_fill[GPENCIL_MATERIAL_BUFFER_LEN];
  struct GPUTexture *tex_stroke[GPENCIL_MATERIAL_BUFFER_LEN];
  /* Number of material used in this pool. */
  int used_count;
} GPENCIL_MaterialPool;

typedef struct GPENCIL_LightPool {
  /* GPU representation of materials. */
  gpLight light_data[GPENCIL_LIGHT_BUFFER_LEN];
  /* Matching ubo. */
  struct GPUUniformBuf *ubo;
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
  /** Single linked-list. */
  struct GPENCIL_tVfx *next;
  DRWPass *vfx_ps;
  /* Frame-buffer reference since it may not be allocated yet. */
  GPUFrameBuffer **target_fb;
} GPENCIL_tVfx;

typedef struct GPENCIL_tLayer {
  /** Single linked-list. */
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
  /** Single linked-list. */
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

  /* Use Material Holdout. */
  bool do_mat_holdout;

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
  /* If we are rendering for final render (F12).
   * NOTE: set to false for viewport and opengl rendering (including sequencer scene rendering),
   * but set to true when rendering in #OB_RENDER shading mode (viewport or opengl rendering). */
  bool is_render;
  /* If we are in viewport display (used for VFX). */
  bool is_viewport;
  /* True in selection and auto_depth drawing */
  bool draw_depth_only;
  /* Is shading set to wire-frame. */
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
  /* Playing animation */
  bool playing;
  /* simplify settings */
  bool simplify_fill;
  bool simplify_fx;
  bool simplify_antialias;
  /* Use scene lighting or flat shading (global setting). */
  bool use_lighting;
  /* Use physical lights or just ambient lighting. */
  bool use_lights;
  /* Do we need additional frame-buffers? */
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
  /* X-ray transparency in solid mode. */
  float xray_alpha;
  /* Mask invert uniform. */
  int mask_invert;
  /* Vertex Paint opacity. */
  float vertex_paint_opacity;
} GPENCIL_PrivateData;

/* geometry batch cache functions */
struct GpencilBatchCache *gpencil_batch_cache_get(struct Object *ob, int cfra);

GPENCIL_tObject *gpencil_object_cache_add(GPENCIL_PrivateData *pd,
                                          Object *ob,
                                          bool is_stroke_order_3d,
                                          blender::Bounds<float3> bounds);
void gpencil_object_cache_sort(GPENCIL_PrivateData *pd);

GPENCIL_tLayer *gpencil_layer_cache_add(GPENCIL_PrivateData *pd,
                                        const Object *ob,
                                        const bGPDlayer *gpl,
                                        const bGPDframe *gpf,
                                        GPENCIL_tObject *tgp_ob);
GPENCIL_tLayer *gpencil_layer_cache_get(GPENCIL_tObject *tgp_ob, int number);

GPENCIL_tLayer *grease_pencil_layer_cache_add(GPENCIL_PrivateData *pd,
                                              const Object *ob,
                                              const blender::bke::greasepencil::Layer &layer,
                                              std::optional<int> onion_id,
                                              GPENCIL_tObject *tgp_ob);
/**
 * Creates a linked list of material pool containing all materials assigned for a given object.
 * We merge the material pools together if object does not contain a huge amount of materials.
 * Also return an offset to the first material of the object in the UBO.
 */
GPENCIL_MaterialPool *gpencil_material_pool_create(GPENCIL_PrivateData *pd,
                                                   Object *ob,
                                                   int *ofs,
                                                   bool is_vertex_mode);
void gpencil_material_resources_get(GPENCIL_MaterialPool *first_pool,
                                    int mat_id,
                                    struct GPUTexture **r_tex_stroke,
                                    struct GPUTexture **r_tex_fill,
                                    struct GPUUniformBuf **r_ubo_mat);

void gpencil_light_ambient_add(GPENCIL_LightPool *lightpool, const float color[3]);
void gpencil_light_pool_populate(GPENCIL_LightPool *lightpool, Object *ob);
GPENCIL_LightPool *gpencil_light_pool_add(GPENCIL_PrivateData *pd);
/**
 * Creates a single pool containing all lights assigned (light linked) for a given object.
 */
GPENCIL_LightPool *gpencil_light_pool_create(GPENCIL_PrivateData *pd, Object *ob);

/* effects */
void gpencil_vfx_cache_populate(GPENCIL_Data *vedata,
                                Object *ob,
                                GPENCIL_tObject *tgp_ob,
                                const bool is_edit_mode);

/* Shaders */
struct GPUShader *GPENCIL_shader_antialiasing(int stage);
struct GPUShader *GPENCIL_shader_geometry_get(void);
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

/**
 * Initialize render data.
 */
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
#ifdef __cplusplus
}
#endif
