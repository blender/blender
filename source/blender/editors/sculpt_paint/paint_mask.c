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
 * along with this program; if not, write to the Free Software  Foundation,
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

#include "BLI_bitmap_draw_2d.h"
#include "BLI_math_matrix.h"
#include "BLI_math_geom.h"
#include "BLI_utildefines.h"
#include "BLI_lasso_2d.h"
#include "BLI_task.h"

#include "BKE_pbvh.h"
#include "BKE_ccg.h"
#include "BKE_context.h"
#include "BKE_multires.h"
#include "BKE_paint.h"
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
#include "sculpt_intern.h" /* for undo push */

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
} MaskTaskData;

static void mask_flood_fill_task_cb(void *__restrict userdata,
                                    const int i,
                                    const TaskParallelTLS *__restrict UNUSED(tls))
{
  MaskTaskData *data = userdata;

  PBVHNode *node = data->nodes[i];

  const PaintMaskFloodMode mode = data->mode;
  const float value = data->value;

  PBVHVertexIter vi;

  sculpt_undo_push_node(data->ob, node, SCULPT_UNDO_MASK);

  BKE_pbvh_vertex_iter_begin(data->pbvh, node, vi, PBVH_ITER_UNIQUE)
  {
    mask_flood_fill_set_elem(vi.mask, mode, value);
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_mark_redraw(node);
  if (data->multires) {
    BKE_pbvh_node_mark_normals_update(node);
  }
}

static int mask_flood_fill_exec(bContext *C, wmOperator *op)
{
  ARegion *ar = CTX_wm_region(C);
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  PaintMaskFloodMode mode;
  float value;
  PBVH *pbvh;
  PBVHNode **nodes;
  int totnode;
  bool multires;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  mode = RNA_enum_get(op->ptr, "mode");
  value = RNA_float_get(op->ptr, "value");

  BKE_sculpt_update_object_for_edit(depsgraph, ob, false, true);
  pbvh = ob->sculpt->pbvh;
  multires = (BKE_pbvh_type(pbvh) == PBVH_GRIDS);

  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);

  sculpt_undo_push_begin("Mask flood fill");

  MaskTaskData data = {
      .ob = ob,
      .pbvh = pbvh,
      .nodes = nodes,
      .multires = multires,
      .mode = mode,
      .value = value,
  };

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_THREADED_LIMIT);
  BLI_task_parallel_range(

      0, totnode, &data, mask_flood_fill_task_cb, &settings);

  if (multires) {
    multires_mark_as_modified(depsgraph, ob, MULTIRES_COORDS_MODIFIED);
  }

  sculpt_undo_push_end();

  if (nodes) {
    MEM_freeN(nodes);
  }

  ED_region_tag_redraw(ar);

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

void PAINT_OT_mask_flood_fill(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Mask Flood Fill";
  ot->idname = "PAINT_OT_mask_flood_fill";
  ot->description = "Fill the whole mask with a given value, or invert its values";

  /* api callbacks */
  ot->exec = mask_flood_fill_exec;
  ot->poll = sculpt_mode_poll;

  ot->flag = OPTYPE_REGISTER;

  /* rna */
  RNA_def_enum(ot->srna, "mode", mode_items, PAINT_MASK_FLOOD_VALUE, "Mode", NULL);
  RNA_def_float(
      ot->srna,
      "value",
      0,
      0,
      1,
      "Value",
      "Mask level to use when mode is 'Value'; zero means no masking and one is fully masked",
      0,
      1);
}

/* Box select, operator is VIEW3D_OT_select_box, defined in view3d_select.c */

static bool is_effected(float planes[4][4], const float co[3])
{
  return isect_point_planes_v3(planes, 4, co);
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

static void mask_box_select_task_cb(void *__restrict userdata,
                                    const int i,
                                    const TaskParallelTLS *__restrict UNUSED(tls))
{
  MaskTaskData *data = userdata;

  PBVHNode *node = data->nodes[i];

  const PaintMaskFloodMode mode = data->mode;
  const float value = data->value;
  float(*clip_planes_final)[4] = data->clip_planes_final;

  PBVHVertexIter vi;
  bool any_masked = false;

  BKE_pbvh_vertex_iter_begin(data->pbvh, node, vi, PBVH_ITER_UNIQUE)
  {
    if (is_effected(clip_planes_final, vi.co)) {
      if (!any_masked) {
        any_masked = true;

        sculpt_undo_push_node(data->ob, node, SCULPT_UNDO_MASK);

        BKE_pbvh_node_mark_redraw(node);
        if (data->multires) {
          BKE_pbvh_node_mark_normals_update(node);
        }
      }
      mask_flood_fill_set_elem(vi.mask, mode, value);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

bool ED_sculpt_mask_box_select(struct bContext *C, ViewContext *vc, const rcti *rect, bool select)
{
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  Sculpt *sd = vc->scene->toolsettings->sculpt;
  BoundBox bb;
  float clip_planes[4][4];
  float clip_planes_final[4][4];
  ARegion *ar = vc->ar;
  Object *ob = vc->obact;
  PaintMaskFloodMode mode;
  float value;
  bool multires;
  PBVH *pbvh;
  PBVHNode **nodes;
  int totnode, symmpass;
  int symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;

  mode = PAINT_MASK_FLOOD_VALUE;
  value = select ? 1.0 : 0.0;

  /* transform the clip planes in object space */
  ED_view3d_clipping_calc(&bb, clip_planes, vc->ar, vc->obact, rect);
  negate_m4(clip_planes);

  BKE_sculpt_update_object_for_edit(depsgraph, ob, false, true);
  pbvh = ob->sculpt->pbvh;
  multires = (BKE_pbvh_type(pbvh) == PBVH_GRIDS);

  sculpt_undo_push_begin("Mask box fill");

  for (symmpass = 0; symmpass <= symm; ++symmpass) {
    if (symmpass == 0 || (symm & symmpass && (symm != 5 || symmpass != 3) &&
                          (symm != 6 || (symmpass != 3 && symmpass != 5)))) {
      int j = 0;

      /* flip the planes symmetrically as needed */
      for (; j < 4; j++) {
        flip_plane(clip_planes_final[j], clip_planes[j], symmpass);
      }

      BKE_pbvh_search_gather(
          pbvh, BKE_pbvh_node_planes_contain_AABB, clip_planes_final, &nodes, &totnode);

      MaskTaskData data = {
          .ob = ob,
          .pbvh = pbvh,
          .nodes = nodes,
          .multires = multires,
          .mode = mode,
          .value = value,
          .clip_planes_final = clip_planes_final,
      };

      TaskParallelSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      settings.use_threading = ((sd->flags & SCULPT_USE_OPENMP) &&
                                totnode > SCULPT_THREADED_LIMIT);
      BLI_task_parallel_range(0, totnode, &data, mask_box_select_task_cb, &settings);

      if (nodes) {
        MEM_freeN(nodes);
      }
    }
  }

  if (multires) {
    multires_mark_as_modified(depsgraph, ob, MULTIRES_COORDS_MODIFIED);
  }

  sculpt_undo_push_end();

  ED_region_tag_redraw(ar);

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return true;
}

typedef struct LassoMaskData {
  struct ViewContext *vc;
  float projviewobjmat[4][4];
  BLI_bitmap *px;
  int width;
  rcti rect; /* bounding box for scanfilling */
  int symmpass;

  MaskTaskData task_data;
} LassoMaskData;

/**
 * Lasso select. This could be defined as part of #VIEW3D_OT_select_lasso,
 * still the shortcuts conflict, so we will use a separate operator.
 */
static bool is_effected_lasso(LassoMaskData *data, float co[3])
{
  float scr_co_f[2];
  int scr_co_s[2];
  float co_final[3];

  flip_v3_v3(co_final, co, data->symmpass);
  /* first project point to 2d space */
  ED_view3d_project_float_v2_m4(data->vc->ar, co_final, scr_co_f, data->projviewobjmat);

  scr_co_s[0] = scr_co_f[0];
  scr_co_s[1] = scr_co_f[1];

  /* clip against screen, because lasso is limited to screen only */
  if ((scr_co_s[0] < data->rect.xmin) || (scr_co_s[1] < data->rect.ymin) ||
      (scr_co_s[0] >= data->rect.xmax) || (scr_co_s[1] >= data->rect.ymax)) {
    return false;
  }

  scr_co_s[0] -= data->rect.xmin;
  scr_co_s[1] -= data->rect.ymin;

  return BLI_BITMAP_TEST_BOOL(data->px, scr_co_s[1] * data->width + scr_co_s[0]);
}

static void mask_lasso_px_cb(int x, int x_end, int y, void *user_data)
{
  LassoMaskData *data = user_data;
  int index = (y * data->width) + x;
  int index_end = (y * data->width) + x_end;
  do {
    BLI_BITMAP_ENABLE(data->px, index);
  } while (++index != index_end);
}

static void mask_gesture_lasso_task_cb(void *__restrict userdata,
                                       const int i,
                                       const TaskParallelTLS *__restrict UNUSED(tls))
{
  LassoMaskData *lasso_data = userdata;
  MaskTaskData *data = &lasso_data->task_data;

  PBVHNode *node = data->nodes[i];

  const PaintMaskFloodMode mode = data->mode;
  const float value = data->value;

  PBVHVertexIter vi;
  bool any_masked = false;

  BKE_pbvh_vertex_iter_begin(data->pbvh, node, vi, PBVH_ITER_UNIQUE)
  {
    if (is_effected_lasso(lasso_data, vi.co)) {
      if (!any_masked) {
        any_masked = true;

        sculpt_undo_push_node(data->ob, node, SCULPT_UNDO_MASK);

        BKE_pbvh_node_mark_redraw(node);
        if (data->multires) {
          BKE_pbvh_node_mark_normals_update(node);
        }
      }

      mask_flood_fill_set_elem(vi.mask, mode, value);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static int paint_mask_gesture_lasso_exec(bContext *C, wmOperator *op)
{
  int mcords_tot;
  const int(*mcords)[2] = WM_gesture_lasso_path_to_array(C, op, &mcords_tot);

  if (mcords) {
    Depsgraph *depsgraph = CTX_data_depsgraph(C);
    float clip_planes[4][4], clip_planes_final[4][4];
    BoundBox bb;
    Object *ob;
    ViewContext vc;
    LassoMaskData data;
    Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
    int symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;
    PBVH *pbvh;
    PBVHNode **nodes;
    int totnode, symmpass;
    bool multires;
    PaintMaskFloodMode mode = RNA_enum_get(op->ptr, "mode");
    float value = RNA_float_get(op->ptr, "value");

    /* Calculations of individual vertices are done in 2D screen space to diminish the amount of
     * calculations done. Bounding box PBVH collision is not computed against enclosing rectangle
     * of lasso */
    ED_view3d_viewcontext_init(C, &vc);

    /* lasso data calculations */
    data.vc = &vc;
    ob = vc.obact;
    ED_view3d_ob_project_mat_get(vc.rv3d, ob, data.projviewobjmat);

    BLI_lasso_boundbox(&data.rect, mcords, mcords_tot);
    data.width = data.rect.xmax - data.rect.xmin;
    data.px = BLI_BITMAP_NEW(data.width * (data.rect.ymax - data.rect.ymin), __func__);

    BLI_bitmap_draw_2d_poly_v2i_n(data.rect.xmin,
                                  data.rect.ymin,
                                  data.rect.xmax,
                                  data.rect.ymax,
                                  mcords,
                                  mcords_tot,
                                  mask_lasso_px_cb,
                                  &data);

    ED_view3d_clipping_calc(&bb, clip_planes, vc.ar, vc.obact, &data.rect);
    negate_m4(clip_planes);

    BKE_sculpt_update_object_for_edit(depsgraph, ob, false, true);
    pbvh = ob->sculpt->pbvh;
    multires = (BKE_pbvh_type(pbvh) == PBVH_GRIDS);

    sculpt_undo_push_begin("Mask lasso fill");

    for (symmpass = 0; symmpass <= symm; ++symmpass) {
      if ((symmpass == 0) || (symm & symmpass && (symm != 5 || symmpass != 3) &&
                              (symm != 6 || (symmpass != 3 && symmpass != 5)))) {
        int j = 0;

        /* flip the planes symmetrically as needed */
        for (; j < 4; j++) {
          flip_plane(clip_planes_final[j], clip_planes[j], symmpass);
        }

        data.symmpass = symmpass;

        /* gather nodes inside lasso's enclosing rectangle
         * (should greatly help with bigger meshes) */
        BKE_pbvh_search_gather(
            pbvh, BKE_pbvh_node_planes_contain_AABB, clip_planes_final, &nodes, &totnode);

        data.task_data.ob = ob;
        data.task_data.pbvh = pbvh;
        data.task_data.nodes = nodes;
        data.task_data.multires = multires;
        data.task_data.mode = mode;
        data.task_data.value = value;

        TaskParallelSettings settings;
        BLI_parallel_range_settings_defaults(&settings);
        settings.use_threading = ((sd->flags & SCULPT_USE_OPENMP) &&
                                  (totnode > SCULPT_THREADED_LIMIT));
        BLI_task_parallel_range(0, totnode, &data, mask_gesture_lasso_task_cb, &settings);

        if (nodes) {
          MEM_freeN(nodes);
        }
      }
    }

    if (multires) {
      multires_mark_as_modified(depsgraph, ob, MULTIRES_COORDS_MODIFIED);
    }

    sculpt_undo_push_end();

    ED_region_tag_redraw(vc.ar);
    MEM_freeN((void *)mcords);
    MEM_freeN(data.px);

    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_PASS_THROUGH;
}

void PAINT_OT_mask_lasso_gesture(wmOperatorType *ot)
{
  ot->name = "Mask Lasso Gesture";
  ot->idname = "PAINT_OT_mask_lasso_gesture";
  ot->description = "Add mask within the lasso as you move the brush";

  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = paint_mask_gesture_lasso_exec;

  ot->poll = sculpt_mode_poll;

  ot->flag = OPTYPE_REGISTER;

  /* properties */
  WM_operator_properties_gesture_lasso(ot);

  RNA_def_enum(ot->srna, "mode", mode_items, PAINT_MASK_FLOOD_VALUE, "Mode", NULL);
  RNA_def_float(
      ot->srna,
      "value",
      1.0,
      0,
      1.0,
      "Value",
      "Mask level to use when mode is 'Value'; zero means no masking and one is fully masked",
      0,
      1);
}
