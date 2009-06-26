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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_ID.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meta_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"

#include "BKE_action.h"
#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_idprop.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_transform.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_image.h"
#include "ED_keyframing.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_particle.h"
#include "ED_screen.h"
#include "ED_types.h"
#include "ED_util.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "view3d_intern.h"	// own include


/* ******************* view3d space & buttons ************** */


/* op->invoke */
static void redo_cb(bContext *C, void *arg_op, void *arg2)
{
	wmOperator *lastop= arg_op;
	
	if(lastop) {
		int retval;
		
		printf("operator redo %s\n", lastop->type->name);
		ED_undo_pop(C);
		retval= WM_operator_repeat(C, lastop);
		if((retval & OPERATOR_FINISHED)==0) {
			printf("operator redo failed %s\n", lastop->type->name);
			ED_undo_redo(C);
		}
	}
}

static void view3d_panel_operator_redo(const bContext *C, Panel *pa)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	wmOperator *op;
	PointerRNA ptr;
	uiBlock *block;
	
	block= uiLayoutGetBlock(pa->layout);

	/* only for operators that are registered and did an undo push */
	for(op= wm->operators.last; op; op= op->prev)
		if((op->type->flag & OPTYPE_REGISTER) && (op->type->flag & OPTYPE_UNDO))
			break;
	
	if(op==NULL)
		return;
	if(op->type->poll && op->type->poll((bContext *)C)==0)
		return;
	
	uiBlockSetFunc(block, redo_cb, op, NULL);
	
	if(!op->properties) {
		IDPropertyTemplate val = {0};
		op->properties= IDP_New(IDP_GROUP, val, "wmOperatorProperties");
	}
	
	RNA_pointer_create(&wm->id, op->type->srna, op->properties, &ptr);
	uiDefAutoButsRNA(C, pa->layout, &ptr, 1);
}

static void view3d_panel_tools(const bContext *C, Panel *pa)
{
	Object *obedit= CTX_data_edit_object(C);
//	Object *obact = CTX_data_active_object(C);
	uiLayout *col;
	
	if(obedit) {
		if(obedit->type==OB_MESH) {
			
			// void uiItemFullO(uiLayout *layout, char *name, int icon, char *idname, IDProperty *properties, int context)
			col= uiLayoutColumn(pa->layout, 1);
			uiItemFullO(col, NULL, 0, "MESH_OT_delete", NULL, WM_OP_INVOKE_REGION_WIN);
			
			col= uiLayoutColumn(pa->layout, 1);
			uiItemFullO(col, NULL, 0, "MESH_OT_subdivide", NULL, WM_OP_INVOKE_REGION_WIN);
			
			col= uiLayoutColumn(pa->layout, 1);
			uiItemFullO(col, NULL, 0, "MESH_OT_primitive_monkey_add", NULL, WM_OP_INVOKE_REGION_WIN);
			uiItemFullO(col, NULL, 0, "MESH_OT_primitive_uv_sphere_add", NULL, WM_OP_INVOKE_REGION_WIN);
			
			col= uiLayoutColumn(pa->layout, 1);
			uiItemFullO(col, NULL, 0, "MESH_OT_select_all_toggle", NULL, WM_OP_INVOKE_REGION_WIN);
			
			col= uiLayoutColumn(pa->layout, 1);
			uiItemFullO(col, NULL, 0, "MESH_OT_spin", NULL, WM_OP_INVOKE_REGION_WIN);
			uiItemFullO(col, NULL, 0, "MESH_OT_screw", NULL, WM_OP_INVOKE_REGION_WIN);
			
		}
	}
	else {
		
		col= uiLayoutColumn(pa->layout, 1);
		uiItemFullO(col, NULL, 0, "OBJECT_OT_delete", NULL, WM_OP_INVOKE_REGION_WIN);
		uiItemFullO(col, NULL, 0, "OBJECT_OT_primitive_add", NULL, WM_OP_INVOKE_REGION_WIN);
		
		col= uiLayoutColumn(pa->layout, 1);
		uiItemFullO(col, NULL, 0, "OBJECT_OT_parent_set", NULL, WM_OP_INVOKE_REGION_WIN);
		uiItemFullO(col, NULL, 0, "OBJECT_OT_parent_clear", NULL, WM_OP_INVOKE_REGION_WIN);
		
	}
}


void view3d_toolbar_register(ARegionType *art)
{
	PanelType *pt;

	pt= MEM_callocN(sizeof(PanelType), "spacetype view3d panel tools");
	strcpy(pt->idname, "VIEW3D_PT_tools");
	strcpy(pt->label, "Tools");
	pt->draw= view3d_panel_tools;
	BLI_addtail(&art->paneltypes, pt);
	
	pt= MEM_callocN(sizeof(PanelType), "spacetype view3d panel last operator");
	strcpy(pt->idname, "VIEW3D_PT_last_operator");
	strcpy(pt->label, "Last Operator");
	pt->draw= view3d_panel_operator_redo;
	BLI_addtail(&art->paneltypes, pt);
}

static int view3d_toolbar(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= view3d_has_tools_region(sa);
	
	if(ar) {
		ar->flag ^= RGN_FLAG_HIDDEN;
		ar->v2d.flag &= ~V2D_IS_INITIALISED; /* XXX should become hide/unhide api? */
		
		ED_area_initialize(CTX_wm_manager(C), CTX_wm_window(C), sa);
		ED_area_tag_redraw(sa);
	}
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_toolbar(wmOperatorType *ot)
{
	ot->name= "Toolbar";
	ot->idname= "VIEW3D_OT_toolbar";
	
	ot->exec= view3d_toolbar;
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= 0;
}
