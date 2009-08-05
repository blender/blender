#ifndef BM_MARKING_H
#define BM_MARKING_H

typedef struct BMEditSelection
{
	struct BMEditSelection *next, *prev;
	short type;
	void *data;
} BMEditSelection;

/*geometry hiding code*/
void BM_Hide(BMesh *bm, void *element, int hide);
void BM_Hide_Vert(BMesh *bm, BMVert *v, int hide);
void BM_Hide_Edge(BMesh *bm, BMEdge *e, int hide);
void BM_Hide_Face(BMesh *bm, BMFace *f, int hide);

/*Selection code*/
void BM_Select(struct BMesh *bm, void *element, int select);
/*I don't use this function anywhere, been using BM_TestHFlag instead.
  Need to decide either to keep it and convert everything over, or
  chuck it.*/
int BM_Selected(BMesh *bm, void *element);

/*individual element select functions, BM_Select is a shortcut for these
  that automatically detects which one to use*/
void BM_Select_Vert(struct BMesh *bm, struct BMVert *v, int select);
void BM_Select_Edge(struct BMesh *bm, struct BMEdge *e, int select);
void BM_Select_Face(struct BMesh *bm, struct BMFace *f, int select);

void BM_Selectmode_Set(struct BMesh *bm, int selectmode);

/*counts number of elements with flag set*/
int BM_CountFlag(struct BMesh *bm, int type, int flag, int respectflag);

/*edit selection stuff*/
void BM_editselection_center(BMesh *bm, float *center, BMEditSelection *ese);
void BM_editselection_normal(float *normal, BMEditSelection *ese);
void BM_editselection_plane(BMesh *bm, float *plane, BMEditSelection *ese);
void BM_remove_selection(BMesh *bm, void *data);
void BM_store_selection(BMesh *bm, void *data);
void BM_validate_selections(BMesh *bm);

#endif
