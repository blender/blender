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
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_image.h"

#include "BLI_blenlib.h"

#include "BSE_edit.h"
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
#include "interface.h"
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

#define V3D_OBJECTMODE_SEL				ICON_ORTHO
#define V3D_EDITMODE_SEL					ICON_EDITMODE_HLT
#define V3D_FACESELECTMODE_SEL		ICON_FACESEL_HLT
#define V3D_VERTEXPAINTMODE_SEL		ICON_VPAINT_HLT
#define V3D_TEXTUREPAINTMODE_SEL	ICON_TPAINT_HLT
#define V3D_WEIGHTPAINTMODE_SEL		ICON_WPAINT_HLT
#define V3D_POSEMODE_SEL					ICON_POSE_HLT

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
	case 4: /* Zoom In */
		persptoetsen(PADPLUSKEY);
		break;
	case 5: /* Zoom Out */
		persptoetsen(PADMINUS);
		break;
	case 6: /* Reset Zoom */
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
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Orbit Left|NumPad 4",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Orbit Right|NumPad 6", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Orbit Up|NumPad 8",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Orbit Down|NumPad 2",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, 140, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom In|NumPad +", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom Out|NumPad -",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Reset Zoom|NumPad Enter",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 50);
	return block;
}


static void do_view3d_viewmenu(void *arg, int event)
{
	extern int play_anim(int mode);

	float *curs;
	
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
	case 9: /* Frame All (Home) */
		view3d_home(0);
		break;
	case 10: /* Center at Cursor */
		curs= give_cursor();
		G.vd->ofs[0]= -curs[0];
		G.vd->ofs[1]= -curs[1];
		G.vd->ofs[2]= -curs[2];
		scrarea_queue_winredraw(curarea);
		break;
	case 11: /* Center View to Selected */
		centreview();
		break;
	case 12: /* Align View to Selected */
		mainqenter(PADASTERKEY, 1);
		break;
	case 13: /* Play Back Animation */
		play_anim(0);
		break;
	case 14: /* Backdrop and settings Panel */
		add_blockhandler(curarea, VIEW3D_HANDLER_SETTINGS);
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
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BUTS, "Backdrop and Settings Panel",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 14, "");
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
	
	uiDefIconTextBlockBut(block, view3d_view_cameracontrolsmenu, NULL, ICON_RIGHTARROW_THIN, "Viewport Navigation", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Frame All|Home",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Frame Cursor|C",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 10, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Frame Selected|NumPad .",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 11, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Align View to Selected|NumPad *",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 12, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Play Back Animation|Alt A",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 13, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	if(!curarea->full) uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Maximize Window|Ctrl UpArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0,0, "");
	else uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Tile Window|Ctrl DownArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");

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

static void do_view3d_select_objectmenu(void *arg, int event)
{
//	extern void borderselect(void);
//	extern void deselectall(void);
	
	switch(event) {
	
	case 0: /* border select */
		borderselect();
		break;
	case 1: /* Select/Deselect All */
		deselectall();
		break;
	case 2: /* Select Linked */
		selectlinks();
		break;
	case 3: /* Select Grouped */
		group_menu();
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
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Linked...|Shift L",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Grouped...|Shift G",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		
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

static void do_view3d_select_meshmenu(void *arg, int event)
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
			G.qual |= LR_CTRLKEY;
			selectconnected_mesh();
			G.qual &= ~LR_CTRLKEY;
			break;
		case 5: /* select random */
			// selectrandom_mesh();
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
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Inverse",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	//uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Edge Loop|Shift R",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Random Vertices...",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Connected Vertices|Ctrl L",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		
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

static void do_view3d_select_curvemenu(void *arg, int event)
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
	uiBlockSetCol(block, MENUCOL);
	
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
//XXX  extern void borderselect(void);

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
	uiBlockSetCol(block, MENUCOL);
	
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
	uiBlockSetCol(block, MENUCOL);
	
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
	uiBlockSetCol(block, MENUCOL);
	
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
	uiBlockSetCol(block, MENUCOL);
	
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
	uiBlockSetCol(block, MENUCOL);
	
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
	uiBlockSetCol(block, MENUCOL);
	
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
	uiBlockSetCol(block, MENUCOL);
	
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
	uiBlockSetCol(block, MENUCOL);
	
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
			blenderqread(NKEY, 1);
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
	case 4: /* make links */
		linkmenu();
		break;
	case 5: /* make single user */
		single_user();
		break;
	case 6: /* copy properties */
		copymenu();
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
	case 12: /* snap */
		snapmenu();
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
	uiBlockSetCol(block, MENUCOL);
	
	//uiDefIconTextBlockBut(block, 0, NULL, ICON_RIGHTARROW_THIN, "Move", 0, yco-=20, 120, 19, "");
	//uiDefIconTextBlockBut(block, 0, NULL, ICON_RIGHTARROW_THIN, "Rotate", 0, yco-=20, 120, 19, "");
	//uiDefIconTextBlockBut(block, 0, NULL, ICON_RIGHTARROW_THIN, "Scale", 0, yco-=20, 120, 19, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Transform Properties...|N",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBlockBut(block, view3d_edit_object_transformmenu, NULL, ICON_RIGHTARROW_THIN, "Transform", 0, yco-=20, 120, 19, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Snap...|Shift S",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert Keyframe|I",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");	
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate Linked|Alt D",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete|X",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Links...|Ctrl L",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Single User...|U",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Copy Properties...|Ctrl C",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	
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
	uiBlockSetCol(block, MENUCOL);
	
	if (prop_mode==0) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Sharp|Shift O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Sharp|Shift O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	if (prop_mode==1) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Smooth|Shift O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Smooth|Shift O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_mesh_verticesmenu(void *arg, int event)
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
	uiBlockSetCol(block, MENUCOL);
	
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

static void do_view3d_edit_mesh_edgesmenu(void *arg, int event)
{
	extern short editbutflag;
	float fac;
	short randfac;

	switch(event) {
		 
	case 0: /* subdivide smooth */
		subdivideflag(1, 0.0, editbutflag | B_SMOOTH);
		break;
	case 1: /*subdivide fractal */
		randfac= 10;
		if(button(&randfac, 1, 100, "Rand fac:")==0) return;
		fac= -( (float)randfac )/100;
		subdivideflag(1, fac, editbutflag);
		break;
	case 2: /* subdivide */
		subdivideflag(1, 0.0, editbutflag);
		break;
	case 3: /* knife subdivide */
		// KnifeSubdivide();
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
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Knife Subdivide|K",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
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
		}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_mesh_facesmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_mesh_facesmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_mesh_facesmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
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

static void do_view3d_edit_mesh_normalsmenu(void *arg, int event)
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
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Recalculate Outside|Ctrl N",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Recalculate Inside|Ctrl Shift N",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Flip",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_meshmenu(void *arg, int event)
{
	switch(event) {
									
	case 0: /* Undo Editing */
		remake_editMesh();
		break;
	case 1: /* transform properties */
		blenderqread(NKEY, 1);
		break;
	case 2: /* Extrude */
		extrude_mesh();
		break;
	case 3: /* duplicate */
		duplicate_context_selected();
		break;
	case 4: /* Make Edge/Face */
		addedgevlak_mesh();
		break;
	case 5: /* delete */
		delete_context_selected();
		break;
	case 6: /* Shrink/Fatten Along Normals */
		transform('N');
		break;
	case 7: /* Shear */
		transform('S');
		break;
	case 8: /* Warp */
		transform('w');
		break;
	case 9: /* proportional edit (toggle) */
		if(G.f & G_PROPORTIONAL) G.f &= ~G_PROPORTIONAL;
		else G.f |= G_PROPORTIONAL;
		break;
	case 10: /* show hidden vertices */
		reveal_mesh();
		break;
	case 11: /* hide selected vertices */
		hide_mesh(0);
		break;
	case 12: /* hide deselected vertices */
		hide_mesh(1);
		break;
	case 13: /* insert keyframe */
		common_insertkey();
		break;
	case 14: /* snap */
		snapmenu();
		break;
	case 15: /* move to layer */
		movetolayer();
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
		uiBlockSetCol(block, MENUCOL);
		
	/*
	uiDefIconTextBlockBut(block, view3d_edit_mesh_facesmenu, NULL, ICON_RIGHTARROW_THIN, "Move", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, view3d_edit_mesh_facesmenu, NULL, ICON_RIGHTARROW_THIN, "Rotate", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, view3d_edit_mesh_facesmenu, NULL, ICON_RIGHTARROW_THIN, "Scale", 0, yco-=20, 120, 19, "");
	*/
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Editing|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Transform Properties...|N",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Snap...|Shift S",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 14, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert Keyframe|I",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 13, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Extrude|E",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Edge/Face|F",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete...|X",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBlockBut(block, view3d_edit_mesh_verticesmenu, NULL, ICON_RIGHTARROW_THIN, "Vertices", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, view3d_edit_mesh_edgesmenu, NULL, ICON_RIGHTARROW_THIN, "Edges", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, view3d_edit_mesh_facesmenu, NULL, ICON_RIGHTARROW_THIN, "Faces", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, view3d_edit_mesh_normalsmenu, NULL, ICON_RIGHTARROW_THIN, "Normals", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shrink/Fatten Along Normals|Alt S",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shear|Ctrl S",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Warp|Ctrl W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	if(G.f & G_PROPORTIONAL) {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Proportional Editing|O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
	} else {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Proportional Editing|O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
	}
	uiDefIconTextBlockBut(block, view3d_edit_propfalloffmenu, NULL, ICON_RIGHTARROW_THIN, "Proportional Falloff", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Hidden Vertices",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Selected Vertices|H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Deselected Vertices|Shift H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 15, "");
		
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
	uiBlockSetCol(block, MENUCOL);
	
	if (OBACT->type == OB_CURVE) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Tilt|T",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Tilt|Alt T",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Toggle Free/Aligned|H",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Vector|V",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Smooth|Shift H",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	}
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Vertex Parent|Ctrl P",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_curve_segmentsmenu(void *arg, int event)
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
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Subdivide",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Switch Direction",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");

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
		blenderqread(NKEY, 1);
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
	case 10: /* show hidden control points */
		revealNurb();
		break;
	case 11: /* hide selected control points */
		hideNurb(0);
		break;
	case 12: /* hide deselected control points */
		hideNurb(1);
		break;
	case 13: /* Shear */
		transform('S');
		break;
	case 14: /* Warp */
		transform('w');
		break;
	case 15: /* snap */
		snapmenu();
		break;
	case 16: /* move to layer  */
		movetolayer();
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
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Editing|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Transform Properties...|N",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Snap...|Shift S",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 15, "");
	
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
	uiDefIconTextBlockBut(block, view3d_edit_propfalloffmenu, NULL, ICON_RIGHTARROW_THIN, "Proportional Falloff", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Hidden Control Points|Alt H",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Selected Control Points|H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");
	if (OBACT->type == OB_SURF) uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Deselected Control Points|Shift H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 16, "");
	
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
	case 5: /* move to layer */
		movetolayer();
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
	uiBlockSetCol(block, MENUCOL);
	

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete...|X",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shear|Ctrl S",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Warp|Ctrl W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	
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
	uiBlockSetCol(block, MENUCOL);
	
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
	case 2: /* move to layer */
		movetolayer();
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
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Editing|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Paste From Buffer File|Alt V",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBlockBut(block, view3d_edit_text_charsmenu, NULL, ICON_RIGHTARROW_THIN, "Special Characters", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

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
	case 1: /* snap */
		snapmenu();
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
	case 6: /* move to layer */
		movetolayer();
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
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Editing|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Snap...|Shift S",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
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
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	

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
		blenderqread(NKEY, 1);
		break;
	case 2: /* snap */
		snapmenu();
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
	case 8: /* Move to Layer */
		movetolayer();
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
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Editing|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Transform Properties|N", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Snap...|Shift S",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Extrude|E",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete|X",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shear|Ctrl S",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Warp|Ctrl W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
	

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
	uiBlockSetCol(block, MENUCOL);
	
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
		blenderqread(NKEY, 1);
		break;
	case 1: /* insert keyframe */
		common_insertkey();
		break;
	case 2: /* Move to Layer */
		movetolayer();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_pose_armaturemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_pose_armaturemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_pose_armaturemenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Transform Properties|N", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBlockBut(block, view3d_pose_armature_transformmenu, NULL, ICON_RIGHTARROW_THIN, "Transform", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert Keyframe|I",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
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
	uiBlockSetCol(block, MENUCOL);
	
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
	uiBlockSetCol(block, MENUCOL);
	
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
	// case 3: /* uv calculation */
	//	uv_autocalc_tface();
	//	break;
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
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBlockBut(block, view3d_facesel_propertiesmenu, NULL, ICON_RIGHTARROW_THIN, "Active Draw Mode", 0, yco-=20, 120, 19, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Copy Draw Mode",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");

	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Copy UVs & Textures",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Copy Vertex Colors",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Vertex Colors|Shift K",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	/* for some reason calling this from the header messes up the 'from window'
		* UV calculation :(
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Calculate UVs",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	*/
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Rotate UVs|R",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Hidden Faces|Alt H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Selected Faces|H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Deselected Faces|Shift H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
		

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
	char formatstring[1024];

	strcpy(formatstring, "Mode: %%t");
	
	strcat(formatstring, "|%s %%x%d");	// add space in the menu for Object
	
	/* if active object is an armature */
	if (OBACT && OBACT->type==OB_ARMATURE) {
		strcat(formatstring, "|%s %%x%d");	// add space in the menu for pose
	}
	
	/* if active object is a mesh */
	if (OBACT && OBACT->type == OB_MESH) {
		strcat(formatstring, "|%s %%x%d|%s %%x%d|%s %%x%d");	// add space in the menu for faceselect, vertex paint, texture paint
		
		/* if active mesh has an armature */
		if ((((Mesh*)(OBACT->data))->dvert)) {
			strcat(formatstring, "|%s %%x%d");	// add space in the menu for weight paint
		}
	}
	
	/* if active object is editable */
	if (OBACT && ((OBACT->type == OB_MESH) || (OBACT->type == OB_ARMATURE)
	|| (OBACT->type == OB_CURVE) || (OBACT->type == OB_SURF) || (OBACT->type == OB_FONT)
	|| (OBACT->type == OB_MBALL) || (OBACT->type == OB_LATTICE))) {
		strcat(formatstring, "|%s %%x%d");	// add space in the menu for Edit
	}
	
	/*
	 * fill in the spaces in the menu with appropriate mode choices depending on active object
	 */

	/* if active object is an armature */
	if (OBACT && OBACT->type==OB_ARMATURE) {
		sprintf(string, formatstring,
		"Object",						V3D_OBJECTMODE_SEL,
		"Edit",						V3D_EDITMODE_SEL,
		"Pose",						V3D_POSEMODE_SEL
		);
	}
	/* if active object is a mesh with armature */
	else if ((OBACT && OBACT->type == OB_MESH) && ((((Mesh*)(OBACT->data))->dvert))) {
		sprintf(string, formatstring,
		"Object",						V3D_OBJECTMODE_SEL,
		"Edit",						V3D_EDITMODE_SEL,
		"Face Select",			V3D_FACESELECTMODE_SEL,
		"Vertex Paint",			V3D_VERTEXPAINTMODE_SEL,
		"Texture Paint",		V3D_TEXTUREPAINTMODE_SEL,
		"Weight Paint",			V3D_WEIGHTPAINTMODE_SEL 
		);
	}
	/* if active object is a mesh */
	else if (OBACT && OBACT->type == OB_MESH) {
		sprintf(string, formatstring,
		"Object",						V3D_OBJECTMODE_SEL,
		"Edit",						V3D_EDITMODE_SEL,
		"Face Select",			V3D_FACESELECTMODE_SEL,
		"Vertex Paint",			V3D_VERTEXPAINTMODE_SEL,
		"Texture Paint",		V3D_TEXTUREPAINTMODE_SEL
		);
	} 
	/* if active object is editable */
	else if (OBACT && ((OBACT->type == OB_MESH) || (OBACT->type == OB_ARMATURE)
	|| (OBACT->type == OB_CURVE) || (OBACT->type == OB_SURF) || (OBACT->type == OB_FONT)
	|| (OBACT->type == OB_MBALL) || (OBACT->type == OB_LATTICE))) {
		sprintf(string, formatstring,
		"Object",						V3D_OBJECTMODE_SEL,
		"Edit",						V3D_EDITMODE_SEL
		);
	}
	/* if active object is not editable */
	else {
		sprintf(string, formatstring,
		"Object",						V3D_OBJECTMODE_SEL
		);
	}
	
	return (string);
}


char *drawtype_pup(void)
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

void view3d_buttons(void)
{
	uiBlock *block;
	int a;
	short xco = 0;
	char naam[20];
	short xmax;
	
	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, MIDGREY);	

	curarea->butspacetype= SPACE_VIEW3D;
	
	xco = 8;
	
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), xco,0,XIC+10,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	xco+= XIC+22;

	/* pull down menus */
	uiBlockSetEmboss(block, UI_EMBOSSP);
	if(area_is_active_area(curarea)) uiBlockSetCol(block, HEADERCOLSEL);	
	else uiBlockSetCol(block, HEADERCOL); 
	
	/* compensate for local mode when setting up the viewing menu/iconrow values */
	if(G.vd->view==7) G.vd->viewbut= 1;
	else if(G.vd->view==1) G.vd->viewbut= 2;
	else if(G.vd->view==3) G.vd->viewbut= 3;
	else G.vd->viewbut= 0;
	
	xmax= GetButStringLength("View");
	uiDefBlockBut(block, view3d_viewmenu, NULL, "View", xco, -2, xmax, 24, "");
	xco+= xmax;
	
	xmax= GetButStringLength("Select");
	if (G.obedit) {
		if (OBACT && OBACT->type == OB_MESH) {
			uiDefBlockBut(block, view3d_select_meshmenu, NULL, "Select",	xco, 0, xmax, 24, "");
		} else if (OBACT && (OBACT->type == OB_CURVE || OBACT->type == OB_SURF)) {
			uiDefBlockBut(block, view3d_select_curvemenu, NULL, "Select", xco, 0, xmax, 24, "");
		} else if (OBACT && OBACT->type == OB_FONT) {
			uiDefBlockBut(block, view3d_select_meshmenu, NULL, "Select",	xco, 0, xmax, 24, "");
		} else if (OBACT && OBACT->type == OB_MBALL) {
			uiDefBlockBut(block, view3d_select_metaballmenu, NULL, "Select",	xco, 0, xmax, 24, "");
		} else if (OBACT && OBACT->type == OB_LATTICE) {
			uiDefBlockBut(block, view3d_select_latticemenu, NULL, "Select", xco, 0, xmax, 24, "");
		} else if (OBACT && OBACT->type == OB_ARMATURE) {
			uiDefBlockBut(block, view3d_select_armaturemenu, NULL, "Select",	xco, 0, xmax, 24, "");
		}
	} else if (G.f & G_FACESELECT) {
		if (OBACT && OBACT->type == OB_MESH) {
			uiDefBlockBut(block, view3d_select_faceselmenu, NULL, "Select", xco, -2, xmax, 24, "");
		}
	} else if (G.obpose) {
		if (OBACT && OBACT->type == OB_ARMATURE) {
			uiDefBlockBut(block, view3d_select_pose_armaturemenu, NULL, "Select", xco, 0, xmax, 20, "");
		}
	} else if ((G.f & G_VERTEXPAINT) || (G.f & G_TEXTUREPAINT) || (G.f & G_WEIGHTPAINT)) {
		uiDefBut(block, LABEL,0,"", xco, 0, xmax, 20, 0, 0, 0, 0, 0, "");
	} else {
		uiDefBlockBut(block, view3d_select_objectmenu, NULL, "Select",	xco, 0, xmax, 20, "");
	}
	xco+= xmax;
	
	if ((G.f & G_VERTEXPAINT) || (G.f & G_TEXTUREPAINT) || (G.f & G_WEIGHTPAINT)) {
			xmax= GetButStringLength("Paint");
			uiDefBlockBut(block, view3d_paintmenu, NULL, "Paint", xco, 0, xmax, 20, "");
			xco+= xmax;
	} else if (G.obedit) {
		if (OBACT && OBACT->type == OB_MESH) {
			xmax= GetButStringLength("Mesh");
			uiDefBlockBut(block, view3d_edit_meshmenu, NULL, "Mesh",	xco, 0, xmax, 20, "");
			xco+= xmax;
		} else if (OBACT && OBACT->type == OB_CURVE) {
			xmax= GetButStringLength("Curve");
			uiDefBlockBut(block, view3d_edit_curvemenu, NULL, "Curve",	xco, 0, xmax, 20, "");
			xco+= xmax;
		} else if (OBACT && OBACT->type == OB_SURF) {
			xmax= GetButStringLength("Surface");
			uiDefBlockBut(block, view3d_edit_curvemenu, NULL, "Surface",	xco, 0, xmax, 20, "");
			xco+= xmax;
		} else if (OBACT && OBACT->type == OB_FONT) {
			xmax= GetButStringLength("Text");
			uiDefBlockBut(block, view3d_edit_textmenu, NULL, "Text",	xco, 0, xmax, 20, "");
			xco+= xmax;
		} else if (OBACT && OBACT->type == OB_MBALL) {
			xmax= GetButStringLength("Metaball");
			uiDefBlockBut(block, view3d_edit_metaballmenu, NULL, "Metaball",	xco, 0, xmax, 20, "");
			xco+= xmax;
		} else if (OBACT && OBACT->type == OB_LATTICE) {
			xmax= GetButStringLength("Lattice");
			uiDefBlockBut(block, view3d_edit_latticemenu, NULL, "Lattice",	xco, 0, xmax, 20, "");
			xco+= xmax;
		} else if (OBACT && OBACT->type == OB_ARMATURE) {
			xmax= GetButStringLength("Armature");
			uiDefBlockBut(block, view3d_edit_armaturemenu, NULL, "Armature",	xco, 0, xmax, 20, "");
			xco+= xmax;
		}
	} else if (G.f & G_FACESELECT) {
		if (OBACT && OBACT->type == OB_MESH) {
			xmax= GetButStringLength("Face");
			uiDefBlockBut(block, view3d_faceselmenu, NULL, "Face",	xco, 0, xmax, 20, "");
			xco+= xmax;
		}
	} else if (G.obpose) {
		if (OBACT && OBACT->type == OB_ARMATURE) {
			xmax= GetButStringLength("Armature");
			uiDefBlockBut(block, view3d_pose_armaturemenu, NULL, "Armature",	xco, 0, xmax, 20, "");
			xco+= xmax;
		}
	} else {
		xmax= GetButStringLength("Object");
		uiDefBlockBut(block, view3d_edit_objectmenu, NULL, "Object",	xco, 0, xmax, 20, "");
		xco+= xmax;
	}

	/* end pulldowns, other buttons: */
	uiBlockSetCol(block, MIDGREY);
	uiBlockSetEmboss(block, UI_EMBOSSX);
	
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
	
	xco+= 10;

	uiDefIconTextButS(block, MENU, B_MODESELECT, (G.vd->modeselect),view3d_modeselect_pup() , 
																xco,0,120,20, &(G.vd->modeselect), 0, 0, 0, 0, "Mode:");
	
	xco+= 128;
	
	/* DRAWTYPE */
	uiDefIconTextButC(block, ICONTEXTROW,B_REDR, ICON_BBOX, drawtype_pup(), xco,0,XIC+10,YIC, &(G.vd->drawtype), 1.0, 5.0, 0, 0, "Viewport Shading: boundbox/wire/solid/shaded (ZKEY, SHIFT+Z)");

	// uiDefIconButS(block, ICONROW, B_REDR, ICON_BBOX,	xco,0,XIC+10,YIC, &(G.vd->drawtype), 1.0, 5.0, 0, 0, "Drawtype: boundbox/wire/solid/shaded (ZKEY, SHIFT+Z)");

	// uiDefIconTextButS(block, MENU, REDRAWVIEW3D, (ICON_BBOX+G.vd->drawtype-1), "Viewport Shading%t|Bounding Box %x1|Wireframe %x2|Solid %x3|Shaded %x4|Textured %x5",	
	//														xco,0,124,20, &(G.vd->drawtype), 0, 0, 0, 0, "Viewport Shading");
	//	uiDefButS(block, MENU, REDRAWVIEW3D, "Viewport Shading%t|Bounding Box %x1|Wireframe %x2|Solid %x3|Shaded %x4|Textured %x5", 
	//																xco,0,110,20, &(G.vd->drawtype), 0, 0, 0, 0, "Viewport Shading");
	
	xco+= XIC+18;
	/* LAYERS */
	if(G.vd->localview==0) {
		
		for(a=0; a<10; a++) {
			uiDefButI(block, TOG|BIT|(a+10), B_LAY+10+a, "",(short)(xco+a*(XIC/2)), 0,			XIC/2, (YIC)/2, &(G.vd->lay), 0, 0, 0, 0, "Toggles Layer visibility");
			uiDefButI(block, TOG|BIT|a, B_LAY+a, "",	(short)(xco+a*(XIC/2)), (short)(YIC/2),(short)(XIC/2),(short)(YIC/2), &(G.vd->lay), 0, 0, 0, 0, "Toggles Layer visibility");
			if(a==4) xco+= 5;
		}
		xco+= (a-2)*(XIC/2)+5;

		/* LOCK */
		uiDefIconButS(block, ICONTOG, B_SCENELOCK, ICON_UNLOCKED, xco+=XIC,0,XIC,YIC, &(G.vd->scenelock), 0, 0, 0, 0, "Locks layers and used Camera to Scene");
		xco+= 14;

	}
	else xco+= (10+1)*(XIC/2)+10+4;

	/* VIEWMOVE */
	/*
	uiDefIconButI(block, TOG, B_VIEWTRANS, ICON_VIEWMOVE, xco+=XIC,0,XIC,YIC, &viewmovetemp, 0, 0, 0, 0, "Translates view (SHIFT+MiddleMouse)");
	uiDefIconButI(block, TOG, B_VIEWZOOM, ICON_VIEWZOOM,	xco+=XIC,0,XIC,YIC, &viewmovetemp, 0, 0, 0, 0, "Zooms view (CTRL+MiddleMouse)");
	*/
	
	/* around */
	xco+= XIC/2;
	uiDefIconButS(block, ROW, 1, ICON_ROTATE, xco+=XIC,0,XIC,YIC, &G.vd->around, 3.0, 0.0, 0, 0, "Enables Rotation or Scaling around boundbox center (COMMAKEY)");
	uiDefIconButS(block, ROW, 1, ICON_ROTATECENTER, xco+=XIC,0,XIC,YIC, &G.vd->around, 3.0, 3.0, 0, 0, "Enables Rotation or Scaling around median point");
	uiDefIconButS(block, ROW, 1, ICON_CURSOR, xco+=XIC,0,XIC,YIC, &G.vd->around, 3.0, 1.0, 0, 0, "Enables Rotation or Scaling around cursor (DOTKEY)");
	uiDefIconButS(block, ROW, 1, ICON_ROTATECOLLECTION, xco+=XIC,0,XIC,YIC, &G.vd->around, 3.0, 2.0, 0, 0, "Enables Rotation or Scaling around individual object centers");

	if(G.vd->bgpic) {
		xco+= XIC/2;
		uiDefIconButS(block, TOG|BIT|1, B_REDR, ICON_IMAGE_COL, xco+=XIC,0,XIC,YIC, &G.vd->flag, 0, 0, 0, 0, "Displays a Background picture");
	}
	if(G.obedit && (OBACT->type == OB_MESH || OBACT->type == OB_CURVE || OBACT->type == OB_SURF || OBACT->type == OB_LATTICE)) {
		extern int prop_mode;
		xco+= XIC/2;
		uiDefIconButI(block, ICONTOG|BIT|14, B_PROPTOOL, ICON_GRID, xco+=XIC,0,XIC,YIC, &G.f, 0, 0, 0, 0, "Toggles Proportional Vertex Editing (OKEY)");
		if(G.f & G_PROPORTIONAL) {
			uiDefIconButI(block, ROW, 0, ICON_SHARPCURVE, xco+=XIC,0,XIC,YIC, &prop_mode, 4.0, 0.0, 0, 0, "Enables Sharp falloff (SHIFT+OKEY)");
			uiDefIconButI(block, ROW, 0, ICON_SMOOTHCURVE,	xco+=XIC,0,XIC,YIC, &prop_mode, 4.0, 1.0, 0, 0, "Enables Smooth falloff (SHIFT+OKEY)");
		}
	}
	
	xco+=XIC;

	/* Always do this last */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);

}
