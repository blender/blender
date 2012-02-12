/*some of this may come back, such as split face or split edge, if necassary for speed*/

#if 0
/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/intern/bmesh_eulers.c
 *  \ingroup bmesh
 *
 * BM Euler construction API.
 */

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"

#include "BKE_customdata.h"
#include "BKE_utildefines.h"

#include "bmesh.h"
#include "bmesh_private.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"

/*********************************************************
 *                    "Euler API"                        *
 *                                                       *
 *                                                       *
 *	 Primitive construction operators for mesh tools.    *
 *                                                       *
 **********************************************************/


/*
	The functions in this file represent the 'primitive' or 'atomic' operators that
	mesh tools use to manipulate the topology of the structure.* The purpose of these
	functions is to provide a trusted set of operators to manipulate the mesh topology
	and which can also be combined together like building blocks to create more 
	sophisticated tools. It needs to be stressed that NO manipulation of an existing 
	mesh structure should be done outside of these functions.
	
	In the BM system, each euler is named by an ancronym which describes what it actually does.
	Furthermore each Euler has a logical inverse. An important design criteria of all Eulers is that
	through a Euler's logical inverse you can 'undo' an operation. (Special note should
	be taken of bmesh_loop_reverse, which is its own inverse).
		
	bmesh_MF/KF: Make Face and Kill Face
	bmesh_ME/KE: Make Edge and Kill Edge
	bmesh_MV/KV: Make Vert and Kill Vert
	bmesh_SEMV/JEKV: Split Edge, Make Vert and Join Edge, Kill Vert
	bmesh_SFME/JFKE: Split Face, Make Edge and Join Face, Kill Edge
	bmesh_loop_reverse: Reverse a Polygon's loop cycle. (used for flip normals for one)
	
	Using a combination of these eleven eulers any non-manifold modelling operation can be achieved.
	Each Euler operator has a detailed explanation of what is does in the comments preceding its 
	code. 

   *The term "Euler Operator" is actually a misnomer when referring to a non-manifold 
    data structure. Its use is in keeping with the convention established by others.

	BMESH_TODO:
	-Make seperate 'debug levels' of validation
	-Add in the UnglueFaceRegionMakeVert and GlueFaceRegionKillVert eulers.

	NOTE:
	-The functions in this file are notoriously difficult to debug and even understand sometimes.
	 better code comments would be nice....

*/


/*MAKE Eulers*/

/**
 *			bmesh_MV
 *
 *	MAKE VERT EULER:
 *	
 *	Makes a single loose vertex.
 *
 *	Returns -
 *	A BMVert pointer.
 */

BMVert *bmesh_mv(BMesh *bm, const float vec[3])
{
	BMVert *v = bmesh_addvertlist(bm, NULL);	
	copy_v3_v3(v->co,vec);
	return v;
}

/**
 *			bmesh_ME
 *
 *	MAKE EDGE EULER:
 *	
 *	Makes a single wire edge between two vertices.
 *	If the caller does not want there to be duplicate
 *	edges between the vertices, it is up to them to check 
 *	for this condition beforehand.
 *
 *	Returns -
 *	A BMEdge pointer.
 */

BMEdge *bmesh_me(BMesh *bm, BMVert *v1, BMVert *v2)
{
	BMEdge *e=NULL;
	BMNode *d1=NULL, *d2=NULL;
	int valance1=0, valance2=0, edok;
	
	/*edge must be between two distinct vertices...*/
	if(v1 == v2) return NULL;
	
	#ifndef bmesh_FASTEULER
	/*count valance of v1*/
	if(v1->e) {
		d1 = bmesh_disk_getpointer(v1->e,v1);
		if(d1) valance1 = bmesh_cycle_length(d1);
		else bmesh_error();
	}
	if(v2->e) {
		d2 = bmesh_disk_getpointer(v2->e,v2);
		if(d2) valance2 = bmesh_cycle_length(d2);
		else bmesh_error();
	}
	#endif
	
	/*go ahead and add*/
	e = bmesh_addedgelist(bm, v1, v2, NULL);
	bmesh_disk_append_edge(e, e->v1);
	bmesh_disk_append_edge(e, e->v2);
	
	#ifndef bmesh_FASTEULER
	/*verify disk cycle lengths*/
	d1 = bmesh_disk_getpointer(e, e->v1);
	edok = bmesh_cycle_validate(valance1+1, d1);
	if(!edok) bmesh_error();
	d2 = bmesh_disk_getpointer(e, e->v2);
	edok = bmesh_cycle_validate(valance2+1, d2);
	if(!edok) bmesh_error();
	
	/*verify that edge actually made it into the cycle*/
	edok = bmesh_disk_hasedge(v1, e);
	if(!edok) bmesh_error();
	edok = bmesh_disk_hasedge(v2, e);
	if(!edok) bmesh_error();
	#endif
	return e;
}



/**
 *			bmesh_MF
 *
 *	MAKE FACE EULER:
 *	Takes a list of edge pointers which form a closed loop and makes a face 
 *  from them. The first edge in elist is considered to be the start of the 
 *	polygon, and v1 and v2 are its vertices and determine the winding of the face 
 *  Other than the first edge, no other assumptions are made about the order of edges
 *  in the elist array. To verify that it is a single closed loop and derive the correct 
 *  order a simple series of verifications is done and all elements are visited.
 *		
 *  Returns -
 *	A BMFace pointer
 */

#define MF_CANDIDATE	1
#define MF_VISITED		2
#define MF_TAKEN		4 

BMFace *bmesh_mf(BMesh *bm, BMVert *v1, BMVert *v2, BMEdge **elist, int len)
{
	BMFace *f = NULL;
	BMEdge *curedge;
	BMVert *curvert, *tv, **vlist;
	int i, j, done, cont, edok;
	
	if(len < 2) return NULL;
	
	/*make sure that v1 and v2 are in elist[0]*/
	//if(bmesh_verts_in_edge(v1,v2,elist[0]) == 0) 
	//	return NULL;
	
	/*clear euler flags*/
	for(i=0;i<len;i++) {
		BMNode *diskbase;
		BMEdge *curedge;
		BMVert *v1;
		int j;

		for (j=0; j<2; j++) {
			int a, len=0;
			
			v1 = j ? elist[i]->v2 : elist[i]->v1;
			diskbase = bmesh_disk_getpointer(v1->e, v1);
			len = bmesh_cycle_length(diskbase);

			for(a=0,curedge=v1->e;a<len;a++,curedge = bmesh_disk_nextedge(curedge,v1)) {
				curedge->head.eflag1 = curedge->head.eflag2 = 0;
			}
		}
	}

	for(i=0;i<len;i++) {
		elist[i]->head.eflag1 |= MF_CANDIDATE;
		
		/*if elist[i] has a loop, count its radial length*/
		if(elist[i]->loop) elist[i]->head.eflag2 = bmesh_cycle_length(&(elist[i]->l->radial));
		else elist[i]->head.eflag2 = 0;
	}
	
	/*	For each vertex in each edge, it must have exactly two MF_CANDIDATE edges attached to it
		Note that this does not gauruntee that face is a single closed loop. At best it gauruntees
		that elist contains a finite number of seperate closed loops.
	*/
//	for(i=0; i<len; i++) {
//		edok = bmesh_disk_count_edgeflag(elist[i]->v1, MF_CANDIDATE, 0);
//		if(edok != 2) return NULL;
//		edok = bmesh_disk_count_edgeflag(elist[i]->v2, MF_CANDIDATE, 0);
//		if(edok != 2) return NULL;
//	}
	
	/*set start edge, start vert and target vert for our loop traversal*/
	curedge = elist[0];
	tv = v1;
	curvert = v2;
	
	if(bm->vtarlen < len) {
		if (bm->vtar) MEM_freeN(bm->vtar);
		bm->vtar = MEM_callocN(sizeof(BMVert *)* len, "BM Vert pointer array");
		bm->vtarlen = len;
	}
	/*insert tv into vlist since its the first vertex in face*/
	
	i=0;
	vlist=bm->vtar;
	vlist[i] = tv;

	/*	Basic procedure: Starting with curv we find the edge in it's disk cycle which hasn't 
		been visited yet. When we do, we put curv in a linked list and find the next MF_CANDIDATE
		edge, loop until we find TV. We know TV is reachable because of test we did earlier.
	*/
	done=0;
	while(!done) {
		/*add curvert to vlist*/
		/*insert some error cheking here for overflows*/
		i++;
		vlist[i] = curvert;
		
		/*mark curedge as visited*/
		curedge->head.eflag1 |= MF_VISITED;
		
		/*find next edge and vert*/
		curedge = bmesh_disk_next_edgeflag(curedge, curvert, MF_CANDIDATE, 0);
		curvert = bmesh_edge_getothervert(curedge, curvert);
		if(curvert == tv) {
			curedge->head.eflag1 |= MF_VISITED;
			done=1;
		}
	}

	/*	Verify that all edges have been visited It's possible that we did reach tv 
		from sv, but that several unconnected loops were passed in via elist.
	*/
	cont=1;
//	for(i=0; i<len; i++) {
//		if((elist[i]->head.eflag1 & MF_VISITED) == 0) cont = 0;
//	}
	
	/*if we get this far, its ok to allocate the face and add the loops*/
	if(cont) {
		BMLoop *l;
		BMEdge *e;
		f = bmesh_addpolylist(bm, NULL);
		f->len = len;
		for(i=0;i<len;i++) {
			curvert = vlist[i];
			l = bmesh_create_loop(bm,curvert,NULL,f,NULL);
			if(!(f->loopbase)) f->lbase = l;
			bmesh_cycle_append(f->lbase, l);
		}
		
		/*take care of edge pointers and radial cycle*/
		for(i=0, l = f->loopbase; i<len; i++, l= l->next) {
			e = NULL;
			if(l == f->loopbase) e = elist[0]; /*first edge*/
			
			else {/*search elist for others*/
				for(j=1; j<len; j++) {
					edok = bmesh_verts_in_edge(l->v, ((l->next))->v, elist[j]);
					if(edok) {
						e = elist[j];
						break;
					}
				}
			}
			l->e = e; /*set pointer*/
			bmesh_radial_append(e, l); /*append into radial*/
		}

		f->len = len;
		
		/*Validation Loop cycle*/
		edok = bmesh_cycle_validate(len, f->lbase);
		if(!edok) bmesh_error();
		for(i=0, l = f->loopbase; i<len; i++, l=((l->next))) {
			/*validate loop vert pointers*/
			edok = bmesh_verts_in_edge(l->v, ((l->next))->v, l->e);
			if(!edok) bmesh_error();
			/*validate the radial cycle of each edge*/
			edok = bmesh_cycle_length(&(l->radial));
			if(edok != (l->e->head.eflag2 + 1)) bmesh_error();
		}
	}

	for(i=0;i<len;i++) elist[i]->head.eflag1=elist[i]->head.eflag2 = 0;
	return f;
}

/* KILL Eulers */

/**
 *			bmesh_KV
 *
 *	KILL VERT EULER:
 *	
 *	Kills a single loose vertex.
 *
 *	Returns -
 *	1 for success, 0 for failure.
 */

int bmesh_kv(BMesh *bm, BMVert *v)
{
	if(v->e == NULL) {
		if (BM_elem_flag_test(v, BM_ELEM_SELECT)) bm->totvertsel--;

		BLI_remlink(&(bm->verts), &(v->head));
		bmesh_free_vert(bm,v);
		return 1;
	}
	return 0;
}

/**
 *			bmesh_KE
 *
 *	KILL EDGE EULER:
 *	
 *	Kills a wire edge.
 *
 *	Returns -
 *	1 for success, 0 for failure.
 */

int bmesh_ke(BMesh *bm, BMEdge *e)
{
	int edok;
	
	/*Make sure that no faces!*/
	if(e->l == NULL) {
		bmesh_disk_remove_edge(e, e->v1);
		bmesh_disk_remove_edge(e, e->v2);
		
		/*verify that edge out of disk*/
		edok = bmesh_disk_hasedge(e->v1, e);
		if(edok) bmesh_error();
		edok = bmesh_disk_hasedge(e->v2, e);
		if(edok) bmesh_error();
		
		/*remove and deallocate*/
		if (BM_elem_flag_test(e, BM_ELEM_SELECT)) bm->totedgesel--;
		BLI_remlink(&(bm->edges), &(e->head));
		bmesh_free_edge(bm, e);
		return 1;
	}
	return 0;
}

/**
 *			bmesh_KF
 *
 *	KILL FACE EULER:
 *	
 *	The logical inverse of bmesh_MF.
 *	Kills a face and removes each of its loops from the radial that it belongs to.
 *
 *  Returns -
 *	1 for success, 0 for failure.
*/

int bmesh_kf(BMesh *bm, BMFace *bply)
{
	BMLoop *newbase,*oldbase, *curloop;
	int i,len=0;
	
	/*add validation to make sure that radial cycle is cleaned up ok*/
	/*deal with radial cycle first*/
	len = bmesh_cycle_length(bply->lbase);
	for(i=0, curloop=bply->loopbase; i < len; i++, curloop = ((curloop->next)))
		bmesh_radial_remove_loop(curloop, curloop->e);
	
	/*now deallocate the editloops*/
	for(i=0; i < len; i++) {
		newbase = ((bply->lbase->next));
		oldbase = bply->lbase;
		bmesh_cycle_remove(oldbase, oldbase);
		bmesh_free_loop(bm, oldbase);
		bply->loopbase = newbase;
	}
	
	if (BM_elem_flag_test(bply, BM_ELEM_SELECT)) bm->totfacesel--;
	BLI_remlink(&(bm->polys), &(bply->head));
	bmesh_free_poly(bm, bply);
	return 1;
}

/*SPLIT Eulers*/

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

BMVert *bmesh_semv(BMesh *bm, BMVert *tv, BMEdge *e, BMEdge **re)
{
	BMVert *nv, *ov;
	BMNode *diskbase;
	BMEdge *ne;
	int i, edok, valance1=0, valance2=0;
	
	if(bmesh_vert_in_edge(e,tv) == 0) return NULL;
	ov = bmesh_edge_getothervert(e,tv);
	//v2 = tv;

	/*count valance of v1*/
	diskbase = bmesh_disk_getpointer(e, ov);
	valance1 = bmesh_cycle_length(diskbase);
	/*count valance of v2*/
	diskbase = bmesh_disk_getpointer(e, tv);
	valance2 = bmesh_cycle_length(diskbase);
	
	nv = bmesh_addvertlist(bm, tv);
	ne = bmesh_addedgelist(bm, nv, tv, e);
	
	//e->v2 = nv;
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
	diskbase = bmesh_disk_getpointer(ov->e,ov);
	edok = bmesh_cycle_validate(valance1, diskbase);
	if(!edok) bmesh_error();
	diskbase = bmesh_disk_getpointer(tv->e,tv);
	edok = bmesh_cycle_validate(valance2, diskbase);
	if(!edok) bmesh_error();
	diskbase = bmesh_disk_getpointer(nv->e,nv);
	edok = bmesh_cycle_validate(2, diskbase);
	if(!edok) bmesh_error();
	
	/*Split the radial cycle if present*/
	if(e->l) {
		BMLoop *nl,*l;
		BMNode *radEBase=NULL, *radNEBase=NULL;
		int radlen = bmesh_cycle_length(&(e->l->radial));
		/*Take the next loop. Remove it from radial. Split it. Append to appropriate radials.*/
		while(e->l) {
			l=e->l;
			l->f->len++;
			bmesh_radial_remove_loop(l,e);
			
			nl = bmesh_create_loop(bm,NULL,NULL,l->f,l);
			nl->prev = (BMHeader*)l;
			nl->next = (BMHeader*)(l->next);
			nl->prev->next = (BMHeader*)nl;
			nl->next->prev = (BMHeader*)nl;
			nl->v = nv;
			
			/*assign the correct edge to the correct loop*/
			if(bmesh_verts_in_edge(nl->v, ((nl->next))->v, e)) {
				nl->e = e;
				l->e = ne;
				
				/*append l into ne's rad cycle*/
				if(!radNEBase) {
					radNEBase = &(l->radial);
					radNEBase->next = NULL;
					radNEBase->prev = NULL;
				}
				
				if(!radEBase) {
					radEBase = &(nl->radial);
					radEBase->next = NULL;
					radEBase->prev = NULL;
				}
				
				bmesh_cycle_append(radEBase,&(nl->radial));
				bmesh_cycle_append(radNEBase,&(l->radial));
					
			}
			else if(bmesh_verts_in_edge(nl->v,((nl->next))->v,ne)) {
				nl->e = ne;
				l->e = e;
				
				if(!radNEBase) {
					radNEBase = &(nl->radial);
					radNEBase->next = NULL;
					radNEBase->prev = NULL;
				}
				if(!radEBase) {
					radEBase = &(l->radial);
					radEBase->next = NULL;
					radEBase->prev = NULL;
				}
				bmesh_cycle_append(radEBase,&(l->radial));
				bmesh_cycle_append(radNEBase,&(nl->radial));
			}
					
		}
		
		e->l = radEBase->data;
		ne->l = radNEBase->data;
		
		/*verify length of radial cycle*/
		edok = bmesh_cycle_validate(radlen,&(e->l->radial));
		if(!edok) bmesh_error();
		edok = bmesh_cycle_validate(radlen,&(ne->l->radial));
		if(!edok) bmesh_error();
		
		/*verify loop->v and loop->next->v pointers for e*/
		for(i=0,l=e->l; i < radlen; i++, l = l->radial_next) {
			if(!(l->e == e)) bmesh_error();
			if(!(l->radial.data == l)) bmesh_error();
			if( ((l->prev))->e != ne && ((l->next))->e != ne) bmesh_error();
			edok = bmesh_verts_in_edge(l->v, ((l->next))->v, e);
			if(!edok) bmesh_error();
			if(l->v == ((l->next))->v) bmesh_error();
			if(l->e == ((l->next))->e) bmesh_error();
			/*verify loop cycle for kloop->f*/
			edok = bmesh_cycle_validate(l->f->len, l->f->lbase);
			if(!edok) bmesh_error();
		}
		/*verify loop->v and loop->next->v pointers for ne*/
		for(i=0,l=ne->l; i < radlen; i++, l = l->radial_next) {
			if(!(l->e == ne)) bmesh_error();
			if(!(l->radial.data == l)) bmesh_error();
			if( ((l->prev))->e != e && ((l->next))->e != e) bmesh_error();
			edok = bmesh_verts_in_edge(l->v, ((l->next))->v, ne);
			if(!edok) bmesh_error();
			if(l->v == ((l->next))->v) bmesh_error();
			if(l->e == ((l->next))->e) bmesh_error();
			/*verify loop cycle for kloop->f. Redundant*/
			edok = bmesh_cycle_validate(l->f->len, l->f->lbase);
			if(!edok) bmesh_error();
		}
	}
	
	if(re) *re = ne;
	return nv;
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
 *	Note that the input vertices can be part of the same edge. This will result in a two edged face.
 *  This is desirable for advanced construction tools and particularly essential for edge bevel. Because
 *  of this it is up to the caller to decide what to do with the extra edge.
 *
 *  Note that the tesselator abuses eflag2 while using this euler! (don't ever ever do this....)
 *
 *	Returns -
 *  A BMFace pointer
 */
BMFace *bmesh_sfme(BMesh *bm, BMFace *f, BMVert *v1, BMVert *v2, BMLoop **rl)
{

	BMFace *f2;
	BMLoop *v1loop = NULL, *v2loop = NULL, *curloop, *f1loop=NULL, *f2loop=NULL;
	BMEdge *e;
	int i, len, f1len, f2len;
	
	
	/*verify that v1 and v2 are in face.*/
	len = bmesh_cycle_length(f->lbase);
	for(i = 0, curloop = f->loopbase; i < len; i++, curloop = ((curloop->next)) ) {
		if(curloop->v == v1) v1loop = curloop;
		else if(curloop->v == v2) v2loop = curloop;
	}
	
	if(!v1loop || !v2loop) return NULL;
	
	/*allocate new edge between v1 and v2*/
	e = bmesh_addedgelist(bm, v1, v2,NULL);
	bmesh_disk_append_edge(e, v1);
	bmesh_disk_append_edge(e, v2);
	
	f2 = bmesh_addpolylist(bm,f);
	f1loop = bmesh_create_loop(bm,v2,e,f,v2loop);
	f2loop = bmesh_create_loop(bm,v1,e,f2,v1loop);
	
	f1loop->prev = v2loop->prev;
	f2loop->prev = v1loop->prev;
	v2loop->prev->next = (BMHeader*)f1loop;
	v1loop->prev->next = (BMHeader*)f2loop;
	
	f1loop->next = (BMHeader*)v1loop;
	f2loop->next = (BMHeader*)v2loop;
	v1loop->prev = (BMHeader*)f1loop;
	v2loop->prev = (BMHeader*)f2loop;
	
	f2->loopbase = f2loop;
	f->loopbase = f1loop;
	
	/*validate both loops*/
	/*I dont know how many loops are supposed to be in each face at this point! FIXME!*/
	
	/*go through all of f2's loops and make sure they point to it properly.*/
	f2len = bmesh_cycle_length(f2->lbase);
	for(i=0, curloop = f2->loopbase; i < f2len; i++, curloop = ((curloop->next)) ) curloop->f = f2;
	
	/*link up the new loops into the new edges radial*/
	bmesh_radial_append(e, f1loop);
	bmesh_radial_append(e, f2loop);
	
	
	f2->len = f2len;
	
	f1len = bmesh_cycle_length(f->lbase);
	f->len = f1len;
	
	if(rl) *rl = f2loop;
	return f2;
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
	BMNode *diskbase;
	BMLoop *killoop,*nextl;
	int len,radlen=0, halt = 0, i, valance1, valance2,edok;
	
	if(bmesh_vert_in_edge(ke,kv) == 0) return 0;
	diskbase = bmesh_disk_getpointer(kv->e, kv);
	len = bmesh_cycle_length(diskbase);
	
	if(len == 2) {
		oe = bmesh_disk_nextedge(ke, kv);
		tv = bmesh_edge_getothervert(ke, kv);
		ov = bmesh_edge_getothervert(oe, kv);		
		halt = bmesh_verts_in_edge(kv, tv, oe); //check for double edges
		
		if(halt) return 0;
		else {
			
			/*For verification later, count valance of ov and tv*/
			diskbase = bmesh_disk_getpointer(ov->e, ov);
			valance1 = bmesh_cycle_length(diskbase);
			diskbase = bmesh_disk_getpointer(tv->e, tv);
			valance2 = bmesh_cycle_length(diskbase);
			
			/*remove oe from kv's disk cycle*/
			bmesh_disk_remove_edge(oe,kv);
			/*relink oe->kv to be oe->tv*/
			bmesh_edge_swapverts(oe, kv, tv);
			/*append oe to tv's disk cycle*/
			bmesh_disk_append_edge(oe, tv);
			/*remove ke from tv's disk cycle*/
			bmesh_disk_remove_edge(ke, tv);
		
			

			/*deal with radial cycle of ke*/
			if(ke->l) {
				/*first step, fix the neighboring loops of all loops in ke's radial cycle*/
				radlen = bmesh_cycle_length(&(ke->l->radial));
				for(i=0,killoop = ke->l; i<radlen; i++, killoop = bmesh_radial_nextloop(killoop)) {
					/*relink loops and fix vertex pointer*/
					killoop->next->prev = killoop->prev;
					killoop->prev->next = killoop->next;
					if( ((killoop->next))->v == kv) ((killoop->next))->v = tv;
					
					/*fix len attribute of face*/
					killoop->f->len--;
					if(killoop->f->loopbase == killoop) killoop->f->lbase = ((killoop->next));
				}
				/*second step, remove all the hanging loops attached to ke*/
				killoop = ke->l;
				radlen = bmesh_cycle_length(&(ke->l->radial));
				/*make sure we have enough room in bm->lpar*/
				if(bm->lparlen < radlen) {
					MEM_freeN(bm->lpar);
					bm->lpar = MEM_callocN(sizeof(BMLoop *)* radlen, "BM Loop pointer array");
					bm->lparlen = bm->lparlen * radlen;
				}
				/*this should be wrapped into a bme_free_radial function to be used by bmesh_KF as well...*/
				i=0;
				while(i<radlen) {
					bm->lpar[i] = killoop;
					killoop = killoop->radial_next;
					i++;
				}
				i=0;
				while(i<radlen) {
					bmesh_free_loop(bm,bm->lpar[i]);
					i++;
				}
				/*Validate radial cycle of oe*/
				edok = bmesh_cycle_validate(radlen,&(oe->l->radial));
				
			}
			

			/*Validate disk cycles*/
			diskbase = bmesh_disk_getpointer(ov->e,ov);
			edok = bmesh_cycle_validate(valance1, diskbase);
			if(!edok) bmesh_error();
			diskbase = bmesh_disk_getpointer(tv->e,tv);
			edok = bmesh_cycle_validate(valance2, diskbase);
			if(!edok) bmesh_error();
			
			/*Validate loop cycle of all faces attached to oe*/
			for(i=0,nextl = oe->l; i<radlen; i++, nextl = bmesh_radial_nextloop(nextl)) {
				edok = bmesh_cycle_validate(nextl->f->len,nextl->f->lbase);
				if(!edok) bmesh_error();
			}
			/*deallocate edge*/
			BLI_remlink(&(bm->edges), &(ke->head));
			bmesh_free_edge(bm, ke);
			/*deallocate vertex*/
			BLI_remlink(&(bm->verts), &(kv->head));
			bmesh_free_vert(bm, kv);	
			return 1;
		}
	}
	return 0;
}


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

int bmesh_loop_reverse(BMesh *bm, BMFace *f)
{
	BMLoop *l = f->loopbase, *curloop, *oldprev, *oldnext;
	int i, j, edok, len = 0;

	len = bmesh_cycle_length(l);
	if(bm->edarlen < len) {
		MEM_freeN(bm->edar);
		bm->edar = MEM_callocN(sizeof(BMEdge *)* len, "BM Edge pointer array");
		bm->edarlen = len;
	}
	
	for(i=0, curloop = l; i< len; i++, curloop= ((curloop->next)) ) {
		curloop->e->head.eflag1 = 0;
		curloop->e->head.eflag2 = bmesh_cycle_length(&curloop->radial);
		bmesh_radial_remove_loop(curloop, curloop->e);
		/*in case of border edges we HAVE to zero out curloop->radial Next/Prev*/
		curloop->radial.next = curloop->radial.prev = NULL;
		bm->edar[i] = curloop->e;
	}
	
	/*actually reverse the loop. This belongs in bmesh_cycle_reverse!*/
	for(i=0, curloop = l; i < len; i++) {
		oldnext = ((curloop->next));
		oldprev = ((curloop->prev));
		curloop->next = (BMHeader*)oldprev;
		curloop->prev = (BMHeader*)oldnext;
		curloop = oldnext;
	}

	if(len == 2) { //two edged face
		//do some verification here!
		l->e = bm->edar[1];
		((l->next))->e = bm->edar[0];
	}
	else {
		for(i=0, curloop = l; i < len; i++, curloop = ((curloop->next)) ) {
			edok = 0;
			for(j=0; j < len; j++) {
				edok = bmesh_verts_in_edge(curloop->v, ((curloop->next))->v, bm->edar[j]);
				if(edok) {
					curloop->e = bm->edar[j];
					break;
				}
			}
		}
	}
	/*rebuild radial*/
	for(i=0, curloop = l; i < len; i++, curloop = curloop->next ) bmesh_radial_append(curloop->e, curloop);
	
	/*validate radial*/
	for(i=0, curloop = l; i < len; i++, curloop = ((curloop->next)) ) {
		edok = bmesh_cycle_validate(curloop->e->head.eflag2, &(curloop->radial));
		if(!edok) {
			bmesh_error();
		}
	}
	return 1;
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
 *	 |		  |           |        | 
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

//disregarding f1loop and f2loop, if a vertex appears in a joined face more than once, we cancel

BMFace *bmesh_jfke(BMesh *bm, BMFace *f1, BMFace *f2, BMEdge *e)
{
	
	BMLoop *curloop, *f1loop=NULL, *f2loop=NULL;
	int loopok = 0, newlen = 0,i, f1len=0, f2len=0, radlen=0, edok, shared;
	
	if(f1 == f2) return NULL; //can't join a face to itself
	/*verify that e is in both f1 and f2*/
	f1len = bmesh_cycle_length(f1->lbase);
	f2len = bmesh_cycle_length(f2->lbase);
	for(i=0, curloop = f1->loopbase; i < f1len; i++, curloop = ((curloop->next)) ) {
		if(curloop->e == e) {
			f1loop = curloop;
			break;
		}
	}
	for(i=0, curloop = f2->loopbase; i < f2len; i++, curloop = ((curloop->next)) ) {
		if(curloop->e==e) {
			f2loop = curloop;
			break;
		}
	}
	if(!(f1loop && f2loop)) return NULL;
	
	/*validate that edge is 2-manifold edge*/
	radlen = bmesh_cycle_length(&(f1loop->radial));
	if(radlen != 2) return NULL;

	/*validate direction of f2's loop cycle is compatible.*/
	if(f1loop->v == f2loop->v) return NULL;
	
	/*
		validate that for each face, each vertex has another edge in its disk cycle that is 
		not e, and not shared.
	*/
	if(bmesh_radial_find_face( ((f1loop->next))->e,f2)) return NULL;
	if(bmesh_radial_find_face( ((f1loop->prev))->e,f2)) return NULL;
	if(bmesh_radial_find_face( ((f2loop->next))->e,f1)) return NULL;
	if(bmesh_radial_find_face( ((f2loop->prev))->e,f1)) return NULL;
	
	/*validate only one shared edge*/
	shared = BM_face_share_edges(f1,f2);
	if(shared > 1) return NULL;

	/*validate no internal joins*/
	for(i=0, curloop = f1->loopbase; i < f1len; i++, curloop = ((curloop->next)) ) curloop->v->head.eflag1 = 0;
	for(i=0, curloop = f2->loopbase; i < f2len; i++, curloop = ((curloop->next)) ) curloop->v->head.eflag1 = 0;

	for(i=0, curloop = f1->loopbase; i < f1len; i++, curloop = ((curloop->next)) ) {
		if(curloop != f1loop)
			curloop->v->head.eflag1++;
	}
	for(i=0, curloop = f2->loopbase; i < f2len; i++, curloop = ((curloop->next)) ) {
		if(curloop != f2loop)
			curloop->v->head.eflag1++;
	}

	for(i=0, curloop = f1->loopbase; i < f1len; i++, curloop = ((curloop->next)) ) {
		if(curloop->v->head.eflag1 > 1)
			return NULL;
	}
	
	for(i=0, curloop = f2->loopbase; i < f2len; i++, curloop = ((curloop->next)) ) {
		if(curloop->v->head.eflag1 > 1)
			return NULL;
	}

	/*join the two loops*/
	f1loop->prev->next = f2loop->next;
	f2loop->next->prev = f1loop->prev;
	
	f1loop->next->prev = f2loop->prev;
	f2loop->prev->next = f1loop->next;
	
	/*if f1loop was baseloop, give f1loop->next the base.*/
	if(f1->loopbase == f1loop) f1->lbase = ((f1loop->next));
	
	/*validate the new loop*/
	loopok = bmesh_cycle_validate((f1len+f2len)-2, f1->lbase);
	if(!loopok) bmesh_error();
	
	/*make sure each loop points to the proper face*/
	newlen = bmesh_cycle_length(f1->lbase);
	for(i = 0, curloop = f1->loopbase; i < newlen; i++, curloop = ((curloop->next)) ) curloop->f = f1;
	
	f1->len = newlen;
	
	edok = bmesh_cycle_validate(f1->len, f1->lbase);
	if(!edok) bmesh_error();
	
	/*remove edge from the disk cycle of its two vertices.*/
	bmesh_disk_remove_edge(f1loop->e, f1loop->e->v1);
	bmesh_disk_remove_edge(f1loop->e, f1loop->e->v2);
	
	/*deallocate edge and its two loops as well as f2*/
	BLI_remlink(&(bm->edges), &(f1loop->e->head));
	BLI_remlink(&(bm->polys), &(f2->head));
	bmesh_free_edge(bm, f1loop->e);
	bmesh_free_loop(bm, f1loop);
	bmesh_free_loop(bm, f2loop);
	bmesh_free_poly(bm, f2);	
	return f1;
}

/**
*    bmesh_URMV
*
*    UNGLUE REGION MAKE VERT:
*
*    Takes a locally manifold disk of face corners and 'unglues' it
*    creating a new vertex
*
**/

#define URMV_VISIT    1
#define URMV_VISIT2   2

BMVert *bmesh_urmv(BMesh *bm, BMFace *sf, BMVert *sv)
{
    BMVert *nv = NULL;
    BMLoop *l = NULL, *sl = NULL;
    BMEdge *curedge = NULL;
    int numloops = 0, numedges = 0, i, maxedges, maxloops;


    /*BMESH_TODO: Validation*/
    /*validate radial cycle of all collected loops*/
    /*validate the disk cycle of sv, and nv*/
    /*validate the face length of all faces? overkill?*/
    /*validate the l->e pointers of all affected faces, ie: l->v and l->next->v should be equivalent to l->e*/
   
    /*verify that sv has edges*/
    if(sv->e == NULL)
        return NULL;
   
    /*first verify no wire edges on sv*/
    curedge = sv->e;
    do {
        if(curedge->l == NULL)
            return NULL;
        curedge = bmesh_disk_nextedge(curedge, sv);
    } while(curedge != sv->e);
   
    /*next verify that sv is in sf*/
    l = sf->loopbase;
    do {
        if(l->v == sv) {
            sl = l;
            break;
        }
        l = (l->next);
    } while(l != sf->lbase);
   
    if(sl == NULL)
        return NULL;
   
    /*clear euler flags*/
    sv->head.eflag1 = 0;
   
    curedge = sv->e;
    do {
        curedge->head.eflag1 = 0;
        l = curedge->l;
        do {
            l->head.eflag1 = 0;
            l->f->head.eflag1 = 0;
            l = bmesh_radial_nextloop(l);
        } while(l != curedge->l);
        curedge = bmesh_disk_nextedge(curedge, sv);
    } while(curedge != sv->e);
   
    /*search through face disk and flag elements as we go.*/
    /*Note, test this to make sure that it works correct on
    non-manifold faces!
    */
    l = sl;
    l->e->head.eflag1 |= URMV_VISIT;
    l->f->head.eflag1 |= URMV_VISIT;
    do {
        if(l->v == sv)
            l = bmesh_radial_nextloop((l->prev));
        else
            l = bmesh_radial_nextloop((l->next));
        l->e->head.eflag1 |= URMV_VISIT;
        l->f->head.eflag1 |= URMV_VISIT;
    } while(l != sl && (bmesh_cycle_length(&(l->radial)) > 1) );
   
    /*Verify that all visited edges are at least 1 or 2 manifold*/
    curedge = sv->e;
    do {
        if(curedge->head.eflag1 && (bmesh_cycle_length(&(curedge->l->radial)) > 2) )
            return NULL;
        curedge = bmesh_disk_nextedge(curedge, sv);
    } while(curedge != sv->e);

	/*allocate temp storage - we overallocate here instead of trying to be clever*/
	maxedges = 0;
	maxloops = 0;
	curedge = sv->e;
	do {
		if(curedge->l) {
			l = curedge->l;
			do {
				maxloops += l->f->len;
				l = bmesh_radial_nextloop(l);
			} while(l != curedge->l);
		}
		maxedges+= 1;
		curedge = bmesh_disk_nextedge(curedge,sv);
	} while(curedge != sv->e);

	if(bm->edarlen < maxedges) {
		MEM_freeN(bm->edar);
		bm->edar = MEM_callocN(sizeof(BMEdge *) * maxedges, "BM Edge pointer array");
		bm->edarlen = maxedges;
	}
	if(bm->lparlen < maxloops) {
		MEM_freeN(bm->lpar);
		bm->lpar = MEM_callocN(sizeof(BMLoop *) * maxloops, "BM Loop pointer array");
		bm->lparlen = maxloops;
	}

    /*first get loops by looping around edges and loops around that edges faces*/
    curedge = sv->e;
    do {
        if(curedge->l) {
            l = curedge->l;
            do {
                if( (l->head.eflag1 & URMV_VISIT) && (!(l->head.eflag1 & URMV_VISIT2)) ) {
                    bm->lpar[numloops] = l;
                    l->head.eflag1 |= URMV_VISIT2;
                    numloops++;
                }
                l = bmesh_radial_nextloop(l);
            } while(l != curedge->l);
        }
        curedge = bmesh_disk_nextedge(curedge, sv);
    } while(curedge != sv->e);

    /*now collect edges by looping around edges and looking at visited flags*/
    curedge = sv->e;
    do {
        if(curedge->head.eflag1 & URMV_VISIT) {
            bm->edar[numedges] = curedge;
            numedges++;
        }
        curedge = bmesh_disk_nextedge(curedge, sv);
    } while(curedge != sv->e);
   
    /*make new vertex*/
    nv = bmesh_addvertlist(bm, sv);
   
    /*go through and relink edges*/
    for(i = 0;  i <  numedges; i++) {
        curedge = bm->edar[i];
        /*remove curedge from sv*/
        bmesh_disk_remove_edge(curedge, sv);
        /*swap out sv for nv in curedge*/
        bmesh_edge_swapverts(curedge, sv, nv);
        /*add curedge to nv's disk cycle*/
        bmesh_disk_append_edge(curedge, nv);
    }
   
    /*go through and relink loops*/
    for(i = 0; i < numloops; i ++) {
        l = bm->lpar[i];
        if(l->v == sv)
            l->v = nv;
	}
	return nv;
}
#endif
