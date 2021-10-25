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
 *
 * Configurable key-maps - add/remove/find/compare/patch...
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
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_screen.h"

#include "BLT_translation.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"
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
	else {
		kmin->properties = NULL;
		kmin->ptr = NULL;
	}

	return kmin;
}

static void wm_keymap_item_free(wmKeyMapItem *kmi)
{
	/* not kmi itself */
	if (kmi->ptr) {
		WM_operator_properties_free(kmi->ptr);
		MEM_freeN(kmi->ptr);
		kmi->ptr = NULL;
		kmi->properties = NULL;
	}
}

static void wm_keymap_item_properties_set(wmKeyMapItem *kmi)
{
	WM_operator_properties_alloc(&(kmi->ptr), &(kmi->properties), kmi->idname);
	WM_operator_properties_sanitize(kmi->ptr, 1);
}

/**
 * Similar to #wm_keymap_item_properties_set but checks for the wmOperatorType having changed, see [#38042]
 */
static void wm_keymap_item_properties_update_ot(wmKeyMapItem *kmi)
{
	if (kmi->idname[0] == 0) {
		BLI_assert(kmi->ptr == NULL);
		return;
	}

	if (kmi->ptr == NULL) {
		wm_keymap_item_properties_set(kmi);
	}
	else {
		wmOperatorType *ot = WM_operatortype_find(kmi->idname, 0);
		if (ot) {
			if (ot->srna != kmi->ptr->type) {
				/* matches wm_keymap_item_properties_set but doesnt alloc new ptr */
				WM_operator_properties_create_ptr(kmi->ptr, ot);
				/* 'kmi->ptr->data' NULL'd above, keep using existing properties.
				 * Note: the operators property types may have changed,
				 * we will need a more comprehensive sanitize function to support this properly.
				 */
				if (kmi->properties) {
					kmi->ptr->data = kmi->properties;
				}
				WM_operator_properties_sanitize(kmi->ptr, 1);
			}
		}
		else {
			/* zombie keymap item */
			wm_keymap_item_free(kmi);
		}
	}
}

static void wm_keyconfig_properties_update_ot(ListBase *km_lb)
{
	wmKeyMap *km;
	wmKeyMapItem *kmi;

	for (km = km_lb->first; km; km = km->next) {
		wmKeyMapDiffItem *kmdi;

		for (kmi = km->items.first; kmi; kmi = kmi->next) {
			wm_keymap_item_properties_update_ot(kmi);
		}

		for (kmdi = km->diff_items.first; kmdi; kmdi = kmdi->next) {
			if (kmdi->add_item)
				wm_keymap_item_properties_update_ot(kmdi->add_item);
			if (kmdi->remove_item)
				wm_keymap_item_properties_update_ot(kmdi->remove_item);
		}
	}
}

static bool wm_keymap_item_equals_result(wmKeyMapItem *a, wmKeyMapItem *b)
{
	return (STREQ(a->idname, b->idname) &&
	        RNA_struct_equals(a->ptr, b->ptr, RNA_EQ_UNSET_MATCH_NONE) &&
	        (a->flag & KMI_INACTIVE) == (b->flag & KMI_INACTIVE) &&
	        a->propvalue == b->propvalue);
}

static bool wm_keymap_item_equals(wmKeyMapItem *a, wmKeyMapItem *b)
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
	if (LIKELY(kmi->ptr)) {
		WM_operator_properties_free(kmi->ptr);
		MEM_freeN(kmi->ptr);

		kmi->ptr = NULL;
	}

	kmi->properties = properties;

	wm_keymap_item_properties_set(kmi);
}

int WM_keymap_map_type_get(wmKeyMapItem *kmi)
{
	if (ISTIMER(kmi->type)) {
		return KMI_TYPE_TIMER;
	}
	if (ISKEYBOARD(kmi->type)) {
		return KMI_TYPE_KEYBOARD;
	}
	if (ISTWEAK(kmi->type)) {
		return KMI_TYPE_TWEAK;
	}
	if (ISMOUSE(kmi->type)) {
		return KMI_TYPE_MOUSE;
	}
	if (ISNDOF(kmi->type)) {
		return KMI_TYPE_NDOF;
	}
	if (kmi->type == KM_TEXTINPUT) {
		return KMI_TYPE_TEXTINPUT;
	}
	if (ELEM(kmi->type, TABLET_STYLUS, TABLET_ERASER)) {
		return KMI_TYPE_MOUSE;
	}
	return KMI_TYPE_KEYBOARD;
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

bool WM_keyconfig_remove(wmWindowManager *wm, wmKeyConfig *keyconf)
{
	if (BLI_findindex(&wm->keyconfigs, keyconf) != -1) {
		if (STREQLEN(U.keyconfigstr, keyconf->idname, sizeof(U.keyconfigstr))) {
			BLI_strncpy(U.keyconfigstr, wm->defaultconf->idname, sizeof(U.keyconfigstr));
			WM_keyconfig_update_tag(NULL, NULL);
		}

		BLI_remlink(&wm->keyconfigs, keyconf);
		WM_keyconfig_free(keyconf);

		return true;
	}
	else {
		return false;
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

static wmKeyConfig *WM_keyconfig_active(wmWindowManager *wm)
{
	wmKeyConfig *keyconf;

	/* first try from preset */
	keyconf = BLI_findstring(&wm->keyconfigs, U.keyconfigstr, offsetof(wmKeyConfig, idname));
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
	BLI_listbase_clear(&keymapn->items);
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

bool WM_keymap_remove(wmKeyConfig *keyconf, wmKeyMap *keymap)
{
	if (BLI_findindex(&keyconf->keymaps, keymap) != -1) {

		WM_keymap_free(keymap);
		BLI_remlink(&keyconf->keymaps, keymap);
		MEM_freeN(keymap);

		return true;
	}
	else {
		return false;
	}
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
		kmi->shift = (modifier & KM_SHIFT) ? KM_MOD_FIRST : ((modifier & KM_SHIFT2) ? KM_MOD_SECOND : false);
		kmi->ctrl =  (modifier & KM_CTRL)  ? KM_MOD_FIRST : ((modifier & KM_CTRL2)  ? KM_MOD_SECOND : false);
		kmi->alt =   (modifier & KM_ALT)   ? KM_MOD_FIRST : ((modifier & KM_ALT2)   ? KM_MOD_SECOND : false);
		kmi->oskey = (modifier & KM_OSKEY) ? KM_MOD_FIRST : ((modifier & KM_OSKEY2) ? KM_MOD_SECOND : false);
	}
}

static void keymap_item_set_id(wmKeyMap *keymap, wmKeyMapItem *kmi)
{
	keymap->kmi_id++;
	if ((keymap->flag & KEYMAP_USER) == 0) {
		kmi->id = keymap->kmi_id;
	}
	else {
		kmi->id = -keymap->kmi_id; /* User defined keymap entries have negative ids */
	}
}

/* if item was added, then bail out */
wmKeyMapItem *WM_keymap_verify_item(wmKeyMap *keymap, const char *idname, int type, int val, int modifier, int keymodifier)
{
	wmKeyMapItem *kmi;
	
	for (kmi = keymap->items.first; kmi; kmi = kmi->next)
		if (STREQLEN(kmi->idname, idname, OP_MAX_TYPENAME))
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

wmKeyMapItem *WM_keymap_add_menu_pie(wmKeyMap *keymap, const char *idname, int type, int val, int modifier, int keymodifier)
{
	wmKeyMapItem *kmi = WM_keymap_add_item(keymap, "WM_OT_call_menu_pie", type, val, modifier, keymodifier);
	RNA_string_set(kmi->ptr, "name", idname);
	return kmi;
}

bool WM_keymap_remove_item(wmKeyMap *keymap, wmKeyMapItem *kmi)
{
	if (BLI_findindex(&keymap->items, kmi) != -1) {
		if (kmi->ptr) {
			WM_operator_properties_free(kmi->ptr);
			MEM_freeN(kmi->ptr);
		}
		BLI_freelinkN(&keymap->items, kmi);

		WM_keyconfig_update_tag(keymap, NULL);
		return true;
	}
	else {
		return false;
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

			if (!orig_kmi && addon_km)
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
			/* Do not re-add an already existing keymap item! See T42088. */
			/* We seek only for exact copy here! See T42137. */
			kmi_add = wm_keymap_find_item_equals(km, kmdi->add_item);

			/* If kmi_add is same as kmi_remove (can happen in some cases, typically when we got kmi_remove
			 * from wm_keymap_find_item_equals_result()), no need to add or remove anything, see T45579. */
			/* Note: This typically happens when we apply user-defined keymap diff to a base one that was exported
			 *       with that customized keymap already. In that case:
			 *         - wm_keymap_find_item_equals(km, kmdi->remove_item) finds nothing (because actual shortcut of
			 *           current base does not match kmdi->remove_item any more).
			 *         - wm_keymap_find_item_equals_result(km, kmdi->remove_item) finds the current kmi from
			 *           base keymap (because it does exactly the same thing).
			 *         - wm_keymap_find_item_equals(km, kmdi->add_item) finds the same kmi, since base keymap was
			 *           exported with that user-defined shortcut already!
			 *       Maybe we should rather keep user-defined keymaps specific to a given base one? */
			if (kmi_add != NULL && kmi_add == kmi_remove) {
				kmi_remove = NULL;
			}
			/* only if nothing to remove or item to remove found */
			else if (!kmi_add && (!kmdi->remove_item || kmi_remove)) {
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
		/* for compatibility with old user preferences with non-diff
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
			if (STREQLEN(idname, km->idname, KMAP_MAX_NAME))
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
		if (wm->defaultconf) {
			wmKeyMap *defaultkm = WM_keymap_list_find(&wm->defaultconf->keymaps, km->idname, 0, 0);

			if (defaultkm) {
				km->modal_items = defaultkm->modal_items;
				km->poll = defaultkm->poll;
			}
		}
	}
	
	return km;
}

wmKeyMap *WM_modalkeymap_get(wmKeyConfig *keyconf, const char *idname)
{
	wmKeyMap *km;
	
	for (km = keyconf->keymaps.first; km; km = km->next)
		if (km->flag & KEYMAP_MODAL)
			if (STREQLEN(idname, km->idname, KMAP_MAX_NAME))
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

static wmKeyMapItem *wm_modalkeymap_find_propvalue_iter(wmKeyMap *km, wmKeyMapItem *kmi, const int propvalue)
{
	if (km->flag & KEYMAP_MODAL) {
		kmi = kmi ? kmi->next : km->items.first;
		for (; kmi; kmi = kmi->next) {
			if (kmi->propvalue == propvalue) {
				return kmi;
			}
		}
	}
	else {
		BLI_assert(!"called with non modal keymap");
	}

	return NULL;
}

wmKeyMapItem *WM_modalkeymap_find_propvalue(wmKeyMap *km, const int propvalue)
{
	return wm_modalkeymap_find_propvalue_iter(km, NULL, propvalue);
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
		if (wm->defaultconf == NULL) {
			return;
		}

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

const char *WM_key_event_string(const short type, const bool compact)
{
	EnumPropertyItem *it;
	const int i = RNA_enum_from_value(rna_enum_event_type_items, (int)type);

	if (i == -1) {
		return "";
	}
	it = &rna_enum_event_type_items[i];

	/* We first try enum items' description (abused as shortname here), and fall back to usual name if empty. */
	if (compact && it->description[0]) {
		/* XXX No context for enum descriptions... In practice shall not be an issue though. */
		return IFACE_(it->description);
	}

	return CTX_IFACE_(BLT_I18NCONTEXT_UI_EVENTS, it->name);
}

/* TODO: also support (some) value, like e.g. double-click? */
int WM_keymap_item_raw_to_string(
        const short shift, const short ctrl, const short alt, const short oskey,
        const short keymodifier, const short val, const short type, const bool compact,
        const int len, char *r_str)
{
#define ADD_SEP if (p != buf) *p++ = ' '; (void)0

	char buf[128];
	char *p = buf;

	buf[0] = '\0';

	/* TODO: support order (KM_SHIFT vs. KM_SHIFT2) ? */
	if (shift == KM_ANY &&
	    ctrl == KM_ANY &&
	    alt == KM_ANY &&
	    oskey == KM_ANY)
	{
		/* make it implicit in case of compact result expected. */
		if (!compact) {
			ADD_SEP;
			p += BLI_strcpy_rlen(p, IFACE_("Any"));
		}
	}
	else {
		if (shift) {
			ADD_SEP;
			p += BLI_strcpy_rlen(p, IFACE_("Shift"));
		}

		if (ctrl) {
			ADD_SEP;
			p += BLI_strcpy_rlen(p, IFACE_("Ctrl"));
		}

		if (alt) {
			ADD_SEP;
			p += BLI_strcpy_rlen(p, IFACE_("Alt"));
		}

		if (oskey) {
			ADD_SEP;
			p += BLI_strcpy_rlen(p, IFACE_("Cmd"));
		}
	}

	if (keymodifier) {
		ADD_SEP;
		p += BLI_strcpy_rlen(p, WM_key_event_string(keymodifier, compact));
	}

	if (type) {
		ADD_SEP;
		if (val == KM_DBL_CLICK) {
			p += BLI_strcpy_rlen(p, IFACE_("dbl-"));
		}
		p += BLI_strcpy_rlen(p, WM_key_event_string(type, compact));
	}

	/* We assume size of buf is enough to always store any possible shortcut, but let's add a debug check about it! */
	BLI_assert(p - buf < sizeof(buf));

	/* We need utf8 here, otherwise we may 'cut' some unicode chars like arrows... */
	return BLI_strncpy_utf8_rlen(r_str, buf, len);

#undef ADD_SEP
}

int WM_keymap_item_to_string(wmKeyMapItem *kmi, const bool compact, const int len, char *r_str)
{
	return WM_keymap_item_raw_to_string(
	            kmi->shift, kmi->ctrl, kmi->alt, kmi->oskey, kmi->keymodifier, kmi->val, kmi->type,
	            compact, len, r_str);
}

int WM_modalkeymap_items_to_string(
        wmKeyMap *km, const int propvalue, const bool compact, const int len, char *r_str)
{
	int totlen = 0;
	bool add_sep = false;

	if (km) {
		wmKeyMapItem *kmi;

		/* Find all shortcuts related to that propvalue! */
		for (kmi = WM_modalkeymap_find_propvalue(km, propvalue);
		     kmi && totlen < (len - 2);
		     kmi = wm_modalkeymap_find_propvalue_iter(km, kmi, propvalue))
		{
			if (add_sep) {
				r_str[totlen++] = '/';
				r_str[totlen] = '\0';
			}
			else {
				add_sep = true;
			}
			totlen += WM_keymap_item_to_string(kmi, compact, len - totlen, &r_str[totlen]);
		}
	}

	return totlen;
}

int WM_modalkeymap_operator_items_to_string(
        wmOperatorType *ot, const int propvalue, const bool compact, const int len, char *r_str)
{
	return WM_modalkeymap_items_to_string(ot->modalkeymap, propvalue, compact, len, r_str);
}

char *WM_modalkeymap_operator_items_to_string_buf(
        wmOperatorType *ot, const int propvalue, const bool compact,
        const int max_len, int *r_available_len, char **r_str)
{
	char *ret = *r_str;

	if (*r_available_len > 1) {
		int used_len = WM_modalkeymap_operator_items_to_string(
		                   ot, propvalue, compact, min_ii(*r_available_len, max_len), ret) + 1;

		*r_available_len -= used_len;
		*r_str += used_len;
		if (*r_available_len == 0) {
			(*r_str)--;  /* So that *str keeps pointing on a valid char, we'll stay on it anyway. */
		}
	}
	else {
		*ret = '\0';
	}

	return ret;
}

static wmKeyMapItem *wm_keymap_item_find_handlers(
        const bContext *C, ListBase *handlers, const char *opname, int UNUSED(opcontext),
        IDProperty *properties, const bool is_strict, const bool is_hotkey,
        wmKeyMap **r_keymap)
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
				/* skip disabled keymap items [T38447] */
				if (kmi->flag & KMI_INACTIVE)
					continue;
				
				if (STREQ(kmi->idname, opname) && WM_key_event_string(kmi->type, false)[0]) {
					if (is_hotkey) {
						if (!ISHOTKEY(kmi->type))
							continue;
					}

					if (properties) {
						/* example of debugging keymaps */
#if 0
						if (kmi->ptr) {
							if (STREQ("MESH_OT_rip_move", opname)) {
								printf("OPERATOR\n");
								IDP_spit(properties);
								printf("KEYMAP\n");
								IDP_spit(kmi->ptr->data);
							}
						}
#endif

						if (kmi->ptr && IDP_EqualsProperties_ex(properties, kmi->ptr->data, is_strict)) {
							if (r_keymap) *r_keymap = keymap;
							return kmi;
						}
						/* Debug only, helps spotting mismatches between menu entries and shortcuts! */
						else if (G.debug & G_DEBUG_WM) {
							if (is_strict && kmi->ptr) {
								wmOperatorType *ot = WM_operatortype_find(opname, true);
								if (ot) {
									/* make a copy of the properties and set unset ones to their default values. */
									PointerRNA opptr;
									IDProperty *properties_default = IDP_CopyProperty(kmi->ptr->data);

									RNA_pointer_create(NULL, ot->srna, properties_default, &opptr);
									WM_operator_properties_default(&opptr, true);

									if (IDP_EqualsProperties_ex(properties, properties_default, is_strict)) {
										char kmi_str[128];
										WM_keymap_item_to_string(kmi, false, sizeof(kmi_str), kmi_str);
										/* Note gievn properties could come from other things than menu entry... */
										printf("%s: Some set values in menu entry match default op values, "
										       "this might not be desired!\n", opname);
										printf("\tkm: '%s', kmi: '%s'\n", keymap->idname, kmi_str);
#ifndef NDEBUG
#ifdef WITH_PYTHON
										printf("OPERATOR\n");
										IDP_spit(properties);
										printf("KEYMAP\n");
										IDP_spit(kmi->ptr->data);
#endif
#endif
										printf("\n");
									}

									IDP_FreeProperty(properties_default);
									MEM_freeN(properties_default);
								}
							}
						}
					}
					else {
						if (r_keymap) *r_keymap = keymap;
						return kmi;
					}
				}
			}
		}
	}
	
	/* ensure un-initialized keymap is never used */
	if (r_keymap) *r_keymap = NULL;
	return NULL;
}

static wmKeyMapItem *wm_keymap_item_find_props(
        const bContext *C, const char *opname, int opcontext,
        IDProperty *properties, const bool is_strict, const bool is_hotkey,
        wmKeyMap **r_keymap)
{
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	wmKeyMapItem *found = NULL;

	/* look into multiple handler lists to find the item */
	if (win)
		found = wm_keymap_item_find_handlers(C, &win->handlers, opname, opcontext, properties, is_strict, is_hotkey, r_keymap);

	if (sa && found == NULL)
		found = wm_keymap_item_find_handlers(C, &sa->handlers, opname, opcontext, properties, is_strict, is_hotkey, r_keymap);

	if (found == NULL) {
		if (ELEM(opcontext, WM_OP_EXEC_REGION_WIN, WM_OP_INVOKE_REGION_WIN)) {
			if (sa) {
				if (!(ar && ar->regiontype == RGN_TYPE_WINDOW))
					ar = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
				
				if (ar)
					found = wm_keymap_item_find_handlers(C, &ar->handlers, opname, opcontext, properties, is_strict, is_hotkey, r_keymap);
			}
		}
		else if (ELEM(opcontext, WM_OP_EXEC_REGION_CHANNELS, WM_OP_INVOKE_REGION_CHANNELS)) {
			if (!(ar && ar->regiontype == RGN_TYPE_CHANNELS))
				ar = BKE_area_find_region_type(sa, RGN_TYPE_CHANNELS);

			if (ar)
				found = wm_keymap_item_find_handlers(C, &ar->handlers, opname, opcontext, properties, is_strict, is_hotkey, r_keymap);
		}
		else if (ELEM(opcontext, WM_OP_EXEC_REGION_PREVIEW, WM_OP_INVOKE_REGION_PREVIEW)) {
			if (!(ar && ar->regiontype == RGN_TYPE_PREVIEW))
				ar = BKE_area_find_region_type(sa, RGN_TYPE_PREVIEW);

			if (ar)
				found = wm_keymap_item_find_handlers(C, &ar->handlers, opname, opcontext, properties, is_strict, is_hotkey, r_keymap);
		}
		else {
			if (ar)
				found = wm_keymap_item_find_handlers(C, &ar->handlers, opname, opcontext, properties, is_strict, is_hotkey, r_keymap);
		}
	}

	return found;
}

static wmKeyMapItem *wm_keymap_item_find(
        const bContext *C, const char *opname, int opcontext,
        IDProperty *properties, const bool is_hotkey, bool is_strict,
        wmKeyMap **r_keymap)
{
	wmKeyMapItem *found;

	/* XXX Hack! Macro operators in menu entry have their whole props defined, which is not the case for
	 *     relevant keymap entries. Could be good to check and harmonize this, but for now always
	 *     compare non-strict in this case.
	 */
	wmOperatorType *ot = WM_operatortype_find(opname, true);
	if (ot) {
		is_strict = is_strict && ((ot->flag & OPTYPE_MACRO) == 0);
	}

	found = wm_keymap_item_find_props(C, opname, opcontext, properties, is_strict, is_hotkey, r_keymap);

	/* This block is *only* useful in one case: when op uses an enum menu in its prop member
	 * (then, we want to rerun a comparison with that 'prop' unset). Note this remains brittle,
	 * since now any enum prop may be used in UI (specified by name), ot->prop is not so much used...
	 * Otherwise:
	 *     * If non-strict, unset properties always match set ones in IDP_EqualsProperties_ex.
	 *     * If strict, unset properties never match set ones in IDP_EqualsProperties_ex,
	 *       and we do not want that to change (else we get things like T41757)!
	 * ...so in either case, re-running a comparison with unset props set to default is useless.
	 */
	if (!found && properties) {
		if (ot && ot->prop) {  /* XXX Shall we also check ot->prop is actually an enum? */
			/* make a copy of the properties and unset the 'ot->prop' one if set. */
			PointerRNA opptr;
			IDProperty *properties_temp = IDP_CopyProperty(properties);

			RNA_pointer_create(NULL, ot->srna, properties_temp, &opptr);

			if (RNA_property_is_set(&opptr, ot->prop)) {
				/* for operator that has enum menu, unset it so its value does not affect comparison result */
				RNA_property_unset(&opptr, ot->prop);

				found = wm_keymap_item_find_props(C, opname, opcontext, properties_temp,
				                                  is_strict, is_hotkey, r_keymap);
			}

			IDP_FreeProperty(properties_temp);
			MEM_freeN(properties_temp);
		}
	}

	/* Debug only, helps spotting mismatches between menu entries and shortcuts! */
	if (G.debug & G_DEBUG_WM) {
		if (!found && is_strict && properties) {
			wmKeyMap *km;
			wmKeyMapItem *kmi;
			if (ot) {
				/* make a copy of the properties and set unset ones to their default values. */
				PointerRNA opptr;
				IDProperty *properties_default = IDP_CopyProperty(properties);

				RNA_pointer_create(NULL, ot->srna, properties_default, &opptr);
				WM_operator_properties_default(&opptr, true);

				kmi = wm_keymap_item_find_props(C, opname, opcontext, properties_default, is_strict, is_hotkey, &km);
				if (kmi) {
					char kmi_str[128];
					WM_keymap_item_to_string(kmi, false, sizeof(kmi_str), kmi_str);
					printf("%s: Some set values in keymap entry match default op values, "
					       "this might not be desired!\n", opname);
					printf("\tkm: '%s', kmi: '%s'\n", km->idname, kmi_str);
#ifndef NDEBUG
#ifdef WITH_PYTHON
					printf("OPERATOR\n");
					IDP_spit(properties);
					printf("KEYMAP\n");
					IDP_spit(kmi->ptr->data);
#endif
#endif
					printf("\n");
				}

				IDP_FreeProperty(properties_default);
				MEM_freeN(properties_default);
			}
		}
	}

	return found;
}

char *WM_key_event_operator_string(
        const bContext *C, const char *opname, int opcontext,
        IDProperty *properties, const bool is_strict, int len, char *r_str)
{
	wmKeyMapItem *kmi = wm_keymap_item_find(C, opname, opcontext, properties, false, is_strict, NULL);
	
	if (kmi) {
		WM_keymap_item_to_string(kmi, false, len, r_str);
		return r_str;
	}

	return NULL;
}

wmKeyMapItem *WM_key_event_operator(
        const bContext *C, const char *opname, int opcontext,
        IDProperty *properties, const bool is_hotkey,
        wmKeyMap **r_keymap)
{
	return wm_keymap_item_find(C, opname, opcontext, properties, is_hotkey, true, r_keymap);
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
		if (k1->val == KM_CLICK && ELEM(k2->val, KM_PRESS, KM_RELEASE, KM_CLICK) == 0)
			return 0;
		if (k2->val == KM_CLICK && ELEM(k1->val, KM_PRESS, KM_RELEASE, KM_CLICK) == 0)
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

/* so operator removal can trigger update */
enum {
	WM_KEYMAP_UPDATE_RECONFIGURE    = (1 << 0),

	/* ensure all wmKeyMap have their operator types validated after removing an operator */
	WM_KEYMAP_UPDATE_OPERATORTYPE   = (1 << 1),
};

static char wm_keymap_update_flag = 0;

void WM_keyconfig_update_tag(wmKeyMap *km, wmKeyMapItem *kmi)
{
	/* quick tag to do delayed keymap updates */
	wm_keymap_update_flag |= WM_KEYMAP_UPDATE_RECONFIGURE;

	if (km)
		km->flag |= KEYMAP_UPDATE;
	if (kmi)
		kmi->flag |= KMI_UPDATE;
}

void WM_keyconfig_update_operatortype(void)
{
	wm_keymap_update_flag |= WM_KEYMAP_UPDATE_OPERATORTYPE;
}

static bool wm_keymap_test_and_clear_update(wmKeyMap *km)
{
	wmKeyMapItem *kmi;
	int update;
	
	update = (km->flag & KEYMAP_UPDATE);
	km->flag &= ~KEYMAP_UPDATE;

	for (kmi = km->items.first; kmi; kmi = kmi->next) {
		update = update || (kmi->flag & KMI_UPDATE);
		kmi->flag &= ~KMI_UPDATE;
	}
	
	return (update != 0);
}

static wmKeyMap *wm_keymap_preset(wmWindowManager *wm, wmKeyMap *km)
{
	wmKeyConfig *keyconf = WM_keyconfig_active(wm);
	wmKeyMap *keymap;

	keymap = WM_keymap_list_find(&keyconf->keymaps, km->idname, km->spaceid, km->regionid);
	if (!keymap && wm->defaultconf)
		keymap = WM_keymap_list_find(&wm->defaultconf->keymaps, km->idname, km->spaceid, km->regionid);

	return keymap;
}

void WM_keyconfig_update(wmWindowManager *wm)
{
	wmKeyMap *km, *defaultmap, *addonmap, *usermap, *kmn;
	wmKeyMapItem *kmi;
	wmKeyMapDiffItem *kmdi;
	bool compat_update = false;

	if (G.background)
		return;

	if (wm_keymap_update_flag == 0)
		return;

	if (wm_keymap_update_flag & WM_KEYMAP_UPDATE_OPERATORTYPE) {
		/* an operatortype has been removed, this wont happen often
		 * but when it does we have to check _every_ keymap item */
		wmKeyConfig *kc;

		ListBase *keymaps_lb[] = {
		    &U.user_keymaps,
		    &wm->userconf->keymaps,
		    &wm->defaultconf->keymaps,
		    &wm->addonconf->keymaps,
		    NULL};

		int i;

		for (i = 0; keymaps_lb[i]; i++) {
			wm_keyconfig_properties_update_ot(keymaps_lb[i]);
		}

		for (kc = wm->keyconfigs.first; kc; kc = kc->next) {
			wm_keyconfig_properties_update_ot(&kc->keymaps);
		}

		wm_keymap_update_flag &= ~WM_KEYMAP_UPDATE_OPERATORTYPE;
	}


	if (wm_keymap_update_flag == 0)
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

	wm_keymap_update_flag &= ~WM_KEYMAP_UPDATE_RECONFIGURE;

	BLI_assert(wm_keymap_update_flag == 0);

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
		if (!STREQ(orig->idname, kmi->idname)) {
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
	/* Op types purposely skipped  for now:
	 *     BRUSH_OT
	 *     BOID_OT
	 *     BUTTONS_OT
	 *     CONSTRAINT_OT
	 *     PAINT_OT
	 *     ED_OT
	 *     FLUID_OT
	 *     TEXTURE_OT
	 *     UI_OT
	 *     VIEW2D_OT
	 *     WORLD_OT
	 */

	wmKeyMap *km = NULL;
	SpaceLink *sl = CTX_wm_space_data(C);
	
	/* Window */
	if (STRPREFIX(opname, "WM_OT")) {
		km = WM_keymap_find_all(C, "Window", 0, 0);
	}
	/* Screen & Render */
	else if (STRPREFIX(opname, "SCREEN_OT") ||
	         STRPREFIX(opname, "RENDER_OT") ||
	         STRPREFIX(opname, "SOUND_OT") ||
	         STRPREFIX(opname, "SCENE_OT"))
	{
		km = WM_keymap_find_all(C, "Screen", 0, 0);
	}
	/* Grease Pencil */
	else if (STRPREFIX(opname, "GPENCIL_OT")) {
		km = WM_keymap_find_all(C, "Grease Pencil", 0, 0);
	}
	/* Markers */
	else if (STRPREFIX(opname, "MARKER_OT")) {
		km = WM_keymap_find_all(C, "Markers", 0, 0);
	}
	/* Import/Export*/
	else if (STRPREFIX(opname, "IMPORT_") ||
	         STRPREFIX(opname, "EXPORT_"))
	{
		km = WM_keymap_find_all(C, "Window", 0, 0);
	}
	
	
	/* 3D View */
	else if (STRPREFIX(opname, "VIEW3D_OT")) {
		km = WM_keymap_find_all(C, "3D View", sl->spacetype, 0);
	}
	else if (STRPREFIX(opname, "OBJECT_OT")) {
		/* exception, this needs to work outside object mode too */
		if (STRPREFIX(opname, "OBJECT_OT_mode_set"))
			km = WM_keymap_find_all(C, "Object Non-modal", 0, 0);
		else
			km = WM_keymap_find_all(C, "Object Mode", 0, 0);
	}
	/* Object mode related */
	else if (STRPREFIX(opname, "GROUP_OT") ||
	         STRPREFIX(opname, "MATERIAL_OT") ||
	         STRPREFIX(opname, "PTCACHE_OT") ||
	         STRPREFIX(opname, "RIGIDBODY_OT"))
	{
		km = WM_keymap_find_all(C, "Object Mode", 0, 0);
	}
	
	/* Editing Modes */
	else if (STRPREFIX(opname, "MESH_OT")) {
		km = WM_keymap_find_all(C, "Mesh", 0, 0);
		
		/* some mesh operators are active in object mode too, like add-prim */
		if (km && km->poll && km->poll((bContext *)C) == 0) {
			km = WM_keymap_find_all(C, "Object Mode", 0, 0);
		}
	}
	else if (STRPREFIX(opname, "CURVE_OT") ||
	         STRPREFIX(opname, "SURFACE_OT"))
	{
		km = WM_keymap_find_all(C, "Curve", 0, 0);
		
		/* some curve operators are active in object mode too, like add-prim */
		if (km && km->poll && km->poll((bContext *)C) == 0) {
			km = WM_keymap_find_all(C, "Object Mode", 0, 0);
		}
	}
	else if (STRPREFIX(opname, "ARMATURE_OT") ||
	         STRPREFIX(opname, "SKETCH_OT"))
	{
		km = WM_keymap_find_all(C, "Armature", 0, 0);
	}
	else if (STRPREFIX(opname, "POSE_OT") ||
	         STRPREFIX(opname, "POSELIB_OT"))
	{
		km = WM_keymap_find_all(C, "Pose", 0, 0);
	}
	else if (STRPREFIX(opname, "SCULPT_OT")) {
		switch (CTX_data_mode_enum(C)) {
			case OB_MODE_SCULPT:
				km = WM_keymap_find_all(C, "Sculpt", 0, 0);
				break;
			case OB_MODE_EDIT:
				km = WM_keymap_find_all(C, "UV Sculpt", 0, 0);
				break;
		}
	}
	else if (STRPREFIX(opname, "MBALL_OT")) {
		km = WM_keymap_find_all(C, "Metaball", 0, 0);
		
		/* some mball operators are active in object mode too, like add-prim */
		if (km && km->poll && km->poll((bContext *)C) == 0) {
			km = WM_keymap_find_all(C, "Object Mode", 0, 0);
		}
	}
	else if (STRPREFIX(opname, "LATTICE_OT")) {
		km = WM_keymap_find_all(C, "Lattice", 0, 0);
	}
	else if (STRPREFIX(opname, "PARTICLE_OT")) {
		km = WM_keymap_find_all(C, "Particle", 0, 0);
	}
	else if (STRPREFIX(opname, "FONT_OT")) {
		km = WM_keymap_find_all(C, "Font", 0, 0);
	}
	/* Paint Face Mask */
	else if (STRPREFIX(opname, "PAINT_OT_face_select")) {
		km = WM_keymap_find_all(C, "Face Mask", 0, 0);
	}
	else if (STRPREFIX(opname, "PAINT_OT")) {
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
	/* Timeline */
	else if (STRPREFIX(opname, "TIME_OT")) {
		km = WM_keymap_find_all(C, "Timeline", sl->spacetype, 0);
	}
	/* Image Editor */
	else if (STRPREFIX(opname, "IMAGE_OT")) {
		km = WM_keymap_find_all(C, "Image", sl->spacetype, 0);
	}
	/* Clip Editor */
	else if (STRPREFIX(opname, "CLIP_OT")) {
		km = WM_keymap_find_all(C, "Clip", sl->spacetype, 0);
	}
	else if (STRPREFIX(opname, "MASK_OT")) {
		km = WM_keymap_find_all(C, "Mask Editing", 0, 0);
	}
	/* UV Editor */
	else if (STRPREFIX(opname, "UV_OT")) {
		/* Hack to allow using UV unwrapping ops from 3DView/editmode.
		 * Mesh keymap is probably not ideal, but best place I could find to put those. */
		if (sl->spacetype == SPACE_VIEW3D) {
			km = WM_keymap_find_all(C, "Mesh", 0, 0);
			if (km && km->poll && !km->poll((bContext *)C)) {
				km = NULL;
			}
		}
		if (!km) {
			km = WM_keymap_find_all(C, "UV Editor", 0, 0);
		}
	}
	/* Node Editor */
	else if (STRPREFIX(opname, "NODE_OT")) {
		km = WM_keymap_find_all(C, "Node Editor", sl->spacetype, 0);
	}
	/* Animation Editor Channels */
	else if (STRPREFIX(opname, "ANIM_OT_channels")) {
		km = WM_keymap_find_all(C, "Animation Channels", 0, 0);
	}
	/* Animation Generic - after channels */
	else if (STRPREFIX(opname, "ANIM_OT")) {
		km = WM_keymap_find_all(C, "Animation", 0, 0);
	}
	/* Graph Editor */
	else if (STRPREFIX(opname, "GRAPH_OT")) {
		km = WM_keymap_find_all(C, "Graph Editor", sl->spacetype, 0);
	}
	/* Dopesheet Editor */
	else if (STRPREFIX(opname, "ACTION_OT")) {
		km = WM_keymap_find_all(C, "Dopesheet", sl->spacetype, 0);
	}
	/* NLA Editor */
	else if (STRPREFIX(opname, "NLA_OT")) {
		km = WM_keymap_find_all(C, "NLA Editor", sl->spacetype, 0);
	}
	/* Script */
	else if (STRPREFIX(opname, "SCRIPT_OT")) {
		km = WM_keymap_find_all(C, "Script", sl->spacetype, 0);
	}
	/* Text */
	else if (STRPREFIX(opname, "TEXT_OT")) {
		km = WM_keymap_find_all(C, "Text", sl->spacetype, 0);
	}
	/* Sequencer */
	else if (STRPREFIX(opname, "SEQUENCER_OT")) {
		km = WM_keymap_find_all(C, "Sequencer", sl->spacetype, 0);
	}
	/* Console */
	else if (STRPREFIX(opname, "CONSOLE_OT")) {
		km = WM_keymap_find_all(C, "Console", sl->spacetype, 0);
	}
	/* Console */
	else if (STRPREFIX(opname, "INFO_OT")) {
		km = WM_keymap_find_all(C, "Info", sl->spacetype, 0);
	}
	/* File browser */
	else if (STRPREFIX(opname, "FILE_OT")) {
		km = WM_keymap_find_all(C, "File Browser", sl->spacetype, 0);
	}
	/* Logic Editor */
	else if (STRPREFIX(opname, "LOGIC_OT")) {
		km = WM_keymap_find_all(C, "Logic Editor", sl->spacetype, 0);
	}
	/* Outliner */
	else if (STRPREFIX(opname, "OUTLINER_OT")) {
		km = WM_keymap_find_all(C, "Outliner", sl->spacetype, 0);
	}
	/* Transform */
	else if (STRPREFIX(opname, "TRANSFORM_OT")) {
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
				km = WM_keymap_find_all(C, "UV Editor", 0, 0);
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

const char *WM_bool_as_string(bool test)
{
	return test ? IFACE_("ON") : IFACE_("OFF");
}
