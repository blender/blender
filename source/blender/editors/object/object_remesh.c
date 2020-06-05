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
 * The Original Code is Copyright (C) 2019 by Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup edobj
 */

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mirror.h"
#include "BKE_mesh_remesh_voxel.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_shrinkwrap.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_space_api.h"
#include "ED_undo.h"
#include "ED_view3d.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "GPU_draw.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "UI_interface.h"

#include "BLF_api.h"

#include "object_intern.h"  // own include

/* TODO(sebpa): unstable, can lead to unrecoverable errors. */
// #define USE_MESH_CURVATURE

/* -------------------------------------------------------------------- */
/** \name Voxel Remesh Operator
 * \{ */

static bool object_remesh_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  if (ob == NULL) {
    return false;
  }

  if (BKE_object_is_in_editmode(ob)) {
    CTX_wm_operator_poll_msg_set(C, "The remesher cannot run from edit mode");
    return false;
  }

  if (ob->mode == OB_MODE_SCULPT && ob->sculpt->bm) {
    CTX_wm_operator_poll_msg_set(C, "The remesher cannot run with dyntopo activated");
    return false;
  }

  if (BKE_modifiers_uses_multires(ob)) {
    CTX_wm_operator_poll_msg_set(
        C, "The remesher cannot run with a Multires modifier in the modifier stack");
    return false;
  }

  return ED_operator_object_active_editable_mesh(C);
}

static int voxel_remesh_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);

  Mesh *mesh = ob->data;
  Mesh *new_mesh;

  if (mesh->remesh_voxel_size <= 0.0f) {
    BKE_report(op->reports, RPT_ERROR, "Voxel remesher cannot run with a voxel size of 0.0");
    return OPERATOR_CANCELLED;
  }

  float isovalue = 0.0f;
  if (mesh->flag & ME_REMESH_REPROJECT_VOLUME) {
    isovalue = mesh->remesh_voxel_size * 0.3f;
  }

  new_mesh = BKE_mesh_remesh_voxel_to_mesh_nomain(
      mesh, mesh->remesh_voxel_size, mesh->remesh_voxel_adaptivity, isovalue);

  if (!new_mesh) {
    BKE_report(op->reports, RPT_ERROR, "Voxel remesher failed to create mesh");
    return OPERATOR_CANCELLED;
  }

  if (ob->mode == OB_MODE_SCULPT) {
    ED_sculpt_undo_geometry_begin(ob, op->type->name);
  }

  if (mesh->flag & ME_REMESH_FIX_POLES && mesh->remesh_voxel_adaptivity <= 0.0f) {
    new_mesh = BKE_mesh_remesh_voxel_fix_poles(new_mesh);
    BKE_mesh_calc_normals(new_mesh);
  }

  if (mesh->flag & ME_REMESH_REPROJECT_VOLUME || mesh->flag & ME_REMESH_REPROJECT_PAINT_MASK ||
      mesh->flag & ME_REMESH_REPROJECT_SCULPT_FACE_SETS) {
    BKE_mesh_runtime_clear_geometry(mesh);
  }

  if (mesh->flag & ME_REMESH_REPROJECT_VOLUME) {
    BKE_shrinkwrap_remesh_target_project(new_mesh, mesh, ob);
  }

  if (mesh->flag & ME_REMESH_REPROJECT_PAINT_MASK) {
    BKE_mesh_remesh_reproject_paint_mask(new_mesh, mesh);
  }

  if (mesh->flag & ME_REMESH_REPROJECT_SCULPT_FACE_SETS) {
    BKE_remesh_reproject_sculpt_face_sets(new_mesh, mesh);
  }

  BKE_mesh_nomain_to_mesh(new_mesh, mesh, ob, &CD_MASK_MESH, true);

  if (mesh->flag & ME_REMESH_SMOOTH_NORMALS) {
    BKE_mesh_smooth_flag_set(ob->data, true);
  }

  if (ob->mode == OB_MODE_SCULPT) {
    ED_sculpt_undo_geometry_end(ob);
  }

  BKE_mesh_batch_cache_dirty_tag(ob->data, BKE_MESH_BATCH_DIRTY_ALL);
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_voxel_remesh(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Voxel Remesh";
  ot->description =
      "Calculates a new manifold mesh based on the volume of the current mesh. All data layers "
      "will be lost";
  ot->idname = "OBJECT_OT_voxel_remesh";

  /* api callbacks */
  ot->poll = object_remesh_poll;
  ot->exec = voxel_remesh_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Voxel Size Operator
 * \{ */

#define VOXEL_SIZE_EDIT_MAX_GRIDS_LINES 500
#define VOXEL_SIZE_EDIT_MAX_STR_LEN 20

typedef struct VoxelSizeEditCustomData {
  void *draw_handle;
  Object *active_object;

  float init_mval[2];
  float slow_mval[2];

  bool slow_mode;

  float init_voxel_size;
  float slow_voxel_size;
  float voxel_size;

  float preview_plane[4][3];

  float text_mat[4][4];
} VoxelSizeEditCustomData;

static void voxel_size_parallel_lines_draw(uint pos3d,
                                           const float initial_co[3],
                                           const float end_co[3],
                                           const float length_co[3],
                                           const float spacing)
{
  const float total_len = len_v3v3(initial_co, end_co);
  const int tot_lines = (int)(total_len / spacing);
  const int tot_lines_half = (tot_lines / 2) + 1;
  float spacing_dir[3], lines_start[3];
  float line_dir[3];
  sub_v3_v3v3(spacing_dir, end_co, initial_co);
  normalize_v3(spacing_dir);

  sub_v3_v3v3(line_dir, length_co, initial_co);

  if (tot_lines > VOXEL_SIZE_EDIT_MAX_GRIDS_LINES || tot_lines <= 1) {
    return;
  }

  mid_v3_v3v3(lines_start, initial_co, end_co);

  immBegin(GPU_PRIM_LINES, (uint)tot_lines_half * 2);
  for (int i = 0; i < tot_lines_half; i++) {
    float line_start[3];
    float line_end[3];
    madd_v3_v3v3fl(line_start, lines_start, spacing_dir, spacing * i);
    add_v3_v3v3(line_end, line_start, line_dir);
    immVertex3fv(pos3d, line_start);
    immVertex3fv(pos3d, line_end);
  }
  immEnd();

  mul_v3_fl(spacing_dir, -1.0f);

  immBegin(GPU_PRIM_LINES, (uint)(tot_lines_half - 1) * 2);
  for (int i = 1; i < tot_lines_half; i++) {
    float line_start[3];
    float line_end[3];
    madd_v3_v3v3fl(line_start, lines_start, spacing_dir, spacing * i);
    add_v3_v3v3(line_end, line_start, line_dir);
    immVertex3fv(pos3d, line_start);
    immVertex3fv(pos3d, line_end);
  }
  immEnd();
}

static void voxel_size_edit_draw(const bContext *UNUSED(C), ARegion *UNUSED(ar), void *arg)
{
  VoxelSizeEditCustomData *cd = arg;

  GPU_blend(true);
  GPU_line_smooth(true);

  uint pos3d = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  GPU_matrix_push();
  GPU_matrix_mul(cd->active_object->obmat);

  /* Draw Rect */
  immUniformColor4f(0.9f, 0.9f, 0.9f, 0.8f);
  GPU_line_width(3.0f);

  immBegin(GPU_PRIM_LINES, 8);
  immVertex3fv(pos3d, cd->preview_plane[0]);
  immVertex3fv(pos3d, cd->preview_plane[1]);

  immVertex3fv(pos3d, cd->preview_plane[1]);
  immVertex3fv(pos3d, cd->preview_plane[2]);

  immVertex3fv(pos3d, cd->preview_plane[2]);
  immVertex3fv(pos3d, cd->preview_plane[3]);

  immVertex3fv(pos3d, cd->preview_plane[3]);
  immVertex3fv(pos3d, cd->preview_plane[0]);
  immEnd();

  /* Draw Grid */
  GPU_line_width(1.0f);

  const float total_len = len_v3v3(cd->preview_plane[0], cd->preview_plane[1]);
  const int tot_lines = (int)(total_len / cd->voxel_size);

  /* Smoothstep to reduce the alpha of the grid as the line number increases. */
  const float a = VOXEL_SIZE_EDIT_MAX_GRIDS_LINES * 0.1f;
  const float b = VOXEL_SIZE_EDIT_MAX_GRIDS_LINES;
  const float x = clamp_f((tot_lines - a) / (b - a), 0.0f, 1.0);
  const float alpha_factor = 1.0f - (x * x * (3.0f - 2.0f * x));

  immUniformColor4f(0.9f, 0.9f, 0.9f, 0.75f * alpha_factor);
  voxel_size_parallel_lines_draw(
      pos3d, cd->preview_plane[0], cd->preview_plane[1], cd->preview_plane[3], cd->voxel_size);
  voxel_size_parallel_lines_draw(
      pos3d, cd->preview_plane[1], cd->preview_plane[2], cd->preview_plane[0], cd->voxel_size);

  /* Draw text */
  const uiStyle *style = UI_style_get();
  const uiFontStyle *fstyle = &style->widget;
  const int fontid = fstyle->uifont_id;
  float strwidth, strheight;
  short fstyle_points = fstyle->points;
  char str[VOXEL_SIZE_EDIT_MAX_STR_LEN];
  short strdrawlen = 0;

  BLI_snprintf(str, VOXEL_SIZE_EDIT_MAX_STR_LEN, "%.4f", cd->voxel_size);
  strdrawlen = BLI_strlen_utf8(str);

  immUnbindProgram();

  GPU_matrix_push();
  GPU_matrix_mul(cd->text_mat);
  BLF_size(fontid, 10.0f * fstyle_points, U.dpi);
  BLF_color3f(fontid, 1.0f, 1.0f, 1.0f);
  BLF_width_and_height(fontid, str, strdrawlen, &strwidth, &strheight);
  BLF_position(fontid, -0.5f * strwidth, -0.5f * strheight, 0.0f);
  BLF_draw(fontid, str, strdrawlen);
  GPU_matrix_pop();

  GPU_matrix_pop();

  GPU_blend(false);
  GPU_line_smooth(false);
}

static void voxel_size_edit_cancel(bContext *C, wmOperator *op)
{
  ARegion *ar = CTX_wm_region(C);
  VoxelSizeEditCustomData *cd = op->customdata;

  ED_region_draw_cb_exit(ar->type, cd->draw_handle);

  MEM_freeN(op->customdata);

  ED_workspace_status_text(C, NULL);
}

static int voxel_size_edit_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  VoxelSizeEditCustomData *cd = op->customdata;
  Object *active_object = cd->active_object;
  Mesh *mesh = (Mesh *)active_object->data;

  /* Cancel modal operator */
  if ((event->type == EVT_ESCKEY && event->val == KM_PRESS) ||
      (event->type == RIGHTMOUSE && event->val == KM_PRESS)) {
    voxel_size_edit_cancel(C, op);
    ED_region_tag_redraw(ar);
    return OPERATOR_FINISHED;
  }

  /* Finish modal operator */
  if ((event->type == LEFTMOUSE && event->val == KM_RELEASE) ||
      (event->type == EVT_RETKEY && event->val == KM_PRESS) ||
      (event->type == EVT_PADENTER && event->val == KM_PRESS)) {
    ED_region_draw_cb_exit(ar->type, cd->draw_handle);
    mesh->remesh_voxel_size = cd->voxel_size;
    MEM_freeN(op->customdata);
    ED_region_tag_redraw(ar);
    return OPERATOR_FINISHED;
  }

  float mval[2] = {event->mval[0], event->mval[1]};

  float d = cd->init_mval[0] - mval[0];

  if (cd->slow_mode) {
    d = cd->slow_mval[0] - mval[0];
  }

  if (event->ctrl) {
    /* Linear mode, enables jumping to any voxel size. */
    d = d * 0.0005f;
  }
  else {
    /* Multiply d by the initial voxel size to prevent uncontrollable speeds when using low voxel
     * sizes. */
    /* When the voxel size is slower, it needs more precision. */
    d = d * min_ff(pow2f(cd->init_voxel_size), 0.1f) * 0.05f;
  }
  if (cd->slow_mode) {
    cd->voxel_size = cd->slow_voxel_size + d * 0.05f;
  }
  else {
    cd->voxel_size = cd->init_voxel_size + d;
  }

  if (event->type == EVT_LEFTSHIFTKEY && event->val == KM_PRESS) {
    cd->slow_mode = true;
    copy_v2_v2(cd->slow_mval, mval);
    cd->slow_voxel_size = cd->voxel_size;
  }
  if (event->type == EVT_LEFTSHIFTKEY && event->val == KM_RELEASE) {
    cd->slow_mode = false;
    cd->slow_voxel_size = 0.0f;
  }

  cd->voxel_size = clamp_f(cd->voxel_size, 0.0001f, 1.0f);

  ED_region_tag_redraw(ar);
  return OPERATOR_RUNNING_MODAL;
}

static int voxel_size_edit_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  Object *active_object = CTX_data_active_object(C);
  Mesh *mesh = (Mesh *)active_object->data;

  VoxelSizeEditCustomData *cd = MEM_callocN(sizeof(VoxelSizeEditCustomData),
                                            "Voxel Size Edit OP Custom Data");

  /* Initial operator Custom Data setup. */
  cd->draw_handle = ED_region_draw_cb_activate(
      ar->type, voxel_size_edit_draw, cd, REGION_DRAW_POST_VIEW);
  cd->active_object = active_object;
  cd->init_mval[0] = event->mval[0];
  cd->init_mval[1] = event->mval[1];
  cd->init_voxel_size = mesh->remesh_voxel_size;
  cd->voxel_size = mesh->remesh_voxel_size;
  op->customdata = cd;

  /* Select the front facing face of the mesh boundig box. */
  BoundBox *bb = BKE_mesh_boundbox_get(cd->active_object);

  /* Indices of the Bounding Box faces. */
  int BB_faces[6][4] = {
      {3, 0, 4, 7},
      {1, 2, 6, 5},
      {3, 2, 1, 0},
      {4, 5, 6, 7},
      {0, 1, 5, 4},
      {2, 3, 7, 6},
  };

  copy_v3_v3(cd->preview_plane[0], bb->vec[BB_faces[0][0]]);
  copy_v3_v3(cd->preview_plane[1], bb->vec[BB_faces[0][1]]);
  copy_v3_v3(cd->preview_plane[2], bb->vec[BB_faces[0][2]]);
  copy_v3_v3(cd->preview_plane[3], bb->vec[BB_faces[0][3]]);

  RegionView3D *rv3d = CTX_wm_region_view3d(C);

  float mat[3][3];
  float current_normal[3];
  float view_normal[3] = {0.0f, 0.0f, 1.0f};

  /* Calculate the view normal. */
  invert_m4_m4(active_object->imat, active_object->obmat);
  copy_m3_m4(mat, rv3d->viewinv);
  mul_m3_v3(mat, view_normal);
  copy_m3_m4(mat, active_object->imat);
  mul_m3_v3(mat, view_normal);
  normalize_v3(view_normal);

  normal_tri_v3(current_normal, cd->preview_plane[0], cd->preview_plane[1], cd->preview_plane[2]);

  float min_dot = dot_v3v3(current_normal, view_normal);
  float current_dot = 1;

  /* Check if there is a face that is more aligned towards the view. */
  for (int i = 0; i < 6; i++) {
    normal_tri_v3(
        current_normal, bb->vec[BB_faces[i][0]], bb->vec[BB_faces[i][1]], bb->vec[BB_faces[i][2]]);
    current_dot = dot_v3v3(current_normal, view_normal);

    if (current_dot < min_dot) {
      min_dot = current_dot;
      copy_v3_v3(cd->preview_plane[0], bb->vec[BB_faces[i][0]]);
      copy_v3_v3(cd->preview_plane[1], bb->vec[BB_faces[i][1]]);
      copy_v3_v3(cd->preview_plane[2], bb->vec[BB_faces[i][2]]);
      copy_v3_v3(cd->preview_plane[3], bb->vec[BB_faces[i][3]]);
    }
  }

  /* Matrix calculation to position the text in 3D space. */
  float text_pos[3];
  float scale_mat[4][4];

  float d_a[3], d_b[3];
  float d_a_proj[2], d_b_proj[2];
  float preview_plane_proj[4][3];
  float y_axis_proj[2] = {0.0f, 1.0f};

  mid_v3_v3v3(text_pos, cd->preview_plane[0], cd->preview_plane[2]);

  /* Project the selected face in the previous step of the Bounding Box. */
  for (int i = 0; i < 4; i++) {
    ED_view3d_project(ar, cd->preview_plane[i], preview_plane_proj[i]);
  }

  /* Get the initial X and Y axis of the basis from the edges of the Bounding Box face. */
  sub_v3_v3v3(d_a, cd->preview_plane[1], cd->preview_plane[0]);
  sub_v3_v3v3(d_b, cd->preview_plane[3], cd->preview_plane[0]);
  normalize_v3(d_a);
  normalize_v3(d_b);

  /* Project the X and Y axis. */
  sub_v2_v2v2(d_a_proj, preview_plane_proj[1], preview_plane_proj[0]);
  sub_v2_v2v2(d_b_proj, preview_plane_proj[3], preview_plane_proj[0]);
  normalize_v2(d_a_proj);
  normalize_v2(d_b_proj);

  unit_m4(cd->text_mat);

  /* Select the axis that is aligned with the view Y axis to use it as the basis Y. */
  if (fabsf(dot_v2v2(d_a_proj, y_axis_proj)) > fabsf(dot_v2v2(d_b_proj, y_axis_proj))) {
    copy_v3_v3(cd->text_mat[0], d_b);
    copy_v3_v3(cd->text_mat[1], d_a);

    /* Flip the X and Y basis vectors to make sure they always point upwards and to the right. */
    if (d_b_proj[0] < 0.0f) {
      mul_v3_fl(cd->text_mat[0], -1.0f);
    }
    if (d_a_proj[1] < 0.0f) {
      mul_v3_fl(cd->text_mat[1], -1.0f);
    }
  }
  else {
    copy_v3_v3(cd->text_mat[0], d_a);
    copy_v3_v3(cd->text_mat[1], d_b);
    if (d_a_proj[0] < 0.0f) {
      mul_v3_fl(cd->text_mat[0], -1.0f);
    }
    if (d_b_proj[1] < 0.0f) {
      mul_v3_fl(cd->text_mat[1], -1.0f);
    }
  }

  /* Use the Bounding Box face normal as the basis Z. */
  normal_tri_v3(cd->text_mat[2], cd->preview_plane[0], cd->preview_plane[1], cd->preview_plane[2]);

  /* Write the text position into the matrix. */
  copy_v3_v3(cd->text_mat[3], text_pos);

  /* Scale the text.  */
  unit_m4(scale_mat);
  scale_m4_fl(scale_mat, 0.0008f);
  mul_m4_m4_post(cd->text_mat, scale_mat);

  WM_event_add_modal_handler(C, op);

  ED_region_tag_redraw(ar);

  const char *status_str = TIP_(
      "Move the mouse to change the voxel size. LMB: confirm size, ESC/RMB: cancel");
  ED_workspace_status_text(C, status_str);

  return OPERATOR_RUNNING_MODAL;
}

static bool voxel_size_edit_poll(bContext *C)
{
  return CTX_wm_region_view3d(C) && object_remesh_poll(C);
}

void OBJECT_OT_voxel_size_edit(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Edit Voxel Size";
  ot->description = "Modify the mesh voxel size interactively used in the voxel remesher";
  ot->idname = "OBJECT_OT_voxel_size_edit";

  /* api callbacks */
  ot->poll = voxel_size_edit_poll;
  ot->invoke = voxel_size_edit_invoke;
  ot->modal = voxel_size_edit_modal;
  ot->cancel = voxel_size_edit_cancel;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quadriflow Remesh Operator
 * \{ */

#define QUADRIFLOW_MIRROR_BISECT_TOLERANCE 0.005f

enum {
  QUADRIFLOW_REMESH_RATIO = 1,
  QUADRIFLOW_REMESH_EDGE_LENGTH,
  QUADRIFLOW_REMESH_FACES,
};

typedef enum eSymmetryAxes {
  SYMMETRY_AXES_X = (1 << 0),
  SYMMETRY_AXES_Y = (1 << 1),
  SYMMETRY_AXES_Z = (1 << 2),
} eSymmetryAxes;

typedef struct QuadriFlowJob {
  /* from wmJob */
  struct Object *owner;
  short *stop, *do_update;
  float *progress;

  int target_faces;
  int seed;
  bool use_paint_symmetry;
  eSymmetryAxes symmetry_axes;

  bool use_preserve_sharp;
  bool use_preserve_boundary;
  bool use_mesh_curvature;

  bool preserve_paint_mask;
  bool smooth_normals;

  int success;
  bool is_nonblocking_job;
} QuadriFlowJob;

static bool mesh_is_manifold_consistent(Mesh *mesh)
{
  /* In this check we count boundary edges as manifold. Additionally, we also
   * check that the direction of the faces are consistent and doesn't suddenly
   * flip
   */

  bool is_manifold_consistent = true;
  const MLoop *mloop = mesh->mloop;
  char *edge_faces = (char *)MEM_callocN(mesh->totedge * sizeof(char), "remesh_manifold_check");
  int *edge_vert = (int *)MEM_malloc_arrayN(
      mesh->totedge, sizeof(uint), "remesh_consistent_check");

  for (uint i = 0; i < mesh->totedge; i++) {
    edge_vert[i] = -1;
  }

  for (uint loop_idx = 0; loop_idx < mesh->totloop; loop_idx++) {
    const MLoop *loop = &mloop[loop_idx];
    edge_faces[loop->e] += 1;
    if (edge_faces[loop->e] > 2) {
      is_manifold_consistent = false;
      break;
    }

    if (edge_vert[loop->e] == -1) {
      edge_vert[loop->e] = loop->v;
    }
    else if (edge_vert[loop->e] == loop->v) {
      /* Mesh has flips in the surface so it is non consistent */
      is_manifold_consistent = false;
      break;
    }
  }

  if (is_manifold_consistent) {
    /* check for wire edges */
    for (uint i = 0; i < mesh->totedge; i++) {
      if (edge_faces[i] == 0) {
        is_manifold_consistent = false;
        break;
      }
    }
  }

  MEM_freeN(edge_faces);
  MEM_freeN(edge_vert);

  return is_manifold_consistent;
}

static void quadriflow_free_job(void *customdata)
{
  QuadriFlowJob *qj = customdata;
  MEM_freeN(qj);
}

/* called by quadriflowjob, only to check job 'stop' value */
static int quadriflow_break_job(void *customdata)
{
  QuadriFlowJob *qj = (QuadriFlowJob *)customdata;
  // return *(qj->stop);

  /* this is not nice yet, need to make the jobs list template better
   * for identifying/acting upon various different jobs */
  /* but for now we'll reuse the render break... */
  bool should_break = (G.is_break);

  if (should_break) {
    qj->success = -1;
  }

  return should_break;
}

/* called by oceanbake, wmJob sends notifier */
static void quadriflow_update_job(void *customdata, float progress, int *cancel)
{
  QuadriFlowJob *qj = customdata;

  if (quadriflow_break_job(qj)) {
    *cancel = 1;
  }
  else {
    *cancel = 0;
  }

  *(qj->do_update) = true;
  *(qj->progress) = progress;
}

static Mesh *remesh_symmetry_bisect(Mesh *mesh, eSymmetryAxes symmetry_axes)
{
  MirrorModifierData mmd = {{0}};
  mmd.tolerance = QUADRIFLOW_MIRROR_BISECT_TOLERANCE;

  Mesh *mesh_bisect, *mesh_bisect_temp;
  mesh_bisect = BKE_mesh_copy_for_eval(mesh, false);

  int axis;
  float plane_co[3], plane_no[3];
  zero_v3(plane_co);

  for (char i = 0; i < 3; i++) {
    eSymmetryAxes symm_it = (eSymmetryAxes)(1 << i);
    if (symmetry_axes & symm_it) {
      axis = i;
      mmd.flag = 0;
      mmd.flag &= MOD_MIR_BISECT_AXIS_X << i;
      zero_v3(plane_no);
      plane_no[axis] = -1.0f;
      mesh_bisect_temp = mesh_bisect;
      mesh_bisect = BKE_mesh_mirror_bisect_on_mirror_plane(
          &mmd, mesh_bisect, axis, plane_co, plane_no);
      if (mesh_bisect_temp != mesh_bisect) {
        BKE_id_free(NULL, mesh_bisect_temp);
      }
    }
  }

  BKE_id_free(NULL, mesh);

  return mesh_bisect;
}

static Mesh *remesh_symmetry_mirror(Object *ob, Mesh *mesh, eSymmetryAxes symmetry_axes)
{
  MirrorModifierData mmd = {{0}};
  mmd.tolerance = QUADRIFLOW_MIRROR_BISECT_TOLERANCE;
  Mesh *mesh_mirror, *mesh_mirror_temp;

  mesh_mirror = mesh;

  int axis;

  for (char i = 0; i < 3; i++) {
    eSymmetryAxes symm_it = (eSymmetryAxes)(1 << i);
    if (symmetry_axes & symm_it) {
      axis = i;
      mmd.flag = 0;
      mmd.flag &= MOD_MIR_AXIS_X << i;
      mesh_mirror_temp = mesh_mirror;
      mesh_mirror = BKE_mesh_mirror_apply_mirror_on_axis(&mmd, NULL, ob, mesh_mirror, axis);
      if (mesh_mirror_temp != mesh_mirror) {
        BKE_id_free(NULL, mesh_mirror_temp);
      }
    }
  }

  return mesh_mirror;
}

static void quadriflow_start_job(void *customdata, short *stop, short *do_update, float *progress)
{
  QuadriFlowJob *qj = customdata;

  qj->stop = stop;
  qj->do_update = do_update;
  qj->progress = progress;
  qj->success = 1;

  if (qj->is_nonblocking_job) {
    G.is_break = false; /* XXX shared with render - replace with job 'stop' switch */
  }

  Object *ob = qj->owner;
  Mesh *mesh = ob->data;
  Mesh *new_mesh;
  Mesh *bisect_mesh;

  /* Check if the mesh is manifold. Quadriflow requires manifold meshes */
  if (!mesh_is_manifold_consistent(mesh)) {
    qj->success = -2;
    return;
  }

  /* Run Quadriflow bisect operations on a copy of the mesh to keep the code readable without
   * freeing the original ID */
  bisect_mesh = BKE_mesh_copy_for_eval(mesh, false);

  /* Bisect the input mesh using the paint symmetry settings */
  bisect_mesh = remesh_symmetry_bisect(bisect_mesh, qj->symmetry_axes);

  new_mesh = BKE_mesh_remesh_quadriflow_to_mesh_nomain(
      bisect_mesh,
      qj->target_faces,
      qj->seed,
      qj->use_preserve_sharp,
      (qj->use_preserve_boundary || qj->use_paint_symmetry),
#ifdef USE_MESH_CURVATURE
      qj->use_mesh_curvature,
#else
      false,
#endif
      quadriflow_update_job,
      (void *)qj);

  BKE_id_free(NULL, bisect_mesh);

  if (new_mesh == NULL) {
    *do_update = true;
    *stop = 0;
    if (qj->success == 1) {
      /* This is not a user cancellation event. */
      qj->success = 0;
    }
    return;
  }

  /* Mirror the Quadriflow result to build the final mesh */
  new_mesh = remesh_symmetry_mirror(qj->owner, new_mesh, qj->symmetry_axes);

  if (ob->mode == OB_MODE_SCULPT) {
    ED_sculpt_undo_geometry_begin(ob, "QuadriFlow Remesh");
  }

  if (qj->preserve_paint_mask) {
    BKE_mesh_runtime_clear_geometry(mesh);
    BKE_mesh_remesh_reproject_paint_mask(new_mesh, mesh);
  }

  BKE_mesh_nomain_to_mesh(new_mesh, mesh, ob, &CD_MASK_MESH, true);

  if (qj->smooth_normals) {
    if (qj->use_paint_symmetry) {
      BKE_mesh_calc_normals(ob->data);
    }
    BKE_mesh_smooth_flag_set(ob->data, true);
  }

  if (ob->mode == OB_MODE_SCULPT) {
    ED_sculpt_undo_geometry_end(ob);
  }

  BKE_mesh_batch_cache_dirty_tag(ob->data, BKE_MESH_BATCH_DIRTY_ALL);

  *do_update = true;
  *stop = 0;
}

static void quadriflow_end_job(void *customdata)
{
  QuadriFlowJob *qj = customdata;

  Object *ob = qj->owner;

  if (qj->is_nonblocking_job) {
    WM_set_locked_interface(G_MAIN->wm.first, false);
  }

  switch (qj->success) {
    case 1:
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      WM_reportf(RPT_INFO, "QuadriFlow: Remeshing completed");
      break;
    case 0:
      WM_reportf(RPT_ERROR, "QuadriFlow: Remeshing failed");
      break;
    case -1:
      WM_report(RPT_WARNING, "QuadriFlow: Remeshing cancelled");
      break;
    case -2:
      WM_report(RPT_WARNING,
                "QuadriFlow: The mesh needs to be manifold and have face normals that point in a "
                "consistent direction");
      break;
  }
}

static int quadriflow_remesh_exec(bContext *C, wmOperator *op)
{
  QuadriFlowJob *job = MEM_mallocN(sizeof(QuadriFlowJob), "QuadriFlowJob");

  job->owner = CTX_data_active_object(C);

  job->target_faces = RNA_int_get(op->ptr, "target_faces");
  job->seed = RNA_int_get(op->ptr, "seed");

  job->use_paint_symmetry = RNA_boolean_get(op->ptr, "use_paint_symmetry");

  job->use_preserve_sharp = RNA_boolean_get(op->ptr, "use_preserve_sharp");
  job->use_preserve_boundary = RNA_boolean_get(op->ptr, "use_preserve_boundary");

#ifdef USE_MESH_CURVATURE
  job->use_mesh_curvature = RNA_boolean_get(op->ptr, "use_mesh_curvature");
#endif

  job->preserve_paint_mask = RNA_boolean_get(op->ptr, "preserve_paint_mask");
  job->smooth_normals = RNA_boolean_get(op->ptr, "smooth_normals");

  /* Update the target face count if symmetry is enabled */
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  if (sd && job->use_paint_symmetry) {
    job->symmetry_axes = (eSymmetryAxes)(sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL);
    for (char i = 0; i < 3; i++) {
      eSymmetryAxes symm_it = (eSymmetryAxes)(1 << i);
      if (job->symmetry_axes & symm_it) {
        job->target_faces = job->target_faces / 2;
      }
    }
  }
  else {
    job->use_paint_symmetry = false;
    job->symmetry_axes = 0;
  }

  if (op->flag == 0) {
    /* This is called directly from the exec operator, this operation is now blocking */
    job->is_nonblocking_job = false;
    short stop = 0, do_update = true;
    float progress;
    quadriflow_start_job(job, &stop, &do_update, &progress);
    quadriflow_end_job(job);
    quadriflow_free_job(job);
  }
  else {
    /* Non blocking call. For when the operator has been called from the gui */
    job->is_nonblocking_job = true;

    wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
                                CTX_wm_window(C),
                                CTX_data_scene(C),
                                "QuadriFlow Remesh",
                                WM_JOB_PROGRESS,
                                WM_JOB_TYPE_QUADRIFLOW_REMESH);

    WM_jobs_customdata_set(wm_job, job, quadriflow_free_job);
    WM_jobs_timer(wm_job, 0.1, NC_GEOM | ND_DATA, NC_GEOM | ND_DATA);
    WM_jobs_callbacks(wm_job, quadriflow_start_job, NULL, NULL, quadriflow_end_job);

    WM_set_locked_interface(CTX_wm_manager(C), true);

    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }
  return OPERATOR_FINISHED;
}

static bool quadriflow_check(bContext *C, wmOperator *op)
{
  int mode = RNA_enum_get(op->ptr, "mode");

  if (mode == QUADRIFLOW_REMESH_EDGE_LENGTH) {
    float area = RNA_float_get(op->ptr, "mesh_area");
    if (area < 0.0f) {
      Object *ob = CTX_data_active_object(C);
      area = BKE_mesh_calc_area(ob->data);
      RNA_float_set(op->ptr, "mesh_area", area);
    }
    int num_faces;
    float edge_len = RNA_float_get(op->ptr, "target_edge_length");

    num_faces = area / (edge_len * edge_len);
    RNA_int_set(op->ptr, "target_faces", num_faces);
  }
  else if (mode == QUADRIFLOW_REMESH_RATIO) {
    Object *ob = CTX_data_active_object(C);
    Mesh *mesh = ob->data;

    int num_faces;
    float ratio = RNA_float_get(op->ptr, "target_ratio");

    num_faces = mesh->totpoly * ratio;

    RNA_int_set(op->ptr, "target_faces", num_faces);
  }

  return true;
}

/* Hide the target variables if they are not active */
static bool quadriflow_poll_property(const bContext *C, wmOperator *op, const PropertyRNA *prop)
{
  const char *prop_id = RNA_property_identifier(prop);

  if (STRPREFIX(prop_id, "target")) {
    int mode = RNA_enum_get(op->ptr, "mode");

    if (STREQ(prop_id, "target_edge_length") && mode != QUADRIFLOW_REMESH_EDGE_LENGTH) {
      return false;
    }
    else if (STREQ(prop_id, "target_faces")) {
      if (mode != QUADRIFLOW_REMESH_FACES) {
        /* Make sure we can edit the target_faces value even if it doesn't start as EDITABLE */
        float area = RNA_float_get(op->ptr, "mesh_area");
        if (area < -0.8f) {
          area += 0.2f;
          /* Make sure we have up to date values from the start */
          RNA_def_property_flag((PropertyRNA *)prop, PROP_EDITABLE);
          quadriflow_check((bContext *)C, op);
        }

        /* Only disable input */
        RNA_def_property_clear_flag((PropertyRNA *)prop, PROP_EDITABLE);
      }
      else {
        RNA_def_property_flag((PropertyRNA *)prop, PROP_EDITABLE);
      }
    }
    else if (STREQ(prop_id, "target_ratio") && mode != QUADRIFLOW_REMESH_RATIO) {
      return false;
    }
  }

  return true;
}

static const EnumPropertyItem mode_type_items[] = {
    {QUADRIFLOW_REMESH_RATIO,
     "RATIO",
     0,
     "Ratio",
     "Specify target number of faces relative to the current mesh"},
    {QUADRIFLOW_REMESH_EDGE_LENGTH,
     "EDGE",
     0,
     "Edge Length",
     "Input target edge length in the new mesh"},
    {QUADRIFLOW_REMESH_FACES, "FACES", 0, "Faces", "Input target number of faces in the new mesh"},
    {0, NULL, 0, NULL, NULL},
};

void OBJECT_OT_quadriflow_remesh(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "QuadriFlow Remesh";
  ot->description =
      "Create a new quad based mesh using the surface data of the current mesh. All data "
      "layers will be lost";
  ot->idname = "OBJECT_OT_quadriflow_remesh";

  /* api callbacks */
  ot->poll = object_remesh_poll;
  ot->poll_property = quadriflow_poll_property;
  ot->check = quadriflow_check;
  ot->invoke = WM_operator_props_popup_confirm;
  ot->exec = quadriflow_remesh_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "use_paint_symmetry",
                  true,
                  "Use Paint Symmetry",
                  "Generates a symmetrical mesh using the paint symmetry configuration");

  RNA_def_boolean(ot->srna,
                  "use_preserve_sharp",
                  false,
                  "Preserve Sharp",
                  "Try to preserve sharp features on the mesh");

  RNA_def_boolean(ot->srna,
                  "use_preserve_boundary",
                  false,
                  "Preserve Mesh Boundary",
                  "Try to preserve mesh boundary on the mesh");
#ifdef USE_MESH_CURVATURE
  RNA_def_boolean(ot->srna,
                  "use_mesh_curvature",
                  false,
                  "Use Mesh Curvature",
                  "Take the mesh curvature into account when remeshing");
#endif
  RNA_def_boolean(ot->srna,
                  "preserve_paint_mask",
                  false,
                  "Preserve Paint Mask",
                  "Reproject the paint mask onto the new mesh");

  RNA_def_boolean(ot->srna,
                  "smooth_normals",
                  false,
                  "Smooth Normals",
                  "Set the output mesh normals to smooth");

  RNA_def_enum(ot->srna,
               "mode",
               mode_type_items,
               QUADRIFLOW_REMESH_FACES,
               "Mode",
               "How to specify the amount of detail for the new mesh");

  prop = RNA_def_float(ot->srna,
                       "target_ratio",
                       1,
                       0,
                       FLT_MAX,
                       "Ratio",
                       "Relative number of faces compared to the current mesh",
                       0.0f,
                       1.0f);

  prop = RNA_def_float(ot->srna,
                       "target_edge_length",
                       0.1f,
                       0.0000001f,
                       FLT_MAX,
                       "Edge Length",
                       "Target edge length in the new mesh",
                       0.00001f,
                       1.0f);

  prop = RNA_def_int(ot->srna,
                     "target_faces",
                     4000,
                     1,
                     INT_MAX,
                     "Number of Faces",
                     "Approximate number of faces (quads) in the new mesh",
                     1,
                     INT_MAX);

  prop = RNA_def_float(
      ot->srna,
      "mesh_area",
      -1.0f,
      -FLT_MAX,
      FLT_MAX,
      "Old Object Face Area",
      "This property is only used to cache the object area for later calculations",
      0.0f,
      FLT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  RNA_def_int(ot->srna,
              "seed",
              0,
              0,
              INT_MAX,
              "Seed",
              "Random seed to use with the solver. Different seeds will cause the remesher to "
              "come up with different quad layouts on the mesh",
              0,
              255);
}

/** \} */
