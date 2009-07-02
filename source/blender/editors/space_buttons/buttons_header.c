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

#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

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
		case 1:
		case 2:
			sbuts->align= event;
			sbuts->re_align= 1;
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
	
	block= uiBeginBlock(C, ar, "dummy_viewmenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_viewmenu, NULL);
	
	if (sbuts->align == 1) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Horizontal", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Horizontal", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");

	if (sbuts->align == 2) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Vertical", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Vertical", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

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
	SpaceButs *sbuts= (SpaceButs*)CTX_wm_space_data(C);

	switch(event) {
		case B_NEWFRAME:
			WM_event_add_notifier(C, NC_SCENE|ND_FRAME, NULL);
			break;
		case B_CONTEXT_SWITCH:
		case B_BUTSPREVIEW:
			ED_area_tag_redraw(CTX_wm_area(C));

			/* silly exception */
			if(sbuts->mainb == BCONTEXT_WORLD)
				sbuts->flag |= SB_WORLD_TEX;
			else if(sbuts->mainb != BCONTEXT_TEXTURE)
				sbuts->flag &= ~SB_WORLD_TEX;

			sbuts->preview= 1;
			break;
	}
}

void buttons_header_buttons(const bContext *C, ARegion *ar)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceButs *sbuts= (SpaceButs*)CTX_wm_space_data(C);
	uiBlock *block;
	int xco, yco= 3;

	buttons_context_compute(C, sbuts);
	
	block= uiBeginBlock(C, ar, "header buttons", UI_EMBOSS);
	uiBlockSetHandleFunc(block, do_buttons_buttons, NULL);
	
	xco= ED_area_header_standardbuttons(C, block, yco);
	
	if((sa->flag & HEADER_NO_PULLDOWN)==0) {
		int xmax;
		
		xmax= GetButStringLength("View");
		uiDefPulldownBut(block, dummy_viewmenu, CTX_wm_area(C), 
						 "View", xco, yco, xmax-3, 20, "");
		
		xco+=xmax;
	}
	
	uiBlockSetEmboss(block, UI_EMBOSS);

	xco -= XIC;
	
	// Default panels
	uiBlockBeginAlign(block);
	if(sbuts->pathflag & (1<<BCONTEXT_SCENE))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_SCENE,			xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)BCONTEXT_SCENE, 0, 0, "Scene");
	if(sbuts->pathflag & (1<<BCONTEXT_WORLD))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_WORLD,		xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)BCONTEXT_WORLD, 0, 0, "World");
	if(sbuts->pathflag & (1<<BCONTEXT_OBJECT))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_OBJECT_DATA,	xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)BCONTEXT_OBJECT, 0, 0, "Object");
	if(sbuts->pathflag & (1<<BCONTEXT_CONSTRAINT))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_CONSTRAINT,	xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)BCONTEXT_CONSTRAINT, 0, 0, "Constraint");
	if(sbuts->pathflag & (1<<BCONTEXT_DATA))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	sbuts->dataicon,	xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)BCONTEXT_DATA, 0, 0, "Object Data");
	if(sbuts->pathflag & (1<<BCONTEXT_MODIFIER))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_MODIFIER,	xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)BCONTEXT_MODIFIER, 0, 0, "Modifier");
	if(sbuts->pathflag & (1<<BCONTEXT_BONE))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_BONE_DATA,	xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)BCONTEXT_BONE, 0, 0, "Bone");
	if(sbuts->pathflag & (1<<BCONTEXT_MATERIAL))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_MATERIAL,	xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)BCONTEXT_MATERIAL, 0, 0, "Material");
	if(sbuts->pathflag & (1<<BCONTEXT_TEXTURE))
		uiDefIconButS(block, ROW, B_BUTSPREVIEW,	ICON_TEXTURE,	xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)BCONTEXT_TEXTURE, 0, 0, "Texture");
	if(sbuts->pathflag & (1<<BCONTEXT_PARTICLE))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_PARTICLES,	xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)BCONTEXT_PARTICLE, 0, 0, "Particles");
	if(sbuts->pathflag & (1<<BCONTEXT_PHYSICS))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_PHYSICS,	xco+=XIC, yco, XIC, YIC, &(sbuts->mainb), 0.0, (float)BCONTEXT_PHYSICS, 0, 0, "Physics");

	xco+= XIC;
	
	uiBlockEndAlign(block);
	
	xco+=XIC;
	uiDefButI(block, NUM, B_NEWFRAME, "",	(xco+20),yco,60,YIC, &(CTX_data_scene(C)->r.cfra), 1.0, MAXFRAMEF, 0, 0, "Displays Current Frame of animation. Click to change.");
	xco+= 80;
	
	/* always as last  */
	UI_view2d_totRect_set(&ar->v2d, xco+XIC+80, ar->v2d.tot.ymax-ar->v2d.tot.ymin);
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
}


