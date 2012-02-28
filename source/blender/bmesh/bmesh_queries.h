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
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_QUERIES_H__
#define __BMESH_QUERIES_H__

/** \file blender/bmesh/bmesh_queries.h
 *  \ingroup bmesh
 */

#include <stdio.h>

/* Queries */

/* counts number of elements of type type are in the mesh. */
int BM_mesh_elem_count(BMesh *bm, const char htype);

/*returns true if v is in f*/
int BM_vert_in_face(BMFace *f, BMVert *v);

// int BM_verts_in_face(BMFace *f, BMVert **varr, int len);
int BM_verts_in_face(BMesh *bm, BMFace *f, BMVert **varr, int len);

int BM_edge_in_face(BMFace *f, BMEdge *e);

int BM_vert_in_edge(BMEdge *e, BMVert *v);

int BM_verts_in_edge(BMVert *v1, BMVert *v2, BMEdge *e);

/*get opposing vert from v in edge e.*/
BMVert *BM_edge_other_vert(BMEdge *e, BMVert *v);

/*finds other loop that shares v with e's loop in f.*/
BMLoop *BM_face_other_loop(BMEdge *e, BMFace *f, BMVert *v);

/*returns the edge existing between v1 and v2, or NULL if there isn't one.*/
BMEdge *BM_edge_exists(BMVert *v1, BMVert *v2);


/*returns number of edges aroudn a vert*/
int BM_vert_edge_count(BMVert *v);

/*returns number of faces around an edge*/
int BM_edge_face_count(BMEdge *e);

/*returns number of faces around a vert.*/
int BM_vert_face_count(BMVert *v);


/*returns true if v is a wire vert*/
int BM_vert_is_wire(BMesh *bm, BMVert *v);

/*returns true if e is a wire edge*/
int BM_edge_is_wire(BMesh *bm, BMEdge *e);

/* returns FALSE if v is part of a non-manifold edge in the mesh,
 * I believe this includes if it's part of both a wire edge and
 * a face.*/
int BM_vert_is_manifold(BMesh *bm, BMVert *v);

/* returns FALSE if e is shared by more then two faces. */
int BM_edge_is_manifold(BMesh *bm, BMEdge *e);

/* returns true if e is a boundary edge, e.g. has only 1 face bordering it. */
int BM_edge_is_boundary(BMEdge *e);

/* returns the face corner angle */
float BM_loop_face_angle(BMesh *bm, BMLoop *l);

/* returns angle of two faces surrounding an edge.  note there must be
 * exactly two faces sharing the edge.*/
float BM_edge_face_angle(BMesh *bm, BMEdge *e);

/* returns angle of two faces surrounding edges.  note there must be
 * exactly two edges sharing the vertex.*/
float BM_vert_edge_angle(BMesh *bm, BMVert *v);

/* checks overlapping of existing faces with the verts in varr. */
int BM_face_exists_overlap(BMesh *bm, BMVert **varr, int len, BMFace **r_existface,
                           const short do_partial);

/* checks if many existing faces overlap the faces defined by varr */
int BM_face_exists_multi(BMesh *bm, BMVert **varr, BMEdge **earr, int len);
int BM_face_exists_multi_edge(BMesh *bm, BMEdge **earr, int len);

/* checks if a face defined by varr already exists. */
int BM_face_exists(BMesh *bm, BMVert **varr, int len, BMFace **r_existface);


/* returns number of edges f1 and f2 share. */
int BM_face_share_edge_count(BMFace *f1, BMFace *f2);

/* returns number of faces e1 and e2 share. */
int BM_edge_share_face_count(BMEdge *e1, BMEdge *e2);

/* returns bool 1/0 if the edges share a vertex */
int BM_edge_share_vert_count(BMEdge *e1, BMEdge *e2);

BMVert *BM_edge_share_vert(BMEdge *e1, BMEdge *e2);

/* edge verts in winding order from face */
void BM_edge_ordered_verts(BMEdge *edge, BMVert **r_v1, BMVert **r_v2);

/* checks if a face is valid in the data structure */
int BM_face_validate(BMesh *bm, BMFace *face, FILE *err);

/* each pair of loops defines a new edge, a split.  this function goes
 * through and sets pairs that are geometrically invalid to null.  a
 * split is invalid, if it forms a concave angle or it intersects other
 * edges in the face.*/
void BM_face_legal_splits(BMesh *bm, BMFace *f, BMLoop *(*loops)[2], int len);

#endif /* __BMESH_QUERIES_H__ */
