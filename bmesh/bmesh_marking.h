#ifndef BM_MARKING_H
#define BM_MARKING_H

/*Selection code*/
void BM_Select_Vert(struct BMesh *bm, struct BMVert *v, int select);
void BM_Select_Edge(struct BMesh *bm, struct BMEdge *e, int select);
void BM_Select_Face(struct BMesh *bm, struct BMFace *f, int select);
void BM_Selectmode_Set(struct BMesh *bm, int selectmode);

void BM_Select(struct BMesh *bm, void *element, int select);
int BM_Is_Selected(BMesh *bm, void *element);

#endif
