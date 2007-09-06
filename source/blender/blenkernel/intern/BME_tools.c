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

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"

#include "BKE_global.h"
#include "BKE_depsgraph.h"
#include "BKE_utildefines.h"
#include "BKE_bmesh.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include <windows.h> // for sleep, debug





/*Vertex Tools*/
void BME_connect_verts(BME_Mesh *bm)
{
	BME_Poly *f;
	BME_Loop *l;
	int split;
	
	/*visit the faces with selected verts*/
	for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)){
		split = 0;
		if(!(BME_NEWELEM(f))){
			l = f->loopbase;
			do{
				if(BME_SELECTED(l->v)){ 
					BME_VISIT(l->v);
					split ++;
				}
				l = l->next;
			}while(l != f->loopbase);
			
			if(split>1){ 
				BME_split_face(bm,f);
			}
		}
	}
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

void BME_cut_edge(BME_Mesh *bm, BME_Edge *e, int numcuts)
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
	





