/**
 * $Id:
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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>

#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_blender.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_window.h"
#include "wm_event_system.h"
#include "wm_event_types.h"

/* ***************** generic call, exported **************** */

static void keymap_set(wmKeymapItem *km, short type, short val, int modifier, short keymodifier)
{
	km->type= type;
	km->val= val;
	km->keymodifier= keymodifier;
	
	if(modifier & KM_SHIFT)
		km->shift= 1;
	else if(modifier & KM_SHIFT2)
		km->shift= 2;
	if(modifier & KM_CTRL)
		km->ctrl= 1;
	else if(modifier & KM_CTRL2)
		km->ctrl= 2;
	if(modifier & KM_ALT)
		km->alt= 1;
	else if(modifier & KM_ALT2)
		km->alt= 2;
	if(modifier & KM_OSKEY)
		km->oskey= 1;
	else if(modifier & KM_OSKEY2)
		km->oskey= 2;	
}

/* if item was added, then replace */
void WM_keymap_verify_item(ListBase *lb, char *idname, short type, short val, int modifier, short keymodifier)
{
	wmKeymapItem *km;
	
	/* if item was added, then bail out */
	for(km= lb->first; km; km= km->next)
		if(strncmp(km->idname, idname, OP_MAX_TYPENAME)==0)
			break;
	if(km==NULL) {
		km= MEM_callocN(sizeof(wmKeymapItem), "keymap entry");
		
		BLI_addtail(lb, km);
		BLI_strncpy(km->idname, idname, OP_MAX_TYPENAME);
		
		keymap_set(km, type, val, modifier, keymodifier);
	}
	
}

/* if item was added, then replace */
void WM_keymap_set_item(ListBase *lb, char *idname, short type, short val, int modifier, short keymodifier)
{
	wmKeymapItem *km;
	
	for(km= lb->first; km; km= km->next)
		if(strncmp(km->idname, idname, OP_MAX_TYPENAME)==0)
			break;
	if(km==NULL) {
		km= MEM_callocN(sizeof(wmKeymapItem), "keymap entry");
	
		BLI_addtail(lb, km);
		BLI_strncpy(km->idname, idname, OP_MAX_TYPENAME);
	}
	keymap_set(km, type, val, modifier, keymodifier);
}



