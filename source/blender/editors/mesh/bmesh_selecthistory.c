/**
 * $Id: 
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* ********* Selection History ************ */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "MTC_matrixops.h"

#include "DNA_mesh_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_context.h"
#include "BKE_displist.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_report.h"
#include "BKE_tessmesh.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "RE_render_ext.h"  /* externtex */

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "bmesh.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "mesh_intern.h"

#include "BLO_sys_types.h" // for intptr_t support

/* XXX */
static void waitcursor() {}
static int pupmenu() {return 0;}

/* generic way to get data from an EditSelection type 
These functions were written to be used by the Modifier widget when in Rotate about active mode,
but can be used anywhere.
EM_editselection_center
EM_editselection_normal
EM_editselection_plane
*/
void EDBM_editselection_center(BMEditMesh *em, float *center, BMEditSelection *ese)
{
	if (ese->type==BM_VERT) {
		BMVert *eve= ese->data;
		VecCopyf(center, eve->co);
	} else if (ese->type==BM_EDGE) {
		BMEdge *eed= ese->data;
		VecAddf(center, eed->v1->co, eed->v2->co);
		VecMulf(center, 0.5);
	} else if (ese->type==BM_FACE) {
		BMFace *efa= ese->data;
		BM_Compute_Face_Center(em->bm, efa, center);
	}
}

void EDBM_editselection_normal(float *normal, BMEditSelection *ese)
{
	if (ese->type==BM_VERT) {
		BMVert *eve= ese->data;
		VecCopyf(normal, eve->no);
	} else if (ese->type==BM_EDGE) {
		BMEdge *eed= ese->data;
		float plane[3]; /* need a plane to correct the normal */
		float vec[3]; /* temp vec storage */
		
		VecAddf(normal, eed->v1->no, eed->v2->no);
		VecSubf(plane, eed->v2->co, eed->v1->co);
		
		/* the 2 vertex normals will be close but not at rightangles to the edge
		for rotate about edge we want them to be at right angles, so we need to
		do some extra colculation to correct the vert normals,
		we need the plane for this */
		Crossf(vec, normal, plane);
		Crossf(normal, plane, vec); 
		Normalize(normal);
		
	} else if (ese->type==BM_FACE) {
		BMFace *efa= ese->data;
		VecCopyf(normal, efa->no);
	}
}

/* Calculate a plane that is rightangles to the edge/vert/faces normal
also make the plane run allong an axis that is related to the geometry,
because this is used for the manipulators Y axis.*/
void EDBM_editselection_plane(BMEditMesh *em, float *plane, BMEditSelection *ese)
{
	if (ese->type==BM_VERT) {
		BMVert *eve= ese->data;
		float vec[3]={0,0,0};
		
		if (ese->prev) { /*use previously selected data to make a usefull vertex plane */
			EDBM_editselection_center(em, vec, ese->prev);
			VecSubf(plane, vec, eve->co);
		} else {
			/* make a fake  plane thats at rightangles to the normal
			we cant make a crossvec from a vec thats the same as the vec
			unlikely but possible, so make sure if the normal is (0,0,1)
			that vec isnt the same or in the same direction even.*/
			if (eve->no[0]<0.5)		vec[0]=1;
			else if (eve->no[1]<0.5)	vec[1]=1;
			else				vec[2]=1;
			Crossf(plane, eve->no, vec);
		}
	} else if (ese->type==BM_EDGE) {
		BMEdge *eed= ese->data;

		/*the plane is simple, it runs allong the edge
		however selecting different edges can swap the direction of the y axis.
		this makes it less likely for the y axis of the manipulator
		(running along the edge).. to flip less often.
		at least its more pradictable */
		if (eed->v2->co[1] > eed->v1->co[1]) /*check which to do first */
			VecSubf(plane, eed->v2->co, eed->v1->co);
		else
			VecSubf(plane, eed->v1->co, eed->v2->co);
		
	} else if (ese->type==BM_FACE) {
		BMFace *efa= ese->data;
		float vec[3] = {0.0f, 0.0f, 0.0f};
		
		/*for now, use face normal*/

		/* make a fake  plane thats at rightangles to the normal
		we cant make a crossvec from a vec thats the same as the vec
		unlikely but possible, so make sure if the normal is (0,0,1)
		that vec isnt the same or in the same direction even.*/
		if (efa->no[0]<0.5)		vec[0]=1.0f;
		else if (efa->no[1]<0.5)	vec[1]=1.0f;
		else				vec[2]=1.0f;
		Crossf(plane, efa->no, vec);
#if 0

		if (efa->v4) { /*if its a quad- set the plane along the 2 longest edges.*/
			float vecA[3], vecB[3];
			VecSubf(vecA, efa->v4->co, efa->v3->co);
			VecSubf(vecB, efa->v1->co, efa->v2->co);
			VecAddf(plane, vecA, vecB);
			
			VecSubf(vecA, efa->v1->co, efa->v4->co);
			VecSubf(vecB, efa->v2->co, efa->v3->co);
			VecAddf(vec, vecA, vecB);						
			/*use the biggest edge length*/
			if (plane[0]*plane[0]+plane[1]*plane[1]+plane[2]*plane[2] < vec[0]*vec[0]+vec[1]*vec[1]+vec[2]*vec[2])
				VecCopyf(plane, vec);
		} else {
			/*start with v1-2 */
			VecSubf(plane, efa->v1->co, efa->v2->co);
			
			/*test the edge between v2-3, use if longer */
			VecSubf(vec, efa->v2->co, efa->v3->co);
			if (plane[0]*plane[0]+plane[1]*plane[1]+plane[2]*plane[2] < vec[0]*vec[0]+vec[1]*vec[1]+vec[2]*vec[2])
				VecCopyf(plane, vec);
			
			/*test the edge between v1-3, use if longer */
			VecSubf(vec, efa->v3->co, efa->v1->co);
			if (plane[0]*plane[0]+plane[1]*plane[1]+plane[2]*plane[2] < vec[0]*vec[0]+vec[1]*vec[1]+vec[2]*vec[2])
				VecCopyf(plane, vec);
		}
#endif
	}
	Normalize(plane);
}

static int EDBM_check_selection(BMEditMesh *em, void *data)
{
	BMEditSelection *ese;
	
	for(ese = em->selected.first; ese; ese = ese->next){
		if(ese->data == data) return 1;
	}
	
	return 0;
}

void EDBM_remove_selection(BMEditMesh *em, void *data)
{
	BMEditSelection *ese;
	for(ese=em->selected.first; ese; ese = ese->next){
		if(ese->data == data){
			BLI_freelinkN(&(em->selected),ese);
			break;
		}
	}
}

void EDBM_store_selection(BMEditMesh *em, void *data)
{
	BMEditSelection *ese;
	if(!EDBM_check_selection(em, data)){
		ese = (BMEditSelection*) MEM_callocN( sizeof(BMEditSelection), "BMEdit Selection");
		ese->type = ((BMHeader*)data)->type;
		ese->data = data;
		BLI_addtail(&(em->selected),ese);
	}
}

void EDBM_validate_selections(BMEditMesh *em)
{
	BMEditSelection *ese, *nextese;

	ese = em->selected.first;

	while(ese){
		nextese = ese->next;
		if (!BM_TestHFlag(ese->data, BM_SELECT)) BLI_freelinkN(&(em->selected), ese);
		ese = nextese;
	}
}
