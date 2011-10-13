#include <limits.h>

#include "BLI_math_vector.h"

#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_listbase.h"
#include "BLI_mempool.h"
#include "BLI_ghash.h"
#include "BLI_array.h"

#include "DNA_listBase.h"

#include "bmesh_class.h"

#include "bmesh_iterators.h"
#include "bmesh_private.h"

BMVert *BM_Make_Vert(BMesh *bm, float co[3], const struct BMVert *example) {
	BMVert *v = BLI_mempool_calloc(bm->vpool);
	
	bm->totvert += 1;

	v->head.type = BM_VERT;

	/* 'v->no' is handled by BM_Copy_Attributes */
	if (co) copy_v3_v3(v->co, co);
	
	/*allocate flags*/
	v->head.flags = BLI_mempool_calloc(bm->toolflagpool);

	CustomData_bmesh_set_default(&bm->vdata, &v->head.data);
	
	if (example) {
		BM_Copy_Attributes(bm, bm, (BMVert*)example, (BMVert*)v);
	}

	CHECK_ELEMENT(bm, v);

	return (BMVert*) v;
}

/**
 *			BMESH EDGE EXIST
 *
 *  Finds out if two vertices already have an edge
 *  connecting them. Note that multiple edges may
 *  exist between any two vertices, and therefore
 *  This function only returns the first one found.
 *
 *  Returns -
 *	BMEdge pointer
 */
BMEdge *BM_Edge_Exist(BMVert *v1, BMVert *v2)
{
	BMIter iter;
	BMEdge *e;

	BM_ITER(e, &iter, NULL, BM_EDGES_OF_VERT, v1) {
		if (e->v1 == v2 || e->v2 == v2)
			return e;
	}

	return NULL;
}

BMEdge *BM_Make_Edge(BMesh *bm, BMVert *v1, BMVert *v2, const BMEdge *example, int nodouble) {
	BMEdge *e;
	
	if (nodouble && (e= BM_Edge_Exist(v1, v2)))
		return (BMEdge*)e;
	
	e = BLI_mempool_calloc(bm->epool);
	bm->totedge += 1;
	e->head.type = BM_EDGE;
	
	/*allocate flags*/
	e->head.flags = BLI_mempool_calloc(bm->toolflagpool);

	e->v1 = (BMVert*) v1;
	e->v2 = (BMVert*) v2;
	
	
	CustomData_bmesh_set_default(&bm->edata, &e->head.data);
	
	bmesh_disk_append_edge(e, e->v1);
	bmesh_disk_append_edge(e, e->v2);
	
	if (example)
		BM_Copy_Attributes(bm, bm, (BMEdge*)example, (BMEdge*)e);
	
	CHECK_ELEMENT(bm, e);

	return (BMEdge*) e;
}

static BMLoop *bmesh_create_loop(BMesh *bm, BMVert *v, BMEdge *e, BMFace *f, const BMLoop *example){
	BMLoop *l=NULL;

	l = BLI_mempool_calloc(bm->lpool);
	l->next = l->prev = NULL;
	l->v = v;
	l->e = e;
	l->f = f;
	l->radial_next = l->radial_prev = NULL;
	l->head.data = NULL;
	l->head.type = BM_LOOP;

	bm->totloop++;

	if(example)
		CustomData_bmesh_copy_data(&bm->ldata, &bm->ldata, example->head.data, &l->head.data);
	else
		CustomData_bmesh_set_default(&bm->ldata, &l->head.data);

	return l;
}

static BMLoop *BM_Add_FaceBoundary(BMesh *bm, BMFace *f, BMVert *startv, BMEdge *starte) {
	BMLoopList *lst = BLI_mempool_calloc(bm->looplistpool);
	BMLoop *l = bmesh_create_loop(bm, startv, starte, f, NULL);
	
	bmesh_radial_append(starte, l);

	lst->first = lst->last = l;
	BLI_addtail(&f->loops, lst);
	
	l->f = f;
	
	return l;	
}

BMFace *BM_Copy_Face(BMesh *bm, BMFace *f, int copyedges, int copyverts)
{
	BMEdge **edges = NULL;
	BMVert **verts = NULL;
	BLI_array_staticdeclare(edges, 256);
	BLI_array_staticdeclare(verts, 256);
	BMLoop *l, *l2;
	BMFace *f2;
	int i;

	l = bm_firstfaceloop(f);
	do {
		if (copyverts) {
			BMVert *v = BM_Make_Vert(bm, l->v->co, l->v);
			BLI_array_append(verts, v);
		} else {	
			BLI_array_append(verts, l->v);
		}
		l = l->next;
	} while (l != bm_firstfaceloop(f));

	l = bm_firstfaceloop(f);
	i = 0;
	do {
		if (copyedges) {
			BMEdge *e;
			BMVert *v1, *v2;
			
			if (l->e->v1 == verts[i]) {
				v1 = verts[i];
				v2 = verts[(i+1)%f->len];
			} else {
				v2 = verts[i];
				v1 = verts[(i+1)%f->len];
			}
			
			e = BM_Make_Edge(bm,  v1, v2, l->e, 0);
			BLI_array_append(edges, e);
		} else {
			BLI_array_append(edges, l->e);
		}
		
		i++;
		l = l->next;
	} while (l != bm_firstfaceloop(f));
	
	f2 = BM_Make_Face(bm, verts, edges, f->len, 0);
	
	BM_Copy_Attributes(bm, bm, f, f2);
	
	l = bm_firstfaceloop(f);
	l2 = bm_firstfaceloop(f2);
	do {
		BM_Copy_Attributes(bm, bm, l, l2);
		l = l->next;
		l2 = l2->next;
	} while (l != bm_firstfaceloop(f));
	
	return f2;
}

BMFace *BM_Make_Face(BMesh *bm, BMVert **verts, BMEdge **edges, int len, int nodouble) {
	BMFace *f = NULL;
	BMLoop *l, *startl, *lastl;
	int i, overlap;
	
	if (len == 0) {
		/*just return NULL for now*/
		return NULL;
	}

	if (nodouble) {
		/* Check if face already exists */
		overlap = BM_Face_Exists(bm, verts, len, &f);
		if (overlap) {
			return f;
		}
		else {
			BLI_assert(f == NULL);
		}
	}
	
	f = BLI_mempool_calloc(bm->fpool);
	bm->totface += 1;
	f->head.type = BM_FACE;

	startl = lastl = BM_Add_FaceBoundary(bm, (BMFace*)f, verts[0], edges[0]);
	
	startl->v = (BMVert*) verts[0];
	startl->e = (BMEdge*) edges[0];
	for (i=1; i<len; i++) {
		l = bmesh_create_loop(bm, verts[i], edges[i], (BMFace *)f, edges[i]->l);
		
		l->f = (BMFace*) f;
		bmesh_radial_append(edges[i], l);

		l->prev = lastl;
		lastl->next = l;
		lastl = l;
	}
	
	/*allocate flags*/
	f->head.flags = BLI_mempool_calloc(bm->toolflagpool);

	CustomData_bmesh_set_default(&bm->pdata, &f->head.data);
	
	startl->prev = lastl;
	lastl->next = startl;
	
	f->len = len;
	f->totbounds = 0;
	
	CHECK_ELEMENT(bm, f);

	return (BMFace*) f;
}

int bmesh_check_element(BMesh *UNUSED(bm), void *element, int type) {
	BMHeader *head = element;
	int err = 0;

	if (!element)
		return 1;

	if (head->type != type)
		return 2;
	
	switch (type) {
	case BM_VERT: {
		BMVert *v = element;
		if (v->e && v->e->head.type != BM_EDGE) {
			err |= 4;
		}
		break;
	}
	case BM_EDGE: {
		BMEdge *e = element;
		if (e->l && e->l->head.type != BM_LOOP)
			err |= 8;
		if (e->l && e->l->f->head.type != BM_FACE)
			err |= 16;
		if (e->dlink1.prev == NULL || e->dlink2.prev == NULL || e->dlink1.next == NULL || e->dlink2.next == NULL)
			err |= 32;
		if (e->l && (e->l->radial_next == NULL || e->l->radial_prev == NULL))
			err |= 64;
		if (e->l && e->l->f->len <= 0)
			err |= 128;
		break;
	}
	case BM_LOOP: {
		BMLoop *l = element, *l2;
		int i;

		if (l->f->head.type != BM_FACE)
			err |= 256;
		if (l->e->head.type != BM_EDGE)
			err |= 512;
		if (l->v->head.type !=  BM_VERT)
			err |= 1024;
		if (!BM_Vert_In_Edge(l->e, l->v)) {
			fprintf(stderr, "%s: fatal bmesh error (vert not in edge)! (bmesh internal error)\n", __func__);
			err |= 2048;
		}

		if (l->radial_next == NULL || l->radial_prev == NULL)
			err |= (1<<12);
		if (l->f->len <= 0)
			err |= (1<<13);

		/*validate boundary loop--invalid for hole loops, of course,
		  but we won't be allowing those for a while yet*/
		l2 = l;
		i = 0;
		do {
			if (i >= 9999999)
				break;

			i++;
			l2 = l2->next;
		} while (l2 != l);

		if (i != l->f->len || l2 != l)
			err |= (1<<14);

		if (!bmesh_radial_validate(bmesh_radial_length(l), l))
			err |= (1<<15);

		break;
	}
	case BM_FACE: {
		BMFace *f = element;
		BMLoop *l;
		int len=0;

		if (!f->loops.first)
			err |= (1<<16);
		l = bm_firstfaceloop(f);
		do {
			if (l->f != f) {
				fprintf(stderr, "%s: loop inside one face points to another! (bmesh internal error)\n", __func__);
				err |= (1<<17);
			}

			if (!l->e)
				err |= (1<<18);
			if (!l->v)
				err |= (1<<19);
			if (!BM_Vert_In_Edge(l->e, l->v) || !BM_Vert_In_Edge(l->e, l->next->v)) {
				err |= (1<<20);
			}

			if (!bmesh_radial_validate(bmesh_radial_length(l), l))
				err |= (1<<21);

			if (!bmesh_disk_count(l->v) || !bmesh_disk_count(l->next->v))
				err |= (1<<22);

			len++;
			l = l->next;
		} while (l != bm_firstfaceloop(f));

		if (len != f->len)
			err |= (1<<23);
	}
	}

	if (err) {
		bmesh_error();
	}

	return err;
}

static void bmesh_kill_loop(BMesh *bm, BMLoop *l) {
	bm->totloop--;
	if (l->head.data)
		CustomData_bmesh_free_block(&bm->ldata, &l->head.data);

	if (l->head.flags)
		BLI_mempool_free(bm->toolflagpool, l->head.flags);
	BLI_mempool_free(bm->lpool, l);
}

void BM_Kill_Face_Edges(BMesh *bm, BMFace *f) {
	BMEdge **edges = NULL;
	BLI_array_staticdeclare(edges, 256);
	BMLoop *l;
	int i;
	
	l = bm_firstfaceloop(f);
	do {
		BLI_array_append(edges, l->e);
		l = l->next;
	} while (l != bm_firstfaceloop(f));
	
	for (i=0; i<BLI_array_count(edges); i++) {
		BM_Kill_Edge(bm, edges[i]);
	}
	
	BLI_array_free(edges);
}

void BM_Kill_Face_Verts(BMesh *bm, BMFace *f) {
	BMVert**verts = NULL;
	BLI_array_staticdeclare(verts, 256);
	BMLoop *l;
	int i;
	
	l = bm_firstfaceloop(f);
	do {
		BLI_array_append(verts, l->v);
		l = l->next;
	} while (l != bm_firstfaceloop(f));
	
	for (i=0; i<BLI_array_count(verts); i++) {
		BM_Kill_Vert(bm, verts[i]);
	}
	
	BLI_array_free(verts);
}

void BM_Kill_Face(BMesh *bm, BMFace *f) {
	BMLoopList *ls, *lsnext;

	CHECK_ELEMENT(bm, f);

	for (ls=f->loops.first; ls; ls=lsnext) {
		BMLoop *l, *lnext;

		lsnext = ls->next;
		l = ls->first;
		do {
			lnext = l->next;

			bmesh_radial_remove_loop(l, l->e);
			bmesh_kill_loop(bm, l);

			l = lnext;
		} while (l != ls->first);
		
		BLI_mempool_free(bm->looplistpool, ls);
	}
	
	if (bm->act_face == f)
		bm->act_face = NULL;
	
	bm->totface--;
	BM_remove_selection(bm, f);
	if (f->head.data)
		CustomData_bmesh_free_block(&bm->pdata, &f->head.data);

	BLI_mempool_free(bm->toolflagpool, f->head.flags);

	BLI_mempool_free(bm->fpool, f);
}

void BM_Kill_Edge(BMesh *bm, BMEdge *e) {

	bmesh_disk_remove_edge(e, e->v1);
	bmesh_disk_remove_edge(e, e->v2);
		
	if (e->l) {
		BMLoop *l = e->l, *lnext, *startl=e->l;
			
		do {
			lnext = l->radial_next;
			if (lnext->f == l->f) {
				BM_Kill_Face(bm, l->f);
				break;					
			}
			
			BM_Kill_Face(bm, l->f);
		
			if (l == lnext)
				break;
			l = lnext;
		} while (l != startl);
	}
	
	bm->totedge--;
	BM_remove_selection(bm, e);
	if (e->head.data)
		CustomData_bmesh_free_block(&bm->edata, &e->head.data);

	BLI_mempool_free(bm->toolflagpool, e->head.flags);
	BLI_mempool_free(bm->epool, e);
}

void BM_Kill_Vert(BMesh *bm, BMVert *v) {
	if (v->e) {
		BMEdge *e, *nexte;
		
		e = v->e;
		while (v->e) {
			nexte=bmesh_disk_nextedge(e, v);
			BM_Kill_Edge(bm, e);
			e = nexte;
		}
	}

	bm->totvert--;
	BM_remove_selection(bm, v);
	if (v->head.data)
		CustomData_bmesh_free_block(&bm->vdata, &v->head.data);

	BLI_mempool_free(bm->toolflagpool, v->head.flags);
	BLI_mempool_free(bm->vpool, v);
}

/********** private disk and radial cycle functions ************/

/**
 *			bmesh_loop_reverse
 *
 *	FLIP FACE EULER
 *
 *	Changes the winding order of a face from CW to CCW or vice versa.
 *	This euler is a bit peculiar in compairson to others as it is its
 *	own inverse.
 *
 *	BMESH_TODO: reinsert validation code.
 *
 *  Returns -
 *	1 for success, 0 for failure.
 */

static int bmesh_loop_length(BMLoop *l)
{
	BMLoop *ol = l;
	int i = 0;

	do {
		l = l->next;
		i++;
	} while (l != ol);

	return i;
}

static int bmesh_loop_reverse_loop(BMesh *bm, BMFace *f, BMLoopList *lst){
	BMLoop *l = lst->first, *curloop, *oldprev, *oldnext;
	BMEdge **edar = NULL;
	MDisps *md;
	BLI_array_staticdeclare(edar, 64);
	int i, j, edok, len = 0, do_disps = CustomData_has_layer(&bm->ldata, CD_MDISPS);

	len = bmesh_loop_length(l);

	for(i=0, curloop = l; i< len; i++, curloop= curloop->next) {
		BMEdge *curedge = curloop->e;
		bmesh_radial_remove_loop(curloop, curedge);
		BLI_array_append(edar, curedge);
	}

	/*actually reverse the loop.*/
	for(i=0, curloop = l; i < len; i++){
		oldnext = curloop->next;
		oldprev = curloop->prev;
		curloop->next = oldprev;
		curloop->prev = oldnext;
		curloop = oldnext;
		
		if (do_disps) {
			float (*co)[3];
			int x, y, sides;
			
			md = CustomData_bmesh_get(&bm->ldata, curloop->head.data, CD_MDISPS);
			if (!md->totdisp || !md->disps)
				continue;
					
			sides=sqrt(md->totdisp);
			co = md->disps;
			
			for (x=0; x<sides; x++) {
				for (y=0; y<x; y++) {
					swap_v3_v3(co[y*sides+x], co[sides*x + y]);
				}
			}
		}
	}

	if(len == 2){ //two edged face
		//do some verification here!
		l->e = edar[1];
		l->next->e = edar[0];
	}
	else{
		for(i=0, curloop = l; i < len; i++, curloop = curloop->next) {
			edok = 0;
			for(j=0; j < len; j++){
				edok = bmesh_verts_in_edge(curloop->v, curloop->next->v, edar[j]);
				if(edok){
					curloop->e = edar[j];
					break;
				}
			}
		}
	}
	/*rebuild radial*/
	for(i=0, curloop = l; i < len; i++, curloop = curloop->next)
		bmesh_radial_append(curloop->e, curloop);

	/*validate radial*/
	for(i=0, curloop = l; i < len; i++, curloop = curloop->next) {
		CHECK_ELEMENT(bm, curloop);
		CHECK_ELEMENT(bm, curloop->e);
		CHECK_ELEMENT(bm, curloop->v);
		CHECK_ELEMENT(bm, curloop->f);
	}

	BLI_array_free(edar);

	CHECK_ELEMENT(bm, f);

	return 1;
}

int bmesh_loop_reverse(BMesh *bm, BMFace *f)
{
	return bmesh_loop_reverse_loop(bm, f, f->loops.first);
}

static void bmesh_systag_elements(BMesh *UNUSED(bm), void *veles, int tot, int flag)
{
	BMHeader **eles = veles;
	int i;

	for (i=0; i<tot; i++) {
		bmesh_api_setflag(eles[i], flag);
	}
}

static void bmesh_clear_systag_elements(BMesh *UNUSED(bm), void *veles, int tot, int flag)
{
	BMHeader **eles = veles;
	int i;

	for (i=0; i<tot; i++) {
		bmesh_api_clearflag(eles[i], flag);
	}
}

#define FACE_MARK	(1<<10)

static int count_flagged_radial(BMesh *bm, BMLoop *l, int flag)
{
	BMLoop *l2 = l;
	int i = 0, c=0;

	do {
		if (!l2) {
			bmesh_error();
			goto error;
		}
		
		i += bmesh_api_getflag(l2->f, flag) ? 1 : 0;
		l2 = bmesh_radial_nextloop(l2);
		if (c >= 800000) {
			bmesh_error();
			goto error;
		}
		c++;
	} while (l2 != l);

	return i;

error:
	BMO_RaiseError(bm, bm->currentop, BMERR_MESH_ERROR, NULL);
	return 0;
}

static int count_flagged_disk(BMVert *v, int flag)
{
	BMEdge *e = v->e;
	int i=0;

	if (!e)
		return 0;

	do {
		i += bmesh_api_getflag(e, flag) ? 1 : 0;
		e = bmesh_disk_nextedge(e, v);
	} while (e != v->e);

	return i;
}

static int disk_is_flagged(BMVert *v, int flag)
{
	BMEdge *e = v->e;

	if (!e)
		return 0;

	do {
		BMLoop *l = e->l;

		if (!l) {
			return 0;
		}
		
		if (bmesh_radial_length(l) == 1)
			return 0;
		
		do {
			if (!bmesh_api_getflag(l->f, flag))
				return 0;

			l = l->radial_next;
		} while (l != e->l);

		e = bmesh_disk_nextedge(e, v);
	} while (e != v->e);

	return 1;
}

/* Midlevel Topology Manipulation Functions */

/*
 * BM_Join_Faces
 *
 * Joins a collected group of faces into one. Only restriction on
 * the input data is that the faces must be connected to each other.
 *
 * If a pair of faces share multiple edges, the pair of
 * faces will be joined at every edge.
 *
 * Returns a pointer to the combined face.
 */
BMFace *BM_Join_Faces(BMesh *bm, BMFace **faces, int totface)
{
	BMFace *f, *newf;
	BMLoopList *lst;
	BMLoop *l;
	BMEdge **edges = NULL;
	BMEdge **deledges = NULL;
	BMVert **delverts = NULL;
	BLI_array_staticdeclare(edges, 64);
	BLI_array_staticdeclare(deledges, 64);
	BLI_array_staticdeclare(delverts, 64);
	BMVert *v1=NULL, *v2=NULL;
	ListBase holes = {NULL, NULL};
	const char *err = NULL;
	int i, tote=0;

	if (!totface) {
		bmesh_error();
		return NULL;
	}

	if (totface == 1)
		return faces[0];

	bmesh_systag_elements(bm, faces, totface, _FLAG_JF);

	for (i=0; i<totface; i++) {
		f = faces[i];
		l = bm_firstfaceloop(f);
		do {
			int rlen = count_flagged_radial(bm, l, _FLAG_JF);

			if (rlen > 2) {
				err = "Input faces do not form a contiguous manifold region";
				goto error;
			} else if (rlen == 1) {
				BLI_array_append(edges, l->e);

				if (!v1) {
					v1 = l->v;
					v2 = BM_OtherEdgeVert(l->e, l->v);
				}
				tote++;
			} else if (rlen == 2) {
				int d1, d2;

				d1 = disk_is_flagged(l->e->v1, _FLAG_JF);
				d2 = disk_is_flagged(l->e->v2, _FLAG_JF);

				if (!d1 && !d2 && !bmesh_api_getflag(l->e, _FLAG_JF)) {
					BLI_array_append(deledges, l->e);
					bmesh_api_setflag(l->e, _FLAG_JF);
				} else {
					if (d1 && !bmesh_api_getflag(l->e->v1, _FLAG_JF)) {
						BLI_array_append(delverts, l->e->v1);
						bmesh_api_setflag(l->e->v1, _FLAG_JF);
					}

					if (d2 && !bmesh_api_getflag(l->e->v2, _FLAG_JF)) {
						BLI_array_append(delverts, l->e->v2);
						bmesh_api_setflag(l->e->v2, _FLAG_JF);
					}
				}
			}

			l = l->next;
		} while (l != bm_firstfaceloop(f));

		for (lst=f->loops.first; lst; lst=lst->next) {
			if (lst == f->loops.first) continue;
			
			BLI_remlink(&f->loops, lst);
			BLI_addtail(&holes, lst);
		}
	}

	/*create region face*/
	newf = BM_Make_Ngon(bm, v1, v2, edges, tote, 0);
	if (!newf || BMO_HasError(bm)) {
		if (!BMO_HasError(bm)) 
			err = "Invalid boundary region to join faces";
		goto error;
	}

	/*copy over loop data*/
	l = bm_firstfaceloop(newf);
	do {
		BMLoop *l2 = l->radial_next;

		do {
			if (bmesh_api_getflag(l2->f, _FLAG_JF))
				break;
			l2 = l2->radial_next;
		} while (l2 != l);

		if (l2 != l) {
			/*I think this is correct?*/
			if (l2->v != l->v) {
				l2 = l2->next;
			}

			BM_Copy_Attributes(bm, bm, l2, l);
		}

		l = l->next;
	} while (l != bm_firstfaceloop(newf));
	
	BM_Copy_Attributes(bm, bm, faces[0], newf);

	/*add holes*/
	BLI_movelisttolist(&newf->loops, &holes);

	/*update loop face pointers*/
	for (lst=newf->loops.first; lst; lst=lst->next) {
		l = lst->first;
		do {
			l->f = newf;
			l = l->next;
		} while (l != lst->first);
	}

	bmesh_clear_systag_elements(bm, faces, totface, _FLAG_JF);
	bmesh_api_clearflag(newf, _FLAG_JF);

	/* handle multires data*/
	if (CustomData_has_layer(&bm->ldata, CD_MDISPS)) {
		l = bm_firstfaceloop(newf);
		do {
			for (i=0; i<totface; i++) {
				BM_loop_interp_multires(bm, l, faces[i]);
			}
			
			l = l->next;
		} while (l != bm_firstfaceloop(newf));
	}	

	/*delete old geometry*/
	for (i=0; i<BLI_array_count(deledges); i++) {
		BM_Kill_Edge(bm, deledges[i]);
	}

	for (i=0; i<BLI_array_count(delverts); i++) {
		BM_Kill_Vert(bm, delverts[i]);
	}
	
	BLI_array_free(edges);
	BLI_array_free(deledges);
	BLI_array_free(delverts);

	CHECK_ELEMENT(bm, newf);
	return newf;
error:
	bmesh_clear_systag_elements(bm, faces, totface, _FLAG_JF);
	BLI_array_free(edges);
	BLI_array_free(deledges);
	BLI_array_free(delverts);

	if (err) {
		BMO_RaiseError(bm, bm->currentop, BMERR_DISSOLVEFACES_FAILED, err);
	}
	return NULL;
}

static BMFace *bmesh_addpolylist(BMesh *bm, BMFace *UNUSED(example)) {
	BMFace *f;
	BMLoopList *lst;

	f = BLI_mempool_calloc(bm->fpool);
	lst = BLI_mempool_calloc(bm->looplistpool);

	f->head.type = BM_FACE;
	BLI_addtail(&f->loops, lst);
	bm->totface++;

	/*allocate flags*/
	f->head.flags = BLI_mempool_calloc(bm->toolflagpool);

	CustomData_bmesh_set_default(&bm->pdata, &f->head.data);

	f->len = 0;
	f->totbounds = 1;

	return (BMFace*) f;
}

/**
 *			bmesh_SFME
 *
 *	SPLIT FACE MAKE EDGE:
 *
 *	Takes as input two vertices in a single face. An edge is created which divides the original face
 *	into two distinct regions. One of the regions is assigned to the original face and it is closed off.
 *	The second region has a new face assigned to it.
 *
 *	Examples:
 *
 *     Before:               After:
 *	 ----------           ----------
 *	 |        |           |        |
 *	 |        |           |   f1   |
 *	v1   f1   v2          v1======v2
 *	 |        |           |   f2   |
 *	 |        |           |        |
 *	 ----------           ----------
 *
 *	Note that the input vertices can be part of the same edge. This will
 *  result in a two edged face. This is desirable for advanced construction
 *  tools and particularly essential for edge bevel. Because of this it is
 *  up to the caller to decide what to do with the extra edge.
 *
 *  If holes is NULL, then both faces will lose
 *  all holes from the original face.  Also, you cannot split between
 *  a hole vert and a boundary vert; that case is handled by higher-
 *  level wrapping functions (when holes are fully implemented, anyway).
 *
 *  Note that holes represents which holes goes to the new face, and of
 *  course this requires removing them from the exitsing face first, since
 *  you cannot have linked list links inside multiple lists.
 *
 *	Returns -
 *  A BMFace pointer
 */
BMFace *bmesh_sfme(BMesh *bm, BMFace *f, BMVert *v1, BMVert *v2,
				   BMLoop **rl, ListBase *holes)
{

	BMFace *f2;
	BMLoop *v1loop = NULL, *v2loop = NULL, *curloop, *f1loop=NULL, *f2loop=NULL;
	BMEdge *e;
	BMLoopList *lst, *lst2;
	int i, len, f1len, f2len;

	/*verify that v1 and v2 are in face.*/
	len = f->len;
	for(i = 0, curloop = bm_firstfaceloop(f); i < len; i++, curloop = curloop->next) {
		if(curloop->v == v1) v1loop = curloop;
		else if(curloop->v == v2) v2loop = curloop;
	}

	if(!v1loop || !v2loop) return NULL;

	/*allocate new edge between v1 and v2*/
	e = BM_Make_Edge(bm, v1, v2, NULL, 0);

	f2 = bmesh_addpolylist(bm,f);
	f1loop = bmesh_create_loop(bm,v2,e,f,v2loop);
	f2loop = bmesh_create_loop(bm,v1,e,f2,v1loop);

	f1loop->prev = v2loop->prev;
	f2loop->prev = v1loop->prev;
	v2loop->prev->next = f1loop;
	v1loop->prev->next = f2loop;

	f1loop->next = v1loop;
	f2loop->next = v2loop;
	v1loop->prev = f1loop;
	v2loop->prev = f2loop;

	lst = f->loops.first;
	lst2 = f2->loops.first;

	lst2->first = lst2->last = f2loop;
	lst->first = lst->last = f1loop;

	/*validate both loops*/
	/*I dont know how many loops are supposed to be in each face at this point! FIXME!*/

	/*go through all of f2's loops and make sure they point to it properly.*/
	curloop = lst2->first;
	f2len = 0;
	do {
		curloop->f = f2;

		curloop = curloop->next;
		f2len++;
	} while (curloop != lst2->first);

	/*link up the new loops into the new edges radial*/
	bmesh_radial_append(e, f1loop);
	bmesh_radial_append(e, f2loop);

	f2->len = f2len;

	f1len = 0;
	curloop = lst->first;
	do {
		f1len++;
		curloop = curloop->next;
	} while (curloop != lst->first);

	f->len = f1len;

	if(rl) *rl = f2loop;

	if (holes) {
		BLI_movelisttolist(&f2->loops, holes);
	} else {
		/*this code is not significant until holes actually work ;) */
		//printf("warning: call to split face euler without holes argument; holes will be tossed.\n");
		for (lst=f->loops.last; lst != f->loops.first; lst=lst2) {
			lst2 = lst->prev;
			BLI_mempool_free(bm->looplistpool, lst);
		}
	}

	CHECK_ELEMENT(bm, e);
	CHECK_ELEMENT(bm, f);
	CHECK_ELEMENT(bm, f2);
	
	return f2;
}

/**
 *			bmesh_SEMV
 *
 *	SPLIT EDGE MAKE VERT:
 *	Takes a given edge and splits it into two, creating a new vert.
 *
 *
 *		Before:	OV---------TV
 *		After:	OV----NV---TV
 *
 *  Returns -
 *	BMVert pointer.
 *
*/

BMVert *bmesh_semv(BMesh *bm, BMVert *tv, BMEdge *e, BMEdge **re){
	BMLoop *nextl;
	BMEdge *ne;
	BMVert *nv, *ov;
	int i, edok, valence1=0, valence2=0;

	if(bmesh_vert_in_edge(e,tv) == 0) return NULL;
	ov = bmesh_edge_getothervert(e,tv);

	/*count valence of v1*/
	valence1 = bmesh_disk_count(ov);

	/*count valence of v2*/
	valence2 = bmesh_disk_count(tv);

	nv = BM_Make_Vert(bm, tv->co, tv);
	ne = BM_Make_Edge(bm, nv, tv, e, 0);

	bmesh_disk_remove_edge(ne, tv);
	bmesh_disk_remove_edge(ne, nv);

	/*remove e from v2's disk cycle*/
	bmesh_disk_remove_edge(e, tv);

	/*swap out tv for nv in e*/
	bmesh_edge_swapverts(e, tv, nv);

	/*add e to nv's disk cycle*/
	bmesh_disk_append_edge(e, nv);

	/*add ne to nv's disk cycle*/
	bmesh_disk_append_edge(ne, nv);

	/*add ne to tv's disk cycle*/
	bmesh_disk_append_edge(ne, tv);

	/*verify disk cycles*/
	edok = bmesh_disk_validate(valence1, ov->e, ov);
	if(!edok) bmesh_error();
	edok = bmesh_disk_validate(valence2, tv->e, tv);
	if(!edok) bmesh_error();
	edok = bmesh_disk_validate(2, nv->e, nv);
	if(!edok) bmesh_error();

	/*Split the radial cycle if present*/
	nextl = e->l;
	e->l = NULL;
	if(nextl) {
		BMLoop *nl, *l;
		int radlen = bmesh_radial_length(nextl);
		int first1=0, first2=0;

		/*Take the next loop. Remove it from radial. Split it. Append to appropriate radials.*/
		while(nextl) {
			l=nextl;
			l->f->len++;
			nextl = nextl!=nextl->radial_next ? nextl->radial_next : NULL;
			bmesh_radial_remove_loop(l, NULL);

			nl = bmesh_create_loop(bm,NULL,NULL,l->f,l);
			nl->prev = l;
			nl->next = (l->next);
			nl->prev->next = nl;
			nl->next->prev = nl;
			nl->v = nv;

			/*assign the correct edge to the correct loop*/
			if(bmesh_verts_in_edge(nl->v, nl->next->v, e)) {
				nl->e = e;
				l->e = ne;

				/*append l into ne's rad cycle*/
				if(!first1) {
					first1 = 1;
					l->radial_next = l->radial_prev = NULL;
				}

				if(!first2) {
					first2 = 1;
					l->radial_next = l->radial_prev = NULL;
				}
				
				bmesh_radial_append(nl->e, nl);
				bmesh_radial_append(l->e, l);
			}
			else if(bmesh_verts_in_edge(nl->v, nl->next->v, ne)){
				nl->e = ne;
				l->e = e;

				/*append l into ne's rad cycle*/
				if(!first1) {
					first1 = 1;
					l->radial_next = l->radial_prev = NULL;
				}

				if(!first2) {
					first2 = 1;
					l->radial_next = l->radial_prev = NULL;
				}

				bmesh_radial_append(nl->e, nl);
				bmesh_radial_append(l->e, l);
			}

		}

		/*verify length of radial cycle*/
		edok = bmesh_radial_validate(radlen, e->l);
		if(!edok) bmesh_error();
		edok = bmesh_radial_validate(radlen, ne->l);
		if(!edok) bmesh_error();

		/*verify loop->v and loop->next->v pointers for e*/
		for(i=0,l=e->l; i < radlen; i++, l = l->radial_next){
			if(!(l->e == e)) bmesh_error();
			//if(!(l->radial_next == l)) bmesh_error();
			if(l->prev->e != ne && l->next->e != ne) bmesh_error();
			edok = bmesh_verts_in_edge(l->v, l->next->v, e);
			if(!edok) bmesh_error();
			if(l->v == l->next->v) bmesh_error();
			if(l->e == l->next->e) bmesh_error();

			/*verify loop cycle for kloop->f*/
			CHECK_ELEMENT(bm, l);
			CHECK_ELEMENT(bm, l->v);
			CHECK_ELEMENT(bm, l->e);
			CHECK_ELEMENT(bm, l->f);
		}
		/*verify loop->v and loop->next->v pointers for ne*/
		for(i=0,l=ne->l; i < radlen; i++, l = l->radial_next){
			if(!(l->e == ne)) bmesh_error();
			//if(!(l->radial_next == l)) bmesh_error();
			if( l->prev->e != e && l->next->e != e) bmesh_error();
			edok = bmesh_verts_in_edge(l->v, l->next->v, ne);
			if(!edok) bmesh_error();
			if(l->v == l->next->v) bmesh_error();
			if(l->e == l->next->e) bmesh_error();

			CHECK_ELEMENT(bm, l);
			CHECK_ELEMENT(bm, l->v);
			CHECK_ELEMENT(bm, l->e);
			CHECK_ELEMENT(bm, l->f);
		}
	}

	CHECK_ELEMENT(bm, ne);
	CHECK_ELEMENT(bm, nv);
	CHECK_ELEMENT(bm, ov);
	CHECK_ELEMENT(bm, e);
	CHECK_ELEMENT(bm, tv);

	if(re) *re = ne;
	return nv;
}

/**
 *			bmesh_JEKV
 *
 *	JOIN EDGE KILL VERT:
 *	Takes a an edge and pointer to one of its vertices and collapses
 *	the edge on that vertex.
 *	
 *	Before:    OE      KE
 *             	 ------- -------
 *               |     ||      |
 *		OV     KV      TV
 *
 *
 *   After:             OE      
 *             	 ---------------
 *               |             |
 *		OV             TV
 *
 *
 *	Restrictions:
 *	KV is a vertex that must have a valance of exactly two. Furthermore
 *  both edges in KV's disk cycle (OE and KE) must be unique (no double
 *  edges).
 *
 *	It should also be noted that this euler has the possibility of creating
 *	faces with just 2 edges. It is up to the caller to decide what to do with
 *  these faces.
 *
 *  Returns -
 *	1 for success, 0 for failure.
 */
int bmesh_jekv(BMesh *bm, BMEdge *ke, BMVert *kv)
{
	BMEdge *oe;
	BMVert *ov, *tv;
	BMLoop *killoop, *l;
	int len,radlen=0, halt = 0, i, valence1, valence2,edok;
	BMLoop **loops = NULL;
	BLI_array_staticdeclare(loops, 256);

	if(bmesh_vert_in_edge(ke,kv) == 0) return 0;
	len = bmesh_disk_count(kv);
	
	if(len == 2){
		oe = bmesh_disk_nextedge(ke, kv);
		tv = bmesh_edge_getothervert(ke, kv);
		ov = bmesh_edge_getothervert(oe, kv);		
		halt = bmesh_verts_in_edge(kv, tv, oe); /*check for double edges*/
		
		if(halt) return 0;
		else{
			/*For verification later, count valence of ov and tv*/
			valence1 = bmesh_disk_count(ov);
			valence2 = bmesh_disk_count(tv);
			
			/*remove oe from kv's disk cycle*/
			bmesh_disk_remove_edge(oe,kv);
			/*relink oe->kv to be oe->tv*/
			bmesh_edge_swapverts(oe, kv, tv);
			/*append oe to tv's disk cycle*/
			bmesh_disk_append_edge(oe, tv);
			/*remove ke from tv's disk cycle*/
			bmesh_disk_remove_edge(ke, tv);
		
			/*deal with radial cycle of ke*/
			radlen = bmesh_radial_length(ke->l);
			if(ke->l){
				/*first step, fix the neighboring loops of all loops in ke's radial cycle*/
				for(i=0,killoop = ke->l; i<radlen; i++, killoop = bmesh_radial_nextloop(killoop)){
					/*relink loops and fix vertex pointer*/
					if( killoop->next->v == kv ) killoop->next->v = tv;

					killoop->next->prev = killoop->prev;
					killoop->prev->next = killoop->next;
					if (bm_firstfaceloop(killoop->f) == killoop)
						bm_firstfaceloop(killoop->f) = killoop->next;
					killoop->next = NULL;
					killoop->prev = NULL;

					/*fix len attribute of face*/
					killoop->f->len--;
				}
				/*second step, remove all the hanging loops attached to ke*/
				killoop = ke->l;
				radlen = bmesh_radial_length(ke->l);
				/*this should be wrapped into a bme_free_radial function to be used by bmesh_KF as well...*/
				for (i=0;i<radlen;i++) {
					BLI_array_growone(loops);
					loops[BLI_array_count(loops)-1] = killoop;
					killoop = bmesh_radial_nextloop(killoop);
				}
				for (i=0;i<radlen;i++) {
					bm->totloop--;
					BLI_mempool_free(bm->lpool, loops[i]);
				}
				/*Validate radial cycle of oe*/
				edok = bmesh_radial_validate(radlen,oe->l);
				if(!edok) bmesh_error();
			}

			/*deallocate edge*/
			BM_remove_selection(bm, ke);
			BLI_mempool_free(bm->toolflagpool, ke->head.flags);
			BLI_mempool_free(bm->epool, ke);
			bm->totedge--;
			/*deallocate vertex*/
			BM_remove_selection(bm, kv);
			BLI_mempool_free(bm->toolflagpool, kv->head.flags);
			BLI_mempool_free(bm->vpool, kv);
			bm->totvert--;

			/*Validate disk cycle lengths of ov,tv are unchanged*/
			edok = bmesh_disk_validate(valence1, ov->e, ov);
			if(!edok) bmesh_error();
			edok = bmesh_disk_validate(valence2, tv->e, tv);
			if(!edok) bmesh_error();

			/*Validate loop cycle of all faces attached to oe*/
			for(i=0,l = oe->l; i<radlen; i++, l = bmesh_radial_nextloop(l)){
				if(l->e != oe) bmesh_error();
				edok = bmesh_verts_in_edge(l->v, l->next->v, oe);
				if(!edok) bmesh_error();
				edok = bmesh_loop_validate(l->f);
				if(!edok) bmesh_error();

				CHECK_ELEMENT(bm, l);
				CHECK_ELEMENT(bm, l->v);
				CHECK_ELEMENT(bm, l->e);
				CHECK_ELEMENT(bm, l->f);
			}

			CHECK_ELEMENT(bm, ov);
			CHECK_ELEMENT(bm, tv);
			CHECK_ELEMENT(bm, oe);

			return 1;
		}
	}
	return 0;
}

/**
 *			bmesh_JFKE
 *
 *	JOIN FACE KILL EDGE:
 *	
 *	Takes two faces joined by a single 2-manifold edge and fuses them togather.
 *	The edge shared by the faces must not be connected to any other edges which have
 *	Both faces in its radial cycle
 *
 *	Examples:
 *	
 *        A                   B
 *	 ----------           ----------
 *	 |        |           |        | 
 *	 |   f1   |           |   f1   |
 *	v1========v2 = Ok!    v1==V2==v3 == Wrong!
 *	 |   f2   |           |   f2   |
 *	 |        |           |        |
 *	 ----------           ---------- 
 *
 *	In the example A, faces f1 and f2 are joined by a single edge, and the euler can safely be used.
 *	In example B however, f1 and f2 are joined by multiple edges and will produce an error. The caller
 *	in this case should call bmesh_JEKV on the extra edges before attempting to fuse f1 and f2.
 *
 *	Also note that the order of arguments decides whether or not certain per-face attributes are present
 *	in the resultant face. For instance vertex winding, material index, smooth flags, ect are inherited
 *	from f1, not f2.
 *
 *  Returns -
 *	A BMFace pointer
*/
BMFace *bmesh_jfke(BMesh *bm, BMFace *f1, BMFace *f2, BMEdge *e)
{
	BMLoop *curloop, *f1loop=NULL, *f2loop=NULL;
	int loopok = 0, newlen = 0,i, f1len=0, f2len=0, radlen=0, edok, shared;
	BMIter iter;

	/*can't join a face to itself*/
	if(f1 == f2) return NULL;
	/*verify that e is in both f1 and f2*/
	f1len = f1->len;
	f2len = f2->len;
	BM_ITER(curloop, &iter, bm, BM_LOOPS_OF_FACE, f1) {
		if(curloop->e == e){ 
			f1loop = curloop;
			break;
		}
	}
	BM_ITER(curloop, &iter, bm, BM_LOOPS_OF_FACE, f2) {
		if(curloop->e == e){
			f2loop = curloop;
			break;
		}
	}
	if (!(f1loop && f2loop)) return NULL;
	
	/*validate that edge is 2-manifold edge*/
	radlen = bmesh_radial_length(f1loop);
	if(radlen != 2) return NULL;

	/*validate direction of f2's loop cycle is compatible.*/
	if(f1loop->v == f2loop->v) return NULL;

	/*
		validate that for each face, each vertex has another edge in its disk cycle that is 
		not e, and not shared.
	*/
	if(bmesh_radial_find_face(f1loop->next->e,f2)) return NULL;
	if(bmesh_radial_find_face(f1loop->prev->e,f2)) return NULL;
	if(bmesh_radial_find_face(f2loop->next->e,f1)) return NULL;
	if(bmesh_radial_find_face(f2loop->prev->e,f1)) return NULL;
	
	/*validate only one shared edge*/
	shared = BM_Face_Sharededges(f1,f2);
	if(shared > 1) return NULL;

	/*validate no internal joins*/
	for(i=0, curloop = bm_firstfaceloop(f1); i < f1len; i++, curloop = curloop->next)
		bmesh_api_setindex(curloop->v, 0);
	for(i=0, curloop = bm_firstfaceloop(f2); i < f2len; i++, curloop = curloop->next)
		bmesh_api_setindex(curloop->v, 0);

	for(i=0, curloop = bm_firstfaceloop(f1); i < f1len; i++, curloop = curloop->next) {
		if (curloop != f1loop)
			bmesh_api_setindex(curloop->v, bmesh_api_getindex(curloop->v) + 1);
	}
	for(i=0, curloop = bm_firstfaceloop(f2); i < f2len; i++, curloop = curloop->next) {
		if (curloop != f2loop)
			bmesh_api_setindex(curloop->v, bmesh_api_getindex(curloop->v) + 1);
	}

	for(i=0, curloop = bm_firstfaceloop(f1); i < f1len; i++, curloop = curloop->next) {
		if (bmesh_api_getindex(curloop->v) > 1)
			return NULL;
	}
	
	for(i=0, curloop = bm_firstfaceloop(f2); i < f2len; i++, curloop = curloop->next) {
		if (bmesh_api_getindex(curloop->v) > 1)
			return NULL;
	}

	/*join the two loops*/
	f1loop->prev->next = f2loop->next;
	f2loop->next->prev = f1loop->prev;
	
	f1loop->next->prev = f2loop->prev;
	f2loop->prev->next = f1loop->next;
	
	/*if f1loop was baseloop, make f1loop->next the base.*/
	if(bm_firstfaceloop(f1) == f1loop)
		bm_firstfaceloop(f1) = f1loop->next;

	/*increase length of f1*/
	f1->len += (f2->len - 2);

	/*make sure each loop points to the proper face*/
	newlen = f1->len;
	for(i = 0, curloop = bm_firstfaceloop(f1); i < newlen; i++, curloop = curloop->next)
		curloop->f = f1;
	
	/*remove edge from the disk cycle of its two vertices.*/
	bmesh_disk_remove_edge(f1loop->e, f1loop->e->v1);
	bmesh_disk_remove_edge(f1loop->e, f1loop->e->v2);
	
	/*deallocate edge and its two loops as well as f2*/
	BLI_mempool_free(bm->toolflagpool, f1loop->e->head.flags);
	BLI_mempool_free(bm->epool, f1loop->e);
	bm->totedge--;
	BLI_mempool_free(bm->lpool, f1loop);
	bm->totloop--;
	BLI_mempool_free(bm->lpool, f2loop);
	bm->totloop--;
	BLI_mempool_free(bm->toolflagpool, f2->head.flags);
	BLI_mempool_free(bm->fpool, f2);
	bm->totface--;

	CHECK_ELEMENT(bm, f1);

	/*validate the new loop cycle*/
	edok = bmesh_loop_validate(f1);
	if(!edok) bmesh_error();
	
	return f1;
}

/*
 * BMESH SPLICE VERT
 *
 * merges two verts into one (v into vtarget).
 */
static int bmesh_splicevert(BMesh *bm, BMVert *v, BMVert *vtarget)
{
	BMEdge *e;
	BMLoop *l;
	BMIter liter;

	/* verts already spliced */
	if (v == vtarget) {
		return 0;
	}

	/* retarget all the loops of v to vtarget */
	BM_ITER(l, &liter, bm, BM_LOOPS_OF_VERT, v) {
		l->v = vtarget;
	}

	/* move all the edges from v's disk to vtarget's disk */
	e = v->e;
	while (e != NULL) {
		bmesh_disk_remove_edge(e, v);
		bmesh_edge_swapverts(e, v, vtarget);
		bmesh_disk_append_edge(e, vtarget);
		e = v->e;
	}

	/* v is unused now, and can be killed */
	BM_Kill_Vert(bm, v);

	return 1;
}

/* BMESH CUT VERT
 *
 * cut all disjoint fans that meet at a vertex, making a unique
 * vertex for each region. returns an array of all resulting
 * vertices.
 */
static int bmesh_cutvert(BMesh *bm, BMVert *v, BMVert ***vout, int *len)
{
	BMEdge **stack = NULL;
	BLI_array_declare(stack);
	BMVert **verts = NULL;
	GHash *visithash;
	BMIter eiter, liter;
	BMLoop *l;
	BMEdge *e;
	int i, maxindex;
	BMLoop *nl;

	visithash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "bmesh_cutvert visithash");

	maxindex = 0;
	BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, v) {
		if (BLI_ghash_haskey(visithash, e)) {
			continue;
		}

		/* Prime the stack with this unvisited edge */
		BLI_array_append(stack, e);

		/* Walk over edges that:
		   1) have v as one of the vertices
		   2) are connected to e through face loop cycles 
		   assigning a unique index to that group of edges */
		while (e = BLI_array_pop(stack)) {
			BLI_ghash_insert(visithash, e, SET_INT_IN_POINTER(maxindex));
			BM_SetIndex(e, maxindex);

			BM_ITER(l, &liter, bm, BM_LOOPS_OF_EDGE, e) {
				nl = (l->v == v) ? l->prev : l->next;
				if (!BLI_ghash_haskey(visithash, nl->e)) {
					BLI_array_append(stack, nl->e);
				}
			}
		}

		maxindex++;
	}

	/* Make enough verts to split v for each group */
	verts = MEM_callocN(sizeof(BMVert *) * maxindex, "bmesh_cutvert");
	verts[0] = v;
	for (i = 1; i < maxindex; i++) {
		verts[i] = BM_Make_Vert(bm, v->co, v);
	}

	/* Replace v with the new verts in each group */
	BM_ITER(l, &liter, bm, BM_LOOPS_OF_VERT, v) {
		i = GET_INT_FROM_POINTER(BLI_ghash_lookup(visithash, l->e));
		if (i == 0) {
			continue;
		}

		if (l->v == v) {
			l->v = verts[i];
		}
		if (l->next->v == v) {
			l->next->v = verts[i];
		}
	}

	BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, v) {
		i = GET_INT_FROM_POINTER(BLI_ghash_lookup(visithash, e));
		if (i == 0) {
			continue;
		}

		BM_ITER(l, &liter, bm, BM_LOOPS_OF_EDGE, e) {
			if (l->v == v) {
				l->v = verts[i];
			}
			if (l->next->v == v) {
				l->next->v = verts[i];
			}
		}

		if (e->v1 == v || e->v2 == v) {
			bmesh_disk_remove_edge(e, v);
			bmesh_edge_swapverts(e, v, verts[i]);
			bmesh_disk_append_edge(e, verts[i]);
		}
	}

	BLI_ghash_free(visithash, NULL, NULL);
	BLI_array_free(stack);

	*vout = verts;
	*len = maxindex;

	return 1;
}

/* BMESH SPLICE EDGE
 *
 * splice two unique edges which share the same two vertices into one edge.
 *
 * edges must already have the same vertices
 */
static int bmesh_spliceedge(BMesh *bm, BMEdge *e, BMEdge *etarget)
{
	BMLoop *l;

	if (!BM_Vert_In_Edge(e, etarget->v1) || !BM_Vert_In_Edge(e, etarget->v2)) {
		/* not the same vertices can't splice */
		return 0;
	}

	while (e->l) {
		l = e->l;
		bmesh_radial_remove_loop(l, e);
		bmesh_radial_append(etarget, l);
	}

	BM_Kill_Edge(bm, e);

	return 1;
}

/*
 * BMESH CUT EDGE
 *
 * Cuts a single edge into two edge: the original edge and
 * a new edge that has only "cutl" in its radial.
 *
 * Does nothing if cutl is already the only loop in the
 * edge radial.
 */
static int bmesh_cutedge(BMesh *bm, BMEdge *e, BMLoop *cutl)
{
	BMEdge *ne;

	BLI_assert(cutl->e == e);
	BLI_assert(e->l);
	
	if (bmesh_radial_length(e->l) < 2) {
		/* no cut required */
		return 1;
	}

	if (cutl == e->l) {
		e->l = cutl->radial_next;
	}

	ne = BM_Make_Edge(bm, e->v1, e->v2, e, 0);
	bmesh_radial_remove_loop(cutl, e);
	bmesh_radial_append(ne, cutl);
	cutl->e = ne;

	return 1;
}

/*
 * BMESH UNGLUE REGION MAKE VERT
 *
 * Disconnects a face from its vertex fan at loop sl.
 */
static BMVert *bmesh_urmv_loop(BMesh *bm, BMLoop *sl)
{
	BMVert **vtar;
	int len, i;
	BMVert *nv = NULL;
	BMVert *sv = sl->v;

	/* peel the face from the edge radials on both sides of the
	   loop vert, disconnecting the face from its fan */
	bmesh_cutedge(bm, sl->e, sl);
	bmesh_cutedge(bm, sl->prev->e, sl->prev);

	if (bmesh_disk_count(sv) == 2) {
		/* If there are still only two edges out of sv, then
		   this whole URMV was just a no-op, so exit now. */
		return sv;
	}

	/* Update the disk start, so that v->e points to an edge
	   not touching the split loop. This is so that bmesh_cutvert
	   will leave the original sv on some *other* fan (not the
	   one-face fan that holds the unglue face). */
	while (sv->e == sl->e || sv->e == sl->prev->e) {
		sv->e = bmesh_disk_nextedge(sv->e, sv);
	}

	/* Split all fans connected to the vert, duplicating it for
	   each fans. */
	bmesh_cutvert(bm, sv, &vtar, &len);

	/* There should have been at least two fans cut apart here,
	   otherwise the early exit would have kicked in. */
	BLI_assert(len >= 2);

	nv = sl->v;

	/* Desired result here is that a new vert should always be
	   created for the unglue face. This is so we can glue any
	   extras back into the original vert. */
	BLI_assert(nv != sv);
	BLI_assert(sv == vtar[0]);

	/* If there are more than two verts as a result, glue together
	   all the verts except the one this URMV intended to create */
	if (len > 2) {
		for (i = 0; i < len; i++) {
			if (vtar[i] == nv) {
				break;
			}
		}

		if (i != len) {
			/* Swap the single vert that was needed for the
			   unglue into the last array slot */
			SWAP(BMVert *, vtar[i], vtar[len - 1]);

			/* And then glue the rest back together */
			for (i = 1; i < len - 1; i++) {
				bmesh_splicevert(bm, vtar[i], vtar[0]);
			}
		}
	}

	MEM_freeN(vtar);

	return nv;
}

/*
 * BMESH UNGLUE REGION MAKE VERT
 *
 * Disconnects sf from the vertex fan at sv
 */
BMVert *bmesh_urmv(BMesh *bm, BMFace *sf, BMVert *sv)
{
	BMLoop *hl, *sl;

	hl = sl = bm_firstfaceloop(sf);
	do {
		if (sl->v == sv) break;
		sl = sl->next;
	} while (sl != hl);
		
	if (sl->v != sv) {
		/* sv is not part of sf */
		return NULL;
	}

	return bmesh_urmv_loop(bm, sl);
}
