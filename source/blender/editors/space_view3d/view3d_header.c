/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2004-2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_header.c
 *  \ingroup spview3d
 */


#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "RNA_access.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_effect.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_paint.h"
#include "BKE_screen.h"

#include "ED_mesh.h"
#include "ED_util.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "view3d_intern.h"


/* View3d->modeselect 
 * This is a bit of a dodgy hack to enable a 'mode' menu with icons+labels
 * rather than those buttons.
 * I know the implementation's not good - it's an experiment to see if this
 * approach would work well
 *
 * This can be cleaned when I make some new 'mode' icons.
 */

/* view3d handler codes */
#define VIEW3D_HANDLER_BACKGROUND	1
#define VIEW3D_HANDLER_PROPERTIES	2
#define VIEW3D_HANDLER_OBJECT		3
#define VIEW3D_HANDLER_PREVIEW		4
#define VIEW3D_HANDLER_MULTIRES         5
#define VIEW3D_HANDLER_TRANSFORM	6
#define VIEW3D_HANDLER_GREASEPENCIL 7
#define VIEW3D_HANDLER_BONESKETCH	8

/* end XXX ************* */

static void do_view3d_header_buttons(bContext *C, void *arg, int event);

#define B_SCENELOCK 101
#define B_FULL		102
#define B_HOME		103
#define B_VIEWBUT	104
#define B_PERSP		105
#define B_MODESELECT 108
#define B_SEL_VERT	110
#define B_SEL_EDGE	111
#define B_SEL_FACE	112
#define B_MAN_TRANS	116
#define B_MAN_ROT	117
#define B_MAN_SCALE	118
#define B_NDOF		119	
#define B_MAN_MODE	120
#define B_REDR		122
#define B_NOP		123

// XXX quickly ported across
static void handle_view3d_lock(bContext *C) 
{
	Main *bmain= CTX_data_main(C);
	Scene *scene= CTX_data_scene(C);
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= CTX_wm_view3d(C);
	
	if (v3d != NULL && sa != NULL) {
		if(v3d->localvd==NULL && v3d->scenelock && sa->spacetype==SPACE_VIEW3D) {
			/* copy to scene */
			scene->lay= v3d->lay;
			scene->layact= v3d->layact;
			scene->camera= v3d->camera;

			/* not through notifiery, listener don't have context
			   and non-open screens or spaces need to be updated too */
			BKE_screen_view3d_main_sync(&bmain->screen, scene);
			
			/* notifiers for scene update */
			WM_event_add_notifier(C, NC_SCENE|ND_LAYER, scene);
		}
	}
}

/* layer code is on three levels actually:
- here for operator
- uiTemplateLayers in interface/ code for buttons
- ED_view3d_scene_layer_set for RNA
 */
static void view3d_layers_editmode_ensure(Scene *scene, View3D *v3d)
{
	/* sanity check - when in editmode disallow switching the editmode layer off since its confusing
	 * an alternative would be to always draw the editmode object. */
	if(scene->obedit && (scene->obedit->lay & v3d->lay)==0) {
		int bit;
		for(bit=0; bit<32; bit++) {
			if(scene->obedit->lay & (1<<bit)) {
				v3d->lay |= 1<<bit;
				break;
			}
		}
	}
}

static int view3d_layers_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= sa->spacedata.first;
	int nr= RNA_int_get(op->ptr, "nr");
	int toggle= RNA_boolean_get(op->ptr, "toggle");
	
	if(nr < 0)
		return OPERATOR_CANCELLED;

	if(nr == 0) {
		/* all layers */
		if(!v3d->layact)
			v3d->layact= 1;

		if (toggle && v3d->lay == ((1<<20)-1)) {
			/* return to active layer only */
			v3d->lay = v3d->layact;

			view3d_layers_editmode_ensure(scene, v3d);
		}
		else {
			v3d->lay |= (1<<20)-1;
		}		
	}
	else {
		int bit;
		nr--;

		if(RNA_boolean_get(op->ptr, "extend")) {
			if(toggle && v3d->lay & (1<<nr) && (v3d->lay & ~(1<<nr)))
				v3d->lay &= ~(1<<nr);
			else
				v3d->lay |= (1<<nr);
		} else {
			v3d->lay = (1<<nr);
		}

		view3d_layers_editmode_ensure(scene, v3d);

		/* set active layer, ensure to always have one */
		if(v3d->lay & (1<<nr))
		   v3d->layact= 1<<nr;
		else if((v3d->lay & v3d->layact)==0) {
			for(bit=0; bit<32; bit++) {
				if(v3d->lay & (1<<bit)) {
					v3d->layact= 1<<bit;
					break;
				}
			}
		}
	}
	
	if(v3d->scenelock) handle_view3d_lock(C);
	
	DAG_on_visible_update(CTX_data_main(C), FALSE);

	ED_area_tag_redraw(sa);
	
	return OPERATOR_FINISHED;
}

/* applies shift and alt, lazy coding or ok? :) */
/* the local per-keymap-entry keymap will solve it */
static int view3d_layers_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	if(event->ctrl || event->oskey)
		return OPERATOR_PASS_THROUGH;
	
	if(event->shift)
		RNA_boolean_set(op->ptr, "extend", TRUE);
	
	if(event->alt) {
		int nr= RNA_int_get(op->ptr, "nr") + 10;
		RNA_int_set(op->ptr, "nr", nr);
	}
	view3d_layers_exec(C, op);
	
	return OPERATOR_FINISHED;
}

static int view3d_layers_poll(bContext *C)
{
	return (ED_operator_view3d_active(C) && CTX_wm_view3d(C)->localvd==NULL);
}

void VIEW3D_OT_layers(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Layers";
	ot->description= "Toggle layer(s) visibility";
	ot->idname= "VIEW3D_OT_layers";
	
	/* api callbacks */
	ot->invoke= view3d_layers_invoke;
	ot->exec= view3d_layers_exec;
	ot->poll= view3d_layers_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_int(ot->srna, "nr", 1, 0, 20, "Number", "The layer number to set, zero for all layers", 0, 20);
	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Add this layer to the current view layers");
	RNA_def_boolean(ot->srna, "toggle", 1, "Toggle", "Toggle the layer");
}

static int modeselect_addmode(char *str, const char *title, int id, int icon)
{
	static char formatstr[] = "|%s %%x%d %%i%d";

	return sprintf(str, formatstr, IFACE_(title), id, icon);
}

static char *view3d_modeselect_pup(Scene *scene)
{
	Object *ob= OBACT;
	static char string[512];
	const char *title= IFACE_("Mode: %t");
	char *str = string;

	BLI_strncpy(str, title, sizeof(string));

	str += modeselect_addmode(str, N_("Object Mode"), OB_MODE_OBJECT, ICON_OBJECT_DATA);

	if(ob==NULL || ob->data==NULL) return string;
	if(ob->id.lib) return string;

	if(!((ID *)ob->data)->lib) {
		/* if active object is editable */
		if ( ((ob->type == OB_MESH)
			|| (ob->type == OB_CURVE) || (ob->type == OB_SURF) || (ob->type == OB_FONT)
			|| (ob->type == OB_MBALL) || (ob->type == OB_LATTICE))) {

			str += modeselect_addmode(str, N_("Edit Mode"), OB_MODE_EDIT, ICON_EDITMODE_HLT);
		}
		else if (ob->type == OB_ARMATURE) {
			if (ob->mode & OB_MODE_POSE)
				str += modeselect_addmode(str, N_("Edit Mode"), OB_MODE_EDIT|OB_MODE_POSE, ICON_EDITMODE_HLT);
			else
				str += modeselect_addmode(str, N_("Edit Mode"), OB_MODE_EDIT, ICON_EDITMODE_HLT);
		}

		if (ob->type == OB_MESH) {

			str += modeselect_addmode(str, N_("Sculpt Mode"), OB_MODE_SCULPT, ICON_SCULPTMODE_HLT);
			str += modeselect_addmode(str, N_("Vertex Paint"), OB_MODE_VERTEX_PAINT, ICON_VPAINT_HLT);
			str += modeselect_addmode(str, N_("Texture Paint"), OB_MODE_TEXTURE_PAINT, ICON_TPAINT_HLT);
			str += modeselect_addmode(str, N_("Weight Paint"), OB_MODE_WEIGHT_PAINT, ICON_WPAINT_HLT);
		}
	}

	/* if active object is an armature */
	if (ob->type==OB_ARMATURE) {
		str += modeselect_addmode(str, N_("Pose Mode"), OB_MODE_POSE, ICON_POSE_HLT);
	}

	if ( ob->particlesystem.first ||
	     modifiers_findByType(ob, eModifierType_Cloth) ||
	     modifiers_findByType(ob, eModifierType_Softbody))
	{
		str += modeselect_addmode(str, N_("Particle Mode"), OB_MODE_PARTICLE_EDIT, ICON_PARTICLEMODE);
	}
	(void)str;
	return (string);
}


static void do_view3d_header_buttons(bContext *C, void *UNUSED(arg), int event)
{
	wmWindow *win= CTX_wm_window(C);
	ToolSettings *ts= CTX_data_tool_settings(C);
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= sa->spacedata.first;
	Object *obedit = CTX_data_edit_object(C);
	EditMesh *em= NULL;
	int ctrl= win->eventstate->ctrl, shift= win->eventstate->shift;
	PointerRNA props_ptr;
	
	if(obedit && obedit->type==OB_MESH) {
		em= BKE_mesh_get_editmesh((Mesh *)obedit->data);
	}
	/* watch it: if sa->win does not exist, check that when calling direct drawing routines */

	switch(event) {
	case B_REDR:
		ED_area_tag_redraw(sa);
		break;
		
	case B_MODESELECT:
		WM_operator_properties_create(&props_ptr, "OBJECT_OT_mode_set");
		RNA_enum_set(&props_ptr, "mode", v3d->modeselect);
		WM_operator_name_call(C, "OBJECT_OT_mode_set", WM_OP_EXEC_REGION_WIN, &props_ptr);
		WM_operator_properties_free(&props_ptr);
		break;		
		
	case B_SEL_VERT:
		if(em) {
			if(shift==0 || em->selectmode==0)
				em->selectmode= SCE_SELECT_VERTEX;
			ts->selectmode= em->selectmode;
			EM_selectmode_set(em);
			WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);
			ED_undo_push(C, "Selectmode Set: Vertex");
		}
		break;
	case B_SEL_EDGE:
		if(em) {
			if(shift==0 || em->selectmode==0){
				if( (em->selectmode ^ SCE_SELECT_EDGE) == SCE_SELECT_VERTEX){
					if(ctrl) EM_convertsel(em, SCE_SELECT_VERTEX,SCE_SELECT_EDGE); 
				}
				em->selectmode = SCE_SELECT_EDGE;
			}
			ts->selectmode= em->selectmode;
			EM_selectmode_set(em);
			WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);
			ED_undo_push(C, "Selectmode Set: Edge");
		}
		break;
	case B_SEL_FACE:
		if(em) {
			if( shift==0 || em->selectmode==0){
				if( ((em->selectmode ^ SCE_SELECT_FACE) == SCE_SELECT_VERTEX) || ((em->selectmode ^ SCE_SELECT_FACE) == SCE_SELECT_EDGE)){
					if(ctrl)
						EM_convertsel(em, (em->selectmode ^ SCE_SELECT_FACE),SCE_SELECT_FACE);
				}
				em->selectmode = SCE_SELECT_FACE;
			}
			ts->selectmode= em->selectmode;
			EM_selectmode_set(em);
			WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);
			ED_undo_push(C, "Selectmode Set: Face");
		}
		break;	

	case B_MAN_TRANS:
		if( shift==0 || v3d->twtype==0) {
			v3d->twtype= V3D_MANIP_TRANSLATE;
		}
		ED_area_tag_redraw(sa);
		break;
	case B_MAN_ROT:
		if( shift==0 || v3d->twtype==0) {
			v3d->twtype= V3D_MANIP_ROTATE;
		}
		ED_area_tag_redraw(sa);
		break;
	case B_MAN_SCALE:
		if( shift==0 || v3d->twtype==0) {
			v3d->twtype= V3D_MANIP_SCALE;
		}
		ED_area_tag_redraw(sa);
		break;
	case B_NDOF:
		ED_area_tag_redraw(sa);
		break;
	case B_MAN_MODE:
		ED_area_tag_redraw(sa);
		break;
	default:
		break;
	}

	if(obedit && obedit->type==OB_MESH)
		BKE_mesh_end_editmesh(obedit->data, em);
}

/* Returns the icon associated with an object mode */
static int object_mode_icon(int mode)
{
	EnumPropertyItem *item = object_mode_items;
	
	while(item->name != NULL) {
		if(item->value == mode)
			return item->icon;
		++item;
	}

	return ICON_OBJECT_DATAMODE;
}

void uiTemplateEditModeSelection(uiLayout *layout, struct bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);
	uiBlock *block= uiLayoutGetBlock(layout);

	uiBlockSetHandleFunc(block, do_view3d_header_buttons, NULL);

	if(obedit && (obedit->type == OB_MESH)) {
		EditMesh *em= BKE_mesh_get_editmesh((Mesh *)obedit->data);
		uiLayout *row;

		row= uiLayoutRow(layout, 1);
		block= uiLayoutGetBlock(row);
		uiDefIconButBitS(block, TOG, SCE_SELECT_VERTEX, B_SEL_VERT, ICON_VERTEXSEL, 0,0,UI_UNIT_X,UI_UNIT_Y, &em->selectmode, 1.0, 0.0, 0, 0, "Vertex select mode");
		uiDefIconButBitS(block, TOG, SCE_SELECT_EDGE, B_SEL_EDGE, ICON_EDGESEL, 0,0,UI_UNIT_X,UI_UNIT_Y, &em->selectmode, 1.0, 0.0, 0, 0, "Edge select mode");
		uiDefIconButBitS(block, TOG, SCE_SELECT_FACE, B_SEL_FACE, ICON_FACESEL, 0,0,UI_UNIT_X,UI_UNIT_Y, &em->selectmode, 1.0, 0.0, 0, 0, "Face select mode");

		BKE_mesh_end_editmesh(obedit->data, em);
	}
}

void uiTemplateHeader3D(uiLayout *layout, struct bContext *C)
{
	bScreen *screen= CTX_wm_screen(C);
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= sa->spacedata.first;
	Scene *scene= CTX_data_scene(C);
	ToolSettings *ts= CTX_data_tool_settings(C);
	PointerRNA v3dptr, toolsptr, sceneptr;
	Object *ob= OBACT;
	Object *obedit = CTX_data_edit_object(C);
	uiBlock *block;
	uiBut *but;
	uiLayout *row;
	const float dpi_fac= UI_DPI_FAC;
	
	RNA_pointer_create(&screen->id, &RNA_SpaceView3D, v3d, &v3dptr);	
	RNA_pointer_create(&scene->id, &RNA_ToolSettings, ts, &toolsptr);
	RNA_pointer_create(&scene->id, &RNA_Scene, scene, &sceneptr);

	block= uiLayoutGetBlock(layout);
	uiBlockSetHandleFunc(block, do_view3d_header_buttons, NULL);

	/* other buttons: */
	uiBlockSetEmboss(block, UI_EMBOSS);
	
	/* mode */
	if(ob) {
		v3d->modeselect = ob->mode;
	}
	else {
		v3d->modeselect = OB_MODE_OBJECT;
	}

	row= uiLayoutRow(layout, 1);
	uiDefIconTextButS(block, MENU, B_MODESELECT, object_mode_icon(v3d->modeselect), view3d_modeselect_pup(scene) , 
			  0,0,126 * dpi_fac, UI_UNIT_Y, &(v3d->modeselect), 0, 0, 0, 0, TIP_("Mode"));
	
	/* Draw type */
	uiItemR(layout, &v3dptr, "viewport_shade", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);

	if (obedit==NULL && ((ob && ob->mode & (OB_MODE_VERTEX_PAINT|OB_MODE_WEIGHT_PAINT|OB_MODE_TEXTURE_PAINT)))) {
		/* Manipulators aren't used in weight paint mode */
		
		PointerRNA meshptr;

		RNA_pointer_create(&ob->id, &RNA_Mesh, ob->data, &meshptr);
		if(ob->mode & (OB_MODE_TEXTURE_PAINT|OB_MODE_VERTEX_PAINT)) {
			uiItemR(layout, &meshptr, "use_paint_mask", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
		}
		else {
			
			row= uiLayoutRow(layout, 1);
			uiItemR(row, &meshptr, "use_paint_mask", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
			uiItemR(row, &meshptr, "use_paint_mask_vertex", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
		}
	} else {
		const char *str_menu;

		row= uiLayoutRow(layout, 1);
		uiItemR(row, &v3dptr, "pivot_point", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);

		/* pose/object only however we want to allow in weight paint mode too
		 * so dont be totally strict and just check not-editmode for now */
		if (obedit == NULL) {
			uiItemR(row, &v3dptr, "use_pivot_point_align", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
		}

		/* Transform widget / manipulators */
		row= uiLayoutRow(layout, 1);
		uiItemR(row, &v3dptr, "show_manipulator", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
		block= uiLayoutGetBlock(row);
		
		if(v3d->twflag & V3D_USE_MANIPULATOR) {
			but= uiDefIconButBitC(block, TOG, V3D_MANIP_TRANSLATE, B_MAN_TRANS, ICON_MAN_TRANS, 0,0,UI_UNIT_X,UI_UNIT_Y, &v3d->twtype, 1.0, 0.0, 0, 0, TIP_("Translate manipulator mode"));
			uiButClearFlag(but, UI_BUT_UNDO); /* skip undo on screen buttons */
			but= uiDefIconButBitC(block, TOG, V3D_MANIP_ROTATE, B_MAN_ROT, ICON_MAN_ROT, 0,0,UI_UNIT_X,UI_UNIT_Y, &v3d->twtype, 1.0, 0.0, 0, 0, TIP_("Rotate manipulator mode"));
			uiButClearFlag(but, UI_BUT_UNDO); /* skip undo on screen buttons */
			but= uiDefIconButBitC(block, TOG, V3D_MANIP_SCALE, B_MAN_SCALE, ICON_MAN_SCALE, 0,0,UI_UNIT_X,UI_UNIT_Y, &v3d->twtype, 1.0, 0.0, 0, 0, TIP_("Scale manipulator mode"));
			uiButClearFlag(but, UI_BUT_UNDO); /* skip undo on screen buttons */
		}
			
		if (v3d->twmode > (BIF_countTransformOrientation(C) - 1) + V3D_MANIP_CUSTOM) {
			v3d->twmode = 0;
		}
			
		str_menu = BIF_menustringTransformOrientation(C, "Orientation");
		but= uiDefButC(block, MENU, B_MAN_MODE, str_menu,0,0,70 * dpi_fac, UI_UNIT_Y, &v3d->twmode, 0, 0, 0, 0, TIP_("Transform Orientation"));
		uiButClearFlag(but, UI_BUT_UNDO); /* skip undo on screen buttons */
		MEM_freeN((void *)str_menu);
	}

	if(obedit==NULL && v3d->localvd==NULL) {
		unsigned int ob_lay = ob ? ob->lay : 0;

		/* Layers */
		uiTemplateLayers(layout, v3d->scenelock ? &sceneptr : &v3dptr, "layers", &v3dptr, "layers_used", ob_lay);

		/* Scene lock */
		uiItemR(layout, &v3dptr, "lock_camera_and_layers", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
	}
	
	uiTemplateEditModeSelection(layout, C);
}
