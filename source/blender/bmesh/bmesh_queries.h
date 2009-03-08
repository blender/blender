#ifndef BMESH_QUERIES_H
#define BMESH_QUERIES_H
#include <stdio.h>

/*Queries*/
int BM_Count_Element(struct BMesh *bm, int type);
int BM_Vert_In_Edge(struct BMEdge *e, struct BMVert *v);
int BM_Vert_In_Face(struct BMFace *f, struct BMVert *v);
// int BM_Verts_In_Face(struct BMFace *f, struct BMVert **varr, int len);
int BM_Verts_In_Face(struct BMesh *bm, struct BMFace *f, struct BMVert **varr, int len);
int BM_Edge_In_Face(struct BMFace *f, struct BMEdge *e);
int BM_Verts_In_Edge(struct BMVert *v1, struct BMVert *v2, BMEdge *e);

struct BMVert *BM_OtherEdgeVert(struct BMEdge *e, struct BMVert *v);

/*finds other loop that shares v with e's loop in f.*/
struct BMLoop *BM_OtherFaceLoop(BMEdge *e, BMFace *f, BMVert *v);

//#define BM_OtherEdgeVert(e, v) (v==e->v1?e->v2:e->v1)

struct BMEdge *BM_Edge_Exist(struct BMVert *v1, struct BMVert *v2);
int BM_Vert_EdgeCount(struct BMVert *v);
int BM_Edge_FaceCount(struct BMEdge *e);
int BM_Vert_FaceCount(struct BMVert *v);
int BM_Wire_Vert(struct BMesh *bm, struct BMVert *v);
int BM_Wire_Edge(struct BMesh *bm, struct BMEdge *e);
int BM_Nonmanifold_Vert(struct BMesh *bm, struct BMVert *v);
int BM_Nonmanifold_Edge(struct BMesh *bm, struct BMEdge *e);
int BM_Boundary_Edge(struct BMEdge *e);
int BM_Face_Sharededges(struct BMFace *f1, struct BMFace *f2);
float BM_Face_Angle(struct BMesh *bm, struct BMEdge *e);
int BM_Exist_Face_Overlaps(struct BMesh *bm, struct BMVert **varr, int len, struct BMFace **existface);
int BM_Edge_Share_Faces(struct BMEdge *e1, struct BMEdge *e2);
int BM_Validate_Face(BMesh *bm, BMFace *face, FILE *err);
#endif
