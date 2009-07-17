#ifndef BM_MARKING_H
#define BM_MARKING_H

typedef struct BMEditSelection
{
	struct BMEditSelection *next, *prev;
	short type;
	void *data;
} BMEditSelection;

/*Selection code*/
void BM_Select_Vert(struct BMesh *bm, struct BMVert *v, int select);
void BM_Select_Edge(struct BMesh *bm, struct BMEdge *e, int select);
void BM_Select_Face(struct BMesh *bm, struct BMFace *f, int select);
void BM_Selectmode_Set(struct BMesh *bm, int selectmode);

/*counts number of elements with flag set*/
int BM_CountFlag(struct BMesh *bm, int type, int flag);

void BM_Select(struct BMesh *bm, void *element, int select);
int BM_Is_Selected(BMesh *bm, void *element);

/*edit selection stuff*/
void BM_editselection_center(BMesh *bm, float *center, BMEditSelection *ese);
void BM_editselection_normal(float *normal, BMEditSelection *ese);
void BM_editselection_plane(BMesh *bm, float *plane, BMEditSelection *ese);
void BM_remove_selection(BMesh *bm, void *data);
void BM_store_selection(BMesh *bm, void *data);
void BM_validate_selections(BMesh *bm);

#endif
