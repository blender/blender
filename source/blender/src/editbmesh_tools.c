/**
 * editbmesh_tools.c    July 2007
 *
 *	Blender Editmode Tools Interface
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
 **/

#include "MEM_guardedalloc.h"

#include "BKE_bmesh.h"
#include "BKE_mesh.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_depsgraph.h"
#include "BKE_utildefines.h"
#include "BKE_object.h"

#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_customdata_types.h"
#include "DNA_view3d_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BLI_PointerArray.h"
#include "BLI_memarena.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_resources.h"
#include "BIF_language.h"
#include "BIF_transform.h"

#include "BDR_editobject.h"
#include "BDR_drawobject.h"

#include "BSE_drawview.h"

#include "mydevice.h"

void EM_cut_edges(int numcuts){
	BME_model_begin(G.editMesh);
	BME_cut_edges(G.editMesh,numcuts);
	BME_model_end(G.editMesh);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 0);
}

void EM_dissolve_edges(void){
	BME_model_begin(G.editMesh);
	BME_dissolve_edges(G.editMesh);	
	BME_model_end(G.editMesh);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 0);
}
void EM_connect_verts(void){
	BME_model_begin(G.editMesh);
	BME_connect_verts(G.editMesh);
	BME_model_end(G.editMesh);
	DAG_object_flush_update(G.scene,G.obedit,OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D,0);
}

void EM_delete_context(void){
	BME_model_begin(G.editMesh);
	if(G.scene->selectmode == SCE_SELECT_VERTEX) BME_delete_verts(G.editMesh);
	else if(G.scene->selectmode == SCE_SELECT_EDGE) BME_delete_edges(G.editMesh);
	else if(G.scene->selectmode == SCE_SELECT_FACE) BME_delete_polys(G.editMesh);
	BME_model_end(G.editMesh);
	DAG_object_flush_update(G.scene,G.obedit,OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D,0);
}

void EM_extrude_mesh(void){
	float nor[3]= {0.0, 0.0, 0.0};
	short transmode=0,extrudemode=0; //1 = verts, 2, = edges, 3 = faces, 4 = individual faces... should actually be inset then move along individual normals?
	
	BME_model_begin(G.editMesh);
	if(G.scene->selectmode == SCE_SELECT_VERTEX){ 
		transmode = BME_extrude_verts(G.editMesh);
		if(transmode) extrudemode = 1;
	}
	else if(G.scene->selectmode == SCE_SELECT_EDGE){
		transmode = BME_extrude_edges(G.editMesh);
		if(transmode) extrudemode = 2;
	}
	
	if(transmode){
		/* We need to force immediate calculation here because 
		* transform may use derived objects (which are now stale).
		*
		* This shouldn't be necessary, derived queries should be
		* automatically building this data if invalid. Or something.
		*/
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);	
		object_handle_update(G.obedit);

		/* individual faces? */
		BIF_TransformSetUndo("Extrude");
		/*if(nr==2) {
			initTransform(TFM_SHRINKFATTEN, CTX_NO_PET);
			Transform();
		}
		else {
			initTransform(TFM_TRANSLATION, CTX_NO_PET);
			if(transmode=='n') {
				Mat4MulVecfl(G.obedit->obmat, nor);
				VecSubf(nor, nor, G.obedit->obmat[3]);
				BIF_setSingleAxisConstraint(nor, NULL);
			}
			Transform();
		}*/
		
		initTransform(TFM_TRANSLATION,CTX_NO_PET);
		Transform();
	}	
	BME_model_end(G.editMesh);
	DAG_object_flush_update(G.scene,G.obedit,OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D,0);
}

