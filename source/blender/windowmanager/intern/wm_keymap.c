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

#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_types.h"
#include "RNA_enum_types.h"

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
	
	if(modifier == KM_ANY) {
		kmi->shift= kmi->ctrl= kmi->alt= kmi->oskey= KM_ANY;
	}
	else {
		
		/* defines? */
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
}

static void keymap_properties_set(wmKeymapItem *kmi)
{
	if(!kmi->ptr) {
		kmi->ptr= MEM_callocN(sizeof(PointerRNA), "wmKeymapItemPtr");
		WM_operator_properties_create(kmi->ptr, kmi->idname);
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

static wmKeyMap *wm_keymap_add(wmWindowManager *wm, const char *nameid, short spaceid, short regionid)
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
		BLI_addtail(&wm->keymaps, km);
	}
	
	return km;
}

ListBase *WM_keymap_listbase(wmWindowManager *wm, const char *nameid, short spaceid, short regionid)
{
	wmKeyMap *km= wm_keymap_add(wm, nameid, spaceid, regionid);
	return &km->keymap;
}

/* ****************** modal keymaps ************ */

/* modal maps get linked to a running operator, and filter the keys before sending to modal() callback */

wmKeyMap *WM_modalkeymap_add(wmWindowManager *wm, const char *nameid, EnumPropertyItem *items)
{
	wmKeyMap *km= wm_keymap_add(wm, nameid, 0, 0);
	km->is_modal= 1;
	km->items= items;
	
	return km;
}

wmKeyMap *WM_modalkeymap_get(wmWindowManager *wm, const char *nameid)
{
	wmKeyMap *km;
	
	for(km= wm->keymaps.first; km; km= km->next)
		if(km->is_modal)
			if(0==strncmp(nameid, km->nameid, KMAP_MAX_NAME))
				break;
	
	return km;
}


void WM_modalkeymap_add_item(wmKeyMap *km, short type, short val, int modifier, short keymodifier, short value)
{
	wmKeymapItem *kmi= MEM_callocN(sizeof(wmKeymapItem), "keymap entry");
	
	BLI_addtail(&km->keymap, kmi);
	kmi->propvalue= value;
	
	keymap_event_set(kmi, type, val, modifier, keymodifier);
}

void WM_modalkeymap_assign(wmKeyMap *km, const char *opname)
{
	wmOperatorType *ot= WM_operatortype_find(opname, 0);
	
	if(ot)
		ot->modalkeymap= km;
	else
		printf("error: modalkeymap_assign, unknown operator %s\n", opname);
}


/* ***************** get string from key events **************** */

const char *WM_key_event_string(short type)
{
	const char *name= NULL;
	if(RNA_enum_name(event_type_items, (int)type, &name))
		return name;
	
	return "";
}

static char *wm_keymap_item_to_string(wmKeymapItem *kmi, char *str, int len)
{
	char buf[100];

	buf[0]= 0;

	if(kmi->shift)
		strcat(buf, "Shift ");

	if(kmi->ctrl)
		strcat(buf, "Ctrl ");

	if(kmi->alt)
		strcat(buf, "Alt ");

	if(kmi->oskey)
		strcat(buf, "OS ");

	strcat(buf, WM_key_event_string(kmi->type));
	BLI_strncpy(str, buf, len);

	return str;
}

static char *wm_keymap_item_find(ListBase *handlers, const char *opname, int opcontext, IDProperty *properties, char *str, int len)
{
	wmEventHandler *handler;
	wmKeymapItem *kmi;

	/* find keymap item in handlers */
	for(handler=handlers->first; handler; handler=handler->next)
		if(handler->keymap)
			for(kmi=handler->keymap->first; kmi; kmi=kmi->next)
				if(strcmp(kmi->idname, opname) == 0 && WM_key_event_string(kmi->type)[0])
					if(kmi->ptr && IDP_EqualsProperties(properties, kmi->ptr->data))
						return wm_keymap_item_to_string(kmi, str, len);
	
	return NULL;
}

char *WM_key_event_operator_string(const bContext *C, const char *opname, int opcontext, IDProperty *properties, char *str, int len)
{
	char *found= NULL;

	/* look into multiple handler lists to find the item */
	if(CTX_wm_window(C))
		if((found= wm_keymap_item_find(&CTX_wm_window(C)->handlers, opname, opcontext, properties, str, len)))
			return found;

	if(CTX_wm_area(C))
		if((found= wm_keymap_item_find(&CTX_wm_area(C)->handlers, opname, opcontext, properties, str, len)))
			return found;

	if(ELEM(opcontext, WM_OP_EXEC_REGION_WIN, WM_OP_INVOKE_REGION_WIN)) {
		if(CTX_wm_area(C)) {
			ARegion *ar= CTX_wm_area(C)->regionbase.first;
			for(; ar; ar= ar->next)
				if(ar->regiontype==RGN_TYPE_WINDOW)
					break;

			if(ar)
				if((found= wm_keymap_item_find(&ar->handlers, opname, opcontext, properties, str, len)))
					return found;
		}
	}
	else {
		if(CTX_wm_region(C))
			if((found= wm_keymap_item_find(&CTX_wm_region(C)->handlers, opname, opcontext, properties, str, len)))
				return found;
	}

	return NULL;
}

/* ********************* */

int WM_key_event_is_tweak(short type)
{
	if(type>=EVT_TWEAK_L && type<=EVT_GESTURE)
		return 1;
	return 0;
}


