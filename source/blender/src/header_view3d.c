/**
 * header_view3d.c oct-2003
 *
 * Functions to draw the "3D Viewport" window header
 * and handle user events sent to it.
 * 
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "BMF_Api.h"
#include "BIF_language.h"
#ifdef INTERNATIONAL
#include "FTF_Api.h"
#endif

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_image_types.h"
#include "DNA_texture_types.h"

#include "BKE_library.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_image.h"

#include "BLI_blenlib.h"

#include "BSE_edit.h"
#include "BSE_editaction.h"
#include "BSE_editipo.h"
#include "BSE_headerbuttons.h"
#include "BSE_view.h"


#include "BDR_editcurve.h"
#include "BDR_editface.h"
#include "BDR_editmball.h"
#include "BDR_editobject.h"
#include "BDR_vpaint.h"

#include "BIF_editlattice.h"
#include "BIF_editarmature.h"
#include "BIF_editfont.h"
#include "BIF_editmesh.h"
#include "BIF_editview.h"
#include "BIF_interface.h"
#include "BIF_mainqueue.h"
#include "BIF_poseobject.h"
#include "BIF_renderwin.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toets.h"
#include "BIF_toolbox.h"
#include "BIF_gl.h"

#include "blendef.h"
#include "mydevice.h"
#include "butspace.h"

#include "BIF_poseobject.h"

#include "TPT_DependKludge.h"

/* View3d->modeselect 
 * This is a bit of a dodgy hack to enable a 'mode' menu with icons+labels
 * rather than those buttons.
 * I know the implementation's not good - it's an experiment to see if this
 * approach would work well
 *
 * This can be cleaned when I make some new 'mode' icons.
 */

#define V3D_OBJECTMODE_SEL				ICON_OBJECT
#define V3D_EDITMODE_SEL				ICON_EDITMODE_HLT
#define V3D_FACESELECTMODE_SEL		ICON_FACESEL_HLT
#define V3D_VERTEXPAINTMODE_SEL		ICON_VPAINT_HLT
#define V3D_TEXTUREPAINTMODE_SEL	ICON_TPAINT_HLT
#define V3D_WEIGHTPAINTMODE_SEL		ICON_WPAINT_HLT
#define V3D_POSEMODE_SEL					ICON_POSE_HLT

#define TEST_EDITMESH	if(G.obedit==0) return; \
						if( (G.vd->lay & G.obedit->lay)==0 ) return;

static int viewmovetemp = 0;

void do_layer_buttons(short event)
{
	static int oldlay= 1;
	
	if(G.vd==0) return;
	if(G.vd->localview) return;
	
	if(event==-1 && (G.qual & LR_CTRLKEY)) {
		G.vd->scenelock= !G.vd->scenelock;
		do_view3d_buttons(B_SCENELOCK);
	} else if (event==-1) {
		if(G.vd->lay== (2<<20)-1) {
			if(G.qual & LR_SHIFTKEY) G.vd->lay= oldlay;
		}
		else {
			oldlay= G.vd->lay;
			G.vd->lay= (2<<20)-1;
		}
		
		if(G.vd->scenelock) handle_view3d_lock();
		scrarea_queue_winredraw(curarea);
	}
	else {
		if(G.qual & LR_ALTKEY) {
			if(event<11) event+= 10;
		}
		if(G.qual & LR_SHIFTKEY) {
			if(G.vd->lay & (1<<event)) G.vd->lay -= (1<<event);
			else	G.vd->lay += (1<<event);
		}
		do_view3d_buttons(event+B_LAY);
	}
	/* redraw seems double: but the queue nicely handles that */
	scrarea_queue_headredraw(curarea);
	
	if(curarea->spacetype==SPACE_OOPS) allqueue(REDRAWVIEW3D, 1); /* 1==also do headwin */
	
}

static void do_view3d_view_cameracontrolsmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* Orbit Left */
		persptoetsen(PAD4);
		break;
	case 1: /* Orbit Right */
		persptoetsen(PAD6);
		break;
	case 2: /* Orbit Up */
		persptoetsen(PAD8);
		break;
	case 3: /* Orbit Down */
		persptoetsen(PAD2);
		break;
	case 4: /* Pan left */
		/* ugly hack alert */
		G.qual |= LR_CTRLKEY;
		persptoetsen(PAD4);
		G.qual &= ~LR_CTRLKEY;
	case 5: /* Pan right */
		/* ugly hack alert */
		G.qual |= LR_CTRLKEY;
		persptoetsen(PAD6);
		G.qual &= ~LR_CTRLKEY;
	case 6: /* Pan up */
		/* ugly hack alert */
		G.qual |= LR_CTRLKEY;
		persptoetsen(PAD8);
		G.qual &= ~LR_CTRLKEY;
	case 7: /* Pan down */
		/* ugly hack alert */
		G.qual |= LR_CTRLKEY;
		persptoetsen(PAD2);
		G.qual &= ~LR_CTRLKEY;
	case 9: /* Zoom In */
		persptoetsen(PADPLUSKEY);
		break;
	case 10: /* Zoom Out */
		persptoetsen(PADMINUS);
		break;
	case 11: /* Reset Zoom */
		persptoetsen(PADENTER);
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_view_cameracontrolsmenu(void *arg_unused)
{
/*		static short tog=0; */
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_view_cameracontrolsmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_view_cameracontrolsmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Orbit Left|NumPad 4",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Orbit Right|NumPad 6", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Orbit Up|NumPad 8",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Orbit Down|NumPad 2",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, 140, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Pan Left|Ctrl NumPad 4",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Pan Right|Ctrl NumPad 6", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Pan Up|Ctrl NumPad 8",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Pan Down|Ctrl NumPad 2",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, 140, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom In|NumPad +", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom Out|NumPad -",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Reset Zoom|NumPad Enter",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 10, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_view_alignviewmenu(void *arg, int event)
{
	View3D *v3d= curarea->spacedata.first;
	float *curs;
	
	switch(event) {

	case 0: /* Align View to Selected (edit/faceselect mode) */
	case 1:
	case 2:
		if ((G.obedit) && (G.obedit->type == OB_MESH)) {
			editmesh_align_view_to_selected(v3d, event);
		} else if (G.f & G_FACESELECT) {
			Object *obact= OBACT;
			if (obact && obact->type==OB_MESH) {
				Mesh *me= obact->data;

				if (me->tface) {
					faceselect_align_view_to_selected(v3d, me, event);
					addqueue(v3d->area->win, REDRAW, 1);
				}
			}
		}
		break;
	case 3: /* Center View to Cursor */
		curs= give_cursor();
		G.vd->ofs[0]= -curs[0];
		G.vd->ofs[1]= -curs[1];
		G.vd->ofs[2]= -curs[2];
		scrarea_queue_winredraw(curarea);
		break;
	case 4: /* Align Active Camera to View */
		/* This ugly hack is a symptom of the nasty persptoetsen function, 
		 * but at least it works for now.
		 */
		G.qual |= LR_SHIFTKEY;
		persptoetsen(PAD0);
		G.qual &= ~LR_SHIFTKEY;
		break;
	case 5: /* Align View to Selected (object mode) */
		mainqenter(PADASTERKEY, 1);
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_view_alignviewmenu(void *arg_unused)
{
/*		static short tog=0; */
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_view_alignviewmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_view_alignviewmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Centre View to Cursor|C",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Align Active Camera to View|Shift NumPad 0",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");	

	if (((G.obedit) && (G.obedit->type == OB_MESH)) || (G.f & G_FACESELECT)) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Align View to Selected (Top)|Shift V",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Align View to Selected (Front)|Shift V",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Align View to Selected (Side)|Shift V",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	} else {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Align View to Selected|NumPad *",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	}
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_viewmenu(void *arg, int event)
{
	extern int play_anim(int mode);
	
	switch(event) {
	case 0: /* User */
		G.vd->viewbut = 0;
		G.vd->persp = 1;
		break;
	case 1: /* Camera */
		persptoetsen(PAD0);
		break;
	case 2: /* Top */
		persptoetsen(PAD7);
		break;
	case 3: /* Front */
		persptoetsen(PAD1);
		break;
	case 4: /* Side */
		persptoetsen(PAD3);
		break;
	case 5: /* Perspective */
		G.vd->persp=1;
		break;
	case 6: /* Orthographic */
		G.vd->persp=0;
		break;
	case 7: /* Local View */
		G.vd->localview= 1;
		initlocalview();
		break;
	case 8: /* Global View */
		G.vd->localview= 0;
		endlocalview(curarea);
		break;
	case 9: /* View All (Home) */
		view3d_home(0);
		break;
	case 11: /* View Selected */
		centreview();
		break;
	case 13: /* Play Back Animation */
		play_anim(0);
		break;
	case 15: /* Background Image... */
		add_blockhandler(curarea, VIEW3D_HANDLER_BACKGROUND, UI_PNL_UNSTOW);
		break;
	case 16: /* View  Panel */
		add_blockhandler(curarea, VIEW3D_HANDLER_PROPERTIES, UI_PNL_UNSTOW);
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_viewmenu(void *arg_unused)
{
/*		static short tog=0; */
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_viewmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_viewmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL, "View Properties...",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 16, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL, "Background Image...",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 15, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	if ((G.vd->viewbut == 0) && !(G.vd->persp == 2)) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "User",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "User",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	if (G.vd->persp == 2) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Camera|NumPad 0",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Camera|NumPad 0",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	if (G.vd->viewbut == 1) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Top|NumPad 7",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Top|NumPad 7",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	if (G.vd->viewbut == 2) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Front|NumPad 1",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Front|NumPad 1",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	if (G.vd->viewbut == 3) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Side|NumPad 3",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Side|NumPad 3",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	if(G.vd->persp==1) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Perspective|NumPad 5",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Perspective|NumPad 5",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	if(G.vd->persp==0) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Orthographic|NumPad 5", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Orthographic|NumPad 5", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	if(G.vd->localview) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Local View|NumPad /",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Local View|NumPad /", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	if(!G.vd->localview) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Global View|NumPad /",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Global View|NumPad /",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBlockBut(block, view3d_view_cameracontrolsmenu, NULL, ICON_RIGHTARROW_THIN, "View Navigation", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, view3d_view_alignviewmenu, NULL, ICON_RIGHTARROW_THIN, "Align View", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "View Selected|NumPad .",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 11, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "View All|Home",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");
	if(!curarea->full) uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Maximize Window|Ctrl UpArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 99, "");
	else uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Tile Window|Ctrl DownArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 99, "");

	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Play Back Animation|Alt A",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 13, "");

	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	
	return block;
}

void do_view3d_select_object_typemenu(void *arg, int event)
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
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_select_object_typemenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_select_object_typemenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
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

void do_view3d_select_object_layermenu(void *arg, int event)
{
	extern void selectall_layer(int layernum);
	
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
		selectall_layer(event);
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_select_object_layermenu(void *arg_unused)
{
	uiBlock *block;
	short xco= 0, yco = 20, menuwidth = 22;

	block= uiNewBlock(&curarea->uiblocks, "view3d_select_object_layermenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
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
	//uiTextBoundsBlock(block, 100);
	return block;
}

void do_view3d_select_object_linkedmenu(void *arg, int event)
{
	switch(event) {
	case 1: /* Object Ipo */
	case 2: /* ObData */
	case 3: /* Current Material */
	case 4: /* Current Texture */
		selectlinks(event);
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_select_object_linkedmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_select_object_linkedmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_select_object_linkedmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Object Ipo|Shift L, 1",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "ObData|Shift L, 2",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Material|Shift L, 3",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Texture|Shift L, 4",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

void do_view3d_select_object_groupedmenu(void *arg, int event)
{
	switch(event) {
	case 1: /* Children */
	case 2: /* Immediate Children */
	case 3: /* Parent */
	case 4: /* Objects on Shared Layers */
		select_group((short)event);
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_select_object_groupedmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_select_object_groupedmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_select_object_groupedmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Children|Shift G, 1",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Immediate Children|Shift G, 2",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Parent|Shift G, 3",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Objects on Shared Layers|Shift G, 4",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_select_objectmenu(void *arg, int event)
{
	switch(event) {
	
	case 0: /* border select */
		borderselect();
		break;
	case 1: /* Select/Deselect All */
		deselectall();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_select_objectmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_select_objectmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_select_objectmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBlockBut(block, view3d_select_object_layermenu, NULL, ICON_RIGHTARROW_THIN, "Select All by Layer", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, view3d_select_object_typemenu, NULL, ICON_RIGHTARROW_THIN, "Select All by Type", 0, yco-=20, 120, 19, "");
		
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBlockBut(block, view3d_select_object_linkedmenu, NULL, ICON_RIGHTARROW_THIN, "Linked", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, view3d_select_object_groupedmenu, NULL, ICON_RIGHTARROW_THIN, "Grouped", 0, yco-=20, 120, 19, "");

	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}
	
	uiTextBoundsBlock(block, 50);
	return block;
}

void do_view3d_select_meshmenu(void *arg, int event)
{
//	extern void borderselect(void);

	switch(event) {
	
		case 0: /* border select */
			borderselect();
			break;
		case 2: /* Select/Deselect all */
			deselectall_mesh();
			break;
		case 3: /* Inverse */
			selectswap_mesh();
			break;
		case 4: /* select linked vertices */
			selectconnected_mesh(LR_CTRLKEY);
			break;
		case 5: /* select random */
			selectrandom_mesh();
			break;
		case 6: /* select Faceloop */
			loop('s');
			break;
		case 7: /* select more */
			select_more();
			break;
		case 8: /* select less */
			select_less();
			break;
		case 9: /* select less */
			select_non_manifold();
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}


static uiBlock *view3d_select_meshmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_select_meshmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_select_meshmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Inverse",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Random...",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Non-Manifold|Ctrl Alt Shift M", 
					 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
	
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "More|Ctrl NumPad +",
					 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Less|Ctrl NumPad -",
					 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");

	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Face Loop...|Shift R",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Linked Vertices|Ctrl L",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}

void do_view3d_select_curvemenu(void *arg, int event)
{
//	extern void borderselect(void);

	switch(event) {
		case 0: /* border select */
			borderselect();
			break;
		case 2: /* Select/Deselect all */
			deselectall_nurb();
			break;
		case 3: /* Inverse */
			selectswapNurb();
			break;
		//case 4: /* select connected control points */
			//G.qual |= LR_CTRLKEY;
			//selectconnected_nurb();
			//G.qual &= ~LR_CTRLKEY;
			//break;
		case 5: /* select row (nurb) */
			selectrow_nurb();
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}


static uiBlock *view3d_select_curvemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_select_curvemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_select_curvemenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Inverse",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
	if (OBACT->type == OB_SURF) {
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Control Point Row|Shift R",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	}
	/* commented out because it seems to only like the LKEY method - based on mouse pointer position :( */
	//uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Connected Control Points|Ctrl L",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}
	
	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_select_metaballmenu(void *arg, int event)
{

	switch(event) {
		case 0: /* border select */
			borderselect();
			break;
		case 2: /* Select/Deselect all */
			deselectall_mball();
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}


static uiBlock *view3d_select_metaballmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_select_metaballmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_select_metaballmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_select_latticemenu(void *arg, int event)
{
//	extern void borderselect(void);
	
	switch(event) {
			case 0: /* border select */
			borderselect();
			break;
		case 2: /* Select/Deselect all */
			deselectall_Latt();
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_select_latticemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_select_latticemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_select_latticemenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_select_armaturemenu(void *arg, int event)
{
//	extern void borderselect(void);

	switch(event) {
			case 0: /* border select */
			borderselect();
			break;
		case 2: /* Select/Deselect all */
			deselectall_armature();
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_select_armaturemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_select_armaturemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_select_armaturemenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_select_pose_armaturemenu(void *arg, int event)
{
//	extern void borderselect(void);
	
	switch(event) {
			case 0: /* border select */
			borderselect();
			break;
		case 2: /* Select/Deselect all */
			deselectall_posearmature(1);
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_select_pose_armaturemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_select_pose_armaturemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_select_pose_armaturemenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_select_faceselmenu(void *arg, int event)
{
//	extern void borderselect(void);
	
	switch(event) {
			case 0: /* border select */
			borderselect();
			break;
		case 2: /* Select/Deselect all */
			deselectall_tface();
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_select_faceselmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_select_faceselmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_select_faceselmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}

void do_view3d_edit_snapmenu(void *arg, int event)
{
	switch(event) {
	case 1: /* Selection to grid */
	    snap_sel_to_grid();
	    break;
	case 2: /* Selection to cursor */
	    snap_sel_to_curs();
	    break;	    
	case 3: /* Cursor to grid */
	    snap_curs_to_grid();
	    break;
	case 4: /* Cursor to selection */
	    snap_curs_to_sel();
	    break;
	case 5: /* Selection to center of selection*/
	    snap_to_center();
	    break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_snapmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_snapmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_snapmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Selection -> Grid|Shift S, 1",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Selection -> Cursor|Shift S, 2",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Cursor -> Grid|Shift S, 3",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Cursor -> Selection|Shift S, 4",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Selection -> Center|Shift S, 5",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_object_transformmenu(void *arg, int event)
{
	switch(event) {
	case 0: /*	clear origin */
		clear_object('o');
		break;
	case 1: /* clear size */
		clear_object('s');
		break;
	case 2: /* clear rotation */
		clear_object('r');
		break;
	case 3: /* clear location */
		clear_object('g');
		break;
	case 4: /* apply deformation */
		make_duplilist_real();
		break;
	case 5: /* apply size/rotation */
		apply_object();
		break;	
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_object_transformmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_object_transformmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_object_transformmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Apply Size/Rotation|Ctrl A",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Apply Deformation|Ctrl Shift A",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Location|Alt G", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Rotation|Alt R", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Size|Alt S",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Origin|Alt O",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_object_makelinksmenu(void *arg, int event)
{
	switch(event) {
	case 1:
	case 2:
	case 3:
	case 4:
		make_links((short)event);
		break;
		}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_object_makelinksmenu(void *arg_unused)
{
	Object *ob;
	
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	ob= OBACT;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_object_makelinksmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_object_makelinksmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "To Scene...|Ctrl L, 1",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Object Ipo|Ctrl L, 2",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	
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
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_object_singleusermenu(void *arg, int event)
{
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
	}
	
	clear_id_newpoins();
	countall();
	
	allqueue(REDRAWALL, 0);
}

static uiBlock *view3d_edit_object_singleusermenu(void *arg_unused)
{
	Object *ob;
	
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	ob= OBACT;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_object_singleusermenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_object_singleusermenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Object|U, 1",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Object & ObData|U, 2",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Object & ObData & Materials+Tex|U, 3",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Materials+Tex|U, 4",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_object_copyattrmenu(void *arg, int event)
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
		copy_attr((short)event);
		break;
		}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_object_copyattrmenu(void *arg_unused)
{
	Object *ob;
	
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	ob= OBACT;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_object_copyattrmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_object_copyattrmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Location|Ctrl C, 1",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Rotation|Ctrl C, 2",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Size|Ctrl C, 3",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Drawtype|Ctrl C, 4",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Time Offset|Ctrl C, 5",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Dupli|Ctrl C, 6",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Mass|Ctrl C, 7",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Damping|Ctrl C, 8",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Properties|Ctrl C, 9",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Logic Bricks|Ctrl C, 10",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Object Constraints|Ctrl C, 11",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 22, "");
	
	if ((ob->type == OB_MESH) || (ob->type == OB_CURVE) || (ob->type == OB_SURF) ||
			(ob->type == OB_FONT) || (ob->type == OB_MBALL)) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Texture Space|Ctrl C, 12",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 17, "");
	}	
	
	if(ob->type == OB_FONT) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Font Settings|Ctrl C, 13",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 18, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Bevel Settings|Ctrl C, 14",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 19, "");
	}
	if(ob->type == OB_CURVE) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Bevel Settings|Ctrl C, 13",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 19, "");
	}

	if(ob->type==OB_MESH) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Subdiv|Ctrl C, 13",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 21, "");
	}

	if( give_parteff(ob) ) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Particle Settings|Ctrl C, 14",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 20, "");
	}
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}


static void do_view3d_edit_object_parentmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* clear parent */
		clear_parent();
		break;
	case 1: /* make parent */
		make_parent();
		break;
		}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_object_parentmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_object_parentmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_object_parentmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Parent...|Ctrl P",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Parent...|Alt P",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_object_trackmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* clear track */
		clear_track();
		break;
	case 1: /* make track */
		make_track();
		break;
		}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_object_trackmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_object_trackmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_object_trackmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Track...|Ctrl T",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Track...|Alt T",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_objectmenu(void *arg, int event)
{
	/* needed to check for valid selected objects */
	Base *base=NULL;
	Object *ob=NULL;

	base= BASACT;
	if (base) ob= base->object;
	
	switch(event) {
	 
	case 0: /* transform	properties*/
		mainqenter(NKEY, 1);
		break;
	case 1: /* delete */
		delete_context_selected();
		break;
	case 2: /* duplicate */
		duplicate_context_selected();
		break;
	case 3: /* duplicate linked */
		G.qual |= LR_ALTKEY;
		adduplicate(0);
		G.qual &= ~LR_ALTKEY;
		break;
	case 5: /* make single user */
		single_user();
		break;
	case 7: /* boolean operation */
		special_editmenu();
		break;
	case 8: /* join objects */
		if( (ob= OBACT) ) {
			if(ob->type == OB_MESH) join_mesh();
			else if(ob->type == OB_CURVE) join_curve(OB_CURVE);
			else if(ob->type == OB_SURF) join_curve(OB_SURF);
			else if(ob->type == OB_ARMATURE) join_armature();
		}
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
	case 15: /* Object Panel */
		add_blockhandler(curarea, VIEW3D_HANDLER_OBJECT, UI_PNL_UNSTOW);
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_objectmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_objectmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_edit_objectmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL, "Transform Properties|N",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 15, "");

	//uiDefIconTextBlockBut(block, 0, NULL, ICON_RIGHTARROW_THIN, "Move", 0, yco-=20, 120, 19, "");
	//uiDefIconTextBlockBut(block, 0, NULL, ICON_RIGHTARROW_THIN, "Rotate", 0, yco-=20, 120, 19, "");
	//uiDefIconTextBlockBut(block, 0, NULL, ICON_RIGHTARROW_THIN, "Scale", 0, yco-=20, 120, 19, "");
	// uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Transform Properties...|N",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");

	uiDefIconTextBlockBut(block, view3d_edit_object_transformmenu, NULL, ICON_RIGHTARROW_THIN, "Transform", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, view3d_edit_snapmenu, NULL, ICON_RIGHTARROW_THIN, "Snap", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert Keyframe|I",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");	
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate Linked|Alt D",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete|X",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBlockBut(block, view3d_edit_object_makelinksmenu, NULL, ICON_RIGHTARROW_THIN, "Make Links", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, view3d_edit_object_singleusermenu, NULL, ICON_RIGHTARROW_THIN, "Make Single User", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, view3d_edit_object_copyattrmenu, NULL, ICON_RIGHTARROW_THIN, "Copy Attributes", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBlockBut(block, view3d_edit_object_parentmenu, NULL, ICON_RIGHTARROW_THIN, "Parent", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, view3d_edit_object_trackmenu, NULL, ICON_RIGHTARROW_THIN, "Track", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	if (OBACT && OBACT->type == OB_MESH) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Boolean Operation...|W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
	}
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Join Objects|Ctrl J",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Convert Object Type...|Alt C",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
		
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}


static void do_view3d_edit_propfalloffmenu(void *arg, int event)
{
	extern int prop_mode;
	
	switch(event) {
	case 0: /* proportional edit - sharp*/
		prop_mode = 0;
		break;
	case 1: /* proportional edit - smooth*/
		prop_mode = 1;
		break;
		}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_propfalloffmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;
	extern int prop_mode;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_propfalloffmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_propfalloffmenu, NULL);
	
	if (prop_mode==0) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Sharp|Shift O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Sharp|Shift O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	if (prop_mode==1) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Smooth|Shift O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Smooth|Shift O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_mesh_undohistorymenu(void *arg, int event)
{
	TEST_EDITMESH
	
	if(event<1) return;

	if (event==1) remake_editMesh();
	else undo_pop_mesh(G.undo_edit_level-event+3);
	
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_mesh_undohistorymenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;
	int i, lasti;

	lasti = (G.undo_edit_level>25) ? G.undo_edit_level-25 : 0;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_mesh_undohistorymenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_mesh_undohistorymenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo All Changes|Ctrl U", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	for (i=G.undo_edit_level; i>=lasti; i--) {
		if (i == G.undo_edit_level) uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, G.undo_edit[i].name, 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, (float)i+2, "");
	}
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

void do_view3d_edit_mesh_verticesmenu(void *arg, int event)
{
	extern float doublimit;
	
	switch(event) {
		 
	case 0: /* make vertex parent */
		make_parent();
		break;
	case 1: /* remove doubles */
		notice("Removed: %d", removedoublesflag(1, doublimit));
		break;
	case 2: /* smooth */
		vertexsmooth();
		break;
	case 3: /* separate */
		separate_mesh();
		break;
	case 4: /*split */
		split_mesh();
		break;
	case 5: /*merge */
		mergemenu();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_mesh_verticesmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_mesh_verticesmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_mesh_verticesmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Merge...|Alt M",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Split|Y",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Separate|P",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Smooth",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Remove Doubles",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Vertex Parent|Ctrl P",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

void do_view3d_edit_mesh_edgesmenu(void *arg, int event)
{
	extern short editbutflag;
	float fac;
	short randfac;

	switch(event) {
		 
	case 0: /* subdivide smooth */
		undo_push_mesh("Subdivide Smooth");
		subdivideflag(1, 0.0, editbutflag | B_SMOOTH);
		break;
	case 1: /*subdivide fractal */
		undo_push_mesh("Subdivide Fractal");
		randfac= 10;
		if(button(&randfac, 1, 100, "Rand fac:")==0) return;
		fac= -( (float)randfac )/100;
		subdivideflag(1, fac, editbutflag);
		break;
	case 2: /* subdivide */
		undo_push_mesh("Subdivide");
		subdivideflag(1, 0.0, editbutflag);
		break;
	case 3: /* knife subdivide */
		KnifeSubdivide(KNIFE_PROMPT);
		break;
	case 4: /* Loop subdivide */
		loop('c');
		break;
	case 5: /* Make Edge/Face */
		addedgevlak_mesh();
		break;
	case 6:
		bevel_menu();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_mesh_edgesmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_mesh_edgesmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_mesh_edgesmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Edge/Face|F",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Bevel",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Loop Subdivide...|Ctrl R",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Knife Subdivide...|Shift K",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Subdivide",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Subdivide Fractal",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Subdivide Smooth",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_mesh_facesmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* Fill Faces */
		fill_mesh();
		break;
	case 1: /* Beauty Fill Faces */
		beauty_fill();
		break;
	case 2: /* Quads to Tris */
		convert_to_triface(0);
		allqueue(REDRAWVIEW3D, 0);
		countall();
		makeDispList(G.obedit);
		break;
	case 3: /* Tris to Quads */
		join_triangles();
		break;
	case 4: /* Flip triangle edges */
		edge_flip();
		break;
	case 5: /* Make Edge/Face */
		addedgevlak_mesh();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_mesh_facesmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_mesh_facesmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_mesh_facesmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Edge/Face|F",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Fill|Shift F",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Beauty Fill|Alt F",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Convert Quads to Triangles|Ctrl T",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Convert Triangles to Quads|Alt J", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Flip Triangle Edges|Ctrl F",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

void do_view3d_edit_mesh_normalsmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* flip */
		flip_editnormals();
		break;
	case 1: /* recalculate inside */
		righthandfaces(2);
		break;
	case 2: /* recalculate outside */
		righthandfaces(1);
		break;
		}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_mesh_normalsmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_mesh_normalsmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_mesh_normalsmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Recalculate Outside|Ctrl N",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Recalculate Inside|Ctrl Shift N",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Flip",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

void do_view3d_edit_mesh_mirrormenu(void *arg, int event)
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
			mirror(event);
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_mesh_mirrormenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_mesh_mirrormenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_mesh_mirrormenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "X Global|M, 1",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Y Global|M, 2",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Z Global|M, 3",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "X Local|M, 4",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Y Local|M, 5",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Z Local|M, 6",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "X View|M, 7",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Y View|M, 8",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Z View|M, 9",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}


static void do_view3d_edit_mesh_showhidemenu(void *arg, int event)
{
	
	switch(event) {
		 
	case 0: /* show hidden vertices */
		reveal_mesh();
		break;
	case 1: /* hide selected vertices */
		hide_mesh(0);
		break;
	case 2: /* hide deselected vertices */
		hide_mesh(1);
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_mesh_showhidemenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_mesh_showhidemenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_mesh_showhidemenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Hidden|Alt H",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Selected|H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Deselected|Shift H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_meshmenu(void *arg, int event)
{
	switch(event) {
	
	case 0: /* Undo Editing */
		undo_pop_mesh(1);
		break;
	case 1: /* Redo Editing */
		undo_redo_mesh();
		break;
	case 2: /* transform properties */
		add_blockhandler(curarea, VIEW3D_HANDLER_OBJECT, 0);
		break;
	case 4: /* insert keyframe */
		common_insertkey();
		break;
	case 5: /* Extrude */
		extrude_mesh();
		break;
	case 6: /* duplicate */
		duplicate_context_selected();
		break;
	case 8: /* delete */
		delete_context_selected();
		break;
	case 9: /* Shrink/Fatten Along Normals */
		transform('N');
		break;
	case 10: /* Shear */
		transform('S');
		break;
	case 11: /* Warp */
		transform('w');
		break;
	case 12: /* proportional edit (toggle) */
		if(G.f & G_PROPORTIONAL) G.f &= ~G_PROPORTIONAL;
		else G.f |= G_PROPORTIONAL;
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_meshmenu(void *arg_unused)
{

	uiBlock *block;
	short yco= 0, menuwidth=120;
		
	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_meshmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_edit_meshmenu, NULL);
		
	/*
	uiDefIconTextBlockBut(block, view3d_edit_mesh_facesmenu, NULL, ICON_RIGHTARROW_THIN, "Move", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, view3d_edit_mesh_facesmenu, NULL, ICON_RIGHTARROW_THIN, "Rotate", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, view3d_edit_mesh_facesmenu, NULL, ICON_RIGHTARROW_THIN, "Scale", 0, yco-=20, 120, 19, "");
	*/
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Editing|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Redo Editing|Shift U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBlockBut(block, view3d_edit_mesh_undohistorymenu, NULL, ICON_RIGHTARROW_THIN, "Undo History", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL, "Transform Properties...|N",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBlockBut(block, view3d_edit_snapmenu, NULL, ICON_RIGHTARROW_THIN, "Snap", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert Keyframe|I",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Extrude|E",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Edge/Face|F",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete...|X",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBlockBut(block, view3d_edit_mesh_verticesmenu, NULL, ICON_RIGHTARROW_THIN, "Vertices", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, view3d_edit_mesh_edgesmenu, NULL, ICON_RIGHTARROW_THIN, "Edges", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, view3d_edit_mesh_facesmenu, NULL, ICON_RIGHTARROW_THIN, "Faces", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, view3d_edit_mesh_normalsmenu, NULL, ICON_RIGHTARROW_THIN, "Normals", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBlockBut(block, view3d_edit_mesh_mirrormenu, NULL, ICON_RIGHTARROW_THIN, "Mirror", 0, yco-=20, 120, 19, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shrink/Fatten Along Normals|Alt S",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shear|Ctrl S",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Warp|Shift W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	if(G.f & G_PROPORTIONAL) {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Proportional Editing|O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
	} else {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Proportional Editing|O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
	}
	uiDefIconTextBlockBut(block, view3d_edit_propfalloffmenu, NULL, ICON_RIGHTARROW_THIN, "Proportional Falloff", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBlockBut(block, view3d_edit_mesh_showhidemenu, NULL, ICON_RIGHTARROW_THIN, "Show/Hide Vertices", 0, yco-=20, 120, 19, "");
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_edit_curve_controlpointsmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* tilt */
		transform('t');
		break;
	case 1: /* clear tilt */
		clear_tilt();
		break;
	case 2: /* Free */
		sethandlesNurb(3);
		makeDispList(G.obedit);
		break;
	case 3: /* vector */
		sethandlesNurb(2);
		makeDispList(G.obedit);
		break;
	case 4: /* smooth */
		sethandlesNurb(1);
		makeDispList(G.obedit);
		break;
	case 5: /* make vertex parent */
		make_parent();
		break;
		}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_curve_controlpointsmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_curve_controlpointsmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_curve_controlpointsmenu, NULL);
	
	if (OBACT->type == OB_CURVE) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Tilt|T",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Tilt|Alt T",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Automatic|Shift H",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Toggle Free/Aligned|H",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Vector|V",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		
	}
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Vertex Parent|Ctrl P",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

void do_view3d_edit_curve_segmentsmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* subdivide */
		subdivideNurb();
		break;
	case 1: /* switch direction */
		switchdirectionNurb2();
		break;
		}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_curve_segmentsmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_curve_segmentsmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_curve_segmentsmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Subdivide",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Switch Direction",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

void do_view3d_edit_curve_showhidemenu(void *arg, int event)
{
	switch(event) {
	case 10: /* show hidden control points */
		revealNurb();
		break;
	case 11: /* hide selected control points */
		hideNurb(0);
		break;
	case 12: /* hide deselected control points */
		hideNurb(1);
		break;
		}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_curve_showhidemenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_curve_showhidemenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_curve_showhidemenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Hidden|Alt H",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Selected|H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");
	if (OBACT->type == OB_SURF) uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Deselected Control Points|Shift H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
	

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}
static void do_view3d_edit_curvemenu(void *arg, int event)
{
	switch(event) {
	
	case 0: /* Undo Editing */
		remake_editNurb();
		break;
	case 1: /* transformation properties */
		mainqenter(NKEY, 1);
		break;
	case 2: /* insert keyframe */
		common_insertkey();
		break;
	case 4: /* extrude */
		if (OBACT->type == OB_CURVE) {
			addvert_Nurb('e');
		} else if (OBACT->type == OB_SURF) {
			extrude_nurb();
		}
		break;
	case 5: /* duplicate */
		duplicate_context_selected();
		break;
	case 6: /* make segment */
		addsegment_nurb();
		break;
	case 7: /* toggle cyclic */
		makecyclicNurb();
		makeDispList(G.obedit);
		break;
	case 8: /* delete */
		delete_context_selected();
		break;
	case 9: /* proportional edit (toggle) */
		if(G.f & G_PROPORTIONAL) G.f &= ~G_PROPORTIONAL;
		else G.f |= G_PROPORTIONAL;
		break;
	case 13: /* Shear */
		transform('S');
		break;
	case 14: /* Warp */
		transform('w');
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_curvemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_curvemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_edit_curvemenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Reload Original|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL, "Transform Properties...|N",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBlockBut(block, view3d_edit_snapmenu, NULL, ICON_RIGHTARROW_THIN, "Snap", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert Keyframe|I",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Extrude|E",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Segment|F",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Toggle Cyclic|C",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete...|X",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBlockBut(block, view3d_edit_curve_controlpointsmenu, NULL, ICON_RIGHTARROW_THIN, "Control Points", 0, yco-=20, menuwidth, 19, "");
	uiDefIconTextBlockBut(block, view3d_edit_curve_segmentsmenu, NULL, ICON_RIGHTARROW_THIN, "Segments", 0, yco-=20, menuwidth, 19, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shear|Ctrl S",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 13, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Warp|Ctrl W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 14, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	if(G.f & G_PROPORTIONAL) {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Proportional Editing|O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
	} else {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Proportional Editing|O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
	}
	uiDefIconTextBlockBut(block, view3d_edit_propfalloffmenu, NULL, ICON_RIGHTARROW_THIN, "Proportional Falloff", 0, yco-=20, menuwidth, 19, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBlockBut(block, view3d_edit_curve_showhidemenu, NULL, ICON_RIGHTARROW_THIN, "Show/Hide Control Points", 0, yco-=20, menuwidth, 19, "");
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_edit_metaballmenu(void *arg, int event)
{
	switch(event) {
	case 1: /* duplicate */
		duplicate_context_selected();
		break;
	case 2: /* delete */
		delete_context_selected();
		break;
	case 3: /* Shear */
		transform('S');
		break;
	case 4: /* Warp */
		transform('w');
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_metaballmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
		
	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_metaballmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_edit_metaballmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete...|X",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shear|Ctrl S",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Warp|Ctrl W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");

	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_edit_text_charsmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* copyright */
		do_textedit(0,0,169);
		break;
	case 1: /* registered trademark */
		do_textedit(0,0,174);
		break;
	case 2: /* degree sign */
		do_textedit(0,0,176);
		break;
	case 3: /* Multiplication Sign */
		do_textedit(0,0,215);
		break;
	case 4: /* Circle */
		do_textedit(0,0,138);
		break;
	case 5: /* superscript 1 */
		do_textedit(0,0,185);
		break;
	case 6: /* superscript 2 */
		do_textedit(0,0,178);
		break;
	case 7: /* superscript 3 */
		do_textedit(0,0,179);
		break;
	case 8: /* double >> */
		do_textedit(0,0,187);
		break;
	case 9: /* double << */
		do_textedit(0,0,171);
		break;
	case 10: /* Promillage */
		do_textedit(0,0,139);
		break;
	case 11: /* dutch florin */
		do_textedit(0,0,164);
		break;
	case 12: /* british pound */
		do_textedit(0,0,163);
		break;
	case 13: /* japanese yen*/
		do_textedit(0,0,165);
		break;
	case 14: /* german S */
		do_textedit(0,0,223);
		break;
	case 15: /* spanish question mark */
		do_textedit(0,0,191);
		break;
	case 16: /* spanish exclamation mark */
		do_textedit(0,0,161);
		break;
		}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_text_charsmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_text_charsmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_text_charsmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Copyright|Alt C",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Registered Trademark|Alt R",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Degree Sign|Alt G",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Multiplication Sign|Alt x",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Circle|Alt .",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Superscript 1|Alt 1",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Superscript 2|Alt 2",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Superscript 3|Alt 3",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Double >>|Alt >",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Double <<|Alt <",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Promillage|Alt %",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Dutch Florin|Alt F",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "British Pound|Alt L",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Japanese Yen|Alt Y",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 13, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "German S|Alt S",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 14, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Spanish Question Mark|Alt ?",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 15, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Spanish Exclamation Mark|Alt !",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 16, "");
		
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
		
	return block;
}

static void do_view3d_edit_textmenu(void *arg, int event)
{
	switch(event) {
									
	case 0: /* Undo Editing */
		remake_editText();
		break;
	case 1: /* paste from file buffer */
		paste_editText();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_textmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_textmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_edit_textmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Editing|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Paste From Buffer File|Alt V",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBlockBut(block, view3d_edit_text_charsmenu, NULL, ICON_RIGHTARROW_THIN, "Special Characters", 0, yco-=20, 120, 19, "");

	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_edit_latticemenu(void *arg, int event)
{
	switch(event) {
									
	case 0: /* Undo Editing */
		remake_editLatt();
		break;
	case 2: /* insert keyframe */
		common_insertkey();
		break;
	case 3: /* Shear */
		transform('S');
		break;
	case 4: /* Warp */
		transform('w');
		break;
	case 5: /* proportional edit (toggle) */
		if(G.f & G_PROPORTIONAL) G.f &= ~G_PROPORTIONAL;
		else G.f |= G_PROPORTIONAL;
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_latticemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
		
	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_latticemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_edit_latticemenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Editing|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBlockBut(block, view3d_edit_snapmenu, NULL, ICON_RIGHTARROW_THIN, "Snap", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert Keyframe|I",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shear|Ctrl S",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Warp|Ctrl W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	if(G.f & G_PROPORTIONAL) {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Proportional Editing|O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	} else {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Proportional Editing|O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	}
	uiDefIconTextBlockBut(block, view3d_edit_propfalloffmenu, NULL, ICON_RIGHTARROW_THIN, "Proportional Falloff", 0, yco-=20, 120, 19, "");

	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_edit_armaturemenu(void *arg, int event)
{
	switch(event) {
	
	case 0: /* Undo Editing */
		remake_editArmature();
		break;
	case 1: /* transformation properties */
		mainqenter(NKEY, 1);
		break;
	case 3: /* extrude */
		extrude_armature();
		break;
	case 4: /* duplicate */
		duplicate_context_selected();
		break;
	case 5: /* delete */
		delete_context_selected();
		break;
	case 6: /* Shear */
		transform('S');
		break;
	case 7: /* Warp */
		transform('w');
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_armaturemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_armaturemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_edit_armaturemenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Editing|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL, "Transform Properties|N", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBlockBut(block, view3d_edit_snapmenu, NULL, ICON_RIGHTARROW_THIN, "Snap", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Extrude|E",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete|X",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shear|Ctrl S",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Warp|Ctrl W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");

	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	
	return block;
}

static void do_view3d_pose_armature_transformmenu(void *arg, int event)
{
	switch(event) {
	case 0: /*	clear origin */
		clear_object('o');
		break;
	case 1: /* clear size */
		clear_object('s');
		break;
	case 2: /* clear rotation */
		clear_object('r');
		break;
	case 3: /* clear location */
		clear_object('g');
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_pose_armature_transformmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_pose_armature_transformmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_pose_armature_transformmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Location|Alt G", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Rotation|Alt R", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Size|Alt S",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Origin|Alt O",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_pose_armaturemenu(void *arg, int event)
{
	switch(event) {
	
	case 0: /* transform properties */
		mainqenter(NKEY, 1);
		break;
	case 1: /* copy current pose */
		copy_posebuf();
		break;
	case 2: /* paste pose */
		paste_posebuf(0);
		break;
	case 3: /* paste flipped pose */
		paste_posebuf(1);
		break;
	case 4: /* insert keyframe */
		common_insertkey();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static void do_view3d_pose_armature_showhidemenu(void *arg, int event)
{
	
	switch(event) {
		 
	case 0: /* show hidden bones */
		show_all_pose_bones();
		break;
	case 1: /* hide selected bones */
		hide_selected_pose_bones();
		break;
	case 2: /* hide deselected bones */
		hide_unselected_pose_bones();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_pose_armature_showhidemenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_pose_armature_showhidemenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_pose_armature_showhidemenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Hidden|Alt H",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Selected|H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Deselected|Shift H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static uiBlock *view3d_pose_armaturemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_pose_armaturemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_pose_armaturemenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL, "Transform Properties|N", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBlockBut(block, view3d_pose_armature_transformmenu, NULL, ICON_RIGHTARROW_THIN, "Transform", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert Keyframe|I",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Copy Current Pose",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Paste Pose",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Paste Flipped Pose",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");	

	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBlockBut(block, view3d_pose_armature_showhidemenu, 
						  NULL, ICON_RIGHTARROW_THIN, 
						  "Show/Hide Bones", 0, yco-=20, 120, 19, "");

	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	
	return block;
}


static void do_view3d_paintmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* undo vertex painting */
		vpaint_undo();
		break;
	case 1: /* undo weight painting */
		wpaint_undo();
		break;
	case 2: /* clear vertex colors */
		clear_vpaint();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_paintmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_paintmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_paintmenu, NULL);
	
	if (G.f & G_VERTEXPAINT) uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Vertex Painting|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	if (G.f & G_WEIGHTPAINT) uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Weight Painting|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	if (G.f & G_TEXTUREPAINT) uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	if (G.f & G_VERTEXPAINT) {
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Vertex Colors|Shift K",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	}
		
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_facesel_propertiesmenu(void *arg, int event)
{
	extern TFace *lasttface;
	set_lasttface();
	
	switch(event) {
	case 0: /*	textured */
		lasttface->mode ^= TF_TEX;
		break;
	case 1: /* tiled*/
		lasttface->mode ^= TF_TILES;
		break;
	case 2: /* light */
		lasttface->mode ^= TF_LIGHT;
		break;
	case 3: /* invisible */
		lasttface->mode ^= TF_INVISIBLE;
		break;
	case 4: /* collision */
		lasttface->mode ^= TF_DYNAMIC;
		break;
	case 5: /* shared vertex colors */
		lasttface->mode ^= TF_SHAREDCOL;
		break;
	case 6: /* two sided */
		lasttface->mode ^= TF_TWOSIDE;
		break;
	case 7: /* use object color */
		lasttface->mode ^= TF_OBCOL;
		break;
	case 8: /* halo */
		lasttface->mode ^= TF_BILLBOARD;
		break;
	case 9: /* billboard */
		lasttface->mode ^= TF_BILLBOARD2;
		break;
	case 10: /* shadow */
		lasttface->mode ^= TF_SHADOW;
		break;
	case 11: /* text */
		lasttface->mode ^= TF_BMFONT;
		break;
	case 12: /* opaque blend mode */
		lasttface->transp = TF_SOLID;
		break;
	case 13: /* additive blend mode */
		lasttface->transp |= TF_ADD;
		break;
	case 14: /* alpha blend mode */
		lasttface->transp = TF_ALPHA;
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSLOGIC, 0);
}

static uiBlock *view3d_facesel_propertiesmenu(void *arg_unused)
{
	extern TFace *lasttface;
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	/* to display ticks/crosses depending on face properties */
	set_lasttface();

	block= uiNewBlock(&curarea->uiblocks, "view3d_facesel_propertiesmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_facesel_propertiesmenu, NULL);
	
	if (lasttface->mode & TF_TEX) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Textured",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Textured",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	
	if (lasttface->mode & TF_TILES) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Tiled",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Tiled",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	
	if (lasttface->mode & TF_LIGHT) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Light",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Light",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	
	if (lasttface->mode & TF_INVISIBLE) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Invisible",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Invisible",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	
	if (lasttface->mode & TF_DYNAMIC) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Collision",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Collision",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	
	if (lasttface->mode & TF_SHAREDCOL) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Shared Vertex Colors",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Shared Vertex Colors",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	
	if (lasttface->mode & TF_TWOSIDE) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Two Sided",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Two Sided",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
	
	if (lasttface->mode & TF_OBCOL) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Use Object Color",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Use Object Color",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	
	if (lasttface->mode & TF_BILLBOARD) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Halo",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Halo",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
	
	if (lasttface->mode & TF_BILLBOARD2) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Billboard",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Billboard",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");
		
	if (lasttface->mode & TF_SHADOW) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Shadow",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 10, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Shadow",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 10, "");
	
	if (lasttface->mode & TF_BMFONT) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Text",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 11, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Text",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 11, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	if (lasttface->transp == TF_SOLID) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Opaque Blend Mode",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 12, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Opaque Blend Mode",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 12, "");
	
	if (lasttface->transp == TF_ADD) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Additive Blend Mode",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 13, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Additive Blend Mode",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 13, "");
	
	if (lasttface->transp == TF_ALPHA) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Alpha Blend Mode",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 14, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Alpha Blend Mode",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 14, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_facesel_showhidemenu(void *arg, int event)
{
	switch(event) {
	case 4: /* show hidden faces */
		reveal_tface();
		break;
	case 5: /* hide selected faces */
		hide_tface();
		break;
	case 6: /* hide deselected faces */
		G.qual |= LR_SHIFTKEY;
		hide_tface();
		G.qual &= ~LR_SHIFTKEY;
		break;
		}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_facesel_showhidemenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_facesel_showhidemenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_facesel_showhidemenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Hidden Faces|Alt H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Selected Faces|H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Deselected Faces|Shift H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}
static void do_view3d_faceselmenu(void *arg, int event)
{
	/* code copied from buttons.c :(	
		would be nice if it was split up into functions */
	Mesh *me;
	Object *ob;
	extern TFace *lasttface; /* caches info on tface bookkeeping ?*/
	
	ob= OBACT;
	
	switch(event) {
	case 0: /* copy draw mode */
	case 1: /* copy UVs */
	case 2: /* copy vertex colors */
		me= get_mesh(ob);
		if(me && me->tface) {

			TFace *tface= me->tface;
			int a= me->totface;
			
			set_lasttface();
			if(lasttface) {
			
				while(a--) {
					if(tface!=lasttface && (tface->flag & TF_SELECT)) {
						if(event==0) {
							tface->mode= lasttface->mode;
							tface->transp= lasttface->transp;
						} else if(event==1) {
							memcpy(tface->uv, lasttface->uv, sizeof(tface->uv));
							tface->tpage= lasttface->tpage;
							tface->tile= lasttface->tile;
							
							if(lasttface->mode & TF_TILES) tface->mode |= TF_TILES;
							else tface->mode &= ~TF_TILES;
							
						} else if(event==2) memcpy(tface->col, lasttface->col, sizeof(tface->col));
					}
					tface++;
				}
			}
			do_shared_vertexcol(me);	
		}
		break;
	case 3: /* clear vertex colors */
		clear_vpaint_selectedfaces();
		break;
	case 8: /* uv calculation */
		uv_autocalc_tface();
		break;
	case 7: /* rotate UVs */
		rotate_uv_tface();
		break;
	
	}
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSLOGIC, 0);
	allqueue(REDRAWIMAGE, 0);
}

static uiBlock *view3d_faceselmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	set_lasttface();
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_faceselmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_faceselmenu, NULL);
	
	uiDefIconTextBlockBut(block, view3d_facesel_propertiesmenu, NULL, ICON_RIGHTARROW_THIN, "Active Draw Mode", 0, yco-=20, 120, 19, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Copy Draw Mode",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");

	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Unwrap UVs|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Rotate UVs|R",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");

	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Copy UVs & Textures",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Copy Vertex Colors",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Vertex Colors|Shift K",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBlockBut(block, view3d_facesel_showhidemenu, NULL, ICON_RIGHTARROW_THIN, "Show/Hide Faces", 0, yco-=20, 120, 19, "");

	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	return block;
}


static char *view3d_modeselect_pup(void)
{
	static char string[1024];
	char formatstr[1024];
	char tempstr[1024];
	
	strcpy(string, "Mode: %t");
	strcpy(formatstr, "|%s %%x%d %%i%d");
	
	sprintf(tempstr, formatstr, "Object Mode", V3D_OBJECTMODE_SEL, ICON_OBJECT);
	strcat(string, tempstr);
	
	/* if active object is editable */
	if (OBACT && ((OBACT->type == OB_MESH) || (OBACT->type == OB_ARMATURE)
		|| (OBACT->type == OB_CURVE) || (OBACT->type == OB_SURF) || (OBACT->type == OB_FONT)
		|| (OBACT->type == OB_MBALL) || (OBACT->type == OB_LATTICE))) {
		
		sprintf(tempstr, formatstr, "Edit Mode", V3D_EDITMODE_SEL, ICON_EDITMODE_HLT);
		strcat(string, tempstr);
	}

	if (OBACT && OBACT->type == OB_MESH) {
	
		sprintf(tempstr, formatstr, "UV Face Select", V3D_FACESELECTMODE_SEL, ICON_FACESEL_HLT);
		strcat(string, tempstr);
		sprintf(tempstr, formatstr, "Vertex Paint", V3D_VERTEXPAINTMODE_SEL, ICON_VPAINT_HLT);
		strcat(string, tempstr);
		sprintf(tempstr, formatstr, "Texture Paint", V3D_TEXTUREPAINTMODE_SEL, ICON_TPAINT_HLT);
		strcat(string, tempstr);

	
		if ( ((Mesh*)(OBACT->data))->dvert)  {
			sprintf(tempstr, formatstr, "Weight Paint", V3D_WEIGHTPAINTMODE_SEL, ICON_WPAINT_HLT);
			strcat(string, tempstr);
		}
	}

	
	/* if active object is an armature */
	if (OBACT && OBACT->type==OB_ARMATURE) {
		sprintf(tempstr, formatstr, "Pose Mode", V3D_POSEMODE_SEL, ICON_POSE_HLT);
		strcat(string, tempstr);
	}
	
	return (string);
}


static char *drawtype_pup(void)
{
	static char string[512];

	strcpy(string, "Draw type:%t"); 
	strcat(string, "|Bounding Box %x1"); 
	strcat(string, "|Wireframe %x2");
	strcat(string, "|Solid %x3");
	strcat(string, "|Shaded %x4");
	strcat(string, "|Textured %x5");
	return (string);
}
static char *around_pup(void)
{
	static char string[512];

	strcpy(string, "Pivot:%t"); 
	strcat(string, "|Bounding Box Center %x0"); 
	strcat(string, "|Median Point %x3");
	strcat(string, "|3D Cursor %x1");
	strcat(string, "|Individual Object Centers %x2");
	return (string);
}

char *propfalloff_pup(void)
{
	static char string[512];

	strcpy(string, "Falloff:%t"); 
	strcat(string, "|Sharp Falloff%x0"); 
	strcat(string, "|Smooth Falloff%x1");
	return (string);
}


void do_view3d_buttons(short event)
{
	int bit;

	/* watch it: if curarea->win does not exist, check that when calling direct drawing routines */

	switch(event) {
	case B_HOME:
		view3d_home(0);
		break;
	case B_SCENELOCK:
		if(G.vd->scenelock) {
			G.vd->lay= G.scene->lay;
			/* seek for layact */
			bit= 0;
			while(bit<32) {
				if(G.vd->lay & (1<<bit)) {
					G.vd->layact= 1<<bit;
					break;
				}
				bit++;
			}
			G.vd->camera= G.scene->camera;
			scrarea_queue_winredraw(curarea);
			scrarea_queue_headredraw(curarea);
		}
		break;
	case B_LOCALVIEW:
		if(G.vd->localview) initlocalview();
		else endlocalview(curarea);
		scrarea_queue_headredraw(curarea);
		break;
	case B_EDITMODE:
		if (G.f & G_VERTEXPAINT) {
			/* Switch off vertex paint */
			G.f &= ~G_VERTEXPAINT;
		}
		if (G.f & G_WEIGHTPAINT){
			/* Switch off weight paint */
			G.f &= ~G_WEIGHTPAINT;
		}
#ifdef NAN_TPT
		if (G.f & G_TEXTUREPAINT) {
			/* Switch off texture paint */
			G.f &= ~G_TEXTUREPAINT;
		}
#endif /* NAN_VPT */
		if(G.obedit==0) enter_editmode();
		else exit_editmode(1);
		scrarea_queue_headredraw(curarea);
		break;
	case B_POSEMODE:
	/*	if (G.obedit){
			error("Unable to perform function in EditMode");
			G.vd->flag &= ~V3D_POSEMODE;
			scrarea_queue_headredraw(curarea);
		}
		else{
		*/	
		if (G.obpose==NULL) enter_posemode();
		else exit_posemode(1);

		allqueue(REDRAWHEADERS, 0); 
		
		break;
	case B_WPAINT:
		if (G.f & G_VERTEXPAINT) {
			/* Switch off vertex paint */
			G.f &= ~G_VERTEXPAINT;
		}
#ifdef NAN_TPT
		if ((!(G.f & G_WEIGHTPAINT)) && (G.f & G_TEXTUREPAINT)) {
			/* Switch off texture paint */
			G.f &= ~G_TEXTUREPAINT;
		}
#endif /* NAN_VPT */
		if(G.obedit) {
			error("Unable to perform function in EditMode");
			G.vd->flag &= ~V3D_WEIGHTPAINT;
			scrarea_queue_headredraw(curarea);
		}
		else if(G.obpose) {
			error("Unable to perform function in PoseMode");
			G.vd->flag &= ~V3D_WEIGHTPAINT;
			scrarea_queue_headredraw(curarea);
		}
		else set_wpaint();
		break;
	case B_VPAINT:
		if ((!(G.f & G_VERTEXPAINT)) && (G.f & G_WEIGHTPAINT)) {
			G.f &= ~G_WEIGHTPAINT;
		}
#ifdef NAN_TPT
		if ((!(G.f & G_VERTEXPAINT)) && (G.f & G_TEXTUREPAINT)) {
			/* Switch off texture paint */
			G.f &= ~G_TEXTUREPAINT;
		}
#endif /* NAN_VPT */
		if(G.obedit) {
			error("Unable to perform function in EditMode");
			G.vd->flag &= ~V3D_VERTEXPAINT;
			scrarea_queue_headredraw(curarea);
		}
		else if(G.obpose) {
			error("Unable to perform function in PoseMode");
			G.vd->flag &= ~V3D_VERTEXPAINT;
			scrarea_queue_headredraw(curarea);
		}
		else set_vpaint();
		break;
		
#ifdef NAN_TPT
	case B_TEXTUREPAINT:
		if (G.f & G_TEXTUREPAINT) {
			G.f &= ~G_TEXTUREPAINT;
		}
		else {
			if (G.obedit) {
				error("Unable to perform function in EditMode");
				G.vd->flag &= ~V3D_TEXTUREPAINT;
			}
			else {
				if (G.f & G_WEIGHTPAINT){
					/* Switch off weight paint */
					G.f &= ~G_WEIGHTPAINT;
				}
				if (G.f & G_VERTEXPAINT) {
					/* Switch off vertex paint */
					G.f &= ~G_VERTEXPAINT;
				}
				if (G.f & G_FACESELECT) {
					/* Switch off face select */
					G.f &= ~G_FACESELECT;
				}
				G.f |= G_TEXTUREPAINT;
				scrarea_queue_headredraw(curarea);
			}
		}
		break;
#endif /* NAN_TPT */

	case B_FACESEL:
		if(G.obedit) {
			error("Unable to perform function in EditMode");
			G.vd->flag &= ~V3D_FACESELECT;
			scrarea_queue_headredraw(curarea);
		}
		else if(G.obpose) {
			error("Unable to perform function in PoseMode");
			G.vd->flag &= ~V3D_FACESELECT;
			scrarea_queue_headredraw(curarea);
		}
		else set_faceselect();
		break;
		
	case B_VIEWBUT:
	
		if(G.vd->viewbut==1) persptoetsen(PAD7);
		else if(G.vd->viewbut==2) persptoetsen(PAD1);
		else if(G.vd->viewbut==3) persptoetsen(PAD3);
		break;

	case B_PERSP:
	
		if(G.vd->persp==2) persptoetsen(PAD0);
		else {
			G.vd->persp= 1-G.vd->persp;
			persptoetsen(PAD5);
		}
		
		break;
	case B_PROPTOOL:
		allqueue(REDRAWHEADERS, 0);
		break;
	case B_VIEWRENDER:
		if (curarea->spacetype==SPACE_VIEW3D) {
			BIF_do_ogl_render(curarea->spacedata.first, G.qual!=0 );
		}
		break;
	case B_STARTGAME:
		if (select_area(SPACE_VIEW3D)) {
				start_game();
		}
		break;
	case B_VIEWZOOM:
		viewmovetemp= 0;
		viewmove(2);
		scrarea_queue_headredraw(curarea);
		break;
	case B_VIEWTRANS:
		viewmovetemp= 0;
		viewmove(1);
		scrarea_queue_headredraw(curarea);
		break;
	case B_MODESELECT:
		if (G.vd->modeselect == V3D_OBJECTMODE_SEL) { 
			G.vd->flag &= ~V3D_MODE;
			G.f &= ~G_VERTEXPAINT;		/* Switch off vertex paint */
			G.f &= ~G_TEXTUREPAINT;		/* Switch off texture paint */
			G.f &= ~G_WEIGHTPAINT;		/* Switch off weight paint */
			G.f &= ~G_FACESELECT;		/* Switch off face select */
			if (G.obpose) exit_posemode(1); /* exit posemode */
			if(G.obedit) exit_editmode(1);	/* exit editmode */
		} else if (G.vd->modeselect == V3D_EDITMODE_SEL) {
			if(!G.obedit) {
				G.vd->flag &= ~V3D_MODE;
				G.f &= ~G_VERTEXPAINT;		/* Switch off vertex paint */
				G.f &= ~G_TEXTUREPAINT;		/* Switch off texture paint */
				G.f &= ~G_WEIGHTPAINT;		/* Switch off weight paint */
				if (G.obpose) exit_posemode(1); /* exit posemode */
					
				enter_editmode();
			}
		} else if (G.vd->modeselect == V3D_FACESELECTMODE_SEL) {
			if ((G.obedit) && (G.f & G_FACESELECT)) {
				exit_editmode(1); /* exit editmode */
			} else if ((G.f & G_FACESELECT) && (G.f & G_VERTEXPAINT)) {
				G.f &= ~G_VERTEXPAINT;	
			} else if ((G.f & G_FACESELECT) && (G.f & G_TEXTUREPAINT)) {
				G.f &= ~G_TEXTUREPAINT; 
			} else {
				G.vd->flag &= ~V3D_MODE;
				G.f &= ~G_VERTEXPAINT;		/* Switch off vertex paint */
				G.f &= ~G_TEXTUREPAINT;		/* Switch off texture paint */
				G.f &= ~G_WEIGHTPAINT;		/* Switch off weight paint */
				if (G.obpose) exit_posemode(1); /* exit posemode */
				if (G.obedit) exit_editmode(1); /* exit editmode */
				
				set_faceselect();
			}
		} else if (G.vd->modeselect == V3D_VERTEXPAINTMODE_SEL) {
			if (!(G.f & G_VERTEXPAINT)) {
				G.vd->flag &= ~V3D_MODE;
				G.f &= ~G_TEXTUREPAINT;		/* Switch off texture paint */
				G.f &= ~G_WEIGHTPAINT;		/* Switch off weight paint */
				if (G.obpose) exit_posemode(1); /* exit posemode */
				if(G.obedit) exit_editmode(1);	/* exit editmode */
					
				set_vpaint();
			}
		} else if (G.vd->modeselect == V3D_TEXTUREPAINTMODE_SEL) {
			if (!(G.f & G_TEXTUREPAINT)) {
				G.vd->flag &= ~V3D_MODE;
				G.f &= ~G_VERTEXPAINT;		/* Switch off vertex paint */
				G.f &= ~G_WEIGHTPAINT;		/* Switch off weight paint */
				if (G.obpose) exit_posemode(1); /* exit posemode */
				if(G.obedit) exit_editmode(1);	/* exit editmode */
					
				G.f |= G_TEXTUREPAINT;		/* Switch on texture paint flag */
			}
		} else if (G.vd->modeselect == V3D_WEIGHTPAINTMODE_SEL) {
			if (!(G.f & G_WEIGHTPAINT) && (OBACT && OBACT->type == OB_MESH) && ((((Mesh*)(OBACT->data))->dvert))) {
				G.vd->flag &= ~V3D_MODE;
				G.f &= ~G_VERTEXPAINT;		/* Switch off vertex paint */
				G.f &= ~G_TEXTUREPAINT;		/* Switch off texture paint */
				if (G.obpose) exit_posemode(1); /* exit posemode */
				if(G.obedit) exit_editmode(1);	/* exit editmode */
				
				set_wpaint();
			}
		} else if (G.vd->modeselect == V3D_POSEMODE_SEL) {
			if (!G.obpose) {
				G.vd->flag &= ~V3D_MODE;
				if(G.obedit) exit_editmode(1);	/* exit editmode */
					
				enter_posemode();
			}
		}
		allqueue(REDRAWVIEW3D, 0);
		break;
	
	default:

		if(event>=B_LAY && event<B_LAY+31) {
			if(G.vd->lay!=0 && (G.qual & LR_SHIFTKEY)) {
				
				/* but do find active layer */
				
				bit= event-B_LAY;
				if( G.vd->lay & (1<<bit)) G.vd->layact= 1<<bit;
				else {
					if( (G.vd->lay & G.vd->layact) == 0) {
						bit= 0;
						while(bit<32) {
							if(G.vd->lay & (1<<bit)) {
								G.vd->layact= 1<<bit;
								break;
							}
							bit++;
						}
					}
				}
			}
			else {
				bit= event-B_LAY;
				G.vd->lay= 1<<bit;
				G.vd->layact= G.vd->lay;
				scrarea_queue_headredraw(curarea);
			}
			scrarea_queue_winredraw(curarea);
			countall();

			if(G.vd->scenelock) handle_view3d_lock();
			allqueue(REDRAWOOPS, 0);
		}
		break;
	}
}

static void view3d_header_pulldowns(uiBlock *block, short *xcoord)
{
	short xmax, xco= *xcoord;
	
	/* pull down menus */
	uiBlockSetEmboss(block, UI_EMBOSSP);
	
	/* compensate for local mode when setting up the viewing menu/iconrow values */
	if(G.vd->view==7) G.vd->viewbut= 1;
	else if(G.vd->view==1) G.vd->viewbut= 2;
	else if(G.vd->view==3) G.vd->viewbut= 3;
	else G.vd->viewbut= 0;
	
	
	/* the 'xmax - 3' rather than xmax is to prevent some weird flickering where the highlighted
	 * menu is drawn wider than it should be. The ypos of -2 is to make it properly fill the
	 * height of the header */
	
	xmax= GetButStringLength("View");
	uiDefBlockBut(block, view3d_viewmenu, NULL, "View", xco, -2, xmax-3, 24, "");
	xco+= xmax;
	
	xmax= GetButStringLength("Select");
	if (G.obedit) {
		if (OBACT && OBACT->type == OB_MESH) {
			uiDefBlockBut(block, view3d_select_meshmenu, NULL, "Select",	xco,-2, xmax-3, 24, "");
		} else if (OBACT && (OBACT->type == OB_CURVE || OBACT->type == OB_SURF)) {
			uiDefBlockBut(block, view3d_select_curvemenu, NULL, "Select", xco,-2, xmax-3, 24, "");
		} else if (OBACT && OBACT->type == OB_FONT) {
			uiDefBlockBut(block, view3d_select_meshmenu, NULL, "Select",	xco, -2, xmax-3, 24, "");
		} else if (OBACT && OBACT->type == OB_MBALL) {
			uiDefBlockBut(block, view3d_select_metaballmenu, NULL, "Select",	xco,-2, xmax-3, 24, "");
		} else if (OBACT && OBACT->type == OB_LATTICE) {
			uiDefBlockBut(block, view3d_select_latticemenu, NULL, "Select", xco,-2, xmax-3, 24, "");
		} else if (OBACT && OBACT->type == OB_ARMATURE) {
			uiDefBlockBut(block, view3d_select_armaturemenu, NULL, "Select",	xco,-2, xmax-3, 24, "");
		}
	} else if (G.f & G_FACESELECT) {
		if (OBACT && OBACT->type == OB_MESH) {
			uiDefBlockBut(block, view3d_select_faceselmenu, NULL, "Select", xco,-2, xmax-3, 24, "");
		}
	} else if (G.obpose) {
		if (OBACT && OBACT->type == OB_ARMATURE) {
			uiDefBlockBut(block, view3d_select_pose_armaturemenu, NULL, "Select", xco,-2, xmax-3, 24, "");
		}
	} else if ((G.f & G_VERTEXPAINT) || (G.f & G_TEXTUREPAINT) || (G.f & G_WEIGHTPAINT)) {
		uiDefBut(block, LABEL,0,"", xco, 0, xmax, 20, 0, 0, 0, 0, 0, "");
	} else {
		uiDefBlockBut(block, view3d_select_objectmenu, NULL, "Select",	xco,-2, xmax-3, 24, "");
	}
	xco+= xmax;
	
	
	if (G.obedit) {
		if (OBACT && OBACT->type == OB_MESH) {
			xmax= GetButStringLength("Mesh");
			uiDefBlockBut(block, view3d_edit_meshmenu, NULL, "Mesh",	xco,-2, xmax-3, 24, "");
			xco+= xmax;
		} else if (OBACT && OBACT->type == OB_CURVE) {
			xmax= GetButStringLength("Curve");
			uiDefBlockBut(block, view3d_edit_curvemenu, NULL, "Curve",	xco,-2, xmax-3, 24, "");
			xco+= xmax;
		} else if (OBACT && OBACT->type == OB_SURF) {
			xmax= GetButStringLength("Surface");
			uiDefBlockBut(block, view3d_edit_curvemenu, NULL, "Surface",	xco,-2, xmax-3, 24, "");
			xco+= xmax;
		} else if (OBACT && OBACT->type == OB_FONT) {
			xmax= GetButStringLength("Text");
			uiDefBlockBut(block, view3d_edit_textmenu, NULL, "Text",	xco,-2, xmax-3, 24, "");
			xco+= xmax;
		} else if (OBACT && OBACT->type == OB_MBALL) {
			xmax= GetButStringLength("Metaball");
			uiDefBlockBut(block, view3d_edit_metaballmenu, NULL, "Metaball",	xco,-2, xmax-3, 24, "");
			xco+= xmax;
		} else if (OBACT && OBACT->type == OB_LATTICE) {
			xmax= GetButStringLength("Lattice");
			uiDefBlockBut(block, view3d_edit_latticemenu, NULL, "Lattice",	xco,-2, xmax-3, 24, "");
			xco+= xmax;
		} else if (OBACT && OBACT->type == OB_ARMATURE) {
			xmax= GetButStringLength("Armature");
			uiDefBlockBut(block, view3d_edit_armaturemenu, NULL, "Armature",	xco,-2, xmax-3, 24, "");
			xco+= xmax;
		}
	}
	else if ((G.f & G_VERTEXPAINT) || (G.f & G_TEXTUREPAINT) || (G.f & G_WEIGHTPAINT)) {
		xmax= GetButStringLength("Paint");
		uiDefBlockBut(block, view3d_paintmenu, NULL, "Paint", xco,-2, xmax-3, 24, "");
		xco+= xmax;
	} 
	else if (G.f & G_FACESELECT) {
		if (OBACT && OBACT->type == OB_MESH) {
			xmax= GetButStringLength("Face");
			uiDefBlockBut(block, view3d_faceselmenu, NULL, "Face",	xco,-2, xmax-3, 24, "");
			xco+= xmax;
		}
	} else if (G.obpose) {
		if (OBACT && OBACT->type == OB_ARMATURE) {
			xmax= GetButStringLength("Armature");
			uiDefBlockBut(block, view3d_pose_armaturemenu, NULL, "Armature",	xco,-2, xmax-3, 24, "");
			xco+= xmax;
		}
	} else {
		xmax= GetButStringLength("Object");
		uiDefBlockBut(block, view3d_edit_objectmenu, NULL, "Object",	xco,-2, xmax-3, 24, "");
		xco+= xmax;
	}

	*xcoord= xco;
}

void view3d_buttons(void)
{
	uiBlock *block;
	int a;
	short xco = 0;
	
	block= uiNewBlock(&curarea->uiblocks, "header view3d", UI_EMBOSS, UI_HELV, curarea->headwin);

	if(area_is_active_area(curarea)) uiBlockSetCol(block, TH_HEADER);
	else uiBlockSetCol(block, TH_HEADERDESEL);

	curarea->butspacetype= SPACE_VIEW3D;
	
	xco = 8;
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), xco,0,XIC+10,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");
	xco+= XIC+14;

	uiBlockSetEmboss(block, UI_EMBOSSN);
	if(curarea->flag & HEADER_NO_PULLDOWN) {
		uiDefIconButS(block, TOG|BIT|0, B_FLIPINFOMENU, ICON_DISCLOSURE_TRI_RIGHT,
				xco,2,XIC,YIC-2,
				&(curarea->flag), 0, 0, 0, 0, "Enables display of pulldown menus");
	} else {
		uiDefIconButS(block, TOG|BIT|0, B_FLIPINFOMENU, ICON_DISCLOSURE_TRI_DOWN,
				xco,2,XIC,YIC-2,
				&(curarea->flag), 0, 0, 0, 0, "Hides pulldown menus");
	}
	uiBlockSetEmboss(block, UI_EMBOSS);
	xco+=XIC;

	if((curarea->flag & HEADER_NO_PULLDOWN)==0) 
		view3d_header_pulldowns(block, &xco);

	/* other buttons: */
	uiBlockSetEmboss(block, UI_EMBOSS);
	
	/* mode */
	G.vd->modeselect = V3D_OBJECTMODE_SEL;
	if (G.f & G_WEIGHTPAINT) G.vd->modeselect = V3D_WEIGHTPAINTMODE_SEL;
	else if (G.f & G_VERTEXPAINT) G.vd->modeselect = V3D_VERTEXPAINTMODE_SEL;
	else if (G.f & G_TEXTUREPAINT) G.vd->modeselect = V3D_TEXTUREPAINTMODE_SEL;
	else if(G.f & G_FACESELECT) G.vd->modeselect = V3D_FACESELECTMODE_SEL;
	if (G.obpose) G.vd->modeselect = V3D_POSEMODE_SEL;
	if (G.obedit) G.vd->modeselect = V3D_EDITMODE_SEL;
		
	G.vd->flag &= ~V3D_MODE;
	if(G.obedit) G.vd->flag |= V3D_EDITMODE;
	if(G.f & G_VERTEXPAINT) G.vd->flag |= V3D_VERTEXPAINT;
	if(G.f & G_WEIGHTPAINT) G.vd->flag |= V3D_WEIGHTPAINT;
#ifdef NAN_TPT
	if (G.f & G_TEXTUREPAINT) G.vd->flag |= V3D_TEXTUREPAINT;
#endif /* NAN_TPT */
	if(G.f & G_FACESELECT) G.vd->flag |= V3D_FACESELECT;
	if(G.obpose){
		G.vd->flag |= V3D_POSEMODE;
	}
	
	uiDefIconTextButS(block, MENU, B_MODESELECT, (G.vd->modeselect),view3d_modeselect_pup() , 
																xco,0,126,20, &(G.vd->modeselect), 0, 0, 0, 0, "Mode:");
	
	xco+= 126+8;
	
	/* DRAWTYPE */
	uiDefIconTextButS(block, ICONTEXTROW,B_REDR, ICON_BBOX, drawtype_pup(), xco,0,XIC+10,YIC, &(G.vd->drawtype), 1.0, 5.0, 0, 0, "Viewport Shading (Hotkeys: Z, Shift Z, Ctrl Z, Alt Z,");

	// uiDefIconButS(block, ICONROW, B_REDR, ICON_BBOX,	xco,0,XIC+10,YIC, &(G.vd->drawtype), 1.0, 5.0, 0, 0, "Drawtype: boundbox/wire/solid/shaded (ZKEY, SHIFT+Z)");

	// uiDefIconTextButS(block, MENU, REDRAWVIEW3D, (ICON_BBOX+G.vd->drawtype-1), "Viewport Shading%t|Bounding Box %x1|Wireframe %x2|Solid %x3|Shaded %x4|Textured %x5",	
	//														xco,0,124,20, &(G.vd->drawtype), 0, 0, 0, 0, "Viewport Shading");
	//	uiDefButS(block, MENU, REDRAWVIEW3D, "Viewport Shading%t|Bounding Box %x1|Wireframe %x2|Solid %x3|Shaded %x4|Textured %x5", 
	//																xco,0,110,20, &(G.vd->drawtype), 0, 0, 0, 0, "Viewport Shading");
	
	
	/* around */
	xco+= XIC+18;
	uiDefIconTextButS(block, ICONTEXTROW,B_REDR, ICON_ROTATE, around_pup(), xco,0,XIC+10,YIC, &(G.vd->around), 0, 3.0, 0, 0, "Rotation/Scaling Pivot (Hotkeys: Comma, Period) ");
	/*
	uiDefIconButS(block, ROW, 1, ICON_ROTATE, xco+=XIC,0,XIC,YIC, &G.vd->around, 3.0, 0.0, 0, 0, "Enables Rotation or Scaling around boundbox center (COMMAKEY)");
	uiDefIconButS(block, ROW, 1, ICON_ROTATECENTER, xco+=XIC,0,XIC,YIC, &G.vd->around, 3.0, 3.0, 0, 0, "Enables Rotation or Scaling around median point");
	uiDefIconButS(block, ROW, 1, ICON_CURSOR, xco+=XIC,0,XIC,YIC, &G.vd->around, 3.0, 1.0, 0, 0, "Enables Rotation or Scaling around cursor (DOTKEY)");
	uiDefIconButS(block, ROW, 1, ICON_ROTATECOLLECTION, xco+=XIC,0,XIC,YIC, &G.vd->around, 3.0, 2.0, 0, 0, "Enables Rotation or Scaling around individual object centers");
	*/
	
	
	
	xco+= XIC+18;
	/* LAYERS */
	if(G.vd->localview==0) {
		
		uiBlockBeginAlign(block);
		for(a=0; a<5; a++)
			uiDefButI(block, TOG|BIT|a, B_LAY+a, "",	(short)(xco+a*(XIC/2)), (short)(YIC/2),(short)(XIC/2),(short)(YIC/2), &(G.vd->lay), 0, 0, 0, 0, "Toggles Layer visibility");
		for(a=0; a<5; a++)
			uiDefButI(block, TOG|BIT|(a+10), B_LAY+10+a, "",(short)(xco+a*(XIC/2)), 0,			XIC/2, (YIC)/2, &(G.vd->lay), 0, 0, 0, 0, "Toggles Layer visibility");
			
		xco+= 5;
		uiBlockBeginAlign(block);
		for(a=5; a<10; a++)
			uiDefButI(block, TOG|BIT|a, B_LAY+a, "",	(short)(xco+a*(XIC/2)), (short)(YIC/2),(short)(XIC/2),(short)(YIC/2), &(G.vd->lay), 0, 0, 0, 0, "Toggles Layer visibility");
		for(a=5; a<10; a++)
			uiDefButI(block, TOG|BIT|(a+10), B_LAY+10+a, "",(short)(xco+a*(XIC/2)), 0,			XIC/2, (YIC)/2, &(G.vd->lay), 0, 0, 0, 0, "Toggles Layer visibility");

		uiBlockEndAlign(block);
		
		xco+= (a-2)*(XIC/2)+3;

		/* LOCK */
		uiDefIconButS(block, ICONTOG, B_SCENELOCK, ICON_UNLOCKED, xco+=XIC,0,XIC,YIC, &(G.vd->scenelock), 0, 0, 0, 0, "Locks layers and used Camera to Scene");
		xco+= XIC+10;

	}
	else xco+= (10+1)*(XIC/2)+10;
	
	if(G.obedit && (OBACT->type == OB_MESH || OBACT->type == OB_CURVE || OBACT->type == OB_SURF || OBACT->type == OB_LATTICE)) {
		extern int prop_mode;
		if(G.f & G_PROPORTIONAL) {
			uiDefIconTextButI(block, ICONTEXTROW,B_REDR, ICON_SHARPCURVE, propfalloff_pup(), xco,0,XIC+10,YIC, &(prop_mode), 0, 1.0, 0, 0, "Proportional Edit Falloff (Hotkey: Shift O) ");
			xco+= XIC+20;
		}
	}

	uiDefIconBut(block, BUT, B_VIEWRENDER, ICON_SCENE_DEHLT, xco,0,XIC,YIC, NULL, 0, 1.0, 0, 0, "Render this window (hold CTRL for anim)");
	
	if (G.obpose){
		xco+= XIC/2;
		if(curarea->headertype==HEADERTOP) {
			uiDefIconBut(block, BUT, B_ACTCOPY, ICON_COPYUP, 
						 xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, 
						 "Copies the current pose to the buffer");
			uiSetButLock(G.obpose->id.lib!=0, "Can't edit library data");
			uiDefIconBut(block, BUT, B_ACTPASTE, ICON_PASTEUP,	
						 xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, 
						 "Pastes the pose from the buffer");
			uiDefIconBut(block, BUT, B_ACTPASTEFLIP, ICON_PASTEFLIPUP,
						 xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, 
						 "Pastes the mirrored pose from the buffer");
		}
		else {
			uiDefIconBut(block, BUT, B_ACTCOPY, ICON_COPYDOWN,
						 xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, 
						 "Copies the current pose to the buffer");
			uiSetButLock(G.obpose->id.lib!=0, "Can't edit library data");
			uiDefIconBut(block, BUT, B_ACTPASTE, ICON_PASTEDOWN,
						 xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, 
						 "Pastes the pose from the buffer");
			uiDefIconBut(block, BUT, B_ACTPASTEFLIP, ICON_PASTEFLIPDOWN, 
						 xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, 
						 "Pastes the mirrored pose from the buffer");
		}
	}

	/* Always do this last */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);

}
