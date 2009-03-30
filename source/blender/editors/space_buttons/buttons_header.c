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

#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_screen.h"
#include "ED_types.h"
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
	SpaceButs *sbuts= (SpaceButs*)CTX_wm_space_data(C);
	ScrArea *sa= CTX_wm_area(C);

	switch(event) {
		case 0: /* panel alignment */
		case 1:
		case 2:
			sbuts->align= event;
			if(event) {
				sbuts->re_align= 1;
				// uiAlignPanelStep(sa, 1.0);
			}
            break;
    }

	ED_area_tag_redraw(sa);
}

static uiBlock *dummy_viewmenu(bContext *C, ARegion *ar, void *arg_unused)
{
	SpaceButs *sbuts= (SpaceButs*)CTX_wm_space_data(C);
	ScrArea *sa= CTX_wm_area(C);
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, ar, "dummy_viewmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_viewmenu, NULL);
	
	if (sbuts->align == 1) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Horizontal", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Horizontal", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");

	if (sbuts->align == 2) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Vertical", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Vertical", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

	if (sbuts->align == 0) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Free", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Free", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");

	if(sa->headertype==HEADERTOP) {
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

#define B_CONTEXT_SWITCH	101
#define B_BUTSPREVIEW		102
#define B_NEWFRAME			103

static void do_buttons_buttons(bContext *C, void *arg, int event)
{
	switch(event) {
		case B_NEWFRAME:
			WM_event_add_notifier(C, NC_SCENE|ND_FRAME, NULL);
			break;
		case B_CONTEXT_SWITCH:
			ED_area_tag_redraw(CTX_wm_area(C));
			break;
	}
}


void buttons_header_buttons(const bContext *C, ARegion *ar)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceButs *sbuts= (SpaceButs*)CTX_wm_space_data(C);
	uiBlock *block;
	int xco, yco= 3;
	
	block= uiBeginBlock(C, ar, "header buttons", UI_EMBOSS, UI_HELV);
	uiBlockSetHandleFunc(block, do_buttons_buttons, NULL);
	
	xco= ED_area_header_standardbuttons(C, block, yco);
	
	if((sa->flag & HEADER_NO_PULLDOWN)==0) {
		int xmax;
		
		/* pull down menus */
		uiBlockSetEmboss(block, UI_EMBOSSP);
		
		xmax= GetButStringLength("View");
		uiDefPulldownBut(block, dummy_viewmenu, CTX_wm_area(C), 
						 "View", xco, yco-2, xmax-3, 24, "");
		
		xco+=XIC+xmax;
	}
	
	uiBlockSetEmboss(block, UI_EMBOSS);

	uiBlockBeginAlign(block);
	uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_GAME,			xco, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)CONTEXT_LOGIC, 0, 0, "Logic (F4) ");
	uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_SCRIPT,		xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)CONTEXT_SCRIPT, 0, 0, "Script ");
	uiDefIconButS(block, ROW, B_BUTSPREVIEW,	ICON_MATERIAL_DATA,xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)CONTEXT_SHADING, 0, 0, "Shading (F5) ");
	uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_OBJECT_DATA,		xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)CONTEXT_OBJECT, 0, 0, "Object (F7) ");
	uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_EDIT,			xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)CONTEXT_EDITING, 0, 0, "Editing (F9) ");
	uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_SCENE_DATA,	xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)CONTEXT_SCENE, 0, 0, "Scene (F10) ");
	
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
			uiDefIconButC(block, ROW, B_CONTEXT_SWITCH,		ICON_OBJECT_DATA,	xco+=XIC, yco, XIC, YIC, &(sbuts->tab[CONTEXT_OBJECT]), 1.0, (float)TAB_OBJECT_OBJECT, 0, 0, "Object buttons ");
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
	uiDefButI(block, NUM, B_NEWFRAME, "",	(xco+20),yco,60,YIC, &(CTX_data_scene(C)->r.cfra), 1.0, MAXFRAMEF, 0, 0, "Displays Current Frame of animation. Click to change.");
	xco+= 80;
	
// XXX	buttons_active_id(&id, &idfrom);
//	sbuts->lockpoin= id;
	
	
	
	/* always as last  */
	UI_view2d_totRect_set(&ar->v2d, xco+XIC+80, ar->v2d.tot.ymax-ar->v2d.tot.ymin);
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
}


