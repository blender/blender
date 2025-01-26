/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */
#include "DRW_engine.hh"
#include "DRW_render.hh"

#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_gpencil_geom_legacy.h"
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

#include "gpencil_engine.h"

#include "DEG_depsgraph_query.hh"

#include "ED_grease_pencil.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "draw_manager_profiling.hh"

/* *********** FUNCTIONS *********** */

void GPENCIL_engine_init(void *ved)
{
  GPENCIL_Data *vedata = (GPENCIL_Data *)ved;
  GPENCIL_StorageList *stl = vedata->stl;
  const DRWContextState *ctx = DRW_context_state_get();
  const View3D *v3d = ctx->v3d;

  if (vedata->instance == nullptr) {
    vedata->instance = new GPENCIL_Instance();
  }
  GPENCIL_Instance &inst = *vedata->instance;

  if (!stl->pd) {
    stl->pd = static_cast<GPENCIL_PrivateData *>(
        MEM_callocN(sizeof(GPENCIL_PrivateData), "GPENCIL_PrivateData"));
  }

  if (!inst.dummy_texture.is_valid()) {
    const float pixels[1][4] = {{1.0f, 0.0f, 1.0f, 1.0f}};
    inst.dummy_texture.ensure_2d(GPU_RGBA8, int2(1), GPU_TEXTURE_USAGE_SHADER_READ, &pixels[0][0]);
  }
  if (!inst.dummy_depth.is_valid()) {
    const float pixels[1] = {1.0f};
    inst.dummy_depth.ensure_2d(
        GPU_DEPTH_COMPONENT24, int2(1), GPU_TEXTURE_USAGE_SHADER_READ, &pixels[0]);
  }

  GPENCIL_ViewLayerData *vldata = GPENCIL_view_layer_data_ensure();

  /* Resize and reset memory-blocks. */
  BLI_memblock_clear(vldata->gp_light_pool, gpencil_light_pool_free);
  BLI_memblock_clear(vldata->gp_material_pool, gpencil_material_pool_free);
  BLI_memblock_clear(vldata->gp_object_pool, nullptr);
  vldata->gp_layer_pool->clear();
  vldata->gp_vfx_pool->clear();
  BLI_memblock_clear(vldata->gp_maskbit_pool, nullptr);

  stl->pd->gp_light_pool = vldata->gp_light_pool;
  stl->pd->gp_material_pool = vldata->gp_material_pool;
  stl->pd->gp_maskbit_pool = vldata->gp_maskbit_pool;
  stl->pd->gp_object_pool = vldata->gp_object_pool;
  stl->pd->gp_layer_pool = vldata->gp_layer_pool;
  stl->pd->gp_vfx_pool = vldata->gp_vfx_pool;
  stl->pd->view_layer = ctx->view_layer;
  stl->pd->scene = ctx->scene;
  stl->pd->v3d = ctx->v3d;
  stl->pd->last_light_pool = nullptr;
  stl->pd->last_material_pool = nullptr;
  stl->pd->tobjects.first = nullptr;
  stl->pd->tobjects.last = nullptr;
  stl->pd->tobjects_infront.first = nullptr;
  stl->pd->tobjects_infront.last = nullptr;
  stl->pd->sbuffer_tobjects.first = nullptr;
  stl->pd->sbuffer_tobjects.last = nullptr;
  stl->pd->dummy_tx = inst.dummy_texture;
  stl->pd->dummy_depth = inst.dummy_depth;
  stl->pd->draw_wireframe = (v3d && v3d->shading.type == OB_WIRE);
  stl->pd->scene_depth_tx = nullptr;
  stl->pd->scene_fb = nullptr;
  stl->pd->is_render = inst.render_depth_tx.is_valid() || (v3d && v3d->shading.type == OB_RENDER);
  stl->pd->is_viewport = (v3d != nullptr);
  stl->pd->global_light_pool = gpencil_light_pool_add(stl->pd);
  stl->pd->shadeless_light_pool = gpencil_light_pool_add(stl->pd);
  /* Small HACK: we don't want the global pool to be reused,
   * so we set the last light pool to nullptr. */
  stl->pd->last_light_pool = nullptr;

  bool use_scene_lights = false;
  bool use_scene_world = false;

  if (v3d) {
    use_scene_lights = V3D_USES_SCENE_LIGHTS(v3d);

    use_scene_world = V3D_USES_SCENE_WORLD(v3d);

    stl->pd->v3d_color_type = (v3d->shading.type == OB_SOLID) ? v3d->shading.color_type : -1;
    /* Special case: If we're in Vertex Paint mode, enforce #V3D_SHADING_VERTEX_COLOR setting. */
    if (v3d->shading.type == OB_SOLID && ctx->obact &&
        (ctx->obact->mode & OB_MODE_VERTEX_GREASE_PENCIL) != 0)
    {
      stl->pd->v3d_color_type = V3D_SHADING_VERTEX_COLOR;
    }

    copy_v3_v3(stl->pd->v3d_single_color, v3d->shading.single_color);

    /* For non active frame, use only lines in multiedit mode. */
    const bool overlays_on = (v3d->flag2 & V3D_HIDE_OVERLAYS) == 0;
    stl->pd->use_multiedit_lines_only = overlays_on &&
                                        (v3d->gp_flag & V3D_GP_SHOW_MULTIEDIT_LINES) != 0;

    const bool shmode_xray_support = v3d->shading.type <= OB_SOLID;
    stl->pd->xray_alpha = (shmode_xray_support && XRAY_ENABLED(v3d)) ? XRAY_ALPHA(v3d) : 1.0f;
    stl->pd->force_stroke_order_3d = v3d->gp_flag & V3D_GP_FORCE_STROKE_ORDER_3D;
  }
  else if (stl->pd->is_render) {
    use_scene_lights = true;
    use_scene_world = true;
    stl->pd->use_multiedit_lines_only = false;
    stl->pd->xray_alpha = 1.0f;
    stl->pd->v3d_color_type = -1;
    stl->pd->force_stroke_order_3d = false;
  }

  stl->pd->use_lighting = (v3d && v3d->shading.type > OB_SOLID) || stl->pd->is_render;
  stl->pd->use_lights = use_scene_lights;

  gpencil_light_ambient_add(stl->pd->shadeless_light_pool, blender::float3{1.0f, 1.0f, 1.0f});

  World *world = ctx->scene->world;
  if (world != nullptr && use_scene_world) {
    gpencil_light_ambient_add(stl->pd->global_light_pool, &world->horr);
  }
  else if (v3d) {
    float world_light[3];
    copy_v3_fl(world_light, v3d->shading.studiolight_intensity);
    gpencil_light_ambient_add(stl->pd->global_light_pool, world_light);
  }

  float4x4 viewmatinv = blender::draw::View::default_get().viewinv();
  copy_v3_v3(stl->pd->camera_z_axis, viewmatinv[2]);
  copy_v3_v3(stl->pd->camera_pos, viewmatinv[3]);
  stl->pd->camera_z_offset = dot_v3v3(viewmatinv[3], viewmatinv[2]);

  if (ctx && ctx->rv3d && v3d) {
    stl->pd->camera = (ctx->rv3d->persp == RV3D_CAMOB) ? v3d->camera : nullptr;
  }
  else {
    stl->pd->camera = nullptr;
  }
}

void GPENCIL_cache_init(void *ved)
{
  using namespace blender::draw;
  GPENCIL_Data *vedata = (GPENCIL_Data *)ved;
  GPENCIL_Instance *inst = vedata->instance;
  GPENCIL_PrivateData *pd = vedata->stl->pd;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  pd->cfra = int(DEG_get_ctime(draw_ctx->depsgraph));
  pd->simplify_antialias = GPENCIL_SIMPLIFY_AA(draw_ctx->scene);
  pd->use_layer_fb = false;
  pd->use_object_fb = false;
  pd->use_mask_fb = false;
  /* Always use high precision for render. */
  pd->use_signed_fb = !pd->is_viewport;

  if (draw_ctx->v3d) {
    const bool hide_overlay = ((draw_ctx->v3d->flag2 & V3D_HIDE_OVERLAYS) != 0);
    const bool show_onion = ((draw_ctx->v3d->gp_flag & V3D_GP_SHOW_ONION_SKIN) != 0);
    const bool playing = (draw_ctx->evil_C != nullptr) ?
                             ED_screen_animation_playing(CTX_wm_manager(draw_ctx->evil_C)) !=
                                 nullptr :
                             false;
    pd->do_onion = show_onion && !hide_overlay && !playing;
    pd->playing = playing;
    /* Save simplify flags (can change while drawing, so it's better to save). */
    Scene *scene = draw_ctx->scene;
    pd->simplify_fill = GPENCIL_SIMPLIFY_FILL(scene, playing);
    pd->simplify_fx = GPENCIL_SIMPLIFY_FX(scene, playing) ||
                      (draw_ctx->v3d->shading.type < OB_RENDER);

    /* Fade Layer. */
    const bool is_fade_layer = ((!hide_overlay) && (!pd->is_render) &&
                                (draw_ctx->v3d->gp_flag & V3D_GP_FADE_NOACTIVE_LAYERS));
    pd->fade_layer_opacity = (is_fade_layer) ? draw_ctx->v3d->overlay.gpencil_fade_layer : -1.0f;
    pd->vertex_paint_opacity = draw_ctx->v3d->overlay.gpencil_vertex_paint_opacity;
    /* Fade GPencil Objects. */
    const bool is_fade_object = ((!hide_overlay) && (!pd->is_render) &&
                                 (draw_ctx->v3d->gp_flag & V3D_GP_FADE_OBJECTS) &&
                                 (draw_ctx->v3d->gp_flag & V3D_GP_FADE_NOACTIVE_GPENCIL));
    pd->fade_gp_object_opacity = (is_fade_object) ? draw_ctx->v3d->overlay.gpencil_paper_opacity :
                                                    -1.0f;
    pd->fade_3d_object_opacity = ((!hide_overlay) && (!pd->is_render) &&
                                  (draw_ctx->v3d->gp_flag & V3D_GP_FADE_OBJECTS)) ?
                                     draw_ctx->v3d->overlay.gpencil_paper_opacity :
                                     -1.0f;
  }
  else {
    pd->do_onion = true;
    Scene *scene = draw_ctx->scene;
    pd->simplify_fill = GPENCIL_SIMPLIFY_FILL(scene, false);
    pd->simplify_fx = GPENCIL_SIMPLIFY_FX(scene, false);
    pd->fade_layer_opacity = -1.0f;
    pd->playing = false;
  }

  {
    pd->stroke_batch = nullptr;
    pd->fill_batch = nullptr;
    pd->do_fast_drawing = false;

    pd->obact = draw_ctx->obact;
  }

  if (pd->do_fast_drawing) {
    pd->snapshot_buffer_dirty = !inst->snapshot_depth_tx.is_valid();
    const float *size = DRW_viewport_size_get();

    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT;
    inst->snapshot_depth_tx.ensure_2d(GPU_DEPTH24_STENCIL8, int2(size), usage);
    inst->snapshot_color_tx.ensure_2d(GPU_R11F_G11F_B10F, int2(size), usage);
    inst->snapshot_reveal_tx.ensure_2d(GPU_R11F_G11F_B10F, int2(size), usage);

    inst->snapshot_fb.ensure(GPU_ATTACHMENT_TEXTURE(inst->snapshot_depth_tx),
                             GPU_ATTACHMENT_TEXTURE(inst->snapshot_color_tx),
                             GPU_ATTACHMENT_TEXTURE(inst->snapshot_reveal_tx));
  }
  else {
    /* Free unneeded buffers. */
    inst->snapshot_depth_tx.free();
    inst->snapshot_color_tx.free();
    inst->snapshot_reveal_tx.free();
  }

  {
    blender::draw::PassSimple &pass = inst->merge_depth_ps;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
    pass.shader_set(GPENCIL_shader_depth_merge_get());
    pass.bind_texture("depthBuf", &inst->depth_tx);
    pass.push_constant("strokeOrder3d", &pd->is_stroke_order_3d);
    pass.push_constant("gpModelMatrix", &inst->object_bound_mat);
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
  {
    blender::draw::PassSimple &pass = inst->mask_invert_ps;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_LOGIC_INVERT);
    pass.shader_set(GPENCIL_shader_mask_invert_get());
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }

  Camera *cam = static_cast<Camera *>(
      (pd->camera != nullptr && pd->camera->type == OB_CAMERA) ? pd->camera->data : nullptr);

  /* Pseudo DOF setup. */
  if (cam && (cam->dof.flag & CAM_DOF_ENABLED)) {
    const float *vp_size = DRW_viewport_size_get();
    float fstop = cam->dof.aperture_fstop;
    float sensor = BKE_camera_sensor_size(cam->sensor_fit, cam->sensor_x, cam->sensor_y);
    float focus_dist = BKE_camera_object_dof_distance(pd->camera);
    float focal_len = cam->lens;

    const float scale_camera = 0.001f;
    /* We want radius here for the aperture number. */
    float aperture = 0.5f * scale_camera * focal_len / fstop;
    float focal_len_scaled = scale_camera * focal_len;
    float sensor_scaled = scale_camera * sensor;

    if (draw_ctx->rv3d != nullptr) {
      sensor_scaled *= draw_ctx->rv3d->viewcamtexcofac[0];
    }

    pd->dof_params[1] = aperture * fabsf(focal_len_scaled / (focus_dist - focal_len_scaled));
    pd->dof_params[1] *= vp_size[0] / sensor_scaled;
    pd->dof_params[0] = -focus_dist * pd->dof_params[1];
  }
  else {
    /* Disable DoF blur scaling. */
    pd->camera = nullptr;
  }
}

#define DISABLE_BATCHING 0

/* Check if the passed in layer is used by any other layer as a mask (in the viewlayer). */
static bool is_used_as_layer_mask_in_viewlayer(const GreasePencil &grease_pencil,
                                               const blender::bke::greasepencil::Layer &mask_layer,
                                               const ViewLayer &view_layer)
{
  using namespace blender::bke::greasepencil;
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

/* Returns true if this layer should be rendered (as part of the viewlayer). */
static bool use_layer_in_render(const GreasePencil &grease_pencil,
                                const blender::bke::greasepencil::Layer &layer,
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

static GPENCIL_tObject *grease_pencil_object_cache_populate(
    GPENCIL_Instance *inst,
    GPENCIL_PrivateData *pd,
    Object *ob,
    blender::draw::ResourceHandle res_handle)
{
  using namespace blender;
  using namespace blender::ed::greasepencil;
  using namespace blender::bke::greasepencil;
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);
  const bool is_vertex_mode = (ob->mode & OB_MODE_VERTEX_PAINT) != 0;
  const blender::Bounds<float3> bounds = grease_pencil.bounds_min_max_eval().value_or(
      blender::Bounds(float3(0)));

  const bool do_onion = !pd->is_render && pd->do_onion;
  const bool do_multi_frame = (((pd->scene->toolsettings->gpencil_flags &
                                 GP_USE_MULTI_FRAME_EDITING) != 0) &&
                               (ob->mode != OB_MODE_OBJECT));
  const bool use_stroke_order_3d = pd->force_stroke_order_3d ||
                                   ((grease_pencil.flag & GREASE_PENCIL_STROKE_ORDER_3D) != 0);
  GPENCIL_tObject *tgp_ob = gpencil_object_cache_add(pd, ob, use_stroke_order_3d, bounds);

  int mat_ofs = 0;
  GPENCIL_MaterialPool *matpool = gpencil_material_pool_create(pd, ob, &mat_ofs, is_vertex_mode);

  GPUTexture *tex_fill = pd->dummy_tx;
  GPUTexture *tex_stroke = pd->dummy_tx;

  blender::gpu::Batch *iter_geom = nullptr;
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
      [&](PassSimple &pass, blender::gpu::Batch *draw_geom, const int v_first, const int v_count) {
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
  const Vector<DrawingInfo> drawings = retrieve_visible_drawings(*pd->scene, grease_pencil, true);
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
        grease_pencil, layer, *pd->view_layer, is_layer_used_as_mask);
    if (!show_drawing_in_render) {
      /* Skip over the entire drawing. */
      t_offset += total_num_triangles;
      t_offset += total_num_vertices * 2;
      continue;
    }

    if (last_pass) {
      drawcall_flush(*last_pass);
    }

    GPENCIL_tLayer *tgp_layer = grease_pencil_layer_cache_add(
        inst, pd, ob, layer, info.onion_id, is_layer_used_as_mask, tgp_ob);
    PassSimple &pass = *tgp_layer->geom_ps;
    last_pass = &pass;

    const bool use_lights = pd->use_lighting &&
                            ((layer.base.flag & GP_LAYER_TREE_NODE_USE_LIGHTS) != 0) &&
                            (ob->dtx & OB_USE_GPENCIL_LIGHTS);

    GPUUniformBuf *lights_ubo = (use_lights) ? pd->global_light_pool->ubo :
                                               pd->shadeless_light_pool->ubo;

    GPUUniformBuf *ubo_mat;
    gpencil_material_resources_get(matpool, 0, nullptr, nullptr, &ubo_mat);

    pass.bind_ubo("gp_lights", lights_ubo);
    pass.bind_ubo("gp_materials", ubo_mat);
    pass.bind_texture("gpFillTexture", tex_fill);
    pass.bind_texture("gpStrokeTexture", tex_stroke);
    pass.push_constant("gpMaterialOffset", mat_ofs);
    /* Since we don't use the sbuffer in GPv3, this is always 0. */
    pass.push_constant("gpStrokeIndexOffset", 0.0f);
    pass.push_constant("viewportSize", float2(DRW_viewport_size_get()));

    const VArray<int> stroke_materials = *attributes.lookup_or_default<int>(
        "material_index", bke::AttrDomain::Curve, 0);

    const bool only_lines = !ELEM(ob->mode,
                                  OB_MODE_PAINT_GREASE_PENCIL,
                                  OB_MODE_WEIGHT_GREASE_PENCIL,
                                  OB_MODE_VERTEX_GREASE_PENCIL) &&
                            info.frame_number != pd->cfra && pd->use_multiedit_lines_only &&
                            do_multi_frame;
    const bool is_onion = info.onion_id != 0;

    visible_strokes.foreach_index([&](const int stroke_i, const int pos) {
      const IndexRange points = points_by_curve[stroke_i];
      /* The material index is allowed to be negative as it's stored as a generic attribute. We
       * clamp it here to avoid crashing in the rendering code. Any stroke with a material < 0 will
       * use the first material in the first material slot. */
      const int material_index = std::max(stroke_materials[stroke_i], 0);
      const MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, material_index + 1);

      const bool hide_material = (gp_style->flag & GP_MATERIAL_HIDE) != 0;
      const bool show_stroke = ((gp_style->flag & GP_MATERIAL_STROKE_SHOW) != 0);
      const bool show_fill = (points.size() >= 3) &&
                             ((gp_style->flag & GP_MATERIAL_FILL_SHOW) != 0) &&
                             (!pd->simplify_fill);
      const bool hide_onion = is_onion && ((gp_style->flag & GP_MATERIAL_HIDE_ONIONSKIN) != 0 ||
                                           (!do_onion && !do_multi_frame));
      const bool skip_stroke = hide_material || (!show_stroke && !show_fill) ||
                               (only_lines && !do_onion && is_onion) || hide_onion;

      if (skip_stroke) {
        t_offset += num_triangles_per_stroke[pos];
        t_offset += num_vertices_per_stroke[pos] * 2;
        return;
      }

      GPUUniformBuf *new_ubo_mat;
      GPUTexture *new_tex_fill = nullptr;
      GPUTexture *new_tex_stroke = nullptr;
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
          pass.bind_texture("gpFillTexture", new_tex_fill);
          tex_fill = new_tex_fill;
        }
        if (new_tex_stroke) {
          pass.bind_texture("gpStrokeTexture", new_tex_stroke);
          tex_stroke = new_tex_stroke;
        }
      }

      blender::gpu::Batch *geom = draw::DRW_cache_grease_pencil_get(pd->scene, ob);
      if (iter_geom != geom) {
        drawcall_flush(pass);

        blender::gpu::VertBuf *position_tx = draw::DRW_cache_grease_pencil_position_buffer_get(
            pd->scene, ob);
        blender::gpu::VertBuf *color_tx = draw::DRW_cache_grease_pencil_color_buffer_get(pd->scene,
                                                                                         ob);
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

void GPENCIL_cache_populate(void *ved, Object *ob)
{
  GPENCIL_Data *vedata = (GPENCIL_Data *)ved;
  GPENCIL_Instance *inst = vedata->instance;
  GPENCIL_PrivateData *pd = vedata->stl->pd;

  /* object must be visible */
  if (!(DRW_object_visibility_in_active_context(ob) & OB_VISIBLE_SELF)) {
    return;
  }

  if (ob->data && (ob->type == OB_GREASE_PENCIL) && (ob->dt >= OB_SOLID)) {
    blender::draw::Manager *manager = DRW_manager_get();
    blender::draw::ObjectRef ob_ref = DRW_object_ref_get(ob);
    blender::draw::ResourceHandle res_handle = manager->unique_handle(ob_ref);

    GPENCIL_tObject *tgp_ob = grease_pencil_object_cache_populate(inst, pd, ob, res_handle);
    gpencil_vfx_cache_populate(
        vedata,
        ob,
        tgp_ob,
        ELEM(ob->mode, OB_MODE_EDIT, OB_MODE_SCULPT_GREASE_PENCIL, OB_MODE_WEIGHT_GREASE_PENCIL));
  }

  if (ob->type == OB_LAMP && pd->use_lights) {
    gpencil_light_pool_populate(pd->global_light_pool, ob);
  }
}

void GPENCIL_cache_finish(void *ved)
{
  GPENCIL_Data *vedata = (GPENCIL_Data *)ved;
  GPENCIL_PrivateData *pd = vedata->stl->pd;

  /* Upload UBO data. */
  BLI_memblock_iter iter;
  BLI_memblock_iternew(pd->gp_material_pool, &iter);
  GPENCIL_MaterialPool *pool;
  while ((pool = (GPENCIL_MaterialPool *)BLI_memblock_iterstep(&iter))) {
    GPU_uniformbuf_update(pool->ubo, pool->mat_data);
  }

  BLI_memblock_iternew(pd->gp_light_pool, &iter);
  GPENCIL_LightPool *lpool;
  while ((lpool = (GPENCIL_LightPool *)BLI_memblock_iterstep(&iter))) {
    GPU_uniformbuf_update(lpool->ubo, lpool->light_data);
  }
}

void GPENCIL_Instance::acquire_resources(GPENCIL_PrivateData *pd)
{
  /* Create frame-buffers only if needed. */
  if (pd->tobjects.first == nullptr) {
    return;
  }

  const float *size_f = DRW_viewport_size_get();
  const int2 size(size_f[0], size_f[1]);

  eGPUTextureFormat format = pd->use_signed_fb ? GPU_RGBA16F : GPU_R11F_G11F_B10F;

  this->depth_tx.acquire(size, GPU_DEPTH24_STENCIL8);
  this->color_tx.acquire(size, format);
  this->reveal_tx.acquire(size, format);

  this->gpencil_fb.ensure(GPU_ATTACHMENT_TEXTURE(this->depth_tx),
                          GPU_ATTACHMENT_TEXTURE(this->color_tx),
                          GPU_ATTACHMENT_TEXTURE(this->reveal_tx));

  if (pd->use_layer_fb) {
    this->color_layer_tx.acquire(size, format);
    this->reveal_layer_tx.acquire(size, format);

    this->layer_fb.ensure(GPU_ATTACHMENT_TEXTURE(this->depth_tx),
                          GPU_ATTACHMENT_TEXTURE(this->color_layer_tx),
                          GPU_ATTACHMENT_TEXTURE(this->reveal_layer_tx));
  }

  if (pd->use_object_fb) {
    this->color_object_tx.acquire(size, format);
    this->reveal_object_tx.acquire(size, format);

    this->object_fb.ensure(GPU_ATTACHMENT_TEXTURE(this->depth_tx),
                           GPU_ATTACHMENT_TEXTURE(this->color_object_tx),
                           GPU_ATTACHMENT_TEXTURE(this->reveal_object_tx));
  }

  if (pd->use_mask_fb) {
    /* Use high quality format for render. */
    eGPUTextureFormat mask_format = pd->is_render ? GPU_R16 : GPU_R8;
    /* We need an extra depth to not disturb the normal drawing. */
    this->mask_depth_tx.acquire(size, GPU_DEPTH24_STENCIL8);
    /* The mask_color_tx is needed for frame-buffer completeness. */
    this->mask_color_tx.acquire(size, GPU_R8);
    this->mask_tx.acquire(size, mask_format);

    this->mask_fb.ensure(GPU_ATTACHMENT_TEXTURE(this->mask_depth_tx),
                         GPU_ATTACHMENT_TEXTURE(this->mask_color_tx),
                         GPU_ATTACHMENT_TEXTURE(this->mask_tx));
  }
}

void GPENCIL_Instance::release_resources()
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

static void gpencil_draw_mask(GPENCIL_Data *vedata,
                              blender::draw::View &view,
                              GPENCIL_tObject *ob,
                              GPENCIL_tLayer *layer)
{
  GPENCIL_Instance *inst = vedata->instance;
  blender::draw::Manager *manager = DRW_manager_get();

  const float clear_col[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  float clear_depth = ob->is_drawmode3d ? 1.0f : 0.0f;
  bool inverted = false;
  /* OPTI(@fclem): we could optimize by only clearing if the new mask_bits does not contain all
   * the masks already rendered in the buffer, and drawing only the layers not already drawn. */
  bool cleared = false;

  DRW_stats_group_start("GPencil Mask");

  GPU_framebuffer_bind(inst->mask_fb);

  for (int i = 0; i < GP_MAX_MASKBITS; i++) {
    if (!BLI_BITMAP_TEST(layer->mask_bits, i)) {
      continue;
    }

    if (BLI_BITMAP_TEST_BOOL(layer->mask_invert_bits, i) != inverted) {
      if (cleared) {
        manager->submit(inst->mask_invert_ps);
      }
      inverted = !inverted;
    }

    if (!cleared) {
      cleared = true;
      GPU_framebuffer_clear_color_depth(inst->mask_fb, clear_col, clear_depth);
    }

    GPENCIL_tLayer *mask_layer = grease_pencil_layer_cache_get(ob, i, true);
    /* When filtering by view-layer, the mask could be null and must be ignored. */
    if (mask_layer == nullptr) {
      continue;
    }

    manager->submit(*mask_layer->geom_ps, view);
  }

  if (!inverted) {
    /* Blend shader expect an opacity mask not a reavealage buffer. */
    manager->submit(inst->mask_invert_ps);
  }

  DRW_stats_group_end();
}

static void GPENCIL_draw_object(GPENCIL_Data *vedata,
                                blender::draw::View &view,
                                GPENCIL_tObject *ob)
{
  GPENCIL_Instance *inst = vedata->instance;
  blender::draw::Manager *manager = DRW_manager_get();

  GPENCIL_PrivateData *pd = vedata->stl->pd;
  const float clear_cols[2][4] = {{0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}};

  DRW_stats_group_start("GPencil Object");

  GPUFrameBuffer *fb_object = (ob->vfx.first) ? inst->object_fb : inst->gpencil_fb;

  GPU_framebuffer_bind(fb_object);
  GPU_framebuffer_clear_depth_stencil(fb_object, ob->is_drawmode3d ? 1.0f : 0.0f, 0x00);

  if (ob->vfx.first) {
    GPU_framebuffer_multi_clear(fb_object, clear_cols);
  }

  LISTBASE_FOREACH (GPENCIL_tLayer *, layer, &ob->layers) {
    if (layer->mask_bits) {
      gpencil_draw_mask(vedata, view, ob, layer);
    }

    if (layer->blend_ps) {
      GPU_framebuffer_bind(inst->layer_fb);
      GPU_framebuffer_multi_clear(inst->layer_fb, clear_cols);
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

  LISTBASE_FOREACH (GPENCIL_tVfx *, vfx, &ob->vfx) {
    GPU_framebuffer_bind(*(vfx->target_fb));
    manager->submit(*vfx->vfx_ps);
  }

  inst->object_bound_mat = float4x4(ob->plane_mat);
  pd->is_stroke_order_3d = ob->is_drawmode3d;

  if (pd->scene_fb) {
    GPU_framebuffer_bind(pd->scene_fb);
    manager->submit(inst->merge_depth_ps, view);
  }

  DRW_stats_group_end();
}

static void GPENCIL_fast_draw_start(GPENCIL_Data *vedata)
{
  GPENCIL_Instance *inst = vedata->instance;
  GPENCIL_PrivateData *pd = vedata->stl->pd;
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  if (!pd->snapshot_buffer_dirty) {
    /* Copy back cached render. */
    GPU_framebuffer_blit(inst->snapshot_fb, 0, dfbl->default_fb, 0, GPU_DEPTH_BIT);
    GPU_framebuffer_blit(inst->snapshot_fb, 0, inst->gpencil_fb, 0, GPU_COLOR_BIT);
    GPU_framebuffer_blit(inst->snapshot_fb, 1, inst->gpencil_fb, 1, GPU_COLOR_BIT);
    /* Bypass drawing. */
    pd->tobjects.first = pd->tobjects.last = nullptr;
  }
}

static void GPENCIL_fast_draw_end(GPENCIL_Data *vedata, blender::draw::View &view)
{
  GPENCIL_Instance *inst = vedata->instance;
  GPENCIL_PrivateData *pd = vedata->stl->pd;
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  if (pd->snapshot_buffer_dirty) {
    /* Save to snapshot buffer. */
    GPU_framebuffer_blit(dfbl->default_fb, 0, inst->snapshot_fb, 0, GPU_DEPTH_BIT);
    GPU_framebuffer_blit(inst->gpencil_fb, 0, inst->snapshot_fb, 0, GPU_COLOR_BIT);
    GPU_framebuffer_blit(inst->gpencil_fb, 1, inst->snapshot_fb, 1, GPU_COLOR_BIT);
    pd->snapshot_buffer_dirty = false;
  }
  /* Draw the sbuffer stroke(s). */
  LISTBASE_FOREACH (GPENCIL_tObject *, ob, &pd->sbuffer_tobjects) {
    GPENCIL_draw_object(vedata, view, ob);
  }
}

void GPENCIL_draw_scene(void *ved)
{
  using namespace blender::draw;
  GPENCIL_Data *vedata = (GPENCIL_Data *)ved;
  GPENCIL_Instance &inst = *vedata->instance;
  GPENCIL_PrivateData *pd = vedata->stl->pd;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  if (inst.render_depth_tx.is_valid()) {
    pd->scene_depth_tx = inst.render_depth_tx;
    pd->scene_fb = inst.render_fb;
  }
  else {
    pd->scene_fb = dfbl->default_fb;
    pd->scene_depth_tx = dtxl->depth;
  }
  BLI_assert(pd->scene_depth_tx);

  float clear_cols[2][4] = {{0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}};

  /* Fade 3D objects. */
  if ((!pd->is_render) && (pd->fade_3d_object_opacity > -1.0f) && (pd->obact != nullptr) &&
      ELEM(pd->obact->type, OB_GREASE_PENCIL))
  {
    float background_color[3];
    ED_view3d_background_color_get(pd->scene, pd->v3d, background_color);
    /* Blend color. */
    interp_v3_v3v3(clear_cols[0], background_color, clear_cols[0], pd->fade_3d_object_opacity);

    mul_v4_fl(clear_cols[1], pd->fade_3d_object_opacity);
  }

  /* Sort object by decreasing Z to avoid most of alpha ordering issues. */
  gpencil_object_cache_sort(pd);

  if (pd->tobjects.first == nullptr) {
    return;
  }

  GPENCIL_antialiasing_init(&inst, pd);

  inst.acquire_resources(pd);

  if (pd->do_fast_drawing) {
    GPENCIL_fast_draw_start(vedata);
  }

  if (pd->tobjects.first) {
    GPU_framebuffer_bind(inst.gpencil_fb);
    GPU_framebuffer_multi_clear(inst.gpencil_fb, clear_cols);
  }

  blender::draw::View &view = blender::draw::View::default_get();

  LISTBASE_FOREACH (GPENCIL_tObject *, ob, &pd->tobjects) {
    GPENCIL_draw_object(vedata, view, ob);
  }

  if (pd->do_fast_drawing) {
    GPENCIL_fast_draw_end(vedata, view);
  }

  if (pd->scene_fb) {
    GPENCIL_antialiasing_draw(vedata);
  }

  pd->gp_object_pool = pd->gp_maskbit_pool = nullptr;
  pd->gp_vfx_pool = nullptr;
  pd->gp_layer_pool = nullptr;

  inst.release_resources();
}

static void GPENCIL_engine_free()
{
  GPENCIL_shader_free();
}

static void GPENCIL_instance_free(void *instance)
{
  delete reinterpret_cast<GPENCIL_Instance *>(instance);
}

static const DrawEngineDataSize GPENCIL_data_size = DRW_VIEWPORT_DATA_SIZE(GPENCIL_Data);

DrawEngineType draw_engine_gpencil_type = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ N_("GpencilMode"),
    /*vedata_size*/ &GPENCIL_data_size,
    /*engine_init*/ &GPENCIL_engine_init,
    /*engine_free*/ &GPENCIL_engine_free,
    /*instance_free*/ &GPENCIL_instance_free,
    /*cache_init*/ &GPENCIL_cache_init,
    /*cache_populate*/ &GPENCIL_cache_populate,
    /*cache_finish*/ &GPENCIL_cache_finish,
    /*draw_scene*/ &GPENCIL_draw_scene,
    /*view_update*/ nullptr,
    /*id_update*/ nullptr,
    /*render_to_image*/ &GPENCIL_render_to_image,
    /*store_metadata*/ nullptr,
};
