#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "bmesh.h"
#include "mesh_intern.h"
#include "bmesh_private.h"
#include "BLI_arithb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FACE_MARK	1
#define FACE_ORIG	2
#define FACE_NEW	4
#define EDGE_MARK	1

#define VERT_MARK	1

static int check_hole_in_region(BMesh *bm, BMFace *f) {
	BMWalker regwalker;
	BMIter liter2;
	BMLoop *l2, *l3;
	BMFace *f2;
	
	/*checks if there are any unmarked boundary edges in the face region*/

	BMW_Init(&regwalker, bm, BMW_ISLAND, FACE_MARK);
	f2 = BMW_Begin(&regwalker, f);
	for (; f2; f2=BMW_Step(&regwalker)) {
		l2 = BMIter_New(&liter2, bm, BM_LOOPS_OF_FACE, f2);
		for (; l2; l2=BMIter_Step(&liter2)) {			
			l3 = bmesh_radial_nextloop(l2);
			if (BMO_TestFlag(bm, l3->f, FACE_MARK) 
			    != BMO_TestFlag(bm, l2->f, FACE_MARK))
			{
				if (!BMO_TestFlag(bm, l2->e, EDGE_MARK)) {
					return 0;
				}
			}
		}
	}
	BMW_End(&regwalker);

	return 1;
}

void dissolvefaces_exec(BMesh *bm, BMOperator *op)
{
	BMOIter oiter;
	BMIter liter, liter2;
	BMLoop *l, *l2;
	BMFace *f, *f2, *nf = NULL;
	V_DECLARE(region);
	V_DECLARE(regions);
	BMLoop ***regions = NULL;
	BMLoop **region = NULL;
	BMWalker walker, regwalker;
	int i, j, a, fcopied;

	BMO_Flag_Buffer(bm, op, BMOP_DISFACES_FACEIN, FACE_MARK);
	
	/*collect regions*/
	f = BMO_IterNew(&oiter, bm, op, BMOP_DISFACES_FACEIN);
	for (; f; f=BMO_IterStep(&oiter)) {
		if (!BMO_TestFlag(bm, f, FACE_MARK)) continue;

		l = BMIter_New(&liter, bm, BM_LOOPS_OF_FACE, f);
		for (; l; l=BMIter_Step(&liter)) {
			l2 = bmesh_radial_nextloop(l);
			if (l2!=l && BMO_TestFlag(bm, l2->f, FACE_MARK))
				continue;
			if (BM_FacesAroundEdge(l->e) <= 2) {
				V_RESET(region);
				region = NULL; /*forces different allocation*/

				/*yay, walk!*/
				BMW_Init(&walker, bm, BMW_ISLANDBOUND, FACE_MARK);
				l = BMW_Begin(&walker, l);
				for (; l; l=BMW_Step(&walker)) {
					V_GROW(region);
					region[V_COUNT(region)-1] = l;
					BMO_SetFlag(bm, l->e, EDGE_MARK);
				}
				BMW_End(&walker);

				if (BMO_HasError(bm)) {
					BMO_ClearStack(bm);
					BMO_RaiseError(bm, op, BMERR_DISSOLVEFACES_FAILED, NULL);
					goto cleanup;
				}
				
				if (region == NULL) continue;

				/*check for holes in boundary*/
				if (!check_hole_in_region(bm, region[0]->f)) {
					BMO_RaiseError(bm, op, 
					      BMERR_DISSOLVEFACES_FAILED, 
					      "Hole(s) in face region");
					goto cleanup;
				}
				
				BMW_Init(&regwalker, bm, BMW_ISLAND, FACE_MARK);
				f2 = BMW_Begin(&regwalker, region[0]->f);
				for (; f2; f2=BMW_Step(&regwalker)) {
					BMO_ClearFlag(bm, f2, FACE_MARK);
					BMO_SetFlag(bm, f2, FACE_ORIG);
				}
				BMW_End(&regwalker);

				if (BMO_HasError(bm)) {
					BMO_ClearStack(bm);
					BMO_RaiseError(bm, op, BMERR_DISSOLVEFACES_FAILED, NULL);
					goto cleanup;
				}
				
				V_GROW(region);
				V_GROW(regions);
				regions[V_COUNT(regions)-1] = region;
				region[V_COUNT(region)-1] = NULL;
				break;
			}
		}
	}
	
	for (i=0; i<V_COUNT(regions); i++) {
		BMEdge **edges = NULL;
		V_DECLARE(edges);

		region = regions[i];
		for (j=0; region[j]; j++) {
			V_GROW(edges);
			edges[V_COUNT(edges)-1] = region[j]->e;
		}
		
		if (region[0]->e->v1 == region[0]->v)
			f= BM_Make_Ngon(bm, region[0]->e->v1, region[0]->e->v2,  edges, j, 1);
		else
			f= BM_Make_Ngon(bm, region[0]->e->v2, region[0]->e->v1,  edges, j, 1);

		if (!f) {
			BMO_RaiseError(bm, op, BMERR_DISSOLVEFACES_FAILED, 
			                "Could not create merged face");
			goto cleanup;
		}

		/*if making the new face failed (e.g. overlapping test)
		  unmark the original faces for deletion.*/
		BMO_ClearFlag(bm, f, FACE_ORIG);
		BMO_SetFlag(bm, f, FACE_NEW);

		fcopied = 0;
		l=BMIter_New(&liter, bm, BM_LOOPS_OF_FACE, f);
		for (; l; l=BMIter_Step(&liter)) {
			/*ensure winding is identical*/
			l2 = BMIter_New(&liter2, bm, BM_LOOPS_OF_LOOP, l);
			for (; l2; l2=BMIter_Step(&liter2)) {
				if (BMO_TestFlag(bm, l2->f, FACE_ORIG)) {
					if (l2->v != l->v)
						bmesh_loop_reverse(bm, l2->f);
					break;
				}
			}
			
			/*copy over attributes*/
			l2 = BMIter_New(&liter2, bm, BM_LOOPS_OF_LOOP, l);
			for (; l2; l2=BMIter_Step(&liter2)) {
				if (BMO_TestFlag(bm, l2->f, FACE_ORIG)) {
					if (!fcopied) {
						BM_Copy_Attributes(bm, bm, l2->f, f);
						fcopied = 1;
					}
					BM_Copy_Attributes(bm, bm, l2, l);
					break;
				}
			}
		}
	}

	BMO_CallOpf(bm, "del geom=%ff context=%d", FACE_ORIG, DEL_FACES);
	if (BMO_HasError(bm)) goto cleanup;

	BMO_Flag_To_Slot(bm, op, BMOP_DISFACES_REGIONOUT, FACE_NEW, BM_FACE);

cleanup:
	/*free/cleanup*/
	for (i=0; i<V_COUNT(regions); i++) {
		if (regions[i]) V_FREE(regions[i]);
	}

	V_FREE(regions);
}

void dissolveverts_exec(BMesh *bm, BMOperator *op)
{
	BMOpSlot *vinput;
	BMIter iter, fiter;
	BMVert *v;
	BMFace *f;
	int i;
	
	vinput = BMO_GetSlot(op, BMOP_DISVERTS_VERTIN);

	BMO_Flag_Buffer(bm, op, BMOP_DISVERTS_VERTIN, VERT_MARK);
	
	for (v=BMIter_New(&iter, bm, BM_VERTS, NULL); v; v=BMIter_Step(&iter)) {
		if (BMO_TestFlag(bm, v, VERT_MARK)) {
			f=BMIter_New(&fiter, bm, BM_FACES_OF_VERT, v);
			for (; f; f=BMIter_Step(&fiter)) {
				BMO_SetFlag(bm, f, FACE_MARK);
			}
		}
	}

	BMO_CallOpf(bm, "dissolvefaces faces=%ff", FACE_MARK);
	if (BMO_HasError(bm)) {
			char *msg;

			BMO_GetError(bm, &msg, NULL);
			BMO_ClearStack(bm);
			BMO_RaiseError(bm, op, BMERR_DISSOLVEVERTS_FAILED,msg);
	}
	
	/*clean up any remaining*/
	for (v=BMIter_New(&iter, bm, BM_VERTS, NULL); v; v=BMIter_Step(&iter)) {
		if (BMO_TestFlag(bm, v, VERT_MARK)) {
			if (!BM_Dissolve_Vert(bm, v)) {
				BMO_RaiseError(bm, op, 
					BMERR_DISSOLVEVERTS_FAILED, NULL);
				return;
			}
		}
	}

}

/*this code is for cleaning up two-edged faces, it shall become
  it's own function one day.*/
#if 0
		/*clean up two-edged faces*/
		/*basic idea is to keep joining 2-edged faces until their
		  gone.  this however relies on joining two 2-edged faces
		  together to work, which doesn't.*/
		found3 = 1;
		while (found3) {
			found3 = 0;
			for (f=BMIter_New(&iter, bm, BM_FACES, NULL); f; f=BMIter_Step(&iter)){
				if (!BM_Validate_Face(bm, f, stderr)) {
					printf("error.\n");
				}

				if (f->len == 2) {
					//this design relies on join faces working
					//with two-edged faces properly.
					//commenting this line disables the
					//outermost loop.
					//found3 = 1;
					found2 = 0;
					l = BMIter_New(&liter, bm, BM_LOOPS_OF_FACE, f);
					fe = l->e;
					for (; l; l=BMIter_Step(&liter)) {
						f2 = BMIter_New(&fiter, bm,
								BM_FACES_OF_EDGE, l->e);
						for (; f2; f2=BMIter_Step(&fiter)) {
							if (f2 != f) {
								BM_Join_Faces(bm, f, f2, l->e, 
									      1, 0);
								found2 = 1;
								break;
							}
						}
						if (found2) break;
					}

					if (!found2) {
						bmesh_kf(bm, f);
						bmesh_ke(bm, fe);
					}
				} /*else if (f->len == 3) {
					BMEdge *ed[3];
					BMVert *vt[3];
					BMLoop *lp[3];
					int i=0;

					//check for duplicate edges
					l = BMIter_New(&liter, bm, BM_LOOPS_OF_FACE, f);
					for (; l; l=BMIter_Step(&liter)) {
						ed[i] = l->e;	
						lp[i] = l;
						vt[i++] = l->v;
					}
					if (vt[0] == vt[1] || vt[0] == vt[2]) {
						i += 1;
					}
				}*/
			}
		}
		if (oldlen == len) break;
		oldlen = len;

#endif
