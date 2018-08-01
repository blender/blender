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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/interface_utils.c
 *  \ingroup edinterface
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_listbase.h"

#include "BLT_translation.h"

#include "BKE_report.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.h"


/*************************** RNA Utilities ******************************/

uiBut *uiDefAutoButR(uiBlock *block, PointerRNA *ptr, PropertyRNA *prop, int index, const char *name, int icon, int x1, int y1, int x2, int y2)
{
	uiBut *but = NULL;

	switch (RNA_property_type(prop)) {
		case PROP_BOOLEAN:
		{
			int arraylen = RNA_property_array_length(ptr, prop);

			if (arraylen && index == -1)
				return NULL;

			if (icon && name && name[0] == '\0')
				but = uiDefIconButR_prop(block, UI_BTYPE_ICON_TOGGLE, 0, icon, x1, y1, x2, y2, ptr, prop, index, 0, 0, -1, -1, NULL);
			else if (icon)
				but = uiDefIconTextButR_prop(block, UI_BTYPE_ICON_TOGGLE, 0, icon, name, x1, y1, x2, y2, ptr, prop, index, 0, 0, -1, -1, NULL);
			else
				but = uiDefButR_prop(block, UI_BTYPE_CHECKBOX, 0, name, x1, y1, x2, y2, ptr, prop, index, 0, 0, -1, -1, NULL);
			break;
		}
		case PROP_INT:
		case PROP_FLOAT:
		{
			int arraylen = RNA_property_array_length(ptr, prop);

			if (arraylen && index == -1) {
				if (ELEM(RNA_property_subtype(prop), PROP_COLOR, PROP_COLOR_GAMMA)) {
					but = uiDefButR_prop(block, UI_BTYPE_COLOR, 0, name, x1, y1, x2, y2, ptr, prop, -1, 0, 0, -1, -1, NULL);
				}
				else {
					return NULL;
				}
			}
			else if (RNA_property_subtype(prop) == PROP_PERCENTAGE || RNA_property_subtype(prop) == PROP_FACTOR)
				but = uiDefButR_prop(block, UI_BTYPE_NUM_SLIDER, 0, name, x1, y1, x2, y2, ptr, prop, index, 0, 0, -1, -1, NULL);
			else
				but = uiDefButR_prop(block, UI_BTYPE_NUM, 0, name, x1, y1, x2, y2, ptr, prop, index, 0, 0, -1, -1, NULL);

			if (RNA_property_flag(prop) & PROP_TEXTEDIT_UPDATE) {
				UI_but_flag_enable(but, UI_BUT_TEXTEDIT_UPDATE);
			}
			break;
		}
		case PROP_ENUM:
			if (icon && name && name[0] == '\0')
				but = uiDefIconButR_prop(block, UI_BTYPE_MENU, 0, icon, x1, y1, x2, y2, ptr, prop, index, 0, 0, -1, -1, NULL);
			else if (icon)
				but = uiDefIconTextButR_prop(block, UI_BTYPE_MENU, 0, icon, NULL, x1, y1, x2, y2, ptr, prop, index, 0, 0, -1, -1, NULL);
			else
				but = uiDefButR_prop(block, UI_BTYPE_MENU, 0, name, x1, y1, x2, y2, ptr, prop, index, 0, 0, -1, -1, NULL);
			break;
		case PROP_STRING:
			if (icon && name && name[0] == '\0')
				but = uiDefIconButR_prop(block, UI_BTYPE_TEXT, 0, icon, x1, y1, x2, y2, ptr, prop, index, 0, 0, -1, -1, NULL);
			else if (icon)
				but = uiDefIconTextButR_prop(block, UI_BTYPE_TEXT, 0, icon, name, x1, y1, x2, y2, ptr, prop, index, 0, 0, -1, -1, NULL);
			else
				but = uiDefButR_prop(block, UI_BTYPE_TEXT, 0, name, x1, y1, x2, y2, ptr, prop, index, 0, 0, -1, -1, NULL);

			if (RNA_property_flag(prop) & PROP_TEXTEDIT_UPDATE) {
				/* TEXTEDIT_UPDATE is usally used for search buttons. For these we also want
				 * the 'x' icon to clear search string, so setting VALUE_CLEAR flag, too. */
				UI_but_flag_enable(but, UI_BUT_TEXTEDIT_UPDATE | UI_BUT_VALUE_CLEAR);
			}
			break;
		case PROP_POINTER:
		{
			if (icon == 0) {
				PointerRNA pptr = RNA_property_pointer_get(ptr, prop);
				icon = RNA_struct_ui_icon(pptr.type ? pptr.type : RNA_property_pointer_type(ptr, prop));
			}
			if (icon == ICON_DOT)
				icon = 0;

			but = uiDefIconTextButR_prop(block, UI_BTYPE_SEARCH_MENU, 0, icon, name, x1, y1, x2, y2, ptr, prop, index, 0, 0, -1, -1, NULL);
			break;
		}
		case PROP_COLLECTION:
		{
			char text[256];
			BLI_snprintf(text, sizeof(text), IFACE_("%d items"), RNA_property_collection_length(ptr, prop));
			but = uiDefBut(block, UI_BTYPE_LABEL, 0, text, x1, y1, x2, y2, NULL, 0, 0, 0, 0, NULL);
			UI_but_flag_enable(but, UI_BUT_DISABLED);
			break;
		}
		default:
			but = NULL;
			break;
	}

	return but;
}

/**
 * \a check_prop callback filters functions to avoid drawing certain properties,
 * in cases where PROP_HIDDEN flag can't be used for a property.
 */
eAutoPropButsReturn uiDefAutoButsRNA(
        uiLayout *layout, PointerRNA *ptr,
        bool (*check_prop)(PointerRNA *ptr, PropertyRNA *prop, void *user_data), void *user_data,
        const eButLabelAlign label_align, const bool compact)
{
	eAutoPropButsReturn return_info = UI_PROP_BUTS_NONE_ADDED;
	uiLayout *split, *col;
	int flag;
	const char *name;

	RNA_STRUCT_BEGIN (ptr, prop)
	{
		flag = RNA_property_flag(prop);

		if (flag & PROP_HIDDEN) {
			continue;
		}
		if (check_prop && check_prop(ptr, prop, user_data) == 0) {
			return_info |= UI_PROP_BUTS_ANY_FAILED_CHECK;
			continue;
		}

		switch (label_align) {
			case UI_BUT_LABEL_ALIGN_COLUMN:
			case UI_BUT_LABEL_ALIGN_SPLIT_COLUMN:
			{
				PropertyType type = RNA_property_type(prop);
				const bool is_boolean = (type == PROP_BOOLEAN && !RNA_property_array_check(prop));

				name = RNA_property_ui_name(prop);

				if (label_align == UI_BUT_LABEL_ALIGN_COLUMN) {
					col = uiLayoutColumn(layout, true);

					if (!is_boolean)
						uiItemL(col, name, ICON_NONE);
				}
				else {
					BLI_assert(label_align == UI_BUT_LABEL_ALIGN_SPLIT_COLUMN);
					split = uiLayoutSplit(layout, 0.5f, false);

					col = uiLayoutColumn(split, false);
					uiItemL(col, (is_boolean) ? "" : name, ICON_NONE);
					col = uiLayoutColumn(split, false);
				}

				/* may meed to add more cases here.
				 * don't override enum flag names */

				/* name is shown above, empty name for button below */
				name = (flag & PROP_ENUM_FLAG || is_boolean) ? NULL : "";

				break;
			}
			case UI_BUT_LABEL_ALIGN_NONE:
			default:
				col = layout;
				name = NULL; /* no smart label alignment, show default name with button */
				break;
		}

		uiItemFullR(col, ptr, prop, -1, 0, compact ? UI_ITEM_R_COMPACT : 0, name, ICON_NONE);
		return_info &= ~UI_PROP_BUTS_NONE_ADDED;
	}
	RNA_STRUCT_END;

	return return_info;
}

/* *** RNA collection search menu *** */

typedef struct CollItemSearch {
	struct CollItemSearch *next, *prev;
	void *data;
	char *name;
	int index;
	int iconid;
} CollItemSearch;

static int sort_search_items_list(const void *a, const void *b)
{
	const CollItemSearch *cis1 = a;
	const CollItemSearch *cis2 = b;

	if (BLI_strcasecmp(cis1->name, cis2->name) > 0)
		return 1;
	else
		return 0;
}

void ui_rna_collection_search_cb(const struct bContext *C, void *arg, const char *str, uiSearchItems *items)
{
	uiRNACollectionSearch *data = arg;
	char *name;
	int i = 0, iconid = 0, flag = RNA_property_flag(data->target_prop);
	ListBase *items_list = MEM_callocN(sizeof(ListBase), "items_list");
	CollItemSearch *cis;
	const bool skip_filter = !(data->but_changed && *data->but_changed);

	/* build a temporary list of relevant items first */
	RNA_PROP_BEGIN (&data->search_ptr, itemptr, data->search_prop)
	{

		if (flag & PROP_ID_SELF_CHECK)
			if (itemptr.data == data->target_ptr.id.data)
				continue;

		/* use filter */
		if (RNA_property_type(data->target_prop) == PROP_POINTER) {
			if (RNA_property_pointer_poll(&data->target_ptr, data->target_prop, &itemptr) == 0)
				continue;
		}

		name = RNA_struct_name_get_alloc(&itemptr, NULL, 0, NULL); /* could use the string length here */
		iconid = 0;
		if (itemptr.type && RNA_struct_is_ID(itemptr.type)) {
			iconid = ui_id_icon_get(C, itemptr.data, false);
		}

		if (name) {
			if (skip_filter || BLI_strcasestr(name, str)) {
				cis = MEM_callocN(sizeof(CollItemSearch), "CollectionItemSearch");
				cis->data = itemptr.data;
				cis->name = MEM_dupallocN(name);
				cis->index = i;
				cis->iconid = iconid;
				BLI_addtail(items_list, cis);
			}
			MEM_freeN(name);
		}

		i++;
	}
	RNA_PROP_END;

	BLI_listbase_sort(items_list, sort_search_items_list);

	/* add search items from temporary list */
	for (cis = items_list->first; cis; cis = cis->next) {
		if (UI_search_item_add(items, cis->name, cis->data, cis->iconid) == false) {
			break;
		}
	}

	for (cis = items_list->first; cis; cis = cis->next) {
		MEM_freeN(cis->name);
	}
	BLI_freelistN(items_list);
	MEM_freeN(items_list);
}


/***************************** ID Utilities *******************************/
int UI_icon_from_id(ID *id)
{
	Object *ob;
	PointerRNA ptr;
	short idcode;

	if (id == NULL)
		return ICON_NONE;

	idcode = GS(id->name);

	/* exception for objects */
	if (idcode == ID_OB) {
		ob = (Object *)id;

		if (ob->type == OB_EMPTY)
			return ICON_EMPTY_DATA;
		else
			return UI_icon_from_id(ob->data);
	}

	/* otherwise get it through RNA, creating the pointer
	 * will set the right type, also with subclassing */
	RNA_id_pointer_create(id, &ptr);

	return (ptr.type) ? RNA_struct_ui_icon(ptr.type) : ICON_NONE;
}

/* see: report_type_str */
int UI_icon_from_report_type(int type)
{
	if (type & RPT_ERROR_ALL)
		return ICON_ERROR;
	else if (type & RPT_WARNING_ALL)
		return ICON_ERROR;
	else if (type & RPT_INFO_ALL)
		return ICON_INFO;
	else
		return ICON_NONE;
}

/********************************** Misc **************************************/

/**
 * Returns the best "UI" precision for given floating value, so that e.g. 10.000001 rather gets drawn as '10'...
 */
int UI_calc_float_precision(int prec, double value)
{
	static const double pow10_neg[UI_PRECISION_FLOAT_MAX + 1] = {1e0, 1e-1, 1e-2, 1e-3, 1e-4, 1e-5, 1e-6};
	static const double max_pow = 10000000.0;  /* pow(10, UI_PRECISION_FLOAT_MAX) */

	BLI_assert(prec <= UI_PRECISION_FLOAT_MAX);
	BLI_assert(fabs(pow10_neg[prec] - pow(10, -prec)) < 1e-16);

	/* check on the number of decimal places need to display the number, this is so 0.00001 is not displayed as 0.00,
	 * _but_, this is only for small values si 10.0001 will not get the same treatment.
	 */
	value = ABS(value);
	if ((value < pow10_neg[prec]) && (value > (1.0 / max_pow))) {
		int value_i = (int)((value * max_pow) + 0.5);
		if (value_i != 0) {
			const int prec_span = 3; /* show: 0.01001, 5 would allow 0.0100001 for eg. */
			int test_prec;
			int prec_min = -1;
			int dec_flag = 0;
			int i = UI_PRECISION_FLOAT_MAX;
			while (i && value_i) {
				if (value_i % 10) {
					dec_flag |= 1 << i;
					prec_min = i;
				}
				value_i /= 10;
				i--;
			}

			/* even though its a small value, if the second last digit is not 0, use it */
			test_prec = prec_min;

			dec_flag = (dec_flag >> (prec_min + 1)) & ((1 << prec_span) - 1);

			while (dec_flag) {
				test_prec++;
				dec_flag = dec_flag >> 1;
			}

			if (test_prec > prec) {
				prec = test_prec;
			}
		}
	}

	CLAMP(prec, 0, UI_PRECISION_FLOAT_MAX);

	return prec;
}

bool UI_but_online_manual_id(const uiBut *but, char *r_str, size_t maxlength)
{
	if (but->rnapoin.id.data && but->rnapoin.data && but->rnaprop) {
		BLI_snprintf(
		        r_str, maxlength, "%s.%s", RNA_struct_identifier(but->rnapoin.type),
		        RNA_property_identifier(but->rnaprop));
		return true;
	}
	else if (but->optype) {
		WM_operator_py_idname(r_str, but->optype->idname);
		return true;
	}

	*r_str = '\0';
	return false;
}

bool UI_but_online_manual_id_from_active(const struct bContext *C, char *r_str, size_t maxlength)
{
	uiBut *but = UI_context_active_but_get(C);

	if (but) {
		return UI_but_online_manual_id(but, r_str, maxlength);
	}

	*r_str = '\0';
	return false;
}


/* -------------------------------------------------------------------- */
/* Modal Button Store API */

/** \name Button Store
 *
 * Store for modal operators & handlers to register button pointers
 * which are maintained while drawing or NULL when removed.
 *
 * This is needed since button pointers are continuously freed and re-allocated.
 *
 * \{ */

struct uiButStore {
	struct uiButStore *next, *prev;
	uiBlock *block;
	ListBase items;
};

struct uiButStoreElem {
	struct uiButStoreElem *next, *prev;
	uiBut **but_p;
};

/**
 * Create a new button store, the caller must manage and run #UI_butstore_free
 */
uiButStore *UI_butstore_create(uiBlock *block)
{
	uiButStore *bs_handle = MEM_callocN(sizeof(uiButStore), __func__);

	bs_handle->block = block;
	BLI_addtail(&block->butstore, bs_handle);

	return bs_handle;
}

void UI_butstore_free(uiBlock *block, uiButStore *bs_handle)
{
	/* Workaround for button store being moved into new block,
	 * which then can't use the previous buttons state ('ui_but_update_from_old_block' fails to find a match),
	 * keeping the active button in the old block holding a reference to the button-state in the new block: see T49034.
	 *
	 * Ideally we would manage moving the 'uiButStore', keeping a correct state.
	 * All things considered this is the most straightforward fix - Campbell.
	 */
	if (block != bs_handle->block && bs_handle->block != NULL) {
		block = bs_handle->block;
	}

	BLI_freelistN(&bs_handle->items);
	BLI_remlink(&block->butstore, bs_handle);

	MEM_freeN(bs_handle);
}

bool UI_butstore_is_valid(uiButStore *bs)
{
	return (bs->block != NULL);
}

bool UI_butstore_is_registered(uiBlock *block, uiBut *but)
{
	uiButStore *bs_handle;

	for (bs_handle = block->butstore.first; bs_handle; bs_handle = bs_handle->next) {
		uiButStoreElem *bs_elem;

		for (bs_elem = bs_handle->items.first; bs_elem; bs_elem = bs_elem->next) {
			if (*bs_elem->but_p == but) {
				return true;
			}
		}
	}

	return false;
}

void UI_butstore_register(uiButStore *bs_handle, uiBut **but_p)
{
	uiButStoreElem *bs_elem = MEM_callocN(sizeof(uiButStoreElem), __func__);
	BLI_assert(*but_p);
	bs_elem->but_p = but_p;

	BLI_addtail(&bs_handle->items, bs_elem);

}

void UI_butstore_unregister(uiButStore *bs_handle, uiBut **but_p)
{
	uiButStoreElem *bs_elem, *bs_elem_next;

	for (bs_elem = bs_handle->items.first; bs_elem; bs_elem = bs_elem_next) {
		bs_elem_next = bs_elem->next;
		if (bs_elem->but_p == but_p) {
			BLI_remlink(&bs_handle->items, bs_elem);
			MEM_freeN(bs_elem);
		}
	}

	BLI_assert(0);
}

/**
 * Update the pointer for a registered button.
 */
bool UI_butstore_register_update(uiBlock *block, uiBut *but_dst, const uiBut *but_src)
{
	uiButStore *bs_handle;
	bool found = false;

	for (bs_handle = block->butstore.first; bs_handle; bs_handle = bs_handle->next) {
		uiButStoreElem *bs_elem;
		for (bs_elem = bs_handle->items.first; bs_elem; bs_elem = bs_elem->next) {
			if (*bs_elem->but_p == but_src) {
				*bs_elem->but_p = but_dst;
				found = true;
			}
		}
	}

	return found;
}

/**
 * NULL all pointers, don't free since the owner needs to be able to inspect.
 */
void UI_butstore_clear(uiBlock *block)
{
	uiButStore *bs_handle;

	for (bs_handle = block->butstore.first; bs_handle; bs_handle = bs_handle->next) {
		uiButStoreElem *bs_elem;

		bs_handle->block = NULL;

		for (bs_elem = bs_handle->items.first; bs_elem; bs_elem = bs_elem->next) {
			*bs_elem->but_p = NULL;
		}
	}
}

/**
 * Map freed buttons from the old block and update pointers.
 */
void UI_butstore_update(uiBlock *block)
{
	uiButStore *bs_handle;

	/* move this list to the new block */
	if (block->oldblock) {
		if (block->oldblock->butstore.first) {
			block->butstore = block->oldblock->butstore;
			BLI_listbase_clear(&block->oldblock->butstore);
		}
	}

	if (LIKELY(block->butstore.first == NULL))
		return;

	/* warning, loop-in-loop, in practice we only store <10 buttons at a time,
	 * so this isn't going to be a problem, if that changes old-new mapping can be cached first */
	for (bs_handle = block->butstore.first; bs_handle; bs_handle = bs_handle->next) {

		BLI_assert((bs_handle->block == NULL) ||
		           (bs_handle->block == block) ||
		           (block->oldblock && block->oldblock == bs_handle->block));

		if (bs_handle->block == block->oldblock) {
			uiButStoreElem *bs_elem;

			bs_handle->block = block;

			for (bs_elem = bs_handle->items.first; bs_elem; bs_elem = bs_elem->next) {
				if (*bs_elem->but_p) {
					uiBut *but_new = ui_but_find_new(block, *bs_elem->but_p);

					/* can be NULL if the buttons removed,
					 * note: we could allow passing in a callback when buttons are removed
					 * so the caller can cleanup */
					*bs_elem->but_p = but_new;
				}
			}
		}
	}
}

/** \} */
