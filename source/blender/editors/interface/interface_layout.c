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
 * Contributor(s): Blender Foundation 2009.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/interface_layout.c
 *  \ingroup edinterface
 */


#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_armature_types.h"
#include "DNA_userdef_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_screen.h"

#include "RNA_access.h"

#include "UI_interface.h"

#include "ED_armature.h"


#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.h"

/************************ Structs and Defines *************************/

// #define USE_OP_RESET_BUT  // we may want to make this optional, disable for now.

#define UI_OPERATOR_ERROR_RET(_ot, _opname, return_statement)                 \
	if (ot == NULL) {                                                         \
		ui_item_disabled(layout, _opname);                                    \
		RNA_warning("'%s' unknown operator", _opname);                        \
		return_statement;                                                     \
	} (void)0                                                                 \


/* uiLayoutRoot */

typedef struct uiLayoutRoot {
	struct uiLayoutRoot *next, *prev;

	int type;
	int opcontext;

	int emw, emh;
	int padding;

	uiMenuHandleFunc handlefunc;
	void *argv;

	uiStyle *style;
	uiBlock *block;
	uiLayout *layout;
} uiLayoutRoot;

/* Item */

typedef enum uiItemType {
	ITEM_BUTTON,

	ITEM_LAYOUT_ROW,
	ITEM_LAYOUT_COLUMN,
	ITEM_LAYOUT_COLUMN_FLOW,
	ITEM_LAYOUT_ROW_FLOW,
	ITEM_LAYOUT_BOX,
	ITEM_LAYOUT_ABSOLUTE,
	ITEM_LAYOUT_SPLIT,
	ITEM_LAYOUT_OVERLAP,

	ITEM_LAYOUT_ROOT
#if 0
	TEMPLATE_COLUMN_FLOW,
	TEMPLATE_SPLIT,
	TEMPLATE_BOX,

	TEMPLATE_HEADER,
	TEMPLATE_HEADER_ID
#endif
} uiItemType;

typedef struct uiItem {
	void *next, *prev;
	uiItemType type;
	int flag;
} uiItem;

typedef struct uiButtonItem {
	uiItem item;
	uiBut *but;
} uiButtonItem;

struct uiLayout {
	uiItem item;

	uiLayoutRoot *root;
	bContextStore *context;
	ListBase items;

	int x, y, w, h;
	float scale[2];
	short space;
	bool align;
	bool active;
	bool enabled;
	bool redalert;
	bool keepaspect;
	char alignment;
};

typedef struct uiLayoutItemFlow {
	uiLayout litem;
	int number;
	int totcol;
} uiLayoutItemFlow;

typedef struct uiLayoutItemBx {
	uiLayout litem;
	uiBut *roundbox;
} uiLayoutItemBx;

typedef struct uiLayoutItemSplit {
	uiLayout litem;
	float percentage;
} uiLayoutItemSplit;

typedef struct uiLayoutItemRoot {
	uiLayout litem;
} uiLayoutItemRoot;

/************************** Item ***************************/

static const char *ui_item_name_add_colon(const char *name, char namestr[UI_MAX_NAME_STR])
{
	int len = strlen(name);

	if (len != 0 && len + 1 < UI_MAX_NAME_STR) {
		BLI_strncpy(namestr, name, UI_MAX_NAME_STR);
		namestr[len] = ':';
		namestr[len + 1] = '\0';
		return namestr;
	}

	return name;
}

static int ui_item_fit(int item, int pos, int all, int available, int last, int alignment, int *offset)
{
	if (offset)
		*offset = 0;

	/* available == 0 is unlimited */
	if (available == 0)
		return item;

	if (all > available) {
		/* contents is bigger than available space */
		if (last)
			return available - pos;
		else
			return (item * available) / all;
	}
	else {
		/* contents is smaller or equal to available space */
		if (alignment == UI_LAYOUT_ALIGN_EXPAND) {
			if (last)
				return available - pos;
			else
				return (item * available) / all;
		}
		else
			return item;
	}
}

/* variable button size in which direction? */
#define UI_ITEM_VARY_X  1
#define UI_ITEM_VARY_Y  2

static int ui_layout_vary_direction(uiLayout *layout)
{
	return (layout->root->type == UI_LAYOUT_HEADER || layout->alignment != UI_LAYOUT_ALIGN_EXPAND) ? UI_ITEM_VARY_X : UI_ITEM_VARY_Y;
}

/* estimated size of text + icon */
static int ui_text_icon_width(uiLayout *layout, const char *name, int icon, bool compact)
{
	bool variable;

	if (icon && !name[0])
		return UI_UNIT_X;  /* icon only */

	variable = (ui_layout_vary_direction(layout) == UI_ITEM_VARY_X);

	if (variable) {
		/* it may seem odd that the icon only adds (UI_UNIT_X / 4)
		 * but taking margins into account its fine */
		return (UI_GetStringWidth(name) +
		        (UI_UNIT_X * ((compact ? 1.25f : 1.50f) +
		                      (icon    ? 0.25f : 0.0f))));
	}
	else {
		return UI_UNIT_X * 10;
	}
}

static void ui_item_size(uiItem *item, int *r_w, int *r_h)
{
	if (item->type == ITEM_BUTTON) {
		uiButtonItem *bitem = (uiButtonItem *)item;

		if (r_w) *r_w = BLI_rctf_size_x(&bitem->but->rect);
		if (r_h) *r_h = BLI_rctf_size_y(&bitem->but->rect);
	}
	else {
		uiLayout *litem = (uiLayout *)item;

		if (r_w) *r_w = litem->w;
		if (r_h) *r_h = litem->h;
	}
}

static void ui_item_offset(uiItem *item, int *r_x, int *r_y)
{
	if (item->type == ITEM_BUTTON) {
		uiButtonItem *bitem = (uiButtonItem *)item;

		if (r_x) *r_x = bitem->but->rect.xmin;
		if (r_y) *r_y = bitem->but->rect.ymin;
	}
	else {
		if (r_x) *r_x = 0;
		if (r_y) *r_y = 0;
	}
}

static void ui_item_position(uiItem *item, int x, int y, int w, int h)
{
	if (item->type == ITEM_BUTTON) {
		uiButtonItem *bitem = (uiButtonItem *)item;

		bitem->but->rect.xmin = x;
		bitem->but->rect.ymin = y;
		bitem->but->rect.xmax = x + w;
		bitem->but->rect.ymax = y + h;
		
		ui_check_but(bitem->but); /* for strlen */
	}
	else {
		uiLayout *litem = (uiLayout *)item;

		litem->x = x;
		litem->y = y + h;
		litem->w = w;
		litem->h = h;
	}
}

/******************** Special RNA Items *********************/

static int ui_layout_local_dir(uiLayout *layout)
{
	switch (layout->item.type) {
		case ITEM_LAYOUT_ROW:
		case ITEM_LAYOUT_ROOT:
		case ITEM_LAYOUT_OVERLAP:
			return UI_LAYOUT_HORIZONTAL;
		case ITEM_LAYOUT_COLUMN:
		case ITEM_LAYOUT_COLUMN_FLOW:
		case ITEM_LAYOUT_SPLIT:
		case ITEM_LAYOUT_ABSOLUTE:
		case ITEM_LAYOUT_BOX:
		default:
			return UI_LAYOUT_VERTICAL;
	}
}

static uiLayout *ui_item_local_sublayout(uiLayout *test, uiLayout *layout, int align)
{
	uiLayout *sub;

	if (ui_layout_local_dir(test) == UI_LAYOUT_HORIZONTAL)
		sub = uiLayoutRow(layout, align);
	else
		sub = uiLayoutColumn(layout, align);
	
	sub->space = 0;
	return sub;
}

static void ui_layer_but_cb(bContext *C, void *arg_but, void *arg_index)
{
	wmWindow *win = CTX_wm_window(C);
	uiBut *but = arg_but, *cbut;
	PointerRNA *ptr = &but->rnapoin;
	PropertyRNA *prop = but->rnaprop;
	int i, index = GET_INT_FROM_POINTER(arg_index);
	int shift = win->eventstate->shift;
	int len = RNA_property_array_length(ptr, prop);

	if (!shift) {
		RNA_property_boolean_set_index(ptr, prop, index, true);

		for (i = 0; i < len; i++)
			if (i != index)
				RNA_property_boolean_set_index(ptr, prop, i, 0);

		RNA_property_update(C, ptr, prop);

		for (cbut = but->block->buttons.first; cbut; cbut = cbut->next)
			ui_check_but(cbut);
	}
}

/* create buttons for an item with an RNA array */
static void ui_item_array(uiLayout *layout, uiBlock *block, const char *name, int icon,
                          PointerRNA *ptr, PropertyRNA *prop, int len, int x, int y, int w, int UNUSED(h),
                          int expand, int slider, int toggle, int icon_only)
{
	uiStyle *style = layout->root->style;
	uiBut *but;
	PropertyType type;
	PropertySubType subtype;
	uiLayout *sub;
	unsigned int a, b;

	/* retrieve type and subtype */
	type = RNA_property_type(prop);
	subtype = RNA_property_subtype(prop);

	sub = ui_item_local_sublayout(layout, layout, 1);
	uiBlockSetCurLayout(block, sub);

	/* create label */
	if (name[0])
		uiDefBut(block, LABEL, 0, name, 0, 0, w, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");

	/* create buttons */
	if (type == PROP_BOOLEAN && ELEM(subtype, PROP_LAYER, PROP_LAYER_MEMBER)) {
		/* special check for layer layout */
		int butw, buth, unit;
		int cols = (len >= 20) ? 2 : 1;
		const unsigned int colbuts = len / (2 * cols);
		unsigned int layer_used = 0;
		unsigned int layer_active = 0;

		uiBlockSetCurLayout(block, uiLayoutAbsolute(layout, false));

		unit = UI_UNIT_X * 0.75;
		butw = unit;
		buth = unit;

		if (ptr->type == &RNA_Armature) {
			bArmature *arm = (bArmature *)ptr->data;
			
			layer_used = arm->layer_used;
			
			if (arm->edbo) {
				if (arm->act_edbone) {
					layer_active |= arm->act_edbone->layer;
				}
			}
			else {
				if (arm->act_bone) {
					layer_active |= arm->act_bone->layer;
				}
			}
		}

		for (b = 0; b < cols; b++) {
			uiBlockBeginAlign(block);

			for (a = 0; a < colbuts; a++) {
				const int layer_num  = a + b * colbuts;
				const unsigned int layer_flag = (1u << layer_num);
				
				if (layer_used & layer_flag) {
					if (layer_active & layer_flag)
						icon = ICON_LAYER_ACTIVE;
					else
						icon = ICON_LAYER_USED;
				}
				else {
					icon = ICON_BLANK1;
				}

				but = uiDefAutoButR(block, ptr, prop, layer_num, "", icon, x + butw * a, y + buth, butw, buth);
				if (subtype == PROP_LAYER_MEMBER)
					uiButSetFunc(but, ui_layer_but_cb, but, SET_INT_IN_POINTER(layer_num));
			}
			for (a = 0; a < colbuts; a++) {
				const int layer_num  = a + len / 2 + b * colbuts;
				const unsigned int layer_flag = (1u << layer_num);
				
				if (layer_used & layer_flag) {
					if (layer_active & layer_flag)
						icon = ICON_LAYER_ACTIVE;
					else
						icon = ICON_LAYER_USED;
				}
				else {
					icon = ICON_BLANK1;
				}

				but = uiDefAutoButR(block, ptr, prop, layer_num, "", icon, x + butw * a, y, butw, buth);
				if (subtype == PROP_LAYER_MEMBER)
					uiButSetFunc(but, ui_layer_but_cb, but, SET_INT_IN_POINTER(layer_num));
			}
			uiBlockEndAlign(block);

			x += colbuts * butw + style->buttonspacex;
		}
	}
	else if (subtype == PROP_MATRIX) {
		int totdim, dim_size[3];    /* 3 == RNA_MAX_ARRAY_DIMENSION */
		int row, col;

		uiBlockSetCurLayout(block, uiLayoutAbsolute(layout, true));

		totdim = RNA_property_array_dimension(ptr, prop, dim_size);
		if (totdim != 2) return;    /* only 2D matrices supported in UI so far */
		
		w /= dim_size[0];
		/* h /= dim_size[1]; */ /* UNUSED */

		for (a = 0; a < len; a++) {
			col = a % dim_size[0];
			row = a / dim_size[0];

			but = uiDefAutoButR(block, ptr, prop, a, "", ICON_NONE, x + w * col, y + (dim_size[1] * UI_UNIT_Y) - (row * UI_UNIT_Y), w, UI_UNIT_Y);
			if (slider && but->type == NUM)
				but->type = NUMSLI;
		}
	}
	else if (subtype == PROP_DIRECTION && !expand) {
		uiDefButR_prop(block, BUT_NORMAL, 0, name, x, y, UI_UNIT_X * 3, UI_UNIT_Y * 3, ptr, prop, 0, 0, 0, -1, -1, NULL);
	}
	else {
		/* note, this block of code is a bit arbitrary and has just been made
		 * to work with common cases, but may need to be re-worked */

		/* special case, boolean array in a menu, this could be used in a more generic way too */
		if (ELEM(subtype, PROP_COLOR, PROP_COLOR_GAMMA) && !expand) {
			uiDefAutoButR(block, ptr, prop, -1, "", ICON_NONE, 0, 0, w, UI_UNIT_Y);
		}
		else {
			int *boolarr = NULL;

			/* even if 'expand' is fale, expanding anyway */

			/* layout for known array subtypes */
			char str[3] = {'\0'};

			if (!icon_only) {
				if (type != PROP_BOOLEAN) {
					str[1] = ':';
				}
			}

			/* show checkboxes for rna on a non-emboss block (menu for eg) */
			if (type == PROP_BOOLEAN && ELEM(layout->root->block->dt, UI_EMBOSSN, UI_EMBOSSP)) {
				boolarr = MEM_callocN(sizeof(int) * len, "ui_item_array");
				RNA_property_boolean_get_array(ptr, prop, boolarr);
			}

			for (a = 0; a < len; a++) {
				if (!icon_only) str[0] = RNA_property_array_item_char(prop, a);
				if (boolarr) icon = boolarr[a] ? ICON_CHECKBOX_HLT : ICON_CHECKBOX_DEHLT;
				but = uiDefAutoButR(block, ptr, prop, a, str, icon, 0, 0, w, UI_UNIT_Y);
				if (slider && but->type == NUM)
					but->type = NUMSLI;
				if (toggle && but->type == OPTION)
					but->type = TOG;
				if ((a == 0) && (subtype == PROP_AXISANGLE))
					uiButSetUnitType(but, PROP_UNIT_ROTATION);
			}

			if (boolarr) {
				MEM_freeN(boolarr);
			}
		}
	}

	uiBlockSetCurLayout(block, layout);
}

static void ui_item_enum_expand_handle(bContext *C, void *arg1, void *arg2)
{
	wmWindow *win = CTX_wm_window(C);

	if (!win->eventstate->shift) {
		uiBut *but = (uiBut *)arg1;
		int enum_value = GET_INT_FROM_POINTER(arg2);

		int current_value = RNA_property_enum_get(&but->rnapoin, but->rnaprop);
		if (!(current_value & enum_value)) {
			current_value = enum_value;
		}
		else {
			current_value &= enum_value;
		}
		RNA_property_enum_set(&but->rnapoin, but->rnaprop, current_value);
	}
}
static void ui_item_enum_expand(uiLayout *layout, uiBlock *block, PointerRNA *ptr, PropertyRNA *prop,
                                const char *uiname, int h, int icon_only)
{
	/* XXX The way this function currently handles uiname parameter is insane and inconsistent with general UI API:
	 *     * uiname is the *enum property* label.
	 *     * when it is NULL or empty, we do not draw *enum items* labels, this doubles the icon_only parameter.
	 *     * we *never* draw (i.e. really use) the enum label uiname, it is just used as a mere flag!
	 *     Unfortunately, fixing this implies an API "soft break", so better to defer it for later... :/
	 *     --mont29
	 */

	uiBut *but;
	EnumPropertyItem *item, *item_array;
	const char *name;
	int itemw, icon, value;
	bool free;

	RNA_property_enum_items_gettexted(block->evil_C, ptr, prop, &item_array, NULL, &free);

	/* we dont want nested rows, cols in menus */
	if (layout->root->type != UI_LAYOUT_MENU) {
		uiBlockSetCurLayout(block, ui_item_local_sublayout(layout, layout, 1));
	}
	else {
		uiBlockSetCurLayout(block, layout);
	}

	for (item = item_array; item->identifier; item++) {
		if (!item->identifier[0])
			continue;

		name = (!uiname || uiname[0]) ? item->name : "";
		icon = item->icon;
		value = item->value;
		itemw = ui_text_icon_width(block->curlayout, icon_only ? "" : name, icon, 0);

		if (icon && name[0] && !icon_only)
			but = uiDefIconTextButR_prop(block, ROW, 0, icon, name, 0, 0, itemw, h, ptr, prop, -1, 0, value, -1, -1, NULL);
		else if (icon)
			but = uiDefIconButR_prop(block, ROW, 0, icon, 0, 0, itemw, h, ptr, prop, -1, 0, value, -1, -1, NULL);
		else
			but = uiDefButR_prop(block, ROW, 0, name, 0, 0, itemw, h, ptr, prop, -1, 0, value, -1, -1, NULL);

		if (RNA_property_flag(prop) & PROP_ENUM_FLAG) {
			uiButSetFunc(but, ui_item_enum_expand_handle, but, SET_INT_IN_POINTER(value));
		}

		if (ui_layout_local_dir(layout) != UI_LAYOUT_HORIZONTAL)
			but->drawflag |= UI_BUT_TEXT_LEFT;
	}
	uiBlockSetCurLayout(block, layout);

	if (free) {
		MEM_freeN(item_array);
	}
}

/* callback for keymap item change button */
static void ui_keymap_but_cb(bContext *UNUSED(C), void *but_v, void *UNUSED(key_v))
{
	uiBut *but = but_v;

	RNA_boolean_set(&but->rnapoin, "shift", (but->modifier_key & KM_SHIFT) != 0);
	RNA_boolean_set(&but->rnapoin, "ctrl", (but->modifier_key & KM_CTRL) != 0);
	RNA_boolean_set(&but->rnapoin, "alt", (but->modifier_key & KM_ALT) != 0);
	RNA_boolean_set(&but->rnapoin, "oskey", (but->modifier_key & KM_OSKEY) != 0);
}

/* create label + button for RNA property */
static uiBut *ui_item_with_label(uiLayout *layout, uiBlock *block, const char *name, int icon, PointerRNA *ptr, PropertyRNA *prop, int index, int x, int y, int w, int h, int flag)
{
	uiLayout *sub;
	uiBut *but = NULL;
	PropertyType type;
	PropertySubType subtype;
	int labelw;

	sub = uiLayoutRow(layout, layout->align);
	uiBlockSetCurLayout(block, sub);

	if (name[0]) {
		/* XXX UI_GetStringWidth is not accurate */
#if 0
		labelw = UI_GetStringWidth(name);
		CLAMP(labelw, w / 4, 3 * w / 4);
#endif
		labelw = w / 3;
		uiDefBut(block, LABEL, 0, name, x, y, labelw, h, NULL, 0.0, 0.0, 0, 0, "");
		w = w - labelw;
	}

	type = RNA_property_type(prop);
	subtype = RNA_property_subtype(prop);

	if (subtype == PROP_FILEPATH || subtype == PROP_DIRPATH) {
		uiBlockSetCurLayout(block, uiLayoutRow(sub, true));
		but = uiDefAutoButR(block, ptr, prop, index, "", icon, x, y, w - UI_UNIT_X, h);

		/* BUTTONS_OT_file_browse calls uiFileBrowseContextProperty */
		uiDefIconButO(block, BUT, subtype == PROP_DIRPATH ? "BUTTONS_OT_directory_browse" : "BUTTONS_OT_file_browse",
		              WM_OP_INVOKE_DEFAULT, ICON_FILESEL, x, y, UI_UNIT_X, h, NULL);
	}
	else if (flag & UI_ITEM_R_EVENT) {
		uiDefButR_prop(block, KEYEVT, 0, name, x, y, w, h, ptr, prop, index, 0, 0, -1, -1, NULL);
	}
	else if (flag & UI_ITEM_R_FULL_EVENT) {
		if (RNA_struct_is_a(ptr->type, &RNA_KeyMapItem)) {
			char buf[128];

			WM_keymap_item_to_string(ptr->data, buf, sizeof(buf));

			but = uiDefButR_prop(block, HOTKEYEVT, 0, buf, x, y, w, h, ptr, prop, 0, 0, 0, -1, -1, NULL);
			uiButSetFunc(but, ui_keymap_but_cb, but, NULL);
			if (flag & UI_ITEM_R_IMMEDIATE)
				uiButSetFlag(but, UI_BUT_IMMEDIATE);
		}
	}
	else
		but = uiDefAutoButR(block, ptr, prop, index, (type == PROP_ENUM && !(flag & UI_ITEM_R_ICON_ONLY)) ? NULL : "", icon, x, y, w, h);

	uiBlockSetCurLayout(block, layout);
	return but;
}

void uiFileBrowseContextProperty(const bContext *C, PointerRNA *ptr, PropertyRNA **prop)
{
	ARegion *ar = CTX_wm_region(C);
	uiBlock *block;
	uiBut *but, *prevbut;

	memset(ptr, 0, sizeof(*ptr));
	*prop = NULL;

	if (!ar)
		return;

	for (block = ar->uiblocks.first; block; block = block->next) {
		for (but = block->buttons.first; but; but = but->next) {
			prevbut = but->prev;

			/* find the button before the active one */
			if ((but->flag & UI_BUT_LAST_ACTIVE) && prevbut && prevbut->rnapoin.data) {
				if (RNA_property_type(prevbut->rnaprop) == PROP_STRING) {
					*ptr = prevbut->rnapoin;
					*prop = prevbut->rnaprop;
					return;
				}
			}
		}
	}
}

/********************* Button Items *************************/

/**
 * Update a buttons tip with an enum's description if possible.
 */
static void ui_but_tip_from_enum_item(uiBut *but, EnumPropertyItem *item)
{
	if (but->tip == NULL || but->tip[0] == '\0') {
		if (item->description && item->description[0]) {
			but->tip = item->description;
		}
	}
}

/* disabled item */
static void ui_item_disabled(uiLayout *layout, const char *name)
{
	uiBlock *block = layout->root->block;
	uiBut *but;
	int w;

	uiBlockSetCurLayout(block, layout);

	if (!name)
		name = "";

	w = ui_text_icon_width(layout, name, 0, 0);

	but = uiDefBut(block, LABEL, 0, name, 0, 0, w, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
	but->flag |= UI_BUT_DISABLED;
	but->lock = true;
	but->lockstr = "";
}

/* operator items */
PointerRNA uiItemFullO_ptr(uiLayout *layout, wmOperatorType *ot, const char *name, int icon, IDProperty *properties, int context, int flag)
{
	uiBlock *block = layout->root->block;
	uiBut *but;
	int w;

	if (!name) {
		if (ot && ot->srna && (flag & UI_ITEM_R_ICON_ONLY) == 0)
			name = RNA_struct_ui_name(ot->srna);
		else
			name = "";
	}

	if (layout->root->type == UI_LAYOUT_MENU && !icon)
		icon = ICON_BLANK1;

	/* create button */
	uiBlockSetCurLayout(block, layout);

	w = ui_text_icon_width(layout, name, icon, 0);

	if (flag & UI_ITEM_R_NO_BG)
		uiBlockSetEmboss(block, UI_EMBOSSN);

	/* create the button */
	if (icon) {
		if (name[0]) {
			but = uiDefIconTextButO_ptr(block, BUT, ot, context, icon, name, 0, 0, w, UI_UNIT_Y, NULL);
		}
		else {
			but = uiDefIconButO_ptr(block, BUT, ot, context, icon, 0, 0, w, UI_UNIT_Y, NULL);
		}
	}
	else {
		but = uiDefButO_ptr(block, BUT, ot, context, name, 0, 0, w, UI_UNIT_Y, NULL);
	}

	assert(but->optype != NULL);

	/* text alignment for toolbar buttons */
	if ((layout->root->type == UI_LAYOUT_TOOLBAR) && !icon)
		but->drawflag |= UI_BUT_TEXT_LEFT;

	if (flag & UI_ITEM_R_NO_BG)
		uiBlockSetEmboss(block, UI_EMBOSS);

	if (layout->redalert)
		uiButSetFlag(but, UI_BUT_REDALERT);

	/* assign properties */
	if (properties || (flag & UI_ITEM_O_RETURN_PROPS)) {
		PointerRNA *opptr = uiButGetOperatorPtrRNA(but);

		if (properties) {
			opptr->data = properties;
		}
		else {
			IDPropertyTemplate val = {0};
			opptr->data = IDP_New(IDP_GROUP, &val, "wmOperatorProperties");
		}

		return *opptr;
	}

	return PointerRNA_NULL;
}

PointerRNA uiItemFullO(uiLayout *layout, const char *opname, const char *name, int icon, IDProperty *properties, int context, int flag)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0); /* print error next */

	UI_OPERATOR_ERROR_RET(ot, opname, return PointerRNA_NULL);

	return uiItemFullO_ptr(layout, ot, name, icon, properties, context, flag);
}

static const char *ui_menu_enumpropname(uiLayout *layout, PointerRNA *ptr, PropertyRNA *prop, int retval)
{
	EnumPropertyItem *item;
	bool free;
	const char *name;

	RNA_property_enum_items(layout->root->block->evil_C, ptr, prop, &item, NULL, &free);
	if (RNA_enum_name(item, retval, &name)) {
		name = CTX_IFACE_(RNA_property_translation_context(prop), name);
	}
	else {
		name = "";
	}

	if (free) {
		MEM_freeN(item);
	}

	return name;
}

void uiItemEnumO_ptr(uiLayout *layout, wmOperatorType *ot, const char *name, int icon, const char *propname, int value)
{
	PointerRNA ptr;
	PropertyRNA *prop;

	WM_operator_properties_create_ptr(&ptr, ot);

	if ((prop = RNA_struct_find_property(&ptr, propname))) {
		/* pass */
	}
	else {
		RNA_warning("%s.%s not found", RNA_struct_identifier(ptr.type), propname);
		return;
	}

	RNA_property_enum_set(&ptr, prop, value);

	if (!name)
		name = ui_menu_enumpropname(layout, &ptr, prop, value);

	uiItemFullO_ptr(layout, ot, name, icon, ptr.data, layout->root->opcontext, 0);
}
void uiItemEnumO(uiLayout *layout, const char *opname, const char *name, int icon, const char *propname, int value)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0); /* print error next */

	if (ot) {
		uiItemEnumO_ptr(layout, ot, name, icon, propname, value);
	}
	else {
		ui_item_disabled(layout, opname);
		RNA_warning("unknown operator '%s'", opname);
	}

}

void uiItemsFullEnumO(uiLayout *layout, const char *opname, const char *propname, IDProperty *properties,
                      int context, int flag)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0); /* print error next */

	PointerRNA ptr;
	PropertyRNA *prop;
	uiBlock *block = layout->root->block;

	if (!ot || !ot->srna) {
		ui_item_disabled(layout, opname);
		RNA_warning("%s '%s'", ot ? "unknown operator" : "operator missing srna", opname);
		return;
	}

	WM_operator_properties_create_ptr(&ptr, ot);
	/* so the context is passed to itemf functions (some need it) */
	WM_operator_properties_sanitize(&ptr, false);
	prop = RNA_struct_find_property(&ptr, propname);

	/* don't let bad properties slip through */
	BLI_assert((prop == NULL) || (RNA_property_type(prop) == PROP_ENUM));

	if (prop && RNA_property_type(prop) == PROP_ENUM) {
		EnumPropertyItem *item, *item_array = NULL;
		bool free;
		uiLayout *split = uiLayoutSplit(layout, 0.0f, false);
		uiLayout *column = uiLayoutColumn(split, layout->align);

		RNA_property_enum_items_gettexted(block->evil_C, &ptr, prop, &item_array, NULL, &free);
		for (item = item_array; item->identifier; item++) {
			if (item->identifier[0]) {
				PointerRNA tptr;

				WM_operator_properties_create_ptr(&tptr, ot);
				if (properties) {
					if (tptr.data) {
						IDP_FreeProperty(tptr.data);
						MEM_freeN(tptr.data);
					}
					tptr.data = IDP_CopyProperty(properties);
				}
				RNA_property_enum_set(&tptr, prop, item->value);

				uiItemFullO_ptr(column, ot, item->name, item->icon, tptr.data, context, flag);
				ui_but_tip_from_enum_item(block->buttons.last, item);
			}
			else {
				if (item->name) {
					uiBut *but;
					if (item != item_array) {
						column = uiLayoutColumn(split, layout->align);
						/* inconsistent, but menus with labels do not look good flipped */
						block->flag |= UI_BLOCK_NO_FLIP;
					}

					if (item->icon) {
						uiItemL(column, item->name, item->icon);
						but = block->buttons.last;
					}
					else {
						/* Do not use uiItemL here, as our root layout is a menu one, it will add a fake blank icon! */
						but = uiDefBut(block, LABEL, 0, item->name, 0, 0, UI_UNIT_X * 5, UI_UNIT_Y, NULL,
						               0.0, 0.0, 0, 0, "");
					}
					ui_but_tip_from_enum_item(but, item);
				}
				else {  /* XXX bug here, colums draw bottom item badly */
					uiItemS(column);
				}
			}
		}
		if (free) {
			MEM_freeN(item_array);
		}
	}
	else if (prop && RNA_property_type(prop) != PROP_ENUM) {
		RNA_warning("%s.%s, not an enum type", RNA_struct_identifier(ptr.type), propname);
		return;
	}
	else {
		RNA_warning("%s.%s not found", RNA_struct_identifier(ptr.type), propname);
		return;
	}
}

void uiItemsEnumO(uiLayout *layout, const char *opname, const char *propname)
{
	uiItemsFullEnumO(layout, opname, propname, NULL, layout->root->opcontext, 0);
}

/* for use in cases where we have */
void uiItemEnumO_value(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, int value)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0); /* print error next */
	PointerRNA ptr;
	PropertyRNA *prop;

	UI_OPERATOR_ERROR_RET(ot, opname, return );

	WM_operator_properties_create_ptr(&ptr, ot);

	/* enum lookup */
	if ((prop = RNA_struct_find_property(&ptr, propname))) {
		/* pass */
	}
	else {
		RNA_warning("%s.%s not found", RNA_struct_identifier(ptr.type), propname);
		return;
	}

	RNA_property_enum_set(&ptr, prop, value);

	/* same as uiItemEnumO */
	if (!name)
		name = ui_menu_enumpropname(layout, &ptr, prop, value);

	uiItemFullO_ptr(layout, ot, name, icon, ptr.data, layout->root->opcontext, 0);
}

void uiItemEnumO_string(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, const char *value_str)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0); /* print error next */
	PointerRNA ptr;
	PropertyRNA *prop;

	EnumPropertyItem *item;
	int value;
	bool free;

	UI_OPERATOR_ERROR_RET(ot, opname, return );

	WM_operator_properties_create_ptr(&ptr, ot);

	/* enum lookup */
	if ((prop = RNA_struct_find_property(&ptr, propname))) {
		/* no need for translations here */
		RNA_property_enum_items(layout->root->block->evil_C, &ptr, prop, &item, NULL, &free);
		if (item == NULL || RNA_enum_value_from_id(item, value_str, &value) == 0) {
			if (free) {
				MEM_freeN(item);
			}
			RNA_warning("%s.%s, enum %s not found", RNA_struct_identifier(ptr.type), propname, value_str);
			return;
		}

		if (free) {
			MEM_freeN(item);
		}
	}
	else {
		RNA_warning("%s.%s not found", RNA_struct_identifier(ptr.type), propname);
		return;
	}

	RNA_property_enum_set(&ptr, prop, value);

	/* same as uiItemEnumO */
	if (!name)
		name = ui_menu_enumpropname(layout, &ptr, prop, value);

	uiItemFullO_ptr(layout, ot, name, icon, ptr.data, layout->root->opcontext, 0);
}

void uiItemBooleanO(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, int value)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0); /* print error next */
	PointerRNA ptr;

	UI_OPERATOR_ERROR_RET(ot, opname, return );

	WM_operator_properties_create_ptr(&ptr, ot);
	RNA_boolean_set(&ptr, propname, value);

	uiItemFullO_ptr(layout, ot, name, icon, ptr.data, layout->root->opcontext, 0);
}

void uiItemIntO(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, int value)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0); /* print error next */
	PointerRNA ptr;

	UI_OPERATOR_ERROR_RET(ot, opname, return );

	WM_operator_properties_create_ptr(&ptr, ot);
	RNA_int_set(&ptr, propname, value);

	uiItemFullO_ptr(layout, ot, name, icon, ptr.data, layout->root->opcontext, 0);
}

void uiItemFloatO(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, float value)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0); /* print error next */
	PointerRNA ptr;

	UI_OPERATOR_ERROR_RET(ot, opname, return );

	WM_operator_properties_create_ptr(&ptr, ot);
	RNA_float_set(&ptr, propname, value);

	uiItemFullO_ptr(layout, ot, name, icon, ptr.data, layout->root->opcontext, 0);
}

void uiItemStringO(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, const char *value)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0); /* print error next */
	PointerRNA ptr;

	UI_OPERATOR_ERROR_RET(ot, opname, return );

	WM_operator_properties_create_ptr(&ptr, ot);
	RNA_string_set(&ptr, propname, value);

	uiItemFullO_ptr(layout, ot, name, icon, ptr.data, layout->root->opcontext, 0);
}

void uiItemO(uiLayout *layout, const char *name, int icon, const char *opname)
{
	uiItemFullO(layout, opname, name, icon, NULL, layout->root->opcontext, 0);
}

/* RNA property items */

static void ui_item_rna_size(uiLayout *layout, const char *name, int icon, PointerRNA *ptr, PropertyRNA *prop,
                             int index, int icon_only, int *r_w, int *r_h)
{
	PropertyType type;
	PropertySubType subtype;
	int len, w = 0, h;

	/* arbitrary extended width by type */
	type = RNA_property_type(prop);
	subtype = RNA_property_subtype(prop);
	len = RNA_property_array_length(ptr, prop);

	if (!name[0] && !icon_only) {
		if (ELEM(type, PROP_STRING, PROP_POINTER)) {
			name = "non-empty text";
		}
		else if (type == PROP_BOOLEAN) {
			icon = ICON_DOT;
		}
		else if (type == PROP_ENUM) {
			/* Find the longest enum item name, instead of using a dummy text! */
			EnumPropertyItem *item, *item_array;
			bool free;

			RNA_property_enum_items_gettexted(layout->root->block->evil_C, ptr, prop, &item_array, NULL, &free);
			for (item = item_array; item->identifier; item++) {
				if (item->identifier[0]) {
					w = max_ii(w, ui_text_icon_width(layout, item->name, item->icon, 0));
				}
			}
			if (free) {
				MEM_freeN(item_array);
			}
		}
	}

	if (!w)
		w = ui_text_icon_width(layout, name, icon, 0);
	h = UI_UNIT_Y;

	/* increase height for arrays */
	if (index == RNA_NO_INDEX && len > 0) {
		if (!name[0] && icon == ICON_NONE)
			h = 0;

		if (ELEM(subtype, PROP_LAYER, PROP_LAYER_MEMBER))
			h += 2 * UI_UNIT_Y;
		else if (subtype == PROP_MATRIX)
			h += ceilf(sqrtf(len)) * UI_UNIT_Y;
		else
			h += len * UI_UNIT_Y;
	}
	else if (ui_layout_vary_direction(layout) == UI_ITEM_VARY_X) {
		if (type == PROP_BOOLEAN && name[0])
			w += UI_UNIT_X / 5;
		else if (type == PROP_ENUM)
			w += UI_UNIT_X / 4;
		else if (type == PROP_FLOAT || type == PROP_INT)
			w += UI_UNIT_X * 3;
	}

	*r_w = w;
	*r_h = h;
}

void uiItemFullR(uiLayout *layout, PointerRNA *ptr, PropertyRNA *prop, int index, int value, int flag, const char *name, int icon)
{
	uiBlock *block = layout->root->block;
	uiBut *but = NULL;
	PropertyType type;
	char namestr[UI_MAX_NAME_STR];
	int len, w, h, slider, toggle, expand, icon_only, no_bg;
	bool is_array;

	uiBlockSetCurLayout(block, layout);

	/* retrieve info */
	type = RNA_property_type(prop);
	is_array = RNA_property_array_check(prop);
	len = (is_array) ? RNA_property_array_length(ptr, prop) : 0;

	/* set name and icon */
	if (!name) {
		if ((flag & UI_ITEM_R_ICON_ONLY) == 0) {
			name = RNA_property_ui_name(prop);
		}
		else {
			name = "";
		}
	}

	if (icon == ICON_NONE)
		icon = RNA_property_ui_icon(prop);
	
	if (flag & UI_ITEM_R_ICON_ONLY) {
		/* pass */
	}
	else if (ELEM(type, PROP_INT, PROP_FLOAT, PROP_STRING, PROP_POINTER)) {
		name = ui_item_name_add_colon(name, namestr);
	}
	else if (type == PROP_BOOLEAN && is_array && index == RNA_NO_INDEX) {
		name = ui_item_name_add_colon(name, namestr);
	}
	else if (type == PROP_ENUM && index != RNA_ENUM_VALUE) {
		name = ui_item_name_add_colon(name, namestr);
	}

	if (layout->root->type == UI_LAYOUT_MENU) {
		if (type == PROP_BOOLEAN && ((is_array == false) || (index != RNA_NO_INDEX))) {
			if (is_array) icon = (RNA_property_boolean_get_index(ptr, prop, index)) ? ICON_CHECKBOX_HLT : ICON_CHECKBOX_DEHLT;
			else icon = (RNA_property_boolean_get(ptr, prop)) ? ICON_CHECKBOX_HLT : ICON_CHECKBOX_DEHLT;
		}
		else if (type == PROP_ENUM && index == RNA_ENUM_VALUE) {
			int enum_value = RNA_property_enum_get(ptr, prop);
			if (RNA_property_flag(prop) & PROP_ENUM_FLAG) {
				icon = (enum_value & value) ? ICON_CHECKBOX_HLT : ICON_CHECKBOX_DEHLT;
			}
			else {
				icon = (enum_value == value) ? ICON_CHECKBOX_HLT : ICON_CHECKBOX_DEHLT;
			}
		}
	}

	slider = (flag & UI_ITEM_R_SLIDER);
	toggle = (flag & UI_ITEM_R_TOGGLE);
	expand = (flag & UI_ITEM_R_EXPAND);
	icon_only = (flag & UI_ITEM_R_ICON_ONLY);
	no_bg = (flag & UI_ITEM_R_NO_BG);

	/* get size */
	ui_item_rna_size(layout, name, icon, ptr, prop, index, icon_only, &w, &h);

	if (no_bg)
		uiBlockSetEmboss(block, UI_EMBOSSN);
	
	/* array property */
	if (index == RNA_NO_INDEX && is_array)
		ui_item_array(layout, block, name, icon, ptr, prop, len, 0, 0, w, h, expand, slider, toggle, icon_only);
	/* enum item */
	else if (type == PROP_ENUM && index == RNA_ENUM_VALUE) {
		if (icon && name[0] && !icon_only)
			uiDefIconTextButR_prop(block, ROW, 0, icon, name, 0, 0, w, h, ptr, prop, -1, 0, value, -1, -1, NULL);
		else if (icon)
			uiDefIconButR_prop(block, ROW, 0, icon, 0, 0, w, h, ptr, prop, -1, 0, value, -1, -1, NULL);
		else
			uiDefButR_prop(block, ROW, 0, name, 0, 0, w, h, ptr, prop, -1, 0, value, -1, -1, NULL);
	}
	/* expanded enum */
	else if (type == PROP_ENUM && (expand || RNA_property_flag(prop) & PROP_ENUM_FLAG))
		ui_item_enum_expand(layout, block, ptr, prop, name, h, icon_only);
	/* property with separate label */
	else if (type == PROP_ENUM || type == PROP_STRING || type == PROP_POINTER) {
		but = ui_item_with_label(layout, block, name, icon, ptr, prop, index, 0, 0, w, h, flag);
		ui_but_add_search(but, ptr, prop, NULL, NULL);
		
		if (layout->redalert)
			uiButSetFlag(but, UI_BUT_REDALERT);
	}
	/* single button */
	else {
		but = uiDefAutoButR(block, ptr, prop, index, name, icon, 0, 0, w, h);

		if (slider && but->type == NUM)
			but->type = NUMSLI;

		if (toggle && but->type == OPTION)
			but->type = TOG;
		
		if (layout->redalert)
			uiButSetFlag(but, UI_BUT_REDALERT);
	}

	/* Mark non-embossed textfields inside a listbox. */
	if (but && (block->flag & UI_BLOCK_LIST_ITEM) && (but->type == TEX) && (but->dt & UI_EMBOSSN)) {
		uiButSetFlag(but, UI_BUT_LIST_ITEM);
	}

	if (no_bg)
		uiBlockSetEmboss(block, UI_EMBOSS);

	/* ensure text isn't added to icon_only buttons */
	if (but && icon_only) {
		BLI_assert(but->str[0] == '\0');
	}

}

void uiItemR(uiLayout *layout, PointerRNA *ptr, const char *propname, int flag, const char *name, int icon)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

	if (!prop) {
		ui_item_disabled(layout, propname);
		RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}

	uiItemFullR(layout, ptr, prop, RNA_NO_INDEX, 0, flag, name, icon);
}

void uiItemEnumR(uiLayout *layout, const char *name, int icon, struct PointerRNA *ptr, const char *propname, int value)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

	if (!prop || RNA_property_type(prop) != PROP_ENUM) {
		ui_item_disabled(layout, propname);
		RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}

	uiItemFullR(layout, ptr, prop, RNA_ENUM_VALUE, value, 0, name, icon);
}

void uiItemEnumR_string(uiLayout *layout, struct PointerRNA *ptr, const char *propname, const char *value, const char *name, int icon)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
	EnumPropertyItem *item;
	int ivalue, a;
	bool free;

	if (!prop || RNA_property_type(prop) != PROP_ENUM) {
		ui_item_disabled(layout, propname);
		RNA_warning("enum property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}

	RNA_property_enum_items(layout->root->block->evil_C, ptr, prop, &item, NULL, &free);

	if (!RNA_enum_value_from_id(item, value, &ivalue)) {
		if (free) {
			MEM_freeN(item);
		}
		ui_item_disabled(layout, propname);
		RNA_warning("enum property value not found: %s", value);
		return;
	}

	for (a = 0; item[a].identifier; a++) {
		if (item[a].value == ivalue) {
			const char *item_name = CTX_IFACE_(RNA_property_translation_context(prop), item[a].name);

			uiItemFullR(layout, ptr, prop, RNA_ENUM_VALUE, ivalue, 0, item_name ? item_name : name, icon ? icon : item[a].icon);
			break;
		}
	}

	if (free) {
		MEM_freeN(item);
	}
}

void uiItemsEnumR(uiLayout *layout, struct PointerRNA *ptr, const char *propname)
{
	PropertyRNA *prop;
	uiBlock *block = layout->root->block;
	uiBut *bt;

	prop = RNA_struct_find_property(ptr, propname);

	if (!prop) {
		ui_item_disabled(layout, propname);
		RNA_warning("enum property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}

	if (RNA_property_type(prop) != PROP_ENUM) {
		RNA_warning("not an enum property: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}
	else {
		EnumPropertyItem *item;
		int totitem, i;
		bool free;
		uiLayout *split = uiLayoutSplit(layout, 0.0f, false);
		uiLayout *column = uiLayoutColumn(split, false);

		RNA_property_enum_items_gettexted(block->evil_C, ptr, prop, &item, &totitem, &free);

		for (i = 0; i < totitem; i++) {
			if (item[i].identifier[0]) {
				uiItemEnumR(column, item[i].name, ICON_NONE, ptr, propname, item[i].value);
				ui_but_tip_from_enum_item(block->buttons.last, &item[i]);
			}
			else {
				if (item[i].name) {
					if (i != 0) {
						column = uiLayoutColumn(split, false);
						/* inconsistent, but menus with labels do not look good flipped */
						block->flag |= UI_BLOCK_NO_FLIP;
					}

					uiItemL(column, item[i].name, ICON_NONE);
					bt = block->buttons.last;
					bt->drawflag = UI_BUT_TEXT_LEFT;

					ui_but_tip_from_enum_item(bt, &item[i]);
				}
				else
					uiItemS(column);
			}
		}

		if (free) {
			MEM_freeN(item);
		}
	}
}

/* Pointer RNA button with search */

typedef struct CollItemSearch {
	struct CollItemSearch *next, *prev;
	char *name;
	int index;
	int iconid;
} CollItemSearch;

static int sort_search_items_list(void *a, void *b)
{
	CollItemSearch *cis1 = (CollItemSearch *)a;
	CollItemSearch *cis2 = (CollItemSearch *)b;
	
	if (BLI_strcasecmp(cis1->name, cis2->name) > 0)
		return 1;
	else
		return 0;
}

static void rna_search_cb(const struct bContext *C, void *arg_but, const char *str, uiSearchItems *items)
{
	uiBut *but = arg_but;
	char *name;
	int i = 0, iconid = 0, flag = RNA_property_flag(but->rnaprop);
	ListBase *items_list = MEM_callocN(sizeof(ListBase), "items_list");
	CollItemSearch *cis;
	const int skip_filter = !but->changed;

	/* build a temporary list of relevant items first */
	RNA_PROP_BEGIN (&but->rnasearchpoin, itemptr, but->rnasearchprop)
	{
		if (flag & PROP_ID_SELF_CHECK)
			if (itemptr.data == but->rnapoin.id.data)
				continue;

		/* use filter */
		if (RNA_property_type(but->rnaprop) == PROP_POINTER) {
			if (RNA_property_pointer_poll(&but->rnapoin, but->rnaprop, &itemptr) == 0)
				continue;
		}

		if (itemptr.type && RNA_struct_is_ID(itemptr.type)) {
			ID *id = itemptr.data;
			char name_ui[MAX_ID_NAME];

#if 0       /* this name is used for a string comparison and can't be modified, TODO */
			/* if ever enabled, make name_ui be MAX_ID_NAME+1 */
			name_uiprefix_id(name_ui, id);
#else
			BLI_strncpy(name_ui, id->name + 2, sizeof(name_ui));
#endif
			name = BLI_strdup(name_ui);
			iconid = ui_id_icon_get((bContext *)C, id, false);
		}
		else {
			name = RNA_struct_name_get_alloc(&itemptr, NULL, 0, NULL); /* could use the string length here */
			iconid = 0;
		}

		if (name) {
			if (skip_filter || BLI_strcasestr(name, str)) {
				cis = MEM_callocN(sizeof(CollItemSearch), "CollectionItemSearch");
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
	
	BLI_sortlist(items_list, sort_search_items_list);
	
	/* add search items from temporary list */
	for (cis = items_list->first; cis; cis = cis->next) {
		if (false == uiSearchItemAdd(items, cis->name, SET_INT_IN_POINTER(cis->index), cis->iconid)) {
			break;
		}
	}

	for (cis = items_list->first; cis; cis = cis->next) {
		MEM_freeN(cis->name);
	}
	BLI_freelistN(items_list);
	MEM_freeN(items_list);
}

static void search_id_collection(StructRNA *ptype, PointerRNA *ptr, PropertyRNA **prop)
{
	StructRNA *srna;

	/* look for collection property in Main */
	RNA_main_pointer_create(G.main, ptr);

	*prop = NULL;

	RNA_STRUCT_BEGIN (ptr, iprop)
	{
		/* if it's a collection and has same pointer type, we've got it */
		if (RNA_property_type(iprop) == PROP_COLLECTION) {
			srna = RNA_property_pointer_type(ptr, iprop);

			if (ptype == srna) {
				*prop = iprop;
				break;
			}
		}
	}
	RNA_STRUCT_END;
}

void ui_but_add_search(uiBut *but, PointerRNA *ptr, PropertyRNA *prop, PointerRNA *searchptr, PropertyRNA *searchprop)
{
	StructRNA *ptype;
	PointerRNA sptr;

	/* for ID's we do automatic lookup */
	if (!searchprop) {
		if (RNA_property_type(prop) == PROP_POINTER) {
			ptype = RNA_property_pointer_type(ptr, prop);
			search_id_collection(ptype, &sptr, &searchprop);
			searchptr = &sptr;
		}
	}

	/* turn button into search button */
	if (searchprop) {
		but->type = RNA_property_is_unlink(prop) ? SEARCH_MENU_UNLINK : SEARCH_MENU;
		but->hardmax = MAX2(but->hardmax, 256.0f);
		but->rnasearchpoin = *searchptr;
		but->rnasearchprop = searchprop;
		but->drawflag |= UI_BUT_ICON_LEFT | UI_BUT_TEXT_LEFT;

		if (RNA_property_type(prop) == PROP_ENUM) {
			/* XXX, this will have a menu string,
			 * but in this case we just want the text */
			but->str[0] = 0;
		}

		uiButSetSearchFunc(but, rna_search_cb, but, NULL, NULL);
	}
}

void uiItemPointerR(uiLayout *layout, struct PointerRNA *ptr, const char *propname, struct PointerRNA *searchptr, const char *searchpropname, const char *name, int icon)
{
	PropertyRNA *prop, *searchprop;
	PropertyType type;
	uiBut *but;
	uiBlock *block;
	StructRNA *icontype;
	int w, h;
	char namestr[UI_MAX_NAME_STR];
	
	/* validate arguments */
	prop = RNA_struct_find_property(ptr, propname);

	if (!prop) {
		RNA_warning("property not found: %s.%s",
		            RNA_struct_identifier(ptr->type), propname);
		return;
	}
	
	type = RNA_property_type(prop);
	if (!ELEM(type, PROP_POINTER, PROP_STRING, PROP_ENUM)) {
		RNA_warning("Property %s must be a pointer, string or enum", propname);
		return;
	}

	searchprop = RNA_struct_find_property(searchptr, searchpropname);


	if (!searchprop) {
		RNA_warning("search collection property not found: %s.%s",
		            RNA_struct_identifier(searchptr->type), searchpropname);
		return;
	}
	else if (RNA_property_type(searchprop) != PROP_COLLECTION) {
		RNA_warning("search collection property is not a collection type: %s.%s",
		            RNA_struct_identifier(searchptr->type), searchpropname);
		return;
	}

	/* get icon & name */
	if (icon == ICON_NONE) {
		if (type == PROP_POINTER)
			icontype = RNA_property_pointer_type(ptr, prop);
		else
			icontype = RNA_property_pointer_type(searchptr, searchprop);

		icon = RNA_struct_ui_icon(icontype);
	}
	if (!name)
		name = RNA_property_ui_name(prop);

	name = ui_item_name_add_colon(name, namestr);

	/* create button */
	block = uiLayoutGetBlock(layout);

	ui_item_rna_size(layout, name, icon, ptr, prop, 0, 0, &w, &h);
	w += UI_UNIT_X; /* X icon needs more space */
	but = ui_item_with_label(layout, block, name, icon, ptr, prop, 0, 0, 0, w, h, 0);

	ui_but_add_search(but, ptr, prop, searchptr, searchprop);
}

/* menu item */
static void ui_item_menutype_func(bContext *C, uiLayout *layout, void *arg_mt)
{
	MenuType *mt = (MenuType *)arg_mt;
	Menu menu = {NULL};

	menu.type = mt;
	menu.layout = layout;

	if (G.debug & G_DEBUG_WM) {
		printf("%s: opening menu \"%s\"\n", __func__, mt->idname);
	}

	if (layout->context)
		CTX_store_set(C, layout->context);

	mt->draw(C, &menu);

	if (layout->context)
		CTX_store_set(C, NULL);
}

static uiBut *ui_item_menu(uiLayout *layout, const char *name, int icon, uiMenuCreateFunc func, void *arg, void *argN,
                           const char *tip, bool force_menu)
{
	uiBlock *block = layout->root->block;
	uiBut *but;
	int w, h;

	uiBlockSetCurLayout(block, layout);

	if (layout->root->type == UI_LAYOUT_HEADER)
		uiBlockSetEmboss(block, UI_EMBOSS);

	if (!name)
		name = "";
	if (layout->root->type == UI_LAYOUT_MENU && !icon)
		icon = ICON_BLANK1;

	w = ui_text_icon_width(layout, name, icon, 1);
	h = UI_UNIT_Y;

	if (layout->root->type == UI_LAYOUT_HEADER) { /* ugly .. */
		if (force_menu) {
			w += UI_UNIT_Y;
		}
		else {
			if (name[0]) {
				w -= UI_UNIT_Y / 2;
			}
		}
	}

	if (name[0] && icon)
		but = uiDefIconTextMenuBut(block, func, arg, icon, name, 0, 0, w, h, tip);
	else if (icon)
		but = uiDefIconMenuBut(block, func, arg, icon, 0, 0, w, h, tip);
	else
		but = uiDefMenuBut(block, func, arg, name, 0, 0, w, h, tip);

	if (argN) { /* ugly .. */
		but->poin = (char *)but;
		but->func_argN = argN;
	}

	if (layout->root->type == UI_LAYOUT_HEADER) {
		uiBlockSetEmboss(block, UI_EMBOSS);
	}
	if (ELEM(layout->root->type, UI_LAYOUT_PANEL, UI_LAYOUT_TOOLBAR) ||
	    (force_menu && layout->root->type != UI_LAYOUT_MENU))  /* We never want a dropdown in menu! */
	{
		uiButSetMenuFromPulldown(but);
	}

	return but;
}

void uiItemM(uiLayout *layout, bContext *UNUSED(C), const char *menuname, const char *name, int icon)
{
	MenuType *mt;

	mt = WM_menutype_find(menuname, false);

	if (mt == NULL) {
		RNA_warning("not found %s", menuname);
		return;
	}

	if (!name) {
		name = CTX_IFACE_(mt->translation_context, mt->label);
	}

	if (layout->root->type == UI_LAYOUT_MENU && !icon)
		icon = ICON_BLANK1;

	ui_item_menu(layout, name, icon, ui_item_menutype_func, mt, NULL, TIP_(mt->description), false);
}

/* label item */
static uiBut *uiItemL_(uiLayout *layout, const char *name, int icon)
{
	uiBlock *block = layout->root->block;
	uiBut *but;
	int w;

	uiBlockSetCurLayout(block, layout);

	if (!name)
		name = "";
	if (layout->root->type == UI_LAYOUT_MENU && !icon)
		icon = ICON_BLANK1;

	w = ui_text_icon_width(layout, name, icon, 0);

	if (icon && name[0])
		but = uiDefIconTextBut(block, LABEL, 0, icon, name, 0, 0, w, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
	else if (icon)
		but = uiDefIconBut(block, LABEL, 0, icon, 0, 0, w, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
	else
		but = uiDefBut(block, LABEL, 0, name, 0, 0, w, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");

	/* to compensate for string size padding in ui_text_icon_width,
	 * make text aligned right if the layout is aligned right.
	 */
	if (uiLayoutGetAlignment(layout) == UI_LAYOUT_ALIGN_RIGHT) {
		but->drawflag &= ~UI_BUT_TEXT_LEFT;	/* default, needs to be unset */
		but->drawflag |= UI_BUT_TEXT_RIGHT;
	}

	/* Mark as a label inside a listbox. */
	if (block->flag & UI_BLOCK_LIST_ITEM) {
		but->flag |= UI_BUT_LIST_ITEM;
	}

	return but;
}

void uiItemL(uiLayout *layout, const char *name, int icon)
{
	uiItemL_(layout, name, icon);
}

void uiItemLDrag(uiLayout *layout, PointerRNA *ptr, const char *name, int icon)
{
	uiBut *but = uiItemL_(layout, name, icon);

	if (ptr && ptr->type)
		if (RNA_struct_is_ID(ptr->type))
			uiButSetDragID(but, ptr->id.data);
}


/* value item */
void uiItemV(uiLayout *layout, const char *name, int icon, int argval)
{
	/* label */
	uiBlock *block = layout->root->block;
	int *retvalue = (block->handle) ? &block->handle->retvalue : NULL;
	int w;

	uiBlockSetCurLayout(block, layout);

	if (!name)
		name = "";
	if (layout->root->type == UI_LAYOUT_MENU && !icon)
		icon = ICON_BLANK1;

	w = ui_text_icon_width(layout, name, icon, 0);

	if (icon && name[0])
		uiDefIconTextButI(block, BUT, argval, icon, name, 0, 0, w, UI_UNIT_Y, retvalue, 0.0, 0.0, 0, -1, "");
	else if (icon)
		uiDefIconButI(block, BUT, argval, icon, 0, 0, w, UI_UNIT_Y, retvalue, 0.0, 0.0, 0, -1, "");
	else
		uiDefButI(block, BUT, argval, name, 0, 0, w, UI_UNIT_Y, retvalue, 0.0, 0.0, 0, -1, "");
}

/* separator item */
void uiItemS(uiLayout *layout)
{
	uiBlock *block = layout->root->block;
	bool is_menu = ui_block_is_menu(block);
	int space = (is_menu) ? 0.45f * UI_UNIT_X : 0.3f * UI_UNIT_X;

	uiBlockSetCurLayout(block, layout);
	uiDefBut(block, (is_menu) ? SEPRLINE : SEPR, 0, "", 0, 0, space, space, NULL, 0.0, 0.0, 0, 0, "");
}

/* level items */
void uiItemMenuF(uiLayout *layout, const char *name, int icon, uiMenuCreateFunc func, void *arg)
{
	if (!func)
		return;

	ui_item_menu(layout, name, icon, func, arg, NULL, "", false);
}

typedef struct MenuItemLevel {
	int opcontext;
	/* don't use pointers to the strings because python can dynamically
	 * allocate strings and free before the menu draws, see [#27304] */
	char opname[OP_MAX_TYPENAME];
	char propname[MAX_IDPROP_NAME];
	PointerRNA rnapoin;
} MenuItemLevel;

static void menu_item_enum_opname_menu(bContext *UNUSED(C), uiLayout *layout, void *arg)
{
	MenuItemLevel *lvl = (MenuItemLevel *)(((uiBut *)arg)->func_argN);

	uiLayoutSetOperatorContext(layout, lvl->opcontext);
	uiItemsEnumO(layout, lvl->opname, lvl->propname);

	/* override default, needed since this was assumed pre 2.70 */
	uiBlockSetDirection(layout->root->block, UI_DOWN);
}

void uiItemMenuEnumO(uiLayout *layout, bContext *C, const char *opname, const char *propname, const char *name, int icon)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0); /* print error next */
	MenuItemLevel *lvl;
	uiBut *but;

	UI_OPERATOR_ERROR_RET(ot, opname, return );

	if (!ot->srna) {
		ui_item_disabled(layout, opname);
		RNA_warning("operator missing srna '%s'", opname);
		return;
	}

	if (name == NULL) {
		name = RNA_struct_ui_name(ot->srna);
	}

	if (layout->root->type == UI_LAYOUT_MENU && !icon)
		icon = ICON_BLANK1;

	lvl = MEM_callocN(sizeof(MenuItemLevel), "MenuItemLevel");
	BLI_strncpy(lvl->opname, opname, sizeof(lvl->opname));
	BLI_strncpy(lvl->propname, propname, sizeof(lvl->propname));
	lvl->opcontext = layout->root->opcontext;

	but = ui_item_menu(layout, name, icon, menu_item_enum_opname_menu, NULL, lvl,
	                   RNA_struct_ui_description(ot->srna), true);

	/* add hotkey here, lower UI code can't detect it */
	if ((layout->root->block->flag & UI_BLOCK_LOOP) &&
	    (ot->prop && ot->invoke))
	{
		char keybuf[128];
		if (WM_key_event_operator_string(C, ot->idname, layout->root->opcontext, NULL, false,
		                                 keybuf, sizeof(keybuf)))
		{
			ui_but_add_shortcut(but, keybuf, false);
		}
	}
}

static void menu_item_enum_rna_menu(bContext *UNUSED(C), uiLayout *layout, void *arg)
{
	MenuItemLevel *lvl = (MenuItemLevel *)(((uiBut *)arg)->func_argN);

	uiLayoutSetOperatorContext(layout, lvl->opcontext);
	uiItemsEnumR(layout, &lvl->rnapoin, lvl->propname);
}

void uiItemMenuEnumR(uiLayout *layout, struct PointerRNA *ptr, const char *propname, const char *name, int icon)
{
	MenuItemLevel *lvl;
	PropertyRNA *prop;

	prop = RNA_struct_find_property(ptr, propname);
	if (!prop) {
		ui_item_disabled(layout, propname);
		RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}

	if (!name)
		name = RNA_property_ui_name(prop);
	if (layout->root->type == UI_LAYOUT_MENU && !icon)
		icon = ICON_BLANK1;

	lvl = MEM_callocN(sizeof(MenuItemLevel), "MenuItemLevel");
	lvl->rnapoin = *ptr;
	BLI_strncpy(lvl->propname, propname, sizeof(lvl->propname));
	lvl->opcontext = layout->root->opcontext;

	ui_item_menu(layout, name, icon, menu_item_enum_rna_menu, NULL, lvl, RNA_property_description(prop), false);
}

/**************************** Layout Items ***************************/

/* single-row layout */
static void ui_litem_estimate_row(uiLayout *litem)
{
	uiItem *item;
	int itemw, itemh;

	litem->w = 0;
	litem->h = 0;

	for (item = litem->items.first; item; item = item->next) {
		ui_item_size(item, &itemw, &itemh);

		litem->w += itemw;
		litem->h = MAX2(itemh, litem->h);

		if (item->next)
			litem->w += litem->space;
	}
}

static int ui_litem_min_width(int itemw)
{
	return MIN2(2 * UI_UNIT_X, itemw);
}

static void ui_litem_layout_row(uiLayout *litem)
{
	uiItem *item;
	int x, y, w, tot, totw, neww, newtotw, itemw, minw, itemh, offset;
	int fixedw, freew, fixedx, freex, flag = 0, lastw = 0;

	/* x = litem->x; */ /* UNUSED */
	y = litem->y;
	w = litem->w;
	totw = 0;
	tot = 0;

	for (item = litem->items.first; item; item = item->next) {
		ui_item_size(item, &itemw, &itemh);
		totw += itemw;
		tot++;
	}

	if (totw == 0)
		return;
	
	if (w != 0)
		w -= (tot - 1) * litem->space;
	fixedw = 0;

	/* keep clamping items to fixed minimum size until all are done */
	do {
		freew = 0;
		x = 0;
		flag = 0;
		newtotw = totw;

		for (item = litem->items.first; item; item = item->next) {
			if (item->flag)
				continue;

			ui_item_size(item, &itemw, &itemh);
			minw = ui_litem_min_width(itemw);

			if (w - lastw > 0)
				neww = ui_item_fit(itemw, x, totw, w - lastw, !item->next, litem->alignment, NULL);
			else
				neww = 0;  /* no space left, all will need clamping to minimum size */

			x += neww;

			if ((neww < minw || itemw == minw) && w != 0) {
				/* fixed size */
				item->flag = 1;
				fixedw += minw;
				flag = 1;
				newtotw -= itemw;
			}
			else {
				/* keep free size */
				item->flag = 0;
				freew += itemw;
			}
		}

		totw = newtotw;
		lastw = fixedw;
	} while (flag);

	freex = 0;
	fixedx = 0;
	x = litem->x;

	for (item = litem->items.first; item; item = item->next) {
		ui_item_size(item, &itemw, &itemh);
		minw = ui_litem_min_width(itemw);

		if (item->flag) {
			/* fixed minimum size items */
			itemw = ui_item_fit(minw, fixedx, fixedw, min_ii(w, fixedw), !item->next, litem->alignment, NULL);
			fixedx += itemw;
		}
		else {
			/* free size item */
			itemw = ui_item_fit(itemw, freex, freew, w - fixedw, !item->next, litem->alignment, NULL);
			freex += itemw;
		}

		/* align right/center */
		offset = 0;
		if (litem->alignment == UI_LAYOUT_ALIGN_RIGHT) {
			if (freew + fixedw > 0 && freew + fixedw < w)
				offset = w - (fixedw + freew);
		}
		else if (litem->alignment == UI_LAYOUT_ALIGN_CENTER) {
			if (freew + fixedw > 0 && freew + fixedw < w)
				offset = (w - (fixedw + freew)) / 2;
		}

		/* position item */
		ui_item_position(item, x + offset, y - itemh, itemw, itemh);

		x += itemw;
		if (item->next)
			x += litem->space;
	}

	litem->w = x - litem->x;
	litem->h = litem->y - y;
	litem->x = x;
	litem->y = y;
}

/* single-column layout */
static void ui_litem_estimate_column(uiLayout *litem)
{
	uiItem *item;
	int itemw, itemh;

	litem->w = 0;
	litem->h = 0;

	for (item = litem->items.first; item; item = item->next) {
		ui_item_size(item, &itemw, &itemh);

		litem->w = MAX2(litem->w, itemw);
		litem->h += itemh;

		if (item->next)
			litem->h += litem->space;
	}
}

static void ui_litem_layout_column(uiLayout *litem)
{
	uiItem *item;
	int itemh, x, y;

	x = litem->x;
	y = litem->y;

	for (item = litem->items.first; item; item = item->next) {
		ui_item_size(item, NULL, &itemh);

		y -= itemh;
		ui_item_position(item, x, y, litem->w, itemh);

		if (item->next)
			y -= litem->space;
	}

	litem->h = litem->y - y;
	litem->x = x;
	litem->y = y;
}

/* root layout */
static void ui_litem_estimate_root(uiLayout *UNUSED(litem))
{
	/* nothing to do */
}

static void ui_litem_layout_root(uiLayout *litem)
{
	if (litem->root->type == UI_LAYOUT_HEADER)
		ui_litem_layout_row(litem);
	else
		ui_litem_layout_column(litem);
}

/* box layout */
static void ui_litem_estimate_box(uiLayout *litem)
{
	uiStyle *style = litem->root->style;

	ui_litem_estimate_column(litem);
	litem->w += 2 * style->boxspace;
	litem->h += style->boxspace;
}

static void ui_litem_layout_box(uiLayout *litem)
{
	uiLayoutItemBx *box = (uiLayoutItemBx *)litem;
	uiStyle *style = litem->root->style;
	uiBut *but;
	int w, h;

	w = litem->w;
	h = litem->h;

	litem->x += style->boxspace;

	if (w != 0) litem->w -= 2 * style->boxspace;
	if (h != 0) litem->h -= 2 * style->boxspace;

	ui_litem_layout_column(litem);

	litem->x -= style->boxspace;
	litem->y -= style->boxspace;

	if (w != 0) litem->w += 2 * style->boxspace;
	if (h != 0) litem->h += style->boxspace;

	/* roundbox around the sublayout */
	but = box->roundbox;
	but->rect.xmin = litem->x;
	but->rect.ymin = litem->y;
	but->rect.xmax = litem->x + litem->w;
	but->rect.ymax = litem->y + litem->h;
}

/* multi-column layout, automatically flowing to the next */
static void ui_litem_estimate_column_flow(uiLayout *litem)
{
	uiStyle *style = litem->root->style;
	uiLayoutItemFlow *flow = (uiLayoutItemFlow *)litem;
	uiItem *item;
	int col, x, y, emh, emy, miny, itemw, itemh, maxw = 0;
	int toth, totitem;

	/* compute max needed width and total height */
	toth = 0;
	totitem = 0;
	for (item = litem->items.first; item; item = item->next) {
		ui_item_size(item, &itemw, &itemh);
		maxw = MAX2(maxw, itemw);
		toth += itemh;
		totitem++;
	}

	if (flow->number <= 0) {
		/* auto compute number of columns, not very good */
		if (maxw == 0) {
			flow->totcol = 1;
			return;
		}

		flow->totcol = max_ii(litem->root->emw / maxw, 1);
		flow->totcol = min_ii(flow->totcol, totitem);
	}
	else
		flow->totcol = flow->number;

	/* compute sizes */
	x = 0;
	y = 0;
	emy = 0;
	miny = 0;

	maxw = 0;
	emh = toth / flow->totcol;

	/* create column per column */
	col = 0;
	for (item = litem->items.first; item; item = item->next) {
		ui_item_size(item, &itemw, &itemh);

		y -= itemh + style->buttonspacey;
		miny = min_ii(miny, y);
		emy -= itemh;
		maxw = max_ii(itemw, maxw);

		/* decide to go to next one */
		if (col < flow->totcol - 1 && emy <= -emh) {
			x += maxw + litem->space;
			maxw = 0;
			y = 0;
			emy = 0; /* need to reset height again for next column */
			col++;
		}
	}

	litem->w = x;
	litem->h = litem->y - miny;
}

static void ui_litem_layout_column_flow(uiLayout *litem)
{
	uiStyle *style = litem->root->style;
	uiLayoutItemFlow *flow = (uiLayoutItemFlow *)litem;
	uiItem *item;
	int col, x, y, w, emh, emy, miny, itemw, itemh;
	int toth, totitem, offset;

	/* compute max needed width and total height */
	toth = 0;
	totitem = 0;
	for (item = litem->items.first; item; item = item->next) {
		ui_item_size(item, &itemw, &itemh);
		toth += itemh;
		totitem++;
	}

	/* compute sizes */
	x = litem->x;
	y = litem->y;
	emy = 0;
	miny = 0;

	w = litem->w - (flow->totcol - 1) * style->columnspace;
	emh = toth / flow->totcol;

	/* create column per column */
	col = 0;
	for (item = litem->items.first; item; item = item->next) {
		ui_item_size(item, NULL, &itemh);
		itemw = ui_item_fit(1, x - litem->x, flow->totcol, w, col == flow->totcol - 1, litem->alignment, &offset);
	
		y -= itemh;
		emy -= itemh;
		ui_item_position(item, x + offset, y, itemw, itemh);
		y -= style->buttonspacey;
		miny = min_ii(miny, y);

		/* decide to go to next one */
		if (col < flow->totcol - 1 && emy <= -emh) {
			x += itemw + style->columnspace;
			y = litem->y;
			emy = 0; /* need to reset height again for next column */
			col++;
		}
	}

	litem->h = litem->y - miny;
	litem->x = x;
	litem->y = miny;
}

/* free layout */
static void ui_litem_estimate_absolute(uiLayout *litem)
{
	uiItem *item;
	int itemx, itemy, itemw, itemh, minx, miny;

	minx = 1e6;
	miny = 1e6;
	litem->w = 0;
	litem->h = 0;

	for (item = litem->items.first; item; item = item->next) {
		ui_item_offset(item, &itemx, &itemy);
		ui_item_size(item, &itemw, &itemh);

		minx = min_ii(minx, itemx);
		miny = min_ii(miny, itemy);

		litem->w = MAX2(litem->w, itemx + itemw);
		litem->h = MAX2(litem->h, itemy + itemh);
	}

	litem->w -= minx;
	litem->h -= miny;
}

static void ui_litem_layout_absolute(uiLayout *litem)
{
	uiItem *item;
	float scalex = 1.0f, scaley = 1.0f;
	int x, y, newx, newy, itemx, itemy, itemh, itemw, minx, miny, totw, toth;

	minx = 1e6;
	miny = 1e6;
	totw = 0;
	toth = 0;

	for (item = litem->items.first; item; item = item->next) {
		ui_item_offset(item, &itemx, &itemy);
		ui_item_size(item, &itemw, &itemh);

		minx = min_ii(minx, itemx);
		miny = min_ii(miny, itemy);

		totw = max_ii(totw, itemx + itemw);
		toth = max_ii(toth, itemy + itemh);
	}

	totw -= minx;
	toth -= miny;

	if (litem->w && totw > 0)
		scalex = (float)litem->w / (float)totw;
	if (litem->h && toth > 0)
		scaley = (float)litem->h / (float)toth;
	
	x = litem->x;
	y = litem->y - scaley * toth;

	for (item = litem->items.first; item; item = item->next) {
		ui_item_offset(item, &itemx, &itemy);
		ui_item_size(item, &itemw, &itemh);

		if (scalex != 1.0f) {
			newx = (itemx - minx) * scalex;
			itemw = (itemx - minx + itemw) * scalex - newx;
			itemx = minx + newx;
		}

		if (scaley != 1.0f) {
			newy = (itemy - miny) * scaley;
			itemh = (itemy - miny + itemh) * scaley - newy;
			itemy = miny + newy;
		}

		ui_item_position(item, x + itemx - minx, y + itemy - miny, itemw, itemh);
	}

	litem->w = scalex * totw;
	litem->h = litem->y - y;
	litem->x = x + litem->w;
	litem->y = y;
}

/* split layout */
static void ui_litem_estimate_split(uiLayout *litem)
{
	ui_litem_estimate_row(litem);
}

static void ui_litem_layout_split(uiLayout *litem)
{
	uiLayoutItemSplit *split = (uiLayoutItemSplit *)litem;
	uiItem *item;
	float percentage;
	const int tot = BLI_countlist(&litem->items);
	int itemh, x, y, w, colw = 0;

	if (tot == 0)
		return;

	x = litem->x;
	y = litem->y;

	percentage = (split->percentage == 0.0f) ? 1.0f / (float)tot : split->percentage;
	
	w = (litem->w - (tot - 1) * litem->space);
	colw = w * percentage;
	colw = MAX2(colw, 0);

	for (item = litem->items.first; item; item = item->next) {
		ui_item_size(item, NULL, &itemh);

		ui_item_position(item, x, y - itemh, colw, itemh);
		x += colw;

		if (item->next) {
			colw = (w - (int)(w * percentage)) / (tot - 1);
			colw = MAX2(colw, 0);

			x += litem->space;
		}
	}

	litem->w = x - litem->x;
	litem->h = litem->y - y;
	litem->x = x;
	litem->y = y;
}

/* overlap layout */
static void ui_litem_estimate_overlap(uiLayout *litem)
{
	uiItem *item;
	int itemw, itemh;

	litem->w = 0;
	litem->h = 0;

	for (item = litem->items.first; item; item = item->next) {
		ui_item_size(item, &itemw, &itemh);

		litem->w = MAX2(itemw, litem->w);
		litem->h = MAX2(itemh, litem->h);
	}
}

static void ui_litem_layout_overlap(uiLayout *litem)
{
	uiItem *item;
	int itemw, itemh, x, y;

	x = litem->x;
	y = litem->y;

	for (item = litem->items.first; item; item = item->next) {
		ui_item_size(item, &itemw, &itemh);
		ui_item_position(item, x, y - itemh, litem->w, itemh);

		litem->h = MAX2(litem->h, itemh);
	}

	litem->x = x;
	litem->y = y - litem->h;
}

/* layout create functions */
uiLayout *uiLayoutRow(uiLayout *layout, int align)
{
	uiLayout *litem;

	litem = MEM_callocN(sizeof(uiLayout), "uiLayoutRow");
	litem->item.type = ITEM_LAYOUT_ROW;
	litem->root = layout->root;
	litem->align = align;
	litem->active = true;
	litem->enabled = true;
	litem->context = layout->context;
	litem->space = (align) ? 0 : layout->root->style->buttonspacex;
	litem->redalert = layout->redalert;
	litem->w = layout->w;
	BLI_addtail(&layout->items, litem);

	uiBlockSetCurLayout(layout->root->block, litem);

	return litem;
}

uiLayout *uiLayoutColumn(uiLayout *layout, int align)
{
	uiLayout *litem;

	litem = MEM_callocN(sizeof(uiLayout), "uiLayoutColumn");
	litem->item.type = ITEM_LAYOUT_COLUMN;
	litem->root = layout->root;
	litem->align = align;
	litem->active = true;
	litem->enabled = true;
	litem->context = layout->context;
	litem->space = (litem->align) ? 0 : layout->root->style->buttonspacey;
	litem->redalert = layout->redalert;
	litem->w = layout->w;
	BLI_addtail(&layout->items, litem);

	uiBlockSetCurLayout(layout->root->block, litem);

	return litem;
}

uiLayout *uiLayoutColumnFlow(uiLayout *layout, int number, int align)
{
	uiLayoutItemFlow *flow;

	flow = MEM_callocN(sizeof(uiLayoutItemFlow), "uiLayoutItemFlow");
	flow->litem.item.type = ITEM_LAYOUT_COLUMN_FLOW;
	flow->litem.root = layout->root;
	flow->litem.align = align;
	flow->litem.active = true;
	flow->litem.enabled = true;
	flow->litem.context = layout->context;
	flow->litem.space = (flow->litem.align) ? 0 : layout->root->style->columnspace;
	flow->litem.redalert = layout->redalert;
	flow->litem.w = layout->w;
	flow->number = number;
	BLI_addtail(&layout->items, flow);

	uiBlockSetCurLayout(layout->root->block, &flow->litem);

	return &flow->litem;
}

static uiLayoutItemBx *ui_layout_box(uiLayout *layout, int type)
{
	uiLayoutItemBx *box;

	box = MEM_callocN(sizeof(uiLayoutItemBx), "uiLayoutItemBx");
	box->litem.item.type = ITEM_LAYOUT_BOX;
	box->litem.root = layout->root;
	box->litem.active = 1;
	box->litem.enabled = 1;
	box->litem.context = layout->context;
	box->litem.space = layout->root->style->columnspace;
	box->litem.redalert = layout->redalert;
	box->litem.w = layout->w;
	BLI_addtail(&layout->items, box);

	uiBlockSetCurLayout(layout->root->block, &box->litem);

	box->roundbox = uiDefBut(layout->root->block, type, 0, "", 0, 0, 0, 0, NULL, 0.0, 0.0, 0, 0, "");

	return box;
}

uiLayout *uiLayoutBox(uiLayout *layout)
{
	return (uiLayout *)ui_layout_box(layout, ROUNDBOX);
}

/* Check all buttons defined in this layout, and set any button flagged as UI_BUT_LIST_ITEM as active/selected.
 * Needed to handle correctly text colors of active (selected) list item.
 */
void ui_layout_list_set_labels_active(uiLayout *layout)
{
	uiButtonItem *bitem;
	for (bitem = layout->items.first; bitem; bitem = bitem->item.next) {
		if (bitem->item.type != ITEM_BUTTON) {
			ui_layout_list_set_labels_active((uiLayout *)(&bitem->item));
		}
		else if (bitem->but->flag & UI_BUT_LIST_ITEM) {
			uiButSetFlag(bitem->but, UI_SELECT);
		}
	}
}

uiLayout *uiLayoutListBox(uiLayout *layout, uiList *ui_list, PointerRNA *ptr, PropertyRNA *prop, PointerRNA *actptr,
                          PropertyRNA *actprop)
{
	uiLayoutItemBx *box = ui_layout_box(layout, LISTBOX);
	uiBut *but = box->roundbox;

	but->custom_data = ui_list;

	but->rnasearchpoin = *ptr;
	but->rnasearchprop = prop;
	but->rnapoin = *actptr;
	but->rnaprop = actprop;

	/* only for the undo string */
	if (but->flag & UI_BUT_UNDO) {
		but->tip = RNA_property_description(actprop);
	}

	return (uiLayout *)box;
}

uiLayout *uiLayoutAbsolute(uiLayout *layout, int align)
{
	uiLayout *litem;

	litem = MEM_callocN(sizeof(uiLayout), "uiLayoutAbsolute");
	litem->item.type = ITEM_LAYOUT_ABSOLUTE;
	litem->root = layout->root;
	litem->align = align;
	litem->active = 1;
	litem->enabled = 1;
	litem->context = layout->context;
	litem->redalert = layout->redalert;
	BLI_addtail(&layout->items, litem);

	uiBlockSetCurLayout(layout->root->block, litem);

	return litem;
}

uiBlock *uiLayoutAbsoluteBlock(uiLayout *layout)
{
	uiBlock *block;

	block = uiLayoutGetBlock(layout);
	uiLayoutAbsolute(layout, false);

	return block;
}

uiLayout *uiLayoutOverlap(uiLayout *layout)
{
	uiLayout *litem;

	litem = MEM_callocN(sizeof(uiLayout), "uiLayoutOverlap");
	litem->item.type = ITEM_LAYOUT_OVERLAP;
	litem->root = layout->root;
	litem->active = true;
	litem->enabled = true;
	litem->context = layout->context;
	litem->redalert = layout->redalert;
	BLI_addtail(&layout->items, litem);

	uiBlockSetCurLayout(layout->root->block, litem);

	return litem;
}

uiLayout *uiLayoutSplit(uiLayout *layout, float percentage, int align)
{
	uiLayoutItemSplit *split;

	split = MEM_callocN(sizeof(uiLayoutItemSplit), "uiLayoutItemSplit");
	split->litem.item.type = ITEM_LAYOUT_SPLIT;
	split->litem.root = layout->root;
	split->litem.align = align;
	split->litem.active = true;
	split->litem.enabled = true;
	split->litem.context = layout->context;
	split->litem.space = layout->root->style->columnspace;
	split->litem.redalert = layout->redalert;
	split->litem.w = layout->w;
	split->percentage = percentage;
	BLI_addtail(&layout->items, split);

	uiBlockSetCurLayout(layout->root->block, &split->litem);

	return &split->litem;
}

void uiLayoutSetActive(uiLayout *layout, bool active)
{
	layout->active = active;
}

void uiLayoutSetEnabled(uiLayout *layout, bool enabled)
{
	layout->enabled = enabled;
}

void uiLayoutSetRedAlert(uiLayout *layout, bool redalert)
{
	layout->redalert = redalert;
}

void uiLayoutSetKeepAspect(uiLayout *layout, bool keepaspect)
{
	layout->keepaspect = keepaspect;
}

void uiLayoutSetAlignment(uiLayout *layout, char alignment)
{
	layout->alignment = alignment;
}

void uiLayoutSetScaleX(uiLayout *layout, float scale)
{
	layout->scale[0] = scale;
}

void uiLayoutSetScaleY(uiLayout *layout, float scale)
{
	layout->scale[1] = scale;
}

bool uiLayoutGetActive(uiLayout *layout)
{
	return layout->active;
}

bool uiLayoutGetEnabled(uiLayout *layout)
{
	return layout->enabled;
}

bool uiLayoutGetRedAlert(uiLayout *layout)
{
	return layout->redalert;
}

bool uiLayoutGetKeepAspect(uiLayout *layout)
{
	return layout->keepaspect;
}

int uiLayoutGetAlignment(uiLayout *layout)
{
	return layout->alignment;
}

int uiLayoutGetWidth(uiLayout *layout)
{
	return layout->w;
}

float uiLayoutGetScaleX(uiLayout *layout)
{
	return layout->scale[0];
}

float uiLayoutGetScaleY(uiLayout *layout)
{
	return layout->scale[1];
}

/********************** Layout *******************/

static void ui_item_scale(uiLayout *litem, const float scale[2])
{
	uiItem *item;
	int x, y, w, h;

	for (item = litem->items.last; item; item = item->prev) {
		ui_item_size(item, &w, &h);
		ui_item_offset(item, &x, &y);

		if (scale[0] != 0.0f) {
			x *= scale[0];
			w *= scale[0];
		}

		if (scale[1] != 0.0f) {
			y *= scale[1];
			h *= scale[1];
		}

		ui_item_position(item, x, y, w, h);
	}
}

static void ui_item_estimate(uiItem *item)
{
	uiItem *subitem;

	if (item->type != ITEM_BUTTON) {
		uiLayout *litem = (uiLayout *)item;

		for (subitem = litem->items.first; subitem; subitem = subitem->next)
			ui_item_estimate(subitem);

		if (BLI_listbase_is_empty(&litem->items))
			return;

		if (litem->scale[0] != 0.0f || litem->scale[1] != 0.0f)
			ui_item_scale(litem, litem->scale);

		switch (litem->item.type) {
			case ITEM_LAYOUT_COLUMN:
				ui_litem_estimate_column(litem);
				break;
			case ITEM_LAYOUT_COLUMN_FLOW:
				ui_litem_estimate_column_flow(litem);
				break;
			case ITEM_LAYOUT_ROW:
				ui_litem_estimate_row(litem);
				break;
			case ITEM_LAYOUT_BOX:
				ui_litem_estimate_box(litem);
				break;
			case ITEM_LAYOUT_ROOT:
				ui_litem_estimate_root(litem);
				break;
			case ITEM_LAYOUT_ABSOLUTE:
				ui_litem_estimate_absolute(litem);
				break;
			case ITEM_LAYOUT_SPLIT:
				ui_litem_estimate_split(litem);
				break;
			case ITEM_LAYOUT_OVERLAP:
				ui_litem_estimate_overlap(litem);
				break;
			default:
				break;
		}
	}
}

static void ui_item_align(uiLayout *litem, short nr)
{
	uiItem *item;
	uiButtonItem *bitem;
	uiLayoutItemBx *box;

	for (item = litem->items.last; item; item = item->prev) {
		if (item->type == ITEM_BUTTON) {
			bitem = (uiButtonItem *)item;
			if (ui_but_can_align(bitem->but))
				if (!bitem->but->alignnr)
					bitem->but->alignnr = nr;
		}
		else if (item->type == ITEM_LAYOUT_ABSOLUTE) {
			/* pass */
		}
		else if (item->type == ITEM_LAYOUT_OVERLAP) {
			/* pass */
		}
		else if (item->type == ITEM_LAYOUT_BOX) {
			box = (uiLayoutItemBx *)item;
			box->roundbox->alignnr = nr;
			BLI_remlink(&litem->root->block->buttons, box->roundbox);
			BLI_addhead(&litem->root->block->buttons, box->roundbox);
		}
		else if (((uiLayout *)item)->align) {
			ui_item_align((uiLayout *)item, nr);
		}
	}
}

static void ui_item_flag(uiLayout *litem, int flag)
{
	uiItem *item;
	uiButtonItem *bitem;

	for (item = litem->items.last; item; item = item->prev) {
		if (item->type == ITEM_BUTTON) {
			bitem = (uiButtonItem *)item;
			bitem->but->flag |= flag;
		}
		else
			ui_item_flag((uiLayout *)item, flag);
	}
}

static void ui_item_layout(uiItem *item)
{
	uiItem *subitem;

	if (item->type != ITEM_BUTTON) {
		uiLayout *litem = (uiLayout *)item;

		if (BLI_listbase_is_empty(&litem->items))
			return;

		if (litem->align)
			ui_item_align(litem, ++litem->root->block->alignnr);
		if (!litem->active)
			ui_item_flag(litem, UI_BUT_INACTIVE);
		if (!litem->enabled)
			ui_item_flag(litem, UI_BUT_DISABLED);

		switch (litem->item.type) {
			case ITEM_LAYOUT_COLUMN:
				ui_litem_layout_column(litem);
				break;
			case ITEM_LAYOUT_COLUMN_FLOW:
				ui_litem_layout_column_flow(litem);
				break;
			case ITEM_LAYOUT_ROW:
				ui_litem_layout_row(litem);
				break;
			case ITEM_LAYOUT_BOX:
				ui_litem_layout_box(litem);
				break;
			case ITEM_LAYOUT_ROOT:
				ui_litem_layout_root(litem);
				break;
			case ITEM_LAYOUT_ABSOLUTE:
				ui_litem_layout_absolute(litem);
				break;
			case ITEM_LAYOUT_SPLIT:
				ui_litem_layout_split(litem);
				break;
			case ITEM_LAYOUT_OVERLAP:
				ui_litem_layout_overlap(litem);
				break;
			default:
				break;
		}

		for (subitem = litem->items.first; subitem; subitem = subitem->next)
			ui_item_layout(subitem);
	}
}

static void ui_layout_end(uiBlock *block, uiLayout *layout, int *x, int *y)
{
	if (layout->root->handlefunc)
		uiBlockSetHandleFunc(block, layout->root->handlefunc, layout->root->argv);

	ui_item_estimate(&layout->item);
	ui_item_layout(&layout->item);

	if (x) *x = layout->x;
	if (y) *y = layout->y;
}

static void ui_layout_free(uiLayout *layout)
{
	uiItem *item, *next;

	for (item = layout->items.first; item; item = next) {
		next = item->next;

		if (item->type == ITEM_BUTTON)
			MEM_freeN(item);
		else
			ui_layout_free((uiLayout *)item);
	}

	MEM_freeN(layout);
}

static void ui_layout_add_padding_button(uiLayoutRoot *root)
{
	if (root->padding) {
		/* add an invisible button for padding */
		uiBlock *block = root->block;
		uiLayout *prev_layout = block->curlayout;

		block->curlayout = root->layout;
		uiDefBut(block, SEPR, 0, "", 0, 0, root->padding, root->padding, NULL, 0.0, 0.0, 0, 0, "");
		block->curlayout = prev_layout;
	}
}

uiLayout *uiBlockLayout(uiBlock *block, int dir, int type, int x, int y, int size, int em, int padding, uiStyle *style)
{
	uiLayout *layout;
	uiLayoutRoot *root;

	root = MEM_callocN(sizeof(uiLayoutRoot), "uiLayoutRoot");
	root->type = type;
	root->style = style;
	root->block = block;
	root->padding = padding;
	root->opcontext = WM_OP_INVOKE_REGION_WIN;

	layout = MEM_callocN(sizeof(uiLayout), "uiLayout");
	layout->item.type = ITEM_LAYOUT_ROOT;

	layout->x = x;
	layout->y = y;
	layout->root = root;
	layout->space = style->templatespace;
	layout->active = 1;
	layout->enabled = 1;
	layout->context = NULL;

	if (type == UI_LAYOUT_MENU)
		layout->space = 0;

	if (dir == UI_LAYOUT_HORIZONTAL) {
		layout->h = size;
		layout->root->emh = em * UI_UNIT_Y;
	}
	else {
		layout->w = size;
		layout->root->emw = em * UI_UNIT_X;
	}

	block->curlayout = layout;
	root->layout = layout;
	BLI_addtail(&block->layouts, root);

	ui_layout_add_padding_button(root);
	
	return layout;
}

uiBlock *uiLayoutGetBlock(uiLayout *layout)
{
	return layout->root->block;
}

int uiLayoutGetOperatorContext(uiLayout *layout)
{
	return layout->root->opcontext;
}


void uiBlockSetCurLayout(uiBlock *block, uiLayout *layout)
{
	block->curlayout = layout;
}

void ui_layout_add_but(uiLayout *layout, uiBut *but)
{
	uiButtonItem *bitem;
	
	bitem = MEM_callocN(sizeof(uiButtonItem), "uiButtonItem");
	bitem->item.type = ITEM_BUTTON;
	bitem->but = but;
	BLI_addtail(&layout->items, bitem);

	if (layout->context) {
		but->context = layout->context;
		but->context->used = true;
	}
}

void uiLayoutSetOperatorContext(uiLayout *layout, int opcontext)
{
	layout->root->opcontext = opcontext;
}

void uiLayoutSetFunc(uiLayout *layout, uiMenuHandleFunc handlefunc, void *argv)
{
	layout->root->handlefunc = handlefunc;
	layout->root->argv = argv;
}

void uiBlockLayoutResolve(uiBlock *block, int *x, int *y)
{
	uiLayoutRoot *root;

	BLI_assert(block->active);

	if (x) *x = 0;
	if (y) *y = 0;

	block->curlayout = NULL;

	for (root = block->layouts.first; root; root = root->next) {
		ui_layout_add_padding_button(root);

		/* NULL in advance so we don't interfere when adding button */
		ui_layout_end(block, root->layout, x, y);
		ui_layout_free(root->layout);
	}

	BLI_freelistN(&block->layouts);

	/* XXX silly trick, interface_templates.c doesn't get linked
	 * because it's not used by other files in this module? */
	{
		UI_template_fix_linking();
	}
}

void uiLayoutSetContextPointer(uiLayout *layout, const char *name, PointerRNA *ptr)
{
	uiBlock *block = layout->root->block;
	layout->context = CTX_store_add(&block->contexts, name, ptr);
}

void uiLayoutContextCopy(uiLayout *layout, bContextStore *context)
{
	uiBlock *block = layout->root->block;
	layout->context = CTX_store_add_all(&block->contexts, context);
}


/* introspect funcs */
#include "BLI_dynstr.h"

static void ui_intro_button(DynStr *ds, uiButtonItem *bitem)
{
	uiBut *but = bitem->but;
	BLI_dynstr_appendf(ds, "'type':%d, ", (int)but->type);
	BLI_dynstr_appendf(ds, "'draw_string':'''%s''', ", but->drawstr);
	BLI_dynstr_appendf(ds, "'tip':'''%s''', ", but->tip ? but->tip : "");  /* not exactly needed, rna has this */

	if (but->optype) {
		char *opstr = WM_operator_pystring_ex(but->block->evil_C, NULL, false, true, but->optype, but->opptr);
		BLI_dynstr_appendf(ds, "'operator':'''%s''', ", opstr ? opstr : "");
		MEM_freeN(opstr);
	}

	if (but->rnaprop) {
		BLI_dynstr_appendf(ds, "'rna':'%s.%s[%d]', ", RNA_struct_identifier(but->rnapoin.type), RNA_property_identifier(but->rnaprop), but->rnaindex);
	}

}

static void ui_intro_items(DynStr *ds, ListBase *lb)
{
	uiItem *item;

	BLI_dynstr_append(ds, "[");

	for (item = lb->first; item; item = item->next) {

		BLI_dynstr_append(ds, "{");

		/* could also use the INT but this is nicer*/
		switch (item->type) {
			case ITEM_BUTTON:             BLI_dynstr_append(ds, "'type':'BUTTON', "); break;
			case ITEM_LAYOUT_ROW:         BLI_dynstr_append(ds, "'type':'ROW', "); break;
			case ITEM_LAYOUT_COLUMN:      BLI_dynstr_append(ds, "'type':'COLUMN', "); break;
			case ITEM_LAYOUT_COLUMN_FLOW: BLI_dynstr_append(ds, "'type':'COLUMN_FLOW', "); break;
			case ITEM_LAYOUT_ROW_FLOW:    BLI_dynstr_append(ds, "'type':'ROW_FLOW', "); break;
			case ITEM_LAYOUT_BOX:         BLI_dynstr_append(ds, "'type':'BOX', "); break;
			case ITEM_LAYOUT_ABSOLUTE:    BLI_dynstr_append(ds, "'type':'ABSOLUTE', "); break;
			case ITEM_LAYOUT_SPLIT:       BLI_dynstr_append(ds, "'type':'SPLIT', "); break;
			case ITEM_LAYOUT_OVERLAP:     BLI_dynstr_append(ds, "'type':'OVERLAP', "); break;
			case ITEM_LAYOUT_ROOT:        BLI_dynstr_append(ds, "'type':'ROOT', "); break;
			default:                      BLI_dynstr_append(ds, "'type':'UNKNOWN', "); break;
		}

		switch (item->type) {
			case ITEM_BUTTON:
				ui_intro_button(ds, (uiButtonItem *)item);
				break;
			default:
				BLI_dynstr_append(ds, "'items':");
				ui_intro_items(ds, &((uiLayout *)item)->items);
				break;
		}

		BLI_dynstr_append(ds, "}");

		if (item != lb->last)
			BLI_dynstr_append(ds, ", ");
	}
	BLI_dynstr_append(ds, "], ");
}

static void ui_intro_uiLayout(DynStr *ds, uiLayout *layout)
{
	ui_intro_items(ds, &layout->items);
}

static char *str = NULL;  /* XXX, constant re-freeing, far from ideal. */
const char *uiLayoutIntrospect(uiLayout *layout)
{
	DynStr *ds = BLI_dynstr_new();

	if (str) {
		MEM_freeN(str);
	}

	ui_intro_uiLayout(ds, layout);

	str = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);

	return str;
}

#ifdef USE_OP_RESET_BUT
static void ui_layout_operator_buts__reset_cb(bContext *UNUSED(C), void *op_pt, void *UNUSED(arg_dummy2))
{
	WM_operator_properties_reset((wmOperator *)op_pt);
}
#endif

/* this function does not initialize the layout, functions can be called on the layout before and after */
void uiLayoutOperatorButs(const bContext *C, uiLayout *layout, wmOperator *op,
                          bool (*check_prop)(struct PointerRNA *, struct PropertyRNA *),
                          const char label_align, const short flag)
{
	if (!op->properties) {
		IDPropertyTemplate val = {0};
		op->properties = IDP_New(IDP_GROUP, &val, "wmOperatorProperties");
	}

	if (flag & UI_LAYOUT_OP_SHOW_TITLE) {
		uiItemL(layout, RNA_struct_ui_name(op->type->srna), ICON_NONE);
	}

	/* poll() on this operator may still fail, at the moment there is no nice feedback when this happens
	 * just fails silently */
	if (!WM_operator_repeat_check(C, op)) {
		uiBlockSetButLock(uiLayoutGetBlock(layout), true, "Operator can't' redo");

		/* XXX, could give some nicer feedback or not show redo panel at all? */
		uiItemL(layout, IFACE_("* Redo Unsupported *"), ICON_NONE);
	}

	/* menu */
	if (op->type->flag & OPTYPE_PRESET) {
		/* XXX, no simple way to get WM_MT_operator_presets.bl_label from python! Label remains the same always! */
		PointerRNA op_ptr;
		uiLayout *row;

		uiLayoutGetBlock(layout)->ui_operator = op;

		row = uiLayoutRow(layout, true);
		uiItemM(row, (bContext *)C, "WM_MT_operator_presets", NULL, ICON_NONE);

		WM_operator_properties_create(&op_ptr, "WM_OT_operator_preset_add");
		RNA_string_set(&op_ptr, "operator", op->type->idname);
		uiItemFullO(row, "WM_OT_operator_preset_add", "", ICON_ZOOMIN, op_ptr.data, WM_OP_INVOKE_DEFAULT, 0);

		WM_operator_properties_create(&op_ptr, "WM_OT_operator_preset_add");
		RNA_string_set(&op_ptr, "operator", op->type->idname);
		RNA_boolean_set(&op_ptr, "remove_active", true);
		uiItemFullO(row, "WM_OT_operator_preset_add", "", ICON_ZOOMOUT, op_ptr.data, WM_OP_INVOKE_DEFAULT, 0);
	}

	if (op->type->ui) {
		op->layout = layout;
		op->type->ui((bContext *)C, op);
		op->layout = NULL;

		/* UI_LAYOUT_OP_SHOW_EMPTY ignored */
	}
	else {
		wmWindowManager *wm = CTX_wm_manager(C);
		PointerRNA ptr;
		int empty;

		RNA_pointer_create(&wm->id, op->type->srna, op->properties, &ptr);

		/* main draw call */
		empty = uiDefAutoButsRNA(layout, &ptr, check_prop, label_align) == 0;

		if (empty && (flag & UI_LAYOUT_OP_SHOW_EMPTY)) {
			uiItemL(layout, IFACE_("No Properties"), ICON_NONE);
		}
	}

#ifdef USE_OP_RESET_BUT
	/* its possible that reset can do nothing if all have PROP_SKIP_SAVE enabled
	 * but this is not so important if this button is drawn in those cases
	 * (which isn't all that likely anyway) - campbell */
	if (op->properties->len) {
		uiBlock *block;
		uiBut *but;
		uiLayout *col; /* needed to avoid alignment errors with previous buttons */

		col = uiLayoutColumn(layout, false);
		block = uiLayoutGetBlock(col);
		but = uiDefIconTextBut(block, BUT, 0, ICON_FILE_REFRESH, IFACE_("Reset"), 0, 0, UI_UNIT_X, UI_UNIT_Y,
		                       NULL, 0.0, 0.0, 0.0, 0.0, TIP_("Reset operator defaults"));
		uiButSetFunc(but, ui_layout_operator_buts__reset_cb, op, NULL);
	}
#endif

	/* set various special settings for buttons */
	{
		uiBlock *block = uiLayoutGetBlock(layout);
		const bool is_popup = (block->flag & UI_BLOCK_KEEP_OPEN) != 0;
		uiBut *but;

		
		for (but = block->buttons.first; but; but = but->next) {
			/* no undo for buttons for operator redo panels */
			uiButClearFlag(but, UI_BUT_UNDO);
			
			/* only for popups, see [#36109] */

			/* if button is operator's default property, and a text-field, enable focus for it
			 *	- this is used for allowing operators with popups to rename stuff with fewer clicks
			 */
			if (is_popup) {
				if ((but->rnaprop == op->type->prop) && (but->type == TEX)) {
					uiButSetFocusOnEnter(CTX_wm_window(C), but);
				}
			}
		}
	}
}

/* this is a bit of a hack but best keep it in one place at least */
MenuType *uiButGetMenuType(uiBut *but)
{
	if (but->menu_create_func == ui_item_menutype_func) {
		return (MenuType *)but->poin;
	}
	else {
		return NULL;
	}
}
