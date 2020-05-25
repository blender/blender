/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_task.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_view3d.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"

#include "bmesh.h"

#include <math.h>
#include <stdlib.h>

typedef struct {
  const float *ray_start;
  bool hit;
  float depth;
  float edge_length;

  struct IsectRayPrecalc isect_precalc;
} SculptDetailRaycastData;

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

static int sculpt_detail_flood_fill_exec(bContext *C, wmOperator *UNUSED(op))
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  float size;
  float bb_min[3], bb_max[3], center[3], dim[3];
  int totnodes;
  PBVHNode **nodes;

  BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnodes);

  if (!totnodes) {
    return OPERATOR_CANCELLED;
  }

  for (int i = 0; i < totnodes; i++) {
    BKE_pbvh_node_mark_topology_update(nodes[i]);
  }
  /* Get the bounding box, it's center and size. */
  BKE_pbvh_bounding_box(ob->sculpt->pbvh, bb_min, bb_max);
  add_v3_v3v3(center, bb_min, bb_max);
  mul_v3_fl(center, 0.5f);
  sub_v3_v3v3(dim, bb_max, bb_min);
  size = max_fff(dim[0], dim[1], dim[2]);

  /* Update topology size. */
  float object_space_constant_detail = 1.0f / (sd->constant_detail * mat4_to_scale(ob->obmat));
  BKE_pbvh_bmesh_detail_size_set(ss->pbvh, object_space_constant_detail);

  SCULPT_undo_push_begin("Dynamic topology flood fill");
  SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_COORDS);

  while (BKE_pbvh_bmesh_update_topology(
      ss->pbvh, PBVH_Collapse | PBVH_Subdivide, center, NULL, size, false, false)) {
    for (int i = 0; i < totnodes; i++) {
      BKE_pbvh_node_mark_topology_update(nodes[i]);
    }
  }

  MEM_SAFE_FREE(nodes);
  SCULPT_undo_push_end();

  /* Force rebuild of pbvh for better BB placement. */
  SCULPT_pbvh_clear(ob);
  /* Redraw. */
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

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

typedef enum eSculptSampleDetailModeTypes {
  SAMPLE_DETAIL_DYNTOPO = 0,
  SAMPLE_DETAIL_VOXEL = 1,
} eSculptSampleDetailModeTypes;

static EnumPropertyItem prop_sculpt_sample_detail_mode_types[] = {
    {SAMPLE_DETAIL_DYNTOPO, "DYNTOPO", 0, "Dyntopo", "Sample dyntopo detail"},
    {SAMPLE_DETAIL_VOXEL, "VOXEL", 0, "Voxel", "Sample mesh voxel size"},
    {0, NULL, 0, NULL, NULL},
};

static void sample_detail_voxel(bContext *C, ViewContext *vc, int mx, int my)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = vc->obact;
  Mesh *mesh = ob->data;

  SculptSession *ss = ob->sculpt;
  SculptCursorGeometryInfo sgi;
  SCULPT_vertex_random_access_init(ss);

  /* Update the active vertex. */
  float mouse[2] = {mx, my};
  SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false);

  /* Average the edge length of the connected edges to the active vertex. */
  int active_vertex = SCULPT_active_vertex_get(ss);
  const float *active_vertex_co = SCULPT_active_vertex_co_get(ss);
  float edge_length = 0.0f;
  int tot = 0;
  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, active_vertex, ni) {
    edge_length += len_v3v3(active_vertex_co, SCULPT_vertex_co_get(ss, ni.index));
    tot += 1;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  if (tot > 0) {
    mesh->remesh_voxel_size = edge_length / (float)tot;
  }
}

static void sculpt_raycast_detail_cb(PBVHNode *node, void *data_v, float *tmin)
{
  if (BKE_pbvh_node_get_tmin(node) < *tmin) {
    SculptDetailRaycastData *srd = data_v;
    if (BKE_pbvh_bmesh_node_raycast_detail(
            node, srd->ray_start, &srd->isect_precalc, &srd->depth, &srd->edge_length)) {
      srd->hit = true;
      *tmin = srd->depth;
    }
  }
}

static void sample_detail_dyntopo(bContext *C, ViewContext *vc, ARegion *region, int mx, int my)
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Object *ob = vc->obact;
  Brush *brush = BKE_paint_brush(&sd->paint);

  SCULPT_stroke_modifiers_check(C, ob, brush);

  float mouse[2] = {mx - region->winrct.xmin, my - region->winrct.ymin};
  float ray_start[3], ray_end[3], ray_normal[3];
  float depth = SCULPT_raycast_init(vc, mouse, ray_start, ray_end, ray_normal, false);

  SculptDetailRaycastData srd;
  srd.hit = 0;
  srd.ray_start = ray_start;
  srd.depth = depth;
  srd.edge_length = 0.0f;
  isect_ray_tri_watertight_v3_precalc(&srd.isect_precalc, ray_normal);

  BKE_pbvh_raycast(ob->sculpt->pbvh, sculpt_raycast_detail_cb, &srd, ray_start, ray_normal, false);

  if (srd.hit && srd.edge_length > 0.0f) {
    /* Convert edge length to world space detail resolution. */
    sd->constant_detail = 1 / (srd.edge_length * mat4_to_scale(ob->obmat));
  }
}

static int sample_detail(bContext *C, int mx, int my, int mode)
{
  /* Find 3D view to pick from. */
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = BKE_screen_find_area_xy(screen, SPACE_VIEW3D, mx, my);
  ARegion *region = (area) ? BKE_area_find_region_xy(area, RGN_TYPE_WINDOW, mx, my) : NULL;
  if (region == NULL) {
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
  if (ob == NULL) {
    return OPERATOR_CANCELLED;
  }

  SculptSession *ss = ob->sculpt;
  if (!ss->pbvh) {
    return OPERATOR_CANCELLED;
  }

  /* Pick sample detail. */
  switch (mode) {
    case SAMPLE_DETAIL_DYNTOPO:
      if (BKE_pbvh_type(ss->pbvh) != PBVH_BMESH) {
        CTX_wm_area_set(C, prev_area);
        CTX_wm_region_set(C, prev_region);
        return OPERATOR_CANCELLED;
      }
      sample_detail_dyntopo(C, &vc, region, mx, my);
      break;
    case SAMPLE_DETAIL_VOXEL:
      if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
        CTX_wm_area_set(C, prev_area);
        CTX_wm_region_set(C, prev_region);
        return OPERATOR_CANCELLED;
      }
      sample_detail_voxel(C, &vc, mx, my);
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
  return sample_detail(C, ss_co[0], ss_co[1], mode);
}

static int sculpt_sample_detail_size_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(e))
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
        int ss_co[2] = {event->x, event->y};

        int mode = RNA_enum_get(op->ptr, "mode");
        sample_detail(C, ss_co[0], ss_co[1], mode);

        RNA_int_set_array(op->ptr, "location", ss_co);
        WM_cursor_modal_restore(CTX_wm_window(C));
        ED_workspace_status_text(C, NULL);
        WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, NULL);

        return OPERATOR_FINISHED;
      }
      break;

    case RIGHTMOUSE: {
      WM_cursor_modal_restore(CTX_wm_window(C));
      ED_workspace_status_text(C, NULL);

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
                    NULL,
                    0,
                    SHRT_MAX,
                    "Location",
                    "Screen Coordinates of sampling",
                    0,
                    SHRT_MAX);
  RNA_def_enum(ot->srna,
               "mode",
               prop_sculpt_sample_detail_mode_types,
               SAMPLE_DETAIL_DYNTOPO,
               "Detail Mode",
               "Target sculpting workflow that is going to use the sampled size");
}

/* Dynamic-topology detail size.
 *
 * This should be improved further, perhaps by showing a triangle
 * grid rather than brush alpha. */
static void set_brush_rc_props(PointerRNA *ptr, const char *prop)
{
  char *path = BLI_sprintfN("tool_settings.sculpt.brush.%s", prop);
  RNA_string_set(ptr, "data_path_primary", path);
  MEM_freeN(path);
}

static int sculpt_set_detail_size_exec(bContext *C, wmOperator *UNUSED(op))
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  PointerRNA props_ptr;
  wmOperatorType *ot = WM_operatortype_find("WM_OT_radial_control", true);

  WM_operator_properties_create_ptr(&props_ptr, ot);

  if (sd->flags & (SCULPT_DYNTOPO_DETAIL_CONSTANT | SCULPT_DYNTOPO_DETAIL_MANUAL)) {
    set_brush_rc_props(&props_ptr, "constant_detail_resolution");
    RNA_string_set(
        &props_ptr, "data_path_primary", "tool_settings.sculpt.constant_detail_resolution");
  }
  else if (sd->flags & SCULPT_DYNTOPO_DETAIL_BRUSH) {
    set_brush_rc_props(&props_ptr, "constant_detail_resolution");
    RNA_string_set(&props_ptr, "data_path_primary", "tool_settings.sculpt.detail_percent");
  }
  else {
    set_brush_rc_props(&props_ptr, "detail_size");
    RNA_string_set(&props_ptr, "data_path_primary", "tool_settings.sculpt.detail_size");
  }

  WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &props_ptr);

  WM_operator_properties_free(&props_ptr);

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
