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
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* This file defines the system for filtering data into a form suitable for
 * use by the Animation Editors, thus acting as a means by which the Animation 
 * Editors maintain a level of abstraction from the data they actually manipulate.
 * Thus, they only need to check on the type of the data they're manipulating, and
 * NOT worry about various layers of context/hierarchy checks.
 *
 * While this is primarily used for the Action/Dopesheet Editor (and its accessory modes),
 * the IPO Editor also uses this for it's channel list and for determining which curves
 * are being edited.
 *
 * -- Joshua Leung, Dec 2008
 */

#include <string.h>
#include <stdio.h>

#include "DNA_listBase.h"
#include "DNA_ID.h"
#include "DNA_action_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
#include "DNA_lattice_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "ED_anim_api.h"
#include "ED_types.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

/* ************************************************************ */
/* Blender Context <-> Animation Context mapping */

/* ----------- Private Stuff - Action Editor ------------- */

/* Get shapekey data being edited (for Action Editor -> ShapeKey mode) */
/* Note: there's a similar function in key.c (ob_get_key) */
Key *actedit_get_shapekeys (const bContext *C, SpaceAction *saction) 
{
    Scene *scene= CTX_data_scene(C);
    Object *ob;
    Key *key;
	
    ob = OBACT;  // XXX er...
    if (ob == NULL) 
		return NULL;
	
	/* pinning is not available in 'ShapeKey' mode... */
	//if (saction->pin) return NULL;
	
	/* shapekey data is stored with geometry data */
	switch (ob->type) {
		case OB_MESH:
			key= ((Mesh *)ob->data)->key;
			break;
			
		case OB_LATTICE:
			key= ((Lattice *)ob->data)->key;
			break;
			
		case OB_CURVE:
		case OB_SURF:
			key= ((Curve *)ob->data)->key;
			break;
			
		default:
			return NULL;
	}
	
	if (key) {
		if (key->type == KEY_RELATIVE)
			return key;
	}
	
    return NULL;
}

/* Get data being edited in Action Editor (depending on current 'mode') */
static void *actedit_get_context (const bContext *C, SpaceAction *saction, short *datatype)
{
	Scene *scene= CTX_data_scene(C);
	
	/* sync settings with current view status, then return appropriate data */
	switch (saction->mode) {
		case SACTCONT_ACTION: /* 'Action Editor' */
			/* if not pinned, sync with active object */
			if (saction->pin == 0) {
				if (OBACT)
					saction->action = OBACT->action;
				else
					saction->action= NULL;
			}
				
			*datatype= ANIMCONT_ACTION;
			return saction->action;
			
		case SACTCONT_SHAPEKEY: /* 'ShapeKey Editor' */
			*datatype= ANIMCONT_SHAPEKEY;
			return actedit_get_shapekeys(C, saction);
			
		case SACTCONT_GPENCIL: /* Grease Pencil */ // XXX review how this mode is handled...
			*datatype=ANIMCONT_GPENCIL;
			return CTX_wm_screen(C); // FIXME: add that dopesheet type thing here!
			break;
			
		case SACTCONT_DOPESHEET: /* DopeSheet */
			/* update scene-pointer (no need to check for pinning yet, as not implemented) */
			saction->ads.source= (ID *)scene;
			
			*datatype= ANIMCONT_DOPESHEET;
			return &saction->ads;
		
		default: /* unhandled yet */
			*datatype= ANIMCONT_NONE;
			return NULL;
	}
}

/* ----------- Private Stuff - IPO Editor ------------- */

/* ----------- Public API --------------- */

/* Obtain current anim-data context from Blender Context info */
void *animdata_get_context (const bContext *C, short *datatype)
{
	ScrArea *sa= CTX_wm_area(C);
	
	/* set datatype to 'None' for convenience */
	if (datatype == NULL) return NULL;
	*datatype= ANIMCONT_NONE;
	if (sa == NULL) return NULL; /* highly unlikely to happen, but still! */
	
	/* context depends on editor we are currently in */
	switch (sa->spacetype) {
		case SPACE_ACTION:
		{
			SpaceAction *saction= (SpaceAction *)CTX_wm_space_data(C);
			return actedit_get_context(C, saction, datatype);
		}
			break;
			
		case SPACE_IPO:
		{
			SpaceIpo *sipo= (SpaceIpo *)CTX_wm_space_data(C);
			// ...
		}
			break;
	}
	
	/* nothing appropriate */
	return NULL;
}

/* ************************************************************ */

/* ************************************************************ */
