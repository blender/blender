#include "bmesh.h"

/*
note: this is a pattern-based edge subdivider.
it tries to match a pattern to edge selections on faces.

the patterns are defined as followed:

the patterns are defined for the state of the face after
initial splitting.  each edge that was split is flagged, as is
the new resulting edge.

subdpattern pattern = {
	//boolean flags for if an edge should have been split or not
	{1, 0, 0, 0},
	//connection values for verts,
	{2, -1, -1, -1},
	//second stage split flags, splits newly created edges
	{0, 0, 0, 0, 1},
	//second stage connection values for verts, connects stuff again.
	{-1, -1, -1, -1, 3},
	4 //len of face before second stage splits, but after initial edge splits
};

*/
typedef struct subdpattern {
	int *seledges; //selected edges mask, for splitting
	int *connectverts //verts to connect;

	int *secondstage_splitedges;
	//verts to connect afterwards.  size must be len + number 
	//of edges split in secondstage_splitedges
	int *secondstage_connect;

	int len; /*total number of verts*/
} subdpattern;


/*note: the patterns are rotated as necassary to
  match the input geometry.  they're also based on the
  post-splitted state of the faces.  note that
  second stage splitted stuff doesn't count
  for pattern->len!*/

/*
     v2
    /  \
v1 s_e1 e2
  /e0     \
v0---e3----v3

handle case of one edge selected.
*/

subdpattern t_1edge = {
	{1, 1, 0, 0},
	{-1, 3, -1, -1},
	NULL,
	NULL,
	4
};


/*
     v2
    /  \e2
v1 e1   e3 v3
  /e0     \
v0---e2----v4

handle case of two edges selected.
*/
subdpattern t_2edge = {
	{1, 1, 1, 1, 0},
	{0, 3, -1, -1, -1},
	NULL,
	NULL,
	5
};

/*
     v2
    /  \e2
v1 e1   e3 v3
  /e0     \
v0--e5--e4-v4
     v5

handle case of one edge selected.
make an edge between v1 and v3,
than split that and make an edge between
the new vert and v4
*/
subdpattern t_3edge = {
	{1, 1, 1, 1, 1, 1},
	{-1, 3, -1, -1, -1}, //creates e5
	{0, 0, 0, 0, 0, 1},
	{-1, -1, -1, -1, -1, 6, -1},
	6
};

subdpattern *patterns[] = {
	t_1edge,
	t_2edge,
	t_3edge,
};

#define PLEN	(sizeof(patterns) / sizeof(void*))

#define SUBD_SPLIT	1
#define MAX_FACE	10

void esubdivide_exec(BMesh *bmesh, BMOperator *op)
{
	BMOpSlot *einput;
	BMEdge *edge, *nedge, edges[MAX_FACE];
	BMFace *face;
	BMLoop *nl;
	BMVert *v1, verts[MAX_FACE], *lastv, *newv;
	BMIter fiter, eiter;
	subdpattern *pat;
	int i, j, matched, a, b, newlen, newvlen;
	
	BMO_Flag_Buffer(bm, dupeop, BMOP_ESUBDIVIDE_EDGES, SUBD_SPLIT);

	einput = BMO_GetSlot(op, BMOP_ESUBDIVIDE_EDGES);

	/*first go through and split edges*/
	for (i=0; i<einput->len; i++) {
		edge = ((BMEdge**)einput->data.p)[i];
		v1 = BM_Split_Edge(bmesh, edge->v1, edge, &nedge, 0.5, 1);
		BMO_SetFlag(bmesh, vert, SUBD_SPLIT);
		BMO_SetFlag(bmesh, nedge, SUBD_SPLIT);
	}

	/*now go through all the faces and connect the new geometry*/
	for (face = BMIter_New(&fiter, bmesh, BM_FACES, NULL); face; face=BMIter_Step(&fiter)) {
		matched = 0;

		if (face->len < MAX_FACE) {
			/*try all possible pattern rotations*/
			for (i=0, i<PLEN; i++) {
				if (patterns[i]->len != face->len) continue;
				lastv = NULL;
				for (j=0; j<patterns[i]->len; j++) {
					for (a=0, edge=BMIter_New(&eiter, bmesh, BM_EDGES_OF_FACE, face); edge; a++, edge=BMIter_Step(&eiter)) {
						verts[a] = edge->v1 == lastv? edge->v2 : edge->v1;
						edges[a] = edge;

						b = (j + a) % patterns[i]->len;
						if (!(patterns[i]->seledges[b] == BMOP_TestFlag(bmesh, edge, SUBD_SPLIT))) break;

						lastv = verts[a];
					}
					if (a == face->len) {
						matched = 1;
						pat = patterns[i];
						break;
					}
				}
			}
		}
		struct BMFace *BM_Split_Face(struct BMesh *bm, struct BMFace *f, struct BMVert *v1, struct BMVert *v2, struct BMLoop **nl, struct BMEdge *example, int calcnorm);

		if (matched) {
			/*first stage*/
			newlen = pat->len;
			for (i=0; i<pat->len; i++) {
				if (pat->connectverts[i] != -1) {
					BM_Split_Face(bmesh, face, verts[i], verts[pat->connectverts[i]], &nl, edges[i], 1);
					edges[newlen] = nl->edge;
					newlen++;
				}
			}

			newvlen = pat->len;			
			/*second stage*/
			for (i=0; i<pat->len; i++) {
				if (pat->secondstage_splitedges[i]) {
					v1 = BM_Split_Edge(bmesh, edges[i]->v1, edges[i], &nedge, 0.5, 1);
					verts[newvlen++] = v1;
				}
			}

			for (i=0; i<newvlen; i++) {
				if (pat->secondstage_connectverts[i] != -1) {
					BM_Split_Face(bmesh, face, verts[i], verts[pat->secondstage_connectverts[i]], &nl, edges[i], 1);
					edges[newlen] = nl->edge;
					newlen++;
				}
			}
		} else { /*no match in the pattern*/
		}
	}
}