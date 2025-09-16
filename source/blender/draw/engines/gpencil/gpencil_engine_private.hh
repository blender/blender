/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "BLI_bitmap.h"
#include "BLI_memblock.h"

#include "DNA_shader_fx_types.h"
#include "DRW_render.hh"

#include "BKE_grease_pencil.hh"

#include "GPU_batch.hh"

#include "draw_pass.hh"
#include "draw_view_data.hh"

#define GP_LIGHT

#include "gpencil_defines.hh"
#include "gpencil_shader.hh"
#include "gpencil_shader_shared.hh"

struct GpencilBatchCache;
struct Object;
struct RenderEngine;
struct RenderLayer;
struct View3D;

/* used to convert pixel scale. */
#define GPENCIL_PIXEL_FACTOR 2000.0f

/* used to expand VBOs. Size has a big impact in the speed */
#define GPENCIL_VBO_BLOCK_SIZE 128

#define GP_MAX_MASKBITS 256

namespace blender::draw::gpencil {

struct MaterialPool {
  /* Single linked-list. */
  MaterialPool *next;
  /* GPU representation of materials. */
  gpMaterial mat_data[GPENCIL_MATERIAL_BUFFER_LEN];
  /* Matching ubo. */
  gpu::UniformBuf *ubo;
  /* Texture per material. NULL means none. */
  gpu::Texture *tex_fill[GPENCIL_MATERIAL_BUFFER_LEN];
  gpu::Texture *tex_stroke[GPENCIL_MATERIAL_BUFFER_LEN];
  /* Number of material used in this pool. */
  int used_count;
};

struct LightPool {
  /* GPU representation of materials. */
  gpLight light_data[GPENCIL_LIGHT_BUFFER_LEN];
  /* Matching ubo. */
  gpu::UniformBuf *ubo;
  /* Number of light in the pool. */
  int light_used;
};

/* Temporary gpencil FX reflection used by the gpencil::Instance. */
struct tVfx {
  /** Single linked-list. */
  struct tVfx *next = nullptr;
  std::unique_ptr<PassSimple> vfx_ps = std::make_unique<PassSimple>("vfx");
  /* Frame-buffer reference since it may not be allocated yet. */
  gpu::FrameBuffer **target_fb = nullptr;
};

/* Temporary gpencil layer reflection used by the gpencil::Instance. */
struct tLayer {
  /** Single linked-list. */
  struct tLayer *next;
  /** Geometry pass (draw all strokes). */
  std::unique_ptr<PassSimple> geom_ps;
  /** Blend pass to composite onto the target buffer (blends modes). NULL if not needed. */
  std::unique_ptr<PassSimple> blend_ps;
  /** Layer id of the mask. */
  BLI_bitmap *mask_bits;
  BLI_bitmap *mask_invert_bits;
  /** Index in the layer list. Used as id for masking. */
  int layer_id;
  /** True if this pass is part of the onion skinning. */
  bool is_onion;
};

/* Temporary object reflection used by the gpencil::Instance. */
struct tObject {
  /** Single linked-list. */
  struct tObject *next;

  struct {
    tLayer *first, *last;
  } layers;

  struct {
    tVfx *first, *last;
  } vfx;

  /* Distance to camera. Used for sorting. */
  float camera_z;
  /* Normal used for shading. Based on view angle. */
  float3 plane_normal;
  /* Used for drawing depth merge pass. */
  float plane_mat[4][4];

  bool is_drawmode3d;

  /* Use Material Holdout. */
  bool do_mat_holdout;
};

/* *********** LISTS *********** */

struct Instance final : public DrawEngine {
  PassSimple smaa_edge_ps = {"smaa_edge"};
  PassSimple smaa_weight_ps = {"smaa_weight"};
  PassSimple smaa_resolve_ps = {"smaa_resolve"};
  PassSimple accumulate_ps = {"aa_accumulate"};
  /* Composite the object depth to the default depth buffer to occlude overlays. */
  PassSimple merge_depth_ps = {"merge_depth_ps"};
  /* Invert mask buffer content. */
  PassSimple mask_invert_ps = {"mask_invert_ps"};

  float4x4 object_bound_mat;

  /* Dummy texture to avoid errors cause by empty sampler. */
  Texture dummy_texture = {"dummy_texture"};
  Texture dummy_depth = {"dummy_depth"};
  /* Textures used during render. Containing underlying rendered scene. */
  Texture render_depth_tx = {"render_depth_tx"};
  Texture render_color_tx = {"render_color_tx"};
  /* Snapshot for smoother drawing. */
  Texture snapshot_depth_tx = {"snapshot_depth_tx"};
  Texture snapshot_color_tx = {"snapshot_color_tx"};
  Texture snapshot_reveal_tx = {"snapshot_reveal_tx"};
  /* Textures used by Anti-aliasing. */
  Texture smaa_area_tx = {"smaa_area_tx"};
  Texture smaa_search_tx = {"smaa_search_tx"};

  /* Temp Textures (shared with other engines). */
  TextureFromPool depth_tx = {"depth_tx"};
  TextureFromPool color_tx = {"color_tx"};
  TextureFromPool color_layer_tx = {"color_layer_tx"};
  TextureFromPool color_object_tx = {"color_object_tx"};
  /* Revealage is 1 - alpha */
  TextureFromPool reveal_tx = {"reveal_tx"};
  TextureFromPool reveal_layer_tx = {"reveal_layer_tx"};
  TextureFromPool reveal_object_tx = {"reveal_object_tx"};
  /* Mask texture */
  TextureFromPool mask_depth_tx = {"mask_depth_tx"};
  TextureFromPool mask_color_tx = {"mask_color_tx"};
  TextureFromPool mask_tx = {"mask_tx"};
  /* Anti-Aliasing. */
  TextureFromPool smaa_edge_tx = {"smaa_edge_tx"};
  TextureFromPool smaa_weight_tx = {"smaa_weight_tx"};

  Framebuffer render_fb = {"render_fb"};
  Framebuffer gpencil_fb = {"gpencil_fb"};
  Framebuffer gpencil_pass_fb = {"gpencil_pass_fb"};
  Framebuffer snapshot_fb = {"snapshot_fb"};
  Framebuffer layer_fb = {"layer_fb"};
  Framebuffer object_fb = {"object_fb"};
  Framebuffer mask_fb = {"mask_fb"};
  Framebuffer smaa_edge_fb = {"smaa_edge_fb"};
  Framebuffer smaa_weight_fb = {"smaa_weight_fb"};

  /* NOTE: These do not preserve the PassSimple memory across frames.
   * If that becomes a bottleneck, these containers can be improved. */
  using tVfx_Pool = draw::detail::SubPassVector<tVfx>;
  using tLayer_Pool = draw::detail::SubPassVector<tLayer>;

  /* tObject */
  struct BLI_memblock *gp_object_pool = BLI_memblock_create(sizeof(tObject));
  /* tLayer */
  tLayer_Pool *gp_layer_pool = new tLayer_Pool();
  /* tVfx */
  tVfx_Pool *gp_vfx_pool = new tVfx_Pool();
  /* MaterialPool */
  struct BLI_memblock *gp_material_pool = BLI_memblock_create(sizeof(MaterialPool));
  /* LightPool */
  struct BLI_memblock *gp_light_pool = BLI_memblock_create(sizeof(LightPool));
  /* BLI_bitmap */
  struct BLI_memblock *gp_maskbit_pool = BLI_memblock_create(BLI_BITMAP_SIZE(GP_MAX_MASKBITS));

  const DRWContext *draw_ctx = nullptr;

  /* Last used material pool. */
  MaterialPool *last_material_pool;
  /* Last used light pool. */
  LightPool *last_light_pool;
  /* Common lightpool containing all lights in the scene. */
  LightPool *global_light_pool;
  /* Common lightpool containing one ambient white light. */
  LightPool *shadeless_light_pool;
  /* Linked list of tObjects. */
  struct {
    tObject *first, *last;
  } tobjects, tobjects_infront;
  /* Used to record whether the `tobjects` list is sorted. Do not sort drawings again in separate
   * pass rendering to avoid generating infinite lists. */
  bool is_sorted;
  /* Pointer to dtxl->depth */
  gpu::Texture *scene_depth_tx;
  gpu::FrameBuffer *scene_fb;
  /* Used for render accumulation antialiasing. */
  Texture accumulation_tx = {"gp_accumulation_tx"};
  Framebuffer accumulation_fb = {"gp_accumulation_fb"};
  /* Copy of txl->dummy_tx */
  gpu::Texture *dummy_tx;
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
  /* Is shading set to wire-frame. */
  bool draw_wireframe;
  /* Used by the depth merge step. */
  int is_stroke_order_3d;
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
  /* List of temp objects containing the stroke. */
  struct {
    tObject *first, *last;
  } sbuffer_tobjects;
  /* Batches containing the temp stroke. */
  gpu::Batch *stroke_batch;
  gpu::Batch *fill_batch;
  bool snapshot_buffer_dirty;

  /* Display onion skinning */
  bool do_onion;
  /* Show only the onion skins of the active object. */
  bool do_onion_only_active_object;
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
  /* If viewport compositor is active, we need to render grease pencil onto another additional
   * pass. */
  bool use_separate_pass;
  /* Some blend mode needs to add negative values.
   * This is only supported if target texture is signed. Only switch for the `reveal_tex`. */
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
  /* Force 3D depth rendering. */
  bool force_stroke_order_3d;

  ~Instance() final
  {
    BLI_memblock_destroy(gp_light_pool, light_pool_free);
    BLI_memblock_destroy(gp_material_pool, material_pool_free);
    BLI_memblock_destroy(gp_maskbit_pool, nullptr);
    BLI_memblock_destroy(gp_object_pool, nullptr);
    delete gp_layer_pool;
    delete gp_vfx_pool;
  }

  void acquire_resources();
  void release_resources();

  StringRefNull name_get() final
  {
    return "Grease Pencil";
  }

  void init() final;

  void begin_sync() final;
  void object_sync(ObjectRef &ob_ref, Manager &manager) final;
  void end_sync() final;

  void draw(Manager &manager) final;

  void antialiasing_accumulate(Manager &manager, float alpha);

  static float2 antialiasing_sample_get(int sample_index, int sample_count);

 private:
  tObject *object_sync_do(Object *ob, ResourceHandleRange res_handle);

  /* Check if the passed in layer is used by any other layer as a mask (in the viewlayer). */
  bool is_used_as_layer_mask_in_viewlayer(const GreasePencil &grease_pencil,
                                          const bke::greasepencil::Layer &mask_layer,
                                          const ViewLayer &view_layer);

  /* Returns true if this layer should be rendered (as part of the viewlayer). */
  bool use_layer_in_render(const GreasePencil &grease_pencil,
                           const bke::greasepencil::Layer &layer,
                           const ViewLayer &view_layer,
                           bool &r_is_used_as_mask);

  void draw_mask(View &view, tObject *ob, tLayer *layer);
  void draw_object(View &view, tObject *ob);

  void antialiasing_init();
  void antialiasing_draw(Manager &manager);

  struct VfxFramebufferRef {
    /* These may not be allocated yet, use address of future pointer. */
    gpu::FrameBuffer **fb;
    gpu::Texture **color_tx;
    gpu::Texture **reveal_tx;
  };

  SwapChain<VfxFramebufferRef, 2> vfx_swapchain_;

  PassSimple &vfx_pass_create(const char *name,
                              DRWState state,
                              gpu::Shader *sh,
                              tObject *tgp_ob,
                              GPUSamplerState sampler = GPUSamplerState::internal_sampler());

  void vfx_blur_sync(BlurShaderFxData *fx, Object *ob, tObject *tgp_ob);
  void vfx_colorize_sync(ColorizeShaderFxData *fx, Object *ob, tObject *tgp_ob);
  void vfx_flip_sync(FlipShaderFxData *fx, Object *ob, tObject *tgp_ob);
  void vfx_rim_sync(RimShaderFxData *fx, Object *ob, tObject *tgp_ob);
  void vfx_pixelize_sync(PixelShaderFxData *fx, Object *ob, tObject *tgp_ob);
  void vfx_shadow_sync(ShadowShaderFxData *fx, Object *ob, tObject *tgp_ob);
  void vfx_glow_sync(GlowShaderFxData *fx, Object *ob, tObject *tgp_ob);
  void vfx_wave_sync(WaveShaderFxData *fx, Object *ob, tObject *tgp_ob);
  void vfx_swirl_sync(SwirlShaderFxData *fx, Object *ob, tObject *tgp_ob);

  void vfx_sync(Object *ob, tObject *tgp_ob);

  static void material_pool_free(void *storage)
  {
    MaterialPool *matpool = (MaterialPool *)storage;
    GPU_UBO_FREE_SAFE(matpool->ubo);
  }

  static void light_pool_free(void *storage)
  {
    LightPool *lightpool = (LightPool *)storage;
    GPU_UBO_FREE_SAFE(lightpool->ubo);
  }
};

struct GPENCIL_Data {
  void *engine_type; /* Required */
  struct Instance *instance;

  char info[GPU_INFO_SIZE];
};

/* geometry batch cache functions */
struct GpencilBatchCache *gpencil_batch_cache_get(struct Object *ob, int cfra);

tObject *gpencil_object_cache_add(Instance *inst,
                                  Object *ob,
                                  bool is_stroke_order_3d,
                                  Bounds<float3> bounds);
void gpencil_object_cache_sort(Instance *inst);

tLayer *grease_pencil_layer_cache_get(tObject *tgp_ob, int layer_id, bool skip_onion);

tLayer *grease_pencil_layer_cache_add(Instance *inst,
                                      const Object *ob,
                                      const bke::greasepencil::Layer &layer,
                                      int onion_id,
                                      bool is_used_as_mask,
                                      tObject *tgp_ob);
/**
 * Creates a linked list of material pool containing all materials assigned for a given object.
 * We merge the material pools together if object does not contain a huge amount of materials.
 * Also return an offset to the first material of the object in the UBO.
 */
MaterialPool *gpencil_material_pool_create(Instance *inst,
                                           Object *ob,
                                           int *ofs,
                                           bool is_vertex_mode);
void gpencil_material_resources_get(MaterialPool *first_pool,
                                    int mat_id,
                                    gpu::Texture **r_tex_stroke,
                                    gpu::Texture **r_tex_fill,
                                    gpu::UniformBuf **r_ubo_mat);

void gpencil_light_ambient_add(LightPool *lightpool, const float color[3]);
void gpencil_light_pool_populate(LightPool *lightpool, Object *ob);
LightPool *gpencil_light_pool_add(Instance *inst);
/**
 * Creates a single pool containing all lights assigned (light linked) for a given object.
 */
LightPool *gpencil_light_pool_create(Instance *inst, Object *ob);

}  // namespace blender::draw::gpencil
