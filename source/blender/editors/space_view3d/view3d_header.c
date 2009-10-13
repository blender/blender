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

#include "BLI_arithb.h"
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
static void countall(void) {}
extern void borderselect();
static int retopo_mesh_paint_check() {return 0;}

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
#define B_VIEWRENDER	106
#define B_STARTGAME	107
#define B_MODESELECT 108
#define B_AROUND	109
#define B_SEL_VERT	110
#define B_SEL_EDGE	111
#define B_SEL_FACE	112
#define B_SEL_PATH	113
#define B_SEL_POINT	114
#define B_SEL_END	115
#define B_MAN_TRANS	116
#define B_MAN_ROT	117
#define B_MAN_SCALE	118
#define B_NDOF		119	
#define B_MAN_MODE	120
#define B_VIEW_BUTSEDIT	121
#define B_REDR		122
#define B_NOP		123
#define B_ACTCOPY	124
#define B_ACTPASTE	125
#define B_ACTPASTEFLIP 126

#define B_LAY		201


static RegionView3D *wm_region_view3d(const bContext *C)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar;
	/* XXX handle foursplit? */
	for(ar= sa->regionbase.first; ar; ar= ar->next)
		if(ar->regiontype==RGN_TYPE_WINDOW)
			return ar->regiondata;
	return NULL;
}

// XXX quickly ported across
static void handle_view3d_lock(bContext *C) 
{
	Scene *scene= CTX_data_scene(C);
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= CTX_wm_view3d(C);
	
	if (v3d != NULL && sa != NULL) {
		if(v3d->localvd==NULL && v3d->scenelock && sa->spacetype==SPACE_VIEW3D) {
			
			/* copy to scene */
			scene->lay= v3d->lay;
			scene->camera= v3d->camera;
			
			//copy_view3d_lock(REDRAW);
		}
	}
}

static int layers_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= sa->spacedata.first;
	int nr= RNA_int_get(op->ptr, "nr");
	
	if(nr < 0)
		return OPERATOR_CANCELLED;
	
	
	if(nr == 0) {
		/* all layers */
		v3d->lay |= (1<<20)-1;

		if(!v3d->layact)
			v3d->layact= 1;
	}
	else {
		nr--;

		if(RNA_boolean_get(op->ptr, "extend"))
			v3d->lay |= (1<<nr);
		else
			v3d->lay = (1<<nr);
		
		/* set active layer, ensure to always have one */
		if(v3d->lay & (1<<nr))
		   v3d->layact= 1<<nr;
		else if((v3d->lay & v3d->layact)==0) {
			int bit= 0;

			while(bit<32) {
				if(v3d->lay & (1<<bit)) {
					v3d->layact= 1<<bit;
					break;
				}
				bit++;
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

void VIEW3D_OT_layers(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Layers";
	ot->description= "Toggle layer(s) visibility.";
	ot->idname= "VIEW3D_OT_layers";
	
	/* api callbacks */
	ot->invoke= layers_invoke;
	ot->exec= layers_exec;
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_int(ot->srna, "nr", 1, 0, 20, "Number", "The layer number to set, zero for all layers", 0, 20);
	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Add this layer to the current view layers");
}

#if 0
void do_view3d_select_object_typemenu(bContext *C, void *arg, int event)
{

	extern void selectall_type(short obtype);
	
	switch(event) {
	case 1: /* Mesh */
		selectall_type(OB_MESH);
		break;
	case 2: /* Curve */
		selectall_type(OB_CURVE);
		break;
	case 3: /* Surface */
		selectall_type(OB_SURF);
		break;
	case 4: /* Meta */
		selectall_type(OB_MBALL);
		break;
	case 5: /* Armature */
		selectall_type(OB_ARMATURE);
		break;
	case 6: /* Lattice */
		selectall_type(OB_LATTICE);
		break;
	case 7: /* Text */
		selectall_type(OB_FONT);
		break;
	case 8: /* Empty */
		selectall_type(OB_EMPTY);
		break;
	case 9: /* Camera */
		selectall_type(OB_CAMERA);
		break;
	case 10: /* Lamp */
		selectall_type(OB_LAMP);
		break;
	case 20:
		do_layer_buttons(C, -2);
		break;
	}
}

static uiBlock *view3d_select_object_typemenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiBeginBlock(C, ar, "view3d_select_object_typemenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_view3d_select_object_typemenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Mesh",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Curve",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Surface",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Meta",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Armature",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Lattice",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Text",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Empty",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Camera",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Lamp",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}


void do_view3d_select_object_layermenu(bContext *C, void *arg, int event)
{
// XXX	extern void selectall_layer(unsigned int layernum);
	
	switch(event) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
	case 17:
	case 18:
	case 19:
	case 20:
// XXX		selectall_layer(event);
		break;
	}
}

static uiBlock *view3d_select_object_layermenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short xco= 0, yco = 20, menuwidth = 22;

	block= uiBeginBlock(C, ar, "view3d_select_object_layermenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_view3d_select_object_layermenu, NULL);

	uiDefBut(block, BUTM, 1, "1",		xco, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefBut(block, BUTM, 1, "2",		xco+=(menuwidth+1), yco, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefBut(block, BUTM, 1, "3",		xco+=(menuwidth+1), yco, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefBut(block, BUTM, 1, "4",		xco+=(menuwidth+1), yco, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefBut(block, BUTM, 1, "5",		xco+=(menuwidth+1), yco, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	xco += 6;
	uiDefBut(block, BUTM, 1, "6",		xco+=(menuwidth+1), yco, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	uiDefBut(block, BUTM, 1, "7",		xco+=(menuwidth+1), yco, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
	uiDefBut(block, BUTM, 1, "8",		xco+=(menuwidth+1), yco, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
	uiDefBut(block, BUTM, 1, "9",		xco+=(menuwidth+1), yco, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
	uiDefBut(block, BUTM, 1, "10",		xco+=(menuwidth+1), yco, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
	xco = 0;
	uiDefBut(block, BUTM, 1, "11",		xco, yco-=24, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");
	uiDefBut(block, BUTM, 1, "12",		xco+=(menuwidth+1), yco, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
	uiDefBut(block, BUTM, 1, "13",		xco+=(menuwidth+1), yco, menuwidth, 19, NULL, 0.0, 0.0, 1, 13, "");
	uiDefBut(block, BUTM, 1, "14",		xco+=(menuwidth+1), yco, menuwidth, 19, NULL, 0.0, 0.0, 1, 14, "");
	uiDefBut(block, BUTM, 1, "15",		xco+=(menuwidth+1), yco, menuwidth, 19, NULL, 0.0, 0.0, 1, 15, "");
	xco += 6;
	uiDefBut(block, BUTM, 1, "16",		xco+=(menuwidth+1), yco, menuwidth, 19, NULL, 0.0, 0.0, 1, 16, "");
	uiDefBut(block, BUTM, 1, "17",		xco+=(menuwidth+1), yco, menuwidth, 19, NULL, 0.0, 0.0, 1, 17, "");
	uiDefBut(block, BUTM, 1, "18",		xco+=(menuwidth+1), yco, menuwidth, 19, NULL, 0.0, 0.0, 1, 18, "");
	uiDefBut(block, BUTM, 1, "19",		xco+=(menuwidth+1), yco, menuwidth, 19, NULL, 0.0, 0.0, 1, 19, "");
	uiDefBut(block, BUTM, 1, "20",		xco+=(menuwidth+1), yco, menuwidth, 19, NULL, 0.0, 0.0, 1, 20, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	/*uiTextBoundsBlock(block, 100);*/
	return block;
}

void do_view3d_select_object_linkedmenu(bContext *C, void *arg, int event)
{
	switch(event) {
	case 1: /* Object Ipo */
	case 2: /* ObData */
	case 3: /* Current Material */
	case 4: /* Current Texture */
		selectlinks(event);
		break;
	}
	countall();
}

static uiBlock *view3d_select_object_linkedmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiBeginBlock(C, ar, "view3d_select_object_linkedmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_view3d_select_object_linkedmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Object Ipo|Shift L, 1",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "ObData|Shift L, 2",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Material|Shift L, 3",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Texture|Shift L, 4",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

void do_view3d_select_object_groupedmenu(bContext *C, void *arg, int event)
{

	switch(event) {
	case 1: /* Children */
	case 2: /* Immediate Children */
	case 3: /* Parent */
	case 4: /* Siblings */
	case 5: /* Type */
	case 6: /* Objects on Shared Layers */
	case 7: /* Objects in Same Group */
	case 8: /* Object Hooks*/
	case 9: /* Object PassIndex*/
	case 10: /* Object Color*/
	case 11: /* Game Properties*/
		select_object_grouped((short)event);
		break;
	}
}

static uiBlock *view3d_select_object_groupedmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiBeginBlock(C, ar, "view3d_select_object_groupedmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_view3d_select_object_groupedmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Children|Shift G, 1",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Immediate Children|Shift G, 2",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Parent|Shift G, 3",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Siblings (Shared Parent)|Shift G, 4",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Objects of Same Type|Shift G, 5",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Objects on Shared Layers|Shift G, 6",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Objects in Same Group|Shift G, 7",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Object Hooks|Shift G, 8",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Object PassIndex|Shift G, 9",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Object Color|Shift G, 0",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Game Properties|Shift G, Alt+1",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");	

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

#endif

void do_view3d_select_faceselmenu(bContext *C, void *arg, int event)
{
#if 0
	/* events >= 6 are registered bpython scripts */
#ifndef DISABLE_PYTHON
	if (event >= 6) BPY_menu_do_python(PYMENU_FACESELECT, event - 6);
#endif
	
	switch(event) {
		case 0: /* border select */
			borderselect();
			break;
		case 2: /* Select/Deselect all */
			deselectall_tface();
			break;
		case 3: /* Select Inverse */
			selectswap_tface();
			break;
		case 4: /* Select Linked */
			select_linked_tfaces(2);
			break;
	}
#endif
}

static uiBlock *view3d_select_faceselmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
#ifndef DISABLE_PYTHON
// XXX	BPyMenu *pym;
//	int i = 0;
#endif

	block= uiBeginBlock(C, ar, "view3d_select_faceselmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_view3d_select_faceselmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Inverse",                0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");

	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Linked Faces|Ctrl L",                0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");

#ifndef DISABLE_PYTHON
//	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	/* note that we account for the 6 previous entries with i+6: */
//	for (pym = BPyMenuTable[PYMENU_FACESELECT]; pym; pym = pym->next, i++) {
//		uiDefIconTextBut(block, BUTM, 1, ICON_PYTHON, pym->name, 0, yco-=20,
//			menuwidth, 19, NULL, 0.0, 0.0, 1, i+6,
//			pym->tooltip?pym->tooltip:pym->filename);
//	}
#endif
	
	if(ar->alignment==RGN_ALIGN_TOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}

/* wrapper for python layouts */
void uiTemplate_view3d_select_faceselmenu(uiLayout *layout, bContext *C)
{
	void *arg_unused = NULL;
	ARegion *ar= CTX_wm_region(C);
	view3d_select_faceselmenu(C, ar, arg_unused);
}

#if 0
void do_view3d_transform_moveaxismenu(bContext *C, void *arg, int event)
{
#if 0
	float mat[3][3];
	
	Mat3One(mat);
	
	switch(event)
	{
	    case 0: /* X Global */
			initTransform(TFM_TRANSLATION, CTX_NONE);
			BIF_setSingleAxisConstraint(mat[0], " X");
			Transform();
			break;
		case 1: /* Y Global */
			initTransform(TFM_TRANSLATION, CTX_NONE);
			BIF_setSingleAxisConstraint(mat[1], " Y");
			Transform();
			break;
		case 2: /* Z Global */
			initTransform(TFM_TRANSLATION, CTX_NONE);
			BIF_setSingleAxisConstraint(mat[2], " Z");
			Transform();
			break;
		case 3: /* X Local */
			initTransform(TFM_TRANSLATION, CTX_NONE);
			BIF_setLocalAxisConstraint('X', " X");
			Transform();
			break;
		case 4: /* Y Local */
			initTransform(TFM_TRANSLATION, CTX_NONE);
			BIF_setLocalAxisConstraint('Y', " Y");
			Transform();
			break;
		case 5: /* Z Local */
			initTransform(TFM_TRANSLATION, CTX_NONE);
			BIF_setLocalAxisConstraint('Z', " Z");
			Transform();
			break;
	}
#endif
}

static uiBlock *view3d_transform_moveaxismenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiBeginBlock(C, ar, "view3d_transform_moveaxismenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_view3d_transform_moveaxismenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "X Global|G, X",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Y Global|G, Y",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Z Global|G, Z",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "X Local|G, X, X",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Y Local|G, Y, Y",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Z Local|G, Z, Z",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

void do_view3d_transform_rotateaxismenu(bContext *C, void *arg, int event)
{
#if 0
	float mat[3][3];
	
	Mat3One(mat);
	
	switch(event)
	{
	    case 0: /* X Global */
			initTransform(TFM_ROTATION, CTX_NONE);
			BIF_setSingleAxisConstraint(mat[0], " X");
			Transform();
			break;
		case 1: /* Y Global */
			initTransform(TFM_ROTATION, CTX_NONE);
			BIF_setSingleAxisConstraint(mat[1], " Y");
			Transform();
			break;
		case 2: /* Z Global */
			initTransform(TFM_ROTATION, CTX_NONE);
			BIF_setSingleAxisConstraint(mat[2], " Z");
			Transform();
 			break;
		case 3: /* X Local */
			initTransform(TFM_ROTATION, CTX_NONE);
			BIF_setLocalAxisConstraint('X', " X");
			Transform();
			break;
		case 4: /* Y Local */
			initTransform(TFM_ROTATION, CTX_NONE);
			BIF_setLocalAxisConstraint('Y', " Y");
			Transform();
			break;
		case 5: /* Z Local */
			initTransform(TFM_ROTATION, CTX_NONE);
			BIF_setLocalAxisConstraint('Z', " Z");
			Transform();
			break;
	}
#endif
}

static uiBlock *view3d_transform_rotateaxismenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiBeginBlock(C, ar, "view3d_transform_rotateaxismenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_view3d_transform_rotateaxismenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "X Global|R, X",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Y Global|R, Y",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Z Global|R, Z",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "X Local|R, X, X",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Y Local|R, Y, Y",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Z Local|R, Z, Z",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

void do_view3d_transform_scaleaxismenu(bContext *C, void *arg, int event)
{
#if 0
	float mat[3][3];
	
	Mat3One(mat);
	
	switch(event)
	{
	    case 0: /* X Global */
			initTransform(TFM_RESIZE, CTX_NONE);
			BIF_setSingleAxisConstraint(mat[0], " X");
			Transform();
			break;
		case 1: /* Y Global */
			initTransform(TFM_RESIZE, CTX_NONE);
			BIF_setSingleAxisConstraint(mat[1], " Y");
			Transform();
			break;
		case 2: /* Z Global */
			initTransform(TFM_RESIZE, CTX_NONE);
			BIF_setSingleAxisConstraint(mat[2], " Z");
			Transform();
			break;
		case 3: /* X Local */
			initTransform(TFM_RESIZE, CTX_NONE);
			BIF_setLocalAxisConstraint('X', " X");
			Transform();
			break;
		case 4: /* Y Local */
			initTransform(TFM_RESIZE, CTX_NONE);
			BIF_setLocalAxisConstraint('X', " X");
			Transform();
			break;
		case 5: /* Z Local */
			initTransform(TFM_RESIZE, CTX_NONE);
			BIF_setLocalAxisConstraint('X', " X");
			Transform();
			break;
	}
#endif
}

static uiBlock *view3d_transform_scaleaxismenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiBeginBlock(C, ar, "view3d_transform_scaleaxismenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_view3d_transform_scaleaxismenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "X Global|S, X",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Y Global|S, Y",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Z Global|S, Z",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "X Local|S, X, X",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Y Local|S, Y, Y",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Z Local|S, Z, Z",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}
#endif

#if 0
static void do_view3d_transformmenu(bContext *C, void *arg, int event)
{
#if 0
	Scene *scene= CTX_data_scene(C);
	ToolSettings *ts= CTX_data_tool_settings(C);
	
	switch(event) {
	case 1:
		initTransform(TFM_TRANSLATION, CTX_NONE);
		Transform();
		break;
	case 2:
		initTransform(TFM_ROTATION, CTX_NONE);
		Transform();
		break;
	case 3:
		initTransform(TFM_RESIZE, CTX_NONE);
		Transform();
		break;
	case 4:
		image_aspect();
		break;
	case 5:
		initTransform(TFM_TOSPHERE, CTX_NONE);
		Transform();
		break;
	case 6:
		initTransform(TFM_SHEAR, CTX_NONE);
		Transform();
		break;
	case 7:
		initTransform(TFM_WARP, CTX_NONE);
		Transform();
		break;
	case 8:
		initTransform(TFM_PUSHPULL, CTX_NONE);
		Transform();
		break;
	case 9:
		if (obedit) {
			if (obedit->type == OB_MESH)
				initTransform(TFM_SHRINKFATTEN, CTX_NONE);
				Transform();
		} else error("Only meshes can be shrinked/fattened");
		break;
	case 10:
		docenter(0);
		break;
	case 11:
		docenter_new();
		break;
	case 12:
		docenter_cursor();
		break;
	case 13:
		initTransform(TFM_TILT, CTX_NONE);
		Transform();
		break;
	case 14:
		initTransform(TFM_CURVE_SHRINKFATTEN, CTX_NONE);
		Transform();
		break;
	case 15:
		ts->snap_flag &= ~SCE_SNAP;
		break;
	case 16:
		ts->snap_flag |= SCE_SNAP;
		break;
	case 17:
		ts->snap_target = SCE_SNAP_TARGET_CLOSEST;
		break;
	case 18:
		ts->snap_target = SCE_SNAP_TARGET_CENTER;
		break;
	case 19:
		ts->snap_target = SCE_SNAP_TARGET_MEDIAN;
		break;
	case 20:
		ts->snap_target = SCE_SNAP_TARGET_ACTIVE;
		break;
	case 21:
		alignmenu();
		break;
	}
#endif
}

static uiBlock *view3d_transformmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	ToolSettings *ts= CTX_data_tool_settings(C);
	Object *obedit = CTX_data_edit_object(C);
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiBeginBlock(C, ar, "view3d_transformmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_view3d_transformmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Grab/Move|G",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBlockBut(block, view3d_transform_moveaxismenu, NULL, ICON_RIGHTARROW_THIN, "Grab/Move on Axis", 0, yco-=20, 120, 19, "");
		
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Rotate|R",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBlockBut(block, view3d_transform_rotateaxismenu, NULL, ICON_RIGHTARROW_THIN, "Rotate on Axis", 0, yco-=20, 120, 19, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Scale|S",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBlockBut(block, view3d_transform_scaleaxismenu, NULL, ICON_RIGHTARROW_THIN, "Scale on Axis", 0, yco-=20, 120, 19, "");

	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	if (obedit) {
 		if (obedit->type == OB_MESH)
 			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shrink/Fatten Along Normals|Alt S",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
		else if (obedit->type == OB_CURVE) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Tilt|T",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 13, "");
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shrink/Fatten Radius|Alt S",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 14, "");
		}
 	}
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "To Sphere|Ctrl Shift S",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	if (obedit) uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shear|Ctrl S",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shear|Ctrl Shift Alt S",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Warp|Shift W",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Push/Pull|Shift P",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
	
	if (!obedit) {
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Scale to Image Aspect Ratio|Alt V",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	}
	
	uiDefBut(block, SEPR, 0, "",                    0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "ObData to Center",               0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
	if (!obedit) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Center New",             0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Center Cursor",          0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Align to Transform Orientation|Ctrl Alt A", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 21, "");
	}
	
	if (BIF_snappingSupported(obedit))
	{
		uiDefBut(block, SEPR, 0, "",                    0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
		if (ts->snap_flag & SCE_SNAP)
		{
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Grid",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 15, "");
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Snap",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 16, "");
		}
		else
		{
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Grid",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 15, "");
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Snap",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 16, "");
		}
			
		uiDefBut(block, SEPR, 0, "",                    0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		switch(ts->snap_target)
		{
			case SCE_SNAP_TARGET_CLOSEST:
				uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Snap Closest",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 17, "");
				uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Snap Center",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 18, "");
				uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Snap Median",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 19, "");
				uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Snap Active",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 20, "");
				break;
			case SCE_SNAP_TARGET_CENTER:
				uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Snap Closest",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 17, "");
				uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Snap Center",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 18, "");
				uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Snap Median",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 19, "");
				uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Snap Active",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 20, "");
				break;
			case SCE_SNAP_TARGET_MEDIAN:
				uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Snap Closest",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 17, "");
				uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Snap Center",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 18, "");
				uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Snap Median",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 19, "");
				uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Snap Active",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 20, "");
				break;
			case SCE_SNAP_TARGET_ACTIVE:
				uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Snap Closest",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 17, "");
				uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Snap Center",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 18, "");
				uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Snap Median",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 19, "");
				uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Snap Active",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 20, "");
				break;
		}
	}

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

#if 0
void do_view3d_object_mirrormenu(bContext *C, void *arg, int event)
{
#if 0
	switch(event) {
		case 0:
			initTransform(TFM_MIRROR, CTX_NO_PET);
			Transform();
			break;
		case 1:
			initTransform(TFM_MIRROR, CTX_NO_PET|CTX_AUTOCONFIRM);
			BIF_setLocalAxisConstraint('X', " on X axis");
			Transform();
			break;
		case 2:
			initTransform(TFM_MIRROR, CTX_NO_PET|CTX_AUTOCONFIRM);
			BIF_setLocalAxisConstraint('Y', " on Y axis");
			Transform();
			break;
		case 3:
			initTransform(TFM_MIRROR, CTX_NO_PET|CTX_AUTOCONFIRM);
			BIF_setLocalAxisConstraint('Z', " on Z axis");
			Transform();
			break;
	}
#endif
}

static uiBlock *view3d_object_mirrormenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiBeginBlock(C, ar, "view3d_object_mirrormenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_view3d_object_mirrormenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Interactive Mirror|Ctrl M",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "X Local|Ctrl M, X",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Y Local|Ctrl M, Y",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Z Local|Ctrl M, Z",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}
#endif
#endif

#if 0
static void view3d_edit_object_transformmenu(bContext *C, uiLayout *layout, void *arg_unused)
{
#if 0 // XXX not used anymore
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Apply Scale/Rotation to ObData|Ctrl A, 1",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	apply_objects_locrot();
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Apply Visual Transform|Ctrl A, 2",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
	apply_objects_visual_tx();
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Apply Deformation|Ctrl Shift A",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	if(OBACT) object_apply_deform(OBACT);
#endif
	uiItemO(layout, NULL, 0, "OBJECT_OT_duplicates_make_real");

	uiItemS(layout);

	uiItemO(layout, NULL, 0, "OBJECT_OT_location_clear");
	uiItemO(layout, NULL, 0, "OBJECT_OT_rotation_clear");
	uiItemO(layout, NULL, 0, "OBJECT_OT_scale_clear");
	uiItemO(layout, NULL, 0, "OBJECT_OT_origin_clear");
}
#endif 

#if 0
static void do_view3d_edit_object_makelocalmenu(bContext *C, void *arg, int event)
{
#if 0
	switch(event) {
		case 1:
		case 2:
		case 3:
			make_local(event);
			break;
	}
#endif
}

static uiBlock *view3d_edit_object_makelocalmenu(bContext *C, ARegion *ar, void *arg_unused)
{	
	uiBlock *block;
	short yco = 20, menuwidth = 120;
	
	block= uiBeginBlock(C, ar, "view3d_edit_object_makelocalmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_view3d_edit_object_makelocalmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Selected Objects|L, 1",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Selected Objects and Data|L, 2",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "All|L, 3",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_object_makelinksmenu(bContext *C, void *arg, int event)
{
#if 0
	switch(event) {
	case 1:
	case 2:
	case 3:
	case 4:
		make_links((short)event);
		break;
		}
#endif
}

static uiBlock *view3d_edit_object_makelinksmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob=NULL;
	
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiBeginBlock(C, ar, "view3d_edit_object_makelinksmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_view3d_edit_object_makelinksmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "To Scene...|Ctrl L, 1",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Object Ipo|Ctrl L, 2",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	
	if ((ob=OBACT)) {
	
		if(ob->type==OB_MESH) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Mesh Data|Ctrl L, 3",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Materials|Ctrl L, 4",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		} else if(ob->type==OB_CURVE) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Curve Data|Ctrl L, 3",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Materials|Ctrl L, 4",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		} else if(ob->type==OB_FONT) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Text Data|Ctrl L, 3",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Materials|Ctrl L, 4",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		} else if(ob->type==OB_SURF) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Surface Data|Ctrl L, 3",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Materials|Ctrl L, 4",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		} else if(ob->type==OB_MBALL) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Materials|Ctrl L, 3",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		} else if(ob->type==OB_CAMERA) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Camera Data|Ctrl L, 3",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		} else if(ob->type==OB_LAMP) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Lamp Data|Ctrl L, 3",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		} else if(ob->type==OB_LATTICE) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Lattice Data|Ctrl L, 3",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		} else if(ob->type==OB_ARMATURE) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Armature Data|Ctrl L, 3",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		}
	}
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_object_singleusermenu(bContext *C, void *arg, int event)
{
#if 0
	switch(event) {
	case 1: /* Object */
		single_object_users(1);
		break;
	case 2: /* Object & ObData */ 
		single_object_users(1);
		single_obdata_users(1);
		break;
	case 3: /* Object & ObData & Materials+Tex */
		single_object_users(1);
		single_obdata_users(1);
		single_mat_users(1); /* also tex */
		break;
	case 4: /* Materials+Tex */
		single_mat_users(1);
		break;
	case 5: /* Ipo */
		single_ipo_users(1);
		break;
	}
	
	clear_id_newpoins();
	countall();
	
#endif
}

static uiBlock *view3d_edit_object_singleusermenu(bContext *C, ARegion *ar, void *arg_unused)
{

	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiBeginBlock(C, ar, "view3d_edit_object_singleusermenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_view3d_edit_object_singleusermenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Object|U, 1",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Object & ObData|U, 2",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Object & ObData & Materials+Tex|U, 3",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Materials+Tex|U, 4",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Ipos|U, 5",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_object_copyattrmenu(bContext *C, void *arg, int event)
{
	switch(event) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
	case 17:
	case 18:
	case 19:
	case 20:
	case 21:
	case 22:
	case 23:
	case 24:
	case 25:
	case 26:
	case 29:
	case 30:
// XXX		copy_attr((short)event);
		break;
		}
}

static uiBlock *view3d_edit_object_copyattrmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob=NULL;
	
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiBeginBlock(C, ar, "view3d_edit_object_copyattrmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_view3d_edit_object_copyattrmenu, NULL);
	
	ob= OBACT;
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Location|Ctrl C, 1",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Rotation|Ctrl C, 2",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Size|Ctrl C, 3",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Drawtype|Ctrl C, 4",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Time Offset|Ctrl C, 5",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Dupli|Ctrl C, 6",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Mass|Ctrl C, 7",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Damping|Ctrl C, 8",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "All Physical Attributes|Ctrl C, 11",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Properties|Ctrl C, 9",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Logic Bricks|Ctrl C, 10",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Protected Transform |Ctrl C",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 29, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Object Constraints|Ctrl C",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 22, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "NLA Strips|Ctrl C",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 26, "");
	
	if (ob) {
	
		if ((ob->type == OB_MESH) || (ob->type == OB_CURVE) || (ob->type == OB_SURF) ||
				(ob->type == OB_FONT) || (ob->type == OB_MBALL)) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Texture Space|Ctrl C",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 17, "");
		}	
		
		if(ob->type == OB_FONT) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Font Settings|Ctrl C",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 18, "");
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Bevel Settings|Ctrl C",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 19, "");
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Curve Resolution|Ctrl C",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 25, "");
		}
		if(ob->type == OB_CURVE) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Bevel Settings|Ctrl C",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 19, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Curve Resolution|Ctrl C",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 25, "");
		}
	
		if(ob->type==OB_MESH) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Subsurf Settings|Ctrl C",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 21, "");
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Modifiers ...|Ctrl C",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 24, "");
		}
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Object Pass Index|Ctrl C", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 30, "");
	}
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}
#endif

#if 0
#ifndef DISABLE_PYTHON
static void do_view3d_edit_object_scriptsmenu(bContext *C, void *arg, int event)
{
#if 0
	BPY_menu_do_python(PYMENU_OBJECT, event);

#endif
}

static uiBlock *view3d_edit_object_scriptsmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
//	short yco = 20, menuwidth = 120;
// XXX	BPyMenu *pym;
//	int i = 0;

	block= uiBeginBlock(C, ar, "v3d_eobject_pymenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_view3d_edit_object_scriptsmenu, NULL);

//	for (pym = BPyMenuTable[PYMENU_OBJECT]; pym; pym = pym->next, i++) {
//		uiDefIconTextBut(block, BUTM, 1, ICON_PYTHON, pym->name, 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, i, pym->tooltip?pym->tooltip:pym->filename);
//	}

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}
#endif /* DISABLE_PYTHON */
#endif


#if 0
static void do_view3d_edit_objectmenu(bContext *C, void *arg, int event)
{
	Scene *scene= CTX_data_scene(C);
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= sa->spacedata.first;
	
	switch(event) {
	 
	case 0: /* transform	properties*/
// XXX		mainqenter(NKEY, 1);
		break;
	case 5: /* make single user */
		single_user();
		break;
	case 7: /* boolean operation */
		special_editmenu();
		break;
	case 8: /* join objects */
		join_menu();
		break;
	case 9: /* convert object type */
		convertmenu();
		break;
	case 10: /* move to layer */
		movetolayer();
		break;
	case 11: /* insert keyframe */
		common_insertkey();
		break;
	case 16: /* make proxy object*/
		make_proxy();
		break;
	case 18: /* delete keyframe */
		common_deletekey();
		break; 
	}
}
#endif


/* texture paint menu (placeholder, no items yet??) */
static void do_view3d_tpaintmenu(bContext *C, void *arg, int event)
{
#if 0
	switch(event) {
	case 0: /* undo image painting */
		undo_imagepaint_step(1);
		break;
	}

#endif
}

static uiBlock *view3d_tpaintmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "view3d_paintmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_view3d_tpaintmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Texture Painting|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	if(ar->alignment==RGN_ALIGN_TOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}


static void do_view3d_wpaintmenu(bContext *C, void *arg, int event)
{
#if 0
	Object *ob= OBACT;
	
	/* events >= 3 are registered bpython scripts */
#ifndef DISABLE_PYTHON
	if (event >= 4) BPY_menu_do_python(PYMENU_WEIGHTPAINT, event - 4);
#endif	
	switch(event) {
	case 0: /* undo weight painting */
		BIF_undo();
		break;
	case 1: /* set vertex colors/weight */
		clear_wpaint_selectedfaces();
		break;
	case 2: /* vgroups from envelopes */
		pose_adds_vgroups(ob, 0);
		break;
	case 3: /* vgroups from bone heat */
		pose_adds_vgroups(ob, 1);
		break;
	}
#endif
}

static uiBlock *view3d_wpaintmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120, menunr=1;
#ifndef DISABLE_PYTHON
// XXX	BPyMenu *pym;
//	int i=0;
#endif
		
	block= uiBeginBlock(C, ar, "view3d_paintmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_view3d_wpaintmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Weight Painting|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");

	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Apply Bone Heat Weights to Vertex Groups|W, 2",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Apply Bone Envelopes to Vertex Groups|W, 1",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	if (paint_facesel_test(CTX_data_active_object(C))) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Set Weight|Shift K",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		menunr++;
	}

#ifndef DISABLE_PYTHON
	/* note that we account for the 4 previous entries with i+4:
	even if the last item isnt displayed, it dosent matter */
//	for (pym = BPyMenuTable[PYMENU_WEIGHTPAINT]; pym; pym = pym->next, i++) {
//		uiDefIconTextBut(block, BUTM, 1, ICON_PYTHON, pym->name, 0, yco-=20,
//			menuwidth, 19, NULL, 0.0, 0.0, 1, i+4,
//			pym->tooltip?pym->tooltip:pym->filename);
//	}
#endif

	if(ar->alignment==RGN_ALIGN_TOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_facesel_showhidemenu(bContext *C, void *arg, int event)
{
#if 0
	switch(event) {
	case 4: /* show hidden faces */
		reveal_tface();
		break;
	case 5: /* hide selected faces */
		hide_tface();
		break;
	case 6: /* XXX hide deselected faces */
//		G.qual |= LR_SHIFTKEY;
		hide_tface();
//		G.qual &= ~LR_SHIFTKEY;
		break;
		}
#endif
}

static uiBlock *view3d_facesel_showhidemenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiBeginBlock(C, ar, "view3d_facesel_showhidemenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_view3d_facesel_showhidemenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Hidden Faces|Alt H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Selected Faces|H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Unselected Faces|Shift H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_faceselmenu(bContext *C, void *arg, int event)
{
#if 0
	switch(event) {
	case 0: /* set vertex colors */
		clear_vpaint_selectedfaces();
		break;
	case 1: /* mark border seam */
		seam_mark_clear_tface(1);
		break;
	case 2: /* clear seam */
		seam_mark_clear_tface(2);
		break;
	}
#endif
}

static uiBlock *view3d_faceselmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "view3d_faceselmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_view3d_faceselmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Set Vertex Colors|Shift K",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Seam|Ctrl E",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Mark Border Seam|Ctrl E",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");

	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBlockBut(block, view3d_facesel_showhidemenu, NULL, ICON_RIGHTARROW_THIN, "Show/Hide Faces", 0, yco-=20, 120, 19, "");

	if(ar->alignment==RGN_ALIGN_TOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
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


static char *drawtype_pup(void)
{
 	static char string[512];
 	char *str = string;
	
	str += sprintf(str, "%s", "Draw type: %t"); 
	str += sprintf(str, "%s", "|Bounding Box %x1"); 
	str += sprintf(str, "%s", "|Wireframe %x2");
	str += sprintf(str, "%s", "|Solid %x3");
	str += sprintf(str, "%s", "|Shaded %x4");
	str += sprintf(str, "%s", "|Textured %x5");
	return string;
}
static char *around_pup(const bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);
	static char string[512];
	char *str = string;

	str += sprintf(str, "%s", "Pivot: %t"); 
	str += sprintf(str, "%s", "|Bounding Box Center %x0"); 
	str += sprintf(str, "%s", "|Median Point %x3");
	str += sprintf(str, "%s", "|3D Cursor %x1");
	str += sprintf(str, "%s", "|Individual Centers %x2");
	if ((obedit) && (obedit->type == OB_MESH))
		str += sprintf(str, "%s", "|Active Vert/Edge/Face %x4");
	else
		str += sprintf(str, "%s", "|Active Object %x4");
	return string;
}

static char *ndof_pup(void)
{
	static char string[512];
	char *str = string;

	str += sprintf(str, "%s", "ndof mode: %t"); 
	str += sprintf(str, "%s", "|turntable %x0"); 
	str += sprintf(str, "%s", "|fly %x1");
	str += sprintf(str, "%s", "|transform %x2");
	return string;
}


static char *snapmode_pup(void)
{
	static char string[512];
	char *str = string;
	
	str += sprintf(str, "%s", "Snap Element: %t"); 
	str += sprintf(str, "%s", "|Vertex%x0");
	str += sprintf(str, "%s", "|Edge%x1");
	str += sprintf(str, "%s", "|Face%x2"); 
	str += sprintf(str, "%s", "|Volume%x3"); 
	return string;
}

static char *propfalloff_pup(void)
{
	static char string[512];
	char *str = string;
	
	str += sprintf(str, "%s", "Falloff: %t"); 
	str += sprintf(str, "%s", "|Smooth Falloff%x0");
	str += sprintf(str, "%s", "|Sphere Falloff%x1");
	str += sprintf(str, "%s", "|Root Falloff%x2"); 
	str += sprintf(str, "%s", "|Sharp Falloff%x3"); 
	str += sprintf(str, "%s", "|Linear Falloff%x4");
	str += sprintf(str, "%s", "|Random Falloff%x6");
	str += sprintf(str, "%s", "|Constant, No Falloff%x5");
	return string;
}


static void do_view3d_header_buttons(bContext *C, void *arg, int event)
{
	wmWindow *win= CTX_wm_window(C);
	Scene *scene= CTX_data_scene(C);
	ToolSettings *ts= CTX_data_tool_settings(C);
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= sa->spacedata.first;
	Object *obedit = CTX_data_edit_object(C);
	Object *ob = CTX_data_active_object(C);
	EditMesh *em= NULL;
	int bit, ctrl= win->eventstate->ctrl, shift= win->eventstate->shift;
	PointerRNA props_ptr;
	
	if(obedit && obedit->type==OB_MESH) {
		em= BKE_mesh_get_editmesh((Mesh *)obedit->data);
	}
	/* watch it: if sa->win does not exist, check that when calling direct drawing routines */

	switch(event) {
	case B_HOME:
		WM_operator_name_call(C, "VIEW3D_OT_view_all", WM_OP_EXEC_REGION_WIN, NULL);
		break;
	case B_REDR:
		ED_area_tag_redraw(sa);
		break;
	case B_SCENELOCK:
		if(v3d->scenelock) {
			v3d->lay= scene->lay;
			/* seek for layact */
			bit= 0;
			while(bit<32) {
				if(v3d->lay & (1<<bit)) {
					v3d->layact= 1<<bit;
					break;
				}
				bit++;
			}
			v3d->camera= scene->camera;
			ED_area_tag_redraw(sa);
		}
		break;
		
	case B_VIEWBUT:
	

	case B_PERSP:
	
		
		break;
	case B_VIEWRENDER:
		if (sa->spacetype==SPACE_VIEW3D) {
// XXX			BIF_do_ogl_render(v3d, shift);
		}
		break;
	case B_STARTGAME:
// XXX		start_game();
		break;
	case B_MODESELECT:
		WM_operator_properties_create(&props_ptr, "OBJECT_OT_mode_set");
		RNA_enum_set(&props_ptr, "mode", v3d->modeselect);
		WM_operator_name_call(C, "OBJECT_OT_mode_set", WM_OP_EXEC_REGION_WIN, &props_ptr);
		WM_operator_properties_free(&props_ptr);
		break;		
	case B_AROUND:
// XXX		handle_view3d_around(); /* copies to other 3d windows */
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

	case B_SEL_PATH:
		ts->particle.selectmode= SCE_SELECT_PATH;
		WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
		ED_undo_push(C, "Selectmode Set: Path");
		break;
	case B_SEL_POINT:
		ts->particle.selectmode = SCE_SELECT_POINT;
		WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
		ED_undo_push(C, "Selectmode Set: Point");
		break;
	case B_SEL_END:
		ts->particle.selectmode = SCE_SELECT_END;
		WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
		ED_undo_push(C, "Selectmode Set: End point");
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
	case B_VIEW_BUTSEDIT:
		break;
		
	default:

		if(event>=B_LAY && event<B_LAY+31) {
			if(v3d->lay!=0 && shift) {
				
				/* but do find active layer */
				
				bit= event-B_LAY;
				if( v3d->lay & (1<<bit)) v3d->layact= 1<<bit;
				else {
					if( (v3d->lay & v3d->layact) == 0) {
						bit= 0;
						while(bit<32) {
							if(v3d->lay & (1<<bit)) {
								v3d->layact= 1<<bit;
								break;
							}
							bit++;
						}
					}
				}
			}
			else {
				bit= event-B_LAY;
				v3d->lay= 1<<bit;
				v3d->layact= v3d->lay;
			}
			
			if(v3d->scenelock) handle_view3d_lock(C);
			
			ED_area_tag_redraw(sa);
			countall();
			
			/* new layers might need unflushed events events */
			DAG_scene_update_flags(scene, v3d->lay);	/* tags all that moves and flushes */

		}
		break;
	}

	if(obedit && obedit->type==OB_MESH)
		BKE_mesh_end_editmesh(obedit->data, em);
}

static void view3d_header_pulldowns(const bContext *C, uiBlock *block, Object *ob, int *xcoord, int yco)
{
	Object *obedit = CTX_data_edit_object(C);
	RegionView3D *rv3d= wm_region_view3d(C);
	short xmax, xco= *xcoord;
	
	/* compensate for local mode when setting up the viewing menu/iconrow values */
	if(rv3d->view==7) rv3d->viewbut= 1;
	else if(rv3d->view==1) rv3d->viewbut= 2;
	else if(rv3d->view==3) rv3d->viewbut= 3;
	else rv3d->viewbut= 0;
	
	/* the 'xmax - 3' rather than xmax is to prevent some weird flickering where the highlighted
	 * menu is drawn wider than it should be. The ypos of -2 is to make it properly fill the
	 * height of the header */
	
	xmax= GetButStringLength("Select");

	xco+= xmax;
	
	if (obedit) {
	}
	else if (ob && ob->mode & OB_MODE_WEIGHT_PAINT) {
		xmax= GetButStringLength("Paint");
		uiDefPulldownBut(block, view3d_wpaintmenu, NULL, "Paint", xco,yco, xmax-3, 20, "");
		xco+= xmax;
	}
	else if (ob && ob->mode & OB_MODE_TEXTURE_PAINT) {
		xmax= GetButStringLength("Paint");
		uiDefPulldownBut(block, view3d_tpaintmenu, NULL, "Paint", xco,yco, xmax-3, 20, "");
		xco+= xmax;
	}
	else if (paint_facesel_test(ob)) {
		if (ob && ob->type == OB_MESH) {
			xmax= GetButStringLength("Face");
			uiDefPulldownBut(block, view3d_faceselmenu, NULL, "Face",	xco,yco, xmax-3, 20, "");
			xco+= xmax;
		}
	}
	else if(ob && ob->mode & OB_MODE_PARTICLE_EDIT) {
		/* ported to python */
	}
	else {
		if (ob && (ob->mode & OB_MODE_POSE)) {
		/* ported to python */
		}
	}

	*xcoord= xco;
}

static int view3d_layer_icon(int but_lay, int ob_lay, int used_lay)
{
	if (but_lay & ob_lay)
		return ICON_LAYER_ACTIVE;
	else if (but_lay & used_lay)
		return ICON_LAYER_USED;
	else
		return ICON_BLANK1;
}

static void header_xco_step(ARegion *ar, int *xco, int *yco, int *maxco, int step)
{
	*xco += step;
	if(*maxco < *xco) *maxco = *xco;
	
	if(ar->winy > *yco + 44) {
		if(*xco > ar->winrct.xmax) {
			*xco= 8;
			*yco+= 22;
		}
	}
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
	ARegion *ar= CTX_wm_region(C);
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= sa->spacedata.first;
	Scene *scene= CTX_data_scene(C);
	ToolSettings *ts= CTX_data_tool_settings(C);
	Object *ob= OBACT;
	Object *obedit = CTX_data_edit_object(C);
	uiBlock *block;
	int a, xco=0, maxco=0, yco= 0;
	
	block= uiLayoutAbsoluteBlock(layout);
	uiBlockSetHandleFunc(block, do_view3d_header_buttons, NULL);
	
	if((sa->flag & HEADER_NO_PULLDOWN)==0) 
		view3d_header_pulldowns(C, block, ob, &xco, yco);

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

	uiDefIconTextButS(block, MENU, B_MODESELECT, object_mode_icon(v3d->modeselect), view3d_modeselect_pup(scene) , 
			  xco,yco,126,20, &(v3d->modeselect), 0, 0, 0, 0, "Mode (Hotkeys: Tab, V, Ctrl Tab)");
	header_xco_step(ar, &xco, &yco, &maxco, 126+8);
	
	/* DRAWTYPE */
	uiDefIconTextButS(block, ICONTEXTROW,B_REDR, ICON_BBOX, drawtype_pup(), xco,yco,XIC+10,YIC, &(v3d->drawtype), 1.0, 5.0, 0, 0, "Viewport Shading (Hotkeys: Z, Shift Z, Alt Z)");

	header_xco_step(ar, &xco, &yco, &maxco, XIC+18);

	uiBlockBeginAlign(block);

	if(retopo_mesh_paint_check()) {
 		void *rpd= NULL; // XXX RetopoPaintData *rpd= get_retopo_paint_data();
 		if(rpd) {
 			ToolSettings *ts= scene->toolsettings;
 			
 			uiDefButC(block,ROW,B_REDR,"Pen",xco,yco,40,20,&ts->retopo_paint_tool,6.0,RETOPO_PEN,0,0,"");
			xco+= 40;
 			uiDefButC(block,ROW,B_REDR,"Line",xco,yco,40,20,&ts->retopo_paint_tool,6.0,RETOPO_LINE,0,0,"");
			xco+= 40;
 			uiDefButC(block,ROW,B_REDR,"Ellipse",xco,yco,60,20,&ts->retopo_paint_tool,6.0,RETOPO_ELLIPSE,0,0,"");
			xco+= 65;
			
 			uiBlockBeginAlign(block);
 			if(ts->retopo_paint_tool == RETOPO_PEN) {
 				uiDefButC(block,TOG,B_NOP,"Hotspot",xco,yco,60,20, &ts->retopo_hotspot, 0,0,0,0,"Show hotspots at line ends to allow line continuation");
				xco+= 80;
 			}
 			else if(ts->retopo_paint_tool == RETOPO_LINE) {
	 			uiDefButC(block,NUM,B_NOP,"LineDiv",xco,yco,80,20,&ts->line_div,1,50,0,0,"Subdivisions per retopo line");
				xco+= 80;
	 		}
			else if(ts->retopo_paint_tool == RETOPO_ELLIPSE) {
	 			uiDefButC(block,NUM,B_NOP,"EllDiv",xco,yco,80,20,&ts->ellipse_div,3,50,0,0,"Subdivisions per retopo ellipse");
				xco+= 80;
	 		}
			header_xco_step(ar, &xco, &yco, &maxco, 5);
 			
 			uiBlockEndAlign(block);
 		}
 	} else {
 		if (obedit==NULL && ((ob && ob->mode & (OB_MODE_VERTEX_PAINT|OB_MODE_WEIGHT_PAINT|OB_MODE_TEXTURE_PAINT)))) {
 			uiDefIconButBitI(block, TOG, G_FACESELECT, B_VIEW_BUTSEDIT, ICON_FACESEL_HLT,xco,yco,XIC,YIC, &G.f, 0, 0, 0, 0, "Painting Mask (FKey)");
			header_xco_step(ar, &xco, &yco, &maxco, XIC+10);
 		} else {
 			/* Manipulators aren't used in weight paint mode */
 			char *str_menu;
			uiDefIconTextButS(block, ICONTEXTROW,B_AROUND, ICON_ROTATE, around_pup(C), xco,yco,XIC+10,YIC, &(v3d->around), 0, 3.0, 0, 0, "Rotation/Scaling Pivot (Hotkeys: Comma, Shift Comma, Period, Ctrl Period, Alt Period)");
			xco+= XIC+10;
		
			uiDefIconButBitS(block, TOG, V3D_ALIGN, B_AROUND, ICON_ALIGN,
					 xco,yco,XIC,YIC,
					 &v3d->flag, 0, 0, 0, 0, "Move object centers only");	
			uiBlockEndAlign(block);
		
			header_xco_step(ar, &xco, &yco, &maxco, XIC+8);
	
			uiBlockBeginAlign(block);

			/* NDOF */
			if (G.ndofdevice ==0 ) {
				uiDefIconTextButC(block, ICONTEXTROW,B_NDOF, ICON_NDOF_TURN, ndof_pup(), xco,yco,XIC+10,YIC, &(v3d->ndofmode), 0, 3.0, 0, 0, "Ndof mode");
				xco+= XIC+10;
		
				uiDefIconButC(block, TOG, B_NDOF,  ICON_NDOF_DOM,
					 xco,yco,XIC,YIC,
					 &v3d->ndoffilter, 0, 1, 0, 0, "dominant axis");	
				uiBlockEndAlign(block);
		
				header_xco_step(ar, &xco, &yco, &maxco, XIC+8);
			}
			uiBlockEndAlign(block);

			/* Transform widget / manipulators */
			uiBlockBeginAlign(block);
			uiDefIconButBitS(block, TOG, V3D_USE_MANIPULATOR, B_REDR, ICON_MANIPUL,xco,yco,XIC,YIC, &v3d->twflag, 0, 0, 0, 0, "Use 3d transform manipulator (Ctrl Space)");	
			xco+= XIC;
		
			if(v3d->twflag & V3D_USE_MANIPULATOR) {
				uiDefIconButBitS(block, TOG, V3D_MANIP_TRANSLATE, B_MAN_TRANS, ICON_MAN_TRANS, xco,yco,XIC,YIC, &v3d->twtype, 1.0, 0.0, 0, 0, "Translate manipulator mode (Ctrl Alt G)");
				xco+= XIC;
				uiDefIconButBitS(block, TOG, V3D_MANIP_ROTATE, B_MAN_ROT, ICON_MAN_ROT, xco,yco,XIC,YIC, &v3d->twtype, 1.0, 0.0, 0, 0, "Rotate manipulator mode (Ctrl Alt R)");
				xco+= XIC;
				uiDefIconButBitS(block, TOG, V3D_MANIP_SCALE, B_MAN_SCALE, ICON_MAN_SCALE, xco,yco,XIC,YIC, &v3d->twtype, 1.0, 0.0, 0, 0, "Scale manipulator mode (Ctrl Alt S)");
				xco+= XIC;
			}
			
			if (v3d->twmode > (BIF_countTransformOrientation(C) - 1) + V3D_MANIP_CUSTOM) {
				v3d->twmode = 0;
			}
			
			str_menu = BIF_menustringTransformOrientation(C, "Orientation");
			uiDefButS(block, MENU, B_MAN_MODE, str_menu,xco,yco,70,YIC, &v3d->twmode, 0, 0, 0, 0, "Transform Orientation (ALT+Space)");
			MEM_freeN(str_menu);
			
			header_xco_step(ar, &xco, &yco, &maxco, 78);
			uiBlockEndAlign(block);
 		}
 		
		/* LAYERS */
		if(obedit==NULL && v3d->localvd==NULL) {
			int ob_lay = ob ? ob->lay : 0;
			uiBlockBeginAlign(block);
			for(a=0; a<5; a++) {
				uiDefIconButBitI(block, TOG, 1<<a, B_LAY+a, view3d_layer_icon(1<<a, ob_lay, v3d->lay_used), (short)(xco+a*(XIC/2)), yco+(short)(YIC/2),(short)(XIC/2),(short)(YIC/2), &(v3d->lay), 0, 0, 0, 0, "Toggles Layer visibility (Alt Num, Alt Shift Num)");
			}
			for(a=0; a<5; a++) {
				uiDefIconButBitI(block, TOG, 1<<(a+10), B_LAY+10+a, view3d_layer_icon(1<<(a+10), ob_lay, v3d->lay_used), (short)(xco+a*(XIC/2)), yco,			XIC/2, (YIC)/2, &(v3d->lay), 0, 0, 0, 0, "Toggles Layer visibility (Alt Num, Alt Shift Num)");
			}
			xco+= 5;
			uiBlockBeginAlign(block);
			for(a=5; a<10; a++) {
				uiDefIconButBitI(block, TOG, 1<<a, B_LAY+a, view3d_layer_icon(1<<a, ob_lay, v3d->lay_used), (short)(xco+a*(XIC/2)), yco+(short)(YIC/2),(short)(XIC/2),(short)(YIC/2), &(v3d->lay), 0, 0, 0, 0, "Toggles Layer visibility (Alt Num, Alt Shift Num)");
			}
			for(a=5; a<10; a++) {
				uiDefIconButBitI(block, TOG, 1<<(a+10), B_LAY+10+a, view3d_layer_icon(1<<(a+10), ob_lay, v3d->lay_used), (short)(xco+a*(XIC/2)), yco, XIC/2, (YIC)/2, &(v3d->lay), 0, 0, 0, 0, "Toggles Layer visibility (Alt Num, Alt Shift Num)");
			}
			uiBlockEndAlign(block);
		
			xco+= (a-2)*(XIC/2)+3;

			/* LOCK */
			uiDefIconButS(block, ICONTOG, B_SCENELOCK, ICON_LOCKVIEW_OFF, xco+=XIC,yco,XIC,YIC, &(v3d->scenelock), 0, 0, 0, 0, "Locks Active Camera and layers to Scene (Ctrl `)");
			header_xco_step(ar, &xco, &yco, &maxco, XIC+10);

		}
	
		/* proportional falloff */
		if((obedit && (obedit->type == OB_MESH || obedit->type == OB_CURVE || obedit->type == OB_SURF || obedit->type == OB_LATTICE)) || (ob && ob->mode & OB_MODE_PARTICLE_EDIT)) {
		
			uiBlockBeginAlign(block);
			uiDefIconTextButS(block, ICONTEXTROW,B_REDR, ICON_PROP_OFF, "Proportional %t|Off %x0|On %x1|Connected %x2", xco,yco,XIC+10,YIC, &(ts->proportional), 0, 1.0, 0, 0, "Proportional Edit Falloff (Hotkeys: O, Alt O) ");
			xco+= XIC+10;
		
			if(ts->proportional) {
				uiDefIconTextButS(block, ICONTEXTROW,B_REDR, ICON_SMOOTHCURVE, propfalloff_pup(), xco,yco,XIC+10,YIC, &(ts->prop_mode), 0.0, 0.0, 0, 0, "Proportional Edit Falloff (Hotkey: Shift O) ");
				xco+= XIC+10;
			}
			uiBlockEndAlign(block);
			header_xco_step(ar, &xco, &yco, &maxco, 10);
		}

		/* Snap */
		if (BIF_snappingSupported(obedit)) {
			uiBlockBeginAlign(block);

			if (ts->snap_flag & SCE_SNAP) {
				uiDefIconButBitS(block, TOG, SCE_SNAP, B_REDR, ICON_SNAP_GEO,xco,yco,XIC,YIC, &ts->snap_flag, 0, 0, 0, 0, "Snap while Ctrl is held during transform (Shift Tab)");
				xco+= XIC;
				uiDefIconButBitS(block, TOG, SCE_SNAP_ROTATE, B_REDR, ICON_SNAP_NORMAL,xco,yco,XIC,YIC, &ts->snap_flag, 0, 0, 0, 0, "Align rotation with the snapping target");	
				xco+= XIC;
				if (ts->snap_mode == SCE_SNAP_MODE_VOLUME) {
					uiDefIconButBitS(block, TOG, SCE_SNAP_PEEL_OBJECT, B_REDR, ICON_SNAP_PEEL_OBJECT,xco,yco,XIC,YIC, &ts->snap_flag, 0, 0, 0, 0, "Consider objects as whole when finding volume center");	
					xco+= XIC;
				}
				if (ts->snap_mode == SCE_SNAP_MODE_FACE) {
					uiDefIconButBitS(block, TOG, SCE_SNAP_PROJECT, B_REDR, ICON_RETOPO,xco,yco,XIC,YIC, &ts->snap_flag, 0, 0, 0, 0, "Project elements instead of snapping them");
					xco+= XIC;
				}
				uiDefIconTextButS(block, ICONTEXTROW,B_REDR, ICON_SNAP_VERTEX, snapmode_pup(), xco,yco,XIC+10,YIC, &(ts->snap_mode), 0.0, 0.0, 0, 0, "Snapping mode");
				xco+= XIC + 10;
				uiDefButS(block, MENU, B_NOP, "Snap Mode%t|Closest%x0|Center%x1|Median%x2|Active%x3",xco,yco,70,YIC, &ts->snap_target, 0, 0, 0, 0, "Snap Target Mode");
				xco+= 70;
			} else {
				uiDefIconButBitS(block, TOG, SCE_SNAP, B_REDR, ICON_SNAP_GEAR,xco,yco,XIC,YIC, &ts->snap_flag, 0, 0, 0, 0, "Snap while Ctrl is held during transform (Shift Tab)");	
				xco+= XIC;
			}

			uiBlockEndAlign(block);
			header_xco_step(ar, &xco, &yco, &maxco, 10);
		}

		/* selection modus */
		if(obedit && (obedit->type == OB_MESH)) {
			EditMesh *em= BKE_mesh_get_editmesh((Mesh *)obedit->data);

			uiBlockBeginAlign(block);
			uiDefIconButBitS(block, TOG, SCE_SELECT_VERTEX, B_SEL_VERT, ICON_VERTEXSEL, xco,yco,XIC,YIC, &em->selectmode, 1.0, 0.0, 0, 0, "Vertex select mode (Ctrl Tab 1)");
			xco+= XIC;
			uiDefIconButBitS(block, TOG, SCE_SELECT_EDGE, B_SEL_EDGE, ICON_EDGESEL, xco,yco,XIC,YIC, &em->selectmode, 1.0, 0.0, 0, 0, "Edge select mode (Ctrl Tab 2)");
			xco+= XIC;
			uiDefIconButBitS(block, TOG, SCE_SELECT_FACE, B_SEL_FACE, ICON_FACESEL, xco,yco,XIC,YIC, &em->selectmode, 1.0, 0.0, 0, 0, "Face select mode (Ctrl Tab 3)");
			xco+= XIC;
			uiBlockEndAlign(block);
			header_xco_step(ar, &xco, &yco, &maxco, 10);
			if(v3d->drawtype > OB_WIRE) {
				uiDefIconButBitS(block, TOG, V3D_ZBUF_SELECT, B_REDR, ICON_ORTHO, xco,yco,XIC,YIC, &v3d->flag, 1.0, 0.0, 0, 0, "Occlude background geometry");
			}
			xco+= XIC;
			uiBlockEndAlign(block);
			header_xco_step(ar, &xco, &yco, &maxco, XIC);

			BKE_mesh_end_editmesh(obedit->data, em);
		}
		else if(ob && ob->mode & OB_MODE_PARTICLE_EDIT) {
			uiBlockBeginAlign(block);
			uiDefIconButBitI(block, TOG, SCE_SELECT_PATH, B_SEL_PATH, ICON_EDGESEL, xco,yco,XIC,YIC, &ts->particle.selectmode, 1.0, 0.0, 0, 0, "Path edit mode");
			xco+= XIC;
			uiDefIconButBitI(block, TOG, SCE_SELECT_POINT, B_SEL_POINT, ICON_VERTEXSEL, xco,yco,XIC,YIC, &ts->particle.selectmode, 1.0, 0.0, 0, 0, "Point select mode");
			xco+= XIC;
			uiDefIconButBitI(block, TOG, SCE_SELECT_END, B_SEL_END, ICON_FACESEL, xco,yco,XIC,YIC, &ts->particle.selectmode, 1.0, 0.0, 0, 0, "Tip select mode");
			xco+= XIC;
			uiBlockEndAlign(block);
			
			if(v3d->drawtype > OB_WIRE) {
				uiDefIconButBitS(block, TOG, V3D_ZBUF_SELECT, B_REDR, ICON_ORTHO, xco,yco,XIC,YIC, &v3d->flag, 1.0, 0.0, 0, 0, "Limit selection to visible (clipped with depth buffer)");
				xco+= XIC;
			}
			uiBlockEndAlign(block);
			header_xco_step(ar, &xco, &yco, &maxco, XIC);
		}

		uiDefIconBut(block, BUT, B_VIEWRENDER, ICON_SCENE, xco,yco,XIC,YIC, NULL, 0, 1.0, 0, 0, "Render this window (Ctrl Click for anim)");
		
		if (ob && (ob->mode & OB_MODE_POSE)) {
			xco+= XIC*2;
			uiBlockBeginAlign(block);
			
			uiDefIconButO(block, BUT, "POSE_OT_copy", WM_OP_INVOKE_REGION_WIN, ICON_COPYDOWN, xco,yco,XIC,YIC, NULL);
			uiBlockSetButLock(block, object_data_is_libdata(ob), "Can't edit external libdata");
			xco+= XIC;
			
			uiDefIconButO(block, BUT, "POSE_OT_paste", WM_OP_INVOKE_REGION_WIN, ICON_PASTEDOWN, xco,yco,XIC,YIC, NULL);
			xco+= XIC;
				// FIXME: this needs an extra arg...
			uiDefIconButO(block, BUT, "POSE_OT_paste", WM_OP_INVOKE_REGION_WIN, ICON_PASTEFLIPDOWN, xco,yco,XIC,YIC, NULL);
			uiBlockEndAlign(block);
			header_xco_step(ar, &xco, &yco, &maxco, XIC);

		}
	}
}

