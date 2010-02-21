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

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_color_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BLI_string.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_icons.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_utildefines.h"

#include "ED_screen.h"
#include "ED_render.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "interface_intern.h"

void ui_template_fix_linking()
{
}

/********************** Header Template *************************/

void uiTemplateHeader(uiLayout *layout, bContext *C, int menus)
{
	uiBlock *block;

	block= uiLayoutAbsoluteBlock(layout);
	if(menus) ED_area_header_standardbuttons(C, block, 0);
	else ED_area_header_switchbutton(C, block, 0);
}

/********************** DopeSheet Filter Template *************************/

void uiTemplateDopeSheetFilter(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	Main *mainptr= CTX_data_main(C);
	ScrArea *sa= CTX_wm_area(C);
	uiLayout *row= layout;
	short nlaActive= ((sa) && (sa->spacetype==SPACE_NLA));
	
	/* more 'generic' filtering options */
	row= uiLayoutRow(layout, 1);
	
	uiItemR(row, "", 0, ptr, "only_selected", 0);
	uiItemR(row, "", 0, ptr, "display_transforms", 0); // xxx: include in another position instead?
	
	if (nlaActive)
		uiItemR(row, "", 0, ptr, "include_missing_nla", 0);
	
	/* datatype based - only available datatypes are shown */
	row= uiLayoutRow(layout, 1);

	uiItemR(row, "", 0, ptr, "display_scene", 0);
	uiItemR(row, "", 0, ptr, "display_world", 0);
	uiItemR(row, "", 0, ptr, "display_node", 0);
	
	if (mainptr && mainptr->mesh.first)
		uiItemR(row, "", 0, ptr, "display_mesh", 0);
	if (mainptr && mainptr->key.first)
		uiItemR(row, "", 0, ptr, "display_shapekeys", 0);
	if (mainptr && mainptr->mat.first)
		uiItemR(row, "", 0, ptr, "display_material", 0);
	if (mainptr && mainptr->lamp.first)
		uiItemR(row, "", 0, ptr, "display_lamp", 0);
	if (mainptr && mainptr->tex.first)
		uiItemR(row, "", 0, ptr, "display_texture", 0);
	if (mainptr && mainptr->camera.first)
		uiItemR(row, "", 0, ptr, "display_camera", 0);
	if (mainptr && mainptr->curve.first)
		uiItemR(row, "", 0, ptr, "display_curve", 0);
	if (mainptr && mainptr->mball.first)
		uiItemR(row, "", 0, ptr, "display_metaball", 0);
	if (mainptr && mainptr->armature.first)
		uiItemR(row, "", 0, ptr, "display_armature", 0);
	if (mainptr && mainptr->particle.first)
		uiItemR(row, "", 0, ptr, "display_particle", 0);
	
	/* group-based filtering (only when groups are available */
	if (mainptr && mainptr->group.first) {
		row= uiLayoutRow(layout, 1);
		
		uiItemR(row, "", 0, ptr, "only_group_objects", 0);
		
		/* if enabled, show the group selection field too */
		if (RNA_boolean_get(ptr, "only_group_objects"))
			uiItemR(row, "", 0, ptr, "filtering_group", 0);
	}
}

/********************** Search Callbacks *************************/

typedef struct TemplateID {
	PointerRNA ptr;
	PropertyRNA *prop;

	ListBase *idlb;
	int prv_rows, prv_cols;
} TemplateID;

/* Search browse menu, assign  */
static void id_search_call_cb(bContext *C, void *arg_template, void *item)
{
	TemplateID *template= (TemplateID*)arg_template;

	/* ID */
	if(item) {
		PointerRNA idptr;

		RNA_id_pointer_create(item, &idptr);
		RNA_property_pointer_set(&template->ptr, template->prop, idptr);
		RNA_property_update(C, &template->ptr, template->prop);
	}
}

/* ID Search browse menu, do the search */
static void id_search_cb(const bContext *C, void *arg_template, char *str, uiSearchItems *items)
{
	TemplateID *template= (TemplateID*)arg_template;
	ListBase *lb= template->idlb;
	ID *id;
	int iconid;

	/* ID listbase */
	for(id= lb->first; id; id= id->next) {
		if(BLI_strcasestr(id->name+2, str)) {
			iconid= ui_id_icon_get((bContext*)C, id, 0);

			if(!uiSearchItemAdd(items, id->name+2, id, iconid))
				break;
		}
	}
}

/* ID Search browse menu, open */
static uiBlock *id_search_menu(bContext *C, ARegion *ar, void *arg_litem)
{
	static char search[256];
	static TemplateID template;
	PointerRNA idptr;
	wmEvent event;
	wmWindow *win= CTX_wm_window(C);
	uiBlock *block;
	uiBut *but;
	
	/* clear initial search string, then all items show */
	search[0]= 0;
	/* arg_litem is malloced, can be freed by parent button */
	template= *((TemplateID*)arg_litem);
	
	/* get active id for showing first item */
	idptr= RNA_property_pointer_get(&template.ptr, template.prop);

	block= uiBeginBlock(C, ar, "_popup", UI_EMBOSS);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1);
	
	/* preview thumbnails */
	if (template.prv_rows > 0 && template.prv_cols > 0) {
		int w = 96 * template.prv_cols;
		int h = 96 * template.prv_rows + 20;
		
		/* fake button, it holds space for search items */
		uiDefBut(block, LABEL, 0, "", 10, 15, w, h, NULL, 0, 0, 0, 0, NULL);
		
		but= uiDefSearchBut(block, search, 0, ICON_VIEWZOOM, 256, 10, 0, w, 19, template.prv_rows, template.prv_cols, "");
		uiButSetSearchFunc(but, id_search_cb, &template, id_search_call_cb, idptr.data);
	}
	/* list view */
	else {
		/* fake button, it holds space for search items */
		uiDefBut(block, LABEL, 0, "", 10, 15, 150, uiSearchBoxhHeight(), NULL, 0, 0, 0, 0, NULL);
		
		but= uiDefSearchBut(block, search, 0, ICON_VIEWZOOM, 256, 10, 0, 150, 19, 0, 0, "");
		uiButSetSearchFunc(but, id_search_cb, &template, id_search_call_cb, idptr.data);
	}
		
	
	uiBoundsBlock(block, 6);
	uiBlockSetDirection(block, UI_DOWN);	
	uiEndBlock(C, block);
	
	event= *(win->eventstate);	/* XXX huh huh? make api call */
	event.type= EVT_BUT_OPEN;
	event.val= KM_PRESS;
	event.customdata= but;
	event.customdatafree= FALSE;
	wm_event_add(win, &event);
	
	return block;
}

/************************ ID Template ***************************/
/* This is for browsing and editing the ID-blocks used */

/* for new/open operators */
void uiIDContextProperty(bContext *C, PointerRNA *ptr, PropertyRNA **prop)
{
	TemplateID *template;
	ARegion *ar= CTX_wm_region(C);
	uiBlock *block;
	uiBut *but;

	memset(ptr, 0, sizeof(*ptr));
	*prop= NULL;

	if(!ar)
		return;

	for(block=ar->uiblocks.first; block; block=block->next) {
		for(but=block->buttons.first; but; but= but->next) {
			/* find the button before the active one */
			if((but->flag & (UI_BUT_LAST_ACTIVE|UI_ACTIVE))) {
				if(but->func_argN) {
					template= but->func_argN;
					*ptr= template->ptr;
					*prop= template->prop;
					return;
				}
			}
		}
	}
}


static void template_id_cb(bContext *C, void *arg_litem, void *arg_event)
{
	TemplateID *template= (TemplateID*)arg_litem;
	PointerRNA idptr= RNA_property_pointer_get(&template->ptr, template->prop);
	ID *id= idptr.data, *newid;
	int event= GET_INT_FROM_POINTER(arg_event);
	
	switch(event) {
		case UI_ID_BROWSE:
		case UI_ID_PIN:
			printf("warning, id event %d shouldnt come here\n", event);
			break;
		case UI_ID_OPEN:
		case UI_ID_ADD_NEW:
			/* these call uiIDContextPropertySet */
			break;
		case UI_ID_DELETE:
			memset(&idptr, 0, sizeof(idptr));
			RNA_property_pointer_set(&template->ptr, template->prop, idptr);
			RNA_property_update(C, &template->ptr, template->prop);
			break;
		case UI_ID_FAKE_USER:
			if(id) {
				if(id->flag & LIB_FAKEUSER) id->us++;
				else id->us--;
			}
			else return;
			break;
		case UI_ID_LOCAL:
			if(id) {
				if(id_make_local(id, 0)) {
					/* reassign to get get proper updates/notifiers */
					idptr= RNA_property_pointer_get(&template->ptr, template->prop);
					RNA_property_pointer_set(&template->ptr, template->prop, idptr);
					RNA_property_update(C, &template->ptr, template->prop);
				}
			}
			break;
		case UI_ID_ALONE:
			if(id) {
				/* make copy */
				if(id_copy(id, &newid, 0) && newid) {
					/* us is 1 by convention, but RNA_property_pointer_set
					   will also incremement it, so set it to zero */
					newid->us= 0;

					/* assign copy */
					RNA_id_pointer_create(newid, &idptr);
					RNA_property_pointer_set(&template->ptr, template->prop, idptr);
					RNA_property_update(C, &template->ptr, template->prop);
				}
			}
			break;
#if 0
		case UI_ID_AUTO_NAME:
			break;
#endif
	}
}

static void template_ID(bContext *C, uiLayout *layout, TemplateID *template, StructRNA *type, int flag, char *newop, char *openop, char *unlinkop)
{
	uiBut *but;
	uiBlock *block;
	PointerRNA idptr;
	ListBase *lb;
	ID *id, *idfrom;

	idptr= RNA_property_pointer_get(&template->ptr, template->prop);
	id= idptr.data;
	idfrom= template->ptr.id.data;
	lb= template->idlb;

	block= uiLayoutGetBlock(layout);
	uiBlockBeginAlign(block);

	if(idptr.type)
		type= idptr.type;

	if(flag & UI_ID_PREVIEWS) {

		but= uiDefBlockButN(block, id_search_menu, MEM_dupallocN(template), "", 0, 0, UI_UNIT_X*6, UI_UNIT_Y*6, "Browse ID data");
		if(type) {
			but->icon= RNA_struct_ui_icon(type);
			if (id) but->icon = ui_id_icon_get(C, id, 1);
			uiButSetFlag(but, UI_HAS_ICON|UI_ICON_PREVIEW);
		}
		if((idfrom && idfrom->lib))
			uiButSetFlag(but, UI_BUT_DISABLED);
		
		
		uiLayoutRow(layout, 1);
	} else 
		
	if(flag & UI_ID_BROWSE) {
		but= uiDefBlockButN(block, id_search_menu, MEM_dupallocN(template), "", 0, 0, UI_UNIT_X*1.6, UI_UNIT_Y, "Browse ID data");
		if(type) {
			but->icon= RNA_struct_ui_icon(type);
			/* default dragging of icon for id browse buttons */
			uiButSetDragID(but, id);
			uiButSetFlag(but, UI_HAS_ICON|UI_ICON_LEFT);
		}

		if((idfrom && idfrom->lib))
			uiButSetFlag(but, UI_BUT_DISABLED);
	}

	/* text button with name */
	if(id) {
		char name[UI_MAX_NAME_STR];

		//text_idbutton(id, name);
		name[0]= '\0';
		but= uiDefButR(block, TEX, 0, name, 0, 0, UI_UNIT_X*6, UI_UNIT_Y, &idptr, "name", -1, 0, 0, -1, -1, NULL);
		uiButSetNFunc(but, template_id_cb, MEM_dupallocN(template), SET_INT_IN_POINTER(UI_ID_RENAME));

		if(id->lib) {
			if(id->flag & LIB_INDIRECT) {
				but= uiDefIconBut(block, BUT, 0, ICON_LIBRARY_DATA_INDIRECT, 0,0,UI_UNIT_X,UI_UNIT_Y, 0, 0, 0, 0, 0,
					"Indirect library datablock, cannot change.");
				uiButSetFlag(but, UI_BUT_DISABLED);
			}
			else {
				but= uiDefIconBut(block, BUT, 0, ICON_LIBRARY_DATA_DIRECT, 0,0,UI_UNIT_X,UI_UNIT_Y, 0, 0, 0, 0, 0,
					"Direct linked library datablock, click to make local.");
				if(!id_make_local(id, 1 /* test */) || (idfrom && idfrom->lib))
					uiButSetFlag(but, UI_BUT_DISABLED);
			}

			uiButSetNFunc(but, template_id_cb, MEM_dupallocN(template), SET_INT_IN_POINTER(UI_ID_LOCAL));
		}

		if(id->us > 1) {
			char str[32];

			sprintf(str, "%d", id->us);

			if(id->us<10)
				but= uiDefBut(block, BUT, 0, str, 0,0,UI_UNIT_X,UI_UNIT_Y, 0, 0, 0, 0, 0, "Displays number of users of this data. Click to make a single-user copy.");
			else
				but= uiDefBut(block, BUT, 0, str, 0,0,UI_UNIT_X+10,UI_UNIT_Y, 0, 0, 0, 0, 0, "Displays number of users of this data. Click to make a single-user copy.");

			uiButSetNFunc(but, template_id_cb, MEM_dupallocN(template), SET_INT_IN_POINTER(UI_ID_ALONE));
			if(!id_copy(id, NULL, 1 /* test only */) || (idfrom && idfrom->lib))
				uiButSetFlag(but, UI_BUT_DISABLED);
		}
	}
	
	if(flag & UI_ID_ADD_NEW) {
		int w= id?UI_UNIT_X: (flag & UI_ID_OPEN)? UI_UNIT_X*3: UI_UNIT_X*6;
		
		if(newop) {
			but= uiDefIconTextButO(block, BUT, newop, WM_OP_EXEC_DEFAULT, ICON_ZOOMIN, (id)? "": "New", 0, 0, w, UI_UNIT_Y, NULL);
			uiButSetNFunc(but, template_id_cb, MEM_dupallocN(template), SET_INT_IN_POINTER(UI_ID_ADD_NEW));
		}
		else {
			but= uiDefIconTextBut(block, BUT, 0, ICON_ZOOMIN, (id)? "": "New", 0, 0, w, UI_UNIT_Y, NULL, 0, 0, 0, 0, NULL);
			uiButSetNFunc(but, template_id_cb, MEM_dupallocN(template), SET_INT_IN_POINTER(UI_ID_ADD_NEW));
		}

		if((idfrom && idfrom->lib))
			uiButSetFlag(but, UI_BUT_DISABLED);
	}

	if(flag & UI_ID_OPEN) {
		int w= id?UI_UNIT_X: (flag & UI_ID_ADD_NEW)? UI_UNIT_X*3: UI_UNIT_X*6;
		
		if(openop) {
			but= uiDefIconTextButO(block, BUT, openop, WM_OP_INVOKE_DEFAULT, ICON_FILESEL, (id)? "": "Open", 0, 0, w, UI_UNIT_Y, NULL);
			uiButSetNFunc(but, template_id_cb, MEM_dupallocN(template), SET_INT_IN_POINTER(UI_ID_OPEN));
		}
		else {
			but= uiDefIconTextBut(block, BUT, 0, ICON_FILESEL, (id)? "": "Open", 0, 0, w, UI_UNIT_Y, NULL, 0, 0, 0, 0, NULL);
			uiButSetNFunc(but, template_id_cb, MEM_dupallocN(template), SET_INT_IN_POINTER(UI_ID_OPEN));
		}

		if((idfrom && idfrom->lib))
			uiButSetFlag(but, UI_BUT_DISABLED);
	}
	
	/* delete button */
	if(id && (flag & UI_ID_DELETE)) {
		if(unlinkop) {
			but= uiDefIconButO(block, BUT, unlinkop, WM_OP_INVOKE_REGION_WIN, ICON_X, 0, 0, UI_UNIT_X, UI_UNIT_Y, NULL);
		}
		else {
			but= uiDefIconBut(block, BUT, 0, ICON_X, 0, 0, UI_UNIT_X, UI_UNIT_Y, NULL, 0, 0, 0, 0, NULL);
			uiButSetNFunc(but, template_id_cb, MEM_dupallocN(template), SET_INT_IN_POINTER(UI_ID_DELETE));
		}

		if((idfrom && idfrom->lib))
			uiButSetFlag(but, UI_BUT_DISABLED);
	}
	
	uiBlockEndAlign(block);
}

static void ui_template_id(uiLayout *layout, bContext *C, PointerRNA *ptr, char *propname, char *newop, char *openop, char *unlinkop, int flag, int prv_rows, int prv_cols)
{
	TemplateID *template;
	PropertyRNA *prop;
	StructRNA *type;

	prop= RNA_struct_find_property(ptr, propname);

	if(!prop || RNA_property_type(prop) != PROP_POINTER) {
		printf("uiTemplateID: pointer property not found: %s\n", propname);
		return;
	}

	template= MEM_callocN(sizeof(TemplateID), "TemplateID");
	template->ptr= *ptr;
	template->prop= prop;
	template->prv_rows = prv_rows;
	template->prv_cols = prv_cols;
	
	if(newop)
		flag |= UI_ID_ADD_NEW;
	if(openop)
		flag |= UI_ID_OPEN;
	
	type= RNA_property_pointer_type(ptr, prop);
	template->idlb= wich_libbase(CTX_data_main(C), RNA_type_to_ID_code(type));
	
	/* create UI elements for this template
	 *	- template_ID makes a copy of the template data and assigns it to the relevant buttons
	 */
	if(template->idlb) {
		uiLayoutRow(layout, 1);
		template_ID(C, layout, template, type, flag, newop, openop, unlinkop);
	}

	MEM_freeN(template);
	
}

void uiTemplateID(uiLayout *layout, bContext *C, PointerRNA *ptr, char *propname, char *newop, char *openop, char *unlinkop)
{
	ui_template_id(layout, C, ptr, propname, newop, openop, unlinkop, UI_ID_BROWSE|UI_ID_RENAME|UI_ID_DELETE, 0, 0);
}

void uiTemplateIDBrowse(uiLayout *layout, bContext *C, PointerRNA *ptr, char *propname, char *newop, char *openop, char *unlinkop)
{
	ui_template_id(layout, C, ptr, propname, newop, openop, unlinkop, UI_ID_BROWSE|UI_ID_RENAME, 0, 0);
}

void uiTemplateIDPreview(uiLayout *layout, bContext *C, PointerRNA *ptr, char *propname, char *newop, char *openop, char *unlinkop, int rows, int cols)
{
	ui_template_id(layout, C, ptr, propname, newop, openop, unlinkop, UI_ID_BROWSE|UI_ID_RENAME|UI_ID_DELETE|UI_ID_PREVIEWS, rows, cols);
}

/************************ ID Chooser Template ***************************/

/* This is for selecting the type of ID-block to use, and then from the relevant type choosing the block to use 
 *
 * - propname: property identifier for property that ID-pointer gets stored to
 * - proptypename: property identifier for property used to determine the type of ID-pointer that can be used
 */
void uiTemplateAnyID(uiLayout *layout, bContext *C, PointerRNA *ptr, char *propname, char *proptypename, char *text)
{
	PropertyRNA *propID, *propType;
	uiLayout *row;
	
	/* get properties... */
	propID= RNA_struct_find_property(ptr, propname);
	propType= RNA_struct_find_property(ptr, proptypename);

	if (!propID || RNA_property_type(propID) != PROP_POINTER) {
		printf("uiTemplateAnyID: pointer property not found: %s\n", propname);
		return;
	}
	if (!propType || RNA_property_type(propType) != PROP_ENUM) { 
		printf("uiTemplateAnyID: pointer-type property not found: %s\n", proptypename);
		return;
	}
	
	/* Start drawing UI Elements using standard defines */
	row= uiLayoutRow(layout, 1);
	
	/* Label - either use the provided text, or will become "ID-Block:" */
	if (text)
		uiItemL(row, text, 0);
	else
		uiItemL(row, "ID-Block:", 0);
	
	/* ID-Type Selector - just have a menu of icons */
	// FIXME: the icon-only setting doesn't work when we supply a blank name
	uiItemFullR(row, "", 0, ptr, propType, 0, 0, UI_ITEM_R_ICON_ONLY);
	
	/* ID-Block Selector - just use pointer widget... */
	uiItemFullR(row, "", 0, ptr, propID, 0, 0, 0);
}

/********************* RNA Path Builder Template ********************/

/* ---------- */

/* This is creating/editing RNA-Paths 
 *
 * - ptr: struct which holds the path property
 * - propname: property identifier for property that path gets stored to
 * - root_ptr: struct that path gets built from
 */
void uiTemplatePathBuilder(uiLayout *layout, bContext *C, PointerRNA *ptr, char *propname, PointerRNA *root_ptr, char *text)
{
	PropertyRNA *propPath;
	uiLayout *row;
	
	/* check that properties are valid */
	propPath= RNA_struct_find_property(ptr, propname);
	if (!propPath || RNA_property_type(propPath) != PROP_STRING) {
		printf("uiTemplatePathBuilder: path property not found: %s\n", propname);
		return;
	}
	
	/* Start drawing UI Elements using standard defines */
	row= uiLayoutRow(layout, 1);
	
	/* Path (existing string) Widget */
	uiItemR(row, text, ICON_RNA, ptr, propname, 0);
	
	// TODO: attach something to this to make allow searching of nested properties to 'build' the path
}

/************************ Modifier Template *************************/

#define ERROR_LIBDATA_MESSAGE "Can't edit external libdata"

#include <string.h>

#include "DNA_object_force.h"
#include "DNA_object_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_report.h"

#include "UI_resources.h"
#include "ED_util.h"

#include "BLI_math.h"
#include "BLI_listbase.h"

#include "ED_object.h"

static void modifiers_setOnCage(bContext *C, void *ob_v, void *md_v)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = ob_v;
	ModifierData *md= md_v;
	int i, cageIndex = modifiers_getCageIndex(scene, ob, NULL, 0);

	/* undo button operation */
	md->mode ^= eModifierMode_OnCage;

	for(i = 0, md=ob->modifiers.first; md; ++i, md=md->next) {
		if(md == md_v) {
			if(i >= cageIndex)
				md->mode ^= eModifierMode_OnCage;
			break;
		}
	}

	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
}

static void modifiers_convertToReal(bContext *C, void *ob_v, void *md_v)
{
	Object *ob = ob_v;
	ModifierData *md = md_v;
	ModifierData *nmd = modifier_new(md->type);

	modifier_copyData(md, nmd);
	nmd->mode &= ~eModifierMode_Virtual;

	BLI_addhead(&ob->modifiers, nmd);
	
	modifier_unique_name(&ob->modifiers, nmd);

	ob->partype = PAROBJECT;

	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

	ED_undo_push(C, "Modifier convert to real");
}

static int modifier_can_delete(ModifierData *md)
{
	// fluid particle modifier can't be deleted here
	if(md->type == eModifierType_ParticleSystem)
		if(((ParticleSystemModifierData *)md)->psys->part->type == PART_FLUID)
			return 0;

	return 1;
}

static uiLayout *draw_modifier(uiLayout *layout, Scene *scene, Object *ob, ModifierData *md, int index, int cageIndex, int lastCageIndex, int compact)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	PointerRNA ptr;
	uiBut *but;
	uiBlock *block;
	uiLayout *box, *column, *row, *col;
	uiLayout *result= NULL;
	int isVirtual = (md->mode & eModifierMode_Virtual);
	char str[128];

	/* create RNA pointer */
	RNA_pointer_create(&ob->id, &RNA_Modifier, md, &ptr);

	column= uiLayoutColumn(layout, 1);
	uiLayoutSetContextPointer(column, "modifier", &ptr);

	/* rounded header ------------------------------------------------------------------- */
	box= uiLayoutBox(column);
	
	if (isVirtual) {
		row= uiLayoutRow(box, 0);
		uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_EXPAND);
		block= uiLayoutGetBlock(row);
		/* VIRTUAL MODIFIER */
		// XXX this is not used now, since these cannot be accessed via RNA
		sprintf(str, "%s parent deform", md->name);
		uiDefBut(block, LABEL, 0, str, 0, 0, 185, UI_UNIT_Y, NULL, 0.0, 0.0, 0.0, 0.0, "Modifier name"); 
		
		but = uiDefBut(block, BUT, 0, "Make Real", 0, 0, 80, 16, NULL, 0.0, 0.0, 0.0, 0.0, "Convert virtual modifier to a real modifier");
		uiButSetFunc(but, modifiers_convertToReal, ob, md);
	}
	else {
		/* REAL MODIFIER */
		uiLayout *split;
		
		split = uiLayoutSplit(box, 0.16, 0);
		
		col= uiLayoutColumn(split, 0);
		row = uiLayoutRow(col, 1);
		
		block = uiLayoutGetBlock(row);
		
		uiBlockSetEmboss(block, UI_EMBOSSN);
		
		/* Open/Close .................................  */
		uiItemR(row, "", 0, &ptr, "expanded", 0);
		
		/* modifier-type icon */
		uiItemL(row, "", RNA_struct_ui_icon(ptr.type));
		
		uiBlockSetEmboss(block, UI_EMBOSS);
		
	
		/* 'Middle Column' ............................ 
		 *	- first row is the name of the modifier 
		 *	- second row is the visibility settings, since the layouts were not wide enough to show all
		 */
		col= uiLayoutColumn(split, 0);
		
		row= uiLayoutRow(col, 0);
		uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_EXPAND);
		
		block = uiLayoutGetBlock(row);
		
		/* modifier name */
		uiItemR(row, "", 0, &ptr, "name", 0);
		
		if (compact) {
			/* insert delete button at end of top row before splitting to second line */
			uiBlockSetEmboss(block, UI_EMBOSSN);
			if (modifier_can_delete(md))
				uiItemO(row, "", ICON_X, "OBJECT_OT_modifier_remove");
			uiBlockSetEmboss(block, UI_EMBOSS);
			
			split = uiLayoutSplit(box, 0.17, 0);
			col= uiLayoutColumn(split, 0);
			uiItemL(col, "", 0);
			col= uiLayoutColumn(split, 0);
			row = uiLayoutRow(col, 1);
		}
		
		/* mode enabling buttons */
		uiBlockBeginAlign(block);
		/* Softbody not allowed in this situation, enforce! */
		if ( ((md->type!=eModifierType_Softbody && md->type!=eModifierType_Collision) || !(ob->pd && ob->pd->deflect)) 
			&& (md->type!=eModifierType_Surface) ) 
		{
			uiItemR(row, "", 0, &ptr, "render", 0);
			uiItemR(row, "", 0, &ptr, "realtime", 0);
			
			if (mti->flags & eModifierTypeFlag_SupportsEditmode)
				uiItemR(row, "", 0, &ptr, "editmode", 0);
		}
		if ((ob->type==OB_MESH) && modifier_couldBeCage(scene, md) && (index <= lastCageIndex)) 
		{
			/* -- convert to rna ? */
			but = uiDefIconButBitI(block, TOG, eModifierMode_OnCage, 0, ICON_MESH_DATA, 0, 0, 16, 20, &md->mode, 0.0, 0.0, 0.0, 0.0, "Apply modifier to editing cage during Editmode");
			if (index < cageIndex)
				uiButSetFlag(but, UI_BUT_DISABLED);
			uiButSetFunc(but, modifiers_setOnCage, ob, md);
		}
		uiBlockEndAlign(block);
		
		/* Up/Down + Delete ........................... */
		uiBlockBeginAlign(block);
		uiItemO(row, "", ICON_TRIA_UP, "OBJECT_OT_modifier_move_up");
		uiItemO(row, "", ICON_TRIA_DOWN, "OBJECT_OT_modifier_move_down");
		uiBlockEndAlign(block);
		
		if(!compact) {
			uiBlockSetEmboss(block, UI_EMBOSSN);
			if (modifier_can_delete(md))
				uiItemO(row, "", ICON_X, "OBJECT_OT_modifier_remove");
			uiBlockSetEmboss(block, UI_EMBOSS);
		}
	}

	
	/* modifier settings (under the header) --------------------------------------------------- */
	if (!isVirtual && (md->mode & eModifierMode_Expanded)) {
		/* apply/convert/copy */
		box= uiLayoutBox(column);
		row= uiLayoutRow(box, 0);
		
		if (!ELEM(md->type, eModifierType_Collision, eModifierType_Surface)) {
			/* only here obdata, the rest of modifiers is ob level */
			uiBlockSetButLock(block, object_data_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
			
			if (md->type==eModifierType_ParticleSystem) {
		    	ParticleSystem *psys= ((ParticleSystemModifierData *)md)->psys;
				
	    		if (!(ob->mode & OB_MODE_PARTICLE_EDIT) && psys->pathcache) {
					if(ELEM(psys->part->ren_as, PART_DRAW_GR, PART_DRAW_OB))
						uiItemO(row, "Convert", 0, "OBJECT_OT_duplicates_make_real");
					else if(psys->part->ren_as == PART_DRAW_PATH)
						uiItemO(row, "Convert", 0, "OBJECT_OT_modifier_convert");
				}
			}
			else {
				uiItemEnumO(row, "Apply", 0, "OBJECT_OT_modifier_apply", "apply_as", MODIFIER_APPLY_DATA);
				
				if (modifier_sameTopology(md))
					uiItemEnumO(row, "Apply as Shape", 0, "OBJECT_OT_modifier_apply", "apply_as", MODIFIER_APPLY_SHAPE);
			}
			
			uiBlockClearButLock(block);
			uiBlockSetButLock(block, ob && ob->id.lib, ERROR_LIBDATA_MESSAGE);
			
			if (!ELEM4(md->type, eModifierType_Fluidsim, eModifierType_Softbody, eModifierType_ParticleSystem, eModifierType_Cloth))
				uiItemO(row, "Copy", 0, "OBJECT_OT_modifier_copy");
		}
		
		/* result is the layout block inside the box, that we return so that modifier settings can be drawn */
		result= uiLayoutColumn(box, 0);
		block= uiLayoutAbsoluteBlock(box);
	}
	
	/* error messages */
	if(md->error) {
		box = uiLayoutBox(column);
		row = uiLayoutRow(box, 0);
		uiItemL(row, md->error, ICON_ERROR);
	}
	
	return result;
}

uiLayout *uiTemplateModifier(uiLayout *layout, bContext *C, PointerRNA *ptr, int compact)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob;
	ModifierData *md, *vmd;
	int i, lastCageIndex, cageIndex;

	/* verify we have valid data */
	if(!RNA_struct_is_a(ptr->type, &RNA_Modifier)) {
		printf("uiTemplateModifier: expected modifier on object.\n");
		return NULL;
	}

	ob= ptr->id.data;
	md= ptr->data;

	if(!ob || !(GS(ob->id.name) == ID_OB)) {
		printf("uiTemplateModifier: expected modifier on object.\n");
		return NULL;
	}
	
	uiBlockSetButLock(uiLayoutGetBlock(layout), (ob && ob->id.lib), ERROR_LIBDATA_MESSAGE);
	
	/* find modifier and draw it */
	cageIndex = modifiers_getCageIndex(scene, ob, &lastCageIndex, 0);

	// XXX virtual modifiers are not accesible for python
	vmd = modifiers_getVirtualModifierList(ob);

	for(i=0; vmd; i++, vmd=vmd->next) {
		if(md == vmd)
			return draw_modifier(layout, scene, ob, md, i, cageIndex, lastCageIndex, compact);
		else if(vmd->mode & eModifierMode_Virtual)
			i--;
	}

	return NULL;
}

/************************ Constraint Template *************************/

#include "DNA_action_types.h"
#include "DNA_constraint_types.h"

#include "BKE_action.h"
#include "BKE_constraint.h"

#define REDRAWIPO					1
#define REDRAWNLA					2
#define REDRAWBUTSOBJECT			3		
#define REDRAWACTION				4
#define B_CONSTRAINT_TEST			5
#define B_CONSTRAINT_CHANGETARGET	6
#define REMAKEIPO					8
#define B_DIFF						9

void do_constraint_panels(bContext *C, void *arg, int event)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	
	switch(event) {
	case B_CONSTRAINT_TEST:
		// XXX allqueue(REDRAWVIEW3D, 0);
		// XXX allqueue(REDRAWBUTSOBJECT, 0);
		// XXX allqueue(REDRAWBUTSEDIT, 0);
		break;  // no handling
	case B_CONSTRAINT_CHANGETARGET:
		if (ob->pose) ob->pose->flag |= POSE_RECALC;	// checks & sorts pose channels
		DAG_scene_sort(scene);
		break;
	default:
		break;
	}

	// note: RNA updates now call this, commenting else it gets called twice.
	// if there are problems because of this, then rna needs changed update functions.
	// 
	// object_test_constraints(ob);
	// if(ob->pose) update_pose_constraint_flags(ob->pose);
	
	if(ob->type==OB_ARMATURE) DAG_id_flush_update(&ob->id, OB_RECALC_DATA|OB_RECALC_OB);
	else DAG_id_flush_update(&ob->id, OB_RECALC_OB);

	WM_event_add_notifier(C, NC_OBJECT|ND_CONSTRAINT, ob);
	
	// XXX allqueue(REDRAWVIEW3D, 0);
	// XXX allqueue(REDRAWBUTSOBJECT, 0);
	// XXX allqueue(REDRAWBUTSEDIT, 0);
}

static void constraint_active_func(bContext *C, void *ob_v, void *con_v)
{
	ED_object_constraint_set_active(ob_v, con_v);
}


/* some commonly used macros in the constraints drawing code */
#define is_armature_target(target) (target && target->type==OB_ARMATURE)
#define is_armature_owner(ob) ((ob->type == OB_ARMATURE) && (ob->mode & OB_MODE_POSE))
#define is_geom_target(target) (target && (ELEM(target->type, OB_MESH, OB_LATTICE)) )

/* Helper function for draw constraint - draws constraint space stuff 
 * This function should not be called if no menus are required 
 * owner/target: -1 = don't draw menu; 0= not posemode, 1 = posemode 
 */
static void draw_constraint_spaceselect (uiBlock *block, bConstraint *con, short xco, short yco, short owner, short target)
{
	short tarx, ownx, iconx;
	short bwidth;
	short iconwidth = 20;
	
	/* calculate sizes and placement of menus */
	if (owner == -1) {
		bwidth = 125;
		tarx = 120;
		ownx = 0;
	}
	else if (target == -1) {
		bwidth = 125;
		tarx = 0;
		ownx = 120;
	}
	else {
		bwidth = 100;
		tarx = 85;
		iconx = tarx + bwidth + 5;
		ownx = tarx + bwidth + iconwidth + 10;
	}
	
	
	uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Convert:", xco, yco, 80,18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 

	/* Target-Space */
	if (target == 1) {
		uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Target Space %t|World Space %x0|Pose Space %x2|Local with Parent %x3|Local Space %x1", 
												tarx, yco, bwidth, 18, &con->tarspace, 0, 0, 0, 0, "Choose space that target is evaluated in");	
	}
	else if (target == 0) {
		uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Target Space %t|World Space %x0|Local (Without Parent) Space %x1", 
										tarx, yco, bwidth, 18, &con->tarspace, 0, 0, 0, 0, "Choose space that target is evaluated in");	
	}
	
	if ((target != -1) && (owner != -1))
		uiDefIconBut(block, LABEL, 0, ICON_ARROW_LEFTRIGHT,
			iconx, yco, 20, 20, NULL, 0.0, 0.0, 0.0, 0.0, "");
	
	/* Owner-Space */
	if (owner == 1) {
		uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Owner Space %t|World Space %x0|Pose Space %x2|Local with Parent %x3|Local Space %x1", 
												ownx, yco, bwidth, 18, &con->ownspace, 0, 0, 0, 0, "Choose space that owner is evaluated in");	
	}
	else if (owner == 0) {
		uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Owner Space %t|World Space %x0|Local (Without Parent) Space %x1", 
										ownx, yco, bwidth, 18, &con->ownspace, 0, 0, 0, 0, "Choose space that owner is evaluated in");	
	}
}

static void test_obpoin_but(bContext *C, char *name, ID **idpp)
{
	ID *id= BLI_findstring(&CTX_data_main(C)->object, name, offsetof(ID, name) + 2);
	*idpp= id; /* can be NULL */
	
	if(id)
		id_lib_extern(id);	/* checks lib data, sets correct flag for saving then */
}

/* draw panel showing settings for a constraint */
static uiLayout *draw_constraint(uiLayout *layout, Object *ob, bConstraint *con)
{
	bPoseChannel *pchan= get_active_posechannel(ob);
	bConstraintTypeInfo *cti;
	uiBlock *block;
	uiLayout *result= NULL, *col, *box, *row, *subrow;
	uiBut *but;
	PointerRNA ptr;
	char typestr[32];
	short width = 265;
	short proxy_protected, xco=0, yco=0;
	int rb_col;

	/* get constraint typeinfo */
	cti= constraint_get_typeinfo(con);
	if (cti == NULL) {
		/* exception for 'Null' constraint - it doesn't have constraint typeinfo! */
		if (con->type == CONSTRAINT_TYPE_NULL)
			strcpy(typestr, "Null");
		else
			strcpy(typestr, "Unknown");
	}
	else
		strcpy(typestr, cti->name);
		
	/* determine whether constraint is proxy protected or not */
	if (proxylocked_constraints_owner(ob, pchan))
		proxy_protected= (con->flag & CONSTRAINT_PROXY_LOCAL)==0;
	else
		proxy_protected= 0;

	/* unless button has own callback, it adds this callback to button */
	block= uiLayoutGetBlock(layout);
	uiBlockSetHandleFunc(block, do_constraint_panels, NULL);
	uiBlockSetFunc(block, constraint_active_func, ob, con);

	RNA_pointer_create(&ob->id, &RNA_Constraint, con, &ptr);

	col= uiLayoutColumn(layout, 1);
	uiLayoutSetContextPointer(col, "constraint", &ptr);

	box= uiLayoutBox(col);
	row= uiLayoutRow(box, 0);

	block= uiLayoutGetBlock(box);

	subrow= uiLayoutRow(row, 0);
	uiLayoutSetAlignment(subrow, UI_LAYOUT_ALIGN_LEFT);

	/* Draw constraint header */
	uiBlockSetEmboss(block, UI_EMBOSSN);
	
	/* rounded header */
	rb_col= (con->flag & CONSTRAINT_ACTIVE)?50:20;
	
	/* open/close */
	uiItemR(subrow, "", 0, &ptr, "expanded", UI_ITEM_R_ICON_ONLY);
	
	/* name */	
	uiBlockSetEmboss(block, UI_EMBOSS);
	
	/* XXX if (con->flag & CONSTRAINT_DISABLE)
		uiBlockSetCol(block, TH_REDALERT);*/
	
	uiDefBut(block, LABEL, B_CONSTRAINT_TEST, typestr, xco+10, yco, 100, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
	
	if(proxy_protected == 0) {
		uiItemR(subrow, "", 0, &ptr, "name", 0);
	}
	else
		uiItemL(subrow, con->name, 0);

	subrow= uiLayoutRow(row, 0);
	uiLayoutSetAlignment(subrow, UI_LAYOUT_ALIGN_RIGHT);
	
	/* proxy-protected constraints cannot be edited, so hide up/down + close buttons */
	if (proxy_protected) {
		uiBlockSetEmboss(block, UI_EMBOSSN);
		
		/* draw a ghost icon (for proxy) and also a lock beside it, to show that constraint is "proxy locked" */
		uiDefIconBut(block, BUT, B_CONSTRAINT_TEST, ICON_GHOST, xco+244, yco, 19, 19, NULL, 0.0, 0.0, 0.0, 0.0, "Proxy Protected");
		uiDefIconBut(block, BUT, B_CONSTRAINT_TEST, ICON_LOCKED, xco+262, yco, 19, 19, NULL, 0.0, 0.0, 0.0, 0.0, "Proxy Protected");
		
		uiBlockSetEmboss(block, UI_EMBOSS);
	}
	else {
		short prev_proxylock, show_upbut, show_downbut;
		
		/* Up/Down buttons: 
		 *	Proxy-constraints are not allowed to occur after local (non-proxy) constraints
		 *	as that poses problems when restoring them, so disable the "up" button where
		 *	it may cause this situation. 
		 *
		 * 	Up/Down buttons should only be shown (or not greyed - todo) if they serve some purpose. 
		 */
		if (proxylocked_constraints_owner(ob, pchan)) {
			if (con->prev) {
				prev_proxylock= (con->prev->flag & CONSTRAINT_PROXY_LOCAL) ? 0 : 1;
			}
			else
				prev_proxylock= 0;
		}
		else
			prev_proxylock= 0;
			
		show_upbut= ((prev_proxylock == 0) && (con->prev));
		show_downbut= (con->next) ? 1 : 0;
		
		if (show_upbut || show_downbut) {
			uiBlockBeginAlign(block);
				uiBlockSetEmboss(block, UI_EMBOSS);
				
				if (show_upbut)
					uiDefIconButO(block, BUT, "CONSTRAINT_OT_move_up", WM_OP_INVOKE_DEFAULT, ICON_TRIA_UP, xco+width-50, yco, 16, 18, "Move constraint up in constraint stack");
				
				if (show_downbut)
					uiDefIconButO(block, BUT, "CONSTRAINT_OT_move_down", WM_OP_INVOKE_DEFAULT, ICON_TRIA_DOWN, xco+width-50+18, yco, 16, 18, "Move constraint down in constraint stack");
			uiBlockEndAlign(block);
		}
	
		/* Close 'button' - emboss calls here disable drawing of 'button' behind X */
		uiBlockSetEmboss(block, UI_EMBOSSN);
			uiDefIconButBitS(block, ICONTOGN, CONSTRAINT_OFF, B_CONSTRAINT_TEST, ICON_CHECKBOX_DEHLT, xco+243, yco, 19, 19, &con->flag, 0.0, 0.0, 0.0, 0.0, "enable/disable constraint");
			
			uiDefIconButO(block, BUT, "CONSTRAINT_OT_delete", WM_OP_INVOKE_DEFAULT, ICON_X, xco+262, yco, 19, 19, "Delete constraint");
		uiBlockSetEmboss(block, UI_EMBOSS);
	}
	
	/* Set but-locks for protected settings (magic numbers are used here!) */
	if (proxy_protected)
		uiBlockSetButLock(block, 1, "Cannot edit Proxy-Protected Constraint");
	
	/* Draw constraint data */
	if ((con->flag & CONSTRAINT_EXPAND) == 0) {
		(yco) -= 21;
	}
	else {
		box= uiLayoutBox(col);
		block= uiLayoutAbsoluteBlock(box);

		switch (con->type) {
#ifndef DISABLE_PYTHON
		case CONSTRAINT_TYPE_PYTHON:
			{
				bPythonConstraint *data = con->data;
				bConstraintTarget *ct;
				// uiBut *but2;
				int tarnum, theight;
				// static int pyconindex=0;
				// char *menustr;
				
				theight = (data->tarnum)? (data->tarnum * 38) : (38);
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Script:", xco+60, yco-24, 55, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* do the scripts menu */
				/* XXX menustr = buildmenu_pyconstraints(data->text, &pyconindex);
				but2 = uiDefButI(block, MENU, B_CONSTRAINT_TEST, menustr,
				      xco+120, yco-24, 150, 20, &pyconindex,
				      0, 0, 0, 0, "Set the Script Constraint to use");
				uiButSetFunc(but2, validate_pyconstraint_cb, data, &pyconindex);
				MEM_freeN(menustr);	 */
				
				/* draw target(s) */
				if (data->flag & PYCON_USETARGETS) {
					/* Draw target parameters */ 
					for (ct=data->targets.first, tarnum=1; ct; ct=ct->next, tarnum++) {
						char tarstr[32];
						short yoffset= ((tarnum-1) * 38);
	
						/* target label */
						sprintf(tarstr, "Target %d:", tarnum);
						uiDefBut(block, LABEL, B_CONSTRAINT_TEST, tarstr, xco+45, yco-(48+yoffset), 100, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
						
						/* target space-selector - per target */
						if (is_armature_target(ct->tar)) {
							uiDefButS(block, MENU, B_CONSTRAINT_TEST, "Target Space %t|World Space %x0|Pose Space %x3|Local with Parent %x4|Local Space %x1", 
															xco+10, yco-(66+yoffset), 100, 18, &ct->space, 0, 0, 0, 0, "Choose space that target is evaluated in");	
						}
						else {
							uiDefButS(block, MENU, B_CONSTRAINT_TEST, "Target Space %t|World Space %x0|Local (Without Parent) Space %x1", 
															xco+10, yco-(66+yoffset), 100, 18, &ct->space, 0, 0, 0, 0, "Choose space that target is evaluated in");	
						}
						
						uiBlockBeginAlign(block);
							/* target object */
							uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", xco+120, yco-(48+yoffset), 150, 18, &ct->tar, "Target Object"); 
							
							/* subtarget */
							if (is_armature_target(ct->tar)) {
								but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", xco+120, yco-(66+yoffset),150,18, &ct->subtarget, 0, 24, 0, 0, "Subtarget Bone");
								//uiButSetCompleteFunc(but, autocomplete_bone, (void *)ct->tar);
							}
							else if (is_geom_target(ct->tar)) {
								but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", xco+120, yco-(66+yoffset),150,18, &ct->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
								//uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)ct->tar);
							}
							else {
								strcpy(ct->subtarget, "");
							}
						uiBlockEndAlign(block);
					}
				}
				else {
					/* Draw indication that no target needed */
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", xco+60, yco-48, 55, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Not Applicable", xco+120, yco-48, 150, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				}
				
				/* settings */
				uiBlockBeginAlign(block);
					but=uiDefBut(block, BUT, B_CONSTRAINT_TEST, "Options", xco, yco-(52+theight), (width/2),18, NULL, 0, 24, 0, 0, "Change some of the constraint's settings.");
					// XXX uiButSetFunc(but, BPY_pyconstraint_settings, data, NULL);
					
					but=uiDefBut(block, BUT, B_CONSTRAINT_TEST, "Refresh", xco+((width/2)+10), yco-(52+theight), (width/2),18, NULL, 0, 24, 0, 0, "Force constraint to refresh it's settings");
				uiBlockEndAlign(block);
				
				/* constraint space settings */
				draw_constraint_spaceselect(block, con, xco, yco-(73+theight), is_armature_owner(ob), -1);
			}
			break;
#endif 

		case CONSTRAINT_TYPE_NULL:
			{
				uiItemL(box, "", 0);
			}
			break;
		default:
			result= box;
			break;
		}
	}
	
	/* clear any locks set up for proxies/lib-linking */
	uiBlockClearButLock(block);

	return result;
}

uiLayout *uiTemplateConstraint(uiLayout *layout, PointerRNA *ptr)
{
	Object *ob;
	bConstraint *con;

	/* verify we have valid data */
	if(!RNA_struct_is_a(ptr->type, &RNA_Constraint)) {
		printf("uiTemplateConstraint: expected constraint on object.\n");
		return NULL;
	}

	ob= ptr->id.data;
	con= ptr->data;

	if(!ob || !(GS(ob->id.name) == ID_OB)) {
		printf("uiTemplateConstraint: expected constraint on object.\n");
		return NULL;
	}
	
	uiBlockSetButLock(uiLayoutGetBlock(layout), (ob && ob->id.lib), ERROR_LIBDATA_MESSAGE);

	/* hrms, the temporal constraint should not draw! */
	if(con->type==CONSTRAINT_TYPE_KINEMATIC) {
		bKinematicConstraint *data= con->data;
		if(data->flag & CONSTRAINT_IK_TEMP)
			return NULL;
	}

	return draw_constraint(layout, ob, con);
}


/************************* Preview Template ***************************/

#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_world_types.h"

#define B_MATPRV 1

static void do_preview_buttons(bContext *C, void *arg, int event)
{
	switch(event) {
		case B_MATPRV:
			WM_event_add_notifier(C, NC_MATERIAL|ND_SHADING, arg);
			break;
	}
}

void uiTemplatePreview(uiLayout *layout, ID *id, ID *parent, MTex *slot)
{
	uiLayout *row, *col;
	uiBlock *block;
	Material *ma= NULL;
	ID *pid, *pparent;
	short *pr_texture= NULL;

	if(id && !ELEM4(GS(id->name), ID_MA, ID_TE, ID_WO, ID_LA)) {
		printf("uiTemplatePreview: expected ID of type material, texture, lamp or world.\n");
		return;
	}

	/* decide what to render */
	pid= id;
	pparent= NULL;

	if(id && (GS(id->name) == ID_TE)) {
		if(parent && (GS(parent->name) == ID_MA))
			pr_texture= &((Material*)parent)->pr_texture;
		else if(parent && (GS(parent->name) == ID_WO))
			pr_texture= &((World*)parent)->pr_texture;
		else if(parent && (GS(parent->name) == ID_LA))
			pr_texture= &((Lamp*)parent)->pr_texture;

		if(pr_texture) {
			if(*pr_texture == TEX_PR_OTHER)
				pid= parent;
			else if(*pr_texture == TEX_PR_BOTH)
				pparent= parent;
		}
	}

	/* layout */
	block= uiLayoutGetBlock(layout);
	row= uiLayoutRow(layout, 0);
	col= uiLayoutColumn(row, 0);
	uiLayoutSetKeepAspect(col, 1);
	
	/* add preview */
	uiDefBut(block, BUT_EXTRA, 0, "", 0, 0, UI_UNIT_X*6, UI_UNIT_Y*6, pid, 0.0, 0.0, 0, 0, "");
	uiBlockSetDrawExtraFunc(block, ED_preview_draw, pparent, slot);
	uiBlockSetHandleFunc(block, do_preview_buttons, NULL);
	
	/* add buttons */
	if(pid) {
		if(GS(pid->name) == ID_MA || (pparent && GS(pparent->name) == ID_MA)) {
			if(GS(pid->name) == ID_MA) ma= (Material*)pid;
			else ma= (Material*)pparent;

			uiLayoutColumn(row, 1);

			uiDefIconButC(block, ROW, B_MATPRV, ICON_MATPLANE,  0, 0,UI_UNIT_X*1.5,UI_UNIT_Y, &(ma->pr_type), 10, MA_FLAT, 0, 0, "Preview type: Flat XY plane");
			uiDefIconButC(block, ROW, B_MATPRV, ICON_MATSPHERE, 0, 0,UI_UNIT_X*1.5,UI_UNIT_Y, &(ma->pr_type), 10, MA_SPHERE, 0, 0, "Preview type: Sphere");
			uiDefIconButC(block, ROW, B_MATPRV, ICON_MATCUBE,   0, 0,UI_UNIT_X*1.5,UI_UNIT_Y, &(ma->pr_type), 10, MA_CUBE, 0, 0, "Preview type: Cube");
			uiDefIconButC(block, ROW, B_MATPRV, ICON_MONKEY,    0, 0,UI_UNIT_X*1.5,UI_UNIT_Y, &(ma->pr_type), 10, MA_MONKEY, 0, 0, "Preview type: Monkey");
			uiDefIconButC(block, ROW, B_MATPRV, ICON_HAIR,      0, 0,UI_UNIT_X*1.5,UI_UNIT_Y, &(ma->pr_type), 10, MA_HAIR, 0, 0, "Preview type: Hair strands");
			uiDefIconButC(block, ROW, B_MATPRV, ICON_MAT_SPHERE_SKY, 0, 0,UI_UNIT_X*1.5,UI_UNIT_Y, &(ma->pr_type), 10, MA_SPHERE_A, 0, 0, "Preview type: Large sphere with sky");
		}

		if(pr_texture) {
			uiLayoutRow(layout, 1);

			uiDefButS(block, ROW, B_MATPRV, "Texture",  0, 0,UI_UNIT_X*10,UI_UNIT_Y, pr_texture, 10, TEX_PR_TEXTURE, 0, 0, "");
			if(GS(parent->name) == ID_MA)
				uiDefButS(block, ROW, B_MATPRV, "Material",  0, 0,UI_UNIT_X*10,UI_UNIT_Y, pr_texture, 10, TEX_PR_OTHER, 0, 0, "");
			else if(GS(parent->name) == ID_LA)
				uiDefButS(block, ROW, B_MATPRV, "Lamp",  0, 0,UI_UNIT_X*10,UI_UNIT_Y, pr_texture, 10, TEX_PR_OTHER, 0, 0, "");
			else if(GS(parent->name) == ID_WO)
				uiDefButS(block, ROW, B_MATPRV, "World",  0, 0,UI_UNIT_X*10,UI_UNIT_Y, pr_texture, 10, TEX_PR_OTHER, 0, 0, "");
			uiDefButS(block, ROW, B_MATPRV, "Both",  0, 0,UI_UNIT_X*10,UI_UNIT_Y, pr_texture, 10, TEX_PR_BOTH, 0, 0, "");
		}
	}
}

/********************** ColorRamp Template **************************/

#include "BKE_texture.h"

typedef struct RNAUpdateCb {
	PointerRNA ptr;
	PropertyRNA *prop;
} RNAUpdateCb;

static void rna_update_cb(bContext *C, void *arg_cb, void *arg_unused)
{
	RNAUpdateCb *cb= (RNAUpdateCb*)arg_cb;

	/* we call update here on the pointer property, this way the
	   owner of the curve mapping can still define it's own update
	   and notifier, even if the CurveMapping struct is shared. */
	RNA_property_update(C, &cb->ptr, cb->prop);
}

#define B_BANDCOL 1

static int vergcband(const void *a1, const void *a2)
{
	const CBData *x1=a1, *x2=a2;

	if( x1->pos > x2->pos ) return 1;
	else if( x1->pos < x2->pos) return -1;
	return 0;
}

static void colorband_pos_cb(bContext *C, void *cb_v, void *coba_v)
{
	ColorBand *coba= coba_v;
	int a;

	if(coba->tot<2) return;

	for(a=0; a<coba->tot; a++) coba->data[a].cur= a;
	qsort(coba->data, coba->tot, sizeof(CBData), vergcband);
	for(a=0; a<coba->tot; a++) {
		if(coba->data[a].cur==coba->cur) {
			coba->cur= a;
			break;
		}
	}

	rna_update_cb(C, cb_v, NULL);
}

static void colorband_add_cb(bContext *C, void *cb_v, void *coba_v)
{
	ColorBand *coba= coba_v;

	if(coba->tot < MAXCOLORBAND-1) coba->tot++;
	coba->cur= coba->tot-1;

	colorband_pos_cb(C, cb_v, coba_v);

	ED_undo_push(C, "Add colorband");
}

static void colorband_del_cb(bContext *C, void *cb_v, void *coba_v)
{
	ColorBand *coba= coba_v;
	int a;

	if(coba->tot<2) return;

	for(a=coba->cur; a<coba->tot; a++) {
		coba->data[a]= coba->data[a+1];
	}
	if(coba->cur) coba->cur--;
	coba->tot--;

	ED_undo_push(C, "Delete colorband");

	rna_update_cb(C, cb_v, NULL);
}


/* offset aligns from bottom, standard width 300, height 115 */
static void colorband_buttons_large(uiLayout *layout, uiBlock *block, ColorBand *coba, int xoffs, int yoffs, RNAUpdateCb *cb)
{
	
	uiBut *bt;
	uiLayout *row;

	if(coba==NULL) return;

	bt= uiDefBut(block, BUT, 0,	"Add",			0+xoffs,100+yoffs,50,20, 0, 0, 0, 0, 0, "Add a new color stop to the colorband");
	uiButSetNFunc(bt, colorband_add_cb, MEM_dupallocN(cb), coba);

	bt= uiDefBut(block, BUT, 0,	"Delete",		60+xoffs,100+yoffs,50,20, 0, 0, 0, 0, 0, "Delete the active position");
	uiButSetNFunc(bt, colorband_del_cb, MEM_dupallocN(cb), coba);

	uiDefButS(block, NUM, 0,		"",				120+xoffs,100+yoffs,80, 20, &coba->cur, 0.0, (float)(MAX2(0, coba->tot-1)), 0, 0, "Choose active color stop");

	bt= uiDefButS(block, MENU, 0,		"Interpolation %t|Ease %x1|Cardinal %x3|Linear %x0|B-Spline %x2|Constant %x4",
			210+xoffs, 100+yoffs, 90, 20,		&coba->ipotype, 0.0, 0.0, 0, 0, "Set interpolation between color stops");
	uiButSetNFunc(bt, rna_update_cb, MEM_dupallocN(cb), NULL);
	uiBlockEndAlign(block);

	bt= uiDefBut(block, BUT_COLORBAND, 0, "", 	xoffs,65+yoffs,300,30, coba, 0, 0, 0, 0, "");
	uiButSetNFunc(bt, rna_update_cb, MEM_dupallocN(cb), NULL);

	if(coba->tot) {
		CBData *cbd= coba->data + coba->cur;

		/* better to use rna so we can animate them */
		PointerRNA ptr;
		RNA_pointer_create(cb->ptr.id.data, &RNA_ColorRampElement, cbd, &ptr);
		row= uiLayoutRow(layout, 0);
		uiItemR(row, "Pos", 0, &ptr, "position", 0);
		uiItemR(row, "", 0, &ptr, "color", 0);
	}

}

static void colorband_buttons_small(uiLayout *layout, uiBlock *block, ColorBand *coba, rctf *butr, RNAUpdateCb *cb)
{
	uiBut *bt;
	float unit= (butr->xmax-butr->xmin)/14.0f;
	float xs= butr->xmin;

	uiBlockBeginAlign(block);
	bt= uiDefBut(block, BUT, 0,	"Add",			xs,butr->ymin+20.0f,2.0f*unit,20,	NULL, 0, 0, 0, 0, "Add a new color stop to the colorband");
	uiButSetNFunc(bt, colorband_add_cb, MEM_dupallocN(cb), coba);
	bt= uiDefBut(block, BUT, 0,	"Delete",		xs+2.0f*unit,butr->ymin+20.0f,2.0f*unit,20,	NULL, 0, 0, 0, 0, "Delete the active position");
	uiButSetNFunc(bt, colorband_del_cb, MEM_dupallocN(cb), coba);
	uiBlockEndAlign(block);

	if(coba->tot) {
		CBData *cbd= coba->data + coba->cur;
		PointerRNA ptr;
		RNA_pointer_create(cb->ptr.id.data, &RNA_ColorRampElement, cbd, &ptr);
		uiItemR(layout, "", 0, &ptr, "color", 0);
	}

	bt= uiDefButS(block, MENU, 0,		"Interpolation %t|Ease %x1|Cardinal %x3|Linear %x0|B-Spline %x2|Constant %x4",
			xs+10.0f*unit, butr->ymin+20.0f, unit*4, 20,		&coba->ipotype, 0.0, 0.0, 0, 0, "Set interpolation between color stops");
	uiButSetNFunc(bt, rna_update_cb, MEM_dupallocN(cb), NULL);

	bt= uiDefBut(block, BUT_COLORBAND, 0, "",		xs,butr->ymin,butr->xmax-butr->xmin,20.0f, coba, 0, 0, 0, 0, "");
	uiButSetNFunc(bt, rna_update_cb, MEM_dupallocN(cb), NULL);

	uiBlockEndAlign(block);
}

static void colorband_buttons_layout(uiLayout *layout, uiBlock *block, ColorBand *coba, rctf *butr, int small, RNAUpdateCb *cb)
{
	if(small)
		colorband_buttons_small(layout, block, coba, butr, cb);
	else
		colorband_buttons_large(layout, block, coba, 0, 0, cb);
}

void uiTemplateColorRamp(uiLayout *layout, PointerRNA *ptr, char *propname, int expand)
{
	PropertyRNA *prop= RNA_struct_find_property(ptr, propname);
	PointerRNA cptr;
	RNAUpdateCb *cb;
	uiBlock *block;
	rctf rect;

	if(!prop || RNA_property_type(prop) != PROP_POINTER)
		return;

	cptr= RNA_property_pointer_get(ptr, prop);
	if(!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_ColorRamp))
		return;

	cb= MEM_callocN(sizeof(RNAUpdateCb), "RNAUpdateCb");
	cb->ptr= *ptr;
	cb->prop= prop;

	rect.xmin= 0; rect.xmax= 200;
	rect.ymin= 0; rect.ymax= 190;

	block= uiLayoutAbsoluteBlock(layout);
	colorband_buttons_layout(layout, block, cptr.data, &rect, !expand, cb);

	MEM_freeN(cb);
}

/********************* Histogram Template ************************/

void uiTemplateHistogram(uiLayout *layout, PointerRNA *ptr, char *propname, int expand)
{
	PropertyRNA *prop= RNA_struct_find_property(ptr, propname);
	PointerRNA cptr;
	RNAUpdateCb *cb;
	uiBlock *block;
	uiBut *bt;
	Histogram *hist;
	rctf rect;
	
	if(!prop || RNA_property_type(prop) != PROP_POINTER)
		return;
	
	cptr= RNA_property_pointer_get(ptr, prop);
	if(!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_Histogram))
		return;
	
	cb= MEM_callocN(sizeof(RNAUpdateCb), "RNAUpdateCb");
	cb->ptr= *ptr;
	cb->prop= prop;
	
	rect.xmin= 0; rect.xmax= 200;
	rect.ymin= 0; rect.ymax= 190;
	
	block= uiLayoutAbsoluteBlock(layout);
	//colorband_buttons_layout(layout, block, cptr.data, &rect, !expand, cb);
	
	hist = (Histogram *)cptr.data;
	
	bt= uiDefBut(block, HISTOGRAM, 0, "",		rect.xmin, rect.ymin, rect.xmax-rect.xmin, 100.0f, hist, 0, 0, 0, 0, "");
	uiButSetNFunc(bt, rna_update_cb, MEM_dupallocN(cb), NULL);
	
	MEM_freeN(cb);
}

/********************* CurveMapping Template ************************/

#include "BKE_colortools.h"

static void curvemap_buttons_zoom_in(bContext *C, void *cumap_v, void *unused)
{
	CurveMapping *cumap = cumap_v;
	float d;

	/* we allow 20 times zoom */
	if( (cumap->curr.xmax - cumap->curr.xmin) > 0.04f*(cumap->clipr.xmax - cumap->clipr.xmin) ) {
		d= 0.1154f*(cumap->curr.xmax - cumap->curr.xmin);
		cumap->curr.xmin+= d;
		cumap->curr.xmax-= d;
		d= 0.1154f*(cumap->curr.ymax - cumap->curr.ymin);
		cumap->curr.ymin+= d;
		cumap->curr.ymax-= d;
	}

	ED_region_tag_redraw(CTX_wm_region(C));
}

static void curvemap_buttons_zoom_out(bContext *C, void *cumap_v, void *unused)
{
	CurveMapping *cumap = cumap_v;
	float d, d1;

	/* we allow 20 times zoom, but dont view outside clip */
	if( (cumap->curr.xmax - cumap->curr.xmin) < 20.0f*(cumap->clipr.xmax - cumap->clipr.xmin) ) {
		d= d1= 0.15f*(cumap->curr.xmax - cumap->curr.xmin);

		if(cumap->flag & CUMA_DO_CLIP) 
			if(cumap->curr.xmin-d < cumap->clipr.xmin)
				d1= cumap->curr.xmin - cumap->clipr.xmin;
		cumap->curr.xmin-= d1;

		d1= d;
		if(cumap->flag & CUMA_DO_CLIP) 
			if(cumap->curr.xmax+d > cumap->clipr.xmax)
				d1= -cumap->curr.xmax + cumap->clipr.xmax;
		cumap->curr.xmax+= d1;

		d= d1= 0.15f*(cumap->curr.ymax - cumap->curr.ymin);

		if(cumap->flag & CUMA_DO_CLIP) 
			if(cumap->curr.ymin-d < cumap->clipr.ymin)
				d1= cumap->curr.ymin - cumap->clipr.ymin;
		cumap->curr.ymin-= d1;

		d1= d;
		if(cumap->flag & CUMA_DO_CLIP) 
			if(cumap->curr.ymax+d > cumap->clipr.ymax)
				d1= -cumap->curr.ymax + cumap->clipr.ymax;
		cumap->curr.ymax+= d1;
	}

	ED_region_tag_redraw(CTX_wm_region(C));
}

static void curvemap_buttons_setclip(bContext *C, void *cumap_v, void *unused)
{
	CurveMapping *cumap = cumap_v;

	curvemapping_changed(cumap, 0);
}	

static void curvemap_buttons_delete(bContext *C, void *cb_v, void *cumap_v)
{
	CurveMapping *cumap = cumap_v;

	curvemap_remove(cumap->cm+cumap->cur, SELECT);
	curvemapping_changed(cumap, 0);

	rna_update_cb(C, cb_v, NULL);
}

/* NOTE: this is a block-menu, needs 0 events, otherwise the menu closes */
static uiBlock *curvemap_clipping_func(bContext *C, struct ARegion *ar, void *cumap_v)
{
	CurveMapping *cumap = cumap_v;
	uiBlock *block;
	uiBut *bt;

	block= uiBeginBlock(C, ar, "curvemap_clipping_func", UI_EMBOSS);

	/* use this for a fake extra empy space around the buttons */
	uiDefBut(block, LABEL, 0, "",			-4, 16, 128, 106, NULL, 0, 0, 0, 0, "");

	bt= uiDefButBitI(block, TOG, CUMA_DO_CLIP, 1, "Use Clipping",	 
			0,100,120,18, &cumap->flag, 0.0, 0.0, 10, 0, "");
	uiButSetFunc(bt, curvemap_buttons_setclip, cumap, NULL);

	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, 0, "Min X ",	 0,74,120,18, &cumap->clipr.xmin, -100.0, cumap->clipr.xmax, 10, 0, "");
	uiDefButF(block, NUM, 0, "Min Y ",	 0,56,120,18, &cumap->clipr.ymin, -100.0, cumap->clipr.ymax, 10, 0, "");
	uiDefButF(block, NUM, 0, "Max X ",	 0,38,120,18, &cumap->clipr.xmax, cumap->clipr.xmin, 100.0, 10, 0, "");
	uiDefButF(block, NUM, 0, "Max Y ",	 0,20,120,18, &cumap->clipr.ymax, cumap->clipr.ymin, 100.0, 10, 0, "");

	uiBlockSetDirection(block, UI_RIGHT);

	uiEndBlock(C, block);
	return block;
}

static void curvemap_tools_dofunc(bContext *C, void *cumap_v, int event)
{
	CurveMapping *cumap = cumap_v;
	CurveMap *cuma= cumap->cm+cumap->cur;

	switch(event) {
		case 0: /* reset */
			curvemap_reset(cuma, &cumap->clipr, CURVE_PRESET_LINE);
			curvemapping_changed(cumap, 0);
			break;
		case 1:
			cumap->curr= cumap->clipr;
			break;
		case 2:	/* set vector */
			curvemap_sethandle(cuma, 1);
			curvemapping_changed(cumap, 0);
			break;
		case 3: /* set auto */
			curvemap_sethandle(cuma, 0);
			curvemapping_changed(cumap, 0);
			break;
		case 4: /* extend horiz */
			cuma->flag &= ~CUMA_EXTEND_EXTRAPOLATE;
			curvemapping_changed(cumap, 0);
			break;
		case 5: /* extend extrapolate */
			cuma->flag |= CUMA_EXTEND_EXTRAPOLATE;
			curvemapping_changed(cumap, 0);
			break;
		case 6: /* reset smooth */
			curvemap_reset(cuma, &cumap->clipr, CURVE_PRESET_SMOOTH);
			curvemapping_changed(cumap, 0);
			break;
	}
	ED_region_tag_redraw(CTX_wm_region(C));
}

static uiBlock *curvemap_tools_func(bContext *C, struct ARegion *ar, void *cumap_v)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiBeginBlock(C, ar, "curvemap_tools_func", UI_EMBOSS);
	uiBlockSetButmFunc(block, curvemap_tools_dofunc, cumap_v);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Reset View",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Vector Handle",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Auto Handle",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Extend Horizontal",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Extend Extrapolated",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Reset Curve",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 50);

	uiEndBlock(C, block);
	return block;
}

static uiBlock *curvemap_brush_tools_func(bContext *C, struct ARegion *ar, void *cumap_v)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiBeginBlock(C, ar, "curvemap_tools_func", UI_EMBOSS);
	uiBlockSetButmFunc(block, curvemap_tools_dofunc, cumap_v);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Reset View",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Vector Handle",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Auto Handle",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Reset Curve",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 50);

	uiEndBlock(C, block);
	return block;
}

static void curvemap_buttons_redraw(bContext *C, void *arg1, void *arg2)
{
	ED_region_tag_redraw(CTX_wm_region(C));
}

static void curvemap_buttons_reset(bContext *C, void *cb_v, void *cumap_v)
{
	CurveMapping *cumap = cumap_v;
	int a;
	
	for(a=0; a<CM_TOT; a++)
		curvemap_reset(cumap->cm+a, &cumap->clipr, CURVE_PRESET_LINE);
	
	cumap->black[0]=cumap->black[1]=cumap->black[2]= 0.0f;
	cumap->white[0]=cumap->white[1]=cumap->white[2]= 1.0f;
	curvemapping_set_black_white(cumap, NULL, NULL);
	
	curvemapping_changed(cumap, 0);

	rna_update_cb(C, cb_v, NULL);
}

/* still unsure how this call evolves... we use labeltype for defining what curve-channels to show */
static void curvemap_buttons_layout(uiLayout *layout, PointerRNA *ptr, char labeltype, int levels, int brush, RNAUpdateCb *cb)
{
	CurveMapping *cumap= ptr->data;
	uiLayout *row, *sub, *split;
	uiBlock *block;
	uiBut *bt;
	float dx= UI_UNIT_X;
	int icon, size;
	int bg=-1;

	block= uiLayoutGetBlock(layout);

	/* curve chooser */
	row= uiLayoutRow(layout, 0);

	if(labeltype=='v') {
		/* vector */
		sub= uiLayoutRow(row, 1);
		uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_LEFT);

		if(cumap->cm[0].curve) {
			bt= uiDefButI(block, ROW, 0, "X", 0, 0, dx, 16, &cumap->cur, 0.0, 0.0, 0.0, 0.0, "");
			uiButSetFunc(bt, curvemap_buttons_redraw, NULL, NULL);
		}
		if(cumap->cm[1].curve) {
			bt= uiDefButI(block, ROW, 0, "Y", 0, 0, dx, 16, &cumap->cur, 0.0, 1.0, 0.0, 0.0, "");
			uiButSetFunc(bt, curvemap_buttons_redraw, NULL, NULL);
		}
		if(cumap->cm[2].curve) {
			bt= uiDefButI(block, ROW, 0, "Z", 0, 0, dx, 16, &cumap->cur, 0.0, 2.0, 0.0, 0.0, "");
			uiButSetFunc(bt, curvemap_buttons_redraw, NULL, NULL);
		}
	}
	else if(labeltype=='c') {
		/* color */
		sub= uiLayoutRow(row, 1);
		uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_LEFT);

		if(cumap->cm[3].curve) {
			bt= uiDefButI(block, ROW, 0, "C", 0, 0, dx, 16, &cumap->cur, 0.0, 3.0, 0.0, 0.0, "");
			uiButSetFunc(bt, curvemap_buttons_redraw, NULL, NULL);
		}
		if(cumap->cm[0].curve) {
			bt= uiDefButI(block, ROW, 0, "R", 0, 0, dx, 16, &cumap->cur, 0.0, 0.0, 0.0, 0.0, "");
			uiButSetFunc(bt, curvemap_buttons_redraw, NULL, NULL);
		}
		if(cumap->cm[1].curve) {
			bt= uiDefButI(block, ROW, 0, "G", 0, 0, dx, 16, &cumap->cur, 0.0, 1.0, 0.0, 0.0, "");
			uiButSetFunc(bt, curvemap_buttons_redraw, NULL, NULL);
		}
		if(cumap->cm[2].curve) {
			bt= uiDefButI(block, ROW, 0, "B", 0, 0, dx, 16, &cumap->cur, 0.0, 2.0, 0.0, 0.0, "");
			uiButSetFunc(bt, curvemap_buttons_redraw, NULL, NULL);
		}
	}
	else if (labeltype == 'h') {
		/* HSV */
		sub= uiLayoutRow(row, 1);
		uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_LEFT);
		
		if(cumap->cm[0].curve) {
			bt= uiDefButI(block, ROW, 0, "H", 0, 0, dx, 16, &cumap->cur, 0.0, 0.0, 0.0, 0.0, "");
			uiButSetFunc(bt, curvemap_buttons_redraw, NULL, NULL);
		}
		if(cumap->cm[1].curve) {
			bt= uiDefButI(block, ROW, 0, "S", 0, 0, dx, 16, &cumap->cur, 0.0, 1.0, 0.0, 0.0, "");
			uiButSetFunc(bt, curvemap_buttons_redraw, NULL, NULL);
		}
		if(cumap->cm[2].curve) {
			bt= uiDefButI(block, ROW, 0, "V", 0, 0, dx, 16, &cumap->cur, 0.0, 2.0, 0.0, 0.0, "");
			uiButSetFunc(bt, curvemap_buttons_redraw, NULL, NULL);
		}
	}
	else
		uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_RIGHT);
	
	if (labeltype=='h')
		bg = UI_GRAD_H;

	/* operation buttons */
	sub= uiLayoutRow(row, 1);

	uiBlockSetEmboss(block, UI_EMBOSSN);

	bt= uiDefIconBut(block, BUT, 0, ICON_ZOOMIN, 0, 0, dx, 14, NULL, 0.0, 0.0, 0.0, 0.0, "Zoom in");
	uiButSetFunc(bt, curvemap_buttons_zoom_in, cumap, NULL);

	bt= uiDefIconBut(block, BUT, 0, ICON_ZOOMOUT, 0, 0, dx, 14, NULL, 0.0, 0.0, 0.0, 0.0, "Zoom out");
	uiButSetFunc(bt, curvemap_buttons_zoom_out, cumap, NULL);

	if(brush)
		bt= uiDefIconBlockBut(block, curvemap_brush_tools_func, cumap, 0, ICON_MODIFIER, 0, 0, dx, 18, "Tools");
	else
		bt= uiDefIconBlockBut(block, curvemap_tools_func, cumap, 0, ICON_MODIFIER, 0, 0, dx, 18, "Tools");

	uiButSetNFunc(bt, rna_update_cb, MEM_dupallocN(cb), NULL);

	if(cumap->flag & CUMA_DO_CLIP) icon= ICON_CLIPUV_HLT; else icon= ICON_CLIPUV_DEHLT;
	bt= uiDefIconBlockBut(block, curvemap_clipping_func, cumap, 0, icon, 0, 0, dx, 18, "Clipping Options");
	uiButSetNFunc(bt, rna_update_cb, MEM_dupallocN(cb), NULL);

	bt= uiDefIconBut(block, BUT, 0, ICON_X, 0, 0, dx, 18, NULL, 0.0, 0.0, 0.0, 0.0, "Delete points");
	uiButSetNFunc(bt, curvemap_buttons_delete, MEM_dupallocN(cb), cumap);

	uiBlockSetEmboss(block, UI_EMBOSS);

	uiBlockSetNFunc(block, rna_update_cb, MEM_dupallocN(cb), NULL);

	/* curve itself */
	size= uiLayoutGetWidth(layout);
	row= uiLayoutRow(layout, 0);
	uiDefBut(block, BUT_CURVE, 0, "", 0, 0, size, MIN2(size, 200), cumap, 0.0f, 1.0f, bg, 0, "");

	/* black/white levels */
	if(levels) {
		split= uiLayoutSplit(layout, 0, 0);
		uiItemR(uiLayoutColumn(split, 0), NULL, 0, ptr, "black_level", UI_ITEM_R_EXPAND);
		uiItemR(uiLayoutColumn(split, 0), NULL, 0, ptr, "white_level", UI_ITEM_R_EXPAND);

		uiLayoutRow(layout, 0);
		bt=uiDefBut(block, BUT, 0, "Reset",	0, 0, UI_UNIT_X*10, UI_UNIT_Y, NULL, 0.0f, 0.0f, 0, 0, "Reset Black/White point and curves");
		uiButSetNFunc(bt, curvemap_buttons_reset, MEM_dupallocN(cb), cumap);
	}

	uiBlockSetNFunc(block, NULL, NULL, NULL);
}

void uiTemplateCurveMapping(uiLayout *layout, PointerRNA *ptr, char *propname, int type, int levels, int brush)
{
	RNAUpdateCb *cb;
	PropertyRNA *prop= RNA_struct_find_property(ptr, propname);
	PointerRNA cptr;

	if(!prop || RNA_property_type(prop) != PROP_POINTER)
		return;

	cptr= RNA_property_pointer_get(ptr, prop);
	if(!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_CurveMapping))
		return;

	cb= MEM_callocN(sizeof(RNAUpdateCb), "RNAUpdateCb");
	cb->ptr= *ptr;
	cb->prop= prop;

	curvemap_buttons_layout(layout, &cptr, type, levels, brush, cb);

	MEM_freeN(cb);
}

/********************* ColorWheel Template ************************/

#define WHEEL_SIZE	100

void uiTemplateColorWheel(uiLayout *layout, PointerRNA *ptr, char *propname, int value_slider, int lock)
{
	PropertyRNA *prop= RNA_struct_find_property(ptr, propname);
	uiBlock *block= uiLayoutGetBlock(layout);
	uiLayout *col, *row;
	float softmin, softmax, step, precision;
	
	if (!prop) {
		printf("uiTemplateColorWheel: property not found: %s\n", propname);
		return;
	}
	
	RNA_property_float_ui_range(ptr, prop, &softmin, &softmax, &step, &precision);
	
	col = uiLayoutColumn(layout, 0);
	row= uiLayoutRow(col, 1);
	
	uiDefButR(block, HSVCIRCLE, 0, "",	0, 0, WHEEL_SIZE, WHEEL_SIZE, ptr, propname, -1, 0.0, 0.0, 0, lock, "");
	
	uiItemS(row);
	
	if (value_slider)
		uiDefButR(block, HSVCUBE, 0, "", WHEEL_SIZE+6, 0, 14, WHEEL_SIZE, ptr, propname, -1, softmin, softmax, 9, 0, "");

	/* maybe a switch for this?
	row= uiLayoutRow(col, 0);
	if(ELEM(RNA_property_subtype(prop), PROP_COLOR, PROP_COLOR_GAMMA) && RNA_property_array_length(ptr, prop) == 4) {
		but= uiDefAutoButR(block, ptr, prop, 3, "A:", 0, 0, 0, WHEEL_SIZE+20, UI_UNIT_Y);
	}
	*/
	
}


/********************* TriColor (ThemeWireColorSet) Template ************************/

void uiTemplateTriColorSet(uiLayout *layout, PointerRNA *ptr, char *propname)
{
	PropertyRNA *prop= RNA_struct_find_property(ptr, propname);
	uiLayout *row;
	PointerRNA csPtr;

	if (!prop) {
		printf("uiTemplateTriColorSet: property not found: %s\n", propname);
		return;
	}
	
	/* we lay out the data in a row as 3 color swatches */
	row= uiLayoutRow(layout, 1);
	
	/* nselected, selected, active color swatches */
	csPtr= RNA_property_pointer_get(ptr, prop);
	
	uiItemR(row, "", 0, &csPtr, "normal", 0);
	uiItemR(row, "", 0, &csPtr, "selected", 0);
	uiItemR(row, "", 0, &csPtr, "active", 0);
}

/********************* Layer Buttons Template ************************/

static void handle_layer_buttons(bContext *C, void *arg1, void *arg2)
{
	uiBut *but = arg1;
	int cur = GET_INT_FROM_POINTER(arg2);
	wmWindow *win= CTX_wm_window(C);
	int i, tot, shift= win->eventstate->shift;

	if(!shift) {
		tot= RNA_property_array_length(&but->rnapoin, but->rnaprop);
		
		/* Normally clicking only selects one layer */
		RNA_property_boolean_set_index(&but->rnapoin, but->rnaprop, cur, 1);
		for(i = 0; i < tot; ++i) {
			if(i != cur)
				RNA_property_boolean_set_index(&but->rnapoin, but->rnaprop, i, 0);
		}
	}
}

// TODO:
//	- for now, grouping of layers is determined by dividing up the length of 
//	  the array of layer bitflags

void uiTemplateLayers(uiLayout *layout, PointerRNA *ptr, char *propname,
		      PointerRNA *used_ptr, char *used_propname, int active_layer)
{
	uiLayout *uRow, *uCol;
	PropertyRNA *prop, *used_prop= NULL;
	int groups, cols, layers;
	int group, col, layer, row;
	int cols_per_group = 5;
	const char *desc;
	
	prop= RNA_struct_find_property(ptr, propname);
	if (!prop) {
		printf("uiTemplateLayer: layers property not found: %s\n", propname);
		return;
	}

	desc= RNA_property_description(prop);
	
	/* the number of layers determines the way we group them 
	 *	- we want 2 rows only (for now)
	 *	- the number of columns (cols) is the total number of buttons per row
	 *	  the 'remainder' is added to this, as it will be ok to have first row slightly wider if need be
	 *	- for now, only split into groups if group will have at least 5 items
	 */
	layers= RNA_property_array_length(ptr, prop);
	cols= (layers / 2) + (layers % 2);
	groups= ((cols / 2) < cols_per_group) ? (1) : (cols / cols_per_group);

	if(used_ptr && used_propname) {
		used_prop= RNA_struct_find_property(used_ptr, used_propname);
		if (!used_prop) {
			printf("uiTemplateLayer: used layers property not found: %s\n", used_propname);
			return;
		}

		if(RNA_property_array_length(used_ptr, used_prop) < layers)
			used_prop = NULL;
	}
	
	/* layers are laid out going across rows, with the columns being divided into groups */
	
	for (group= 0; group < groups; group++) {
		uCol= uiLayoutColumn(layout, 1);
		
		for (row= 0; row < 2; row++) {
			uiBlock *block;
			uiBut *but;

			uRow= uiLayoutRow(uCol, 1);
			block= uiLayoutGetBlock(uRow);
			layer= groups*cols_per_group*row + cols_per_group*group;
			
			/* add layers as toggle buts */
			for (col= 0; (col < cols_per_group) && (layer < layers); col++, layer++) {
				int icon = 0;
				int butlay = 1 << layer;

				if(active_layer & butlay)
					icon = ICON_LAYER_ACTIVE;
				else if(used_prop && RNA_property_boolean_get_index(used_ptr, used_prop, layer))
					icon = ICON_LAYER_USED;
				
				but= uiDefAutoButR(block, ptr, prop, layer, "", icon, 0, 0, 10, 10);
				uiButSetFunc(but, handle_layer_buttons, but, SET_INT_IN_POINTER(layer));
				but->type= TOG;
			}
		}
	}
}


/************************* List Template **************************/

static int list_item_icon_get(bContext *C, PointerRNA *itemptr, int rnaicon)
{
	ID *id= NULL;
	int icon;

	if(!itemptr->data)
		return rnaicon;

	/* try ID, material or texture slot */
	if(RNA_struct_is_ID(itemptr->type)) {
		id= itemptr->id.data;
	}
	else if(RNA_struct_is_a(itemptr->type, &RNA_MaterialSlot)) {
		id= RNA_pointer_get(itemptr, "material").data;
	}
	else if(RNA_struct_is_a(itemptr->type, &RNA_TextureSlot)) {
		id= RNA_pointer_get(itemptr, "texture").data;
	}

	/* get icon from ID */
	if(id) {
		icon= ui_id_icon_get(C, id, 1);

		if(icon)
			return icon;
	}

	return rnaicon;
}

static void list_item_row(bContext *C, uiLayout *layout, PointerRNA *ptr, PointerRNA *itemptr, int i, int rnaicon, PointerRNA *activeptr, char *activepropname)
{
	Object *ob;
	uiBlock *block= uiLayoutGetBlock(layout);
	uiBut *but;
	uiLayout *split, *overlap, *sub, *row;
	char *name, *namebuf;
	int icon;

	overlap= uiLayoutOverlap(layout);

	/* list item behind label & other buttons */
	sub= uiLayoutRow(overlap, 0);

	if(itemptr->type == &RNA_ShapeKey) {
		ob= (Object*)activeptr->data;
		if(ob->mode == OB_MODE_EDIT && !(ob->type == OB_MESH))
			uiLayoutSetEnabled(sub, 0);
	}

	but= uiDefButR(block, LISTROW, 0, "", 0,0, UI_UNIT_X*10,UI_UNIT_Y, activeptr, activepropname, 0, 0, i, 0, 0, "");
	uiButSetFlag(but, UI_BUT_NO_TOOLTIP);

	sub= uiLayoutRow(overlap, 0);

	/* retrieve icon and name */
	icon= list_item_icon_get(C, itemptr, rnaicon);
	if(!icon || icon == ICON_DOT)
		icon= 0;

	namebuf= RNA_struct_name_get_alloc(itemptr, NULL, 0);
	name= (namebuf)? namebuf: "";

	/* hardcoded types */
	if(itemptr->type == &RNA_MeshTextureFaceLayer || itemptr->type == &RNA_MeshColorLayer) {
		uiItemL(sub, name, icon);
		uiBlockSetEmboss(block, UI_EMBOSSN);
		uiDefIconButR(block, TOG, 0, ICON_SCENE, 0, 0, UI_UNIT_X, UI_UNIT_Y, itemptr, "active_render", 0, 0, 0, 0, 0, NULL);
		uiBlockSetEmboss(block, UI_EMBOSS);
	}
	else if(RNA_struct_is_a(itemptr->type, &RNA_MaterialTextureSlot)) {
		uiItemL(sub, name, icon);
		uiBlockSetEmboss(block, UI_EMBOSS);
		uiDefButR(block, OPTION, 0, "", 0, 0, UI_UNIT_X, UI_UNIT_Y, ptr, "use_textures", i, 0, 0, 0, 0,  NULL);
	}
	else if(RNA_struct_is_a(itemptr->type, &RNA_SceneRenderLayer)) {
		uiItemL(sub, name, icon);
		uiBlockSetEmboss(block, UI_EMBOSS);
		uiDefButR(block, OPTION, 0, "", 0, 0, UI_UNIT_X, UI_UNIT_Y, itemptr, "enabled", 0, 0, 0, 0, 0,  NULL);
	}
	else if(itemptr->type == &RNA_ShapeKey) {
		ob= (Object*)activeptr->data;

		split= uiLayoutSplit(sub, 0.75f, 0);

		uiItemL(split, name, icon);

		uiBlockSetEmboss(block, UI_EMBOSSN);
		row= uiLayoutRow(split, 1);
		if(i == 0) uiItemL(row, "", 0);
		else uiItemR(row, "", 0, itemptr, "value", 0);

		if(ob->mode == OB_MODE_EDIT && !((ob->shapeflag & OB_SHAPE_EDIT_MODE) && ob->type == OB_MESH))
			uiLayoutSetActive(row, 0);
		//uiItemR(row, "", ICON_MUTE_IPO_OFF, itemptr, "mute", 0);
		uiBlockSetEmboss(block, UI_EMBOSS);
	}
	else
		uiItemL(sub, name, icon); /* fails, backdrop LISTROW... */

	/* free name */
	if(namebuf)
		MEM_freeN(namebuf);
}

void uiTemplateList(uiLayout *layout, bContext *C, PointerRNA *ptr, char *propname, PointerRNA *activeptr, char *activepropname, int rows, int maxrows, int listtype)
{
	//Scene *scene= CTX_data_scene(C);
	PropertyRNA *prop= NULL, *activeprop;
	PropertyType type, activetype;
	StructRNA *ptype;
	uiLayout *box, *row, *col;
	uiBlock *block;
	uiBut *but;
	Panel *pa;
	char *name, str[32];
	int rnaicon=0, icon=0, i= 0, activei= 0, len= 0, items, found, min, max;

	/* validate arguments */
	block= uiLayoutGetBlock(layout);
	pa= block->panel;

	if(!pa) {
		printf("uiTemplateList: only works inside a panel.\n");
		return;
	}

	if(!activeptr->data)
		return;
	
	if(ptr->data) {
		prop= RNA_struct_find_property(ptr, propname);
		if(!prop) {
			printf("uiTemplateList: property not found: %s\n", propname);
			return;
		}
	}

	activeprop= RNA_struct_find_property(activeptr, activepropname);
	if(!activeprop) {
		printf("uiTemplateList: property not found: %s\n", activepropname);
		return;
	}

	if(prop) {
		type= RNA_property_type(prop);
		if(type != PROP_COLLECTION) {
			printf("uiTemplateList: expected collection property.\n");
			return;
		}
	}

	activetype= RNA_property_type(activeprop);
	if(activetype != PROP_INT) {
		printf("uiTemplateList: expected integer property.\n");
		return;
	}

	/* get icon */
	if(ptr->data && prop) {
		ptype= RNA_property_pointer_type(ptr, prop);
		rnaicon= RNA_struct_ui_icon(ptype);
	}

	/* get active data */
	activei= RNA_property_int_get(activeptr, activeprop);

	if(listtype == 'i') {
		box= uiLayoutListBox(layout, ptr, prop, activeptr, activeprop);
		col= uiLayoutColumn(box, 1);
		row= uiLayoutRow(col, 0);

		if(ptr->data && prop) {
			/* create list items */
			RNA_PROP_BEGIN(ptr, itemptr, prop) {
				/* create button */
				if(i == 9)
					row= uiLayoutRow(col, 0);

				icon= list_item_icon_get(C, &itemptr, rnaicon);
				but= uiDefIconButR(block, LISTROW, 0, icon, 0,0,UI_UNIT_X*10,UI_UNIT_Y, activeptr, activepropname, 0, 0, i, 0, 0, "");
				uiButSetFlag(but, UI_BUT_NO_TOOLTIP);
				

				i++;
			}
			RNA_PROP_END;
		}
	}
	else if(listtype == 'c') {
		/* compact layout */
		found= 0;

		row= uiLayoutRow(layout, 1);

		if(ptr->data && prop) {
			/* create list items */
			RNA_PROP_BEGIN(ptr, itemptr, prop) {
				found= (activei == i);

				if(found) {
					/* create button */
					name= RNA_struct_name_get_alloc(&itemptr, NULL, 0);
					icon= list_item_icon_get(C, &itemptr, rnaicon);
					uiItemL(row, (name)? name: "", icon);

					if(name)
						MEM_freeN(name);
				}

				i++;
			}
			RNA_PROP_END;
		}

		/* if not found, add in dummy button */
		if(i == 0)
			uiItemL(row, "", 0);

		/* next/prev button */
		sprintf(str, "%d :", i);
		but= uiDefIconTextButR(block, NUM, 0, 0, str, 0,0,UI_UNIT_X*5,UI_UNIT_Y, activeptr, activepropname, 0, 0, 0, 0, 0, "");
		if(i == 0)
			uiButSetFlag(but, UI_BUT_DISABLED);
	}
	else {
		/* default rows */
		if(rows == 0)
			rows= 5;
		if (maxrows == 0)
			maxrows = 5;
		if(pa->list_grip_size != 0)
			rows= pa->list_grip_size;

		/* layout */
		box= uiLayoutListBox(layout, ptr, prop, activeptr, activeprop);
		row= uiLayoutRow(box, 0);
		col = uiLayoutColumn(row, 1);

		/* init numbers */
		RNA_property_int_range(activeptr, activeprop, &min, &max);

		if(prop)
			len= RNA_property_collection_length(ptr, prop);
		items= CLAMPIS(len, rows, MAX2(rows, maxrows));

		/* if list length changes and active is out of view, scroll to it */
		if(pa->list_last_len != len)
			if((activei < pa->list_scroll || activei >= pa->list_scroll+items))
				pa->list_scroll= activei;

		pa->list_scroll= MIN2(pa->list_scroll, len-items);
		pa->list_scroll= MAX2(pa->list_scroll, 0);
		pa->list_size= items;
		pa->list_last_len= len;

		if(ptr->data && prop) {
			/* create list items */
			RNA_PROP_BEGIN(ptr, itemptr, prop) {
				if(i >= pa->list_scroll && i<pa->list_scroll+items)
					list_item_row(C, col, ptr, &itemptr, i, rnaicon, activeptr, activepropname);

				i++;
			}
			RNA_PROP_END;
		}

		/* add dummy buttons to fill space */
		while(i < pa->list_scroll+items) {
			if(i >= pa->list_scroll)
				uiItemL(col, "", 0);
			i++;
		}

		/* add scrollbar */
		if(len > items) {
			col= uiLayoutColumn(row, 0);
			uiDefButI(block, SCROLL, 0, "", 0,0,UI_UNIT_X*0.75,UI_UNIT_Y*items, &pa->list_scroll, 0, len-items, items, 0, "");
		}
	}
}

/************************* Operator Search Template **************************/

static void operator_call_cb(bContext *C, void *arg1, void *arg2)
{
	wmOperatorType *ot= arg2;
	
	if(ot)
		WM_operator_name_call(C, ot->idname, WM_OP_INVOKE_DEFAULT, NULL);
}

static void operator_search_cb(const bContext *C, void *arg, char *str, uiSearchItems *items)
{
	wmOperatorType *ot = WM_operatortype_first();
	
	for(; ot; ot= ot->next) {
		
		if(BLI_strcasestr(ot->name, str)) {
			if(WM_operator_poll((bContext*)C, ot)) {
				char name[256];
				int len= strlen(ot->name);
				
				/* display name for menu, can hold hotkey */
				BLI_strncpy(name, ot->name, 256);
				
				/* check for hotkey */
				if(len < 256-6) {
					if(WM_key_event_operator_string(C, ot->idname, WM_OP_EXEC_DEFAULT, NULL, &name[len+1], 256-len-1))
						name[len]= '|';
				}
				
				if(0==uiSearchItemAdd(items, name, ot, 0))
					break;
			}
		}
	}
}

void uiTemplateOperatorSearch(uiLayout *layout)
{
	uiBlock *block;
	uiBut *but;
	static char search[256]= "";
		
	block= uiLayoutGetBlock(layout);
	uiBlockSetCurLayout(block, layout);

	but= uiDefSearchBut(block, search, 0, ICON_VIEWZOOM, sizeof(search), 0, 0, UI_UNIT_X*6, UI_UNIT_Y, 0, 0, "");
	uiButSetSearchFunc(but, operator_search_cb, NULL, operator_call_cb, NULL);
}

/************************* Running Jobs Template **************************/

#define B_STOPRENDER	1
#define B_STOPCAST		2
#define B_STOPANIM		3

static void do_running_jobs(bContext *C, void *arg, int event)
{
	switch(event) {
		case B_STOPRENDER:
			G.afbreek= 1;
			break;
		case B_STOPCAST:
			WM_jobs_stop(CTX_wm_manager(C), CTX_wm_screen(C));
			break;
		case B_STOPANIM:
			WM_operator_name_call(C, "SCREEN_OT_animation_play", WM_OP_INVOKE_SCREEN, NULL);
			break;
	}
}

void uiTemplateRunningJobs(uiLayout *layout, bContext *C)
{
	bScreen *screen= CTX_wm_screen(C);
	Scene *scene= CTX_data_scene(C);
	wmWindowManager *wm= CTX_wm_manager(C);
	uiBlock *block;

	block= uiLayoutGetBlock(layout);
	uiBlockSetCurLayout(block, layout);

	uiBlockSetHandleFunc(block, do_running_jobs, NULL);

	if(WM_jobs_test(wm, scene))
		uiDefIconTextBut(block, BUT, B_STOPRENDER, ICON_CANCEL, "Render", 0,0,75,UI_UNIT_Y, NULL, 0.0f, 0.0f, 0, 0, "Stop rendering");
	if(WM_jobs_test(wm, screen))
		uiDefIconTextBut(block, BUT, B_STOPCAST, ICON_CANCEL, "Capture", 0,0,85,UI_UNIT_Y, NULL, 0.0f, 0.0f, 0, 0, "Stop screencast");
	if(screen->animtimer)
		uiDefIconTextBut(block, BUT, B_STOPANIM, ICON_CANCEL, "Anim Player", 0,0,100,UI_UNIT_Y, NULL, 0.0f, 0.0f, 0, 0, "Stop animation playback");
}


