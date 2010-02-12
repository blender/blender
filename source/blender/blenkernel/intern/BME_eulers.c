/**
 * BME_eulers.c    jan 2007
 *
 *	BMesh Euler construction API.
 *
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * about this.	
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

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"

#include "BKE_utildefines.h"
#include "BKE_customdata.h"
#include "BKE_bmesh.h"

#include "BLI_blenlib.h"
#include "bmesh_private.h"
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
	
	In the BMesh system, each euler is named by an ancronym which describes what it actually does.
	Furthermore each Euler has a logical inverse. An important design criteria of all Eulers is that
	through a Euler's logical inverse you can 'undo' an operation. (Special note should
	be taken of BME_loop_reverse, which is its own inverse).
		
	BME_MF/KF: Make Face and Kill Face
	BME_ME/KE: Make Edge and Kill Edge
	BME_MV/KV: Make Vert and Kill Vert
	BME_SEMV/JEKV: Split Edge, Make Vert and Join Edge, Kill Vert
	BME_SFME/JFKE: Split Face, Make Edge and Join Face, Kill Edge
	BME_loop_reverse: Reverse a Polygon's loop cycle. (used for flip normals for one)
	
	Using a combination of these eleven eulers any non-manifold modelling operation can be achieved.
	Each Euler operator has a detailed explanation of what is does in the comments preceding its 
	code. 

   *The term "Euler Operator" is actually a misnomer when referring to a non-manifold 
    data structure. Its use is in keeping with the convention established by others.

	TODO:
	-Finish inserting 'strict' validation in all Eulers
*/

void *BME_exit(char *s) {
	if (s) printf("%s\n",s);
	return NULL;
}

#define RETCLEAR(bm) {bm->rval->v = bm->rval->e = bm->rval->f = bm->rva->l = NULL;}
/*MAKE Eulers*/

/**
 *			BME_MV
 *
 *	MAKE VERT EULER:
 *	
 *	Makes a single loose vertex.
 *
 *	Returns -
 *	A BME_Vert pointer.
 */

BME_Vert *BME_MV(BME_Mesh *bm, float *vec){
	BME_Vert *v = BME_addvertlist(bm, NULL);	
	VECCOPY(v->co,vec);
	return v;
}

/**
 *			BME_ME
 *
 *	MAKE EDGE EULER:
 *	
 *	Makes a single wire edge between two vertices.
 *	If the caller does not want there to be duplicate
 *	edges between the vertices, it is up to them to check 
 *	for this condition beforehand.
 *
 *	Returns -
 *	A BME_Edge pointer.
 */

BME_Edge *BME_ME(BME_Mesh *bm, BME_Vert *v1, BME_Vert *v2){
	BME_Edge *e=NULL;
	BME_CycleNode *d1=NULL, *d2=NULL;
	int valance1=0, valance2=0, edok;
	
	/*edge must be between two distinct vertices...*/
	if(v1 == v2) return NULL;
	
	#ifndef BME_FASTEULER
	/*count valance of v1*/
	if(v1->edge){ 
		d1 = BME_disk_getpointer(v1->edge,v1);
		if(d1) valance1 = BME_cycle_length(d1);
		else BME_error();
	}
	if(v2->edge){
		d2 = BME_disk_getpointer(v2->edge,v2);
		if(d2) valance2 = BME_cycle_length(d2);
		else BME_error();
	}
	#endif
	
	/*go ahead and add*/
	e = BME_addedgelist(bm, v1, v2, NULL);
	BME_disk_append_edge(e, e->v1);
	BME_disk_append_edge(e, e->v2);
	
	#ifndef BME_FASTEULER
	/*verify disk cycle lengths*/
	d1 = BME_disk_getpointer(e, e->v1);
	edok = BME_cycle_validate(valance1+1, d1);
	if(!edok) BME_error();
	d2 = BME_disk_getpointer(e, e->v2);
	edok = BME_cycle_validate(valance2+1, d2);
	if(!edok) BME_error();
	
	/*verify that edge actually made it into the cycle*/
	edok = BME_disk_hasedge(v1, e);
	if(!edok) BME_error();
	edok = BME_disk_hasedge(v2, e);
	if(!edok) BME_error();
	#endif
	return e;
}



/**
 *			BME_MF
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
 *	A BME_Poly pointer
 */

#define MF_CANDIDATE	1
#define MF_VISITED		2
#define MF_TAKEN		4 

BME_Poly *BME_MF(BME_Mesh *bm, BME_Vert *v1, BME_Vert *v2, BME_Edge **elist, int len)
{
	BME_Poly *f = NULL;
	BME_Edge *curedge;
	BME_Vert *curvert, *tv, **vlist;
	int i, j, done, cont, edok;
	
	if(len < 2) return NULL;
	
	/*make sure that v1 and v2 are in elist[0]*/
	if(BME_verts_in_edge(v1,v2,elist[0]) == 0) return NULL;
	
	/*clear euler flags*/
	for(i=0;i<len;i++) elist[i]->eflag1=elist[i]->eflag2 = 0;
	for(i=0;i<len;i++){
		elist[i]->eflag1 |= MF_CANDIDATE;
		
		/*if elist[i] has a loop, count its radial length*/
		if(elist[i]->loop) elist[i]->eflag2 = BME_cycle_length(&(elist[i]->loop->radial));
		else elist[i]->eflag2 = 0;
	}
	
	/*	For each vertex in each edge, it must have exactly two MF_CANDIDATE edges attached to it
		Note that this does not gauruntee that face is a single closed loop. At best it gauruntees
		that elist contains a finite number of seperate closed loops.
	*/
	for(i=0; i<len; i++){
		edok = BME_disk_count_edgeflag(elist[i]->v1, MF_CANDIDATE, 0);
		if(edok != 2) return NULL;
		edok = BME_disk_count_edgeflag(elist[i]->v2, MF_CANDIDATE, 0);
		if(edok != 2) return NULL;
	}
	
	/*set start edge, start vert and target vert for our loop traversal*/
	curedge = elist[0];
	tv = v1;
	curvert = v2;
	
	if(bm->vtarlen < len){
		MEM_freeN(bm->vtar);
		bm->vtar = MEM_callocN(sizeof(BME_Vert *)* len, "BMesh Vert pointer array");
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
	while(!done){
		/*add curvert to vlist*/
		/*insert some error cheking here for overflows*/
		i++;
		vlist[i] = curvert;
		
		/*mark curedge as visited*/
		curedge->eflag1 |= MF_VISITED;
		
		/*find next edge and vert*/
		curedge = BME_disk_next_edgeflag(curedge, curvert, MF_CANDIDATE, 0);
		curvert = BME_edge_getothervert(curedge, curvert);
		if(curvert == tv){
			curedge->eflag1 |= MF_VISITED;
			done=1;
		}
	}

	/*	Verify that all edges have been visited It's possible that we did reach tv 
		from sv, but that several unconnected loops were passed in via elist.
	*/
	cont=1;
	for(i=0; i<len; i++){
		if((elist[i]->eflag1 & MF_VISITED) == 0) cont = 0;
	}
	
	/*if we get this far, its ok to allocate the face and add the loops*/
	if(cont){
		BME_Loop *l;
		BME_Edge *e;
		f = BME_addpolylist(bm, NULL);
		f->len = len;
		for(i=0;i<len;i++){
			curvert = vlist[i];
			l = BME_create_loop(bm,curvert,NULL,f,NULL);
			if(!(f->loopbase)) f->loopbase = l;
			BME_cycle_append(f->loopbase, l);
		}
		
		/*take care of edge pointers and radial cycle*/
		for(i=0, l = f->loopbase; i<len; i++, l=l->next){
			e = NULL;
			if(l == f->loopbase) e = elist[0]; /*first edge*/
			
			else{/*search elist for others*/
				for(j=1; j<len; j++){
					edok = BME_verts_in_edge(l->v, l->next->v, elist[j]);
					if(edok){ 
						e = elist[j];
						break;
					}
				}
			}
			l->e = e; /*set pointer*/
			BME_radial_append(e, l); /*append into radial*/
		}

		f->len = len;
		
		/*Validation Loop cycle*/
		edok = BME_cycle_validate(len, f->loopbase);
		if(!edok) BME_error();
		for(i=0, l = f->loopbase; i<len; i++, l=l->next){
			/*validate loop vert pointers*/
			edok = BME_verts_in_edge(l->v, l->next->v, l->e);
			if(!edok) BME_error();
			/*validate the radial cycle of each edge*/
			edok = BME_cycle_length(&(l->radial));
			if(edok != (l->e->eflag2 + 1)) BME_error();
		}
	}
	return f;
}

/* KILL Eulers */

/**
 *			BME_KV
 *
 *	KILL VERT EULER:
 *	
 *	Kills a single loose vertex.
 *
 *	Returns -
 *	1 for success, 0 for failure.
 */

int BME_KV(BME_Mesh *bm, BME_Vert *v){
	if(v->edge == NULL){ 
		BLI_remlink(&(bm->verts), v);
		BME_free_vert(bm,v);
		return 1;
	}
	return 0;
}

/**
 *			BME_KE
 *
 *	KILL EDGE EULER:
 *	
 *	Kills a wire edge.
 *
 *	Returns -
 *	1 for success, 0 for failure.
 */

int BME_KE(BME_Mesh *bm, BME_Edge *e){
	int edok;
	
	/*Make sure that no faces!*/
	if(e->loop == NULL){
		BME_disk_remove_edge(e, e->v1);
		BME_disk_remove_edge(e, e->v2);
		
		/*verify that edge out of disk*/
		edok = BME_disk_hasedge(e->v1, e);
		if(edok) BME_error();
		edok = BME_disk_hasedge(e->v2, e);
		if(edok) BME_error();
		
		/*remove and deallocate*/
		BLI_remlink(&(bm->edges), e);
		BME_free_edge(bm, e);
		return 1;
	}
	return 0;
}

/**
 *			BME_KF
 *
 *	KILL FACE EULER:
 *	
 *	The logical inverse of BME_MF.
 *	Kills a face and removes each of its loops from the radial that it belongs to.
 *
 *  Returns -
 *	1 for success, 0 for failure.
*/

int BME_KF(BME_Mesh *bm, BME_Poly *bply){
	BME_Loop *newbase,*oldbase, *curloop;
	int i,len=0;
	
	/*add validation to make sure that radial cycle is cleaned up ok*/
	/*deal with radial cycle first*/
	len = BME_cycle_length(bply->loopbase);
	for(i=0, curloop=bply->loopbase; i < len; i++, curloop = curloop->next) 
		BME_radial_remove_loop(curloop, curloop->e);
	
	/*now deallocate the editloops*/
	for(i=0; i < len; i++){
		newbase = bply->loopbase->next;
		oldbase = bply->loopbase;
		BME_cycle_remove(oldbase, oldbase);
		BME_free_loop(bm, oldbase);
		bply->loopbase = newbase;
	}
	
	BLI_remlink(&(bm->polys), bply);
	BME_free_poly(bm, bply);
	return 1;
}

/*SPLIT Eulers*/

/**
 *			BME_SEMV
 *
 *	SPLIT EDGE MAKE VERT:
 *	Takes a given edge and splits it into two, creating a new vert.
 *
 *
 *		Before:	OV---------TV	
 *		After:	OV----NV---TV
 *
 *  Returns -
 *	BME_Vert pointer.
 *
*/

BME_Vert *BME_SEMV(BME_Mesh *bm, BME_Vert *tv, BME_Edge *e, BME_Edge **re){
	BME_Vert *nv, *ov;
	BME_CycleNode *diskbase;
	BME_Edge *ne;
	int i, edok, valance1=0, valance2=0;
	
	if(BME_vert_in_edge(e,tv) == 0) return NULL;
	ov = BME_edge_getothervert(e,tv);
	//v2 = tv;

	/*count valance of v1*/
	diskbase = BME_disk_getpointer(e, ov);
	valance1 = BME_cycle_length(diskbase);
	/*count valance of v2*/
	diskbase = BME_disk_getpointer(e, tv);
	valance2 = BME_cycle_length(diskbase);
	
	nv = BME_addvertlist(bm, tv);
	ne = BME_addedgelist(bm, nv, tv, e);
	
	//e->v2 = nv;
	/*remove e from v2's disk cycle*/
	BME_disk_remove_edge(e, tv);
	/*swap out tv for nv in e*/
	BME_edge_swapverts(e, tv, nv);
	/*add e to nv's disk cycle*/
	BME_disk_append_edge(e, nv);
	/*add ne to nv's disk cycle*/
	BME_disk_append_edge(ne, nv);
	/*add ne to tv's disk cycle*/
	BME_disk_append_edge(ne, tv);
	/*verify disk cycles*/
	diskbase = BME_disk_getpointer(ov->edge,ov);
	edok = BME_cycle_validate(valance1, diskbase);
	if(!edok) BME_error();
	diskbase = BME_disk_getpointer(tv->edge,tv);
	edok = BME_cycle_validate(valance2, diskbase);
	if(!edok) BME_error();
	diskbase = BME_disk_getpointer(nv->edge,nv);
	edok = BME_cycle_validate(2, diskbase);
	if(!edok) BME_error();
	
	/*Split the radial cycle if present*/
	if(e->loop){
		BME_Loop *nl,*l;
		BME_CycleNode *radEBase=NULL, *radNEBase=NULL;
		int radlen = BME_cycle_length(&(e->loop->radial));
		/*Take the next loop. Remove it from radial. Split it. Append to appropriate radials.*/
		while(e->loop){
			l=e->loop;
			l->f->len++;
			BME_radial_remove_loop(l,e);
			
			nl = BME_create_loop(bm,NULL,NULL,l->f,l);
			nl->prev = l;
			nl->next = l->next;
			nl->prev->next = nl;
			nl->next->prev = nl;
			nl->v = nv;
			
			/*assign the correct edge to the correct loop*/
			if(BME_verts_in_edge(nl->v, nl->next->v, e)){
				nl->e = e;
				l->e = ne;
				
				/*append l into ne's rad cycle*/
				if(!radNEBase){
					radNEBase = &(l->radial);
					radNEBase->next = NULL;
					radNEBase->prev = NULL;
				}
				
				if(!radEBase){
					radEBase = &(nl->radial);
					radEBase->next = NULL;
					radEBase->prev = NULL;
				}
				
				BME_cycle_append(radEBase,&(nl->radial));
				BME_cycle_append(radNEBase,&(l->radial));
					
			}
			else if(BME_verts_in_edge(nl->v,nl->next->v,ne)){
				nl->e = ne;
				l->e = e;
				
				if(!radNEBase){
					radNEBase = &(nl->radial);
					radNEBase->next = NULL;
					radNEBase->prev = NULL;
				}
				if(!radEBase){
					radEBase = &(l->radial);
					radEBase->next = NULL;
					radEBase->prev = NULL;
				}
				BME_cycle_append(radEBase,&(l->radial));
				BME_cycle_append(radNEBase,&(nl->radial));
			}
					
		}
		
		e->loop = radEBase->data;
		ne->loop = radNEBase->data;
		
		/*verify length of radial cycle*/
		edok = BME_cycle_validate(radlen,&(e->loop->radial));
		if(!edok) BME_error();
		edok = BME_cycle_validate(radlen,&(ne->loop->radial));
		if(!edok) BME_error();
		
		/*verify loop->v and loop->next->v pointers for e*/
		for(i=0,l=e->loop; i < radlen; i++, l = l->radial.next->data){
			if(!(l->e == e)) BME_error();
			if(!(l->radial.data == l)) BME_error();
			if(l->prev->e != ne && l->next->e != ne) BME_error();
			edok = BME_verts_in_edge(l->v, l->next->v, e);
			if(!edok) BME_error();
			if(l->v == l->next->v) BME_error();
			if(l->e == l->next->e) BME_error();
			/*verify loop cycle for kloop->f*/
			edok = BME_cycle_validate(l->f->len, l->f->loopbase);
			if(!edok) BME_error();
		}
		/*verify loop->v and loop->next->v pointers for ne*/
		for(i=0,l=ne->loop; i < radlen; i++, l = l->radial.next->data){
			if(!(l->e == ne)) BME_error();
			if(!(l->radial.data == l)) BME_error();
			if(l->prev->e != e && l->next->e != e) BME_error();
			edok = BME_verts_in_edge(l->v, l->next->v, ne);
			if(!edok) BME_error();
			if(l->v == l->next->v) BME_error();
			if(l->e == l->next->e) BME_error();
			/*verify loop cycle for kloop->f. Redundant*/
			edok = BME_cycle_validate(l->f->len, l->f->loopbase);
			if(!edok) BME_error();
		}
	}
	
	if(re) *re = ne;
	return nv;
}

/**
 *			BME_SFME
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
 *	Returns -
 *  A BME_Poly pointer
 */
BME_Poly *BME_SFME(BME_Mesh *bm, BME_Poly *f, BME_Vert *v1, BME_Vert *v2, BME_Loop **rl){

	BME_Poly *f2;
	BME_Loop *v1loop = NULL, *v2loop = NULL, *curloop, *f1loop=NULL, *f2loop=NULL;
	BME_Edge *e;
	int i, len, f1len, f2len;
	
	
	/*verify that v1 and v2 are in face.*/
	len = BME_cycle_length(f->loopbase);
	for(i = 0, curloop = f->loopbase; i < len; i++, curloop = curloop->next){
		if(curloop->v == v1) v1loop = curloop;
		else if(curloop->v == v2) v2loop = curloop;
	}
	
	if(!v1loop || !v2loop) return NULL;
	
	/*allocate new edge between v1 and v2*/
	e = BME_addedgelist(bm, v1, v2,NULL);
	BME_disk_append_edge(e, v1);
	BME_disk_append_edge(e, v2);
	
	f2 = BME_addpolylist(bm,f);
	f1loop = BME_create_loop(bm,v2,e,f,v2loop);
	f2loop = BME_create_loop(bm,v1,e,f2,v1loop);
	
	f1loop->prev = v2loop->prev;
	f2loop->prev = v1loop->prev;
	v2loop->prev->next = f1loop;
	v1loop->prev->next = f2loop;
	
	f1loop->next = v1loop;
	f2loop->next = v2loop;
	v1loop->prev = f1loop;
	v2loop->prev = f2loop;
	
	f2->loopbase = f2loop;
	f->loopbase = f1loop;
	
	/*validate both loops*/
	/*I dont know how many loops are supposed to be in each face at this point! FIXME!*/
	
	/*go through all of f2's loops and make sure they point to it properly.*/
	f2len = BME_cycle_length(f2->loopbase);
	for(i=0, curloop = f2->loopbase; i < f2len; i++, curloop = curloop->next) curloop->f = f2;
	
	/*link up the new loops into the new edges radial*/
	BME_radial_append(e, f1loop);
	BME_radial_append(e, f2loop);
	
	
	f2->len = f2len;
	
	f1len = BME_cycle_length(f->loopbase);
	f->len = f1len;
	
	if(rl) *rl = f2loop;
	return f2;
}


/**
 *			BME_JEKV
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
int BME_JEKV(BME_Mesh *bm, BME_Edge *ke, BME_Vert *kv)
{
	BME_Edge *oe;
	BME_Vert *ov, *tv;
	BME_CycleNode *diskbase;
	BME_Loop *killoop,*nextl;
	int len,radlen=0, halt = 0, i, valance1, valance2,edok;
	
	if(BME_vert_in_edge(ke,kv) == 0) return 0;
	diskbase = BME_disk_getpointer(kv->edge, kv);
	len = BME_cycle_length(diskbase);
	
	if(len == 2){
		oe = BME_disk_nextedge(ke, kv);
		tv = BME_edge_getothervert(ke, kv);
		ov = BME_edge_getothervert(oe, kv);		
		halt = BME_verts_in_edge(kv, tv, oe); //check for double edges
		
		if(halt) return 0;
		else{
			
			/*For verification later, count valance of ov and tv*/
			diskbase = BME_disk_getpointer(ov->edge, ov);
			valance1 = BME_cycle_length(diskbase);
			diskbase = BME_disk_getpointer(tv->edge, tv);
			valance2 = BME_cycle_length(diskbase);
			
			/*remove oe from kv's disk cycle*/
			BME_disk_remove_edge(oe,kv);
			/*relink oe->kv to be oe->tv*/
			BME_edge_swapverts(oe, kv, tv);
			/*append oe to tv's disk cycle*/
			BME_disk_append_edge(oe, tv);
			/*remove ke from tv's disk cycle*/
			BME_disk_remove_edge(ke, tv);
		
			

			/*deal with radial cycle of ke*/
			if(ke->loop){
				/*first step, fix the neighboring loops of all loops in ke's radial cycle*/
				radlen = BME_cycle_length(&(ke->loop->radial));
				for(i=0,killoop = ke->loop; i<radlen; i++, killoop = BME_radial_nextloop(killoop)){
					/*relink loops and fix vertex pointer*/
					killoop->next->prev = killoop->prev;
					killoop->prev->next = killoop->next;
					if(killoop->next->v == kv) killoop->next->v = tv;
					
					/*fix len attribute of face*/
					killoop->f->len--;
					if(killoop->f->loopbase == killoop) killoop->f->loopbase = killoop->next;
				}
				/*second step, remove all the hanging loops attached to ke*/
				killoop = ke->loop;
				radlen = BME_cycle_length(&(ke->loop->radial));
				/*make sure we have enough room in bm->lpar*/
				if(bm->lparlen < radlen){
					MEM_freeN(bm->lpar);
					bm->lpar = MEM_callocN(sizeof(BME_Loop *)* radlen, "BMesh Loop pointer array");
					bm->lparlen = bm->lparlen * radlen;
				}
				/*this should be wrapped into a bme_free_radial function to be used by BME_KF as well...*/
				i=0;
				while(i<radlen){
					bm->lpar[i] = killoop;
					killoop = killoop->radial.next->data;
					i++;
				}
				i=0;
				while(i<radlen){
					BME_free_loop(bm,bm->lpar[i]);
					i++;
				}
				/*Validate radial cycle of oe*/
				edok = BME_cycle_validate(radlen,&(oe->loop->radial));
				
			}
			

			/*Validate disk cycles*/
			diskbase = BME_disk_getpointer(ov->edge,ov);
			edok = BME_cycle_validate(valance1, diskbase);
			if(!edok) BME_error();
			diskbase = BME_disk_getpointer(tv->edge,tv);
			edok = BME_cycle_validate(valance2, diskbase);
			if(!edok) BME_error();
			
			/*Validate loop cycle of all faces attached to oe*/
			for(i=0,nextl = oe->loop; i<radlen; i++, nextl = BME_radial_nextloop(nextl)){
				edok = BME_cycle_validate(nextl->f->len,nextl->f->loopbase);
				if(!edok) BME_error();
			}
			/*deallocate edge*/
			BLI_remlink(&(bm->edges), ke);
			BME_free_edge(bm, ke);
			/*deallocate vertex*/
			BLI_remlink(&(bm->verts), kv);
			BME_free_vert(bm, kv);	
			return 1;
		}
	}
	return 0;
}


/**
 *			BME_loop_reverse
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

int BME_loop_reverse(BME_Mesh *bm, BME_Poly *f){
	BME_Loop *l = f->loopbase, *curloop, *oldprev, *oldnext;
	int i, j, edok, len = 0;

	len = BME_cycle_length(l);
	if(bm->edarlen < len){
		MEM_freeN(bm->edar);
		bm->edar = MEM_callocN(sizeof(BME_Edge *)* len, "BMesh Edge pointer array");
		bm->edarlen = len;
	}
	
	for(i=0, curloop = l; i< len; i++, curloop=curloop->next){
		curloop->e->eflag1 = 0;
		curloop->e->eflag2 = BME_cycle_length(&curloop->radial);
		BME_radial_remove_loop(curloop, curloop->e);
		/*in case of border edges we HAVE to zero out curloop->radial Next/Prev*/
		curloop->radial.next = curloop->radial.prev = NULL;
		bm->edar[i] = curloop->e;
	}
	
	/*actually reverse the loop. This belongs in BME_cycle_reverse!*/
	for(i=0, curloop = l; i < len; i++){
		oldnext = curloop->next;
		oldprev = curloop->prev;
		curloop->next = oldprev;
		curloop->prev = oldnext;
		curloop = oldnext;
	}

	if(len == 2){ //two edged face
		//do some verification here!
		l->e = bm->edar[1];
		l->next->e = bm->edar[0];
	}
	else{
		for(i=0, curloop = l; i < len; i++, curloop = curloop->next){
			edok = 0;
			for(j=0; j < len; j++){
				edok = BME_verts_in_edge(curloop->v, curloop->next->v, bm->edar[j]);
				if(edok){
					curloop->e = bm->edar[j];
					break;
				}
			}
		}
	}
	/*rebuild radial*/
	for(i=0, curloop = l; i < len; i++, curloop = curloop->next) BME_radial_append(curloop->e, curloop);
	
	/*validate radial*/
	for(i=0, curloop = l; i < len; i++, curloop = curloop->next){
		edok = BME_cycle_validate(curloop->e->eflag2, &(curloop->radial));
		if(!edok){
			BME_error();
		}
	}
	return 1;
}

/**
 *			BME_JFKE
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
 *	in this case should call BME_JEKV on the extra edges before attempting to fuse f1 and f2.
 *
 *	Also note that the order of arguments decides whether or not certain per-face attributes are present
 *	in the resultant face. For instance vertex winding, material index, smooth flags, ect are inherited
 *	from f1, not f2.
 *
 *  Returns -
 *	A BME_Poly pointer
*/

BME_Poly *BME_JFKE(BME_Mesh *bm, BME_Poly *f1, BME_Poly *f2, BME_Edge *e)
{
	
	BME_Loop *curloop, *f1loop=NULL, *f2loop=NULL;
	int loopok = 0, newlen = 0,i, f1len=0, f2len=0, radlen=0, edok;
	
	if(f1 == f2) return NULL; //can't join a face to itself
	/*verify that e is in both f1 and f2*/
	f1len = BME_cycle_length(f1->loopbase);
	f2len = BME_cycle_length(f2->loopbase);
	for(i=0, curloop = f1->loopbase; i < f1len; i++, curloop = curloop->next){
		if(curloop->e == e){ 
			f1loop = curloop;
			break;
		}
	}
	for(i=0, curloop = f2->loopbase; i < f2len; i++, curloop = curloop->next){
		if(curloop->e==e){
			f2loop = curloop;
			break;
		}
	}
	if(!(f1loop && f2loop)) return NULL;
	
	/*validate that edge is 2-manifold edge*/
	radlen = BME_cycle_length(&(f1loop->radial));
	if(radlen != 2) return NULL;

	/*validate direction of f2's loop cycle is compatible.*/
	if(f1loop->v == f2loop->v) return NULL;
	
	/*
		Finally validate that for each face, each vertex has another edge in its disk cycle that is 
		not e, and not shared.
	*/
	if(BME_radial_find_face(f1loop->next->e,f2)) return NULL;
	if(BME_radial_find_face(f1loop->prev->e,f2)) return NULL;
	if(BME_radial_find_face(f2loop->next->e,f1)) return NULL;
	if(BME_radial_find_face(f2loop->prev->e,f1)) return NULL;
	
	/*join the two loops*/
	f1loop->prev->next = f2loop->next;
	f2loop->next->prev = f1loop->prev;
	
	f1loop->next->prev = f2loop->prev;
	f2loop->prev->next = f1loop->next;
	
	/*if f1loop was baseloop, give f1loop->next the base.*/
	if(f1->loopbase == f1loop) f1->loopbase = f1loop->next;
	
	/*validate the new loop*/
	loopok = BME_cycle_validate((f1len+f2len)-2, f1->loopbase);
	if(!loopok) BME_error();
	
	/*make sure each loop points to the proper face*/
	newlen = BME_cycle_length(f1->loopbase);
	for(i = 0, curloop = f1->loopbase; i < newlen; i++, curloop = curloop->next) curloop->f = f1;
	
	f1->len = newlen;
	
	edok = BME_cycle_validate(f1->len, f1->loopbase);
	if(!edok) BME_error();
	
	/*remove edge from the disk cycle of its two vertices.*/
	BME_disk_remove_edge(f1loop->e, f1loop->e->v1);
	BME_disk_remove_edge(f1loop->e, f1loop->e->v2);
	
	/*deallocate edge and its two loops as well as f2*/
	BLI_remlink(&(bm->edges), f1loop->e);
	BLI_remlink(&(bm->polys), f2);
	BME_free_edge(bm, f1loop->e);
	BME_free_loop(bm, f1loop);
	BME_free_loop(bm, f2loop);
	BME_free_poly(bm, f2);	
	return f1;
}
