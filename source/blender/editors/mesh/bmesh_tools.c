 /* $Id: bmesh_tools.c
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
 * The Original Code is Copyright (C) 2004 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"
#include "PIL_time.h"

#include "BLO_sys_types.h" // for intptr_t support

#include "DNA_mesh_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_key_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_types.h"
#include "RNA_define.h"
#include "RNA_access.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_heap.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"
#include "BKE_bmesh.h"
#include "BKE_report.h"
#include "BKE_tessmesh.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BMF_Api.h"

#include "ED_mesh.h"
#include "ED_view3d.h"
#include "ED_util.h"
#include "ED_screen.h"
#include "BIF_transform.h"

#include "UI_interface.h"

#include "mesh_intern.h"
#include "bmesh.h"

static int subdivide_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	
	BM_esubdivideflag(obedit, ((Mesh *)obedit->data)->edit_btmesh->bm, 
		1, 0.0, scene->toolsettings->editbutflag, 1, 0);
		
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;	
}

void MESH_OT_subdivide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Subdivide";
	ot->idname= "MESH_OT_subdivide";
	
	/* api callbacks */
	ot->exec= subdivide_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int subdivide_multi_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;
	
	BM_esubdivideflag(obedit, em->bm, 1, 0.0, scene->toolsettings->editbutflag, RNA_int_get(op->ptr,"number_cuts"), 0);
		
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;	
}

void MESH_OT_subdivide_multi(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Subdivide Multi";
	ot->idname= "MESH_OT_subdivide_multi";
	
	/* api callbacks */
	ot->exec= subdivide_multi_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_int(ot->srna, "number_cuts", 4, 1, 100, "Number of Cuts", "", 1, INT_MAX);
}

static int subdivide_multi_fractal_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;

	BM_esubdivideflag(obedit, em->bm, 1, -(RNA_float_get(op->ptr, "random_factor")/100), scene->toolsettings->editbutflag, RNA_int_get(op->ptr, "number_cuts"), 0);
		
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;	
}

void MESH_OT_subdivide_multi_fractal(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Subdivide Multi Fractal";
	ot->idname= "MESH_OT_subdivide_multi_fractal";
	
	/* api callbacks */
	ot->exec= subdivide_multi_fractal_exec;
	ot->poll= ED_operator_editmesh;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_int(ot->srna, "number_cuts", 4, 1, 100, "Number of Cuts", "", 1, INT_MAX);
	RNA_def_float(ot->srna, "random_factor", 5.0, 0.0f, FLT_MAX, "Random Factor", "", 0.0f, 1000.0f);
}

static int subdivide_smooth_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;

	BM_esubdivideflag(obedit, em->bm, 1, 0.292f*RNA_float_get(op->ptr, "smoothness"), scene->toolsettings->editbutflag | B_SMOOTH, 1, 0);
		
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;	
}

void MESH_OT_subdivide_smooth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Subdivide Smooth";
	ot->idname= "MESH_OT_subdivide_smooth";
	
	/* api callbacks */
	ot->exec= subdivide_smooth_exec;
	ot->poll= ED_operator_editmesh;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_float(ot->srna, "smoothness", 1.0f, 0.0f, 1000.0f, "Smoothness", "", 0.0f, FLT_MAX);
}

static int subdivs_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	uiMenuItem *head;

	head= uiPupMenuBegin("Subdivision Type", 0);
	uiMenuItemsEnumO(head, "MESH_OT_subdivs", "type");
	uiPupMenuEnd(C, head);
	
	return OPERATOR_CANCELLED;
}

static int subdivs_exec(bContext *C, wmOperator *op)
{	
	switch(RNA_int_get(op->ptr, "type"))
	{
		case 0: // simple
			subdivide_exec(C,op);
			break;
		case 1: // multi
			subdivide_multi_exec(C,op);
			break;
		case 2: // fractal;
			subdivide_multi_fractal_exec(C,op);
			break;
		case 3: //smooth
			subdivide_smooth_exec(C,op);
			break;
	}
					 
	return OPERATOR_FINISHED;
}

void MESH_OT_subdivs(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[]= {
		{0, "SIMPLE", "Simple", ""},
		{1, "MULTI", "Multi", ""},
		{2, "FRACTAL", "Fractal", ""},
		{3, "SMOOTH", "Smooth", ""},
		{0, NULL, NULL}};

	/* identifiers */
	ot->name= "subdivs";
	ot->idname= "MESH_OT_subdivs";
	
	/* api callbacks */
	ot->invoke= subdivs_invoke;
	ot->exec= subdivs_exec;
	
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/*props */
	RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "");
	
	/* this is temp, the ops are different, but they are called from subdivs, so all the possible props should be here as well*/
	RNA_def_int(ot->srna, "number_cuts", 4, 1, 10, "Number of Cuts", "", 1, INT_MAX);
	RNA_def_float(ot->srna, "random_factor", 5.0, 0.0f, FLT_MAX, "Random Factor", "", 0.0f, 1000.0f);
	RNA_def_float(ot->srna, "smoothness", 1.0f, 0.0f, 1000.0f, "Smoothness", "", 0.0f, FLT_MAX);
		
}

