/*
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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_keymap.c
 *  \ingroup wm
 */


#include <string.h>

#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_screen.h"


#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_window.h"
#include "wm_event_system.h"
#include "wm_event_types.h"

/******************************* Keymap Item **********************************
* Item in a keymap, that maps from an event to an operator or modal map item */

static wmKeyMapItem *wm_keymap_item_copy(wmKeyMapItem *kmi)
{
	wmKeyMapItem *kmin = MEM_dupallocN(kmi);

	kmin->prev = kmin->next = NULL;
	kmin->flag &= ~KMI_UPDATE;

	if (kmin->properties) {
		kmin->ptr = MEM_callocN(sizeof(PointerRNA), "UserKeyMapItemPtr");
		WM_operator_properties_create(kmin->ptr, kmin->idname);

		kmin->properties = IDP_CopyProperty(kmin->properties);
		kmin->ptr->data = kmin->properties;
	}

	return kmin;
}

static void wm_keymap_item_free(wmKeyMapItem *kmi)
{
	/* not kmi itself */
	if (kmi->ptr) {
		WM_operator_properties_free(kmi->ptr);
		MEM_freeN(kmi->ptr);
	}
}

static void wm_keymap_item_properties_set(wmKeyMapItem *kmi)
{
	WM_operator_properties_alloc(&(kmi->ptr), &(kmi->properties), kmi->idname);
	WM_operator_properties_sanitize(kmi->ptr, 1);
}

static int wm_keymap_item_equals_result(wmKeyMapItem *a, wmKeyMapItem *b)
{
	if (strcmp(a->idname, b->idname) != 0)
		return 0;
	
	if (!((a->ptr == NULL && b->ptr == NULL) ||
	      (a->ptr && b->ptr && IDP_EqualsProperties(a->ptr->data, b->ptr->data))))
		return 0;
	
	if ((a->flag & KMI_INACTIVE) != (b->flag & KMI_INACTIVE))
		return 0;
	
	return (a->propvalue == b->propvalue);
}

static int wm_keymap_item_equals(wmKeyMapItem *a, wmKeyMapItem *b)
{
	return (wm_keymap_item_equals_result(a, b) &&
	        a->type == b->type &&
	        a->val == b->val &&
	        a->shift == b->shift &&
	        a->ctrl == b->ctrl &&
	        a->alt == b->alt &&
	        a->oskey == b->oskey &&
	        a->keymodifier == b->keymodifier &&
	        a->maptype == b->maptype);
}

/* properties can be NULL, otherwise the arg passed is used and ownership is given to the kmi */
void WM_keymap_properties_reset(wmKeyMapItem *kmi, struct IDProperty *properties)
{
	WM_operator_properties_free(kmi->ptr);
	MEM_freeN(kmi->ptr);

	kmi->ptr = NULL;
	kmi->properties = properties;

	wm_keymap_item_properties_set(kmi);
}

/**************************** Keymap Diff Item *********************************
 * Item in a diff keymap, used for saving diff of keymaps in user preferences */

static wmKeyMapDiffItem *wm_keymap_diff_item_copy(wmKeyMapDiffItem *kmdi)
{
	wmKeyMapDiffItem *kmdin = MEM_dupallocN(kmdi);

	kmdin->next = kmdin->prev = NULL;
	if (kmdi->add_item)
		kmdin->add_item = wm_keymap_item_copy(kmdi->add_item);
	if (kmdi->remove_item)
		kmdin->remove_item = wm_keymap_item_copy(kmdi->remove_item);
	
	return kmdin;
}

static void wm_keymap_diff_item_free(wmKeyMapDiffItem *kmdi)
{
	if (kmdi->remove_item) {
		wm_keymap_item_free(kmdi->remove_item);
		MEM_freeN(kmdi->remove_item);
	}
	if (kmdi->add_item) {
		wm_keymap_item_free(kmdi->add_item);
		MEM_freeN(kmdi->add_item);
	}
}

/***************************** Key Configuration ******************************
 * List of keymaps for all editors, modes, ... . There is a builtin default key
 * configuration, a user key configuration, and other preset configurations. */

wmKeyConfig *WM_keyconfig_new(wmWindowManager *wm, const char *idname)
{
	wmKeyConfig *keyconf;
	
	keyconf = MEM_callocN(sizeof(wmKeyConfig), "wmKeyConfig");
	BLI_strncpy(keyconf->idname, idname, sizeof(keyconf->idname));
	BLI_addtail(&wm->keyconfigs, keyconf);

	return keyconf;
}

wmKeyConfig *WM_keyconfig_new_user(wmWindowManager *wm, const char *idname)
{
	wmKeyConfig *keyconf = WM_keyconfig_new(wm, idname);

	keyconf->flag |= KEYCONF_USER;

	return keyconf;
}

void WM_keyconfig_remove(wmWindowManager *wm, wmKeyConfig *keyconf)
{
	if (keyconf) {
		if (strncmp(U.keyconfigstr, keyconf->idname, sizeof(U.keyconfigstr)) == 0) {
			BLI_strncpy(U.keyconfigstr, wm->defaultconf->idname, sizeof(U.keyconfigstr));
			WM_keyconfig_update_tag(NULL, NULL);
		}

		BLI_remlink(&wm->keyconfigs, keyconf);
		WM_keyconfig_free(keyconf);
	}
}

void WM_keyconfig_free(wmKeyConfig *keyconf)
{
	wmKeyMap *km;

	while ((km = keyconf->keymaps.first)) {
		WM_keymap_free(km);
		BLI_freelinkN(&keyconf->keymaps, km);
	}

	MEM_freeN(keyconf);
}

static wmKeyConfig *wm_keyconfig_list_find(ListBase *lb, char *idname)
{
	wmKeyConfig *kc;

	for (kc = lb->first; kc; kc = kc->next)
		if (0 == strncmp(idname, kc->idname, KMAP_MAX_NAME))
			return kc;
	
	return NULL;
}

static wmKeyConfig *WM_keyconfig_active(wmWindowManager *wm)
{
	wmKeyConfig *keyconf;

	/* first try from preset */
	keyconf = wm_keyconfig_list_find(&wm->keyconfigs, U.keyconfigstr);
	if (keyconf)
		return keyconf;
	
	/* otherwise use default */
	return wm->defaultconf;
}

void WM_keyconfig_set_active(wmWindowManager *wm, const char *idname)
{
	/* setting a different key configuration as active: we ensure all is
	 * updated properly before and after making the change */

	WM_keyconfig_update(wm);

	BLI_strncpy(U.keyconfigstr, idname, sizeof(U.keyconfigstr));

	WM_keyconfig_update_tag(NULL, NULL);
	WM_keyconfig_update(wm);
}

/********************************** Keymap *************************************
 * List of keymap items for one editor, mode, modal operator, ... */

static wmKeyMap *wm_keymap_new(const char *idname, int spaceid, int regionid)
{
	wmKeyMap *km = MEM_callocN(sizeof(struct wmKeyMap), "keymap list");

	BLI_strncpy(km->idname, idname, KMAP_MAX_NAME);
	km->spaceid = spaceid;
	km->regionid = regionid;

	return km;
}

static wmKeyMap *wm_keymap_copy(wmKeyMap *keymap)
{
	wmKeyMap *keymapn = MEM_dupallocN(keymap);
	wmKeyMapItem *kmi, *kmin;
	wmKeyMapDiffItem *kmdi, *kmdin;

	keymapn->modal_items = keymap->modal_items;
	keymapn->poll = keymap->poll;
	keymapn->items.first = keymapn->items.last = NULL;
	keymapn->flag &= ~(KEYMAP_UPDATE | KEYMAP_EXPANDED);

	for (kmdi = keymap->diff_items.first; kmdi; kmdi = kmdi->next) {
		kmdin = wm_keymap_diff_item_copy(kmdi);
		BLI_addtail(&keymapn->items, kmdin);
	}

	for (kmi = keymap->items.first; kmi; kmi = kmi->next) {
		kmin = wm_keymap_item_copy(kmi);
		BLI_addtail(&keymapn->items, kmin);
	}

	return keymapn;
}

void WM_keymap_free(wmKeyMap *keymap)
{
	wmKeyMapItem *kmi;
	wmKeyMapDiffItem *kmdi;

	for (kmdi = keymap->diff_items.first; kmdi; kmdi = kmdi->next)
		wm_keymap_diff_item_free(kmdi);

	for (kmi = keymap->items.first; kmi; kmi = kmi->next)
		wm_keymap_item_free(kmi);

	BLI_freelistN(&keymap->diff_items);
	BLI_freelistN(&keymap->items);
}

static void keymap_event_set(wmKeyMapItem *kmi, short type, short val, int modifier, short keymodifier)
{
	kmi->type = type;
	kmi->val = val;
	kmi->keymodifier = keymodifier;

	if (modifier == KM_ANY) {
		kmi->shift = kmi->ctrl = kmi->alt = kmi->oskey = KM_ANY;
	}
	else {
		kmi->shift = (modifier & KM_SHIFT) ? KM_MOD_FIRST : ((modifier & KM_SHIFT2) ? KM_MOD_SECOND : FALSE);
		kmi->ctrl =  (modifier & KM_CTRL)  ? KM_MOD_FIRST : ((modifier & KM_CTRL2)  ? KM_MOD_SECOND : FALSE);
		kmi->alt =   (modifier & KM_ALT)   ? KM_MOD_FIRST : ((modifier & KM_ALT2)   ? KM_MOD_SECOND : FALSE);
		kmi->oskey = (modifier & KM_OSKEY) ? KM_MOD_FIRST : ((modifier & KM_OSKEY2) ? KM_MOD_SECOND : FALSE);
	}
}

static void keymap_item_set_id(wmKeyMap *keymap, wmKeyMapItem *kmi)
{
	keymap->kmi_id++;
	if ((keymap->flag & KEYMAP_USER) == 0) {
		kmi->id = keymap->kmi_id;
	}
	else {
		kmi->id = -keymap->kmi_id; // User defined keymap entries have negative ids
	}
}

/* if item was added, then bail out */
wmKeyMapItem *WM_keymap_verify_item(wmKeyMap *keymap, const char *idname, int type, int val, int modifier, int keymodifier)
{
	wmKeyMapItem *kmi;
	
	for (kmi = keymap->items.first; kmi; kmi = kmi->next)
		if (strncmp(kmi->idname, idname, OP_MAX_TYPENAME) == 0)
			break;
	if (kmi == NULL) {
		kmi = MEM_callocN(sizeof(wmKeyMapItem), "keymap entry");
		
		BLI_addtail(&keymap->items, kmi);
		BLI_strncpy(kmi->idname, idname, OP_MAX_TYPENAME);
		
		keymap_item_set_id(keymap, kmi);

		keymap_event_set(kmi, type, val, modifier, keymodifier);
		wm_keymap_item_properties_set(kmi);
	}
	return kmi;
}

/* always add item */
wmKeyMapItem *WM_keymap_add_item(wmKeyMap *keymap, const char *idname, int type, int val, int modifier, int keymodifier)
{
	wmKeyMapItem *kmi = MEM_callocN(sizeof(wmKeyMapItem), "keymap entry");
	
	BLI_addtail(&keymap->items, kmi);
	BLI_strncpy(kmi->idname, idname, OP_MAX_TYPENAME);

	keymap_event_set(kmi, type, val, modifier, keymodifier);
	wm_keymap_item_properties_set(kmi);

	keymap_item_set_id(keymap, kmi);

	WM_keyconfig_update_tag(keymap, kmi);

	return kmi;
}

/* menu wrapper for WM_keymap_add_item */
wmKeyMapItem *WM_keymap_add_menu(wmKeyMap *keymap, const char *idname, int type, int val, int modifier, int keymodifier)
{
	wmKeyMapItem *kmi = WM_keymap_add_item(keymap, "WM_OT_call_menu", type, val, modifier, keymodifier);
	RNA_string_set(kmi->ptr, "name", idname);
	return kmi;
}

void WM_keymap_remove_item(wmKeyMap *keymap, wmKeyMapItem *kmi)
{
	if (BLI_findindex(&keymap->items, kmi) != -1) {
		if (kmi->ptr) {
			WM_operator_properties_free(kmi->ptr);
			MEM_freeN(kmi->ptr);
		}
		BLI_freelinkN(&keymap->items, kmi);

		WM_keyconfig_update_tag(keymap, kmi);
	}
}

/************************** Keymap Diff and Patch ****************************
 * Rather than saving the entire keymap for user preferences, we only save a
 * diff so that changes in the defaults get synced. This system is not perfect
 * but works better than overriding the keymap entirely when only few items
 * are changed. */

static void wm_keymap_addon_add(wmKeyMap *keymap, wmKeyMap *addonmap)
{
	wmKeyMapItem *kmi, *kmin;

	for (kmi = addonmap->items.first; kmi; kmi = kmi->next) {
		kmin = wm_keymap_item_copy(kmi);
		keymap_item_set_id(keymap, kmin);
		BLI_addhead(&keymap->items, kmin);
	}
}

static wmKeyMapItem *wm_keymap_find_item_equals(wmKeyMap *km, wmKeyMapItem *needle)
{
	wmKeyMapItem *kmi;

	for (kmi = km->items.first; kmi; kmi = kmi->next)
		if (wm_keymap_item_equals(kmi, needle))
			return kmi;
	
	return NULL;
}

static wmKeyMapItem *wm_keymap_find_item_equals_result(wmKeyMap *km, wmKeyMapItem *needle)
{
	wmKeyMapItem *kmi;

	for (kmi = km->items.first; kmi; kmi = kmi->next)
		if (wm_keymap_item_equals_result(kmi, needle))
			return kmi;
	
	return NULL;
}

static void wm_keymap_diff(wmKeyMap *diff_km, wmKeyMap *from_km, wmKeyMap *to_km, wmKeyMap *orig_km, wmKeyMap *addon_km)
{
	wmKeyMapItem *kmi, *to_kmi, *orig_kmi;
	wmKeyMapDiffItem *kmdi;

	for (kmi = from_km->items.first; kmi; kmi = kmi->next) {
		to_kmi = WM_keymap_item_find_id(to_km, kmi->id);

		if (!to_kmi) {
			/* remove item */
			kmdi = MEM_callocN(sizeof(wmKeyMapDiffItem), "wmKeyMapDiffItem");
			kmdi->remove_item = wm_keymap_item_copy(kmi);
			BLI_addtail(&diff_km->diff_items, kmdi);
		}
		else if (to_kmi && !wm_keymap_item_equals(kmi, to_kmi)) {
			/* replace item */
			kmdi = MEM_callocN(sizeof(wmKeyMapDiffItem), "wmKeyMapDiffItem");
			kmdi->remove_item = wm_keymap_item_copy(kmi);
			kmdi->add_item = wm_keymap_item_copy(to_kmi);
			BLI_addtail(&diff_km->diff_items, kmdi);
		}

		/* sync expanded flag back to original so we don't loose it on repatch */
		if (to_kmi) {
			orig_kmi = WM_keymap_item_find_id(orig_km, kmi->id);

			if (!orig_kmi)
				orig_kmi = wm_keymap_find_item_equals(addon_km, kmi);

			if (orig_kmi) {
				orig_kmi->flag &= ~KMI_EXPANDED;
				orig_kmi->flag |= (to_kmi->flag & KMI_EXPANDED);
			}
		}
	}

	for (kmi = to_km->items.first; kmi; kmi = kmi->next) {
		if (kmi->id < 0) {
			/* add item */
			kmdi = MEM_callocN(sizeof(wmKeyMapDiffItem), "wmKeyMapDiffItem");
			kmdi->add_item = wm_keymap_item_copy(kmi);
			BLI_addtail(&diff_km->diff_items, kmdi);
		}
	}
}

static void wm_keymap_patch(wmKeyMap *km, wmKeyMap *diff_km)
{
	wmKeyMapDiffItem *kmdi;
	wmKeyMapItem *kmi_remove, *kmi_add;

	for (kmdi = diff_km->diff_items.first; kmdi; kmdi = kmdi->next) {
		/* find item to remove */
		kmi_remove = NULL;
		if (kmdi->remove_item) {
			kmi_remove = wm_keymap_find_item_equals(km, kmdi->remove_item);
			if (!kmi_remove)
				kmi_remove = wm_keymap_find_item_equals_result(km, kmdi->remove_item);
		}

		/* add item */
		if (kmdi->add_item) {
			/* only if nothing to remove or item to remove found */
			if (!kmdi->remove_item || kmi_remove) {
				kmi_add = wm_keymap_item_copy(kmdi->add_item);
				kmi_add->flag |= KMI_USER_MODIFIED;

				if (kmi_remove) {
					kmi_add->flag &= ~KMI_EXPANDED;
					kmi_add->flag |= (kmi_remove->flag & KMI_EXPANDED);
					kmi_add->id = kmi_remove->id;
					BLI_insertlinkbefore(&km->items, kmi_remove, kmi_add);
				}
				else {
					keymap_item_set_id(km, kmi_add);
					BLI_addtail(&km->items, kmi_add);
				}
			}
		}

		/* remove item */
		if (kmi_remove) {
			wm_keymap_item_free(kmi_remove);
			BLI_freelinkN(&km->items, kmi_remove);
		}
	}
}

static wmKeyMap *wm_keymap_patch_update(ListBase *lb, wmKeyMap *defaultmap, wmKeyMap *addonmap, wmKeyMap *usermap)
{
	wmKeyMap *km;
	int expanded = 0;

	/* remove previous keymap in list, we will replace it */
	km = WM_keymap_list_find(lb, defaultmap->idname, defaultmap->spaceid, defaultmap->regionid);
	if (km) {
		expanded = (km->flag & (KEYMAP_EXPANDED | KEYMAP_CHILDREN_EXPANDED));
		WM_keymap_free(km);
		BLI_freelinkN(lb, km);
	}

	/* copy new keymap from an existing one */
	if (usermap && !(usermap->flag & KEYMAP_DIFF)) {
		/* for compatibiltiy with old user preferences with non-diff
		 * keymaps we override the original entirely */
		wmKeyMapItem *kmi, *orig_kmi;

		km = wm_keymap_copy(usermap);

		/* try to find corresponding id's for items */
		for (kmi = km->items.first; kmi; kmi = kmi->next) {
			orig_kmi = wm_keymap_find_item_equals(defaultmap, kmi);
			if (!orig_kmi)
				orig_kmi = wm_keymap_find_item_equals_result(defaultmap, kmi);

			if (orig_kmi)
				kmi->id = orig_kmi->id;
			else
				kmi->id = -(km->kmi_id++);
		}

		km->flag |= KEYMAP_UPDATE; /* update again to create diff */
	}
	else
		km = wm_keymap_copy(defaultmap);

	/* add addon keymap items */
	if (addonmap)
		wm_keymap_addon_add(km, addonmap);

	/* tag as being user edited */
	if (usermap)
		km->flag |= KEYMAP_USER_MODIFIED;
	km->flag |= KEYMAP_USER | expanded;

	/* apply user changes of diff keymap */
	if (usermap && (usermap->flag & KEYMAP_DIFF))
		wm_keymap_patch(km, usermap);

	/* add to list */
	BLI_addtail(lb, km);
	
	return km;
}

static void wm_keymap_diff_update(ListBase *lb, wmKeyMap *defaultmap, wmKeyMap *addonmap, wmKeyMap *km)
{
	wmKeyMap *diffmap, *prevmap, *origmap;

	/* create temporary default + addon keymap for diff */
	origmap = defaultmap;

	if (addonmap) {
		defaultmap = wm_keymap_copy(defaultmap);
		wm_keymap_addon_add(defaultmap, addonmap);
	}

	/* remove previous diff keymap in list, we will replace it */
	prevmap = WM_keymap_list_find(lb, km->idname, km->spaceid, km->regionid);
	if (prevmap) {
		WM_keymap_free(prevmap);
		BLI_freelinkN(lb, prevmap);
	}

	/* create diff keymap */
	diffmap = wm_keymap_new(km->idname, km->spaceid, km->regionid);
	diffmap->flag |= KEYMAP_DIFF;
	if (defaultmap->flag & KEYMAP_MODAL)
		diffmap->flag |= KEYMAP_MODAL;
	wm_keymap_diff(diffmap, defaultmap, km, origmap, addonmap);

	/* add to list if not empty */
	if (diffmap->diff_items.first) {
		BLI_addtail(lb, diffmap);
	}
	else {
		WM_keymap_free(diffmap);
		MEM_freeN(diffmap);
	}

	/* free temporary default map */
	if (addonmap) {
		WM_keymap_free(defaultmap);
		MEM_freeN(defaultmap);
	}
}

/* ****************** storage in WM ************ */

/* name id's are for storing general or multiple keymaps, 
 * space/region ids are same as DNA_space_types.h */
/* gets freed in wm.c */

wmKeyMap *WM_keymap_list_find(ListBase *lb, const char *idname, int spaceid, int regionid)
{
	wmKeyMap *km;

	for (km = lb->first; km; km = km->next)
		if (km->spaceid == spaceid && km->regionid == regionid)
			if (0 == strncmp(idname, km->idname, KMAP_MAX_NAME))
				return km;
	
	return NULL;
}

wmKeyMap *WM_keymap_find(wmKeyConfig *keyconf, const char *idname, int spaceid, int regionid)
{
	wmKeyMap *km = WM_keymap_list_find(&keyconf->keymaps, idname, spaceid, regionid);
	
	if (km == NULL) {
		km = wm_keymap_new(idname, spaceid, regionid);
		BLI_addtail(&keyconf->keymaps, km);

		WM_keyconfig_update_tag(km, NULL);
	}
	
	return km;
}

wmKeyMap *WM_keymap_find_all(const bContext *C, const char *idname, int spaceid, int regionid)
{
	wmWindowManager *wm = CTX_wm_manager(C);

	return WM_keymap_list_find(&wm->userconf->keymaps, idname, spaceid, regionid);
}

/* ****************** modal keymaps ************ */

/* modal maps get linked to a running operator, and filter the keys before sending to modal() callback */

wmKeyMap *WM_modalkeymap_add(wmKeyConfig *keyconf, const char *idname, EnumPropertyItem *items)
{
	wmKeyMap *km = WM_keymap_find(keyconf, idname, 0, 0);
	km->flag |= KEYMAP_MODAL;
	km->modal_items = items;

	if (!items) {
		/* init modal items from default config */
		wmWindowManager *wm = G.main->wm.first;
		wmKeyMap *defaultkm = WM_keymap_list_find(&wm->defaultconf->keymaps, km->idname, 0, 0);

		if (defaultkm) {
			km->modal_items = defaultkm->modal_items;
			km->poll = defaultkm->poll;
		}
	}
	
	return km;
}

wmKeyMap *WM_modalkeymap_get(wmKeyConfig *keyconf, const char *idname)
{
	wmKeyMap *km;
	
	for (km = keyconf->keymaps.first; km; km = km->next)
		if (km->flag & KEYMAP_MODAL)
			if (0 == strncmp(idname, km->idname, KMAP_MAX_NAME))
				break;
	
	return km;
}


wmKeyMapItem *WM_modalkeymap_add_item(wmKeyMap *km, int type, int val, int modifier, int keymodifier, int value)
{
	wmKeyMapItem *kmi = MEM_callocN(sizeof(wmKeyMapItem), "keymap entry");
	
	BLI_addtail(&km->items, kmi);
	kmi->propvalue = value;
	
	keymap_event_set(kmi, type, val, modifier, keymodifier);

	keymap_item_set_id(km, kmi);

	WM_keyconfig_update_tag(km, kmi);

	return kmi;
}

wmKeyMapItem *WM_modalkeymap_add_item_str(wmKeyMap *km, int type, int val, int modifier, int keymodifier, const char *value)
{
	wmKeyMapItem *kmi = MEM_callocN(sizeof(wmKeyMapItem), "keymap entry");

	BLI_addtail(&km->items, kmi);
	BLI_strncpy(kmi->propvalue_str, value, sizeof(kmi->propvalue_str));

	keymap_event_set(kmi, type, val, modifier, keymodifier);

	keymap_item_set_id(km, kmi);

	WM_keyconfig_update_tag(km, kmi);

	return kmi;
}

void WM_modalkeymap_assign(wmKeyMap *km, const char *opname)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0);
	
	if (ot)
		ot->modalkeymap = km;
	else
		printf("error: modalkeymap_assign, unknown operator %s\n", opname);
}

static void wm_user_modal_keymap_set_items(wmWindowManager *wm, wmKeyMap *km)
{
	/* here we convert propvalue string values delayed, due to python keymaps
	 * being created before the actual modal keymaps, so no modal_items */
	wmKeyMap *defaultkm;
	wmKeyMapItem *kmi;
	int propvalue;

	if (km && (km->flag & KEYMAP_MODAL) && !km->modal_items) {
		defaultkm = WM_keymap_list_find(&wm->defaultconf->keymaps, km->idname, 0, 0);

		if (!defaultkm)
			return;

		km->modal_items = defaultkm->modal_items;
		km->poll = defaultkm->poll;

		for (kmi = km->items.first; kmi; kmi = kmi->next) {
			if (kmi->propvalue_str[0]) {
				if (RNA_enum_value_from_id(km->modal_items, kmi->propvalue_str, &propvalue))
					kmi->propvalue = propvalue;
				kmi->propvalue_str[0] = '\0';
			}
		}
	}
}

/* ***************** get string from key events **************** */

const char *WM_key_event_string(short type)
{
	const char *name = NULL;
	if (RNA_enum_name(event_type_items, (int)type, &name))
		return name;
	
	return "";
}

char *WM_keymap_item_to_string(wmKeyMapItem *kmi, char *str, int len)
{
	char buf[128];

	buf[0] = 0;

	if (kmi->shift == KM_ANY &&
	    kmi->ctrl == KM_ANY &&
	    kmi->alt == KM_ANY &&
	    kmi->oskey == KM_ANY) {

		strcat(buf, "Any ");
	}
	else {
		if (kmi->shift)
			strcat(buf, "Shift ");

		if (kmi->ctrl)
			strcat(buf, "Ctrl ");

		if (kmi->alt)
			strcat(buf, "Alt ");

		if (kmi->oskey)
			strcat(buf, "Cmd ");
	}
		
	if (kmi->keymodifier) {
		strcat(buf, WM_key_event_string(kmi->keymodifier));
		strcat(buf, " ");
	}

	strcat(buf, WM_key_event_string(kmi->type));
	BLI_strncpy(str, buf, len);

	return str;
}

static wmKeyMapItem *wm_keymap_item_find_handlers(
    const bContext *C, ListBase *handlers, const char *opname, int UNUSED(opcontext),
    IDProperty *properties, int compare_props, int hotkey, wmKeyMap **keymap_r)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmEventHandler *handler;
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;

	/* find keymap item in handlers */
	for (handler = handlers->first; handler; handler = handler->next) {
		keymap = WM_keymap_active(wm, handler->keymap);

		if (keymap && (!keymap->poll || keymap->poll((bContext *)C))) {
			for (kmi = keymap->items.first; kmi; kmi = kmi->next) {
				
				if (strcmp(kmi->idname, opname) == 0 && WM_key_event_string(kmi->type)[0]) {
					if (hotkey)
						if (!ISHOTKEY(kmi->type))
							continue;
					
					if (compare_props) {
						if (kmi->ptr && IDP_EqualsProperties(properties, kmi->ptr->data)) {
							if (keymap_r) *keymap_r = keymap;
							return kmi;
						}
					}
					else {
						if (keymap_r) *keymap_r = keymap;
						return kmi;
					}
				}
			}
		}
	}
	
	/* ensure un-initialized keymap is never used */
	if (keymap_r) *keymap_r = NULL;
	return NULL;
}

static wmKeyMapItem *wm_keymap_item_find_props(
    const bContext *C, const char *opname, int opcontext,
    IDProperty *properties, int compare_props, int hotkey, wmKeyMap **keymap_r)
{
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	wmKeyMapItem *found = NULL;

	/* look into multiple handler lists to find the item */
	if (win)
		found = wm_keymap_item_find_handlers(C, &win->handlers, opname, opcontext, properties, compare_props, hotkey, keymap_r);
	

	if (sa && found == NULL)
		found = wm_keymap_item_find_handlers(C, &sa->handlers, opname, opcontext, properties, compare_props, hotkey, keymap_r);

	if (found == NULL) {
		if (ELEM(opcontext, WM_OP_EXEC_REGION_WIN, WM_OP_INVOKE_REGION_WIN)) {
			if (sa) {
				if (!(ar && ar->regiontype == RGN_TYPE_WINDOW))
					ar = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
				
				if (ar)
					found = wm_keymap_item_find_handlers(C, &ar->handlers, opname, opcontext, properties, compare_props, hotkey, keymap_r);
			}
		}
		else if (ELEM(opcontext, WM_OP_EXEC_REGION_CHANNELS, WM_OP_INVOKE_REGION_CHANNELS)) {
			if (!(ar && ar->regiontype == RGN_TYPE_CHANNELS))
				ar = BKE_area_find_region_type(sa, RGN_TYPE_CHANNELS);

			if (ar)
				found = wm_keymap_item_find_handlers(C, &ar->handlers, opname, opcontext, properties, compare_props, hotkey, keymap_r);
		}
		else if (ELEM(opcontext, WM_OP_EXEC_REGION_PREVIEW, WM_OP_INVOKE_REGION_PREVIEW)) {
			if (!(ar && ar->regiontype == RGN_TYPE_PREVIEW))
				ar = BKE_area_find_region_type(sa, RGN_TYPE_PREVIEW);

			if (ar)
				found = wm_keymap_item_find_handlers(C, &ar->handlers, opname, opcontext, properties, compare_props, hotkey, keymap_r);
		}
		else {
			if (ar)
				found = wm_keymap_item_find_handlers(C, &ar->handlers, opname, opcontext, properties, compare_props, hotkey, keymap_r);
		}
	}
	
	return found;
}

static wmKeyMapItem *wm_keymap_item_find(
    const bContext *C, const char *opname, int opcontext,
    IDProperty *properties, const short hotkey, const short sloppy, wmKeyMap **keymap_r)
{
	wmKeyMapItem *found = wm_keymap_item_find_props(C, opname, opcontext, properties, 1, hotkey, keymap_r);

	if (!found && sloppy)
		found = wm_keymap_item_find_props(C, opname, opcontext, NULL, 0, hotkey, keymap_r);

	return found;
}

char *WM_key_event_operator_string(
    const bContext *C, const char *opname, int opcontext,
    IDProperty *properties, const short sloppy, char *str, int len)
{
	wmKeyMapItem *kmi = wm_keymap_item_find(C, opname, opcontext, properties, 0, sloppy, NULL);
	
	if (kmi) {
		WM_keymap_item_to_string(kmi, str, len);
		return str;
	}

	return NULL;
}

int WM_key_event_operator_id(
    const bContext *C, const char *opname, int opcontext,
    IDProperty *properties, int hotkey, wmKeyMap **keymap_r)
{
	wmKeyMapItem *kmi = wm_keymap_item_find(C, opname, opcontext, properties, hotkey, TRUE, keymap_r);
	
	if (kmi)
		return kmi->id;
	else
		return 0;
}

int WM_keymap_item_compare(wmKeyMapItem *k1, wmKeyMapItem *k2)
{
	int k1type, k2type;

	if (k1->flag & KMI_INACTIVE || k2->flag & KMI_INACTIVE)
		return 0;

	/* take event mapping into account */
	k1type = WM_userdef_event_map(k1->type);
	k2type = WM_userdef_event_map(k2->type);

	if (k1type != KM_ANY && k2type != KM_ANY && k1type != k2type)
		return 0;

	if (k1->val != KM_ANY && k2->val != KM_ANY) {
		/* take click, press, release conflict into account */
		if (k1->val == KM_CLICK && ELEM3(k2->val, KM_PRESS, KM_RELEASE, KM_CLICK) == 0)
			return 0;
		if (k2->val == KM_CLICK && ELEM3(k1->val, KM_PRESS, KM_RELEASE, KM_CLICK) == 0)
			return 0;
		if (k1->val != k2->val)
			return 0;
	}

	if (k1->shift != KM_ANY && k2->shift != KM_ANY && k1->shift != k2->shift)
		return 0;

	if (k1->ctrl != KM_ANY && k2->ctrl != KM_ANY && k1->ctrl != k2->ctrl)
		return 0;

	if (k1->alt != KM_ANY && k2->alt != KM_ANY && k1->alt != k2->alt)
		return 0;

	if (k1->oskey != KM_ANY && k2->oskey != KM_ANY && k1->oskey != k2->oskey)
		return 0;

	if (k1->keymodifier != k2->keymodifier)
		return 0;

	return 1;
}

/************************* Update Final Configuration *************************
 * On load or other changes, the final user key configuration is rebuilt from
 * the preset, addon and user preferences keymaps. We also test if the final
 * configuration changed and write the changes to the user preferences. */

static int WM_KEYMAP_UPDATE = 0;

void WM_keyconfig_update_tag(wmKeyMap *km, wmKeyMapItem *kmi)
{
	/* quick tag to do delayed keymap updates */
	WM_KEYMAP_UPDATE = 1;

	if (km)
		km->flag |= KEYMAP_UPDATE;
	if (kmi)
		kmi->flag |= KMI_UPDATE;
}

static int wm_keymap_test_and_clear_update(wmKeyMap *km)
{
	wmKeyMapItem *kmi;
	int update;
	
	update = (km->flag & KEYMAP_UPDATE);
	km->flag &= ~KEYMAP_UPDATE;

	for (kmi = km->items.first; kmi; kmi = kmi->next) {
		update = update || (kmi->flag & KMI_UPDATE);
		kmi->flag &= ~KMI_UPDATE;
	}
	
	return update;
}

static wmKeyMap *wm_keymap_preset(wmWindowManager *wm, wmKeyMap *km)
{
	wmKeyConfig *keyconf = WM_keyconfig_active(wm);
	wmKeyMap *keymap;

	keymap = WM_keymap_list_find(&keyconf->keymaps, km->idname, km->spaceid, km->regionid);
	if (!keymap)
		keymap = WM_keymap_list_find(&wm->defaultconf->keymaps, km->idname, km->spaceid, km->regionid);

	return keymap;
}

void WM_keyconfig_update(wmWindowManager *wm)
{
	wmKeyMap *km, *defaultmap, *addonmap, *usermap, *kmn;
	wmKeyMapItem *kmi;
	wmKeyMapDiffItem *kmdi;
	int compat_update = 0;

	if (G.background)
		return;
	if (!WM_KEYMAP_UPDATE)
		return;
	
	/* update operator properties for non-modal user keymaps */
	for (km = U.user_keymaps.first; km; km = km->next) {
		if ((km->flag & KEYMAP_MODAL) == 0) {
			for (kmdi = km->diff_items.first; kmdi; kmdi = kmdi->next) {
				if (kmdi->add_item)
					wm_keymap_item_properties_set(kmdi->add_item);
				if (kmdi->remove_item)
					wm_keymap_item_properties_set(kmdi->remove_item);
			}

			for (kmi = km->items.first; kmi; kmi = kmi->next)
				wm_keymap_item_properties_set(kmi);
		}
	}

	/* update U.user_keymaps with user key configuration changes */
	for (km = wm->userconf->keymaps.first; km; km = km->next) {
		/* only diff if the user keymap was modified */
		if (wm_keymap_test_and_clear_update(km)) {
			/* find keymaps */
			defaultmap = wm_keymap_preset(wm, km);
			addonmap = WM_keymap_list_find(&wm->addonconf->keymaps, km->idname, km->spaceid, km->regionid);

			/* diff */
			if (defaultmap)
				wm_keymap_diff_update(&U.user_keymaps, defaultmap, addonmap, km);
		}
	}

	/* create user key configuration from preset + addon + user preferences */
	for (km = wm->defaultconf->keymaps.first; km; km = km->next) {
		/* find keymaps */
		defaultmap = wm_keymap_preset(wm, km);
		addonmap = WM_keymap_list_find(&wm->addonconf->keymaps, km->idname, km->spaceid, km->regionid);
		usermap = WM_keymap_list_find(&U.user_keymaps, km->idname, km->spaceid, km->regionid);

		wm_user_modal_keymap_set_items(wm, defaultmap);

		/* add */
		kmn = wm_keymap_patch_update(&wm->userconf->keymaps, defaultmap, addonmap, usermap);

		if (kmn) {
			kmn->modal_items = km->modal_items;
			kmn->poll = km->poll;
		}

		/* in case of old non-diff keymaps, force extra update to create diffs */
		compat_update = compat_update || (usermap && !(usermap->flag & KEYMAP_DIFF));
	}

	WM_KEYMAP_UPDATE = 0;

	if (compat_update) {
		WM_keyconfig_update_tag(NULL, NULL);
		WM_keyconfig_update(wm);
	}
}

/********************************* Event Handling *****************************
 * Handlers have pointers to the keymap in the default configuration. During
 * event handling this function is called to get the keymap from the final
 * configuration. */

wmKeyMap *WM_keymap_active(wmWindowManager *wm, wmKeyMap *keymap)
{
	wmKeyMap *km;

	if (!keymap)
		return NULL;
	
	/* first user defined keymaps */
	km = WM_keymap_list_find(&wm->userconf->keymaps, keymap->idname, keymap->spaceid, keymap->regionid);

	if (km)
		return km;

	return keymap;
}

/******************************* Keymap Editor ********************************
 * In the keymap editor the user key configuration is edited. */

void WM_keymap_restore_item_to_default(bContext *C, wmKeyMap *keymap, wmKeyMapItem *kmi)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmKeyMap *defaultmap, *addonmap;
	wmKeyMapItem *orig;

	if (!keymap)
		return;

	/* construct default keymap from preset + addons */
	defaultmap = wm_keymap_preset(wm, keymap);
	addonmap = WM_keymap_list_find(&wm->addonconf->keymaps, keymap->idname, keymap->spaceid, keymap->regionid);

	if (addonmap) {
		defaultmap = wm_keymap_copy(defaultmap);
		wm_keymap_addon_add(defaultmap, addonmap);
	}

	/* find original item */
	orig = WM_keymap_item_find_id(defaultmap, kmi->id);

	if (orig) {
		/* restore to original */
		if (strcmp(orig->idname, kmi->idname) != 0) {
			BLI_strncpy(kmi->idname, orig->idname, sizeof(kmi->idname));
			WM_keymap_properties_reset(kmi, NULL);
		}

		if (orig->properties) {
			if (kmi->properties) {
				IDP_FreeProperty(kmi->properties);
				MEM_freeN(kmi->properties);
				kmi->properties = NULL;
			}

			kmi->properties = IDP_CopyProperty(orig->properties);
			kmi->ptr->data = kmi->properties;
		}

		kmi->propvalue = orig->propvalue;
		kmi->type = orig->type;
		kmi->val = orig->val;
		kmi->shift = orig->shift;
		kmi->ctrl = orig->ctrl;
		kmi->alt = orig->alt;
		kmi->oskey = orig->oskey;
		kmi->keymodifier = orig->keymodifier;
		kmi->maptype = orig->maptype;

		WM_keyconfig_update_tag(keymap, kmi);
	}

	/* free temporary keymap */
	if (addonmap) {
		WM_keymap_free(defaultmap);
		MEM_freeN(defaultmap);
	}
}

void WM_keymap_restore_to_default(wmKeyMap *keymap, bContext *C)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmKeyMap *usermap;

	/* remove keymap from U.user_keymaps and update */
	usermap = WM_keymap_list_find(&U.user_keymaps, keymap->idname, keymap->spaceid, keymap->regionid);

	if (usermap) {
		WM_keymap_free(usermap);
		BLI_freelinkN(&U.user_keymaps, usermap);

		WM_keyconfig_update_tag(NULL, NULL);
		WM_keyconfig_update(wm);
	}
}

wmKeyMapItem *WM_keymap_item_find_id(wmKeyMap *keymap, int id)
{
	wmKeyMapItem *kmi;
	
	for (kmi = keymap->items.first; kmi; kmi = kmi->next) {
		if (kmi->id == id) {
			return kmi;
		}
	}
	
	return NULL;
}

/* Guess an appropriate keymap from the operator name */
/* Needs to be kept up to date with Keymap and Operator naming */
wmKeyMap *WM_keymap_guess_opname(const bContext *C, const char *opname)
{
	wmKeyMap *km = NULL;
	SpaceLink *sl = CTX_wm_space_data(C);
	
	/* Window */
	if (strstr(opname, "WM_OT")) {
		km = WM_keymap_find_all(C, "Window", 0, 0);
	}
	/* Screen */
	else if (strstr(opname, "SCREEN_OT")) {
		km = WM_keymap_find_all(C, "Screen", 0, 0);
	}
	/* Grease Pencil */
	else if (strstr(opname, "GPENCIL_OT")) {
		km = WM_keymap_find_all(C, "Grease Pencil", 0, 0);
	}
	/* Markers */
	else if (strstr(opname, "MARKER_OT")) {
		km = WM_keymap_find_all(C, "Markers", 0, 0);
	}
	/* Import/Export*/
	else if (strstr(opname, "IMPORT_") || strstr(opname, "EXPORT_")) {
		km = WM_keymap_find_all(C, "Window", 0, 0);
	}
	
	
	/* 3D View */
	else if (strstr(opname, "VIEW3D_OT")) {
		km = WM_keymap_find_all(C, "3D View", sl->spacetype, 0);
	}
	else if (strstr(opname, "OBJECT_OT")) {
		km = WM_keymap_find_all(C, "Object Mode", 0, 0);
	}

	
	/* Editing Modes */
	else if (strstr(opname, "MESH_OT")) {
		km = WM_keymap_find_all(C, "Mesh", 0, 0);
		
		/* some mesh operators are active in object mode too, like add-prim */
		if (km && km->poll && km->poll((bContext *)C) == 0) {
			km = WM_keymap_find_all(C, "Object Mode", 0, 0);
		}
	}
	else if (strstr(opname, "CURVE_OT")) {
		km = WM_keymap_find_all(C, "Curve", 0, 0);
		
		/* some curve operators are active in object mode too, like add-prim */
		if (km && km->poll && km->poll((bContext *)C) == 0) {
			km = WM_keymap_find_all(C, "Object Mode", 0, 0);
		}
	}
	else if (strstr(opname, "ARMATURE_OT")) {
		km = WM_keymap_find_all(C, "Armature", 0, 0);
	}
	else if (strstr(opname, "POSE_OT")) {
		km = WM_keymap_find_all(C, "Pose", 0, 0);
	}
	else if (strstr(opname, "SCULPT_OT")) {
		switch (CTX_data_mode_enum(C)) {
			case OB_MODE_SCULPT:
				km = WM_keymap_find_all(C, "Sculpt", 0, 0);
				break;
			case OB_MODE_EDIT:
				km = WM_keymap_find_all(C, "UV Sculpt", 0, 0);
				break;
		}
	}
	else if (strstr(opname, "MBALL_OT")) {
		km = WM_keymap_find_all(C, "Metaball", 0, 0);
		
		/* some mball operators are active in object mode too, like add-prim */
		if (km && km->poll && km->poll((bContext *)C) == 0) {
			km = WM_keymap_find_all(C, "Object Mode", 0, 0);
		}
	}
	else if (strstr(opname, "LATTICE_OT")) {
		km = WM_keymap_find_all(C, "Lattice", 0, 0);
	}
	else if (strstr(opname, "PARTICLE_OT")) {
		km = WM_keymap_find_all(C, "Particle", 0, 0);
	}
	else if (strstr(opname, "FONT_OT")) {
		km = WM_keymap_find_all(C, "Font", 0, 0);
	}
	else if (strstr(opname, "PAINT_OT")) {
		
		/* check for relevant mode */
		switch (CTX_data_mode_enum(C)) {
			case OB_MODE_WEIGHT_PAINT:
				km = WM_keymap_find_all(C, "Weight Paint", 0, 0);
				break;
			case OB_MODE_VERTEX_PAINT:
				km = WM_keymap_find_all(C, "Vertex Paint", 0, 0);
				break;
			case OB_MODE_TEXTURE_PAINT:
				km = WM_keymap_find_all(C, "Image Paint", 0, 0);
				break;
		}
	}
	/* Paint Face Mask */
	else if (strstr(opname, "PAINT_OT_face_select")) {
		km = WM_keymap_find_all(C, "Face Mask", sl->spacetype, 0);
	}
	/* Timeline */
	else if (strstr(opname, "TIME_OT")) {
		km = WM_keymap_find_all(C, "Timeline", sl->spacetype, 0);
	}
	/* Image Editor */
	else if (strstr(opname, "IMAGE_OT")) {
		km = WM_keymap_find_all(C, "Image", sl->spacetype, 0);
	}
	/* UV Editor */
	else if (strstr(opname, "UV_OT")) {
		km = WM_keymap_find_all(C, "UV Editor", sl->spacetype, 0);
	}
	/* Node Editor */
	else if (strstr(opname, "NODE_OT")) {
		km = WM_keymap_find_all(C, "Node Editor", sl->spacetype, 0);
	}
	/* Animation Editor Channels */
	else if (strstr(opname, "ANIM_OT_channels")) {
		km = WM_keymap_find_all(C, "Animation Channels", sl->spacetype, 0);
	}
	/* Animation Generic - after channels */
	else if (strstr(opname, "ANIM_OT")) {
		km = WM_keymap_find_all(C, "Animation", 0, 0);
	}
	/* Graph Editor */
	else if (strstr(opname, "GRAPH_OT")) {
		km = WM_keymap_find_all(C, "Graph Editor", sl->spacetype, 0);
	}
	/* Dopesheet Editor */
	else if (strstr(opname, "ACTION_OT")) {
		km = WM_keymap_find_all(C, "Dopesheet", sl->spacetype, 0);
	}
	/* NLA Editor */
	else if (strstr(opname, "NLA_OT")) {
		km = WM_keymap_find_all(C, "NLA Editor", sl->spacetype, 0);
	}
	/* Script */
	else if (strstr(opname, "SCRIPT_OT")) {
		km = WM_keymap_find_all(C, "Script", sl->spacetype, 0);
	}
	/* Text */
	else if (strstr(opname, "TEXT_OT")) {
		km = WM_keymap_find_all(C, "Text", sl->spacetype, 0);
	}
	/* Sequencer */
	else if (strstr(opname, "SEQUENCER_OT")) {
		km = WM_keymap_find_all(C, "Sequencer", sl->spacetype, 0);
	}
	/* Console */
	else if (strstr(opname, "CONSOLE_OT")) {
		km = WM_keymap_find_all(C, "Console", sl->spacetype, 0);
	}
	/* Console */
	else if (strstr(opname, "INFO_OT")) {
		km = WM_keymap_find_all(C, "Info", sl->spacetype, 0);
	}
	
	/* Transform */
	else if (strstr(opname, "TRANSFORM_OT")) {
		
		/* check for relevant editor */
		switch (sl->spacetype) {
			case SPACE_VIEW3D:
				km = WM_keymap_find_all(C, "3D View", sl->spacetype, 0);
				break;
			case SPACE_IPO:
				km = WM_keymap_find_all(C, "Graph Editor", sl->spacetype, 0);
				break;
			case SPACE_ACTION:
				km = WM_keymap_find_all(C, "Dopesheet", sl->spacetype, 0);
				break;
			case SPACE_NLA:
				km = WM_keymap_find_all(C, "NLA Editor", sl->spacetype, 0);
				break;
			case SPACE_IMAGE:
				km = WM_keymap_find_all(C, "UV Editor", sl->spacetype, 0);
				break;
			case SPACE_NODE:
				km = WM_keymap_find_all(C, "Node Editor", sl->spacetype, 0);
				break;
			case SPACE_SEQ:
				km = WM_keymap_find_all(C, "Sequencer", sl->spacetype, 0);
				break;
		}
	}
	
	return km;
}

