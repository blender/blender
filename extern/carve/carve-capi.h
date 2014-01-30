/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __CARVE_CAPI_H__
#define __CARVE_CAPI_H__

#ifdef __cplusplus
extern "C" {
#endif

struct CarveMeshDescr;

//
// Importer from external storage to Carve module
//

struct ImportMeshData;

// Get number of vertices.
typedef int (*CarveImporter_GetNumVerts) (struct ImportMeshData *import_data);

// Get number of edges.
typedef int (*CarveImporter_GetNumEdges) (struct ImportMeshData *import_data);

// Get number of loops.
typedef int (*CarveImporter_GetNumLoops) (struct ImportMeshData *import_data);

// Get number of polys.
typedef int (*CarveImporter_GetNumPolys) (struct ImportMeshData *import_data);

// Get 3D coordinate of vertex with given index.
typedef void (*CarveImporter_GetVertCoord) (struct ImportMeshData *import_data, int vert_index, float coord[3]);

// Get index of vertices which are adjucent to edge specified by it's index.
typedef void (*CarveImporter_GetEdgeVerts) (struct ImportMeshData *import_data, int edge_index, int *v1, int *v2);

// Get number of adjucent vertices to the poly specified by it's index.
typedef int (*CarveImporter_GetPolyNumVerts) (struct ImportMeshData *import_data, int poly_index);

// Get list of adjucent vertices to the poly specified by it's index.
typedef void (*CarveImporter_GetPolyVerts) (struct ImportMeshData *import_data, int poly_index, int *verts);

// Triangulate 2D polygon.
typedef int (*CarveImporter_Triangulate2DPoly) (struct ImportMeshData *import_data,
                                                const float (*vertices)[2], int num_vertices,
                                                unsigned int (*triangles)[3]);

typedef struct CarveMeshImporter {
	CarveImporter_GetNumVerts getNumVerts;
	CarveImporter_GetNumEdges getNumEdges;
	CarveImporter_GetNumLoops getNumLoops;
	CarveImporter_GetNumPolys getNumPolys;
	CarveImporter_GetVertCoord getVertCoord;
	CarveImporter_GetEdgeVerts getEdgeVerts;
	CarveImporter_GetPolyNumVerts getNumPolyVerts;
	CarveImporter_GetPolyVerts getPolyVerts;
	CarveImporter_Triangulate2DPoly triangulate2DPoly;
} CarveMeshImporter;

//
// Exporter from Carve module to external storage
//

struct ExportMeshData;

// Initialize arrays for geometry.
typedef void (*CarveExporter_InitGeomArrays) (struct ExportMeshData *export_data,
                                              int num_verts, int num_edges,
                                              int num_polys, int num_loops);

// Set coordinate of vertex with given index.
typedef void (*CarveExporter_SetVert) (struct ExportMeshData *export_data,
                                       int vert_index, float coord[3],
                                       int which_orig_mesh, int orig_edge_index);

// Set vertices which are adjucent to the edge specified by it's index.
typedef void (*CarveExporter_SetEdge) (struct ExportMeshData *export_data,
                                       int edge_index, int v1, int v2,
                                       int which_orig_mesh, int orig_edge_index);

// Set adjucent loops to the poly specified by it's index.
typedef void (*CarveExporter_SetPoly) (struct ExportMeshData *export_data,
                                       int poly_index, int start_loop, int num_loops,
                                       int which_orig_mesh, int orig_poly_index);

// Set list vertex and edge which are adjucent to loop with given index.
typedef void (*CarveExporter_SetLoop) (struct ExportMeshData *export_data,
                                       int loop_index, int vertex, int edge,
                                       int which_orig_mesh, int orig_loop_index);

// Get edge index from a loop index for a given original mesh.
//
// A bit of a bummer to access original operands data on export stage,
// but Blender side still does have this information in derived meshes
// and we use API to get this data instead of duplicating it in Carve
// API side. This is because of optimizations reasons.
typedef int (*CarveExporter_MapLoopToEdge) (struct ExportMeshData *export_data,
                                            int which_mesh, int loop_index);

typedef struct CarveMeshExporter {
	CarveExporter_InitGeomArrays initGeomArrays;
	CarveExporter_SetVert setVert;
	CarveExporter_SetEdge setEdge;
	CarveExporter_SetPoly setPoly;
	CarveExporter_SetLoop setLoop;
	CarveExporter_MapLoopToEdge mapLoopToEdge;
} CarveMeshExporter;

enum {
	CARVE_OP_UNION,
	CARVE_OP_INTERSECTION,
	CARVE_OP_A_MINUS_B,
};

enum {
	CARVE_MESH_NONE,
	CARVE_MESH_LEFT,
	CARVE_MESH_RIGHT
};

struct CarveMeshDescr *carve_addMesh(struct ImportMeshData *import_data,
                                     CarveMeshImporter *mesh_importer);

void carve_deleteMesh(struct CarveMeshDescr *mesh_descr);

bool carve_performBooleanOperation(struct CarveMeshDescr *left_mesh,
                                   struct CarveMeshDescr *right_mesh,
                                   int operation,
                                   struct CarveMeshDescr **output_mesh);

void carve_exportMesh(struct CarveMeshDescr *mesh_descr,
                      CarveMeshExporter *mesh_exporter,
                      struct ExportMeshData *export_data);

void carve_unionIntersections(struct CarveMeshDescr **left_mesh_r, struct CarveMeshDescr **right_mesh_r);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // __CARVE_CAPI_H__
