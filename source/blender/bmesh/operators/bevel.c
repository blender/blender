#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "BLI_ghash.h"
#include "BLI_memarena.h"
#include "BLI_blenlib.h"
#include "BLI_array.h"
#include "BLI_math.h"
#include "BLI_array.h"
#include "BLI_utildefines.h"
#include "BLI_smallhash.h"

#include "bmesh.h"
#include "bmesh_operators_private.h"

#define BEVEL_FLAG	1
#define BEVEL_DEL	2
#define FACE_NEW	4
#define EDGE_OLD	8
#define FACE_OLD	16
#define FACE_DONE	32
#define VERT_OLD	64
#define FACE_SPAN	128
#define FACE_HOLE	256

typedef struct LoopTag {
	BMVert *newv;
} LoopTag;

typedef struct EdgeTag {
	BMVert *newv1, *newv2;
} EdgeTag;


/* "Projects" a vector perpendicular to vec2 against vec1, such that
 * the projected vec1 + vec2 has a min distance of 1 from the "edge" defined by vec2.
 * note: the direction, is_forward, is used in conjunction with up_vec to determine
 * whether this is a convex or concave corner. If it is a concave corner, it will
 * be projected "backwards." If vec1 is before vec2, is_forward should be 0 (we are projecting backwards).
 * vec1 is the vector to project onto (expected to be normalized)
 * vec2 is the direction of projection (pointing away from vec1)
 * up_vec is used for orientation (expected to be normalized)
 * returns the length of the projected vector that lies along vec1 */
static float BM_bevel_project_vec(float *vec1, float *vec2, float *up_vec, int is_forward) {
	float factor, vec3[3], tmp[3],c1,c2;

	cross_v3_v3v3(tmp,vec1,vec2);
	normalize_v3(tmp);
	factor = dot_v3v3(up_vec,tmp);
	if ((factor > 0 && is_forward) || (factor < 0 && !is_forward)) {
		cross_v3_v3v3(vec3,vec2,tmp); /* hmm, maybe up_vec should be used instead of tmp */
	}
	else {
		cross_v3_v3v3(vec3,tmp,vec2); /* hmm, maybe up_vec should be used instead of tmp */
	}
	normalize_v3(vec3);
	c1 = dot_v3v3(vec3,vec1);
	c2 = dot_v3v3(vec1,vec1);
	if (fabs(c1) < 0.000001f || fabs(c2) < 0.000001f) {
		factor = 0.0f;
	}
	else {
		factor = c2/c1;
	}

	return factor;
}

void calc_corner_co(BMesh *bm, BMLoop *l, float *co, float fac)
{
	float no[3], tan[3], vec1[3], vec2[3], v1[3], v2[3], v3[3], v4[3];
	float p1[3], p2[3], w[3];
	float l1, l2;
	int ret;

	copy_v3_v3(v1, l->prev->v->co);
	copy_v3_v3(v2, l->v->co);
	copy_v3_v3(v3, l->v->co);
	copy_v3_v3(v4, l->next->v->co);
	
	/*calculate normal*/
	sub_v3_v3v3(vec1, v1, v2);
	sub_v3_v3v3(vec2, v4, v3);
#if 0
	cross_v3_v3v3(no, vec2, vec1);
	normalize_v3(no);
	
	if (dot_v3v3(no, no) < DBL_EPSILON*10) {
		copy_v3_v3(no, l->f->no);
	}
	
	/*compute offsets*/
	l1 = len_v3(vec1)*fac;
	l2 = len_v3(vec2)*fac;
	if (dot_v3v3(no, l->f->no) < 0.0) {
		l1 = -l1;
		l2 = -l2;
	}	
	
	/*compute tangent and offset first edge*/
	cross_v3_v3v3(tan, vec1, no);
	normalize_v3(tan);

	mul_v3_fl(tan, l1);
	
	add_v3_v3(v1, tan);
	add_v3_v3(v2, tan);
	
	/*compute tangent and offset second edge*/
	cross_v3_v3v3(tan, no, vec2);
	normalize_v3(tan);
	
	mul_v3_fl(tan, l2);

	add_v3_v3(v3, tan);
	add_v3_v3(v4, tan);
	
	/*compute intersection*/
	ret = isect_line_line_v3(v1, v2, v3, v4, p1, p2);
	if (ret==1) {
		copy_v3_v3(co, p1);
	} else if (ret==2) {
		add_v3_v3v3(co, p1, p2);
		mul_v3_fl(co, 0.5);
	} else { /*colinear case*/
		add_v3_v3v3(co, v2, v3);
		mul_v3_fl(co, 0.5);
	}
#endif
	/*oddly, this simplistic method seems to work the best*/
	mul_v3_fl(vec1, fac);
	mul_v3_fl(vec2, fac);
	add_v3_v3(vec1, vec2);
	mul_v3_fl(vec1, 0.5);
	
	add_v3_v3v3(co, vec1, l->v->co);
}

#define ETAG_SET(e, v, nv) (v) == (e)->v1 ? (etags[BMINDEX_GET((e))].newv1 = (nv)) : (etags[BMINDEX_GET((e))].newv2 = (nv))
#define ETAG_GET(e, v) ((v) == (e)->v1 ? (etags[BMINDEX_GET((e))].newv1) : (etags[BMINDEX_GET((e))].newv2))

void bmesh_bevel_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMIter iter;
	BMEdge *e;
	BMVert *v;
	BMFace **faces = NULL;
	LoopTag *tags=NULL, *tag;
	EdgeTag *etags = NULL, *etag;
	BMVert **verts = NULL;
	BMEdge **edges = NULL;
	BLI_array_declare(faces);
	BLI_array_declare(tags);
	BLI_array_declare(etags);
	BLI_array_declare(verts);
	BLI_array_declare(edges);
	SmallHash hash;
	float fac = BMO_Get_Float(op, "percent");
	int i;
	
	BLI_smallhash_init(&hash);
	
	BMO_ITER(e, &siter, bm, op, "geom", BM_EDGE) {
		BMO_SetFlag(bm, e, BEVEL_FLAG|BEVEL_DEL);
		BMO_SetFlag(bm, e->v1, BEVEL_FLAG|BEVEL_DEL);
		BMO_SetFlag(bm, e->v2, BEVEL_FLAG|BEVEL_DEL);
		
		if (BM_Edge_FaceCount(e) < 2) {
			BMO_ClearFlag(bm, e, BEVEL_DEL);
			BMO_ClearFlag(bm, e->v1, BEVEL_DEL);
			BMO_ClearFlag(bm, e->v2, BEVEL_DEL);
		}
	}
	
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		BMO_SetFlag(bm, v, VERT_OLD);
	}

#if 0
	//a bit of cleaner code that, alas, doens't work.
	/*build edge tags*/
	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		if (BMO_TestFlag(bm, e->v1, BEVEL_FLAG) || BMO_TestFlag(bm, e->v2, BEVEL_FLAG)) {
			BMIter liter;
			BMLoop *l;
			
			if (!BMO_TestFlag(bm, e, EDGE_OLD)) {
				BMINDEX_SET(e, BLI_array_count(etags));
				BLI_array_growone(etags);
				
				BMO_SetFlag(bm, e, EDGE_OLD);
			}
			
			BM_ITER(l, &liter, bm, BM_LOOPS_OF_EDGE, e) {
				BMLoop *l2;
				BMIter liter2;
				
				if (BMO_TestFlag(bm, l->f, BEVEL_FLAG))
					continue;
			
				BM_ITER(l2, &liter2, bm, BM_LOOPS_OF_FACE, l->f) {
					BMINDEX_SET(l2, BLI_array_count(tags));
					BLI_array_growone(tags);

					if (!BMO_TestFlag(bm, l2->e, EDGE_OLD)) {
						BMINDEX_SET(l2->e, BLI_array_count(etags));
						BLI_array_growone(etags);
						
						BMO_SetFlag(bm, l2->e, EDGE_OLD);
					}
				}

				BMO_SetFlag(bm, l->f, BEVEL_FLAG);
				BLI_array_append(faces, l->f);
			}
		} else {
			BMINDEX_SET(e, -1);
		}
	}
#endif
	
	/*create and assign looptag structures*/
	BMO_ITER(e, &siter, bm, op, "geom", BM_EDGE) {
		BMLoop *l;
		BMIter liter;

		BMO_SetFlag(bm, e->v1, BEVEL_FLAG|BEVEL_DEL);
		BMO_SetFlag(bm, e->v2, BEVEL_FLAG|BEVEL_DEL);
		
		if (BM_Edge_FaceCount(e) < 2) {
			BMO_ClearFlag(bm, e, BEVEL_DEL);
			BMO_ClearFlag(bm, e->v1, BEVEL_DEL);
			BMO_ClearFlag(bm, e->v2, BEVEL_DEL);
			//continue;	
		}
		
		if (!BLI_smallhash_haskey(&hash, (intptr_t)e)) {
			BLI_array_growone(etags);
			BMINDEX_SET(e, BLI_array_count(etags)-1);
			BLI_smallhash_insert(&hash, (intptr_t)e, NULL);
			BMO_SetFlag(bm, e, EDGE_OLD);
		}
		
		/*find all faces surrounding e->v1 and, e->v2*/
		for (i=0; i<2; i++) {
			BM_ITER(l, &liter, bm, BM_LOOPS_OF_VERT, i?e->v2:e->v1) {
				BMLoop *l2;
				BMIter liter2;
				
				/*see if we've already processed this loop's face*/
				if (BLI_smallhash_haskey(&hash, (intptr_t)l->f))
					continue;
				
				/*create tags for all loops in l->f*/
				BM_ITER(l2, &liter2, bm, BM_LOOPS_OF_FACE, l->f) {
					BLI_array_growone(tags);
					BMINDEX_SET(l2, BLI_array_count(tags)-1);
					
					if (!BLI_smallhash_haskey(&hash, (intptr_t)l2->e)) {
						BLI_array_growone(etags);
						BMINDEX_SET(l2->e, BLI_array_count(etags)-1);
						BLI_smallhash_insert(&hash, (intptr_t)l2->e, NULL);						
						BMO_SetFlag(bm, l2->e, EDGE_OLD);
					}
				}
	
				BLI_smallhash_insert(&hash, (intptr_t)l->f, NULL);
				BMO_SetFlag(bm, l->f, BEVEL_FLAG);
				BLI_array_append(faces, l->f);
			}
		}
	}

	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		BMIter eiter;
		
		if (!BMO_TestFlag(bm, v, BEVEL_FLAG))
			continue;
		
		BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, v) {
			if (!BMO_TestFlag(bm, e, BEVEL_FLAG) && !ETAG_GET(e, v)) {
				BMVert *v2;
				float co[3];
				
				v2 = BM_OtherEdgeVert(e, v);
				sub_v3_v3v3(co, v2->co, v->co);
				mul_v3_fl(co, fac);
				add_v3_v3(co, v->co);
				
				v2 = BM_Make_Vert(bm, co, v);
				ETAG_SET(e, v, v2);
			}
		}
	}
	
	for (i=0; i<BLI_array_count(faces); i++) {
		BMLoop *l;
		BMIter liter;
		
		BMO_SetFlag(bm, faces[i], FACE_OLD);
		
		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, faces[i]) {
			float co[3];

			if (BMO_TestFlag(bm, l->e, BEVEL_FLAG)) {
				if (BMO_TestFlag(bm, l->prev->e, BEVEL_FLAG))
				{
					tag = tags + BMINDEX_GET(l);
					calc_corner_co(bm, l, co, fac);
					tag->newv = BM_Make_Vert(bm, co, l->v);
				} else {
					tag = tags + BMINDEX_GET(l);
					tag->newv = ETAG_GET(l->prev->e, l->v);
					
					if (!tag->newv) {
						sub_v3_v3v3(co, l->prev->v->co, l->v->co);
						mul_v3_fl(co, fac);
						add_v3_v3(co, l->v->co);
					
						tag->newv = BM_Make_Vert(bm, co, l->v);
						
						ETAG_SET(l->prev->e, l->v, tag->newv);
					}
				}
			} else if (BMO_TestFlag(bm, l->v, BEVEL_FLAG)) {
				tag = tags + BMINDEX_GET(l);
				tag->newv = ETAG_GET(l->e, l->v);				
		
				if (!tag->newv) {
					sub_v3_v3v3(co, l->next->v->co, l->v->co);
					mul_v3_fl(co, fac);
					add_v3_v3(co, l->v->co);
			
					tag = tags + BMINDEX_GET(l);
					tag->newv = BM_Make_Vert(bm, co, l->v);
					
					ETAG_SET(l->e, l->v, tag->newv);
				}					
			} else {
				tag = tags + BMINDEX_GET(l);
				tag->newv = l->v;
				BMO_ClearFlag(bm, l->v, BEVEL_DEL);
			}
		}
	}
	
	/*create new faces*/
	for (i=0; i<BLI_array_count(faces); i++) {
		BMLoop *l;
		BMIter liter;
		BMFace *f;
		int j;
		
		BMO_SetFlag(bm, faces[i], BEVEL_DEL);
		
		BLI_array_empty(verts);
		BLI_array_empty(edges);
		
		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, faces[i]) {
			BMVert *v2;
			
			tag = tags + BMINDEX_GET(l);
			BLI_array_append(verts, tag->newv);
			
			etag = etags + BMINDEX_GET(l->e);
			v2 = l->next->v == l->e->v1 ? etag->newv1 : etag->newv2;
			
			tag = tags + BMINDEX_GET(l->next);
			if (!BMO_TestFlag(bm, l->e, BEVEL_FLAG) && v2 && v2 != tag->newv) {
				BLI_array_append(verts, v2);
			}
		}
		
		for (j=0; j<BLI_array_count(verts); j++) {
			BMVert *next = verts[(j+1)%BLI_array_count(verts)];

			e = BM_Make_Edge(bm, next, verts[j], NULL, 1);
			BLI_array_append(edges, e);
		}
		
		f = BM_Make_Face(bm, verts, edges, BLI_array_count(verts));
		if (!f) {
			printf("eck!!\n");
			continue;
		}
			
		BMO_SetFlag(bm, f, FACE_NEW);
		
		/*create quad spans between split edges*/
		BMO_SetFlag(bm, f, FACE_NEW);
		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, faces[i]) {
			BMVert *v1=NULL, *v2=NULL, *v3=NULL, *v4=NULL;
			
			if (!BMO_TestFlag(bm, l->e, BEVEL_FLAG))
				continue;
			
			v1 = tags[BMINDEX_GET(l)].newv;
			v2 = tags[BMINDEX_GET(l->next)].newv;
			if (l->radial_next != l) {
				v3 = tags[BMINDEX_GET(l->radial_next)].newv;
				if (l->radial_next->next->v == l->next->v) {
					v4 = v3;
					v3 = tags[BMINDEX_GET(l->radial_next->next)].newv;
				} else {
					v4 = tags[BMINDEX_GET(l->radial_next->next)].newv;
				}
			} else {
				v3 = l->next->v;
				v4 = l->v;
				
				for (j=0; j<2; j++) {
					BMIter eiter;
					BMVert *v = j ? v4 : v3;

					BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, v) {
						if (!BM_Vert_In_Edge(e, v3) || !BM_Vert_In_Edge(e, v4))
							continue;
						
						if (!BMO_TestFlag(bm, e, BEVEL_FLAG) && BMO_TestFlag(bm, e, EDGE_OLD)) {
							BMVert *vv;
							
							vv = ETAG_GET(e, v);
							if (!vv || BMO_TestFlag(bm, vv, BEVEL_FLAG))
								continue;
							
							if (j)
								v1 = vv;
							else
								v2 = vv;
							break;
						}
					}
				}

				BMO_ClearFlag(bm, v3, BEVEL_DEL);
				BMO_ClearFlag(bm, v4, BEVEL_DEL);
			}
			
			if (v1 != v2 && v2 != v3 && v3 != v4) {
				BMIter liter2;
				BMLoop *l2;
				
				f = BM_Make_QuadTri(bm, v4, v3, v2, v1, l->f, 1);
				if (!f) {
					printf("eek!\n");
					continue;
				}
				
				BMO_SetFlag(bm, f, FACE_NEW|FACE_SPAN);
				
				/*un-tag edges in f for deletion*/
				BM_ITER(l2, &liter2, bm, BM_LOOPS_OF_FACE, f) {
					BMO_ClearFlag(bm, l2->e, BEVEL_DEL);
				}
			} else {
				f = NULL;
			}
		}	
	}
	
	/*fill in holes at vertices*/
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		BMIter eiter;
		BMVert *vv, *vstart=NULL, *lastv=NULL;
		SmallHash tmphash;
		int rad, insorig=0, err=0;
		
		BLI_smallhash_init(&tmphash);
		
		if (!BMO_TestFlag(bm, v, BEVEL_FLAG))
			continue;
		
		BLI_array_empty(verts);
		BLI_array_empty(edges);
		
		BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, v) {
			BMIter liter;
			BMVert *v1=NULL, *v2=NULL;
			BMLoop *l;
			
			if (BM_Edge_FaceCount(e) < 2)
				insorig = 1;
			
			rad = 0;
			BM_ITER(l, &liter, bm, BM_LOOPS_OF_EDGE, e) {
				if (!BMO_TestFlag(bm, l->f, FACE_OLD))
					continue;
				
				rad++;
				
				if (l->v == v)
					tag = tags + BMINDEX_GET(l);
				else
					tag = tags + BMINDEX_GET(l->next);
				
				if (!v1)
					v1 = tag->newv;
				else if (!v2);
					v2 = tag->newv;
			}
			
			if (rad < 2)
				insorig = 1;
			
			if (!v1)
				v1 = ETAG_GET(e, v);
			if (!v2 || v1 == v2)
				v2 = ETAG_GET(e, v);
			
			if (v1) {
				if (!BLI_smallhash_haskey(&tmphash, (intptr_t)v1)) {
					BLI_array_append(verts, v1);
					BLI_smallhash_insert(&tmphash, (intptr_t)v1, NULL);
				}
				
				if (v2 && v1 != v2 && !BLI_smallhash_haskey(&tmphash, (intptr_t)v2)) {
					BLI_array_append(verts, v2);
					BLI_smallhash_insert(&tmphash, (intptr_t)v2, NULL);
				}
			}
		}
		
		if (!BLI_array_count(verts))
			continue;
		
		if (insorig) {
			BLI_array_append(verts, v);
			BLI_smallhash_insert(&tmphash, (intptr_t)v, NULL);
		}
		
		/*find edges that exist between vertices in verts.  this is basically
          a topological walk of the edges connecting them.*/
		vstart = vstart ? vstart : verts[0];
		vv = vstart;
		do {
			BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, vv) {
				BMVert *vv2 = BM_OtherEdgeVert(e, vv);
				
				if (vv2 != lastv && BLI_smallhash_haskey(&tmphash, (intptr_t)vv2)) {
					/*if we've go over the same vert twice, break out of outer loop*/
					if (BLI_smallhash_lookup(&tmphash, (intptr_t)vv2) != NULL) {
						e = NULL;
						err = 1;
						break;
					}
					
					/*use self pointer as tag*/
					BLI_smallhash_remove(&tmphash, (intptr_t)vv2);
					BLI_smallhash_insert(&tmphash, (intptr_t)vv2, vv2);
					
					lastv = vv;
					BLI_array_append(edges, e);
					vv = vv2;
					break;
				}
			}
			if (e == NULL)
				break;
		} while (vv != vstart);
		
		if (err)
			continue;
		
		/*there may not be a complete loop of edges, so start again and make
          final edge afterwards.  in this case, the previous loop worked to
          find one of the two edges at the extremes.*/
		if (vv != vstart) {
			/*undo previous tagging*/
			for (i=0; i<BLI_array_count(verts); i++) {
				BLI_smallhash_remove(&tmphash, (intptr_t)verts[i]);
				BLI_smallhash_insert(&tmphash, (intptr_t)verts[i], NULL);
			}

			vstart = vv;
			lastv = NULL;
			BLI_array_empty(edges);
			do {
				BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, vv) {
					BMVert *vv2 = BM_OtherEdgeVert(e, vv);
					
					if (vv2 != lastv && BLI_smallhash_haskey(&tmphash, (intptr_t)vv2)) {
						/*if we've go over the same vert twice, break out of outer loop*/
						if (BLI_smallhash_lookup(&tmphash, (intptr_t)vv2) != NULL) {
							e = NULL;
							err = 1;
							break;
						}
						
						/*use self pointer as tag*/
						BLI_smallhash_remove(&tmphash, (intptr_t)vv2);
						BLI_smallhash_insert(&tmphash, (intptr_t)vv2, vv2);
						
						lastv = vv;
						BLI_array_append(edges, e);
						vv = vv2;
						break;
					}
				}
				if (e == NULL)
					break;
			} while (vv != vstart);
			
			if (!err) {
				e = BM_Make_Edge(bm, vv, vstart, NULL, 1);
				BLI_array_append(edges, e);
			}
		}
		
		if (err)
			continue;
		
		if (BLI_array_count(edges) >= 3) {
			BMFace *f;
			
			f = BM_Make_Ngon(bm, lastv, vstart, edges, BLI_array_count(edges), 0);
			if (!f) {
				printf("eek! in bevel vert fill!\n");
			} else 
				BMO_SetFlag(bm, f, FACE_NEW|FACE_HOLE);
		}
		BLI_smallhash_release(&tmphash);
	}
	
	/*copy over customdata*/
	for (i=0; i<BLI_array_count(faces); i++) {
		BMLoop *l;
		BMIter liter;
		BMFace *f = faces[i];
		
		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
			BMLoop *l2;
			BMIter liter2;
			
			tag = tags + BMINDEX_GET(l);
			if (!tag->newv)
				continue;
			
			BM_ITER(l2, &liter2, bm, BM_LOOPS_OF_VERT, tag->newv) {
				if (!BMO_TestFlag(bm, l2->f, FACE_NEW) || (l2->v != tag->newv && l2->v != l->v))
					continue;
				
				if (tag->newv != l->v) {
					BM_Copy_Attributes(bm, bm, l->f, l2->f);
					BM_loop_interp_from_face(bm, l2, f);
				} else {
					BM_Copy_Attributes(bm, bm, l->f, l2->f);
					BM_Copy_Attributes(bm, bm, l, l2);
				}
			}
		}
	}
	
	/*handle vertices along boundary edges*/
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		if (BMO_TestFlag(bm, v, VERT_OLD) && BMO_TestFlag(bm, v, BEVEL_FLAG) && !BMO_TestFlag(bm, v, BEVEL_DEL)) {
			BMLoop *l;
			BMLoop *lorig=NULL;
			BMIter liter;
			
			BM_ITER(l, &liter, bm, BM_LOOPS_OF_VERT, v) {
				BMIter liter2;
				BMLoop *l2 = l->v == v ? l : l->next, *l3;
				
				if (BMO_TestFlag(bm, l->f, FACE_OLD)) {
					lorig = l;
					break;
				}
			}
			
			if (!lorig)
				continue;
			
			BM_ITER(l, &liter, bm, BM_LOOPS_OF_VERT, v) {
				BMLoop *l2 = l->v == v ? l : l->next, *l3;
				
				BM_Copy_Attributes(bm, bm, lorig->f, l2->f);
				BM_Copy_Attributes(bm, bm, lorig, l2);
			}
		}
	}

	BMO_CallOpf(bm, "del geom=%fv context=%i", BEVEL_DEL, DEL_VERTS);

	/*clean up any edges that might not get properly deleted*/
	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		if (BMO_TestFlag(bm, e, EDGE_OLD) && !e->l)
			BMO_SetFlag(bm, e, BEVEL_DEL);
	}

	BMO_CallOpf(bm, "del geom=%fe context=%i", BEVEL_DEL, DEL_EDGES);
	BMO_CallOpf(bm, "del geom=%ff context=%i", BEVEL_DEL, DEL_FACES);
	
	BLI_smallhash_release(&hash);
	BLI_array_free(tags);
	BLI_array_free(etags);
	BLI_array_free(verts);
	BLI_array_free(edges);
	BLI_array_free(faces);
	
	BMO_Flag_To_Slot(bm, op, "face_spans", FACE_SPAN, BM_FACE);
	BMO_Flag_To_Slot(bm, op, "face_holes", FACE_HOLE, BM_FACE);
}
