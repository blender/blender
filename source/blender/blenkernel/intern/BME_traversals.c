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

int BME_test_restrictflags(BME_Mesh *bm, BME_Edge *e, short restrictflag){
	if(restrictflag & BME_RESTRICTWIRE){
		if(e->loop) return 0;
	}
	if(restrictflag & BME_RESTRICTSELECT){
		if(!(BME_SELECTED(e))) return 0;
	}
	if(restrictflag & BME_RESTRICTVISIT){
		if(BME_ISVISITED(e)) return 0;
	}
	return 1;
}

/*generic functions for iteration around a mesh*/

void BME_MeshLoop_walk(BME_Mesh *bm, BME_Edge *se, void*(*func)(void *userData, BME_Edge *walkedge, BME_Vert *walkvert), void *userData, short restrictflag){
	BME_Edge *nexte;
	BME_VISIT(se);
	/*find next edge for v1 and walk*/
	nexte = func(userData,se,se->v1);
	if(nexte && BME_test_restrictflags(bm,nexte,restrictflag) && !(BME_ISVISITED(nexte))) BME_MeshLoop_walk(bm,nexte,func,userData,restrictflag);
	/*find next edge for v2 and walk*/
	nexte = func(userData,se,se->v2);
	if(nexte && BME_test_restrictflags(bm,nexte,restrictflag) && !(BME_ISVISITED(nexte))) BME_MeshLoop_walk(bm,nexte,func,userData,restrictflag);
} 

void BME_MeshRing_walk(BME_Mesh *bm, BME_Edge *se, void*(*func)(void *userData, BME_Loop *walkloop), void *userData, short restrictflag)
{
	BME_Edge *nexte;
	BME_VISIT(se);
	
	if((se->loop) && (BME_cycle_length(&(se->loop->radial)) < 3)){
		nexte = func(userData,se->loop);
		if(nexte && BME_test_restrictflags(bm,nexte,restrictflag) && !(BME_ISVISITED(nexte))) BME_MeshRing_walk(bm,nexte,func,userData,restrictflag);
		nexte = func(userData,se->loop->radial.next->data);
		if(nexte && BME_test_restrictflags(bm,nexte,restrictflag) && !(BME_ISVISITED(nexte))) BME_MeshRing_walk(bm,nexte,func, userData,restrictflag);
	}
}

void BME_MeshWalk(BME_Mesh *bm, BME_Vert *v, void (*func)(void *userData, BME_Edge *apply), void *userData, short restrictflag){ 
	
	BME_Edge *curedge;
	
	if(!(BME_ISVISITED(v)) && (v->edge)){
		BME_VISIT(v);
		curedge = v->edge;
		do{
			if(BME_test_restrictflags(bm,curedge,restrictflag)){
				if(func) func(userData,curedge);
				BME_MeshWalk(bm,BME_edge_getothervert(curedge,v),func,userData,restrictflag);
			}
			curedge = BME_disk_nextedge(curedge,v);
		}while(curedge != v->edge);
	}
}


/*Edge Loop Callback for BME_MeshLoop_walk*/
BME_Edge *BME_edgeloop_nextedge(void *userData, BME_Edge *e, BME_Vert *sv)
{
        BME_CycleNode *diskbase;
        BME_Edge *curedge=NULL;
        int valance=0, radlen;
		
	diskbase = BME_disk_getpointer(e,sv);
        valance = BME_cycle_length(diskbase);

		if(valance == 4){
			//first some verification
			curedge = sv->edge;
			do{
				if(curedge->loop){
					radlen = BME_cycle_length(&(curedge->loop->radial));
					if(radlen != 2) return NULL;
				}else return NULL;
				curedge = BME_disk_nextedge(curedge,sv);
			}while(curedge != sv->edge);
			
			
			curedge = sv->edge;
			do{
				if(curedge != e && !(BME_edge_shareface(e,curedge))) return curedge;
				curedge = BME_disk_nextedge(curedge,sv);
			}while(curedge != sv->edge);
        }
        return NULL;
}

/*Shell Boundary callback for BME_MeshLoop_walk*/
BME_Edge *BME_edgeshell_nextedge(void *userData, BME_Edge *e, BME_Vert *sv)
{
        BME_Edge *curedge=NULL, *nexte=NULL;
        int valance=0, radlen;
		
		curedge = sv->edge;
		do{
			if(curedge->loop){
				radlen = BME_cycle_length(&(curedge->loop->radial));
				if(radlen == 1 && curedge != e){
					valance++;
					nexte = curedge;
				}
			}else return NULL;
			curedge = BME_disk_nextedge(curedge,sv);
		}while(curedge != sv->edge);
		
		if(valance == 1 && (!BME_edge_shareface(e,nexte))) return nexte;
		
        return NULL;
}

/*edge ring callback for BME_MeshRing_walk*/
BME_Edge *BME_edgering_nextedge(void *userData, BME_Loop *walkloop)
{
	BME_Edge *nexte=NULL;
	if(walkloop->f->len == 4) nexte = walkloop->next->next->e;
	return nexte;
}
