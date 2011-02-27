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

#include "BLI_utildefines.h"

#include "BKE_context.h"


#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"


/*************************** RNA Utilities ******************************/

uiBut *uiDefAutoButR(uiBlock *block, PointerRNA *ptr, PropertyRNA *prop, int index, const char *name, int icon, int x1, int y1, int x2, int y2)
{
	uiBut *but=NULL;
	const char *propname= RNA_property_identifier(prop);
	char prop_item[sizeof(((IDProperty *)NULL)->name)+4]; /* size of the ID prop name + room for [""] */
	int arraylen= RNA_property_array_length(ptr, prop);

	/* support for custom props */
	if(RNA_property_is_idprop(prop)) {
		sprintf(prop_item, "[\"%s\"]", propname);
		propname= prop_item;
	}

	switch(RNA_property_type(prop)) {
		case PROP_BOOLEAN: {

			if(arraylen && index == -1)
				return NULL;
			
			if(icon && name && name[0] == '\0')
				but= uiDefIconButR(block, ICONTOG, 0, icon, x1, y1, x2, y2, ptr, propname, index, 0, 0, -1, -1, NULL);
			else if(icon)
				but= uiDefIconTextButR(block, ICONTOG, 0, icon, name, x1, y1, x2, y2, ptr, propname, index, 0, 0, -1, -1, NULL);
			else
				but= uiDefButR(block, OPTION, 0, name, x1, y1, x2, y2, ptr, propname, index, 0, 0, -1, -1, NULL);
			break;
		}
		case PROP_INT:
		case PROP_FLOAT:
			if(arraylen && index == -1) {
				if(ELEM(RNA_property_subtype(prop), PROP_COLOR, PROP_COLOR_GAMMA))
					but= uiDefButR(block, COL, 0, name, x1, y1, x2, y2, ptr, propname, 0, 0, 0, -1, -1, NULL);
			}
			else if(RNA_property_subtype(prop) == PROP_PERCENTAGE || RNA_property_subtype(prop) == PROP_FACTOR)
				but= uiDefButR(block, NUMSLI, 0, name, x1, y1, x2, y2, ptr, propname, index, 0, 0, -1, -1, NULL);
			else
				but= uiDefButR(block, NUM, 0, name, x1, y1, x2, y2, ptr, propname, index, 0, 0, -1, -1, NULL);
			break;
		case PROP_ENUM:
			if(icon && name && name[0] == '\0')
				but= uiDefIconButR(block, MENU, 0, icon, x1, y1, x2, y2, ptr, propname, index, 0, 0, -1, -1, NULL);
			else if(icon)
				but= uiDefIconTextButR(block, MENU, 0, icon, NULL, x1, y1, x2, y2, ptr, propname, index, 0, 0, -1, -1, NULL);
			else
				but= uiDefButR(block, MENU, 0, NULL, x1, y1, x2, y2, ptr, propname, index, 0, 0, -1, -1, NULL);
			break;
		case PROP_STRING:
			if(icon && name && name[0] == '\0')
				but= uiDefIconButR(block, TEX, 0, icon, x1, y1, x2, y2, ptr, propname, index, 0, 0, -1, -1, NULL);
			else if(icon)
				but= uiDefIconTextButR(block, TEX, 0, icon, name, x1, y1, x2, y2, ptr, propname, index, 0, 0, -1, -1, NULL);
			else
				but= uiDefButR(block, TEX, 0, name, x1, y1, x2, y2, ptr, propname, index, 0, 0, -1, -1, NULL);
			break;
		case PROP_POINTER: {
			PointerRNA pptr;

			pptr= RNA_property_pointer_get(ptr, prop);
			if(!pptr.type)
				pptr.type= RNA_property_pointer_type(ptr, prop);
			icon= RNA_struct_ui_icon(pptr.type);
			if(icon == ICON_DOT)
				icon= 0;

			but= uiDefIconTextButR(block, IDPOIN, 0, icon, name, x1, y1, x2, y2, ptr, propname, index, 0, 0, -1, -1, NULL);
			break;
		}
		case PROP_COLLECTION: {
			char text[256];
			sprintf(text, "%d items", RNA_property_collection_length(ptr, prop));
			but= uiDefBut(block, LABEL, 0, text, x1, y1, x2, y2, NULL, 0, 0, 0, 0, NULL);
			uiButSetFlag(but, UI_BUT_DISABLED);
			break;
		}
		default:
			but= NULL;
			break;
	}

	return but;
}

int uiDefAutoButsRNA(uiLayout *layout, PointerRNA *ptr, int (*check_prop)(PropertyRNA *), const char label_align)
{
	uiLayout *split, *col;
	int flag;
	const char *name;
	int tot= 0;

	assert(ELEM3(label_align, '\0', 'H', 'V'));

	RNA_STRUCT_BEGIN(ptr, prop) {
		flag= RNA_property_flag(prop);
		if(flag & PROP_HIDDEN || (check_prop && check_prop(prop)==FALSE))
			continue;

		if(label_align != '\0') {
			name= RNA_property_ui_name(prop);

			if(label_align=='V') {
				col= uiLayoutColumn(layout, 1);
				uiItemL(col, name, ICON_NONE);
			}
			else if(label_align=='H') {
				split = uiLayoutSplit(layout, 0.5f, 0);

				uiItemL(uiLayoutColumn(split, 0), name, ICON_NONE);
				col= uiLayoutColumn(split, 0);
			}
			else {
				col= NULL;
			}

			/* may meed to add more cases here.
			 * don't override enum flag names */
			if(flag & PROP_ENUM_FLAG) {
				name= NULL;
			}
			else {
				name= ""; /* name is shown above, empty name for button below */
			}
		}
		else {
			col= layout;
			name= NULL; /* no smart label alignment, show default name with button */
		}

		uiItemFullR(col, ptr, prop, -1, 0, 0, name, ICON_NONE);
		tot++;
	}
	RNA_STRUCT_END;

	return tot;
}

/***************************** ID Utilities *******************************/

int uiIconFromID(ID *id)
{
	Object *ob;
	PointerRNA ptr;
	short idcode;

	if(id==NULL)
		return ICON_NONE;
	
	idcode= GS(id->name);

	/* exception for objects */
	if(idcode == ID_OB) {
		ob= (Object*)id;

		if(ob->type == OB_EMPTY)
			return ICON_EMPTY_DATA;
		else
			return uiIconFromID(ob->data);
	}

	/* otherwise get it through RNA, creating the pointer
	   will set the right type, also with subclassing */
	RNA_id_pointer_create(id, &ptr);

	return (ptr.type)? RNA_struct_ui_icon(ptr.type) : ICON_NONE;
}
