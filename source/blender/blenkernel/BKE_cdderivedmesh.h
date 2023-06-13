/* SPDX-FileCopyrightText: 2006 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 * \section aboutcdderivedmesh CDDerivedMesh interface
 *   CDDerivedMesh (CD = Custom Data) is a DerivedMesh backend which stores
 *   mesh elements (vertices, edges and faces) as layers of custom element data.
 *
 * \note This is deprecated & should eventually be removed.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct DerivedMesh;
struct Mesh;

/* creates a CDDerivedMesh from the given Mesh, this will reference the
 * original data in Mesh, but it is safe to apply vertex coordinates or
 * calculate normals as those functions will automatically create new
 * data to not overwrite the original. */
struct DerivedMesh *CDDM_from_mesh(struct Mesh *mesh);

#ifdef __cplusplus
}
#endif
