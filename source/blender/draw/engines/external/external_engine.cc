/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Base engine for external render engines.
 * We use it for depth and non-mesh objects.
 */

#include "BKE_paint.hh"
#include "DRW_engine.hh"
#include "DRW_render.hh"

#include "BLI_string.h"

#include "BLT_translation.hh"

#include "DNA_particle_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "ED_image.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "GPU_debug.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "draw_cache.hh"
#include "draw_cache_impl.hh"
#include "draw_command.hh"
#include "draw_common.hh"
#include "draw_pass.hh"
#include "draw_sculpt.hh"
#include "draw_view.hh"
#include "draw_view_data.hh"

#include "external_engine.h" /* own include */

/* Shaders */

namespace blender::draw::external {

/**
 * A depth pass that write surface depth when it is needed.
 * Used only when grease pencil needs correct depth in the viewport.
 * Should ultimately be replaced by render engine depth output.
 */
class Prepass {
 private:
  PassMain ps_ = {"prepass"};
  PassMain::Sub *mesh_ps_ = nullptr;
  PassMain::Sub *curves_ps_ = nullptr;
  PassMain::Sub *pointcloud_ps_ = nullptr;

  /* Reuse overlay shaders. */
  gpu::StaticShader depth_mesh = {"overlay_depth_mesh"};
  gpu::StaticShader depth_curves = {"overlay_depth_curves"};
  gpu::StaticShader depth_pointcloud = {"overlay_depth_pointcloud"};

  draw::UniformBuffer<float4> dummy_buf;

 public:
  void begin_sync()
  {
    dummy_buf.push_update();

    ps_.init();
    ps_.state_set(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL);
    /* Dummy binds. They are unused in the variant we use.
     * Just avoid validation layers complaining. */
    ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &dummy_buf);
    ps_.bind_ubo(DRW_CLIPPING_UBO_SLOT, &dummy_buf);
    {
      auto &sub = ps_.sub("Mesh");
      sub.shader_set(depth_mesh.get());
      mesh_ps_ = &sub;
    }
    {
      auto &sub = ps_.sub("Curves");
      sub.shader_set(depth_curves.get());
      curves_ps_ = &sub;
    }
    {
      auto &sub = ps_.sub("PointCloud");
      sub.shader_set(depth_pointcloud.get());
      pointcloud_ps_ = &sub;
    }
  }

  void particle_sync(Manager &manager, const ObjectRef &ob_ref)
  {
    Object *ob = ob_ref.object;

    ResourceHandleRange handle = {};

    LISTBASE_FOREACH (ParticleSystem *, psys, &ob->particlesystem) {
      if (!DRW_object_is_visible_psys_in_active_context(ob, psys)) {
        continue;
      }

      const ParticleSettings *part = psys->part;
      const int draw_as = (part->draw_as == PART_DRAW_REND) ? part->ren_as : part->draw_as;
      if (draw_as == PART_DRAW_PATH && part->draw_as == PART_DRAW_REND) {
        /* Case where the render engine should have rendered it, but we need to draw it for
         * selection purpose. */
        if (!handle.is_valid()) {
          handle = manager.resource_handle_for_psys(ob_ref, ob_ref.particles_matrix());
        }

        gpu::Batch *geom = DRW_cache_particles_get_hair(ob, psys, nullptr);
        mesh_ps_->draw(geom, handle);
        break;
      }
    }
  }

  void sculpt_sync(Manager &manager, const ObjectRef &ob_ref)
  {
    ResourceHandleRange handle = manager.unique_handle_for_sculpt(ob_ref);

    for (SculptBatch &batch : sculpt_batches_get(ob_ref.object, SCULPT_BATCH_DEFAULT)) {
      mesh_ps_->draw(batch.batch, handle);
    }
  }

  void object_sync(Manager &manager, const ObjectRef &ob_ref, const DRWContext &draw_ctx)
  {
    bool is_solid = ob_ref.object->dt >= OB_SOLID ||
                    !(ob_ref.object->visibility_flag & OB_HIDE_CAMERA);

    if (!is_solid) {
      return;
    }

    particle_sync(manager, ob_ref);

    const bool use_sculpt_pbvh = BKE_sculptsession_use_pbvh_draw(ob_ref.object, draw_ctx.rv3d);

    if (use_sculpt_pbvh) {
      sculpt_sync(manager, ob_ref);
      return;
    }

    gpu::Batch *geom_single = nullptr;
    Span<gpu::Batch *> geom_list(&geom_single, 1);

    PassMain::Sub *pass = nullptr;
    switch (ob_ref.object->type) {
      case OB_MESH:
        geom_single = DRW_cache_mesh_surface_get(ob_ref.object);
        pass = mesh_ps_;
        break;
      case OB_POINTCLOUD:
        geom_single = pointcloud_sub_pass_setup(*pointcloud_ps_, ob_ref.object);
        pass = pointcloud_ps_;
        break;
      case OB_CURVES: {
        const char *error = nullptr;
        /* We choose to ignore the error here as the external engine can display them properly.
         * The overlays can still be broken but it should be detected in solid mode. */
        geom_single = curves_sub_pass_setup(*curves_ps_, draw_ctx.scene, ob_ref.object, error);
        pass = curves_ps_;
        break;
      }
      default:
        break;
    }

    if (pass == nullptr) {
      return;
    }

    ResourceHandleRange res_handle = manager.unique_handle(ob_ref);

    for (int material_id : geom_list.index_range()) {
      pass->draw(geom_list[material_id], res_handle);
    }
  }

  void submit(Manager &manager, View &view)
  {
    manager.submit(ps_, view);
  }
};

class Instance : public DrawEngine {
  const DRWContext *draw_ctx = nullptr;

  Prepass prepass;
  /* Only do prepass if there is a need for it.
   * This is only needed for GPencil integration. */
  bool do_prepass = false;

  blender::StringRefNull name_get() final
  {
    return "External";
  }

  void init() final
  {
    draw_ctx = DRW_context_get();
    do_prepass = DRW_gpencil_engine_needed_viewport(draw_ctx->depsgraph, draw_ctx->v3d);
  }

  void begin_sync() final
  {
    if (do_prepass) {
      prepass.begin_sync();
    }
  }

  void object_sync(blender::draw::ObjectRef &ob_ref, blender::draw::Manager &manager) final
  {
    if (do_prepass) {
      prepass.object_sync(manager, ob_ref, *draw_ctx);
    }
  }

  void end_sync() final {}

  void draw_scene_do_v3d(blender::draw::Manager &manager, draw::View &view)
  {
    RegionView3D *rv3d = draw_ctx->rv3d;
    ARegion *region = draw_ctx->region;

    blender::draw::command::StateSet::set(DRW_STATE_WRITE_COLOR);

    /* The external engine can use the OpenGL rendering API directly, so make sure the state is
     * already applied. */
    GPU_apply_state();

    /* Create render engine. */
    RenderEngine *render_engine = nullptr;
    if (!rv3d->view_render) {
      RenderEngineType *engine_type = ED_view3d_engine_type(draw_ctx->scene,
                                                            draw_ctx->v3d->shading.type);

      if (!(engine_type->view_update && engine_type->view_draw)) {
        return;
      }

      rv3d->view_render = RE_NewViewRender(engine_type);
      render_engine = RE_view_engine_get(rv3d->view_render);
      engine_type->view_update(render_engine, draw_ctx->evil_C, draw_ctx->depsgraph);
    }
    else {
      render_engine = RE_view_engine_get(rv3d->view_render);
    }

    /* Rendered draw. */
    GPU_matrix_push_projection();
    GPU_matrix_push();
    ED_region_pixelspace(region);

    /* Render result draw. */
    const RenderEngineType *type = render_engine->type;
    type->view_draw(render_engine, draw_ctx->evil_C, draw_ctx->depsgraph);

    GPU_matrix_pop();
    GPU_matrix_pop_projection();

    if (do_prepass) {
      prepass.submit(manager, view);
    }

    /* Set render info. */
    if (render_engine->text[0] != '\0') {
      STRNCPY(info, render_engine->text);
    }
    else {
      info[0] = '\0';
    }
  }

  /* Configure current matrix stack so that the external engine can use the same drawing code for
   * both viewport and image editor drawing.
   *
   * The engine draws result in the pixel space, and is applying render offset. For image editor we
   * need to switch from normalized space to pixel space, and "un-apply" offset. */
  void external_image_space_matrix_set(const RenderEngine *engine)
  {
    BLI_assert(engine != nullptr);

    SpaceImage *space_image = (SpaceImage *)draw_ctx->space_data;

    /* Apply current view as transformation matrix.
     * This will configure drawing for normalized space with current zoom and pan applied. */

    float4x4 view_matrix = blender::draw::View::default_get().viewmat();
    float4x4 projection_matrix = blender::draw::View::default_get().winmat();

    GPU_matrix_projection_set(projection_matrix.ptr());
    GPU_matrix_set(view_matrix.ptr());

    /* Switch from normalized space to pixel space. */
    {
      int width, height;
      ED_space_image_get_size(space_image, &width, &height);

      const float width_inv = width ? 1.0f / width : 0.0f;
      const float height_inv = height ? 1.0f / height : 0.0f;
      GPU_matrix_scale_2f(width_inv, height_inv);
    }

    /* Un-apply render offset. */
    {
      Render *render = engine->re;
      rctf view_rect;
      rcti render_rect;
      RE_GetViewPlane(render, &view_rect, &render_rect);

      GPU_matrix_translate_2f(-render_rect.xmin, -render_rect.ymin);
    }
  }

  void draw_scene_do_image()
  {
    Scene *scene = draw_ctx->scene;
    Render *re = RE_GetSceneRender(scene);
    RenderEngine *engine = RE_engine_get(re);

    /* Is tested before enabling the drawing engine. */
    BLI_assert(re != nullptr);
    BLI_assert(engine != nullptr);

    blender::draw::command::StateSet::set(DRW_STATE_WRITE_COLOR);

    /* The external engine can use the OpenGL rendering API directly, so make sure the state is
     * already applied. */
    GPU_apply_state();

    const DefaultFramebufferList *dfbl = draw_ctx->viewport_framebuffer_list_get();

    /* Clear the depth buffer to the value used by the background overlay so that the overlay is
     * not happening outside of the drawn image.
     *
     * NOTE: The external engine only draws color. The depth is taken care of using the depth pass
     * which initialized the depth to the values expected by the background overlay. */
    GPU_framebuffer_clear_depth(dfbl->default_fb, 1.0f);

    GPU_matrix_push_projection();
    GPU_matrix_push();

    external_image_space_matrix_set(engine);

    GPU_debug_group_begin("External Engine");

    const RenderEngineType *engine_type = engine->type;
    BLI_assert(engine_type != nullptr);
    BLI_assert(engine_type->draw != nullptr);

    engine_type->draw(engine, draw_ctx->evil_C, draw_ctx->depsgraph);

    GPU_debug_group_end();

    GPU_matrix_pop();
    GPU_matrix_pop_projection();

    blender::draw::command::StateSet::set();

    RE_engine_draw_release(re);
  }

  void draw_scene_do(blender::draw::Manager &manager, View &view)
  {
    if (draw_ctx->v3d != nullptr) {
      draw_scene_do_v3d(manager, view);
      return;
    }

    if (draw_ctx->space_data == nullptr) {
      return;
    }

    const eSpace_Type space_type = eSpace_Type(draw_ctx->space_data->spacetype);
    if (space_type == SPACE_IMAGE) {
      draw_scene_do_image();
      return;
    }
  }

  void draw(blender::draw::Manager &manager) final
  {
    /* TODO(fclem): Remove global access. */
    View &view = View::default_get();

    const DefaultFramebufferList *dfbl = draw_ctx->viewport_framebuffer_list_get();

    /* Will be nullptr during OpenGL render.
     * OpenGL render is used for quick preview (thumbnails or sequencer preview)
     * where using the rendering engine to preview doesn't make so much sense. */
    if (draw_ctx->evil_C) {
      const float clear_col[4] = {0, 0, 0, 0};
      /* This is to keep compatibility with external engine. */
      /* TODO(fclem): remove it eventually. */
      GPU_framebuffer_bind(dfbl->default_fb);
      GPU_framebuffer_clear_color(dfbl->default_fb, clear_col);

      DRW_submission_start();
      draw_scene_do(manager, view);
      DRW_submission_end();
    }
  }
};

DrawEngine *Engine::create_instance()
{
  return new Instance();
}

}  // namespace blender::draw::external

/* Functions */

/* NOTE: currently unused,
 * we should not register unless we want to see this when debugging the view. */

RenderEngineType DRW_engine_viewport_external_type = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ "BLENDER_EXTERNAL",
    /*name*/ N_("External"),
    /*flag*/ RE_INTERNAL | RE_USE_STEREO_VIEWPORT,
    /*update*/ nullptr,
    /*render*/ nullptr,
    /*render_frame_finish*/ nullptr,
    /*draw*/ nullptr,
    /*bake*/ nullptr,
    /*view_update*/ nullptr,
    /*view_draw*/ nullptr,
    /*update_script_node*/ nullptr,
    /*update_render_passes*/ nullptr,
    /*update_custom_camera*/ nullptr,
    /*draw_engine*/ nullptr,
    /*rna_ext*/
    {
        /*data*/ nullptr,
        /*srna*/ nullptr,
        /*call*/ nullptr,
    },
};

bool DRW_engine_external_acquire_for_image_editor(const DRWContext *draw_ctx)
{
  const SpaceLink *space_data = draw_ctx->space_data;
  Scene *scene = draw_ctx->scene;

  if (space_data == nullptr) {
    return false;
  }

  const eSpace_Type space_type = eSpace_Type(draw_ctx->space_data->spacetype);
  if (space_type != SPACE_IMAGE) {
    return false;
  }

  SpaceImage *space_image = (SpaceImage *)space_data;
  const Image *image = ED_space_image(space_image);
  if (image == nullptr || image->type != IMA_TYPE_R_RESULT) {
    return false;
  }

  if (image->render_slot != image->last_render_slot) {
    return false;
  }

  /* Render is allocated on main thread, so it is safe to access it from here. */
  Render *re = RE_GetSceneRender(scene);

  if (re == nullptr) {
    return false;
  }

  return RE_engine_draw_acquire(re);
}

void DRW_engine_external_free(RegionView3D *rv3d)
{
  if (rv3d->view_render) {
    /* Free engine with DRW context enabled, as this may clean up per-context
     * resources like VAOs. */
    bool swap_context = !DRW_gpu_context_is_enabled();
    if (swap_context) {
      DRW_gpu_context_enable_ex(true);
    }
    RE_FreeViewRender(rv3d->view_render);
    rv3d->view_render = nullptr;
    if (swap_context) {
      DRW_gpu_context_disable_ex(true);
    }
  }
}
