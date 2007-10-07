/**
 * editbmesh_select.c    July 2007
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

#include "BSE_edit.h"

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

void EM_deselectall_mesh(void){
	BME_Vert *v;
	BME_Edge *e;
	BME_Poly *f;
	
	int select;

	if(G.obedit->lay & G.vd->lay){
		if(G.totvertsel) select = 0;
		else select = 1;
		for(v=BME_first(G.editMesh,BME_VERT);v;v=BME_next(G.editMesh,BME_VERT,v))BME_select_vert(G.editMesh,v,select);
		if(G.totedgesel) select = 0;
		else select = 1;
		for(e=BME_first(G.editMesh,BME_EDGE);e;e=BME_next(G.editMesh,BME_EDGE,e))BME_select_edge(G.editMesh,e,select);
		if(G.totfacesel) select = 0;
		else select = 1;
		for(f=BME_first(G.editMesh,BME_POLY);f;f=BME_next(G.editMesh,BME_POLY,f))BME_select_poly(G.editMesh,f,select);
	}
	
	countall();
	DAG_object_flush_update(G.scene,G.obedit,OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D,0);
}