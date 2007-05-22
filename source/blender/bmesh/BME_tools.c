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
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"

#include "BKE_utildefines.h"
#include "BKE_bmesh.h"
#include "BLI_blenlib.h"

/**
 *			BME_dissolve_edge
 *
 *	Edge Dissolve Function:
 *	
 *	Dissolves a 2-manifold edge by joining it's two faces. if 
 *	they have opposite windings it first makes them consistent 
 *	by calling BME_loop_reverse()
 *
 *	Returns -
*/

/**
 *			BME_inset_edge
 *
 *	Edge Inset Function:
 *	
 *	Splits a face in two along an edge and returns the next loop 
 *
 *	Returns -
 *	A BME_Poly pointer.
 */

BME_Loop *BME_inset_edge(BME_Mesh *bm, BME_Loop *l, BME_Poly *f){
	BME_Loop *nloop;
	BME_SFME(bm, f, l->v, l->next->v, &nloop);
	return nloop->next; 
}

/**
 *			BME_inset_poly
 *
 *	Face Inset Tool:
 *	
 *	Insets a single face and returns a pointer to the face at the 
 *	center of the newly created region
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