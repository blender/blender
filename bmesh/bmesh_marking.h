#ifndef BMESH_MARKING_H
#define BMESH_MARKING_H

/*Selection code*/
void BM_Select_Vert(struct BMesh *bm, struct BMVert *v, int select);
void BM_Select_Edge(struct BMesh *bm, struct BMEdge *e, int select);
void BM_Select_Face(struct BMesh *bm, struct BMFace *f, int select);
void BM_Selectmode_Set(struct BMesh *bm, int selectmode);

void BM_Select(struct BMesh *bm, void *element, int select);


/* NOTE: unused, bad: 

  simple system to manipulate flags, coded here
  to avoid having to spend excess time refactoring
  customdata.*/
enum {
	BM_SELECT,
	BM_SMOOTH,
} BM_CommonMarks;

enum {
	BM_VNUMMARKS
} BM_VertMarks;

enum {
	BM_FGON,
	BM_SHARP,
	BM_SEAM,
	BM_ENUMMARKS
} BM_EdgeMarks;

enum {
	BM_MATERIAL,
	BM_FNUMMARKS
} BM_FaceMarks;

/*returns if the specifid flag is equal in both elements
  (which much be BMHeader-derived structs of the same type).*/
int BM_FlagEqual(void *element1, void *element2, int type);

#endif
