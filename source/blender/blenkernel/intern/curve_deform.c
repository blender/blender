/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Deform coordinates by a curve object (used by modifier).
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_curve_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_anim_path.h"
#include "BKE_curve.h"
#include "BKE_editmesh.h"
#include "BKE_lattice.h"
#include "BKE_modifier.h"

#include "BKE_deform.h"

/* -------------------------------------------------------------------- */
/** \name Curve Deform Internal Utilities
 * \{ */

/**
 * Calculations is in local space of deformed object
 * so we store matrices to transform points to/from local-space.
 */
typedef struct {
  float dmin[3], dmax[3];
  float curvespace[4][4], objectspace[4][4], objectspace3[3][3];
  int no_rot_axis;
} CurveDeform;

static void init_curve_deform(const Object *ob_curve, const Object *ob_target, CurveDeform *cd)
{
  float imat[4][4];
  invert_m4_m4(imat, ob_target->object_to_world);
  mul_m4_m4m4(cd->objectspace, imat, ob_curve->object_to_world);
  invert_m4_m4(cd->curvespace, cd->objectspace);
  copy_m3_m4(cd->objectspace3, cd->objectspace);
  cd->no_rot_axis = 0;
}

/**
 * For each point, rotate & translate to curve.
 *
 * \param co: local coord, result local too.
 * \param r_quat: returns quaternion for rotation,
 * using #CurveDeform.no_rot_axis axis is using another define.
 */
static bool calc_curve_deform(
    const Object *ob_curve, float co[3], const short axis, const CurveDeform *cd, float r_quat[4])
{
  const Curve *cu = ob_curve->data;
  float fac, loc[4], dir[3], new_quat[4], radius;
  short index;
  const bool is_neg_axis = (axis > 2);

  if (ob_curve->runtime.curve_cache == NULL) {
    /* Happens with a cyclic dependencies. */
    return false;
  }

  if (ob_curve->runtime.curve_cache->anim_path_accum_length == NULL) {
    return false; /* happens on append, cyclic dependencies and empty curves */
  }

  /* options */
  if (is_neg_axis) {
    index = axis - 3;
    if (cu->flag & CU_STRETCH) {
      const float divisor = cd->dmax[index] - cd->dmin[index];
      if (LIKELY(divisor > FLT_EPSILON)) {
        fac = -(co[index] - cd->dmax[index]) / divisor;
      }
      else {
        fac = 0.0f;
      }
    }
    else {
      const CurveCache *cc = ob_curve->runtime.curve_cache;
      float totdist = BKE_anim_path_get_length(cc);
      if (LIKELY(totdist > FLT_EPSILON)) {
        fac = -(co[index] - cd->dmax[index]) / totdist;
      }
      else {
        fac = 0.0f;
      }
    }
  }
  else {
    index = axis;
    if (cu->flag & CU_STRETCH) {
      const float divisor = cd->dmax[index] - cd->dmin[index];
      if (LIKELY(divisor > FLT_EPSILON)) {
        fac = (co[index] - cd->dmin[index]) / divisor;
      }
      else {
        fac = 0.0f;
      }
    }
    else {
      const CurveCache *cc = ob_curve->runtime.curve_cache;
      float totdist = BKE_anim_path_get_length(cc);
      if (LIKELY(totdist > FLT_EPSILON)) {
        fac = +(co[index] - cd->dmin[index]) / totdist;
      }
      else {
        fac = 0.0f;
      }
    }
  }

  if (BKE_where_on_path(ob_curve, fac, loc, dir, new_quat, &radius, NULL)) { /* returns OK */
    float quat[4], cent[3];

    if (cd->no_rot_axis) { /* set by caller */

      /* This is not exactly the same as 2.4x, since the axis is having rotation removed rather
       * than changing the axis before calculating the tilt but serves much the same purpose. */
      float dir_flat[3] = {0, 0, 0}, q[4];
      copy_v3_v3(dir_flat, dir);
      dir_flat[cd->no_rot_axis - 1] = 0.0f;

      normalize_v3(dir);
      normalize_v3(dir_flat);

      rotation_between_vecs_to_quat(q, dir, dir_flat); /* Could this be done faster? */

      mul_qt_qtqt(new_quat, q, new_quat);
    }

    /* Logic for 'cent' orientation *
     *
     * The way 'co' is copied to 'cent' may seem to have no meaning, but it does.
     *
     * Use a curve modifier to stretch a cube out, color each side RGB,
     * positive side light, negative dark.
     * view with X up (default), from the angle that you can see 3 faces RGB colors (light),
     * anti-clockwise
     * Notice X,Y,Z Up all have light colors and each ordered CCW.
     *
     * Now for Neg Up XYZ, the colors are all dark, and ordered clockwise - Campbell
     *
     * NOTE: moved functions into quat_apply_track/vec_apply_track
     */
    copy_qt_qt(quat, new_quat);
    copy_v3_v3(cent, co);

    /* Zero the axis which is not used,
     * the big block of text above now applies to these 3 lines.
     * The `upflag` argument may be a dummy, set so no rotation is done. */
    quat_apply_track(quat, axis, ELEM(axis, 0, 2) ? 1 : 0);
    vec_apply_track(cent, axis);
    cent[index] = 0.0f;

    /* scale if enabled */
    if (cu->flag & CU_PATH_RADIUS) {
      mul_v3_fl(cent, radius);
    }

    /* local rotation */
    normalize_qt(quat);
    mul_qt_v3(quat, cent);

    /* translation */
    add_v3_v3v3(co, cent, loc);

    if (r_quat) {
      copy_qt_qt(r_quat, quat);
    }

    return true;
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curve Deform #BKE_curve_deform_coords API
 *
 * #BKE_curve_deform and related functions.
 * \{ */

static void curve_deform_coords_impl(const Object *ob_curve,
                                     const Object *ob_target,
                                     float (*vert_coords)[3],
                                     const int vert_coords_len,
                                     const MDeformVert *dvert,
                                     const int defgrp_index,
                                     const short flag,
                                     const short defaxis,
                                     BMEditMesh *em_target)
{
  Curve *cu;
  int a;
  CurveDeform cd;
  const bool is_neg_axis = (defaxis > 2);
  const bool invert_vgroup = (flag & MOD_CURVE_INVERT_VGROUP) != 0;
  bool use_dverts = false;
  int cd_dvert_offset;

  if (ob_curve->type != OB_CURVES_LEGACY) {
    return;
  }

  cu = ob_curve->data;

  init_curve_deform(ob_curve, ob_target, &cd);

  if (cu->flag & CU_DEFORM_BOUNDS_OFF) {
    /* Dummy bounds. */
    if (is_neg_axis == false) {
      cd.dmin[0] = cd.dmin[1] = cd.dmin[2] = 0.0f;
      cd.dmax[0] = cd.dmax[1] = cd.dmax[2] = 1.0f;
    }
    else {
      /* Negative, these bounds give a good rest position. */
      cd.dmin[0] = cd.dmin[1] = cd.dmin[2] = -1.0f;
      cd.dmax[0] = cd.dmax[1] = cd.dmax[2] = 0.0f;
    }
  }
  else {
    /* Set mesh min/max bounds. */
    INIT_MINMAX(cd.dmin, cd.dmax);
  }

  if (em_target != NULL) {
    cd_dvert_offset = CustomData_get_offset(&em_target->bm->vdata, CD_MDEFORMVERT);
    if (cd_dvert_offset != -1) {
      use_dverts = true;
    }
  }
  else {
    if (dvert != NULL) {
      use_dverts = true;
    }
  }

  if (use_dverts) {
    if (cu->flag & CU_DEFORM_BOUNDS_OFF) {

#define DEFORM_OP(dvert) \
  { \
    const float weight = invert_vgroup ? 1.0f - BKE_defvert_find_weight(dvert, defgrp_index) : \
                                         BKE_defvert_find_weight(dvert, defgrp_index); \
    if (weight > 0.0f) { \
      float vec[3]; \
      mul_m4_v3(cd.curvespace, vert_coords[a]); \
      copy_v3_v3(vec, vert_coords[a]); \
      calc_curve_deform(ob_curve, vec, defaxis, &cd, NULL); \
      interp_v3_v3v3(vert_coords[a], vert_coords[a], vec, weight); \
      mul_m4_v3(cd.objectspace, vert_coords[a]); \
    } \
  } \
  ((void)0)

      if (em_target != NULL) {
        BMIter iter;
        BMVert *v;
        BM_ITER_MESH_INDEX (v, &iter, em_target->bm, BM_VERTS_OF_MESH, a) {
          dvert = BM_ELEM_CD_GET_VOID_P(v, cd_dvert_offset);
          DEFORM_OP(dvert);
        }
      }
      else {
        for (a = 0; a < vert_coords_len; a++) {
          DEFORM_OP(&dvert[a]);
        }
      }

#undef DEFORM_OP
    }
    else {

#define DEFORM_OP_MINMAX(dvert) \
  { \
    const float weight = invert_vgroup ? 1.0f - BKE_defvert_find_weight(dvert, defgrp_index) : \
                                         BKE_defvert_find_weight(dvert, defgrp_index); \
    if (weight > 0.0f) { \
      mul_m4_v3(cd.curvespace, vert_coords[a]); \
      minmax_v3v3_v3(cd.dmin, cd.dmax, vert_coords[a]); \
    } \
  } \
  ((void)0)

      /* Already in 'cd.curvespace', previous for loop. */
#define DEFORM_OP_CLAMPED(dvert) \
  { \
    const float weight = invert_vgroup ? 1.0f - BKE_defvert_find_weight(dvert, defgrp_index) : \
                                         BKE_defvert_find_weight(dvert, defgrp_index); \
    if (weight > 0.0f) { \
      float vec[3]; \
      copy_v3_v3(vec, vert_coords[a]); \
      calc_curve_deform(ob_curve, vec, defaxis, &cd, NULL); \
      interp_v3_v3v3(vert_coords[a], vert_coords[a], vec, weight); \
      mul_m4_v3(cd.objectspace, vert_coords[a]); \
    } \
  } \
  ((void)0)

      if (em_target != NULL) {
        BMIter iter;
        BMVert *v;
        BM_ITER_MESH_INDEX (v, &iter, em_target->bm, BM_VERTS_OF_MESH, a) {
          dvert = BM_ELEM_CD_GET_VOID_P(v, cd_dvert_offset);
          DEFORM_OP_MINMAX(dvert);
        }

        BM_ITER_MESH_INDEX (v, &iter, em_target->bm, BM_VERTS_OF_MESH, a) {
          dvert = BM_ELEM_CD_GET_VOID_P(v, cd_dvert_offset);
          DEFORM_OP_CLAMPED(dvert);
        }
      }
      else {

        for (a = 0; a < vert_coords_len; a++) {
          DEFORM_OP_MINMAX(&dvert[a]);
        }

        for (a = 0; a < vert_coords_len; a++) {
          DEFORM_OP_CLAMPED(&dvert[a]);
        }
      }
    }

#undef DEFORM_OP_MINMAX
#undef DEFORM_OP_CLAMPED
  }
  else {
    if (cu->flag & CU_DEFORM_BOUNDS_OFF) {
      for (a = 0; a < vert_coords_len; a++) {
        mul_m4_v3(cd.curvespace, vert_coords[a]);
        calc_curve_deform(ob_curve, vert_coords[a], defaxis, &cd, NULL);
        mul_m4_v3(cd.objectspace, vert_coords[a]);
      }
    }
    else {
      for (a = 0; a < vert_coords_len; a++) {
        mul_m4_v3(cd.curvespace, vert_coords[a]);
        minmax_v3v3_v3(cd.dmin, cd.dmax, vert_coords[a]);
      }

      for (a = 0; a < vert_coords_len; a++) {
        /* Already in 'cd.curvespace', previous for loop. */
        calc_curve_deform(ob_curve, vert_coords[a], defaxis, &cd, NULL);
        mul_m4_v3(cd.objectspace, vert_coords[a]);
      }
    }
  }
}

void BKE_curve_deform_coords(const Object *ob_curve,
                             const Object *ob_target,
                             float (*vert_coords)[3],
                             const int vert_coords_len,
                             const MDeformVert *dvert,
                             const int defgrp_index,
                             const short flag,
                             const short defaxis)
{
  curve_deform_coords_impl(
      ob_curve, ob_target, vert_coords, vert_coords_len, dvert, defgrp_index, flag, defaxis, NULL);
}

void BKE_curve_deform_coords_with_editmesh(const Object *ob_curve,
                                           const Object *ob_target,
                                           float (*vert_coords)[3],
                                           const int vert_coords_len,
                                           const int defgrp_index,
                                           const short flag,
                                           const short defaxis,
                                           BMEditMesh *em_target)
{
  curve_deform_coords_impl(ob_curve,
                           ob_target,
                           vert_coords,
                           vert_coords_len,
                           NULL,
                           defgrp_index,
                           flag,
                           defaxis,
                           em_target);
}

void BKE_curve_deform_co(const Object *ob_curve,
                         const Object *ob_target,
                         const float orco[3],
                         float vec[3],
                         const int no_rot_axis,
                         float r_mat[3][3])
{
  CurveDeform cd;
  float quat[4];

  if (ob_curve->type != OB_CURVES_LEGACY) {
    unit_m3(r_mat);
    return;
  }

  init_curve_deform(ob_curve, ob_target, &cd);
  cd.no_rot_axis = no_rot_axis; /* option to only rotate for XY, for example */

  copy_v3_v3(cd.dmin, orco);
  copy_v3_v3(cd.dmax, orco);

  mul_m4_v3(cd.curvespace, vec);

  if (calc_curve_deform(ob_curve, vec, ob_target->trackflag, &cd, quat)) {
    float qmat[3][3];

    quat_to_mat3(qmat, quat);
    mul_m3_m3m3(r_mat, qmat, cd.objectspace3);
  }
  else {
    unit_m3(r_mat);
  }

  mul_m4_v3(cd.objectspace, vec);
}

/** \} */
