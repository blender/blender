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

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_vec_types.h"

#include "BLI_alloca.h"
#include "BLI_bitmap_draw_2d.h"
#include "BLI_lasso_2d.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_polyfill_2d.h"
#include "BLI_rect.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
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
#include "bmesh_tools.h"
#include "tools/bmesh_boolean.h"

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

  SCULPT_undo_push_begin(ob, "Mask flood fill");

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

  SCULPT_tag_update_overlays(C);

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
  SCULPT_GESTURE_SHAPE_LINE,
} eMaskGesturesShapeType;

typedef struct LassoGestureData {
  float projviewobjmat[4][4];

  rcti boundbox;
  int width;

  /* 2D bitmap to test if a vertex is affected by the lasso shape. */
  BLI_bitmap *mask_px;
} LassoGestureData;

typedef struct LineGestureData {
  /* Plane aligned to the gesture line. */
  float true_plane[4];
  float plane[4];

  /* Planes to limit the action to the length of the gesture segment at both sides of the affected
   * area. */
  float side_plane[2][4];
  float true_side_plane[2][4];
  bool use_side_planes;

  bool flip;
} LineGestureData;

struct SculptGestureOperation;

typedef struct SculptGestureContext {
  SculptSession *ss;
  ViewContext vc;

  /* Enabled and currently active symmetry. */
  ePaintSymmetryFlags symm;
  ePaintSymmetryFlags symmpass;

  /* Operation parameters. */
  eMaskGesturesShapeType shape_type;
  bool front_faces_only;

  struct SculptGestureOperation *operation;

  /* Gesture data. */
  /* Screen space points that represent the gesture shape. */
  float (*gesture_points)[2];
  int tot_gesture_points;

  /* View parameters. */
  float true_view_normal[3];
  float view_normal[3];

  float true_view_origin[3];
  float view_origin[3];

  float true_clip_planes[4][4];
  float clip_planes[4][4];

  /* These store the view origin and normal in world space, which is used in some gestures to
   * generate geometry aligned from the view directly in world space. */
  /* World space view origin and normal are not affected by object symmetry when doing symmetry
   * passes, so there is no separate variables with the true_ prefix to store their original values
   * without symmetry modifications. */
  float world_space_view_origin[3];
  float world_space_view_normal[3];

  /* Lasso Gesture. */
  LassoGestureData lasso;

  /* Line Gesture. */
  LineGestureData line;

  /* Task Callback Data. */
  PBVHNode **nodes;
  int totnode;
} SculptGestureContext;

typedef struct SculptGestureOperation {
  /* Initial setup (data updates, special undo push...). */
  void (*sculpt_gesture_begin)(struct bContext *, SculptGestureContext *);

  /* Apply the gesture action for each symmetry pass. */
  void (*sculpt_gesture_apply_for_symmetry_pass)(struct bContext *, SculptGestureContext *);

  /* Remaining actions after finishing the symmetry passes iterations
   * (updating data-layers, tagging PBVH updates...). */
  void (*sculpt_gesture_end)(struct bContext *, SculptGestureContext *);
} SculptGestureOperation;

static void sculpt_gesture_operator_properties(wmOperatorType *ot)
{
  RNA_def_boolean(ot->srna,
                  "use_front_faces_only",
                  false,
                  "Front Faces Only",
                  "Affect only faces facing towards the view");

  RNA_def_boolean(ot->srna,
                  "use_limit_to_segment",
                  false,
                  "Limit to Segment",
                  "Apply the gesture action only to the area that is contained within the "
                  "segment without extending its effect to the entire line");
}

static void sculpt_gesture_context_init_common(bContext *C,
                                               wmOperator *op,
                                               SculptGestureContext *sgcontext)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ED_view3d_viewcontext_init(C, &sgcontext->vc, depsgraph);
  Object *ob = sgcontext->vc.obact;

  /* Operator properties. */
  sgcontext->front_faces_only = RNA_boolean_get(op->ptr, "use_front_faces_only");
  sgcontext->line.use_side_planes = RNA_boolean_get(op->ptr, "use_limit_to_segment");

  /* SculptSession */
  sgcontext->ss = ob->sculpt;

  /* Symmetry. */
  sgcontext->symm = SCULPT_mesh_symmetry_xyz_get(ob);

  /* View Normal. */
  float mat[3][3];
  float view_dir[3] = {0.0f, 0.0f, 1.0f};
  copy_m3_m4(mat, sgcontext->vc.rv3d->viewinv);
  mul_m3_v3(mat, view_dir);
  normalize_v3_v3(sgcontext->world_space_view_normal, view_dir);
  copy_m3_m4(mat, ob->imat);
  mul_m3_v3(mat, view_dir);
  normalize_v3_v3(sgcontext->true_view_normal, view_dir);

  /* View Origin. */
  copy_v3_v3(sgcontext->world_space_view_origin, sgcontext->vc.rv3d->viewinv[3]);
  copy_v3_v3(sgcontext->true_view_origin, sgcontext->vc.rv3d->viewinv[3]);
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
  const int lasso_width = 1 + sgcontext->lasso.boundbox.xmax - sgcontext->lasso.boundbox.xmin;
  const int lasso_height = 1 + sgcontext->lasso.boundbox.ymax - sgcontext->lasso.boundbox.ymin;
  sgcontext->lasso.width = lasso_width;
  sgcontext->lasso.mask_px = BLI_BITMAP_NEW(lasso_width * lasso_height, __func__);

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

  sgcontext->gesture_points = MEM_malloc_arrayN(mcoords_len, sizeof(float[2]), "trim points");
  sgcontext->tot_gesture_points = mcoords_len;
  for (int i = 0; i < mcoords_len; i++) {
    sgcontext->gesture_points[i][0] = mcoords[i][0];
    sgcontext->gesture_points[i][1] = mcoords[i][1];
  }

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

  sgcontext->gesture_points = MEM_calloc_arrayN(4, sizeof(float[2]), "trim points");
  sgcontext->tot_gesture_points = 4;

  sgcontext->gesture_points[0][0] = rect.xmax;
  sgcontext->gesture_points[0][1] = rect.ymax;

  sgcontext->gesture_points[1][0] = rect.xmax;
  sgcontext->gesture_points[1][1] = rect.ymin;

  sgcontext->gesture_points[2][0] = rect.xmin;
  sgcontext->gesture_points[2][1] = rect.ymin;

  sgcontext->gesture_points[3][0] = rect.xmin;
  sgcontext->gesture_points[3][1] = rect.ymax;
  return sgcontext;
}

static void sculpt_gesture_line_plane_from_tri(float *r_plane,
                                               SculptGestureContext *sgcontext,
                                               const bool flip,
                                               const float p1[3],
                                               const float p2[3],
                                               const float p3[3])
{
  float normal[3];
  normal_tri_v3(normal, p1, p2, p3);
  mul_v3_mat3_m4v3(normal, sgcontext->vc.obact->imat, normal);
  if (flip) {
    mul_v3_fl(normal, -1.0f);
  }
  float plane_point_object_space[3];
  mul_v3_m4v3(plane_point_object_space, sgcontext->vc.obact->imat, p1);
  plane_from_point_normal_v3(r_plane, plane_point_object_space, normal);
}

/* Creates 4 points in the plane defined by the line and 2 extra points with an offset relative to
 * this plane. */
static void sculpt_gesture_line_calculate_plane_points(SculptGestureContext *sgcontext,
                                                       float line_points[2][2],
                                                       float r_plane_points[4][3],
                                                       float r_offset_plane_points[2][3])
{
  float depth_point[3];
  add_v3_v3v3(depth_point, sgcontext->true_view_origin, sgcontext->true_view_normal);
  ED_view3d_win_to_3d(
      sgcontext->vc.v3d, sgcontext->vc.region, depth_point, line_points[0], r_plane_points[0]);
  ED_view3d_win_to_3d(
      sgcontext->vc.v3d, sgcontext->vc.region, depth_point, line_points[1], r_plane_points[3]);

  madd_v3_v3v3fl(depth_point, sgcontext->true_view_origin, sgcontext->true_view_normal, 10.0f);
  ED_view3d_win_to_3d(
      sgcontext->vc.v3d, sgcontext->vc.region, depth_point, line_points[0], r_plane_points[1]);
  ED_view3d_win_to_3d(
      sgcontext->vc.v3d, sgcontext->vc.region, depth_point, line_points[1], r_plane_points[2]);

  float normal[3];
  normal_tri_v3(normal, r_plane_points[0], r_plane_points[1], r_plane_points[2]);
  add_v3_v3v3(r_offset_plane_points[0], r_plane_points[0], normal);
  add_v3_v3v3(r_offset_plane_points[1], r_plane_points[3], normal);
}

static SculptGestureContext *sculpt_gesture_init_from_line(bContext *C, wmOperator *op)
{
  SculptGestureContext *sgcontext = MEM_callocN(sizeof(SculptGestureContext),
                                                "sculpt gesture context line");
  sgcontext->shape_type = SCULPT_GESTURE_SHAPE_LINE;

  sculpt_gesture_context_init_common(C, op, sgcontext);

  float line_points[2][2];
  line_points[0][0] = RNA_int_get(op->ptr, "xstart");
  line_points[0][1] = RNA_int_get(op->ptr, "ystart");
  line_points[1][0] = RNA_int_get(op->ptr, "xend");
  line_points[1][1] = RNA_int_get(op->ptr, "yend");

  sgcontext->line.flip = RNA_boolean_get(op->ptr, "flip");

  float plane_points[4][3];
  float offset_plane_points[2][3];
  sculpt_gesture_line_calculate_plane_points(
      sgcontext, line_points, plane_points, offset_plane_points);

  /* Calculate line plane and normal. */
  const bool flip = sgcontext->line.flip ^ (!sgcontext->vc.rv3d->is_persp);
  sculpt_gesture_line_plane_from_tri(sgcontext->line.true_plane,
                                     sgcontext,
                                     flip,
                                     plane_points[0],
                                     plane_points[1],
                                     plane_points[2]);

  /* Calculate the side planes. */
  sculpt_gesture_line_plane_from_tri(sgcontext->line.true_side_plane[0],
                                     sgcontext,
                                     false,
                                     plane_points[1],
                                     plane_points[0],
                                     offset_plane_points[0]);
  sculpt_gesture_line_plane_from_tri(sgcontext->line.true_side_plane[1],
                                     sgcontext,
                                     false,
                                     plane_points[3],
                                     plane_points[2],
                                     offset_plane_points[1]);

  return sgcontext;
}

static void sculpt_gesture_context_free(SculptGestureContext *sgcontext)
{
  MEM_SAFE_FREE(sgcontext->lasso.mask_px);
  MEM_SAFE_FREE(sgcontext->gesture_points);
  MEM_SAFE_FREE(sgcontext->operation);
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
  flip_v3_v3(sgcontext->view_origin, sgcontext->true_view_origin, symmpass);
  flip_plane(sgcontext->line.plane, sgcontext->line.true_plane, symmpass);
  flip_plane(sgcontext->line.side_plane[0], sgcontext->line.true_side_plane[0], symmpass);
  flip_plane(sgcontext->line.side_plane[1], sgcontext->line.true_side_plane[1], symmpass);
}

static void sculpt_gesture_update_effected_nodes_by_line_plane(SculptGestureContext *sgcontext)
{
  SculptSession *ss = sgcontext->ss;
  float clip_planes[3][4];
  copy_v4_v4(clip_planes[0], sgcontext->line.plane);
  copy_v4_v4(clip_planes[1], sgcontext->line.side_plane[0]);
  copy_v4_v4(clip_planes[2], sgcontext->line.side_plane[1]);

  const int num_planes = sgcontext->line.use_side_planes ? 3 : 1;
  PBVHFrustumPlanes frustum = {.planes = clip_planes, .num_planes = num_planes};
  BKE_pbvh_search_gather(ss->pbvh,
                         BKE_pbvh_node_frustum_contain_AABB,
                         &frustum,
                         &sgcontext->nodes,
                         &sgcontext->totnode);
}

static void sculpt_gesture_update_effected_nodes_by_clip_planes(SculptGestureContext *sgcontext)
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

static void sculpt_gesture_update_effected_nodes(SculptGestureContext *sgcontext)
{
  switch (sgcontext->shape_type) {
    case SCULPT_GESTURE_SHAPE_BOX:
    case SCULPT_GESTURE_SHAPE_LASSO:
      sculpt_gesture_update_effected_nodes_by_clip_planes(sgcontext);
      break;
    case SCULPT_GESTURE_SHAPE_LINE:
      sculpt_gesture_update_effected_nodes_by_line_plane(sgcontext);
      break;
  }
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
    case SCULPT_GESTURE_SHAPE_LINE:
      if (sgcontext->line.use_side_planes) {
        return plane_point_side_v3(sgcontext->line.plane, vd->co) > 0.0f &&
               plane_point_side_v3(sgcontext->line.side_plane[0], vd->co) > 0.0f &&
               plane_point_side_v3(sgcontext->line.side_plane[1], vd->co) > 0.0f;
      }
      return plane_point_side_v3(sgcontext->line.plane, vd->co) > 0.0f;
  }
  return false;
}

static void sculpt_gesture_apply(bContext *C, SculptGestureContext *sgcontext)
{
  SculptGestureOperation *operation = sgcontext->operation;
  SCULPT_undo_push_begin(CTX_data_active_object(C), "Sculpt Gesture Apply");

  operation->sculpt_gesture_begin(C, sgcontext);

  for (ePaintSymmetryFlags symmpass = 0; symmpass <= sgcontext->symm; symmpass++) {
    if (SCULPT_is_symmetry_iteration_valid(symmpass, sgcontext->symm)) {
      sculpt_gesture_flip_for_symmetry_pass(sgcontext, symmpass);
      sculpt_gesture_update_effected_nodes(sgcontext);

      operation->sculpt_gesture_apply_for_symmetry_pass(C, sgcontext);

      MEM_SAFE_FREE(sgcontext->nodes);
    }
  }

  operation->sculpt_gesture_end(C, sgcontext);

  SCULPT_undo_push_end();

  SCULPT_tag_update_overlays(C);
}

/* Face Set Gesture Operation. */

typedef struct SculptGestureFaceSetOperation {
  SculptGestureOperation op;

  int new_face_set_id;
} SculptGestureFaceSetOperation;

static void sculpt_gesture_face_set_begin(bContext *C, SculptGestureContext *sgcontext)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  BKE_sculpt_update_object_for_edit(depsgraph, sgcontext->vc.obact, true, false, false);

  /* Face Sets modifications do a single undo push. */
  SCULPT_undo_push_node(sgcontext->vc.obact, NULL, SCULPT_UNDO_FACE_SETS);
}

static void face_set_gesture_apply_task_cb(void *__restrict userdata,
                                           const int i,
                                           const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptGestureContext *sgcontext = userdata;
  SculptGestureFaceSetOperation *face_set_operation = (SculptGestureFaceSetOperation *)
                                                          sgcontext->operation;
  PBVHNode *node = sgcontext->nodes[i];
  PBVHVertexIter vd;
  bool any_updated = false;

  BKE_pbvh_vertex_iter_begin(sgcontext->ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_gesture_is_vertex_effected(sgcontext, &vd)) {
      SCULPT_vertex_face_set_set(sgcontext->ss, vd.index, face_set_operation->new_face_set_id);
      any_updated = true;
    }
  }
  BKE_pbvh_vertex_iter_end;

  if (any_updated) {
    BKE_pbvh_node_mark_update_visibility(node);
  }
}

static void sculpt_gesture_face_set_apply_for_symmetry_pass(bContext *UNUSED(C),
                                                            SculptGestureContext *sgcontext)
{
  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, sgcontext->totnode);
  BLI_task_parallel_range(
      0, sgcontext->totnode, sgcontext, face_set_gesture_apply_task_cb, &settings);
}

static void sculpt_gesture_face_set_end(bContext *UNUSED(C), SculptGestureContext *sgcontext)
{
  BKE_pbvh_update_vertex_data(sgcontext->ss->pbvh, PBVH_UpdateVisibility);
}

static void sculpt_gesture_init_face_set_properties(SculptGestureContext *sgcontext,
                                                    wmOperator *UNUSED(op))
{
  struct Mesh *mesh = BKE_mesh_from_object(sgcontext->vc.obact);
  sgcontext->operation = MEM_callocN(sizeof(SculptGestureFaceSetOperation), "Face Set Operation");

  SculptGestureFaceSetOperation *face_set_operation = (SculptGestureFaceSetOperation *)
                                                          sgcontext->operation;

  face_set_operation->op.sculpt_gesture_begin = sculpt_gesture_face_set_begin;
  face_set_operation->op.sculpt_gesture_apply_for_symmetry_pass =
      sculpt_gesture_face_set_apply_for_symmetry_pass;
  face_set_operation->op.sculpt_gesture_end = sculpt_gesture_face_set_end;

  face_set_operation->new_face_set_id = ED_sculpt_face_sets_find_next_available_id(mesh);
}

/* Mask Gesture Operation. */

typedef struct SculptGestureMaskOperation {
  SculptGestureOperation op;

  PaintMaskFloodMode mode;
  float value;
} SculptGestureMaskOperation;

static void sculpt_gesture_mask_begin(bContext *C, SculptGestureContext *sgcontext)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  BKE_sculpt_update_object_for_edit(depsgraph, sgcontext->vc.obact, false, true, false);
}

static void mask_gesture_apply_task_cb(void *__restrict userdata,
                                       const int i,
                                       const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptGestureContext *sgcontext = userdata;
  SculptGestureMaskOperation *mask_operation = (SculptGestureMaskOperation *)sgcontext->operation;
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
      mask_flood_fill_set_elem(vd.mask, mask_operation->mode, mask_operation->value);
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

static void sculpt_gesture_mask_apply_for_symmetry_pass(bContext *UNUSED(C),
                                                        SculptGestureContext *sgcontext)
{
  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, sgcontext->totnode);
  BLI_task_parallel_range(0, sgcontext->totnode, sgcontext, mask_gesture_apply_task_cb, &settings);
}

static void sculpt_gesture_mask_end(bContext *C, SculptGestureContext *sgcontext)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  if (BKE_pbvh_type(sgcontext->ss->pbvh) == PBVH_GRIDS) {
    multires_mark_as_modified(depsgraph, sgcontext->vc.obact, MULTIRES_COORDS_MODIFIED);
  }
  BKE_pbvh_update_vertex_data(sgcontext->ss->pbvh, PBVH_UpdateMask);
}

static void sculpt_gesture_init_mask_properties(SculptGestureContext *sgcontext, wmOperator *op)
{
  sgcontext->operation = MEM_callocN(sizeof(SculptGestureMaskOperation), "Mask Operation");

  SculptGestureMaskOperation *mask_operation = (SculptGestureMaskOperation *)sgcontext->operation;

  mask_operation->op.sculpt_gesture_begin = sculpt_gesture_mask_begin;
  mask_operation->op.sculpt_gesture_apply_for_symmetry_pass =
      sculpt_gesture_mask_apply_for_symmetry_pass;
  mask_operation->op.sculpt_gesture_end = sculpt_gesture_mask_end;

  mask_operation->mode = RNA_enum_get(op->ptr, "mode");
  mask_operation->value = RNA_float_get(op->ptr, "value");
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

/* Trim Gesture Operation. */

typedef enum eSculptTrimOperationType {
  SCULPT_GESTURE_TRIM_INTERSECT,
  SCULPT_GESTURE_TRIM_DIFFERENCE,
  SCULPT_GESTURE_TRIM_UNION,
  SCULPT_GESTURE_TRIM_JOIN,
} eSculptTrimOperationType;

/* Intersect is not exposed in the UI because it does not work correctly with symmetry (it deletes
 * the symmetrical part of the mesh in the first symmetry pass). */
static EnumPropertyItem prop_trim_operation_types[] = {
    {SCULPT_GESTURE_TRIM_DIFFERENCE,
     "DIFFERENCE",
     0,
     "Difference",
     "Use a difference boolean operation"},
    {SCULPT_GESTURE_TRIM_UNION, "UNION", 0, "Union", "Use a union boolean operation"},
    {SCULPT_GESTURE_TRIM_JOIN,
     "JOIN",
     0,
     "Join",
     "Join the new mesh as separate geometry, without performing any boolean operation"},
    {0, NULL, 0, NULL, NULL},
};

typedef enum eSculptTrimOrientationType {
  SCULPT_GESTURE_TRIM_ORIENTATION_VIEW,
  SCULPT_GESTURE_TRIM_ORIENTATION_SURFACE,
} eSculptTrimOrientationType;
static EnumPropertyItem prop_trim_orientation_types[] = {
    {SCULPT_GESTURE_TRIM_ORIENTATION_VIEW,
     "VIEW",
     0,
     "View",
     "Use the view to orientate the trimming shape"},
    {SCULPT_GESTURE_TRIM_ORIENTATION_SURFACE,
     "SURFACE",
     0,
     "Surface",
     "Use the surface normal to orientate the trimming shape"},
    {0, NULL, 0, NULL, NULL},
};

typedef struct SculptGestureTrimOperation {
  SculptGestureOperation op;

  Mesh *mesh;
  float (*true_mesh_co)[3];

  float depth_front;
  float depth_back;

  bool use_cursor_depth;

  eSculptTrimOperationType mode;
  eSculptTrimOrientationType orientation;
} SculptGestureTrimOperation;

static void sculpt_gesture_trim_normals_update(SculptGestureContext *sgcontext)
{
  SculptGestureTrimOperation *trim_operation = (SculptGestureTrimOperation *)sgcontext->operation;
  Mesh *trim_mesh = trim_operation->mesh;
  BKE_mesh_calc_normals(trim_mesh);

  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(trim_mesh);
  BMesh *bm;
  bm = BM_mesh_create(&allocsize,
                      &((struct BMeshCreateParams){
                          .use_toolflags = true,
                      }));

  BM_mesh_bm_from_me(bm,
                     trim_mesh,
                     (&(struct BMeshFromMeshParams){
                         .calc_face_normal = true,
                     }));
  BM_mesh_elem_hflag_enable_all(bm, BM_FACE, BM_ELEM_TAG, false);
  BMO_op_callf(bm,
               (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
               "recalc_face_normals faces=%hf",
               BM_ELEM_TAG);
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  Mesh *result = BKE_mesh_from_bmesh_nomain(bm,
                                            (&(struct BMeshToMeshParams){
                                                .calc_object_remap = false,
                                            }),
                                            trim_mesh);
  BM_mesh_free(bm);
  BKE_id_free(NULL, trim_mesh);
  trim_operation->mesh = result;
}

/* Get the origin and normal that are going to be used for calculating the depth and position the
 * trimming geometry. */
static void sculpt_gesture_trim_shape_origin_normal_get(SculptGestureContext *sgcontext,
                                                        float *r_origin,
                                                        float *r_normal)
{
  SculptGestureTrimOperation *trim_operation = (SculptGestureTrimOperation *)sgcontext->operation;
  /* Use the view origin and normal in world space. The trimming mesh coordinates are
   * calculated in world space, aligned to the view, and then converted to object space to
   * store them in the final trimming mesh which is going to be used in the boolean operation.
   */
  switch (trim_operation->orientation) {
    case SCULPT_GESTURE_TRIM_ORIENTATION_VIEW:
      copy_v3_v3(r_origin, sgcontext->world_space_view_origin);
      copy_v3_v3(r_normal, sgcontext->world_space_view_normal);
      break;
    case SCULPT_GESTURE_TRIM_ORIENTATION_SURFACE:
      mul_v3_m4v3(r_origin, sgcontext->vc.obact->obmat, sgcontext->ss->gesture_initial_location);
      /* Transforming the normal does not take non uniform scaling into account. Sculpt mode is not
       * expected to work on object with non uniform scaling. */
      copy_v3_v3(r_normal, sgcontext->ss->gesture_initial_normal);
      mul_mat3_m4_v3(sgcontext->vc.obact->obmat, r_normal);
      break;
  }
}

static void sculpt_gesture_trim_calculate_depth(SculptGestureContext *sgcontext)
{
  SculptGestureTrimOperation *trim_operation = (SculptGestureTrimOperation *)sgcontext->operation;

  SculptSession *ss = sgcontext->ss;
  ViewContext *vc = &sgcontext->vc;

  const int totvert = SCULPT_vertex_count_get(ss);

  float shape_plane[4];
  float shape_origin[3];
  float shape_normal[3];
  sculpt_gesture_trim_shape_origin_normal_get(sgcontext, shape_origin, shape_normal);
  plane_from_point_normal_v3(shape_plane, shape_origin, shape_normal);

  trim_operation->depth_front = FLT_MAX;
  trim_operation->depth_back = -FLT_MAX;

  for (int i = 0; i < totvert; i++) {
    const float *vco = SCULPT_vertex_co_get(ss, i);
    /* Convert the coordinates to world space to calculate the depth. When generating the trimming
     * mesh, coordinates are first calculated in world space, then converted to object space to
     * store them. */
    float world_space_vco[3];
    mul_v3_m4v3(world_space_vco, vc->obact->obmat, vco);
    const float dist = dist_signed_to_plane_v3(world_space_vco, shape_plane);
    trim_operation->depth_front = min_ff(dist, trim_operation->depth_front);
    trim_operation->depth_back = max_ff(dist, trim_operation->depth_back);
  }

  if (trim_operation->use_cursor_depth) {
    float world_space_gesture_initial_location[3];
    mul_v3_m4v3(
        world_space_gesture_initial_location, vc->obact->obmat, ss->gesture_initial_location);

    float mid_point_depth;
    if (trim_operation->orientation == SCULPT_GESTURE_TRIM_ORIENTATION_VIEW) {
      mid_point_depth = ss->gesture_initial_hit ?
                            dist_signed_to_plane_v3(world_space_gesture_initial_location,
                                                    shape_plane) :
                            (trim_operation->depth_back + trim_operation->depth_front) * 0.5f;
    }
    else {
      /* When using normal orientation, if the stroke started over the mesh, position the mid point
       * at 0 distance from the shape plane. This positions the trimming shape half inside of the
       * surface. */
      mid_point_depth = ss->gesture_initial_hit ?
                            0.0f :
                            (trim_operation->depth_back + trim_operation->depth_front) * 0.5f;
    }

    const float depth_radius = ss->cursor_radius;
    trim_operation->depth_front = mid_point_depth - depth_radius;
    trim_operation->depth_back = mid_point_depth + depth_radius;
  }
}

static void sculpt_gesture_trim_geometry_generate(SculptGestureContext *sgcontext)
{
  SculptGestureTrimOperation *trim_operation = (SculptGestureTrimOperation *)sgcontext->operation;
  ViewContext *vc = &sgcontext->vc;
  ARegion *region = vc->region;

  const int tot_screen_points = sgcontext->tot_gesture_points;
  float(*screen_points)[2] = sgcontext->gesture_points;

  const int trim_totverts = tot_screen_points * 2;
  const int trim_totpolys = (2 * (tot_screen_points - 2)) + (2 * tot_screen_points);
  trim_operation->mesh = BKE_mesh_new_nomain(
      trim_totverts, 0, 0, trim_totpolys * 3, trim_totpolys);
  trim_operation->true_mesh_co = MEM_malloc_arrayN(trim_totverts, 3 * sizeof(float), "mesh orco");

  float depth_front = trim_operation->depth_front;
  float depth_back = trim_operation->depth_back;

  if (!trim_operation->use_cursor_depth) {
    /* When using cursor depth, don't modify the depth set by the cursor radius. If full depth is
     * used, adding a little padding to the trimming shape can help avoiding booleans with coplanar
     * faces. */
    depth_front -= 0.1f;
    depth_back += 0.1f;
  }

  float shape_origin[3];
  float shape_normal[3];
  float shape_plane[4];
  sculpt_gesture_trim_shape_origin_normal_get(sgcontext, shape_origin, shape_normal);
  plane_from_point_normal_v3(shape_plane, shape_origin, shape_normal);

  const float(*ob_imat)[4] = vc->obact->imat;

  /* Write vertices coordinates for the front face. */
  float depth_point[3];
  madd_v3_v3v3fl(depth_point, shape_origin, shape_normal, depth_front);
  for (int i = 0; i < tot_screen_points; i++) {
    float new_point[3];
    if (trim_operation->orientation == SCULPT_GESTURE_TRIM_ORIENTATION_VIEW) {
      ED_view3d_win_to_3d(vc->v3d, region, depth_point, screen_points[i], new_point);
    }
    else {
      ED_view3d_win_to_3d_on_plane(region, shape_plane, screen_points[i], false, new_point);
      madd_v3_v3fl(new_point, shape_normal, depth_front);
    }
    mul_v3_m4v3(trim_operation->mesh->mvert[i].co, ob_imat, new_point);
    mul_v3_m4v3(trim_operation->true_mesh_co[i], ob_imat, new_point);
  }

  /* Write vertices coordinates for the back face. */
  madd_v3_v3v3fl(depth_point, shape_origin, shape_normal, depth_back);
  for (int i = 0; i < tot_screen_points; i++) {
    float new_point[3];
    if (trim_operation->orientation == SCULPT_GESTURE_TRIM_ORIENTATION_VIEW) {
      ED_view3d_win_to_3d(vc->v3d, region, depth_point, screen_points[i], new_point);
    }
    else {
      ED_view3d_win_to_3d_on_plane(region, shape_plane, screen_points[i], false, new_point);
      madd_v3_v3fl(new_point, shape_normal, depth_back);
    }
    mul_v3_m4v3(trim_operation->mesh->mvert[i + tot_screen_points].co, ob_imat, new_point);
    mul_v3_m4v3(trim_operation->true_mesh_co[i + tot_screen_points], ob_imat, new_point);
  }

  /* Get the triangulation for the front/back poly. */
  const int tot_tris_face = tot_screen_points - 2;
  uint(*r_tris)[3] = MEM_malloc_arrayN(tot_tris_face, 3 * sizeof(uint), "tris");
  BLI_polyfill_calc(screen_points, tot_screen_points, 0, r_tris);

  /* Write the front face triangle indices. */
  MPoly *mp = trim_operation->mesh->mpoly;
  MLoop *ml = trim_operation->mesh->mloop;
  for (int i = 0; i < tot_tris_face; i++, mp++, ml += 3) {
    mp->loopstart = (int)(ml - trim_operation->mesh->mloop);
    mp->totloop = 3;
    ml[0].v = r_tris[i][0];
    ml[1].v = r_tris[i][1];
    ml[2].v = r_tris[i][2];
  }

  /* Write the back face triangle indices. */
  for (int i = 0; i < tot_tris_face; i++, mp++, ml += 3) {
    mp->loopstart = (int)(ml - trim_operation->mesh->mloop);
    mp->totloop = 3;
    ml[0].v = r_tris[i][0] + tot_screen_points;
    ml[1].v = r_tris[i][1] + tot_screen_points;
    ml[2].v = r_tris[i][2] + tot_screen_points;
  }

  MEM_freeN(r_tris);

  /* Write the indices for the lateral triangles. */
  for (int i = 0; i < tot_screen_points; i++, mp++, ml += 3) {
    mp->loopstart = (int)(ml - trim_operation->mesh->mloop);
    mp->totloop = 3;
    int current_index = i;
    int next_index = current_index + 1;
    if (next_index >= tot_screen_points) {
      next_index = 0;
    }
    ml[0].v = next_index + tot_screen_points;
    ml[1].v = next_index;
    ml[2].v = current_index;
  }

  for (int i = 0; i < tot_screen_points; i++, mp++, ml += 3) {
    mp->loopstart = (int)(ml - trim_operation->mesh->mloop);
    mp->totloop = 3;
    int current_index = i;
    int next_index = current_index + 1;
    if (next_index >= tot_screen_points) {
      next_index = 0;
    }
    ml[0].v = current_index;
    ml[1].v = current_index + tot_screen_points;
    ml[2].v = next_index + tot_screen_points;
  }

  BKE_mesh_calc_edges(trim_operation->mesh, false, false);
  sculpt_gesture_trim_normals_update(sgcontext);
}

static void sculpt_gesture_trim_geometry_free(SculptGestureContext *sgcontext)
{
  SculptGestureTrimOperation *trim_operation = (SculptGestureTrimOperation *)sgcontext->operation;
  BKE_id_free(NULL, trim_operation->mesh);
  MEM_freeN(trim_operation->true_mesh_co);
}

static int bm_face_isect_pair(BMFace *f, void *UNUSED(user_data))
{
  return BM_elem_flag_test(f, BM_ELEM_DRAW) ? 1 : 0;
}

static void sculpt_gesture_apply_trim(SculptGestureContext *sgcontext)
{
  SculptGestureTrimOperation *trim_operation = (SculptGestureTrimOperation *)sgcontext->operation;
  Mesh *sculpt_mesh = BKE_mesh_from_object(sgcontext->vc.obact);
  Mesh *trim_mesh = trim_operation->mesh;

  BMesh *bm;
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(sculpt_mesh, trim_mesh);
  bm = BM_mesh_create(&allocsize,
                      &((struct BMeshCreateParams){
                          .use_toolflags = false,
                      }));

  BM_mesh_bm_from_me(bm,
                     trim_mesh,
                     &((struct BMeshFromMeshParams){
                         .calc_face_normal = true,
                     }));

  BM_mesh_bm_from_me(bm,
                     sculpt_mesh,
                     &((struct BMeshFromMeshParams){
                         .calc_face_normal = true,
                     }));

  const int looptris_tot = poly_to_tri_count(bm->totface, bm->totloop);
  int tottri;
  BMLoop *(*looptris)[3];
  looptris = MEM_malloc_arrayN(looptris_tot, sizeof(*looptris), __func__);
  BM_mesh_calc_tessellation_beauty(bm, looptris, &tottri);

  BMIter iter;
  int i;
  const int i_faces_end = trim_mesh->totpoly;

  /* We need face normals because of 'BM_face_split_edgenet'
   * we could calculate on the fly too (before calling split). */

  const short ob_src_totcol = trim_mesh->totcol;
  short *material_remap = BLI_array_alloca(material_remap, ob_src_totcol ? ob_src_totcol : 1);

  BMFace *efa;
  i = 0;
  BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
    normalize_v3(efa->no);

    /* Temp tag to test which side split faces are from. */
    BM_elem_flag_enable(efa, BM_ELEM_DRAW);

    /* Remap material. */
    if (efa->mat_nr < ob_src_totcol) {
      efa->mat_nr = material_remap[efa->mat_nr];
    }

    if (++i == i_faces_end) {
      break;
    }
  }

  /* Join does not do a boolean operation, it just adds the geometry. */
  if (trim_operation->mode != SCULPT_GESTURE_TRIM_JOIN) {
    int boolean_mode = 0;
    switch (trim_operation->mode) {
      case SCULPT_GESTURE_TRIM_INTERSECT:
        boolean_mode = eBooleanModifierOp_Intersect;
        break;
      case SCULPT_GESTURE_TRIM_DIFFERENCE:
        boolean_mode = eBooleanModifierOp_Difference;
        break;
      case SCULPT_GESTURE_TRIM_UNION:
        boolean_mode = eBooleanModifierOp_Union;
        break;
      case SCULPT_GESTURE_TRIM_JOIN:
        BLI_assert(false);
        break;
    }
    BM_mesh_boolean(
        bm, looptris, tottri, bm_face_isect_pair, NULL, 2, true, true, false, boolean_mode);
  }

  MEM_freeN(looptris);

  Mesh *result = BKE_mesh_from_bmesh_nomain(bm,
                                            (&(struct BMeshToMeshParams){
                                                .calc_object_remap = false,
                                            }),
                                            sculpt_mesh);
  BM_mesh_free(bm);
  result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
  BKE_mesh_nomain_to_mesh(
      result, sgcontext->vc.obact->data, sgcontext->vc.obact, &CD_MASK_MESH, true);
}

static void sculpt_gesture_trim_begin(bContext *C, SculptGestureContext *sgcontext)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  sculpt_gesture_trim_calculate_depth(sgcontext);
  sculpt_gesture_trim_geometry_generate(sgcontext);
  BKE_sculpt_update_object_for_edit(depsgraph, sgcontext->vc.obact, true, false, false);
  SCULPT_undo_push_node(sgcontext->vc.obact, NULL, SCULPT_UNDO_GEOMETRY);
}

static void sculpt_gesture_trim_apply_for_symmetry_pass(bContext *UNUSED(C),
                                                        SculptGestureContext *sgcontext)
{
  SculptGestureTrimOperation *trim_operation = (SculptGestureTrimOperation *)sgcontext->operation;
  Mesh *trim_mesh = trim_operation->mesh;
  for (int i = 0; i < trim_mesh->totvert; i++) {
    flip_v3_v3(trim_mesh->mvert[i].co, trim_operation->true_mesh_co[i], sgcontext->symmpass);
  }
  sculpt_gesture_trim_normals_update(sgcontext);
  sculpt_gesture_apply_trim(sgcontext);
}

static void sculpt_gesture_trim_end(bContext *UNUSED(C), SculptGestureContext *sgcontext)
{
  Object *object = sgcontext->vc.obact;
  SculptSession *ss = object->sculpt;
  ss->face_sets = CustomData_get_layer(&((Mesh *)object->data)->pdata, CD_SCULPT_FACE_SETS);
  if (ss->face_sets) {
    /* Assign a new Face Set ID to the new faces created by the trim operation. */
    const int next_face_set_id = ED_sculpt_face_sets_find_next_available_id(object->data);
    ED_sculpt_face_sets_initialize_none_to_id(object->data, next_face_set_id);
  }

  sculpt_gesture_trim_geometry_free(sgcontext);

  SCULPT_undo_push_node(sgcontext->vc.obact, NULL, SCULPT_UNDO_GEOMETRY);
  BKE_mesh_batch_cache_dirty_tag(sgcontext->vc.obact->data, BKE_MESH_BATCH_DIRTY_ALL);
  DEG_id_tag_update(&sgcontext->vc.obact->id, ID_RECALC_GEOMETRY);
}

static void sculpt_gesture_init_trim_properties(SculptGestureContext *sgcontext, wmOperator *op)
{
  sgcontext->operation = MEM_callocN(sizeof(SculptGestureTrimOperation), "Trim Operation");

  SculptGestureTrimOperation *trim_operation = (SculptGestureTrimOperation *)sgcontext->operation;

  trim_operation->op.sculpt_gesture_begin = sculpt_gesture_trim_begin;
  trim_operation->op.sculpt_gesture_apply_for_symmetry_pass =
      sculpt_gesture_trim_apply_for_symmetry_pass;
  trim_operation->op.sculpt_gesture_end = sculpt_gesture_trim_end;

  trim_operation->mode = RNA_enum_get(op->ptr, "trim_mode");
  trim_operation->use_cursor_depth = RNA_boolean_get(op->ptr, "use_cursor_depth");
  trim_operation->orientation = RNA_enum_get(op->ptr, "trim_orientation");

  /* If the cursor was not over the mesh, force the orientation to view. */
  if (!sgcontext->ss->gesture_initial_hit) {
    trim_operation->orientation = SCULPT_GESTURE_TRIM_ORIENTATION_VIEW;
  }
}

static void sculpt_trim_gesture_operator_properties(wmOperatorType *ot)
{
  RNA_def_enum(ot->srna,
               "trim_mode",
               prop_trim_operation_types,
               SCULPT_GESTURE_TRIM_DIFFERENCE,
               "Trim Mode",
               NULL);
  RNA_def_boolean(
      ot->srna,
      "use_cursor_depth",
      false,
      "Use Cursor for Depth",
      "Use cursor location and radius for the dimensions and position of the trimming shape");
  RNA_def_enum(ot->srna,
               "trim_orientation",
               prop_trim_orientation_types,
               SCULPT_GESTURE_TRIM_ORIENTATION_VIEW,
               "Shape Orientation",
               NULL);
}

/* Project Gesture Operation. */

typedef struct SculptGestureProjectOperation {
  SculptGestureOperation operation;
} SculptGestureProjectOperation;

static void sculpt_gesture_project_begin(bContext *C, SculptGestureContext *sgcontext)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  BKE_sculpt_update_object_for_edit(depsgraph, sgcontext->vc.obact, false, false, false);
}

static void project_line_gesture_apply_task_cb(void *__restrict userdata,
                                               const int i,
                                               const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptGestureContext *sgcontext = userdata;

  PBVHNode *node = sgcontext->nodes[i];
  PBVHVertexIter vd;
  bool any_updated = false;

  SCULPT_undo_push_node(sgcontext->vc.obact, node, SCULPT_UNDO_COORDS);

  BKE_pbvh_vertex_iter_begin(sgcontext->ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
  {
    if (!sculpt_gesture_is_vertex_effected(sgcontext, &vd)) {
      continue;
    }

    float projected_pos[3];
    closest_to_plane_v3(projected_pos, sgcontext->line.plane, vd.co);

    float disp[3];
    sub_v3_v3v3(disp, projected_pos, vd.co);
    const float mask = vd.mask ? *vd.mask : 0.0f;
    mul_v3_fl(disp, 1.0f - mask);
    if (is_zero_v3(disp)) {
      continue;
    }
    add_v3_v3(vd.co, disp);
    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
    any_updated = true;
  }
  BKE_pbvh_vertex_iter_end;

  if (any_updated) {
    BKE_pbvh_node_mark_update(node);
  }
}

static void sculpt_gesture_project_apply_for_symmetry_pass(bContext *UNUSED(C),
                                                           SculptGestureContext *sgcontext)
{
  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, sgcontext->totnode);

  switch (sgcontext->shape_type) {
    case SCULPT_GESTURE_SHAPE_LINE:
      BLI_task_parallel_range(
          0, sgcontext->totnode, sgcontext, project_line_gesture_apply_task_cb, &settings);
      break;
    case SCULPT_GESTURE_SHAPE_LASSO:
    case SCULPT_GESTURE_SHAPE_BOX:
      /* Gesture shape projection not implemented yet. */
      BLI_assert(false);
      break;
  }
}

static void sculpt_gesture_project_end(bContext *C, SculptGestureContext *sgcontext)
{
  SculptSession *ss = sgcontext->ss;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  if (ss->deform_modifiers_active || ss->shapekey_active) {
    SCULPT_flush_stroke_deform(sd, sgcontext->vc.obact, true);
  }

  SCULPT_flush_update_step(C, SCULPT_UPDATE_COORDS);
  SCULPT_flush_update_done(C, sgcontext->vc.obact, SCULPT_UPDATE_COORDS);
}

static void sculpt_gesture_init_project_properties(SculptGestureContext *sgcontext,
                                                   wmOperator *UNUSED(op))
{
  sgcontext->operation = MEM_callocN(sizeof(SculptGestureFaceSetOperation), "Project Operation");

  SculptGestureProjectOperation *project_operation = (SculptGestureProjectOperation *)
                                                         sgcontext->operation;

  project_operation->operation.sculpt_gesture_begin = sculpt_gesture_project_begin;
  project_operation->operation.sculpt_gesture_apply_for_symmetry_pass =
      sculpt_gesture_project_apply_for_symmetry_pass;
  project_operation->operation.sculpt_gesture_end = sculpt_gesture_project_end;
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

static int paint_mask_gesture_line_exec(bContext *C, wmOperator *op)
{
  SculptGestureContext *sgcontext = sculpt_gesture_init_from_line(C, op);
  if (!sgcontext) {
    return OPERATOR_CANCELLED;
  }
  sculpt_gesture_init_mask_properties(sgcontext, op);
  sculpt_gesture_apply(C, sgcontext);
  sculpt_gesture_context_free(sgcontext);
  return OPERATOR_FINISHED;
}

static int face_set_gesture_box_exec(bContext *C, wmOperator *op)
{
  SculptGestureContext *sgcontext = sculpt_gesture_init_from_box(C, op);
  if (!sgcontext) {
    return OPERATOR_CANCELLED;
  }
  sculpt_gesture_init_face_set_properties(sgcontext, op);
  sculpt_gesture_apply(C, sgcontext);
  sculpt_gesture_context_free(sgcontext);
  return OPERATOR_FINISHED;
}

static int face_set_gesture_lasso_exec(bContext *C, wmOperator *op)
{
  SculptGestureContext *sgcontext = sculpt_gesture_init_from_lasso(C, op);
  if (!sgcontext) {
    return OPERATOR_CANCELLED;
  }
  sculpt_gesture_init_face_set_properties(sgcontext, op);
  sculpt_gesture_apply(C, sgcontext);
  sculpt_gesture_context_free(sgcontext);
  return OPERATOR_FINISHED;
}

static int sculpt_trim_gesture_box_exec(bContext *C, wmOperator *op)
{
  Object *object = CTX_data_active_object(C);
  SculptSession *ss = object->sculpt;
  if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
    /* Not supported in Multires and Dyntopo. */
    return OPERATOR_CANCELLED;
  }

  if (ss->totvert == 0) {
    /* No geometry to trim or to detect a valid position for the trimming shape. */
    return OPERATOR_CANCELLED;
  }

  SculptGestureContext *sgcontext = sculpt_gesture_init_from_box(C, op);
  if (!sgcontext) {
    return OPERATOR_CANCELLED;
  }

  sculpt_gesture_init_trim_properties(sgcontext, op);
  sculpt_gesture_apply(C, sgcontext);
  sculpt_gesture_context_free(sgcontext);
  return OPERATOR_FINISHED;
}

static int sculpt_trim_gesture_box_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  SculptCursorGeometryInfo sgi;
  float mouse[2] = {event->mval[0], event->mval[1]};
  SCULPT_vertex_random_access_ensure(ss);
  ss->gesture_initial_hit = SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false);
  if (ss->gesture_initial_hit) {
    copy_v3_v3(ss->gesture_initial_location, sgi.location);
    copy_v3_v3(ss->gesture_initial_normal, sgi.normal);
  }

  return WM_gesture_box_invoke(C, op, event);
}

static int sculpt_trim_gesture_lasso_exec(bContext *C, wmOperator *op)
{
  Object *object = CTX_data_active_object(C);
  SculptSession *ss = object->sculpt;
  if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
    /* Not supported in Multires and Dyntopo. */
    return OPERATOR_CANCELLED;
  }

  if (ss->totvert == 0) {
    /* No geometry to trim or to detect a valid position for the trimming shape. */
    return OPERATOR_CANCELLED;
  }

  SculptGestureContext *sgcontext = sculpt_gesture_init_from_lasso(C, op);
  if (!sgcontext) {
    return OPERATOR_CANCELLED;
  }
  sculpt_gesture_init_trim_properties(sgcontext, op);
  sculpt_gesture_apply(C, sgcontext);
  sculpt_gesture_context_free(sgcontext);
  return OPERATOR_FINISHED;
}

static int sculpt_trim_gesture_lasso_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  SculptCursorGeometryInfo sgi;
  float mouse[2] = {event->mval[0], event->mval[1]};
  SCULPT_vertex_random_access_ensure(ss);
  ss->gesture_initial_hit = SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false);
  if (ss->gesture_initial_hit) {
    copy_v3_v3(ss->gesture_initial_location, sgi.location);
    copy_v3_v3(ss->gesture_initial_normal, sgi.normal);
  }

  return WM_gesture_lasso_invoke(C, op, event);
}

static int project_gesture_line_exec(bContext *C, wmOperator *op)
{
  SculptGestureContext *sgcontext = sculpt_gesture_init_from_line(C, op);
  if (!sgcontext) {
    return OPERATOR_CANCELLED;
  }
  sculpt_gesture_init_project_properties(sgcontext, op);
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

void PAINT_OT_mask_line_gesture(wmOperatorType *ot)
{
  ot->name = "Mask Line Gesture";
  ot->idname = "PAINT_OT_mask_line_gesture";
  ot->description = "Add mask to the right of a line as you move the brush";

  ot->invoke = WM_gesture_straightline_active_side_invoke;
  ot->modal = WM_gesture_straightline_oneshot_modal;
  ot->exec = paint_mask_gesture_line_exec;

  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER;

  /* Properties. */
  WM_operator_properties_gesture_straightline(ot, WM_CURSOR_EDIT);
  sculpt_gesture_operator_properties(ot);

  paint_mask_gesture_operator_properties(ot);
}

void SCULPT_OT_face_set_lasso_gesture(wmOperatorType *ot)
{
  ot->name = "Face Set Lasso Gesture";
  ot->idname = "SCULPT_OT_face_set_lasso_gesture";
  ot->description = "Add face set within the lasso as you move the brush";

  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = face_set_gesture_lasso_exec;

  /* Properties. */
  WM_operator_properties_gesture_lasso(ot);
  sculpt_gesture_operator_properties(ot);
}

void SCULPT_OT_face_set_box_gesture(wmOperatorType *ot)
{
  ot->name = "Face Set Box Gesture";
  ot->idname = "SCULPT_OT_face_set_box_gesture";
  ot->description = "Add face set within the box as you move the brush";

  ot->invoke = WM_gesture_box_invoke;
  ot->modal = WM_gesture_box_modal;
  ot->exec = face_set_gesture_box_exec;

  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER;

  /* Properties. */
  WM_operator_properties_border(ot);
  sculpt_gesture_operator_properties(ot);
}

void SCULPT_OT_trim_lasso_gesture(wmOperatorType *ot)
{
  ot->name = "Trim Lasso Gesture";
  ot->idname = "SCULPT_OT_trim_lasso_gesture";
  ot->description = "Trims the mesh within the lasso as you move the brush";

  ot->invoke = sculpt_trim_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = sculpt_trim_gesture_lasso_exec;

  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER;

  /* Properties. */
  WM_operator_properties_gesture_lasso(ot);
  sculpt_gesture_operator_properties(ot);

  sculpt_trim_gesture_operator_properties(ot);
}

void SCULPT_OT_trim_box_gesture(wmOperatorType *ot)
{
  ot->name = "Trim Box Gesture";
  ot->idname = "SCULPT_OT_trim_box_gesture";
  ot->description = "Trims the mesh within the box as you move the brush";

  ot->invoke = sculpt_trim_gesture_box_invoke;
  ot->modal = WM_gesture_box_modal;
  ot->exec = sculpt_trim_gesture_box_exec;

  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER;

  /* Properties. */
  WM_operator_properties_border(ot);
  sculpt_gesture_operator_properties(ot);

  sculpt_trim_gesture_operator_properties(ot);
}

void SCULPT_OT_project_line_gesture(wmOperatorType *ot)
{
  ot->name = "Project Line Gesture";
  ot->idname = "SCULPT_OT_project_line_gesture";
  ot->description = "Project the geometry onto a plane defined by a line";

  ot->invoke = WM_gesture_straightline_active_side_invoke;
  ot->modal = WM_gesture_straightline_oneshot_modal;
  ot->exec = project_gesture_line_exec;

  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER;

  /* Properties. */
  WM_operator_properties_gesture_straightline(ot, WM_CURSOR_EDIT);
  sculpt_gesture_operator_properties(ot);
}
