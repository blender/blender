/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgizmolib
 */

#pragma once

#include "gizmo_geometry.h"

struct bContext;
struct wmGizmoProperty;

#define DIAL_RESOLUTION 48

/**
 * Data for common interactions. Used in `gizmo_library_utils.cc` functions.
 */
struct GizmoCommonData {
  float range_fac; /* factor for arrow min/max distance */
  float offset;

  /* property range for constrained gizmos */
  float range;
  /* min/max value for constrained gizmos */
  float min, max;

  uint is_custom_range_set : 1;
};

struct GizmoInteraction {
  float init_value; /* initial property value */
  float init_mval[2];
  float init_offset;
  float init_matrix_final[4][4];
  float init_matrix_basis[4][4];

  /* offset of last handling step */
  float prev_offset;
  /* Total offset added by precision tweaking.
   * Needed to allow toggling precision on/off without causing jumps */
  float precision_offset;
};

float gizmo_offset_from_value(GizmoCommonData *data, float value, bool constrained, bool inverted);
float gizmo_value_from_offset(GizmoCommonData *data,
                              GizmoInteraction *inter,
                              float offset,
                              bool constrained,
                              bool inverted,
                              bool use_precision);

void gizmo_property_data_update(
    wmGizmo *gz, GizmoCommonData *data, wmGizmoProperty *gz_prop, bool constrained, bool inverted);

void gizmo_property_value_reset(bContext *C,
                                const wmGizmo *gz,
                                GizmoInteraction *inter,
                                wmGizmoProperty *gz_prop);

/* -------------------------------------------------------------------- */

void gizmo_color_get(const wmGizmo *gz, bool highlight, float r_color[4]);

/**
 * Takes mouse coordinates and returns them in relation to the gizmo.
 * Both 2D & 3D supported, use so we can use 2D gizmos in the 3D view.
 */
bool gizmo_window_project_2d(
    bContext *C, const wmGizmo *gz, const float mval[2], int axis, bool use_offset, float r_co[2]);

bool gizmo_window_project_3d(
    bContext *C, const wmGizmo *gz, const float mval[2], bool use_offset, float r_co[3]);

/* -------------------------------------------------------------------- */
/* Gizmo RNA Utils. */

wmGizmo *gizmo_find_from_properties(const IDProperty *properties,
                                    const int spacetype,
                                    const int regionid);

/* -------------------------------------------------------------------- */
/* Gizmo drawing */

/**
 * Main draw call for #GizmoGeomInfo data
 */
void wm_gizmo_geometryinfo_draw(const GizmoGeomInfo *info, bool select, const float color[4]);
void wm_gizmo_vec_draw(
    const float color[4], const float (*verts)[3], uint vert_count, uint pos, uint primitive_type);
