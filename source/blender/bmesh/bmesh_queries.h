#ifndef BMESH_QUERIES_H
#define BMESH_QUERIES_H
#include <stdio.h>

/*Queries*/
/*counts number of elements of type type are in the mesh.*/
int BM_Count_Element(struct BMesh *bm, int type);

/*returns true if v is in e.*/
int BM_Vert_In_Edge(struct BMEdge *e, struct BMVert *v);

/*returns true if v is in f*/
int BM_Vert_In_Face(struct BMFace *f, struct BMVert *v);

// int BM_VERTS_OF_MESH_In_Face(struct BMFace *f, struct BMVert **varr, int len);
int BM_VERTS_OF_MESH_In_Face(struct BMesh *bm, struct BMFace *f, struct BMVert **varr, int len);

int BM_Edge_In_Face(struct BMFace *f, struct BMEdge *e);

int BM_VERTS_OF_MESH_In_Edge(struct BMVert *v1, struct BMVert *v2, BMEdge *e);


/*get opposing vert from v in edge e.*/
struct BMVert *BM_OtherEdgeVert(struct BMEdge *e, struct BMVert *v);

/*finds other loop that shares v with e's loop in f.*/
struct BMLoop *BM_OtherFaceLoop(BMEdge *e, BMFace *f, BMVert *v);

//#define BM_OtherEdgeVert(e, v) (v==e->v1?e->v2:e->v1)

/*returns the edge existing between v1 and v2, or NULL if there isn't one.*/
struct BMEdge *BM_Edge_Exist(struct BMVert *v1, struct BMVert *v2);


/*returns number of edges aroudn a vert*/
int BM_Vert_EdgeCount(struct BMVert *v);

/*returns number of faces around an edge*/
int BM_Edge_FaceCount(struct BMEdge *e);

/*returns number of faces around a vert.*/
int BM_Vert_FaceCount(struct BMVert *v);


/*returns true if v is a wire vert*/
int BM_Wire_Vert(struct BMesh *bm, struct BMVert *v);

/*returns true if e is a wire edge*/
int BM_Wire_Edge(struct BMesh *bm, struct BMEdge *e);

/*returns true if v is part of a non-manifold edge in the mesh,
  I believe this includes if it's part of both a wire edge and
  a face.*/
int BM_Nonmanifold_Vert(struct BMesh *bm, struct BMVert *v);

/*returns true if e is shared by more then two faces.*/
int BM_Nonmanifold_Edge(struct BMesh *bm, struct BMEdge *e);

/*returns true if e is a boundary edge, e.g. has only 1 face bordering it.*/
int BM_Boundary_Edge(struct BMEdge *e);


/*returns angle of two faces surrounding an edge.  note there must be
  exactly two faces sharing the edge.*/
float BM_Face_Angle(struct BMesh *bm, struct BMEdge *e);

/*checks overlapping of existing faces with the verts in varr.*/
int BM_Exist_Face_Overlaps(struct BMesh *bm, struct BMVert **varr, int len, struct BMFace **existface);

/*checks if a face defined by varr already exists.*/
int BM_Face_Exists(BMesh *bm, BMVert **varr, int len, BMFace **existface);


/*returns number of edges f1 and f2 share.*/
int BM_Face_Sharededges(struct BMFace *f1, struct BMFace *f2);

/*returns number of faces e1 and e2 share.*/
int BM_Edge_Share_Faces(struct BMEdge *e1, struct BMEdge *e2);

/*checks if a face is valid in the data structure*/
int BM_Validate_Face(BMesh *bm, BMFace *face, FILE *err);

/*each pair of loops defines a new edge, a split.  this function goes
  through and sets pairs that are geometrically invalid to null.  a
  split is invalid, if it forms a concave angle or it intersects other
  edges in the face.*/
void BM_LegalSplits(BMesh *bm, BMFace *f, BMLoop *(*loops)[2], int len);

#endif
