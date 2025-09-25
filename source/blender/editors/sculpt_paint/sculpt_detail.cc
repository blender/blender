/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */
#include "sculpt_dyntopo.hh"

#include "MEM_guardedalloc.h"

#include "BLI_index_mask.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.hh"
#include "BLI_string_utf8.h"
#include "BLI_time.h"

#include "BLT_translation.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"

#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_screen.hh"

#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_view3d.hh"

#include "DEG_depsgraph.hh"

#include "sculpt_intern.hh"
#include "sculpt_undo.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"

#include "CLG_log.h"

#include <cstdlib>

#include "bmesh.hh"

namespace blender::ed::sculpt_paint::dyntopo {

static CLG_LogRef LOG = {"sculpt.detail"};

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

struct SculptDetailRaycastData {
  const float *ray_start;
  bool hit;
  float depth;
  float edge_length;

  IsectRayPrecalc isect_precalc;
};

static bool sculpt_and_constant_or_manual_detail_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  return SCULPT_mode_poll(C) && ob->sculpt->bm &&
         (sd->flags & (SCULPT_DYNTOPO_DETAIL_CONSTANT | SCULPT_DYNTOPO_DETAIL_MANUAL));
}

static bool sculpt_and_dynamic_topology_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  return SCULPT_mode_poll(C) && ob->sculpt->bm;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Detail Flood Fill
 * \{ */

static wmOperatorStatus sculpt_detail_flood_fill_exec(bContext *C, wmOperator *op)
{
  const Scene &scene = *CTX_data_scene(C);
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Object &ob = *CTX_data_active_object(C);
  SculptSession &ss = *ob.sculpt;

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();

  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);

  if (nodes.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  node_mask.foreach_index([&](const int i) { BKE_pbvh_node_mark_topology_update(nodes[i]); });

  /* Get the bounding box, its center and size. */
  const Bounds<float3> bounds = bke::pbvh::bounds_get(pbvh);
  const float3 center = math::midpoint(bounds.min, bounds.max);
  const float3 dim = bounds.max - bounds.min;
  const float size = math::reduce_max(dim);

  /* Update topology size. */
  const float max_edge_len = 1.0f /
                             (sd->constant_detail * mat4_to_scale(ob.object_to_world().ptr()));
  const float min_edge_len = max_edge_len * detail_size::EDGE_LENGTH_MIN_FACTOR;

  undo::push_begin(scene, ob, op);
  undo::push_node(depsgraph, ob, nullptr, undo::Type::Position);

  const double start_time = BLI_time_now_seconds();

  while (bke::pbvh::bmesh_update_topology(*ss.bm,
                                          pbvh,
                                          *ss.bm_log,
                                          PBVH_Collapse | PBVH_Subdivide,
                                          min_edge_len,
                                          max_edge_len,
                                          center,
                                          std::nullopt,
                                          size,
                                          false,
                                          false))
  {
    node_mask.foreach_index([&](const int i) { BKE_pbvh_node_mark_topology_update(nodes[i]); });
  }

  CLOG_DEBUG(&LOG, "Detail flood fill took %f seconds.", BLI_time_now_seconds() - start_time);

  undo::push_end(ob);

  /* Force rebuild of bke::pbvh::Tree for better BB placement. */
  BKE_sculptsession_free_pbvh(ob);
  DEG_id_tag_update(&ob.id, ID_RECALC_GEOMETRY);

  /* Redraw. */
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, &ob);

  return OPERATOR_FINISHED;
}

void SCULPT_OT_detail_flood_fill(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Detail Flood Fill";
  ot->idname = "SCULPT_OT_detail_flood_fill";
  ot->description = "Flood fill the mesh with the selected detail setting";

  /* API callbacks. */
  ot->exec = sculpt_detail_flood_fill_exec;
  ot->poll = sculpt_and_constant_or_manual_detail_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sample Detail Size
 * \{ */

enum class SampleDetailModeType {
  Dyntopo = 0,
  Voxel = 1,
};

static EnumPropertyItem prop_sculpt_sample_detail_mode_types[] = {
    {int(SampleDetailModeType::Dyntopo), "DYNTOPO", 0, "Dyntopo", "Sample dyntopo detail"},
    {int(SampleDetailModeType::Voxel), "VOXEL", 0, "Voxel", "Sample mesh voxel size"},
    {0, nullptr, 0, nullptr, nullptr},
};

static bool sample_detail_voxel(bContext *C, ViewContext *vc, const int mval[2])
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object &ob = *vc->obact;
  SculptSession &ss = *ob.sculpt;
  Mesh &mesh = *static_cast<Mesh *>(ob.data);
  const Span<float3> positions = bke::pbvh::vert_positions_eval(*depsgraph, ob);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

  CursorGeometryInfo cgi;

  /* Update the active vertex. */
  const float mval_fl[2] = {float(mval[0]), float(mval[1])};
  if (!cursor_geometry_info_update(C, &cgi, mval_fl, false)) {
    return false;
  }
  BKE_sculpt_update_object_for_edit(depsgraph, &ob, false);

  /* Average the edge length of the connected edges to the active vertex. */
  const int active_vert = std::get<int>(ss.active_vert());
  const float3 active_vert_position = positions[active_vert];
  float edge_length = 0.0f;
  Vector<int> neighbors;
  for (const int neighbor : vert_neighbors_get_mesh(
           faces, corner_verts, vert_to_face_map, hide_poly, active_vert, neighbors))
  {
    edge_length += math::distance(active_vert_position, positions[neighbor]);
  }
  mesh.remesh_voxel_size = edge_length / float(neighbors.size());
  return true;
}

static void sculpt_raycast_detail_cb(bke::pbvh::BMeshNode &node,
                                     SculptDetailRaycastData &srd,
                                     float *tmin)
{
  if (BKE_pbvh_node_get_tmin(&node) < *tmin) {
    if (bke::pbvh::raycast_node_detail_bmesh(
            node, srd.ray_start, &srd.isect_precalc, &srd.depth, &srd.edge_length))
    {
      srd.hit = true;
      *tmin = srd.depth;
    }
  }
}

static void sample_detail_dyntopo(bContext *C, ViewContext *vc, const int mval[2])
{
  Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;
  Object &ob = *vc->obact;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  SCULPT_stroke_modifiers_check(C, ob, brush);

  const float2 mval_fl = {float(mval[0]), float(mval[1])};
  float3 ray_start;
  float3 ray_end;
  float3 ray_normal;
  float depth = raycast_init(vc, mval_fl, ray_start, ray_end, ray_normal, false);

  SculptDetailRaycastData srd;
  srd.hit = false;
  srd.ray_start = ray_start;
  srd.depth = depth;
  srd.edge_length = 0.0f;
  isect_ray_tri_watertight_v3_precalc(&srd.isect_precalc, ray_normal);

  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);

  bke::pbvh::raycast(
      pbvh,
      [&](bke::pbvh::Node &node, float *tmin) {
        sculpt_raycast_detail_cb(static_cast<bke::pbvh::BMeshNode &>(node), srd, tmin);
      },
      ray_start,
      ray_normal,
      false);

  if (srd.hit && srd.edge_length > 0.0f) {
    /* Convert edge length to world space detail resolution. */
    sd.constant_detail = 1 / (srd.edge_length * mat4_to_scale(ob.object_to_world().ptr()));
  }
}

static wmOperatorStatus sample_detail(bContext *C,
                                      const int event_xy[2],
                                      const SampleDetailModeType mode)
{
  /* Find 3D view to pick from. */
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = BKE_screen_find_area_xy(screen, SPACE_VIEW3D, event_xy);
  ARegion *region = (area) ? BKE_area_find_region_xy(area, RGN_TYPE_WINDOW, event_xy) : nullptr;
  if (region == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* Set context to 3D view. */
  ScrArea *prev_area = CTX_wm_area(C);
  ARegion *prev_region = CTX_wm_region(C);
  CTX_wm_area_set(C, area);
  CTX_wm_region_set(C, region);

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);

  Object *ob = vc.obact;
  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bke::pbvh::Tree *pbvh = bke::object::pbvh_get(*ob);
  if (!pbvh) {
    return OPERATOR_CANCELLED;
  }

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  const int mval[2] = {
      event_xy[0] - region->winrct.xmin,
      event_xy[1] - region->winrct.ymin,
  };

  /* Pick sample detail. */
  switch (mode) {
    case SampleDetailModeType::Dyntopo:
      if (pbvh->type() != bke::pbvh::Type::BMesh) {
        CTX_wm_area_set(C, prev_area);
        CTX_wm_region_set(C, prev_region);
        return OPERATOR_CANCELLED;
      }
      sample_detail_dyntopo(C, &vc, mval);
      break;
    case SampleDetailModeType::Voxel:
      if (pbvh->type() != bke::pbvh::Type::Mesh) {
        CTX_wm_area_set(C, prev_area);
        CTX_wm_region_set(C, prev_region);
        return OPERATOR_CANCELLED;
      }
      if (!sample_detail_voxel(C, &vc, mval)) {
        return OPERATOR_CANCELLED;
      }
      break;
  }

  /* Restore context. */
  CTX_wm_area_set(C, prev_area);
  CTX_wm_region_set(C, prev_region);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus sculpt_sample_detail_size_exec(bContext *C, wmOperator *op)
{
  int ss_co[2];
  RNA_int_get_array(op->ptr, "location", ss_co);
  const SampleDetailModeType mode = SampleDetailModeType(RNA_enum_get(op->ptr, "mode"));
  return sample_detail(C, ss_co, mode);
}

static wmOperatorStatus sculpt_sample_detail_size_invoke(bContext *C,
                                                         wmOperator *op,
                                                         const wmEvent * /*event*/)
{
  ED_workspace_status_text(C, IFACE_("Click on the mesh to set the detail"));
  WM_cursor_modal_set(CTX_wm_window(C), WM_CURSOR_EYEDROPPER);
  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus sculpt_sample_detail_size_modal(bContext *C,
                                                        wmOperator *op,
                                                        const wmEvent *event)
{
  switch (event->type) {
    case LEFTMOUSE:
      if (event->val == KM_PRESS) {
        SampleDetailModeType mode = SampleDetailModeType(RNA_enum_get(op->ptr, "mode"));
        sample_detail(C, event->xy, mode);

        RNA_int_set_array(op->ptr, "location", event->xy);
        WM_cursor_modal_restore(CTX_wm_window(C));
        ED_workspace_status_text(C, nullptr);
        WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, nullptr);

        return OPERATOR_FINISHED;
      }
      break;
    case EVT_ESCKEY:
    case RIGHTMOUSE: {
      WM_cursor_modal_restore(CTX_wm_window(C));
      ED_workspace_status_text(C, nullptr);

      return OPERATOR_CANCELLED;
    }
    default: {
      break;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

void SCULPT_OT_sample_detail_size(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Sample Detail Size";
  ot->idname = "SCULPT_OT_sample_detail_size";
  ot->description = "Sample the mesh detail on clicked point";

  /* API callbacks. */
  ot->invoke = sculpt_sample_detail_size_invoke;
  ot->exec = sculpt_sample_detail_size_exec;
  ot->modal = sculpt_sample_detail_size_modal;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop;
  prop = RNA_def_int_array(ot->srna,
                           "location",
                           2,
                           nullptr,
                           0,
                           SHRT_MAX,
                           "Location",
                           "Screen coordinates of sampling",
                           0,
                           SHRT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_enum(ot->srna,
                      "mode",
                      prop_sculpt_sample_detail_mode_types,
                      int(SampleDetailModeType::Dyntopo),
                      "Detail Mode",
                      "Target sculpting workflow that is going to use the sampled size");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dyntopo Detail Size Edit Operator
 * \{ */

/* Defines how much the mouse movement will modify the detail size value. */
#define DETAIL_SIZE_DELTA_SPEED 0.08f
#define DETAIL_SIZE_DELTA_ACCURATE_SPEED 0.004f

enum eDyntopoDetailingMode {
  DETAILING_MODE_RESOLUTION = 0,
  DETAILING_MODE_BRUSH_PERCENT = 1,
  DETAILING_MODE_DETAIL_SIZE = 2
};

struct DyntopoDetailSizeEditCustomData {
  void *draw_handle;
  Object *active_object;

  eDyntopoDetailingMode mode;

  float init_mval[2];
  float accurate_mval[2];

  float outline_col[4];

  bool accurate_mode;
  bool sample_mode;

  /* The values stored here vary based on the detailing mode. */
  float init_value;
  float accurate_value;
  float current_value;

  float radius;

  float brush_radius;
  float pixel_radius;

  float min_value;
  float max_value;

  float preview_tri[3][3];
  float gizmo_mat[4][4];
};

static void dyntopo_detail_size_parallel_lines_draw(uint pos3d,
                                                    DyntopoDetailSizeEditCustomData *cd,
                                                    const float start_co[3],
                                                    const float end_co[3],
                                                    bool flip,
                                                    const float angle)
{
  float object_space_constant_detail;
  if (cd->mode == DETAILING_MODE_RESOLUTION) {
    object_space_constant_detail = detail_size::constant_to_detail_size(cd->current_value,
                                                                        *cd->active_object);
  }
  else if (cd->mode == DETAILING_MODE_BRUSH_PERCENT) {
    object_space_constant_detail = detail_size::brush_to_detail_size(cd->current_value,
                                                                     cd->brush_radius);
  }
  else {
    object_space_constant_detail = detail_size::relative_to_detail_size(
        cd->current_value, cd->brush_radius, cd->pixel_radius, U.pixelsize);
  }

  /* The constant detail represents the maximum edge length allowed before subdividing it. If the
   * triangle grid preview is created with this value it will represent an ideal mesh density where
   * all edges have the exact maximum length, which never happens in practice. As the minimum edge
   * length for dyntopo is 0.4 * max_edge_length, this adjust the detail size to the average
   * between max and min edge length so the preview is more accurate. */
  object_space_constant_detail *= 0.7f;

  const float total_len = len_v3v3(cd->preview_tri[0], cd->preview_tri[1]);
  const int tot_lines = int(total_len / object_space_constant_detail) + 1;
  const float tot_lines_fl = total_len / object_space_constant_detail;
  float spacing_disp[3];
  sub_v3_v3v3(spacing_disp, end_co, start_co);
  normalize_v3(spacing_disp);

  float line_disp[3];
  zero_v3(line_disp);
  rotate_v2_v2fl(line_disp, spacing_disp, DEG2RAD(angle));
  mul_v3_fl(spacing_disp, total_len / tot_lines_fl);

  immBegin(GPU_PRIM_LINES, uint(tot_lines) * 2);
  for (int i = 0; i < tot_lines; i++) {
    float line_length;
    if (flip) {
      line_length = total_len * (float(i) / float(tot_lines_fl));
    }
    else {
      line_length = total_len * (1.0f - (float(i) / float(tot_lines_fl)));
    }
    float line_start[3];
    copy_v3_v3(line_start, start_co);
    madd_v3_v3v3fl(line_start, line_start, spacing_disp, i);
    float line_end[3];
    madd_v3_v3v3fl(line_end, line_start, line_disp, line_length);
    immVertex3fv(pos3d, line_start);
    immVertex3fv(pos3d, line_end);
  }
  immEnd();
}

static void dyntopo_detail_size_edit_draw(const bContext * /*C*/, ARegion * /*region*/, void *arg)
{
  DyntopoDetailSizeEditCustomData *cd = static_cast<DyntopoDetailSizeEditCustomData *>(arg);
  GPU_blend(GPU_BLEND_ALPHA);
  GPU_line_smooth(true);

  uint pos3d = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  GPU_matrix_push();
  GPU_matrix_mul(cd->gizmo_mat);

  /* Draw Cursor */
  immUniformColor4fv(cd->outline_col);
  GPU_line_width(3.0f);

  imm_draw_circle_wire_3d(pos3d, 0, 0, cd->radius, 80);

  /* Draw Triangle. */
  immUniformColor4f(0.9f, 0.9f, 0.9f, 0.8f);
  immBegin(GPU_PRIM_LINES, 6);
  immVertex3fv(pos3d, cd->preview_tri[0]);
  immVertex3fv(pos3d, cd->preview_tri[1]);

  immVertex3fv(pos3d, cd->preview_tri[1]);
  immVertex3fv(pos3d, cd->preview_tri[2]);

  immVertex3fv(pos3d, cd->preview_tri[2]);
  immVertex3fv(pos3d, cd->preview_tri[0]);
  immEnd();

  /* Draw Grid */
  GPU_line_width(1.0f);
  dyntopo_detail_size_parallel_lines_draw(
      pos3d, cd, cd->preview_tri[0], cd->preview_tri[1], false, 60.0f);
  dyntopo_detail_size_parallel_lines_draw(
      pos3d, cd, cd->preview_tri[0], cd->preview_tri[1], true, 120.0f);
  dyntopo_detail_size_parallel_lines_draw(
      pos3d, cd, cd->preview_tri[0], cd->preview_tri[2], false, -60.0f);

  immUnbindProgram();
  GPU_matrix_pop();
  GPU_blend(GPU_BLEND_NONE);
  GPU_line_smooth(false);
}

static void dyntopo_detail_size_edit_cancel(bContext *C, wmOperator *op)
{
  Object *active_object = CTX_data_active_object(C);
  SculptSession &ss = *active_object->sculpt;
  ARegion *region = CTX_wm_region(C);
  DyntopoDetailSizeEditCustomData *cd = static_cast<DyntopoDetailSizeEditCustomData *>(
      op->customdata);
  ED_region_draw_cb_exit(region->runtime->type, cd->draw_handle);
  ss.draw_faded_cursor = false;
  MEM_freeN(cd);
  op->customdata = nullptr;
  ED_workspace_status_text(C, nullptr);

  ScrArea *area = CTX_wm_area(C);
  ED_area_status_text(area, nullptr);
}

static void dyntopo_detail_size_bounds(DyntopoDetailSizeEditCustomData *cd)
{
  /* TODO: Get range from RNA for these values? */
  if (cd->mode == DETAILING_MODE_RESOLUTION) {
    cd->min_value = 1.0f;
    cd->max_value = 500.0f;
  }
  else if (cd->mode == DETAILING_MODE_BRUSH_PERCENT) {
    cd->min_value = 0.5f;
    cd->max_value = 100.0f;
  }
  else {
    cd->min_value = 0.5f;
    cd->max_value = 40.0f;
  }
}

static void dyntopo_detail_size_sample_from_surface(Object &ob,
                                                    DyntopoDetailSizeEditCustomData *cd)
{
  SculptSession &ss = *ob.sculpt;
  const ActiveVert active_vert = ss.active_vert();
  if (std::holds_alternative<std::monostate>(active_vert)) {
    return;
  }
  BMVert *active_vertex = std::get<BMVert *>(active_vert);

  float len_accum = 0;
  BMeshNeighborVerts neighbors;
  for (BMVert *neighbor : vert_neighbors_get_bmesh(*active_vertex, neighbors)) {
    len_accum += len_v3v3(active_vertex->co, neighbor->co);
  }
  const int num_neighbors = neighbors.size();

  if (num_neighbors > 0) {
    const float avg_edge_len = len_accum / num_neighbors;
    /* Use 0.7 as the average of min and max dyntopo edge length. */
    const float detail_size = 0.7f / (avg_edge_len *
                                      mat4_to_scale(cd->active_object->object_to_world().ptr()));
    float sampled_value;
    if (cd->mode == DETAILING_MODE_RESOLUTION) {
      sampled_value = detail_size;
    }
    else if (cd->mode == DETAILING_MODE_BRUSH_PERCENT) {
      sampled_value = detail_size::constant_to_brush_detail(
          detail_size, cd->brush_radius, *cd->active_object);
    }
    else {
      sampled_value = detail_size::constant_to_relative_detail(
          detail_size, cd->brush_radius, cd->pixel_radius, U.pixelsize, *cd->active_object);
    }
    cd->current_value = clamp_f(sampled_value, cd->min_value, cd->max_value);
  }
}

static void dyntopo_detail_size_update_from_mouse_delta(DyntopoDetailSizeEditCustomData *cd,
                                                        const wmEvent *event)
{
  const float mval[2] = {float(event->mval[0]), float(event->mval[1])};

  float detail_size_delta;
  float invert = cd->mode == DETAILING_MODE_RESOLUTION ? 1.0f : -1.0f;
  if (cd->accurate_mode) {
    detail_size_delta = mval[0] - cd->accurate_mval[0];
    cd->current_value = cd->accurate_value +
                        detail_size_delta * DETAIL_SIZE_DELTA_ACCURATE_SPEED * invert;
  }
  else {
    detail_size_delta = mval[0] - cd->init_mval[0];
    cd->current_value = cd->init_value + detail_size_delta * DETAIL_SIZE_DELTA_SPEED * invert;
  }

  if (event->type == EVT_LEFTSHIFTKEY && event->val == KM_PRESS) {
    cd->accurate_mode = true;
    copy_v2_v2(cd->accurate_mval, mval);
    cd->accurate_value = cd->current_value;
  }
  if (event->type == EVT_LEFTSHIFTKEY && event->val == KM_RELEASE) {
    cd->accurate_mode = false;
    cd->accurate_value = 0.0f;
  }

  cd->current_value = clamp_f(cd->current_value, cd->min_value, cd->max_value);
}

static void dyntopo_detail_size_update_header(bContext *C,
                                              const DyntopoDetailSizeEditCustomData *cd)
{
  Scene *scene = CTX_data_scene(C);

  Sculpt *sd = scene->toolsettings->sculpt;
  PointerRNA sculpt_ptr = RNA_pointer_create_discrete(&scene->id, &RNA_Sculpt, sd);

  char msg[UI_MAX_DRAW_STR];
  const char *format_string;
  const char *property_name;
  if (cd->mode == DETAILING_MODE_RESOLUTION) {
    property_name = "constant_detail_resolution";
    format_string = "%s: %0.4f";
  }
  else if (cd->mode == DETAILING_MODE_BRUSH_PERCENT) {
    property_name = "detail_percent";
    format_string = "%s: %3.1f%%";
  }
  else {
    property_name = "detail_size";
    format_string = "%s: %0.4f";
  }
  const PropertyRNA *prop = RNA_struct_find_property(&sculpt_ptr, property_name);
  const char *ui_name = RNA_property_ui_name(prop);
  SNPRINTF_UTF8(msg, format_string, ui_name, cd->current_value);
  ScrArea *area = CTX_wm_area(C);
  ED_area_status_text(area, msg);

  WorkspaceStatus status(C);
  status.item(IFACE_("Confirm"), ICON_EVENT_RETURN, ICON_MOUSE_LMB);
  status.item(IFACE_("Cancel"), ICON_EVENT_ESC, ICON_MOUSE_RMB);
  status.item(IFACE_("Change Size"), ICON_MOUSE_MOVE);
  status.item_bool(IFACE_("Sample Mode"), cd->sample_mode, ICON_EVENT_CTRL);
  status.item_bool(IFACE_("Precision Mode"), cd->accurate_mode, ICON_EVENT_SHIFT);
}

static wmOperatorStatus dyntopo_detail_size_edit_modal(bContext *C,
                                                       wmOperator *op,
                                                       const wmEvent *event)
{
  Object &active_object = *CTX_data_active_object(C);
  SculptSession &ss = *active_object.sculpt;
  ARegion *region = CTX_wm_region(C);
  DyntopoDetailSizeEditCustomData *cd = static_cast<DyntopoDetailSizeEditCustomData *>(
      op->customdata);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  /* Cancel modal operator */
  if ((event->type == EVT_ESCKEY && event->val == KM_PRESS) ||
      (event->type == RIGHTMOUSE && event->val == KM_PRESS))
  {
    dyntopo_detail_size_edit_cancel(C, op);
    ED_region_tag_redraw(region);
    return OPERATOR_FINISHED;
  }

  /* Finish modal operator */
  if ((event->type == LEFTMOUSE && event->val == KM_RELEASE) ||
      (event->type == EVT_RETKEY && event->val == KM_PRESS) ||
      (event->type == EVT_PADENTER && event->val == KM_PRESS))
  {
    ED_region_draw_cb_exit(region->runtime->type, cd->draw_handle);
    if (cd->mode == DETAILING_MODE_RESOLUTION) {
      sd->constant_detail = cd->current_value;
    }
    else if (cd->mode == DETAILING_MODE_BRUSH_PERCENT) {
      sd->detail_percent = cd->current_value;
    }
    else {
      sd->detail_size = cd->current_value;
    }

    ss.draw_faded_cursor = false;
    MEM_freeN(cd);
    ED_region_tag_redraw(region);
    ED_workspace_status_text(C, nullptr);

    ScrArea *area = CTX_wm_area(C);
    ED_area_status_text(area, nullptr);
    return OPERATOR_FINISHED;
  }

  ED_region_tag_redraw(region);

  if (ELEM(event->type, EVT_LEFTCTRLKEY, EVT_RIGHTCTRLKEY)) {
    if (event->val == KM_PRESS) {
      cd->sample_mode = true;
    }
    else if (event->val == KM_RELEASE) {
      cd->sample_mode = false;
    }
  }

  /* Sample mode sets the detail size sampling the average edge length under the surface. */
  if (cd->sample_mode) {
    dyntopo_detail_size_sample_from_surface(active_object, cd);
    dyntopo_detail_size_update_header(C, cd);
    return OPERATOR_RUNNING_MODAL;
  }
  /* Regular mode, changes the detail size by moving the cursor. */
  dyntopo_detail_size_update_from_mouse_delta(cd, event);
  dyntopo_detail_size_update_header(C, cd);

  return OPERATOR_RUNNING_MODAL;
}

static float dyntopo_detail_size_initial_value(const Sculpt *sd, const eDyntopoDetailingMode mode)
{
  if (mode == DETAILING_MODE_RESOLUTION) {
    return sd->constant_detail;
  }
  if (mode == DETAILING_MODE_BRUSH_PERCENT) {
    return sd->detail_percent;
  }
  return sd->detail_size;
}

static wmOperatorStatus dyntopo_detail_size_edit_invoke(bContext *C,
                                                        wmOperator *op,
                                                        const wmEvent *event)
{
  const ToolSettings *tool_settings = CTX_data_tool_settings(C);
  Sculpt *sd = tool_settings->sculpt;

  ARegion *region = CTX_wm_region(C);
  Object &active_object = *CTX_data_active_object(C);
  Brush *brush = BKE_paint_brush(&sd->paint);

  DyntopoDetailSizeEditCustomData *cd = MEM_callocN<DyntopoDetailSizeEditCustomData>(__func__);

  /* Initial operator Custom Data setup. */
  cd->draw_handle = ED_region_draw_cb_activate(
      region->runtime->type, dyntopo_detail_size_edit_draw, cd, REGION_DRAW_POST_VIEW);
  cd->active_object = &active_object;
  cd->init_mval[0] = event->mval[0];
  cd->init_mval[1] = event->mval[1];
  if (sd->flags & (SCULPT_DYNTOPO_DETAIL_CONSTANT | SCULPT_DYNTOPO_DETAIL_MANUAL)) {
    cd->mode = DETAILING_MODE_RESOLUTION;
  }
  else if (sd->flags & SCULPT_DYNTOPO_DETAIL_BRUSH) {
    cd->mode = DETAILING_MODE_BRUSH_PERCENT;
  }
  else {
    cd->mode = DETAILING_MODE_DETAIL_SIZE;
  }

  const float initial_detail_size = dyntopo_detail_size_initial_value(sd, cd->mode);
  cd->current_value = initial_detail_size;
  cd->init_value = initial_detail_size;
  copy_v4_v4(cd->outline_col, brush->add_col);
  op->customdata = cd;

  SculptSession &ss = *active_object.sculpt;
  dyntopo_detail_size_bounds(cd);
  cd->radius = ss.cursor_radius;

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);

  cd->brush_radius = object_space_radius_get(vc, sd->paint, *brush, ss.cursor_location);
  cd->pixel_radius = BKE_brush_radius_get(&sd->paint, brush);

  /* Generates the matrix to position the gizmo in the surface of the mesh using the same
   * location and orientation as the brush cursor. */
  float cursor_trans[4][4], cursor_rot[4][4];
  const float z_axis[4] = {0.0f, 0.0f, 1.0f, 0.0f};
  float quat[4];
  copy_m4_m4(cursor_trans, active_object.object_to_world().ptr());
  translate_m4(cursor_trans, ss.cursor_location[0], ss.cursor_location[1], ss.cursor_location[2]);

  float cursor_normal[3];
  if (ss.cursor_sampled_normal) {
    copy_v3_v3(cursor_normal, *ss.cursor_sampled_normal);
  }
  else {
    copy_v3_v3(cursor_normal, ss.cursor_normal);
  }

  rotation_between_vecs_to_quat(quat, z_axis, cursor_normal);
  quat_to_mat4(cursor_rot, quat);
  copy_m4_m4(cd->gizmo_mat, cursor_trans);
  mul_m4_m4_post(cd->gizmo_mat, cursor_rot);

  /* Initialize the position of the triangle vertices. */
  const float y_axis[3] = {0.0f, cd->radius, 0.0f};
  for (int i = 0; i < 3; i++) {
    zero_v3(cd->preview_tri[i]);
    rotate_v2_v2fl(cd->preview_tri[i], y_axis, DEG2RAD(120.0f * i));
  }

  vert_random_access_ensure(active_object);

  WM_event_add_modal_handler(C, op);
  ED_region_tag_redraw(region);

  ss.draw_faded_cursor = true;

  const char *status_str = IFACE_(
      "Move the mouse to change the dyntopo detail size. LMB: confirm size, ESC/RMB: cancel, "
      "SHIFT: precision mode, CTRL: sample detail size");

  ED_workspace_status_text(C, status_str);
  dyntopo_detail_size_update_header(C, cd);

  return OPERATOR_RUNNING_MODAL;
}

void SCULPT_OT_dyntopo_detail_size_edit(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Edit Dyntopo Detail Size";
  ot->description = "Modify the detail size of dyntopo interactively";
  ot->idname = "SCULPT_OT_dyntopo_detail_size_edit";

  /* API callbacks. */
  ot->poll = sculpt_and_dynamic_topology_poll;
  ot->invoke = dyntopo_detail_size_edit_invoke;
  ot->modal = dyntopo_detail_size_edit_modal;
  ot->cancel = dyntopo_detail_size_edit_cancel;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

}  // namespace blender::ed::sculpt_paint::dyntopo

namespace blender::ed::sculpt_paint::dyntopo::detail_size {

float constant_to_detail_size(const float constant_detail, const Object &ob)
{
  return 1.0f / (constant_detail * mat4_to_scale(ob.object_to_world().ptr()));
}

float brush_to_detail_size(const float brush_percent, const float brush_radius)
{
  return brush_radius * brush_percent / 100.0f;
}

float relative_to_detail_size(const float relative_detail,
                              const float brush_radius,
                              const float pixel_radius,
                              const float pixel_size)
{
  return (brush_radius / pixel_radius) * (relative_detail * pixel_size) / RELATIVE_SCALE_FACTOR;
}

float constant_to_brush_detail(const float constant_detail,
                               const float brush_radius,
                               const Object &ob)
{
  const float object_scale = mat4_to_scale(ob.object_to_world().ptr());

  return 100.0f / (constant_detail * brush_radius * object_scale);
}

float constant_to_relative_detail(const float constant_detail,
                                  const float brush_radius,
                                  const float pixel_radius,
                                  const float pixel_size,
                                  const Object &ob)
{
  const float object_scale = mat4_to_scale(ob.object_to_world().ptr());

  return (pixel_radius / brush_radius) * (RELATIVE_SCALE_FACTOR / pixel_size) *
         (1.0f / (constant_detail * object_scale));
}

}  // namespace blender::ed::sculpt_paint::dyntopo::detail_size

/** \} */
