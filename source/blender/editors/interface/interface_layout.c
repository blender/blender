
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
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

#define COLUMN_SPACE	5
#define TEMPLATE_SPACE	5
#define BOX_SPACE		5
#define BUTTON_SPACE_X	5
#define BUTTON_SPACE_Y	2

#define RNA_NO_INDEX	-1

#define EM_UNIT_X		XIC
#define EM_UNIT_Y		YIC

/* Item */

typedef enum uiItemType {
	ITEM_OPERATOR,
	ITEM_RNA_PROPERTY,
	ITEM_MENU,
	ITEM_LABEL
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
	int icon;
} uiItem;

typedef struct uiItemRNA {
	uiItem item;

	PointerRNA ptr;
	PropertyRNA *prop;
	int index;
	int expand;
} uiItemRNA;

typedef struct uiItemOp {
	uiItem item;

	wmOperatorType *ot;
	IDProperty *properties;
	int context;
} uiItemOp;

typedef struct uiItemLMenu {
	uiItem item;

	uiMenuCreateFunc func;
} uiItemLMenu;

/* Template */

typedef enum uiTemplateType {
	TEMPLATE_ROW,
	TEMPLATE_COLUMN,
	TEMPLATE_COLUMN_FLOW,
	TEMPLATE_SPLIT,
	TEMPLATE_BOX,

	TEMPLATE_HEADER_MENUS,
	TEMPLATE_HEADER_BUTTONS,
	TEMPLATE_HEADER_ID
} uiTemplateType;

typedef struct uiTemplate {
	struct uiTemplate *next, *prev;
	uiTemplateType type;

	ListBase items;
	int color, slot;
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
	char *propname;
	int flag;
	uiIDPoinFunc func;
} uiTemplateHeadID;

/* Layout */

struct uiLayout {
	ListBase templates;
	int opcontext;
	int dir;
	int x, y, w, h;
	int emw, emh;
};

void ui_layout_free(uiLayout *layout);
void ui_layout_end(const bContext *C, uiBlock *block, uiLayout *layout, int *x, int *y);

/************************** Item ***************************/

#define UI_FIT_EXPAND 1

static int ui_item_fit(int item, int pos, int all, int available, int last, int flag)
{
	if(all > available) {
		/* contents is bigger than available space */
		if(last)
			return available-pos;
		else
			return (item*available)/all;
	}
	else {
		/* contents is smaller or equal to available space */
		if(flag & UI_FIT_EXPAND) {
			if(last)
				return available-pos;
			else
				return (item*available)/all;
		}
		else
			return item;
	}
}

/* create buttons for an item with an RNA array */
static void ui_item_array(uiBlock *block, uiItemRNA *rnaitem, int len, int x, int y, int w, int h)
{
	PropertyType type;
	PropertySubType subtype;
	char *name;
	int a;

	/* retrieve type and subtype */
	type= RNA_property_type(&rnaitem->ptr, rnaitem->prop);
	subtype= RNA_property_subtype(&rnaitem->ptr, rnaitem->prop);

	/* create label */
	if(rnaitem->item.name)
		name= (char*)rnaitem->item.name;
	else
		name= (char*)RNA_property_ui_name(&rnaitem->ptr, rnaitem->prop);

	if(strcmp(name, "") != 0)
		uiDefBut(block, LABEL, 0, name, x, y + h - EM_UNIT_Y, w, EM_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");

	/* create buttons */
	uiBlockBeginAlign(block);

	if(type == PROP_BOOLEAN && len == 20) {
		/* special check for layer layout */
		int butw, buth;

		butw= ui_item_fit(EM_UNIT_X, 0, EM_UNIT_X*10 + BUTTON_SPACE_X, w, 0, UI_FIT_EXPAND);
		buth= MIN2(EM_UNIT_Y, butw);

		y += 2*(EM_UNIT_Y - buth);

		uiBlockBeginAlign(block);
		for(a=0; a<5; a++)
			uiDefAutoButR(block, &rnaitem->ptr, rnaitem->prop, a, "", ICON_BLANK1, x + butw*a, y+buth, butw, buth);
		for(a=0; a<5; a++)
			uiDefAutoButR(block, &rnaitem->ptr, rnaitem->prop, a+10, "", ICON_BLANK1, x + butw*a, y, butw, buth);
		uiBlockEndAlign(block);

		x += 5*butw + BUTTON_SPACE_X;

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
	
	propname= RNA_property_identifier(&rnaitem->ptr, rnaitem->prop);
	RNA_property_enum_items(&rnaitem->ptr, rnaitem->prop, &item, &totitem);

	uiBlockBeginAlign(block);
	pos= 0;
	for(a=0; a<totitem; a++) {
		itemw= ui_item_fit(1, pos, totitem, w, a == totitem-1, UI_FIT_EXPAND);
		uiDefButR(block, ROW, 0, NULL, x+pos, y, itemw, h, &rnaitem->ptr, propname, -1, 0, item[a].value, -1, -1, NULL);
		pos += itemw;
	}
	uiBlockEndAlign(block);
}

/* create label + button for RNA property */
static void ui_item_with_label(uiBlock *block, uiItemRNA *rnaitem, int x, int y, int w, int h)
{
	char *name;

	if(rnaitem->item.name)
		name= (char*)rnaitem->item.name;
	else
		name= (char*)RNA_property_ui_name(&rnaitem->ptr, rnaitem->prop);
	
	if(strcmp(name, "") != 0) {
		w= w/2;
		uiDefBut(block, LABEL, 0, name, x, y, w, h, NULL, 0.0, 0.0, 0, 0, "");
		x += w;
	}

	uiDefAutoButR(block, &rnaitem->ptr, rnaitem->prop, rnaitem->index, "", rnaitem->item.icon, x, y, w, h);
}

/* create buttons for an arbitrary item */
static void ui_item_buts(uiBlock *block, uiItem *item, int x, int y, int w, int h)
{
	if(item->type == ITEM_RNA_PROPERTY) {
		/* RNA property */
		uiItemRNA *rnaitem= (uiItemRNA*)item;
		PropertyType type;
		int len;
		
		/* retrieve info */
		type= RNA_property_type(&rnaitem->ptr, rnaitem->prop);
		len= RNA_property_array_length(&rnaitem->ptr, rnaitem->prop);

		/* array property */
		if(rnaitem->index == RNA_NO_INDEX && len > 0)
			ui_item_array(block, rnaitem, len, x, y, w, h);
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

		if(item->icon && item->name)
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
		uiItemLMenu *menuitem= (uiItemLMenu*)item;

		uiDefMenuBut(block, menuitem->func, NULL, (char*)item->name, x, y-2, w-3, h+4, "");
	}
	else if(item->type == ITEM_LABEL) {
		/* label */

		if(item->icon && item->name)
			uiDefIconTextBut(block, LABEL, 0, item->icon, (char*)item->name, x, y, w, h, NULL, 0.0, 0.0, 0, 0, "");
		else if(item->icon)
			uiDefIconBut(block, LABEL, 0, item->icon, x, y, w, h, NULL, 0.0, 0.0, 0, 0, "");
		else if((char*)item->name)
			uiDefBut(block, LABEL, 0, (char*)item->name, x, y, w, h, NULL, 0.0, 0.0, 0, 0, "");
	}
	else {
		/* separator */
		uiDefBut(block, SEPR, 0, "", x, y, w, h, NULL, 0.0, 0.0, 0, 0, "");
	}
}

/* estimated size of text + icon */
static int ui_text_icon_width(char *name, int icon)
{
	if(icon && name && strcmp(name, "") == 0)
		return EM_UNIT_X; /* icon only */
	else if(icon)
		return 10*EM_UNIT_X; /* icon + text */
	else
		return 10*EM_UNIT_X; /* text only */
}

/* estimated size of an item */
static void ui_item_size(uiItem *item, int *r_w, int *r_h)
{
	int w, h;

	if(item->type == ITEM_RNA_PROPERTY) {
		/* RNA property */
		uiItemRNA *rnaitem= (uiItemRNA*)item;
		PropertyType type;
		PropertySubType subtype;
		int len;

		w= ui_text_icon_width(item->name, item->icon);
		h= EM_UNIT_Y;

		/* arbitrary extended width by type */
		type= RNA_property_type(&rnaitem->ptr, rnaitem->prop);
		subtype= RNA_property_subtype(&rnaitem->ptr, rnaitem->prop);
		len= RNA_property_array_length(&rnaitem->ptr, rnaitem->prop);

		if(type == PROP_STRING)
			w += 10*EM_UNIT_X;

		/* increase height for arrays */
		if(rnaitem->index == RNA_NO_INDEX && len > 0) {
			if(item->name && strcmp(item->name, "") == 0 && item->icon == 0)
				h= 0;

			if(type == PROP_BOOLEAN && len == 20)
				h += 2*EM_UNIT_Y;
			else if(subtype == PROP_MATRIX)
				h += ceil(sqrt(len))*EM_UNIT_Y;
			else
				h += len*EM_UNIT_Y;
		}
	}
	else {
		/* other */
		w= ui_text_icon_width(item->name, item->icon);
		h= EM_UNIT_Y;
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

/* operator items */
void uiItemFullO(uiLayout *layout, char *name, int icon, char *idname, IDProperty *properties, int context)
{
	uiTemplate *template= layout->templates.last;
	wmOperatorType *ot= WM_operatortype_find(idname);
	uiItemOp *opitem;

	if(!template)
		return;
	if(!ot)
		return;

	opitem= MEM_callocN(sizeof(uiItemOp), "uiItemOp");

	opitem->item.name= name;
	opitem->item.icon= icon;
	opitem->item.type= ITEM_OPERATOR;
	opitem->item.slot= template->slot;

	opitem->ot= ot;
	opitem->properties= properties;
	opitem->context= context;

	BLI_addtail(&template->items, opitem);
}

void uiItemEnumO(uiLayout *layout, char *name, int icon, char *opname, char *propname, int value)
{
	PointerRNA ptr;

	WM_operator_properties_create(&ptr, opname);
	RNA_enum_set(&ptr, propname, value);

	uiItemFullO(layout, name, icon, opname, ptr.data, layout->opcontext);
}

void uiItemsEnumO(uiLayout *layout, char *opname, char *propname)
{
	wmOperatorType *ot= WM_operatortype_find(opname);
	PointerRNA ptr;
	PropertyRNA *prop;

	if(!ot || !ot->srna)
		return;

	RNA_pointer_create(NULL, ot->srna, NULL, &ptr);
	prop= RNA_struct_find_property(&ptr, propname);

	if(prop && RNA_property_type(&ptr, prop) == PROP_ENUM) {
		const EnumPropertyItem *item;
		int totitem, i;

		RNA_property_enum_items(&ptr, prop, &item, &totitem);

		for(i=0; i<totitem; i++)
			uiItemEnumO(layout, "", 0, opname, propname, item[i].value);
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
void uiItemFullR(uiLayout *layout, char *name, int icon, PointerRNA *ptr, PropertyRNA *prop, int index, int expand)
{
	uiTemplate *template= layout->templates.last;
	uiItemRNA *rnaitem;

	if(!ptr->data || !prop)
		return;
	if(!template)
		return;

	rnaitem= MEM_callocN(sizeof(uiItemRNA), "uiItemRNA");

	rnaitem->item.name= name;
	rnaitem->item.icon= icon;
	rnaitem->item.type= ITEM_RNA_PROPERTY;
	rnaitem->item.slot= template->slot;

	rnaitem->ptr= *ptr;
	rnaitem->prop= prop;
	rnaitem->index= index;
	rnaitem->expand= expand;

	BLI_addtail(&template->items, rnaitem);
}

void uiItemR(uiLayout *layout, char *name, int icon, PointerRNA *ptr, char *propname, int expand)
{
	PropertyRNA *prop;

	prop= RNA_struct_find_property(ptr, propname);

	if(!ptr->data)
		return;
	if(!prop) {
		printf("uiItemR: property not found: %s\n",propname);
		return;
	}
	
	uiItemFullR(layout, name, icon, ptr, prop, RNA_NO_INDEX, expand);
}

/* menu item */
void uiItemM(uiLayout *layout, char *name, int icon, uiMenuCreateFunc func)
{
	uiTemplate *template= layout->templates.last;
	uiItemLMenu *menuitem;
	
	if(!template)
		return;

	menuitem= MEM_callocN(sizeof(uiItemLMenu), "uiItemLMenu");

	menuitem->item.name= name;
	menuitem->item.icon= icon;
	menuitem->item.type= ITEM_MENU;
	menuitem->item.slot= template->slot;

	menuitem->func= func;

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

	item->name= name;
	item->icon= icon;
	item->type= ITEM_LABEL;
	item->slot= template->slot;

	BLI_addtail(&template->items, item);
}

/**************************** Template ***************************/

/* single row layout */
static void ui_layout_row(uiLayout *layout, uiBlock *block, uiTemplate *template)
{
	uiItem *item;
	int tot=0, totw= 0, maxh= 0, itemw, itemh, x, w;

	/* estimate total width of buttons */
	for(item=template->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh);
		totw += itemw;
		maxh= MAX2(maxh, itemh);
		tot++;
	}

	if(totw == 0)
		return;
	
	/* create buttons starting from left */
	x= 0;
	w= layout->w - (tot-1)*BUTTON_SPACE_X;

	for(item=template->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh);
		itemw= ui_item_fit(itemw, x, totw, w, !item->next, UI_FIT_EXPAND);

		ui_item_buts(block, item, layout->x+x, layout->y-itemh, itemw, itemh);
		x += itemw+BUTTON_SPACE_X;
	}

	layout->y -= maxh;
}

/* multi-column layout */
static void ui_layout_column(uiLayout *layout, uiBlock *block, uiTemplate *template)
{
	uiItem *item;
	int col, totcol= 0, x, y, miny, itemw, itemh, w;

	/* compute number of columns */
	for(item=template->items.first; item; item=item->next)
		totcol= MAX2(item->slot+1, totcol);
	
	if(totcol == 0)
		return;
	
	x= 0;
	miny= 0;
	w= layout->w - (totcol-1)*COLUMN_SPACE;

	/* create column per column */
	for(col=0; col<totcol; col++) {
		y= 0;

		itemw= ui_item_fit(1, x, totcol, w, col == totcol-1, UI_FIT_EXPAND);

		for(item=template->items.first; item; item=item->next) {
			if(item->slot != col)
				continue;

			ui_item_size(item, NULL, &itemh);

			y -= itemh;
			ui_item_buts(block, item, layout->x+x, layout->y+y, itemw, itemh);
			y -= BUTTON_SPACE_Y;
		}

		x += itemw + COLUMN_SPACE;
		miny= MIN2(miny, y);
	}

	layout->y += miny;
}

/* multi-column layout, automatically flowing to the next */
static void ui_layout_column_flow(uiLayout *layout, uiBlock *block, uiTemplate *template)
{
	uiTemplateFlow *flow= (uiTemplateFlow*)template;
	uiItem *item;
	int col, x, y, w, emh, emy, miny, itemw, itemh, maxw=0;
	int toth, totcol, totitem;

	/* compute max needed width and total height */
	toth= 0;
	totitem= 0;
	for(item=template->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh);
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

	w= layout->w - totcol*(COLUMN_SPACE);
	emh= toth/totcol;

	/* create column per column */
	col= 0;
	for(item=template->items.first; item; item=item->next) {
		ui_item_size(item, NULL, &itemh);
		itemw= ui_item_fit(1, x, totcol, w, col == totcol-1, UI_FIT_EXPAND);
	
		y -= itemh;
		emy -= itemh;
		ui_item_buts(block, item, layout->x+x, layout->y+y, itemw, itemh);
		y -= BUTTON_SPACE_Y;
		miny= MIN2(miny, y);

		/* decide to go to next one */
		if(col < totcol-1 && emy <= -emh) {
			x += itemw + COLUMN_SPACE;
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
		ui_item_size(item, &itemw, &itemh);
		totw += itemw;
		maxh= MAX2(maxh, itemh);
		tot++;
	}

	if(totw == 0)
		return;
	
	/* create buttons starting from left and right */
	lx= 0;
	rx= 0;
	w= layout->w - BUTTON_SPACE_X*(tot-1) + BUTTON_SPACE_X;

	for(item=template->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh);

		if(item->slot == UI_TSLOT_LR_LEFT) {
			itemw= ui_item_fit(itemw, lx, totw, w, 0, 0);
			ui_item_buts(block, item, layout->x+lx, layout->y-itemh, itemw, itemh);
			lx += itemw + BUTTON_SPACE_X;
		}
		else {
			itemw= ui_item_fit(itemw, totw + rx, totw, w, 0, 0);
			rx -= itemw + BUTTON_SPACE_X;
			ui_item_buts(block, item, layout->x+layout->w+rx, layout->y-itemh, itemw, itemh);
		}
	}

	layout->y -= maxh;
}
#endif

/* split in columns */
static void ui_layout_split(const bContext *C, uiLayout *layout, uiBlock *block, uiTemplate *template)
{
	uiTemplateSplt *split= (uiTemplateSplt*)template;
	uiLayout *sublayout;
	int a, x, y, miny, w= layout->w, h= layout->h, splitw;

	x= 0;
	y= 0;
	miny= layout->y;

	for(a=0; a<split->number; a++) {
		sublayout= split->sublayout[a];

		splitw= ui_item_fit(1, x, split->number, w, a == split->number-1, UI_FIT_EXPAND);
		sublayout->x= layout->x + x;
		sublayout->w= splitw;
		sublayout->y= layout->y;
		sublayout->h= h;

		sublayout->emw= layout->emw/split->number;
		sublayout->emh= layout->emh;

		/* do layout for elements in sublayout */
		ui_layout_end(C, block, sublayout, NULL, &y);
		miny= MIN2(y, miny);

		x += splitw + COLUMN_SPACE;
	}

	layout->y= miny;
}

/* element in a box layout */
static void ui_layout_box(const bContext *C, uiLayout *layout, uiBlock *block, uiTemplate *template)
{
	uiTemplateBx *box= (uiTemplateBx*)template;
	int starty, startx, w= layout->w, h= layout->h;

	startx= layout->x;
	starty= layout->y;

	/* some extra padding */
	box->sublayout->x= layout->x + BOX_SPACE;
	box->sublayout->w= w - 2*BOX_SPACE;
	box->sublayout->y= layout->y - BOX_SPACE;
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
		ui_item_size(item, &itemw, &itemh);
		ui_item_buts(block, item, layout->x, layout->y, itemw, itemh);
		layout->x += itemw;
	}

	uiBlockEndAlign(block);
}

static void ui_layout_header_menus(const bContext *C, uiLayout *layout, uiBlock *block, uiTemplate *template)
{
	ScrArea *sa= CTX_wm_area(C);

	layout->x= ED_area_header_standardbuttons(C, block, layout->y);

	if((sa->flag & HEADER_NO_PULLDOWN)==0) {
		uiBlockSetEmboss(block, UI_EMBOSSP);
		ui_layout_header_buttons(layout, block, template);
	}

	uiBlockSetEmboss(block, UI_EMBOSS);
}

static void ui_layout_header_id(const bContext *C, uiLayout *layout, uiBlock *block, uiTemplate *template)
{
	uiTemplateHeadID *idtemplate= (uiTemplateHeadID*)template;
	PointerRNA idptr;

	idptr= RNA_pointer_get(&idtemplate->ptr, idtemplate->propname);

	layout->x= uiDefIDPoinButs(block, CTX_data_main(C), NULL, (ID*)idptr.data, ID_TXT, NULL,
		layout->x, layout->y, idtemplate->func,
		UI_ID_BROWSE|UI_ID_RENAME|UI_ID_ADD_NEW|UI_ID_OPEN|UI_ID_DELETE);
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
	box->sublayout= uiLayoutBegin(layout->dir, 0, 0, 0, 0);
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
		split->sublayout[a]= uiLayoutBegin(layout->dir, 0, 0, 0, 0);

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

void uiTemplateHeaderMenus(uiLayout *layout)
{
	uiTemplate *template;

	template= MEM_callocN(sizeof(uiTemplate), "uiTemplate");
	template->type= TEMPLATE_HEADER_MENUS;

	BLI_addtail(&layout->templates, template);
}

void uiTemplateHeaderButtons(uiLayout *layout)
{
	uiTemplate *template;

	template= MEM_callocN(sizeof(uiTemplate), "uiTemplate");
	template->type= TEMPLATE_HEADER_BUTTONS;

	BLI_addtail(&layout->templates, template);
}

void uiTemplateHeaderID(uiLayout *layout, PointerRNA *ptr, char *propname, int flag, uiIDPoinFunc func)
{
	uiTemplateHeadID *idtemplate;

	idtemplate= MEM_callocN(sizeof(uiTemplateHeadID), "uiTemplateHeadID");
	idtemplate->template.type= TEMPLATE_HEADER_ID;
	idtemplate->ptr= *ptr;
	idtemplate->propname= propname;
	idtemplate->flag= flag;
	idtemplate->func= func;

	BLI_addtail(&layout->templates, idtemplate);
}

void uiTemplateSetColor(uiLayout *layout, int color)
{
	uiTemplate *template= layout->templates.last;

	if(template)
		template->color= color;
}

void uiTemplateSlot(uiLayout *layout, int slot)
{
	uiTemplate *template= layout->templates.last;

	if(template)
		template->slot= slot;
}

/********************** Layout *******************/

static void ui_layout_templates(const bContext *C, uiBlock *block, uiLayout *layout)
{
	uiTemplate *template;

	if(layout->dir == UI_LAYOUT_HORIZONTAL) {
		for(template=layout->templates.first; template; template=template->next) {
			switch(template->type) {
				case TEMPLATE_HEADER_MENUS:
					ui_layout_header_menus(C, layout, block, template);
					break;
				case TEMPLATE_HEADER_ID:
					ui_layout_header_id(C, layout, block, template);
					break;
				case TEMPLATE_HEADER_BUTTONS:
				default:
					ui_layout_header_buttons(layout, block, template);
					break;
			}
		}

		layout->x += TEMPLATE_SPACE;
	}
	else {
		for(template=layout->templates.first; template; template=template->next) {
			if(template->color) {
				// XXX oldcolor= uiBlockGetCol(block);
				// XXX uiBlockSetCol(block, template->color);
			}

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

	// XXX 	if(template->color)
	// XXX 		uiBlockSetCol(block, oldcolor);

			layout->y -= TEMPLATE_SPACE;
		}
	}
}

void ui_layout_end(const bContext *C, uiBlock *block, uiLayout *layout, int *x, int *y)
{
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

uiLayout *uiLayoutBegin(int dir, int x, int y, int size, int em)
{
	uiLayout *layout;

	layout= MEM_callocN(sizeof(uiLayout), "uiLayout");
	layout->opcontext= WM_OP_INVOKE_REGION_WIN;
	layout->dir= dir;
	layout->x= x;
	layout->y= y;

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

void uiLayoutEnd(const bContext *C, uiBlock *block, uiLayout *layout, int *x, int *y)
{
	ui_layout_end(C, block, layout, x, y);
	ui_layout_free(layout);
}

/************************ Utilities ************************/

void uiRegionPanelLayout(const bContext *C, ARegion *ar, int vertical, char *context)
{
	uiBlock *block;
	PanelType *pt;
	Panel *panel;
	float col[3];
	int xco, yco, x=PNL_DIST, y=-PNL_HEADER-PNL_DIST, w, em;

	// XXX this only hides cruft

	/* clear */
	UI_GetThemeColor3fv(TH_HEADER, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	/* set view2d view matrix for scrolling (without scrollers) */
	UI_view2d_view_ortho(C, &ar->v2d);
	
	uiBeginPanels(C, ar);

	for(pt= ar->type->paneltypes.first; pt; pt= pt->next) {
		if(context)
			if(!pt->context || strcmp(context, pt->context) != 0)
				continue;

		if(pt->draw && (!pt->poll || pt->poll(C))) {
			block= uiBeginBlock(C, ar, pt->idname, UI_EMBOSS);
			
			if(vertical) {
				w= (ar->type->minsizex)? ar->type->minsizex-12: block->aspect*ar->winx-12;
				em= (ar->type->minsizex)? 10: 20;
			}
			else {
				w= (ar->type->minsizex)? ar->type->minsizex-12: UI_PANEL_WIDTH-12;
				em= (ar->type->minsizex)? 10: 20;
			}

			if(uiNewPanel(C, ar, block, pt->name, pt->name, x, y, w, 0)) {
				panel= uiPanelFromBlock(block);
				panel->type= pt;
				panel->layout= uiLayoutBegin(UI_LAYOUT_VERTICAL, x, y, w, em);

				pt->draw(C, panel);

				uiLayoutEnd(C, block, panel->layout, &xco, &yco);
				panel->layout= NULL;
				uiNewPanelHeight(block, y - yco + 12);
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
	uiBlock *block;
	uiLayout *layout;
	HeaderType *ht;
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
		layout= uiLayoutBegin(UI_LAYOUT_HORIZONTAL, xco, yco, 24, 1);

		if(ht->draw)
			ht->draw(C, layout);

		uiLayoutEnd(C, block, layout, &xco, &yco);
		uiEndBlock(C, block);
		uiDrawBlock(C, block);
	}

	/* always as last  */
	UI_view2d_totRect_set(&ar->v2d, xco+XIC+80, ar->v2d.tot.ymax-ar->v2d.tot.ymin);

	/* restore view matrix? */
	UI_view2d_view_restore(C);
}

