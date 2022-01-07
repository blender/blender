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
 * Mesh Fairing algorithm designed by Brett Fedack, used in the addon "Mesh Fairing":
 * https://github.com/fedackb/mesh-fairing.
 */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Mesh Fairing. */
/* Creates a smooth as possible geometry patch in a defined area. Different values of depth allow
 * to minimize changes in the vertex positions or tangency in the affected area. */

typedef enum eMeshFairingDepth {
  MESH_FAIRING_DEPTH_POSITION = 1,
  MESH_FAIRING_DEPTH_TANGENCY = 2,
} eMeshFairingDepth;

/* affect_vertices is used to define the fairing area. Indexed by vertex index, set to true when
 * the vertex should be modified by fairing. */
void BKE_bmesh_prefair_and_fair_vertices(struct BMesh *bm,
                                         bool *affect_vertices,
                                         eMeshFairingDepth depth);

/* This function can optionally use the MVert coordinates of deform_mverts to read and write the
 * fairing result. When NULL, the function will use mesh->mverts directly. */
void BKE_mesh_prefair_and_fair_vertices(struct Mesh *mesh,
                                        struct MVert *deform_mverts,
                                        bool *affect_vertices,
                                        eMeshFairingDepth depth);

#ifdef __cplusplus
}
#endif
