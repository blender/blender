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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>

#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_global.h"
#include "BKE_screen.h"

#include "ED_screen.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "buttons_intern.h"


/* ************************ header area region *********************** */

static void do_viewmenu(bContext *C, void *arg, int event)
{
	
}

static uiBlock *dummy_viewmenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	ScrArea *curarea= C->area;
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, handle->region, "dummy_viewmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_viewmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Nothing yet", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}
	
	uiTextBoundsBlock(block, 50);
	uiEndBlock(C, block);
	
	return block;
}

#define B_NEWSPACE			100
#define B_CONTEXT_SWITCH	101
#define B_BUTSPREVIEW		102
#define B_NEWFRAME			103

static void do_buttons_buttons(bContext *C, void *arg, int event)
{
	switch(event) {
		case B_NEWSPACE:
			ED_newspace(C->area, C->area->butspacetype);
			WM_event_add_notifier(C, WM_NOTE_SCREEN_CHANGED, 0, NULL);
			break;
		case B_NEWFRAME:
			WM_event_add_notifier(C, WM_NOTE_WINDOW_REDRAW, 0, NULL);
			break;
		}
}


void buttons_header_buttons(const bContext *C, ARegion *ar)
{
	ScrArea *sa= C->area;
	SpaceButs *sbuts= sa->spacedata.first;
	uiBlock *block;
	int xco, yco= 3;
	
	block= uiBeginBlock(C, ar, "header buttons", UI_EMBOSS, UI_HELV);
	uiBlockSetHandleFunc(block, do_buttons_buttons, NULL);
	
	if(ED_screen_area_active(C)) uiBlockSetCol(block, TH_HEADER);
	else uiBlockSetCol(block, TH_HEADERDESEL);

	xco = 8;
	
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, 
					  windowtype_pup(), xco, yco, XIC+10, YIC, 
					  &(C->area->butspacetype), 1.0, SPACEICONMAX, 0, 0, 
					  "Displays Current Window Type. "
					  "Click for menu of available types.");
	
	xco += XIC + 14;
	
	uiBlockSetEmboss(block, UI_EMBOSSN);
	if (sa->flag & HEADER_NO_PULLDOWN) {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, 0, 
						 ICON_DISCLOSURE_TRI_RIGHT,
						 xco,yco,XIC,YIC-2,
						 &(sa->flag), 0, 0, 0, 0, 
						 "Show pulldown menus");
	}
	else {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, 0, 
						 ICON_DISCLOSURE_TRI_DOWN,
						 xco,yco,XIC,YIC-2,
						 &(sa->flag), 0, 0, 0, 0, 
						 "Hide pulldown menus");
	}
	uiBlockSetEmboss(block, UI_EMBOSS);
	xco+=XIC;
	
	if((sa->flag & HEADER_NO_PULLDOWN)==0) {
		int xmax;
		
		/* pull down menus */
		uiBlockSetEmboss(block, UI_EMBOSSP);
		
		xmax= GetButStringLength("View");
		uiDefPulldownBut(block, dummy_viewmenu, C->area, 
						 "View", xco, yco-2, xmax-3, 24, "");
		
		xco+=XIC+xmax;
	}
	
	uiBlockSetEmboss(block, UI_EMBOSS);

	uiBlockBeginAlign(block);
	uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_GAME,			xco, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)CONTEXT_LOGIC, 0, 0, "Logic (F4) ");
	uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_SCRIPT,		xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)CONTEXT_SCRIPT, 0, 0, "Script ");
	uiDefIconButS(block, ROW, B_BUTSPREVIEW,	ICON_MATERIAL_DEHLT,xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)CONTEXT_SHADING, 0, 0, "Shading (F5) ");
	uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_OBJECT,		xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)CONTEXT_OBJECT, 0, 0, "Object (F7) ");
	uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_EDIT,			xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)CONTEXT_EDITING, 0, 0, "Editing (F9) ");
	uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_SCENE_DEHLT,	xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)CONTEXT_SCENE, 0, 0, "Scene (F10) ");
	
	xco+= XIC;
	
	/* select the context to be drawn, per contex/tab the actual context is tested */
	uiBlockSetEmboss(block, UI_EMBOSS);	// normal
	switch(sbuts->mainb) {
		case CONTEXT_SCENE:
			uiBlockBeginAlign(block);
			uiDefIconButC(block, ROW, B_CONTEXT_SWITCH,		ICON_SCENE,	xco+=XIC, yco, XIC, YIC, &(sbuts->tab[CONTEXT_SCENE]), 1.0, (float)TAB_SCENE_RENDER, 0, 0, "Render buttons ");
			uiDefIconButC(block, ROW, B_CONTEXT_SWITCH,		ICON_SEQUENCE,	xco+=XIC, yco, XIC, YIC, &(sbuts->tab[CONTEXT_SCENE]), 1.0, (float)TAB_SCENE_SEQUENCER, 0, 0, "Sequencer buttons ");
			uiDefIconButC(block, ROW, B_CONTEXT_SWITCH,		ICON_ANIM,	xco+=XIC, yco, XIC, YIC, &(sbuts->tab[CONTEXT_SCENE]), 1.0, (float)TAB_SCENE_ANIM, 0, 0, "Anim/playback buttons");
			uiDefIconButC(block, ROW, B_CONTEXT_SWITCH,		ICON_SOUND,	xco+=XIC, yco, XIC, YIC, &(sbuts->tab[CONTEXT_SCENE]), 1.0, (float)TAB_SCENE_SOUND, 0, 0, "Sound block buttons");
			
			break;
		case CONTEXT_OBJECT:
			uiBlockBeginAlign(block);
			uiDefIconButC(block, ROW, B_CONTEXT_SWITCH,		ICON_OBJECT,	xco+=XIC, yco, XIC, YIC, &(sbuts->tab[CONTEXT_OBJECT]), 1.0, (float)TAB_OBJECT_OBJECT, 0, 0, "Object buttons ");
			uiDefIconButC(block, ROW, B_CONTEXT_SWITCH,		ICON_PHYSICS,	xco+=XIC, yco, XIC, YIC, &(sbuts->tab[CONTEXT_OBJECT]), 1.0, (float)TAB_OBJECT_PHYSICS, 0, 0, "Physics buttons");
			uiDefIconButC(block, ROW, B_CONTEXT_SWITCH,		ICON_PARTICLES,	xco+=XIC, yco, XIC, YIC, &(sbuts->tab[CONTEXT_OBJECT]), 1.0, (float)TAB_OBJECT_PARTICLE, 0, 0, "Particle buttons");
			
			break;
		case CONTEXT_SHADING:
			uiBlockBeginAlign(block);
			uiDefIconButC(block, ROW, B_BUTSPREVIEW,	ICON_LAMP,	xco+=XIC, yco, XIC, YIC, &(sbuts->tab[CONTEXT_SHADING]), 1.0, (float)TAB_SHADING_LAMP, 0, 0, "Lamp buttons");
			uiDefIconButC(block, ROW, B_BUTSPREVIEW,	ICON_MATERIAL,	xco+=XIC, yco, XIC, YIC, &(sbuts->tab[CONTEXT_SHADING]), 1.0, (float)TAB_SHADING_MAT, 0, 0, "Material buttons");
			uiDefIconButC(block, ROW, B_BUTSPREVIEW,	ICON_TEXTURE,	xco+=XIC, yco, XIC, YIC, &(sbuts->tab[CONTEXT_SHADING]), 1.0, (float)TAB_SHADING_TEX, 0, 0, "Texture buttons(F6)");
			uiDefIconButC(block, ROW, B_CONTEXT_SWITCH,			ICON_RADIO,xco+=XIC, yco, XIC, YIC, &(sbuts->tab[CONTEXT_SHADING]), 1.0, (float)TAB_SHADING_RAD, 0, 0, "Radiosity buttons");
			uiDefIconButC(block, ROW, B_BUTSPREVIEW,	ICON_WORLD,	xco+=XIC, yco, XIC, YIC, &(sbuts->tab[CONTEXT_SHADING]), 1.0, (float)TAB_SHADING_WORLD, 0, 0, "World buttons");
			
			break;
		case CONTEXT_EDITING:
			
			break;
		case CONTEXT_SCRIPT:
			
			break;
		case CONTEXT_LOGIC:
			
			break;
	}
	
	uiBlockEndAlign(block);
	
	xco+=XIC;
	uiDefButI(block, NUM, B_NEWFRAME, "",	(xco+20),yco,60,YIC, &(C->scene->r.cfra), 1.0, MAXFRAMEF, 0, 0, "Displays Current Frame of animation. Click to change.");
	xco+= 80;
	
// XXX	buttons_active_id(&id, &idfrom);
//	sbuts->lockpoin= id;
	
	
	
	/* always as last  */
	sa->headbutlen= xco+XIC+80; // +80 because the last button is not an icon
	
	uiEndBlock(C, block);
	uiDrawBlock(block);
}


