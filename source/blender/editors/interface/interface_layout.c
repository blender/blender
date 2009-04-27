
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

#include "BIF_gl.h"

#include "ED_util.h"
#include "ED_types.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.h"

/************************ Structs and Defines *************************/

#define RNA_NO_INDEX	-1
#define RNA_ENUM_VALUE	-2

#define EM_UNIT_X		XIC
#define EM_UNIT_Y		YIC

#define EM_SEPR_X		6
#define EM_SEPR_Y		6

/* Item */

typedef enum uiItemType {
	ITEM_OPERATOR,
	ITEM_RNA_PROPERTY,
	ITEM_MENU,
	ITEM_LABEL,
	ITEM_VALUE,
	ITEM_SEPARATOR
} uiItemType;

enum uiItemFlag {
	ITEM_ICON,
	ITEM_TEXT
};

typedef struct uiItem {
	struct uiItem *next, *prev;
	uiItemType type;
	int slot;

	char *name;
	char namestr[UI_MAX_NAME_STR];
	int icon;
	int disabled;
} uiItem;

typedef struct uiItemRNA {
	uiItem item;

	PointerRNA ptr;
	PropertyRNA *prop;
	int index, value;
	int expand;
} uiItemRNA;

typedef struct uiItemOp {
	uiItem item;

	wmOperatorType *ot;
	IDProperty *properties;
	int context;
} uiItemOp;

typedef struct uiItemMenu {
	uiItem item;

	char *menuname;
	uiMenuCreateFunc func;
	void *arg, *argN;
} uiItemMenu;

typedef struct uiItemValue {
	uiItem item;

	int argval;
} uiItemValue;

/* Template */

typedef enum uiTemplateType {
	TEMPLATE_ROW,
	TEMPLATE_COLUMN,
	TEMPLATE_COLUMN_FLOW,
	TEMPLATE_SPLIT,
	TEMPLATE_BOX,

	TEMPLATE_HEADER,
	TEMPLATE_HEADER_ID
} uiTemplateType;

typedef struct uiTemplate {
	struct uiTemplate *next, *prev;
	uiTemplateType type;

	ListBase items;
	int slot;
} uiTemplate;

typedef struct uiTemplateFlow {
	uiTemplate template;
	int number;
} uiTemplateFlow;

typedef struct uiTemplateSplt {
	uiTemplate template;
	int number;
	int lr;
	uiLayout **sublayout;
} uiTemplateSplt;

typedef struct uiTemplateBx {
	uiTemplate template;
	uiLayout *sublayout;
} uiTemplateBx;

typedef struct uiTemplateHeadID {
	uiTemplate template;

	PointerRNA ptr;
	PropertyRNA *prop;

	int flag;
	short browse;
	char *newop;
	char *openop;
	char *unlinkop;
} uiTemplateHeadID;

/* Layout */

struct uiLayout {
	ListBase templates;
	int opcontext;
	int dir, type;
	int x, y, w, h;
	int emw, emh;

	uiMenuHandleFunc handlefunc;
	void *argv;

	uiStyle *style;
};

void ui_layout_free(uiLayout *layout);
void ui_layout_end(const bContext *C, uiBlock *block, uiLayout *layout, int *x, int *y);

/************************** Item ***************************/

static void ui_item_name(uiItem *item, char *name)
{
	if(!item->name && name) {
		BLI_strncpy(item->namestr, name, sizeof(item->namestr));
		item->name= item->namestr;
	}
}
static void ui_item_name_add_colon(uiItem *item)
{
	int len= strlen(item->namestr);

	if(len != 0 && len+1 < sizeof(item->namestr)) {
		item->namestr[len]= ':';
		item->namestr[len+1]= '\0';
	}
}

#define UI_FIT_EXPAND 1

static int ui_item_fit(int item, int pos, int all, int available, int spacing, int last, int flag)
{
	if(all > available-spacing) {
		/* contents is bigger than available space */
		if(last)
			return available-pos;
		else
			return (item*(available-spacing))/all;
	}
	else {
		/* contents is smaller or equal to available space */
		if(flag & UI_FIT_EXPAND) {
			if(last)
				return available-pos;
			else
				return (item*(available-spacing))/all;
		}
		else
			return item;
	}
}

/* create buttons for an item with an RNA array */
static void ui_item_array(uiLayout *layout, uiBlock *block, uiItemRNA *rnaitem, int len, int x, int y, int w, int h)
{
	uiStyle *style= layout->style;
	PropertyType type;
	PropertySubType subtype;
	int a;

	/* retrieve type and subtype */
	type= RNA_property_type(rnaitem->prop);
	subtype= RNA_property_subtype(rnaitem->prop);

	/* create label */
	if(strcmp(rnaitem->item.name, "") != 0)
		uiDefBut(block, LABEL, 0, rnaitem->item.name, x, y + h - EM_UNIT_Y, w, EM_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");

	/* create buttons */
	uiBlockBeginAlign(block);

	if(type == PROP_BOOLEAN && len == 20) {
		/* special check for layer layout */
		int butw, buth;

		butw= ui_item_fit(EM_UNIT_X, 0, EM_UNIT_X*10 + style->buttonspacex, w, 0, 0, UI_FIT_EXPAND);
		buth= MIN2(EM_UNIT_Y, butw);

		y += 2*(EM_UNIT_Y - buth);

		uiBlockBeginAlign(block);
		for(a=0; a<5; a++)
			uiDefAutoButR(block, &rnaitem->ptr, rnaitem->prop, a, "", ICON_BLANK1, x + butw*a, y+buth, butw, buth);
		for(a=0; a<5; a++)
			uiDefAutoButR(block, &rnaitem->ptr, rnaitem->prop, a+10, "", ICON_BLANK1, x + butw*a, y, butw, buth);
		uiBlockEndAlign(block);

		x += 5*butw + style->buttonspacex;

		uiBlockBeginAlign(block);
		for(a=0; a<5; a++)
			uiDefAutoButR(block, &rnaitem->ptr, rnaitem->prop, a+5, "", ICON_BLANK1, x + butw*a, y+buth, butw, buth);
		for(a=0; a<5; a++)
			uiDefAutoButR(block, &rnaitem->ptr, rnaitem->prop, a+15, "", ICON_BLANK1, x + butw*a, y, butw, buth);
		uiBlockEndAlign(block);
	}
	else if(subtype == PROP_MATRIX) {
		/* matrix layout */
		int row, col;

		len= ceil(sqrt(len));

		h /= len;
		w /= len;

		// XXX test
		for(a=0; a<len; a++) {
			col= a%len;
			row= a/len;

			uiDefAutoButR(block, &rnaitem->ptr, rnaitem->prop, a, "", 0, x + w*col, y+(row-a-1)*EM_UNIT_Y, w, EM_UNIT_Y);
		}
	}
	else if(len <= 4 && ELEM3(subtype, PROP_ROTATION, PROP_VECTOR, PROP_COLOR)) {
		/* layout for known array subtypes */
		static char vectoritem[4]= {'X', 'Y', 'Z', 'W'};
		static char quatitem[4]= {'W', 'X', 'Y', 'Z'};
		static char coloritem[4]= {'R', 'G', 'B', 'A'};
		char str[3];

		for(a=0; a<len; a++) {
			if(len == 4 && subtype == PROP_ROTATION)
				str[0]= quatitem[a];
			else if(subtype == PROP_VECTOR || subtype == PROP_ROTATION)
				str[0]= vectoritem[a];
			else
				str[0]= coloritem[a];

			if(type == PROP_BOOLEAN) {
				str[1]= '\0';
			}
			else {
				str[1]= ':';
				str[2]= '\0';
			}

			uiDefAutoButR(block, &rnaitem->ptr, rnaitem->prop, a, str, 0, x, y+(len-a-1)*EM_UNIT_Y, w, EM_UNIT_Y);
		}
	}
	else {
		/* default array layout */
		for(a=0; a<len; a++)
			uiDefAutoButR(block, &rnaitem->ptr, rnaitem->prop, a, "", 0, x, y+(len-a-1)*EM_UNIT_Y, w, EM_UNIT_Y);
	}

	uiBlockEndAlign(block);
}

static void ui_item_enum_row(uiBlock *block, uiItemRNA *rnaitem, int x, int y, int w, int h)
{
	const EnumPropertyItem *item;
	int a, totitem, pos, itemw;
	const char *propname;
	
	propname= RNA_property_identifier(rnaitem->prop);
	RNA_property_enum_items(&rnaitem->ptr, rnaitem->prop, &item, &totitem);

	uiBlockBeginAlign(block);
	pos= 0;
	for(a=0; a<totitem; a++) {
		itemw= ui_item_fit(1, pos, totitem, w, 0, a == totitem-1, UI_FIT_EXPAND);
		uiDefButR(block, ROW, 0, NULL, x+pos, y, itemw, h, &rnaitem->ptr, propname, -1, 0, item[a].value, -1, -1, NULL);
		pos += itemw;
	}
	uiBlockEndAlign(block);
}

/* create label + button for RNA property */
static void ui_item_with_label(uiBlock *block, uiItemRNA *rnaitem, int x, int y, int w, int h)
{
	if(strcmp(rnaitem->item.name, "") != 0) {
		w= w/2;
		uiDefBut(block, LABEL, 0, rnaitem->item.name, x, y, w, h, NULL, 0.0, 0.0, 0, 0, "");
		x += w;
	}

	uiDefAutoButR(block, &rnaitem->ptr, rnaitem->prop, rnaitem->index, "", rnaitem->item.icon, x, y, w, h);
}

/* create buttons for an arbitrary item */
static void ui_item_buts(uiLayout *layout, uiBlock *block, uiItem *item, int x, int y, int w, int h)
{
	if(item->type == ITEM_RNA_PROPERTY) {
		/* RNA property */
		uiItemRNA *rnaitem= (uiItemRNA*)item;
		PropertyType type;
		int len;
		
		/* retrieve info */
		type= RNA_property_type(rnaitem->prop);
		len= RNA_property_array_length(rnaitem->prop);

		/* array property */
		if(rnaitem->index == RNA_NO_INDEX && len > 0)
			ui_item_array(layout, block, rnaitem, len, x, y, w, h);
		/* enum item */
		else if(type == PROP_ENUM && rnaitem->index == RNA_ENUM_VALUE) {
			char *identifier= (char*)RNA_property_identifier(rnaitem->prop);

			if(item->icon && strcmp(item->name, "") != 0)
				uiDefIconTextButR(block, ROW, 0, item->icon, item->name, x, y, w, h, &rnaitem->ptr, identifier, -1, 0, rnaitem->value, -1, -1, NULL);
			else if(item->icon)
				uiDefIconButR(block, ROW, 0, item->icon, x, y, w, h, &rnaitem->ptr, identifier, -1, 0, rnaitem->value, -1, -1, NULL);
			else
				uiDefButR(block, ROW, 0, item->name, x, y, w, h, &rnaitem->ptr, identifier, -1, 0, rnaitem->value, -1, -1, NULL);
		}
		/* expanded enum */
		else if(type == PROP_ENUM && rnaitem->expand)
			ui_item_enum_row(block, rnaitem, x, y, w, h);
		/* property with separate label */
		else if(type == PROP_ENUM || type == PROP_STRING || type == PROP_POINTER)
			ui_item_with_label(block, rnaitem, x, y, w, h);
		/* single button */
		else
			uiDefAutoButR(block, &rnaitem->ptr, rnaitem->prop, rnaitem->index, (char*)item->name, item->icon, x, y, w, h);
	}
	else if(item->type == ITEM_OPERATOR) {
		/* operator */
		uiItemOp *opitem= (uiItemOp*)item;
		uiBut *but;

		if(item->icon && strcmp(item->name, "") != 0)
			but= uiDefIconTextButO(block, BUT, opitem->ot->idname, opitem->context, item->icon, (char*)item->name, x, y, w, h, NULL);
		else if(item->icon)
			but= uiDefIconButO(block, BUT, opitem->ot->idname, opitem->context, item->icon, x, y, w, h, NULL);
		/* text only */
		else
			but= uiDefButO(block, BUT, opitem->ot->idname, opitem->context, (char*)item->name, x, y, w, h, NULL);

		if(but && opitem->properties) {
			/* assign properties */
			PointerRNA *opptr= uiButGetOperatorPtrRNA(but);
			opptr->data= opitem->properties;
			opitem->properties= NULL;
		}
	}
	else if(item->type == ITEM_MENU) {
		/* menu */
		uiBut *but;
		uiItemMenu *menuitem= (uiItemMenu*)item;

		if(layout->type == UI_LAYOUT_HEADER) { /* ugly .. */
			y -= 2;
			w -= 3;
			h += 4;
		}

		if(item->icon)
			but= uiDefIconTextMenuBut(block, menuitem->func, menuitem->arg, item->icon, (char*)item->name, x, y, w, h, "");
		else
			but= uiDefMenuBut(block, menuitem->func, menuitem->arg, (char*)item->name, x, y, w, h, "");

		if(menuitem->argN) { /* ugly .. */
			but->poin= (char*)but;
			but->func_argN= menuitem->argN;
		}
	}
	else if(item->type == ITEM_LABEL) {
		/* label */
		uiBut *but;

		if(item->icon && strcmp(item->name, "") != 0)
			but= uiDefIconTextBut(block, LABEL, 0, item->icon, (char*)item->name, x, y, w, h, NULL, 0.0, 0.0, 0, 0, "");
		else if(item->icon)
			but= uiDefIconBut(block, LABEL, 0, item->icon, x, y, w, h, NULL, 0.0, 0.0, 0, 0, "");
		else
			but= uiDefBut(block, LABEL, 0, (char*)item->name, x, y, w, h, NULL, 0.0, 0.0, 0, 0, "");

		if(item->disabled) {
			but->flag |= UI_BUT_DISABLED;
			but->lock = 1;
			but->lockstr = "";
		}
	}
	else if(item->type == ITEM_VALUE) {
		/* label */
		uiItemValue *vitem= (uiItemValue*)item;
		float *retvalue= (block->handle)? &block->handle->retvalue: NULL;

		if(item->icon && strcmp(item->name, "") != 0)
			uiDefIconTextButF(block, BUTM, 0, item->icon, (char*)item->name, x, y, w, h, retvalue, 0.0, 0.0, 0, vitem->argval, "");
		else if(item->icon)
			uiDefIconButF(block, BUTM, 0, item->icon, x, y, w, h, retvalue, 0.0, 0.0, 0, vitem->argval, "");
		else
			uiDefButF(block, BUTM, 0, (char*)item->name, x, y, w, h, retvalue, 0.0, 0.0, 0, vitem->argval, "");
	}
	else {
		/* separator */
		uiDefBut(block, SEPR, 0, "", x, y, w, h, NULL, 0.0, 0.0, 0, 0, "");
	}
}

/* estimated size of text + icon */
static int ui_text_icon_width(char *name, int icon, int variable)
{
	if(icon && strcmp(name, "") == 0)
		return EM_UNIT_X; /* icon only */
	else if(icon)
		return (variable)? UI_GetStringWidth(name) + EM_UNIT_X: 10*EM_UNIT_X; /* icon + text */
	else
		return (variable)? UI_GetStringWidth(name) + EM_UNIT_X: 10*EM_UNIT_X; /* text only */
}

/* estimated size of an item */
#define UI_ITEM_VARY_X	1
#define UI_ITEM_VARY_Y	2

static void ui_item_size(uiItem *item, int *r_w, int *r_h, int flag)
{
	int w, h;

	if(item->type == ITEM_RNA_PROPERTY) {
		/* RNA property */
		uiItemRNA *rnaitem= (uiItemRNA*)item;
		PropertyType type;
		PropertySubType subtype;
		int len;

		w= ui_text_icon_width(item->name, item->icon, flag & UI_ITEM_VARY_X);
		h= EM_UNIT_Y;

		/* arbitrary extended width by type */
		type= RNA_property_type(rnaitem->prop);
		subtype= RNA_property_subtype(rnaitem->prop);
		len= RNA_property_array_length(rnaitem->prop);

		if(type == PROP_STRING)
			w += 10*EM_UNIT_X;

		/* increase height for arrays */
		if(rnaitem->index == RNA_NO_INDEX && len > 0) {
			if(strcmp(item->name, "") == 0 && item->icon == 0)
				h= 0;

			if(type == PROP_BOOLEAN && len == 20)
				h += 2*EM_UNIT_Y;
			else if(subtype == PROP_MATRIX)
				h += ceil(sqrt(len))*EM_UNIT_Y;
			else
				h += len*EM_UNIT_Y;
		}
		else if(flag & UI_ITEM_VARY_X) {
			if(type == PROP_BOOLEAN && strcmp(item->name, "") != 0)
				w += EM_UNIT_X;
		}
	}
	else {
		/* other */
		if(item->type == ITEM_SEPARATOR) {
			w= EM_SEPR_X;
			h= EM_SEPR_Y;
		}
		else {
			w= ui_text_icon_width(item->name, item->icon, flag & UI_ITEM_VARY_X);
			h= EM_UNIT_Y;
		}
	}

	if(r_w) *r_w= w;
	if(r_h) *r_h= h;
}

static void ui_item_free(uiItem *item)
{
	if(item->type == ITEM_OPERATOR) {
		uiItemOp *opitem= (uiItemOp*)item;

		if(opitem->properties) {
			IDP_FreeProperty(opitem->properties);
			MEM_freeN(opitem->properties);
		}
	}
}

/* disabled item */
static void ui_item_disabled(uiLayout *layout, char *name)
{
	uiTemplate *template= layout->templates.last;
	uiItem *item;
	
	if(!template)
		return;

	item= MEM_callocN(sizeof(uiItem), "uiItem");

	ui_item_name(item, name);
	item->disabled= 1;
	item->type= ITEM_LABEL;
	item->slot= template->slot;

	BLI_addtail(&template->items, item);
}

/* operator items */
void uiItemFullO(uiLayout *layout, char *name, int icon, char *idname, IDProperty *properties, int context)
{
	uiTemplate *template= layout->templates.last;
	wmOperatorType *ot= WM_operatortype_find(idname);
	uiItemOp *opitem;

	if(!template)
		return;
	if(!ot) {
		ui_item_disabled(layout, idname);
		return;
	}

	opitem= MEM_callocN(sizeof(uiItemOp), "uiItemOp");

	ui_item_name(&opitem->item, name);
	opitem->item.icon= icon;
	opitem->item.type= ITEM_OPERATOR;
	opitem->item.slot= template->slot;

	opitem->ot= ot;
	opitem->properties= properties;
	opitem->context= context;

	BLI_addtail(&template->items, opitem);
}

static char *ui_menu_enumpropname(char *opname, char *propname, int retval)
{
	wmOperatorType *ot= WM_operatortype_find(opname);
	PointerRNA ptr;
	PropertyRNA *prop;

	if(!ot || !ot->srna)
		return "";
	
	RNA_pointer_create(NULL, ot->srna, NULL, &ptr);
	prop= RNA_struct_find_property(&ptr, propname);
	
	if(prop) {
		const EnumPropertyItem *item;
		int totitem, i;
		
		RNA_property_enum_items(&ptr, prop, &item, &totitem);
		
		for (i=0; i<totitem; i++) {
			if(item[i].value==retval)
				return (char*)item[i].name;
		}
	}

	return "";
}

void uiItemEnumO(uiLayout *layout, char *name, int icon, char *opname, char *propname, int value)
{
	PointerRNA ptr;

	WM_operator_properties_create(&ptr, opname);
	RNA_enum_set(&ptr, propname, value);

	if(!name)
		name= ui_menu_enumpropname(opname, propname, value);

	uiItemFullO(layout, name, icon, opname, ptr.data, layout->opcontext);
}

void uiItemsEnumO(uiLayout *layout, char *opname, char *propname)
{
	wmOperatorType *ot= WM_operatortype_find(opname);
	PointerRNA ptr;
	PropertyRNA *prop;

	if(!ot || !ot->srna) {
		ui_item_disabled(layout, opname);
		return;
	}

	RNA_pointer_create(NULL, ot->srna, NULL, &ptr);
	prop= RNA_struct_find_property(&ptr, propname);

	if(prop && RNA_property_type(prop) == PROP_ENUM) {
		const EnumPropertyItem *item;
		int totitem, i;

		RNA_property_enum_items(&ptr, prop, &item, &totitem);

		for(i=0; i<totitem; i++)
			uiItemEnumO(layout, NULL, 0, opname, propname, item[i].value);
	}
}

void uiItemBooleanO(uiLayout *layout, char *name, int icon, char *opname, char *propname, int value)
{
	PointerRNA ptr;

	WM_operator_properties_create(&ptr, opname);
	RNA_boolean_set(&ptr, propname, value);

	uiItemFullO(layout, name, icon, opname, ptr.data, layout->opcontext);
}

void uiItemIntO(uiLayout *layout, char *name, int icon, char *opname, char *propname, int value)
{
	PointerRNA ptr;

	WM_operator_properties_create(&ptr, opname);
	RNA_int_set(&ptr, propname, value);

	uiItemFullO(layout, name, icon, opname, ptr.data, layout->opcontext);
}

void uiItemFloatO(uiLayout *layout, char *name, int icon, char *opname, char *propname, float value)
{
	PointerRNA ptr;

	WM_operator_properties_create(&ptr, opname);
	RNA_float_set(&ptr, propname, value);

	uiItemFullO(layout, name, icon, opname, ptr.data, layout->opcontext);
}

void uiItemStringO(uiLayout *layout, char *name, int icon, char *opname, char *propname, char *value)
{
	PointerRNA ptr;

	WM_operator_properties_create(&ptr, opname);
	RNA_string_set(&ptr, propname, value);

	uiItemFullO(layout, name, icon, opname, ptr.data, layout->opcontext);
}

void uiItemO(uiLayout *layout, char *name, int icon, char *opname)
{
	uiItemFullO(layout, name, icon, opname, NULL, layout->opcontext);
}

/* RNA property items */
void uiItemFullR(uiLayout *layout, char *name, int icon, PointerRNA *ptr, PropertyRNA *prop, int index, int value, int expand)
{
	uiTemplate *template= layout->templates.last;
	uiItemRNA *rnaitem;

	if(!ptr->data || !prop)
		return;
	if(!template)
		return;

	rnaitem= MEM_callocN(sizeof(uiItemRNA), "uiItemRNA");

	ui_item_name(&rnaitem->item, name);
	rnaitem->item.icon= icon;
	rnaitem->item.type= ITEM_RNA_PROPERTY;
	rnaitem->item.slot= template->slot;

	rnaitem->ptr= *ptr;
	rnaitem->prop= prop;
	rnaitem->index= index;
	rnaitem->value= value;
	rnaitem->expand= expand;

	BLI_addtail(&template->items, rnaitem);
}

void uiItemR(uiLayout *layout, char *name, int icon, PointerRNA *ptr, char *propname, int expand)
{
	PropertyRNA *prop;

	if(!ptr->data || !propname)
		return;

	prop= RNA_struct_find_property(ptr, propname);

	if(!prop) {
		ui_item_disabled(layout, propname);
		printf("uiItemR: property not found: %s\n", propname);
		return;
	}
	
	uiItemFullR(layout, name, icon, ptr, prop, RNA_NO_INDEX, 0, expand);
}

void uiItemEnumR(uiLayout *layout, char *name, int icon, struct PointerRNA *ptr, char *propname, int value)
{
	PropertyRNA *prop;

	if(!ptr->data || !propname)
		return;

	prop= RNA_struct_find_property(ptr, propname);

	if(!prop) {
		ui_item_disabled(layout, propname);
		printf("uiItemEnumR: property not found: %s\n", propname);
		return;
	}
	
	uiItemFullR(layout, name, icon, ptr, prop, RNA_ENUM_VALUE, value, 0);
}

void uiItemsEnumR(uiLayout *layout, struct PointerRNA *ptr, char *propname)
{
	PropertyRNA *prop;

	prop= RNA_struct_find_property(ptr, propname);

	if(!prop) {
		ui_item_disabled(layout, propname);
		return;
	}

	if(RNA_property_type(prop) == PROP_ENUM) {
		const EnumPropertyItem *item;
		int totitem, i;

		RNA_property_enum_items(ptr, prop, &item, &totitem);

		for(i=0; i<totitem; i++)
			uiItemEnumR(layout, (char*)item[i].name, 0, ptr, propname, item[i].value);
	}
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

void uiItemM(uiLayout *layout, char *name, int icon, char *menuname)
{
	uiTemplate *template= layout->templates.last;
	uiItemMenu *menuitem;
	
	if(!template)
		return;
	if(!menuname)
		return;

	menuitem= MEM_callocN(sizeof(uiItemMenu), "uiItemMenu");

	ui_item_name(&menuitem->item, name);
	menuitem->item.icon= icon;
	menuitem->item.type= ITEM_MENU;
	menuitem->item.slot= template->slot;

	menuitem->func= ui_item_menutype_func;
	menuitem->menuname= menuname;

	BLI_addtail(&template->items, menuitem);
}

/* label item */
void uiItemL(uiLayout *layout, char *name, int icon)
{
	uiTemplate *template= layout->templates.last;
	uiItem *item;
	
	if(!template)
		return;

	item= MEM_callocN(sizeof(uiItem), "uiItem");

	ui_item_name(item, name);
	item->icon= icon;
	item->type= ITEM_LABEL;
	item->slot= template->slot;

	BLI_addtail(&template->items, item);
}

/* value item */
void uiItemV(uiLayout *layout, char *name, int icon, int argval)
{
	uiTemplate *template= layout->templates.last;
	uiItemValue *vitem;
	
	if(!template)
		return;

	vitem= MEM_callocN(sizeof(uiItemValue), "uiItemValue");

	vitem->item.name= name;
	vitem->item.icon= icon;
	vitem->item.type= ITEM_VALUE;
	vitem->item.slot= template->slot;
	vitem->argval= argval;

	BLI_addtail(&template->items, vitem);
}

/* separator item */
void uiItemS(uiLayout *layout)
{
	uiTemplate *template= layout->templates.last;
	uiItem *item;
	
	if(!template)
		return;

	item= MEM_callocN(sizeof(uiItem), "uiItem");

	item->type= ITEM_SEPARATOR;
	item->slot= template->slot;

	BLI_addtail(&template->items, item);
}

/* level items */
void uiItemMenuF(uiLayout *layout, char *name, int icon, uiMenuCreateFunc func)
{
	uiTemplate *template= layout->templates.last;
	uiItemMenu *menuitem;
	
	if(!template)
		return;
	if(!func)
		return;

	menuitem= MEM_callocN(sizeof(uiItemMenu), "uiItemMenu");

	if(!icon && layout->type == UI_LAYOUT_MENU)
		icon= ICON_RIGHTARROW_THIN,

	ui_item_name(&menuitem->item, name);
	menuitem->item.icon= icon;
	menuitem->item.type= ITEM_MENU;
	menuitem->item.slot= template->slot;

	menuitem->func= func;

	BLI_addtail(&template->items, menuitem);
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

	uiLayoutContext(layout, WM_OP_EXEC_REGION_WIN);
	uiItemsEnumO(layout, lvl->opname, lvl->propname);
}

void uiItemMenuEnumO(uiLayout *layout, char *name, int icon, char *opname, char *propname)
{
	wmOperatorType *ot= WM_operatortype_find(opname);
	uiTemplate *template= layout->templates.last;
	uiItemMenu *menuitem;
	MenuItemLevel *lvl;

	if(!ot || !ot->srna) {
		ui_item_disabled(layout, opname);
		return;
	}
	if(!template)
		return;

	menuitem= MEM_callocN(sizeof(uiItemMenu), "uiItemMenu");

	if(!icon && layout->type == UI_LAYOUT_MENU)
		icon= ICON_RIGHTARROW_THIN;
	if(!name)
		name= ot->name;

	ui_item_name(&menuitem->item, name);
	menuitem->item.icon= icon;
	menuitem->item.type= ITEM_MENU;
	menuitem->item.slot= template->slot;

	lvl= MEM_callocN(sizeof(MenuItemLevel), "MenuItemLevel");
	lvl->opname= opname;
	lvl->propname= propname;
	lvl->opcontext= layout->opcontext;

	menuitem->func= menu_item_enum_opname_menu;
	menuitem->argN= lvl;

	BLI_addtail(&template->items, menuitem);
}

static void menu_item_enum_rna_menu(bContext *C, uiLayout *layout, void *arg)
{
	MenuItemLevel *lvl= (MenuItemLevel*)(((uiBut*)arg)->func_argN);

	uiLayoutContext(layout, lvl->opcontext);
	uiItemsEnumR(layout, &lvl->rnapoin, lvl->propname);
}

void uiItemMenuEnumR(uiLayout *layout, char *name, int icon, struct PointerRNA *ptr, char *propname)
{
	uiTemplate *template= layout->templates.last;
	uiItemMenu *menuitem;
	MenuItemLevel *lvl;
	PropertyRNA *prop;
	
	if(!template)
		return;

	prop= RNA_struct_find_property(ptr, propname);
	if(!prop) {
		ui_item_disabled(layout, propname);
		return;
	}

	menuitem= MEM_callocN(sizeof(uiItemMenu), "uiItemMenu");

	if(!icon && layout->type == UI_LAYOUT_MENU)
		icon= ICON_RIGHTARROW_THIN;
	if(!name)
		name= (char*)RNA_property_ui_name(prop);

	ui_item_name(&menuitem->item, name);
	menuitem->item.icon= icon;
	menuitem->item.type= ITEM_MENU;
	menuitem->item.slot= template->slot;

	lvl= MEM_callocN(sizeof(MenuItemLevel), "MenuItemLevel");
	lvl->rnapoin= *ptr;
	lvl->propname= propname;
	lvl->opcontext= layout->opcontext;

	menuitem->func= menu_item_enum_rna_menu;
	menuitem->argN= lvl;

	BLI_addtail(&template->items, menuitem);
}

/**************************** Template ***************************/

/* single row layout */
static void ui_layout_row(uiLayout *layout, uiBlock *block, uiTemplate *template)
{
	uiStyle *style= layout->style;
	uiItem *item;
	int tot=0, totw= 0, maxh= 0, itemw, itemh, x, w;

	/* estimate total width of buttons */
	for(item=template->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh, UI_ITEM_VARY_Y);
		totw += itemw;
		maxh= MAX2(maxh, itemh);
		tot++;
	}

	if(totw == 0)
		return;
	
	/* create buttons starting from left */
	x= 0;
	w= layout->w;

	for(item=template->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh, UI_ITEM_VARY_Y);
		itemw= ui_item_fit(itemw, x, totw, w, (tot-1)*style->buttonspacex, !item->next, UI_FIT_EXPAND);

		ui_item_buts(layout, block, item, layout->x+x, layout->y-itemh, itemw, itemh);
		x += itemw+style->buttonspacex;
	}

	layout->y -= maxh;
}

/* multi-column layout */
static void ui_layout_column(uiLayout *layout, uiBlock *block, uiTemplate *template)
{
	uiStyle *style= layout->style;
	uiItem *item;
	int col, totcol= 0, x, y, miny, itemw, itemh, w;

	/* compute number of columns */
	for(item=template->items.first; item; item=item->next)
		totcol= MAX2(item->slot+1, totcol);
	
	if(totcol == 0)
		return;
	
	x= 0;
	miny= 0;
	w= layout->w;

	/* create column per column */
	for(col=0; col<totcol; col++) {
		y= 0;

		itemw= ui_item_fit(1, x, totcol, w, (totcol-1)*style->columnspace, col == totcol-1, UI_FIT_EXPAND);

		for(item=template->items.first; item; item=item->next) {
			if(item->slot != col)
				continue;

			ui_item_size(item, NULL, &itemh, UI_ITEM_VARY_Y);

			y -= itemh;
			ui_item_buts(layout, block, item, layout->x+x, layout->y+y, itemw, itemh);
			y -= style->buttonspacey;
		}

		x += itemw + style->columnspace;
		miny= MIN2(miny, y);
	}

	layout->y += miny;
}

/* multi-column layout, automatically flowing to the next */
static void ui_layout_column_flow(uiLayout *layout, uiBlock *block, uiTemplate *template)
{
	uiStyle *style= layout->style;
	uiTemplateFlow *flow= (uiTemplateFlow*)template;
	uiItem *item;
	int col, x, y, w, emh, emy, miny, itemw, itemh, maxw=0;
	int toth, totcol, totitem;

	/* compute max needed width and total height */
	toth= 0;
	totitem= 0;
	for(item=template->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh, UI_ITEM_VARY_Y);
		maxw= MAX2(maxw, itemw);
		toth += itemh;
		totitem++;
	}

	if(flow->number <= 0) {
		/* auto compute number of columns, not very good */
		if(maxw == 0)
			return;

		totcol= MAX2(layout->emw/maxw, 1);
		totcol= MIN2(totcol, totitem);
	}
	else
		totcol= flow->number;

	/* compute sizes */
	x= 0;
	y= 0;
	emy= 0;
	miny= 0;

	w= layout->w;
	emh= toth/totcol;

	/* create column per column */
	col= 0;
	for(item=template->items.first; item; item=item->next) {
		ui_item_size(item, NULL, &itemh, UI_ITEM_VARY_Y);
		itemw= ui_item_fit(1, x, totcol, w, (totcol-1)*style->columnspace, col == totcol-1, UI_FIT_EXPAND);
	
		y -= itemh;
		emy -= itemh;
		ui_item_buts(layout, block, item, layout->x+x, layout->y+y, itemw, itemh);
		y -= style->buttonspacey;
		miny= MIN2(miny, y);

		/* decide to go to next one */
		if(col < totcol-1 && emy <= -emh) {
			x += itemw + style->columnspace;
			y= 0;
			col++;
		}
	}

	layout->y += miny;
}

#if 0
/* left-right layout, with buttons aligned on both sides */
static void ui_layout_split(uiLayout *layout, uiBlock *block, uiTemplate *template)
{
	uiItem *item;
	int tot=0, totw= 0, maxh= 0, itemw, itemh, lx, rx, w;

	/* estimate total width of buttons */
	for(item=template->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh, UI_ITEM_VARY_Y);
		totw += itemw;
		maxh= MAX2(maxh, itemh);
		tot++;
	}

	if(totw == 0)
		return;
	
	/* create buttons starting from left and right */
	lx= 0;
	rx= 0;
	w= layout->w - style->buttonspacex*(tot-1) + style->buttonspacex;

	for(item=template->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh, UI_ITEM_VARY_Y);

		if(item->slot == UI_TSLOT_LR_LEFT) {
			itemw= ui_item_fit(itemw, lx, totw, w, 0, 0);
			ui_item_buts(layout, block, item, layout->x+lx, layout->y-itemh, itemw, itemh);
			lx += itemw + style->buttonspacex;
		}
		else {
			itemw= ui_item_fit(itemw, totw + rx, totw, w, 0, 0);
			rx -= itemw + style->buttonspacex;
			ui_item_buts(layout, block, item, layout->x+layout->w+rx, layout->y-itemh, itemw, itemh);
		}
	}

	layout->y -= maxh;
}
#endif

/* split in columns */
static void ui_layout_split(const bContext *C, uiLayout *layout, uiBlock *block, uiTemplate *template)
{
	uiStyle *style= layout->style;
	uiTemplateSplt *split= (uiTemplateSplt*)template;
	uiLayout *sublayout;
	int a, x, y, miny, w= layout->w, h= layout->h, splitw;

	x= 0;
	y= 0;
	miny= layout->y;

	for(a=0; a<split->number; a++) {
		sublayout= split->sublayout[a];

		splitw= ui_item_fit(1, x, split->number, w, (split->number-1)*style->columnspace, a == split->number-1, UI_FIT_EXPAND);
		sublayout->x= layout->x + x;
		sublayout->w= splitw;
		sublayout->y= layout->y;
		sublayout->h= h;

		sublayout->emw= layout->emw/split->number;
		sublayout->emh= layout->emh;

		/* do layout for elements in sublayout */
		ui_layout_end(C, block, sublayout, NULL, &y);
		miny= MIN2(y, miny);

		x += splitw + style->columnspace;
	}

	layout->y= miny;
}

/* element in a box layout */
static void ui_layout_box(const bContext *C, uiLayout *layout, uiBlock *block, uiTemplate *template)
{
	uiStyle *style= layout->style;
	uiTemplateBx *box= (uiTemplateBx*)template;
	int starty, startx, w= layout->w, h= layout->h;

	startx= layout->x;
	starty= layout->y;

	/* some extra padding */
	box->sublayout->x= layout->x + style->boxspace;
	box->sublayout->w= w - 2*style->boxspace;
	box->sublayout->y= layout->y - style->boxspace;
	box->sublayout->h= h;

	box->sublayout->emw= layout->emw;
	box->sublayout->emh= layout->emh;

	/* do layout for elements in sublayout */
	ui_layout_end(C, block, box->sublayout, NULL, &layout->y);

	/* roundbox around the sublayout */
	uiDefBut(block, ROUNDBOX, 0, "", startx, layout->y, w, starty - layout->y, NULL, 7.0, 0.0, 3, 20, "");
}

static void ui_layout_header_buttons(uiLayout *layout, uiBlock *block, uiTemplate *template)
{
	uiItem *item;
	int itemw, itemh;
	
	uiBlockBeginAlign(block);

	for(item=template->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh, UI_ITEM_VARY_X);
		ui_item_buts(layout, block, item, layout->x, layout->y, itemw, itemh);
		layout->x += itemw;
	}

	uiBlockEndAlign(block);
}

static void ui_layout_header(const bContext *C, uiLayout *layout, uiBlock *block, uiTemplate *template)
{
	ScrArea *sa= CTX_wm_area(C);

	layout->x= ED_area_header_standardbuttons(C, block, layout->y);

	if((sa->flag & HEADER_NO_PULLDOWN)==0) {
		uiBlockSetEmboss(block, UI_EMBOSSP);
		ui_layout_header_buttons(layout, block, template);
	}

	uiBlockSetEmboss(block, UI_EMBOSS);
}

static void header_id_cb(bContext *C, void *arg_template, void *arg_event)
{
	uiTemplateHeadID *idtemplate= (uiTemplateHeadID*)arg_template;
	PointerRNA idptr= RNA_property_pointer_get(&idtemplate->ptr, idtemplate->prop);
	ID *idtest, *id= idptr.data;
	ListBase *lb= wich_libbase(CTX_data_main(C), ID_TXT); // XXX
	int nr, event= GET_INT_FROM_POINTER(arg_event);
	
	if(event == UI_ID_BROWSE && idtemplate->browse == 32767)
		event= UI_ID_ADD_NEW;
	else if(event == UI_ID_BROWSE && idtemplate->browse == 32766)
		event= UI_ID_OPEN;

	switch(event) {
		case UI_ID_BROWSE: {
			if(id==0) id= lb->first;
			if(id==0) return;

			if(idtemplate->browse== -2) {
				/* XXX implement or find a replacement
				 * activate_databrowse((ID *)G.buts->lockpoin, GS(id->name), 0, B_MESHBROWSE, &idtemplate->browse, do_global_buttons); */
				return;
			}
			if(idtemplate->browse < 0)
				return;

			for(idtest=lb->first, nr=1; idtest; idtest=idtest->next, nr++) {
				if(nr==idtemplate->browse) {
					if(id == idtest)
						return;

					id= idtest;
					RNA_id_pointer_create(id, &idptr);
					RNA_property_pointer_set(&idtemplate->ptr, idtemplate->prop, idptr);
					RNA_property_update(C, &idtemplate->ptr, idtemplate->prop);
					/* XXX */

					break;
				}
			}
			break;
		}
#if 0
		case UI_ID_DELETE:
			id= NULL;
			break;
		case UI_ID_FAKE_USER:
			if(id) {
				if(id->flag & LIB_FAKEUSER) id->us++;
				else id->us--;
			}
			else return;
			break;
#endif
		case UI_ID_PIN:
			break;
		case UI_ID_ADD_NEW:
			WM_operator_name_call(C, idtemplate->newop, WM_OP_INVOKE_REGION_WIN, NULL);
			break;
		case UI_ID_OPEN:
			WM_operator_name_call(C, idtemplate->openop, WM_OP_INVOKE_REGION_WIN, NULL);
			break;
#if 0
		case UI_ID_ALONE:
			if(!id || id->us < 1)
				return;
			break;
		case UI_ID_LOCAL:
			if(!id || id->us < 1)
				return;
			break;
		case UI_ID_AUTO_NAME:
			break;
#endif
	}
}

static void ui_layout_header_id(const bContext *C, uiLayout *layout, uiBlock *block, uiTemplate *template)
{
	uiTemplateHeadID *duptemplate, *idtemplate= (uiTemplateHeadID*)template;
	uiBut *but;
	PointerRNA idptr= RNA_property_pointer_get(&idtemplate->ptr, idtemplate->prop);
	ListBase *lb= wich_libbase(CTX_data_main(C), ID_TXT); // XXX

	if(idtemplate->flag & UI_ID_BROWSE) {
		char *extrastr, *str;
		
		if((idtemplate->flag & UI_ID_ADD_NEW) && (idtemplate->flag && UI_ID_OPEN))
			extrastr= "OPEN NEW %x 32766 |ADD NEW %x 32767";
		else if(idtemplate->flag & UI_ID_ADD_NEW)
			extrastr= "ADD NEW %x 32767";
		else if(idtemplate->flag & UI_ID_OPEN)
			extrastr= "OPEN NEW %x 32766";
		else
			extrastr= NULL;

		duptemplate= MEM_dupallocN(idtemplate);
		IDnames_to_pupstring(&str, NULL, extrastr, lb, idptr.data, &duptemplate->browse);

		but= uiDefButS(block, MENU, 0, str, layout->x, layout->y, EM_UNIT_X, EM_UNIT_Y, &duptemplate->browse, 0, 0, 0, 0, "Browse existing choices, or add new");
		uiButSetNFunc(but, header_id_cb, duptemplate, SET_INT_IN_POINTER(UI_ID_BROWSE));
		layout->x+= EM_UNIT_X;
	
		MEM_freeN(str);
	}

	/* text button with name */
	if(idptr.data) {
		char name[64];

		text_idbutton(idptr.data, name);
		but= uiDefButR(block, TEX, 0, name, layout->x, layout->y, EM_UNIT_X*6, EM_UNIT_Y, &idptr, "name", -1, 0, 0, -1, -1, NULL);
		uiButSetNFunc(but, header_id_cb, MEM_dupallocN(idtemplate), SET_INT_IN_POINTER(UI_ID_RENAME));
		layout->x += EM_UNIT_X*6;

		/* delete button */
		if(idtemplate->flag & UI_ID_DELETE) {
			but= uiDefIconButO(block, BUT, idtemplate->unlinkop, WM_OP_EXEC_REGION_WIN, ICON_X, layout->x, layout->y, EM_UNIT_X, EM_UNIT_Y, NULL);
			layout->x += EM_UNIT_X;
		}
	}
}

void ui_template_free(uiTemplate *template)
{
	uiItem *item;
	int a;

	if(template->type == TEMPLATE_BOX) {
		uiTemplateBx *box= (uiTemplateBx*)template;
		ui_layout_free(box->sublayout);
	}
	if(template->type == TEMPLATE_SPLIT) {
		uiTemplateSplt *split= (uiTemplateSplt*)template;

		for(a=0; a<split->number; a++)
			ui_layout_free(split->sublayout[a]);
		MEM_freeN(split->sublayout);
	}

	for(item=template->items.first; item; item=item->next)
		ui_item_free(item);

	BLI_freelistN(&template->items);
}

/* template create functions */
void uiLayoutRow(uiLayout *layout)
{
	uiTemplate *template;

	template= MEM_callocN(sizeof(uiTemplate), "uiTemplate");
	template->type= TEMPLATE_ROW;

	BLI_addtail(&layout->templates, template);
}

void uiLayoutColumn(uiLayout *layout)
{
	uiTemplate *template;

	template= MEM_callocN(sizeof(uiTemplate), "uiTemplate");
	template->type= TEMPLATE_COLUMN;

	BLI_addtail(&layout->templates, template);
}

void uiLayoutColumnFlow(uiLayout *layout, int number)
{
	uiTemplateFlow *flow;

	flow= MEM_callocN(sizeof(uiTemplateFlow), "uiTemplateFlow");
	flow->template.type= TEMPLATE_COLUMN_FLOW;
	flow->number= number;
	BLI_addtail(&layout->templates, flow);
}

uiLayout *uiLayoutBox(uiLayout *layout)
{
	uiTemplateBx *box;

	box= MEM_callocN(sizeof(uiTemplateBx), "uiTemplateBx");
	box->template.type= TEMPLATE_BOX;
	box->sublayout= uiLayoutBegin(layout->dir, layout->type, 0, 0, 0, 0, layout->style);
	BLI_addtail(&layout->templates, box);

	return box->sublayout;
}

void uiLayoutSplit(uiLayout *layout, int number, int lr)
{
	uiTemplateSplt *split;
	int a;

	split= MEM_callocN(sizeof(uiTemplateSplt), "uiTemplateSplt");
	split->template.type= TEMPLATE_SPLIT;
	split->number= number;
	split->lr= lr;
	split->sublayout= MEM_callocN(sizeof(uiLayout*)*number, "uiTemplateSpltSub");

	for(a=0; a<number; a++)
		split->sublayout[a]= uiLayoutBegin(layout->dir, layout->type, 0, 0, 0, 0, layout->style);

	BLI_addtail(&layout->templates, split);
}

uiLayout *uiLayoutSub(uiLayout *layout, int n)
{
	uiTemplate *template= layout->templates.last;

	if(template) {
		switch(template->type) {
			case TEMPLATE_SPLIT:
				if(n >= 0 && n < ((uiTemplateSplt*)template)->number)
					return ((uiTemplateSplt*)template)->sublayout[n];
				break;
			case TEMPLATE_BOX:
				return ((uiTemplateBx*)template)->sublayout;
				break;
			default:
				break;
		}
	}

	return NULL;
}

void uiTemplateHeader(uiLayout *layout)
{
	uiTemplate *template;

	template= MEM_callocN(sizeof(uiTemplate), "uiTemplate");
	template->type= TEMPLATE_HEADER;

	BLI_addtail(&layout->templates, template);
}

void uiTemplateHeaderID(uiLayout *layout, PointerRNA *ptr, char *propname, char *newop, char *openop, char *unlinkop)
{
	uiTemplateHeadID *idtemplate;
	PropertyRNA *prop;

	if(!ptr->data)
		return;

	prop= RNA_struct_find_property(ptr, propname);

	if(!prop) {
		printf("uiTemplateHeaderID: property not found: %s\n", propname);
		return;
	}

	idtemplate= MEM_callocN(sizeof(uiTemplateHeadID), "uiTemplateHeadID");
	idtemplate->template.type= TEMPLATE_HEADER_ID;
	idtemplate->ptr= *ptr;
	idtemplate->prop= prop;
	idtemplate->flag= UI_ID_BROWSE|UI_ID_RENAME;

	if(newop) {
		idtemplate->flag |= UI_ID_ADD_NEW;
		idtemplate->newop= newop;
	}
	if(openop) {
		idtemplate->flag |= UI_ID_OPEN;
		idtemplate->openop= openop;
	}
	if(unlinkop) {
		idtemplate->flag |= UI_ID_DELETE;
		idtemplate->unlinkop= unlinkop;
	}

	BLI_addtail(&layout->templates, idtemplate);
}

void uiTemplateSlot(uiLayout *layout, int slot)
{
	uiTemplate *template= layout->templates.last;

	if(template)
		template->slot= slot;
}

/********************** Layout *******************/

static void ui_layout_init_items(const bContext *C, uiLayout *layout)
{
	ARegion *ar= CTX_wm_region(C);
	MenuType *mt;
	uiTemplate *template;
	uiItem *item;
	uiItemMenu *menuitem;
	uiItemRNA *rnaitem;
	uiItemOp *opitem;
	PropertyType type;

	for(template=layout->templates.first; template; template=template->next) {
		for(item=template->items.first; item; item=item->next) {
			/* initialize buttons names */
			if(item->type == ITEM_MENU) {
				menuitem= (uiItemMenu*)item;

				if(menuitem->menuname) {
					for(mt=ar->type->menutypes.first; mt; mt=mt->next) {
						if(strcmp(menuitem->menuname, mt->idname) == 0) {
							menuitem->arg= mt;
							ui_item_name(item, mt->label);
							break;
						}
					}
				}
			}
			else if(item->type == ITEM_RNA_PROPERTY) {
				rnaitem= (uiItemRNA*)item;
				type= RNA_property_type(rnaitem->prop);

				ui_item_name(item, (char*)RNA_property_ui_name(rnaitem->prop));

				if(ELEM4(type, PROP_INT, PROP_FLOAT, PROP_STRING, PROP_ENUM))
					ui_item_name_add_colon(item);
			}
			else if(item->type == ITEM_OPERATOR) {
				opitem= (uiItemOp*)item;
				ui_item_name(item, opitem->ot->name);
			}

			ui_item_name(item, "");

			/* initialize icons */
			if(layout->type == UI_LAYOUT_MENU) {
				if(item->type == ITEM_RNA_PROPERTY) {
					rnaitem= (uiItemRNA*)item;
					type= RNA_property_type(rnaitem->prop);

					if(type == PROP_BOOLEAN)
						item->icon= (RNA_property_boolean_get(&rnaitem->ptr, rnaitem->prop))? ICON_CHECKBOX_HLT: ICON_CHECKBOX_DEHLT;
					else if(type == PROP_ENUM && rnaitem->index == RNA_ENUM_VALUE)
						item->icon= (RNA_property_enum_get(&rnaitem->ptr, rnaitem->prop) == rnaitem->value)? ICON_CHECKBOX_HLT: ICON_CHECKBOX_DEHLT; 
				}

				if(!item->icon)
					item->icon= ICON_BLANK1;
			}
		}
	}
}

static void ui_layout_templates(const bContext *C, uiBlock *block, uiLayout *layout)
{
	uiStyle *style= layout->style;
	uiTemplate *template;

	ui_layout_init_items(C, layout);

	if(layout->dir == UI_LAYOUT_HORIZONTAL) {
		for(template=layout->templates.first; template; template=template->next) {
			switch(template->type) {
				case TEMPLATE_HEADER:
					ui_layout_header(C, layout, block, template);
					break;
				case TEMPLATE_HEADER_ID:
					ui_layout_header_id(C, layout, block, template);
					break;
				default:
					ui_layout_header_buttons(layout, block, template);
					break;
			}

			layout->x += style->templatespace;
		}
	}
	else {
		for(template=layout->templates.first; template; template=template->next) {
			switch(template->type) {
				case TEMPLATE_ROW:
					ui_layout_row(layout, block, template);
					break;
				case TEMPLATE_COLUMN_FLOW:
					ui_layout_column_flow(layout, block, template);
					break;
				case TEMPLATE_SPLIT:
					ui_layout_split(C, layout, block, template);
					break;
				case TEMPLATE_BOX:
					ui_layout_box(C, layout, block, template);
					break;
				case TEMPLATE_COLUMN:
				default:
					ui_layout_column(layout, block, template);
					break;
			}

			layout->y -= style->templatespace;
		}
	}
}

void ui_layout_end(const bContext *C, uiBlock *block, uiLayout *layout, int *x, int *y)
{
	if(layout->handlefunc)
		uiBlockSetButmFunc(block, layout->handlefunc, layout->argv);

	ui_layout_templates(C, block, layout);

	if(x) *x= layout->x;
	if(y) *y= layout->y;
	
}

void ui_layout_free(uiLayout *layout)
{
	uiTemplate *template;

	for(template=layout->templates.first; template; template=template->next)
		ui_template_free(template);

	BLI_freelistN(&layout->templates);
	MEM_freeN(layout);
}

uiLayout *uiLayoutBegin(int dir, int type, int x, int y, int size, int em, uiStyle *style)
{
	uiLayout *layout;

	layout= MEM_callocN(sizeof(uiLayout), "uiLayout");
	layout->opcontext= WM_OP_INVOKE_REGION_WIN;
	layout->dir= dir;
	layout->type= type;
	layout->x= x;
	layout->y= y;
	layout->style= style;

	if(dir == UI_LAYOUT_HORIZONTAL) {
		layout->h= size;
		layout->emh= em*EM_UNIT_Y;
	}
	else {
		layout->w= size;
		layout->emw= em*EM_UNIT_X;
	}

	return layout;
}

void uiLayoutContext(uiLayout *layout, int opcontext)
{
	layout->opcontext= opcontext;
}

void uiLayoutFunc(uiLayout *layout, uiMenuHandleFunc handlefunc, void *argv)
{
	layout->handlefunc= handlefunc;
	layout->argv= argv;
}

void uiLayoutEnd(const bContext *C, uiBlock *block, uiLayout *layout, int *x, int *y)
{
	ui_layout_end(C, block, layout, x, y);
	ui_layout_free(layout);
}

/************************ Utilities ************************/

void uiRegionPanelLayout(const bContext *C, ARegion *ar, int vertical, char *context)
{
	uiStyle *style= U.uistyles.first;
	uiBlock *block;
	PanelType *pt;
	Panel *panel;
	float col[3];
	int xco, yco, x=PNL_DIST, y=-PNL_HEADER-PNL_DIST, w, em;

	// XXX this only hides cruft

	/* clear */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	/* set view2d view matrix for scrolling (without scrollers) */
	UI_view2d_view_ortho(C, &ar->v2d);
	
	uiBeginPanels(C, ar);

	for(pt= ar->type->paneltypes.first; pt; pt= pt->next) {
		if(context)
			if(!pt->context || strcmp(context, pt->context) != 0)
				continue;

		if(pt->draw && (!pt->poll || pt->poll(C, pt))) {
			block= uiBeginBlock(C, ar, pt->idname, UI_EMBOSS);
			panel= uiBeginPanel(ar, block, pt);

			if(panel) {
				if(vertical) {
					w= (ar->type->minsizex)? ar->type->minsizex-12: block->aspect*ar->winx-12;
					em= (ar->type->minsizex)? 10: 20;
				}
				else {
					w= (ar->type->minsizex)? ar->type->minsizex-12: UI_PANEL_WIDTH-12;
					em= (ar->type->minsizex)? 10: 20;
				}

				panel->type= pt;
				panel->layout= uiLayoutBegin(UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, PNL_SAFETY, 0, w-2*PNL_SAFETY, em, style);

				pt->draw(C, panel);

				uiLayoutEnd(C, block, panel->layout, &xco, &yco);
				panel->layout= NULL;
				uiEndPanel(block, w, -yco + 12);
			}
			else {
				w= PNL_HEADER;
				yco= PNL_HEADER;
			}

			uiEndBlock(C, block);

			if(vertical)
				y += yco+PNL_DIST;
			else
				x += w+PNL_DIST;
		}
	}

	uiEndPanels(C, ar);
	
	/* restore view matrix? */
	UI_view2d_view_restore(C);
}

void uiRegionHeaderLayout(const bContext *C, ARegion *ar)
{
	uiStyle *style= U.uistyles.first;
	uiBlock *block;
	uiLayout *layout;
	HeaderType *ht;
	Header header = {0};
	float col[3];
	int xco, yco;

	// XXX this only hides cruft
	
	/* clear */
	if(ED_screen_area_active(C))
		UI_GetThemeColor3fv(TH_HEADER, col);
	else
		UI_GetThemeColor3fv(TH_HEADERDESEL, col);
	
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	/* set view2d view matrix for scrolling (without scrollers) */
	UI_view2d_view_ortho(C, &ar->v2d);

	xco= 8;
	yco= 3;

	/* draw all headers types */
	for(ht= ar->type->headertypes.first; ht; ht= ht->next) {
		block= uiBeginBlock(C, ar, "header buttons", UI_EMBOSS);
		layout= uiLayoutBegin(UI_LAYOUT_HORIZONTAL, UI_LAYOUT_HEADER, xco, yco, 24, 1, style);

		if(ht->draw) {
			header.type= ht;
			header.layout= layout;
			ht->draw(C, &header);
		}

		uiLayoutEnd(C, block, layout, &xco, &yco);
		uiEndBlock(C, block);
		uiDrawBlock(C, block);
	}

	/* always as last  */
	UI_view2d_totRect_set(&ar->v2d, xco+XIC+80, ar->v2d.tot.ymax-ar->v2d.tot.ymin);

	/* restore view matrix? */
	UI_view2d_view_restore(C);
}

