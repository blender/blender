/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2016 Blender Foundation */

/** \file
 * \ingroup edgizmolib
 *
 * \name Gizmo Geometry
 *
 * \brief Prototypes for arrays defining the gizmo geometry.
 * The actual definitions can be found in files usually
 * called geom_xxx_gizmo.c
 */

#pragma once

#include "BLI_sys_types.h"

typedef struct GizmoGeomInfo {
  int nverts;
  int ntris;
  const float (*verts)[3];
  const float (*normals)[3];
  const ushort *indices;
} GizmoGeomInfo;

/* arrow gizmo */
extern GizmoGeomInfo wm_gizmo_geom_data_arrow;

/* cube gizmo */
extern GizmoGeomInfo wm_gizmo_geom_data_cube;

/* dial gizmo */
extern GizmoGeomInfo wm_gizmo_geom_data_dial;
