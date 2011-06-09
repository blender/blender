/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_buttons/buttons_header.c
 *  \ingroup spbuttons
 */


#include <string.h>
#include <stdio.h>

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"


#include "BKE_context.h"

#include "ED_screen.h"
#include "ED_types.h"

#include "DNA_object_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "buttons_intern.h"


#define B_CONTEXT_SWITCH	101
#define B_BUTSPREVIEW		102

static void set_texture_context(bContext *C, SpaceButs *sbuts)
{
	switch(sbuts->mainb) {
		case BCONTEXT_MATERIAL:
			sbuts->texture_context = SB_TEXC_MAT_OR_LAMP;
			break;
		case BCONTEXT_DATA:
		{
			Object *ob = CTX_data_active_object(C);
			if(ob && ob->type==OB_LAMP)
				sbuts->texture_context = SB_TEXC_MAT_OR_LAMP;
			break;
		}
		case BCONTEXT_WORLD:
			sbuts->texture_context = SB_TEXC_WORLD;
			break;
		case BCONTEXT_PARTICLE:
			sbuts->texture_context = SB_TEXC_PARTICLES;
			break;
	}
}

static void do_buttons_buttons(bContext *C, void *UNUSED(arg), int event)
{
	SpaceButs *sbuts= CTX_wm_space_buts(C);

	if(!sbuts) /* editor type switch */
		return;

	switch(event) {
		case B_CONTEXT_SWITCH:
		case B_BUTSPREVIEW:
			ED_area_tag_redraw(CTX_wm_area(C));

			set_texture_context(C, sbuts);

			sbuts->preview= 1;
			break;
	}

	sbuts->mainbuser= sbuts->mainb;
}

#define BUT_UNIT_X (UI_UNIT_X+2)

void buttons_header_buttons(const bContext *C, ARegion *ar)
{
	SpaceButs *sbuts= CTX_wm_space_buts(C);
	uiBlock *block;
	int xco, yco= 2;

	buttons_context_compute(C, sbuts);
	
	block= uiBeginBlock(C, ar, "header buttons", UI_EMBOSS);
	uiBlockSetHandleFunc(block, do_buttons_buttons, NULL);
	
	xco= ED_area_header_switchbutton(C, block, yco);
	
	uiBlockSetEmboss(block, UI_EMBOSS);

	xco -= UI_UNIT_X;
	
	// Default panels
	uiBlockBeginAlign(block);
	if(sbuts->pathflag & (1<<BCONTEXT_RENDER))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_SCENE,			xco+=BUT_UNIT_X, yco, BUT_UNIT_X, UI_UNIT_Y, &(sbuts->mainb), 0.0, (float)BCONTEXT_RENDER, 0, 0, "Render");
	if(sbuts->pathflag & (1<<BCONTEXT_SCENE))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_SCENE_DATA,			xco+=BUT_UNIT_X, yco, BUT_UNIT_X, UI_UNIT_Y, &(sbuts->mainb), 0.0, (float)BCONTEXT_SCENE, 0, 0, "Scene");
	if(sbuts->pathflag & (1<<BCONTEXT_WORLD))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_WORLD,		xco+=BUT_UNIT_X, yco, BUT_UNIT_X, UI_UNIT_Y, &(sbuts->mainb), 0.0, (float)BCONTEXT_WORLD, 0, 0, "World");
	if(sbuts->pathflag & (1<<BCONTEXT_OBJECT))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_OBJECT_DATA,	xco+=BUT_UNIT_X, yco, BUT_UNIT_X, UI_UNIT_Y, &(sbuts->mainb), 0.0, (float)BCONTEXT_OBJECT, 0, 0, "Object");
	if(sbuts->pathflag & (1<<BCONTEXT_CONSTRAINT))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_CONSTRAINT,	xco+=BUT_UNIT_X, yco, BUT_UNIT_X, UI_UNIT_Y, &(sbuts->mainb), 0.0, (float)BCONTEXT_CONSTRAINT, 0, 0, "Object Constraints");
	if(sbuts->pathflag & (1<<BCONTEXT_MODIFIER))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_MODIFIER,	xco+=BUT_UNIT_X, yco, BUT_UNIT_X, UI_UNIT_Y, &(sbuts->mainb), 0.0, (float)BCONTEXT_MODIFIER, 0, 0, "Modifiers");
	if(sbuts->pathflag & (1<<BCONTEXT_DATA))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	sbuts->dataicon,	xco+=BUT_UNIT_X, yco, BUT_UNIT_X, UI_UNIT_Y, &(sbuts->mainb), 0.0, (float)BCONTEXT_DATA, 0, 0, "Object Data");
	if(sbuts->pathflag & (1<<BCONTEXT_BONE))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_BONE_DATA,	xco+=BUT_UNIT_X, yco, BUT_UNIT_X, UI_UNIT_Y, &(sbuts->mainb), 0.0, (float)BCONTEXT_BONE, 0, 0, "Bone");
	if(sbuts->pathflag & (1<<BCONTEXT_BONE_CONSTRAINT))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_CONSTRAINT_BONE,	xco+=BUT_UNIT_X, yco, BUT_UNIT_X, UI_UNIT_Y, &(sbuts->mainb), 0.0, (float)BCONTEXT_BONE_CONSTRAINT, 0, 0, "Bone Constraints");
	if(sbuts->pathflag & (1<<BCONTEXT_MATERIAL))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_MATERIAL,	xco+=BUT_UNIT_X, yco, BUT_UNIT_X, UI_UNIT_Y, &(sbuts->mainb), 0.0, (float)BCONTEXT_MATERIAL, 0, 0, "Material");
	if(sbuts->pathflag & (1<<BCONTEXT_TEXTURE))
		uiDefIconButS(block, ROW, B_BUTSPREVIEW,	ICON_TEXTURE,	xco+=BUT_UNIT_X, yco, BUT_UNIT_X, UI_UNIT_Y, &(sbuts->mainb), 0.0, (float)BCONTEXT_TEXTURE, 0, 0, "Texture");
	if(sbuts->pathflag & (1<<BCONTEXT_PARTICLE))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_PARTICLES,	xco+=BUT_UNIT_X, yco, BUT_UNIT_X, UI_UNIT_Y, &(sbuts->mainb), 0.0, (float)BCONTEXT_PARTICLE, 0, 0, "Particles");
	if(sbuts->pathflag & (1<<BCONTEXT_PHYSICS))
		uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_PHYSICS,	xco+=BUT_UNIT_X, yco, BUT_UNIT_X, UI_UNIT_Y, &(sbuts->mainb), 0.0, (float)BCONTEXT_PHYSICS, 0, 0, "Physics");
	xco+= BUT_UNIT_X;
	
	uiBlockEndAlign(block);
	
	/* always as last  */
	UI_view2d_totRect_set(&ar->v2d, xco+(UI_UNIT_X/2), ar->v2d.tot.ymax-ar->v2d.tot.ymin);
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
}


