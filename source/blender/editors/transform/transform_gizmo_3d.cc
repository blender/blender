/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 *
 * \name 3D Transform Gizmo
 *
 * Used for 3D View
 */

#include "BLI_array_utils.h"
#include "BLI_bounds.hh"
#include "BLI_function_ref.hh"
#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"

#include "DNA_armature_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meta_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_crazyspace.hh"
#include "BKE_curve.hh"
#include "BKE_editmesh.hh"
#include "BKE_global.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_layer.hh"
#include "BKE_library.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BKE_pointcache.h"
#include "BKE_scene.hh"
#include "BKE_screen.hh"

#include "WM_api.hh"
#include "WM_message.hh"

#include "ED_armature.hh"
#include "ED_curves.hh"
#include "ED_gizmo_library.hh"
#include "ED_gizmo_utils.hh"
#include "ED_gpencil_legacy.hh"
#include "ED_grease_pencil.hh"
#include "ED_object.hh"
#include "ED_particle.hh"
#include "ED_screen.hh"

#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "ANIM_armature.hh"

/* Local module include. */
#include "transform.hh"
#include "transform_convert.hh"
#include "transform_gizmo.hh"
#include "transform_snap.hh"

namespace blender::ed::transform {

static wmGizmoGroupType *g_GGT_xform_gizmo = nullptr;
static wmGizmoGroupType *g_GGT_xform_gizmo_context = nullptr;

static void gizmogroup_refresh_from_matrix(wmGizmoGroup *gzgroup,
                                           const float twmat[4][4],
                                           const float scale[3],
                                           const bool ignore_hidden);

/* Return codes for select, and drawing flags. */

#define MAN_TRANS_X (1 << 0)
#define MAN_TRANS_Y (1 << 1)
#define MAN_TRANS_Z (1 << 2)
#define MAN_TRANS_C (MAN_TRANS_X | MAN_TRANS_Y | MAN_TRANS_Z)

#define MAN_ROT_X (1 << 3)
#define MAN_ROT_Y (1 << 4)
#define MAN_ROT_Z (1 << 5)
#define MAN_ROT_C (MAN_ROT_X | MAN_ROT_Y | MAN_ROT_Z)

#define MAN_SCALE_X (1 << 8)
#define MAN_SCALE_Y (1 << 9)
#define MAN_SCALE_Z (1 << 10)
#define MAN_SCALE_C (MAN_SCALE_X | MAN_SCALE_Y | MAN_SCALE_Z)

/* Threshold for testing view aligned gizmo axis. */
static struct {
  float min, max;
} g_tw_axis_range[2] = {
    /* Regular range. */
    {0.02f, 0.1f},
    /* Use a different range because we flip the dot product,
     * also the view aligned planes are harder to see so hiding early is preferred. */
    {0.175f, 0.25f},
};

/* Axes as index. */
enum {
  MAN_AXIS_TRANS_X = 0,
  MAN_AXIS_TRANS_Y,
  MAN_AXIS_TRANS_Z,
  MAN_AXIS_TRANS_C,

  MAN_AXIS_TRANS_XY,
  MAN_AXIS_TRANS_YZ,
  MAN_AXIS_TRANS_ZX,
#define MAN_AXIS_RANGE_TRANS_START MAN_AXIS_TRANS_X
#define MAN_AXIS_RANGE_TRANS_END (MAN_AXIS_TRANS_ZX + 1)

  MAN_AXIS_ROT_X,
  MAN_AXIS_ROT_Y,
  MAN_AXIS_ROT_Z,
  MAN_AXIS_ROT_C,
  MAN_AXIS_ROT_T, /* Trackball rotation. */
#define MAN_AXIS_RANGE_ROT_START MAN_AXIS_ROT_X
#define MAN_AXIS_RANGE_ROT_END (MAN_AXIS_ROT_T + 1)

  MAN_AXIS_SCALE_X,
  MAN_AXIS_SCALE_Y,
  MAN_AXIS_SCALE_Z,
  MAN_AXIS_SCALE_C,
  MAN_AXIS_SCALE_XY,
  MAN_AXIS_SCALE_YZ,
  MAN_AXIS_SCALE_ZX,
#define MAN_AXIS_RANGE_SCALE_START MAN_AXIS_SCALE_X
#define MAN_AXIS_RANGE_SCALE_END (MAN_AXIS_SCALE_ZX + 1)

  MAN_AXIS_LAST = MAN_AXIS_SCALE_ZX + 1,
};

/* Axis types. */
enum {
  MAN_AXES_ALL = 0,
  MAN_AXES_TRANSLATE,
  MAN_AXES_ROTATE,
  MAN_AXES_SCALE,
};

struct GizmoGroup {
  bool all_hidden;
  int twtype;

  /* Users may change the twtype, detect changes to re-setup gizmo options. */
  int twtype_init;
  int twtype_prev;
  int use_twtype_refresh;

  /* Only for view orientation. */
  struct {
    float viewinv_m3[3][3];
  } prev;

  /* Only for Rotate operator. */
  float rotation;

  wmGizmo *gizmos[MAN_AXIS_LAST];
};

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

/* Loop over axes. */
#define MAN_ITER_AXES_BEGIN(axis, axis_idx) \
  { \
    wmGizmo *axis; \
    int axis_idx; \
    for (axis_idx = 0; axis_idx < MAN_AXIS_LAST; axis_idx++) { \
      axis = gizmo_get_axis_from_index(ggd, axis_idx);

#define MAN_ITER_AXES_END \
  } \
  } \
  ((void)0)

static wmGizmo *gizmo_get_axis_from_index(const GizmoGroup *ggd, const short axis_idx)
{
  BLI_assert(IN_RANGE_INCL(axis_idx, float(MAN_AXIS_TRANS_X), float(MAN_AXIS_LAST)));
  return ggd->gizmos[axis_idx];
}

static short gizmo_get_axis_type(const int axis_idx)
{
  if (axis_idx >= MAN_AXIS_RANGE_TRANS_START && axis_idx < MAN_AXIS_RANGE_TRANS_END) {
    return MAN_AXES_TRANSLATE;
  }
  if (axis_idx >= MAN_AXIS_RANGE_ROT_START && axis_idx < MAN_AXIS_RANGE_ROT_END) {
    return MAN_AXES_ROTATE;
  }
  if (axis_idx >= MAN_AXIS_RANGE_SCALE_START && axis_idx < MAN_AXIS_RANGE_SCALE_END) {
    return MAN_AXES_SCALE;
  }
  BLI_assert(0);
  return -1;
}

static uint gizmo_orientation_axis(const int axis_idx, bool *r_is_plane)
{
  switch (axis_idx) {
    case MAN_AXIS_TRANS_YZ:
    case MAN_AXIS_SCALE_YZ:
      if (r_is_plane) {
        *r_is_plane = true;
      }
      ATTR_FALLTHROUGH;
    case MAN_AXIS_TRANS_X:
    case MAN_AXIS_ROT_X:
    case MAN_AXIS_SCALE_X:
      return 0;

    case MAN_AXIS_TRANS_ZX:
    case MAN_AXIS_SCALE_ZX:
      if (r_is_plane) {
        *r_is_plane = true;
      }
      ATTR_FALLTHROUGH;
    case MAN_AXIS_TRANS_Y:
    case MAN_AXIS_ROT_Y:
    case MAN_AXIS_SCALE_Y:
      return 1;

    case MAN_AXIS_TRANS_XY:
    case MAN_AXIS_SCALE_XY:
      if (r_is_plane) {
        *r_is_plane = true;
      }
      ATTR_FALLTHROUGH;
    case MAN_AXIS_TRANS_Z:
    case MAN_AXIS_ROT_Z:
    case MAN_AXIS_SCALE_Z:
      return 2;
  }
  return 3;
}

static bool gizmo_is_axis_visible(const RegionView3D *rv3d,
                                  const int twtype,
                                  const float idot[3],
                                  const int axis_type,
                                  const int axis_idx)
{
  if ((axis_idx >= MAN_AXIS_RANGE_ROT_START && axis_idx < MAN_AXIS_RANGE_ROT_END) == 0) {
    bool is_plane = false;
    const uint aidx_norm = gizmo_orientation_axis(axis_idx, &is_plane);
    /* Don't draw axis perpendicular to the view. */
    if (aidx_norm < 3) {
      float idot_axis = idot[aidx_norm];
      if (is_plane) {
        idot_axis = 1.0f - idot_axis;
      }
      if (idot_axis < g_tw_axis_range[is_plane].min) {
        return false;
      }
    }
  }

  if ((axis_type == MAN_AXES_TRANSLATE && !(twtype & V3D_GIZMO_SHOW_OBJECT_TRANSLATE)) ||
      (axis_type == MAN_AXES_ROTATE && !(twtype & V3D_GIZMO_SHOW_OBJECT_ROTATE)) ||
      (axis_type == MAN_AXES_SCALE && !(twtype & V3D_GIZMO_SHOW_OBJECT_SCALE)))
  {
    return false;
  }

  switch (axis_idx) {
    case MAN_AXIS_TRANS_X:
      return (rv3d->twdrawflag & MAN_TRANS_X);
    case MAN_AXIS_TRANS_Y:
      return (rv3d->twdrawflag & MAN_TRANS_Y);
    case MAN_AXIS_TRANS_Z:
      return (rv3d->twdrawflag & MAN_TRANS_Z);
    case MAN_AXIS_TRANS_C:
      return (rv3d->twdrawflag & MAN_TRANS_C);
    case MAN_AXIS_ROT_X:
      return (rv3d->twdrawflag & MAN_ROT_X);
    case MAN_AXIS_ROT_Y:
      return (rv3d->twdrawflag & MAN_ROT_Y);
    case MAN_AXIS_ROT_Z:
      return (rv3d->twdrawflag & MAN_ROT_Z);
    case MAN_AXIS_ROT_C:
    case MAN_AXIS_ROT_T:
      return (rv3d->twdrawflag & MAN_ROT_C);
    case MAN_AXIS_SCALE_X:
      return (rv3d->twdrawflag & MAN_SCALE_X);
    case MAN_AXIS_SCALE_Y:
      return (rv3d->twdrawflag & MAN_SCALE_Y);
    case MAN_AXIS_SCALE_Z:
      return (rv3d->twdrawflag & MAN_SCALE_Z);
    case MAN_AXIS_SCALE_C:
      return (rv3d->twdrawflag & MAN_SCALE_C && (twtype & V3D_GIZMO_SHOW_OBJECT_TRANSLATE) == 0);
    case MAN_AXIS_TRANS_XY:
      return (rv3d->twdrawflag & MAN_TRANS_X && rv3d->twdrawflag & MAN_TRANS_Y &&
              (twtype & V3D_GIZMO_SHOW_OBJECT_ROTATE) == 0);
    case MAN_AXIS_TRANS_YZ:
      return (rv3d->twdrawflag & MAN_TRANS_Y && rv3d->twdrawflag & MAN_TRANS_Z &&
              (twtype & V3D_GIZMO_SHOW_OBJECT_ROTATE) == 0);
    case MAN_AXIS_TRANS_ZX:
      return (rv3d->twdrawflag & MAN_TRANS_Z && rv3d->twdrawflag & MAN_TRANS_X &&
              (twtype & V3D_GIZMO_SHOW_OBJECT_ROTATE) == 0);
    case MAN_AXIS_SCALE_XY:
      return (rv3d->twdrawflag & MAN_SCALE_X && rv3d->twdrawflag & MAN_SCALE_Y &&
              (twtype & V3D_GIZMO_SHOW_OBJECT_TRANSLATE) == 0 &&
              (twtype & V3D_GIZMO_SHOW_OBJECT_ROTATE) == 0);
    case MAN_AXIS_SCALE_YZ:
      return (rv3d->twdrawflag & MAN_SCALE_Y && rv3d->twdrawflag & MAN_SCALE_Z &&
              (twtype & V3D_GIZMO_SHOW_OBJECT_TRANSLATE) == 0 &&
              (twtype & V3D_GIZMO_SHOW_OBJECT_ROTATE) == 0);
    case MAN_AXIS_SCALE_ZX:
      return (rv3d->twdrawflag & MAN_SCALE_Z && rv3d->twdrawflag & MAN_SCALE_X &&
              (twtype & V3D_GIZMO_SHOW_OBJECT_TRANSLATE) == 0 &&
              (twtype & V3D_GIZMO_SHOW_OBJECT_ROTATE) == 0);
  }
  return false;
}

static void gizmo_get_axis_color(const int axis_idx,
                                 const float idot[3],
                                 float r_col[4],
                                 float r_col_hi[4])
{
  /* Alpha values for normal/highlighted states. */
  const float alpha = 0.6f;
  const float alpha_hi = 1.0f;
  float alpha_fac;

  if (axis_idx >= MAN_AXIS_RANGE_ROT_START && axis_idx < MAN_AXIS_RANGE_ROT_END) {
    /* Never fade rotation rings. */
    /* Trackball rotation axis is a special case, we only draw a slight overlay. */
    alpha_fac = (axis_idx == MAN_AXIS_ROT_T) ? 0.05f : 1.0f;
  }
  else {
    bool is_plane = false;
    const int axis_idx_norm = gizmo_orientation_axis(axis_idx, &is_plane);
    /* Get alpha fac based on axis angle,
     * to fade axis out when hiding it because it points towards view. */
    if (axis_idx_norm < 3) {
      const float idot_min = g_tw_axis_range[is_plane].min;
      const float idot_max = g_tw_axis_range[is_plane].max;
      float idot_axis = idot[axis_idx_norm];
      if (is_plane) {
        idot_axis = 1.0f - idot_axis;
      }
      alpha_fac = ((idot_axis > idot_max) ? 1.0f :
                   (idot_axis < idot_min) ? 0.0f :
                                            ((idot_axis - idot_min) / (idot_max - idot_min)));
    }
    else {
      alpha_fac = 1.0f;
    }
  }

  switch (axis_idx) {
    case MAN_AXIS_TRANS_X:
    case MAN_AXIS_ROT_X:
    case MAN_AXIS_SCALE_X:
    case MAN_AXIS_TRANS_YZ:
    case MAN_AXIS_SCALE_YZ:
      UI_GetThemeColor4fv(TH_AXIS_X, r_col);
      break;
    case MAN_AXIS_TRANS_Y:
    case MAN_AXIS_ROT_Y:
    case MAN_AXIS_SCALE_Y:
    case MAN_AXIS_TRANS_ZX:
    case MAN_AXIS_SCALE_ZX:
      UI_GetThemeColor4fv(TH_AXIS_Y, r_col);
      break;
    case MAN_AXIS_TRANS_Z:
    case MAN_AXIS_ROT_Z:
    case MAN_AXIS_SCALE_Z:
    case MAN_AXIS_TRANS_XY:
    case MAN_AXIS_SCALE_XY:
      UI_GetThemeColor4fv(TH_AXIS_Z, r_col);
      break;
    case MAN_AXIS_TRANS_C:
    case MAN_AXIS_ROT_C:
    case MAN_AXIS_SCALE_C:
    case MAN_AXIS_ROT_T:
      UI_GetThemeColor4fv(TH_GIZMO_VIEW_ALIGN, r_col);
      break;
  }

  copy_v4_v4(r_col_hi, r_col);

  r_col[3] = alpha * alpha_fac;
  r_col_hi[3] = alpha_hi * alpha_fac;
}

static void gizmo_get_axis_constraint(const int axis_idx, bool r_axis[3])
{
  ARRAY_SET_ITEMS(r_axis, 0, 0, 0);

  switch (axis_idx) {
    case MAN_AXIS_TRANS_X:
    case MAN_AXIS_ROT_X:
    case MAN_AXIS_SCALE_X:
      r_axis[0] = true;
      break;
    case MAN_AXIS_TRANS_Y:
    case MAN_AXIS_ROT_Y:
    case MAN_AXIS_SCALE_Y:
      r_axis[1] = true;
      break;
    case MAN_AXIS_TRANS_Z:
    case MAN_AXIS_ROT_Z:
    case MAN_AXIS_SCALE_Z:
      r_axis[2] = true;
      break;
    case MAN_AXIS_TRANS_XY:
    case MAN_AXIS_SCALE_XY:
      r_axis[0] = r_axis[1] = true;
      break;
    case MAN_AXIS_TRANS_YZ:
    case MAN_AXIS_SCALE_YZ:
      r_axis[1] = r_axis[2] = true;
      break;
    case MAN_AXIS_TRANS_ZX:
    case MAN_AXIS_SCALE_ZX:
      r_axis[2] = r_axis[0] = true;
      break;
    default:
      break;
  }
}

/* **************** Preparation Stuff **************** */

static void reset_tw_center(TransformBounds *tbounds)
{
  INIT_MINMAX(tbounds->min, tbounds->max);
  zero_v3(tbounds->center);

  for (int i = 0; i < 3; i++) {
    tbounds->axis_min[i] = +FLT_MAX;
    tbounds->axis_max[i] = -FLT_MAX;
  }
}

/**
 * Transform widget center calc helper for below.
 */
static void calc_tw_center(TransformBounds *tbounds, const float co[3])
{
  minmax_v3v3_v3(tbounds->min, tbounds->max, co);
  add_v3_v3(tbounds->center, co);

  for (int i = 0; i < 3; i++) {
    const float d = dot_v3v3(tbounds->axis[i], co);
    tbounds->axis_min[i] = min_ff(d, tbounds->axis_min[i]);
    tbounds->axis_max[i] = max_ff(d, tbounds->axis_max[i]);
  }
}

static void protectflag_to_drawflags(short protectflag, short *drawflags)
{
  if (protectflag & OB_LOCK_LOCX) {
    *drawflags &= ~MAN_TRANS_X;
  }
  if (protectflag & OB_LOCK_LOCY) {
    *drawflags &= ~MAN_TRANS_Y;
  }
  if (protectflag & OB_LOCK_LOCZ) {
    *drawflags &= ~MAN_TRANS_Z;
  }

  if (protectflag & OB_LOCK_ROTX) {
    *drawflags &= ~MAN_ROT_X;
  }
  if (protectflag & OB_LOCK_ROTY) {
    *drawflags &= ~MAN_ROT_Y;
  }
  if (protectflag & OB_LOCK_ROTZ) {
    *drawflags &= ~MAN_ROT_Z;
  }

  if (protectflag & OB_LOCK_SCALEX) {
    *drawflags &= ~MAN_SCALE_X;
  }
  if (protectflag & OB_LOCK_SCALEY) {
    *drawflags &= ~MAN_SCALE_Y;
  }
  if (protectflag & OB_LOCK_SCALEZ) {
    *drawflags &= ~MAN_SCALE_Z;
  }
}

/* Similar to #transform_object_deform_pose_armature_get but does not check visibility. */
static Object *gizmo_3d_transform_space_object_get(Scene *scene, ViewLayer *view_layer)
{
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  if (ob && ob->mode & OB_MODE_WEIGHT_PAINT) {
    /* It is assumed that when the object is in Weight Paint mode, it is not in Edit mode. So we
     * don't need to check the #OB_MODE_EDIT flag. */
    BLI_assert(!(ob->mode & OB_MODE_EDIT));
    Object *obpose = BKE_object_pose_armature_get(ob);
    if (obpose != nullptr) {
      ob = obpose;
    }
  }
  return ob;
}

/**
 * Run \a user_fn for each coordinate of elements selected in View3D (vertices, particles...).
 * \note Each coordinate has the space matrix of the active object.
 *
 * \param orient_index: A #TransformOrientationSlot.type. Here used for calculating \a r_drawflags.
 * \param use_curve_handles: If true, the handles of curves are traversed.
 * \param use_only_center: For objects in object mode, defines whether the corners of the bounds or
 *                         just the center are traversed.
 * \param user_fn: Callback that runs on each coordinate.
 * \param r_mat: Returns the space matrix of the coordinates.
 * \param r_drawflags: Drawing flags for gizmos. Usually stored in #RegionView3D::drawflags.
 */
static int gizmo_3d_foreach_selected(const bContext *C,
                                     const short orient_index,
                                     const bool use_curve_handles,
                                     const bool use_only_center,
                                     FunctionRef<void(const float3 &)> user_fn,
                                     const float (**r_mat)[4],
                                     short *r_drawflags)
{
  const auto run_coord_with_matrix =
      [&](const float co[3], const bool use_matrix, const float matrix[4][4]) {
        float co_world[3];
        if (use_matrix) {
          mul_v3_m4v3(co_world, matrix, co);
          co = co_world;
        }
        user_fn(co);
      };

  ScrArea *area = CTX_wm_area(C);
  Scene *scene = CTX_data_scene(C);
  /* TODO(sergey): This function is used from operator's modal() and from gizmo's refresh().
   * Is it fine to possibly evaluate dependency graph here? */
  Depsgraph *depsgraph = CTX_data_expect_evaluated_depsgraph(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = static_cast<View3D *>(area->spacedata.first);
  int a, totsel = 0;

  Object *ob = gizmo_3d_transform_space_object_get(scene, view_layer);

  if (Object *obedit = OBEDIT_FROM_OBACT(ob)) {

#define FOREACH_EDIT_OBJECT_BEGIN(ob_iter, use_mat_local) \
  { \
    invert_m4_m4(obedit->runtime->world_to_object.ptr(), obedit->object_to_world().ptr()); \
    Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode( \
        scene, view_layer, CTX_wm_view3d(C)); \
    for (Object *ob_iter : objects) { \
      const bool use_mat_local = (ob_iter != obedit);

#define FOREACH_EDIT_OBJECT_END() \
  } \
  } \
  ((void)0)

    ob = obedit;
    if (obedit->type == OB_MESH) {
      FOREACH_EDIT_OBJECT_BEGIN (ob_iter, use_mat_local) {
        BMEditMesh *em_iter = BKE_editmesh_from_object(ob_iter);
        BMesh *bm = em_iter->bm;

        if (bm->totvertsel == 0) {
          continue;
        }

        BMVert *eve;
        BMIter iter;

        float mat_local[4][4];
        if (use_mat_local) {
          mul_m4_m4m4(
              mat_local, obedit->world_to_object().ptr(), ob_iter->object_to_world().ptr());
        }

        BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
          if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
            if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
              run_coord_with_matrix(eve->co, use_mat_local, mat_local);
              totsel++;
            }
          }
        }
      }
      FOREACH_EDIT_OBJECT_END();
    } /* End editmesh. */
    else if (obedit->type == OB_ARMATURE) {
      FOREACH_EDIT_OBJECT_BEGIN (ob_iter, use_mat_local) {
        bArmature *arm = static_cast<bArmature *>(ob_iter->data);

        float mat_local[4][4];
        if (use_mat_local) {
          mul_m4_m4m4(
              mat_local, obedit->world_to_object().ptr(), ob_iter->object_to_world().ptr());
        }
        LISTBASE_FOREACH (EditBone *, ebo, arm->edbo) {
          if (blender::animrig::bone_is_visible(arm, ebo)) {
            if (ebo->flag & BONE_TIPSEL) {
              run_coord_with_matrix(ebo->tail, use_mat_local, mat_local);
              totsel++;
            }
            if ((ebo->flag & BONE_ROOTSEL) &&
                /* Don't include same point multiple times. */
                ((ebo->flag & BONE_CONNECTED) && (ebo->parent != nullptr) &&
                 (ebo->parent->flag & BONE_TIPSEL) &&
                 blender::animrig::bone_is_visible(arm, ebo->parent)) == 0)
            {
              run_coord_with_matrix(ebo->head, use_mat_local, mat_local);
              totsel++;

              if (r_drawflags) {
                if (ebo->flag & (BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL)) {
                  if (ebo->flag & BONE_EDITMODE_LOCKED) {
                    protectflag_to_drawflags(OB_LOCK_LOC | OB_LOCK_ROT | OB_LOCK_SCALE,
                                             r_drawflags);
                  }
                }
              }
            }
          }
        }
      }
      FOREACH_EDIT_OBJECT_END();
    }
    else if (ELEM(obedit->type, OB_CURVES_LEGACY, OB_SURF)) {
      FOREACH_EDIT_OBJECT_BEGIN (ob_iter, use_mat_local) {
        Curve *cu = static_cast<Curve *>(ob_iter->data);
        BezTriple *bezt;
        BPoint *bp;
        ListBase *nurbs = BKE_curve_editNurbs_get(cu);

        float mat_local[4][4];
        if (use_mat_local) {
          mul_m4_m4m4(
              mat_local, obedit->world_to_object().ptr(), ob_iter->object_to_world().ptr());
        }

        Nurb *nu = static_cast<Nurb *>(nurbs->first);
        while (nu) {
          if (nu->type == CU_BEZIER) {
            bezt = nu->bezt;
            a = nu->pntsu;
            while (a--) {
              /* Exceptions:
               * - If handles are hidden then only check the center points.
               * - If the center knot is selected then only use this as the center point.
               */
              if (v3d->overlay.handle_display == CURVE_HANDLE_NONE) {
                if (bezt->f2 & SELECT) {
                  run_coord_with_matrix(bezt->vec[1], use_mat_local, mat_local);
                  totsel++;
                }
              }
              else if (bezt->f2 & SELECT) {
                run_coord_with_matrix(bezt->vec[1], use_mat_local, mat_local);
                totsel++;
              }
              else {
                if (bezt->f1 & SELECT) {
                  const float *co = bezt->vec[!use_curve_handles ? 1 : 0];
                  run_coord_with_matrix(co, use_mat_local, mat_local);
                  totsel++;
                }
                if (bezt->f3 & SELECT) {
                  const float *co = bezt->vec[!use_curve_handles ? 1 : 2];
                  run_coord_with_matrix(co, use_mat_local, mat_local);
                  totsel++;
                }
              }
              bezt++;
            }
          }
          else {
            bp = nu->bp;
            a = nu->pntsu * nu->pntsv;
            while (a--) {
              if (bp->f1 & SELECT) {
                run_coord_with_matrix(bp->vec, use_mat_local, mat_local);
                totsel++;
              }
              bp++;
            }
          }
          nu = nu->next;
        }
      }
      FOREACH_EDIT_OBJECT_END();
    }
    else if (obedit->type == OB_MBALL) {
      FOREACH_EDIT_OBJECT_BEGIN (ob_iter, use_mat_local) {
        MetaBall *mb = (MetaBall *)ob_iter->data;

        float mat_local[4][4];
        if (use_mat_local) {
          mul_m4_m4m4(
              mat_local, obedit->world_to_object().ptr(), ob_iter->object_to_world().ptr());
        }

        LISTBASE_FOREACH (MetaElem *, ml, mb->editelems) {
          if (ml->flag & SELECT) {
            run_coord_with_matrix(&ml->x, use_mat_local, mat_local);
            totsel++;
          }
        }
      }
      FOREACH_EDIT_OBJECT_END();
    }
    else if (obedit->type == OB_LATTICE) {
      FOREACH_EDIT_OBJECT_BEGIN (ob_iter, use_mat_local) {
        Lattice *lt = ((Lattice *)ob_iter->data)->editlatt->latt;
        BPoint *bp = lt->def;
        a = lt->pntsu * lt->pntsv * lt->pntsw;

        float mat_local[4][4];
        if (use_mat_local) {
          mul_m4_m4m4(
              mat_local, obedit->world_to_object().ptr(), ob_iter->object_to_world().ptr());
        }

        while (a--) {
          if (bp->f1 & SELECT) {
            run_coord_with_matrix(bp->vec, use_mat_local, mat_local);
            totsel++;
          }
          bp++;
        }
      }
      FOREACH_EDIT_OBJECT_END();
    }
    else if (obedit->type == OB_CURVES) {
      FOREACH_EDIT_OBJECT_BEGIN (ob_iter, use_mat_local) {
        const Curves &curves_id = *static_cast<Curves *>(ob_iter->data);
        const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
        const bke::crazyspace::GeometryDeformation deformation =
            bke::crazyspace::get_evaluated_curves_deformation(*depsgraph, *ob);

        float4x4 mat_local;
        if (use_mat_local) {
          mat_local = obedit->world_to_object() * ob_iter->object_to_world();
        }

        IndexMaskMemory memory;
        const IndexMask selected_points = ed::curves::retrieve_selected_points(curves, memory);
        const Span<float3> positions = deformation.positions;
        totsel += selected_points.size();
        selected_points.foreach_index([&](const int point_i) {
          run_coord_with_matrix(positions[point_i], use_mat_local, mat_local.ptr());
        });
      }
      FOREACH_EDIT_OBJECT_END();
    }
    else if (obedit->type == OB_POINTCLOUD) {
      FOREACH_EDIT_OBJECT_BEGIN (ob_iter, use_mat_local) {
        const PointCloud &pointcloud = *static_cast<const PointCloud *>(ob_iter->data);

        float4x4 mat_local;
        if (use_mat_local) {
          mat_local = obedit->world_to_object() * ob_iter->object_to_world();
        }

        const bke::AttributeAccessor attributes = pointcloud.attributes();
        const VArray selection = *attributes.lookup_or_default<bool>(
            ".selection", bke::AttrDomain::Point, true);

        IndexMaskMemory memory;
        const IndexMask mask = IndexMask::from_bools(selection, memory);
        const Span<float3> positions = pointcloud.positions();
        totsel += mask.size();
        mask.foreach_index([&](const int point) {
          run_coord_with_matrix(positions[point], use_mat_local, mat_local.ptr());
        });
      }
      FOREACH_EDIT_OBJECT_END();
    }
    else if (obedit->type == OB_GREASE_PENCIL) {
      FOREACH_EDIT_OBJECT_BEGIN (ob_iter, use_mat_local) {
        GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob_iter->data);

        float4x4 mat_local = float4x4::identity();
        if (use_mat_local) {
          mat_local = obedit->world_to_object() * ob_iter->object_to_world();
        }

        const Vector<ed::greasepencil::MutableDrawingInfo> drawings =
            ed::greasepencil::retrieve_editable_drawings(*scene, grease_pencil);
        threading::parallel_for_each(
            drawings, [&](const ed::greasepencil::MutableDrawingInfo &info) {
              const bke::CurvesGeometry &curves = info.drawing.strokes();

              const bke::crazyspace::GeometryDeformation deformation =
                  bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(
                      *depsgraph, *ob, info.drawing);

              const float4x4 layer_transform =
                  mat_local * grease_pencil.layer(info.layer_index).to_object_space(*ob_iter);

              IndexMaskMemory memory;
              const IndexMask selected_points = ed::curves::retrieve_selected_points(curves,
                                                                                     memory);
              const Span<float3> positions = deformation.positions;
              totsel += selected_points.size();
              selected_points.foreach_index([&](const int point_i) {
                run_coord_with_matrix(positions[point_i], true, layer_transform.ptr());
              });
            });
      }
      FOREACH_EDIT_OBJECT_END();
    }

#undef FOREACH_EDIT_OBJECT_BEGIN
#undef FOREACH_EDIT_OBJECT_END
  }
  else if (ob && (ob->mode & OB_MODE_POSE)) {
    invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());

    Vector<Object *> objects = BKE_object_pose_array_get(scene, view_layer, v3d);

    for (Object *ob_iter : objects) {
      const bool use_mat_local = (ob_iter != ob);
      /* Mislead counting bones... bah. We don't know the gizmo mode, could be mixed. */
      const int mode = TFM_ROTATION;

      transform_convert_pose_transflags_update(ob_iter, mode, V3D_AROUND_CENTER_BOUNDS);

      float mat_local[4][4];
      if (use_mat_local) {
        mul_m4_m4m4(mat_local, ob->world_to_object().ptr(), ob_iter->object_to_world().ptr());
      }

      bArmature *arm = static_cast<bArmature *>(ob_iter->data);
      /* Use channels to get stats. */
      LISTBASE_FOREACH (bPoseChannel *, pchan, &ob_iter->pose->chanbase) {
        if (!(pchan->runtime.flag & POSE_RUNTIME_TRANSFORM)) {
          continue;
        }

        float pchan_pivot[3];
        BKE_pose_channel_transform_location(arm, pchan, pchan_pivot);
        run_coord_with_matrix(pchan_pivot, use_mat_local, mat_local);
        totsel++;

        if (r_drawflags) {
          /* Protect-flags apply to local space in pose mode, so only let them influence axis
           * visibility if we show the global orientation, otherwise it's confusing. */
          if (ELEM(orient_index, V3D_ORIENT_LOCAL, V3D_ORIENT_GIMBAL)) {
            protectflag_to_drawflags(pchan->protectflag, r_drawflags);
          }
        }
      }
    }
  }
  else if (ob && (ob->mode & OB_MODE_ALL_PAINT)) {
    if (ob->mode & OB_MODE_SCULPT) {
      totsel = 1;
      run_coord_with_matrix(ob->sculpt->pivot_pos, false, ob->object_to_world().ptr());
    }
  }
  else if (ob && ob->mode & OB_MODE_PARTICLE_EDIT) {
    PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);
    PTCacheEditPoint *point;
    PTCacheEditKey *ek;
    int k;

    if (edit) {
      point = edit->points;
      for (a = 0; a < edit->totpoint; a++, point++) {
        if (point->flag & PEP_HIDE) {
          continue;
        }

        for (k = 0, ek = point->keys; k < point->totkey; k++, ek++) {
          if (ek->flag & PEK_SELECT) {
            user_fn((ek->flag & PEK_USE_WCO) ? ek->world_co : ek->co);
            totsel++;
          }
        }
      }
    }
  }
  else {

    /* We need the one selected object, if its not active. */
    BKE_view_layer_synced_ensure(scene, view_layer);
    {
      Base *base = BKE_view_layer_active_base_get(view_layer);
      ob = base ? base->object : nullptr;
      if (base && ((base->flag & BASE_SELECTED) == 0)) {
        ob = nullptr;
      }
    }

    LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
      if (!BASE_SELECTED_EDITABLE(v3d, base)) {
        continue;
      }
      if (ob == nullptr) {
        ob = base->object;
      }

      /* Get the boundbox out of the evaluated object. */
      std::optional<std::array<float3, 8>> bb;
      if (use_only_center == false) {
        if (std::optional<Bounds<float3>> bounds = BKE_object_boundbox_get(base->object)) {
          bb.emplace(bounds::corners(*bounds));
        }
      }

      if (use_only_center || !bb) {
        user_fn(base->object->object_to_world().location());
      }
      else {
        for (uint j = 0; j < 8; j++) {
          float co[3];
          mul_v3_m4v3(co, base->object->object_to_world().ptr(), (*bb)[j]);
          user_fn(co);
        }
      }
      totsel++;
      if (r_drawflags) {
        if (orient_index == V3D_ORIENT_GLOBAL) {
          /* Ignore scale/rotate lock flag while global orientation is active.
           * Otherwise when object is rotated, global and local axes are misaligned, implying wrong
           * axis as hidden/locked, see: !133286. */
          protectflag_to_drawflags(base->object->protectflag & OB_LOCK_LOC, r_drawflags);
        }
        else if (ELEM(orient_index, V3D_ORIENT_LOCAL, V3D_ORIENT_GIMBAL)) {
          protectflag_to_drawflags(base->object->protectflag, r_drawflags);
        }
      }
    }
  }

  if (r_mat && ob) {
    *r_mat = ob->object_to_world().ptr();
  }

  return totsel;
}

int calc_gizmo_stats(const bContext *C,
                     const TransformCalcParams *params,
                     TransformBounds *tbounds,
                     RegionView3D *rv3d)
{
  ScrArea *area = CTX_wm_area(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = static_cast<View3D *>(area->spacedata.first);
  int totsel = 0;

  const int pivot_point = scene->toolsettings->transform_pivot_point;
  const short orient_index = params->orientation_index ?
                                 (params->orientation_index - 1) :
                                 BKE_scene_orientation_get_index(scene, SCE_ORIENT_DEFAULT);

  Object *ob = gizmo_3d_transform_space_object_get(scene, view_layer);
  Object *obedit = OBEDIT_FROM_OBACT(ob);

  tbounds->use_matrix_space = false;
  unit_m3(tbounds->axis);

  /* Global, local or normal orientation?
   * if we could check 'totsel' now, this should be skipped with no selection. */
  if (ob) {
    float mat[3][3];
    calc_orientation_from_type_ex(
        scene, view_layer, v3d, rv3d, ob, obedit, orient_index, pivot_point, mat);
    copy_m3_m3(tbounds->axis, mat);
  }

  reset_tw_center(tbounds);

  if (rv3d) {
    /* Transform widget centroid/center. */
    copy_m4_m3(rv3d->twmat, tbounds->axis);
    rv3d->twdrawflag = short(0xFFFF);
  }

  if (params->use_local_axis && (ob && ob->mode & (OB_MODE_EDIT | OB_MODE_POSE))) {
    float diff_mat[3][3];
    copy_m3_m4(diff_mat, ob->object_to_world().ptr());
    normalize_m3(diff_mat);
    invert_m3(diff_mat);
    mul_m3_m3_pre(tbounds->axis, diff_mat);
    normalize_m3(tbounds->axis);

    tbounds->use_matrix_space = true;
    copy_m4_m4(tbounds->matrix_space, ob->object_to_world().ptr());
  }

  const auto gizmo_3d_tbounds_calc_fn = [&](const float3 &co) { calc_tw_center(tbounds, co); };

  totsel = gizmo_3d_foreach_selected(C,
                                     orient_index,
                                     (pivot_point != V3D_AROUND_LOCAL_ORIGINS),
                                     params->use_only_center,
                                     gizmo_3d_tbounds_calc_fn,
                                     nullptr,
                                     rv3d ? &rv3d->twdrawflag : nullptr);

  if (totsel) {
    mul_v3_fl(tbounds->center, 1.0f / float(totsel)); /* Centroid! */

    if (obedit || (ob && (ob->mode & (OB_MODE_POSE | OB_MODE_SCULPT)))) {
      if (ob->mode & OB_MODE_POSE) {
        invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());
      }
      mul_m4_v3(ob->object_to_world().ptr(), tbounds->center);
      mul_m4_v3(ob->object_to_world().ptr(), tbounds->min);
      mul_m4_v3(ob->object_to_world().ptr(), tbounds->max);
    }
  }

  if (rv3d) {
    if (totsel == 0) {
      unit_m4(rv3d->twmat);
      unit_m3(rv3d->tw_axis_matrix);
      zero_v3(rv3d->tw_axis_min);
      zero_v3(rv3d->tw_axis_max);
    }
    else {
      copy_m3_m3(rv3d->tw_axis_matrix, tbounds->axis);
      copy_v3_v3(rv3d->tw_axis_min, tbounds->axis_min);
      copy_v3_v3(rv3d->tw_axis_max, tbounds->axis_max);
    }
  }

  return totsel;
}

static void gizmo_get_idot(const RegionView3D *rv3d, float r_idot[3])
{
  float view_vec[3], axis_vec[3];
  ED_view3d_global_to_vector(rv3d, rv3d->twmat[3], view_vec);
  for (int i = 0; i < 3; i++) {
    normalize_v3_v3(axis_vec, rv3d->twmat[i]);
    r_idot[i] = 1.0f - fabsf(dot_v3v3(view_vec, axis_vec));
  }
}

static bool gizmo_3d_calc_pos(const bContext *C,
                              const Scene *scene,
                              const TransformBounds *tbounds,
                              const short pivot_type,
                              float r_pivot_pos[3])
{
  switch (pivot_type) {
    case V3D_AROUND_CURSOR:
      copy_v3_v3(r_pivot_pos, scene->cursor.location);
      return true;
    case V3D_AROUND_ACTIVE: {
      ViewLayer *view_layer = CTX_data_view_layer(C);
      BKE_view_layer_synced_ensure(scene, view_layer);
      Object *ob = BKE_view_layer_active_object_get(view_layer);
      if (ob != nullptr) {
        if ((ob->mode & OB_MODE_ALL_SCULPT) && ob->sculpt) {
          SculptSession *ss = ob->sculpt;
          copy_v3_v3(r_pivot_pos, ss->pivot_pos);
          return true;
        }
        if (object::calc_active_center(ob, false, r_pivot_pos)) {
          return true;
        }
      }
    }
      [[fallthrough]];
    case V3D_AROUND_CENTER_BOUNDS: {
      TransformBounds tbounds_stack;
      if (tbounds == nullptr) {
        TransformCalcParams calc_params{};
        calc_params.use_only_center = true;
        if (calc_gizmo_stats(C, &calc_params, &tbounds_stack, nullptr)) {
          tbounds = &tbounds_stack;
        }
      }
      if (tbounds) {
        mid_v3_v3v3(r_pivot_pos, tbounds->min, tbounds->max);
        return true;
      }
      break;
    }
    case V3D_AROUND_LOCAL_ORIGINS:
    case V3D_AROUND_CENTER_MEDIAN: {
      if (tbounds) {
        copy_v3_v3(r_pivot_pos, tbounds->center);
        return true;
      }

      float co_sum[3] = {0.0f, 0.0f, 0.0f};
      const auto gizmo_3d_calc_center_fn = [&](const float3 &co) { add_v3_v3(co_sum, co); };
      const float (*r_mat)[4] = nullptr;
      int totsel;
      totsel = gizmo_3d_foreach_selected(C,
                                         0,
                                         (pivot_type != V3D_AROUND_LOCAL_ORIGINS),
                                         true,
                                         gizmo_3d_calc_center_fn,
                                         &r_mat,
                                         nullptr);
      if (totsel) {
        mul_v3_v3fl(r_pivot_pos, co_sum, 1.0f / float(totsel));
        if (r_mat) {
          mul_m4_v3(r_mat, r_pivot_pos);
        }
        return true;
      }
    }
  }

  return false;
}

void gizmo_prepare_mat(const bContext *C, RegionView3D *rv3d, const TransformBounds *tbounds)
{
  Scene *scene = CTX_data_scene(C);
  gizmo_3d_calc_pos(C, scene, tbounds, scene->toolsettings->transform_pivot_point, rv3d->twmat[3]);
}

/**
 * Sets up \a r_start and \a r_len to define arrow line range.
 * Needed to adjust line drawing for combined gizmo axis types.
 */
static void gizmo_line_range(const int twtype, const short axis_type, float *r_start, float *r_end)
{
  float start = 0.2f;
  float end = 1.0f;

  switch (axis_type) {
    case MAN_AXES_TRANSLATE:
      if (twtype & V3D_GIZMO_SHOW_OBJECT_SCALE) {
        start = end - 0.125f;
      }
      if (twtype & V3D_GIZMO_SHOW_OBJECT_ROTATE) {
        /* Avoid rotate and translate gizmos overlap. */
        const float rotate_offset = 0.215f;
        start += rotate_offset;
        end += rotate_offset + 0.2f;
      }
      break;
    case MAN_AXES_SCALE:
      if (twtype & (V3D_GIZMO_SHOW_OBJECT_TRANSLATE | V3D_GIZMO_SHOW_OBJECT_ROTATE)) {
        end -= 0.225f;
      }
      break;
  }

  if (r_start) {
    *r_start = start;
  }
  if (r_end) {
    *r_end = end;
  }
}

void gizmo_xform_message_subscribe(wmGizmoGroup *gzgroup,
                                   wmMsgBus *mbus,
                                   Scene *scene,
                                   bScreen *screen,
                                   ScrArea *area,
                                   ARegion *region,
                                   void (*type_fn)(wmGizmoGroupType *))
{
  /* Subscribe to view properties. */
  wmMsgSubscribeValue msg_sub_value_gz_tag_refresh{};
  msg_sub_value_gz_tag_refresh.owner = region;
  msg_sub_value_gz_tag_refresh.user_data = gzgroup->parent_gzmap;
  msg_sub_value_gz_tag_refresh.notify = WM_gizmo_do_msg_notify_tag_refresh;

  int orient_flag = 0;
  if (type_fn == VIEW3D_GGT_xform_gizmo) {
    GizmoGroup *ggd = static_cast<GizmoGroup *>(gzgroup->customdata);
    orient_flag = ggd->twtype_init;
  }
  else if (type_fn == VIEW3D_GGT_xform_cage) {
    orient_flag = V3D_GIZMO_SHOW_OBJECT_SCALE;
    /* Pass. */
  }
  else if (type_fn == VIEW3D_GGT_xform_shear) {
    orient_flag = V3D_GIZMO_SHOW_OBJECT_ROTATE;
  }
  TransformOrientationSlot *orient_slot = BKE_scene_orientation_slot_get_from_flag(scene,
                                                                                   orient_flag);
  PointerRNA orient_ref_ptr = RNA_pointer_create_discrete(
      &scene->id, &RNA_TransformOrientationSlot, orient_slot);
  const ToolSettings *ts = scene->toolsettings;

  PointerRNA scene_ptr = RNA_id_pointer_create(&scene->id);
  {
    const PropertyRNA *props[] = {
        &rna_Scene_transform_orientation_slots,
    };
    for (int i = 0; i < ARRAY_SIZE(props); i++) {
      WM_msg_subscribe_rna(mbus, &scene_ptr, props[i], &msg_sub_value_gz_tag_refresh, __func__);
    }
  }

  if ((ts->transform_pivot_point == V3D_AROUND_CURSOR) || (orient_slot->type == V3D_ORIENT_CURSOR))
  {
    /* We could be more specific here, for now subscribe to any cursor change. */
    PointerRNA cursor_ptr = RNA_pointer_create_discrete(
        &scene->id, &RNA_View3DCursor, &scene->cursor);
    WM_msg_subscribe_rna(mbus, &cursor_ptr, nullptr, &msg_sub_value_gz_tag_refresh, __func__);
  }

  {
    const PropertyRNA *props[] = {
        &rna_TransformOrientationSlot_type,
        &rna_TransformOrientationSlot_use,
    };
    for (int i = 0; i < ARRAY_SIZE(props); i++) {
      if (props[i]) {
        WM_msg_subscribe_rna(
            mbus, &orient_ref_ptr, props[i], &msg_sub_value_gz_tag_refresh, __func__);
      }
    }
  }

  PointerRNA toolsettings_ptr = RNA_pointer_create_discrete(
      &scene->id, &RNA_ToolSettings, scene->toolsettings);

  if (ELEM(type_fn, VIEW3D_GGT_xform_gizmo, VIEW3D_GGT_xform_shear)) {
    const PropertyRNA *props[] = {
        &rna_ToolSettings_transform_pivot_point,
    };
    for (int i = 0; i < ARRAY_SIZE(props); i++) {
      WM_msg_subscribe_rna(
          mbus, &toolsettings_ptr, props[i], &msg_sub_value_gz_tag_refresh, __func__);
    }
  }

  {
    const PropertyRNA *props[] = {
        &rna_ToolSettings_workspace_tool_type,
    };
    for (int i = 0; i < ARRAY_SIZE(props); i++) {
      WM_msg_subscribe_rna(
          mbus, &toolsettings_ptr, props[i], &msg_sub_value_gz_tag_refresh, __func__);
    }
  }

  PointerRNA view3d_ptr = RNA_pointer_create_discrete(
      &screen->id, &RNA_SpaceView3D, area->spacedata.first);

  if (type_fn == VIEW3D_GGT_xform_gizmo) {
    GizmoGroup *ggd = static_cast<GizmoGroup *>(gzgroup->customdata);
    if (ggd->use_twtype_refresh) {
      const PropertyRNA *props[] = {
          &rna_SpaceView3D_show_gizmo_object_translate,
          &rna_SpaceView3D_show_gizmo_object_rotate,
          &rna_SpaceView3D_show_gizmo_object_scale,
      };
      for (int i = 0; i < ARRAY_SIZE(props); i++) {
        WM_msg_subscribe_rna(mbus, &view3d_ptr, props[i], &msg_sub_value_gz_tag_refresh, __func__);
      }
    }
  }
  else if (type_fn == VIEW3D_GGT_xform_cage) {
    /* Pass. */
  }
  else if (type_fn == VIEW3D_GGT_xform_shear) {
    /* Pass. */
  }
  else {
    BLI_assert(0);
  }

  WM_msg_subscribe_rna_anon_prop(mbus, Window, view_layer, &msg_sub_value_gz_tag_refresh);
  WM_msg_subscribe_rna_anon_prop(mbus, EditBone, lock, &msg_sub_value_gz_tag_refresh);
}

static void gizmo_3d_dial_matrixbasis_calc(const ARegion *region,
                                           const float axis[3],
                                           const float center_global[3],
                                           const float mval_init[2],
                                           float r_mat_basis[4][4])
{
  plane_from_point_normal_v3(r_mat_basis[2], center_global, axis);
  copy_v3_v3(r_mat_basis[3], center_global);

  if (ED_view3d_win_to_3d_on_plane(region, r_mat_basis[2], mval_init, false, r_mat_basis[1])) {
    sub_v3_v3(r_mat_basis[1], center_global);
    normalize_v3(r_mat_basis[1]);
    cross_v3_v3v3(r_mat_basis[0], r_mat_basis[1], r_mat_basis[2]);
  }
  else {
    /* The plane and the mouse direction are parallel.
     * Calculate a matrix orthogonal to the axis. */
    ortho_basis_v3v3_v3(r_mat_basis[0], r_mat_basis[1], r_mat_basis[2]);
  }

  r_mat_basis[0][3] = 0.0f;
  r_mat_basis[1][3] = 0.0f;
  r_mat_basis[2][3] = 0.0f;
  r_mat_basis[3][3] = 1.0f;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Gizmo
 * \{ */

/** Scale of the two-axis planes. */
#define MAN_AXIS_SCALE_PLANE_SCALE 0.7f

static void rotation_get_fn(const wmGizmo * /*gz*/, wmGizmoProperty *gz_prop, void *value)
{
  const GizmoGroup *ggd = (const GizmoGroup *)gz_prop->custom_func.user_data;
  *(float *)value = ggd->rotation;
}

static void rotation_set_fn(const wmGizmo * /*gz*/, wmGizmoProperty *gz_prop, const void *value)
{
  GizmoGroup *ggd = (GizmoGroup *)gz_prop->custom_func.user_data;
  ggd->rotation = *(const float *)value;
}

static void gizmo_3d_setup_default_matrix(wmGizmo *axis, const int axis_idx)
{
  float matrix[3][3];

  switch (axis_idx) {
    /* Arrow. */
    case MAN_AXIS_TRANS_X:
    case MAN_AXIS_SCALE_X:
    case MAN_AXIS_ROT_X:
      copy_v3_fl3(matrix[0], 0.0f, -1.0f, 0.0f);
      copy_v3_fl3(matrix[1], 0.0f, 0.0f, -1.0f);
      copy_v3_fl3(matrix[2], 1.0f, 0.0f, 0.0f);
      break;
    case MAN_AXIS_TRANS_Y:
    case MAN_AXIS_SCALE_Y:
    case MAN_AXIS_ROT_Y:
      copy_v3_fl3(matrix[0], 1.0f, 0.0f, 0.0f);
      copy_v3_fl3(matrix[1], 0.0f, 0.0f, -1.0f);
      copy_v3_fl3(matrix[2], 0.0f, 1.0f, 0.0f);
      break;
    case MAN_AXIS_TRANS_Z:
    case MAN_AXIS_SCALE_Z:
    case MAN_AXIS_ROT_Z:
      copy_v3_fl3(matrix[0], 1.0f, 0.0f, 0.0f);
      copy_v3_fl3(matrix[1], 0.0f, 1.0f, 0.0f);
      copy_v3_fl3(matrix[2], 0.0f, 0.0f, 1.0f);
      break;

    case MAN_AXIS_TRANS_XY:
    case MAN_AXIS_SCALE_XY:
      copy_v3_fl3(matrix[0], -M_SQRT1_2, M_SQRT1_2, 0.0f);
      copy_v3_fl3(matrix[1], 0.0f, 0.0f, 1.0f);
      copy_v3_fl3(matrix[2], M_SQRT1_2, M_SQRT1_2, 0.0f);
      break;
    case MAN_AXIS_TRANS_YZ:
    case MAN_AXIS_SCALE_YZ:
      copy_v3_fl3(matrix[0], 0.0f, -M_SQRT1_2, M_SQRT1_2);
      copy_v3_fl3(matrix[1], 1.0f, 0.0f, 0.0f);
      copy_v3_fl3(matrix[2], 0, M_SQRT1_2, M_SQRT1_2);
      break;
    case MAN_AXIS_SCALE_ZX:
    case MAN_AXIS_TRANS_ZX:
      copy_v3_fl3(matrix[0], M_SQRT1_2, 0.0f, -M_SQRT1_2);
      copy_v3_fl3(matrix[1], 0.0f, 1.0f, 0.0f);
      copy_v3_fl3(matrix[2], M_SQRT1_2, 0.0f, M_SQRT1_2);
      break;

    case MAN_AXIS_TRANS_C:
    case MAN_AXIS_SCALE_C:
    case MAN_AXIS_ROT_C:
    case MAN_AXIS_ROT_T:
    default:
      return;
  }

  copy_m4_m3(axis->matrix_offset, matrix);
}

static void gizmo_3d_setup_draw_default(wmGizmo *axis, const int axis_idx)
{
  gizmo_3d_setup_default_matrix(axis, axis_idx);

  switch (axis_idx) {
    /* Arrow. */
    case MAN_AXIS_TRANS_X:
    case MAN_AXIS_TRANS_Y:
    case MAN_AXIS_TRANS_Z:
      RNA_enum_set(axis->ptr, "draw_style", ED_GIZMO_ARROW_STYLE_NORMAL);
      break;
    case MAN_AXIS_SCALE_X:
    case MAN_AXIS_SCALE_Y:
    case MAN_AXIS_SCALE_Z:
      RNA_enum_set(axis->ptr, "draw_style", ED_GIZMO_ARROW_STYLE_BOX);
      RNA_enum_set(axis->ptr, "draw_options", ED_GIZMO_ARROW_DRAW_FLAG_STEM);
      break;
    case MAN_AXIS_TRANS_XY:
    case MAN_AXIS_TRANS_YZ:
    case MAN_AXIS_TRANS_ZX:
    case MAN_AXIS_SCALE_XY:
    case MAN_AXIS_SCALE_YZ:
    case MAN_AXIS_SCALE_ZX:
      RNA_enum_set(axis->ptr, "draw_style", ED_GIZMO_ARROW_STYLE_PLANE);
      RNA_enum_set(axis->ptr, "draw_options", 0);
      RNA_float_set(axis->ptr, "length", MAN_AXIS_SCALE_PLANE_SCALE);
      break;

    /* Primitive. */
    case MAN_AXIS_TRANS_C:
      RNA_enum_set(axis->ptr, "draw_style", ED_GIZMO_PRIMITIVE_STYLE_CIRCLE);
      RNA_boolean_set(axis->ptr, "draw_inner", false);
      WM_gizmo_set_scale(axis, 0.2f);

      /* Prevent axis gizmos overlapping the center point, see: #63744. */
      axis->select_bias = 2.0f;
      break;
    case MAN_AXIS_SCALE_C:
      RNA_enum_set(axis->ptr, "draw_style", ED_GIZMO_PRIMITIVE_STYLE_ANNULUS);
      RNA_boolean_set(axis->ptr, "draw_inner", false);

      /* Use 6 since this is '1.2' if the main scale is 0.2. */
      RNA_float_set(axis->ptr, "arc_inner_factor", 6.0f);
      WM_gizmo_set_scale(axis, 0.2f);

      /* Prevent axis gizmos overlapping the center point, see: #63744. */
      axis->select_bias = -2.0f;
      break;

    /* Dial. */
    case MAN_AXIS_ROT_X:
    case MAN_AXIS_ROT_Y:
    case MAN_AXIS_ROT_Z:
      RNA_enum_set(axis->ptr, "draw_options", ED_GIZMO_DIAL_DRAW_FLAG_CLIP);
      WM_gizmo_set_flag(axis, WM_GIZMO_DRAW_VALUE, true);
      break;
    case MAN_AXIS_ROT_C:
      RNA_enum_set(axis->ptr, "draw_options", ED_GIZMO_DIAL_DRAW_FLAG_NOP);
      WM_gizmo_set_flag(axis, WM_GIZMO_DRAW_VALUE, true);
      WM_gizmo_set_scale(axis, 1.2f);
      break;
    case MAN_AXIS_ROT_T:
      RNA_enum_set(axis->ptr, "draw_options", ED_GIZMO_DIAL_DRAW_FLAG_FILL);
      WM_gizmo_set_flag(axis, WM_GIZMO_SELECT_BACKGROUND, true);
      WM_gizmo_set_flag(axis, WM_GIZMO_DRAW_HOVER, true);
      break;
  }

  switch (axis_idx) {
    case MAN_AXIS_TRANS_X:
    case MAN_AXIS_TRANS_Y:
    case MAN_AXIS_TRANS_Z:
    case MAN_AXIS_SCALE_X:
    case MAN_AXIS_SCALE_Y:
      WM_gizmo_set_line_width(axis, GIZMO_AXIS_LINE_WIDTH);
      break;
    case MAN_AXIS_TRANS_XY:
    case MAN_AXIS_TRANS_YZ:
    case MAN_AXIS_TRANS_ZX:
    case MAN_AXIS_SCALE_XY:
    case MAN_AXIS_SCALE_YZ:
    case MAN_AXIS_SCALE_ZX:
      WM_gizmo_set_line_width(axis, 1.0f);
      break;
    case MAN_AXIS_ROT_X:
    case MAN_AXIS_ROT_Y:
    case MAN_AXIS_ROT_Z:
      /* Increased line width for better display. */
      WM_gizmo_set_line_width(axis, GIZMO_AXIS_LINE_WIDTH + 1.0f);
      break;
    default:
      WM_gizmo_set_line_width(axis, GIZMO_AXIS_LINE_WIDTH);
      break;
  }

  const short axis_type = gizmo_get_axis_type(axis_idx);
  switch (axis_type) {
    case MAN_AXES_ROTATE: {
      RNA_float_set(axis->ptr, "incremental_angle", 0.0f);
      axis->select_bias = 0.0f;
      break;
    }
    default:
      break;
  }
}

static void gizmo_3d_setup_draw_from_twtype(wmGizmo *axis, const int axis_idx, const int twtype)
{
  switch (axis_idx) {
    case MAN_AXIS_TRANS_X:
    case MAN_AXIS_TRANS_Y:
    case MAN_AXIS_TRANS_Z:
      RNA_enum_set(axis->ptr,
                   "draw_options",
                   (twtype & (V3D_GIZMO_SHOW_OBJECT_ROTATE | V3D_GIZMO_SHOW_OBJECT_SCALE)) ?
                       0 :
                       ED_GIZMO_ARROW_DRAW_FLAG_STEM);
      break;
    default:
      break;
  }

  const short axis_type = gizmo_get_axis_type(axis_idx);
  switch (axis_idx) {
    case MAN_AXIS_TRANS_X:
    case MAN_AXIS_TRANS_Y:
    case MAN_AXIS_TRANS_Z:
    case MAN_AXIS_SCALE_X:
    case MAN_AXIS_SCALE_Y:
    case MAN_AXIS_SCALE_Z: {
      float start;
      float end;
      gizmo_line_range(twtype, axis_type, &start, &end);
      mul_v3_v3fl(axis->matrix_offset[3], axis->matrix_offset[2], start);

      RNA_float_set(axis->ptr, "length", end - start);
      WM_gizmo_set_flag(axis, WM_GIZMO_DRAW_OFFSET_SCALE, true);
      break;
    }
    default:
      break;
  }

  switch (axis_type) {
    case MAN_AXES_ROTATE: {
      if ((twtype & V3D_GIZMO_SHOW_OBJECT_SCALE) && twtype & V3D_GIZMO_SHOW_OBJECT_ROTATE) {
        axis->select_bias = -2.0f;
      }
    }
  }
}

static void gizmo_3d_setup_draw_modal(wmGizmo *axis, const int axis_idx, const int twtype)
{
  const short axis_type = gizmo_get_axis_type(axis_idx);
  switch (axis_idx) {
    case MAN_AXIS_TRANS_X:
    case MAN_AXIS_TRANS_Y:
    case MAN_AXIS_TRANS_Z:
    case MAN_AXIS_SCALE_X:
    case MAN_AXIS_SCALE_Y:
    case MAN_AXIS_SCALE_Z: {

      float end, start_co[3] = {0.0f, 0.0f, 0.0f};
      gizmo_line_range(twtype, axis_type, nullptr, &end);
      RNA_float_set(axis->ptr, "length", end);
      RNA_enum_set(axis->ptr,
                   "draw_options",
                   ED_GIZMO_ARROW_DRAW_FLAG_STEM | ED_GIZMO_ARROW_DRAW_FLAG_ORIGIN);
      WM_gizmo_set_matrix_offset_location(axis, start_co);
      WM_gizmo_set_flag(axis, WM_GIZMO_DRAW_OFFSET_SCALE, false);
      break;
    }
    case MAN_AXIS_TRANS_XY:
    case MAN_AXIS_TRANS_YZ:
    case MAN_AXIS_TRANS_ZX:
    case MAN_AXIS_SCALE_XY:
    case MAN_AXIS_SCALE_YZ:
    case MAN_AXIS_SCALE_ZX:
      RNA_enum_set(axis->ptr, "draw_options", ED_GIZMO_ARROW_DRAW_FLAG_ORIGIN);
      break;
    case MAN_AXIS_SCALE_C:
      RNA_enum_set(axis->ptr, "draw_style", ED_GIZMO_PRIMITIVE_STYLE_CIRCLE);
      break;
    default:
      break;
  }

  switch (axis_type) {
    case MAN_AXES_ROTATE: {
      PropertyRNA *prop = RNA_struct_find_property(axis->ptr, "draw_options");
      const int dial_flag = RNA_property_enum_get(axis->ptr, prop);
      RNA_property_enum_set(axis->ptr, prop, dial_flag | ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_VALUE);
      break;
    }
    default:
      break;
  }
}

static GizmoGroup *gizmogroup_init(wmGizmoGroup *gzgroup)
{
  GizmoGroup *ggd = MEM_callocN<GizmoGroup>(__func__);

  const wmGizmoType *gzt_arrow = WM_gizmotype_find("GIZMO_GT_arrow_3d", true);
  const wmGizmoType *gzt_dial = WM_gizmotype_find("GIZMO_GT_dial_3d", true);
  const wmGizmoType *gzt_prim = WM_gizmotype_find("GIZMO_GT_primitive_3d", true);

  wmGizmoPropertyFnParams params{};
  params.value_get_fn = rotation_get_fn;
  params.value_set_fn = rotation_set_fn;
  params.user_data = ggd;

#define GIZMO_NEW_ARROW(v) \
  { \
    ggd->gizmos[v] = WM_gizmo_new_ptr(gzt_arrow, gzgroup, nullptr); \
  } \
  ((void)0)
#define GIZMO_NEW_DIAL(v) \
  { \
    ggd->gizmos[v] = WM_gizmo_new_ptr(gzt_dial, gzgroup, nullptr); \
    WM_gizmo_target_property_def_func(ggd->gizmos[v], "offset", &params); \
  } \
  ((void)0)
#define GIZMO_NEW_PRIM(v) \
  { \
    ggd->gizmos[v] = WM_gizmo_new_ptr(gzt_prim, gzgroup, nullptr); \
  } \
  ((void)0)

  /* Add/init widgets - order matters! */
  GIZMO_NEW_DIAL(MAN_AXIS_ROT_T);

  GIZMO_NEW_PRIM(MAN_AXIS_SCALE_C);

  GIZMO_NEW_ARROW(MAN_AXIS_SCALE_X);
  GIZMO_NEW_ARROW(MAN_AXIS_SCALE_Y);
  GIZMO_NEW_ARROW(MAN_AXIS_SCALE_Z);

  GIZMO_NEW_ARROW(MAN_AXIS_SCALE_XY);
  GIZMO_NEW_ARROW(MAN_AXIS_SCALE_YZ);
  GIZMO_NEW_ARROW(MAN_AXIS_SCALE_ZX);

  GIZMO_NEW_DIAL(MAN_AXIS_ROT_X);
  GIZMO_NEW_DIAL(MAN_AXIS_ROT_Y);
  GIZMO_NEW_DIAL(MAN_AXIS_ROT_Z);

  /* Initialize screen aligned widget last here, looks better, behaves better. */
  GIZMO_NEW_DIAL(MAN_AXIS_ROT_C);

  GIZMO_NEW_PRIM(MAN_AXIS_TRANS_C);

  GIZMO_NEW_ARROW(MAN_AXIS_TRANS_X);
  GIZMO_NEW_ARROW(MAN_AXIS_TRANS_Y);
  GIZMO_NEW_ARROW(MAN_AXIS_TRANS_Z);

  GIZMO_NEW_ARROW(MAN_AXIS_TRANS_XY);
  GIZMO_NEW_ARROW(MAN_AXIS_TRANS_YZ);
  GIZMO_NEW_ARROW(MAN_AXIS_TRANS_ZX);

  MAN_ITER_AXES_BEGIN (axis, axis_idx) {
    gizmo_3d_setup_draw_default(axis, axis_idx);
  }
  MAN_ITER_AXES_END;

  return ggd;
}

/**
 * Custom handler for gizmo widgets
 */
static wmOperatorStatus gizmo_modal(bContext *C,
                                    wmGizmo *widget,
                                    const wmEvent *event,
                                    eWM_GizmoFlagTweak /*tweak_flag*/)
{
  /* Avoid unnecessary updates, partially address: #55458. */
  if (ELEM(event->type, TIMER, INBETWEEN_MOUSEMOVE)) {
    return OPERATOR_RUNNING_MODAL;
  }

  ARegion *region = CTX_wm_region(C);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  wmGizmoGroup *gzgroup = widget->parent_gzgroup;

  /* Recalculating the orientation has two problems.
   * - The matrix calculated based on the transformed selection may not match the matrix
   *   that was set when transform started.
   * - Inspecting the selection for every update is expensive (for *every* redraw).
   *
   * Instead, use #transform_apply_matrix to transform `rv3d->twmat` or the final scale value
   * when scaling.
   */
  if (false) {
    TransformBounds tbounds;

    TransformCalcParams calc_params{};
    calc_params.use_only_center = true;
    if (calc_gizmo_stats(C, &calc_params, &tbounds, rv3d)) {
      gizmo_prepare_mat(C, rv3d, &tbounds);
      LISTBASE_FOREACH (wmGizmo *, gz, &gzgroup->gizmos) {
        WM_gizmo_set_matrix_location(gz, rv3d->twmat[3]);
      }
    }
  }
  else {
    wmWindow *win = CTX_wm_window(C);
    wmOperator *op = nullptr;
    for (const wmGizmoOpElem &gzop : widget->op_data) {
      op = WM_operator_find_modal_by_type(win, gzop.type);
      if (op != nullptr) {
        break;
      }
    }

    if (op != nullptr) {
      GizmoGroup *ggd = static_cast<GizmoGroup *>(gzgroup->customdata);
      const int axis_idx = BLI_array_findindex(ggd->gizmos, ARRAY_SIZE(ggd->gizmos), &widget);
      const short axis_type = gizmo_get_axis_type(axis_idx);

      float twmat[4][4];
      float scale_buf[3];
      float *scale = nullptr;
      bool update = false;
      copy_m4_m4(twmat, rv3d->twmat);

      if (axis_type == MAN_AXES_SCALE) {
        scale = scale_buf;
        transform_final_value_get(static_cast<const TransInfo *>(op->customdata), scale, 3);
        update = true;
      }
      else if (axis_type == MAN_AXES_ROTATE) {
        transform_final_value_get(
            static_cast<const TransInfo *>(op->customdata), &ggd->rotation, 1);
        if (widget != ggd->gizmos[MAN_AXIS_ROT_C]) {
          ggd->rotation *= -1;
        }
        RNA_float_set(
            widget->ptr,
            "incremental_angle",
            transform_snap_increment_get(static_cast<const TransInfo *>(op->customdata)));
      }
      else if (transform_apply_matrix(static_cast<TransInfo *>(op->customdata), twmat)) {
        update = true;
      }

      if (update) {
        gizmogroup_refresh_from_matrix(gzgroup, twmat, scale, true);
        ED_region_tag_redraw_editor_overlays(region);
      }
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

static void gizmogroup_init_properties_from_twtype(wmGizmoGroup *gzgroup)
{
  struct {
    wmOperatorType *translate, *rotate, *trackball, *resize;
  } ot_store = {nullptr};
  GizmoGroup *ggd = static_cast<GizmoGroup *>(gzgroup->customdata);

  MAN_ITER_AXES_BEGIN (axis, axis_idx) {
    const short axis_type = gizmo_get_axis_type(axis_idx);
    bool constraint_axis[3] = {true, false, false};
    PointerRNA *ptr = nullptr;

    gizmo_get_axis_constraint(axis_idx, constraint_axis);

    /* Custom handler! */
    WM_gizmo_set_fn_custom_modal(axis, gizmo_modal);

    gizmo_3d_setup_draw_from_twtype(axis, axis_idx, ggd->twtype);

    switch (axis_type) {
      case MAN_AXES_TRANSLATE:
        if (ot_store.translate == nullptr) {
          ot_store.translate = WM_operatortype_find("TRANSFORM_OT_translate", true);
        }
        ptr = WM_gizmo_operator_set(axis, 0, ot_store.translate, nullptr);
        break;
      case MAN_AXES_ROTATE: {
        wmOperatorType *ot_rotate;
        if (axis_idx == MAN_AXIS_ROT_T) {
          if (ot_store.trackball == nullptr) {
            ot_store.trackball = WM_operatortype_find("TRANSFORM_OT_trackball", true);
          }
          ot_rotate = ot_store.trackball;
        }
        else {
          if (ot_store.rotate == nullptr) {
            ot_store.rotate = WM_operatortype_find("TRANSFORM_OT_rotate", true);
          }
          ot_rotate = ot_store.rotate;
        }
        ptr = WM_gizmo_operator_set(axis, 0, ot_rotate, nullptr);
        break;
      }
      case MAN_AXES_SCALE: {
        if (ot_store.resize == nullptr) {
          ot_store.resize = WM_operatortype_find("TRANSFORM_OT_resize", true);
        }
        ptr = WM_gizmo_operator_set(axis, 0, ot_store.resize, nullptr);
        break;
      }
    }

    if (ptr) {
      PropertyRNA *prop;
      if (ELEM(true, UNPACK3(constraint_axis))) {
        if ((prop = RNA_struct_find_property(ptr, "constraint_axis"))) {
          RNA_property_boolean_set_array(ptr, prop, constraint_axis);
        }
      }

      RNA_boolean_set(ptr, "release_confirm", true);
    }
  }
  MAN_ITER_AXES_END;
}

static void WIDGETGROUP_gizmo_setup(const bContext *C, wmGizmoGroup *gzgroup)
{
  GizmoGroup *ggd = gizmogroup_init(gzgroup);

  gzgroup->customdata = ggd;

  {
    ScrArea *area = CTX_wm_area(C);
    const bToolRef *tref = area->runtime.tool;

    ggd->twtype = 0;
    if (tref && STREQ(tref->idname, "builtin.move")) {
      ggd->twtype |= V3D_GIZMO_SHOW_OBJECT_TRANSLATE;
    }
    else if (tref && STREQ(tref->idname, "builtin.rotate")) {
      ggd->twtype |= V3D_GIZMO_SHOW_OBJECT_ROTATE;
    }
    else if (tref && STREQ(tref->idname, "builtin.scale")) {
      ggd->twtype |= V3D_GIZMO_SHOW_OBJECT_SCALE;
    }
    else if (tref && STREQ(tref->idname, "builtin.transform")) {
      ggd->twtype = V3D_GIZMO_SHOW_OBJECT_TRANSLATE | V3D_GIZMO_SHOW_OBJECT_ROTATE |
                    V3D_GIZMO_SHOW_OBJECT_SCALE;
    }
    else {
      /* This is also correct logic for 'builtin.transform', no special check needed. */
      /* Setup all gizmos, they can be toggled via #ToolSettings::gizmo_flag. */
      ggd->twtype = V3D_GIZMO_SHOW_OBJECT_TRANSLATE | V3D_GIZMO_SHOW_OBJECT_ROTATE |
                    V3D_GIZMO_SHOW_OBJECT_SCALE;
      ggd->use_twtype_refresh = true;
    }
    BLI_assert(ggd->twtype != 0);
    ggd->twtype_init = ggd->twtype;
  }

  /* *** set properties for axes *** */
  gizmogroup_init_properties_from_twtype(gzgroup);
}

/**
 * Set properties for axes.
 *
 * \param twmat: The transform matrix (typically #RegionView3D.twmat).
 * \param scale: Optional scale, to show scale while modally dragging the scale handles.
 */
static void gizmo_refresh_from_matrix(wmGizmo *axis,
                                      const int axis_idx,
                                      const int twtype,
                                      const float twmat[4][4],
                                      const float scale[3])
{
  const short axis_type = gizmo_get_axis_type(axis_idx);
  const int aidx_norm = gizmo_orientation_axis(axis_idx, nullptr);

  switch (axis_idx) {
    case MAN_AXIS_TRANS_XY:
    case MAN_AXIS_TRANS_YZ:
    case MAN_AXIS_TRANS_ZX:
    case MAN_AXIS_SCALE_XY:
    case MAN_AXIS_SCALE_YZ:
    case MAN_AXIS_SCALE_ZX:
      copy_m4_m4(axis->matrix_basis, twmat);
      if (scale) {
        RNA_float_set(axis->ptr,
                      "length",
                      MAN_AXIS_SCALE_PLANE_SCALE * scale[aidx_norm == 2 ? 0 : aidx_norm + 1]);
      }
      break;
    case MAN_AXIS_SCALE_X:
    case MAN_AXIS_SCALE_Y:
    case MAN_AXIS_SCALE_Z:
      copy_m4_m4(axis->matrix_basis, twmat);
      if (scale) {
        float end;
        gizmo_line_range(twtype, axis_type, nullptr, &end);
        RNA_float_set(axis->ptr, "length", end * scale[aidx_norm]);
      }

      break;
    case MAN_AXIS_TRANS_X:
    case MAN_AXIS_TRANS_Y:
    case MAN_AXIS_TRANS_Z:
      copy_m4_m4(axis->matrix_basis, twmat);
      break;
    case MAN_AXIS_SCALE_C:
      WM_gizmo_set_matrix_location(axis, twmat[3]);
      if (scale) {
        WM_gizmo_set_scale(axis, 0.2f * scale[0]);
      }
      break;
    case MAN_AXIS_ROT_X:
    case MAN_AXIS_ROT_Y:
    case MAN_AXIS_ROT_Z:
      copy_m4_m4(axis->matrix_basis, twmat);
      orthogonalize_m4(axis->matrix_basis, aidx_norm);
      break;
    case MAN_AXIS_ROT_C:
    case MAN_AXIS_ROT_T:
    default:
      WM_gizmo_set_matrix_location(axis, twmat[3]);
      break;
  }
}

static void gizmogroup_refresh_from_matrix(wmGizmoGroup *gzgroup,
                                           const float twmat[4][4],
                                           const float scale[3],
                                           const bool ignore_hidden)
{
  GizmoGroup *ggd = static_cast<GizmoGroup *>(gzgroup->customdata);

  MAN_ITER_AXES_BEGIN (axis, axis_idx) {
    if (ignore_hidden && axis->flag & WM_GIZMO_HIDDEN) {
      continue;
    }
    gizmo_refresh_from_matrix(axis, axis_idx, ggd->twtype, twmat, scale);
  }
  MAN_ITER_AXES_END;
}

static void WIDGETGROUP_gizmo_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  if (WM_gizmo_group_is_modal(gzgroup)) {
    return;
  }

  ARegion *region = CTX_wm_region(C);
  GizmoGroup *ggd = static_cast<GizmoGroup *>(gzgroup->customdata);
  Scene *scene = CTX_data_scene(C);
  ScrArea *area = CTX_wm_area(C);
  View3D *v3d = static_cast<View3D *>(area->spacedata.first);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  TransformBounds tbounds;

  if (ggd->use_twtype_refresh) {
    ggd->twtype = v3d->gizmo_show_object & ggd->twtype_init;
    if (ggd->twtype != ggd->twtype_prev) {
      ggd->twtype_prev = ggd->twtype;
      gizmogroup_init_properties_from_twtype(gzgroup);
    }
  }

  const int orient_index = BKE_scene_orientation_get_index_from_flag(scene, ggd->twtype_init);

  /* Skip, we don't draw anything anyway. */
  TransformCalcParams calc_params{};
  calc_params.use_only_center = true;
  calc_params.orientation_index = orient_index + 1;
  if ((ggd->all_hidden = (calc_gizmo_stats(C, &calc_params, &tbounds, rv3d) == 0))) {
    return;
  }

  gizmo_3d_calc_pos(
      C, scene, &tbounds, scene->toolsettings->transform_pivot_point, rv3d->twmat[3]);

  gizmogroup_refresh_from_matrix(gzgroup, rv3d->twmat, nullptr, false);
}

static void WIDGETGROUP_gizmo_message_subscribe(const bContext *C,
                                                wmGizmoGroup *gzgroup,
                                                wmMsgBus *mbus)
{
  Scene *scene = CTX_data_scene(C);
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  gizmo_xform_message_subscribe(
      gzgroup, mbus, scene, screen, area, region, VIEW3D_GGT_xform_gizmo);
}

static void gizmogroup_hide_all(GizmoGroup *ggd)
{
  MAN_ITER_AXES_BEGIN (axis, axis_idx) {
    WM_gizmo_set_flag(axis, WM_GIZMO_HIDDEN, true);
  }
  MAN_ITER_AXES_END;
}

static void WIDGETGROUP_gizmo_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
  GizmoGroup *ggd = static_cast<GizmoGroup *>(gzgroup->customdata);
  // ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  // View3D *v3d =static_cast< View3D *> (area->spacedata.first);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  float viewinv_m3[3][3];
  copy_m3_m4(viewinv_m3, rv3d->viewinv);
  float idot[3];

  /* Re-calculate hidden unless modal. */
  const bool is_modal = WM_gizmo_group_is_modal(gzgroup);

  /* When looking through a selected camera, the gizmo can be at the
   * exact same position as the view, skip so we don't break selection. */
  if (ggd->all_hidden || fabsf(ED_view3d_pixel_size(rv3d, rv3d->twmat[3])) < 5e-7f) {
    if (!is_modal) {
      gizmogroup_hide_all(ggd);
    }
    return;
  }
  gizmo_get_idot(rv3d, idot);

  /* *** set properties for axes *** */
  MAN_ITER_AXES_BEGIN (axis, axis_idx) {
    if (is_modal) {
      if (axis->flag & WM_GIZMO_HIDDEN) {
        continue;
      }
    }
    else {
      const short axis_type = gizmo_get_axis_type(axis_idx);
      if (gizmo_is_axis_visible(rv3d, ggd->twtype, idot, axis_type, axis_idx)) {
        /* XXX maybe unset _HIDDEN flag on redraw? */
        WM_gizmo_set_flag(axis, WM_GIZMO_HIDDEN, false);
      }
      else {
        WM_gizmo_set_flag(axis, WM_GIZMO_HIDDEN, true);
        continue;
      }

      /* Align to view. */
      switch (axis_idx) {
        case MAN_AXIS_TRANS_C:
        case MAN_AXIS_ROT_C:
        case MAN_AXIS_SCALE_C:
        case MAN_AXIS_ROT_T:
          WM_gizmo_set_matrix_rotation_from_z_axis(axis, rv3d->viewinv[2]);
          break;
      }
    }

    float color[4], color_hi[4];
    gizmo_get_axis_color(axis_idx, idot, color, color_hi);
    WM_gizmo_set_color(axis, color);
    WM_gizmo_set_color_highlight(axis, color_hi);
  }
  MAN_ITER_AXES_END;

  /* Refresh handled above when using view orientation. */
  if (!equals_m3m3(viewinv_m3, ggd->prev.viewinv_m3)) {
    {
      Scene *scene = CTX_data_scene(C);
      const TransformOrientationSlot *orient_slot = BKE_scene_orientation_slot_get_from_flag(
          scene, ggd->twtype_init);
      switch (orient_slot->type) {
        case V3D_ORIENT_VIEW: {
          WIDGETGROUP_gizmo_refresh(C, gzgroup);
          break;
        }
      }
    }
    copy_m3_m4(ggd->prev.viewinv_m3, rv3d->viewinv);
  }
}

static void gizmo_3d_draw_invoke(wmGizmoGroup *gzgroup,
                                 const ARegion *region,
                                 const int axis_idx_active,
                                 const float mval[2])
{
  GizmoGroup *ggd = static_cast<GizmoGroup *>(gzgroup->customdata);
  const RegionView3D *rv3d = static_cast<const RegionView3D *>(region->regiondata);

  wmGizmo *axis_active = ggd->gizmos[axis_idx_active];

  const short axis_active_type = gizmo_get_axis_type(axis_idx_active);

  /* Display only the active gizmo. */
  gizmogroup_hide_all(ggd);
  WM_gizmo_set_flag(axis_active, WM_GIZMO_HIDDEN, false);
  gizmo_refresh_from_matrix(axis_active, axis_idx_active, ggd->twtype, rv3d->twmat, nullptr);

  if (ELEM(axis_idx_active, MAN_AXIS_TRANS_C, MAN_AXIS_SCALE_C, MAN_AXIS_ROT_C, MAN_AXIS_ROT_T)) {
    WM_gizmo_set_matrix_rotation_from_z_axis(axis_active, rv3d->viewinv[2]);
  }

  gizmo_3d_setup_draw_modal(axis_active, axis_idx_active, ggd->twtype);

  if (axis_active_type == MAN_AXES_TRANSLATE) {
    /* Arrows are used for visual reference, so keep all visible. */
    for (int axis_idx = MAN_AXIS_TRANS_X; axis_idx <= MAN_AXIS_TRANS_Z; axis_idx++) {
      if (axis_idx == axis_idx_active) {
        continue;
      }
      wmGizmo *axis = ggd->gizmos[axis_idx];
      WM_gizmo_set_flag(axis, WM_GIZMO_HIDDEN, false);
      gizmo_refresh_from_matrix(axis, axis_idx, ggd->twtype, rv3d->twmat, nullptr);
      gizmo_3d_setup_draw_default(axis, axis_idx);
      gizmo_3d_setup_draw_from_twtype(axis, axis_idx, ggd->twtype);
      RNA_enum_set(axis->ptr, "draw_options", ED_GIZMO_ARROW_DRAW_FLAG_STEM);
    }
  }
  else if (axis_active_type == MAN_AXES_ROTATE && axis_idx_active != MAN_AXIS_ROT_T) {
    float mat[3][3];
    mul_m3_m4m4(mat, axis_active->matrix_basis, axis_active->matrix_offset);
    gizmo_3d_dial_matrixbasis_calc(
        region, mat[2], axis_active->matrix_basis[3], mval, axis_active->matrix_offset);

    copy_m3_m4(mat, axis_active->matrix_basis);
    invert_m3(mat);
    mul_m4_m3m4(axis_active->matrix_offset, mat, axis_active->matrix_offset);
    zero_v3(axis_active->matrix_offset[3]);
  }
}

static void WIDGETGROUP_gizmo_invoke_prepare(const bContext *C,
                                             wmGizmoGroup *gzgroup,
                                             wmGizmo *gz,
                                             const wmEvent *event)
{
  GizmoGroup *ggd = static_cast<GizmoGroup *>(gzgroup->customdata);
  const int axis_idx = BLI_array_findindex(ggd->gizmos, ARRAY_SIZE(ggd->gizmos), &gz);

  const float mval[2] = {float(event->mval[0]), float(event->mval[1])};
  gizmo_3d_draw_invoke(gzgroup, CTX_wm_region(C), axis_idx, mval);

  /* Support gizmo specific orientation. */
  if (gz != ggd->gizmos[MAN_AXIS_ROT_T]) {
    Scene *scene = CTX_data_scene(C);
    wmGizmoOpElem *gzop = WM_gizmo_operator_get(gz, 0);
    PointerRNA *ptr = &gzop->ptr;
    PropertyRNA *prop_orient_type = RNA_struct_find_property(ptr, "orient_type");
    const TransformOrientationSlot *orient_slot = BKE_scene_orientation_slot_get_from_flag(
        scene, ggd->twtype_init);
    if ((gz == ggd->gizmos[MAN_AXIS_ROT_C]) ||
        (orient_slot == &scene->orientation_slots[SCE_ORIENT_DEFAULT]))
    {
      /* #MAN_AXIS_ROT_C always uses the #V3D_ORIENT_VIEW orientation,
       * optionally we could set this orientation instead of unset the property. */
      RNA_property_unset(ptr, prop_orient_type);
    }
    else {
      /* TODO: API function. */
      int index = BKE_scene_orientation_slot_get_index(orient_slot);
      RNA_property_enum_set(ptr, prop_orient_type, index);
    }
  }

  /* Support shift click to constrain axis. */
  int axis = -1;
  switch (axis_idx) {
    case MAN_AXIS_TRANS_X:
    case MAN_AXIS_TRANS_Y:
    case MAN_AXIS_TRANS_Z:
      axis = axis_idx - MAN_AXIS_TRANS_X;
      break;
    case MAN_AXIS_SCALE_X:
    case MAN_AXIS_SCALE_Y:
    case MAN_AXIS_SCALE_Z:
      axis = axis_idx - MAN_AXIS_SCALE_X;
      break;
  }

  if (axis != -1) {
    /* Swap single axis for two-axis constraint. */
    const bool flip = (event->modifier & KM_SHIFT) != 0;
    BLI_assert(axis_idx != -1);
    const short axis_type = gizmo_get_axis_type(axis_idx);
    if (axis_type != MAN_AXES_ROTATE) {
      wmGizmoOpElem *gzop = WM_gizmo_operator_get(gz, 0);
      PointerRNA *ptr = &gzop->ptr;
      PropertyRNA *prop_constraint_axis = RNA_struct_find_property(ptr, "constraint_axis");
      if (prop_constraint_axis) {
        bool constraint[3] = {false};
        constraint[axis] = true;
        if (flip) {
          for (int i = 0; i < ARRAY_SIZE(constraint); i++) {
            constraint[i] = !constraint[i];
          }
        }
        RNA_property_boolean_set_array(ptr, prop_constraint_axis, constraint);
      }
    }
  }
}

static bool WIDGETGROUP_gizmo_poll_generic(View3D *v3d)
{
  if (v3d->gizmo_flag & V3D_GIZMO_HIDE) {
    return false;
  }
  if (G.moving & (G_TRANSFORM_OBJ | G_TRANSFORM_EDIT)) {
    return false;
  }
  return true;
}

static bool WIDGETGROUP_gizmo_poll_context(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  ScrArea *area = CTX_wm_area(C);
  View3D *v3d = static_cast<View3D *>(area->spacedata.first);
  if (!WIDGETGROUP_gizmo_poll_generic(v3d)) {
    return false;
  }

  const bToolRef *tref = area->runtime.tool;
  if (v3d->gizmo_flag & V3D_GIZMO_HIDE_CONTEXT) {
    return false;
  }
  if ((v3d->gizmo_show_object & (V3D_GIZMO_SHOW_OBJECT_TRANSLATE | V3D_GIZMO_SHOW_OBJECT_ROTATE |
                                 V3D_GIZMO_SHOW_OBJECT_SCALE)) == 0)
  {
    return false;
  }

  /* Don't show if the tool has a gizmo. */
  if (tref && tref->runtime && tref->runtime->gizmo_group[0]) {
    return false;
  }
  return true;
}

static bool WIDGETGROUP_gizmo_poll_tool(const bContext *C, wmGizmoGroupType *gzgt)
{
  if (!ED_gizmo_poll_or_unlink_delayed_from_tool(C, gzgt)) {
    return false;
  }

  ScrArea *area = CTX_wm_area(C);
  View3D *v3d = static_cast<View3D *>(area->spacedata.first);
  if (!WIDGETGROUP_gizmo_poll_generic(v3d)) {
    return false;
  }

  if (v3d->gizmo_flag & V3D_GIZMO_HIDE_TOOL) {
    return false;
  }

  return true;
}

/* Expose as multiple gizmos so tools use one, persistent context another.
 * Needed because they use different options which isn't so simple to dynamically update. */

void VIEW3D_GGT_xform_gizmo(wmGizmoGroupType *gzgt)
{
  gzgt->name = "3D View: Transform Gizmo";
  gzgt->idname = "VIEW3D_GGT_xform_gizmo";

  gzgt->flag = WM_GIZMOGROUPTYPE_3D | WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP |
               WM_GIZMOGROUPTYPE_DELAY_REFRESH_FOR_TWEAK;

  gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  gzgt->poll = WIDGETGROUP_gizmo_poll_tool;
  gzgt->setup = WIDGETGROUP_gizmo_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->refresh = WIDGETGROUP_gizmo_refresh;
  gzgt->message_subscribe = WIDGETGROUP_gizmo_message_subscribe;
  gzgt->draw_prepare = WIDGETGROUP_gizmo_draw_prepare;
  gzgt->invoke_prepare = WIDGETGROUP_gizmo_invoke_prepare;

  static const EnumPropertyItem rna_enum_gizmo_items[] = {
      {V3D_GIZMO_SHOW_OBJECT_TRANSLATE, "TRANSLATE", 0, "Move", ""},
      {V3D_GIZMO_SHOW_OBJECT_ROTATE, "ROTATE", 0, "Rotate", ""},
      {V3D_GIZMO_SHOW_OBJECT_SCALE, "SCALE", 0, "Scale", ""},
      {0, "NONE", 0, "None", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };
  RNA_def_enum(gzgt->srna,
               "drag_action",
               rna_enum_gizmo_items,
               V3D_GIZMO_SHOW_OBJECT_TRANSLATE,
               "Drag Action",
               "");

  g_GGT_xform_gizmo = gzgt;
}

void VIEW3D_GGT_xform_gizmo_context(wmGizmoGroupType *gzgt)
{
  gzgt->name = "3D View: Transform Gizmo Context";
  gzgt->idname = "VIEW3D_GGT_xform_gizmo_context";

  gzgt->flag = WM_GIZMOGROUPTYPE_3D | WM_GIZMOGROUPTYPE_PERSISTENT |
               WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP | WM_GIZMOGROUPTYPE_DELAY_REFRESH_FOR_TWEAK;

  gzgt->poll = WIDGETGROUP_gizmo_poll_context;
  gzgt->setup = WIDGETGROUP_gizmo_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->refresh = WIDGETGROUP_gizmo_refresh;
  gzgt->message_subscribe = WIDGETGROUP_gizmo_message_subscribe;
  gzgt->draw_prepare = WIDGETGROUP_gizmo_draw_prepare;
  gzgt->invoke_prepare = WIDGETGROUP_gizmo_invoke_prepare;

  g_GGT_xform_gizmo_context = gzgt;
}

/** \} */

static wmGizmoGroup *gizmogroup_xform_find(TransInfo *t)
{
  wmGizmoMap *gizmo_map = t->region->runtime->gizmo_map;
  if (gizmo_map == nullptr) {
    BLI_assert_msg(false, "#T_NO_GIZMO should already be set to return early before.");
    return nullptr;
  }

  wmGizmo *gizmo_modal_current = WM_gizmomap_get_modal(gizmo_map);
  if (gizmo_modal_current) {
    wmGizmoGroup *gzgroup = gizmo_modal_current->parent_gzgroup;
    /* Check #wmGizmoGroup::customdata to make sure the GizmoGroup has been initialized. */
    if (gzgroup->customdata && ELEM(gzgroup->type, g_GGT_xform_gizmo, g_GGT_xform_gizmo_context)) {
      return gzgroup;
    }
  }
  else {
    /* See #WM_gizmomap_group_find_ptr. */
    LISTBASE_FOREACH (wmGizmoGroup *, gzgroup, WM_gizmomap_group_list(gizmo_map)) {
      if (ELEM(gzgroup->type, g_GGT_xform_gizmo, g_GGT_xform_gizmo_context)) {
        /* Choose the one that has been initialized. */
        if (gzgroup->customdata) {
          return gzgroup;
        }
      }
    }
  }

  return nullptr;
}

void transform_gizmo_3d_model_from_constraint_and_mode_init(TransInfo *t)
{
  wmGizmo *gizmo_modal_current = t->region && t->region->runtime->gizmo_map ?
                                     WM_gizmomap_get_modal(t->region->runtime->gizmo_map) :
                                     nullptr;
  if (!gizmo_modal_current || !ELEM(gizmo_modal_current->parent_gzgroup->type,
                                    g_GGT_xform_gizmo,
                                    g_GGT_xform_gizmo_context))
  {
    t->flag |= T_NO_GIZMO;
  }
}

void transform_gizmo_3d_model_from_constraint_and_mode_set(TransInfo *t)
{
  if (t->flag & T_NO_GIZMO) {
    return;
  }

  wmGizmoGroup *gzgroup_xform = gizmogroup_xform_find(t);
  if (gzgroup_xform == nullptr) {
    return;
  }

  int axis_idx = -1;
  if (t->mode == TFM_TRACKBALL) {
    /* Pass. Do not display gizmo. */
  }
  else if (ELEM(t->mode, TFM_TRANSLATION, TFM_ROTATION, TFM_RESIZE)) {
    const int axis_map[3][7] = {
        {MAN_AXIS_TRANS_X,
         MAN_AXIS_TRANS_Y,
         MAN_AXIS_TRANS_XY,
         MAN_AXIS_TRANS_Z,
         MAN_AXIS_TRANS_ZX,
         MAN_AXIS_TRANS_YZ,
         MAN_AXIS_TRANS_C},
        {MAN_AXIS_ROT_X,
         MAN_AXIS_ROT_Y,
         MAN_AXIS_ROT_Z,
         MAN_AXIS_ROT_Z,
         MAN_AXIS_ROT_Y,
         MAN_AXIS_ROT_X,
         MAN_AXIS_ROT_C},
        {MAN_AXIS_SCALE_X,
         MAN_AXIS_SCALE_Y,
         MAN_AXIS_SCALE_XY,
         MAN_AXIS_SCALE_Z,
         MAN_AXIS_SCALE_ZX,
         MAN_AXIS_SCALE_YZ,
         MAN_AXIS_SCALE_C},
    };

    BLI_STATIC_ASSERT(
        /* Assert mode values. */
        ((TFM_ROTATION == TFM_TRANSLATION + 1) && (TFM_RESIZE == TFM_TRANSLATION + 2) &&
         /* Assert constrain values. */
         (CON_AXIS0 == (1 << 1)) && (CON_AXIS1 == (1 << 2)) && (CON_AXIS2 == (1 << 3))),
        "");

    const int trans_mode = t->mode - TFM_TRANSLATION;
    int con_mode = ((CON_AXIS0 | CON_AXIS1 | CON_AXIS2) >> 1) - 1;
    if (t->con.mode & CON_APPLY) {
      con_mode = ((t->con.mode & (CON_AXIS0 | CON_AXIS1 | CON_AXIS2)) >> 1) - 1;
    }

    axis_idx = axis_map[trans_mode][con_mode];
  }

  wmGizmo *gizmo_modal_current = WM_gizmomap_get_modal(t->region->runtime->gizmo_map);
  if (axis_idx != -1) {
    RegionView3D *rv3d = static_cast<RegionView3D *>(t->region->regiondata);
    float (*mat_cmp)[3] = t->orient[t->orient_curr != O_DEFAULT ? t->orient_curr : O_SCENE].matrix;

    bool update_orientation = !(equals_v3v3(rv3d->twmat[0], mat_cmp[0]) &&
                                equals_v3v3(rv3d->twmat[1], mat_cmp[1]) &&
                                equals_v3v3(rv3d->twmat[2], mat_cmp[2]));

    GizmoGroup *ggd = static_cast<GizmoGroup *>(gzgroup_xform->customdata);
    wmGizmo *gizmo_expected = ggd->gizmos[axis_idx];
    if (update_orientation || gizmo_modal_current != gizmo_expected) {
      if (update_orientation) {
        copy_m4_m3(rv3d->twmat, mat_cmp);
        copy_v3_v3(rv3d->twmat[3], t->center_global);
      }

      wmEvent event = {nullptr};

      /* Set the initial mouse value. Used for rotation gizmos. */
      copy_v2_v2_int(event.mval, int2(t->mouse.imval));

      /* We need to update the position of the gizmo before invoking otherwise
       * #wmGizmo::scale_final could be calculated wrong. */
      gizmo_refresh_from_matrix(gizmo_expected, axis_idx, ggd->twtype, rv3d->twmat, nullptr);

      BLI_assert_msg(!gizmo_modal_current || gizmo_modal_current->highlight_part == 0,
                     "Avoid changing the highlight part");
      gizmo_expected->highlight_part = 0;
      WM_gizmo_modal_set_while_modal(
          t->region->runtime->gizmo_map, t->context, gizmo_expected, &event);
      WM_gizmo_highlight_set(t->region->runtime->gizmo_map, gizmo_expected);
    }
  }
  else if (gizmo_modal_current) {
    WM_gizmo_modal_set_while_modal(t->region->runtime->gizmo_map, t->context, nullptr, nullptr);
  }
}

void transform_gizmo_3d_model_from_constraint_and_mode_restore(TransInfo *t)
{
  if (t->flag & T_NO_GIZMO) {
    return;
  }

  wmGizmoGroup *gzgroup_xform = gizmogroup_xform_find(t);
  if (gzgroup_xform == nullptr) {
    return;
  }

  GizmoGroup *ggd = static_cast<GizmoGroup *>(gzgroup_xform->customdata);

  /* #wmGizmoGroup::draw_prepare will handle the rest. */
  MAN_ITER_AXES_BEGIN (axis, axis_idx) {
    gizmo_3d_setup_draw_default(axis, axis_idx);
    gizmo_3d_setup_draw_from_twtype(axis, axis_idx, ggd->twtype);
  }
  MAN_ITER_AXES_END;
}

bool calc_pivot_pos(const bContext *C, const short pivot_type, float r_pivot_pos[3])
{
  Scene *scene = CTX_data_scene(C);
  return gizmo_3d_calc_pos(C, scene, nullptr, pivot_type, r_pivot_pos);
}

}  // namespace blender::ed::transform
