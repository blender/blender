/**
 * $Id$
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
 * The Original Code is Copyright (C) 2004-2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "DNA_armature_types.h"
#include "DNA_ID.h"
#include "DNA_image_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h" /* U.smooth_viewtx */
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.h"

#include "MEM_guardedalloc.h"

#include "BKE_action.h"
#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h" /* for VECCOPY */

#include "ED_armature.h"
#include "ED_particle.h"
#include "ED_object.h"
#include "ED_mesh.h"
#include "ED_util.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "view3d_intern.h"


/* View3d->modeselect 
 * This is a bit of a dodgy hack to enable a 'mode' menu with icons+labels
 * rather than those buttons.
 * I know the implementation's not good - it's an experiment to see if this
 * approach would work well
 *
 * This can be cleaned when I make some new 'mode' icons.
 */

#define TEST_EDITMESH	if(obedit==0) return; \
						if( (v3d->lay & obedit->lay)==0 ) return;

/* XXX port over */	
extern void borderselect();

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
			scene->camera= v3d->camera;

			/* not through notifiery, listener don't have context
			   and non-open screens or spaces need to be updated too */
			ED_view3d_scene_layers_update(bmain, scene);
			
			/* notifiers for scene update */
			WM_event_add_notifier(C, NC_SCENE|ND_LAYER, scene);
		}
	}
}

static int layers_exec(bContext *C, wmOperator *op)
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
		v3d->lay |= (1<<20)-1;

		if(!v3d->layact)
			v3d->layact= 1;
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

			/* sanity check - when in editmode disallow switching the editmode layer off since its confusing
			 * an alternative would be to always draw the editmode object. */
			if(scene->obedit && (scene->obedit->lay & v3d->lay)==0) {
				for(bit=0; bit<32; bit++) {
					if(scene->obedit->lay & (1<<bit)) {
						v3d->lay |= 1<<bit;
						break;
					}
				}
			}
		}
		
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
	
	/* new layers might need unflushed events events */
	DAG_scene_update_flags(scene, v3d->lay);	/* tags all that moves and flushes */

	ED_area_tag_redraw(sa);
	
	return OPERATOR_FINISHED;
}

/* applies shift and alt, lazy coding or ok? :) */
/* the local per-keymap-entry keymap will solve it */
static int layers_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	if(event->ctrl || event->oskey)
		return OPERATOR_PASS_THROUGH;
	
	if(event->shift)
		RNA_boolean_set(op->ptr, "extend", 1);
	
	if(event->alt) {
		int nr= RNA_int_get(op->ptr, "nr") + 10;
		RNA_int_set(op->ptr, "nr", nr);
	}
	layers_exec(C, op);
	
	return OPERATOR_FINISHED;
}

int layers_poll(bContext *C)
{
	return (ED_operator_view3d_active(C) && CTX_wm_view3d(C)->localvd==NULL);
}

void VIEW3D_OT_layers(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Layers";
	ot->description= "Toggle layer(s) visibility.";
	ot->idname= "VIEW3D_OT_layers";
	
	/* api callbacks */
	ot->invoke= layers_invoke;
	ot->exec= layers_exec;
	ot->poll= layers_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_int(ot->srna, "nr", 1, 0, 20, "Number", "The layer number to set, zero for all layers", 0, 20);
	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Add this layer to the current view layers");
	RNA_def_boolean(ot->srna, "toggle", 1, "Toggle", "Toggle the layer");
}

static char *view3d_modeselect_pup(Scene *scene)
{
	Object *ob= OBACT;
	static char string[1024];
	static char formatstr[] = "|%s %%x%d %%i%d";
	char *str = string;

	str += sprintf(str, "Mode: %%t");
	
	str += sprintf(str, formatstr, "Object Mode", OB_MODE_OBJECT, ICON_OBJECT_DATA);
	
	if(ob==NULL) return string;
	
	/* if active object is editable */
	if ( ((ob->type == OB_MESH)
		|| (ob->type == OB_CURVE) || (ob->type == OB_SURF) || (ob->type == OB_FONT)
		|| (ob->type == OB_MBALL) || (ob->type == OB_LATTICE))) {
		
		str += sprintf(str, formatstr, "Edit Mode", OB_MODE_EDIT, ICON_EDITMODE_HLT);
	}
	else if (ob->type == OB_ARMATURE) {
		if (ob->mode & OB_MODE_POSE)
			str += sprintf(str, formatstr, "Edit Mode", OB_MODE_EDIT|OB_MODE_POSE, ICON_EDITMODE_HLT);
		else
			str += sprintf(str, formatstr, "Edit Mode", OB_MODE_EDIT, ICON_EDITMODE_HLT);
	}

	if (ob->type == OB_MESH) {

		str += sprintf(str, formatstr, "Sculpt Mode", OB_MODE_SCULPT, ICON_SCULPTMODE_HLT);
		str += sprintf(str, formatstr, "Vertex Paint", OB_MODE_VERTEX_PAINT, ICON_VPAINT_HLT);
		str += sprintf(str, formatstr, "Texture Paint", OB_MODE_TEXTURE_PAINT, ICON_TPAINT_HLT);
		str += sprintf(str, formatstr, "Weight Paint", OB_MODE_WEIGHT_PAINT, ICON_WPAINT_HLT);
	}

	
	/* if active object is an armature */
	if (ob->type==OB_ARMATURE) {
		str += sprintf(str, formatstr, "Pose Mode", OB_MODE_POSE, ICON_POSE_HLT);
	}

	if (ob->particlesystem.first || modifiers_findByType(ob, eModifierType_Cloth) || modifiers_findByType(ob, eModifierType_Softbody)) {
		str += sprintf(str, formatstr, "Particle Mode", OB_MODE_PARTICLE_EDIT, ICON_PARTICLEMODE);
	}

	return (string);
}


static void do_view3d_header_buttons(bContext *C, void *arg, int event)
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
				if( ((ts->selectmode ^ SCE_SELECT_FACE) == SCE_SELECT_VERTEX) || ((ts->selectmode ^ SCE_SELECT_FACE) == SCE_SELECT_EDGE)){
					if(ctrl) EM_convertsel(em, (ts->selectmode ^ SCE_SELECT_FACE),SCE_SELECT_FACE);
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
	uiLayout *row;
	
	RNA_pointer_create(&screen->id, &RNA_Space3DView, v3d, &v3dptr);	
	RNA_pointer_create(&scene->id, &RNA_ToolSettings, ts, &toolsptr);
	RNA_pointer_create(&scene->id, &RNA_Scene, scene, &sceneptr);

	block= uiLayoutGetBlock(layout);
	uiBlockSetHandleFunc(block, do_view3d_header_buttons, NULL);

	/* other buttons: */
	uiBlockSetEmboss(block, UI_EMBOSS);
	
	/* mode */
	if(ob)
		v3d->modeselect = ob->mode;
	else
		v3d->modeselect = OB_MODE_OBJECT;
		
	v3d->flag &= ~V3D_MODE;
	
	/* not sure what the v3d->flag is useful for now... modeselect is confusing */
	if(obedit) v3d->flag |= V3D_EDITMODE;
	if(ob && (ob->mode & OB_MODE_POSE)) v3d->flag |= V3D_POSEMODE;
	if(ob && (ob->mode & OB_MODE_VERTEX_PAINT)) v3d->flag |= V3D_VERTEXPAINT;
	if(ob && (ob->mode & OB_MODE_WEIGHT_PAINT)) v3d->flag |= V3D_WEIGHTPAINT;
	if(ob && (ob->mode & OB_MODE_TEXTURE_PAINT)) v3d->flag |= V3D_TEXTUREPAINT;
	if(paint_facesel_test(ob)) v3d->flag |= V3D_FACESELECT;

	uiBlockBeginAlign(block);
	uiDefIconTextButS(block, MENU, B_MODESELECT, object_mode_icon(v3d->modeselect), view3d_modeselect_pup(scene) , 
			  0,0,126,20, &(v3d->modeselect), 0, 0, 0, 0, "Mode");
	uiBlockEndAlign(block);
	
	/* Draw type */
	uiItemR(layout, "", 0, &v3dptr, "viewport_shading", UI_ITEM_R_ICON_ONLY);

	if (obedit==NULL && ((ob && ob->mode & (OB_MODE_VERTEX_PAINT|OB_MODE_WEIGHT_PAINT|OB_MODE_TEXTURE_PAINT)))) {
		/* Manipulators aren't used in weight paint mode */
		
		PointerRNA meshptr;

		RNA_pointer_create(&ob->id, &RNA_Mesh, ob->data, &meshptr);
		uiItemR(layout, "", 0, &meshptr, "use_paint_mask", UI_ITEM_R_ICON_ONLY);
	} else {
		char *str_menu;

		row= uiLayoutRow(layout, 1);
		uiItemR(row, "", 0, &v3dptr, "pivot_point", UI_ITEM_R_ICON_ONLY);
		uiItemR(row, "", 0, &v3dptr, "pivot_point_align", UI_ITEM_R_ICON_ONLY);

		/* NDOF */
		/* Not implemented yet
		 if (G.ndofdevice ==0 ) {
			uiDefIconTextButC(block, ICONTEXTROW,B_NDOF, ICON_NDOF_TURN, ndof_pup(), 0,0,XIC+10,YIC, &(v3d->ndofmode), 0, 3.0, 0, 0, "Ndof mode");
		
			uiDefIconButC(block, TOG, B_NDOF,  ICON_NDOF_DOM,
				      0,0,XIC,YIC,
				      &v3d->ndoffilter, 0, 1, 0, 0, "dominant axis");	
		}
		 */

		/* Transform widget / manipulators */
		row= uiLayoutRow(layout, 1);
		uiItemR(row, "", 0, &v3dptr, "manipulator", UI_ITEM_R_ICON_ONLY);
		block= uiLayoutGetBlock(row);
		
		if(v3d->twflag & V3D_USE_MANIPULATOR) {
			uiDefIconButBitS(block, TOG, V3D_MANIP_TRANSLATE, B_MAN_TRANS, ICON_MAN_TRANS, 0,0,XIC,YIC, &v3d->twtype, 1.0, 0.0, 0, 0, "Translate manipulator mode");
			uiDefIconButBitS(block, TOG, V3D_MANIP_ROTATE, B_MAN_ROT, ICON_MAN_ROT, 0,0,XIC,YIC, &v3d->twtype, 1.0, 0.0, 0, 0, "Rotate manipulator mode");
			uiDefIconButBitS(block, TOG, V3D_MANIP_SCALE, B_MAN_SCALE, ICON_MAN_SCALE, 0,0,XIC,YIC, &v3d->twtype, 1.0, 0.0, 0, 0, "Scale manipulator mode");
		}
			
		if (v3d->twmode > (BIF_countTransformOrientation(C) - 1) + V3D_MANIP_CUSTOM) {
			v3d->twmode = 0;
		}
			
		str_menu = BIF_menustringTransformOrientation(C, "Orientation");
		uiDefButS(block, MENU, B_MAN_MODE, str_menu,0,0,70,YIC, &v3d->twmode, 0, 0, 0, 0, "Transform Orientation");
		MEM_freeN(str_menu);
	}
 		
	if(obedit==NULL && v3d->localvd==NULL) {
		int ob_lay = ob ? ob->lay : 0;
		
		/* Layers */
		if (v3d->scenelock)
			uiTemplateLayers(layout, &sceneptr, "visible_layers", &v3dptr, "used_layers", ob_lay);
		else
			uiTemplateLayers(layout, &v3dptr, "visible_layers", &v3dptr, "used_layers", ob_lay);

		/* Scene lock */
		uiItemR(layout, "", 0, &v3dptr, "lock_camera_and_layers", UI_ITEM_R_ICON_ONLY);
	}
	
	/* selection modus, dont use python for this since it cant do the toggle buttons with shift+click as well as clicking to set one. */
	if(obedit && (obedit->type == OB_MESH)) {
		EditMesh *em= BKE_mesh_get_editmesh((Mesh *)obedit->data);

		row= uiLayoutRow(layout, 1);
		block= uiLayoutGetBlock(row);
		uiDefIconButBitS(block, TOG, SCE_SELECT_VERTEX, B_SEL_VERT, ICON_VERTEXSEL, 0,0,XIC,YIC, &em->selectmode, 1.0, 0.0, 0, 0, "Vertex select mode");
		uiDefIconButBitS(block, TOG, SCE_SELECT_EDGE, B_SEL_EDGE, ICON_EDGESEL, 0,0,XIC,YIC, &em->selectmode, 1.0, 0.0, 0, 0, "Edge select mode");
		uiDefIconButBitS(block, TOG, SCE_SELECT_FACE, B_SEL_FACE, ICON_FACESEL, 0,0,XIC,YIC, &em->selectmode, 1.0, 0.0, 0, 0, "Face select mode");

		BKE_mesh_end_editmesh(obedit->data, em);
	}
}
