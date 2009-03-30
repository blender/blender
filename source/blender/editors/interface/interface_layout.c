
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

/************************ Structs and Defines *************************/

#define COLUMN_SPACE	5
#define TEMPLATE_SPACE	5
#define STACK_SPACE		5
#define BUTTON_SPACE_X	5
#define BUTTON_SPACE_Y	2

#define RNA_NO_INDEX	-1

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

	const char *name;
	int icon;
} uiItem;

typedef struct uiItemRNA {
	uiItem item;

	PointerRNA ptr;
	PropertyRNA *prop;
	int index;
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
	TEMPLATE_COLUMN,
	TEMPLATE_LR,
	TEMPLATE_STACK,

	TEMPLATE_HEADER_MENUS,
	TEMPLATE_HEADER_BUTTONS,
	TEMPLATE_HEADER_ID
} uiTemplateType;

typedef struct uiTemplate {
	struct uiTemplate *next, *prev;
	uiTemplateType type;

	ListBase items;
	int color;
} uiTemplate;

typedef struct uiTemplateStck {
	uiTemplate template;
	uiLayout *sublayout;
} uiTemplateStck;

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
};

void ui_layout_free(uiLayout *layout);
void ui_layout_end(const bContext *C, uiBlock *block, uiLayout *layout, int *x, int *y);

/************************** Item ***************************/

static int ui_item_fit(int item, int all, int available)
{
	if(all > available)
		return (item*available)/all;
	
	return all;
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
		uiDefBut(block, LABEL, 0, name, x, y + h - YIC, w, YIC, NULL, 0.0, 0.0, 0, 0, "");

	/* create buttons */
	uiBlockBeginAlign(block);

	if(type == PROP_BOOLEAN && len == 20) {
		/* special check for layer layout */
		int butw, buth;

		butw= ui_item_fit(XIC, XIC*10 + BUTTON_SPACE_X, w);
		buth= MIN2(YIC, butw);

		y += 2*(YIC - buth);

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

			uiDefAutoButR(block, &rnaitem->ptr, rnaitem->prop, a, "", 0, x + w*col, y+(row-a-1)*YIC, w, YIC);
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

			uiDefAutoButR(block, &rnaitem->ptr, rnaitem->prop, a, str, 0, x, y+(len-a-1)*YIC, w, YIC);
		}
	}
	else {
		/* default array layout */
		for(a=0; a<len; a++)
			uiDefAutoButR(block, &rnaitem->ptr, rnaitem->prop, a, "", 0, x, y+(len-a-1)*YIC, w, YIC);
	}

	uiBlockEndAlign(block);
}

/* create lable + button for RNA property */
static void ui_item_with_label(uiBlock *block, uiItemRNA *rnaitem, int x, int y, int w, int h)
{
	char *name;
	int butw;

	if(rnaitem->item.name)
		name= (char*)rnaitem->item.name;
	else
		name= (char*)RNA_property_ui_name(&rnaitem->ptr, rnaitem->prop);
	
	if(strcmp(name, "") != 0) {
		butw= GetButStringLength(name);
		uiDefBut(block, LABEL, 0, name, x, y, butw, h, NULL, 0.0, 0.0, 0, 0, "");

		x += butw;
		w -= butw;
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
static int ui_text_icon_width(const char *name, int icon)
{
	if(icon && name && strcmp(name, "") == 0)
		return XIC; /* icon only */
	else if(icon && name)
		return XIC + GetButStringLength((char*)name); /* icon + text */
	else if(name)
		return GetButStringLength((char*)name); /* text only */
	else
		return 0;
}

/* estimated size of an item */
static void ui_item_size(uiItem *item, int *r_w, int *r_h)
{
	const char *name;
	int w, h;

	if(item->type == ITEM_RNA_PROPERTY) {
		/* RNA property */
		uiItemRNA *rnaitem= (uiItemRNA*)item;
		PropertyType type;
		PropertySubType subtype;
		int len;

		name= item->name;
		if(!name)
			name= RNA_property_ui_name(&rnaitem->ptr, rnaitem->prop);

		w= ui_text_icon_width(name, item->icon);
		h= YIC;

		/* arbitrary extended width by type */
		type= RNA_property_type(&rnaitem->ptr, rnaitem->prop);
		subtype= RNA_property_subtype(&rnaitem->ptr, rnaitem->prop);
		len= RNA_property_array_length(&rnaitem->ptr, rnaitem->prop);

		if(type == PROP_BOOLEAN && !item->icon)
			w += XIC;
		else if(type == PROP_INT || type == PROP_FLOAT)
			w += 2*XIC;
		else if(type == PROP_STRING)
			w += 8*XIC;

		/* increase height for arrays */
		if(rnaitem->index == RNA_NO_INDEX && len > 0) {
			if(name && strcmp(name, "") == 0 && item->icon == 0)
				h= 0;

			if(type == PROP_BOOLEAN && len == 20)
				h += 2*YIC;
			else if(subtype == PROP_MATRIX)
				h += ceil(sqrt(len))*YIC;
			else
				h += len*YIC;
		}
	}
	else if(item->type == ITEM_OPERATOR) {
		/* operator */
		uiItemOp *opitem= (uiItemOp*)item;

		name= item->name;
		if(!name)
			name= opitem->ot->name;

		w= ui_text_icon_width(name, item->icon);
		h= YIC;
	}
	else {
		/* other */
		w= ui_text_icon_width(item->name, item->icon);
		h= YIC;
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
void uiItemFullO(uiLayout *layout, int slot, const char *name, int icon, char *idname, IDProperty *properties, int context)
{
	uiTemplate *template= layout->templates.last;
	wmOperatorType *ot= WM_operatortype_find(idname);
	uiItemOp *opitem;

	if(!ot)
		return;

	opitem= MEM_callocN(sizeof(uiItemOp), "uiItemOp");

	opitem->item.name= name;
	opitem->item.icon= icon;
	opitem->item.type= ITEM_OPERATOR;
	opitem->item.slot= slot;

	opitem->ot= ot;
	opitem->properties= properties;
	opitem->context= context;

	BLI_addtail(&template->items, opitem);
}

void uiItemEnumO(uiLayout *layout, int slot, const char *name, int icon, char *opname, char *propname, int value)
{
	PointerRNA ptr;

	WM_operator_properties_create(&ptr, opname);
	RNA_enum_set(&ptr, propname, value);

	uiItemFullO(layout, slot, name, icon, opname, ptr.data, layout->opcontext);
}

void uiItemsEnumO(uiLayout *layout, int slot, char *opname, char *propname)
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
			uiItemEnumO(layout, slot, "", 0, opname, propname, item[i].value);
	}
}

void uiItemBooleanO(uiLayout *layout, int slot, const char *name, int icon, char *opname, char *propname, int value)
{
	PointerRNA ptr;

	WM_operator_properties_create(&ptr, opname);
	RNA_boolean_set(&ptr, propname, value);

	uiItemFullO(layout, slot, name, icon, opname, ptr.data, layout->opcontext);
}

void uiItemIntO(uiLayout *layout, int slot, const char *name, int icon, char *opname, char *propname, int value)
{
	PointerRNA ptr;

	WM_operator_properties_create(&ptr, opname);
	RNA_int_set(&ptr, propname, value);

	uiItemFullO(layout, slot, name, icon, opname, ptr.data, layout->opcontext);
}

void uiItemFloatO(uiLayout *layout, int slot, const char *name, int icon, char *opname, char *propname, float value)
{
	PointerRNA ptr;

	WM_operator_properties_create(&ptr, opname);
	RNA_float_set(&ptr, propname, value);

	uiItemFullO(layout, slot, name, icon, opname, ptr.data, layout->opcontext);
}

void uiItemStringO(uiLayout *layout, int slot, const char *name, int icon, char *opname, char *propname, char *value)
{
	PointerRNA ptr;

	WM_operator_properties_create(&ptr, opname);
	RNA_string_set(&ptr, propname, value);

	uiItemFullO(layout, slot, name, icon, opname, ptr.data, layout->opcontext);
}

void uiItemO(uiLayout *layout, int slot, const char *name, int icon, char *opname)
{
	uiItemFullO(layout, slot, name, icon, opname, NULL, layout->opcontext);
}

/* RNA property items */
void uiItemFullR(uiLayout *layout, int slot, const char *name, int icon, PointerRNA *ptr, char *propname, int index)
{
	uiTemplate *template= layout->templates.last;
	PropertyRNA *prop;
	uiItemRNA *rnaitem;
	
	prop= RNA_struct_find_property(ptr, propname);
	if(!prop){
		printf("Property not found : %s \n",propname);
		return;
	}
	
	rnaitem= MEM_callocN(sizeof(uiItemRNA), "uiItemRNA");

	rnaitem->item.name= name;
	rnaitem->item.icon= icon;
	rnaitem->item.type= ITEM_RNA_PROPERTY;
	rnaitem->item.slot= slot;

	rnaitem->ptr= *ptr;
	rnaitem->prop= prop;
	rnaitem->index= index;

	BLI_addtail(&template->items, rnaitem);
}

void uiItemR(uiLayout *layout, int slot, const char *name, int icon, PointerRNA *ptr, char *propname)
{
	uiItemFullR(layout, slot, name, icon, ptr, propname, RNA_NO_INDEX);
}

/* menu item */
void uiItemMenu(uiLayout *layout, int slot, const char *name, int icon, uiMenuCreateFunc func)
{
	uiTemplate *template= layout->templates.last;
	uiItemLMenu *menuitem= MEM_callocN(sizeof(uiItemLMenu), "uiItemLMenu");

	menuitem->item.name= name;
	menuitem->item.icon= icon;
	menuitem->item.type= ITEM_MENU;
	menuitem->item.slot= slot;

	menuitem->func= func;

	BLI_addtail(&template->items, menuitem);
}

/* label item */
void uiItemLabel(uiLayout *layout, int slot, const char *name, int icon)
{
	uiTemplate *template= layout->templates.last;
	uiItem *item= MEM_callocN(sizeof(uiItem), "uiItem");

	item->name= name;
	item->icon= icon;
	item->type= ITEM_LABEL;
	item->slot= slot;

	BLI_addtail(&template->items, item);
}

/**************************** Template ***************************/

/* multi-column layout */
static void ui_layout_column(uiLayout *layout, uiBlock *block, uiTemplate *template, int *x, int *y, int w, int h)
{
	uiItem *item;
	int col, totcol= 0, colx, coly, colw, miny, itemw, itemh;

	/* compute number of columns */
	for(item=template->items.first; item; item=item->next)
		totcol= MAX2(item->slot+1, totcol);
	
	if(totcol == 0)
		return;
	
	colx= *x;
	colw= (w - (totcol-1)*COLUMN_SPACE)/totcol;
	miny= *y;

	/* create column per column */
	for(col=0; col<totcol; col++) {
		coly= *y;

		for(item=template->items.first; item; item=item->next) {
			if(item->slot != col)
				continue;

			ui_item_size(item, &itemw, &itemh);

			coly -= itemh + BUTTON_SPACE_Y;
			ui_item_buts(block, item, colx, coly, colw, itemh);
		}

		colx += colw + COLUMN_SPACE;
		miny= MIN2(miny, coly);
	}

	*y= miny;
}

/* left-right layout, with buttons aligned on both sides */
static void ui_layout_lr(uiLayout *layout, uiBlock *block, uiTemplate *template, int *x, int *y, int w, int h)
{
	uiItem *item;
	int totw= 0, maxh= 0, itemw, itemh, leftx, rightx;

	/* estimate total width of buttons */
	for(item=template->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh);
		totw += itemw;
		maxh= MAX2(maxh, itemh);
	}

	if(totw == 0)
		return;
	
	/* create buttons starting from left and right */
	leftx= *x;
	rightx= *x + w;

	for(item=template->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh);
		itemw= ui_item_fit(itemw, totw+BUTTON_SPACE_X, w);

		if(item->slot == UI_TSLOT_LR_LEFT) {
			ui_item_buts(block, item, leftx, *y-itemh, itemw, itemh);
			leftx += itemw;
		}
		else {
			rightx -= itemw;
			ui_item_buts(block, item, rightx, *y-itemh, itemw, itemh);
		}
	}

	*y -= maxh;
}

/* element in a stack layout */
static void ui_layout_stack(const bContext *C, uiLayout *layout, uiBlock *block, uiTemplate *template, int *x, int *y, int w, int h)
{
	uiTemplateStck *stack= (uiTemplateStck*)template;
	int starty, startx;

	startx= *x;
	starty= *y;

	/* some extra padding */
	stack->sublayout->x= *x + STACK_SPACE;
	stack->sublayout->w= w - 2*STACK_SPACE;
	stack->sublayout->y= *y - STACK_SPACE;
	stack->sublayout->h= h;

	/* do layout for elements in sublayout */
	ui_layout_end(C, block, stack->sublayout, NULL, y);

	/* roundbox around the sublayout */
	uiDefBut(block, ROUNDBOX, 0, "", startx, *y, w, starty - *y, NULL, 7.0, 0.0, 3, 20, "");
}

static void ui_layout_header_buttons(uiLayout *layout, uiBlock *block, uiTemplate *template, int *x, int *y, int w, int h)
{
	uiItem *item;
	int itemw, itemh;
	
	uiBlockBeginAlign(block);

	for(item=template->items.first; item; item=item->next) {
		ui_item_size(item, &itemw, &itemh);
		ui_item_buts(block, item, *x, *y, itemw, itemh);
		*x += itemw;
	}

	uiBlockEndAlign(block);
}

static void ui_layout_header_menus(const bContext *C, uiLayout *layout, uiBlock *block, uiTemplate *template, int *x, int *y, int w, int h)
{
	ScrArea *sa= CTX_wm_area(C);

	*x= ED_area_header_standardbuttons(C, block, *y);

	if((sa->flag & HEADER_NO_PULLDOWN)==0) {
		uiBlockSetEmboss(block, UI_EMBOSSP);
		ui_layout_header_buttons(layout, block, template, x, y, w, h);
	}

	uiBlockSetEmboss(block, UI_EMBOSS);
}

static void ui_layout_header_id(const bContext *C, uiLayout *layout, uiBlock *block, uiTemplate *template, int *x, int *y, int w, int h)
{
	uiTemplateHeadID *idtemplate= (uiTemplateHeadID*)template;
	PointerRNA idptr;

	idptr= RNA_pointer_get(&idtemplate->ptr, idtemplate->propname);

	*x= uiDefIDPoinButs(block, CTX_data_main(C), NULL, (ID*)idptr.data, ID_TXT, NULL, *x, *y,
		idtemplate->func, UI_ID_BROWSE|UI_ID_RENAME|UI_ID_ADD_NEW|UI_ID_OPEN|UI_ID_DELETE);
}

void ui_template_free(uiTemplate *template)
{
	uiItem *item;

	if(template->type == TEMPLATE_STACK) {
		uiTemplateStck *stack= (uiTemplateStck*)template;
		ui_layout_free(stack->sublayout);
	}

	for(item=template->items.first; item; item=item->next)
		ui_item_free(item);

	BLI_freelistN(&template->items);
}

/* template create functions */
void uiTemplateColumn(uiLayout *layout)
{
	uiTemplate *template;

	template= MEM_callocN(sizeof(uiTemplate), "uiTemplate");
	template->type= TEMPLATE_COLUMN;

	BLI_addtail(&layout->templates, template);
}


void uiTemplateLeftRight(uiLayout *layout)
{
	uiTemplate *template;

	template= MEM_callocN(sizeof(uiTemplate), "uiTemplate");
	template->type= TEMPLATE_LR;

	BLI_addtail(&layout->templates, template);
}

uiLayout *uiTemplateStack(uiLayout *layout)
{
	uiTemplateStck *stack;

	stack= MEM_callocN(sizeof(uiTemplateStck), "uiTemplateStck");
	stack->template.type= TEMPLATE_STACK;
	stack->sublayout= uiLayoutBegin(layout->dir, 0, 0, 0, 0);
	BLI_addtail(&layout->templates, stack);

	return stack->sublayout;
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

	template->color= color;
}

/********************** Layout *******************/

static void ui_layout_templates(const bContext *C, uiBlock *block, uiLayout *layout)
{
	uiTemplate *template;
	int oldcolor= 0;

	for(template=layout->templates.first; template; template=template->next) {
		if(template->color) {
			oldcolor= uiBlockGetCol(block);
			uiBlockSetCol(block, template->color);
		}

		switch(template->type) {
			case TEMPLATE_COLUMN:
				ui_layout_column(layout, block, template, &layout->x, &layout->y, layout->w, layout->h);
				break;
			case TEMPLATE_LR:
				ui_layout_lr(layout, block, template, &layout->x, &layout->y, layout->w, layout->h);
				break;
			case TEMPLATE_STACK:
				ui_layout_stack(C, layout, block, template, &layout->x, &layout->y, layout->w, layout->h);
				break;
			case TEMPLATE_HEADER_MENUS:
				ui_layout_header_menus(C, layout, block, template, &layout->x, &layout->y, layout->w, layout->h);
				break;
			case TEMPLATE_HEADER_BUTTONS:
				ui_layout_header_buttons(layout, block, template, &layout->x, &layout->y, layout->w, layout->h);
				break;
			case TEMPLATE_HEADER_ID:
				ui_layout_header_id(C, layout, block, template, &layout->x, &layout->y, layout->w, layout->h);
				break;
		}

		if(template->color)
			uiBlockSetCol(block, oldcolor);

		if(layout->dir == UI_LAYOUT_HORIZONTAL)
			layout->x += TEMPLATE_SPACE;
		else
			layout->y -= TEMPLATE_SPACE;
	}
}

void ui_layout_end(const bContext *C, uiBlock *block, uiLayout *layout, int *x, int *y)
{
	ui_layout_templates(C, block, layout);

	if(x) *x= layout->x;
	if(y) *y= layout->y;
	
	/* XXX temp, migration flag for drawing code */
	uiBlockSetFlag(block, UI_BLOCK_2_50);
}

void ui_layout_free(uiLayout *layout)
{
	uiTemplate *template;

	for(template=layout->templates.first; template; template=template->next)
		ui_template_free(template);

	BLI_freelistN(&layout->templates);
	MEM_freeN(layout);
}

uiLayout *uiLayoutBegin(int dir, int x, int y, int w, int h)
{
	uiLayout *layout;

	layout= MEM_callocN(sizeof(uiLayout), "uiLayout");
	layout->opcontext= WM_OP_INVOKE_REGION_WIN;
	layout->dir= dir;
	layout->x= x;
	layout->y= y;
	layout->w= w;
	layout->h= h;

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

/* Utilities */

void uiRegionPanelLayout(const bContext *C, ARegion *ar, int vertical, char *context)
{
	uiBlock *block;
	PanelType *pt;
	Panel *panel;
	float col[3];
	int xco, yco, x=0, y=0, w;

	// XXX this only hides cruft

	/* clear */
	UI_GetThemeColor3fv(TH_HEADER, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	/* set view2d view matrix for scrolling (without scrollers) */
	UI_view2d_view_ortho(C, &ar->v2d);
	
	for(pt= ar->type->paneltypes.first; pt; pt= pt->next) {
		if(context)
			if(!pt->context || strcmp(context, pt->context) != 0)
				continue;

		if(pt->draw && (!pt->poll || pt->poll(C))) {
			w= (ar->type->minsizex)? ar->type->minsizex-22: UI_PANEL_WIDTH-22;

			block= uiBeginBlock(C, ar, pt->idname, UI_EMBOSS, UI_HELV);
			if(uiNewPanel(C, ar, block, pt->name, pt->name, x, y, w, 0)==0) return;
			
			panel= uiPanelFromBlock(block);
			panel->type= pt;
			panel->layout= uiLayoutBegin(UI_LAYOUT_VERTICAL, x, y, w, 0);

			pt->draw(C, panel);

			uiLayoutEnd(C, block, panel->layout, &xco, &yco);
			uiEndBlock(C, block);

			panel->layout= NULL;
			uiNewPanelHeight(block, y - yco + 6);

			if(vertical)
				y += yco;
			else
				x += xco;
		}
	}

	uiDrawPanels(C, 1);
	uiMatchPanelsView2d(ar);
	
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
		block= uiBeginBlock(C, ar, "header buttons", UI_EMBOSS, UI_HELV);
		layout= uiLayoutBegin(UI_LAYOUT_HORIZONTAL, xco, yco, 0, 24);

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

