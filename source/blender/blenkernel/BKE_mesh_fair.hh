/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * Mesh Fairing algorithm designed by Brett Fedack, used in the addon "Mesh Fairing":
 * https://github.com/fedackb/mesh-fairing.
 */

#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_utildefines.h"

struct Mesh;

/* Mesh Fairing. */
/* Creates a smooth as possible geometry patch in a defined area. Different values of depth allow
 * to minimize changes in the vertex positions or tangency in the affected area. */

enum eMeshFairingDepth {
  MESH_FAIRING_DEPTH_POSITION = 1,
  MESH_FAIRING_DEPTH_TANGENCY = 2,
};

/**
 * This function can optionally use the vertex coordinates of deform_mverts to read and write the
 * fairing result. When NULL, the function will use mesh positions directly.
 */
void BKE_mesh_prefair_and_fair_verts(Mesh *mesh,
                                     blender::MutableSpan<blender::float3> deform_vert_positions,
                                     const bool affected_verts[],
                                     eMeshFairingDepth depth);
