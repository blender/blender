/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BKE_gpencil_modifier_legacy.h"
#include "BLI_listbase_wrapper.hh"
#include "DEG_depsgraph_query.h"
#include "DNA_shader_fx_types.h"
#include "DRW_engine.h"
#include "DRW_render.h"
#include "ED_screen.hh"
#include "ED_view3d.hh"
#include "GPU_capabilities.h"
#include "IMB_imbuf_types.h"

#include "draw_manager.hh"
#include "draw_pass.hh"

#define GP_LIGHT
#include "gpencil_antialiasing.hh"
#include "gpencil_defines.h"
#include "gpencil_engine.h"
#include "gpencil_layer.hh"
#include "gpencil_light.hh"
#include "gpencil_material.hh"
#include "gpencil_object.hh"
#include "gpencil_shader.hh"
#include "gpencil_shader_shared.h"
#include "gpencil_vfx.hh"

namespace blender::draw::greasepencil {

using namespace draw;

class Instance {
 private:
  ShaderModule &shaders;
  LayerModule layers;
  MaterialModule materials;
  ObjectModule objects;
  LightModule lights;
  VfxModule vfx;
  AntiAliasing anti_aliasing;

  /** Contains all gpencil objects in the scene as well as their effect sub-passes. */
  PassSortable main_ps_ = {"gp_main_ps"};

  /** Contains all composited GPencil object. */
  TextureFromPool depth_tx_ = {"gp_depth_tx"};
  TextureFromPool color_tx_ = {"gp_color_tx"};
  TextureFromPool reveal_tx_ = {"gp_reveal_tx"};
  Framebuffer main_fb_ = {"gp_main_fb"};

  /** Texture format for all intermediate buffers. */
  eGPUTextureFormat texture_format_ = GPU_RGBA16F;

  UniformBuffer<gpScene> scene_buf_;

  /** Dummy textures. */
  static constexpr float dummy_px_[4] = {1.0f, 0.0f, 1.0f, 1.0f};
  Texture dummy_depth_tx_ = {"dummy_depth",
                             GPU_DEPTH_COMPONENT32F,
                             GPU_TEXTURE_USAGE_SHADER_READ,
                             int2(1),
                             (float *)dummy_px_};
  Texture dummy_color_tx_ = {
      "dummy_color", GPU_RGBA16F, GPU_TEXTURE_USAGE_SHADER_READ, int2(1), (float *)dummy_px_};

  /** Scene depth used for manual depth testing. Default to dummy depth to skip depth test. */
  GPUTexture *scene_depth_tx_ = dummy_depth_tx_;

  /** Context. */
  Depsgraph *depsgraph_ = nullptr;
  Object *camera_ = nullptr;

  /** \note Needs not to be temporary variable since it is dereferenced later. */
  std::array<float4, 2> clear_colors_ = {float4(0.0f, 0.0f, 0.0f, 0.0f),
                                         float4(1.0f, 1.0f, 1.0f, 1.0f)};

 public:
  Instance()
      : shaders(*ShaderModule::module_get()),
        objects(layers, materials, shaders),
        vfx(shaders),
        anti_aliasing(shaders){};

  void init(Depsgraph *depsgraph, const View3D *v3d, const RegionView3D *rv3d)
  {
    depsgraph_ = depsgraph;
    const Scene *scene = DEG_get_evaluated_scene(depsgraph_);

    const bool is_viewport = (v3d != nullptr);

    if (is_viewport) {
      /* Use lower precision for viewport. */
      texture_format_ = GPU_R11F_G11F_B10F;
      camera_ = (rv3d->persp == RV3D_CAMOB) ? v3d->camera : nullptr;
    }

    objects.init(v3d, scene);
    lights.init(v3d);
    /* TODO(@fclem): VFX. */
    // vfx.init(use_vfx_, camera_, rv3d);
    anti_aliasing.init(v3d, scene);
  }

  void begin_sync(Manager & /* manager */)
  {
    /* TODO(fclem): Remove global draw manager access. */
    View main_view("GPencil_MainView", DRW_view_default_get());

    objects.begin_sync(depsgraph_, main_view);
    layers.begin_sync();
    materials.begin_sync();
    lights.begin_sync(depsgraph_);

    main_ps_.init();
    PassMain::Sub &sub = main_ps_.sub("InitSubpass", -FLT_MAX);
    sub.framebuffer_set(&main_fb_);
    sub.clear_multi(clear_colors_);
    /* TODO(fclem): Textures. */
    sub.bind_texture(GPENCIL_SCENE_DEPTH_TEX_SLOT, &dummy_depth_tx_);
    sub.bind_texture(GPENCIL_MASK_TEX_SLOT, &dummy_color_tx_);
    sub.bind_texture(GPENCIL_FILL_TEX_SLOT, &dummy_color_tx_);
    sub.bind_texture(GPENCIL_STROKE_TEX_SLOT, &dummy_color_tx_);
    sub.bind_ubo(GPENCIL_SCENE_SLOT, &scene_buf_);
    objects.bind_resources(sub);
    layers.bind_resources(sub);
    materials.bind_resources(sub);
    lights.bind_resources(sub);

    anti_aliasing.begin_sync(color_tx_, reveal_tx_);
  }

  void object_sync(Manager &manager, ObjectRef &object_ref)
  {
    switch (object_ref.object->type) {
      case OB_GREASE_PENCIL:
        objects.sync_grease_pencil(manager, object_ref, main_fb_, main_ps_);
        break;
      case OB_LAMP:
        lights.sync(object_ref);
        break;
      default:
        break;
    }
  }

  void end_sync(Manager & /* manager */)
  {
    objects.end_sync();
    layers.end_sync();
    materials.end_sync();
    lights.end_sync();
  }

  void draw_viewport(Manager &manager,
                     View &view,
                     GPUTexture *dst_depth_tx,
                     GPUTexture *dst_color_tx)
  {
    if (!objects.scene_has_visible_gpencil_object()) {
      return;
    }

    int2 render_size = {GPU_texture_width(dst_depth_tx), GPU_texture_height(dst_depth_tx)};

    depth_tx_.acquire(render_size, GPU_DEPTH24_STENCIL8);
    color_tx_.acquire(render_size, texture_format_);
    reveal_tx_.acquire(render_size, texture_format_);
    main_fb_.ensure(GPU_ATTACHMENT_TEXTURE(depth_tx_),
                    GPU_ATTACHMENT_TEXTURE(color_tx_),
                    GPU_ATTACHMENT_TEXTURE(reveal_tx_));

    scene_buf_.render_size = float2(render_size);
    scene_buf_.push_update();

    objects.acquire_temporary_buffers(render_size, texture_format_);

    manager.submit(main_ps_, view);

    objects.release_temporary_buffers();

    anti_aliasing.draw(manager, dst_color_tx);

    depth_tx_.release();
    color_tx_.release();
    reveal_tx_.release();
  }
};

}  // namespace blender::draw::greasepencil

/* -------------------------------------------------------------------- */
/** \name Interface with legacy C DRW manager
 * \{ */

using namespace blender;

struct GPENCIL_NEXT_Data {
  DrawEngineType *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  DRWViewportEmptyList *psl;
  DRWViewportEmptyList *stl;
  draw::greasepencil::Instance *instance;

  char info[GPU_INFO_SIZE];
};

static void gpencil_engine_init(void *vedata)
{
  /* TODO(fclem): Remove once it is minimum required. */
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }

  GPENCIL_NEXT_Data *ved = reinterpret_cast<GPENCIL_NEXT_Data *>(vedata);
  if (ved->instance == nullptr) {
    ved->instance = new draw::greasepencil::Instance();
  }

  const DRWContextState *ctx_state = DRW_context_state_get();

  ved->instance->init(ctx_state->depsgraph, ctx_state->v3d, ctx_state->rv3d);
}

static void gpencil_draw_scene(void *vedata)
{
  GPENCIL_NEXT_Data *ved = reinterpret_cast<GPENCIL_NEXT_Data *>(vedata);
  if (!GPU_shader_storage_buffer_objects_support()) {
    STRNCPY(ved->info, "Error: No shader storage buffer support");
    return;
  }
  if (DRW_state_is_select() || DRW_state_is_depth()) {
    return;
  }
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  const DRWView *default_view = DRW_view_default_get();
  draw::Manager *manager = DRW_manager_get();
  draw::View view("DefaultView", default_view);
  ved->instance->draw_viewport(*manager, view, dtxl->depth, dtxl->color);
}

static void gpencil_cache_init(void *vedata)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  draw::Manager *manager = DRW_manager_get();
  reinterpret_cast<GPENCIL_NEXT_Data *>(vedata)->instance->begin_sync(*manager);
}

static void gpencil_cache_populate(void *vedata, Object *object)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  draw::Manager *manager = DRW_manager_get();

  draw::ObjectRef ref;
  ref.object = object;
  ref.dupli_object = DRW_object_get_dupli(object);
  ref.dupli_parent = DRW_object_get_dupli_parent(object);

  reinterpret_cast<GPENCIL_NEXT_Data *>(vedata)->instance->object_sync(*manager, ref);
}

static void gpencil_cache_finish(void *vedata)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  draw::Manager *manager = DRW_manager_get();
  reinterpret_cast<GPENCIL_NEXT_Data *>(vedata)->instance->end_sync(*manager);
}

static void gpencil_instance_free(void *instance)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  delete reinterpret_cast<draw::greasepencil::Instance *>(instance);
}

static void gpencil_engine_free()
{
  blender::draw::greasepencil::ShaderModule::module_free();
}

static void gpencil_render_to_image(void * /*vedata*/,
                                    RenderEngine * /*engine*/,
                                    RenderLayer * /*layer*/,
                                    const rcti * /*rect*/)
{
}

extern "C" {

static const DrawEngineDataSize gpencil_data_size = DRW_VIEWPORT_DATA_SIZE(GPENCIL_NEXT_Data);

DrawEngineType draw_engine_gpencil_next_type = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ N_("Gpencil"),
    /*vedata_size*/ &gpencil_data_size,
    /*engine_init*/ &gpencil_engine_init,
    /*engine_free*/ &gpencil_engine_free,
    /*instance_free*/ &gpencil_instance_free,
    /*cache_init*/ &gpencil_cache_init,
    /*cache_populate*/ &gpencil_cache_populate,
    /*cache_finish*/ &gpencil_cache_finish,
    /*draw_scene*/ &gpencil_draw_scene,
    /*view_update*/ nullptr,
    /*id_update*/ nullptr,
    /*render_to_image*/ &gpencil_render_to_image,
    /*store_metadata*/ nullptr,
};
}

/** \} */
