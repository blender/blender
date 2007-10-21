/**
 * BME_tools.c    jan 2007
 *
 *	Functions for changing the topology of a mesh.
 *
 * $Id: BME_eulers.c,v 1.00 2007/01/17 17:42:01 Briggs Exp $
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.	
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "BKE_global.h"
#include "BKE_depsgraph.h"
#include "BKE_utildefines.h"
#include "BKE_bmesh.h"
#include "BLI_memarena.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_ghash.h"

/*Vertex Tools*/
void BME_connect_verts(BME_Mesh *bm)
{
	BME_Poly *f;
	BME_Loop *l, *v1loop, *v2loop;
	int pass;
	
	/*visit the faces with selected verts*/
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		v1loop = v2loop = NULL;
		pass = 0;
		if(!(BME_NEWELEM(f))){
			l = f->loopbase;
			do{
				if(BME_ISVISITED(l->v)){ 
					if(v2loop) pass = 1;
					else if(v1loop) v2loop = l;
					else v1loop = l;
				}
				l = l->next;
			}while(l != f->loopbase);

			if(v1loop && v2loop && (!pass) && (v1loop->next != v2loop) && (v2loop->next != v1loop)) BME_SFME(bm,f,v1loop->v, v2loop->v,NULL);
			
		}
	}
}

void BME_connect_edges(BME_Mesh *bm)
{
	BME_Vert *nv;
	BME_Edge *e, *e1, *e2;
	BME_Loop *l;
	BME_Poly *f;
	int pass;
	
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		e1 = e2 = NULL;
		pass = 0;
		l=f->loopbase;
		do{
			if(BME_SELECTED(l->e)){
				if(e2) pass = 1;
				else if(e1) e2 = l->e;
				else e1 = l->e;
			}
			l = l->next;
		}while(l!=f->loopbase);
		
		//if two edges selected and not pass, mark each one for split
		if((e1) && (e2) && (pass == 0)) e1->tflag1 = e2->tflag1 = 1;
	}
	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		if(e->tflag1){
			nv= BME_cut_edge(bm,e,1);
			BME_VISIT(nv);
		}
	}
	BME_connect_verts(bm);
}


/**
 *			BME_dissolve_edge
 *
 *	Edge Dissolve Function:
 *	
 *	Dissolves a 2-manifold edge by joining it's two faces. 
*	
*	TODO:if  the two adjacent faces have opposite windings, first 
*	make them consistent by calling BME_loop_reverse()?
 *
 *	Returns -
*/

void BME_dissolve_edges(BME_Mesh *bm)
{
	BME_Edge *e,*nexte;
	BME_Poly *f1, *f2;
	BME_Loop *next;
	
	e=BME_first(bm,BME_EDGE);
	while(e){
		nexte = BME_next(bm,BME_EDGE,e);
		if(BME_SELECTED(e)){
			f1 = e->loop->f;
			next = BME_radial_nextloop(e->loop);
			f2 = next->f;
			BME_JFKE(bm,f1,f2,e);
		}
		e = nexte;
	}
}

/**
 *			BME_cut_edge
 *
 *	Cuts a single edge in a mesh into multiple parts
 *
 */

BME_Vert  *BME_cut_edge(BME_Mesh *bm, BME_Edge *e, int numcuts)
{
	int i;
	float percent, step,length, vt1[3], v2[3];
	BME_Vert *nv;
	
	percent = 0.0;
	step = (1.0/((float)numcuts+1));
	
	length = VecLenf(e->v1->co,e->v2->co);
	VECCOPY(v2,e->v2->co);
	VecSubf(vt1, e->v1->co,  e->v2->co);
	
	for(i=0; i < numcuts; i++){
		percent += step;
		nv = BME_SEMV(bm,e->v2,e,NULL);
		VECCOPY(nv->co,vt1);
		VecMulf(nv->co,percent);
		VecAddf(nv->co,v2,nv->co);
	}
	return nv;
}


/**
 *			BME_cut_edges
 *
 *	Edge Cut Function:
 *	
 *	Cuts all selected edges a given number of times 
 *	TODO:
 *	-Do percentage cut for knife tool?
*	-Need to fix up patching of selection flags in eulers as well as marking new elements with BME_NEW.
*		Tools should then test for 'if( BME_SELECTED(e) && (!e->flag & BME_NEW))' when iterating over mesh and cutting elements.
*	-Move BME_model_begin and BME_model_end out of here. They belong in UI code. Also DAG_object_flush_update call and allqueue().
*
 *	Returns -
*/

void BME_cut_edges(BME_Mesh *bm, int numcuts)
{
	BME_Edge *e;
	for(e=BME_first(bm,BME_EDGE); e; e=BME_next(bm,BME_EDGE,e)){
		if(BME_SELECTED(e) && !(BME_NEWELEM(e))) 
			BME_cut_edge(bm,e,numcuts);
	}
}


/**
 *			BME_split_face
 *
 *	Face Split Tool:
 *	
 *	Splits a face into multiple other faces depending oh how 
 *	many of its vertices have been visited.
*	
*	TODO: This is only one 'fill type' for face subdivisions. Need to 
*	port over all the old subdivision fill types from editmesh and figure
*	out best strategy for dealing with n-gons.
*
 *	Returns -
 *	Nothing
 */
void BME_split_face(BME_Mesh *bm, BME_Poly *f)
{
	return;
}

/*

{
	BME_Loop *l, *scanloop, *nextl;
	int len, i,j;
	len = f->len;
	for(i=0,l=f->loopbase; i <= len; ){
		nextl = l->next;
		j=2;
		//printf("Face ID is%i, BM->Nextp is %i",f->EID,bm->nextp);
		if(BME_ISVISITED(l->v)){
			for(scanloop = l->next->next; j < (len-1); j++ , scanloop = scanloop->next){
				if(BME_ISVISITED(scanloop->v)){
					f = BME_SFME(bm,f,l->v,scanloop->v,NULL);
					nextl=scanloop;
					break;
				}
			}
		}
		l=nextl;
		i+=j;
	}
}
*/

/**
 *			BME_inset_poly
 *
 *	Face Inset Tool:
 *	
 *	Insets a single face and returns a pointer to the face at the 
 *	center of the newly created region
*	
*	TODO: Rewrite this. It's a mess. Especially take out the geometry modification stuff and make it a topo only thingy.
*
 *	Returns -
 *	A BME_Poly pointer.
 */

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

BME_Poly *BME_inset_poly(BME_Mesh *bm,BME_Poly *f){
	
	BME_Vert *v, *killvert;
	BME_Edge *killedge;
	BME_Loop *l,*nextloop, *newloop, *killoop, *sloop;
	BME_CycleNode *loopref;
	
	int done,len,i;
	float max[3],min[3],cent[3]; //center of original face
	
	/*get bounding box for face*/
	VECCOPY(max,f->loopbase->v->co);
	VECCOPY(min,f->loopbase->v->co);
	len = f->len;
	for(i=0,l=f->loopbase;i<len;i++,l=l->next){
		max[0] = MAX(max[0],l->v->co[0]);
		max[1] = MAX(max[1],l->v->co[1]);
		max[2] = MAX(max[2],l->v->co[2]);
		
		min[0] = MIN(min[0],l->v->co[0]);
		min[1] = MIN(min[1],l->v->co[1]);
		min[2] = MIN(min[2],l->v->co[2]);
	}
	
	cent[0] = (min[0] + max[0]) / 2.0;
	cent[1] = (min[1] + max[1]) / 2.0;
	cent[2] = (min[2] + max[2]) / 2.0;
	
	
	
	/*inset each edge in the polygon.*/
	len = f->len;
	for(i=0,l=f->loopbase; i < len; i++){
		nextloop = l->next;
		f = BME_SFME(bm,l->f,l->v,l->next->v,NULL);
		l=nextloop;
	}
	
	/*for each new edge, call SEMV twice on it*/
	for(i=0,l=f->loopbase; i < len; i++, l=l->next){ 
		l->tflag1 = 1; //going to store info that this loops edge still needs split
		l->tflag2 = l->v->tflag1 = l->v->tflag2 = 0; 
	}
	
	len = f->len;
	for(i=0,l=f->loopbase; i < len; i++){
		if(l->tflag1){
			l->tflag1 = 0;
			v= BME_SEMV(bm,l->next->v,l->e,NULL);
			VECCOPY(v->co,l->v->co);
			l->e->tflag2 =1; //mark for kill with JFKE
			v->tflag2 = 1; //mark for kill with JEKV
			v->tflag1 = 1; //mark for what?
			v= BME_SEMV(bm,l->next->v,l->e,NULL);
			VECCOPY(v->co,l->next->next->v->co);
			v->tflag1 = 1;
			l = l->next->next->next;
		}
	}
	
	len = f->len;
	sloop = NULL;
	for(i=0,l=f->loopbase; i < len; i++,l=l->next){
		if(l->v->tflag1 && l->next->next->v->tflag1){
			sloop = l;
			break;
		}
	}
	if(sloop){
		for(i=0,l=sloop; i < len; i++){
			nextloop = l->next->next->next;
			f = BME_SFME(bm,f,l->v,l->next->next->v,&killoop);
			i+=2;
			BME_JFKE(bm,l->f,((BME_Loop*)l->radial.next->data)->f,l->e);
			killedge = killoop->e;
			killvert = killoop->v;
			done = BME_JEKV(bm,killedge,killvert);
			if(!done){
				printf("whoops!");
			}
			l=nextloop;
		}
	}
	
	len = f->len;
	for(i=0,l=f->loopbase; i < len; i++,l=l->next){
		l->v->co[0] = (l->v->co[0] + cent[0]) / 2.0;
		l->v->co[1] = (l->v->co[1] + cent[1]) / 2.0;
		l->v->co[2] = (l->v->co[2] + cent[2]) / 2.0;
	}
	return NULL;
}


//*shouild change these to take a BME_DELETE flag instead!
static void remove_tagged_polys(BME_Mesh *bm){
	BME_Poly *f, *nextf;
	f=BME_first(bm,BME_POLY);
	while(f){
		nextf = BME_next(bm,BME_POLY,f);
		if(BME_ISVISITED(f)) BME_KF(bm,f);
		f = nextf;
	}
}
static void remove_tagged_edges(BME_Mesh *bm){
	BME_Edge *e, *nexte;
	e=BME_first(bm,BME_EDGE);
	while(e){
		nexte = BME_next(bm,BME_EDGE,e);
		if(BME_ISVISITED(e)) BME_KE(bm,e);
		e = nexte;
	}
}
static void remove_tagged_verts(BME_Mesh *bm){
	BME_Vert *v, *nextv;
	v=BME_first(bm,BME_VERT);
	while(v){
		nextv = BME_next(bm,BME_VERT,v);
		if(BME_ISVISITED(v)) BME_KV(bm,v);
		v=nextv;
	}
}

void BME_delete_verts(BME_Mesh *bm){
	BME_Vert *v;
	BME_Edge *e, *curedge;
	BME_Loop *curloop;
	
	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if(BME_SELECTED(v)){
			BME_VISIT(v); // mark for delete
			/*visit edges and faces of edges*/
			if(v->edge){
				curedge = v->edge;
				do{
					BME_VISIT(curedge); // mark for delete
					if(curedge->loop){
						curloop = curedge->loop;
						do{
							BME_VISIT(curloop->f); // mark for delete
							curloop = curloop->radial.next->data;
						} while(curloop != curedge->loop);
					}
					curedge = BME_disk_nextedge(curedge,v);
				} while(curedge != v->edge);
			}
		}
	}

	remove_tagged_polys(bm);
	remove_tagged_edges(bm);
	remove_tagged_verts(bm);
}

void BME_delete_edges(BME_Mesh *bm){
	BME_Edge *e;
	BME_Loop *curloop;
	
	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		if(BME_SELECTED(e)){
			BME_VISIT(e); //mark for delete
			if(e->loop){
				curloop = e->loop;
				do{
					BME_VISIT(curloop->f); //mark for delete
					curloop = curloop->radial.next->data;
				} while(curloop != e->loop);
			}
		}
	}
	remove_tagged_polys(bm);
	remove_tagged_edges(bm);
}

void BME_delete_polys(BME_Mesh *bm){
	BME_Poly *f;
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		if(BME_SELECTED(f)) BME_VISIT(f);
	}
	remove_tagged_polys(bm);
}

void BME_delete_context(BME_Mesh *bm, int type){
	BME_Vert *v;
	BME_Edge *e;
	BME_Loop *l;
	BME_Poly *f;
	
	if(type == BME_DEL_VERTS) BME_delete_verts(bm);
	else if(type == BME_DEL_EDGES){ 
		BME_delete_edges(bm);
		//remove loose vertices
		v=BME_first(bm,BME_VERT);
		while(v){
			BME_Vert *nextv = BME_next(bm,BME_VERT,v);
			if(BME_SELECTED(v) && (!(v->edge))) BME_KV(bm, v);
			v = nextv;
		}
	}
	else if(type == BME_DEL_EDGESFACES) BME_delete_edges(bm);
	else if(type == BME_DEL_ONLYFACES) BME_delete_polys(bm);
	else if(type == BME_DEL_FACES){
		/*go through and mark all edges of all faces for delete*/
		for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
			if(BME_SELECTED(f)){
				BME_VISIT(f);
				l=f->loopbase;
				do{
					BME_VISIT(l->e);
					l=l->next;
				}while(l!=f->loopbase);
			}
		}
		
		/*now go through and mark all remaining faces all edges for keeping.*/
		for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
			if(!(BME_SELECTED(f))){
				l=f->loopbase;
				do{
					BME_UNVISIT(l->e);
					l=l->next;
				}while(l!=f->loopbase);
			}
		}
		
		/*now delete marked faces*/
		remove_tagged_polys(bm);
		/*delete marked edges*/
		remove_tagged_edges(bm);
		/*remove loose vertices*/
		v=BME_first(bm,BME_VERT);
		while(v){
			BME_Vert *nextv = BME_next(bm,BME_VERT,v);
			if(BME_SELECTED(v) && (!(v->edge))) BME_KV(bm,v);
			v=nextv;
		}
	}
	else if(type == BME_DEL_ALL){
		f=BME_first(bm,BME_POLY);
		while(f){
			BME_Poly *nextf = BME_next(bm,BME_POLY,f);
			BME_KF(bm,f);
			f = nextf;
		}
		e=BME_first(bm,BME_EDGE);
		while(e){
			BME_Edge *nexte = BME_next(bm,BME_EDGE,e);
			BME_KE(bm,e);
			e = nexte;
		}
		v=BME_first(bm,BME_VERT);
		while(v){
			BME_Vert *nextv = BME_next(bm,BME_VERT,v);
			BME_KV(bm,v);
			v = nextv;
		}
	}
	BME_selectmode_flush(bm);
}

/**
 *			BME_extrudeXX functions
 *
 *	Extrude tool for verts/edges/faces
 *	
*	A rewrite of the old editmesh extrude code with the redundant parts broken into multiple functions
*	in an effort to reduce code. This works with multiple selection modes, and is intended to build the
*	extrusion in steps, depending on what elements are selected. Also decoupled the calculation of transform normal
*	and put it in UI where it probably is more appropriate for the moment.
 */

void BME_extrude_verts(BME_Mesh *bm, GHash *vhash){
	BME_Vert *v, *nv = NULL;
	BME_Edge *ne = NULL;
	float vec[3];

	//extrude the vertices
	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if(BME_SELECTED(v)){
			VECCOPY(vec,v->co);
			nv = BME_MV(bm,vec);
			nv->tflag2 =1; //mark for select
			ne = BME_ME(bm,v,nv);
			ne->tflag1 = 2; //mark as part of skirt 'ring'
			BLI_ghash_insert(vhash,v,nv);
			BME_VISIT(v);
		}
	}
}

void BME_extrude_skirt(BME_Mesh *bm, GHash *ehash){
	
	BME_Poly *nf=NULL;
	BME_Edge *e, *l=NULL, *r=NULL, *edar[4], *ne;
	BME_Vert *v, *v1, *v2, *lv, *rv, *nv;

	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		if(BME_SELECTED(e)){
			/*find one face incident upon e and use it for winding of new face*/
			if(e->loop){
				v1 = e->loop->next->v;
				v2 = e->loop->v;
			}
			else{
				v1 = e->v1;
				v2 = e->v2;
			}
			
			if(v1->edge->tflag1 == 2) l = v1->edge;
			else l = BME_disk_next_edgeflag(v1->edge, v1, 0, 2);
			if(v2->edge->tflag1 == 2) r = v2->edge;
			else r = BME_disk_next_edgeflag(v2->edge, v2, 0, 2);
			
			lv = BME_edge_getothervert(l,v1);
			rv = BME_edge_getothervert(r,v2);
			
			ne = BME_ME(bm,lv,rv);
			ne->tflag2 = 1; //mark for select
			BLI_ghash_insert(ehash,e,ne);
			BME_VISIT(e);
			
			edar[0] = e;
			edar[1] = l;
			edar[2] = ne;
			edar[3] = r;
			BME_MF(bm,v1,v2,edar,4);
		}
	}
}

void BME_cap_skirt(BME_Mesh *bm, GHash *vhash, GHash *ehash){
	BME_Vert *v, *nv, *v1, *v2;
	BME_Edge *e, **edar, *ne;
	BME_Loop *l;
	BME_Poly *f, *nf;
	MemArena *edgearena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
	float vec[3];
	int i, j, del_old =0;
	

	//loop through faces, then loop through their verts. If the verts havnt been visited yet, duplicate these.
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		if(BME_SELECTED(f)){
			l = f->loopbase;
			do{
				if(!(BME_ISVISITED(l->v))){ //interior vertex
					//dupe vert
					VECCOPY(vec,l->v->co);
					nv = BME_MV(bm,vec);
					BLI_ghash_insert(vhash,l->v,nv);
					//mark for delete
					l->v->tflag1 = 1;
					BME_VISIT(l->v); //we dont want to dupe it again.
				}
				l=l->next;
			}while(l!=f->loopbase);
		}
	}
	
	//find out if we delete old faces or not. This needs to be improved a lot.....
	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		if(BME_SELECTED(e) && e->loop){
			i= BME_cycle_length(&(e->loop->radial));
			if(i > 2){ 
				del_old = 1;
				break;
			}
		}
	}
	
	
	//build a new edge net, insert the new edges into the edge hash
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		if(BME_SELECTED(f)){
			l=f->loopbase;
			do{
				if(!(BME_ISVISITED(l->e))){ //interior edge
					//dupe edge
					ne = BME_ME(bm,BLI_ghash_lookup(vhash,l->e->v1),BLI_ghash_lookup(vhash,l->e->v2));
					BLI_ghash_insert(ehash,l->e,ne);
					//mark for delete
					l->e->tflag1 = 1;
					BME_VISIT(l->e); //we dont want to dupe it again.
				}
				l=l->next;
			}while(l!=f->loopbase);
		}
	}
	
	//build new faces. grab edges from edge hash.
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		if(BME_SELECTED(f)){
			edar = MEM_callocN(sizeof(BME_Edge*)*f->len,"Extrude array");
			 v1 = BLI_ghash_lookup(vhash,f->loopbase->v);
			v2 = BLI_ghash_lookup(vhash,f->loopbase->next->v);
			for(i=0,l=f->loopbase; i < f->len; i++,l=l->next){
				ne = BLI_ghash_lookup(ehash,l->e);
				edar[i] = ne;
			}
			nf=BME_MF(bm,v1,v2,edar,f->len);
			nf->tflag2 = 1; // mark for select
			if(del_old) f->tflag1 = 1; //mark for delete
			MEM_freeN(edar);
		}
	}
	BLI_memarena_free(edgearena);
}

/*unified extrude code*/
void BME_extrude_mesh(BME_Mesh *bm, int type){
	
	BME_Vert *v;
	BME_Edge *e;
	BME_Poly *f;
	BME_Loop *l;
	
	struct GHash *vhash, *ehash;
	/*Build a hash table of old pointers and new pointers.*/
	vhash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	ehash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	
	BME_selectmode_flush(bm); //ensure consistent selection. contains hack to make sure faces get consistent select.
	if(type & BME_EXTRUDE_FACES){ //Find selected edges with more than one incident face that is also selected. deselect them.
		for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
			int totsel=0;
			if(e->loop){
				l= e->loop;
				do{
					if(BME_SELECTED(l->f)) totsel++;
					l=BME_radial_nextloop(l);
				}while(l!=e->loop);
			}
			if(totsel > 1) BME_select_edge(bm,e,0);
		}
	}

	/*another hack to ensure consistent selection.....*/
	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		if(BME_SELECTED(e)) BME_select_edge(bm,e,1);
	}
	
	/*now we are ready to extrude*/
	if(type & BME_EXTRUDE_VERTS) BME_extrude_verts(bm,vhash);
	if(type & BME_EXTRUDE_EDGES) BME_extrude_skirt(bm,ehash);
	if(type & BME_EXTRUDE_FACES) BME_cap_skirt(bm,vhash,ehash);
	
	/*clear all selection flags*/
	BME_clear_flag_all(bm, SELECT|BME_VISITED);
	/*go through and fix up selection flags. Anything with BME_NEW should be selected*/
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		if(f->tflag2 == 1) BME_select_poly(bm,f,1);
		if(f->tflag1 == 1) BME_VISIT(f); //mark for delete
	}
	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		if(e->tflag2 == 1) BME_select_edge(bm,e,1);
		if(e->tflag1 == 1) BME_VISIT(e); // mark for delete
	}
	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if(v->tflag2 == 1) BME_select_vert(bm,v,1);
		if(v->tflag1 == 1) BME_VISIT(v); //mark for delete
	}
	/*go through and delete all of our old faces , edges and vertices.*/
	remove_tagged_polys(bm);
	remove_tagged_edges(bm);
	remove_tagged_verts(bm);
	/*free our hash tables*/
	BLI_ghash_free(vhash,NULL, NULL); //check usage!
	BLI_ghash_free(ehash,NULL, NULL); //check usage!
	BME_selectmode_flush(bm);
}



static BME_Vert *BME_dupevert(BME_Mesh *bm, BME_Vert *ov){
	BME_Vert *nv = BME_MV(bm,ov->co);
	/*copy info about ov*/
	if(BME_SELECTED(ov))BME_select_vert(bm,nv,1);
	return nv;
}
static BME_Edge *BME_dupedge(BME_Mesh *bm, BME_Edge *oe,BME_Vert *v1, BME_Vert *v2){
	BME_Edge *ne = BME_ME(bm,v1,v2);
	if(BME_SELECTED(oe))BME_select_edge(bm,ne,1);
	/*copy info about oe*/
	return ne;
}
static BME_Poly *BME_dupepoly(BME_Mesh *bm, BME_Poly *of, BME_Vert *v1, BME_Vert *v2, BME_Edge **edar){

	BME_Poly *nf = NULL;
	nf = BME_MF(bm,v1,v2,edar,of->len);
	/*copy info about of*/
	if(BME_SELECTED(of))BME_select_poly(bm,nf,1);
	return nf;
}
void BME_duplicate(BME_Mesh *bm){
	
	BME_Vert *v, *nv, *ev1, *ev2;
	BME_Edge *e, *ne, **edar;
	BME_Loop *l;
	BME_Poly *f;
	struct GHash *vhash, *ehash;//pointer hashes
	int edarlength, i;
	
	/*Build a hash table of old pointers and new pointers.*/
	vhash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	ehash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	
	/*Edge pointer array, realloc if too small (unlikely)*/
	edarlength = 4096;
	edar = MEM_callocN(sizeof(BME_Edge*)*edarlength,"Mesh duplicate tool \n");
	
	/*first search for selected faces, dupe all verts*/
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		if(BME_SELECTED(f)){
			/*dupe each vertex*/
			l=f->loopbase;
			do{
				if(!(BME_ISVISITED(l->v))){
					nv = BME_dupevert(bm,l->v);
					BLI_ghash_insert(vhash,l->v,nv);
					BME_VISIT(l->v);
				}
				l=l->next;
			}while(l!=f->loopbase);
		}
	}
	
	/*now search for selected edges, dupe all verts (if nessecary)*/
	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		if(BME_SELECTED(e)){
			if(!(BME_ISVISITED(e->v1))){
				nv = BME_dupevert(bm,e->v1);
				BLI_ghash_insert(vhash,e->v1,nv);
				BME_VISIT(e->v1);
			}
			if(!(BME_ISVISITED(e->v2))){
				nv = BME_dupevert(bm,e->v2);
				BLI_ghash_insert(vhash,e->v2,nv);
				BME_VISIT(e->v2);
			}
		}
	}
	/*finally, any unvisited vertices should be duped (loose)*/
	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if(BME_SELECTED(v) && (!(BME_NEWELEM(v)))){
			if(!(BME_ISVISITED(v))){
				nv = BME_dupevert(bm,v);
				BLI_ghash_insert(vhash,v,nv);
				BME_VISIT(v);
			}
		}
	}
		
	/*go through selected edges and dupe using vert hash as lookup.*/
	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		if(BME_SELECTED(e) && (!(BME_NEWELEM(e)))){
			ev1 = ev2 = NULL;
			ev1 = BLI_ghash_lookup(vhash,e->v1);
			ev2 = BLI_ghash_lookup(vhash,e->v2);
			
			ne = BME_dupedge(bm,e,ev1,ev2);
			BLI_ghash_insert(ehash,e,ne);
			BME_VISIT(e);
		}
	}
	/*go through selected faces and dupe edges*/
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		if(BME_SELECTED(f)){
			l=f->loopbase;
			do{
				if(!(BME_ISVISITED(l->e))){
					ev1 = ev2 = NULL;
					ev1 = BLI_ghash_lookup(vhash,l->e->v1);
					ev2 = BLI_ghash_lookup(vhash,l->e->v2);
					
					ne = BME_dupedge(bm,l->e,ev1,ev2);
					BLI_ghash_insert(ehash,l->e,ne);
					BME_VISIT(l->e);
				}
				l=l->next;
			}while(l!=f->loopbase);
		}
	}
	
	
	/*go through faces and dupe using edge hash as lookup*/
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		if(BME_SELECTED(f) && (!(BME_NEWELEM(f)))){
			/*first check facelen and see if edar needs to be reallocated. Highly unlikely.*/
			if(f->len > edarlength){
				MEM_freeN(edar);
				edarlength = edarlength * 2;
				edar = MEM_callocN(sizeof(BME_Edge*)*edarlength,"Mesh duplicate tool");
			}
			/*now loop through face, looking up the edge that should go in edar, and placing in there.*/
			l = f->loopbase;
			i = 0;
			do{
				edar[i] = BLI_ghash_lookup(ehash, l->e);
				BME_VISIT(l->e);
				i++;
				l=l->next;
			}while(l!=f->loopbase);
			ev1=ev2=NULL;
			ev1 = BLI_ghash_lookup(vhash,f->loopbase->v);
			ev2 = BLI_ghash_lookup(vhash,f->loopbase->next->v);
			
			BME_VISIT(f);
			BME_dupepoly(bm,f,ev1,ev2,edar);
		}
	}	
	
	/*finally go through and unselect all 'visited' elements*/
	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if(BME_ISVISITED(v)) BME_select_vert(bm,v,0);
	}
	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		if(BME_ISVISITED(e)) BME_select_edge(bm,e,0);
	}
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		if(BME_ISVISITED(f)) BME_select_poly(bm,f,0);
	}
	
	/*free memory*/
	if(edar) MEM_freeN(edar);
	BLI_ghash_free(vhash,NULL, NULL); //check usage!
	BLI_ghash_free(ehash,NULL, NULL); //check usage!
	BME_selectmode_flush(bm);
}

int convex(float *v1, float *v2, float *v3, float *v4)
{
	float nor[3], nor1[3], nor2[3], vec[4][2];
	
	/* define projection, do both trias apart, quad is undefined! */
	CalcNormFloat(v1, v2, v3, nor1);
	CalcNormFloat(v1, v3, v4, nor2);
	nor[0]= ABS(nor1[0]) + ABS(nor2[0]);
	nor[1]= ABS(nor1[1]) + ABS(nor2[1]);
	nor[2]= ABS(nor1[2]) + ABS(nor2[2]);

	if(nor[2] >= nor[0] && nor[2] >= nor[1]) {
		vec[0][0]= v1[0]; vec[0][1]= v1[1];
		vec[1][0]= v2[0]; vec[1][1]= v2[1];
		vec[2][0]= v3[0]; vec[2][1]= v3[1];
		vec[3][0]= v4[0]; vec[3][1]= v4[1];
	}
	else if(nor[1] >= nor[0] && nor[1]>= nor[2]) {
		vec[0][0]= v1[0]; vec[0][1]= v1[2];
		vec[1][0]= v2[0]; vec[1][1]= v2[2];
		vec[2][0]= v3[0]; vec[2][1]= v3[2];
		vec[3][0]= v4[0]; vec[3][1]= v4[2];
	}
	else {
		vec[0][0]= v1[1]; vec[0][1]= v1[2];
		vec[1][0]= v2[1]; vec[1][1]= v2[2];
		vec[2][0]= v3[1]; vec[2][1]= v3[2];
		vec[3][0]= v4[1]; vec[3][1]= v4[2];
	}
	
	/* linetests, the 2 diagonals have to instersect to be convex */
	if( IsectLL2Df(vec[0], vec[2], vec[1], vec[3]) > 0 ) return 1;
	return 0;
}


static BME_Poly *add_quadtri(BME_Mesh *bm, BME_Vert *v1, BME_Vert *v2, BME_Vert *v3, BME_Vert *v4){
	BME_Poly *nf;
	BME_Edge *edar[4];
	edar[0] = edar[1] = edar[2] = edar[3] = NULL;
	
	
	edar[0] = BME_disk_existedge(v1,v2);
	if(edar[0] == NULL) edar[0] = BME_ME(bm,v1,v2);
	edar[1] = BME_disk_existedge(v2,v3);
	if(edar[1] == NULL) edar[1] = BME_ME(bm,v2,v3);
	if(v4){
		edar[2] = BME_disk_existedge(v3,v4);
		if(edar[2] == NULL) edar[2] = BME_ME(bm,v3,v4);
		edar[3] = BME_disk_existedge(v4,v1);
		if(edar[3] == NULL) edar[3] = BME_ME(bm,v4,v1);
	}
	else{
		edar[2] = BME_disk_existedge(v3,v1);
		if(edar[2]==NULL) edar[2] = BME_ME(bm,v3,v1);
	}
	if(v4) nf = BME_MF(bm,v1,v2,edar,4);
	else nf = BME_MF(bm,v1,v2,edar,3);
	return nf;
}

/*finds out if any of the faces connected to any of the verts have all other vertices in varr as well. */
/*this can be MUCH more compact if we just use BME_VertFaceWalk from BME_traversals.c*/
static int exist_face_overlaps(BME_Mesh *bm, BME_Vert **varr, int len){
	BME_Edge *curedge;
	BME_Loop *curloop, *l;
	int i;
	/*loop through every face in every vert, if it contains all vertices in varr, its an overlap*/ 
	for(i=0;i<len;i++){
		if(varr[i]->edge){
			curedge = varr[i]->edge;
			do{
				if(curedge->loop){
					curloop = curedge->loop;
					do{
						int amount = 0;
						/*if amount of 'visited' verts == len then we have an overlap*/
						l = curloop->f->loopbase;
						do{
							if(BME_SELECTED(l->v)) amount++;
							l = l->next;
						}while(l != curloop->f->loopbase);
						if(amount >= len){
							if(len == curloop->f->len)  return 2; //existface
							else return 1; //overlap
						}
						curloop = BME_radial_nextloop(curloop);
					}while(curloop != curedge->loop);
				}
				curedge = BME_disk_nextedge(curedge,varr[i]);
			}while(curedge!=varr[i]->edge);
		}
	}
	return 0;
}


/* precondition; 4 vertices selected, check for 4 edges and create face */
static BME_Poly *addface_from_edges(BME_Mesh *bm)
{
	BME_Edge *e, *eedar[4]={NULL, NULL, NULL, NULL};
	BME_Vert *v1=NULL, *v2=NULL, *v3=NULL, *v4=NULL;
	int a;
	
	/* find the 4 edges */
	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		if(BME_SELECTED(e)) {
			if(eedar[0]==NULL) eedar[0]= e;
			else if(eedar[1]==NULL) eedar[1]= e;
			else if(eedar[2]==NULL) eedar[2]= e;
			else eedar[3]= e;
		}
	}
	
	if(eedar[3]) {
		/* first 2 points */
		v1= eedar[0]->v1;
		v2= eedar[0]->v2;
		
		/* find the 2 edges connected to first edge */
		for(a=1; a<4; a++) {
			if( eedar[a]->v1 == v2) v3= eedar[a]->v2;
			else if(eedar[a]->v2 == v2) v3= eedar[a]->v1;
			else if( eedar[a]->v1 == v1) v4= eedar[a]->v2;
			else if(eedar[a]->v2 == v1) v4= eedar[a]->v1;
		}
		
		/* verify if last edge exists */
		if(v3 && v4) {
			for(a=1; a<4; a++) {
				if( eedar[a]->v1==v3 && eedar[a]->v2==v4) break;
				if( eedar[a]->v2==v3 && eedar[a]->v1==v4) break;
			}
			if(a!=4) {
				return add_quadtri(bm,v1,v2,v3,v4);
			}
		}
	}
	return NULL;
}

static BME_Poly *make_ngon_from_selected(BME_Mesh *bm){
	
	BME_Edge *e, **edar;
	BME_Poly *nf= NULL;
	int i, edsel=0;
	
	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		if(BME_SELECTED(e)) edsel++;
	}		
	edar = MEM_callocN(sizeof(BME_Edge*)*edsel,"Add edgeface Ngon array");
	i = 0;		
	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		if(BME_SELECTED(e)){	
			edar[i] = e;
			i++;
		}
	}
	nf =  BME_MF(bm,edar[0]->v1,edar[0]->v2,edar,edsel);
	if(edar) MEM_freeN(edar);
	return nf;
}

int BME_make_edgeface(BME_Mesh *bm){

	BME_Vert *v, *newface[4];
	BME_Edge *e, **edar=NULL, *halt;
	BME_Loop *l;
	BME_Poly *f, *nf=NULL;
	int i, amount=0, facesel=0;


	if(bm->selectmode & SCE_SELECT_EDGE){ 
		/* in edge mode finding selected vertices means flushing down edge codes... */
		/* can't make face with only edge selection info... */
		BME_selectmode_set(bm);
	}	

	/*special exception here....  if there is one or more faces selected, we want to fuse them into one face*/
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		/*if all vertices of the face are selected, we need to mark it, and count. Run a seperate function on it to fuse faces*/
		l=f->loopbase;
		do{
			if(BME_SELECTED(l->v)) amount++;
			l=l->next;
		}while(l!=f->loopbase);
		
		if(amount == f->len) facesel++;
	}	

	//insert face fusing code here in case facesel > 1. if == 1, error.

	amount = 0;
	/*we make special excepton for quads and tris, only require vertex information to make them. For n-gons we need  closed loop of edges.*/
	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if(BME_SELECTED(v)){ 
			if(amount == 4){ 
				nf = make_ngon_from_selected(bm);
				break;
			}
			newface[amount] = v;
			amount++;
		}
	}
	
	if(!nf){
		/*if amount == 2, create edge*/
		if(amount == 2){
			if(!(BME_disk_existedge(newface[0],newface[1]))) BME_ME(bm,newface[0],newface[1]);
		}
		/*if amount == 3, triangle...*/
		else if(amount == 3){
			if(exist_face_overlaps(bm,newface,3) == 0) add_quadtri(bm,newface[0],newface[1],newface[2],NULL);
		}
		/*if amount == 4, quad...*/
		else if(amount == 4){
			if(exist_face_overlaps(bm,newface,4) == 0){
				/* if 4 edges exist, we just create the face, convex or not */
				nf= addface_from_edges(bm);
				if(nf==NULL) {
					if( convex(newface[0]->co, newface[1]->co, newface[2]->co, newface[3]->co) ) {
						nf= add_quadtri(bm, newface[0], newface[1], newface[2], newface[3]);
					}
					else if( convex(newface[0]->co, newface[2]->co, newface[3]->co, newface[1]->co) ) {
						nf= add_quadtri(bm, newface[0], newface[2], newface[3], newface[1]);
					}
					else if( convex(newface[0]->co, newface[2]->co, newface[1]->co, newface[3]->co) ) {
						nf= add_quadtri(bm, newface[0], newface[2], newface[1], newface[3]);
					}
					else if( convex(newface[1]->co, newface[2]->co, newface[3]->co, newface[0]->co) ) {
						nf= add_quadtri(bm, newface[1], newface[2], newface[3], newface[0]);
					}
					else if( convex(newface[1]->co, newface[3]->co, newface[0]->co, newface[2]->co) ) {
						nf= add_quadtri(bm, newface[1], newface[3], newface[0], newface[2]);
					}
					else if( convex(newface[1]->co, newface[3]->co, newface[2]->co, newface[0]->co) ) {
						nf= add_quadtri(bm, newface[1], newface[3], newface[2], newface[0]);
					}
					return -1;				
				}
			}
			return -1;;
		}
	}
	
	if(nf) {
		BME_select_poly(bm,nf, 1);
		//fix_new_face(efa); fix me!
		//recalc_editnormals(); fix me too!

	}
	BME_selectmode_flush(bm);
}


/********* qsort routines *********/


typedef struct xvertsort {
	float x;
	BME_Vert *v1;
} xvertsort;

static int vergxco(const void *v1, const void *v2)
{
	const xvertsort *x1=v1, *x2=v2;

	if( x1->x > x2->x ) return 1;
	else if( x1->x < x2->x) return -1;
	return 0;
}

struct facesort {
	unsigned long x;
	struct BME_Poly *f;
};


static int vergface(const void *v1, const void *v2)
{
	const struct facesort *x1=v1, *x2=v2;
	
	if( x1->x > x2->x ) return 1;
	else if( x1->x < x2->x) return -1;
	return 0;
}



/*break this into two functions.... 'find doubles' and 'remove doubles'?*/

static void BME_remove_doubles__splitface(BME_Mesh *bm,BME_Poly *f,GHash *vhash){
	BME_Vert *doub=NULL, *target=NULL;
	BME_Loop *l;
	BME_Poly *f2=NULL;
	int split=0;
	
	l=f->loopbase;
	do{
		if(l->v->tflag1 == 2){
			target = BLI_ghash_lookup(vhash,l->v);
			if((BME_vert_in_face(target,f)) && (target != l->next->v) && (target != l->prev->v)){
				doub = l->v;
				split = 1;
				break;
			}
		}
		
		l= l->next;
	}while(l!= f->loopbase);

	if(split){
		f2 = BME_SFME(bm,f,doub,target,NULL);
		BME_remove_doubles__splitface(bm,f,vhash);
		BME_remove_doubles__splitface(bm,f2,vhash);
	}
}

int BME_remove_doubles(BME_Mesh *bm, float limit)		
{

	/* all verts with (flag & 'flag') are being evaluated */
	BME_Vert *v, *v2, *target;
	BME_Edge *e, **edar, *ne;
	BME_Loop *l;
	BME_Poly *f, *nf;
	xvertsort *sortblock, *sb, *sb1;
	struct GHash *vhash;
	struct facesort *fsortblock, *vsb, *vsb1;
	int a, b, test, amount=0, found;
	float dist;

	/*Build a hash table of doubles to thier target vert/edge.*/
	vhash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);	
	
	/*count amount of selected vertices*/
	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if(BME_SELECTED(v))amount++;
	}
	
	/*qsort vertices based upon average of coordinate. We test this way first.*/
	sb= sortblock= MEM_mallocN(sizeof(xvertsort)*amount,"sortremovedoub");

	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if(BME_SELECTED(v)){
			sb->x = v->co[0]+v->co[1]+v->co[2];
			sb->v1 = v;
			sb++;
		}
	}
	qsort(sortblock, amount, sizeof(xvertsort), vergxco);

	/* test for doubles */
	sb= sortblock;
	for(a=0; a<amount; a++) {
		v= sb->v1;
		if(!(v->tflag1)) { //have we tested yet?
			sb1= sb+1;
			for(b=a+1; b<amount; b++) {
				/* first test: simple distance. Simple way to discard*/
				dist= sb1->x - sb->x;
				if(dist > limit) break;
				
				/* second test: have we already done this vertex?
				(eh this should be swapped, simple equality test should be cheaper than math above... small savings 
				though) */
				v2= sb1->v1;
				if(!(v2->tflag1)) {
					dist= (float)fabs(v2->co[0]-v->co[0]);
					if(dist<=limit) {
						dist= (float)fabs(v2->co[1]-v->co[1]);
						if(dist<=limit) {
							dist= (float)fabs(v2->co[2]-v->co[2]);
							if(dist<=limit) {
								/*v2 is a double of v. We want to throw out v1 and relink everything to v*/
								BLI_ghash_insert(vhash,v2, v);
								v->tflag1 = 1; //mark this vertex as a target
								v->tflag2++; //increase user count for this vert.
								v2->tflag1 = 2; //mark this vertex as a double.
								BME_VISIT(v2); //mark for delete
							}
						}
					}
				}
				sb1++;
			}
		}
		sb++;
	}
	MEM_freeN(sortblock);

	
	/*todo... figure out what this is for...
	for(eve = em->verts.first; eve; eve=eve->next)
		if((eve->f & flag) && (eve->f & 128))
			EM_data_interp_from_verts(eve, eve->tmp.v, eve->tmp.v, 0.5f);
	*/
	
	/*We cannot collapse a vertex onto another vertex if they share a face and are not connected via a collapsable edge.
		so to deal with this we simply find these offending vertices and split the faces. Note that this is not optimal, but works.
	*/
	
	
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		if(!(BME_NEWELEM(f))){
			BME_remove_doubles__splitface(bm,f,vhash);
		}
	}
	
	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		/*If either vertices of this edge are a double, we must mark it for removal and we create a new one.*/
		if(e->v1->tflag1 == 2 || e->v2->tflag1 == 2){ 
			v = v2 = NULL;
			/*For each vertex in the edge, test to find out what it should equal now.*/
			if(e->v1->tflag1 == 2) v= BLI_ghash_lookup(vhash,e->v1);
			else v = e->v1;
			if(e->v2->tflag1 == 2) v2 = BLI_ghash_lookup(vhash,e->v2);
			else v2 = e->v2;
			
			/*small optimization, test to see if the edge needs to be rebuilt at all*/
			if((e->v1 != v) || (e->v2 != v2)){ /*will this always be true of collapsed edges?*/
				if(v == v2) e->tflag1 = 2; /*mark as a collapsed edge*/
				else if(!BME_disk_existedge(v,v2)) ne = BME_ME(bm,v,v2);
				BME_VISIT(e); /*mark for delete*/
			}
		}
	}


	/*need to remove double edges as well. To do this we decide on one edge to keep, and if its inserted into hash then we need to remove all other
	edges incident upon and relink.*/
	/*
	*	REBUILD FACES
	*
	*	Loop through double faces and if they have vertices that have been flagged, they need to be rebuilt.
	*	We do this by looking up the face 
	*rebuild faces. loop through original face, for each loop, if the edge it is attached to is marked for delete and has no 
	*other edge in the hash edge, then we know to skip that loop on face recreation. Simple.
	*/
	
	/*1st loop through, just marking elements*/
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){ //insert bit here about double edges, mark with a flag (e->tflag2) so that we can nuke it later.
		l = f->loopbase;
		do{
			if(l->v->tflag1 == 2) f->tflag1 = 1; //double, mark for rebuild
			if(l->e->tflag1 != 2) f->tflag2++; //count number of edges in the new face.
			l=l->next;
		}while(l!=f->loopbase);
	}
	
	/*now go through and create new faces*/
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		if(f->tflag1 && f->tflag2 < 3) BME_VISIT(f); //mark for delete
		else if (f->tflag1 == 1){ /*is the face marked for rebuild*/
			edar = MEM_callocN(sizeof(BME_Edge *)*f->tflag2,"Remove doubles face creation array.");
			a=0;
			l = f->loopbase;
			do{
				v = l->v;
				v2 = l->next->v;
				if(l->v->tflag1 == 2) v = BLI_ghash_lookup(vhash,l->v);
				if(l->next->v->tflag1 == 2) v2 = BLI_ghash_lookup(vhash,l->next->v);
				ne = BME_disk_existedge(v,v2); //use BME_disk_next_edgeflag here or something to find the edge that is marked as 'target'.
				//add in call here to edge doubles hash array... then bobs your uncle.
				if(ne){
					edar[a] = ne;
					a++;
				}
				l=l->next;
			}while(l!=f->loopbase);
			
			if(BME_vert_in_edge(edar[1],edar[0]->v2)){ 
				v = edar[0]->v1; 
				v2 = edar[0]->v2;
			}
			else{ 
				v = edar[0]->v2; 
				v2 = edar[0]->v1;
			}
			
			nf = BME_MF(bm,v,v2,edar,f->tflag2);
	
			/*copy per loop data here*/
			if(nf){
				BME_VISIT(f); //mark for delete
			}
			MEM_freeN(edar);
		}
	}
	
	/*count amount of removed vert doubles*/
	a = 0;
	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if(v->tflag1 == 2) a++;
	}
	
	/*free memory and return amount removed*/
	remove_tagged_polys(bm);
	remove_tagged_edges(bm);
	remove_tagged_verts(bm);
	BLI_ghash_free(vhash,NULL, NULL);
	BME_selectmode_flush(bm);
	return a;
}

static void BME_MeshWalk__collapsefunc(void *userData, BME_Edge *applyedge){
	int index;
	GHash *collected = userData;
	index = BLI_ghash_size(collected);
	if(!BLI_ghash_lookup(collected,applyedge->v1)){ 
		BLI_ghash_insert(collected,index,applyedge->v1);
		index++;
	}
	if(!BLI_ghash_lookup(collected,applyedge->v2)){
		BLI_ghash_insert(collected,index,applyedge->v2);
	}
}

void BME_collapse_edges(BME_Mesh *bm){

	BME_Vert *v, *cvert;
	GHash *collected;
	float min[3], max[3], cent[3];
	int size, i=0, j, num=0;
	
	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if(!(BME_ISVISITED(v)) && v->edge){
			/*initiate hash table*/
			collected = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
			/*do the walking.*/
			BME_MeshWalk(bm,v,BME_MeshWalk__collapsefunc,collected,BME_RESTRICTSELECT);
			/*now loop through the hash table twice, once to calculate bounding box, second time to do the actual collapse*/
			size = BLI_ghash_size(collected);
			/*initial values*/
			VECCOPY(min,v->co);
			VECCOPY(max,v->co);
			cent[0] = cent[1] = cent[2]=0;
			for(i=0; i<size; i++){
				cvert = BLI_ghash_lookup(collected,i);
				cent[0] = cent[0] + cvert->co[0];
				cent[1] = cent[1] + cvert->co[1];
				cent[2] = cent[2] + cvert->co[2];
			}
			
			cent[0] = cent[0] / size;
			cent[1] = cent[1] / size;
			cent[2] = cent[2] / size;
			
			for(i=0; i<size; i++){
				cvert = BLI_ghash_lookup(collected,i);
				VECCOPY(cvert->co,cent);
				num++;
			}
			/*free the hash table*/
			BLI_ghash_free(collected,NULL, NULL);
		}
	}
	/*if any collapsed, call remove doubles*/
	if(num){
		//need to change selection mode here, OR do something else? Or does tool change selection mode?
		//selectgrep
		//first clear flags
		BME_Edge *e;
		BME_Poly *f;
		BME_clear_flag_all(bm,BME_VISITED);
		for(v=BME_first(bm,BME_VERT); v; v=BME_next(bm,BME_VERT,v)) v->tflag1 = v->tflag2 = 0;
		for(e=BME_first(bm,BME_EDGE); e; e=BME_next(bm,BME_EDGE,e)) e->tflag1 = e->tflag2 = 0;
		for(f=BME_first(bm,BME_POLY); f; f=BME_next(bm,BME_POLY,f)) f->tflag1 = f->tflag2 = 0;
		/*now call remove doubles*/
		BME_remove_doubles(bm,0.0000001);
	}
	BME_selectmode_flush(bm);
}

void BME_split_mesh(BME_Mesh *bm){
	BME_Vert *v;
	BME_Edge *e;
	BME_Poly *f;
	
	/*first mark all selected elements*/
	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if(BME_SELECTED(v)) v->tflag1 = 1;
	}
	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		if(BME_SELECTED(e)) e->tflag1 = 1;
	}
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		if(BME_SELECTED(f)) f->tflag1 = 1;
	}
	
	BME_duplicate(bm); //dupe the selected parts.
	BME_clear_flag_all(bm,BME_VISITED); //clear visited flags set in the duplicate function
	/*now go through and visit all the previously selected parts and delete them*/
	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if(v->tflag1) BME_VISIT(v);
	}
	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		if(e->tflag1) BME_VISIT(e);
	}
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		if(f->tflag1) BME_VISIT(f);
	}
	
	remove_tagged_polys(bm);
	remove_tagged_edges(bm);
	remove_tagged_verts(bm);
}



void BME_vertex_smooth(BME_Mesh *bm)
{
	BME_Vert *v;
	BME_Edge *e;
	float *adror, *adr, fac;
	float fvec[3];
	int selected=0;
	GHash *vhash;

	/* count */
	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if(BME_SELECTED(v)) selected++;
	}
	
	if(!selected) return 0;
	
	vhash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	adr=adror= (float *)MEM_callocN(3*sizeof(float *)*selected, "vertsmooth");
	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if(BME_SELECTED(v)){
			BLI_ghash_insert(vhash,v,adr);
			adr+= 3;
		}
	}

	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		if((BME_SELECTED(e->v1)) || (BME_SELECTED(e->v2))){
			fvec[0]= (e->v1->co[0]+e->v2->co[0])/2.0;
			fvec[1]= (e->v1->co[1]+e->v2->co[1])/2.0;
			fvec[2]= (e->v1->co[2]+e->v2->co[2])/2.0;
			
			if((BME_SELECTED(e->v1)) && e->v1->tflag1<255){
				e->v1->tflag1++;
				VecAddf(BLI_ghash_lookup(vhash,e->v1), BLI_ghash_lookup(vhash,e->v1), fvec);				
			}
			if((BME_SELECTED(e->v2)) && e->v2->tflag1<255){
				e->v2->tflag1++;
				VecAddf(BLI_ghash_lookup(vhash,e->v2), BLI_ghash_lookup(vhash,e->v2), fvec);
			}
		}
	}
	
	for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_EDGE,e)){
		if( (BME_SELECTED(e->v1)) || (BME_SELECTED(e->v2)) ){
			fvec[0]= (e->v1->co[0]+e->v2->co[0])/2.0;
			fvec[1]= (e->v1->co[1]+e->v2->co[1])/2.0;
			fvec[2]= (e->v1->co[2]+e->v2->co[2])/2.0;
		}
	}
	
	for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)){
		if( (BME_SELECTED(v)) ){
			if(v->tflag1){
				adr = BLI_ghash_lookup(vhash,v);
				fac= 0.5/(float)v->tflag1;
				
				v->co[0]= 0.5*v->co[0]+fac*adr[0];
				v->co[1]= 0.5*v->co[1]+fac*adr[1];
				v->co[2]= 0.5*v->co[2]+fac*adr[2];
			}
		}
	}
	BLI_ghash_free(vhash,NULL, NULL);
	MEM_freeN(adror);

	//recalc_editnormals(); replace me!
}
