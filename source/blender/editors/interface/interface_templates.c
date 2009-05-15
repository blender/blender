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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Blender Foundation 2009.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_library.h"
#include "BKE_utildefines.h"

#include "ED_screen.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

static uiBlock *block_free_layout(uiLayout *layout)
{
	uiBlock *block;

	block= uiLayoutBlock(layout);
	uiBlockSetCurLayout(block, uiLayoutFree(layout, 0));

	return block;
}

void ui_template_fix_linking()
{
}

/********************** Header Template *************************/

void uiTemplateHeader(uiLayout *layout, bContext *C)
{
	uiBlock *block;
	
	block= block_free_layout(layout);
	ED_area_header_standardbuttons(C, block, 0);
}

/******************* Header ID Template ************************/

typedef struct TemplateHeaderID {
	PointerRNA ptr;
	PropertyRNA *prop;

	int flag;
	short browse;

	char newop[256];
	char openop[256];
	char unlinkop[256];
} TemplateHeaderID;

static void template_header_id_cb(bContext *C, void *arg_litem, void *arg_event)
{
	TemplateHeaderID *template= (TemplateHeaderID*)arg_litem;
	PointerRNA idptr= RNA_property_pointer_get(&template->ptr, template->prop);
	ID *idtest, *id= idptr.data;
	ListBase *lb= wich_libbase(CTX_data_main(C), ID_TXT);
	int nr, event= GET_INT_FROM_POINTER(arg_event);
	
	if(event == UI_ID_BROWSE && template->browse == 32767)
		event= UI_ID_ADD_NEW;
	else if(event == UI_ID_BROWSE && template->browse == 32766)
		event= UI_ID_OPEN;

	switch(event) {
		case UI_ID_BROWSE: {
			if(template->browse== -2) {
				/* XXX implement or find a replacement
				 * activate_databrowse((ID *)G.buts->lockpoin, GS(id->name), 0, B_MESHBROWSE, &template->browse, do_global_buttons); */
				return;
			}
			if(template->browse < 0)
				return;

			for(idtest=lb->first, nr=1; idtest; idtest=idtest->next, nr++) {
				if(nr==template->browse) {
					if(id == idtest)
						return;

					id= idtest;
					RNA_id_pointer_create(id, &idptr);
					RNA_property_pointer_set(&template->ptr, template->prop, idptr);
					RNA_property_update(C, &template->ptr, template->prop);
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
			WM_operator_name_call(C, template->newop, WM_OP_INVOKE_REGION_WIN, NULL);
			break;
		case UI_ID_OPEN:
			WM_operator_name_call(C, template->openop, WM_OP_INVOKE_REGION_WIN, NULL);
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

static void template_header_ID(bContext *C, uiBlock *block, TemplateHeaderID *template)
{
	uiBut *but;
	TemplateHeaderID *duptemplate;
	PointerRNA idptr;
	ListBase *lb;
	int x= 0, y= 0;

	idptr= RNA_property_pointer_get(&template->ptr, template->prop);
	lb= wich_libbase(CTX_data_main(C), ID_TXT);

	uiBlockBeginAlign(block);
	if(template->flag & UI_ID_BROWSE) {
		char *extrastr, *str;
		
		if((template->flag & UI_ID_ADD_NEW) && (template->flag && UI_ID_OPEN))
			extrastr= "OPEN NEW %x 32766 |ADD NEW %x 32767";
		else if(template->flag & UI_ID_ADD_NEW)
			extrastr= "ADD NEW %x 32767";
		else if(template->flag & UI_ID_OPEN)
			extrastr= "OPEN NEW %x 32766";
		else
			extrastr= NULL;

		duptemplate= MEM_dupallocN(template);
		IDnames_to_pupstring(&str, NULL, extrastr, lb, idptr.data, &duptemplate->browse);

		but= uiDefButS(block, MENU, 0, str, x, y, UI_UNIT_X, UI_UNIT_Y, &duptemplate->browse, 0, 0, 0, 0, "Browse existing choices, or add new");
		uiButSetNFunc(but, template_header_id_cb, duptemplate, SET_INT_IN_POINTER(UI_ID_BROWSE));
		x+= UI_UNIT_X;
	
		MEM_freeN(str);
	}

	/* text button with name */
	if(idptr.data) {
		char name[64];

		text_idbutton(idptr.data, name);
		but= uiDefButR(block, TEX, 0, name, x, y, UI_UNIT_X*6, UI_UNIT_Y, &idptr, "name", -1, 0, 0, -1, -1, NULL);
		uiButSetNFunc(but, template_header_id_cb, MEM_dupallocN(template), SET_INT_IN_POINTER(UI_ID_RENAME));
		x += UI_UNIT_X*6;

		/* delete button */
		if(template->flag & UI_ID_DELETE) {
			but= uiDefIconButO(block, BUT, template->unlinkop, WM_OP_EXEC_REGION_WIN, ICON_X, x, y, UI_UNIT_X, UI_UNIT_Y, NULL);
			x += UI_UNIT_X;
		}
	}
	uiBlockEndAlign(block);
}

void uiTemplateHeaderID(uiLayout *layout, bContext *C, PointerRNA *ptr, char *propname, char *newop, char *openop, char *unlinkop)
{
	TemplateHeaderID *template;
	uiBlock *block;
	PropertyRNA *prop;

	if(!ptr->data)
		return;

	prop= RNA_struct_find_property(ptr, propname);

	if(!prop) {
		printf("uiTemplateHeaderID: property not found: %s\n", propname);
		return;
	}

	template= MEM_callocN(sizeof(TemplateHeaderID), "TemplateHeaderID");
	template->ptr= *ptr;
	template->prop= prop;
	template->flag= UI_ID_BROWSE|UI_ID_RENAME;

	if(newop) {
		template->flag |= UI_ID_ADD_NEW;
		BLI_strncpy(template->newop, newop, sizeof(template->newop));
	}
	if(openop) {
		template->flag |= UI_ID_OPEN;
		BLI_strncpy(template->openop, openop, sizeof(template->openop));
	}
	if(unlinkop) {
		template->flag |= UI_ID_DELETE;
		BLI_strncpy(template->unlinkop, unlinkop, sizeof(template->unlinkop));
	}

	block= block_free_layout(layout);
	template_header_ID(C, block, template);

	MEM_freeN(template);
}

