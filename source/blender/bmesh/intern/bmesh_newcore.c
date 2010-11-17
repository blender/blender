#include <limits.h>

#include "BLI_math_vector.h"

#include "BKE_utildefines.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"

#include "BLI_blenlib.h"
#include "BLI_mempool.h"
#include "BLI_ghash.h"
#include "BLI_array.h"

#include "DNA_listBase.h"

#include "bmesh_class.h"

#include "bmesh_iterators.h"
#include "bmesh_private.h"

/*note: first three layers, base through adjacency, do *not* use the
  LayerType system for speed/memory cache efficiency and whatnot*/

static void inherit_vert(BMesh *bm, BMBaseVert *v)
{
	int i;
	
	for (i=0; i<bm->totlayer; i++) {
		void *ldata = BMSC_GETSELF(bm, v, bm->layers+i);
		bm->layers[i].type->new_vert(bm, v, ldata);
	}
	
	if (bm->baselevel >= LAYER_ADJ) {
		/*not sure anything is needed here either, verts generally come first before all else*/
	}   
}

static void inherit_edge(BMesh *bm, BMBaseEdge *ebase)
{
	int i;
	
	if (bm->baselevel >= LAYER_ADJ) {
		BMEdge *e = (BMEdge*)ebase;

		bmesh_disk_append_edge(e, e->v1);
		bmesh_disk_append_edge(e, e->v2);
	}

	for (i=0; i<bm->totlayer; i++) {
		void *ldata = BMSC_GETSELF(bm, ebase, bm->layers+i);
		bm->layers[i].type->new_edge(bm, ebase, ldata);
	}
}

static void inherit_loop(BMesh *bm, BMBaseLoop *l, BMBaseFace *f)
{
	int i;

	for (i=0; i<bm->totlayer; i++) {
		void *self;

		if (!bm->layers[i].type->new_loop) continue;

		self = BMSC_GETSELF(bm, l, &bm->layers[i]);
		bm->layers[i].type->new_loop(bm, (BMBaseLoop*)l, self, (BMBaseFace*)f);
	}
}

/*
static void inherit_face(BMesh *bm, BMBaseFace *basef)
{
	int i;
	
	for (i=0; i<bm->totlayer; i++) {
		void *fdata = BMSC_GETSELF(bm, basef, bm->layers+i);
		bm->layers[i].type->new_face(bm, basef, fdata);
	}
}
*/

void BM_SubClass(BMesh *bm, BMLayerType *type)
{
}

BMVert *BM_Make_Vert(BMesh *bm, float co[3], struct BMVert *example) {
	BMBaseVert *v = BLI_mempool_calloc(bm->vpool);
	
	bm->totvert += 1;

	v->head.type = BM_VERT;
	if (bm->svpool)
		v->head.layerdata = BLI_mempool_calloc(bm->svpool);

	if (co) VECCOPY(v->co, co);
	
	if (bm->baselevel >= LAYER_TOOL) {
		BMBaseVert *tv = (BMBaseVert*)v;

		/*allocate flags*/
		tv->head.flags = BLI_mempool_calloc(bm->toolflagpool);
	}

	CustomData_bmesh_set_default(&bm->vdata, &v->head.data);
	
	inherit_vert(bm, v);
	
	if (example)
		BM_Copy_Attributes(bm, bm, (BMVert*)example, (BMVert*)v);

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

BMEdge *BM_Make_Edge(BMesh *bm, BMVert *v1, BMVert *v2, BMEdge *example, int nodouble) {
	BMBaseEdge *e;
	
	if (nodouble && (e=(BMBaseEdge*)BM_Edge_Exist(v1, v2)))
		return (BMEdge*)e;
	
	e = BLI_mempool_calloc(bm->epool);
	bm->totedge += 1;
	e->head.type = BM_EDGE;
	if (bm->sepool)
		e->head.layerdata = BLI_mempool_calloc(bm->sepool);
	
	if (bm->baselevel >= LAYER_TOOL) {
		BMBaseEdge *te = (BMBaseEdge*)e;
		
		/*allocate flags*/
		te->head.flags = BLI_mempool_calloc(bm->toolflagpool);
	}

	e->v1 = (BMBaseVert*) v1;
	e->v2 = (BMBaseVert*) v2;
	
	CustomData_bmesh_set_default(&bm->edata, &e->head.data);
	
	inherit_edge(bm, e);

	if (example)
		BM_Copy_Attributes(bm, bm, (BMEdge*)example, (BMEdge*)e);
	
	CHECK_ELEMENT(bm, e);

	return (BMEdge*) e;
}

static BMLoop *bmesh_create_loop(BMesh *bm, BMVert *v, BMEdge *e, BMFace *f, BMLoop *example){
	BMLoop *l=NULL;
	int i;

	l = BLI_mempool_calloc(bm->lpool);
	l->next = l->prev = NULL;
	l->v = v;
	l->e = e;
	if (bm->baselevel >= LAYER_ADJ) {
		l->f = f;
		l->radial_next = l->radial_prev = NULL;
	}
	l->head.data = NULL;
	l->head.type = BM_LOOP;

	bm->totloop++;

	if (bm->slpool)
		l->head.layerdata = BLI_mempool_calloc(bm->slpool);

	inherit_loop(bm, l, f);

	if(example)
		CustomData_bmesh_copy_data(&bm->ldata, &bm->ldata, example->head.data, &l->head.data);
	else
		CustomData_bmesh_set_default(&bm->ldata, &l->head.data);

	return l;
}

BMLoop *BM_Add_FaceBoundary(BMesh *bm, BMFace *f, BMVert *startv, BMEdge *starte) {
	BMBaseLoopList *lst = BLI_mempool_calloc(bm->looplistpool);
	BMLoop *l = (BMLoop*)bmesh_create_loop(bm, startv, starte, (BMBaseFace*)f, NULL);
	int i;
	
	bmesh_radial_append(starte, l);

	lst->first = lst->last = (BMBaseLoop*)l;
	BLI_addtail(&f->loops, lst);
	
	if (bm->baselevel >= LAYER_ADJ)
		l->f = f;
	
	for (i=0; i<bm->totlayer; i++) {
		void *self = BMSC_GETSELF(bm, l, (&bm->layers[i]));
		
		bm->layers[i].type->new_loop(bm, (BMBaseLoop*)l, self, (BMBaseFace*)f);
	}
	
	return l;	
}

BMFace *BM_Make_Face(BMesh *bm, BMVert **verts, BMEdge **edges, int len) {
	BMBaseFace *f;
	BMBaseLoop *l, *startl, *lastl;
	int i;
	
	if (len == 0) {
		/*just return NULL for now*/
		return NULL;
	}
	
	f = BLI_mempool_calloc(bm->fpool);
	bm->totface += 1;
	f->head.type = BM_FACE;

	if (bm->sfpool)
		f->head.layerdata = BLI_mempool_calloc(bm->sfpool);

	startl = lastl = (BMBaseLoop*) BM_Add_FaceBoundary(bm, (BMFace*)f, verts[0], edges[0]);
	
	startl->v = (BMBaseVert*) verts[0];
	startl->e = (BMBaseEdge*) edges[0];
	for (i=1; i<len; i++) {
		l = bmesh_create_loop(bm, verts[i], edges[i], f, edges[i]->l);
		
		if (bm->baselevel >= LAYER_ADJ) {
			BMLoop *bl = (BMLoop*)l;
			bl->f = (BMFace*) f;
			bmesh_radial_append(edges[i], bl);
		}

		l->prev = lastl;
		lastl->next = l;
		lastl = l;
	}
	
	if (bm->baselevel >= LAYER_TOOL) {
		BMBaseFace *tf = (BMBaseFace*) f;
		
		/*allocate flags*/
		tf->head.flags = BLI_mempool_calloc(bm->toolflagpool);
	}

	CustomData_bmesh_set_default(&bm->pdata, &f->head.data);
	
	startl->prev = lastl;
	lastl->next = startl;
	
	f->len = len;
	f->totbounds = 0;
	
	for (i=0; i<bm->totlayer; i++) {
		void *self;
		
		if (!bm->layers[i].type->new_face) continue;
		
		self = BMSC_GETSELF(bm, f, &bm->layers[i]);
		bm->layers[i].type->new_face(bm, f, self);
	}
	
	CHECK_ELEMENT(bm, f);

	return (BMFace*) f;
}

int bmesh_check_element(BMesh *bm, void *element, int type) {
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
			printf("eek!! fatal bmesh error! evil!\n");
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
				printf("yeek!! loop inside one face points to another!\n");
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

void bmesh_kill_loop(BMesh *bm, BMLoop *l) {
	int i;
	
	for (i=0; i<bm->totlayer; i++) {
		bm->layers[i].type->free_loop(bm, (BMBaseLoop*)l);
	}

	bm->totloop--;
	if (l->head.data)
		CustomData_bmesh_free_block(&bm->ldata, &l->head.data);

	if (bm->baselevel >= LAYER_TOOL && l->head.flags) {
		BLI_mempool_free(bm->toolflagpool, l->head.flags);
	}

	BLI_mempool_free(bm->lpool, l);
}

void BM_Kill_Face(BMesh *bm, BMFace *f) {
	BMBaseLoopList *ls, *lsnext;
	int i;
	
	CHECK_ELEMENT(bm, f);

	for (i=0; i<bm->totlayer; i++) {
		bm->layers[i].type->free_face(bm, (BMBaseFace*)f);
	}

	for (ls=f->loops.first; ls; ls=lsnext) {
		BMBaseLoop *l, *lnext;

		lsnext = ls->next;
		l = ls->first;
		do {
			lnext = l->next;

			if (bm->baselevel >= LAYER_ADJ)
				bmesh_radial_remove_loop((BMLoop*)l, l->e);
			bmesh_kill_loop(bm, (BMLoop*)l);

			l = lnext;
		} while (l != ls->first);
		
		BLI_mempool_free(bm->looplistpool, ls);
	}
	
	bm->totface--;
	BM_remove_selection(bm, f);
	if (f->head.data)
		CustomData_bmesh_free_block(&bm->pdata, &f->head.data);

	if (bm->baselevel >= LAYER_TOOL) {
		BLI_mempool_free(bm->toolflagpool, f->head.flags);
	}

	BLI_mempool_free(bm->fpool, f);
}

void BM_Kill_Edge(BMesh *bm, BMEdge *e) {
	int i;
	
	for (i=0; i<bm->totlayer; i++) {
		bm->layers[i].type->free_edge(bm, (BMBaseEdge*)e);
	}
	
	if (bm->baselevel >= LAYER_ADJ) {
		bmesh_disk_remove_edge(e, e->v1);
		bmesh_disk_remove_edge(e, e->v2);
		
		if (e->l) {
			BMLoop *l = e->l, *lnext, *startl=e->l;
			
			do {
				lnext = l->radial_next;
				BM_Kill_Face(bm, l->f);
			
				if (l == lnext)
					break;
				l = lnext;
			} while (l != startl);
		}
	}
	
	bm->totedge--;
	BM_remove_selection(bm, e);
	if (e->head.data)
		CustomData_bmesh_free_block(&bm->edata, &e->head.data);

	if (bm->baselevel >= LAYER_TOOL) {
		BLI_mempool_free(bm->toolflagpool, e->head.flags);
	}
	
	BLI_mempool_free(bm->epool, e);
}

void BM_Kill_Vert(BMesh *bm, BMVert *v) {
	int i;

	for (i=0; i<bm->totlayer; i++) {
		bm->layers[i].type->free_vert(bm, (BMBaseVert*)v);
	}
	
	if (bm->baselevel >= LAYER_ADJ) {
		if (v->e) {
			BMEdge *e, *nexte;
			
			e = v->e;
			while (v->e) {
				nexte=bmesh_disk_nextedge(e, v);

				BM_Kill_Edge(bm, (BMEdge*)e);
				e = nexte;
			}
		}
	}    

	bm->totvert--;
	BM_remove_selection(bm, v);
	if (v->head.data)
		CustomData_bmesh_free_block(&bm->vdata, &v->head.data);

	if (bm->baselevel >= LAYER_TOOL) {
		BLI_mempool_free(bm->toolflagpool, v->head.flags);
	}

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
 *	TODO: reinsert validation code.
 *
 *  Returns -
 *	1 for success, 0 for failure.
 */

int bmesh_loop_length(BMLoop *l)
{
	BMLoop *ol = l;
	int i = 0;

	do {
		l = l->next;
		i++;
	} while (l != ol);

	return i;
}

int bmesh_loop_reverse_loop(BMesh *bm, BMFace *f, BMLoopList *lst){
	BMLoop *l = lst->first, *curloop, *oldprev, *oldnext;
	BMEdge *staticedar[64], **edar;
	int i, j, edok, len = 0;

	len = bmesh_loop_length(l);
	if(len >= 64){
		edar = MEM_callocN(sizeof(BMEdge *)* len, "BM Edge pointer array");
	} else {
		edar = staticedar;
	}

	for(i=0, curloop = l; i< len; i++, curloop= ((BMLoop*)(curloop->next)) ){
		curloop->e->head.eflag1 = 0;
		curloop->e->head.eflag2 = bmesh_radial_length(curloop);
		bmesh_radial_remove_loop(curloop, curloop->e);
		/*in case of border edges we HAVE to zero out curloop->radial Next/Prev*/
		curloop->radial_next = curloop->radial_prev = NULL;
		edar[i] = curloop->e;
	}

	/*actually reverse the loop.*/
	for(i=0, curloop = l; i < len; i++){
		oldnext = ((BMLoop*)(curloop->next));
		oldprev = ((BMLoop*)(curloop->prev));
		curloop->next = (BMLoop*)oldprev;
		curloop->prev = (BMLoop*)oldnext;
		curloop = oldnext;
	}

	if(len == 2){ //two edged face
		//do some verification here!
		l->e = edar[1];
		((BMLoop*)(l->next))->e = edar[0];
	}
	else{
		for(i=0, curloop = l; i < len; i++, curloop = ((BMLoop*)(curloop->next)) ){
			edok = 0;
			for(j=0; j < len; j++){
				edok = bmesh_verts_in_edge(curloop->v, ((BMLoop*)(curloop->next))->v, edar[j]);
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

	if (edar != staticedar)
		MEM_freeN(edar);

	CHECK_ELEMENT(bm, f);

	return 1;
}

int bmesh_loop_reverse(BMesh *bm, BMFace *f)
{
	return bmesh_loop_reverse_loop(bm, f, f->loops.first);
}

void bmesh_systag_elements(BMesh *bm, void *veles, int tot, int flag)
{
	BMHeader **eles = veles;
	int i;

	for (i=0; i<tot; i++) {
		bmesh_api_setflag(eles[i], flag);
	}
}

void bmesh_clear_systag_elements(BMesh *bm, void *veles, int tot, int flag)
{
	BMHeader **eles = veles;
	int i;

	for (i=0; i<tot; i++) {
		bmesh_api_clearflag(eles[i], flag);
	}
}

#define FACE_MARK	(1<<10)

static int count_flagged_radial(BMLoop *l, int flag)
{
	BMLoop *l2 = l;
	int i = 0;

	do {
		if (!l2) {
			bmesh_error();
			return 0;
		}
		
		i += bmesh_api_getflag(l2->f, flag) ? 1 : 0;
		l2 = bmesh_radial_nextloop(l2);
		if (i >= 800000) {
			bmesh_error();
			return 0;
		}
	} while (l2 != l);

	return i;
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

/*joins a collected group of faces into one.  only restriction on
  the input data is that the faces must be connected to each other.*/
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
	char *err = NULL;
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
			int rlen = count_flagged_radial(l, _FLAG_JF);

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
	if (!newf) {
		err = "Invalid boundary region to join faces";
		goto error;
	}

	for (i=0; i<bm->totlayer; i++) {
		BMFace *faces2[2] = {newf, NULL};

		if (bm->layers[i].type->faces_from_faces)
			bm->layers[i].type->faces_from_faces(bm, faces, totface, faces2, 1);
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
	addlisttolist(&newf->loops, &holes);

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

static BMFace *bmesh_addpolylist(BMesh *bm, BMFace *example) {
	BMBaseFace *f;
	BMBaseLoopList *lst;

	f = BLI_mempool_calloc(bm->fpool);
	lst = BLI_mempool_calloc(bm->looplistpool);

	f->head.type = BM_FACE;
	BLI_addtail(&f->loops, lst);
	bm->totface++;

	if (bm->baselevel >= LAYER_TOOL) {
		BMBaseFace *tf = (BMBaseFace*)f;

		/*allocate flags*/
		tf->head.flags = BLI_mempool_calloc(bm->toolflagpool);
	}

	if (bm->sfpool)
		f->head.layerdata = BLI_mempool_calloc(f->head.layerdata);

	CustomData_bmesh_set_default(&bm->pdata, &f->head.data);

	f->len = 0;
	f->totbounds = 1;

	/*okkaay not sure what to do here
	for (i=0; i<bm->totlayer; i++) {
		void *self;

		if (!bm->layers[i].type->new_face) continue;

		self = BMSC_GETSELF(bm, f, &bm->layers[i]);
		bm->layers[i].type->new_face(bm, f, self);
	}
	*/

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
 *	 |		  |           |        |
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
		addlisttolist(&f2->loops, holes);
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
	int i, edok, valance1=0, valance2=0;

	if(bmesh_vert_in_edge(e,tv) == 0) return NULL;
	ov = bmesh_edge_getothervert(e,tv);

	/*count valance of v1*/
	valance1 = bmesh_disk_count(ov);

	/*count valance of v2*/
	valance2 = bmesh_disk_count(tv);

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
	edok = bmesh_disk_validate(valance1, ov->e, ov);
	if(!edok) bmesh_error();
	edok = bmesh_disk_validate(valance2, tv->e, tv);
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
			else if(bmesh_verts_in_edge(nl->v,((BMLoop*)(nl->next))->v,ne)){
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
			if( ((BMLoop*)(l->prev))->e != ne && ((BMLoop*)(l->next))->e != ne) bmesh_error();
			edok = bmesh_verts_in_edge(l->v, ((BMLoop*)(l->next))->v, e);
			if(!edok) bmesh_error();
			if(l->v == ((BMLoop*)(l->next))->v) bmesh_error();
			if(l->e == ((BMLoop*)(l->next))->e) bmesh_error();

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
			if( ((BMLoop*)(l->prev))->e != e && ((BMLoop*)(l->next))->e != e) bmesh_error();
			edok = bmesh_verts_in_edge(l->v, ((BMLoop*)(l->next))->v, ne);
			if(!edok) bmesh_error();
			if(l->v == ((BMLoop*)(l->next))->v) bmesh_error();
			if(l->e == ((BMLoop*)(l->next))->e) bmesh_error();

			CHECK_ELEMENT(bm, l);
			CHECK_ELEMENT(bm, l->v);
			CHECK_ELEMENT(bm, l->e);
			CHECK_ELEMENT(bm, l->f);
		}
	}

	CHECK_ELEMENT(bm, ne);
	CHECK_ELEMENT(bm, nv);
	CHECK_ELEMENT(bm, e);
	CHECK_ELEMENT(bm, tv);

	if(re) *re = ne;
	return nv;
}
