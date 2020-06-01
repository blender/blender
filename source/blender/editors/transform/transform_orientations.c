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
 */

/** \file
 * \ingroup edtransform
 */

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_workspace_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_workspace.h"

#include "BLT_translation.h"

#include "ED_armature.h"

#include "transform.h"

/* *********************** TransSpace ************************** */

void BIF_clearTransformOrientation(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ListBase *transform_orientations = &scene->transform_spaces;

  BLI_freelistN(transform_orientations);

  for (int i = 0; i < ARRAY_SIZE(scene->orientation_slots); i++) {
    TransformOrientationSlot *orient_slot = &scene->orientation_slots[i];
    if (orient_slot->type == V3D_ORIENT_CUSTOM) {
      orient_slot->type = V3D_ORIENT_GLOBAL; /* fallback to global */
      orient_slot->index_custom = -1;
    }
  }
}

static TransformOrientation *findOrientationName(ListBase *lb, const char *name)
{
  return BLI_findstring(lb, name, offsetof(TransformOrientation, name));
}

static bool uniqueOrientationNameCheck(void *arg, const char *name)
{
  return findOrientationName((ListBase *)arg, name) != NULL;
}

static void uniqueOrientationName(ListBase *lb, char *name)
{
  BLI_uniquename_cb(uniqueOrientationNameCheck,
                    lb,
                    CTX_DATA_(BLT_I18NCONTEXT_ID_SCENE, "Space"),
                    '.',
                    name,
                    sizeof(((TransformOrientation *)NULL)->name));
}

static TransformOrientation *createViewSpace(bContext *C,
                                             ReportList *UNUSED(reports),
                                             const char *name,
                                             const bool overwrite)
{
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  float mat[3][3];

  if (!rv3d) {
    return NULL;
  }

  copy_m3_m4(mat, rv3d->viewinv);
  normalize_m3(mat);

  if (name[0] == 0) {
    View3D *v3d = CTX_wm_view3d(C);
    if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
      /* If an object is used as camera, then this space is the same as object space! */
      name = v3d->camera->id.name + 2;
    }
    else {
      name = "Custom View";
    }
  }

  return addMatrixSpace(C, mat, name, overwrite);
}

static TransformOrientation *createObjectSpace(bContext *C,
                                               ReportList *UNUSED(reports),
                                               const char *name,
                                               const bool overwrite)
{
  Base *base = CTX_data_active_base(C);
  Object *ob;
  float mat[3][3];

  if (base == NULL) {
    return NULL;
  }

  ob = base->object;

  copy_m3_m4(mat, ob->obmat);
  normalize_m3(mat);

  /* use object name if no name is given */
  if (name[0] == 0) {
    name = ob->id.name + 2;
  }

  return addMatrixSpace(C, mat, name, overwrite);
}

static TransformOrientation *createBoneSpace(bContext *C,
                                             ReportList *reports,
                                             const char *name,
                                             const bool overwrite)
{
  float mat[3][3];
  float normal[3], plane[3];

  getTransformOrientation(C, normal, plane);

  if (createSpaceNormalTangent(mat, normal, plane) == 0) {
    BKE_reports_prepend(reports, "Cannot use zero-length bone");
    return NULL;
  }

  if (name[0] == 0) {
    name = "Bone";
  }

  return addMatrixSpace(C, mat, name, overwrite);
}

static TransformOrientation *createCurveSpace(bContext *C,
                                              ReportList *reports,
                                              const char *name,
                                              const bool overwrite)
{
  float mat[3][3];
  float normal[3], plane[3];

  getTransformOrientation(C, normal, plane);

  if (createSpaceNormalTangent(mat, normal, plane) == 0) {
    BKE_reports_prepend(reports, "Cannot use zero-length curve");
    return NULL;
  }

  if (name[0] == 0) {
    name = "Curve";
  }

  return addMatrixSpace(C, mat, name, overwrite);
}

static TransformOrientation *createMeshSpace(bContext *C,
                                             ReportList *reports,
                                             const char *name,
                                             const bool overwrite)
{
  float mat[3][3];
  float normal[3], plane[3];
  int type;

  type = getTransformOrientation(C, normal, plane);

  switch (type) {
    case ORIENTATION_VERT:
      if (createSpaceNormal(mat, normal) == 0) {
        BKE_reports_prepend(reports, "Cannot use vertex with zero-length normal");
        return NULL;
      }

      if (name[0] == 0) {
        name = "Vertex";
      }
      break;
    case ORIENTATION_EDGE:
      if (createSpaceNormalTangent(mat, normal, plane) == 0) {
        BKE_reports_prepend(reports, "Cannot use zero-length edge");
        return NULL;
      }

      if (name[0] == 0) {
        name = "Edge";
      }
      break;
    case ORIENTATION_FACE:
      if (createSpaceNormalTangent(mat, normal, plane) == 0) {
        BKE_reports_prepend(reports, "Cannot use zero-area face");
        return NULL;
      }

      if (name[0] == 0) {
        name = "Face";
      }
      break;
    default:
      return NULL;
  }

  return addMatrixSpace(C, mat, name, overwrite);
}

bool createSpaceNormal(float mat[3][3], const float normal[3])
{
  float tangent[3] = {0.0f, 0.0f, 1.0f};

  copy_v3_v3(mat[2], normal);
  if (normalize_v3(mat[2]) == 0.0f) {
    return false; /* error return */
  }

  cross_v3_v3v3(mat[0], mat[2], tangent);
  if (is_zero_v3(mat[0])) {
    tangent[0] = 1.0f;
    tangent[1] = tangent[2] = 0.0f;
    cross_v3_v3v3(mat[0], tangent, mat[2]);
  }

  cross_v3_v3v3(mat[1], mat[2], mat[0]);

  normalize_m3(mat);

  return true;
}

/**
 * \note To recreate an orientation from the matrix:
 * - (plane  == mat[1])
 * - (normal == mat[2])
 */
bool createSpaceNormalTangent(float mat[3][3], const float normal[3], const float tangent[3])
{
  if (normalize_v3_v3(mat[2], normal) == 0.0f) {
    return false; /* error return */
  }

  /* negate so we can use values from the matrix as input */
  negate_v3_v3(mat[1], tangent);
  /* preempt zero length tangent from causing trouble */
  if (is_zero_v3(mat[1])) {
    mat[1][2] = 1.0f;
  }

  cross_v3_v3v3(mat[0], mat[2], mat[1]);
  if (normalize_v3(mat[0]) == 0.0f) {
    return false; /* error return */
  }

  cross_v3_v3v3(mat[1], mat[2], mat[0]);
  normalize_v3(mat[1]);

  /* final matrix must be normalized, do inline */
  // normalize_m3(mat);

  return true;
}

bool BIF_createTransformOrientation(bContext *C,
                                    ReportList *reports,
                                    const char *name,
                                    const bool use_view,
                                    const bool activate,
                                    const bool overwrite)
{
  TransformOrientation *ts = NULL;

  if (use_view) {
    ts = createViewSpace(C, reports, name, overwrite);
  }
  else {
    Object *obedit = CTX_data_edit_object(C);
    Object *ob = CTX_data_active_object(C);
    if (obedit) {
      if (obedit->type == OB_MESH) {
        ts = createMeshSpace(C, reports, name, overwrite);
      }
      else if (obedit->type == OB_ARMATURE) {
        ts = createBoneSpace(C, reports, name, overwrite);
      }
      else if (obedit->type == OB_CURVE) {
        ts = createCurveSpace(C, reports, name, overwrite);
      }
    }
    else if (ob && (ob->mode & OB_MODE_POSE)) {
      ts = createBoneSpace(C, reports, name, overwrite);
    }
    else {
      ts = createObjectSpace(C, reports, name, overwrite);
    }
  }

  if (activate && ts != NULL) {
    BIF_selectTransformOrientation(C, ts);
  }
  return (ts != NULL);
}

TransformOrientation *addMatrixSpace(bContext *C,
                                     float mat[3][3],
                                     const char *name,
                                     const bool overwrite)
{
  TransformOrientation *ts = NULL;
  Scene *scene = CTX_data_scene(C);
  ListBase *transform_orientations = &scene->transform_spaces;
  char name_unique[sizeof(ts->name)];

  if (overwrite) {
    ts = findOrientationName(transform_orientations, name);
  }
  else {
    BLI_strncpy(name_unique, name, sizeof(name_unique));
    uniqueOrientationName(transform_orientations, name_unique);
    name = name_unique;
  }

  /* if not, create a new one */
  if (ts == NULL) {
    ts = MEM_callocN(sizeof(TransformOrientation), "UserTransSpace from matrix");
    BLI_addtail(transform_orientations, ts);
    BLI_strncpy(ts->name, name, sizeof(ts->name));
  }

  /* copy matrix into transform space */
  copy_m3_m3(ts->mat, mat);

  return ts;
}

void BIF_removeTransformOrientation(bContext *C, TransformOrientation *target)
{
  BKE_scene_transform_orientation_remove(CTX_data_scene(C), target);
}

void BIF_removeTransformOrientationIndex(bContext *C, int index)
{
  TransformOrientation *target = BKE_scene_transform_orientation_find(CTX_data_scene(C), index);
  BIF_removeTransformOrientation(C, target);
}

void BIF_selectTransformOrientation(bContext *C, TransformOrientation *target)
{
  Scene *scene = CTX_data_scene(C);
  int index = BKE_scene_transform_orientation_get_index(scene, target);

  BLI_assert(index != -1);

  scene->orientation_slots[SCE_ORIENT_DEFAULT].type = V3D_ORIENT_CUSTOM;
  scene->orientation_slots[SCE_ORIENT_DEFAULT].index_custom = index;
}

int BIF_countTransformOrientation(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ListBase *transform_orientations = &scene->transform_spaces;
  return BLI_listbase_count(transform_orientations);
}

void applyTransformOrientation(const TransformOrientation *ts, float r_mat[3][3], char *r_name)
{
  if (r_name) {
    BLI_strncpy(r_name, ts->name, MAX_NAME);
  }
  copy_m3_m3(r_mat, ts->mat);
}

/* Updates all `BONE_TRANSFORM` flags.
 * Returns total number of bones with `BONE_TRANSFORM`.
 * Note: `transform_convert_pose_transflags_update` has a similar logic. */
static int armature_bone_transflags_update_recursive(bArmature *arm,
                                                     ListBase *lb,
                                                     const bool do_it)
{
  Bone *bone;
  bool do_next;
  int total = 0;

  for (bone = lb->first; bone; bone = bone->next) {
    bone->flag &= ~BONE_TRANSFORM;
    do_next = do_it;
    if (do_it) {
      if (bone->layer & arm->layer) {
        if (bone->flag & BONE_SELECTED) {
          bone->flag |= BONE_TRANSFORM;
          total++;

          /* no transform on children if one parent bone is selected */
          do_next = false;
        }
      }
    }
    total += armature_bone_transflags_update_recursive(arm, &bone->childbase, do_next);
  }

  return total;
}

void ED_transform_calc_orientation_from_type(const bContext *C, float r_mat[3][3])
{
  ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *obedit = CTX_data_edit_object(C);
  RegionView3D *rv3d = region->regiondata;
  Object *ob = OBACT(view_layer);
  const short orientation_type = scene->orientation_slots[SCE_ORIENT_DEFAULT].type;
  const short orientation_index_custom = scene->orientation_slots[SCE_ORIENT_DEFAULT].index_custom;
  const int pivot_point = scene->toolsettings->transform_pivot_point;

  ED_transform_calc_orientation_from_type_ex(
      C, r_mat, scene, rv3d, ob, obedit, orientation_type, orientation_index_custom, pivot_point);
}

short ED_transform_calc_orientation_from_type_ex(const bContext *C,
                                                 float r_mat[3][3],
                                                 /* extra args (can be accessed from context) */
                                                 Scene *scene,
                                                 RegionView3D *rv3d,
                                                 Object *ob,
                                                 Object *obedit,
                                                 const short orientation_type,
                                                 int orientation_index_custom,
                                                 const int pivot_point)
{
  switch (orientation_type) {
    case V3D_ORIENT_GLOBAL: {
      unit_m3(r_mat);
      return V3D_ORIENT_GLOBAL;
    }
    case V3D_ORIENT_GIMBAL: {
      if (gimbal_axis(ob, r_mat)) {
        return V3D_ORIENT_GIMBAL;
      }
      /* if not gimbal, fall through to normal */
      ATTR_FALLTHROUGH;
    }
    case V3D_ORIENT_NORMAL: {
      if (obedit || ob->mode & OB_MODE_POSE) {
        ED_getTransformOrientationMatrix(C, r_mat, pivot_point);
        return V3D_ORIENT_NORMAL;
      }
      /* no break we define 'normal' as 'local' in Object mode */
      ATTR_FALLTHROUGH;
    }
    case V3D_ORIENT_LOCAL: {
      if (ob) {
        if (ob->mode & OB_MODE_POSE) {
          /* each bone moves on its own local axis, but  to avoid confusion,
           * use the active pones axis for display [#33575], this works as expected on a single
           * bone and users who select many bones will understand what's going on and what local
           * means when they start transforming */
          ED_getTransformOrientationMatrix(C, r_mat, pivot_point);
        }
        else {
          copy_m3_m4(r_mat, ob->obmat);
          normalize_m3(r_mat);
        }
        return V3D_ORIENT_LOCAL;
      }
      unit_m3(r_mat);
      return V3D_ORIENT_GLOBAL;
    }
    case V3D_ORIENT_VIEW: {
      if (rv3d != NULL) {
        copy_m3_m4(r_mat, rv3d->viewinv);
        normalize_m3(r_mat);
      }
      else {
        unit_m3(r_mat);
      }
      return V3D_ORIENT_VIEW;
    }
    case V3D_ORIENT_CURSOR: {
      BKE_scene_cursor_rot_to_mat3(&scene->cursor, r_mat);
      return V3D_ORIENT_CURSOR;
    }
    case V3D_ORIENT_CUSTOM_MATRIX: {
      /* Do nothing. */;
      break;
    }
    case V3D_ORIENT_CUSTOM:
    default: {
      BLI_assert(orientation_type >= V3D_ORIENT_CUSTOM);
      TransformOrientation *custom_orientation = BKE_scene_transform_orientation_find(
          scene, orientation_index_custom);
      applyTransformOrientation(custom_orientation, r_mat, NULL);
      break;
    }
  }

  return orientation_type;
}

/* Sets the matrix of the specified space orientation.
 * If the matrix cannot be obtained, an orientation different from the one
 * informed is returned */
short transform_orientation_matrix_get(bContext *C,
                                       TransInfo *t,
                                       const short orientation,
                                       const float custom[3][3],
                                       float r_spacemtx[3][3])
{
  if (orientation == V3D_ORIENT_CUSTOM_MATRIX) {
    copy_m3_m3(r_spacemtx, custom);
    return V3D_ORIENT_CUSTOM_MATRIX;
  }

  if ((t->spacetype == SPACE_VIEW3D) && (t->region->regiontype == RGN_TYPE_WINDOW)) {
    Object *ob = CTX_data_active_object(C);
    Object *obedit = CTX_data_active_object(C);
    RegionView3D *rv3d = t->region->regiondata;
    int orientation_index_custom = 0;

    if (orientation >= V3D_ORIENT_CUSTOM) {
      orientation_index_custom = orientation - V3D_ORIENT_CUSTOM;
    }

    return ED_transform_calc_orientation_from_type_ex(
        C,
        r_spacemtx,
        /* extra args (can be accessed from context) */
        t->scene,
        rv3d,
        ob,
        obedit,
        orientation,
        orientation_index_custom,
        t->around);
  }

  unit_m3(r_spacemtx);
  return V3D_ORIENT_GLOBAL;
}

const char *transform_orientations_spacename_get(TransInfo *t, const short orient_type)
{
  switch (orient_type) {
    case V3D_ORIENT_GLOBAL:
      return TIP_("global");
    case V3D_ORIENT_GIMBAL:
      return TIP_("gimbal");
    case V3D_ORIENT_NORMAL:
      return TIP_("normal");
    case V3D_ORIENT_LOCAL:
      return TIP_("local");
    case V3D_ORIENT_VIEW:
      return TIP_("view");
    case V3D_ORIENT_CURSOR:
      return TIP_("cursor");
    case V3D_ORIENT_CUSTOM_MATRIX:
      return TIP_("custom");
    case V3D_ORIENT_CUSTOM:
    default:
      BLI_assert(orient_type >= V3D_ORIENT_CUSTOM);
      TransformOrientation *ts = BKE_scene_transform_orientation_find(
          t->scene, orient_type - V3D_ORIENT_CUSTOM);
      return ts->name;
  }
}

void transform_orientations_current_set(TransInfo *t, const short orient_index)
{
  const short orientation = t->orient[orient_index].type;
  const char *spacename = transform_orientations_spacename_get(t, orientation);

  BLI_strncpy(t->spacename, spacename, sizeof(t->spacename));
  copy_m3_m3(t->spacemtx, t->orient[orient_index].matrix);
  invert_m3_m3(t->spacemtx_inv, t->spacemtx);
}

/**
 * utility function - get first n, selected vert/edge/faces
 */
static uint bm_mesh_elems_select_get_n__internal(
    BMesh *bm, BMElem **elems, const uint n, const BMIterType itype, const char htype)
{
  BMIter iter;
  BMElem *ele;
  uint i;

  BLI_assert(ELEM(htype, BM_VERT, BM_EDGE, BM_FACE));
  BLI_assert(ELEM(itype, BM_VERTS_OF_MESH, BM_EDGES_OF_MESH, BM_FACES_OF_MESH));

  if (!BLI_listbase_is_empty(&bm->selected)) {
    /* quick check */
    BMEditSelection *ese;
    i = 0;
    for (ese = bm->selected.last; ese; ese = ese->prev) {
      /* shouldn't need this check */
      if (BM_elem_flag_test(ese->ele, BM_ELEM_SELECT)) {

        /* only use contiguous selection */
        if (ese->htype != htype) {
          i = 0;
          break;
        }

        elems[i++] = ese->ele;
        if (n == i) {
          break;
        }
      }
      else {
        BLI_assert(0);
      }
    }

    if (i == 0) {
      /* pass */
    }
    else if (i == n) {
      return i;
    }
  }

  i = 0;
  BM_ITER_MESH (ele, &iter, bm, itype) {
    BLI_assert(ele->head.htype == htype);
    if (BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
      elems[i++] = ele;
      if (n == i) {
        break;
      }
    }
  }

  return i;
}

static uint bm_mesh_verts_select_get_n(BMesh *bm, BMVert **elems, const uint n)
{
  return bm_mesh_elems_select_get_n__internal(
      bm, (BMElem **)elems, min_ii(n, bm->totvertsel), BM_VERTS_OF_MESH, BM_VERT);
}
static uint bm_mesh_edges_select_get_n(BMesh *bm, BMEdge **elems, const uint n)
{
  return bm_mesh_elems_select_get_n__internal(
      bm, (BMElem **)elems, min_ii(n, bm->totedgesel), BM_EDGES_OF_MESH, BM_EDGE);
}
#if 0
static uint bm_mesh_faces_select_get_n(BMesh *bm, BMVert **elems, const uint n)
{
  return bm_mesh_elems_select_get_n__internal(
      bm, (BMElem **)elems, min_ii(n, bm->totfacesel), BM_FACES_OF_MESH, BM_FACE);
}
#endif

int getTransformOrientation_ex(const bContext *C,
                               float normal[3],
                               float plane[3],
                               const short around)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  Object *obedit = CTX_data_edit_object(C);
  Base *base;
  Object *ob = OBACT(view_layer);
  int result = ORIENTATION_NONE;
  const bool activeOnly = (around == V3D_AROUND_ACTIVE);

  zero_v3(normal);
  zero_v3(plane);

  if (obedit) {
    float imat[3][3], mat[3][3];

    /* we need the transpose of the inverse for a normal... */
    copy_m3_m4(imat, ob->obmat);

    invert_m3_m3(mat, imat);
    transpose_m3(mat);

    ob = obedit;

    if (ob->type == OB_MESH) {
      BMEditMesh *em = BKE_editmesh_from_object(ob);
      BMEditSelection ese;
      float vec[3] = {0, 0, 0};

      /* USE LAST SELECTED WITH ACTIVE */
      if (activeOnly && BM_select_history_active_get(em->bm, &ese)) {
        BM_editselection_normal(&ese, normal);
        BM_editselection_plane(&ese, plane);

        switch (ese.htype) {
          case BM_VERT:
            result = ORIENTATION_VERT;
            break;
          case BM_EDGE:
            result = ORIENTATION_EDGE;
            break;
          case BM_FACE:
            result = ORIENTATION_FACE;
            break;
        }
      }
      else {
        if (em->bm->totfacesel >= 1) {
          BMFace *efa;
          BMIter iter;

          BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
            if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
              BM_face_calc_tangent_auto(efa, vec);
              add_v3_v3(normal, efa->no);
              add_v3_v3(plane, vec);
            }
          }

          result = ORIENTATION_FACE;
        }
        else if (em->bm->totvertsel == 3) {
          BMVert *v_tri[3];

          if (bm_mesh_verts_select_get_n(em->bm, v_tri, 3) == 3) {
            BMEdge *e = NULL;
            float no_test[3];

            normal_tri_v3(normal, v_tri[0]->co, v_tri[1]->co, v_tri[2]->co);

            /* check if the normal is pointing opposite to vert normals */
            no_test[0] = v_tri[0]->no[0] + v_tri[1]->no[0] + v_tri[2]->no[0];
            no_test[1] = v_tri[0]->no[1] + v_tri[1]->no[1] + v_tri[2]->no[1];
            no_test[2] = v_tri[0]->no[2] + v_tri[1]->no[2] + v_tri[2]->no[2];
            if (dot_v3v3(no_test, normal) < 0.0f) {
              negate_v3(normal);
            }

            if (em->bm->totedgesel >= 1) {
              /* find an edge that's apart of v_tri (no need to search all edges) */
              float e_length;
              int j;

              for (j = 0; j < 3; j++) {
                BMEdge *e_test = BM_edge_exists(v_tri[j], v_tri[(j + 1) % 3]);
                if (e_test && BM_elem_flag_test(e_test, BM_ELEM_SELECT)) {
                  const float e_test_length = BM_edge_calc_length_squared(e_test);
                  if ((e == NULL) || (e_length < e_test_length)) {
                    e = e_test;
                    e_length = e_test_length;
                  }
                }
              }
            }

            if (e) {
              BMVert *v_pair[2];
              if (BM_edge_is_boundary(e)) {
                BM_edge_ordered_verts(e, &v_pair[0], &v_pair[1]);
              }
              else {
                v_pair[0] = e->v1;
                v_pair[1] = e->v2;
              }
              sub_v3_v3v3(plane, v_pair[0]->co, v_pair[1]->co);
            }
            else {
              BM_vert_tri_calc_tangent_edge(v_tri, plane);
            }
          }
          else {
            BLI_assert(0);
          }

          result = ORIENTATION_FACE;
        }
        else if (em->bm->totedgesel == 1 || em->bm->totvertsel == 2) {
          BMVert *v_pair[2] = {NULL, NULL};
          BMEdge *eed = NULL;

          if (em->bm->totedgesel == 1) {
            if (bm_mesh_edges_select_get_n(em->bm, &eed, 1) == 1) {
              v_pair[0] = eed->v1;
              v_pair[1] = eed->v2;
            }
          }
          else {
            BLI_assert(em->bm->totvertsel == 2);
            bm_mesh_verts_select_get_n(em->bm, v_pair, 2);
          }

          /* should never fail */
          if (LIKELY(v_pair[0] && v_pair[1])) {
            bool v_pair_swap = false;
            /**
             * Logic explained:
             *
             * - Edges and vert-pairs treated the same way.
             * - Point the Y axis along the edge vector (towards the active vertex).
             * - Point the Z axis outwards (the same direction as the normals).
             *
             * \note Z points outwards - along the normal.
             * take care making changes here, see: T38592, T43708
             */

            /* be deterministic where possible and ensure v_pair[0] is active */
            if (BM_mesh_active_vert_get(em->bm) == v_pair[1]) {
              v_pair_swap = true;
            }
            else if (eed && BM_edge_is_boundary(eed)) {
              /* predictable direction for boundary edges */
              if (eed->l->v != v_pair[0]) {
                v_pair_swap = true;
              }
            }

            if (v_pair_swap) {
              SWAP(BMVert *, v_pair[0], v_pair[1]);
            }

            add_v3_v3v3(normal, v_pair[1]->no, v_pair[0]->no);
            sub_v3_v3v3(plane, v_pair[1]->co, v_pair[0]->co);

            if (normalize_v3(plane) != 0.0f) {
              /* For edges it'd important the resulting matrix can rotate around the edge,
               * project onto the plane so we can use a fallback value. */
              project_plane_normalized_v3_v3v3(normal, normal, plane);
              if (UNLIKELY(normalize_v3(normal) == 0.0f)) {
                /* in the case the normal and plane are aligned,
                 * use a fallback normal which is orthogonal to the plane. */
                ortho_v3_v3(normal, plane);
              }
            }
          }

          result = ORIENTATION_EDGE;
        }
        else if (em->bm->totvertsel == 1) {
          BMVert *v = NULL;

          if (bm_mesh_verts_select_get_n(em->bm, &v, 1) == 1) {
            copy_v3_v3(normal, v->no);
            BMEdge *e_pair[2];

            if (BM_vert_edge_pair(v, &e_pair[0], &e_pair[1])) {
              bool v_pair_swap = false;
              BMVert *v_pair[2] = {
                  BM_edge_other_vert(e_pair[0], v),
                  BM_edge_other_vert(e_pair[1], v),
              };
              float dir_pair[2][3];

              if (BM_edge_is_boundary(e_pair[0])) {
                if (e_pair[0]->l->v != v) {
                  v_pair_swap = true;
                }
              }
              else {
                if (BM_edge_calc_length_squared(e_pair[0]) <
                    BM_edge_calc_length_squared(e_pair[1])) {
                  v_pair_swap = true;
                }
              }

              if (v_pair_swap) {
                SWAP(BMVert *, v_pair[0], v_pair[1]);
              }

              sub_v3_v3v3(dir_pair[0], v->co, v_pair[0]->co);
              sub_v3_v3v3(dir_pair[1], v_pair[1]->co, v->co);
              normalize_v3(dir_pair[0]);
              normalize_v3(dir_pair[1]);

              add_v3_v3v3(plane, dir_pair[0], dir_pair[1]);
            }
          }

          result = is_zero_v3(plane) ? ORIENTATION_VERT : ORIENTATION_EDGE;
        }
        else if (em->bm->totvertsel > 3) {
          BMIter iter;
          BMVert *v;

          zero_v3(normal);

          BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
            if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
              add_v3_v3(normal, v->no);
            }
          }
          normalize_v3(normal);
          result = ORIENTATION_VERT;
        }
      }

      /* not needed but this matches 2.68 and older behavior */
      negate_v3(plane);

    } /* end editmesh */
    else if (ELEM(obedit->type, OB_CURVE, OB_SURF)) {
      Curve *cu = obedit->data;
      Nurb *nu = NULL;
      int a;
      ListBase *nurbs = BKE_curve_editNurbs_get(cu);

      void *vert_act = NULL;
      if (activeOnly && BKE_curve_nurb_vert_active_get(cu, &nu, &vert_act)) {
        if (nu->type == CU_BEZIER) {
          BezTriple *bezt = vert_act;
          BKE_nurb_bezt_calc_normal(nu, bezt, normal);
          BKE_nurb_bezt_calc_plane(nu, bezt, plane);
        }
        else {
          BPoint *bp = vert_act;
          BKE_nurb_bpoint_calc_normal(nu, bp, normal);
          BKE_nurb_bpoint_calc_plane(nu, bp, plane);
        }
      }
      else {
        const bool use_handle = v3d->overlay.handle_display != CURVE_HANDLE_NONE;

        for (nu = nurbs->first; nu; nu = nu->next) {
          /* only bezier has a normal */
          if (nu->type == CU_BEZIER) {
            BezTriple *bezt = nu->bezt;
            a = nu->pntsu;
            while (a--) {
              short flag = 0;

#define SEL_F1 (1 << 0)
#define SEL_F2 (1 << 1)
#define SEL_F3 (1 << 2)

              if (use_handle) {
                if (bezt->f1 & SELECT) {
                  flag |= SEL_F1;
                }
                if (bezt->f2 & SELECT) {
                  flag |= SEL_F2;
                }
                if (bezt->f3 & SELECT) {
                  flag |= SEL_F3;
                }
              }
              else {
                flag = (bezt->f2 & SELECT) ? (SEL_F1 | SEL_F2 | SEL_F3) : 0;
              }

              /* exception */
              if (flag) {
                float tvec[3];
                if ((around == V3D_AROUND_LOCAL_ORIGINS) ||
                    ELEM(flag, SEL_F2, SEL_F1 | SEL_F3, SEL_F1 | SEL_F2 | SEL_F3)) {
                  BKE_nurb_bezt_calc_normal(nu, bezt, tvec);
                  add_v3_v3(normal, tvec);
                }
                else {
                  /* ignore bezt->f2 in this case */
                  if (flag & SEL_F1) {
                    sub_v3_v3v3(tvec, bezt->vec[0], bezt->vec[1]);
                    normalize_v3(tvec);
                    add_v3_v3(normal, tvec);
                  }
                  if (flag & SEL_F3) {
                    sub_v3_v3v3(tvec, bezt->vec[1], bezt->vec[2]);
                    normalize_v3(tvec);
                    add_v3_v3(normal, tvec);
                  }
                }

                BKE_nurb_bezt_calc_plane(nu, bezt, tvec);
                add_v3_v3(plane, tvec);
              }

#undef SEL_F1
#undef SEL_F2
#undef SEL_F3

              bezt++;
            }
          }
          else if (nu->bp && (nu->pntsv == 1)) {
            BPoint *bp = nu->bp;
            a = nu->pntsu;
            while (a--) {
              if (bp->f1 & SELECT) {
                float tvec[3];

                BPoint *bp_prev = BKE_nurb_bpoint_get_prev(nu, bp);
                BPoint *bp_next = BKE_nurb_bpoint_get_next(nu, bp);

                const bool is_prev_sel = bp_prev && (bp_prev->f1 & SELECT);
                const bool is_next_sel = bp_next && (bp_next->f1 & SELECT);
                if (is_prev_sel == false && is_next_sel == false) {
                  /* Isolated, add based on surrounding */
                  BKE_nurb_bpoint_calc_normal(nu, bp, tvec);
                  add_v3_v3(normal, tvec);
                }
                else if (is_next_sel) {
                  /* A segment, add the edge normal */
                  sub_v3_v3v3(tvec, bp->vec, bp_next->vec);
                  normalize_v3(tvec);
                  add_v3_v3(normal, tvec);
                }

                BKE_nurb_bpoint_calc_plane(nu, bp, tvec);
                add_v3_v3(plane, tvec);
              }
              bp++;
            }
          }
        }
      }

      if (!is_zero_v3(normal)) {
        result = ORIENTATION_FACE;
      }
    }
    else if (obedit->type == OB_MBALL) {
      MetaBall *mb = obedit->data;
      MetaElem *ml;
      bool ok = false;
      float tmat[3][3];

      if (activeOnly && (ml = mb->lastelem)) {
        quat_to_mat3(tmat, ml->quat);
        add_v3_v3(normal, tmat[2]);
        add_v3_v3(plane, tmat[1]);
        ok = true;
      }
      else {
        for (ml = mb->editelems->first; ml; ml = ml->next) {
          if (ml->flag & SELECT) {
            quat_to_mat3(tmat, ml->quat);
            add_v3_v3(normal, tmat[2]);
            add_v3_v3(plane, tmat[1]);
            ok = true;
          }
        }
      }

      if (ok) {
        if (!is_zero_v3(plane)) {
          result = ORIENTATION_FACE;
        }
      }
    }
    else if (obedit->type == OB_ARMATURE) {
      bArmature *arm = obedit->data;
      EditBone *ebone;
      bool ok = false;
      float tmat[3][3];

      if (activeOnly && (ebone = arm->act_edbone)) {
        ED_armature_ebone_to_mat3(ebone, tmat);
        add_v3_v3(normal, tmat[2]);
        add_v3_v3(plane, tmat[1]);
        ok = true;
      }
      else {
        /* When we only have the root/tip are selected. */
        bool fallback_ok = false;
        float fallback_normal[3];
        float fallback_plane[3];

        zero_v3(fallback_normal);
        zero_v3(fallback_plane);

        for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
          if (arm->layer & ebone->layer) {
            if (ebone->flag & BONE_SELECTED) {
              ED_armature_ebone_to_mat3(ebone, tmat);
              add_v3_v3(normal, tmat[2]);
              add_v3_v3(plane, tmat[1]);
              ok = true;
            }
            else if ((ok == false) &&
                     ((ebone->flag & BONE_TIPSEL) ||
                      ((ebone->flag & BONE_ROOTSEL) &&
                       (ebone->parent && ebone->flag & BONE_CONNECTED) == false))) {
              ED_armature_ebone_to_mat3(ebone, tmat);
              add_v3_v3(fallback_normal, tmat[2]);
              add_v3_v3(fallback_plane, tmat[1]);
              fallback_ok = true;
            }
          }
        }
        if ((ok == false) && fallback_ok) {
          ok = true;
          copy_v3_v3(normal, fallback_normal);
          copy_v3_v3(plane, fallback_plane);
        }
      }

      if (ok) {
        if (!is_zero_v3(plane)) {
          result = ORIENTATION_EDGE;
        }
      }
    }

    /* Vectors from edges don't need the special transpose inverse multiplication */
    if (result == ORIENTATION_EDGE) {
      float tvec[3];

      mul_mat3_m4_v3(ob->obmat, normal);
      mul_mat3_m4_v3(ob->obmat, plane);

      /* align normal to edge direction (so normal is perpendicular to the plane).
       * 'ORIENTATION_EDGE' will do the other way around.
       * This has to be done **after** applying obmat, see T45775! */
      project_v3_v3v3(tvec, normal, plane);
      sub_v3_v3(normal, tvec);
    }
    else {
      mul_m3_v3(mat, normal);
      mul_m3_v3(mat, plane);
    }
  }
  else if (ob && (ob->mode & OB_MODE_POSE)) {
    bArmature *arm = ob->data;
    bPoseChannel *pchan;
    float imat[3][3], mat[3][3];
    bool ok = false;

    if (activeOnly && (pchan = BKE_pose_channel_active(ob))) {
      add_v3_v3(normal, pchan->pose_mat[2]);
      add_v3_v3(plane, pchan->pose_mat[1]);
      ok = true;
    }
    else {
      int transformed_len;
      transformed_len = armature_bone_transflags_update_recursive(arm, &arm->bonebase, true);
      if (transformed_len) {
        /* use channels to get stats */
        for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
          if (pchan->bone && pchan->bone->flag & BONE_TRANSFORM) {
            add_v3_v3(normal, pchan->pose_mat[2]);
            add_v3_v3(plane, pchan->pose_mat[1]);
          }
        }
        ok = true;
      }
    }

    /* use for both active & all */
    if (ok) {
      /* we need the transpose of the inverse for a normal... */
      copy_m3_m4(imat, ob->obmat);

      invert_m3_m3(mat, imat);
      transpose_m3(mat);
      mul_m3_v3(mat, normal);
      mul_m3_v3(mat, plane);

      result = ORIENTATION_EDGE;
    }
  }
  else if (ob && (ob->mode & (OB_MODE_ALL_PAINT | OB_MODE_PARTICLE_EDIT))) {
    /* pass */
  }
  else {
    /* we need the one selected object, if its not active */
    base = BASACT(view_layer);
    ob = OBACT(view_layer);
    if (base && ((base->flag & BASE_SELECTED) != 0)) {
      /* pass */
    }
    else {
      /* first selected */
      ob = NULL;
      for (base = view_layer->object_bases.first; base; base = base->next) {
        if (BASE_SELECTED_EDITABLE(v3d, base)) {
          ob = base->object;
          break;
        }
      }
    }

    if (ob) {
      copy_v3_v3(normal, ob->obmat[2]);
      copy_v3_v3(plane, ob->obmat[1]);
    }
    result = ORIENTATION_NORMAL;
  }

  return result;
}

int getTransformOrientation(const bContext *C, float normal[3], float plane[3])
{
  /* dummy value, not V3D_AROUND_ACTIVE and not V3D_AROUND_LOCAL_ORIGINS */
  short around = V3D_AROUND_CENTER_BOUNDS;

  return getTransformOrientation_ex(C, normal, plane, around);
}

void ED_getTransformOrientationMatrix(const bContext *C,
                                      float orientation_mat[3][3],
                                      const short around)
{
  float normal[3] = {0.0, 0.0, 0.0};
  float plane[3] = {0.0, 0.0, 0.0};

  int type;

  type = getTransformOrientation_ex(C, normal, plane, around);

  /* Fallback, when the plane can't be calculated. */
  if (ORIENTATION_USE_PLANE(type) && is_zero_v3(plane)) {
    type = ORIENTATION_VERT;
  }

  switch (type) {
    case ORIENTATION_NORMAL:
      if (createSpaceNormalTangent(orientation_mat, normal, plane) == 0) {
        type = ORIENTATION_NONE;
      }
      break;
    case ORIENTATION_VERT:
      if (createSpaceNormal(orientation_mat, normal) == 0) {
        type = ORIENTATION_NONE;
      }
      break;
    case ORIENTATION_EDGE:
      if (createSpaceNormalTangent(orientation_mat, normal, plane) == 0) {
        type = ORIENTATION_NONE;
      }
      break;
    case ORIENTATION_FACE:
      if (createSpaceNormalTangent(orientation_mat, normal, plane) == 0) {
        type = ORIENTATION_NONE;
      }
      break;
    default:
      BLI_assert(type == ORIENTATION_NONE);
      break;
  }

  if (type == ORIENTATION_NONE) {
    unit_m3(orientation_mat);
  }
}
