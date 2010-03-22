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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Foundation 2009.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_library.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_util.h"
#include "ED_types.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.h"

/************************ Structs and Defines *************************/

#define RNA_NO_INDEX	-1
#define RNA_ENUM_VALUE	-2

#define EM_SEPR_X		6
#define EM_SEPR_Y		6

/* uiLayoutRoot */

typedef struct uiLayoutRoot {
	struct uiLayoutRoot *next, *prev;

	int type;
	int opcontext;

	int emw, emh;

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
	char align;
	char active;
	char enabled;
	char redalert;
	char keepaspect;
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

typedef struct uiLayoutItemSplt {
	uiLayout litem;
	float percentage;
} uiLayoutItemSplt;

typedef struct uiLayoutItemRoot {
	uiLayout litem;
} uiLayoutItemRoot;

/************************** Item ***************************/

static char *ui_item_name_add_colon(char *name, char namestr[UI_MAX_NAME_STR])
{
	int len= strlen(name);

	if(len != 0 && len+1 < UI_MAX_NAME_STR) {
		BLI_strncpy(namestr, name, UI_MAX_NAME_STR);
		namestr[len]= ':';
		namestr[len+1]= '\0';
		return namestr;
	}

	return name;
}

static int ui_item_fit(int item, int pos, int all, int available, int last, int alignment, int *offset)
{
	/* available == 0 is unlimited */
	if(available == 0)
		return item;
	
	if(offset)
		*offset= 0;
	
	if(all > available) {
		/* contents is bigger than available space */
		if(last)
			return available-pos;
		else
			return (item*available)/all;
	}
	else {
		/* contents is smaller or equal to available space */
		if(alignment == UI_LAYOUT_ALIGN_EXPAND) {
			if(last)
				return available-pos;
			else
				return (item*available)/all;
		}
		else
			return item;
	}
}

/* variable button size in which direction? */
#define UI_ITEM_VARY_X	1
#define UI_ITEM_VARY_Y	2

static int ui_layout_vary_direction(uiLayout *layout)
{
	return (layout->root->type == UI_LAYOUT_HEADER || layout->alignment != UI_LAYOUT_ALIGN_EXPAND)? UI_ITEM_VARY_X: UI_ITEM_VARY_Y;
}

/* estimated size of text + icon */
static int ui_text_icon_width(uiLayout *layout, char *name, int icon, int compact)
{
	int variable = ui_layout_vary_direction(layout) == UI_ITEM_VARY_X;

	if(icon && !name[0])
		return UI_UNIT_X; /* icon only */
	else if(icon)
		return (variable)? UI_GetStringWidth(name) + (compact? 5: 10) + UI_UNIT_X: 10*UI_UNIT_X; /* icon + text */
	else
		return (variable)? UI_GetStringWidth(name) + (compact? 5: 10) + UI_UNIT_X: 10*UI_UNIT_X; /* text only */
}

static void ui_item_size(uiItem *item, int *r_w, int *r_h)
{
	if(item->type == ITEM_BUTTON) {
		uiButtonItem *bitem= (uiButtonItem*)item;

		if(r_w) *r_w= bitem->but->x2 - bitem->but->x1;
		if(r_h) *r_h= bitem->but->y2 - bitem->but->y1;
	}
	else {
		uiLayout *litem= (uiLayout*)item;

		if(r_w) *r_w= litem->w;
		if(r_h) *r_h= litem->h;
	}
}

static void ui_item_offset(uiItem *item, int *r_x, int *r_y)
{
	if(item->type == ITEM_BUTTON) {
		uiButtonItem *bitem= (uiButtonItem*)item;

		if(r_x) *r_x= bitem->but->x1;
		if(r_y) *r_y= bitem->but->y1;
	}
	else {
		if(r_x) *r_x= 0;
		if(r_y) *r_y= 0;
	}
}

static void ui_item_position(uiItem *item, int x, int y, int w, int h)
{
	if(item->type == ITEM_BUTTON) {
		uiButtonItem *bitem= (uiButtonItem*)item;

		bitem->but->x1= x;
		bitem->but->y1= y;
		bitem->but->x2= x+w;
		bitem->but->y2= y+h;
		
		ui_check_but(bitem->but); /* for strlen */
	}
	else {
		uiLayout *litem= (uiLayout*)item;

		litem->x= x;
		litem->y= y+h;
		litem->w= w;
		litem->h= h;
	}
}

/******************** Special RNA Items *********************/

static int ui_layout_local_dir(uiLayout *layout)
{
	switch(layout->item.type) {
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

	if(ui_layout_local_dir(test) == UI_LAYOUT_HORIZONTAL)
		sub= uiLayoutRow(layout, align);
	else
		sub= uiLayoutColumn(layout, align);
	
	sub->space= 0;
	return sub;
}

static void ui_layer_but_cb(bContext *C, void *arg_but, void *arg_index)
{
	wmWindow *win= CTX_wm_window(C);
	uiBut *but= arg_but, *cbut;
	PointerRNA *ptr= &but->rnapoin;
	PropertyRNA *prop= but->rnaprop;
	int i, index= GET_INT_FROM_POINTER(arg_index);
	int shift= win->eventstate->shift;
	int len= RNA_property_array_length(ptr, prop);

	if(!shift) {
		RNA_property_boolean_set_index(ptr, prop, index, 1);

		for(i=0; i<len; i++)
			if(i != index)
				RNA_property_boolean_set_index(ptr, prop, i, 0);

		RNA_property_update(C, ptr, prop);

		for(cbut=but->block->buttons.first; cbut; cbut=cbut->next)
			ui_check_but(cbut);
	}
}

/* create buttons for an item with an RNA array */
static void ui_item_array(uiLayout *layout, uiBlock *block, char *name, int icon, PointerRNA *ptr, PropertyRNA *prop, int len, int x, int y, int w, int h, int expand, int slider, int toggle, int icon_only)
{
	uiStyle *style= layout->root->style;
	uiBut *but;
	PropertyType type;
	PropertySubType subtype;
	uiLayout *sub;
	int a, b;

	/* retrieve type and subtype */
	type= RNA_property_type(prop);
	subtype= RNA_property_subtype(prop);

	sub= ui_item_local_sublayout(layout, layout, 1);
	uiBlockSetCurLayout(block, sub);

	/* create label */
	if(strcmp(name, "") != 0)
		uiDefBut(block, LABEL, 0, name, 0, 0, w, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");

	/* create buttons */
	if(type == PROP_BOOLEAN && ELEM(subtype, PROP_LAYER, PROP_LAYER_MEMBER)) {
		/* special check for layer layout */
		int butw, buth, unit;
		int cols= (len >= 20)? 2: 1;
		int colbuts= len/(2*cols);

		uiBlockSetCurLayout(block, uiLayoutAbsolute(layout, 0));

		unit= UI_UNIT_X*0.75;
		butw= unit;
		buth= unit;

		for(b=0; b<cols; b++) {
			uiBlockBeginAlign(block);

			for(a=0; a<colbuts; a++) {
				but= uiDefAutoButR(block, ptr, prop, a+b*colbuts, "", ICON_BLANK1, x + butw*a, y+buth, butw, buth);
				if(subtype == PROP_LAYER_MEMBER)
					uiButSetFunc(but, ui_layer_but_cb, but, SET_INT_IN_POINTER(a+b*colbuts));
			}
			for(a=0; a<colbuts; a++) {
				but= uiDefAutoButR(block, ptr, prop, a+len/2+b*colbuts, "", ICON_BLANK1, x + butw*a, y, butw, buth);
				if(subtype == PROP_LAYER_MEMBER)
					uiButSetFunc(but, ui_layer_but_cb, but, SET_INT_IN_POINTER(a+len/2+b*colbuts));
			}
			uiBlockEndAlign(block);

			x += colbuts*butw + style->buttonspacex;
		}
	}
	else if(subtype == PROP_MATRIX) {
		int totdim, dim_size[3];	/* 3 == RNA_MAX_ARRAY_DIMENSION */
		int row, col;

		uiBlockSetCurLayout(block, uiLayoutAbsolute(layout, 1));

		totdim= RNA_property_array_dimension(ptr, prop, dim_size);
		if (totdim != 2) return;	/* only 2D matrices supported in UI so far */
		
		w /= dim_size[0];
		h /= dim_size[1];

		for(a=0; a<len; a++) {
			col= a % dim_size[0];
			row= a / dim_size[0];
			
			but= uiDefAutoButR(block, ptr, prop, a, "", 0, x + w*col, y+(dim_size[1]*UI_UNIT_Y)-(row*UI_UNIT_Y), w, UI_UNIT_Y);
			if(slider && but->type==NUM)
				but->type= NUMSLI;
		}
	}
	else {
		if(ELEM(subtype, PROP_COLOR, PROP_COLOR_GAMMA) && !expand)
			uiDefAutoButR(block, ptr, prop, -1, "", 0, 0, 0, w, UI_UNIT_Y);

		if(!ELEM(subtype, PROP_COLOR, PROP_COLOR_GAMMA) || expand) {
			/* layout for known array subtypes */
			char str[3];

			for(a=0; a<len; a++) {
				str[0]= RNA_property_array_item_char(prop, a);

				if(str[0]) {
					if (icon_only) {
						str[0] = '\0';
					}
					else if(type == PROP_BOOLEAN) {
						str[1]= '\0';
					}
					else {
						str[1]= ':';
						str[2]= '\0';
					}
				}

				but= uiDefAutoButR(block, ptr, prop, a, str, icon, 0, 0, w, UI_UNIT_Y);
				if(slider && but->type==NUM)
					but->type= NUMSLI;
				if(toggle && but->type==OPTION)
					but->type= TOG;
			}
		}
		else if(ELEM(subtype, PROP_COLOR, PROP_COLOR_GAMMA) && len == 4) {
			but= uiDefAutoButR(block, ptr, prop, 3, "A:", 0, 0, 0, w, UI_UNIT_Y);
			if(slider && but->type==NUM)
				but->type= NUMSLI;
		}
	}

	uiBlockSetCurLayout(block, layout);
}

static void ui_item_enum_row(uiLayout *layout, uiBlock *block, PointerRNA *ptr, PropertyRNA *prop, char *uiname, int x, int y, int w, int h, int icon_only)
{
	EnumPropertyItem *item;
	const char *identifier;
	char *name;
	int a, totitem, itemw, icon, value, free;

	identifier= RNA_property_identifier(prop);
	RNA_property_enum_items(block->evil_C, ptr, prop, &item, &totitem, &free);

	uiBlockSetCurLayout(block, ui_item_local_sublayout(layout, layout, 1));
	for(a=0; a<totitem; a++) {
		if(!item[a].identifier[0])
			continue;

		name= (!uiname || uiname[0])? (char*)item[a].name: "";
		icon= item[a].icon;
		value= item[a].value;
		itemw= ui_text_icon_width(block->curlayout, name, icon, 0);

		if(icon && strcmp(name, "") != 0 && !icon_only)
			uiDefIconTextButR(block, ROW, 0, icon, name, 0, 0, itemw, h, ptr, identifier, -1, 0, value, -1, -1, NULL);
		else if(icon)
			uiDefIconButR(block, ROW, 0, icon, 0, 0, itemw, h, ptr, identifier, -1, 0, value, -1, -1, NULL);
		else
			uiDefButR(block, ROW, 0, name, 0, 0, itemw, h, ptr, identifier, -1, 0, value, -1, -1, NULL);
	}
	uiBlockSetCurLayout(block, layout);

	if(free)
		MEM_freeN(item);
}

/* callback for keymap item change button */
static void ui_keymap_but_cb(bContext *C, void *but_v, void *key_v)
{
	uiBut *but= but_v;

	RNA_boolean_set(&but->rnapoin, "shift", (but->modifier_key & KM_SHIFT) != 0);
	RNA_boolean_set(&but->rnapoin, "ctrl", (but->modifier_key & KM_CTRL) != 0);
	RNA_boolean_set(&but->rnapoin, "alt", (but->modifier_key & KM_ALT) != 0);
	RNA_boolean_set(&but->rnapoin, "oskey", (but->modifier_key & KM_OSKEY) != 0);
}

/* create label + button for RNA property */
static uiBut *ui_item_with_label(uiLayout *layout, uiBlock *block, char *name, int icon, PointerRNA *ptr, PropertyRNA *prop, int index, int x, int y, int w, int h, int flag)
{
	uiLayout *sub;
	uiBut *but=NULL;
	PropertyType type;
	PropertySubType subtype;
	int labelw;

	sub= uiLayoutRow(layout, 0);
	uiBlockSetCurLayout(block, sub);

	if(strcmp(name, "") != 0) {
		/* XXX UI_GetStringWidth is not accurate
		labelw= UI_GetStringWidth(name);
		CLAMP(labelw, w/4, 3*w/4);*/
		labelw= w/3;
		uiDefBut(block, LABEL, 0, name, x, y, labelw, h, NULL, 0.0, 0.0, 0, 0, "");
		w= w-labelw;
	}

	type= RNA_property_type(prop);
	subtype= RNA_property_subtype(prop);

	if(subtype == PROP_FILEPATH || subtype == PROP_DIRPATH) {
		uiBlockSetCurLayout(block, uiLayoutRow(sub, 1));
		uiDefAutoButR(block, ptr, prop, index, "", icon, x, y, w-UI_UNIT_X, h);

		/* BUTTONS_OT_file_browse calls uiFileBrowseContextProperty */
		but= uiDefIconButO(block, BUT, "BUTTONS_OT_file_browse", WM_OP_INVOKE_DEFAULT, ICON_FILESEL, x, y, UI_UNIT_X, h, "Browse for file or directory");
	}
	else if(subtype == PROP_DIRECTION) {
		uiDefButR(block, BUT_NORMAL, 0, name, x, y, 100, 100, ptr, RNA_property_identifier(prop), index, 0, 0, -1, -1, NULL);
	}
	else if(flag & UI_ITEM_R_EVENT) {
		uiDefButR(block, KEYEVT, 0, name, x, y, w, h, ptr, RNA_property_identifier(prop), index, 0, 0, -1, -1, NULL);
	}
	else if(flag & UI_ITEM_R_FULL_EVENT) {
		if(RNA_struct_is_a(ptr->type, &RNA_KeyMapItem)) {
			char buf[128];

			WM_keymap_item_to_string(ptr->data, buf, sizeof(buf));

			but= uiDefButR(block, HOTKEYEVT, 0, buf, x, y, w, h, ptr, RNA_property_identifier(prop), 0, 0, 0, -1, -1, NULL);
			uiButSetFunc(but, ui_keymap_but_cb, but, NULL);
			if (flag & UI_ITEM_R_IMMEDIATE)
				uiButSetFlag(but, UI_BUT_IMMEDIATE);
		}
	}
	else
		but= uiDefAutoButR(block, ptr, prop, index, (type == PROP_ENUM && !(flag & UI_ITEM_R_ICON_ONLY))? NULL: "", icon, x, y, w, h);

	uiBlockSetCurLayout(block, layout);
	return but;
}

void uiFileBrowseContextProperty(const bContext *C, PointerRNA *ptr, PropertyRNA **prop)
{
	ARegion *ar= CTX_wm_region(C);
	uiBlock *block;
	uiBut *but, *prevbut;

	memset(ptr, 0, sizeof(*ptr));
	*prop= NULL;

	if(!ar)
		return;

	for(block=ar->uiblocks.first; block; block=block->next) {
		for(but=block->buttons.first; but; but= but->next) {
			prevbut= but->prev;

			/* find the button before the active one */
			if((but->flag & UI_BUT_LAST_ACTIVE) && prevbut && prevbut->rnapoin.data) {
				if(RNA_property_type(prevbut->rnaprop) == PROP_STRING) {
					*ptr= prevbut->rnapoin;
					*prop= prevbut->rnaprop;
					return;
				}
			}
		}
	}
}

/********************* Button Items *************************/

/* disabled item */
static void ui_item_disabled(uiLayout *layout, char *name)
{
	uiBlock *block= layout->root->block;
	uiBut *but;
	int w;

	uiBlockSetCurLayout(block, layout);

	if(!name)
		name= "";

	w= ui_text_icon_width(layout, name, 0, 0);

	but= uiDefBut(block, LABEL, 0, (char*)name, 0, 0, w, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
	but->flag |= UI_BUT_DISABLED;
	but->lock = 1;
	but->lockstr = "";
}

/* operator items */
PointerRNA uiItemFullO(uiLayout *layout, char *name, int icon, char *idname, IDProperty *properties, int context, int flag)
{
	uiBlock *block= layout->root->block;
	wmOperatorType *ot= WM_operatortype_find(idname, 0);
	uiBut *but;
	int w;

	if(!ot) {
		ui_item_disabled(layout, idname);
		return PointerRNA_NULL;
	}

	if(!name)
		name= ot->name;
	if(layout->root->type == UI_LAYOUT_MENU && !icon)
		icon= ICON_BLANK1;

	/* create button */
	uiBlockSetCurLayout(block, layout);

	w= ui_text_icon_width(layout, name, icon, 0);

	if(icon && strcmp(name, "") != 0)
		but= uiDefIconTextButO(block, BUT, ot->idname, context, icon, (char*)name, 0, 0, w, UI_UNIT_Y, NULL);
	else if(icon)
		but= uiDefIconButO(block, BUT, ot->idname, context, icon, 0, 0, w, UI_UNIT_Y, NULL);
	else
		but= uiDefButO(block, BUT, ot->idname, context, (char*)name, 0, 0, w, UI_UNIT_Y, NULL);
	
	/* text alignment for toolbar buttons */
	if((layout->root->type == UI_LAYOUT_TOOLBAR) && !icon)
		but->flag |= UI_TEXT_LEFT;
	
	/* assign properties */
	if(properties || (flag & UI_ITEM_O_RETURN_PROPS)) {
		PointerRNA *opptr= uiButGetOperatorPtrRNA(but);

		if(properties) {
			opptr->data= properties;
		}
		else {
			IDPropertyTemplate val = {0};
			opptr->data= IDP_New(IDP_GROUP, val, "wmOperatorProperties");
		}

		return *opptr;
	}

	return PointerRNA_NULL;
}

static char *ui_menu_enumpropname(uiLayout *layout, char *opname, char *propname, int retval)
{
	wmOperatorType *ot= WM_operatortype_find(opname, 0);
	PointerRNA ptr;
	PropertyRNA *prop;

	if(!ot || !ot->srna)
		return "";

	RNA_pointer_create(NULL, ot->srna, NULL, &ptr);
	prop= RNA_struct_find_property(&ptr, propname);

	if(prop) {
		EnumPropertyItem *item;
		int totitem, free;
		const char *name;

		RNA_property_enum_items(layout->root->block->evil_C, &ptr, prop, &item, &totitem, &free);
		if(RNA_enum_name(item, retval, &name)) {
			if(free) MEM_freeN(item);
			return (char*)name;
		}
		
		if(free)
			MEM_freeN(item);
	}

	return "";
}

void uiItemEnumO(uiLayout *layout, char *name, int icon, char *opname, char *propname, int value)
{
	PointerRNA ptr;

	WM_operator_properties_create(&ptr, opname);
	RNA_enum_set(&ptr, propname, value);

	if(!name)
		name= ui_menu_enumpropname(layout, opname, propname, value);

	uiItemFullO(layout, name, icon, opname, ptr.data, layout->root->opcontext, 0);
}

void uiItemsFullEnumO(uiLayout *layout, char *opname, char *propname, IDProperty *properties, int context, int flag)
{
	wmOperatorType *ot= WM_operatortype_find(opname, 0);
	PointerRNA ptr;
	PropertyRNA *prop;
	uiBut *bt;
	uiBlock *block= layout->root->block;

	if(!ot || !ot->srna) {
		ui_item_disabled(layout, opname);
		return;
	}

	RNA_pointer_create(NULL, ot->srna, NULL, &ptr);
	prop= RNA_struct_find_property(&ptr, propname);

	if(prop && RNA_property_type(prop) == PROP_ENUM) {
		EnumPropertyItem *item;
		int totitem, i, free;
		uiLayout *split= uiLayoutSplit(layout, 0, 0);
		uiLayout *column= uiLayoutColumn(split, 0);

		RNA_property_enum_items(block->evil_C, &ptr, prop, &item, &totitem, &free);

		for(i=0; i<totitem; i++) {
			if(item[i].identifier[0]) {
				if(properties) {
					PointerRNA ptr;

					WM_operator_properties_create(&ptr, opname);
					if(ptr.data) {
						IDP_FreeProperty(ptr.data);
						MEM_freeN(ptr.data);
					}
					ptr.data= IDP_CopyProperty(properties);
					RNA_enum_set(&ptr, propname, item[i].value);

					uiItemFullO(column, (char*)item[i].name, item[i].icon, opname, ptr.data, context, flag);
				}
				else
					uiItemEnumO(column, (char*)item[i].name, item[i].icon, opname, propname, item[i].value);
			}
			else {
				if(item[i].name) {
					if(i != 0) {
						column= uiLayoutColumn(split, 0);
						/* inconsistent, but menus with labels do not look good flipped */
						block->flag |= UI_BLOCK_NO_FLIP;
					}

					uiItemL(column, (char*)item[i].name, 0);
					bt= block->buttons.last;
					bt->flag= UI_TEXT_LEFT;
				}
				else
					uiItemS(column);
			}
		}

		if(free)
			MEM_freeN(item);
	}
}

void uiItemsEnumO(uiLayout *layout, char *opname, char *propname)
{
	uiItemsFullEnumO(layout, opname, propname, NULL, layout->root->opcontext, 0);
}

/* for use in cases where we have */
void uiItemEnumO_string(uiLayout *layout, char *name, int icon, char *opname, char *propname, char *value_str)
{
	PointerRNA ptr;
	
	/* for getting the enum */
	PropertyRNA *prop;
	EnumPropertyItem *item;
	int value, free;

	WM_operator_properties_create(&ptr, opname);
	
	/* enum lookup */
	if((prop= RNA_struct_find_property(&ptr, propname))) {
		RNA_property_enum_items(layout->root->block->evil_C, &ptr, prop, &item, NULL, &free);
		if(item==NULL || RNA_enum_value_from_id(item, value_str, &value)==0) {
			if(free) MEM_freeN(item);
			printf("uiItemEnumO_string: %s.%s, enum %s not found.\n", RNA_struct_identifier(ptr.type), propname, value_str);
			return;
		}

		if(free)
			MEM_freeN(item);
	}
	else {
		printf("uiItemEnumO_string: %s.%s not found.\n", RNA_struct_identifier(ptr.type), propname);
		return;
	}
	
	RNA_property_enum_set(&ptr, prop, value);
	
	/* same as uiItemEnumO */
	if(!name)
		name= ui_menu_enumpropname(layout, opname, propname, value);

	uiItemFullO(layout, name, icon, opname, ptr.data, layout->root->opcontext, 0);
}

void uiItemBooleanO(uiLayout *layout, char *name, int icon, char *opname, char *propname, int value)
{
	PointerRNA ptr;

	WM_operator_properties_create(&ptr, opname);
	RNA_boolean_set(&ptr, propname, value);

	uiItemFullO(layout, name, icon, opname, ptr.data, layout->root->opcontext, 0);
}

void uiItemIntO(uiLayout *layout, char *name, int icon, char *opname, char *propname, int value)
{
	PointerRNA ptr;

	WM_operator_properties_create(&ptr, opname);
	RNA_int_set(&ptr, propname, value);

	uiItemFullO(layout, name, icon, opname, ptr.data, layout->root->opcontext, 0);
}

void uiItemFloatO(uiLayout *layout, char *name, int icon, char *opname, char *propname, float value)
{
	PointerRNA ptr;

	WM_operator_properties_create(&ptr, opname);
	RNA_float_set(&ptr, propname, value);

	uiItemFullO(layout, name, icon, opname, ptr.data, layout->root->opcontext, 0);
}

void uiItemStringO(uiLayout *layout, char *name, int icon, char *opname, char *propname, char *value)
{
	PointerRNA ptr;

	WM_operator_properties_create(&ptr, opname);
	RNA_string_set(&ptr, propname, value);

	uiItemFullO(layout, name, icon, opname, ptr.data, layout->root->opcontext, 0);
}

void uiItemO(uiLayout *layout, char *name, int icon, char *opname)
{
	uiItemFullO(layout, name, icon, opname, NULL, layout->root->opcontext, 0);
}

/* RNA property items */

static void ui_item_rna_size(uiLayout *layout, char *name, int icon, PointerRNA *ptr, PropertyRNA *prop, int index, int icon_only, int *r_w, int *r_h)
{
	PropertyType type;
	PropertySubType subtype;
	int len, w, h;

	/* arbitrary extended width by type */
	type= RNA_property_type(prop);
	subtype= RNA_property_subtype(prop);
	len= RNA_property_array_length(ptr, prop);

	if(ELEM3(type, PROP_STRING, PROP_POINTER, PROP_ENUM) && !name[0] && !icon_only)
		name= "non-empty text";
	else if(type == PROP_BOOLEAN && !name[0] && !icon_only)
		icon= ICON_DOT;

	w= ui_text_icon_width(layout, name, icon, 0);
	h= UI_UNIT_Y;

	/* increase height for arrays */
	if(index == RNA_NO_INDEX && len > 0) {
		if(!name[0] && icon == 0)
			h= 0;

		if(ELEM(subtype, PROP_LAYER, PROP_LAYER_MEMBER))
			h += 2*UI_UNIT_Y;
		else if(subtype == PROP_MATRIX)
			h += ceil(sqrt(len))*UI_UNIT_Y;
		else
			h += len*UI_UNIT_Y;
	}
	else if(ui_layout_vary_direction(layout) == UI_ITEM_VARY_X) {
		if(type == PROP_BOOLEAN && strcmp(name, "") != 0)
			w += UI_UNIT_X/5;
		else if(type == PROP_ENUM)
			w += UI_UNIT_X/4;
		else if(type == PROP_FLOAT || type == PROP_INT)
			w += UI_UNIT_X*3;
	}

	*r_w= w;
	*r_h= h;
}

void uiItemFullR(uiLayout *layout, char *name, int icon, PointerRNA *ptr, PropertyRNA *prop, int index, int value, int flag)
{
	uiBlock *block= layout->root->block;
	uiBut *but;
	PropertyType type;
	char namestr[UI_MAX_NAME_STR];
	int len, w, h, slider, toggle, expand, icon_only, no_bg;

	uiBlockSetCurLayout(block, layout);

	/* retrieve info */
	type= RNA_property_type(prop);
	len= RNA_property_array_length(ptr, prop);

	/* set name and icon */
	if(!name)
		name= (char*)RNA_property_ui_name(prop);
	if(!icon)
		icon= RNA_property_ui_icon(prop);
	
	if(ELEM4(type, PROP_INT, PROP_FLOAT, PROP_STRING, PROP_POINTER))
		name= ui_item_name_add_colon(name, namestr);
	else if(type == PROP_BOOLEAN && len && index == RNA_NO_INDEX)
		name= ui_item_name_add_colon(name, namestr);
	else if(type == PROP_ENUM && index != RNA_ENUM_VALUE)
		name= ui_item_name_add_colon(name, namestr);

	if(layout->root->type == UI_LAYOUT_MENU) {
		if(type == PROP_BOOLEAN)
			icon= (RNA_property_boolean_get(ptr, prop))? ICON_CHECKBOX_HLT: ICON_CHECKBOX_DEHLT;
		else if(type == PROP_ENUM && index == RNA_ENUM_VALUE)
			icon= (RNA_property_enum_get(ptr, prop) == value)? ICON_CHECKBOX_HLT: ICON_CHECKBOX_DEHLT;
	}

	slider= (flag & UI_ITEM_R_SLIDER);
	toggle= (flag & UI_ITEM_R_TOGGLE);
	expand= (flag & UI_ITEM_R_EXPAND);
	icon_only= (flag & UI_ITEM_R_ICON_ONLY);
	no_bg= (flag & UI_ITEM_R_NO_BG);

	/* get size */
	ui_item_rna_size(layout, name, icon, ptr, prop, index, icon_only, &w, &h);

	if (no_bg)
		uiBlockSetEmboss(block, UI_EMBOSSN);
	
	/* array property */
	if(index == RNA_NO_INDEX && len > 0)
		ui_item_array(layout, block, name, icon, ptr, prop, len, 0, 0, w, h, expand, slider, toggle, icon_only);
	/* enum item */
	else if(type == PROP_ENUM && index == RNA_ENUM_VALUE) {
		char *identifier= (char*)RNA_property_identifier(prop);

		if(icon && strcmp(name, "") != 0 && !icon_only)
			uiDefIconTextButR(block, ROW, 0, icon, name, 0, 0, w, h, ptr, identifier, -1, 0, value, -1, -1, NULL);
		else if(icon)
			uiDefIconButR(block, ROW, 0, icon, 0, 0, w, h, ptr, identifier, -1, 0, value, -1, -1, NULL);
		else
			uiDefButR(block, ROW, 0, name, 0, 0, w, h, ptr, identifier, -1, 0, value, -1, -1, NULL);
	}
	/* expanded enum */
	else if(type == PROP_ENUM && expand)
		ui_item_enum_row(layout, block, ptr, prop, name, 0, 0, w, h, icon_only);
	/* property with separate label */
	else if(type == PROP_ENUM || type == PROP_STRING || type == PROP_POINTER) {
		but= ui_item_with_label(layout, block, name, icon, ptr, prop, index, 0, 0, w, h, flag);
		ui_but_add_search(but, ptr, prop, NULL, NULL);
	}
	/* single button */
	else {
		but= uiDefAutoButR(block, ptr, prop, index, (char*)name, icon, 0, 0, w, h);

		if(slider && but->type==NUM)
			but->type= NUMSLI;

		if(toggle && but->type==OPTION)
			but->type= TOG;
	}
	
	if (no_bg)
		uiBlockSetEmboss(block, UI_EMBOSS);
}

void uiItemR(uiLayout *layout, char *name, int icon, PointerRNA *ptr, char *propname, int flag)
{
	PropertyRNA *prop= RNA_struct_find_property(ptr, propname);

	if(!prop) {
		ui_item_disabled(layout, propname);
		printf("uiItemR: property not found: %s\n", propname);
		return;
	}

	uiItemFullR(layout, name, icon, ptr, prop, RNA_NO_INDEX, 0, flag);
}

void uiItemEnumR(uiLayout *layout, char *name, int icon, struct PointerRNA *ptr, char *propname, int value)
{
	PropertyRNA *prop= RNA_struct_find_property(ptr, propname);

	if(!prop || RNA_property_type(prop) != PROP_ENUM) {
		ui_item_disabled(layout, propname);
		printf("uiItemEnumR: enum property not found: %s\n", propname);
		return;
	}

	uiItemFullR(layout, name, icon, ptr, prop, RNA_ENUM_VALUE, value, 0);
}

void uiItemEnumR_string(uiLayout *layout, char *name, int icon, struct PointerRNA *ptr, char *propname, char *value)
{
	PropertyRNA *prop= RNA_struct_find_property(ptr, propname);
	EnumPropertyItem *item;
	int ivalue, a, free;

	if(!prop || RNA_property_type(prop) != PROP_ENUM) {
		ui_item_disabled(layout, propname);
		printf("uiItemEnumR: enum property not found: %s\n", propname);
		return;
	}

	RNA_property_enum_items(layout->root->block->evil_C, ptr, prop, &item, NULL, &free);

	if(!RNA_enum_value_from_id(item, value, &ivalue)) {
		if(free) MEM_freeN(item);
		ui_item_disabled(layout, propname);
		printf("uiItemEnumR: enum property value not found: %s\n", value);
		return;
	}

	for(a=0; item[a].identifier; a++) {
		if(item[a].value == ivalue) {
			uiItemFullR(layout, (char*)item[a].name, item[a].icon, ptr, prop, RNA_ENUM_VALUE, ivalue, 0);
			break;
		}
	}

	if(free)
		MEM_freeN(item);
}

void uiItemsEnumR(uiLayout *layout, struct PointerRNA *ptr, char *propname)
{
	PropertyRNA *prop;
	uiBlock *block= layout->root->block;
	uiBut *bt;

	prop= RNA_struct_find_property(ptr, propname);

	if(!prop) {
		ui_item_disabled(layout, propname);
		return;
	}

	if(RNA_property_type(prop) == PROP_ENUM) {
		EnumPropertyItem *item;
		int totitem, i, free;
		uiLayout *split= uiLayoutSplit(layout, 0, 0);
		uiLayout *column= uiLayoutColumn(split, 0);

		RNA_property_enum_items(block->evil_C, ptr, prop, &item, &totitem, &free);

		for(i=0; i<totitem; i++) {
			if(item[i].identifier[0]) {
				uiItemEnumR(column, (char*)item[i].name, 0, ptr, propname, item[i].value);
			}
			else {
				if(item[i].name) {
					if(i != 0) {
						column= uiLayoutColumn(split, 0);
						/* inconsistent, but menus with labels do not look good flipped */
						block->flag |= UI_BLOCK_NO_FLIP;
					}

					uiItemL(column, (char*)item[i].name, 0);
					bt= block->buttons.last;
					bt->flag= UI_TEXT_LEFT;
				}
				else
					uiItemS(column);
			}
		}

		if(free)
			MEM_freeN(item);
	}
}

/* Pointer RNA button with search */

typedef struct CollItemSearch {
	struct CollItemSearch *next, *prev;
	char *name;
	int index;
	int iconid;
} CollItemSearch;

int sort_search_items_list(void *a, void *b)
{
	CollItemSearch *cis1 = (CollItemSearch *)a;
	CollItemSearch *cis2 = (CollItemSearch *)b;
	
	if (BLI_strcasecmp(cis1->name, cis2->name)>0)
		return 1;
	else
		return 0;
}

static void rna_search_cb(const struct bContext *C, void *arg_but, char *str, uiSearchItems *items)
{
	uiBut *but= arg_but;
	char *name;
	int i=0, iconid=0, flag= RNA_property_flag(but->rnaprop);
	ListBase *items_list= MEM_callocN(sizeof(ListBase), "items_list");
	CollItemSearch *cis;

	/* build a temporary list of relevant items first */
	RNA_PROP_BEGIN(&but->rnasearchpoin, itemptr, but->rnasearchprop) {
		if(flag & PROP_ID_SELF_CHECK)
			if(itemptr.data == but->rnapoin.id.data)
				continue;
		
		if(RNA_struct_is_ID(itemptr.type))
			iconid= ui_id_icon_get((bContext*)C, itemptr.data, 0);
		
		name= RNA_struct_name_get_alloc(&itemptr, NULL, 0);
		
		if(name) {
			if(BLI_strcasestr(name, str)) {
				cis = MEM_callocN(sizeof(CollItemSearch), "CollectionItemSearch");
				cis->name = MEM_dupallocN(name);
				cis->index = i;
				cis->iconid = iconid;
				BLI_addtail(items_list, cis);
			}
		}
		MEM_freeN(name);
		
		i++;
	}
	RNA_PROP_END;
	
	BLI_sortlist(items_list, sort_search_items_list);
	
	/* add search items from temporary list */
	for (cis=items_list->first; cis; cis=cis->next) {
		if (!uiSearchItemAdd(items, cis->name, SET_INT_IN_POINTER(cis->index), cis->iconid)) {
			break;
		}
	}

	for (cis=items_list->first; cis; cis=cis->next) {
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

	*prop= NULL;

	RNA_STRUCT_BEGIN(ptr, iprop) {
		/* if it's a collection and has same pointer type, we've got it */
		if(RNA_property_type(iprop) == PROP_COLLECTION) {
			srna= RNA_property_pointer_type(ptr, iprop);

			if(ptype == srna) {
				*prop= iprop;
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
	if(!searchprop) {
		if(RNA_property_type(prop) == PROP_POINTER) {
			ptype= RNA_property_pointer_type(ptr, prop);
			search_id_collection(ptype, &sptr, &searchprop);
			searchptr= &sptr;
		}
	}

	/* turn button into search button */
	if(searchprop) {
		but->type= SEARCH_MENU;
		but->hardmax= MAX2(but->hardmax, 256);
		but->rnasearchpoin= *searchptr;
		but->rnasearchprop= searchprop;
		but->flag |= UI_ICON_LEFT|UI_TEXT_LEFT|UI_BUT_UNDO;

		uiButSetSearchFunc(but, rna_search_cb, but, NULL, NULL);
	}
}

void uiItemPointerR(uiLayout *layout, char *name, int icon, struct PointerRNA *ptr, char *propname, struct PointerRNA *searchptr, char *searchpropname)
{
	PropertyRNA *prop, *searchprop;
	PropertyType type;
	uiBut *but;
	uiBlock *block;
	StructRNA *icontype;
	int w, h;
	
	/* validate arguments */
	prop= RNA_struct_find_property(ptr, propname);

	if(!prop) {
		printf("uiItemPointerR: property not found: %s\n", propname);
		return;
	}
	
	type= RNA_property_type(prop);
	if(!ELEM(type, PROP_POINTER, PROP_STRING)) {
		printf("uiItemPointerR: property %s must be a pointer or string.\n", propname);
		return;
	}

	searchprop= RNA_struct_find_property(searchptr, searchpropname);

	if(!searchprop || RNA_property_type(searchprop) != PROP_COLLECTION) {
		printf("uiItemPointerR: search collection property not found: %s\n", searchpropname);
		return;
	}

	/* get icon & name */
	if(!icon) {
		if(type == PROP_POINTER)
			icontype= RNA_property_pointer_type(ptr, prop);
		else
			icontype= RNA_property_pointer_type(searchptr, searchprop);

		icon= RNA_struct_ui_icon(icontype);
	}
	if(!name)
		name= (char*)RNA_property_ui_name(prop);

	/* create button */
	block= uiLayoutGetBlock(layout);

	ui_item_rna_size(layout, name, icon, ptr, prop, 0, 0, &w, &h);
	but= ui_item_with_label(layout, block, name, icon, ptr, prop, 0, 0, 0, w, h, 0);

	ui_but_add_search(but, ptr, prop, searchptr, searchprop);
}

/* menu item */
static void ui_item_menutype_func(bContext *C, uiLayout *layout, void *arg_mt)
{
	MenuType *mt= (MenuType*)arg_mt;
	Menu menu = {0};

	menu.type= mt;
	menu.layout= layout;
	mt->draw(C, &menu);
}

static void ui_item_menu(uiLayout *layout, char *name, int icon, uiMenuCreateFunc func, void *arg, void *argN)
{
	uiBlock *block= layout->root->block;
	uiBut *but;
	int w, h;

	uiBlockSetCurLayout(block, layout);

	if(layout->root->type == UI_LAYOUT_HEADER)
		uiBlockSetEmboss(block, UI_EMBOSS);

	if(!name)
		name= "";
	if(layout->root->type == UI_LAYOUT_MENU && !icon)
		icon= ICON_BLANK1;

	w= ui_text_icon_width(layout, name, icon, 1);
	h= UI_UNIT_Y;

	if(layout->root->type == UI_LAYOUT_HEADER) /* ugly .. */
		w -= 10;

	if(name[0] && icon)
		but= uiDefIconTextMenuBut(block, func, arg, icon, (char*)name, 0, 0, w, h, "");
	else if(icon)
		but= uiDefIconMenuBut(block, func, arg, icon, 0, 0, w, h, "");
	else
		but= uiDefMenuBut(block, func, arg, (char*)name, 0, 0, w, h, "");

	if(argN) { /* ugly .. */
		but->poin= (char*)but;
		but->func_argN= argN;
	}

	if(layout->root->type == UI_LAYOUT_HEADER)
		uiBlockSetEmboss(block, UI_EMBOSS);
	else if(layout->root->type == UI_LAYOUT_PANEL)
		but->type= MENU;
}

void uiItemM(uiLayout *layout, bContext *C, char *name, int icon, char *menuname)
{
	MenuType *mt;

	mt= WM_menutype_find(menuname, FALSE);

	if(mt==NULL) {
		printf("uiItemM: not found %s\n", menuname);
		return;
	}

	if(!name)
		name= mt->label;
	if(layout->root->type == UI_LAYOUT_MENU && !icon)
		icon= ICON_BLANK1;

	ui_item_menu(layout, name, icon, ui_item_menutype_func, mt, NULL);
}

/* label item */
static uiBut *uiItemL_(uiLayout *layout, char *name, int icon)
{
	uiBlock *block= layout->root->block;
	uiBut *but;
	int w;

	uiBlockSetCurLayout(block, layout);

	if(!name)
		name= "";
	if(layout->root->type == UI_LAYOUT_MENU && !icon)
		icon= ICON_BLANK1;

	w= ui_text_icon_width(layout, name, icon, 0);

	if(icon && strcmp(name, "") != 0)
		but= uiDefIconTextBut(block, LABEL, 0, icon, (char*)name, 0, 0, w, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
	else if(icon)
		but= uiDefIconBut(block, LABEL, 0, icon, 0, 0, w, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
	else
		but= uiDefBut(block, LABEL, 0, (char*)name, 0, 0, w, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
	
	return but;
}

void uiItemL(uiLayout *layout, char *name, int icon)
{
	uiItemL_(layout, name, icon);
}

void uiItemLDrag(uiLayout *layout, PointerRNA *ptr, char *name, int icon)
{
	uiBut *but= uiItemL_(layout, name, icon);

	if(ptr && ptr->type)
		if(RNA_struct_is_ID(ptr->type))
			uiButSetDragID(but, ptr->id.data);
}


/* value item */
void uiItemV(uiLayout *layout, char *name, int icon, int argval)
{
	/* label */
	uiBlock *block= layout->root->block;
	float *retvalue= (block->handle)? &block->handle->retvalue: NULL;
	int w;

	uiBlockSetCurLayout(block, layout);

	if(!name)
		name= "";
	if(layout->root->type == UI_LAYOUT_MENU && !icon)
		icon= ICON_BLANK1;

	w= ui_text_icon_width(layout, name, icon, 0);

	if(icon && strcmp(name, "") != 0)
		uiDefIconTextButF(block, BUTM, 0, icon, (char*)name, 0, 0, w, UI_UNIT_Y, retvalue, 0.0, 0.0, 0, argval, "");
	else if(icon)
		uiDefIconButF(block, BUTM, 0, icon, 0, 0, w, UI_UNIT_Y, retvalue, 0.0, 0.0, 0, argval, "");
	else
		uiDefButF(block, BUTM, 0, (char*)name, 0, 0, w, UI_UNIT_Y, retvalue, 0.0, 0.0, 0, argval, "");
}

/* separator item */
void uiItemS(uiLayout *layout)
{
	uiBlock *block= layout->root->block;

	uiBlockSetCurLayout(block, layout);
	uiDefBut(block, SEPR, 0, "", 0, 0, EM_SEPR_X, EM_SEPR_Y, NULL, 0.0, 0.0, 0, 0, "");
}

/* level items */
void uiItemMenuF(uiLayout *layout, char *name, int icon, uiMenuCreateFunc func, void *arg)
{
	if(!func)
		return;

	ui_item_menu(layout, name, icon, func, arg, NULL);
}

typedef struct MenuItemLevel {
	int opcontext;
	char *opname;
	char *propname;
	PointerRNA rnapoin;
} MenuItemLevel;

static void menu_item_enum_opname_menu(bContext *C, uiLayout *layout, void *arg)
{
	MenuItemLevel *lvl= (MenuItemLevel*)(((uiBut*)arg)->func_argN);

	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);
	uiItemsEnumO(layout, lvl->opname, lvl->propname);
}

void uiItemMenuEnumO(uiLayout *layout, char *name, int icon, char *opname, char *propname)
{
	wmOperatorType *ot= WM_operatortype_find(opname, 0);
	MenuItemLevel *lvl;

	if(!ot || !ot->srna) {
		ui_item_disabled(layout, opname);
		return;
	}

	if(!name)
		name= ot->name;
	if(layout->root->type == UI_LAYOUT_MENU && !icon)
		icon= ICON_BLANK1;

	lvl= MEM_callocN(sizeof(MenuItemLevel), "MenuItemLevel");
	lvl->opname= opname;
	lvl->propname= propname;
	lvl->opcontext= layout->root->opcontext;

	ui_item_menu(layout, name, icon, menu_item_enum_opname_menu, NULL, lvl);
}

static void menu_item_enum_rna_menu(bContext *C, uiLayout *layout, void *arg)
{
	MenuItemLevel *lvl= (MenuItemLevel*)(((uiBut*)arg)->func_argN);

	uiLayoutSetOperatorContext(layout, lvl->opcontext);
	uiItemsEnumR(layout, &lvl->rnapoin, lvl->propname);
}

void uiItemMenuEnumR(uiLayout *layout, char *name, int icon, struct PointerRNA *ptr, char *propname)
{
	MenuItemLevel *lvl;
	PropertyRNA *prop;

	prop= RNA_struct_find_property(ptr, propname);
	if(!prop) {
		ui_item_disabled(layout, propname);
		return;
	}

	if(!name)
		name= (char*)RNA_property_ui_name(prop);
	if(layout->root->type == UI_LAYOUT_MENU && !icon)
		icon= ICON_BLANK1;

	lvl= MEM_callocN(sizeof(MenuItemLevel), "MenuItemLevel");
	lvl->rnapoin= *ptr;
	lvl->propname= propname;
	lvl->opcontext= layout->root->opcontext;

	ui_item_menu(layout, name, icon, menu_item_enum_rna_menu, NULL, lvl);
}

/**************************** Layout Items ***************************/

/* single-row layout */
static void ui_litem_estimate_row(uiLayout *litem)
{
	uiItem *item;
	int itemw, itemh;

	litem->w= 0;
	litem->h= 0;

	for(item=litem->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh);

		litem->w += itemw;
		litem->h= MAX2(itemh, litem->h);

		if(item->next)
			litem->w += litem->space;
	}
}

static int ui_litem_min_width(int itemw)
{
	return MIN2(2*UI_UNIT_X, itemw);
}

static void ui_litem_layout_row(uiLayout *litem)
{
	uiItem *item;
	int x, y, w, tot, totw, neww, itemw, minw, itemh, offset;
	int fixedw, freew, fixedx, freex, flag= 0, lastw= 0;

	x= litem->x;
	y= litem->y;
	w= litem->w;
	totw= 0;
	tot= 0;

	for(item=litem->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh);
		totw += itemw;
		tot++;
	}

	if(totw == 0)
		return;
	
	if(w != 0)
		w -= (tot-1)*litem->space;
	fixedw= 0;

	/* keep clamping items to fixed minimum size until all are done */
	do {
		freew= 0;
		x= 0;
		flag= 0;

		for(item=litem->items.first; item; item=item->next) {
			if(item->flag)
				continue;

			ui_item_size(item, &itemw, &itemh);
			minw= ui_litem_min_width(itemw);

			if(w - lastw > 0)
				neww= ui_item_fit(itemw, x, totw, w-lastw, !item->next, litem->alignment, NULL);
			else
				neww= 0; /* no space left, all will need clamping to minimum size */

			x += neww;

			if((neww < minw || itemw == minw) && w != 0) {
				/* fixed size */
				item->flag= 1;
				fixedw += minw;
				flag= 1;
				totw -= itemw;
			}
			else {
				/* keep free size */
				item->flag= 0;
				freew += itemw;
			}
		}

		lastw= fixedw;
	} while(flag);

	freex= 0;
	fixedx= 0;
	x= litem->x;

	for(item=litem->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh);
		minw= ui_litem_min_width(itemw);

		if(item->flag) {
			/* fixed minimum size items */
			itemw= ui_item_fit(minw, fixedx, fixedw, MIN2(w, fixedw), !item->next, litem->alignment, NULL);
			fixedx += itemw;
		}
		else {
			/* free size item */
			itemw= ui_item_fit(itemw, freex, freew, w-fixedw, !item->next, litem->alignment, NULL);
			freex += itemw;
		}

		/* align right/center */
		offset= 0;
		if(litem->alignment == UI_LAYOUT_ALIGN_RIGHT) {
			if(freew > 0 && freew < w-fixedw)
				offset= (w - fixedw) - freew;
		}
		else if(litem->alignment == UI_LAYOUT_ALIGN_CENTER) {
			if(freew > 0 && freew < w-fixedw)
				offset= ((w - fixedw) - freew)/2;
		}

		/* position item */
		ui_item_position(item, x+offset, y-itemh, itemw, itemh);

		x += itemw;
		if(item->next)
			x += litem->space;
	}

	litem->w= x - litem->x;
	litem->h= litem->y - y;
	litem->x= x;
	litem->y= y;
}

/* single-column layout */
static void ui_litem_estimate_column(uiLayout *litem)
{
	uiItem *item;
	int itemw, itemh;

	litem->w= 0;
	litem->h= 0;

	for(item=litem->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh);

		litem->w= MAX2(litem->w, itemw);
		litem->h += itemh;

		if(item->next)
			litem->h += litem->space;
	}
}

static void ui_litem_layout_column(uiLayout *litem)
{
	uiItem *item;
	int itemh, x, y;

	x= litem->x;
	y= litem->y;

	for(item=litem->items.first; item; item=item->next) {
		ui_item_size(item, NULL, &itemh);

		y -= itemh;
		ui_item_position(item, x, y, litem->w, itemh);

		if(item->next)
			y -= litem->space;
	}

	litem->h= litem->y - y;
	litem->x= x;
	litem->y= y;
}

/* root layout */
static void ui_litem_estimate_root(uiLayout *litem)
{
	/* nothing to do */
}

static void ui_litem_layout_root(uiLayout *litem)
{
	if(litem->root->type == UI_LAYOUT_HEADER)
		ui_litem_layout_row(litem);
	else
		ui_litem_layout_column(litem);
}

/* box layout */
static void ui_litem_estimate_box(uiLayout *litem)
{
	uiStyle *style= litem->root->style;

	ui_litem_estimate_column(litem);
	litem->w += 2*style->boxspace;
	litem->h += style->boxspace;
}

static void ui_litem_layout_box(uiLayout *litem)
{
	uiLayoutItemBx *box= (uiLayoutItemBx*)litem;
	uiStyle *style= litem->root->style;
	uiBut *but;
	int w, h;

	w= litem->w;
	h= litem->h;

	litem->x += style->boxspace;

	if(w != 0) litem->w -= 2*style->boxspace;
	if(h != 0) litem->h -= 2*style->boxspace;

	ui_litem_layout_column(litem);

	litem->x -= style->boxspace;
	litem->y -= style->boxspace;

	if(w != 0) litem->w += 2*style->boxspace;
	if(h != 0) litem->h += style->boxspace;

	/* roundbox around the sublayout */
	but= box->roundbox;
	but->x1= litem->x;
	but->y1= litem->y;
	but->x2= litem->x+litem->w;
	but->y2= litem->y+litem->h;
}

/* multi-column layout, automatically flowing to the next */
static void ui_litem_estimate_column_flow(uiLayout *litem)
{
	uiStyle *style= litem->root->style;
	uiLayoutItemFlow *flow= (uiLayoutItemFlow*)litem;
	uiItem *item;
	int col, x, y, emh, emy, miny, itemw, itemh, maxw=0;
	int toth, totitem;

	/* compute max needed width and total height */
	toth= 0;
	totitem= 0;
	for(item=litem->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh);
		maxw= MAX2(maxw, itemw);
		toth += itemh;
		totitem++;
	}

	if(flow->number <= 0) {
		/* auto compute number of columns, not very good */
		if(maxw == 0) {
			flow->totcol= 1;
			return;
		}

		flow->totcol= MAX2(litem->root->emw/maxw, 1);
		flow->totcol= MIN2(flow->totcol, totitem);
	}
	else
		flow->totcol= flow->number;

	/* compute sizes */
	x= 0;
	y= 0;
	emy= 0;
	miny= 0;

	maxw= 0;
	emh= toth/flow->totcol;

	/* create column per column */
	col= 0;
	for(item=litem->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh);

		y -= itemh + style->buttonspacey;
		miny= MIN2(miny, y);
		emy -= itemh;
		maxw= MAX2(itemw, maxw);

		/* decide to go to next one */
		if(col < flow->totcol-1 && emy <= -emh) {
			x += maxw + litem->space;
			maxw= 0;
			y= 0;
			col++;
		}
	}

	litem->w= x;
	litem->h= litem->y - miny;
}

static void ui_litem_layout_column_flow(uiLayout *litem)
{
	uiStyle *style= litem->root->style;
	uiLayoutItemFlow *flow= (uiLayoutItemFlow*)litem;
	uiItem *item;
	int col, x, y, w, emh, emy, miny, itemw, itemh;
	int toth, totitem, offset;

	/* compute max needed width and total height */
	toth= 0;
	totitem= 0;
	for(item=litem->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh);
		toth += itemh;
		totitem++;
	}

	/* compute sizes */
	x= litem->x;
	y= litem->y;
	emy= 0;
	miny= 0;

	w= litem->w - (flow->totcol-1)*style->columnspace;
	emh= toth/flow->totcol;

	/* create column per column */
	col= 0;
	for(item=litem->items.first; item; item=item->next) {
		ui_item_size(item, NULL, &itemh);
		itemw= ui_item_fit(1, x-litem->x, flow->totcol, w, col == flow->totcol-1, litem->alignment, &offset);
	
		y -= itemh;
		emy -= itemh;
		ui_item_position(item, x+offset, y, itemw, itemh);
		y -= style->buttonspacey;
		miny= MIN2(miny, y);

		/* decide to go to next one */
		if(col < flow->totcol-1 && emy <= -emh) {
			x += itemw + style->columnspace;
			y= litem->y;
			col++;
		}
	}

	litem->h= litem->y - miny;
	litem->x= x;
	litem->y= miny;
}

/* free layout */
static void ui_litem_estimate_absolute(uiLayout *litem)
{
	uiItem *item;
	int itemx, itemy, itemw, itemh, minx, miny;

	minx= 1e6;
	miny= 1e6;
	litem->w= 0;
	litem->h= 0;

	for(item=litem->items.first; item; item=item->next) {
		ui_item_offset(item, &itemx, &itemy);
		ui_item_size(item, &itemw, &itemh);

		minx= MIN2(minx, itemx);
		miny= MIN2(miny, itemy);

		litem->w= MAX2(litem->w, itemx+itemw);
		litem->h= MAX2(litem->h, itemy+itemh);
	}

	litem->w -= minx;
	litem->h -= miny;
}

static void ui_litem_layout_absolute(uiLayout *litem)
{
	uiItem *item;
	float scalex=1.0f, scaley=1.0f;
	int x, y, newx, newy, itemx, itemy, itemh, itemw, minx, miny, totw, toth;

	minx= 1e6;
	miny= 1e6;
	totw= 0;
	toth= 0;

	for(item=litem->items.first; item; item=item->next) {
		ui_item_offset(item, &itemx, &itemy);
		ui_item_size(item, &itemw, &itemh);

		minx= MIN2(minx, itemx);
		miny= MIN2(miny, itemy);

		totw= MAX2(totw, itemx+itemw);
		toth= MAX2(toth, itemy+itemh);
	}

	totw -= minx;
	toth -= miny;

	if(litem->w && totw > 0)
		scalex= (float)litem->w/(float)totw;
	if(litem->h && toth > 0)
		scaley= (float)litem->h/(float)toth;
	
	x= litem->x;
	y= litem->y - scaley*toth;

	for(item=litem->items.first; item; item=item->next) {
		ui_item_offset(item, &itemx, &itemy);
		ui_item_size(item, &itemw, &itemh);

		if(scalex != 1.0f) {
			newx= (itemx - minx)*scalex;
			itemw= (itemx - minx + itemw)*scalex - newx;
			itemx= minx + newx;
		}

		if(scaley != 1.0f) {
			newy= (itemy - miny)*scaley;
			itemh= (itemy - miny + itemh)*scaley - newy;
			itemy= miny + newy;
		}

		ui_item_position(item, x+itemx-minx, y+itemy-miny, itemw, itemh);
	}

	litem->w= scalex*totw;
	litem->h= litem->y - y;
	litem->x= x + litem->w;
	litem->y= y;
}

/* split layout */
static void ui_litem_estimate_split(uiLayout *litem)
{
	ui_litem_estimate_row(litem);
}

static void ui_litem_layout_split(uiLayout *litem)
{
	uiLayoutItemSplt *split= (uiLayoutItemSplt*)litem;
	uiItem *item;
	float percentage;
	int itemh, x, y, w, tot=0, colw=0;

	x= litem->x;
	y= litem->y;

	for(item=litem->items.first; item; item=item->next)
		tot++;
	
	if(tot == 0)
		return;
	
	percentage= (split->percentage == 0.0f)? 1.0f/(float)tot: split->percentage;
	
	w= (litem->w - (tot-1)*litem->space);
	colw= w*percentage;
	colw= MAX2(colw, 0);

	for(item=litem->items.first; item; item=item->next) {
		ui_item_size(item, NULL, &itemh);

		ui_item_position(item, x, y-itemh, colw, itemh);
		x += colw;

		if(item->next) {
			colw= (w - (int)(w*percentage))/(tot-1);
			colw= MAX2(colw, 0);

			x += litem->space;
		}
	}

	litem->w= x - litem->x;
	litem->h= litem->y - y;
	litem->x= x;
	litem->y= y;
}

/* overlap layout */
static void ui_litem_estimate_overlap(uiLayout *litem)
{
	uiItem *item;
	int itemw, itemh;

	litem->w= 0;
	litem->h= 0;

	for(item=litem->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh);

		litem->w= MAX2(itemw, litem->w);
		litem->h= MAX2(itemh, litem->h);
	}
}

static void ui_litem_layout_overlap(uiLayout *litem)
{
	uiItem *item;
	int itemw, itemh, x, y;

	x= litem->x;
	y= litem->y;

	for(item=litem->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh);
		ui_item_position(item, x, y-itemh, litem->w, itemh);

		litem->h= MAX2(litem->h, itemh);
	}

	litem->x= x;
	litem->y= y - litem->h;
}

/* layout create functions */
uiLayout *uiLayoutRow(uiLayout *layout, int align)
{
	uiLayout *litem;

	litem= MEM_callocN(sizeof(uiLayout), "uiLayoutRow");
	litem->item.type= ITEM_LAYOUT_ROW;
	litem->root= layout->root;
	litem->align= align;
	litem->active= 1;
	litem->enabled= 1;
	litem->context= layout->context;
	litem->space= (align)? 0: layout->root->style->buttonspacex;
	BLI_addtail(&layout->items, litem);

	uiBlockSetCurLayout(layout->root->block, litem);

	return litem;
}

uiLayout *uiLayoutColumn(uiLayout *layout, int align)
{
	uiLayout *litem;

	litem= MEM_callocN(sizeof(uiLayout), "uiLayoutColumn");
	litem->item.type= ITEM_LAYOUT_COLUMN;
	litem->root= layout->root;
	litem->align= align;
	litem->active= 1;
	litem->enabled= 1;
	litem->context= layout->context;
	litem->space= (litem->align)? 0: layout->root->style->buttonspacey;
	BLI_addtail(&layout->items, litem);

	uiBlockSetCurLayout(layout->root->block, litem);

	return litem;
}

uiLayout *uiLayoutColumnFlow(uiLayout *layout, int number, int align)
{
	uiLayoutItemFlow *flow;

	flow= MEM_callocN(sizeof(uiLayoutItemFlow), "uiLayoutItemFlow");
	flow->litem.item.type= ITEM_LAYOUT_COLUMN_FLOW;
	flow->litem.root= layout->root;
	flow->litem.align= align;
	flow->litem.active= 1;
	flow->litem.enabled= 1;
	flow->litem.context= layout->context;
	flow->litem.space= (flow->litem.align)? 0: layout->root->style->columnspace;
	flow->number= number;
	BLI_addtail(&layout->items, flow);

	uiBlockSetCurLayout(layout->root->block, &flow->litem);

	return &flow->litem;
}

static uiLayoutItemBx *ui_layout_box(uiLayout *layout, int type)
{
	uiLayoutItemBx *box;

	box= MEM_callocN(sizeof(uiLayoutItemBx), "uiLayoutItemBx");
	box->litem.item.type= ITEM_LAYOUT_BOX;
	box->litem.root= layout->root;
	box->litem.active= 1;
	box->litem.enabled= 1;
	box->litem.context= layout->context;
	box->litem.space= layout->root->style->columnspace;
	BLI_addtail(&layout->items, box);

	uiBlockSetCurLayout(layout->root->block, &box->litem);

	box->roundbox= uiDefBut(layout->root->block, type, 0, "", 0, 0, 0, 0, NULL, 0.0, 0.0, 0, 0, "");

	return box;
}

uiLayout *uiLayoutBox(uiLayout *layout)
{
	return (uiLayout*)ui_layout_box(layout, ROUNDBOX);
}

uiLayout *uiLayoutListBox(uiLayout *layout, PointerRNA *ptr, PropertyRNA *prop, PointerRNA *actptr, PropertyRNA *actprop)
{
	uiLayoutItemBx *box= ui_layout_box(layout, LISTBOX);
	uiBut *but= box->roundbox;

	but->rnasearchpoin= *ptr;
	but->rnasearchprop= prop;
	but->rnapoin= *actptr;
	but->rnaprop= actprop;

	return (uiLayout*)box;
}

uiLayout *uiLayoutAbsolute(uiLayout *layout, int align)
{
	uiLayout *litem;

	litem= MEM_callocN(sizeof(uiLayout), "uiLayoutAbsolute");
	litem->item.type= ITEM_LAYOUT_ABSOLUTE;
	litem->root= layout->root;
	litem->align= align;
	litem->active= 1;
	litem->enabled= 1;
	litem->context= layout->context;
	BLI_addtail(&layout->items, litem);

	uiBlockSetCurLayout(layout->root->block, litem);

	return litem;
}

uiBlock *uiLayoutAbsoluteBlock(uiLayout *layout)
{
	uiBlock *block;

	block= uiLayoutGetBlock(layout);
	uiLayoutAbsolute(layout, 0);

	return block;
}

uiLayout *uiLayoutOverlap(uiLayout *layout)
{
	uiLayout *litem;

	litem= MEM_callocN(sizeof(uiLayout), "uiLayoutOverlap");
	litem->item.type= ITEM_LAYOUT_OVERLAP;
	litem->root= layout->root;
	litem->active= 1;
	litem->enabled= 1;
	litem->context= layout->context;
	BLI_addtail(&layout->items, litem);

	uiBlockSetCurLayout(layout->root->block, litem);

	return litem;
}

uiLayout *uiLayoutSplit(uiLayout *layout, float percentage, int align)
{
	uiLayoutItemSplt *split;

	split= MEM_callocN(sizeof(uiLayoutItemSplt), "uiLayoutItemSplt");
	split->litem.item.type= ITEM_LAYOUT_SPLIT;
	split->litem.root= layout->root;
	split->litem.align= align;
	split->litem.active= 1;
	split->litem.enabled= 1;
	split->litem.context= layout->context;
	split->litem.space= layout->root->style->columnspace;
	split->percentage= percentage;
	BLI_addtail(&layout->items, split);

	uiBlockSetCurLayout(layout->root->block, &split->litem);

	return &split->litem;
}

void uiLayoutSetActive(uiLayout *layout, int active)
{
	layout->active= active;
}

void uiLayoutSetEnabled(uiLayout *layout, int enabled)
{
	layout->enabled= enabled;
}

void uiLayoutSetRedAlert(uiLayout *layout, int redalert)
{
	layout->redalert= redalert;
}

void uiLayoutSetKeepAspect(uiLayout *layout, int keepaspect)
{
	layout->keepaspect= keepaspect;
}

void uiLayoutSetAlignment(uiLayout *layout, int alignment)
{
	layout->alignment= alignment;
}

void uiLayoutSetScaleX(uiLayout *layout, float scale)
{
	layout->scale[0]= scale;
}

void uiLayoutSetScaleY(uiLayout *layout, float scale)
{
	layout->scale[1]= scale;
}

int uiLayoutGetActive(uiLayout *layout)
{
	return layout->active;
}

int uiLayoutGetEnabled(uiLayout *layout)
{
	return layout->enabled;
}

int uiLayoutGetRedAlert(uiLayout *layout)
{
	return layout->redalert;
}

int uiLayoutGetKeepAspect(uiLayout *layout)
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
	return layout->scale[0];
}

/********************** Layout *******************/

static void ui_item_scale(uiLayout *litem, float scale[2])
{
	uiItem *item;
	int x, y, w, h;

	for(item=litem->items.last; item; item=item->prev) {
		ui_item_size(item, &w, &h);
		ui_item_offset(item, &x, &y);

		if(scale[0] != 0.0f) {
			x *= scale[0];
			w *= scale[0];
		}

		if(scale[1] != 0.0f) {
			y *= scale[1];
			h *= scale[1];
		}

		ui_item_position(item, x, y, w, h);
	}
}

static void ui_item_estimate(uiItem *item)
{
	uiItem *subitem;

	if(item->type != ITEM_BUTTON) {
		uiLayout *litem= (uiLayout*)item;

		for(subitem=litem->items.first; subitem; subitem=subitem->next)
			ui_item_estimate(subitem);

		if(litem->items.first == NULL)
			return;

		if(litem->scale[0] != 0.0f || litem->scale[1] != 0.0f)
			ui_item_scale(litem, litem->scale);

		switch(litem->item.type) {
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

static void ui_item_align(uiLayout *litem, int nr)
{
	uiItem *item;
	uiButtonItem *bitem;
	uiLayoutItemBx *box;

	for(item=litem->items.last; item; item=item->prev) {
		if(item->type == ITEM_BUTTON) {
			bitem= (uiButtonItem*)item;
			if(ui_but_can_align(bitem->but))
				if(!bitem->but->alignnr)
					bitem->but->alignnr= nr;
		}
		else if(item->type == ITEM_LAYOUT_ABSOLUTE);
		else if(item->type == ITEM_LAYOUT_OVERLAP);
		else if(item->type == ITEM_LAYOUT_BOX) {
			box= (uiLayoutItemBx*)item;
			box->roundbox->alignnr= nr;
			BLI_remlink(&litem->root->block->buttons, box->roundbox);
			BLI_addhead(&litem->root->block->buttons, box->roundbox);
		}
		else
			ui_item_align((uiLayout*)item, nr);
	}
}

static void ui_item_flag(uiLayout *litem, int flag)
{
	uiItem *item;
	uiButtonItem *bitem;

	for(item=litem->items.last; item; item=item->prev) {
		if(item->type == ITEM_BUTTON) {
			bitem= (uiButtonItem*)item;
			bitem->but->flag |= flag;
		}
		else
			ui_item_flag((uiLayout*)item, flag);
	}
}

static void ui_item_layout(uiItem *item)
{
	uiItem *subitem;

	if(item->type != ITEM_BUTTON) {
		uiLayout *litem= (uiLayout*)item;

		if(litem->items.first == NULL)
			return;

		if(litem->align)
			ui_item_align(litem, ++litem->root->block->alignnr);
		if(!litem->active)
			ui_item_flag(litem, UI_BUT_INACTIVE);
		if(!litem->enabled)
			ui_item_flag(litem, UI_BUT_DISABLED);

		switch(litem->item.type) {
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

		for(subitem=litem->items.first; subitem; subitem=subitem->next)
			ui_item_layout(subitem);
	}
}

static void ui_layout_end(uiBlock *block, uiLayout *layout, int *x, int *y)
{
	if(layout->root->handlefunc)
		uiBlockSetButmFunc(block, layout->root->handlefunc, layout->root->argv);

	ui_item_estimate(&layout->item);
	ui_item_layout(&layout->item);

	if(x) *x= layout->x;
	if(y) *y= layout->y;
}

static void ui_layout_free(uiLayout *layout)
{
	uiItem *item, *next;

	for(item=layout->items.first; item; item=next) {
		next= item->next;

		if(item->type == ITEM_BUTTON)
			MEM_freeN(item);
		else
			ui_layout_free((uiLayout*)item);
	}

	MEM_freeN(layout);
}

uiLayout *uiBlockLayout(uiBlock *block, int dir, int type, int x, int y, int size, int em, uiStyle *style)
{
	uiLayout *layout;
	uiLayoutRoot *root;

	root= MEM_callocN(sizeof(uiLayoutRoot), "uiLayoutRoot");
	root->type= type;
	root->style= style;
	root->block= block;
	root->opcontext= WM_OP_INVOKE_REGION_WIN;

	layout= MEM_callocN(sizeof(uiLayout), "uiLayout");
	layout->item.type= ITEM_LAYOUT_ROOT;

	layout->x= x;
	layout->y= y;
	layout->root= root;
	layout->space= style->templatespace;
	layout->active= 1;
	layout->enabled= 1;
	layout->context= NULL;

	if(type == UI_LAYOUT_MENU)
		layout->space= 0;

	if(dir == UI_LAYOUT_HORIZONTAL) {
		layout->h= size;
		layout->root->emh= em*UI_UNIT_Y;
	}
	else {
		layout->w= size;
		layout->root->emw= em*UI_UNIT_X;
	}

	block->curlayout= layout;
	root->layout= layout;
	BLI_addtail(&block->layouts, root);
	
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
	block->curlayout= layout;
}

void ui_layout_add_but(uiLayout *layout, uiBut *but)
{
	uiButtonItem *bitem;
	
	bitem= MEM_callocN(sizeof(uiButtonItem), "uiButtonItem");
	bitem->item.type= ITEM_BUTTON;
	bitem->but= but;
	BLI_addtail(&layout->items, bitem);

	if(layout->context) {
		but->context= layout->context;
		but->context->used= 1;
	}
}

void uiLayoutSetOperatorContext(uiLayout *layout, int opcontext)
{
	layout->root->opcontext= opcontext;
}

void uiLayoutSetFunc(uiLayout *layout, uiMenuHandleFunc handlefunc, void *argv)
{
	layout->root->handlefunc= handlefunc;
	layout->root->argv= argv;
}

void uiBlockLayoutResolve(uiBlock *block, int *x, int *y)
{
	uiLayoutRoot *root;

	if(x) *x= 0;
	if(y) *y= 0;

	block->curlayout= NULL;

	for(root=block->layouts.first; root; root=root->next) {
		/* NULL in advance so we don't interfere when adding button */
		ui_layout_end(block, root->layout, x, y);
		ui_layout_free(root->layout);
	}

	BLI_freelistN(&block->layouts);

	/* XXX silly trick, interface_templates.c doesn't get linked
	 * because it's not used by other files in this module? */
	{
		void ui_template_fix_linking();
		ui_template_fix_linking();
	}
}

void uiLayoutSetContextPointer(uiLayout *layout, char *name, PointerRNA *ptr)
{
	uiBlock *block= layout->root->block;
	layout->context= CTX_store_add(&block->contexts, name, ptr);
}


/* introspect funcs */
#include "BLI_dynstr.h"

static void ui_intro_button(DynStr *ds, uiButtonItem *bitem)
{
	uiBut *but = bitem->but;
	BLI_dynstr_appendf(ds, "'type':%d, ", but->type); /* see ~ UI_interface.h:200 */
	BLI_dynstr_appendf(ds, "'draw_string':'''%s''', ", but->drawstr);
	BLI_dynstr_appendf(ds, "'tip':'''%s''', ", but->tip ? but->tip : ""); // not exactly needed, rna has this

	if(but->optype) {
		char *opstr = WM_operator_pystring(but->block->evil_C, but->optype, but->opptr, 0);
		BLI_dynstr_appendf(ds, "'operator':'''%s''', ", opstr ? opstr : "");
		MEM_freeN(opstr);
	}

	if(but->rnaprop) {
		BLI_dynstr_appendf(ds, "'rna':'%s.%s[%d]', ", RNA_struct_identifier(but->rnapoin.type), RNA_property_identifier(but->rnaprop), but->rnaindex);
	}

}

static void ui_intro_items(DynStr *ds, ListBase *lb)
{
	uiItem *item;

	BLI_dynstr_append(ds, "[");

	for(item=lb->first; item; item=item->next) {

		BLI_dynstr_append(ds, "{");

		/* could also use the INT but this is nicer*/
		switch(item->type) {
		case ITEM_BUTTON:			BLI_dynstr_append(ds, "'type':'BUTTON', ");break;
		case ITEM_LAYOUT_ROW:		BLI_dynstr_append(ds, "'type':'ROW', "); break;
		case ITEM_LAYOUT_COLUMN:	BLI_dynstr_append(ds, "'type':'COLUMN', "); break;
		case ITEM_LAYOUT_COLUMN_FLOW:BLI_dynstr_append(ds, "'type':'COLUMN_FLOW', "); break;
		case ITEM_LAYOUT_ROW_FLOW:	BLI_dynstr_append(ds, "'type':'ROW_FLOW', "); break;
		case ITEM_LAYOUT_BOX:		BLI_dynstr_append(ds, "'type':'BOX', "); break;
		case ITEM_LAYOUT_ABSOLUTE:	BLI_dynstr_append(ds, "'type':'ABSOLUTE', "); break;
		case ITEM_LAYOUT_SPLIT:		BLI_dynstr_append(ds, "'type':'SPLIT', "); break;
		case ITEM_LAYOUT_OVERLAP:	BLI_dynstr_append(ds, "'type':'OVERLAP', "); break;
		case ITEM_LAYOUT_ROOT:		BLI_dynstr_append(ds, "'type':'ROOT', "); break;
		default:					BLI_dynstr_append(ds, "'type':'UNKNOWN', "); break;
		}

		switch(item->type) {
		case ITEM_BUTTON:
			ui_intro_button(ds, (uiButtonItem *)item);
			break;
		default:
			BLI_dynstr_append(ds, "'items':");
			ui_intro_items(ds, &((uiLayout*)item)->items);
			break;
		}

		BLI_dynstr_append(ds, "}");

		if(item != lb->last)
			BLI_dynstr_append(ds, ", ");
	}
	BLI_dynstr_append(ds, "], ");
}

static void ui_intro_uiLayout(DynStr *ds, uiLayout *layout)
{
	ui_intro_items(ds, &layout->items);
}

static char *str = NULL; // XXX, constant re-freeing, far from ideal.
char *uiLayoutIntrospect(uiLayout *layout)
{
	DynStr *ds= BLI_dynstr_new();

	if(str)
		MEM_freeN(str);

	ui_intro_uiLayout(ds, layout);

	str = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);

	return str;
}
