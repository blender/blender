/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"

#include "BKE_context.h"
#include "BKE_dyntopo.hh"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_view3d.h"
#include "sculpt_intern.hh"

#include "RNA_access.h"
#include "RNA_define.h"

#include "PIL_time.h"

#include <cmath>
#include <cstdlib>

#include <string>

using blender::float3;

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

struct SculptDetailRaycastData {
  const float *ray_start;
  bool hit;
  float depth;
  float edge_length;

  struct IsectRayPrecalc isect_precalc;
  SculptSession *ss;
};

static bool sculpt_and_constant_or_manual_detail_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  // Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  /*checking for constant/manual mode isn't necassary since we do this on the python side
    in the ui scripts*/
  return SCULPT_mode_poll(C) && ob->sculpt->bm; /*&&
         (sd->flags & (SCULPT_DYNTOPO_DETAIL_CONSTANT | SCULPT_DYNTOPO_DETAIL_MANUAL));*/
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
static int sculpt_detail_flood_fill_run(Object *ob, Sculpt *sd, Brush *brush, wmOperator * /*op*/)
{
  using namespace blender::bke::dyntopo;

  SculptSession *ss = ob->sculpt;

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr, nullptr);

  if (nodes.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  SCULPT_apply_dyntopo_settings(ss, sd, brush);
  float detail_range = ss->cached_dyntopo.detail_range;

  /* Update topology size. */
  float object_space_constant_detail = 1.0f / (ss->cached_dyntopo.constant_detail *
                                               mat4_to_scale(ob->object_to_world));
  blender::bke::dyntopo::detail_size_set(ss->pbvh, object_space_constant_detail, detail_range);
  BKE_pbvh_set_bm_log(ss->pbvh, ss->bm_log);

  DyntopoMaskCB mask_cb;
  void *mask_cb_data;

  SCULPT_dyntopo_automasking_init(ss, sd, nullptr, ob, &mask_cb, &mask_cb_data);

  PBVHTopologyUpdateMode mode = PBVHTopologyUpdateMode(0);
  if (ss->cached_dyntopo.flag & DYNTOPO_SUBDIVIDE) {
    mode |= PBVH_Subdivide;
  }
  if (ss->cached_dyntopo.flag & DYNTOPO_COLLAPSE) {
    mode |= PBVH_Collapse;
  }
  if (ss->cached_dyntopo.flag & DYNTOPO_CLEANUP) {
    mode |= PBVH_Cleanup;
  }

  double time = PIL_check_seconds_timer();
  int edge_step_mul = 1 + int(ss->cached_dyntopo.quality * 100.0f);

  for (int i = 0; i < 1 + ss->cached_dyntopo.repeat; i++) {
    nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr, nullptr);

    for (int j = 0; j < nodes.size(); j++) {
      BKE_pbvh_node_mark_topology_update(nodes[j]);
    }

    blender::bke::dyntopo::BrushNoRadius brush_tester;
    remesh_topology(&brush_tester,
                    ss,
                    ss->pbvh,
                    mode,
                    false,
                    float3(0.0f, 0.0f, 1.0f),
                    false,
                    mask_cb,
                    mask_cb_data,
                    edge_step_mul);
  }

  time = (PIL_check_seconds_timer() - time) * 1000.0;
  printf("    Time: %.3fms\n", float(time));

  SCULPT_dyntopo_automasking_end(mask_cb_data);

  after_stroke(ss->pbvh, true);
  DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);

  return OPERATOR_FINISHED;
}

static int sculpt_detail_flood_fill_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, false);

  SCULPT_undo_push_begin(ob, op);
  SCULPT_undo_push_node(ob, nullptr, SCULPT_UNDO_COORDS);

  int ret = sculpt_detail_flood_fill_run(
      CTX_data_active_object(C), sd, BKE_paint_brush(&sd->paint), op);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  SCULPT_undo_push_end(ob);

  return ret;
}

struct FloodFillJob {
  wmJob *job;
  Object *ob;
  Depsgraph *depsgraph;
  Scene *scene;
  wmOperator *op;
  bContext *C;
  Brush *brush;
  Sculpt *sd;
};

static FloodFillJob flood_fill_job;

static void start_fill_job(void * /*custom_data*/,
                           bool *stop,
                           bool *do_update,
                           float * /*progress*/)
{
  printf("Start detail fill job.\n");

  while (true) {
    if (*stop) {
      break;
    }

    WM_job_main_thread_lock_acquire(flood_fill_job.job);

    if (sculpt_detail_flood_fill_run(
            flood_fill_job.ob, flood_fill_job.sd, flood_fill_job.brush, flood_fill_job.op) ==
        OPERATOR_CANCELLED)
    {
      WM_job_main_thread_lock_release(flood_fill_job.job);
      break;
    }

    WM_job_main_thread_lock_release(flood_fill_job.job);

    *do_update = true;

    PIL_sleep_ms(50);
  }

  printf("\nJob finished\n\n");
}

static void end_fill_job(void *)
{
  SCULPT_undo_push_end(flood_fill_job.ob);

  printf("End fill job\n");
}

static void flood_fill_free(void *customdata)
{
  printf("%s: detail flood fill free.\n", __func__);
}

int sculpt_detail_flood_fill_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  ED_workspace_status_text(C, TIP_(""));

  if (RNA_boolean_get(op->ptr, "interactive")) {
    Object *ob = CTX_data_active_object(C);

    BKE_sculpt_update_object_for_edit(
        CTX_data_ensure_evaluated_depsgraph(C), ob, true, false, false);

    SCULPT_undo_push_begin(ob, op);
    SCULPT_undo_push_node(ob, nullptr, SCULPT_UNDO_COORDS);

    flood_fill_job.sd = CTX_data_tool_settings(C)->sculpt;
    flood_fill_job.brush = BKE_paint_brush(&flood_fill_job.sd->paint);
    flood_fill_job.ob = CTX_data_active_object(C);
    flood_fill_job.op = op;
    flood_fill_job.C = C;
    flood_fill_job.scene = CTX_data_scene(C);
    flood_fill_job.depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

    flood_fill_job.job = WM_jobs_get(CTX_wm_manager(C),
                                     CTX_wm_window(C),
                                     static_cast<void *>(&flood_fill_job),
                                     "Dyntopo Flood Fill",
                                     WM_JOB_PROGRESS,
                                     WM_JOB_TYPE_ANY);

    WM_jobs_callbacks(flood_fill_job.job, start_fill_job, nullptr, nullptr, end_fill_job);
    WM_jobs_timer(flood_fill_job.job, 0.5, NC_OBJECT | ND_DRAW, NC_OBJECT | ND_DRAW);
    WM_jobs_customdata_set(flood_fill_job.job, &flood_fill_job, flood_fill_free);

    WM_jobs_start(CTX_wm_manager(C), flood_fill_job.job);

    WM_event_add_modal_handler(C, op);
    return OPERATOR_RUNNING_MODAL;
  }
  else {
    sculpt_detail_flood_fill_exec(C, op);
    return OPERATOR_FINISHED;
  }
}

static int sculpt_sample_flood_fill_modal(bContext *C, wmOperator * /*op*/, const wmEvent *event)
{
  switch (event->type) {
    case EVT_ESCKEY:
    case EVT_RETKEY:
      WM_jobs_kill(CTX_wm_manager(C), &flood_fill_job, start_fill_job);
      return OPERATOR_FINISHED;
  }

  return OPERATOR_RUNNING_MODAL;
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
  ot->invoke = sculpt_detail_flood_fill_invoke;
  ot->modal = sculpt_sample_flood_fill_modal;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "interactive", true, "Interactive", "Interactive mode");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sample Detail Size
 * \{ */

typedef enum eSculptSampleDetailModeTypes {
  SAMPLE_DETAIL_DYNTOPO = 0,
  SAMPLE_DETAIL_VOXEL = 1,
} eSculptSampleDetailModeTypes;

static EnumPropertyItem prop_sculpt_sample_detail_mode_types[] = {
    {SAMPLE_DETAIL_DYNTOPO, "DYNTOPO", 0, "Dyntopo", "Sample dyntopo detail"},
    {SAMPLE_DETAIL_VOXEL, "VOXEL", 0, "Voxel", "Sample mesh voxel size"},
    {0, nullptr, 0, nullptr, nullptr},
};

static void sample_detail_voxel(bContext *C, ViewContext *vc, const int mval[2])
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = vc->obact;
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  SculptSession *ss = ob->sculpt;
  SculptCursorGeometryInfo sgi;
  SCULPT_vertex_random_access_ensure(ss);

  /* Update the active vertex. */
  const float mval_fl[2] = {float(mval[0]), float(mval[1])};
  SCULPT_cursor_geometry_info_update(C, &sgi, mval_fl, false, false);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, false);

  /* Average the edge length of the connected edges to the active vertex. */
  PBVHVertRef active_vertex = SCULPT_active_vertex_get(ss);
  const float *active_vertex_co = SCULPT_active_vertex_co_get(ss);
  float edge_length = 0.0f;
  int tot = 0;
  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, active_vertex, ni) {
    edge_length += len_v3v3(active_vertex_co, SCULPT_vertex_co_get(ss, ni.vertex));
    tot += 1;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  if (tot > 0) {
    mesh->remesh_voxel_size = edge_length / float(tot);
  }
}

static void sculpt_raycast_detail_cb(PBVHNode *node, void *data_v, float *tmin)
{
  if (BKE_pbvh_node_get_tmin(node) < *tmin) {
    SculptDetailRaycastData *srd = static_cast<SculptDetailRaycastData *>(data_v);

    if (BKE_pbvh_bmesh_node_raycast_detail(srd->ss->pbvh,
                                           node,
                                           srd->ray_start,
                                           &srd->isect_precalc,
                                           &srd->depth,
                                           &srd->edge_length))
    {
      srd->hit = true;
      *tmin = srd->depth;
    }
  }
}

static void sample_detail_dyntopo(bContext *C, ViewContext *vc, const int mval[2])
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Object *ob = vc->obact;
  Brush *brush = BKE_paint_brush(&sd->paint);

  SCULPT_stroke_modifiers_check(C, ob, brush);

  const float mval_fl[2] = {float(mval[0]), float(mval[1])};
  float ray_start[3], ray_end[3], ray_normal[3];
  float depth = SCULPT_raycast_init(vc, mval_fl, ray_start, ray_end, ray_normal, false);

  SculptDetailRaycastData srd;
  srd.hit = 0;
  srd.ss = ob->sculpt;

  srd.ray_start = ray_start;
  srd.depth = depth;
  srd.edge_length = 0.0f;
  isect_ray_tri_watertight_v3_precalc(&srd.isect_precalc, ray_normal);

  BKE_pbvh_raycast(ob->sculpt->pbvh,
                   sculpt_raycast_detail_cb,
                   &srd,
                   ray_start,
                   ray_normal,
                   false,
                   srd.ss->stroke_id);

  if (srd.hit && srd.edge_length > 0.0f) {
    DynTopoSettings *dyntopo = brush->dyntopo.inherit & DYNTOPO_INHERIT_CONSTANT_DETAIL ?
                                   &sd->dyntopo :
                                   &brush->dyntopo;

    /* Convert edge length to world space detail resolution. */
    dyntopo->constant_detail = 1.0f / (srd.edge_length * mat4_to_scale(ob->object_to_world));
  }
}

static int sample_detail(bContext *C, const int event_xy[2], int mode)
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
  ViewContext vc;
  ED_view3d_viewcontext_init(C, &vc, depsgraph);

  Object *ob = vc.obact;
  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  SculptSession *ss = ob->sculpt;
  if (!ss->pbvh) {
    return OPERATOR_CANCELLED;
  }

  const int mval[2] = {
      event_xy[0] - region->winrct.xmin,
      event_xy[1] - region->winrct.ymin,
  };

  /* Pick sample detail. */
  switch (mode) {
    case SAMPLE_DETAIL_DYNTOPO:
      if (BKE_pbvh_type(ss->pbvh) != PBVH_BMESH) {
        CTX_wm_area_set(C, prev_area);
        CTX_wm_region_set(C, prev_region);
        return OPERATOR_CANCELLED;
      }
      sample_detail_dyntopo(C, &vc, mval);
      break;
    case SAMPLE_DETAIL_VOXEL:
      if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
        CTX_wm_area_set(C, prev_area);
        CTX_wm_region_set(C, prev_region);
        return OPERATOR_CANCELLED;
      }
      sample_detail_voxel(C, &vc, mval);
      break;
  }

  /* Restore context. */
  CTX_wm_area_set(C, prev_area);
  CTX_wm_region_set(C, prev_region);

  return OPERATOR_FINISHED;
}

static int sculpt_sample_detail_size_exec(bContext *C, wmOperator *op)
{
  int ss_co[2];
  RNA_int_get_array(op->ptr, "location", ss_co);
  int mode = RNA_enum_get(op->ptr, "mode");
  return sample_detail(C, ss_co, mode);
}

static int sculpt_sample_detail_size_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  ED_workspace_status_text(C, TIP_("Click on the mesh to set the detail"));
  WM_cursor_modal_set(CTX_wm_window(C), WM_CURSOR_EYEDROPPER);
  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_sample_detail_size_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  switch (event->type) {
    case LEFTMOUSE:
      if (event->val == KM_PRESS) {
        int mode = RNA_enum_get(op->ptr, "mode");
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

  RNA_def_int_array(ot->srna,
                    "location",
                    2,
                    nullptr,
                    0,
                    SHRT_MAX,
                    "Location",
                    "Screen coordinates of sampling",
                    0,
                    SHRT_MAX);
  RNA_def_enum(ot->srna,
               "mode",
               prop_sculpt_sample_detail_mode_types,
               SAMPLE_DETAIL_DYNTOPO,
               "Detail Mode",
               "Target sculpting workflow that is going to use the sampled size");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dynamic-topology detail size
 *
 * Currently, there are two operators editing the detail size:
 * - #SCULPT_OT_set_detail_size uses radial control for all methods
 * - #SCULPT_OT_dyntopo_detail_size_edit shows a triangle grid representation of the detail
 *   resolution (for constant detail method,
 *   falls back to radial control for the remaining methods).
 * \{ */

static void sculpt_detail_size_set_radial_control(bContext *C)
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  PointerRNA props_ptr;
  wmOperatorType *ot = WM_operatortype_find("WM_OT_radial_control", true);

  WM_operator_properties_create_ptr(&props_ptr, ot);

  int mode = brush->dyntopo.inherit & DYNTOPO_INHERIT_MODE ? sd->dyntopo.mode :
                                                             brush->dyntopo.mode;
  std::string base = "tool_settings.sculpt";
  if (!(brush->dyntopo.inherit & DYNTOPO_INHERIT_MODE)) {
    base += ".brush";
  }

  base += ".dyntopo";

  if (ELEM(mode, DYNTOPO_DETAIL_MANUAL, DYNTOPO_DETAIL_CONSTANT)) {
    base += ".constant_detail";
    RNA_string_set(&props_ptr, "data_path_primary", base.c_str());
  }
  else if (mode == DYNTOPO_DETAIL_BRUSH) {
    base += ".detail_percent";
    RNA_string_set(&props_ptr, "data_path_primary", base.c_str());
  }
  else { /* Relative mode. */
    base += ".detail_size";
    RNA_string_set(&props_ptr, "data_path_primary", base.c_str());
  }

  WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &props_ptr, nullptr);

  WM_operator_properties_free(&props_ptr);
}

static int sculpt_set_detail_size_exec(bContext *C, wmOperator * /*op*/)
{
  sculpt_detail_size_set_radial_control(C);

  return OPERATOR_FINISHED;
}

void SCULPT_OT_set_detail_size(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Set Detail Size";
  ot->idname = "SCULPT_OT_set_detail_size";
  ot->description =
      "Set the mesh detail (either relative or constant one, depending on current dyntopo mode)";

  /* API callbacks. */
  ot->exec = sculpt_set_detail_size_exec;
  ot->poll = sculpt_and_dynamic_topology_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dyntopo Detail Size Edit Operator
 * \{ */

/* Defines how much the mouse movement will modify the detail size value. */
#define DETAIL_SIZE_DELTA_SPEED 0.08f
#define DETAIL_SIZE_DELTA_ACCURATE_SPEED 0.004f

struct DyntopoDetailSizeEditCustomData {
  void *draw_handle;
  Object *active_object;

  float init_mval[2];
  float accurate_mval[2];

  float outline_col[4];

  bool accurate_mode;
  bool sample_mode;

  float init_detail_size;
  float accurate_detail_size;
  float detail_size;
  float detail_range;
  float radius;

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
  float object_space_constant_detail = 1.0f / (cd->detail_size *
                                               mat4_to_scale(cd->active_object->object_to_world));

  /* The constant detail represents the maximum edge length allowed before subdividing it. If the
   * triangle grid preview is created with this value it will represent an ideal mesh density
   * where all edges have the exact maximum length, which never happens in practice. As the
   * minimum edge length for dyntopo is 0.4 * max_edge_length, this adjust the detail size to the
   * average between max and min edge length so the preview is more accurate. */
  object_space_constant_detail *= 1.0f - cd->detail_range * 0.5f;

  const float total_len = len_v3v3(cd->preview_tri[0], cd->preview_tri[1]);
  const int tot_lines = int(total_len / object_space_constant_detail) + 1;
  const float tot_lines_fl = total_len / object_space_constant_detail;
  float spacing_disp[3];
  sub_v3_v3v3(spacing_disp, end_co, start_co);
  normalize_v3(spacing_disp);

  float line_disp[3];
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

  uint pos3d = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
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
  SculptSession *ss = active_object->sculpt;
  ARegion *region = CTX_wm_region(C);
  DyntopoDetailSizeEditCustomData *cd = static_cast<DyntopoDetailSizeEditCustomData *>(
      op->customdata);
  ED_region_draw_cb_exit(region->type, cd->draw_handle);
  ss->draw_faded_cursor = false;
  MEM_freeN(op->customdata);
  ED_workspace_status_text(C, nullptr);
}

static void dyntopo_detail_size_sample_from_surface(Object *ob,
                                                    DyntopoDetailSizeEditCustomData *cd)
{
  SculptSession *ss = ob->sculpt;
  const PBVHVertRef active_vertex = SCULPT_active_vertex_get(ss);

  float len_accum = 0;
  int num_neighbors = 0;
  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, active_vertex, ni) {
    len_accum += len_v3v3(SCULPT_vertex_co_get(ss, active_vertex),
                          SCULPT_vertex_co_get(ss, ni.vertex));
    num_neighbors++;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  if (num_neighbors > 0) {
    const float avg_edge_len = len_accum / num_neighbors;
    /* Use 0.7 as the average of min and max dyntopo edge length. */
    const float detail_size = 0.7f /
                              (avg_edge_len * mat4_to_scale(cd->active_object->object_to_world));
    cd->detail_size = clamp_f(detail_size, 1.0f, 500.0f);
  }
}

static void dyntopo_detail_size_update_from_mouse_delta(DyntopoDetailSizeEditCustomData *cd,
                                                        const wmEvent *event)
{
  const float mval[2] = {float(event->mval[0]), float(event->mval[1])};

  float detail_size_delta;
  if (cd->accurate_mode) {
    detail_size_delta = mval[0] - cd->accurate_mval[0];
    cd->detail_size = cd->accurate_detail_size +
                      detail_size_delta * DETAIL_SIZE_DELTA_ACCURATE_SPEED;
  }
  else {
    detail_size_delta = mval[0] - cd->init_mval[0];
    cd->detail_size = cd->init_detail_size + detail_size_delta * DETAIL_SIZE_DELTA_SPEED;
  }

  if (event->type == EVT_LEFTSHIFTKEY && event->val == KM_PRESS) {
    cd->accurate_mode = true;
    copy_v2_v2(cd->accurate_mval, mval);
    cd->accurate_detail_size = cd->detail_size;
  }
  if (event->type == EVT_LEFTSHIFTKEY && event->val == KM_RELEASE) {
    cd->accurate_mode = false;
    cd->accurate_detail_size = 0.0f;
  }

  cd->detail_size = clamp_f(cd->detail_size, 1.0f, 500.0f);
}

static int dyntopo_detail_size_edit_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *active_object = CTX_data_active_object(C);
  SculptSession *ss = active_object->sculpt;
  ARegion *region = CTX_wm_region(C);
  DyntopoDetailSizeEditCustomData *cd = static_cast<DyntopoDetailSizeEditCustomData *>(
      op->customdata);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

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
    ED_region_draw_cb_exit(region->type, cd->draw_handle);

    if (brush->dyntopo.inherit & DYNTOPO_INHERIT_CONSTANT_DETAIL) {
      sd->dyntopo.constant_detail = cd->detail_size;
    }
    else {
      brush->dyntopo.constant_detail = cd->detail_size;
    }
    ss->draw_faded_cursor = false;
    MEM_freeN(op->customdata);
    ED_region_tag_redraw(region);
    ED_workspace_status_text(C, nullptr);
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
    return OPERATOR_RUNNING_MODAL;
  }
  /* Regular mode, changes the detail size by moving the cursor. */
  dyntopo_detail_size_update_from_mouse_delta(cd, event);

  return OPERATOR_RUNNING_MODAL;
}

static int dyntopo_detail_size_edit_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  /* Fallback to radial control for modes other than SCULPT_DYNTOPO_DETAIL_CONSTANT [same as in
   * SCULPT_OT_set_detail_size]. */
  int mode = !brush || brush->dyntopo.inherit & DYNTOPO_INHERIT_MODE ? sd->dyntopo.mode :
                                                                       brush->dyntopo.mode;
  if (!ELEM(mode, DYNTOPO_DETAIL_MANUAL, DYNTOPO_DETAIL_CONSTANT)) {
    sculpt_detail_size_set_radial_control(C);

    return OPERATOR_FINISHED;
  }

  /* Special method for SCULPT_DYNTOPO_DETAIL_CONSTANT. */
  ARegion *region = CTX_wm_region(C);
  Object *active_object = CTX_data_active_object(C);

  DyntopoDetailSizeEditCustomData *cd = MEM_cnew<DyntopoDetailSizeEditCustomData>(__func__);

  int constant_detail = brush->dyntopo.inherit & DYNTOPO_INHERIT_CONSTANT_DETAIL ?
                            sd->dyntopo.constant_detail :
                            brush->dyntopo.constant_detail;

  /* Initial operator Custom Data setup. */
  cd->detail_range = brush->dyntopo.inherit & DYNTOPO_INHERIT_DETAIL_RANGE ?
                         sd->dyntopo.detail_range :
                         brush->dyntopo.detail_range;
  cd->draw_handle = ED_region_draw_cb_activate(
      region->type, dyntopo_detail_size_edit_draw, cd, REGION_DRAW_POST_VIEW);
  cd->active_object = active_object;
  cd->init_mval[0] = event->mval[0];
  cd->init_mval[1] = event->mval[1];
  cd->detail_size = constant_detail;
  cd->init_detail_size = constant_detail;
  copy_v4_v4(cd->outline_col, brush->add_col);
  op->customdata = cd;

  SculptSession *ss = active_object->sculpt;
  cd->radius = ss->cursor_radius;

  /* Generates the matrix to position the gizmo in the surface of the mesh using the same
   * location and orientation as the brush cursor. */
  float cursor_trans[4][4], cursor_rot[4][4];
  const float z_axis[4] = {0.0f, 0.0f, 1.0f, 0.0f};
  float quat[4];
  copy_m4_m4(cursor_trans, active_object->object_to_world);
  translate_m4(
      cursor_trans, ss->cursor_location[0], ss->cursor_location[1], ss->cursor_location[2]);

  float cursor_normal[3];
  if (!is_zero_v3(ss->cursor_sampled_normal)) {
    copy_v3_v3(cursor_normal, ss->cursor_sampled_normal);
  }
  else {
    copy_v3_v3(cursor_normal, ss->cursor_normal);
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

  SCULPT_vertex_random_access_ensure(ss);

  WM_event_add_modal_handler(C, op);
  ED_region_tag_redraw(region);

  ss->draw_faded_cursor = true;

  const char *status_str = TIP_(
      "Move the mouse to change the dyntopo detail size. LMB: confirm size, ESC/RMB: cancel, "
      "SHIFT: precision mode, CTRL: sample detail size");
  ED_workspace_status_text(C, status_str);

  return OPERATOR_RUNNING_MODAL;
}

void SCULPT_OT_dyntopo_detail_size_edit(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Edit Dyntopo Detail Size";
  ot->description = "Modify the detail size of dyntopo interactively";
  ot->idname = "SCULPT_OT_dyntopo_detail_size_edit";

  /* api callbacks */
  ot->poll = sculpt_and_dynamic_topology_poll;
  ot->invoke = dyntopo_detail_size_edit_invoke;
  ot->modal = dyntopo_detail_size_edit_modal;
  ot->cancel = dyntopo_detail_size_edit_cancel;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */
