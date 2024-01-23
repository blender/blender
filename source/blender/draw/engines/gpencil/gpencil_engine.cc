/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BKE_gpencil_modifier_legacy.h"

#include "BLI_listbase_wrapper.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_shader_fx_types.h"

#include "DRW_engine.hh"
#include "DRW_render.hh"

#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "GPU_capabilities.h"

#include "IMB_imbuf_types.hh"

#include "RE_pipeline.h"

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

  /** Underlying scene pixel. Used to composite the output of the grease pencil render onto the
   * scene (including merging the depth buffers). */
  Framebuffer scene_fb_ = {"gp_scene_fb"};

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
  Manager *manager_ = nullptr;
  draw::View view_ = {"MainView"};

  /** \note Needs not to be temporary variable since it is dereferenced later. */
  std::array<float4, 2> clear_colors_ = {float4(0.0f, 0.0f, 0.0f, 0.0f),
                                         float4(1.0f, 1.0f, 1.0f, 1.0f)};

 public:
  Instance()
      : shaders(*ShaderModule::module_get()),
        objects(layers, materials, shaders),
        vfx(shaders),
        anti_aliasing(shaders){};

  void init(Depsgraph *depsgraph,
            Manager *manager,
            const DRWView *viewport_draw_view,
            const View3D *v3d,
            const RegionView3D *rv3d)
  {
    depsgraph_ = depsgraph;
    manager_ = manager;
    if (viewport_draw_view != nullptr) {
      view_.sync(viewport_draw_view);
    }

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

  void begin_sync()
  {
    objects.begin_sync(depsgraph_, view_);
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

    anti_aliasing.begin_sync(color_tx_, scene_fb_, reveal_tx_);
  }

  void object_sync(ObjectRef &object_ref)
  {
    switch (object_ref.object->type) {
      case OB_GREASE_PENCIL:
        objects.sync_grease_pencil(
            *manager_, object_ref, main_fb_, scene_fb_, depth_tx_, main_ps_);
        break;
      case OB_LAMP:
        lights.sync(object_ref);
        break;
      default:
        break;
    }
  }

  void end_sync()
  {
    objects.end_sync();
    layers.end_sync();
    materials.end_sync();
    lights.end_sync();
  }

  void render_sync(RenderEngine *engine, Depsgraph *depsgraph)
  {
    /* TODO: Remove old draw manager calls. */
    DRW_cache_restart();

    manager_->begin_sync();

    begin_sync();

    auto object_sync_render =
        [](void *vedata, Object *ob, RenderEngine * /*engine*/, Depsgraph * /*depsgraph*/) {
          Instance &inst = *reinterpret_cast<Instance *>(vedata);
          ObjectRef ob_ref = DRW_object_ref_get(ob);
          inst.object_sync(ob_ref);
        };

    /* HACK: We pass `this` here so we have access to the `Instance` in `object_sync_render`. */
    DRW_render_object_iter(this, engine, depsgraph, object_sync_render);

    end_sync();

    manager_->end_sync();

    /* TODO: Remove old draw manager calls. */
    DRW_render_instance_buffer_finish();
  }

  void draw(GPUTexture *dst_color_tx, GPUTexture *dst_depth_tx, const int2 render_resolution)
  {
    if (!objects.scene_has_visible_gpencil_object()) {
      return;
    }

    scene_fb_.ensure(GPU_ATTACHMENT_TEXTURE(dst_depth_tx), GPU_ATTACHMENT_TEXTURE(dst_color_tx));

    depth_tx_.acquire(render_resolution, GPU_DEPTH24_STENCIL8);
    color_tx_.acquire(render_resolution, texture_format_);
    reveal_tx_.acquire(render_resolution, texture_format_);
    main_fb_.ensure(GPU_ATTACHMENT_TEXTURE(depth_tx_),
                    GPU_ATTACHMENT_TEXTURE(color_tx_),
                    GPU_ATTACHMENT_TEXTURE(reveal_tx_));

    scene_buf_.render_size = float2(render_resolution);
    scene_buf_.push_update();

    objects.acquire_temporary_buffers(render_resolution, texture_format_);

    manager_->submit(main_ps_, view_);

    objects.release_temporary_buffers();

    anti_aliasing.draw(*manager_, render_resolution);

    depth_tx_.release();
    color_tx_.release();
    reveal_tx_.release();
  }

  draw::View &view()
  {
    return view_;
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
  GPENCIL_NEXT_Data *ved = reinterpret_cast<GPENCIL_NEXT_Data *>(vedata);
  if (ved->instance == nullptr) {
    ved->instance = new draw::greasepencil::Instance();
  }

  draw::Manager *manager = DRW_manager_get();
  const DRWContextState *ctx_state = DRW_context_state_get();
  const DRWView *default_view = DRW_view_default_get();

  ved->instance->init(
      ctx_state->depsgraph, manager, default_view, ctx_state->v3d, ctx_state->rv3d);
}

static void gpencil_draw_scene(void *vedata)
{
  GPENCIL_NEXT_Data *ved = reinterpret_cast<GPENCIL_NEXT_Data *>(vedata);
  if (DRW_state_is_select() || DRW_state_is_depth()) {
    return;
  }
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  const DRWView *default_view = DRW_view_default_get();
  const float2 viewport_size = DRW_viewport_size_get();
  ved->instance->view().sync(default_view);
  ved->instance->draw(dtxl->color, dtxl->depth, int2(viewport_size));
}

static void gpencil_cache_init(void *vedata)
{
  reinterpret_cast<GPENCIL_NEXT_Data *>(vedata)->instance->begin_sync();
}

static void gpencil_cache_populate(void *vedata, Object *object)
{
  draw::ObjectRef ref;
  ref.object = object;
  ref.dupli_object = DRW_object_get_dupli(object);
  ref.dupli_parent = DRW_object_get_dupli_parent(object);

  reinterpret_cast<GPENCIL_NEXT_Data *>(vedata)->instance->object_sync(ref);
}

static void gpencil_cache_finish(void *vedata)
{
  reinterpret_cast<GPENCIL_NEXT_Data *>(vedata)->instance->end_sync();
}

static void gpencil_instance_free(void *instance)
{
  delete reinterpret_cast<draw::greasepencil::Instance *>(instance);
}

static void gpencil_engine_free()
{
  blender::draw::greasepencil::ShaderModule::module_free();
}

/** Get the color and depth textures of the render result in the render layer. */
static void get_render_result_textures(RenderEngine *engine,
                                       RenderLayer *render_layer,
                                       const draw::View &view,
                                       const int2 render_resolution,
                                       draw::Texture &r_color_tx,
                                       draw::Texture &r_depth_tx)
{
  /* Create depth texture & color texture from render result. */
  const char *viewname = RE_GetActiveRenderView(engine->re);
  RenderPass *rpass_z_src = RE_pass_find_by_name(render_layer, RE_PASSNAME_Z, viewname);
  RenderPass *rpass_col_src = RE_pass_find_by_name(render_layer, RE_PASSNAME_COMBINED, viewname);

  float *pix_z = (rpass_z_src) ? rpass_z_src->ibuf->float_buffer.data : nullptr;
  float *pix_col = (rpass_col_src) ? rpass_col_src->ibuf->float_buffer.data : nullptr;

  if (!pix_z || !pix_col) {
    RE_engine_set_error_message(engine,
                                "Warning: To render grease pencil, enable Combined and Z passes.");
  }

  if (pix_z) {
    /* Depth need to be remapped to [0..1] range. */
    pix_z = static_cast<float *>(MEM_dupallocN(pix_z));

    int pix_num = rpass_z_src->rectx * rpass_z_src->recty;

    if (view.is_persp()) {
      for (int i = 0; i < pix_num; i++) {
        pix_z[i] = (-view.winmat()[3][2] / -pix_z[i]) - view.winmat()[2][2];
        pix_z[i] = clamp_f(pix_z[i] * 0.5f + 0.5f, 0.0f, 1.0f);
      }
    }
    else {
      /* Keep in mind, near and far distance are negatives. */
      float near = view.near_clip();
      float far = view.far_clip();
      float range_inv = 1.0f / fabsf(far - near);
      for (int i = 0; i < pix_num; i++) {
        pix_z[i] = (pix_z[i] + near) * range_inv;
        pix_z[i] = clamp_f(pix_z[i], 0.0f, 1.0f);
      }
    }
  }

  /* FIXME(fclem): we have a precision loss in the depth buffer because of this re-upload.
   * Find where it comes from! */
  const eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  r_depth_tx.ensure_2d(GPU_DEPTH_COMPONENT24, render_resolution, usage, pix_z);
  r_color_tx.ensure_2d(GPU_RGBA16F, render_resolution, usage, pix_col);
}

static void gpencil_render_to_image(void * /*vedata*/,
                                    RenderEngine *engine,
                                    RenderLayer *render_layer,
                                    const rcti * /*rect*/)
{
  draw::greasepencil::Instance instance;
  draw::Manager &manager = *DRW_manager_get();

  Render *render = engine->re;
  Depsgraph *depsgraph = DRW_context_state_get()->depsgraph;
  Object *camera_original_ob = RE_GetCamera(render);
  const char *viewname = RE_GetActiveRenderView(render);
  const int2 render_resolution = int2(engine->resolution_x, engine->resolution_y);

  instance.init(depsgraph, &manager, nullptr, nullptr, nullptr);

  float4x4 viewinv, winmat;
  Object *camera_eval = DEG_get_evaluated_object(depsgraph, camera_original_ob);
  RE_GetCameraModelMatrix(render, camera_eval, viewinv.ptr());
  float4x4 viewmat = math::invert(viewinv);
  RE_GetCameraWindow(render, camera_eval, winmat.ptr());

  instance.view().sync(viewmat, winmat);
  instance.render_sync(engine, depsgraph);

  draw::Texture color_tx;
  draw::Texture depth_tx;
  /* TODO: Support `R_BORDER` render mode. */
  get_render_result_textures(
      engine, render_layer, instance.view(), render_resolution, color_tx, depth_tx);

  instance.draw(color_tx, depth_tx, render_resolution);

  RenderPass *rp = RE_pass_find_by_name(render_layer, RE_PASSNAME_COMBINED, viewname);
  if (!rp) {
    return;
  }
  float *result = reinterpret_cast<float *>(color_tx.read<float4>(GPU_DATA_FLOAT));

  if (result) {
    BLI_mutex_lock(&engine->update_render_passes_mutex);
    /* WORKAROUND: We use texture read to avoid using a frame-buffer to get the render result.
     * However, on some implementation, we need a buffer with a few extra bytes for the read to
     * happen correctly (see #GLTexture::read()). So we need a custom memory allocation. */
    /* Avoid `memcpy()`, replace the pointer directly. */
    RE_pass_set_buffer_data(rp, result);
    BLI_mutex_unlock(&engine->update_render_passes_mutex);
  }
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
