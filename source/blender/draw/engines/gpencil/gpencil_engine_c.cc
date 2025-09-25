/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */
#include "DRW_engine.hh"
#include "DRW_render.hh"

#include "BKE_compositor.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_gpencil_legacy.h"
#include "BKE_grease_pencil.hh"
#include "BKE_material.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_shader_fx.h"

#include "BKE_camera.h"

#include "BLI_listbase.h"
#include "BLI_memblock.h"
#include "BLI_virtual_array.hh"

#include "BLT_translation.hh"

#include "DNA_camera_types.h"
#include "DNA_material_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "GPU_texture.hh"
#include "GPU_uniform_buffer.hh"

#include "draw_cache.hh"
#include "draw_manager.hh"
#include "draw_view.hh"

#include "gpencil_engine.hh"
#include "gpencil_engine_private.hh"

#include "DEG_depsgraph_query.hh"

#include "ED_grease_pencil.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "GPU_debug.hh"

namespace blender::draw::gpencil {

void Instance::init()
{
  this->draw_ctx = DRW_context_get();

  const View3D *v3d = draw_ctx->v3d;

  if (!dummy_texture.is_valid()) {
    const float pixels[1][4] = {{1.0f, 0.0f, 1.0f, 1.0f}};
    dummy_texture.ensure_2d(
        gpu::TextureFormat::UNORM_8_8_8_8, int2(1), GPU_TEXTURE_USAGE_SHADER_READ, &pixels[0][0]);
  }
  if (!dummy_depth.is_valid()) {
    const float pixels[1] = {1.0f};
    dummy_depth.ensure_2d(
        gpu::TextureFormat::SFLOAT_32_DEPTH, int2(1), GPU_TEXTURE_USAGE_SHADER_READ, &pixels[0]);
  }

  /* Resize and reset memory-blocks. */
  BLI_memblock_clear(this->gp_light_pool, light_pool_free);
  BLI_memblock_clear(this->gp_material_pool, material_pool_free);
  BLI_memblock_clear(this->gp_object_pool, nullptr);
  this->gp_layer_pool->clear();
  this->gp_vfx_pool->clear();
  BLI_memblock_clear(this->gp_maskbit_pool, nullptr);

  this->view_layer = draw_ctx->view_layer;
  this->scene = draw_ctx->scene;
  this->v3d = draw_ctx->v3d;
  this->last_light_pool = nullptr;
  this->last_material_pool = nullptr;
  this->tobjects.first = nullptr;
  this->tobjects.last = nullptr;
  this->tobjects_infront.first = nullptr;
  this->tobjects_infront.last = nullptr;
  this->sbuffer_tobjects.first = nullptr;
  this->sbuffer_tobjects.last = nullptr;
  this->dummy_tx = this->dummy_texture;
  this->draw_wireframe = (v3d && v3d->shading.type == OB_WIRE);
  this->scene_depth_tx = nullptr;
  this->scene_fb = nullptr;
  this->is_render = this->render_depth_tx.is_valid() || (v3d && v3d->shading.type == OB_RENDER);
  this->is_viewport = (v3d != nullptr);
  this->global_light_pool = gpencil_light_pool_add(this);
  this->shadeless_light_pool = gpencil_light_pool_add(this);
  /* Small HACK: we don't want the global pool to be reused,
   * so we set the last light pool to nullptr. */
  this->last_light_pool = nullptr;
  this->is_sorted = false;

  bool use_scene_lights = false;
  bool use_scene_world = false;

  if (v3d) {
    use_scene_lights = V3D_USES_SCENE_LIGHTS(v3d);

    use_scene_world = V3D_USES_SCENE_WORLD(v3d);

    this->v3d_color_type = (v3d->shading.type == OB_SOLID) ? v3d->shading.color_type : -1;
    /* Special case: If we're in Vertex Paint mode, enforce #V3D_SHADING_VERTEX_COLOR setting. */
    if (v3d->shading.type == OB_SOLID && draw_ctx->obact &&
        (draw_ctx->obact->mode & OB_MODE_VERTEX_GREASE_PENCIL) != 0)
    {
      this->v3d_color_type = V3D_SHADING_VERTEX_COLOR;
    }

    copy_v3_v3(this->v3d_single_color, v3d->shading.single_color);

    /* For non active frame, use only lines in multiedit mode. */
    const bool overlays_on = (v3d->flag2 & V3D_HIDE_OVERLAYS) == 0;
    this->use_multiedit_lines_only = overlays_on &&
                                     (v3d->gp_flag & V3D_GP_SHOW_MULTIEDIT_LINES) != 0;

    const bool shmode_xray_support = v3d->shading.type <= OB_SOLID;
    this->xray_alpha = (shmode_xray_support && XRAY_ENABLED(v3d)) ? XRAY_ALPHA(v3d) : 1.0f;
    this->force_stroke_order_3d = v3d->gp_flag & V3D_GP_FORCE_STROKE_ORDER_3D;
  }
  else if (this->is_render) {
    use_scene_lights = true;
    use_scene_world = true;
    this->use_multiedit_lines_only = false;
    this->xray_alpha = 1.0f;
    this->v3d_color_type = -1;
    this->force_stroke_order_3d = false;
  }

  this->use_lighting = (v3d && v3d->shading.type > OB_SOLID) || this->is_render;
  this->use_lights = use_scene_lights;

  gpencil_light_ambient_add(this->shadeless_light_pool, float3{1.0f, 1.0f, 1.0f});

  World *world = draw_ctx->scene->world;
  if (world != nullptr && use_scene_world) {
    gpencil_light_ambient_add(this->global_light_pool, &world->horr);
  }
  else if (v3d) {
    float world_light[3];
    copy_v3_fl(world_light, v3d->shading.studiolight_intensity);
    gpencil_light_ambient_add(this->global_light_pool, world_light);
  }

  float4x4 viewmatinv = View::default_get().viewinv();
  copy_v3_v3(this->camera_z_axis, viewmatinv[2]);
  copy_v3_v3(this->camera_pos, viewmatinv[3]);
  this->camera_z_offset = dot_v3v3(viewmatinv[3], viewmatinv[2]);

  if (draw_ctx && draw_ctx->rv3d && v3d) {
    this->camera = (draw_ctx->rv3d->persp == RV3D_CAMOB) ? v3d->camera : nullptr;
  }
  else {
    this->camera = nullptr;
  }
}

void Instance::begin_sync()
{
  this->cfra = int(DEG_get_ctime(draw_ctx->depsgraph));
  this->simplify_antialias = GPENCIL_SIMPLIFY_AA(draw_ctx->scene);
  this->use_layer_fb = false;
  this->use_object_fb = false;
  this->use_mask_fb = false;

  const bool use_viewport_compositor = draw_ctx->is_viewport_compositor_enabled();
  const bool has_grease_pencil_pass =
      bke::compositor::get_used_passes(*scene, view_layer).contains("GreasePencil");
  this->use_separate_pass = use_viewport_compositor ? has_grease_pencil_pass : false;
  this->use_signed_fb = !this->is_viewport;

  if (draw_ctx->v3d) {
    const bool hide_overlay = ((draw_ctx->v3d->flag2 & V3D_HIDE_OVERLAYS) != 0);
    const bool show_onion = ((draw_ctx->v3d->gp_flag & V3D_GP_SHOW_ONION_SKIN) != 0);
    const bool playing = (draw_ctx->evil_C != nullptr) ?
                             ED_screen_animation_playing(CTX_wm_manager(draw_ctx->evil_C)) !=
                                 nullptr :
                             false;
    this->do_onion = show_onion && !hide_overlay && !playing;
    this->do_onion_only_active_object = ((draw_ctx->v3d->gp_flag &
                                          V3D_GP_ONION_SKIN_ACTIVE_OBJECT) != 0);
    this->playing = playing;
    /* Save simplify flags (can change while drawing, so it's better to save). */
    Scene *scene = draw_ctx->scene;
    this->simplify_fill = GPENCIL_SIMPLIFY_FILL(scene, playing);
    this->simplify_fx = GPENCIL_SIMPLIFY_FX(scene, playing) ||
                        (draw_ctx->v3d->shading.type < OB_RENDER);

    /* Fade Layer. */
    const bool is_fade_layer = ((!hide_overlay) && (!this->is_render) &&
                                (draw_ctx->v3d->gp_flag & V3D_GP_FADE_NOACTIVE_LAYERS));
    this->fade_layer_opacity = (is_fade_layer) ? draw_ctx->v3d->overlay.gpencil_fade_layer : -1.0f;
    this->vertex_paint_opacity = draw_ctx->v3d->overlay.gpencil_vertex_paint_opacity;
    /* Fade GPencil Objects. */
    const bool is_fade_object = ((!hide_overlay) && (!this->is_render) &&
                                 (draw_ctx->v3d->gp_flag & V3D_GP_FADE_OBJECTS) &&
                                 (draw_ctx->v3d->gp_flag & V3D_GP_FADE_NOACTIVE_GPENCIL));
    this->fade_gp_object_opacity = (is_fade_object) ?
                                       draw_ctx->v3d->overlay.gpencil_paper_opacity :
                                       -1.0f;
    this->fade_3d_object_opacity = ((!hide_overlay) && (!this->is_render) &&
                                    (draw_ctx->v3d->gp_flag & V3D_GP_FADE_OBJECTS)) ?
                                       draw_ctx->v3d->overlay.gpencil_paper_opacity :
                                       -1.0f;
  }
  else {
    this->do_onion = true;
    Scene *scene = draw_ctx->scene;
    this->simplify_fill = GPENCIL_SIMPLIFY_FILL(scene, false);
    this->simplify_fx = GPENCIL_SIMPLIFY_FX(scene, false);
    this->fade_layer_opacity = -1.0f;
    this->playing = false;
  }

  {
    this->stroke_batch = nullptr;
    this->fill_batch = nullptr;

    this->obact = draw_ctx->obact;
  }

  /* Free unneeded buffers. */
  this->snapshot_depth_tx.free();
  this->snapshot_color_tx.free();
  this->snapshot_reveal_tx.free();

  {
    PassSimple &pass = this->merge_depth_ps;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
    pass.shader_set(ShaderCache::get().depth_merge.get());
    pass.bind_texture("depth_buf", &this->depth_tx);
    pass.push_constant("stroke_order3d", &this->is_stroke_order_3d);
    pass.push_constant("gp_model_matrix", &this->object_bound_mat);
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
  {
    PassSimple &pass = this->mask_invert_ps;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_LOGIC_INVERT);
    pass.shader_set(ShaderCache::get().mask_invert.get());
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }

  Camera *cam = static_cast<Camera *>(
      (this->camera != nullptr && this->camera->type == OB_CAMERA) ? this->camera->data : nullptr);

  /* Pseudo DOF setup. */
  if (cam && (cam->dof.flag & CAM_DOF_ENABLED)) {
    const float2 vp_size = draw_ctx->viewport_size_get();
    float fstop = cam->dof.aperture_fstop;
    float sensor = BKE_camera_sensor_size(cam->sensor_fit, cam->sensor_x, cam->sensor_y);
    float focus_dist = BKE_camera_object_dof_distance(this->camera);
    float focal_len = cam->lens;

    const float scale_camera = 0.001f;
    /* We want radius here for the aperture number. */
    float aperture = 0.5f * scale_camera * focal_len / fstop;
    float focal_len_scaled = scale_camera * focal_len;
    float sensor_scaled = scale_camera * sensor;

    if (draw_ctx->rv3d != nullptr) {
      sensor_scaled *= draw_ctx->rv3d->viewcamtexcofac[0];
    }

    this->dof_params[1] = aperture * fabsf(focal_len_scaled / (focus_dist - focal_len_scaled));
    this->dof_params[1] *= vp_size[0] / sensor_scaled;
    this->dof_params[0] = -focus_dist * this->dof_params[1];
  }
  else {
    /* Disable DoF blur scaling. */
    this->camera = nullptr;
  }
}

#define DISABLE_BATCHING 0

bool Instance::is_used_as_layer_mask_in_viewlayer(const GreasePencil &grease_pencil,
                                                  const bke::greasepencil::Layer &mask_layer,
                                                  const ViewLayer &view_layer)
{
  using namespace bke::greasepencil;
  for (const Layer *layer : grease_pencil.layers()) {
    if (layer->view_layer_name().is_empty() ||
        !STREQ(view_layer.name, layer->view_layer_name().c_str()))
    {
      continue;
    }

    if ((layer->base.flag & GP_LAYER_TREE_NODE_DISABLE_MASKS_IN_VIEWLAYER) != 0) {
      continue;
    }

    LISTBASE_FOREACH (GreasePencilLayerMask *, mask, &layer->masks) {
      if (STREQ(mask->layer_name, mask_layer.name().c_str())) {
        return true;
      }
    }
  }
  return false;
}

bool Instance::use_layer_in_render(const GreasePencil &grease_pencil,
                                   const bke::greasepencil::Layer &layer,
                                   const ViewLayer &view_layer,
                                   bool &r_is_used_as_mask)
{
  if (!layer.view_layer_name().is_empty() &&
      !STREQ(view_layer.name, layer.view_layer_name().c_str()))
  {
    /* Do not skip layers that are masks when rendering the viewlayer so that it can still be used
     * to clip/mask other layers. */
    if (is_used_as_layer_mask_in_viewlayer(grease_pencil, layer, view_layer)) {
      r_is_used_as_mask = true;
    }
    else {
      return false;
    }
  }
  return true;
}

tObject *Instance::object_sync_do(Object *ob, ResourceHandleRange res_handle)
{
  using namespace ed::greasepencil;
  using namespace bke::greasepencil;
  GreasePencil &grease_pencil = DRW_object_get_data_for_drawing<GreasePencil>(*ob);
  const bool is_vertex_mode = (ob->mode & OB_MODE_VERTEX_PAINT) != 0;
  const Bounds<float3> bounds = grease_pencil.bounds_min_max_eval().value_or(Bounds(float3(0)));

  const bool do_onion = !this->is_render && this->do_onion &&
                        (this->do_onion_only_active_object ? this->obact == ob : true);
  const bool do_multi_frame = (((this->scene->toolsettings->gpencil_flags &
                                 GP_USE_MULTI_FRAME_EDITING) != 0) &&
                               (ob->mode != OB_MODE_OBJECT));
  const bool use_stroke_order_3d = this->force_stroke_order_3d ||
                                   ((grease_pencil.flag & GREASE_PENCIL_STROKE_ORDER_3D) != 0);
  tObject *tgp_ob = gpencil_object_cache_add(this, ob, use_stroke_order_3d, bounds);

  int mat_ofs = 0;
  MaterialPool *matpool = gpencil_material_pool_create(this, ob, &mat_ofs, is_vertex_mode);

  gpu::Texture *tex_fill = this->dummy_tx;
  gpu::Texture *tex_stroke = this->dummy_tx;

  gpu::Batch *iter_geom = nullptr;
  PassSimple *last_pass = nullptr;
  int vfirst = 0;
  int vcount = 0;

  const auto drawcall_flush = [&](PassSimple &pass) {
#if !DISABLE_BATCHING
    if (iter_geom != nullptr) {
      pass.draw(iter_geom, 1, vcount, vfirst, res_handle);
    }
#endif
    iter_geom = nullptr;
    vfirst = -1;
    vcount = 0;
  };

  const auto drawcall_add =
      [&](PassSimple &pass, gpu::Batch *draw_geom, const int v_first, const int v_count) {
#if DISABLE_BATCHING
        pass.draw(iter_geom, 1, vcount, vfirst, res_handle);
        return;
#endif
        int last = vfirst + vcount;
        /* Interrupt draw-call grouping if the sequence is not consecutive. */
        if ((draw_geom != iter_geom) || (v_first - last > 0)) {
          drawcall_flush(pass);
        }
        iter_geom = draw_geom;
        if (vfirst == -1) {
          vfirst = v_first;
        }
        vcount = v_first + v_count - vfirst;
      };

  int t_offset = 0;
  /* Note that we loop over all the drawings (including the onion skinned ones) to make sure we
   * match the offsets of the batch cache. */
  const Vector<DrawingInfo> drawings = retrieve_visible_drawings(
      *this->scene, grease_pencil, true);
  const Span<const Layer *> layers = grease_pencil.layers();
  for (const DrawingInfo info : drawings) {
    const Layer &layer = *layers[info.layer_index];

    const bke::CurvesGeometry &curves = info.drawing.strokes();
    const OffsetIndices<int> points_by_curve = curves.evaluated_points_by_curve();
    const bke::AttributeAccessor attributes = curves.attributes();
    const VArray<bool> cyclic = *attributes.lookup_or_default<bool>(
        "cyclic", bke::AttrDomain::Curve, false);

    IndexMaskMemory memory;
    const IndexMask visible_strokes = ed::greasepencil::retrieve_visible_strokes(
        *ob, info.drawing, memory);

    /* Precompute all the triangle and vertex counts.
     * In case the drawing should not be rendered, we need to compute the offset where the next
     * drawing begins. */
    Array<int> num_triangles_per_stroke(visible_strokes.size());
    Array<int> num_vertices_per_stroke(visible_strokes.size());
    int total_num_triangles = 0;
    int total_num_vertices = 0;
    visible_strokes.foreach_index([&](const int stroke_i, const int pos) {
      const IndexRange points = points_by_curve[stroke_i];
      const int num_stroke_triangles = (points.size() >= 3) ? (points.size() - 2) : 0;
      const int num_stroke_vertices = (points.size() +
                                       int(cyclic[stroke_i] && (points.size() >= 3)));
      num_triangles_per_stroke[pos] = num_stroke_triangles;
      num_vertices_per_stroke[pos] = num_stroke_vertices;
      total_num_triangles += num_stroke_triangles;
      total_num_vertices += num_stroke_vertices;
    });

    bool is_layer_used_as_mask = false;
    const bool show_drawing_in_render = use_layer_in_render(
        grease_pencil, layer, *this->view_layer, is_layer_used_as_mask);
    if (!show_drawing_in_render) {
      /* Skip over the entire drawing. */
      t_offset += total_num_triangles;
      t_offset += total_num_vertices * 2;
      continue;
    }

    if (last_pass) {
      drawcall_flush(*last_pass);
    }

    tLayer *tgp_layer = grease_pencil_layer_cache_add(
        this, ob, layer, info.onion_id, is_layer_used_as_mask, tgp_ob);
    PassSimple &pass = *tgp_layer->geom_ps;
    last_pass = &pass;

    const bool use_lights = this->use_lighting &&
                            ((layer.base.flag & GP_LAYER_TREE_NODE_USE_LIGHTS) != 0) &&
                            (ob->dtx & OB_USE_GPENCIL_LIGHTS);

    gpu::UniformBuf *lights_ubo = (use_lights) ? this->global_light_pool->ubo :
                                                 this->shadeless_light_pool->ubo;

    gpu::UniformBuf *ubo_mat;
    gpencil_material_resources_get(matpool, 0, nullptr, nullptr, &ubo_mat);

    pass.bind_ubo("gp_lights", lights_ubo);
    pass.bind_ubo("gp_materials", ubo_mat);
    pass.bind_texture("gp_fill_tx", tex_fill);
    pass.bind_texture("gp_stroke_tx", tex_stroke);
    pass.push_constant("gp_material_offset", mat_ofs);
    /* Since we don't use the sbuffer in GPv3, this is always 0. */
    pass.push_constant("gp_stroke_index_offset", 0.0f);
    pass.push_constant("viewport_size", float2(draw_ctx->viewport_size_get()));

    const VArray<int> stroke_materials = *attributes.lookup_or_default<int>(
        "material_index", bke::AttrDomain::Curve, 0);
    const VArray<bool> is_fill_guide = *attributes.lookup_or_default<bool>(
        ".is_fill_guide", bke::AttrDomain::Curve, false);

    const bool only_lines = !ELEM(ob->mode,
                                  OB_MODE_PAINT_GREASE_PENCIL,
                                  OB_MODE_WEIGHT_GREASE_PENCIL,
                                  OB_MODE_VERTEX_GREASE_PENCIL) &&
                            info.frame_number != this->cfra && this->use_multiedit_lines_only &&
                            do_multi_frame;
    const bool is_onion = info.onion_id != 0;

    visible_strokes.foreach_index([&](const int stroke_i, const int pos) {
      const IndexRange points = points_by_curve[stroke_i];
      /* The material index is allowed to be negative as it's stored as a generic attribute. We
       * clamp it here to avoid crashing in the rendering code. Any stroke with a material < 0 will
       * use the first material in the first material slot. */
      const int material_index = std::max(stroke_materials[stroke_i], 0);
      const MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, material_index + 1);

      const bool is_fill_guide_stroke = is_fill_guide[stroke_i];

      const bool hide_material = (gp_style->flag & GP_MATERIAL_HIDE) != 0;
      const bool show_stroke = ((gp_style->flag & GP_MATERIAL_STROKE_SHOW) != 0) ||
                               is_fill_guide_stroke;
      const bool show_fill = (points.size() >= 3) &&
                             ((gp_style->flag & GP_MATERIAL_FILL_SHOW) != 0) &&
                             (!this->simplify_fill) && !is_fill_guide_stroke;
      const bool hide_onion = is_onion && ((gp_style->flag & GP_MATERIAL_HIDE_ONIONSKIN) != 0 ||
                                           (!do_onion && !do_multi_frame));
      const bool skip_stroke = hide_material || (!show_stroke && !show_fill) ||
                               (only_lines && !do_onion && is_onion) || hide_onion;

      if (skip_stroke) {
        t_offset += num_triangles_per_stroke[pos];
        t_offset += num_vertices_per_stroke[pos] * 2;
        return;
      }

      gpu::UniformBuf *new_ubo_mat;
      gpu::Texture *new_tex_fill = nullptr;
      gpu::Texture *new_tex_stroke = nullptr;
      gpencil_material_resources_get(
          matpool, mat_ofs + material_index, &new_tex_stroke, &new_tex_fill, &new_ubo_mat);

      const bool resource_changed = (ubo_mat != new_ubo_mat) ||
                                    (new_tex_fill && (new_tex_fill != tex_fill)) ||
                                    (new_tex_stroke && (new_tex_stroke != tex_stroke));

      if (resource_changed) {
        drawcall_flush(pass);

        if (new_ubo_mat != ubo_mat) {
          pass.bind_ubo("gp_materials", new_ubo_mat);
          ubo_mat = new_ubo_mat;
        }
        if (new_tex_fill) {
          pass.bind_texture("gp_fill_tx", new_tex_fill);
          tex_fill = new_tex_fill;
        }
        if (new_tex_stroke) {
          pass.bind_texture("gp_stroke_tx", new_tex_stroke);
          tex_stroke = new_tex_stroke;
        }
      }

      gpu::Batch *geom = DRW_cache_grease_pencil_get(this->scene, ob);
      if (iter_geom != geom) {
        drawcall_flush(pass);

        gpu::VertBuf *position_tx = DRW_cache_grease_pencil_position_buffer_get(this->scene, ob);
        gpu::VertBuf *color_tx = DRW_cache_grease_pencil_color_buffer_get(this->scene, ob);
        pass.bind_texture("gp_pos_tx", position_tx);
        pass.bind_texture("gp_col_tx", color_tx);
      }

      if (show_fill) {
        const int v_first = t_offset * 3;
        const int v_count = num_triangles_per_stroke[pos] * 3;
        drawcall_add(pass, geom, v_first, v_count);
      }

      t_offset += num_triangles_per_stroke[pos];

      if (show_stroke) {
        const int v_first = t_offset * 3;
        const int v_count = num_vertices_per_stroke[pos] * 2 * 3;
        drawcall_add(pass, geom, v_first, v_count);
      }

      t_offset += num_vertices_per_stroke[pos] * 2;
    });
  }

  if (last_pass) {
    drawcall_flush(*last_pass);
  }

  return tgp_ob;
}

void Instance::object_sync(ObjectRef &ob_ref, Manager &manager)
{
  Object *ob = ob_ref.object;

  /* object must be visible */
  if (!(DRW_object_visibility_in_active_context(ob) & OB_VISIBLE_SELF)) {
    return;
  }

  if (ob->data && (ob->type == OB_GREASE_PENCIL) && (ob->dt >= OB_SOLID)) {
    ResourceHandleRange res_handle = manager.unique_handle(ob_ref);

    tObject *tgp_ob = object_sync_do(ob, res_handle);
    vfx_sync(ob, tgp_ob);
  }

  if (ob->type == OB_LAMP && this->use_lights) {
    gpencil_light_pool_populate(this->global_light_pool, ob);
  }
}

void Instance::end_sync()
{
  /* Upload UBO data. */
  BLI_memblock_iter iter;
  BLI_memblock_iternew(this->gp_material_pool, &iter);
  MaterialPool *pool;
  while ((pool = (MaterialPool *)BLI_memblock_iterstep(&iter))) {
    GPU_uniformbuf_update(pool->ubo, pool->mat_data);
  }

  BLI_memblock_iternew(this->gp_light_pool, &iter);
  LightPool *lpool;
  while ((lpool = (LightPool *)BLI_memblock_iterstep(&iter))) {
    GPU_uniformbuf_update(lpool->ubo, lpool->light_data);
  }
}

void Instance::acquire_resources()
{
  /* Create frame-buffers only if needed. */
  if (this->tobjects.first == nullptr) {
    return;
  }

  const int2 size = int2(draw_ctx->viewport_size_get());

  const gpu::TextureFormat format_color = gpu::TextureFormat::SFLOAT_16_16_16_16;
  const gpu::TextureFormat format_reveal = this->use_signed_fb ?
                                               gpu::TextureFormat::SFLOAT_16_16_16_16 :
                                               gpu::TextureFormat::UNORM_10_10_10_2;

  this->depth_tx.acquire(size, gpu::TextureFormat::SFLOAT_32_DEPTH_UINT_8);
  this->color_tx.acquire(size, format_color);
  this->reveal_tx.acquire(size, format_reveal);

  this->gpencil_fb.ensure(GPU_ATTACHMENT_TEXTURE(this->depth_tx),
                          GPU_ATTACHMENT_TEXTURE(this->color_tx),
                          GPU_ATTACHMENT_TEXTURE(this->reveal_tx));

  if (this->use_layer_fb) {
    this->color_layer_tx.acquire(size, format_color);
    this->reveal_layer_tx.acquire(size, format_reveal);

    this->layer_fb.ensure(GPU_ATTACHMENT_TEXTURE(this->depth_tx),
                          GPU_ATTACHMENT_TEXTURE(this->color_layer_tx),
                          GPU_ATTACHMENT_TEXTURE(this->reveal_layer_tx));
  }

  if (this->use_object_fb) {
    this->color_object_tx.acquire(size, format_color);
    this->reveal_object_tx.acquire(size, format_reveal);

    this->object_fb.ensure(GPU_ATTACHMENT_TEXTURE(this->depth_tx),
                           GPU_ATTACHMENT_TEXTURE(this->color_object_tx),
                           GPU_ATTACHMENT_TEXTURE(this->reveal_object_tx));
  }

  if (this->use_mask_fb) {
    /* Use high quality format for render. */
    const gpu::TextureFormat mask_format = this->is_render ? gpu::TextureFormat::UNORM_16 :
                                                             gpu::TextureFormat::UNORM_8;
    /* We need an extra depth to not disturb the normal drawing. */
    this->mask_depth_tx.acquire(size, gpu::TextureFormat::SFLOAT_32_DEPTH_UINT_8);
    /* The mask_color_tx is needed for frame-buffer completeness. */
    this->mask_color_tx.acquire(size, gpu::TextureFormat::UNORM_8);
    this->mask_tx.acquire(size, mask_format);

    this->mask_fb.ensure(GPU_ATTACHMENT_TEXTURE(this->mask_depth_tx),
                         GPU_ATTACHMENT_TEXTURE(this->mask_color_tx),
                         GPU_ATTACHMENT_TEXTURE(this->mask_tx));
  }

  if (this->use_separate_pass) {
    const int2 size = int2(draw_ctx->viewport_size_get());
    draw::TextureFromPool &output_pass_texture = DRW_viewport_pass_texture_get("GreasePencil");
    output_pass_texture.acquire(size, gpu::TextureFormat::SFLOAT_16_16_16_16);
    this->gpencil_pass_fb.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(output_pass_texture));
  }
}

void Instance::release_resources()
{
  this->depth_tx.release();
  this->color_tx.release();
  this->reveal_tx.release();
  this->color_layer_tx.release();
  this->reveal_layer_tx.release();
  this->color_object_tx.release();
  this->reveal_object_tx.release();
  this->mask_depth_tx.release();
  this->mask_color_tx.release();
  this->mask_tx.release();
  this->smaa_edge_tx.release();
  this->smaa_weight_tx.release();
}

void Instance::draw_mask(View &view, tObject *ob, tLayer *layer)
{
  Manager *manager = DRW_manager_get();

  const float clear_col[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  float clear_depth = ob->is_drawmode3d ? 1.0f : 0.0f;
  bool inverted = false;
  /* OPTI(@fclem): we could optimize by only clearing if the new mask_bits does not contain all
   * the masks already rendered in the buffer, and drawing only the layers not already drawn. */
  bool cleared = false;

  GPU_debug_group_begin("GPencil Mask");

  GPU_framebuffer_bind(this->mask_fb);

  for (int i = 0; i < GP_MAX_MASKBITS; i++) {
    if (!BLI_BITMAP_TEST(layer->mask_bits, i)) {
      continue;
    }

    if (BLI_BITMAP_TEST_BOOL(layer->mask_invert_bits, i) != inverted) {
      if (cleared) {
        manager->submit(this->mask_invert_ps);
      }
      inverted = !inverted;
    }

    if (!cleared) {
      cleared = true;
      GPU_framebuffer_clear_color_depth(this->mask_fb, clear_col, clear_depth);
    }

    tLayer *mask_layer = grease_pencil_layer_cache_get(ob, i, true);
    /* When filtering by view-layer, the mask could be null and must be ignored. */
    if (mask_layer == nullptr) {
      continue;
    }

    manager->submit(*mask_layer->geom_ps, view);
  }

  if (!inverted) {
    /* Blend shader expect an opacity mask not a reavealage buffer. */
    manager->submit(this->mask_invert_ps);
  }

  GPU_debug_group_end();
}

void Instance::draw_object(View &view, tObject *ob)
{
  Manager *manager = DRW_manager_get();

  const float clear_cols[2][4] = {{0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}};

  GPU_debug_group_begin("GPencil Object");

  gpu::FrameBuffer *fb_object = (ob->vfx.first) ? this->object_fb : this->gpencil_fb;

  GPU_framebuffer_bind(fb_object);
  GPU_framebuffer_clear_depth_stencil(fb_object, ob->is_drawmode3d ? 1.0f : 0.0f, 0x00);

  if (ob->vfx.first) {
    GPU_framebuffer_multi_clear(fb_object, clear_cols);
  }

  LISTBASE_FOREACH (tLayer *, layer, &ob->layers) {
    if (layer->mask_bits) {
      draw_mask(view, ob, layer);
    }

    if (layer->blend_ps) {
      GPU_framebuffer_bind(this->layer_fb);
      GPU_framebuffer_multi_clear(this->layer_fb, clear_cols);
    }
    else {
      GPU_framebuffer_bind(fb_object);
    }

    manager->submit(*layer->geom_ps, view);

    if (layer->blend_ps) {
      GPU_framebuffer_bind(fb_object);
      manager->submit(*layer->blend_ps);
    }
  }

  LISTBASE_FOREACH (tVfx *, vfx, &ob->vfx) {
    GPU_framebuffer_bind(*(vfx->target_fb));
    manager->submit(*vfx->vfx_ps);
  }

  this->object_bound_mat = float4x4(ob->plane_mat);
  this->is_stroke_order_3d = ob->is_drawmode3d;

  if (this->scene_fb) {
    GPU_framebuffer_bind(this->scene_fb);
    manager->submit(this->merge_depth_ps, view);
  }

  GPU_debug_group_end();
}

void Instance::draw(Manager &manager)
{
  DefaultTextureList *dtxl = draw_ctx->viewport_texture_list_get();
  DefaultFramebufferList *dfbl = draw_ctx->viewport_framebuffer_list_get();

  if (this->render_depth_tx.is_valid()) {
    this->scene_depth_tx = this->render_depth_tx;
    this->scene_fb = this->render_fb;
  }
  else {
    this->scene_fb = dfbl->default_fb;
    this->scene_depth_tx = dtxl->depth;
  }
  BLI_assert(this->scene_depth_tx);

  float clear_cols[2][4] = {{0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}};

  /* Fade 3D objects. */
  if ((!this->is_render) && (this->fade_3d_object_opacity > -1.0f) && (this->obact != nullptr) &&
      ELEM(this->obact->type, OB_GREASE_PENCIL))
  {
    float background_color[3];
    ED_view3d_background_color_get(this->scene, this->v3d, background_color);
    /* Blend color. */
    interp_v3_v3v3(clear_cols[0], background_color, clear_cols[0], this->fade_3d_object_opacity);

    mul_v4_fl(clear_cols[1], this->fade_3d_object_opacity);
  }

  /* Sort object by decreasing Z to avoid most of alpha ordering issues. */
  gpencil_object_cache_sort(this);

  if (this->tobjects.first == nullptr) {
    return;
  }

  DRW_submission_start();

  antialiasing_init();

  this->acquire_resources();

  if (this->tobjects.first) {
    GPU_framebuffer_bind(this->gpencil_fb);
    GPU_framebuffer_multi_clear(this->gpencil_fb, clear_cols);
  }

  View &view = View::default_get();

  LISTBASE_FOREACH (tObject *, ob, &this->tobjects) {
    draw_object(view, ob);
  }

  if (this->scene_fb) {
    antialiasing_draw(manager);
  }

  this->release_resources();

  DRW_submission_end();
}

DrawEngine *Engine::create_instance()
{
  return new Instance();
}

void Engine::free_static()
{
  ShaderCache::release();
}

}  // namespace blender::draw::gpencil
