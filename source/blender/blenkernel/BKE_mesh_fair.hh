/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * Mesh Fairing algorithm designed by Brett Fedack, used in the addon "Mesh Fairing":
 * https://github.com/fedackb/mesh-fairing.
 */

#include "BLI_utildefines.h"

/* Mesh Fairing. */
/* Creates a smooth as possible geometry patch in a defined area. Different values of depth allow
 * to minimize changes in the vertex positions or tangency in the affected area. */

typedef enum eMeshFairingDepth {
  MESH_FAIRING_DEPTH_POSITION = 1,
  MESH_FAIRING_DEPTH_TANGENCY = 2,
} eMeshFairingDepth;

/**
 * Affect_vertices is used to define the fairing area. Indexed by vertex index, set to true when
 * the vertex should be modified by fairing.
 */
void BKE_bmesh_prefair_and_fair_verts(struct BMesh *bm,
                                      bool *affect_verts,
                                      eMeshFairingDepth depth);

/**
 * This function can optionally use the vertex coordinates of deform_mverts to read and write the
 * fairing result. When NULL, the function will use mesh positions directly.
 */
void BKE_mesh_prefair_and_fair_verts(struct Mesh *mesh,
                                     float (*deform_vert_positions)[3],
                                     bool *affect_verts,
                                     eMeshFairingDepth depth);
