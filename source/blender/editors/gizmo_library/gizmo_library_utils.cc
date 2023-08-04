/* SPDX-FileCopyrightText: 2015 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgizmolib
 *
 * \name Gizmo Library Utilities
 *
 * \brief This file contains functions for common behaviors of gizmos.
 */

#include "BLI_math.h"

#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "RNA_access.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_screen.hh"
#include "ED_view3d.h"

#include "CLG_log.h"

/* own includes */
#include "gizmo_library_intern.h"

static CLG_LogRef LOG = {"ed.gizmo.library_utils"};

/* factor for precision tweaking */
#define GIZMO_PRECISION_FAC 0.05f

BLI_INLINE float gizmo_offset_from_value_constr(const float range_fac,
                                                const float min,
                                                const float range,
                                                const float value,
                                                const bool inverted)
{
  return inverted ? (range_fac * (min + range - value) / range) : (range_fac * (value / range));
}

BLI_INLINE float gizmo_value_from_offset_constr(const float range_fac,
                                                const float min,
                                                const float range,
                                                const float value,
                                                const bool inverted)
{
  return inverted ? (min + range - (value * range / range_fac)) : (value * range / range_fac);
}

float gizmo_offset_from_value(GizmoCommonData *data,
                              const float value,
                              const bool constrained,
                              const bool inverted)
{
  if (constrained) {
    return gizmo_offset_from_value_constr(
        data->range_fac, data->min, data->range, value, inverted);
  }

  return value;
}

float gizmo_value_from_offset(GizmoCommonData *data,
                              GizmoInteraction *inter,
                              const float offset,
                              const bool constrained,
                              const bool inverted,
                              const bool use_precision)
{
  const float max = data->min + data->range;

  if (use_precision) {
    /* add delta offset of this step to total precision_offset */
    inter->precision_offset += offset - inter->prev_offset;
  }
  inter->prev_offset = offset;

  float ofs_new = inter->init_offset + offset -
                  inter->precision_offset * (1.0f - GIZMO_PRECISION_FAC);
  float value;

  if (constrained) {
    value = gizmo_value_from_offset_constr(
        data->range_fac, data->min, data->range, ofs_new, inverted);
  }
  else {
    value = ofs_new;
  }

  /* clamp to custom range */
  if (data->is_custom_range_set) {
    CLAMP(value, data->min, max);
  }

  return value;
}

void gizmo_property_data_update(wmGizmo *gz,
                                GizmoCommonData *data,
                                wmGizmoProperty *gz_prop,
                                const bool constrained,
                                const bool inverted)
{
  if (gz_prop->custom_func.value_get_fn != nullptr) {
    /* Pass. */
  }
  else if (gz_prop->prop != nullptr) {
    /* Pass. */
  }
  else {
    data->offset = 0.0f;
    return;
  }

  float value = WM_gizmo_target_property_float_get(gz, gz_prop);

  if (constrained) {
    if (data->is_custom_range_set == false) {
      float range[2];
      if (WM_gizmo_target_property_float_range_get(gz, gz_prop, range)) {
        data->range = range[1] - range[0];
        data->min = range[0];
      }
      else {
        BLI_assert(0);
      }
    }
    data->offset = gizmo_offset_from_value_constr(
        data->range_fac, data->min, data->range, value, inverted);
  }
  else {
    data->offset = value;
  }
}

void gizmo_property_value_reset(bContext *C,
                                const wmGizmo *gz,
                                GizmoInteraction *inter,
                                wmGizmoProperty *gz_prop)
{
  WM_gizmo_target_property_float_set(C, gz, gz_prop, inter->init_value);
}

/* -------------------------------------------------------------------- */

void gizmo_color_get(const wmGizmo *gz, const bool highlight, float r_col[4])
{
  if (highlight && !(gz->flag & WM_GIZMO_DRAW_HOVER)) {
    copy_v4_v4(r_col, gz->color_hi);
  }
  else {
    copy_v4_v4(r_col, gz->color);
  }
}

/* -------------------------------------------------------------------- */

bool gizmo_window_project_2d(
    bContext *C, const wmGizmo *gz, const float mval[2], int axis, bool use_offset, float r_co[2])
{
  float mat[4][4], imat[4][4];
  {
    float mat_identity[4][4];
    WM_GizmoMatrixParams params = {nullptr};
    if (use_offset == false) {
      unit_m4(mat_identity);
      params.matrix_offset = mat_identity;
    }
    WM_gizmo_calc_matrix_final_params(gz, &params, mat);
  }

  if (!invert_m4_m4(imat, mat)) {
    CLOG_WARN(&LOG,
              "Gizmo \"%s\" of group \"%s\" has matrix that could not be inverted "
              "(projection will fail)",
              gz->type->idname,
              gz->parent_gzgroup->type->idname);
  }

  /* rotate mouse in relation to the center and relocate it */
  if (gz->parent_gzgroup->type->flag & WM_GIZMOGROUPTYPE_3D) {
    /* For 3d views, transform 2D mouse pos onto plane. */
    ARegion *region = CTX_wm_region(C);

    float plane[4], co[3];
    plane_from_point_normal_v3(plane, mat[3], mat[2]);
    bool clip_ray = ((RegionView3D *)region->regiondata)->is_persp;
    if (ED_view3d_win_to_3d_on_plane(region, plane, mval, clip_ray, co)) {
      mul_m4_v3(imat, co);
      r_co[0] = co[(axis + 1) % 3];
      r_co[1] = co[(axis + 2) % 3];
      return true;
    }
    return false;
  }

  float co[3] = {mval[0], mval[1], 0.0f};
  mul_m4_v3(imat, co);
  copy_v2_v2(r_co, co);
  return true;
}

bool gizmo_window_project_3d(
    bContext *C, const wmGizmo *gz, const float mval[2], bool use_offset, float r_co[3])
{
  float mat[4][4], imat[4][4];
  {
    float mat_identity[4][4];
    WM_GizmoMatrixParams params = {nullptr};
    if (use_offset == false) {
      unit_m4(mat_identity);
      params.matrix_offset = mat_identity;
    }
    WM_gizmo_calc_matrix_final_params(gz, &params, mat);
  }

  if (!invert_m4_m4(imat, mat)) {
    CLOG_WARN(&LOG,
              "Gizmo \"%s\" of group \"%s\" has matrix that could not be inverted "
              "(projection will fail)",
              gz->type->idname,
              gz->parent_gzgroup->type->idname);
  }

  if (gz->parent_gzgroup->type->flag & WM_GIZMOGROUPTYPE_3D) {
    View3D *v3d = CTX_wm_view3d(C);
    ARegion *region = CTX_wm_region(C);
    /* NOTE: we might want a custom reference point passed in,
     * instead of the gizmo center. */
    ED_view3d_win_to_3d(v3d, region, mat[3], mval, r_co);
    mul_m4_v3(imat, r_co);
    return true;
  }

  float co[3] = {mval[0], mval[1], 0.0f};
  mul_m4_v3(imat, co);
  copy_v2_v2(r_co, co);
  return true;
}

/* -------------------------------------------------------------------- */
/** \name RNA Utils
 * \{ */

/* Based on 'rna_GizmoProperties_find_operator'. */
wmGizmo *gizmo_find_from_properties(const IDProperty *properties,
                                    const int spacetype,
                                    const int regionid)
{
  for (bScreen *screen = static_cast<bScreen *>(G_MAIN->screens.first); screen;
       screen = static_cast<bScreen *>(screen->id.next))
  {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (!ELEM(spacetype, SPACE_TYPE_ANY, area->spacetype)) {
        continue;
      }
      LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
        if (region->gizmo_map == nullptr) {
          continue;
        }
        if (!ELEM(regionid, RGN_TYPE_ANY, region->regiontype)) {
          continue;
        }

        LISTBASE_FOREACH (wmGizmoGroup *, gzgroup, WM_gizmomap_group_list(region->gizmo_map)) {
          LISTBASE_FOREACH (wmGizmo *, gz, &gzgroup->gizmos) {
            if (gz->properties == properties) {
              return gz;
            }
          }
        }
      }
    }
  }
  return nullptr;
}

/** \} */
