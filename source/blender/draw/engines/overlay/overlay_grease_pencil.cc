/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.hh"

#include "draw_manager_text.hh"

#include "ED_grease_pencil.hh"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"

#include "DNA_grease_pencil_types.h"

#include "overlay_private.hh"

#include "DEG_depsgraph_query.hh"

static void is_selection_visible(bool &r_show_points, bool &r_show_lines)
{
  using namespace blender;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const ToolSettings *ts = draw_ctx->scene->toolsettings;
  const bool in_sculpt_mode = (draw_ctx->object_mode & OB_MODE_SCULPT_GREASE_PENCIL) != 0;
  const bool in_weight_mode = (draw_ctx->object_mode & OB_MODE_WEIGHT_GREASE_PENCIL) != 0;
  const bool in_vertex_mode = (draw_ctx->object_mode & OB_MODE_VERTEX_GREASE_PENCIL) != 0;
  const bool flag_show_lines = (draw_ctx->v3d->gp_flag & V3D_GP_SHOW_EDIT_LINES) != 0;

  if (in_weight_mode) {
    /* Always display points in weight mode. */
    r_show_points = true;
    r_show_lines = flag_show_lines;
    return;
  }

  if (in_sculpt_mode) {
    /* Sculpt selection modes are flags and can be disabled individually. */
    static constexpr int sculpt_point_modes = GP_SCULPT_MASK_SELECTMODE_POINT |
                                              GP_SCULPT_MASK_SELECTMODE_SEGMENT;
    r_show_points = (ts->gpencil_selectmode_sculpt & sculpt_point_modes) != 0;
    r_show_lines = flag_show_lines && (ts->gpencil_selectmode_sculpt != 0);
    return;
  }

  if (in_vertex_mode) {
    /* Vertex selection modes are flags and can be disabled individually. */
    static constexpr int vertex_point_modes = GP_VERTEX_MASK_SELECTMODE_POINT |
                                              GP_VERTEX_MASK_SELECTMODE_SEGMENT;
    r_show_points = (ts->gpencil_selectmode_vertex & vertex_point_modes) != 0;
    r_show_lines = flag_show_lines && (ts->gpencil_selectmode_vertex != 0);
    return;
  }

  /* Edit selection modes are exclusive. */
  r_show_points = ELEM(ts->gpencil_selectmode_edit, GP_SELECTMODE_POINT, GP_SELECTMODE_SEGMENT);
  r_show_lines = flag_show_lines;
}

static void overlay_grease_pencil_draw_stroke_color_name(Object &object,
                                                         const int mat_nr,
                                                         const blender::float3 position)
{
  Material *ma = BKE_object_material_get_eval(&object, mat_nr + 1);
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ViewLayer *view_layer = draw_ctx->view_layer;

  int theme_id = DRW_object_wire_theme_get(&object, view_layer, nullptr);
  uchar color[4];
  UI_GetThemeColor4ubv(theme_id, color);

  const float3 fpt = blender::math::transform_point(object.object_to_world(), position);

  DRWTextStore *dt = DRW_text_cache_ensure();
  DRW_text_cache_add(dt,
                     fpt,
                     ma->id.name + 2,
                     strlen(ma->id.name + 2),
                     10,
                     0,
                     DRW_TEXT_CACHE_GLOBALSPACE | DRW_TEXT_CACHE_STRING_PTR,
                     color);
}

static void OVERLAY_grease_pencil_material_names(Object *ob)
{
  using namespace blender;

  const DRWContextState *draw_ctx = DRW_context_state_get();

  bool show_points = false;
  bool show_lines = false;
  is_selection_visible(show_points, show_lines);

  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);

  Vector<ed::greasepencil::DrawingInfo> drawings = ed::greasepencil::retrieve_visible_drawings(
      *draw_ctx->scene, grease_pencil, false);
  if (drawings.is_empty()) {
    return;
  }

  for (const ed::greasepencil::DrawingInfo &info : drawings) {
    const bke::greasepencil::Drawing &drawing = info.drawing;

    const bke::CurvesGeometry strokes = drawing.strokes();
    const OffsetIndices<int> points_by_curve = strokes.points_by_curve();
    const bke::AttrDomain domain = show_points ? bke::AttrDomain::Point : bke::AttrDomain::Curve;
    const VArray<bool> selections = *strokes.attributes().lookup_or_default<bool>(
        ".selection", domain, true);
    const VArray<int> materials = *strokes.attributes().lookup_or_default<int>(
        "material_index", bke::AttrDomain::Curve, 0);
    const Span<float3> positions = strokes.positions();

    auto show_stroke_name = [&](const int stroke_i) {
      if (show_points) {
        for (const int point_i : points_by_curve[stroke_i]) {
          if (selections[point_i]) {
            return true;
          }
        }
        return false;
      }
      return selections[stroke_i];
    };

    for (const int stroke_i : strokes.curves_range()) {
      const int point_i = points_by_curve[stroke_i].first();
      if (show_stroke_name(stroke_i)) {
        overlay_grease_pencil_draw_stroke_color_name(*ob, materials[stroke_i], positions[point_i]);
      }
    }
  }
}

void OVERLAY_edit_grease_pencil_cache_init(OVERLAY_Data *vedata)
{
  using namespace blender;
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const bool use_weight = (draw_ctx->object_mode & OB_MODE_WEIGHT_GREASE_PENCIL) != 0;
  View3D *v3d = draw_ctx->v3d;

  GPUShader *sh;
  DRWShadingGroup *grp;

  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                   DRW_STATE_BLEND_ALPHA;
  DRW_PASS_CREATE(psl->edit_grease_pencil_ps, (state | pd->clipping_state));

  bool show_points = false;
  bool show_lines = false;
  is_selection_visible(show_points, show_lines);

  if (show_lines) {
    sh = OVERLAY_shader_edit_particle_strand();
    grp = pd->edit_grease_pencil_wires_grp = DRW_shgroup_create(sh, psl->edit_grease_pencil_ps);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_bool_copy(grp, "useWeight", use_weight);
    DRW_shgroup_uniform_bool_copy(grp, "useGreasePencil", true);
    DRW_shgroup_uniform_texture(grp, "weightTex", G_draw.weight_ramp);
  }
  else {
    pd->edit_grease_pencil_wires_grp = nullptr;
  }

  if (show_points) {
    sh = OVERLAY_shader_edit_particle_point();
    grp = pd->edit_grease_pencil_points_grp = DRW_shgroup_create(sh, psl->edit_grease_pencil_ps);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_bool_copy(grp, "useWeight", use_weight);
    DRW_shgroup_uniform_bool_copy(grp, "useGreasePencil", true);
    DRW_shgroup_uniform_texture(grp, "weightTex", G_draw.weight_ramp);
    const bool show_direction = (v3d->gp_flag & V3D_GP_SHOW_STROKE_DIRECTION) != 0;
    DRW_shgroup_uniform_bool_copy(grp, "doStrokeEndpoints", show_direction);
  }
  else {
    pd->edit_grease_pencil_points_grp = nullptr;
  }
}

void OVERLAY_grease_pencil_cache_init(OVERLAY_Data *vedata)
{
  using namespace blender;
  OVERLAY_PassList *psl = vedata->psl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  ToolSettings *ts = scene->toolsettings;
  const View3D *v3d = draw_ctx->v3d;

  GPUShader *sh;
  DRWShadingGroup *grp;

  /* Default: Display nothing. */
  psl->grease_pencil_canvas_ps = nullptr;

  Object *ob = draw_ctx->obact;
  const bool show_overlays = (v3d->flag2 & V3D_HIDE_OVERLAYS) == 0;
  const bool show_grid = (v3d->gp_flag & V3D_GP_SHOW_GRID) != 0 &&
                         ((ts->gpencil_v3d_align &
                           (GP_PROJECT_DEPTH_VIEW | GP_PROJECT_DEPTH_STROKE)) == 0);
  const bool grid_xray = (v3d->gp_flag & V3D_GP_SHOW_GRID_XRAY);

  if (!ob || (ob->type != OB_GREASE_PENCIL) || !show_grid || !show_overlays) {
    return;
  }
  const float3 base_color = float3(v3d->overlay.gpencil_grid_color);
  const float4 col_grid = float4(base_color, v3d->overlay.gpencil_grid_opacity);

  float4x4 mat = ob->object_to_world();

  const GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);
  if (ts->gp_sculpt.lock_axis != GP_LOCKAXIS_CURSOR && grease_pencil.has_active_layer()) {
    const bke::greasepencil::Layer &layer = *grease_pencil.get_active_layer();
    mat = layer.to_world_space(*ob);
  }
  const View3DCursor *cursor = &scene->cursor;

  /* Set the grid in the selected axis */
  switch (ts->gp_sculpt.lock_axis) {
    case GP_LOCKAXIS_X:
      std::swap(mat[0], mat[2]);
      break;
    case GP_LOCKAXIS_Y:
      std::swap(mat[1], mat[2]);
      break;
    case GP_LOCKAXIS_Z:
      /* Default. */
      break;
    case GP_LOCKAXIS_CURSOR: {
      mat = float4x4(cursor->matrix<float3x3>());
      break;
    }
    case GP_LOCKAXIS_VIEW:
      /* view aligned */
      DRW_view_viewmat_get(nullptr, mat.ptr(), true);
      break;
  }

  if (ts->gpencil_v3d_align & GP_PROJECT_CURSOR) {
    mat.location() = cursor->location;
  }

  /* Note: This is here to match the legacy size. */
  mat.view<3, 3>() *= 2.0f;

  /* Local transform of the grid from the overlay settings. */
  const float3 offset = float3(
      v3d->overlay.gpencil_grid_offset[0], v3d->overlay.gpencil_grid_offset[1], 0.0f);
  const float3 scale = float3(
      v3d->overlay.gpencil_grid_scale[0], v3d->overlay.gpencil_grid_scale[1], 0.0f);
  const float4x4 local_transform = math::from_loc_scale<float4x4>(offset, scale);
  mat = mat * local_transform;

  const int gridlines = v3d->overlay.gpencil_grid_subdivisions;
  const int line_count = gridlines * 4 + 2;

  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA;
  state |= (grid_xray) ? DRW_STATE_DEPTH_ALWAYS : DRW_STATE_DEPTH_LESS_EQUAL;

  DRW_PASS_CREATE(psl->grease_pencil_canvas_ps, state);

  sh = OVERLAY_shader_gpencil_canvas();
  grp = DRW_shgroup_create(sh, psl->grease_pencil_canvas_ps);
  DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
  DRW_shgroup_uniform_vec4_copy(grp, "color", col_grid);
  DRW_shgroup_uniform_vec3_copy(grp, "xAxis", mat[0]);
  DRW_shgroup_uniform_vec3_copy(grp, "yAxis", mat[1]);
  DRW_shgroup_uniform_vec3_copy(grp, "origin", mat[3]);
  DRW_shgroup_uniform_int_copy(grp, "halfLineCount", line_count / 2);
  DRW_shgroup_call_procedural_lines(grp, nullptr, line_count);
}

void OVERLAY_edit_grease_pencil_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  using namespace blender::draw;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();

  DRWShadingGroup *lines_grp = pd->edit_grease_pencil_wires_grp;
  if (lines_grp) {
    blender::gpu::Batch *geom_lines = DRW_cache_grease_pencil_edit_lines_get(draw_ctx->scene, ob);
    if (geom_lines) {
      DRW_shgroup_call_no_cull(lines_grp, geom_lines, ob);
    }
  }

  DRWShadingGroup *points_grp = pd->edit_grease_pencil_points_grp;
  if (points_grp) {
    blender::gpu::Batch *geom_points = DRW_cache_grease_pencil_edit_points_get(draw_ctx->scene,
                                                                               ob);
    if (geom_points) {
      DRW_shgroup_call_no_cull(points_grp, geom_points, ob);
    }
  }

  View3D *v3d = draw_ctx->v3d;
  /* Don't show object extras in set's. */
  if ((ob->base_flag & (BASE_FROM_SET | BASE_FROM_DUPLI)) == 0) {
    if ((v3d->gp_flag & V3D_GP_SHOW_MATERIAL_NAME) && DRW_state_show_text()) {
      OVERLAY_grease_pencil_material_names(ob);
    }
  }
}

void OVERLAY_sculpt_grease_pencil_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  using namespace blender::draw;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();

  DRWShadingGroup *lines_grp = pd->edit_grease_pencil_wires_grp;
  if (lines_grp) {
    blender::gpu::Batch *geom_lines = DRW_cache_grease_pencil_edit_lines_get(draw_ctx->scene, ob);
    if (geom_lines) {
      DRW_shgroup_call_no_cull(lines_grp, geom_lines, ob);
    }
  }

  DRWShadingGroup *points_grp = pd->edit_grease_pencil_points_grp;
  if (points_grp) {
    blender::gpu::Batch *geom_points = DRW_cache_grease_pencil_edit_points_get(draw_ctx->scene,
                                                                               ob);
    if (geom_points) {
      DRW_shgroup_call_no_cull(points_grp, geom_points, ob);
    }
  }
}

void OVERLAY_weight_grease_pencil_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  using namespace blender::draw;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();

  DRWShadingGroup *lines_grp = pd->edit_grease_pencil_wires_grp;
  if (lines_grp) {
    blender::gpu::Batch *geom_lines = DRW_cache_grease_pencil_weight_lines_get(draw_ctx->scene,
                                                                               ob);
    if (geom_lines) {
      DRW_shgroup_call_no_cull(lines_grp, geom_lines, ob);
    }
  }

  DRWShadingGroup *points_grp = pd->edit_grease_pencil_points_grp;
  if (points_grp) {
    blender::gpu::Batch *geom_points = DRW_cache_grease_pencil_weight_points_get(draw_ctx->scene,
                                                                                 ob);
    if (geom_points) {
      DRW_shgroup_call_no_cull(points_grp, geom_points, ob);
    }
  }
}

void OVERLAY_vertex_grease_pencil_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  using namespace blender::draw;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();

  DRWShadingGroup *lines_grp = pd->edit_grease_pencil_wires_grp;
  if (lines_grp) {
    blender::gpu::Batch *geom_lines = DRW_cache_grease_pencil_edit_lines_get(draw_ctx->scene, ob);
    if (geom_lines) {
      DRW_shgroup_call_no_cull(lines_grp, geom_lines, ob);
    }
  }

  DRWShadingGroup *points_grp = pd->edit_grease_pencil_points_grp;
  if (points_grp) {
    blender::gpu::Batch *geom_points = DRW_cache_grease_pencil_edit_points_get(draw_ctx->scene,
                                                                               ob);
    if (geom_points) {
      DRW_shgroup_call_no_cull(points_grp, geom_points, ob);
    }
  }
}

void OVERLAY_grease_pencil_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  if (psl->grease_pencil_canvas_ps) {
    DRW_draw_pass(psl->grease_pencil_canvas_ps);
  }
}

void OVERLAY_edit_grease_pencil_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  if (psl->edit_grease_pencil_ps) {
    DRW_draw_pass(psl->edit_grease_pencil_ps);
  }
}
