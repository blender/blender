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
#include "DNA_userdef_types.h"
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

/* ********************* key config ***********************/

void WM_keymap_properties_reset(wmKeyMapItem *kmi)
{
	WM_operator_properties_free(kmi->ptr);
	MEM_freeN(kmi->ptr);

	kmi->ptr = NULL;
	kmi->properties = NULL;

	WM_operator_properties_alloc(&(kmi->ptr), &(kmi->properties), kmi->idname);
}

static void keymap_properties_set(wmKeyMapItem *kmi)
{
	WM_operator_properties_alloc(&(kmi->ptr), &(kmi->properties), kmi->idname);
}

wmKeyConfig *WM_keyconfig_add(wmWindowManager *wm, char *idname)
{
	wmKeyConfig *keyconf;
	
	keyconf= MEM_callocN(sizeof(wmKeyConfig), "wmKeyConfig");
	BLI_strncpy(keyconf->idname, idname, sizeof(keyconf->idname));
	BLI_addtail(&wm->keyconfigs, keyconf);

	return keyconf;
}

void WM_keyconfig_free(wmKeyConfig *keyconf)
{
	wmKeyMap *km;

	while((km= keyconf->keymaps.first)) {
		WM_keymap_free(km);
		BLI_freelinkN(&keyconf->keymaps, km);
	}

	MEM_freeN(keyconf);
}

void WM_keyconfig_userdef(wmWindowManager *wm)
{
	wmKeyMap *km;
	wmKeyMapItem *kmi;

	for(km=U.keymaps.first; km; km=km->next) {
		/* modal keymaps don't have operator properties */
		if ((km->flag & KEYMAP_MODAL) == 0) {
			for(kmi=km->items.first; kmi; kmi=kmi->next) {
				keymap_properties_set(kmi);
			}
		}
	}
}

static wmKeyConfig *wm_keyconfig_list_find(ListBase *lb, char *idname)
{
	wmKeyConfig *kc;

	for(kc= lb->first; kc; kc= kc->next)
		if(0==strncmp(idname, kc->idname, KMAP_MAX_NAME))
			return kc;
	
	return NULL;
}

/* ************************ free ************************* */

void WM_keymap_free(wmKeyMap *keymap)
{
	wmKeyMapItem *kmi;

	for(kmi=keymap->items.first; kmi; kmi=kmi->next) {
		if(kmi->ptr) {
			WM_operator_properties_free(kmi->ptr);
			MEM_freeN(kmi->ptr);
		}
	}

	BLI_freelistN(&keymap->items);
}

/* ***************** generic call, exported **************** */

static void keymap_event_set(wmKeyMapItem *kmi, short type, short val, int modifier, short keymodifier)
{
	kmi->type= type;
	kmi->val= val;
	kmi->keymodifier= keymodifier;
	
	if(modifier == KM_ANY) {
		kmi->shift= kmi->ctrl= kmi->alt= kmi->oskey= KM_ANY;
	}
	else {
		
		kmi->shift= kmi->ctrl= kmi->alt= kmi->oskey= 0;
		
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

/* if item was added, then bail out */
wmKeyMapItem *WM_keymap_verify_item(wmKeyMap *keymap, char *idname, int type, int val, int modifier, int keymodifier)
{
	wmKeyMapItem *kmi;
	
	for(kmi= keymap->items.first; kmi; kmi= kmi->next)
		if(strncmp(kmi->idname, idname, OP_MAX_TYPENAME)==0)
			break;
	if(kmi==NULL) {
		kmi= MEM_callocN(sizeof(wmKeyMapItem), "keymap entry");
		
		BLI_addtail(&keymap->items, kmi);
		BLI_strncpy(kmi->idname, idname, OP_MAX_TYPENAME);
		
		keymap_event_set(kmi, type, val, modifier, keymodifier);
		keymap_properties_set(kmi);
	}
	return kmi;
}

/* always add item */
wmKeyMapItem *WM_keymap_add_item(wmKeyMap *keymap, char *idname, int type, int val, int modifier, int keymodifier)
{
	wmKeyMapItem *kmi= MEM_callocN(sizeof(wmKeyMapItem), "keymap entry");
	
	BLI_addtail(&keymap->items, kmi);
	BLI_strncpy(kmi->idname, idname, OP_MAX_TYPENAME);

	keymap_event_set(kmi, type, val, modifier, keymodifier);
	keymap_properties_set(kmi);

	if ((keymap->flag & KEYMAP_USER) == 0) {
		keymap->kmi_id++;
		kmi->id = keymap->kmi_id;
	}

	return kmi;
}

/* menu wrapper for WM_keymap_add_item */
wmKeyMapItem *WM_keymap_add_menu(wmKeyMap *keymap, char *idname, int type, int val, int modifier, int keymodifier)
{
	wmKeyMapItem *kmi= WM_keymap_add_item(keymap, "WM_OT_call_menu", type, val, modifier, keymodifier);
	RNA_string_set(kmi->ptr, "name", idname);
	return kmi;
}

void WM_keymap_remove_item(wmKeyMap *keymap, wmKeyMapItem *kmi)
{
	if(BLI_findindex(&keymap->items, kmi) != -1) {
		if(kmi->ptr) {
			WM_operator_properties_free(kmi->ptr);
			MEM_freeN(kmi->ptr);
		}
		BLI_freelinkN(&keymap->items, kmi);
	}
}

/* ****************** storage in WM ************ */

/* name id's are for storing general or multiple keymaps, 
   space/region ids are same as DNA_space_types.h */
/* gets free'd in wm.c */

wmKeyMap *WM_keymap_list_find(ListBase *lb, char *idname, int spaceid, int regionid)
{
	wmKeyMap *km;

	for(km= lb->first; km; km= km->next)
		if(km->spaceid==spaceid && km->regionid==regionid)
			if(0==strncmp(idname, km->idname, KMAP_MAX_NAME))
				return km;
	
	return NULL;
}

wmKeyMap *WM_keymap_find(wmKeyConfig *keyconf, char *idname, int spaceid, int regionid)
{
	wmKeyMap *km= WM_keymap_list_find(&keyconf->keymaps, idname, spaceid, regionid);
	
	if(km==NULL) {
		km= MEM_callocN(sizeof(struct wmKeyMap), "keymap list");
		BLI_strncpy(km->idname, idname, KMAP_MAX_NAME);
		km->spaceid= spaceid;
		km->regionid= regionid;
		BLI_addtail(&keyconf->keymaps, km);
	}
	
	return km;
}

/* ****************** modal keymaps ************ */

/* modal maps get linked to a running operator, and filter the keys before sending to modal() callback */

wmKeyMap *WM_modalkeymap_add(wmKeyConfig *keyconf, char *idname, EnumPropertyItem *items)
{
	wmKeyMap *km= WM_keymap_find(keyconf, idname, 0, 0);
	km->flag |= KEYMAP_MODAL;
	km->modal_items= items;
	
	return km;
}

wmKeyMap *WM_modalkeymap_get(wmKeyConfig *keyconf, char *idname)
{
	wmKeyMap *km;
	
	for(km= keyconf->keymaps.first; km; km= km->next)
		if(km->flag & KEYMAP_MODAL)
			if(0==strncmp(idname, km->idname, KMAP_MAX_NAME))
				break;
	
	return km;
}


wmKeyMapItem *WM_modalkeymap_add_item(wmKeyMap *km, int type, int val, int modifier, int keymodifier, int value)
{
	wmKeyMapItem *kmi= MEM_callocN(sizeof(wmKeyMapItem), "keymap entry");
	
	BLI_addtail(&km->items, kmi);
	kmi->propvalue= value;
	
	keymap_event_set(kmi, type, val, modifier, keymodifier);

	if ((km->flag & KEYMAP_USER) == 0) {
		km->kmi_id++;
		kmi->id = km->kmi_id;
	}

	return kmi;
}

void WM_modalkeymap_assign(wmKeyMap *km, char *opname)
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

char *WM_keymap_item_to_string(wmKeyMapItem *kmi, char *str, int len)
{
	char buf[128];

	buf[0]= 0;

	if (kmi->shift == KM_ANY &&
		kmi->ctrl == KM_ANY &&
		kmi->alt == KM_ANY &&
		kmi->oskey == KM_ANY) {

		strcat(buf, "Any ");
	} else {
		if(kmi->shift)
			strcat(buf, "Shift ");

		if(kmi->ctrl)
			strcat(buf, "Ctrl ");

		if(kmi->alt)
			strcat(buf, "Alt ");

		if(kmi->oskey)
			strcat(buf, "Cmd ");
	}
		
	if(kmi->keymodifier) {
		strcat(buf, WM_key_event_string(kmi->keymodifier));
		strcat(buf, " ");
	}

	strcat(buf, WM_key_event_string(kmi->type));
	BLI_strncpy(str, buf, len);

	return str;
}

static wmKeyMapItem *wm_keymap_item_find_handlers(const bContext *C, ListBase *handlers, const char *opname, int opcontext, IDProperty *properties, int compare_props, wmKeyMap **keymap_r)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	wmEventHandler *handler;
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;

	/* find keymap item in handlers */
	for(handler=handlers->first; handler; handler=handler->next) {
		keymap= WM_keymap_active(wm, handler->keymap);

		if(keymap && (!keymap->poll || keymap->poll((bContext*)C))) {
			for(kmi=keymap->items.first; kmi; kmi=kmi->next) {
				if(strcmp(kmi->idname, opname) == 0 && WM_key_event_string(kmi->type)[0]) {
					if(compare_props) {
						if(kmi->ptr && IDP_EqualsProperties(properties, kmi->ptr->data)) {
							if(keymap_r) *keymap_r= keymap;
							return kmi;
						}
					}
					else {
						if(keymap_r) *keymap_r= keymap;
						return kmi;
					}
				}
			}
		}
	}
	
	return NULL;
}

static wmKeyMapItem *wm_keymap_item_find_props(const bContext *C, const char *opname, int opcontext, IDProperty *properties, int compare_props, wmKeyMap **keymap_r)
{
	wmWindow *win= CTX_wm_window(C);
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= CTX_wm_region(C);
	wmKeyMapItem *found= NULL;

	/* look into multiple handler lists to find the item */
	if(win)
		found= wm_keymap_item_find_handlers(C, &win->handlers, opname, opcontext, properties, compare_props, keymap_r);
	

	if(sa && found==NULL)
		found= wm_keymap_item_find_handlers(C, &sa->handlers, opname, opcontext, properties, compare_props, keymap_r);

	if(found==NULL) {
		if(ELEM(opcontext, WM_OP_EXEC_REGION_WIN, WM_OP_INVOKE_REGION_WIN)) {
			if(sa) {
				ARegion *ar= sa->regionbase.first;
				for(; ar; ar= ar->next)
					if(ar->regiontype==RGN_TYPE_WINDOW)
						break;

				if(ar)
					found= wm_keymap_item_find_handlers(C, &ar->handlers, opname, opcontext, properties, compare_props, keymap_r);
			}
		}
		else {
			if(ar)
				found= wm_keymap_item_find_handlers(C, &ar->handlers, opname, opcontext, properties, compare_props, keymap_r);
		}
	}
	
	return found;
}

static wmKeyMapItem *wm_keymap_item_find(const bContext *C, const char *opname, int opcontext, IDProperty *properties, wmKeyMap **keymap_r)
{
	wmKeyMapItem *found= wm_keymap_item_find_props(C, opname, opcontext, properties, 1, keymap_r);

	if(!found)
		found= wm_keymap_item_find_props(C, opname, opcontext, properties, 0, keymap_r);

	return found;
}

char *WM_key_event_operator_string(const bContext *C, const char *opname, int opcontext, IDProperty *properties, char *str, int len)
{
	wmKeyMapItem *kmi= wm_keymap_item_find(C, opname, opcontext, properties, NULL);
	
	if(kmi) {
		WM_keymap_item_to_string(kmi, str, len);
		return str;
	}

	return NULL;
}

int	WM_keymap_item_compare(wmKeyMapItem *k1, wmKeyMapItem *k2)
{
	int k1type, k2type;

	if (k1->flag & KMI_INACTIVE || k2->flag & KMI_INACTIVE)
		return 0;

	/* take event mapping into account */
	k1type = WM_userdef_event_map(k1->type);
	k2type = WM_userdef_event_map(k2->type);

	if(k1type != KM_ANY && k2type != KM_ANY && k1type != k2type)
		return 0;

	if(k1->val != KM_ANY && k2->val != KM_ANY) {
		/* take click, press, release conflict into account */
		if (k1->val == KM_CLICK && ELEM3(k2->val, KM_PRESS, KM_RELEASE, KM_CLICK) == 0)
			return 0;
		if (k2->val == KM_CLICK && ELEM3(k1->val, KM_PRESS, KM_RELEASE, KM_CLICK) == 0)
			return 0;
		if (k1->val != k2->val)
			return 0;
	}

	if(k1->shift != KM_ANY && k2->shift != KM_ANY && k1->shift != k2->shift)
		return 0;

	if(k1->ctrl != KM_ANY && k2->ctrl != KM_ANY && k1->ctrl != k2->ctrl)
		return 0;

	if(k1->alt != KM_ANY && k2->alt != KM_ANY && k1->alt != k2->alt)
		return 0;

	if(k1->oskey != KM_ANY && k2->oskey != KM_ANY && k1->oskey != k2->oskey)
		return 0;

	if(k1->keymodifier != k2->keymodifier)
		return 0;

	return 1;
}

/* ***************** user preferences ******************* */

int WM_keymap_user_init(wmWindowManager *wm, wmKeyMap *keymap)
{
	wmKeyConfig *keyconf;
	wmKeyMap *km;

	if(!keymap)
		return 0;

	/* init from user key config */
	keyconf= wm_keyconfig_list_find(&wm->keyconfigs, U.keyconfigstr);
	if(keyconf) {
		km= WM_keymap_list_find(&keyconf->keymaps, keymap->idname, keymap->spaceid, keymap->regionid);
		if(km) {
			keymap->poll= km->poll; /* lazy init */
			keymap->modal_items= km->modal_items;
			return 1;
		}
	}

	/* or from default */
	km= WM_keymap_list_find(&wm->defaultconf->keymaps, keymap->idname, keymap->spaceid, keymap->regionid);
	if(km) {
		keymap->poll= km->poll; /* lazy init */
		keymap->modal_items= km->modal_items;
		return 1;
	}

	return 0;
}

wmKeyMap *WM_keymap_active(wmWindowManager *wm, wmKeyMap *keymap)
{
	wmKeyConfig *keyconf;
	wmKeyMap *km;

	if(!keymap)
		return NULL;
	
	/* first user defined keymaps */
	km= WM_keymap_list_find(&U.keymaps, keymap->idname, keymap->spaceid, keymap->regionid);
	if(km) {
		km->poll= keymap->poll; /* lazy init */
		km->modal_items= keymap->modal_items;
		return km;
	}
	
	/* then user key config */
	keyconf= wm_keyconfig_list_find(&wm->keyconfigs, U.keyconfigstr);
	if(keyconf) {
		km= WM_keymap_list_find(&keyconf->keymaps, keymap->idname, keymap->spaceid, keymap->regionid);
		if(km) {
			km->poll= keymap->poll; /* lazy init */
			km->modal_items= keymap->modal_items;
			return km;
		}
	}

	/* then use default */
	km= WM_keymap_list_find(&wm->defaultconf->keymaps, keymap->idname, keymap->spaceid, keymap->regionid);
	return km;
}

wmKeyMap *WM_keymap_copy_to_user(wmKeyMap *keymap)
{
	wmKeyMap *usermap;
	wmKeyMapItem *kmi;

	usermap= WM_keymap_list_find(&U.keymaps, keymap->idname, keymap->spaceid, keymap->regionid);

	if(!usermap) {
		/* not saved yet, duplicate existing */
		usermap= MEM_dupallocN(keymap);
		usermap->modal_items= NULL;
		usermap->poll= NULL;
		usermap->flag |= KEYMAP_USER;

		BLI_addtail(&U.keymaps, usermap);
	}
	else {
		/* already saved, free items for re-copy */
		WM_keymap_free(usermap);
	}

	BLI_duplicatelist(&usermap->items, &keymap->items);

	for(kmi=usermap->items.first; kmi; kmi=kmi->next) {
		if(kmi->properties) {
			kmi->ptr= MEM_callocN(sizeof(PointerRNA), "UserKeyMapItemPtr");
			WM_operator_properties_create(kmi->ptr, kmi->idname);

			kmi->properties= IDP_CopyProperty(kmi->properties);
			kmi->ptr->data= kmi->properties;
		}
	}

	for(kmi=keymap->items.first; kmi; kmi=kmi->next)
		kmi->flag &= ~KMI_EXPANDED;

	return usermap;
}

void WM_keymap_restore_item_to_default(bContext *C, wmKeyMap *keymap, wmKeyMapItem *kmi)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmKeyConfig *keyconf;
	wmKeyMap *km = NULL;

	/* look in user key config */
	keyconf= wm_keyconfig_list_find(&wm->keyconfigs, U.keyconfigstr);
	if(keyconf) {
		km= WM_keymap_list_find(&keyconf->keymaps, keymap->idname, keymap->spaceid, keymap->regionid);
	}

	if (!km) {
		/* or from default */
		km= WM_keymap_list_find(&wm->defaultconf->keymaps, keymap->idname, keymap->spaceid, keymap->regionid);
	}

	if (km) {
		wmKeyMapItem *orig;

		for (orig = km->items.first; orig; orig = orig->next) {
			if (orig->id == kmi->id)
				break;
		}

		if (orig) {
			if(strcmp(orig->idname, kmi->idname) != 0) {
				BLI_strncpy(kmi->idname, orig->idname, sizeof(kmi->idname));

				WM_keymap_properties_reset(kmi);
			}
			kmi->properties= IDP_CopyProperty(orig->properties);
			kmi->ptr->data= kmi->properties;

			kmi->propvalue = orig->propvalue;
			kmi->type = orig->type;
			kmi->val = orig->val;
			kmi->shift = orig->shift;
			kmi->ctrl = orig->ctrl;
			kmi->alt = orig->alt;
			kmi->oskey = orig->oskey;
			kmi->keymodifier = orig->keymodifier;
			kmi->maptype = orig->maptype;

		}

	}
}

void WM_keymap_restore_to_default(wmKeyMap *keymap)
{
	wmKeyMap *usermap;

	usermap= WM_keymap_list_find(&U.keymaps, keymap->idname, keymap->spaceid, keymap->regionid);

	if(usermap) {
		WM_keymap_free(usermap);
		BLI_freelinkN(&U.keymaps, usermap);
	}
}

/* searches context and changes keymap item, if found */
void WM_key_event_operator_change(const bContext *C, const char *opname, int opcontext, IDProperty *properties, short key, short modifier)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	
	kmi= wm_keymap_item_find(C, opname, opcontext, properties, &keymap);

	if(kmi) {
		/* if the existing one is in a default keymap, copy it
		   to user preferences, and lookup again so we get a
		   key map item from the user preferences we can modify */
		if(BLI_findindex(&wm->defaultconf->keymaps, keymap) >= 0) {
			WM_keymap_copy_to_user(keymap);
			kmi= wm_keymap_item_find(C, opname, opcontext, properties, NULL);
		}

		keymap_event_set(kmi, key, KM_PRESS, modifier, 0);
	}
}

