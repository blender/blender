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

#include "BLI_alloca.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_screen.h"
#include "BKE_animsys.h"

#include "RNA_access.h"

#include "UI_interface.h"

#include "ED_armature.h"


#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.h"

/* Show an icon button after each RNA button to use to quickly set keyframes,
 * this is a way to display animation/driven/override status, see T54951. */
#define UI_PROP_DECORATE
/* Alternate draw mode where some buttons can use single icon width,
 * giving more room for the text at the expense of nicely aligned text. */
#define UI_PROP_SEP_ICON_WIDTH_EXCEPTION

/************************ Structs and Defines *************************/

#define UI_OPERATOR_ERROR_RET(_ot, _opname, return_statement)                 \
	if (ot == NULL) {                                                         \
		ui_item_disabled(layout, _opname);                                    \
		RNA_warning("'%s' unknown operator", _opname);                        \
		return_statement;                                                     \
	} (void)0                                                                 \

#define UI_ITEM_PROP_SEP_DIVIDE 0.5f

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
	ITEM_LAYOUT_GRID_FLOW,
	ITEM_LAYOUT_BOX,
	ITEM_LAYOUT_ABSOLUTE,
	ITEM_LAYOUT_SPLIT,
	ITEM_LAYOUT_OVERLAP,
	ITEM_LAYOUT_RADIAL,

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

enum {
	UI_ITEM_FIXED     = 1 << 0,
	UI_ITEM_MIN       = 1 << 1,

	UI_ITEM_BOX_ITEM  = 1 << 2, /* The item is "inside" a box item */
	UI_ITEM_PROP_SEP  = 1 << 3,
	/* Show an icon button next to each property (to set keyframes, show status).
	 * Enabled by default, depends on 'UI_ITEM_PROP_SEP'. */
	UI_ITEM_PROP_DECORATE = 1 << 4,
};

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
	bool variable_size;  /* For layouts inside gridflow, they and their items shall never have a fixed maximal size. */
	char alignment;
	char emboss;
};

typedef struct uiLayoutItemFlow {
	uiLayout litem;
	int number;
	int totcol;
} uiLayoutItemFlow;

typedef struct uiLayoutItemGridFlow {
	uiLayout litem;

	/* Extra parameters */
	bool row_major;     /* Fill first row first, instead of filling first column first. */
	bool even_columns;  /* Same width for all columns. */
	bool even_rows;     /* Same height for all rows. */
	/* If positive, absolute fixed number of columns.
	 * If 0, fully automatic (based on available width).
	 * If negative, automatic but only generates number of columns/rows multiple of given (absolute) value. */
	int columns_len;

	/* Pure internal runtime storage. */
	int tot_items, tot_columns, tot_rows;
} uiLayoutItemGridFlow;

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
		memcpy(namestr, name, len);
		namestr[len] = ':';
		namestr[len + 1] = '\0';
		return namestr;
	}

	return name;
}

static int ui_item_fit(int item, int pos, int all, int available, bool is_last, int alignment, float *extra_pixel)
{
	/* available == 0 is unlimited */
	if (ELEM(0, available, all)) {
		return item;
	}

	if (all > available) {
		/* contents is bigger than available space */
		if (is_last)
			return available - pos;
		else {
			float width = *extra_pixel + (item * available) / (float)all;
			*extra_pixel = width - (int)width;
			return (int)width;
		}
	}
	else {
		/* contents is smaller or equal to available space */
		if (alignment == UI_LAYOUT_ALIGN_EXPAND) {
			if (is_last)
				return available - pos;
			else {
				float width = *extra_pixel + (item * available) / (float)all;
				*extra_pixel = width - (int)width;
				return (int)width;
			}
		}
		else {
			return item;
		}
	}
}

/* variable button size in which direction? */
#define UI_ITEM_VARY_X  1
#define UI_ITEM_VARY_Y  2

static int ui_layout_vary_direction(uiLayout *layout)
{
	return ((ELEM(layout->root->type, UI_LAYOUT_HEADER, UI_LAYOUT_PIEMENU) ||
	        (layout->alignment != UI_LAYOUT_ALIGN_EXPAND)) ?
	        UI_ITEM_VARY_X : UI_ITEM_VARY_Y);
}

static bool ui_layout_variable_size(uiLayout *layout)
{
	/* Note that this code is probably a bit flacky, we'd probably want to know whether it's variable in X and/or Y,
	 * etc. But for now it mimics previous one, with addition of variable flag set for children of gridflow layouts. */
	return ui_layout_vary_direction(layout) == UI_ITEM_VARY_X || layout->variable_size;
}

/* estimated size of text + icon */
static int ui_text_icon_width(uiLayout *layout, const char *name, int icon, bool compact)
{
	bool variable;
	const int unit_x = UI_UNIT_X * (layout->scale[0] ? layout->scale[0] : 1.0f);

	if (icon && !name[0])
		return unit_x;  /* icon only */

	variable = ui_layout_variable_size(layout);

	if (variable) {
		if (layout->alignment != UI_LAYOUT_ALIGN_EXPAND) {
			layout->item.flag |= UI_ITEM_MIN;
		}
		const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
		/* it may seem odd that the icon only adds (unit_x / 4)
		 * but taking margins into account its fine */
		return (UI_fontstyle_string_width(fstyle, name) +
		        (unit_x * ((compact ? 1.25f : 1.50f) +
		                   (icon    ? 0.25f : 0.0f))));
	}
	else {
		return unit_x * 10;
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

		ui_but_update(bitem->but); /* for strlen */
	}
	else {
		uiLayout *litem = (uiLayout *)item;

		litem->x = x;
		litem->y = y + h;
		litem->w = w;
		litem->h = h;
	}
}

static void ui_item_move(uiItem *item, int delta_xmin, int delta_xmax)
{
	if (item->type == ITEM_BUTTON) {
		uiButtonItem *bitem = (uiButtonItem *)item;

		bitem->but->rect.xmin += delta_xmin;
		bitem->but->rect.xmax += delta_xmax;

		ui_but_update(bitem->but); /* for strlen */
	}
	else {
		uiLayout *litem = (uiLayout *)item;

		if (delta_xmin > 0)
			litem->x += delta_xmin;
		else
			litem->w += delta_xmax;
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
		case ITEM_LAYOUT_GRID_FLOW:
		case ITEM_LAYOUT_SPLIT:
		case ITEM_LAYOUT_ABSOLUTE:
		case ITEM_LAYOUT_BOX:
		default:
			return UI_LAYOUT_VERTICAL;
	}
}

static uiLayout *ui_item_local_sublayout(uiLayout *test, uiLayout *layout, bool align)
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
			ui_but_update(cbut);
	}
}

/* create buttons for an item with an RNA array */
static void ui_item_array(
        uiLayout *layout, uiBlock *block, const char *name, int icon,
        PointerRNA *ptr, PropertyRNA *prop, int len, int x, int y, int w, int UNUSED(h),
        bool expand, bool slider, bool toggle, bool icon_only, bool compact, bool show_text)
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
	UI_block_layout_set_current(block, sub);

	/* create label */
	if (name[0] && show_text) {
		uiDefBut(block, UI_BTYPE_LABEL, 0, name, 0, 0, w, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
	}

	/* create buttons */
	if (type == PROP_BOOLEAN && ELEM(subtype, PROP_LAYER, PROP_LAYER_MEMBER)) {
		/* special check for layer layout */
		int butw, buth, unit;
		int cols = (len >= 20) ? 2 : 1;
		const unsigned int colbuts = len / (2 * cols);
		unsigned int layer_used = 0;
		unsigned int layer_active = 0;

		UI_block_layout_set_current(block, uiLayoutAbsolute(layout, false));

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
			UI_block_align_begin(block);

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
					UI_but_func_set(but, ui_layer_but_cb, but, SET_INT_IN_POINTER(layer_num));
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
					UI_but_func_set(but, ui_layer_but_cb, but, SET_INT_IN_POINTER(layer_num));
			}
			UI_block_align_end(block);

			x += colbuts * butw + style->buttonspacex;
		}
	}
	else if (subtype == PROP_MATRIX) {
		int totdim, dim_size[3];    /* 3 == RNA_MAX_ARRAY_DIMENSION */
		int row, col;

		UI_block_layout_set_current(block, uiLayoutAbsolute(layout, true));

		totdim = RNA_property_array_dimension(ptr, prop, dim_size);
		if (totdim != 2) return;    /* only 2D matrices supported in UI so far */

		w /= dim_size[0];
		/* h /= dim_size[1]; */ /* UNUSED */

		for (a = 0; a < len; a++) {
			col = a % dim_size[0];
			row = a / dim_size[0];

			but = uiDefAutoButR(block, ptr, prop, a, "", ICON_NONE, x + w * col, y + (dim_size[1] * UI_UNIT_Y) - (row * UI_UNIT_Y), w, UI_UNIT_Y);
			if (slider && but->type == UI_BTYPE_NUM)
				but->type = UI_BTYPE_NUM_SLIDER;
		}
	}
	else if (subtype == PROP_DIRECTION && !expand) {
		uiDefButR_prop(block, UI_BTYPE_UNITVEC, 0, name, x, y, UI_UNIT_X * 3, UI_UNIT_Y * 3, ptr, prop, -1, 0, 0, -1, -1, NULL);
	}
	else {
		/* note, this block of code is a bit arbitrary and has just been made
		 * to work with common cases, but may need to be re-worked */

		/* special case, boolean array in a menu, this could be used in a more generic way too */
		if (ELEM(subtype, PROP_COLOR, PROP_COLOR_GAMMA) && !expand) {
			uiDefAutoButR(block, ptr, prop, -1, "", ICON_NONE, 0, 0, w, UI_UNIT_Y);
		}
		else {
			bool *boolarr = NULL;

			/* even if 'expand' is fale, expanding anyway */

			/* layout for known array subtypes */
			char str[3] = {'\0'};

			if (!icon_only && show_text) {
				if (type != PROP_BOOLEAN) {
					str[1] = ':';
				}
			}

			/* show checkboxes for rna on a non-emboss block (menu for eg) */
			if (type == PROP_BOOLEAN && ELEM(layout->root->block->dt, UI_EMBOSS_NONE, UI_EMBOSS_PULLDOWN)) {
				boolarr = MEM_callocN(sizeof(bool) * len, __func__);
				RNA_property_boolean_get_array(ptr, prop, boolarr);
			}

			const char *str_buf = show_text ? str: "";
			for (a = 0; a < len; a++) {
				int width_item;

				if (!icon_only && show_text) {
					str[0] = RNA_property_array_item_char(prop, a);
				}
				if (boolarr) {
					icon = boolarr[a] ? ICON_CHECKBOX_HLT : ICON_CHECKBOX_DEHLT;
				}

				width_item = (
				        (compact && type == PROP_BOOLEAN) ?
				        min_ii(w, ui_text_icon_width(layout, str_buf, icon, false)) : w);

				but = uiDefAutoButR(block, ptr, prop, a, str_buf, icon, 0, 0, width_item, UI_UNIT_Y);
				if (slider && but->type == UI_BTYPE_NUM)
					but->type = UI_BTYPE_NUM_SLIDER;
				if (toggle && but->type == UI_BTYPE_CHECKBOX)
					but->type = UI_BTYPE_TOGGLE;
				if ((a == 0) && (subtype == PROP_AXISANGLE))
					UI_but_unit_type_set(but, PROP_UNIT_ROTATION);
			}

			if (boolarr) {
				MEM_freeN(boolarr);
			}
		}
	}

	UI_block_layout_set_current(block, layout);
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
static void ui_item_enum_expand(
        uiLayout *layout, uiBlock *block, PointerRNA *ptr, PropertyRNA *prop,
        const char *uiname, int h, bool icon_only)
{
	/* XXX The way this function currently handles uiname parameter is insane and inconsistent with general UI API:
	 *     * uiname is the *enum property* label.
	 *     * when it is NULL or empty, we do not draw *enum items* labels, this doubles the icon_only parameter.
	 *     * we *never* draw (i.e. really use) the enum label uiname, it is just used as a mere flag!
	 *     Unfortunately, fixing this implies an API "soft break", so better to defer it for later... :/
	 *     --mont29
	 */

	uiBut *but;
	uiLayout *layout_radial = NULL;
	const EnumPropertyItem *item, *item_array;
	const char *name;
	int itemw, icon, value;
	bool free;
	bool radial = (layout->root->type == UI_LAYOUT_PIEMENU);

	if (radial)
		RNA_property_enum_items_gettexted_all(block->evil_C, ptr, prop, &item_array, NULL, &free);
	else
		RNA_property_enum_items_gettexted(block->evil_C, ptr, prop, &item_array, NULL, &free);

	/* we dont want nested rows, cols in menus */
	if (radial) {
		if (layout->root->layout == layout) {
			layout_radial = uiLayoutRadial(layout);
			UI_block_layout_set_current(block, layout_radial);
		}
		else {
			if (layout->item.type == ITEM_LAYOUT_RADIAL) {
				layout_radial = layout;
			}
			UI_block_layout_set_current(block, layout);
		}
	}
	else if (layout->root->type != UI_LAYOUT_MENU) {
		UI_block_layout_set_current(block, ui_item_local_sublayout(layout, layout, 1));
	}
	else {
		UI_block_layout_set_current(block, layout);
	}

	for (item = item_array; item->identifier; item++) {
		if (!item->identifier[0]) {
			const EnumPropertyItem *next_item = item + 1;
			if (next_item->identifier) {
				if (radial && layout_radial) {
					uiItemS(layout_radial);
				}
				else {
					uiItemS(block->curlayout);
				}
			}
			continue;
		}

		name = (!uiname || uiname[0]) ? item->name : "";
		icon = item->icon;
		value = item->value;
		itemw = ui_text_icon_width(block->curlayout, icon_only ? "" : name, icon, 0);

		if (icon && name[0] && !icon_only)
			but = uiDefIconTextButR_prop(block, UI_BTYPE_ROW, 0, icon, name, 0, 0, itemw, h, ptr, prop, -1, 0, value, -1, -1, NULL);
		else if (icon)
			but = uiDefIconButR_prop(block, UI_BTYPE_ROW, 0, icon, 0, 0, itemw, h, ptr, prop, -1, 0, value, -1, -1, NULL);
		else
			but = uiDefButR_prop(block, UI_BTYPE_ROW, 0, name, 0, 0, itemw, h, ptr, prop, -1, 0, value, -1, -1, NULL);

		if (RNA_property_flag(prop) & PROP_ENUM_FLAG) {
			UI_but_func_set(but, ui_item_enum_expand_handle, but, SET_INT_IN_POINTER(value));
		}

		if (ui_layout_local_dir(layout) != UI_LAYOUT_HORIZONTAL)
			but->drawflag |= UI_BUT_TEXT_LEFT;
	}
	UI_block_layout_set_current(block, layout);

	if (free) {
		MEM_freeN((void *)item_array);
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

/**
 * Create label + button for RNA property
 *
 * \param w_hint: For varying width layout, this becomes the label width.
 *                Otherwise it's used to fit both items into it.
 **/
static uiBut *ui_item_with_label(
        uiLayout *layout, uiBlock *block, const char *name, int icon,
        PointerRNA *ptr, PropertyRNA *prop, int index,
        int x, int y, int w_hint, int h, int flag)
{
	uiLayout *sub;
	uiBut *but = NULL;
	PropertyType type;
	PropertySubType subtype;
	int prop_but_width = w_hint;
	const bool use_prop_sep = ((layout->item.flag & UI_ITEM_PROP_SEP) != 0);

	sub = uiLayoutRow(layout, layout->align);
	UI_block_layout_set_current(block, sub);

	if (name[0]) {
		int w_label;

		if (use_prop_sep) {
			w_label = (int)((w_hint * 2) * UI_ITEM_PROP_SEP_DIVIDE);
		}
		else {
			if (ui_layout_variable_size(layout)) {
				/* w_hint is width for label in this case. Use a default width for property button(s) */
				prop_but_width = UI_UNIT_X * 5;
				w_label = w_hint;
			}
			else {
				w_label = w_hint / 3;
			}
		}

		uiBut *but_label = uiDefBut(block, UI_BTYPE_LABEL, 0, name, x, y, w_label, h, NULL, 0.0, 0.0, 0, 0, "");
		if (use_prop_sep) {
			but_label->drawflag |= UI_BUT_TEXT_RIGHT;
			but_label->drawflag &= ~UI_BUT_TEXT_LEFT;
		}
	}

	type = RNA_property_type(prop);
	subtype = RNA_property_subtype(prop);

	if (subtype == PROP_FILEPATH || subtype == PROP_DIRPATH) {
		UI_block_layout_set_current(block, uiLayoutRow(sub, true));
		but = uiDefAutoButR(block, ptr, prop, index, "", icon, x, y, prop_but_width - UI_UNIT_X, h);

		/* BUTTONS_OT_file_browse calls UI_context_active_but_prop_get_filebrowser */
		uiDefIconButO(
		        block, UI_BTYPE_BUT, subtype == PROP_DIRPATH ? "BUTTONS_OT_directory_browse" : "BUTTONS_OT_file_browse",
		        WM_OP_INVOKE_DEFAULT, ICON_FILESEL, x, y, UI_UNIT_X, h, NULL);
	}
	else if (flag & UI_ITEM_R_EVENT) {
		but = uiDefButR_prop(block, UI_BTYPE_KEY_EVENT, 0, name, x, y, prop_but_width, h, ptr, prop, index, 0, 0, -1, -1, NULL);
	}
	else if (flag & UI_ITEM_R_FULL_EVENT) {
		if (RNA_struct_is_a(ptr->type, &RNA_KeyMapItem)) {
			char buf[128];

			WM_keymap_item_to_string(ptr->data, false, buf, sizeof(buf));

			but = uiDefButR_prop(block, UI_BTYPE_HOTKEY_EVENT, 0, buf, x, y, prop_but_width, h, ptr, prop, 0, 0, 0, -1, -1, NULL);
			UI_but_func_set(but, ui_keymap_but_cb, but, NULL);
			if (flag & UI_ITEM_R_IMMEDIATE)
				UI_but_flag_enable(but, UI_BUT_IMMEDIATE);
		}
	}
	else {
		const char *str = (type == PROP_ENUM && !(flag & UI_ITEM_R_ICON_ONLY)) ? NULL : "";
		but = uiDefAutoButR(
		        block, ptr, prop, index, str, icon,
		        x, y, prop_but_width, h);
	}

	UI_block_layout_set_current(block, layout);
	return but;
}

void UI_context_active_but_prop_get_filebrowser(
        const bContext *C,
        PointerRNA *r_ptr, PropertyRNA **r_prop, bool *r_is_undo)
{
	ARegion *ar = CTX_wm_region(C);
	uiBlock *block;
	uiBut *but, *prevbut;

	memset(r_ptr, 0, sizeof(*r_ptr));
	*r_prop = NULL;
	*r_is_undo = false;

	if (!ar)
		return;

	for (block = ar->uiblocks.first; block; block = block->next) {
		for (but = block->buttons.first; but; but = but->next) {
			prevbut = but->prev;

			/* find the button before the active one */
			if ((but->flag & UI_BUT_LAST_ACTIVE) && prevbut && prevbut->rnapoin.data) {
				if (RNA_property_type(prevbut->rnaprop) == PROP_STRING) {
					*r_ptr = prevbut->rnapoin;
					*r_prop = prevbut->rnaprop;
					*r_is_undo = (prevbut->flag & UI_BUT_UNDO) != 0;
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
static void ui_but_tip_from_enum_item(uiBut *but, const EnumPropertyItem *item)
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

	UI_block_layout_set_current(block, layout);

	if (!name)
		name = "";

	w = ui_text_icon_width(layout, name, 0, 0);

	but = uiDefBut(block, UI_BTYPE_LABEL, 0, name, 0, 0, w, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
	but->flag |= UI_BUT_DISABLED;
	but->disabled_info = "";
}

/**
 * Operator Item
 * \param r_opptr: Optional, initialize with operator properties when not NULL.
 * Will always be written to even in the case of errors.
 */
static uiBut *uiItemFullO_ptr_ex(
        uiLayout *layout, wmOperatorType *ot,
        const char *name, int icon, IDProperty *properties, int context, int flag,
        PointerRNA *r_opptr)
{
	/* Take care to fill 'r_opptr' whatever happens. */
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
	UI_block_layout_set_current(block, layout);

	w = ui_text_icon_width(layout, name, icon, 0);

	int prev_emboss = layout->emboss;
	if (flag & UI_ITEM_R_NO_BG) {
		layout->emboss = UI_EMBOSS_NONE;
	}

	/* create the button */
	if (icon) {
		if (name[0]) {
			but = uiDefIconTextButO_ptr(block, UI_BTYPE_BUT, ot, context, icon, name, 0, 0, w, UI_UNIT_Y, NULL);
		}
		else {
			but = uiDefIconButO_ptr(block, UI_BTYPE_BUT, ot, context, icon, 0, 0, w, UI_UNIT_Y, NULL);
		}
	}
	else {
		but = uiDefButO_ptr(block, UI_BTYPE_BUT, ot, context, name, 0, 0, w, UI_UNIT_Y, NULL);
	}

	assert(but->optype != NULL);

	/* text alignment for toolbar buttons */
	if ((layout->root->type == UI_LAYOUT_TOOLBAR) && !icon)
		but->drawflag |= UI_BUT_TEXT_LEFT;

	if (flag & UI_ITEM_R_NO_BG) {
		layout->emboss = prev_emboss;
	}

	if (flag & UI_ITEM_O_DEPRESS) {
		but->flag |= UI_SELECT_DRAW;
	}

	if (layout->redalert)
		UI_but_flag_enable(but, UI_BUT_REDALERT);

	/* assign properties */
	if (properties || r_opptr) {
		PointerRNA *opptr = UI_but_operator_ptr_get(but);
		if (properties) {
			opptr->data = properties;
		}
		else {
			IDPropertyTemplate val = {0};
			opptr->data = IDP_New(IDP_GROUP, &val, "wmOperatorProperties");
		}
		if (r_opptr) {
			*r_opptr = *opptr;
		}
	}

	return but;
}

static void ui_item_menu_hold(struct bContext *C, ARegion *butregion, uiBut *but)
{
	uiPopupMenu *pup = UI_popup_menu_begin(C, "", ICON_NONE);
	uiLayout *layout = UI_popup_menu_layout(pup);
	uiBlock *block = layout->root->block;
	UI_popup_menu_but_set(pup, butregion, but);

	block->flag |= UI_BLOCK_POPUP_HOLD;
	block->flag |= UI_BLOCK_IS_FLIP;

	char direction = UI_DIR_DOWN;
	if (!but->drawstr[0]) {
		if (butregion->alignment == RGN_ALIGN_LEFT) {
			direction = UI_DIR_RIGHT;
		}
		else if (butregion->alignment == RGN_ALIGN_RIGHT) {
			direction = UI_DIR_LEFT;
		}
		else if (butregion->alignment == RGN_ALIGN_BOTTOM) {
			direction = UI_DIR_UP;
		}
		else {
			direction = UI_DIR_DOWN;
		}
	}
	UI_block_direction_set(block, direction);

	const char *menu_id = but->hold_argN;
	MenuType *mt = WM_menutype_find(menu_id, true);
	if (mt) {
		uiLayoutSetContextFromBut(layout, but);
		UI_menutype_draw(C, mt, layout);
	}
	else {
		uiItemL(layout, "Menu Missing:", ICON_NONE);
		uiItemL(layout, menu_id, ICON_NONE);
	}
	UI_popup_menu_end(C, pup);
}

void uiItemFullO_ptr(
        uiLayout *layout, wmOperatorType *ot,
        const char *name, int icon, IDProperty *properties, int context, int flag,
        PointerRNA *r_opptr)
{
	uiItemFullO_ptr_ex(layout, ot, name, icon, properties, context, flag, r_opptr);
}

void uiItemFullOMenuHold_ptr(
        uiLayout *layout, wmOperatorType *ot,
        const char *name, int icon, IDProperty *properties, int context, int flag,
        const char *menu_id,
        PointerRNA *r_opptr)
{
	uiBut *but = uiItemFullO_ptr_ex(layout, ot, name, icon, properties, context, flag, r_opptr);
	UI_but_func_hold_set(but, ui_item_menu_hold, BLI_strdup(menu_id));
}

void uiItemFullO(
        uiLayout *layout, const char *opname,
        const char *name, int icon, IDProperty *properties, int context, int flag,
        PointerRNA *r_opptr)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0); /* print error next */

	UI_OPERATOR_ERROR_RET(
	        ot, opname, {
	            if (r_opptr) {
	                *r_opptr = PointerRNA_NULL;
	            }
	            return;
	        });

	uiItemFullO_ptr(layout, ot, name, icon, properties, context, flag, r_opptr);
}

static const char *ui_menu_enumpropname(uiLayout *layout, PointerRNA *ptr, PropertyRNA *prop, int retval)
{
	const EnumPropertyItem *item;
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
		MEM_freeN((void *)item);
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

	uiItemFullO_ptr(layout, ot, name, icon, ptr.data, layout->root->opcontext, 0, NULL);
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

BLI_INLINE bool ui_layout_is_radial(const uiLayout *layout)
{
	return (layout->item.type == ITEM_LAYOUT_RADIAL) ||
	       ((layout->item.type == ITEM_LAYOUT_ROOT) && (layout->root->type == UI_LAYOUT_PIEMENU));
}

/**
 * Create ui items for enum items in \a item_array.
 *
 * A version of #uiItemsFullEnumO that takes pre-calculated item array.
 */
void uiItemsFullEnumO_items(
        uiLayout *layout, wmOperatorType *ot, PointerRNA ptr, PropertyRNA *prop, IDProperty *properties,
        int context, int flag,
        const EnumPropertyItem *item_array, int totitem)
{
	const char *propname = RNA_property_identifier(prop);
	if (RNA_property_type(prop) != PROP_ENUM) {
		RNA_warning("%s.%s, not an enum type", RNA_struct_identifier(ptr.type), propname);
		return;
	}

	uiLayout *target, *split = NULL;
	const EnumPropertyItem *item;
	uiBlock *block = layout->root->block;
	const bool radial = ui_layout_is_radial(layout);

	if (radial) {
		target = uiLayoutRadial(layout);
	}
	else {
		split = uiLayoutSplit(layout, 0.0f, false);
		target = uiLayoutColumn(split, layout->align);
	}


	int i;
	bool last_iter = false;

	for (i = 1, item = item_array; item->identifier && !last_iter; i++, item++) {
		/* handle oversized pies */
		if (radial && (totitem > PIE_MAX_ITEMS) && (i >= PIE_MAX_ITEMS)) {
			if (item->name) { /* only visible items */
				const EnumPropertyItem *tmp;

				/* Check if there are more visible items for the next level. If not, we don't
				 * add a new level and add the remaining item instead of the 'more' button. */
				for (tmp = item + 1; tmp->identifier; tmp++)
					if (tmp->name)
						break;

				if (tmp->identifier) { /* only true if loop above found item and did early-exit */
					ui_pie_menu_level_create(block, ot, propname, properties, item_array, totitem, context, flag);
					/* break since rest of items is handled in new pie level */
					break;
				}
				else {
					last_iter = true;
				}
			}
			else {
				continue;
			}
		}

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

			uiItemFullO_ptr(target, ot, item->name, item->icon, tptr.data, context, flag, NULL);

			ui_but_tip_from_enum_item(block->buttons.last, item);
		}
		else {
			if (item->name) {
				uiBut *but;

				if (item != item_array && !radial) {
					target = uiLayoutColumn(split, layout->align);

					/* inconsistent, but menus with labels do not look good flipped */
					block->flag |= UI_BLOCK_NO_FLIP;
				}

				if (item->icon || radial) {
					uiItemL(target, item->name, item->icon);

					but = block->buttons.last;
				}
				else {
					/* Do not use uiItemL here, as our root layout is a menu one, it will add a fake blank icon! */
					but = uiDefBut(
					        block, UI_BTYPE_LABEL, 0, item->name, 0, 0, UI_UNIT_X * 5, UI_UNIT_Y, NULL,
					        0.0, 0.0, 0, 0, "");
				}
				ui_but_tip_from_enum_item(but, item);
			}
			else {
				if (radial) {
					/* invisible dummy button to ensure all items are always at the same position */
					uiItemS(target);
				}
				else {
					/* XXX bug here, colums draw bottom item badly */
					uiItemS(target);
				}
			}
		}
	}
}

void uiItemsFullEnumO(
        uiLayout *layout, const char *opname, const char *propname, IDProperty *properties,
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
		const EnumPropertyItem *item_array = NULL;
		int totitem;
		bool free;

		if (ui_layout_is_radial(layout)) {
			/* XXX: While "_all()" guarantees spatial stability, it's bad when an enum has > 8 items total,
			 * but only a small subset will ever be shown at once (e.g. Mode Switch menu, after the
			 * introduction of GP editing modes)
			 */
#if 0
			RNA_property_enum_items_gettexted_all(block->evil_C, &ptr, prop, &item_array, &totitem, &free);
#else
			RNA_property_enum_items_gettexted(block->evil_C, &ptr, prop, &item_array, &totitem, &free);
#endif
		}
		else {
			RNA_property_enum_items_gettexted(block->evil_C, &ptr, prop, &item_array, &totitem, &free);
		}

		/* add items */
		uiItemsFullEnumO_items(
		        layout, ot, ptr, prop, properties, context, flag,
		        item_array, totitem);

		if (free) {
			MEM_freeN((void *)item_array);
		}

		/* intentionally don't touch UI_BLOCK_IS_FLIP here,
		 * we don't know the context this is called in */
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

	UI_OPERATOR_ERROR_RET(ot, opname, return);

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

	uiItemFullO_ptr(layout, ot, name, icon, ptr.data, layout->root->opcontext, 0, NULL);
}

void uiItemEnumO_string(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, const char *value_str)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0); /* print error next */
	PointerRNA ptr;
	PropertyRNA *prop;

	const EnumPropertyItem *item;
	int value;
	bool free;

	UI_OPERATOR_ERROR_RET(ot, opname, return);

	WM_operator_properties_create_ptr(&ptr, ot);

	/* enum lookup */
	if ((prop = RNA_struct_find_property(&ptr, propname))) {
		/* no need for translations here */
		RNA_property_enum_items(layout->root->block->evil_C, &ptr, prop, &item, NULL, &free);
		if (item == NULL || RNA_enum_value_from_id(item, value_str, &value) == 0) {
			if (free) {
				MEM_freeN((void *)item);
			}
			RNA_warning("%s.%s, enum %s not found", RNA_struct_identifier(ptr.type), propname, value_str);
			return;
		}

		if (free) {
			MEM_freeN((void *)item);
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

	uiItemFullO_ptr(layout, ot, name, icon, ptr.data, layout->root->opcontext, 0, NULL);
}

void uiItemBooleanO(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, int value)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0); /* print error next */
	PointerRNA ptr;

	UI_OPERATOR_ERROR_RET(ot, opname, return);

	WM_operator_properties_create_ptr(&ptr, ot);
	RNA_boolean_set(&ptr, propname, value);

	uiItemFullO_ptr(layout, ot, name, icon, ptr.data, layout->root->opcontext, 0, NULL);
}

void uiItemIntO(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, int value)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0); /* print error next */
	PointerRNA ptr;

	UI_OPERATOR_ERROR_RET(ot, opname, return);

	WM_operator_properties_create_ptr(&ptr, ot);
	RNA_int_set(&ptr, propname, value);

	uiItemFullO_ptr(layout, ot, name, icon, ptr.data, layout->root->opcontext, 0, NULL);
}

void uiItemFloatO(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, float value)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0); /* print error next */
	PointerRNA ptr;

	UI_OPERATOR_ERROR_RET(ot, opname, return);

	WM_operator_properties_create_ptr(&ptr, ot);
	RNA_float_set(&ptr, propname, value);

	uiItemFullO_ptr(layout, ot, name, icon, ptr.data, layout->root->opcontext, 0, NULL);
}

void uiItemStringO(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, const char *value)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0); /* print error next */
	PointerRNA ptr;

	UI_OPERATOR_ERROR_RET(ot, opname, return);

	WM_operator_properties_create_ptr(&ptr, ot);
	RNA_string_set(&ptr, propname, value);

	uiItemFullO_ptr(layout, ot, name, icon, ptr.data, layout->root->opcontext, 0, NULL);
}

void uiItemO(uiLayout *layout, const char *name, int icon, const char *opname)
{
	uiItemFullO(layout, opname, name, icon, NULL, layout->root->opcontext, 0, NULL);
}

/* RNA property items */

static void ui_item_rna_size(
        uiLayout *layout, const char *name, int icon,
        PointerRNA *ptr, PropertyRNA *prop,
        int index, bool icon_only, bool compact,
        int *r_w, int *r_h)
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
			const EnumPropertyItem *item, *item_array;
			bool free;

			RNA_property_enum_items_gettexted(layout->root->block->evil_C, ptr, prop, &item_array, NULL, &free);
			for (item = item_array; item->identifier; item++) {
				if (item->identifier[0]) {
					w = max_ii(w, ui_text_icon_width(layout, item->name, item->icon, compact));
				}
			}
			if (free) {
				MEM_freeN((void *)item_array);
			}
		}
	}

	if (!w) {
		if (type == PROP_ENUM && icon_only) {
			w = ui_text_icon_width(layout, "", ICON_BLANK1, compact);
			if (index != RNA_ENUM_VALUE)
				w += 0.6f * UI_UNIT_X;
		}
		else {
			/* not compact for float/int buttons, looks too squashed */
			w = ui_text_icon_width(layout, name, icon, ELEM(type, PROP_FLOAT, PROP_INT) ? false : compact);
		}
	}
	h = UI_UNIT_Y;

	/* increase height for arrays */
	if (index == RNA_NO_INDEX && len > 0) {
		if (!name[0] && icon == ICON_NONE)
			h = 0;
		if (layout->item.flag & UI_ITEM_PROP_SEP)
			h = 0;
		if (ELEM(subtype, PROP_LAYER, PROP_LAYER_MEMBER))
			h += 2 * UI_UNIT_Y;
		else if (subtype == PROP_MATRIX)
			h += ceilf(sqrtf(len)) * UI_UNIT_Y;
		else
			h += len * UI_UNIT_Y;
	}
	else if (ui_layout_variable_size(layout)) {
		if (type == PROP_BOOLEAN && name[0])
			w += UI_UNIT_X / 5;
		else if (type == PROP_ENUM && !icon_only)
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
	int len, w, h;
	bool slider, toggle, expand, icon_only, no_bg, compact;
	bool is_array;
	const bool use_prop_sep = ((layout->item.flag & UI_ITEM_PROP_SEP) != 0);

#ifdef UI_PROP_DECORATE
	struct {
		bool use_prop_decorate;
		int len;
		uiLayout *layout;
		uiBut *but;
	} ui_decorate = {
		.use_prop_decorate = (
		        ((layout->item.flag & UI_ITEM_PROP_DECORATE) != 0) &&
		        (use_prop_sep && ptr->id.data && id_can_have_animdata(ptr->id.data))),
	};
#endif  /* UI_PROP_DECORATE */

	UI_block_layout_set_current(block, layout);

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
		if (use_prop_sep == false) {
			name = ui_item_name_add_colon(name, namestr);
		}
	}
	else if (type == PROP_BOOLEAN && is_array && index == RNA_NO_INDEX) {
		if (use_prop_sep == false) {
			name = ui_item_name_add_colon(name, namestr);
		}
	}
	else if (type == PROP_ENUM && index != RNA_ENUM_VALUE) {
		if (flag & UI_ITEM_R_COMPACT) {
			name = "";
		}
		else {
			if (use_prop_sep == false) {
				name = ui_item_name_add_colon(name, namestr);
			}
		}
	}

	/* menus and pie-menus don't show checkbox without this */
	if ((layout->root->type == UI_LAYOUT_MENU) ||
	    /* use checkboxes only as a fallback in pie-menu's, when no icon is defined */
	    ((layout->root->type == UI_LAYOUT_PIEMENU) && (icon == ICON_NONE)))
	{
		int prop_flag = RNA_property_flag(prop);
		if (type == PROP_BOOLEAN && ((is_array == false) || (index != RNA_NO_INDEX))) {
			if (prop_flag & PROP_ICONS_CONSECUTIVE) {
				icon = ICON_CHECKBOX_DEHLT; /* but->iconadd will set to correct icon */
			}
			else if (is_array) {
				icon = (RNA_property_boolean_get_index(ptr, prop, index)) ? ICON_CHECKBOX_HLT : ICON_CHECKBOX_DEHLT;
			}
			else {
				icon = (RNA_property_boolean_get(ptr, prop)) ? ICON_CHECKBOX_HLT : ICON_CHECKBOX_DEHLT;
			}
		}
		else if (type == PROP_ENUM && index == RNA_ENUM_VALUE) {
			int enum_value = RNA_property_enum_get(ptr, prop);
			if (prop_flag & PROP_ICONS_CONSECUTIVE) {
				icon = ICON_CHECKBOX_DEHLT; /* but->iconadd will set to correct icon */
			}
			else if (prop_flag & PROP_ENUM_FLAG) {
				icon = (enum_value & value) ? ICON_CHECKBOX_HLT : ICON_CHECKBOX_DEHLT;
			}
			else {
				icon = (enum_value == value) ? ICON_CHECKBOX_HLT : ICON_CHECKBOX_DEHLT;
			}
		}
	}

	if ((type == PROP_ENUM) && (RNA_property_flag(prop) & PROP_ENUM_FLAG)) {
		flag |= UI_ITEM_R_EXPAND;
	}

	slider = (flag & UI_ITEM_R_SLIDER) != 0;
	toggle = (flag & UI_ITEM_R_TOGGLE) != 0;
	expand = (flag & UI_ITEM_R_EXPAND) != 0;
	icon_only = (flag & UI_ITEM_R_ICON_ONLY) != 0;
	no_bg = (flag & UI_ITEM_R_NO_BG) != 0;
	compact = (flag & UI_ITEM_R_COMPACT) != 0;

	/* get size */
	ui_item_rna_size(layout, name, icon, ptr, prop, index, icon_only, compact, &w, &h);

	int prev_emboss = layout->emboss;
	if (no_bg) {
		layout->emboss = UI_EMBOSS_NONE;
	}

	/* Split the label / property. */
	if (use_prop_sep) {
		uiLayout *layout_row = NULL;
#ifdef UI_PROP_DECORATE
		if (ui_decorate.use_prop_decorate) {
			layout_row = uiLayoutRow(layout, true);
			layout_row->space = 0;
			ui_decorate.len = max_ii(1, len);
		}
#endif  /* UI_PROP_DECORATE */

		if (name[0] == '\0') {
			/* Ensure we get a column when text is not set. */
			layout = uiLayoutColumn(layout_row ? layout_row : layout, true);
			layout->space = 0;
		}
		else {
			const PropertySubType subtype = RNA_property_subtype(prop);
			uiLayout *layout_split;
#ifdef UI_PROP_SEP_ICON_WIDTH_EXCEPTION
			if (type == PROP_BOOLEAN && (icon == ICON_NONE) && !icon_only) {
				w = UI_UNIT_X;
				layout_split = uiLayoutRow(layout_row ? layout_row : layout, true);
			}
			else
#endif  /* UI_PROP_SEP_ICON_WIDTH_EXCEPTION */
			{
				layout_split = uiLayoutSplit(
				        layout_row ? layout_row : layout,
				        UI_ITEM_PROP_SEP_DIVIDE, true);
			}
			layout_split->space = 0;
			uiLayout *layout_sub = uiLayoutColumn(layout_split, true);
			layout_sub->space = 0;

			if ((index == RNA_NO_INDEX && is_array) &&
			    ((!expand && ELEM(subtype, PROP_COLOR, PROP_COLOR_GAMMA, PROP_DIRECTION)) == 0))
			{
				char name_with_suffix[UI_MAX_DRAW_STR + 2];
				char str[2] = {'\0'};
				for (int a = 0; a < len; a++) {
					str[0] = RNA_property_array_item_char(prop, a);
					const bool use_prefix = (a == 0 && name && name[0]);
					if (use_prefix) {
						char *s = name_with_suffix;
						s += STRNCPY_RLEN(name_with_suffix, name);
						*s++ = ' ';
						*s++ = str[0];
						*s++ = '\0';
					}
					but = uiDefBut(
					        block, UI_BTYPE_LABEL, 0, use_prefix ? name_with_suffix : str,
					        0, 0, w, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
					but->drawflag |= UI_BUT_TEXT_RIGHT;
					but->drawflag &= ~UI_BUT_TEXT_LEFT;
				}
			}
			else {
				if (name) {
					but = uiDefBut(
					        block, UI_BTYPE_LABEL, 0, name,
					        0, 0, w, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
					but->drawflag |= UI_BUT_TEXT_RIGHT;
					but->drawflag &= ~UI_BUT_TEXT_LEFT;
				}
			}

			/* Watch out! We can only write into the new column now. */
			layout = uiLayoutColumn(layout_split, true);
			layout->space = 0;
			if ((type == PROP_ENUM) && (flag & UI_ITEM_R_EXPAND)) {
				/* pass (expanded enums each have their own name) */
			}
			else {
				name = "";
			}
		}

#ifdef UI_PROP_DECORATE
		if (ui_decorate.use_prop_decorate) {
			ui_decorate.layout = uiLayoutColumn(layout_row, true);
			ui_decorate.layout->space = 0;
			UI_block_layout_set_current(block, layout);
			ui_decorate.but = block->buttons.last;
		}
#endif  /* UI_PROP_DECORATE */
	}
	/* End split. */

	/* array property */
	if (index == RNA_NO_INDEX && is_array) {
		ui_item_array(
		        layout, block, name, icon, ptr, prop, len, 0, 0, w, h,
		        expand, slider, toggle, icon_only, compact, !use_prop_sep);
	}
	/* enum item */
	else if (type == PROP_ENUM && index == RNA_ENUM_VALUE) {
		if (icon && name[0] && !icon_only)
			uiDefIconTextButR_prop(block, UI_BTYPE_ROW, 0, icon, name, 0, 0, w, h, ptr, prop, -1, 0, value, -1, -1, NULL);
		else if (icon)
			uiDefIconButR_prop(block, UI_BTYPE_ROW, 0, icon, 0, 0, w, h, ptr, prop, -1, 0, value, -1, -1, NULL);
		else
			uiDefButR_prop(block, UI_BTYPE_ROW, 0, name, 0, 0, w, h, ptr, prop, -1, 0, value, -1, -1, NULL);
	}
	/* expanded enum */
	else if (type == PROP_ENUM && expand) {
		ui_item_enum_expand(layout, block, ptr, prop, name, h, icon_only);
	}
	/* property with separate label */
	else if (type == PROP_ENUM || type == PROP_STRING || type == PROP_POINTER) {
		but = ui_item_with_label(layout, block, name, icon, ptr, prop, index, 0, 0, w, h, flag);
		ui_but_add_search(but, ptr, prop, NULL, NULL);

		if (layout->redalert)
			UI_but_flag_enable(but, UI_BUT_REDALERT);
	}
	/* single button */
	else {
		but = uiDefAutoButR(block, ptr, prop, index, name, icon, 0, 0, w, h);

		if (slider && but->type == UI_BTYPE_NUM)
			but->type = UI_BTYPE_NUM_SLIDER;

		if (toggle && but->type == UI_BTYPE_CHECKBOX)
			but->type = UI_BTYPE_TOGGLE;

		if (layout->redalert)
			UI_but_flag_enable(but, UI_BUT_REDALERT);
	}

	/* Mark non-embossed textfields inside a listbox. */
	if (but && (block->flag & UI_BLOCK_LIST_ITEM) && (but->type == UI_BTYPE_TEXT) && (but->dt & UI_EMBOSS_NONE)) {
		UI_but_flag_enable(but, UI_BUT_LIST_ITEM);
	}

#ifdef UI_PROP_DECORATE
	if (ui_decorate.use_prop_decorate) {
		const bool is_anim = RNA_property_animateable(ptr, prop);
		uiBut *but_decorate = ui_decorate.but ? ui_decorate.but->next : block->buttons.first;
		uiLayout *layout_col = uiLayoutColumn(ui_decorate.layout, false);
		layout_col->space = 0;
		layout_col->emboss = UI_EMBOSS_NONE;
		int i;
		for (i = 0; i < ui_decorate.len && but_decorate; i++) {
			/* The icons are set in 'ui_but_anim_flag' */
			if (is_anim) {
				but = uiDefIconBut(
				        block, UI_BTYPE_BUT, 0, ICON_DOT, 0, 0, UI_UNIT_X, UI_UNIT_Y,
				        NULL, 0.0, 0.0, 0.0, 0.0, TIP_("Animate property"));
				UI_but_func_set(but, ui_but_anim_decorate_cb, but, NULL);
				but->flag |= UI_BUT_UNDO | UI_BUT_DRAG_LOCK;
			}
			else {
				/* We may show other information here in future, for now use empty space. */
				but = uiDefIconBut(
				        block, UI_BTYPE_BUT, 0, ICON_BLANK1, 0, 0, UI_UNIT_X, UI_UNIT_Y,
				        NULL, 0.0, 0.0, 0.0, 0.0, "");
				but->flag |= UI_BUT_DISABLED;
			}
			/* Order the decorator after the button we decorate, this is used so we can always
			 * do a quick lookup. */
			BLI_remlink(&block->buttons, but);
			BLI_insertlinkafter(&block->buttons, but_decorate, but);
			but_decorate = but->next;
		}
		BLI_assert(ELEM(i, 1, ui_decorate.len));
	}
#endif  /* UI_PROP_DECORATE */

	if (no_bg) {
		layout->emboss = prev_emboss;
	}

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

void uiItemEnumR_prop(uiLayout *layout, const char *name, int icon, struct PointerRNA *ptr, PropertyRNA *prop, int value)
{
	if (RNA_property_type(prop) != PROP_ENUM) {
		const char *propname = RNA_property_identifier(prop);
		ui_item_disabled(layout, propname);
		RNA_warning("property not an enum: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}

	uiItemFullR(layout, ptr, prop, RNA_ENUM_VALUE, value, 0, name, icon);
}

void uiItemEnumR(uiLayout *layout, const char *name, int icon, struct PointerRNA *ptr, const char *propname, int value)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

	if (prop == NULL) {
		ui_item_disabled(layout, propname);
		RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}

	uiItemFullR(layout, ptr, prop, RNA_ENUM_VALUE, value, 0, name, icon);
}

void uiItemEnumR_string(uiLayout *layout, struct PointerRNA *ptr, const char *propname, const char *value, const char *name, int icon)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
	const EnumPropertyItem *item;
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
			MEM_freeN((void *)item);
		}
		ui_item_disabled(layout, propname);
		RNA_warning("enum property value not found: %s", value);
		return;
	}

	for (a = 0; item[a].identifier; a++) {
		if (item[a].value == ivalue) {
			const char *item_name = name ? name : CTX_IFACE_(RNA_property_translation_context(prop), item[a].name);
			const int flag = item_name[0] ? 0 : UI_ITEM_R_ICON_ONLY;

			uiItemFullR(layout, ptr, prop, RNA_ENUM_VALUE, ivalue, flag, item_name, icon ? icon : item[a].icon);
			break;
		}
	}

	if (free) {
		MEM_freeN((void *)item);
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
		const EnumPropertyItem *item;
		int totitem, i;
		bool free;
		uiLayout *split = uiLayoutSplit(layout, 0.0f, false);
		uiLayout *column = uiLayoutColumn(split, false);

		RNA_property_enum_items_gettexted(block->evil_C, ptr, prop, &item, &totitem, &free);

		for (i = 0; i < totitem; i++) {
			if (item[i].identifier[0]) {
				uiItemEnumR_prop(column, item[i].name, item[i].icon, ptr, prop, item[i].value);
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
			MEM_freeN((void *)item);
		}
	}

	/* intentionally don't touch UI_BLOCK_IS_FLIP here,
	 * we don't know the context this is called in */
}

/* Pointer RNA button with search */


static void search_id_collection(StructRNA *ptype, PointerRNA *ptr, PropertyRNA **prop)
{
	StructRNA *srna;

	/* look for collection property in Main */
	/* Note: using global Main is OK-ish here, UI shall not access other Mains anyay... */
	RNA_main_pointer_create(G_MAIN, ptr);

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
		uiRNACollectionSearch *coll_search = MEM_mallocN(sizeof(*coll_search), __func__);

		but->type = UI_BTYPE_SEARCH_MENU;
		but->hardmax = MAX2(but->hardmax, 256.0f);
		but->rnasearchpoin = *searchptr;
		but->rnasearchprop = searchprop;
		but->drawflag |= UI_BUT_ICON_LEFT | UI_BUT_TEXT_LEFT;
		if (RNA_property_is_unlink(prop)) {
			but->flag |= UI_BUT_VALUE_CLEAR;
		}

		coll_search->target_ptr = *ptr;
		coll_search->target_prop = prop;
		coll_search->search_ptr = *searchptr;
		coll_search->search_prop = searchprop;
		coll_search->but_changed = &but->changed;

		if (RNA_property_type(prop) == PROP_ENUM) {
			/* XXX, this will have a menu string,
			 * but in this case we just want the text */
			but->str[0] = 0;
		}

		UI_but_func_search_set(
		        but, ui_searchbox_create_generic, ui_rna_collection_search_cb,
		        coll_search, NULL, NULL);
		but->free_search_arg = true;
	}
	else if (but->type == UI_BTYPE_SEARCH_MENU) {
		/* In case we fail to find proper searchprop, so other code might have already set but->type to search menu... */
		but->flag |= UI_BUT_DISABLED;
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
	const bool use_prop_sep = ((layout->item.flag & UI_ITEM_PROP_SEP) != 0);

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

	if (use_prop_sep == false) {
		name = ui_item_name_add_colon(name, namestr);
	}

	/* create button */
	block = uiLayoutGetBlock(layout);

	ui_item_rna_size(layout, name, icon, ptr, prop, 0, 0, false, &w, &h);
	w += UI_UNIT_X; /* X icon needs more space */
	but = ui_item_with_label(layout, block, name, icon, ptr, prop, 0, 0, 0, w, h, 0);

	ui_but_add_search(but, ptr, prop, searchptr, searchprop);
}

/* menu item */
static void ui_item_menutype_func(bContext *C, uiLayout *layout, void *arg_mt)
{
	MenuType *mt = (MenuType *)arg_mt;

	UI_menutype_draw(C, mt, layout);

	/* menus are created flipped (from event handling pov) */
	layout->root->block->flag ^= UI_BLOCK_IS_FLIP;
}

void ui_item_paneltype_func(bContext *C, uiLayout *layout, void *arg_pt)
{
	PanelType *pt = (PanelType *)arg_pt;
	UI_paneltype_draw(C, pt, layout);

	/* panels are created flipped (from event handling pov) */
	layout->root->block->flag ^= UI_BLOCK_IS_FLIP;
}

static uiBut *ui_item_menu(
        uiLayout *layout, const char *name, int icon, uiMenuCreateFunc func, void *arg, void *argN,
        const char *tip, bool force_menu)
{
	uiBlock *block = layout->root->block;
	uiBut *but;
	int w, h;

	UI_block_layout_set_current(block, layout);

	if (!name)
		name = "";
	if (layout->root->type == UI_LAYOUT_MENU && !icon)
		icon = ICON_BLANK1;

	w = ui_text_icon_width(layout, name, icon, 1);
	h = UI_UNIT_Y;

	if (layout->root->type == UI_LAYOUT_HEADER) { /* ugly .. */
		if (icon == ICON_NONE && force_menu) {
			/* pass */
		}
		else if (force_menu) {
			w += UI_UNIT_X;
		}
		else {
			if (name[0]) {
				w -= UI_UNIT_X / 2;
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

	if (ELEM(layout->root->type, UI_LAYOUT_PANEL, UI_LAYOUT_TOOLBAR) ||
	    (force_menu && layout->root->type != UI_LAYOUT_MENU))  /* We never want a dropdown in menu! */
	{
		UI_but_type_set_menu_from_pulldown(but);
	}

	return but;
}

void uiItemM(uiLayout *layout, const char *menuname, const char *name, int icon)
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

/* popover */
void uiItemPopoverPanel_ptr(uiLayout *layout, bContext *C, PanelType *pt, const char *name, int icon)
{
	if (!name) {
		name = CTX_IFACE_(pt->translation_context, pt->label);
	}

	if (layout->root->type == UI_LAYOUT_MENU && !icon) {
		icon = ICON_BLANK1;
	}

	const bool ok = (pt->poll == NULL) || pt->poll(C, pt);
	if (ok && (pt->draw_header != NULL)) {
		layout = uiLayoutRow(layout, true);
		Panel panel = {
			.type = pt,
			.layout = layout,
			.flag = PNL_POPOVER,
		};
		pt->draw_header(C, &panel);
	}
	uiBut *but = ui_item_menu(layout, name, icon, ui_item_paneltype_func, pt, NULL, NULL, true);
	but->type = UI_BTYPE_POPOVER;
	if (!ok) {
		but->flag |= UI_BUT_DISABLED;
	}
}

void uiItemPopoverPanel(
        uiLayout *layout, bContext *C,
        const char *panel_type, const char *name, int icon)
{
	PanelType *pt = WM_paneltype_find(panel_type, true);
	if (pt == NULL) {
		RNA_warning("Panel type not found '%s'", panel_type);
		return;
	}
	uiItemPopoverPanel_ptr(layout, C, pt, name, icon);
}

void uiItemPopoverPanelFromGroup(
        uiLayout *layout, bContext *C,
        int space_id, int region_id, const char *context, const char *category)
{
	SpaceType *st = BKE_spacetype_from_id(space_id);
	if (st == NULL) {
		RNA_warning("space type not found %d", space_id);
		return;
	}
	ARegionType *art = BKE_regiontype_from_id(st, region_id);
	if (art == NULL) {
		RNA_warning("region type not found %d", region_id);
		return;
	}

	for (PanelType *pt = art->paneltypes.first; pt; pt = pt->next) {
		/* Causes too many panels, check context. */
		if (pt->parent_id[0] == '\0') {
			if (/* (*context == '\0') || */ STREQ(pt->context, context)) {
				if ((*category == '\0') || STREQ(pt->category, category)) {
					if (pt->poll == NULL || pt->poll(C, pt)) {
						uiItemPopoverPanel_ptr(layout, C, pt, NULL, ICON_NONE);
					}
				}
			}
		}
	}
}


/* label item */
static uiBut *uiItemL_(uiLayout *layout, const char *name, int icon)
{
	uiBlock *block = layout->root->block;
	uiBut *but;
	int w;

	UI_block_layout_set_current(block, layout);

	if (!name)
		name = "";
	if (layout->root->type == UI_LAYOUT_MENU && !icon)
		icon = ICON_BLANK1;

	w = ui_text_icon_width(layout, name, icon, 0);

	if (icon && name[0])
		but = uiDefIconTextBut(block, UI_BTYPE_LABEL, 0, icon, name, 0, 0, w, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
	else if (icon)
		but = uiDefIconBut(block, UI_BTYPE_LABEL, 0, icon, 0, 0, w, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
	else
		but = uiDefBut(block, UI_BTYPE_LABEL, 0, name, 0, 0, w, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");

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
			UI_but_drag_set_id(but, ptr->id.data);
}


/* value item */
void uiItemV(uiLayout *layout, const char *name, int icon, int argval)
{
	/* label */
	uiBlock *block = layout->root->block;
	int *retvalue = (block->handle) ? &block->handle->retvalue : NULL;
	int w;

	UI_block_layout_set_current(block, layout);

	if (!name)
		name = "";
	if (layout->root->type == UI_LAYOUT_MENU && !icon)
		icon = ICON_BLANK1;

	w = ui_text_icon_width(layout, name, icon, 0);

	if (icon && name[0])
		uiDefIconTextButI(block, UI_BTYPE_BUT, argval, icon, name, 0, 0, w, UI_UNIT_Y, retvalue, 0.0, 0.0, 0, -1, "");
	else if (icon)
		uiDefIconButI(block, UI_BTYPE_BUT, argval, icon, 0, 0, w, UI_UNIT_Y, retvalue, 0.0, 0.0, 0, -1, "");
	else
		uiDefButI(block, UI_BTYPE_BUT, argval, name, 0, 0, w, UI_UNIT_Y, retvalue, 0.0, 0.0, 0, -1, "");
}

/* separator item */
void uiItemS(uiLayout *layout)
{
	uiBlock *block = layout->root->block;
	bool is_menu = ui_block_is_menu(block);
	int space = (is_menu) ? 0.45f * UI_UNIT_X : 0.3f * UI_UNIT_X;

	UI_block_layout_set_current(block, layout);
	uiDefBut(block, (is_menu) ? UI_BTYPE_SEPR_LINE : UI_BTYPE_SEPR, 0, "", 0, 0, space, space, NULL, 0.0, 0.0, 0, 0, "");
}

/* Flexible spacing. */
void uiItemSpacer(uiLayout *layout)
{
	uiBlock *block = layout->root->block;
	bool is_menu = ui_block_is_menu(block);

	if (is_menu) {
		printf("Error: separator_spacer() not supported in menus.\n");
		return;
	}

	if (block->direction & UI_DIR_RIGHT) {
		printf("Error: separator_spacer() only supported in horizontal blocks.\n");
		return;
	}

	UI_block_layout_set_current(block, layout);
	uiDefBut(block, UI_BTYPE_SEPR_SPACER, 0, "", 0, 0, 0.3f * UI_UNIT_X, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
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

	layout->root->block->flag |= UI_BLOCK_IS_FLIP;

	/* override default, needed since this was assumed pre 2.70 */
	UI_block_direction_set(layout->root->block, UI_DIR_DOWN);
}

void uiItemMenuEnumO_ptr(
        uiLayout *layout, bContext *C, wmOperatorType *ot, const char *propname,
        const char *name, int icon)
{
	MenuItemLevel *lvl;
	uiBut *but;

	/* Caller must check */
	BLI_assert(ot->srna != NULL);

	if (name == NULL) {
		name = RNA_struct_ui_name(ot->srna);
	}

	if (layout->root->type == UI_LAYOUT_MENU && !icon)
		icon = ICON_BLANK1;

	lvl = MEM_callocN(sizeof(MenuItemLevel), "MenuItemLevel");
	BLI_strncpy(lvl->opname, ot->idname, sizeof(lvl->opname));
	BLI_strncpy(lvl->propname, propname, sizeof(lvl->propname));
	lvl->opcontext = layout->root->opcontext;

	but = ui_item_menu(
	        layout, name, icon, menu_item_enum_opname_menu, NULL, lvl,
	        RNA_struct_ui_description(ot->srna), true);

	/* add hotkey here, lower UI code can't detect it */
	if ((layout->root->block->flag & UI_BLOCK_LOOP) &&
	    (ot->prop && ot->invoke))
	{
		char keybuf[128];
		if (WM_key_event_operator_string(
		        C, ot->idname, layout->root->opcontext, NULL, false,
		        keybuf, sizeof(keybuf)))
		{
			ui_but_add_shortcut(but, keybuf, false);
		}
	}
}

void uiItemMenuEnumO(
        uiLayout *layout, bContext *C, const char *opname, const char *propname,
        const char *name, int icon)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0); /* print error next */

	UI_OPERATOR_ERROR_RET(ot, opname, return);

	if (!ot->srna) {
		ui_item_disabled(layout, opname);
		RNA_warning("operator missing srna '%s'", opname);
		return;
	}

	uiItemMenuEnumO_ptr(layout, C, ot, propname, name, icon);
}

static void menu_item_enum_rna_menu(bContext *UNUSED(C), uiLayout *layout, void *arg)
{
	MenuItemLevel *lvl = (MenuItemLevel *)(((uiBut *)arg)->func_argN);

	uiLayoutSetOperatorContext(layout, lvl->opcontext);
	uiItemsEnumR(layout, &lvl->rnapoin, lvl->propname);
	layout->root->block->flag |= UI_BLOCK_IS_FLIP;
}

void uiItemMenuEnumR_prop(uiLayout *layout, struct PointerRNA *ptr, PropertyRNA *prop, const char *name, int icon)
{
	MenuItemLevel *lvl;

	if (!name)
		name = RNA_property_ui_name(prop);
	if (layout->root->type == UI_LAYOUT_MENU && !icon)
		icon = ICON_BLANK1;

	lvl = MEM_callocN(sizeof(MenuItemLevel), "MenuItemLevel");
	lvl->rnapoin = *ptr;
	BLI_strncpy(lvl->propname, RNA_property_identifier(prop), sizeof(lvl->propname));
	lvl->opcontext = layout->root->opcontext;

	ui_item_menu(layout, name, icon, menu_item_enum_rna_menu, NULL, lvl, RNA_property_description(prop), false);
}

void uiItemMenuEnumR(uiLayout *layout, struct PointerRNA *ptr, const char *propname, const char *name, int icon)
{
	PropertyRNA *prop;

	prop = RNA_struct_find_property(ptr, propname);
	if (!prop) {
		ui_item_disabled(layout, propname);
		RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}

	uiItemMenuEnumR_prop(layout, ptr, prop, name, icon);
}

/**************************** Layout Items ***************************/

/* single-row layout */
static void ui_litem_estimate_row(uiLayout *litem)
{
	uiItem *item;
	int itemw, itemh;
	bool min_size_flag = true;

	litem->w = 0;
	litem->h = 0;

	for (item = litem->items.first; item; item = item->next) {
		ui_item_size(item, &itemw, &itemh);

		min_size_flag = min_size_flag && (item->flag & UI_ITEM_MIN);

		litem->w += itemw;
		litem->h = MAX2(itemh, litem->h);

		if (item->next)
			litem->w += litem->space;
	}

	if (min_size_flag) {
		litem->item.flag |= UI_ITEM_MIN;
	}
}

static int ui_litem_min_width(int itemw)
{
	return MIN2(2 * UI_UNIT_X, itemw);
}

static void ui_litem_layout_row(uiLayout *litem)
{
	uiItem *item, *last_free_item = NULL;
	int x, y, w, tot, totw, neww, newtotw, itemw, minw, itemh, offset;
	int fixedw, freew, fixedx, freex, flag = 0, lastw = 0;
	float extra_pixel;

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
		extra_pixel = 0.0f;

		for (item = litem->items.first; item; item = item->next) {
			if (item->flag & UI_ITEM_FIXED)
				continue;

			ui_item_size(item, &itemw, &itemh);
			minw = ui_litem_min_width(itemw);

			if (w - lastw > 0)
				neww = ui_item_fit(itemw, x, totw, w - lastw, !item->next, litem->alignment, &extra_pixel);
			else
				neww = 0;  /* no space left, all will need clamping to minimum size */

			x += neww;

			bool min_flag = item->flag & UI_ITEM_MIN;
			/* ignore min flag for rows with right or center alignment */
			if (item->type != ITEM_BUTTON &&
			    ELEM(((uiLayout *)item)->alignment, UI_LAYOUT_ALIGN_RIGHT, UI_LAYOUT_ALIGN_CENTER) &&
			    litem->alignment == UI_LAYOUT_ALIGN_EXPAND &&
			    ((uiItem *)litem)->flag & UI_ITEM_MIN)
			{
				min_flag = false;
			}

			if ((neww < minw || min_flag) && w != 0) {
				/* fixed size */
				item->flag |= UI_ITEM_FIXED;
				if (item->type != ITEM_BUTTON && item->flag & UI_ITEM_MIN) {
					minw = itemw;
				}
				fixedw += minw;
				flag = 1;
				newtotw -= itemw;
			}
			else {
				/* keep free size */
				item->flag &= ~UI_ITEM_FIXED;
				freew += itemw;
			}
		}

		totw = newtotw;
		lastw = fixedw;
	} while (flag);

	freex = 0;
	fixedx = 0;
	extra_pixel = 0.0f;
	x = litem->x;

	for (item = litem->items.first; item; item = item->next) {
		ui_item_size(item, &itemw, &itemh);
		minw = ui_litem_min_width(itemw);

		if (item->flag & UI_ITEM_FIXED) {
			/* fixed minimum size items */
			if (item->type != ITEM_BUTTON && item->flag & UI_ITEM_MIN) {
				minw = itemw;
			}
			itemw = ui_item_fit(minw, fixedx, fixedw, min_ii(w, fixedw), !item->next, litem->alignment, &extra_pixel);
			fixedx += itemw;
		}
		else {
			/* free size item */
			itemw = ui_item_fit(itemw, freex, freew, w - fixedw, !item->next, litem->alignment, &extra_pixel);
			freex += itemw;
			last_free_item = item;
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

	/* add extra pixel */
	uiItem *last_item = litem->items.last;
	extra_pixel = litem->w - (x - litem->x);
	if (extra_pixel > 0 && litem->alignment == UI_LAYOUT_ALIGN_EXPAND &&
	    last_free_item && last_item && last_item->flag & UI_ITEM_FIXED)
	{
		ui_item_move(last_free_item, 0, extra_pixel);
		for (item = last_free_item->next; item; item = item->next)
			ui_item_move(item, extra_pixel, extra_pixel);
	}

	litem->w = x - litem->x;
	litem->h = litem->y - y;
	litem->x = x;
	litem->y = y;
}

/* single-column layout */
static void ui_litem_estimate_column(uiLayout *litem, bool is_box)
{
	uiItem *item;
	int itemw, itemh;
	bool min_size_flag = true;

	litem->w = 0;
	litem->h = 0;

	for (item = litem->items.first; item; item = item->next) {
		ui_item_size(item, &itemw, &itemh);

		min_size_flag = min_size_flag && (item->flag & UI_ITEM_MIN);

		litem->w = MAX2(litem->w, itemw);
		litem->h += itemh;

		if (item->next && (!is_box || item != litem->items.first))
			litem->h += litem->space;
	}

	if (min_size_flag) {
		litem->item.flag |= UI_ITEM_MIN;
	}
}

static void ui_litem_layout_column(uiLayout *litem, bool is_box)
{
	uiItem *item;
	int itemh, x, y;

	x = litem->x;
	y = litem->y;

	for (item = litem->items.first; item; item = item->next) {
		ui_item_size(item, NULL, &itemh);

		y -= itemh;
		ui_item_position(item, x, y, litem->w, itemh);

		if (item->next && (!is_box || item != litem->items.first))
			y -= litem->space;

		if (is_box) {
			item->flag |= UI_ITEM_BOX_ITEM;
		}
	}

	litem->h = litem->y - y;
	litem->x = x;
	litem->y = y;
}

/* calculates the angle of a specified button in a radial menu,
 * stores a float vector in unit circle */
static RadialDirection ui_get_radialbut_vec(float vec[2], short itemnum)
{
	RadialDirection dir;

	if (itemnum >= PIE_MAX_ITEMS) {
		itemnum %= PIE_MAX_ITEMS;
		printf("Warning: Pie menus with more than %i items are currently unsupported\n", PIE_MAX_ITEMS);
	}

	dir = ui_radial_dir_order[itemnum];
	ui_but_pie_dir(dir, vec);

	return dir;
}

static bool ui_item_is_radial_displayable(uiItem *item)
{

	if ((item->type == ITEM_BUTTON) && (((uiButtonItem *)item)->but->type == UI_BTYPE_LABEL))
		return false;

	return true;
}

static bool ui_item_is_radial_drawable(uiButtonItem *bitem)
{

	if (ELEM(bitem->but->type, UI_BTYPE_SEPR, UI_BTYPE_SEPR_LINE, UI_BTYPE_SEPR_SPACER))
		return false;

	return true;
}

static void ui_litem_layout_radial(uiLayout *litem)
{
	uiItem *item;
	int itemh, itemw, x, y;
	int itemnum = 0;
	int totitems = 0;

	/* For the radial layout we will use Matt Ebb's design
	 * for radiation, see http://mattebb.com/weblog/radiation/
	 * also the old code at http://developer.blender.org/T5103
	 */

	int pie_radius = U.pie_menu_radius * UI_DPI_FAC;

	x = litem->x;
	y = litem->y;

	int minx = x, miny = y, maxx = x, maxy = y;

	/* first count total items */
	for (item = litem->items.first; item; item = item->next)
		totitems++;

	if (totitems < 5)
		litem->root->block->pie_data.flags |= UI_PIE_DEGREES_RANGE_LARGE;

	for (item = litem->items.first; item; item = item->next) {
		/* not all button types are drawn in a radial menu, do filtering here */
		if (ui_item_is_radial_displayable(item)) {
			RadialDirection dir;
			float vec[2];
			float factor[2];

			dir = ui_get_radialbut_vec(vec, itemnum);
			factor[0] = (vec[0] > 0.01f) ? 0.0f : ((vec[0] < -0.01f) ? -1.0f : -0.5f);
			factor[1] = (vec[1] > 0.99f) ? 0.0f : ((vec[1] < -0.99f) ? -1.0f : -0.5f);

			itemnum++;

			if (item->type == ITEM_BUTTON) {
				uiButtonItem *bitem = (uiButtonItem *) item;

				bitem->but->pie_dir = dir;
				/* scale the buttons */
				bitem->but->rect.ymax *= 1.5f;
				/* add a little bit more here to include number */
				bitem->but->rect.xmax += 1.5f * UI_UNIT_X;
				/* enable drawing as pie item if supported by widget */
				if (ui_item_is_radial_drawable(bitem)) {
					bitem->but->dt = UI_EMBOSS_RADIAL;
					bitem->but->drawflag |= UI_BUT_ICON_LEFT;
				}
			}

			ui_item_size(item, &itemw, &itemh);

			ui_item_position(item, x + vec[0] * pie_radius + factor[0] * itemw, y + vec[1] * pie_radius + factor[1] * itemh, itemw, itemh);

			minx = min_ii(minx, x + vec[0] * pie_radius - itemw / 2);
			maxx = max_ii(maxx, x + vec[0] * pie_radius + itemw / 2);
			miny = min_ii(miny, y + vec[1] * pie_radius - itemh / 2);
			maxy = max_ii(maxy, y + vec[1] * pie_radius + itemh / 2);
		}
	}

	litem->x = minx;
	litem->y = miny;
	litem->w = maxx - minx;
	litem->h = maxy - miny;
}

/* root layout */
static void ui_litem_estimate_root(uiLayout *UNUSED(litem))
{
	/* nothing to do */
}

static void ui_litem_layout_root_radial(uiLayout *litem)
{
	/* first item is pie menu title, align on center of menu */
	uiItem *item = litem->items.first;

	if (item->type == ITEM_BUTTON) {
		int itemh, itemw, x, y;
		x = litem->x;
		y = litem->y;

		ui_item_size(item, &itemw, &itemh);

		ui_item_position(item, x - itemw / 2, y + U.pixelsize * (U.pie_menu_threshold + 9.0f), itemw, itemh);
	}
}

static void ui_litem_layout_root(uiLayout *litem)
{
	if (litem->root->type == UI_LAYOUT_HEADER)
		ui_litem_layout_row(litem);
	else if (litem->root->type == UI_LAYOUT_PIEMENU)
		ui_litem_layout_root_radial(litem);
	else
		ui_litem_layout_column(litem, false);
}

/* box layout */
static void ui_litem_estimate_box(uiLayout *litem)
{
	uiStyle *style = litem->root->style;

	ui_litem_estimate_column(litem, true);
	litem->w += 2 * style->boxspace;
	litem->h += 2 * style->boxspace;
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
	litem->y -= style->boxspace;

	if (w != 0) litem->w -= 2 * style->boxspace;
	if (h != 0) litem->h -= 2 * style->boxspace;

	ui_litem_layout_column(litem, true);

	litem->x -= style->boxspace;
	litem->y -= style->boxspace;

	if (w != 0) litem->w += 2 * style->boxspace;
	if (h != 0) litem->h += 2 * style->boxspace;

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
	int toth, totitem;

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
	w = (litem->w - (flow->totcol - 1) * style->columnspace) / flow->totcol;
	for (item = litem->items.first; item; item = item->next) {
		ui_item_size(item, &itemw, &itemh);

		itemw = (litem->alignment == UI_LAYOUT_ALIGN_EXPAND) ? w : min_ii(w, itemw);

		y -= itemh;
		emy -= itemh;
		ui_item_position(item, x, y, itemw, itemh);
		y -= style->buttonspacey;
		miny = min_ii(miny, y);

		/* decide to go to next one */
		if (col < flow->totcol - 1 && emy <= -emh) {
			x += w + style->columnspace;
			y = litem->y;
			emy = 0; /* need to reset height again for next column */
			col++;

			/*  (<     remaining width     > - <      space between remaining columns      >) / <remamining columns > */
			w = ((litem->w - (x - litem->x)) - (flow->totcol - col - 1) * style->columnspace) / (flow->totcol - col);
		}
	}

	litem->h = litem->y - miny;
	litem->x = x;
	litem->y = miny;
}

/* multi-column and multi-row layout. */
typedef struct UILayoutGridFlowInput {
	/* General layout controll settings. */
	const bool row_major : 1;  /* Fill rows before columns */
	const bool even_columns : 1;  /* All columns will have same width. */
	const bool even_rows : 1;  /* All rows will have same height. */
	const int space_x;  /* Space between columns. */
	const int space_y;  /* Space between rows. */
	/* Real data about current position and size of this layout item (either estimated, or final values). */
	const int litem_w;  /* Layout item width. */
	const int litem_x;  /* Layout item X position. */
	const int litem_y;  /* Layout item Y position. */
	/* Actual number of columns and rows to generate (computed from first pass usually). */
	const int tot_columns;  /* Number of columns. */
	const int tot_rows;  /* Number of rows. */
} UILayoutGridFlowInput;

typedef struct UILayoutGridFlowOutput {
	int *tot_items;  /* Total number of items in this grid layout. */
	/* Width / X pos data. */
	float *global_avg_w;  /* Computed average width of the columns. */
	int *cos_x_array;  /* Computed X coordinate of each column. */
	int *widths_array;  /* Computed width of each column. */
	int *tot_w;  /* Computed total width. */
	/* Height / Y pos data. */
	int *global_max_h;  /* Computed height of the tallest item in the grid. */
	int *cos_y_array;  /* Computed Y coordinate of each column. */
	int *heights_array;  /* Computed height of each column. */
	int *tot_h;  /* Computed total height. */
} UILayoutGridFlowOutput;

static void ui_litem_grid_flow_compute(
        ListBase *items, UILayoutGridFlowInput *parameters, UILayoutGridFlowOutput *results)
{
	uiItem *item;
	int i;

	float tot_w = 0.0f, tot_h = 0.0f;
	float global_avg_w = 0.0f, global_totweight_w = 0.0f;
	int global_max_h = 0;

	float *avg_w = NULL, *totweight_w = NULL;
	int *max_h = NULL;

	BLI_assert(parameters->tot_columns != 0 || (results->cos_x_array == NULL && results->widths_array == NULL && results->tot_w == NULL));
	BLI_assert(parameters->tot_rows != 0 || (results->cos_y_array == NULL && results->heights_array == NULL && results->tot_h == NULL));

	if (results->tot_items) {
		*results->tot_items = 0;
	}

	if (items->first == NULL) {
		if (results->global_avg_w) {
			*results->global_avg_w = 0.0f;
		}
		if (results->global_max_h) {
			*results->global_max_h = 0;
		}
		return;
	}

	if (parameters->tot_columns != 0) {
		avg_w = BLI_array_alloca(avg_w, parameters->tot_columns);
		totweight_w = BLI_array_alloca(totweight_w, parameters->tot_columns);
		memset(avg_w, 0, sizeof(*avg_w) * parameters->tot_columns);
		memset(totweight_w, 0, sizeof(*totweight_w) * parameters->tot_columns);
	}
	if (parameters->tot_rows != 0) {
		max_h = BLI_array_alloca(max_h, parameters->tot_rows);
		memset(max_h, 0, sizeof(*max_h) * parameters->tot_rows);
	}

	for (i = 0, item = items->first; item; item = item->next, i++) {
		int item_w, item_h;
		ui_item_size(item, &item_w, &item_h);

		global_avg_w += (float)(item_w * item_w);
		global_totweight_w += (float)item_w;
		global_max_h = max_ii(global_max_h, item_h);

		if (parameters->tot_rows != 0 && parameters->tot_columns != 0) {
			const int index_col = parameters->row_major ? i % parameters->tot_columns : i / parameters->tot_rows;
			const int index_row = parameters->row_major ? i / parameters->tot_columns : i % parameters->tot_rows;

			avg_w[index_col] += (float)(item_w * item_w);
			totweight_w[index_col] += (float)item_w;

			max_h[index_row] = max_ii(max_h[index_row], item_h);
		}

		if (results->tot_items) {
			(*results->tot_items)++;
		}
	}

	/* Finalize computing of column average sizes */
	global_avg_w /= global_totweight_w;
	if (parameters->tot_columns != 0) {
		for (i = 0; i < parameters->tot_columns; i++) {
			avg_w[i] /= totweight_w[i];
			tot_w += avg_w[i];
		}
		if (parameters->even_columns) {
			tot_w = ceilf(global_avg_w) * parameters->tot_columns;
		}
	}
	/* Finalize computing of rows max sizes */
	if (parameters->tot_rows != 0) {
		for (i = 0; i < parameters->tot_rows; i++) {
			tot_h += max_h[i];
		}
		if (parameters->even_rows) {
			tot_h = global_max_h * parameters->tot_columns;
		}
	}

	/* Compute positions and sizes of all cells. */
	if (results->cos_x_array != NULL && results->widths_array != NULL) {
		/* We enlarge/narrow columns evenly to match available width. */
		const float wfac = (float)(parameters->litem_w - (parameters->tot_columns - 1) * parameters->space_x) / tot_w;

		for (int col = 0; col < parameters->tot_columns; col++) {
			results->cos_x_array[col] = (
			        col ?
			        results->cos_x_array[col - 1] + results->widths_array[col - 1] + parameters->space_x :
			        parameters->litem_x
			);
			if (parameters->even_columns) {
				/* (< remaining width > - < space between remaining columns >) / < remaining columns > */
				results->widths_array[col] = (
				        ((parameters->litem_w - (results->cos_x_array[col] - parameters->litem_x)) -
				         (parameters->tot_columns - col - 1) * parameters->space_x) / (parameters->tot_columns - col));
			}
			else if (col == parameters->tot_columns - 1) {
				/* Last column copes width rounding errors... */
				results->widths_array[col] = parameters->litem_w - (results->cos_x_array[col] - parameters->litem_x);
			}
			else {
				results->widths_array[col] = (int)(avg_w[col] * wfac);
			}
		}
	}
	if (results->cos_y_array != NULL && results->heights_array != NULL) {
		for (int row = 0; row < parameters->tot_rows; row++) {
			if (parameters->even_rows) {
				results->heights_array[row] = global_max_h;
			}
			else {
				results->heights_array[row] = max_h[row];
			}
			results->cos_y_array[row] = (
			        row ?
			        results->cos_y_array[row - 1] - parameters->space_y - results->heights_array[row] :
			        parameters->litem_y - results->heights_array[row]);
		}
	}

	if (results->global_avg_w) {
		*results->global_avg_w = global_avg_w;
	}
	if (results->global_max_h) {
		*results->global_max_h = global_max_h;
	}
	if (results->tot_w) {
		*results->tot_w = (int)tot_w + parameters->space_x * (parameters->tot_columns - 1);
	}
	if (results->tot_h) {
		*results->tot_h = tot_h + parameters->space_y * (parameters->tot_rows - 1);
	}
}

static void ui_litem_estimate_grid_flow(uiLayout *litem)
{
	uiStyle *style = litem->root->style;
	uiLayoutItemGridFlow *gflow = (uiLayoutItemGridFlow *)litem;

	const int space_x = style->columnspace;
	const int space_y = style->buttonspacey;

	/* Estimate average needed width and height per item. */
	{
		float avg_w;
		int max_h;

		ui_litem_grid_flow_compute(
		        &litem->items,
		        &((UILayoutGridFlowInput) {
		              .row_major = gflow->row_major,
		              .even_columns = gflow->even_columns,
		              .even_rows = gflow->even_rows,
		              .litem_w = litem->w,
		              .litem_x = litem->x,
		              .litem_y = litem->y,
		              .space_x = space_x,
		              .space_y = space_y,
		        }),
		        &((UILayoutGridFlowOutput) {
		              .tot_items = &gflow->tot_items,
		              .global_avg_w = &avg_w,
		              .global_max_h = &max_h,
		        }));

		if (gflow->tot_items == 0) {
			litem->w = litem->h = 0;
			gflow->tot_columns = gflow->tot_rows = 0;
			return;
		}

		/* Even in varying column width case, we fix our columns number from weighted average width of items,
		 * a proper solving of required width would be too costly, and this should give reasonably good results
		 * in all resonable cases... */
		if (gflow->columns_len > 0) {
			gflow->tot_columns = gflow->columns_len;
		}
		else {
			if (avg_w == 0.0f) {
				gflow->tot_columns = 1;
			}
			else {
				gflow->tot_columns = min_ii(max_ii((int)(litem->w / avg_w), 1), gflow->tot_items);
			}
		}
		gflow->tot_rows = (int)ceilf((float)gflow->tot_items / gflow->tot_columns);

		/* Try to tweak number of columns and rows to get better filling of last column or row,
		 * and apply 'modulo' value to number of columns or rows.
		 * Note that modulo does not prevent ending with fewer columns/rows than modulo, if mandatory
		 * to avoid empty column/row. */
		{
			const int modulo = (gflow->columns_len < -1) ? -gflow->columns_len : 0;
			const int step = modulo ? modulo : 1;

			if (gflow->row_major) {
				/* Adjust number of columns to be mutiple of given modulo. */
				if (modulo && gflow->tot_columns % modulo != 0 && gflow->tot_columns > modulo) {
					gflow->tot_columns = gflow->tot_columns - (gflow->tot_columns % modulo);
				}
				/* Find smallest number of columns conserving computed optimal number of rows. */
				for (gflow->tot_rows = (int)ceilf((float)gflow->tot_items / gflow->tot_columns);
				     (gflow->tot_columns - step) > 0 &&
				     (int)ceilf((float)gflow->tot_items / (gflow->tot_columns - step)) <= gflow->tot_rows;
				     gflow->tot_columns -= step);
			}
			else {
				/* Adjust number of rows to be mutiple of given modulo. */
				if (modulo && gflow->tot_rows % modulo != 0) {
					gflow->tot_rows = min_ii(gflow->tot_rows + modulo - (gflow->tot_rows % modulo), gflow->tot_items);
				}
				/* Find smallest number of rows conserving computed optimal number of columns. */
				for (gflow->tot_columns = (int)ceilf((float)gflow->tot_items / gflow->tot_rows);
				     (gflow->tot_rows - step) > 0 &&
				     (int)ceilf((float)gflow->tot_items / (gflow->tot_rows - step)) <= gflow->tot_columns;
				     gflow->tot_rows -= step);
			}
		}

		/* Set evenly-spaced axes size (quick optimization in case we have even columns and rows). */
		if (gflow->even_columns && gflow->even_rows) {
			litem->w = (int)(gflow->tot_columns * avg_w) + space_x * (gflow->tot_columns - 1);
			litem->h = (int)(gflow->tot_rows * max_h) + space_y * (gflow->tot_rows - 1);
			return;
		}
	}

	/* Now that we have our final number of columns and rows,
	 * we can compute actual needed space for non-evenly sized axes. */
	{
		int tot_w, tot_h;

		ui_litem_grid_flow_compute(
		        &litem->items,
		        &((UILayoutGridFlowInput) {
		              .row_major = gflow->row_major,
		              .even_columns = gflow->even_columns,
		              .even_rows = gflow->even_rows,
		              .litem_w = litem->w,
		              .litem_x = litem->x,
		              .litem_y = litem->y,
		              .space_x = space_x,
		              .space_y = space_y,
		              .tot_columns = gflow->tot_columns,
		              .tot_rows = gflow->tot_rows,
		        }),
		        &((UILayoutGridFlowOutput) {
		              .tot_w = &tot_w,
		              .tot_h = &tot_h,
		        }));

		litem->w = tot_w;
		litem->h = tot_h;
	}
}

static void ui_litem_layout_grid_flow(uiLayout *litem)
{
	int i;
	uiStyle *style = litem->root->style;
	uiLayoutItemGridFlow *gflow = (uiLayoutItemGridFlow *)litem;
	uiItem *item;

	if (gflow->tot_items == 0) {
		litem->w = litem->h = 0;
		return;
	}

	BLI_assert(gflow->tot_columns > 0);
	BLI_assert(gflow->tot_rows > 0);

	const int space_x = style->columnspace;
	const int space_y = style->buttonspacey;

	int *widths = BLI_array_alloca(widths, gflow->tot_columns);
	int *heights = BLI_array_alloca(heights, gflow->tot_rows);
	int *cos_x = BLI_array_alloca(cos_x, gflow->tot_columns);
	int *cos_y = BLI_array_alloca(cos_y, gflow->tot_rows);

	/* This time we directly compute coordinates and sizes of all cells. */
	ui_litem_grid_flow_compute(
	        &litem->items,
	        &((UILayoutGridFlowInput) {
	              .row_major = gflow->row_major,
	              .even_columns = gflow->even_columns,
	              .even_rows = gflow->even_rows,
	              .litem_w = litem->w,
	              .litem_x = litem->x,
	              .litem_y = litem->y,
	              .space_x = space_x,
	              .space_y = space_y,
	              .tot_columns = gflow->tot_columns,
	              .tot_rows = gflow->tot_rows,
	        }),
	        &((UILayoutGridFlowOutput) {
	              .cos_x_array = cos_x,
	              .cos_y_array = cos_y,
	              .widths_array = widths,
	              .heights_array = heights,
	        }));

	for (item = litem->items.first, i = 0; item; item = item->next, i++) {
		const int col = gflow->row_major ? i % gflow->tot_columns : i / gflow->tot_rows;
		const int row = gflow->row_major ? i / gflow->tot_columns : i % gflow->tot_rows;
		int item_w, item_h;
		ui_item_size(item, &item_w, &item_h);

		const int w = widths[col];
		const int h = heights[row];

		item_w = (litem->alignment == UI_LAYOUT_ALIGN_EXPAND) ? w : min_ii(w, item_w);
		item_h = (litem->alignment == UI_LAYOUT_ALIGN_EXPAND) ? h : min_ii(h, item_h);

		ui_item_position(item, cos_x[col], cos_y[row], item_w, item_h);
	}

	litem->h = litem->y - cos_y[gflow->tot_rows - 1];
	litem->x = (cos_x[gflow->tot_columns - 1] - litem->x) + widths[gflow->tot_columns - 1];
	litem->y = litem->y - litem->h;
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
	litem->item.flag &= ~UI_ITEM_MIN;
}

static void ui_litem_layout_split(uiLayout *litem)
{
	uiLayoutItemSplit *split = (uiLayoutItemSplit *)litem;
	uiItem *item;
	float percentage, extra_pixel = 0.0f;
	const int tot = BLI_listbase_count(&litem->items);
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
			const float width = extra_pixel + (w - (int)(w * percentage)) / ((float)tot - 1);
			extra_pixel = width - (int)width;
			colw = (int)width;
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

static void ui_litem_init_from_parent(uiLayout *litem, uiLayout *layout, int align)
{
	litem->root = layout->root;
	litem->align = align;
	/* Children of gridflow layout shall never have "ideal big size" returned as estimated size. */
	litem->variable_size = layout->variable_size || layout->item.type == ITEM_LAYOUT_GRID_FLOW;
	litem->active = true;
	litem->enabled = true;
	litem->context = layout->context;
	litem->redalert = layout->redalert;
	litem->w = layout->w;
	litem->emboss = layout->emboss;
	litem->item.flag = (layout->item.flag & (UI_ITEM_PROP_SEP | UI_ITEM_PROP_DECORATE));
	BLI_addtail(&layout->items, litem);
}

/* layout create functions */
uiLayout *uiLayoutRow(uiLayout *layout, bool align)
{
	uiLayout *litem;

	litem = MEM_callocN(sizeof(uiLayout), "uiLayoutRow");
	ui_litem_init_from_parent(litem, layout, align);

	litem->item.type = ITEM_LAYOUT_ROW;
	litem->space = (align) ? 0 : layout->root->style->buttonspacex;

	UI_block_layout_set_current(layout->root->block, litem);

	return litem;
}

uiLayout *uiLayoutColumn(uiLayout *layout, bool align)
{
	uiLayout *litem;

	litem = MEM_callocN(sizeof(uiLayout), "uiLayoutColumn");
	ui_litem_init_from_parent(litem, layout, align);

	litem->item.type = ITEM_LAYOUT_COLUMN;
	litem->space = (align) ? 0 : layout->root->style->buttonspacey;

	UI_block_layout_set_current(layout->root->block, litem);

	return litem;
}

uiLayout *uiLayoutColumnFlow(uiLayout *layout, int number, bool align)
{
	uiLayoutItemFlow *flow;

	flow = MEM_callocN(sizeof(uiLayoutItemFlow), "uiLayoutItemFlow");
	ui_litem_init_from_parent(&flow->litem, layout, align);

	flow->litem.item.type = ITEM_LAYOUT_COLUMN_FLOW;
	flow->litem.space = (flow->litem.align) ? 0 : layout->root->style->columnspace;
	flow->number = number;

	UI_block_layout_set_current(layout->root->block, &flow->litem);

	return &flow->litem;
}

uiLayout *uiLayoutGridFlow(
        uiLayout *layout, bool row_major, int columns_len, bool even_columns, bool even_rows, bool align)
{
	uiLayoutItemGridFlow *flow;

	flow = MEM_callocN(sizeof(uiLayoutItemGridFlow), __func__);
	flow->litem.item.type = ITEM_LAYOUT_GRID_FLOW;
	ui_litem_init_from_parent(&flow->litem, layout, align);

	flow->litem.space = (flow->litem.align) ? 0 : layout->root->style->columnspace;
	flow->row_major = row_major;
	flow->columns_len = columns_len;
	flow->even_columns = even_columns;
	flow->even_rows = even_rows;

	UI_block_layout_set_current(layout->root->block, &flow->litem);

	return &flow->litem;
}

static uiLayoutItemBx *ui_layout_box(uiLayout *layout, int type)
{
	uiLayoutItemBx *box;

	box = MEM_callocN(sizeof(uiLayoutItemBx), "uiLayoutItemBx");
	ui_litem_init_from_parent(&box->litem, layout, false);

	box->litem.item.type = ITEM_LAYOUT_BOX;
	box->litem.space = layout->root->style->columnspace;

	UI_block_layout_set_current(layout->root->block, &box->litem);

	box->roundbox = uiDefBut(layout->root->block, type, 0, "", 0, 0, 0, 0, NULL, 0.0, 0.0, 0, 0, "");

	return box;
}

uiLayout *uiLayoutRadial(uiLayout *layout)
{
	uiLayout *litem;
	uiItem *item;

	/* radial layouts are only valid for radial menus */
	if (layout->root->type != UI_LAYOUT_PIEMENU)
		return ui_item_local_sublayout(layout, layout, 0);

	/* only one radial wheel per root layout is allowed, so check and return that, if it exists */
	for (item = layout->root->layout->items.first; item; item = item->next) {
		litem = (uiLayout *)item;
		if (litem->item.type == ITEM_LAYOUT_RADIAL) {
			UI_block_layout_set_current(layout->root->block, litem);
			return litem;
		}
	}

	litem = MEM_callocN(sizeof(uiLayout), "uiLayoutRadial");
	ui_litem_init_from_parent(litem, layout, false);

	litem->item.type = ITEM_LAYOUT_RADIAL;

	UI_block_layout_set_current(layout->root->block, litem);

	return litem;
}


uiLayout *uiLayoutBox(uiLayout *layout)
{
	return (uiLayout *)ui_layout_box(layout, UI_BTYPE_ROUNDBOX);
}

/**
 * Check all buttons defined in this layout, and set any button flagged as UI_BUT_LIST_ITEM as active/selected.
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
			UI_but_flag_enable(bitem->but, UI_SELECT);
		}
	}
}

uiLayout *uiLayoutListBox(
        uiLayout *layout, uiList *ui_list, PointerRNA *ptr, PropertyRNA *prop, PointerRNA *actptr,
        PropertyRNA *actprop)
{
	uiLayoutItemBx *box = ui_layout_box(layout, UI_BTYPE_LISTBOX);
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

uiLayout *uiLayoutAbsolute(uiLayout *layout, bool align)
{
	uiLayout *litem;

	litem = MEM_callocN(sizeof(uiLayout), "uiLayoutAbsolute");
	ui_litem_init_from_parent(litem, layout, align);

	litem->item.type = ITEM_LAYOUT_ABSOLUTE;

	UI_block_layout_set_current(layout->root->block, litem);

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
	ui_litem_init_from_parent(litem, layout, false);

	litem->item.type = ITEM_LAYOUT_OVERLAP;

	UI_block_layout_set_current(layout->root->block, litem);

	return litem;
}

uiLayout *uiLayoutSplit(uiLayout *layout, float percentage, bool align)
{
	uiLayoutItemSplit *split;

	split = MEM_callocN(sizeof(uiLayoutItemSplit), "uiLayoutItemSplit");
	ui_litem_init_from_parent(&split->litem, layout, align);

	split->litem.item.type = ITEM_LAYOUT_SPLIT;
	split->litem.space = layout->root->style->columnspace;
	split->percentage = percentage;

	UI_block_layout_set_current(layout->root->block, &split->litem);

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

void uiLayoutSetEmboss(uiLayout *layout, char emboss)
{
	layout->emboss = emboss;
}

bool uiLayoutGetPropSep(uiLayout *layout)
{
	return (layout->item.flag & UI_ITEM_PROP_SEP) != 0;
}

void uiLayoutSetPropSep(uiLayout *layout, bool is_sep)
{
	SET_FLAG_FROM_TEST(layout->item.flag, is_sep, UI_ITEM_PROP_SEP);
}

bool uiLayoutGetPropDecorate(uiLayout *layout)
{
	return (layout->item.flag & UI_ITEM_PROP_DECORATE) != 0;
}

void uiLayoutSetPropDecorate(uiLayout *layout, bool is_sep)
{
	SET_FLAG_FROM_TEST(layout->item.flag, is_sep, UI_ITEM_PROP_DECORATE);
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

int uiLayoutGetEmboss(uiLayout *layout)
{
	if (layout->emboss == UI_EMBOSS_UNDEFINED) {
		return layout->root->block->dt;
	}
	else {
		return layout->emboss;
	}
}

/********************** Layout *******************/

static void ui_item_scale(uiLayout *litem, const float scale[2])
{
	uiItem *item;
	int x, y, w, h;

	for (item = litem->items.last; item; item = item->prev) {
		if (item->type != ITEM_BUTTON) {
			uiLayout *subitem = (uiLayout *)item;
			ui_item_scale(subitem, scale);
		}

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

		if (BLI_listbase_is_empty(&litem->items)) {
			litem->w = 0;
			litem->h = 0;
			return;
		}

		if (litem->scale[0] != 0.0f || litem->scale[1] != 0.0f)
			ui_item_scale(litem, litem->scale);

		switch (litem->item.type) {
			case ITEM_LAYOUT_COLUMN:
				ui_litem_estimate_column(litem, false);
				break;
			case ITEM_LAYOUT_COLUMN_FLOW:
				ui_litem_estimate_column_flow(litem);
				break;
			case ITEM_LAYOUT_GRID_FLOW:
				ui_litem_estimate_grid_flow(litem);
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
#ifndef USE_UIBUT_SPATIAL_ALIGN
			if (ui_but_can_align(bitem->but))
#endif
			{
				if (!bitem->but->alignnr) {
					bitem->but->alignnr = nr;
				}
			}
		}
		else if (item->type == ITEM_LAYOUT_ABSOLUTE) {
			/* pass */
		}
		else if (item->type == ITEM_LAYOUT_OVERLAP) {
			/* pass */
		}
		else if (item->type == ITEM_LAYOUT_BOX) {
			box = (uiLayoutItemBx *)item;
			if (!box->roundbox->alignnr) {
				box->roundbox->alignnr = nr;
			}
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
				ui_litem_layout_column(litem, false);
				break;
			case ITEM_LAYOUT_COLUMN_FLOW:
				ui_litem_layout_column_flow(litem);
				break;
			case ITEM_LAYOUT_GRID_FLOW:
				ui_litem_layout_grid_flow(litem);
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
			case ITEM_LAYOUT_RADIAL:
				ui_litem_layout_radial(litem);
				break;
			default:
				break;
		}

		for (subitem = litem->items.first; subitem; subitem = subitem->next) {
			if (item->flag & UI_ITEM_BOX_ITEM) {
				subitem->flag |= UI_ITEM_BOX_ITEM;
			}
			ui_item_layout(subitem);
		}
	}
	else {
		if (item->flag & UI_ITEM_BOX_ITEM) {
			uiButtonItem *bitem = (uiButtonItem *)item;
			bitem->but->drawflag |= UI_BUT_BOX_ITEM;
		}
	}
}

static void ui_layout_end(uiBlock *block, uiLayout *layout, int *x, int *y)
{
	if (layout->root->handlefunc)
		UI_block_func_handle_set(block, layout->root->handlefunc, layout->root->argv);

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
		uiDefBut(block, UI_BTYPE_SEPR, 0, "", 0, 0, root->padding, root->padding, NULL, 0.0, 0.0, 0, 0, "");
		block->curlayout = prev_layout;
	}
}

uiLayout *UI_block_layout(uiBlock *block, int dir, int type, int x, int y, int size, int em, int padding, uiStyle *style)
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

	/* Only used when 'UI_ITEM_PROP_SEP' is set. */
	layout->item.flag = UI_ITEM_PROP_DECORATE;

	layout->x = x;
	layout->y = y;
	layout->root = root;
	layout->space = style->templatespace;
	layout->active = 1;
	layout->enabled = 1;
	layout->context = NULL;
	layout->emboss = UI_EMBOSS_UNDEFINED;

	if (type == UI_LAYOUT_MENU || type == UI_LAYOUT_PIEMENU)
		layout->space = 0;

	if (type == UI_LAYOUT_TOOLBAR) {
		block->flag |= UI_BLOCK_SHOW_SHORTCUT_ALWAYS;
	}

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


void UI_block_layout_set_current(uiBlock *block, uiLayout *layout)
{
	block->curlayout = layout;
}

void ui_layout_add_but(uiLayout *layout, uiBut *but)
{
	uiButtonItem *bitem;

	bitem = MEM_callocN(sizeof(uiButtonItem), "uiButtonItem");
	bitem->item.type = ITEM_BUTTON;
	bitem->but = but;

	int w, h;
	ui_item_size((uiItem *)bitem, &w, &h);
	/* XXX uiBut hasn't scaled yet
	 * we can flag the button as not expandable, depending on its size */
	if (w <= 2 * UI_UNIT_X && (!but->str || but->str[0] == '\0')) {
		bitem->item.flag |= UI_ITEM_MIN;
	}

	BLI_addtail(&layout->items, bitem);

	if (layout->context) {
		but->context = layout->context;
		but->context->used = true;
	}

	if (layout->emboss != UI_EMBOSS_UNDEFINED) {
		but->dt = layout->emboss;
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

void UI_block_layout_resolve(uiBlock *block, int *x, int *y)
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

void uiLayoutSetContextFromBut(uiLayout *layout, uiBut *but)
{
	if (but->opptr) {
		uiLayoutSetContextPointer(layout, "button_operator", but->opptr);
	}

	if (but->rnapoin.data && but->rnaprop) {
		/* TODO: index could be supported as well */
		PointerRNA ptr_prop;
		RNA_pointer_create(NULL, &RNA_Property, but->rnaprop, &ptr_prop);
		uiLayoutSetContextPointer(layout, "button_prop", &ptr_prop);
		uiLayoutSetContextPointer(layout, "button_pointer", &but->rnapoin);
	}
}

/* this is a bit of a hack but best keep it in one place at least */
MenuType *UI_but_menutype_get(uiBut *but)
{
	if (but->menu_create_func == ui_item_menutype_func) {
		return (MenuType *)but->poin;
	}
	else {
		return NULL;
	}
}

/* this is a bit of a hack but best keep it in one place at least */
PanelType *UI_but_paneltype_get(uiBut *but)
{
	if (but->menu_create_func == ui_item_paneltype_func) {
		return (PanelType *)but->poin;
	}
	else {
		return NULL;
	}
}


void UI_menutype_draw(bContext *C, MenuType *mt, struct uiLayout *layout)
{
	Menu menu = {
		.layout = layout,
		.type = mt,
	};

	if (G.debug & G_DEBUG_WM) {
		printf("%s: opening menu \"%s\"\n", __func__, mt->idname);
	}

	if (layout->context) {
		CTX_store_set(C, layout->context);
	}

	mt->draw(C, &menu);

	if (layout->context) {
		CTX_store_set(C, NULL);
	}
}


static void ui_paneltype_draw_impl(
        bContext *C, PanelType *pt, uiLayout *layout, bool show_header)
{
	Panel *panel = MEM_callocN(sizeof(Panel), "popover panel");
	panel->type = pt;
	panel->flag = PNL_POPOVER;

	uiLayout *last_item = layout->items.last;

	/* Draw main panel. */
	if (show_header) {
		uiLayout *row = uiLayoutRow(layout, false);
		if (pt->draw_header) {
			panel->layout = row;
			pt->draw_header(C, panel);
			panel->layout = NULL;
		}
		uiItemL(row, pt->label, ICON_NONE);
	}

	panel->layout = layout;
	pt->draw(C, panel);
	panel->layout = NULL;

	MEM_freeN(panel);

	/* Draw child panels. */
	for (LinkData *link = pt->children.first; link; link = link->next) {
		PanelType *child_pt = link->data;

		if (child_pt->poll == NULL || child_pt->poll(C, child_pt)) {
			/* Add space if something was added to the layout. */
			if (last_item != layout->items.last) {
				uiItemS(layout);
				last_item = layout->items.last;
			}

			uiLayout *col = uiLayoutColumn(layout, false);
			ui_paneltype_draw_impl(C, child_pt, col, true);
		}
	}
}

/**
 * Used for popup panels only.
 */
void UI_paneltype_draw(bContext *C, PanelType *pt, uiLayout *layout)
{
	if (layout->context) {
		CTX_store_set(C, layout->context);
	}

	ui_paneltype_draw_impl(C, pt, layout, false);

	if (layout->context) {
		CTX_store_set(C, NULL);
	}

}
