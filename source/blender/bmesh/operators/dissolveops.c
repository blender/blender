#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "bmesh.h"
#include "mesh_intern.h"
#include "bmesh_private.h"
#include "BLI_math.h"
#include "BLI_array.h"

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

	BMW_Init(&regwalker, bm, BMW_ISLAND, FACE_MARK, 0);
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
	BMIter liter, liter2, liter3;
	BMLoop *l, *l2, *l3;
	BMFace *f, *f2, *nf = NULL;
	BLI_array_declare(region);
	BLI_array_declare(regions);
	BMLoop ***regions = NULL;
	BMLoop **region = NULL;
	BMWalker regwalker;
	int i, j, fcopied;

	BMO_Flag_Buffer(bm, op, "faces", FACE_MARK, BM_FACE);
	
	/*collect regions*/
	f = BMO_IterNew(&oiter, bm, op, "faces", BM_FACE);
	for (; f; f=BMO_IterStep(&oiter)) {
		if (!BMO_TestFlag(bm, f, FACE_MARK)) continue;

		BLI_array_empty(region);
		region = NULL; /*forces different allocation*/

		/*yay, walk!*/
		BMW_Init(&regwalker, bm, BMW_ISLAND, FACE_MARK, 0);
		f2 = BMW_Begin(&regwalker, f);
		for (; f2; f2=BMW_Step(&regwalker)) {
			l2 = BMIter_New(&liter2, bm, BM_LOOPS_OF_FACE, f2);
			for (; l2; l2=BMIter_Step(&liter2)) {
				l3 = BMIter_New(&liter3, bm, BM_LOOPS_OF_LOOP, l2);
				for (; l3; l3=BMIter_Step(&liter3)) {
					if (!BMO_TestFlag(bm, l3->f, FACE_MARK)) {
						BLI_array_growone(region);
						region[BLI_array_count(region)-1] = l2;
						break;
					}
				}
				if (bmesh_radial_nextloop(l2) == l2) {
					BLI_array_growone(region);
					region[BLI_array_count(region)-1] = l2;
				}
			}
		}				
		BMW_End(&regwalker);

		BMW_Init(&regwalker, bm, BMW_ISLAND, FACE_MARK, 0);
		f2 = BMW_Begin(&regwalker, f);
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
		
		BLI_array_growone(region);
		BLI_array_growone(regions);
		regions[BLI_array_count(regions)-1] = region;
		region[BLI_array_count(region)-1] = NULL;
	}
	
	for (i=0; i<BLI_array_count(regions); i++) {
		BMEdge **edges = NULL;
		BLI_array_declare(edges);

		region = regions[i];
		for (j=0; region[j]; j++) {
			BLI_array_growone(edges);
			edges[BLI_array_count(edges)-1] = region[j]->e;
		}
		
		if (!region[0]) {
			BMO_RaiseError(bm, op, BMERR_DISSOLVEFACES_FAILED, 
			                "Could not find boundary of dissolve region");
			goto cleanup;
		}

		if (region[0]->e->v1 == region[0]->v)
			f= BM_Make_Ngon(bm, region[0]->e->v1, region[0]->e->v2,  edges, j, 1);
		else
			f= BM_Make_Ngon(bm, region[0]->e->v2, region[0]->e->v1,  edges, j, 1);
		
		BLI_array_free(edges);

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

	BMO_Flag_To_Slot(bm, op, "regionout", FACE_NEW, BM_FACE);

cleanup:
	/*free/cleanup*/
	for (i=0; i<BLI_array_count(regions); i++) {
		if (regions[i]) BLI_array_free(regions[i]);
	}

	BLI_array_free(regions);
}

/*almost identical to dissolve edge, except it cleans up vertices*/
void dissolve_edgeloop_exec(BMesh *bm, BMOperator *op)
{
	BMOperator fop;
	BMOIter oiter;
	BMIter iter;
	BMVert *v, **verts = NULL;
	BLI_array_declare(verts);
	BMEdge *e;
	BMFace *f;
	int i;

	BMO_ITER(e, &oiter, bm, op, "edges", BM_EDGE) {
		if (BM_Edge_FaceCount(e) == 2) {
			BMO_SetFlag(bm, e->v1, VERT_MARK);
			BMO_SetFlag(bm, e->v2, VERT_MARK);

			BM_Join_Faces(bm, e->loop->f, 
				      ((BMLoop*)e->loop->radial.next->data)->f,
				      e);
		}
	}

	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		if (BMO_TestFlag(bm, v, VERT_MARK) && 
			BM_Vert_EdgeCount(v) == 2) 
		{
			BLI_array_growone(verts);
			verts[BLI_array_count(verts)-1] = v;
		}
	}

	/*clean up extreneous 2-valence vertices*/
	for (i=0; i<BLI_array_count(verts); i++) {
		BM_Collapse_Vert(bm, verts[i]->edge, verts[i], 1.0);
	}
	
	BLI_array_free(verts);

	//BMO_InitOpf(bm, &fop, "dissolvefaces faces=%ff", FACE_MARK);
	//BMO_Exec_Op(bm, &fop);

	//BMO_CopySlot(op, &fop, "regionout", "regionout");

	//BMO_Finish_Op(bm, &fop);
}


void dissolveedges_exec(BMesh *bm, BMOperator *op)
{
	BMOperator fop;
	BMOIter oiter;
	BMIter iter;
	BMVert *v;
	BMEdge *e;
	BMFace *f;
	int i;

	BMO_ITER(e, &oiter, bm, op, "edges", BM_EDGE) {
		if (BM_Edge_FaceCount(e) == 2) {
			BMO_SetFlag(bm, e->v1, VERT_MARK);
			BMO_SetFlag(bm, e->v2, VERT_MARK);

			BM_Join_Faces(bm, e->loop->f, 
				      ((BMLoop*)e->loop->radial.next->data)->f,
				      e);
		}
	}

	//BMO_InitOpf(bm, &fop, "dissolvefaces faces=%ff", FACE_MARK);
	//BMO_Exec_Op(bm, &fop);

	//BMO_CopySlot(op, &fop, "regionout", "regionout");

	//BMO_Finish_Op(bm, &fop);
}

static int test_extra_verts(BMesh *bm, BMVert *v)
{
	BMIter iter, liter, iter2, iter3;
	BMFace *f, *f2;
	BMLoop *l;
	BMEdge *e;
	int found;

	/*test faces around verts for verts that would be wronly killed
	  by dissolve faces.*/
	f = BMIter_New(&iter, bm, BM_FACES_OF_VERT, v);
	for (; f; f=BMIter_Step(&iter)) {
		l=BMIter_New(&liter, bm, BM_LOOPS_OF_FACE, f);
		for (; l; l=BMIter_Step(&liter)) {
			if (!BMO_TestFlag(bm, l->v, VERT_MARK)) {
				/*if an edge around a vert is a boundary edge,
				   then dissolve faces won't destroy it.
				   also if it forms a boundary with one
				   of the face regions*/
				found = 0;
				e = BMIter_New(&iter2, bm, BM_EDGES_OF_VERT, l->v);
				for (; e; e=BMIter_Step(&iter2)) {
					if (BM_Edge_FaceCount(e)==1) found = 1;
					f2 = BMIter_New(&iter3, bm, BM_FACES_OF_EDGE, e);
					for (; f2; f2=BMIter_Step(&iter3)) {
						if (!BMO_TestFlag(bm, f2, FACE_MARK)) {
							found = 1;
							break;
						}
					}
					if (found) break;
				}
				if (!found) return 0;
			}
		}
	}

	return 1;
}
void dissolveverts_exec(BMesh *bm, BMOperator *op)
{
	BMOpSlot *vinput;
	BMIter iter, fiter;
	BMVert *v;
	BMFace *f;
	int i;
	
	vinput = BMO_GetSlot(op, "verts");
	BMO_Flag_Buffer(bm, op, "verts", VERT_MARK, BM_VERT);
	
	for (v=BMIter_New(&iter, bm, BM_VERTS_OF_MESH, NULL); v; v=BMIter_Step(&iter)) {
		if (BMO_TestFlag(bm, v, VERT_MARK)) {
			/*check if it's a two-valence vert*/
			if (BM_Vert_EdgeCount(v) == 2) {

				/*collapse the vert*/
				BM_Collapse_Vert(bm, v->edge, v, 0.5f);
				continue;
			}

			f=BMIter_New(&fiter, bm, BM_FACES_OF_VERT, v);
			for (; f; f=BMIter_Step(&fiter)) {
				BMO_SetFlag(bm, f, FACE_ORIG);
				BMO_SetFlag(bm, f, FACE_MARK);
			}
			
			/*check if our additions to the input to face dissolve
			  will destroy nonmarked vertices.*/
			if (!test_extra_verts(bm, v)) {
				f=BMIter_New(&fiter, bm, BM_FACES_OF_VERT, v);
				for (; f; f=BMIter_Step(&fiter)) {
					if (BMO_TestFlag(bm, f, FACE_ORIG)) {
						BMO_ClearFlag(bm, f, FACE_MARK);
						BMO_ClearFlag(bm, f, FACE_ORIG);
					}
				}
			} else {
				f=BMIter_New(&fiter, bm, BM_FACES_OF_VERT, v);
				for (; f; f=BMIter_Step(&fiter)) {
					BMO_ClearFlag(bm, f, FACE_ORIG);
				}
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
	for (v=BMIter_New(&iter, bm, BM_VERTS_OF_MESH, NULL); v; v=BMIter_Step(&iter)) {
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
			for (f=BMIter_New(&iter, bm, BM_FACES_OF_MESH, NULL); f; f=BMIter_Step(&iter)){
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
								BM_Join_Faces(bm, f, f2, l->e);
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
