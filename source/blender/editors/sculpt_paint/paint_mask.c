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
 * The Original Code is Copyright (C) 2012 by Nicholas Bishop
 * All rights reserved.
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_vec_types.h"

#include "BLI_bitmap_draw_2d.h"
#include "BLI_lasso_2d.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_rect.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_ccg.h"
#include "BKE_context.h"
#include "BKE_multires.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_subsurf.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_view3d.h"

#include "bmesh.h"

#include "paint_intern.h"

/* For undo push. */
#include "sculpt_intern.h"

#include <stdlib.h>

static const EnumPropertyItem mode_items[] = {
    {PAINT_MASK_FLOOD_VALUE,
     "VALUE",
     0,
     "Value",
     "Set mask to the level specified by the 'value' property"},
    {PAINT_MASK_FLOOD_VALUE_INVERSE,
     "VALUE_INVERSE",
     0,
     "Value Inverted",
     "Set mask to the level specified by the inverted 'value' property"},
    {PAINT_MASK_INVERT, "INVERT", 0, "Invert", "Invert the mask"},
    {0}};

static void mask_flood_fill_set_elem(float *elem, PaintMaskFloodMode mode, float value)
{
  switch (mode) {
    case PAINT_MASK_FLOOD_VALUE:
      (*elem) = value;
      break;
    case PAINT_MASK_FLOOD_VALUE_INVERSE:
      (*elem) = 1.0f - value;
      break;
    case PAINT_MASK_INVERT:
      (*elem) = 1.0f - (*elem);
      break;
  }
}

typedef struct MaskTaskData {
  Object *ob;
  PBVH *pbvh;
  PBVHNode **nodes;
  bool multires;

  PaintMaskFloodMode mode;
  float value;
  float (*clip_planes_final)[4];

  bool front_faces_only;
  float view_normal[3];
} MaskTaskData;

static void mask_flood_fill_task_cb(void *__restrict userdata,
                                    const int i,
                                    const TaskParallelTLS *__restrict UNUSED(tls))
{
  MaskTaskData *data = userdata;

  PBVHNode *node = data->nodes[i];

  const PaintMaskFloodMode mode = data->mode;
  const float value = data->value;
  bool redraw = false;

  PBVHVertexIter vi;

  SCULPT_undo_push_node(data->ob, node, SCULPT_UNDO_MASK);

  BKE_pbvh_vertex_iter_begin(data->pbvh, node, vi, PBVH_ITER_UNIQUE)
  {
    float prevmask = *vi.mask;
    mask_flood_fill_set_elem(vi.mask, mode, value);
    if (prevmask != *vi.mask) {
      redraw = true;
    }
  }
  BKE_pbvh_vertex_iter_end;

  if (redraw) {
    BKE_pbvh_node_mark_update_mask(node);
    if (data->multires) {
      BKE_pbvh_node_mark_normals_update(node);
    }
  }
}

static int mask_flood_fill_exec(bContext *C, wmOperator *op)
{
  ARegion *region = CTX_wm_region(C);
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  PaintMaskFloodMode mode;
  float value;
  PBVH *pbvh;
  PBVHNode **nodes;
  int totnode;
  bool multires;

  mode = RNA_enum_get(op->ptr, "mode");
  value = RNA_float_get(op->ptr, "value");

  BKE_sculpt_update_object_for_edit(depsgraph, ob, false, true, false);
  pbvh = ob->sculpt->pbvh;
  multires = (BKE_pbvh_type(pbvh) == PBVH_GRIDS);

  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);

  SCULPT_undo_push_begin("Mask flood fill");

  MaskTaskData data = {
      .ob = ob,
      .pbvh = pbvh,
      .nodes = nodes,
      .multires = multires,
      .mode = mode,
      .value = value,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, mask_flood_fill_task_cb, &settings);

  if (multires) {
    multires_mark_as_modified(depsgraph, ob, MULTIRES_COORDS_MODIFIED);
  }

  BKE_pbvh_update_vertex_data(pbvh, PBVH_UpdateMask);

  SCULPT_undo_push_end();

  if (nodes) {
    MEM_freeN(nodes);
  }

  ED_region_tag_redraw(region);

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

void PAINT_OT_mask_flood_fill(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Mask Flood Fill";
  ot->idname = "PAINT_OT_mask_flood_fill";
  ot->description = "Fill the whole mask with a given value, or invert its values";

  /* API callbacks. */
  ot->exec = mask_flood_fill_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER;

  /* RNA. */
  RNA_def_enum(ot->srna, "mode", mode_items, PAINT_MASK_FLOOD_VALUE, "Mode", NULL);
  RNA_def_float(
      ot->srna,
      "value",
      0.0f,
      0.0f,
      1.0f,
      "Value",
      "Mask level to use when mode is 'Value'; zero means no masking and one is fully masked",
      0.0f,
      1.0f);
}

/* Sculpt Gesture Operators. */

typedef enum eSculptGestureShapeType {
  SCULPT_GESTURE_SHAPE_BOX,
  SCULPT_GESTURE_SHAPE_LASSO,
} eMaskGesturesShapeType;

typedef struct LassoGestureData {
  float projviewobjmat[4][4];

  rcti boundbox;
  int width;

  /* 2D bitmap to test if a vertex is affected by the lasso shape. */
  BLI_bitmap *mask_px;
} LassoGestureData;

typedef struct SculptGestureContext {
  SculptSession *ss;
  ViewContext vc;

  /* Enabled and currently active symmetry. */
  ePaintSymmetryFlags symm;
  ePaintSymmetryFlags symmpass;

  /* Operation parameters. */
  eMaskGesturesShapeType shape_type;
  bool front_faces_only;

  /* Mask operation parameters. */
  PaintMaskFloodMode mask_mode;
  float mask_value;

  /* View parameters. */
  float true_view_normal[3];
  float view_normal[3];

  float true_clip_planes[4][4];
  float clip_planes[4][4];

  /* Lasso Gesture. */
  LassoGestureData lasso;

  /* Task Callback Data. */
  PBVHNode **nodes;
  int totnode;
} SculptGestureContext;

static void sculpt_gesture_operator_properties(wmOperatorType *ot)
{
  RNA_def_boolean(ot->srna,
                  "use_front_faces_only",
                  false,
                  "Front Faces Only",
                  "Affect only faces facing towards the view");
}

static void sculpt_gesture_context_init_common(bContext *C,
                                               wmOperator *op,
                                               SculptGestureContext *sgcontext)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ED_view3d_viewcontext_init(C, &sgcontext->vc, depsgraph);

  Sculpt *sd = sgcontext->vc.scene->toolsettings->sculpt;
  Object *ob = sgcontext->vc.obact;

  /* Operator properties. */
  sgcontext->front_faces_only = RNA_boolean_get(op->ptr, "use_front_faces_only");

  /* SculptSession */
  sgcontext->ss = ob->sculpt;

  /* Symmetry. */
  sgcontext->symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;

  /* View Normal. */
  float mat[3][3];
  float view_dir[3] = {0.0f, 0.0f, 1.0f};
  copy_m3_m4(mat, sgcontext->vc.rv3d->viewinv);
  mul_m3_v3(mat, view_dir);
  copy_m3_m4(mat, ob->imat);
  mul_m3_v3(mat, view_dir);
  normalize_v3_v3(sgcontext->true_view_normal, view_dir);
}

static void sculpt_gesture_lasso_px_cb(int x, int x_end, int y, void *user_data)
{
  SculptGestureContext *mcontext = user_data;
  LassoGestureData *lasso = &mcontext->lasso;
  int index = (y * lasso->width) + x;
  int index_end = (y * lasso->width) + x_end;
  do {
    BLI_BITMAP_ENABLE(lasso->mask_px, index);
  } while (++index != index_end);
}

static SculptGestureContext *sculpt_gesture_init_from_lasso(bContext *C, wmOperator *op)
{
  SculptGestureContext *sgcontext = MEM_callocN(sizeof(SculptGestureContext),
                                                "sculpt gesture context lasso");
  sgcontext->shape_type = SCULPT_GESTURE_SHAPE_LASSO;

  sculpt_gesture_context_init_common(C, op, sgcontext);

  int mcoords_len;
  const int(*mcoords)[2] = WM_gesture_lasso_path_to_array(C, op, &mcoords_len);

  if (!mcoords) {
    return NULL;
  }

  ED_view3d_ob_project_mat_get(
      sgcontext->vc.rv3d, sgcontext->vc.obact, sgcontext->lasso.projviewobjmat);
  BLI_lasso_boundbox(&sgcontext->lasso.boundbox, mcoords, mcoords_len);
  sgcontext->lasso.width = sgcontext->lasso.boundbox.xmax - sgcontext->lasso.boundbox.xmin;
  sgcontext->lasso.mask_px = BLI_BITMAP_NEW(
      sgcontext->lasso.width * (sgcontext->lasso.boundbox.ymax - sgcontext->lasso.boundbox.ymin),
      __func__);

  BLI_bitmap_draw_2d_poly_v2i_n(sgcontext->lasso.boundbox.xmin,
                                sgcontext->lasso.boundbox.ymin,
                                sgcontext->lasso.boundbox.xmax,
                                sgcontext->lasso.boundbox.ymax,
                                mcoords,
                                mcoords_len,
                                sculpt_gesture_lasso_px_cb,
                                sgcontext);

  BoundBox bb;
  ED_view3d_clipping_calc(&bb,
                          sgcontext->true_clip_planes,
                          sgcontext->vc.region,
                          sgcontext->vc.obact,
                          &sgcontext->lasso.boundbox);
  MEM_freeN((void *)mcoords);

  return sgcontext;
}

static SculptGestureContext *sculpt_gesture_init_from_box(bContext *C, wmOperator *op)
{
  SculptGestureContext *sgcontext = MEM_callocN(sizeof(SculptGestureContext),
                                                "sculpt gesture context box");
  sgcontext->shape_type = SCULPT_GESTURE_SHAPE_BOX;

  sculpt_gesture_context_init_common(C, op, sgcontext);

  rcti rect;
  WM_operator_properties_border_to_rcti(op, &rect);

  BoundBox bb;
  ED_view3d_clipping_calc(
      &bb, sgcontext->true_clip_planes, sgcontext->vc.region, sgcontext->vc.obact, &rect);

  return sgcontext;
}

static void sculpt_gesture_context_free(SculptGestureContext *sgcontext)
{
  MEM_SAFE_FREE(sgcontext->lasso.mask_px);
  MEM_SAFE_FREE(sgcontext->nodes);
  MEM_SAFE_FREE(sgcontext);
}

static void flip_plane(float out[4], const float in[4], const char symm)
{
  if (symm & PAINT_SYMM_X) {
    out[0] = -in[0];
  }
  else {
    out[0] = in[0];
  }
  if (symm & PAINT_SYMM_Y) {
    out[1] = -in[1];
  }
  else {
    out[1] = in[1];
  }
  if (symm & PAINT_SYMM_Z) {
    out[2] = -in[2];
  }
  else {
    out[2] = in[2];
  }

  out[3] = in[3];
}

static void sculpt_gesture_flip_for_symmetry_pass(SculptGestureContext *sgcontext,
                                                  const ePaintSymmetryFlags symmpass)
{
  sgcontext->symmpass = symmpass;
  for (int j = 0; j < 4; j++) {
    flip_plane(sgcontext->clip_planes[j], sgcontext->true_clip_planes[j], symmpass);
  }
  negate_m4(sgcontext->clip_planes);
  flip_v3_v3(sgcontext->view_normal, sgcontext->true_view_normal, symmpass);
}

static void sculpt_gesture_update_effected_nodes(SculptGestureContext *sgcontext)
{
  SculptSession *ss = sgcontext->ss;
  float clip_planes[4][4];
  copy_m4_m4(clip_planes, sgcontext->clip_planes);
  negate_m4(clip_planes);
  PBVHFrustumPlanes frustum = {.planes = clip_planes, .num_planes = 4};
  BKE_pbvh_search_gather(ss->pbvh,
                         BKE_pbvh_node_frustum_contain_AABB,
                         &frustum,
                         &sgcontext->nodes,
                         &sgcontext->totnode);
}

static bool sculpt_gesture_is_effected_lasso(SculptGestureContext *sgcontext, const float co[3])
{
  float scr_co_f[2];
  int scr_co_s[2];
  float co_final[3];

  flip_v3_v3(co_final, co, sgcontext->symmpass);

  /* First project point to 2d space. */
  ED_view3d_project_float_v2_m4(
      sgcontext->vc.region, co_final, scr_co_f, sgcontext->lasso.projviewobjmat);

  scr_co_s[0] = scr_co_f[0];
  scr_co_s[1] = scr_co_f[1];

  /* Clip against lasso boundbox. */
  LassoGestureData *lasso = &sgcontext->lasso;
  if (!BLI_rcti_isect_pt(&lasso->boundbox, scr_co_s[0], scr_co_s[1])) {
    return false;
  }

  scr_co_s[0] -= lasso->boundbox.xmin;
  scr_co_s[1] -= lasso->boundbox.ymin;

  return BLI_BITMAP_TEST_BOOL(lasso->mask_px, scr_co_s[1] * lasso->width + scr_co_s[0]);
}

static bool sculpt_gesture_is_vertex_effected(SculptGestureContext *sgcontext, PBVHVertexIter *vd)
{
  float vertex_normal[3];
  SCULPT_vertex_normal_get(sgcontext->ss, vd->index, vertex_normal);
  float dot = dot_v3v3(sgcontext->view_normal, vertex_normal);
  const bool is_effected_front_face = !(sgcontext->front_faces_only && dot < 0.0f);

  if (!is_effected_front_face) {
    return false;
  }

  switch (sgcontext->shape_type) {
    case SCULPT_GESTURE_SHAPE_BOX:
      return isect_point_planes_v3(sgcontext->clip_planes, 4, vd->co);
    case SCULPT_GESTURE_SHAPE_LASSO:
      return sculpt_gesture_is_effected_lasso(sgcontext, vd->co);
  }
  return false;
}

static void mask_gesture_apply_task_cb(void *__restrict userdata,
                                       const int i,
                                       const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptGestureContext *sgcontext = userdata;
  Object *ob = sgcontext->vc.obact;
  PBVHNode *node = sgcontext->nodes[i];

  const bool is_multires = BKE_pbvh_type(sgcontext->ss->pbvh) == PBVH_GRIDS;

  PBVHVertexIter vd;
  bool any_masked = false;
  bool redraw = false;

  BKE_pbvh_vertex_iter_begin(sgcontext->ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_gesture_is_vertex_effected(sgcontext, &vd)) {
      float prevmask = *vd.mask;
      if (!any_masked) {
        any_masked = true;

        SCULPT_undo_push_node(ob, node, SCULPT_UNDO_MASK);

        if (is_multires) {
          BKE_pbvh_node_mark_normals_update(node);
        }
      }
      mask_flood_fill_set_elem(vd.mask, sgcontext->mask_mode, sgcontext->mask_value);
      if (prevmask != *vd.mask) {
        redraw = true;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;

  if (redraw) {
    BKE_pbvh_node_mark_update_mask(node);
  }
}

static void sculpt_gesture_apply(bContext *C, SculptGestureContext *mcontext)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  BKE_sculpt_update_object_for_edit(depsgraph, mcontext->vc.obact, false, true, false);

  SCULPT_undo_push_begin("Sculpt Gesture Apply");

  for (ePaintSymmetryFlags symmpass = 0; symmpass <= mcontext->symm; symmpass++) {
    if (SCULPT_is_symmetry_iteration_valid(symmpass, mcontext->symm)) {
      sculpt_gesture_flip_for_symmetry_pass(mcontext, symmpass);
      sculpt_gesture_update_effected_nodes(mcontext);

      TaskParallelSettings settings;
      BKE_pbvh_parallel_range_settings(&settings, true, mcontext->totnode);
      BLI_task_parallel_range(
          0, mcontext->totnode, mcontext, mask_gesture_apply_task_cb, &settings);

      MEM_SAFE_FREE(mcontext->nodes);
    }
  }

  if (BKE_pbvh_type(mcontext->ss->pbvh) == PBVH_GRIDS) {
    multires_mark_as_modified(depsgraph, mcontext->vc.obact, MULTIRES_COORDS_MODIFIED);
  }

  BKE_pbvh_update_vertex_data(mcontext->ss->pbvh, PBVH_UpdateMask);

  SCULPT_undo_push_end();

  ED_region_tag_redraw(mcontext->vc.region);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, mcontext->vc.obact);
}

static void sculpt_gesture_init_mask_properties(SculptGestureContext *sgcontext, wmOperator *op)
{
  sgcontext->mask_mode = RNA_enum_get(op->ptr, "mode");
  sgcontext->mask_value = RNA_float_get(op->ptr, "value");
}

static void paint_mask_gesture_operator_properties(wmOperatorType *ot)
{
  RNA_def_enum(ot->srna, "mode", mode_items, PAINT_MASK_FLOOD_VALUE, "Mode", NULL);
  RNA_def_float(
      ot->srna,
      "value",
      1.0f,
      0.0f,
      1.0f,
      "Value",
      "Mask level to use when mode is 'Value'; zero means no masking and one is fully masked",
      0.0f,
      1.0f);
}

static int paint_mask_gesture_box_exec(bContext *C, wmOperator *op)
{
  SculptGestureContext *sgcontext = sculpt_gesture_init_from_box(C, op);
  if (!sgcontext) {
    return OPERATOR_CANCELLED;
  }
  sculpt_gesture_init_mask_properties(sgcontext, op);
  sculpt_gesture_apply(C, sgcontext);
  sculpt_gesture_context_free(sgcontext);
  return OPERATOR_FINISHED;
}

static int paint_mask_gesture_lasso_exec(bContext *C, wmOperator *op)
{
  SculptGestureContext *sgcontext = sculpt_gesture_init_from_lasso(C, op);
  if (!sgcontext) {
    return OPERATOR_CANCELLED;
  }
  sculpt_gesture_init_mask_properties(sgcontext, op);
  sculpt_gesture_apply(C, sgcontext);
  sculpt_gesture_context_free(sgcontext);
  return OPERATOR_FINISHED;
}

void PAINT_OT_mask_lasso_gesture(wmOperatorType *ot)
{
  ot->name = "Mask Lasso Gesture";
  ot->idname = "PAINT_OT_mask_lasso_gesture";
  ot->description = "Add mask within the lasso as you move the brush";

  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = paint_mask_gesture_lasso_exec;

  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER;

  /* Properties. */
  WM_operator_properties_gesture_lasso(ot);
  sculpt_gesture_operator_properties(ot);

  paint_mask_gesture_operator_properties(ot);
}

void PAINT_OT_mask_box_gesture(wmOperatorType *ot)
{
  ot->name = "Mask Box Gesture";
  ot->idname = "PAINT_OT_mask_box_gesture";
  ot->description = "Add mask within the box as you move the brush";

  ot->invoke = WM_gesture_box_invoke;
  ot->modal = WM_gesture_box_modal;
  ot->exec = paint_mask_gesture_box_exec;

  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER;

  /* Properties. */
  WM_operator_properties_border(ot);
  sculpt_gesture_operator_properties(ot);

  paint_mask_gesture_operator_properties(ot);
}
