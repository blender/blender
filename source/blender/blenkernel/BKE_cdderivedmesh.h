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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 * \section aboutcdderivedmesh CDDerivedMesh interface
 *   CDDerivedMesh (CD = Custom Data) is a DerivedMesh backend which stores
 *   mesh elements (vertices, edges and faces) as layers of custom element data.
 *
 * \note This is deprecated & should eventually be removed.
 */

#ifndef __BKE_CDDERIVEDMESH_H__
#define __BKE_CDDERIVEDMESH_H__

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

/* Copies the given DerivedMesh with verts, faces & edges stored as
 * custom element data. */
struct DerivedMesh *CDDM_copy(struct DerivedMesh *dm);

#ifdef __cplusplus
}
#endif

#endif
