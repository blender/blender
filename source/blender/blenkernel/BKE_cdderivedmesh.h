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
 * along with this program; if not, write to the Free Software  Foundation,
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
 */

#ifndef __BKE_CDDERIVEDMESH_H__
#define __BKE_CDDERIVEDMESH_H__

#include "BKE_DerivedMesh.h"
#include "BKE_customdata.h"

struct BMEditMesh;
struct CustomData_MeshMasks;
struct DerivedMesh;
struct Mesh;

/* creates a new CDDerivedMesh */
struct DerivedMesh *CDDM_new(int numVerts, int numEdges, int numFaces, int numLoops, int numPolys);

/* creates a CDDerivedMesh from the given Mesh, this will reference the
 * original data in Mesh, but it is safe to apply vertex coordinates or
 * calculate normals as those functions will automatically create new
 * data to not overwrite the original */
struct DerivedMesh *CDDM_from_mesh(struct Mesh *mesh);

/* creates a CDDerivedMesh from the given Mesh with custom allocation type. */
struct DerivedMesh *CDDM_from_mesh_ex(struct Mesh *mesh,
                                      eCDAllocType alloctype,
                                      const struct CustomData_MeshMasks *mask);

/* creates a CDDerivedMesh from the given BMEditMesh */
DerivedMesh *CDDM_from_editbmesh(struct BMEditMesh *em, const bool use_mdisps);

/* Copies the given DerivedMesh with verts, faces & edges stored as
 * custom element data.
 */
struct DerivedMesh *CDDM_copy(struct DerivedMesh *dm);

void CDDM_recalc_looptri(struct DerivedMesh *dm);

/* vertex/edge/face access functions
 * should always succeed if index is within bounds
 * note these return pointers - any change modifies the internals of the mesh
 */
struct MVert *CDDM_get_vert(struct DerivedMesh *dm, int index);
struct MEdge *CDDM_get_edge(struct DerivedMesh *dm, int index);
struct MFace *CDDM_get_tessface(struct DerivedMesh *dm, int index);
struct MLoop *CDDM_get_loop(struct DerivedMesh *dm, int index);
struct MPoly *CDDM_get_poly(struct DerivedMesh *dm, int index);

/* vertex/edge/face array access functions - return the array holding the
 * desired data
 * should always succeed
 * note these return pointers - any change modifies the internals of the mesh
 */
struct MVert *CDDM_get_verts(struct DerivedMesh *dm);
struct MEdge *CDDM_get_edges(struct DerivedMesh *dm);
struct MFace *CDDM_get_tessfaces(struct DerivedMesh *dm);
struct MLoop *CDDM_get_loops(struct DerivedMesh *dm);
struct MPoly *CDDM_get_polys(struct DerivedMesh *dm);

#endif
