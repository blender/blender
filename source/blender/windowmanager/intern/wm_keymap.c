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

#include "RNA_access.h"
#include "RNA_types.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_window.h"
#include "wm_event_system.h"
#include "wm_event_types.h"

/* ***************** generic call, exported **************** */

static void keymap_event_set(wmKeymapItem *kmi, short type, short val, int modifier, short keymodifier)
{
	kmi->type= type;
	kmi->val= val;
	kmi->keymodifier= keymodifier;
	
	if(modifier & KM_SHIFT)
		kmi->shift= 1;
	else if(modifier & KM_SHIFT2)
		kmi->shift= 2;
	if(modifier & KM_CTRL)
		kmi->ctrl= 1;
	else if(modifier & KM_CTRL2)
		kmi->ctrl= 2;
	if(modifier & KM_ALT)
		kmi->alt= 1;
	else if(modifier & KM_ALT2)
		kmi->alt= 2;
	if(modifier & KM_OSKEY)
		kmi->oskey= 1;
	else if(modifier & KM_OSKEY2)
		kmi->oskey= 2;	
}

static void keymap_properties_set(wmKeymapItem *kmi)
{
	wmOperatorType *ot;
	
	if(!kmi->ptr) {
		ot= WM_operatortype_find(kmi->idname);

		if(ot) {
			kmi->ptr= MEM_callocN(sizeof(PointerRNA), "wmKeymapItemPtr");
			RNA_pointer_create(NULL, NULL, ot->srna, kmi, kmi->ptr);
		}
	}
}

/* if item was added, then bail out */
wmKeymapItem *WM_keymap_verify_item(ListBase *lb, char *idname, short type, short val, int modifier, short keymodifier)
{
	wmKeymapItem *kmi;
	
	for(kmi= lb->first; kmi; kmi= kmi->next)
		if(strncmp(kmi->idname, idname, OP_MAX_TYPENAME)==0)
			break;
	if(kmi==NULL) {
		kmi= MEM_callocN(sizeof(wmKeymapItem), "keymap entry");
		
		BLI_addtail(lb, kmi);
		BLI_strncpy(kmi->idname, idname, OP_MAX_TYPENAME);
		
		keymap_event_set(kmi, type, val, modifier, keymodifier);
		keymap_properties_set(kmi);
	}
	return kmi;
}

/* if item was added, then replace */
wmKeymapItem *WM_keymap_set_item(ListBase *lb, char *idname, short type, short val, int modifier, short keymodifier)
{
	wmKeymapItem *kmi;
	
	for(kmi= lb->first; kmi; kmi= kmi->next)
		if(strncmp(kmi->idname, idname, OP_MAX_TYPENAME)==0)
			break;
	if(kmi==NULL) {
		kmi= MEM_callocN(sizeof(wmKeymapItem), "keymap entry");
	
		BLI_addtail(lb, kmi);
		BLI_strncpy(kmi->idname, idname, OP_MAX_TYPENAME);
	}
	keymap_event_set(kmi, type, val, modifier, keymodifier);
	keymap_properties_set(kmi);
	return kmi;
}

/* always add item */
wmKeymapItem *WM_keymap_add_item(ListBase *lb, char *idname, short type, short val, int modifier, short keymodifier)
{
	wmKeymapItem *kmi= MEM_callocN(sizeof(wmKeymapItem), "keymap entry");
	
	BLI_addtail(lb, kmi);
	BLI_strncpy(kmi->idname, idname, OP_MAX_TYPENAME);

	keymap_event_set(kmi, type, val, modifier, keymodifier);
	keymap_properties_set(kmi);
	return kmi;
}

/* ****************** storage in WM ************ */

/* name id's are for storing general or multiple keymaps, 
   space/region ids are same as DNA_space_types.h */
/* gets free'd in wm.c */

ListBase *WM_keymap_listbase(wmWindowManager *wm, const char *nameid, int spaceid, int regionid)
{
	wmKeyMap *km;
	
	for(km= wm->keymaps.first; km; km= km->next)
		if(km->spaceid==spaceid && km->regionid==regionid)
			if(0==strncmp(nameid, km->nameid, KMAP_MAX_NAME))
				break;

	if(km==NULL) {
		km= MEM_callocN(sizeof(struct wmKeyMap), "keymap list");
		BLI_strncpy(km->nameid, nameid, KMAP_MAX_NAME);
		km->spaceid= spaceid;
		km->regionid= regionid;
		printf("added keymap %s %d %d\n", nameid, spaceid, regionid);
		BLI_addtail(&wm->keymaps, km);
	}
	
	return &km->keymap;
}

