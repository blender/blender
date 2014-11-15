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

/** \file blender/editors/interface/interface_templates.c
 *  \ingroup edinterface
 */


#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_dynamicpaint_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_brush_types.h"
#include "DNA_texture_types.h"

#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_ghash.h"
#include "BLI_rect.h"
#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_fnmatch.h"

#include "BLF_api.h"
#include "BLF_translation.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_idcode.h"
#include "BKE_library.h"
#include "BKE_linestyle.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_packedFile.h"
#include "BKE_particle.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_sca.h"
#include "BKE_screen.h"
#include "BKE_texture.h"

#include "ED_screen.h"
#include "ED_object.h"
#include "ED_render.h"
#include "ED_util.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "interface_intern.h"

void UI_template_fix_linking(void)
{
}

/********************** Header Template *************************/

void uiTemplateHeader(uiLayout *layout, bContext *C)
{
	uiBlock *block;

	block = uiLayoutAbsoluteBlock(layout);
	ED_area_header_switchbutton(C, block, 0);
}

/********************** Search Callbacks *************************/

typedef struct TemplateID {
	PointerRNA ptr;
	PropertyRNA *prop;

	ListBase *idlb;
	int prv_rows, prv_cols;
	bool preview;
} TemplateID;

/* Search browse menu, assign  */
static void id_search_call_cb(bContext *C, void *arg_template, void *item)
{
	TemplateID *template = (TemplateID *)arg_template;

	/* ID */
	if (item) {
		PointerRNA idptr;

		RNA_id_pointer_create(item, &idptr);
		RNA_property_pointer_set(&template->ptr, template->prop, idptr);
		RNA_property_update(C, &template->ptr, template->prop);
	}
}

/* ID Search browse menu, do the search */
static void id_search_cb(const bContext *C, void *arg_template, const char *str, uiSearchItems *items)
{
	TemplateID *template = (TemplateID *)arg_template;
	ListBase *lb = template->idlb;
	ID *id, *id_from = template->ptr.id.data;
	int iconid;
	int flag = RNA_property_flag(template->prop);

	/* ID listbase */
	for (id = lb->first; id; id = id->next) {
		if (!((flag & PROP_ID_SELF_CHECK) && id == id_from)) {

			/* use filter */
			if (RNA_property_type(template->prop) == PROP_POINTER) {
				PointerRNA ptr;
				RNA_id_pointer_create(id, &ptr);
				if (RNA_property_pointer_poll(&template->ptr, template->prop, &ptr) == 0)
					continue;
			}

			/* hide dot-datablocks, but only if filter does not force it visible */
			if (U.uiflag & USER_HIDE_DOT)
				if ((id->name[2] == '.') && (str[0] != '.'))
					continue;

			if (*str == '\0' || BLI_strcasestr(id->name + 2, str)) {
				/* +1 is needed because name_uiprefix_id used 3 letter prefix
				 * followed by ID_NAME-2 characters from id->name
				 */
				char name_ui[MAX_ID_NAME + 1];
				name_uiprefix_id(name_ui, id);

				iconid = ui_id_icon_get((bContext *)C, id, template->preview);

				if (false == UI_search_item_add(items, name_ui, id, iconid))
					break;
			}
		}
	}
}

/* ID Search browse menu, open */
static uiBlock *id_search_menu(bContext *C, ARegion *ar, void *arg_litem)
{
	static char search[256];
	static TemplateID template;
	PointerRNA idptr;
	wmWindow *win = CTX_wm_window(C);
	uiBlock *block;
	uiBut *but;
	
	/* clear initial search string, then all items show */
	search[0] = 0;
	/* arg_litem is malloced, can be freed by parent button */
	template = *((TemplateID *)arg_litem);
	
	/* get active id for showing first item */
	idptr = RNA_property_pointer_get(&template.ptr, template.prop);

	block = UI_block_begin(C, ar, "_popup", UI_EMBOSS);
	UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_SEARCH_MENU);
	
	/* preview thumbnails */
	if (template.prv_rows > 0 && template.prv_cols > 0) {
		int w = 4 * U.widget_unit * template.prv_cols;
		int h = 4 * U.widget_unit * template.prv_rows + U.widget_unit;
		
		/* fake button, it holds space for search items */
		uiDefBut(block, UI_BTYPE_LABEL, 0, "", 10, 15, w, h, NULL, 0, 0, 0, 0, NULL);
		
		but = uiDefSearchBut(block, search, 0, ICON_VIEWZOOM, sizeof(search), 10, 0, w, UI_UNIT_Y,
		                     template.prv_rows, template.prv_cols, "");
		UI_but_func_search_set(but, id_search_cb, &template, id_search_call_cb, idptr.data);
	}
	/* list view */
	else {
		const int searchbox_width  = UI_searchbox_size_x();
		const int searchbox_height = UI_searchbox_size_y();
		
		/* fake button, it holds space for search items */
		uiDefBut(block, UI_BTYPE_LABEL, 0, "", 10, 15, searchbox_width, searchbox_height, NULL, 0, 0, 0, 0, NULL);
		but = uiDefSearchBut(block, search, 0, ICON_VIEWZOOM, sizeof(search), 10, 0, searchbox_width, UI_UNIT_Y - 1, 0, 0, "");
		UI_but_func_search_set(but, id_search_cb, &template, id_search_call_cb, idptr.data);
	}
		
	
	UI_block_bounds_set_normal(block, 0.3f * U.widget_unit);
	UI_block_direction_set(block, UI_DIR_DOWN);
	
	/* give search-field focus */
	UI_but_focus_on_enter_event(win, but);
	/* this type of search menu requires undo */
	but->flag |= UI_BUT_UNDO;
	
	return block;
}

/************************ ID Template ***************************/
/* This is for browsing and editing the ID-blocks used */

/* for new/open operators */
void UI_context_active_but_prop_get_templateID(bContext *C, PointerRNA *ptr, PropertyRNA **prop)
{
	TemplateID *template;
	ARegion *ar = CTX_wm_region(C);
	uiBlock *block;
	uiBut *but;

	memset(ptr, 0, sizeof(*ptr));
	*prop = NULL;

	if (!ar)
		return;

	for (block = ar->uiblocks.first; block; block = block->next) {
		for (but = block->buttons.first; but; but = but->next) {
			/* find the button before the active one */
			if ((but->flag & (UI_BUT_LAST_ACTIVE | UI_ACTIVE))) {
				if (but->func_argN) {
					template = but->func_argN;
					*ptr = template->ptr;
					*prop = template->prop;
					return;
				}
			}
		}
	}
}


static void template_id_cb(bContext *C, void *arg_litem, void *arg_event)
{
	TemplateID *template = (TemplateID *)arg_litem;
	PointerRNA idptr = RNA_property_pointer_get(&template->ptr, template->prop);
	ID *id = idptr.data;
	int event = GET_INT_FROM_POINTER(arg_event);
	
	switch (event) {
		case UI_ID_BROWSE:
		case UI_ID_PIN:
			RNA_warning("warning, id event %d shouldnt come here", event);
			break;
		case UI_ID_OPEN:
		case UI_ID_ADD_NEW:
			/* these call UI_context_active_but_prop_get_templateID */
			break;
		case UI_ID_DELETE:
			memset(&idptr, 0, sizeof(idptr));
			RNA_property_pointer_set(&template->ptr, template->prop, idptr);
			RNA_property_update(C, &template->ptr, template->prop);

			if (id && CTX_wm_window(C)->eventstate->shift) /* useful hidden functionality, */
				id->us = 0;

			break;
		case UI_ID_FAKE_USER:
			if (id) {
				if (id->flag & LIB_FAKEUSER) id_us_plus(id);
				else id_us_min(id);
			}
			else {
				return;
			}
			break;
		case UI_ID_LOCAL:
			if (id) {
				if (id_make_local(id, false)) {
					/* reassign to get get proper updates/notifiers */
					idptr = RNA_property_pointer_get(&template->ptr, template->prop);
					RNA_property_pointer_set(&template->ptr, template->prop, idptr);
					RNA_property_update(C, &template->ptr, template->prop);
				}
			}
			break;
		case UI_ID_ALONE:
			if (id) {
				const bool do_scene_obj = (GS(id->name) == ID_OB) &&
				                          (template->ptr.type == &RNA_SceneObjects);

				/* make copy */
				if (do_scene_obj) {
					Main *bmain = CTX_data_main(C);
					Scene *scene = CTX_data_scene(C);
					ED_object_single_user(bmain, scene, (struct Object *)id);
					WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
					DAG_relations_tag_update(bmain);
				}
				else {
					if (id) {
						id_single_user(C, id, &template->ptr, template->prop);
					}
				}
			}
			break;
#if 0
		case UI_ID_AUTO_NAME:
			break;
#endif
	}
}

static const char *template_id_browse_tip(StructRNA *type)
{
	if (type) {
		switch (RNA_type_to_ID_code(type)) {
			case ID_SCE: return N_("Browse Scene to be linked");
			case ID_OB:  return N_("Browse Object to be linked");
			case ID_ME:  return N_("Browse Mesh Data to be linked");
			case ID_CU:  return N_("Browse Curve Data to be linked");
			case ID_MB:  return N_("Browse Metaball Data to be linked");
			case ID_MA:  return N_("Browse Material to be linked");
			case ID_TE:  return N_("Browse Texture to be linked");
			case ID_IM:  return N_("Browse Image to be linked");
			case ID_LS:  return N_("Browse Line Style Data to be linked");
			case ID_LT:  return N_("Browse Lattice Data to be linked");
			case ID_LA:  return N_("Browse Lamp Data to be linked");
			case ID_CA:  return N_("Browse Camera Data to be linked");
			case ID_WO:  return N_("Browse World Settings to be linked");
			case ID_SCR: return N_("Choose Screen layout");
			case ID_TXT: return N_("Browse Text to be linked");
			case ID_SPK: return N_("Browse Speaker Data to be linked");
			case ID_SO:  return N_("Browse Sound to be linked");
			case ID_AR:  return N_("Browse Armature data to be linked");
			case ID_AC:  return N_("Browse Action to be linked");
			case ID_NT:  return N_("Browse Node Tree to be linked");
			case ID_BR:  return N_("Browse Brush to be linked");
			case ID_PA:  return N_("Browse Particle Settings to be linked");
			case ID_GD:  return N_("Browse Grease Pencil Data to be linked");
			case ID_PAL: return N_("Browse Palette Data to be linked");
			case ID_PC:  return N_("Browse Paint Curve Data to be linked");
		}
	}
	return N_("Browse ID data to be linked");
}

/* Return a type-based i18n context, needed e.g. by "New" button.
 * In most languages, this adjective takes different form based on gender of type name...
 */
#ifdef WITH_INTERNATIONAL
static const char *template_id_context(StructRNA *type)
{
	if (type) {
		switch (RNA_type_to_ID_code(type)) {
			case ID_SCE: return BLF_I18NCONTEXT_ID_SCENE;
			case ID_OB:  return BLF_I18NCONTEXT_ID_OBJECT;
			case ID_ME:  return BLF_I18NCONTEXT_ID_MESH;
			case ID_CU:  return BLF_I18NCONTEXT_ID_CURVE;
			case ID_MB:  return BLF_I18NCONTEXT_ID_METABALL;
			case ID_MA:  return BLF_I18NCONTEXT_ID_MATERIAL;
			case ID_TE:  return BLF_I18NCONTEXT_ID_TEXTURE;
			case ID_IM:  return BLF_I18NCONTEXT_ID_IMAGE;
			case ID_LS:  return BLF_I18NCONTEXT_ID_FREESTYLELINESTYLE;
			case ID_LT:  return BLF_I18NCONTEXT_ID_LATTICE;
			case ID_LA:  return BLF_I18NCONTEXT_ID_LAMP;
			case ID_CA:  return BLF_I18NCONTEXT_ID_CAMERA;
			case ID_WO:  return BLF_I18NCONTEXT_ID_WORLD;
			case ID_SCR: return BLF_I18NCONTEXT_ID_SCREEN;
			case ID_TXT: return BLF_I18NCONTEXT_ID_TEXT;
			case ID_SPK: return BLF_I18NCONTEXT_ID_SPEAKER;
			case ID_SO:  return BLF_I18NCONTEXT_ID_SOUND;
			case ID_AR:  return BLF_I18NCONTEXT_ID_ARMATURE;
			case ID_AC:  return BLF_I18NCONTEXT_ID_ACTION;
			case ID_NT:  return BLF_I18NCONTEXT_ID_NODETREE;
			case ID_BR:  return BLF_I18NCONTEXT_ID_BRUSH;
			case ID_PA:  return BLF_I18NCONTEXT_ID_PARTICLESETTINGS;
			case ID_GD:  return BLF_I18NCONTEXT_ID_GPENCIL;
		}
	}
	return BLF_I18NCONTEXT_DEFAULT;
}
#endif

static void template_ID(bContext *C, uiLayout *layout, TemplateID *template, StructRNA *type, short idcode, int flag,
                        const char *newop, const char *openop, const char *unlinkop)
{
	uiBut *but;
	uiBlock *block;
	PointerRNA idptr;
	// ListBase *lb; // UNUSED
	ID *id, *idfrom;
	const bool editable = RNA_property_editable(&template->ptr, template->prop);

	idptr = RNA_property_pointer_get(&template->ptr, template->prop);
	id = idptr.data;
	idfrom = template->ptr.id.data;
	// lb = template->idlb;

	block = uiLayoutGetBlock(layout);
	UI_block_align_begin(block);

	if (idptr.type)
		type = idptr.type;

	if (flag & UI_ID_PREVIEWS) {
		template->preview = true;

		but = uiDefBlockButN(block, id_search_menu, MEM_dupallocN(template), "", 0, 0, UI_UNIT_X * 6, UI_UNIT_Y * 6,
		                     TIP_(template_id_browse_tip(type)));
		but->icon = id ? ui_id_icon_get(C, id, true) : RNA_struct_ui_icon(type);
		UI_but_flag_enable(but, UI_HAS_ICON | UI_BUT_ICON_PREVIEW);

		if ((idfrom && idfrom->lib) || !editable)
			UI_but_flag_enable(but, UI_BUT_DISABLED);
		
		uiLayoutRow(layout, true);
	}
	else if (flag & UI_ID_BROWSE) {
		but = uiDefBlockButN(block, id_search_menu, MEM_dupallocN(template), "", 0, 0, UI_UNIT_X * 1.6, UI_UNIT_Y,
		                     TIP_(template_id_browse_tip(type)));
		but->icon = RNA_struct_ui_icon(type);
		/* default dragging of icon for id browse buttons */
		UI_but_drag_set_id(but, id);
		UI_but_flag_enable(but, UI_HAS_ICON);
		UI_but_drawflag_enable(but, UI_BUT_ICON_LEFT);

		if ((idfrom && idfrom->lib) || !editable)
			UI_but_flag_enable(but, UI_BUT_DISABLED);
	}

	/* text button with name */
	if (id) {
		char name[UI_MAX_NAME_STR];
		const bool user_alert = (id->us <= 0);

		//text_idbutton(id, name);
		name[0] = '\0';
		but = uiDefButR(block, UI_BTYPE_TEXT, 0, name, 0, 0, UI_UNIT_X * 6, UI_UNIT_Y,
		                &idptr, "name", -1, 0, 0, -1, -1, RNA_struct_ui_description(type));
		UI_but_funcN_set(but, template_id_cb, MEM_dupallocN(template), SET_INT_IN_POINTER(UI_ID_RENAME));
		if (user_alert) UI_but_flag_enable(but, UI_BUT_REDALERT);

		if (id->lib) {
			if (id->flag & LIB_INDIRECT) {
				but = uiDefIconBut(block, UI_BTYPE_BUT, 0, ICON_LIBRARY_DATA_INDIRECT, 0, 0, UI_UNIT_X, UI_UNIT_Y,
				                   NULL, 0, 0, 0, 0, TIP_("Indirect library datablock, cannot change"));
				UI_but_flag_enable(but, UI_BUT_DISABLED);
			}
			else {
				but = uiDefIconBut(block, UI_BTYPE_BUT, 0, ICON_LIBRARY_DATA_DIRECT, 0, 0, UI_UNIT_X, UI_UNIT_Y,
				                   NULL, 0, 0, 0, 0, TIP_("Direct linked library datablock, click to make local"));
				if (!id_make_local(id, true /* test */) || (idfrom && idfrom->lib))
					UI_but_flag_enable(but, UI_BUT_DISABLED);
			}

			UI_but_funcN_set(but, template_id_cb, MEM_dupallocN(template), SET_INT_IN_POINTER(UI_ID_LOCAL));
		}

		if (id->us > 1) {
			char numstr[32];

			BLI_snprintf(numstr, sizeof(numstr), "%d", id->us);

			but = uiDefBut(block, UI_BTYPE_BUT, 0, numstr, 0, 0, UI_UNIT_X + ((id->us < 10) ? 0 : 10), UI_UNIT_Y,
			               NULL, 0, 0, 0, 0,
			               TIP_("Display number of users of this data (click to make a single-user copy)"));
			but->flag |= UI_BUT_UNDO;

			UI_but_funcN_set(but, template_id_cb, MEM_dupallocN(template), SET_INT_IN_POINTER(UI_ID_ALONE));
			if (/* test only */
			    (id_copy(id, NULL, true) == false) ||
			    (idfrom && idfrom->lib) ||
			    (!editable) ||
			    /* object in editmode - don't change data */
			    (idfrom && GS(idfrom->name) == ID_OB && (((Object *)idfrom)->mode & OB_MODE_EDIT)))
			{
				UI_but_flag_enable(but, UI_BUT_DISABLED);
			}
		}
	
		if (user_alert) UI_but_flag_enable(but, UI_BUT_REDALERT);
		
		if (id->lib == NULL && !(ELEM(GS(id->name), ID_GR, ID_SCE, ID_SCR, ID_TXT, ID_OB))) {
			uiDefButR(block, UI_BTYPE_TOGGLE, 0, "F", 0, 0, UI_UNIT_X, UI_UNIT_Y, &idptr, "use_fake_user", -1, 0, 0, -1, -1, NULL);
		}
	}
	
	if (flag & UI_ID_ADD_NEW) {
		int w = id ? UI_UNIT_X : (flag & UI_ID_OPEN) ? UI_UNIT_X * 3 : UI_UNIT_X * 6;
		
		/* i18n markup, does nothing! */
		BLF_I18N_MSGID_MULTI_CTXT("New", BLF_I18NCONTEXT_DEFAULT,
		                                 BLF_I18NCONTEXT_ID_SCENE,
		                                 BLF_I18NCONTEXT_ID_OBJECT,
		                                 BLF_I18NCONTEXT_ID_MESH,
		                                 BLF_I18NCONTEXT_ID_CURVE,
		                                 BLF_I18NCONTEXT_ID_METABALL,
		                                 BLF_I18NCONTEXT_ID_MATERIAL,
		                                 BLF_I18NCONTEXT_ID_TEXTURE,
		                                 BLF_I18NCONTEXT_ID_IMAGE,
		                                 BLF_I18NCONTEXT_ID_LATTICE,
		                                 BLF_I18NCONTEXT_ID_LAMP,
		                                 BLF_I18NCONTEXT_ID_CAMERA,
		                                 BLF_I18NCONTEXT_ID_WORLD,
		                                 BLF_I18NCONTEXT_ID_SCREEN,
		                                 BLF_I18NCONTEXT_ID_TEXT,
		);
		BLF_I18N_MSGID_MULTI_CTXT("New", BLF_I18NCONTEXT_ID_SPEAKER,
		                                 BLF_I18NCONTEXT_ID_SOUND,
		                                 BLF_I18NCONTEXT_ID_ARMATURE,
		                                 BLF_I18NCONTEXT_ID_ACTION,
		                                 BLF_I18NCONTEXT_ID_NODETREE,
		                                 BLF_I18NCONTEXT_ID_BRUSH,
		                                 BLF_I18NCONTEXT_ID_PARTICLESETTINGS,
		                                 BLF_I18NCONTEXT_ID_GPENCIL,
		                                 BLF_I18NCONTEXT_ID_FREESTYLELINESTYLE,
		);
		
		if (newop) {
			but = uiDefIconTextButO(block, UI_BTYPE_BUT, newop, WM_OP_INVOKE_DEFAULT, ICON_ZOOMIN,
			                        (id) ? "" : CTX_IFACE_(template_id_context(type), "New"), 0, 0, w, UI_UNIT_Y, NULL);
			UI_but_funcN_set(but, template_id_cb, MEM_dupallocN(template), SET_INT_IN_POINTER(UI_ID_ADD_NEW));
		}
		else {
			but = uiDefIconTextBut(block, UI_BTYPE_BUT, 0, ICON_ZOOMIN, (id) ? "" : CTX_IFACE_(template_id_context(type), "New"),
			                       0, 0, w, UI_UNIT_Y, NULL, 0, 0, 0, 0, NULL);
			UI_but_funcN_set(but, template_id_cb, MEM_dupallocN(template), SET_INT_IN_POINTER(UI_ID_ADD_NEW));
		}

		if ((idfrom && idfrom->lib) || !editable)
			UI_but_flag_enable(but, UI_BUT_DISABLED);
	}

	/* Due to space limit in UI - skip the "open" icon for packed data, and allow to unpack.
	 * Only for images, sound and fonts */
	if (id && BKE_pack_check(id)) {
		but = uiDefIconButO(block, UI_BTYPE_BUT, "FILE_OT_unpack_item", WM_OP_INVOKE_REGION_WIN, ICON_PACKAGE, 0, 0,
		                    UI_UNIT_X, UI_UNIT_Y, TIP_("Packed File, click to unpack"));
		UI_but_operator_ptr_get(but);
		
		RNA_string_set(but->opptr, "id_name", id->name + 2);
		RNA_int_set(but->opptr, "id_type", GS(id->name));
		
	}
	else if (flag & UI_ID_OPEN) {
		int w = id ? UI_UNIT_X : (flag & UI_ID_ADD_NEW) ? UI_UNIT_X * 3 : UI_UNIT_X * 6;
		
		if (openop) {
			but = uiDefIconTextButO(block, UI_BTYPE_BUT, openop, WM_OP_INVOKE_DEFAULT, ICON_FILESEL, (id) ? "" : IFACE_("Open"),
			                        0, 0, w, UI_UNIT_Y, NULL);
			UI_but_funcN_set(but, template_id_cb, MEM_dupallocN(template), SET_INT_IN_POINTER(UI_ID_OPEN));
		}
		else {
			but = uiDefIconTextBut(block, UI_BTYPE_BUT, 0, ICON_FILESEL, (id) ? "" : IFACE_("Open"), 0, 0, w, UI_UNIT_Y,
			                       NULL, 0, 0, 0, 0, NULL);
			UI_but_funcN_set(but, template_id_cb, MEM_dupallocN(template), SET_INT_IN_POINTER(UI_ID_OPEN));
		}

		if ((idfrom && idfrom->lib) || !editable)
			UI_but_flag_enable(but, UI_BUT_DISABLED);
	}
	
	/* delete button */
	/* don't use RNA_property_is_unlink here */
	if (id && (flag & UI_ID_DELETE)) {
		/* allow unlink if 'unlinkop' is passed, even when 'PROP_NEVER_UNLINK' is set */
		but = NULL;

		if (unlinkop) {
			but = uiDefIconButO(block, UI_BTYPE_BUT, unlinkop, WM_OP_INVOKE_REGION_WIN, ICON_X, 0, 0, UI_UNIT_X, UI_UNIT_Y, NULL);
			/* so we can access the template from operators, font unlinking needs this */
			UI_but_funcN_set(but, NULL, MEM_dupallocN(template), NULL);
		}
		else {
			if ((RNA_property_flag(template->prop) & PROP_NEVER_UNLINK) == 0) {
				but = uiDefIconBut(block, UI_BTYPE_BUT, 0, ICON_X, 0, 0, UI_UNIT_X, UI_UNIT_Y, NULL, 0, 0, 0, 0,
				                   TIP_("Unlink datablock "
				                        "(Shift + Click to set users to zero, data will then not be saved)"));
				UI_but_funcN_set(but, template_id_cb, MEM_dupallocN(template), SET_INT_IN_POINTER(UI_ID_DELETE));

				if (RNA_property_flag(template->prop) & PROP_NEVER_NULL) {
					UI_but_flag_enable(but, UI_BUT_DISABLED);
				}
			}
		}

		if (but) {
			if ((idfrom && idfrom->lib) || !editable) {
				UI_but_flag_enable(but, UI_BUT_DISABLED);
			}
		}
	}

	if (idcode == ID_TE)
		uiTemplateTextureShow(layout, C, &template->ptr, template->prop);
	
	UI_block_align_end(block);
}

static void ui_template_id(uiLayout *layout, bContext *C, PointerRNA *ptr, const char *propname, const char *newop,
                           const char *openop, const char *unlinkop, int flag, int prv_rows, int prv_cols)
{
	TemplateID *template;
	PropertyRNA *prop;
	StructRNA *type;
	short idcode;

	prop = RNA_struct_find_property(ptr, propname);

	if (!prop || RNA_property_type(prop) != PROP_POINTER) {
		RNA_warning("pointer property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}

	template = MEM_callocN(sizeof(TemplateID), "TemplateID");
	template->ptr = *ptr;
	template->prop = prop;
	template->prv_rows = prv_rows;
	template->prv_cols = prv_cols;

	if (newop)
		flag |= UI_ID_ADD_NEW;
	if (openop)
		flag |= UI_ID_OPEN;

	type = RNA_property_pointer_type(ptr, prop);
	idcode = RNA_type_to_ID_code(type);
	template->idlb = which_libbase(CTX_data_main(C), idcode);
	
	/* create UI elements for this template
	 *	- template_ID makes a copy of the template data and assigns it to the relevant buttons
	 */
	if (template->idlb) {
		uiLayoutRow(layout, true);
		template_ID(C, layout, template, type, idcode, flag, newop, openop, unlinkop);
	}

	MEM_freeN(template);
}

void uiTemplateID(uiLayout *layout, bContext *C, PointerRNA *ptr, const char *propname, const char *newop,
                  const char *openop, const char *unlinkop)
{
	ui_template_id(layout, C, ptr, propname, newop, openop, unlinkop,
	               UI_ID_BROWSE | UI_ID_RENAME | UI_ID_DELETE, 0, 0);
}

void uiTemplateIDBrowse(uiLayout *layout, bContext *C, PointerRNA *ptr, const char *propname, const char *newop,
                        const char *openop, const char *unlinkop)
{
	ui_template_id(layout, C, ptr, propname, newop, openop, unlinkop, UI_ID_BROWSE | UI_ID_RENAME, 0, 0);
}

void uiTemplateIDPreview(uiLayout *layout, bContext *C, PointerRNA *ptr, const char *propname, const char *newop,
                         const char *openop, const char *unlinkop, int rows, int cols)
{
	ui_template_id(layout, C, ptr, propname, newop, openop, unlinkop,
	               UI_ID_BROWSE | UI_ID_RENAME | UI_ID_DELETE | UI_ID_PREVIEWS, rows, cols);
}

/************************ ID Chooser Template ***************************/

/* This is for selecting the type of ID-block to use, and then from the relevant type choosing the block to use 
 *
 * - propname: property identifier for property that ID-pointer gets stored to
 * - proptypename: property identifier for property used to determine the type of ID-pointer that can be used
 */
void uiTemplateAnyID(uiLayout *layout, PointerRNA *ptr, const char *propname, const char *proptypename,
                     const char *text)
{
	PropertyRNA *propID, *propType;
	uiLayout *split, *row, *sub;
	
	/* get properties... */
	propID = RNA_struct_find_property(ptr, propname);
	propType = RNA_struct_find_property(ptr, proptypename);

	if (!propID || RNA_property_type(propID) != PROP_POINTER) {
		RNA_warning("pointer property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}
	if (!propType || RNA_property_type(propType) != PROP_ENUM) {
		RNA_warning("pointer-type property not found: %s.%s", RNA_struct_identifier(ptr->type), proptypename);
		return;
	}
	
	/* Start drawing UI Elements using standard defines */
	split = uiLayoutSplit(layout, 0.33f, false);  /* NOTE: split amount here needs to be synced with normal labels */
	
	/* FIRST PART ................................................ */
	row = uiLayoutRow(split, false);
	
	/* Label - either use the provided text, or will become "ID-Block:" */
	if (text) {
		if (text[0])
			uiItemL(row, text, ICON_NONE);
	}
	else {
		uiItemL(row, IFACE_("ID-Block:"), ICON_NONE);
	}
	
	/* SECOND PART ................................................ */
	row = uiLayoutRow(split, true);
	
	/* ID-Type Selector - just have a menu of icons */
	sub = uiLayoutRow(row, true);                     /* HACK: special group just for the enum, otherwise we */
	uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_LEFT);  /*       we get ugly layout with text included too...  */
	
	uiItemFullR(sub, ptr, propType, 0, 0, UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
	
	/* ID-Block Selector - just use pointer widget... */
	sub = uiLayoutRow(row, true);                       /* HACK: special group to counteract the effects of the previous */
	uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_EXPAND);  /*       enum, which now pushes everything too far right         */
	
	uiItemFullR(sub, ptr, propID, 0, 0, 0, "", ICON_NONE);
}

/********************* RNA Path Builder Template ********************/

/* ---------- */

/* This is creating/editing RNA-Paths 
 *
 * - ptr: struct which holds the path property
 * - propname: property identifier for property that path gets stored to
 * - root_ptr: struct that path gets built from
 */
void uiTemplatePathBuilder(uiLayout *layout, PointerRNA *ptr, const char *propname, PointerRNA *UNUSED(root_ptr),
                           const char *text)
{
	PropertyRNA *propPath;
	uiLayout *row;
	
	/* check that properties are valid */
	propPath = RNA_struct_find_property(ptr, propname);
	if (!propPath || RNA_property_type(propPath) != PROP_STRING) {
		RNA_warning("path property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}
	
	/* Start drawing UI Elements using standard defines */
	row = uiLayoutRow(layout, true);
	
	/* Path (existing string) Widget */
	uiItemR(row, ptr, propname, 0, text, ICON_RNA);
	
	/* TODO: attach something to this to make allow searching of nested properties to 'build' the path */
}

/************************ Modifier Template *************************/

#define ERROR_LIBDATA_MESSAGE IFACE_("Can't edit external libdata")

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

	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

	ED_undo_push(C, "Modifier convert to real");
}

static int modifier_can_delete(ModifierData *md)
{
	/* fluid particle modifier can't be deleted here */
	if (md->type == eModifierType_ParticleSystem)
		if (((ParticleSystemModifierData *)md)->psys->part->type == PART_FLUID)
			return 0;

	return 1;
}

/* Check whether Modifier is a simulation or not, this is used for switching to the physics/particles context tab */
static int modifier_is_simulation(ModifierData *md)
{
	/* Physic Tab */
	if (ELEM(md->type, eModifierType_Cloth, eModifierType_Collision, eModifierType_Fluidsim, eModifierType_Smoke,
	          eModifierType_Softbody, eModifierType_Surface, eModifierType_DynamicPaint))
	{
		return 1;
	}
	/* Particle Tab */
	else if (md->type == eModifierType_ParticleSystem) {
		return 2;
	}
	else {
		return 0;
	}
}

static uiLayout *draw_modifier(uiLayout *layout, Scene *scene, Object *ob,
                               ModifierData *md, int index, int cageIndex, int lastCageIndex)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	PointerRNA ptr;
	uiBut *but;
	uiBlock *block;
	uiLayout *box, *column, *row, *sub;
	uiLayout *result = NULL;
	int isVirtual = (md->mode & eModifierMode_Virtual);
	char str[128];

	/* create RNA pointer */
	RNA_pointer_create(&ob->id, &RNA_Modifier, md, &ptr);

	column = uiLayoutColumn(layout, true);
	uiLayoutSetContextPointer(column, "modifier", &ptr);

	/* rounded header ------------------------------------------------------------------- */
	box = uiLayoutBox(column);
	
	if (isVirtual) {
		row = uiLayoutRow(box, false);
		uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_EXPAND);
		block = uiLayoutGetBlock(row);
		/* VIRTUAL MODIFIER */
		/* XXX this is not used now, since these cannot be accessed via RNA */
		BLI_snprintf(str, sizeof(str), IFACE_("%s parent deform"), md->name);
		uiDefBut(block, UI_BTYPE_LABEL, 0, str, 0, 0, 185, UI_UNIT_Y, NULL, 0.0, 0.0, 0.0, 0.0, TIP_("Modifier name"));
		
		but = uiDefBut(block, UI_BTYPE_BUT, 0, IFACE_("Make Real"), 0, 0, 80, 16, NULL, 0.0, 0.0, 0.0, 0.0,
		               TIP_("Convert virtual modifier to a real modifier"));
		UI_but_func_set(but, modifiers_convertToReal, ob, md);
	}
	else {
		/* REAL MODIFIER */
		row = uiLayoutRow(box, false);
		block = uiLayoutGetBlock(row);
		
		UI_block_emboss_set(block, UI_EMBOSS_NONE);
		/* Open/Close .................................  */
		uiItemR(row, &ptr, "show_expanded", 0, "", ICON_NONE);
		
		/* modifier-type icon */
		uiItemL(row, "", RNA_struct_ui_icon(ptr.type));
		UI_block_emboss_set(block, UI_EMBOSS);
		
		/* modifier name */
		md->scene = scene;
		if (mti->isDisabled && mti->isDisabled(md, 0)) {
			uiLayoutSetRedAlert(row, true);
		}
		uiItemR(row, &ptr, "name", 0, "", ICON_NONE);
		uiLayoutSetRedAlert(row, false);
		
		/* mode enabling buttons */
		UI_block_align_begin(block);
		/* Softbody not allowed in this situation, enforce! */
		if (((md->type != eModifierType_Softbody && md->type != eModifierType_Collision) || !(ob->pd && ob->pd->deflect)) &&
		    (md->type != eModifierType_Surface) )
		{
			uiItemR(row, &ptr, "show_render", 0, "", ICON_NONE);
			uiItemR(row, &ptr, "show_viewport", 0, "", ICON_NONE);
			
			if (mti->flags & eModifierTypeFlag_SupportsEditmode) {
				sub = uiLayoutRow(row, true);
				if (!(md->mode & eModifierMode_Realtime)) {
					uiLayoutSetActive(sub, false);
				}
				uiItemR(sub, &ptr, "show_in_editmode", 0, "", ICON_NONE);
			}
		}

		if (ob->type == OB_MESH) {
			if (modifier_supportsCage(scene, md) && (index <= lastCageIndex)) {
				sub = uiLayoutRow(row, true);
				if (index < cageIndex || !modifier_couldBeCage(scene, md)) {
					uiLayoutSetActive(sub, false);
				}
				uiItemR(sub, &ptr, "show_on_cage", 0, "", ICON_NONE);
			}
		} /* tessellation point for curve-typed objects */
		else if (ELEM(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
			/* some modifiers could work with pre-tessellated curves only */
			if (ELEM(md->type, eModifierType_Hook, eModifierType_Softbody, eModifierType_MeshDeform)) {
				/* add disabled pre-tessellated button, so users could have
				 * message for this modifiers */
				but = uiDefIconButBitI(block, UI_BTYPE_TOGGLE, eModifierMode_ApplyOnSpline, 0, ICON_SURFACE_DATA, 0, 0,
				                       UI_UNIT_X - 2, UI_UNIT_Y, &md->mode, 0.0, 0.0, 0.0, 0.0,
				                       TIP_("This modifier can only be applied on splines' points"));
				UI_but_flag_enable(but, UI_BUT_DISABLED);
			}
			else if (mti->type != eModifierTypeType_Constructive) {
				/* constructive modifiers tessellates curve before applying */
				uiItemR(row, &ptr, "use_apply_on_spline", 0, "", ICON_NONE);
			}
		}

		UI_block_align_end(block);
		
		/* Up/Down + Delete ........................... */
		UI_block_align_begin(block);
		uiItemO(row, "", ICON_TRIA_UP, "OBJECT_OT_modifier_move_up");
		uiItemO(row, "", ICON_TRIA_DOWN, "OBJECT_OT_modifier_move_down");
		UI_block_align_end(block);
		
		UI_block_emboss_set(block, UI_EMBOSS_NONE);
		/* When Modifier is a simulation, show button to switch to context rather than the delete button. */
		if (modifier_can_delete(md) &&
		    (!modifier_is_simulation(md) ||
		     STREQ(scene->r.engine, RE_engine_id_BLENDER_GAME)))
		{
			uiItemO(row, "", ICON_X, "OBJECT_OT_modifier_remove");
		}
		else if (modifier_is_simulation(md) == 1) {
			uiItemStringO(row, "", ICON_BUTS, "WM_OT_properties_context_change", "context", "PHYSICS");
		}
		else if (modifier_is_simulation(md) == 2) {
			uiItemStringO(row, "", ICON_BUTS, "WM_OT_properties_context_change", "context", "PARTICLES");
		}
		UI_block_emboss_set(block, UI_EMBOSS);
	}

	
	/* modifier settings (under the header) --------------------------------------------------- */
	if (!isVirtual && (md->mode & eModifierMode_Expanded)) {
		/* apply/convert/copy */
		box = uiLayoutBox(column);
		row = uiLayoutRow(box, false);
		
		if (!ELEM(md->type, eModifierType_Collision, eModifierType_Surface)) {
			/* only here obdata, the rest of modifiers is ob level */
			UI_block_lock_set(block, BKE_object_obdata_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
			
			if (md->type == eModifierType_ParticleSystem) {
				ParticleSystem *psys = ((ParticleSystemModifierData *)md)->psys;
				
				if (!(ob->mode & OB_MODE_PARTICLE_EDIT)) {
					if (ELEM(psys->part->ren_as, PART_DRAW_GR, PART_DRAW_OB))
						uiItemO(row, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Convert"), ICON_NONE,
						        "OBJECT_OT_duplicates_make_real");
					else if (psys->part->ren_as == PART_DRAW_PATH && psys->pathcache)
						uiItemO(row, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Convert"), ICON_NONE,
						        "OBJECT_OT_modifier_convert");
				}
			}
			else {
				uiLayoutSetOperatorContext(row, WM_OP_INVOKE_DEFAULT);
				uiItemEnumO(row, "OBJECT_OT_modifier_apply", CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Apply"),
				            0, "apply_as", MODIFIER_APPLY_DATA);
				
				if (modifier_isSameTopology(md) && !modifier_isNonGeometrical(md)) {
					uiItemEnumO(row, "OBJECT_OT_modifier_apply",
					            CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Apply as Shape Key"),
					            0, "apply_as", MODIFIER_APPLY_SHAPE);
				}
			}
			
			UI_block_lock_clear(block);
			UI_block_lock_set(block, ob && ob->id.lib, ERROR_LIBDATA_MESSAGE);
			
			if (!ELEM(md->type, eModifierType_Fluidsim, eModifierType_Softbody, eModifierType_ParticleSystem,
			           eModifierType_Cloth, eModifierType_Smoke))
			{
				uiItemO(row, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Copy"), ICON_NONE,
				        "OBJECT_OT_modifier_copy");
			}
		}
		
		/* result is the layout block inside the box, that we return so that modifier settings can be drawn */
		result = uiLayoutColumn(box, false);
		block = uiLayoutAbsoluteBlock(box);
	}
	
	/* error messages */
	if (md->error) {
		box = uiLayoutBox(column);
		row = uiLayoutRow(box, false);
		uiItemL(row, md->error, ICON_ERROR);
	}
	
	return result;
}

uiLayout *uiTemplateModifier(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob;
	ModifierData *md, *vmd;
	VirtualModifierData virtualModifierData;
	int i, lastCageIndex, cageIndex;

	/* verify we have valid data */
	if (!RNA_struct_is_a(ptr->type, &RNA_Modifier)) {
		RNA_warning("Expected modifier on object");
		return NULL;
	}

	ob = ptr->id.data;
	md = ptr->data;

	if (!ob || !(GS(ob->id.name) == ID_OB)) {
		RNA_warning("Expected modifier on object");
		return NULL;
	}
	
	UI_block_lock_set(uiLayoutGetBlock(layout), (ob && ob->id.lib), ERROR_LIBDATA_MESSAGE);
	
	/* find modifier and draw it */
	cageIndex = modifiers_getCageIndex(scene, ob, &lastCageIndex, 0);

	/* XXX virtual modifiers are not accesible for python */
	vmd = modifiers_getVirtualModifierList(ob, &virtualModifierData);

	for (i = 0; vmd; i++, vmd = vmd->next) {
		if (md == vmd)
			return draw_modifier(layout, scene, ob, md, i, cageIndex, lastCageIndex);
		else if (vmd->mode & eModifierMode_Virtual)
			i--;
	}

	return NULL;
}

/************************ Constraint Template *************************/

#include "DNA_constraint_types.h"

#include "BKE_action.h"
#include "BKE_constraint.h"

#define B_CONSTRAINT_TEST           5
// #define B_CONSTRAINT_CHANGETARGET   6

static void do_constraint_panels(bContext *C, void *ob_pt, int event)
{
	Object *ob = (Object *)ob_pt;

	switch (event) {
		case B_CONSTRAINT_TEST:
			break; /* no handling */
#if 0	/* UNUSED */
		case B_CONSTRAINT_CHANGETARGET:
		{
			Main *bmain = CTX_data_main(C);
			if (ob->pose) ob->pose->flag |= POSE_RECALC;  /* checks & sorts pose channels */
			DAG_relations_tag_update(bmain);
			break;
		}
#endif
		default:
			break;
	}

	/* note: RNA updates now call this, commenting else it gets called twice.
	 * if there are problems because of this, then rna needs changed update functions.
	 *
	 * object_test_constraints(ob);
	 * if (ob->pose) BKE_pose_update_constraint_flags(ob->pose); */
	
	if (ob->type == OB_ARMATURE) DAG_id_tag_update(&ob->id, OB_RECALC_DATA | OB_RECALC_OB);
	else DAG_id_tag_update(&ob->id, OB_RECALC_OB);

	WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);
}

static void constraint_active_func(bContext *UNUSED(C), void *ob_v, void *con_v)
{
	ED_object_constraint_set_active(ob_v, con_v);
}

/* draw panel showing settings for a constraint */
static uiLayout *draw_constraint(uiLayout *layout, Object *ob, bConstraint *con)
{
	bPoseChannel *pchan = BKE_pose_channel_active(ob);
	bConstraintTypeInfo *cti;
	uiBlock *block;
	uiLayout *result = NULL, *col, *box, *row;
	PointerRNA ptr;
	char typestr[32];
	short proxy_protected, xco = 0, yco = 0;
	// int rb_col; // UNUSED

	/* get constraint typeinfo */
	cti = BKE_constraint_typeinfo_get(con);
	if (cti == NULL) {
		/* exception for 'Null' constraint - it doesn't have constraint typeinfo! */
		BLI_strncpy(typestr, (con->type == CONSTRAINT_TYPE_NULL) ? IFACE_("Null") : IFACE_("Unknown"), sizeof(typestr));
	}
	else
		BLI_strncpy(typestr, IFACE_(cti->name), sizeof(typestr));
		
	/* determine whether constraint is proxy protected or not */
	if (BKE_constraints_proxylocked_owner(ob, pchan))
		proxy_protected = (con->flag & CONSTRAINT_PROXY_LOCAL) == 0;
	else
		proxy_protected = 0;

	/* unless button has own callback, it adds this callback to button */
	block = uiLayoutGetBlock(layout);
	UI_block_func_handle_set(block, do_constraint_panels, ob);
	UI_block_func_set(block, constraint_active_func, ob, con);

	RNA_pointer_create(&ob->id, &RNA_Constraint, con, &ptr);

	col = uiLayoutColumn(layout, true);
	uiLayoutSetContextPointer(col, "constraint", &ptr);

	box = uiLayoutBox(col);
	row = uiLayoutRow(box, false);
	block = uiLayoutGetBlock(box);

	/* Draw constraint header */

	/* open/close */
	UI_block_emboss_set(block, UI_EMBOSS_NONE);
	uiItemR(row, &ptr, "show_expanded", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
	UI_block_emboss_set(block, UI_EMBOSS);

	/* name */
	uiDefBut(block, UI_BTYPE_LABEL, B_CONSTRAINT_TEST, typestr,
	         xco + 0.5f * UI_UNIT_X, yco, 5 * UI_UNIT_X, 0.9f * UI_UNIT_Y, NULL, 0.0, 0.0, 0.0, 0.0, "");

	if (con->flag & CONSTRAINT_DISABLE)
		uiLayoutSetRedAlert(row, true);
	
	if (proxy_protected == 0) {
		uiItemR(row, &ptr, "name", 0, "", ICON_NONE);
	}
	else
		uiItemL(row, con->name, ICON_NONE);
	
	uiLayoutSetRedAlert(row, false);
	
	/* proxy-protected constraints cannot be edited, so hide up/down + close buttons */
	if (proxy_protected) {
		UI_block_emboss_set(block, UI_EMBOSS_NONE);
		
		/* draw a ghost icon (for proxy) and also a lock beside it, to show that constraint is "proxy locked" */
		uiDefIconBut(block, UI_BTYPE_BUT, B_CONSTRAINT_TEST, ICON_GHOST, xco + 12.2f * UI_UNIT_X, yco, 0.95f * UI_UNIT_X, 0.95f * UI_UNIT_Y,
		             NULL, 0.0, 0.0, 0.0, 0.0, TIP_("Proxy Protected"));
		uiDefIconBut(block, UI_BTYPE_BUT, B_CONSTRAINT_TEST, ICON_LOCKED, xco + 13.1f * UI_UNIT_X, yco, 0.95f * UI_UNIT_X, 0.95f * UI_UNIT_Y,
		             NULL, 0.0, 0.0, 0.0, 0.0, TIP_("Proxy Protected"));
		
		UI_block_emboss_set(block, UI_EMBOSS);
	}
	else {
		short prev_proxylock, show_upbut, show_downbut;
		
		/* Up/Down buttons: 
		 *	Proxy-constraints are not allowed to occur after local (non-proxy) constraints
		 *	as that poses problems when restoring them, so disable the "up" button where
		 *	it may cause this situation. 
		 *
		 *  Up/Down buttons should only be shown (or not grayed - todo) if they serve some purpose.
		 */
		if (BKE_constraints_proxylocked_owner(ob, pchan)) {
			if (con->prev) {
				prev_proxylock = (con->prev->flag & CONSTRAINT_PROXY_LOCAL) ? 0 : 1;
			}
			else
				prev_proxylock = 0;
		}
		else
			prev_proxylock = 0;
			
		show_upbut = ((prev_proxylock == 0) && (con->prev));
		show_downbut = (con->next) ? 1 : 0;
		
		/* enabled */
		UI_block_emboss_set(block, UI_EMBOSS_NONE);
		uiItemR(row, &ptr, "mute", 0, "",
		        (con->flag & CONSTRAINT_OFF) ? ICON_RESTRICT_VIEW_ON : ICON_RESTRICT_VIEW_OFF);
		UI_block_emboss_set(block, UI_EMBOSS);
		
		uiLayoutSetOperatorContext(row, WM_OP_INVOKE_DEFAULT);
		
		/* up/down */
		if (show_upbut || show_downbut) {
			UI_block_align_begin(block);
			if (show_upbut)
				uiItemO(row, "", ICON_TRIA_UP, "CONSTRAINT_OT_move_up");
				
			if (show_downbut)
				uiItemO(row, "", ICON_TRIA_DOWN, "CONSTRAINT_OT_move_down");
			UI_block_align_end(block);
		}
		
		/* Close 'button' - emboss calls here disable drawing of 'button' behind X */
		UI_block_emboss_set(block, UI_EMBOSS_NONE);
		uiItemO(row, "", ICON_X, "CONSTRAINT_OT_delete");
		UI_block_emboss_set(block, UI_EMBOSS);
	}

	/* Set but-locks for protected settings (magic numbers are used here!) */
	if (proxy_protected)
		UI_block_lock_set(block, true, IFACE_("Cannot edit Proxy-Protected Constraint"));

	/* Draw constraint data */
	if ((con->flag & CONSTRAINT_EXPAND) == 0) {
		(yco) -= 10.5f * UI_UNIT_Y;
	}
	else {
		box = uiLayoutBox(col);
		block = uiLayoutAbsoluteBlock(box);
		result = box;
	}

	/* clear any locks set up for proxies/lib-linking */
	UI_block_lock_clear(block);

	return result;
}

uiLayout *uiTemplateConstraint(uiLayout *layout, PointerRNA *ptr)
{
	Object *ob;
	bConstraint *con;

	/* verify we have valid data */
	if (!RNA_struct_is_a(ptr->type, &RNA_Constraint)) {
		RNA_warning("Expected constraint on object");
		return NULL;
	}

	ob = ptr->id.data;
	con = ptr->data;

	if (!ob || !(GS(ob->id.name) == ID_OB)) {
		RNA_warning("Expected constraint on object");
		return NULL;
	}
	
	UI_block_lock_set(uiLayoutGetBlock(layout), (ob && ob->id.lib), ERROR_LIBDATA_MESSAGE);

	/* hrms, the temporal constraint should not draw! */
	if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
		bKinematicConstraint *data = con->data;
		if (data->flag & CONSTRAINT_IK_TEMP)
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
	switch (event) {
		case B_MATPRV:
			WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_PREVIEW, arg);
			break;
	}
}

void uiTemplatePreview(uiLayout *layout, bContext *C, ID *id, int show_buttons, ID *parent, MTex *slot,
                       const char *preview_id)
{
	uiLayout *row, *col;
	uiBlock *block;
	uiPreview *ui_preview = NULL;
	ARegion *ar;

	Material *ma = NULL;
	Tex *tex = (Tex *)id;
	ID *pid, *pparent;
	short *pr_texture = NULL;
	PointerRNA material_ptr;
	PointerRNA texture_ptr;

	char _preview_id[UI_MAX_NAME_STR];

	if (id && !ELEM(GS(id->name), ID_MA, ID_TE, ID_WO, ID_LA, ID_LS)) {
		RNA_warning("Expected ID of type material, texture, lamp, world or line style");
		return;
	}

	/* decide what to render */
	pid = id;
	pparent = NULL;

	if (id && (GS(id->name) == ID_TE)) {
		if (parent && (GS(parent->name) == ID_MA))
			pr_texture = &((Material *)parent)->pr_texture;
		else if (parent && (GS(parent->name) == ID_WO))
			pr_texture = &((World *)parent)->pr_texture;
		else if (parent && (GS(parent->name) == ID_LA))
			pr_texture = &((Lamp *)parent)->pr_texture;
		else if (parent && (GS(parent->name) == ID_LS))
			pr_texture = &((FreestyleLineStyle *)parent)->pr_texture;

		if (pr_texture) {
			if (*pr_texture == TEX_PR_OTHER)
				pid = parent;
			else if (*pr_texture == TEX_PR_BOTH)
				pparent = parent;
		}
	}

	if (!preview_id || (preview_id[0] == '\0')) {
		/* If no identifier given, generate one from ID type. */
		BLI_snprintf(_preview_id, UI_MAX_NAME_STR, "uiPreview_%s", BKE_idcode_to_name(GS(id->name)));
		preview_id = _preview_id;
	}

	/* Find or add the uiPreview to the current Region. */
	ar = CTX_wm_region(C);
	ui_preview = BLI_findstring(&ar->ui_previews, preview_id, offsetof(uiPreview, preview_id));

	if (!ui_preview) {
		ui_preview = MEM_callocN(sizeof(uiPreview), "uiPreview");
		BLI_strncpy(ui_preview->preview_id, preview_id, sizeof(ui_preview->preview_id));
		ui_preview->height = (short)(UI_UNIT_Y * 5.6f);
		BLI_addtail(&ar->ui_previews, ui_preview);
	}

	if (ui_preview->height < UI_UNIT_Y) {
		ui_preview->height = UI_UNIT_Y;
	}
	else if (ui_preview->height > UI_UNIT_Y * 50) {  /* Rather high upper limit, yet not insane! */
		ui_preview->height = UI_UNIT_Y * 50;
	}

	/* layout */
	block = uiLayoutGetBlock(layout);
	row = uiLayoutRow(layout, false);
	col = uiLayoutColumn(row, false);
	uiLayoutSetKeepAspect(col, true);

	/* add preview */
	uiDefBut(block, UI_BTYPE_EXTRA, 0, "", 0, 0, UI_UNIT_X * 10, ui_preview->height, pid, 0.0, 0.0, 0, 0, "");
	UI_but_func_drawextra_set(block, ED_preview_draw, pparent, slot);
	UI_block_func_handle_set(block, do_preview_buttons, NULL);

	uiDefIconButS(block, UI_BTYPE_GRIP, 0, ICON_GRIP, 0, 0, UI_UNIT_X * 10, (short)(UI_UNIT_Y * 0.3f), &ui_preview->height,
	              UI_UNIT_Y, UI_UNIT_Y * 50.0f, 0.0f, 0.0f, "");

	/* add buttons */
	if (pid && show_buttons) {
		if (GS(pid->name) == ID_MA || (pparent && GS(pparent->name) == ID_MA)) {
			if (GS(pid->name) == ID_MA) ma = (Material *)pid;
			else ma = (Material *)pparent;
			
			/* Create RNA Pointer */
			RNA_pointer_create(&ma->id, &RNA_Material, ma, &material_ptr);

			col = uiLayoutColumn(row, true);
			uiLayoutSetScaleX(col, 1.5);
			uiItemR(col, &material_ptr, "preview_render_type", UI_ITEM_R_EXPAND, "", ICON_NONE);
		}

		if (pr_texture) {
			/* Create RNA Pointer */
			RNA_pointer_create(id, &RNA_Texture, tex, &texture_ptr);
			
			uiLayoutRow(layout, true);
			uiDefButS(block, UI_BTYPE_ROW, B_MATPRV, IFACE_("Texture"),  0, 0, UI_UNIT_X * 10, UI_UNIT_Y,
			          pr_texture, 10, TEX_PR_TEXTURE, 0, 0, "");
			if (GS(parent->name) == ID_MA) {
				uiDefButS(block, UI_BTYPE_ROW, B_MATPRV, IFACE_("Material"),  0, 0, UI_UNIT_X * 10, UI_UNIT_Y,
				          pr_texture, 10, TEX_PR_OTHER, 0, 0, "");
			}
			else if (GS(parent->name) == ID_LA) {
				uiDefButS(block, UI_BTYPE_ROW, B_MATPRV, IFACE_("Lamp"),  0, 0, UI_UNIT_X * 10, UI_UNIT_Y,
				          pr_texture, 10, TEX_PR_OTHER, 0, 0, "");
			}
			else if (GS(parent->name) == ID_WO) {
				uiDefButS(block, UI_BTYPE_ROW, B_MATPRV, IFACE_("World"),  0, 0, UI_UNIT_X * 10, UI_UNIT_Y,
				          pr_texture, 10, TEX_PR_OTHER, 0, 0, "");
			}
			else if (GS(parent->name) == ID_LS) {
				uiDefButS(block, UI_BTYPE_ROW, B_MATPRV, IFACE_("Line Style"),  0, 0, UI_UNIT_X * 10, UI_UNIT_Y,
				          pr_texture, 10, TEX_PR_OTHER, 0, 0, "");
			}
			uiDefButS(block, UI_BTYPE_ROW, B_MATPRV, IFACE_("Both"),  0, 0, UI_UNIT_X * 10, UI_UNIT_Y,
			          pr_texture, 10, TEX_PR_BOTH, 0, 0, "");
			
			/* Alpha button for texture preview */
			if (*pr_texture != TEX_PR_OTHER) {
				row = uiLayoutRow(layout, false);
				uiItemR(row, &texture_ptr, "use_preview_alpha", 0, NULL, ICON_NONE);
			}
		}
	}
}

/********************** ColorRamp Template **************************/


typedef struct RNAUpdateCb {
	PointerRNA ptr;
	PropertyRNA *prop;
} RNAUpdateCb;

static void rna_update_cb(bContext *C, void *arg_cb, void *UNUSED(arg))
{
	RNAUpdateCb *cb = (RNAUpdateCb *)arg_cb;

	/* we call update here on the pointer property, this way the
	 * owner of the curve mapping can still define it's own update
	 * and notifier, even if the CurveMapping struct is shared. */
	RNA_property_update(C, &cb->ptr, cb->prop);
}

static void colorband_add_cb(bContext *C, void *cb_v, void *coba_v)
{
	ColorBand *coba = coba_v;
	float pos = 0.5f;

	if (coba->tot > 1) {
		if (coba->cur > 0) pos = (coba->data[coba->cur - 1].pos + coba->data[coba->cur].pos) * 0.5f;
		else pos = (coba->data[coba->cur + 1].pos + coba->data[coba->cur].pos) * 0.5f;
	}

	if (colorband_element_add(coba, pos)) {
		rna_update_cb(C, cb_v, NULL);
		ED_undo_push(C, "Add colorband");
	}
}

static void colorband_del_cb(bContext *C, void *cb_v, void *coba_v)
{
	ColorBand *coba = coba_v;

	if (colorband_element_remove(coba, coba->cur)) {
		ED_undo_push(C, "Delete colorband");
		rna_update_cb(C, cb_v, NULL);
	}
}

static void colorband_flip_cb(bContext *C, void *cb_v, void *coba_v)
{
	CBData data_tmp[MAXCOLORBAND];

	ColorBand *coba = coba_v;
	int a;

	for (a = 0; a < coba->tot; a++) {
		data_tmp[a] = coba->data[coba->tot - (a + 1)];
	}
	for (a = 0; a < coba->tot; a++) {
		data_tmp[a].pos = 1.0f - data_tmp[a].pos;
		coba->data[a] = data_tmp[a];
	}

	/* may as well flip the cur*/
	coba->cur = coba->tot - (coba->cur + 1);

	ED_undo_push(C, "Flip colorband");

	rna_update_cb(C, cb_v, NULL);
}

static void colorband_update_cb(bContext *UNUSED(C), void *bt_v, void *coba_v)
{
	uiBut *bt = bt_v;
	ColorBand *coba = coba_v;

	/* sneaky update here, we need to sort the colorband points to be in order,
	 * however the RNA pointer then is wrong, so we update it */
	colorband_update_sort(coba);
	bt->rnapoin.data = coba->data + coba->cur;
}

static void colorband_buttons_layout(uiLayout *layout, uiBlock *block, ColorBand *coba, const rctf *butr,
                                     RNAUpdateCb *cb, int expand)
{
	uiLayout *row, *split, *subsplit;
	uiBut *bt;
	float unit = BLI_rctf_size_x(butr) / 14.0f;
	float xs = butr->xmin;
	float ys = butr->ymin;
	PointerRNA ptr;

	RNA_pointer_create(cb->ptr.id.data, &RNA_ColorRamp, coba, &ptr);

	split = uiLayoutSplit(layout, 0.4f, false);

	UI_block_emboss_set(block, UI_EMBOSS_NONE);
	UI_block_align_begin(block);
	row = uiLayoutRow(split, false);

	bt = uiDefIconTextBut(block, UI_BTYPE_BUT, 0, ICON_ZOOMIN, "", 0, 0, 2.0f * unit, UI_UNIT_Y, NULL,
	                      0, 0, 0, 0, TIP_("Add a new color stop to the colorband"));

	UI_but_funcN_set(bt, colorband_add_cb, MEM_dupallocN(cb), coba);

	bt = uiDefIconTextBut(block, UI_BTYPE_BUT, 0, ICON_ZOOMOUT, "", xs +  2.0f * unit, ys + UI_UNIT_Y, 2.0f * unit, UI_UNIT_Y,
	              NULL, 0, 0, 0, 0, TIP_("Delete the active position"));
	UI_but_funcN_set(bt, colorband_del_cb, MEM_dupallocN(cb), coba);

	bt = uiDefIconTextBut(block, UI_BTYPE_BUT, 0, ICON_ARROW_LEFTRIGHT, "", xs + 4.0f * unit, ys + UI_UNIT_Y, 2.0f * unit, UI_UNIT_Y,
	              NULL, 0, 0, 0, 0, TIP_("Flip the color ramp"));
	UI_but_funcN_set(bt, colorband_flip_cb, MEM_dupallocN(cb), coba);
	UI_block_align_end(block);
	UI_block_emboss_set(block, UI_EMBOSS);

	row = uiLayoutRow(split, false);

	UI_block_align_begin(block);
	uiItemR(row, &ptr, "color_mode", 0, "", ICON_NONE);
	if (ELEM(coba->color_mode, COLBAND_BLEND_HSV, COLBAND_BLEND_HSL)) {
		uiItemR(row, &ptr, "hue_interpolation", 0, "", ICON_NONE);
	}
	else {  /* COLBAND_BLEND_RGB */
		uiItemR(row, &ptr, "interpolation", 0, "", ICON_NONE);
	}
	UI_block_align_end(block);

	row = uiLayoutRow(layout, false);

	bt = uiDefBut(block, UI_BTYPE_COLORBAND, 0, "", xs, ys, BLI_rctf_size_x(butr), UI_UNIT_Y, coba, 0, 0, 0, 0, "");
	UI_but_funcN_set(bt, rna_update_cb, MEM_dupallocN(cb), NULL);

	row = uiLayoutRow(layout, false);

	if (coba->tot) {
		CBData *cbd = coba->data + coba->cur;

		RNA_pointer_create(cb->ptr.id.data, &RNA_ColorRampElement, cbd, &ptr);

		if (!expand) {
			split = uiLayoutSplit(layout, 0.3f, false);

			row = uiLayoutRow(split, false);
			uiDefButS(block, UI_BTYPE_NUM, 0, "", 0, 0, 5.0f * UI_UNIT_X, UI_UNIT_Y, &coba->cur, 0.0, (float)(MAX2(0, coba->tot - 1)),
			          0, 0, TIP_("Choose active color stop"));
			row = uiLayoutRow(split, false);
			uiItemR(row, &ptr, "position", 0, IFACE_("Pos"), ICON_NONE);
			bt = block->buttons.last;
			UI_but_func_set(bt, colorband_update_cb, bt, coba);

			row = uiLayoutRow(layout, false);
			uiItemR(row, &ptr, "color", 0, "", ICON_NONE);
			bt = block->buttons.last;
			UI_but_funcN_set(bt, rna_update_cb, MEM_dupallocN(cb), NULL);
		}
		else {
			split = uiLayoutSplit(layout, 0.5f, false);
			subsplit = uiLayoutSplit(split, 0.35f, false);

			row = uiLayoutRow(subsplit, false);
			uiDefButS(block, UI_BTYPE_NUM, 0, "", 0, 0, 5.0f * UI_UNIT_X, UI_UNIT_Y, &coba->cur, 0.0, (float)(MAX2(0, coba->tot - 1)),
			          0, 0, TIP_("Choose active color stop"));
			row = uiLayoutRow(subsplit, false);
			uiItemR(row, &ptr, "position", UI_ITEM_R_SLIDER, IFACE_("Pos"), ICON_NONE);
			bt = block->buttons.last;
			UI_but_func_set(bt, colorband_update_cb, bt, coba);

			row = uiLayoutRow(split, false);
			uiItemR(row, &ptr, "color", 0, "", ICON_NONE);
			bt = block->buttons.last;
			UI_but_funcN_set(bt, rna_update_cb, MEM_dupallocN(cb), NULL);
		}
	}
}

void uiTemplateColorRamp(uiLayout *layout, PointerRNA *ptr, const char *propname, int expand)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
	PointerRNA cptr;
	RNAUpdateCb *cb;
	uiBlock *block;
	ID *id;
	rctf rect;

	if (!prop || RNA_property_type(prop) != PROP_POINTER)
		return;

	cptr = RNA_property_pointer_get(ptr, prop);
	if (!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_ColorRamp))
		return;

	cb = MEM_callocN(sizeof(RNAUpdateCb), "RNAUpdateCb");
	cb->ptr = *ptr;
	cb->prop = prop;

	rect.xmin = 0; rect.xmax = 10.0f * UI_UNIT_X;
	rect.ymin = 0; rect.ymax = 19.5f * UI_UNIT_X;

	block = uiLayoutAbsoluteBlock(layout);

	id = cptr.id.data;
	UI_block_lock_set(block, (id && id->lib), ERROR_LIBDATA_MESSAGE);

	colorband_buttons_layout(layout, block, cptr.data, &rect, cb, expand);

	UI_block_lock_clear(block);

	MEM_freeN(cb);
}


/********************* Icon viewer Template ************************/

/* ID Search browse menu, open */
static uiBlock *ui_icon_view_menu_cb(bContext *C, ARegion *ar, void *arg_litem)
{
	static RNAUpdateCb cb;
	uiBlock *block;
	uiBut *but;
	int icon;
	EnumPropertyItem *item;
	int a;
	bool free;

	/* arg_litem is malloced, can be freed by parent button */
	cb = *((RNAUpdateCb *)arg_litem);
	
	/* unused */
	// icon = RNA_property_enum_get(&cb.ptr, cb.prop);
	
	block = UI_block_begin(C, ar, "_popup", UI_EMBOSS);
	UI_block_flag_enable(block, UI_BLOCK_LOOP);
	
	
	RNA_property_enum_items(C, &cb.ptr, cb.prop, &item, NULL, &free);
	
	for (a = 0; item[a].identifier; a++) {
		int x, y;
		
		/* XXX hardcoded size to 5 x unit */
		x = (a % 8) * UI_UNIT_X * 5;
		y = (a / 8) * UI_UNIT_X * 5;
		
		icon = item[a].icon;
		but = uiDefIconButR_prop(block, UI_BTYPE_ROW, 0, icon, x, y, UI_UNIT_X * 5, UI_UNIT_Y * 5, &cb.ptr, cb.prop, -1, 0, icon, -1, -1, NULL);
		UI_but_flag_enable(but, UI_HAS_ICON | UI_BUT_ICON_PREVIEW);
	}

	UI_block_bounds_set_normal(block, 0.3f * U.widget_unit);
	UI_block_direction_set(block, UI_DIR_UP);

	if (free) {
		MEM_freeN(item);
	}
	
	return block;
}

void uiTemplateIconView(uiLayout *layout, PointerRNA *ptr, const char *propname)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
	RNAUpdateCb *cb;
	uiBlock *block;
	uiBut *but;
//	rctf rect;  /* UNUSED */
	int icon;
	
	if (!prop || RNA_property_type(prop) != PROP_ENUM)
		return;
	
	icon = RNA_property_enum_get(ptr, prop);
	
	cb = MEM_callocN(sizeof(RNAUpdateCb), "RNAUpdateCb");
	cb->ptr = *ptr;
	cb->prop = prop;
	
//	rect.xmin = 0; rect.xmax = 10.0f * UI_UNIT_X;
//	rect.ymin = 0; rect.ymax = 10.0f * UI_UNIT_X;
	
	block = uiLayoutAbsoluteBlock(layout);

	but = uiDefBlockButN(block, ui_icon_view_menu_cb, MEM_dupallocN(cb), "", 0, 0, UI_UNIT_X * 6, UI_UNIT_Y * 6, "");

	
//	but = uiDefIconButR_prop(block, UI_BTYPE_ROW, 0, icon, 0, 0, BLI_rctf_size_x(&rect), BLI_rctf_size_y(&rect), ptr, prop, -1, 0, icon, -1, -1, NULL);
	
	but->icon = icon;
	UI_but_flag_enable(but, UI_HAS_ICON | UI_BUT_ICON_PREVIEW);
	
	UI_but_funcN_set(but, rna_update_cb, MEM_dupallocN(cb), NULL);
	
	MEM_freeN(cb);
}

/********************* Histogram Template ************************/

void uiTemplateHistogram(uiLayout *layout, PointerRNA *ptr, const char *propname)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
	PointerRNA cptr;
	uiBlock *block;
	uiLayout *col;
	Histogram *hist;

	if (!prop || RNA_property_type(prop) != PROP_POINTER)
		return;

	cptr = RNA_property_pointer_get(ptr, prop);
	if (!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_Histogram))
		return;
	hist = (Histogram *)cptr.data;

	if (hist->height < UI_UNIT_Y) {
		hist->height = UI_UNIT_Y;
	}
	else if (hist->height > UI_UNIT_Y * 20) {
		hist->height = UI_UNIT_Y * 20;
	}

	col = uiLayoutColumn(layout, true);
	block = uiLayoutGetBlock(col);

	uiDefBut(block, UI_BTYPE_HISTOGRAM, 0, "", 0, 0, UI_UNIT_X * 10, hist->height, hist, 0, 0, 0, 0, "");

	/* Resize grip. */
	uiDefIconButI(block, UI_BTYPE_GRIP, 0, ICON_GRIP, 0, 0, UI_UNIT_X * 10, (short)(UI_UNIT_Y * 0.3f), &hist->height,
	              UI_UNIT_Y, UI_UNIT_Y * 20.0f, 0.0f, 0.0f, "");
}

/********************* Waveform Template ************************/

void uiTemplateWaveform(uiLayout *layout, PointerRNA *ptr, const char *propname)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
	PointerRNA cptr;
	uiBlock *block;
	uiLayout *col;
	Scopes *scopes;

	if (!prop || RNA_property_type(prop) != PROP_POINTER)
		return;

	cptr = RNA_property_pointer_get(ptr, prop);
	if (!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_Scopes))
		return;
	scopes = (Scopes *)cptr.data;

	col = uiLayoutColumn(layout, true);
	block = uiLayoutGetBlock(col);

	if (scopes->wavefrm_height < UI_UNIT_Y) {
		scopes->wavefrm_height = UI_UNIT_Y;
	}
	else if (scopes->wavefrm_height > UI_UNIT_Y * 20) {
		scopes->wavefrm_height = UI_UNIT_Y * 20;
	}

	uiDefBut(block, UI_BTYPE_WAVEFORM, 0, "", 0, 0, UI_UNIT_X * 10, scopes->wavefrm_height, scopes, 0, 0, 0, 0, "");

	/* Resize grip. */
	uiDefIconButI(block, UI_BTYPE_GRIP, 0, ICON_GRIP, 0, 0, UI_UNIT_X * 10, (short)(UI_UNIT_Y * 0.3f), &scopes->wavefrm_height,
	              UI_UNIT_Y, UI_UNIT_Y * 20.0f, 0.0f, 0.0f, "");
}

/********************* Vectorscope Template ************************/

void uiTemplateVectorscope(uiLayout *layout, PointerRNA *ptr, const char *propname)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
	PointerRNA cptr;
	uiBlock *block;
	uiLayout *col;
	Scopes *scopes;

	if (!prop || RNA_property_type(prop) != PROP_POINTER)
		return;

	cptr = RNA_property_pointer_get(ptr, prop);
	if (!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_Scopes))
		return;
	scopes = (Scopes *)cptr.data;

	if (scopes->vecscope_height < UI_UNIT_Y) {
		scopes->vecscope_height = UI_UNIT_Y;
	}
	else if (scopes->vecscope_height > UI_UNIT_Y * 20) {
		scopes->vecscope_height = UI_UNIT_Y * 20;
	}

	col = uiLayoutColumn(layout, true);
	block = uiLayoutGetBlock(col);

	uiDefBut(block, UI_BTYPE_VECTORSCOPE, 0, "", 0, 0, UI_UNIT_X * 10, scopes->vecscope_height, scopes, 0, 0, 0, 0, "");

	/* Resize grip. */
	uiDefIconButI(block, UI_BTYPE_GRIP, 0, ICON_GRIP, 0, 0, UI_UNIT_X * 10, (short)(UI_UNIT_Y * 0.3f), &scopes->vecscope_height,
	              UI_UNIT_Y, UI_UNIT_Y * 20.0f, 0.0f, 0.0f, "");
}

/********************* CurveMapping Template ************************/


static void curvemap_buttons_zoom_in(bContext *C, void *cumap_v, void *UNUSED(arg))
{
	CurveMapping *cumap = cumap_v;
	float d;

	/* we allow 20 times zoom */
	if (BLI_rctf_size_x(&cumap->curr) > 0.04f * BLI_rctf_size_x(&cumap->clipr)) {
		d = 0.1154f * BLI_rctf_size_x(&cumap->curr);
		cumap->curr.xmin += d;
		cumap->curr.xmax -= d;
		d = 0.1154f * BLI_rctf_size_y(&cumap->curr);
		cumap->curr.ymin += d;
		cumap->curr.ymax -= d;
	}

	ED_region_tag_redraw(CTX_wm_region(C));
}

static void curvemap_buttons_zoom_out(bContext *C, void *cumap_v, void *UNUSED(unused))
{
	CurveMapping *cumap = cumap_v;
	float d, d1;

	/* we allow 20 times zoom, but don't view outside clip */
	if (BLI_rctf_size_x(&cumap->curr) < 20.0f * BLI_rctf_size_x(&cumap->clipr)) {
		d = d1 = 0.15f * BLI_rctf_size_x(&cumap->curr);

		if (cumap->flag & CUMA_DO_CLIP) 
			if (cumap->curr.xmin - d < cumap->clipr.xmin)
				d1 = cumap->curr.xmin - cumap->clipr.xmin;
		cumap->curr.xmin -= d1;

		d1 = d;
		if (cumap->flag & CUMA_DO_CLIP) 
			if (cumap->curr.xmax + d > cumap->clipr.xmax)
				d1 = -cumap->curr.xmax + cumap->clipr.xmax;
		cumap->curr.xmax += d1;

		d = d1 = 0.15f * BLI_rctf_size_y(&cumap->curr);

		if (cumap->flag & CUMA_DO_CLIP) 
			if (cumap->curr.ymin - d < cumap->clipr.ymin)
				d1 = cumap->curr.ymin - cumap->clipr.ymin;
		cumap->curr.ymin -= d1;

		d1 = d;
		if (cumap->flag & CUMA_DO_CLIP) 
			if (cumap->curr.ymax + d > cumap->clipr.ymax)
				d1 = -cumap->curr.ymax + cumap->clipr.ymax;
		cumap->curr.ymax += d1;
	}

	ED_region_tag_redraw(CTX_wm_region(C));
}

static void curvemap_buttons_setclip(bContext *UNUSED(C), void *cumap_v, void *UNUSED(arg))
{
	CurveMapping *cumap = cumap_v;

	curvemapping_changed(cumap, false);
}	

static void curvemap_buttons_delete(bContext *C, void *cb_v, void *cumap_v)
{
	CurveMapping *cumap = cumap_v;

	curvemap_remove(cumap->cm + cumap->cur, SELECT);
	curvemapping_changed(cumap, false);

	rna_update_cb(C, cb_v, NULL);
}

/* NOTE: this is a block-menu, needs 0 events, otherwise the menu closes */
static uiBlock *curvemap_clipping_func(bContext *C, ARegion *ar, void *cumap_v)
{
	CurveMapping *cumap = cumap_v;
	uiBlock *block;
	uiBut *bt;
	float width = 8 * UI_UNIT_X;

	block = UI_block_begin(C, ar, __func__, UI_EMBOSS);

	/* use this for a fake extra empy space around the buttons */
	uiDefBut(block, UI_BTYPE_LABEL, 0, "",           -4, 16, width + 8, 6 * UI_UNIT_Y, NULL, 0, 0, 0, 0, "");

	bt = uiDefButBitI(block, UI_BTYPE_TOGGLE, CUMA_DO_CLIP, 1, IFACE_("Use Clipping"),
	                  0, 5 * UI_UNIT_Y, width, UI_UNIT_Y, &cumap->flag, 0.0, 0.0, 10, 0, "");
	UI_but_func_set(bt, curvemap_buttons_setclip, cumap, NULL);

	UI_block_align_begin(block);
	uiDefButF(block, UI_BTYPE_NUM, 0, IFACE_("Min X "),   0, 4 * UI_UNIT_Y, width, UI_UNIT_Y,
	          &cumap->clipr.xmin, -100.0, cumap->clipr.xmax, 10, 2, "");
	uiDefButF(block, UI_BTYPE_NUM, 0, IFACE_("Min Y "),   0, 3 * UI_UNIT_Y, width, UI_UNIT_Y,
	          &cumap->clipr.ymin, -100.0, cumap->clipr.ymax, 10, 2, "");
	uiDefButF(block, UI_BTYPE_NUM, 0, IFACE_("Max X "),   0, 2 * UI_UNIT_Y, width, UI_UNIT_Y,
	          &cumap->clipr.xmax, cumap->clipr.xmin, 100.0, 10, 2, "");
	uiDefButF(block, UI_BTYPE_NUM, 0, IFACE_("Max Y "),   0, UI_UNIT_Y, width, UI_UNIT_Y,
	          &cumap->clipr.ymax, cumap->clipr.ymin, 100.0, 10, 2, "");

	UI_block_direction_set(block, UI_DIR_RIGHT);

	return block;
}

/* only for curvemap_tools_dofunc */
enum {
	UICURVE_FUNC_RESET_NEG,
	UICURVE_FUNC_RESET_POS,
	UICURVE_FUNC_RESET_VIEW,
	UICURVE_FUNC_HANDLE_VECTOR,
	UICURVE_FUNC_HANDLE_AUTO,
	UICURVE_FUNC_EXTEND_HOZ,
	UICURVE_FUNC_EXTEND_EXP,
};

static void curvemap_tools_dofunc(bContext *C, void *cumap_v, int event)
{
	CurveMapping *cumap = cumap_v;
	CurveMap *cuma = cumap->cm + cumap->cur;

	switch (event) {
		case UICURVE_FUNC_RESET_NEG:
		case UICURVE_FUNC_RESET_POS: /* reset */
			curvemap_reset(cuma, &cumap->clipr, cumap->preset,
			               (event == UICURVE_FUNC_RESET_NEG) ? CURVEMAP_SLOPE_NEGATIVE : CURVEMAP_SLOPE_POSITIVE);
			curvemapping_changed(cumap, false);
			break;
		case UICURVE_FUNC_RESET_VIEW:
			cumap->curr = cumap->clipr;
			break;
		case UICURVE_FUNC_HANDLE_VECTOR: /* set vector */
			curvemap_sethandle(cuma, 1);
			curvemapping_changed(cumap, false);
			break;
		case UICURVE_FUNC_HANDLE_AUTO: /* set auto */
			curvemap_sethandle(cuma, 0);
			curvemapping_changed(cumap, false);
			break;
		case UICURVE_FUNC_EXTEND_HOZ: /* extend horiz */
			cuma->flag &= ~CUMA_EXTEND_EXTRAPOLATE;
			curvemapping_changed(cumap, false);
			break;
		case UICURVE_FUNC_EXTEND_EXP: /* extend extrapolate */
			cuma->flag |= CUMA_EXTEND_EXTRAPOLATE;
			curvemapping_changed(cumap, false);
			break;
	}
	ED_undo_push(C, "CurveMap tools");
	ED_region_tag_redraw(CTX_wm_region(C));
}

static uiBlock *curvemap_tools_posslope_func(bContext *C, ARegion *ar, void *cumap_v)
{
	uiBlock *block;
	short yco = 0, menuwidth = 10 * UI_UNIT_X;

	block = UI_block_begin(C, ar, __func__, UI_EMBOSS);
	UI_block_func_butmenu_set(block, curvemap_tools_dofunc, cumap_v);

	uiDefIconTextBut(block, UI_BTYPE_BUT_MENU, 1, ICON_BLANK1, IFACE_("Reset View"),          0, yco -= UI_UNIT_Y,
	                 menuwidth, UI_UNIT_Y, NULL, 0.0, 0.0, 0, UICURVE_FUNC_RESET_VIEW, "");
	uiDefIconTextBut(block, UI_BTYPE_BUT_MENU, 1, ICON_BLANK1, IFACE_("Vector Handle"),       0, yco -= UI_UNIT_Y,
	                 menuwidth, UI_UNIT_Y, NULL, 0.0, 0.0, 0, UICURVE_FUNC_HANDLE_VECTOR, "");
	uiDefIconTextBut(block, UI_BTYPE_BUT_MENU, 1, ICON_BLANK1, IFACE_("Auto Handle"),         0, yco -= UI_UNIT_Y,
	                 menuwidth, UI_UNIT_Y, NULL, 0.0, 0.0, 0, UICURVE_FUNC_HANDLE_AUTO, "");
	uiDefIconTextBut(block, UI_BTYPE_BUT_MENU, 1, ICON_BLANK1, IFACE_("Extend Horizontal"),   0, yco -= UI_UNIT_Y,
	                 menuwidth, UI_UNIT_Y, NULL, 0.0, 0.0, 0, UICURVE_FUNC_EXTEND_HOZ, "");
	uiDefIconTextBut(block, UI_BTYPE_BUT_MENU, 1, ICON_BLANK1, IFACE_("Extend Extrapolated"), 0, yco -= UI_UNIT_Y,
	                 menuwidth, UI_UNIT_Y, NULL, 0.0, 0.0, 0, UICURVE_FUNC_EXTEND_EXP, "");
	uiDefIconTextBut(block, UI_BTYPE_BUT_MENU, 1, ICON_BLANK1, IFACE_("Reset Curve"),         0, yco -= UI_UNIT_Y,
	                 menuwidth, UI_UNIT_Y, NULL, 0.0, 0.0, 0, UICURVE_FUNC_RESET_POS, "");

	UI_block_direction_set(block, UI_DIR_RIGHT);
	UI_block_bounds_set_text(block, 50);

	return block;
}

static uiBlock *curvemap_tools_negslope_func(bContext *C, ARegion *ar, void *cumap_v)
{
	uiBlock *block;
	short yco = 0, menuwidth = 10 * UI_UNIT_X;

	block = UI_block_begin(C, ar, __func__, UI_EMBOSS);
	UI_block_func_butmenu_set(block, curvemap_tools_dofunc, cumap_v);

	uiDefIconTextBut(block, UI_BTYPE_BUT_MENU, 1, ICON_BLANK1, IFACE_("Reset View"),          0, yco -= UI_UNIT_Y,
	                 menuwidth, UI_UNIT_Y, NULL, 0.0, 0.0, 0, UICURVE_FUNC_RESET_VIEW, "");
	uiDefIconTextBut(block, UI_BTYPE_BUT_MENU, 1, ICON_BLANK1, IFACE_("Vector Handle"),       0, yco -= UI_UNIT_Y,
	                 menuwidth, UI_UNIT_Y, NULL, 0.0, 0.0, 0, UICURVE_FUNC_HANDLE_VECTOR, "");
	uiDefIconTextBut(block, UI_BTYPE_BUT_MENU, 1, ICON_BLANK1, IFACE_("Auto Handle"),         0, yco -= UI_UNIT_Y,
	                 menuwidth, UI_UNIT_Y, NULL, 0.0, 0.0, 0, UICURVE_FUNC_HANDLE_AUTO, "");
	uiDefIconTextBut(block, UI_BTYPE_BUT_MENU, 1, ICON_BLANK1, IFACE_("Extend Horizontal"),   0, yco -= UI_UNIT_Y,
	                 menuwidth, UI_UNIT_Y, NULL, 0.0, 0.0, 0, UICURVE_FUNC_EXTEND_HOZ, "");
	uiDefIconTextBut(block, UI_BTYPE_BUT_MENU, 1, ICON_BLANK1, IFACE_("Extend Extrapolated"), 0, yco -= UI_UNIT_Y,
	                 menuwidth, UI_UNIT_Y, NULL, 0.0, 0.0, 0, UICURVE_FUNC_EXTEND_EXP, "");
	uiDefIconTextBut(block, UI_BTYPE_BUT_MENU, 1, ICON_BLANK1, IFACE_("Reset Curve"),         0, yco -= UI_UNIT_Y,
	                 menuwidth, UI_UNIT_Y, NULL, 0.0, 0.0, 0, UICURVE_FUNC_RESET_NEG, "");

	UI_block_direction_set(block, UI_DIR_RIGHT);
	UI_block_bounds_set_text(block, 50);

	return block;
}

static uiBlock *curvemap_brush_tools_func(bContext *C, ARegion *ar, void *cumap_v)
{
	uiBlock *block;
	short yco = 0, menuwidth = 10 * UI_UNIT_X;

	block = UI_block_begin(C, ar, __func__, UI_EMBOSS);
	UI_block_func_butmenu_set(block, curvemap_tools_dofunc, cumap_v);

	uiDefIconTextBut(block, UI_BTYPE_BUT_MENU, 1, ICON_BLANK1, IFACE_("Reset View"),    0, yco -= UI_UNIT_Y,
	                 menuwidth, UI_UNIT_Y, NULL, 0.0, 0.0, 0, UICURVE_FUNC_RESET_VIEW, "");
	uiDefIconTextBut(block, UI_BTYPE_BUT_MENU, 1, ICON_BLANK1, IFACE_("Vector Handle"), 0, yco -= UI_UNIT_Y,
	                 menuwidth, UI_UNIT_Y, NULL, 0.0, 0.0, 0, UICURVE_FUNC_HANDLE_VECTOR, "");
	uiDefIconTextBut(block, UI_BTYPE_BUT_MENU, 1, ICON_BLANK1, IFACE_("Auto Handle"),   0, yco -= UI_UNIT_Y,
	                 menuwidth, UI_UNIT_Y, NULL, 0.0, 0.0, 0, UICURVE_FUNC_HANDLE_AUTO, "");
	uiDefIconTextBut(block, UI_BTYPE_BUT_MENU, 1, ICON_BLANK1, IFACE_("Reset Curve"),   0, yco -= UI_UNIT_Y,
	                 menuwidth, UI_UNIT_Y, NULL, 0.0, 0.0, 0, UICURVE_FUNC_RESET_NEG, "");

	UI_block_direction_set(block, UI_DIR_RIGHT);
	UI_block_bounds_set_text(block, 50);

	return block;
}

static void curvemap_buttons_redraw(bContext *C, void *UNUSED(arg1), void *UNUSED(arg2))
{
	ED_region_tag_redraw(CTX_wm_region(C));
}

static void curvemap_buttons_update(bContext *C, void *arg1_v, void *cumap_v)
{
	CurveMapping *cumap = cumap_v;
	curvemapping_changed(cumap, true);
	rna_update_cb(C, arg1_v, NULL);
}

static void curvemap_buttons_reset(bContext *C, void *cb_v, void *cumap_v)
{
	CurveMapping *cumap = cumap_v;
	int a;
	
	cumap->preset = CURVE_PRESET_LINE;
	for (a = 0; a < CM_TOT; a++)
		curvemap_reset(cumap->cm + a, &cumap->clipr, cumap->preset, CURVEMAP_SLOPE_POSITIVE);
	
	cumap->black[0] = cumap->black[1] = cumap->black[2] = 0.0f;
	cumap->white[0] = cumap->white[1] = cumap->white[2] = 1.0f;
	curvemapping_set_black_white(cumap, NULL, NULL);
	
	curvemapping_changed(cumap, false);

	rna_update_cb(C, cb_v, NULL);
}

/* still unsure how this call evolves... we use labeltype for defining what curve-channels to show */
static void curvemap_buttons_layout(uiLayout *layout, PointerRNA *ptr, char labeltype, int levels,
                                    int brush, int neg_slope, RNAUpdateCb *cb)
{
	CurveMapping *cumap = ptr->data;
	CurveMap *cm = &cumap->cm[cumap->cur];
	CurveMapPoint *cmp = NULL;
	uiLayout *row, *sub, *split;
	uiBlock *block;
	uiBut *bt;
	float dx = UI_UNIT_X;
	int icon, size;
	int bg = -1, i;

	block = uiLayoutGetBlock(layout);

	/* curve chooser */
	row = uiLayoutRow(layout, false);

	if (labeltype == 'v') {
		/* vector */
		sub = uiLayoutRow(row, true);
		uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_LEFT);

		if (cumap->cm[0].curve) {
			bt = uiDefButI(block, UI_BTYPE_ROW, 0, "X", 0, 0, dx, dx, &cumap->cur, 0.0, 0.0, 0.0, 0.0, "");
			UI_but_func_set(bt, curvemap_buttons_redraw, NULL, NULL);
		}
		if (cumap->cm[1].curve) {
			bt = uiDefButI(block, UI_BTYPE_ROW, 0, "Y", 0, 0, dx, dx, &cumap->cur, 0.0, 1.0, 0.0, 0.0, "");
			UI_but_func_set(bt, curvemap_buttons_redraw, NULL, NULL);
		}
		if (cumap->cm[2].curve) {
			bt = uiDefButI(block, UI_BTYPE_ROW, 0, "Z", 0, 0, dx, dx, &cumap->cur, 0.0, 2.0, 0.0, 0.0, "");
			UI_but_func_set(bt, curvemap_buttons_redraw, NULL, NULL);
		}
	}
	else if (labeltype == 'c') {
		/* color */
		sub = uiLayoutRow(row, true);
		uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_LEFT);

		if (cumap->cm[3].curve) {
			bt = uiDefButI(block, UI_BTYPE_ROW, 0, "C", 0, 0, dx, dx, &cumap->cur, 0.0, 3.0, 0.0, 0.0, "");
			UI_but_func_set(bt, curvemap_buttons_redraw, NULL, NULL);
		}
		if (cumap->cm[0].curve) {
			bt = uiDefButI(block, UI_BTYPE_ROW, 0, "R", 0, 0, dx, dx, &cumap->cur, 0.0, 0.0, 0.0, 0.0, "");
			UI_but_func_set(bt, curvemap_buttons_redraw, NULL, NULL);
		}
		if (cumap->cm[1].curve) {
			bt = uiDefButI(block, UI_BTYPE_ROW, 0, "G", 0, 0, dx, dx, &cumap->cur, 0.0, 1.0, 0.0, 0.0, "");
			UI_but_func_set(bt, curvemap_buttons_redraw, NULL, NULL);
		}
		if (cumap->cm[2].curve) {
			bt = uiDefButI(block, UI_BTYPE_ROW, 0, "B", 0, 0, dx, dx, &cumap->cur, 0.0, 2.0, 0.0, 0.0, "");
			UI_but_func_set(bt, curvemap_buttons_redraw, NULL, NULL);
		}
	}
	else if (labeltype == 'h') {
		/* HSV */
		sub = uiLayoutRow(row, true);
		uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_LEFT);
		
		if (cumap->cm[0].curve) {
			bt = uiDefButI(block, UI_BTYPE_ROW, 0, "H", 0, 0, dx, dx, &cumap->cur, 0.0, 0.0, 0.0, 0.0, "");
			UI_but_func_set(bt, curvemap_buttons_redraw, NULL, NULL);
		}
		if (cumap->cm[1].curve) {
			bt = uiDefButI(block, UI_BTYPE_ROW, 0, "S", 0, 0, dx, dx, &cumap->cur, 0.0, 1.0, 0.0, 0.0, "");
			UI_but_func_set(bt, curvemap_buttons_redraw, NULL, NULL);
		}
		if (cumap->cm[2].curve) {
			bt = uiDefButI(block, UI_BTYPE_ROW, 0, "V", 0, 0, dx, dx, &cumap->cur, 0.0, 2.0, 0.0, 0.0, "");
			UI_but_func_set(bt, curvemap_buttons_redraw, NULL, NULL);
		}
	}
	else
		uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_RIGHT);
	
	if (labeltype == 'h')
		bg = UI_GRAD_H;

	/* operation buttons */
	sub = uiLayoutRow(row, true);

	UI_block_emboss_set(block, UI_EMBOSS_NONE);

	bt = uiDefIconBut(block, UI_BTYPE_BUT, 0, ICON_ZOOMIN, 0, 0, dx, dx, NULL, 0.0, 0.0, 0.0, 0.0, TIP_("Zoom in"));
	UI_but_func_set(bt, curvemap_buttons_zoom_in, cumap, NULL);

	bt = uiDefIconBut(block, UI_BTYPE_BUT, 0, ICON_ZOOMOUT, 0, 0, dx, dx, NULL, 0.0, 0.0, 0.0, 0.0, TIP_("Zoom out"));
	UI_but_func_set(bt, curvemap_buttons_zoom_out, cumap, NULL);

	if (brush)
		bt = uiDefIconBlockBut(block, curvemap_brush_tools_func, cumap, 0, ICON_MODIFIER, 0, 0, dx, dx, TIP_("Tools"));
	else if (neg_slope)
		bt = uiDefIconBlockBut(block, curvemap_tools_negslope_func, cumap, 0, ICON_MODIFIER,
		                       0, 0, dx, dx, TIP_("Tools"));
	else
		bt = uiDefIconBlockBut(block, curvemap_tools_posslope_func, cumap, 0, ICON_MODIFIER,
		                       0, 0, dx, dx, TIP_("Tools"));

	UI_but_funcN_set(bt, rna_update_cb, MEM_dupallocN(cb), NULL);

	icon = (cumap->flag & CUMA_DO_CLIP) ? ICON_CLIPUV_HLT : ICON_CLIPUV_DEHLT;
	bt = uiDefIconBlockBut(block, curvemap_clipping_func, cumap, 0, icon, 0, 0, dx, dx, TIP_("Clipping Options"));
	UI_but_funcN_set(bt, rna_update_cb, MEM_dupallocN(cb), NULL);

	bt = uiDefIconBut(block, UI_BTYPE_BUT, 0, ICON_X, 0, 0, dx, dx, NULL, 0.0, 0.0, 0.0, 0.0, TIP_("Delete points"));
	UI_but_funcN_set(bt, curvemap_buttons_delete, MEM_dupallocN(cb), cumap);

	UI_block_emboss_set(block, UI_EMBOSS);

	UI_block_funcN_set(block, rna_update_cb, MEM_dupallocN(cb), NULL);

	/* curve itself */
	size = uiLayoutGetWidth(layout);
	row = uiLayoutRow(layout, false);
	uiDefBut(block, UI_BTYPE_CURVE, 0, "", 0, 0, size, 8.0f * UI_UNIT_X, cumap, 0.0f, 1.0f, bg, 0, "");

	/* sliders for selected point */
	for (i = 0; i < cm->totpoint; i++) {
		if (cm->curve[i].flag & CUMA_SELECT) {
			cmp = &cm->curve[i];
			break;
		}
	}

	if (cmp) {
		rctf bounds;

		if (cumap->flag & CUMA_DO_CLIP) {
			bounds = cumap->clipr;
		}
		else {
			bounds.xmin = bounds.ymin = -1000.0;
			bounds.xmax = bounds.ymax =  1000.0;
		}

		uiLayoutRow(layout, true);
		UI_block_funcN_set(block, curvemap_buttons_update, MEM_dupallocN(cb), cumap);
		uiDefButF(block, UI_BTYPE_NUM, 0, "X", 0, 2 * UI_UNIT_Y, UI_UNIT_X * 10, UI_UNIT_Y,
		          &cmp->x, bounds.xmin, bounds.xmax, 1, 5, "");
		uiDefButF(block, UI_BTYPE_NUM, 0, "Y", 0, 1 * UI_UNIT_Y, UI_UNIT_X * 10, UI_UNIT_Y,
		          &cmp->y, bounds.ymin, bounds.ymax, 1, 5, "");
	}

	/* black/white levels */
	if (levels) {
		split = uiLayoutSplit(layout, 0.0f, false);
		uiItemR(uiLayoutColumn(split, false), ptr, "black_level", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
		uiItemR(uiLayoutColumn(split, false), ptr, "white_level", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

		uiLayoutRow(layout, false);
		bt = uiDefBut(block, UI_BTYPE_BUT, 0, IFACE_("Reset"), 0, 0, UI_UNIT_X * 10, UI_UNIT_Y, NULL, 0.0f, 0.0f, 0, 0,
		              TIP_("Reset Black/White point and curves"));
		UI_but_funcN_set(bt, curvemap_buttons_reset, MEM_dupallocN(cb), cumap);
	}

	UI_block_funcN_set(block, NULL, NULL, NULL);
}

void uiTemplateCurveMapping(uiLayout *layout, PointerRNA *ptr, const char *propname, int type,
                            int levels, int brush, int neg_slope)
{
	RNAUpdateCb *cb;
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
	PointerRNA cptr;
	ID *id;
	uiBlock *block = uiLayoutGetBlock(layout);

	if (!prop) {
		RNA_warning("curve property not found: %s.%s",
		            RNA_struct_identifier(ptr->type), propname);
		return;
	}

	if (RNA_property_type(prop) != PROP_POINTER) {
		RNA_warning("curve is not a pointer: %s.%s",
		            RNA_struct_identifier(ptr->type), propname);
		return;
	}

	cptr = RNA_property_pointer_get(ptr, prop);
	if (!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_CurveMapping))
		return;

	cb = MEM_callocN(sizeof(RNAUpdateCb), "RNAUpdateCb");
	cb->ptr = *ptr;
	cb->prop = prop;

	id = cptr.id.data;
	UI_block_lock_set(block, (id && id->lib), ERROR_LIBDATA_MESSAGE);

	curvemap_buttons_layout(layout, &cptr, type, levels, brush, neg_slope, cb);

	UI_block_lock_clear(block);

	MEM_freeN(cb);
}

/********************* ColorPicker Template ************************/

#define WHEEL_SIZE  (5 * U.widget_unit)

/* This template now follows User Preference for type - name is not correct anymore... */
void uiTemplateColorPicker(uiLayout *layout, PointerRNA *ptr, const char *propname, int value_slider,
                           int lock, int lock_luminosity, int cubic)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
	uiBlock *block = uiLayoutGetBlock(layout);
	uiLayout *col, *row;
	uiBut *but = NULL;
	ColorPicker *cpicker = ui_block_colorpicker_create(block);
	float softmin, softmax, step, precision;

	if (!prop) {
		RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}

	RNA_property_float_ui_range(ptr, prop, &softmin, &softmax, &step, &precision);

	col = uiLayoutColumn(layout, true);
	row = uiLayoutRow(col, true);

	switch (U.color_picker_type) {
		case USER_CP_SQUARE_SV:
			but = uiDefButR_prop(block, UI_BTYPE_HSVCUBE, 0, "", 0, 0, WHEEL_SIZE, WHEEL_SIZE, ptr, prop,
			                     -1, 0.0, 0.0, UI_GRAD_SV, 0, "");
			break;
		case USER_CP_SQUARE_HS:
			but = uiDefButR_prop(block, UI_BTYPE_HSVCUBE, 0, "", 0, 0, WHEEL_SIZE, WHEEL_SIZE, ptr, prop,
			                     -1, 0.0, 0.0, UI_GRAD_HS, 0, "");
			break;
		case USER_CP_SQUARE_HV:
			but = uiDefButR_prop(block, UI_BTYPE_HSVCUBE, 0, "", 0, 0, WHEEL_SIZE, WHEEL_SIZE, ptr, prop,
			                     -1, 0.0, 0.0, UI_GRAD_HV, 0, "");
			break;

		/* user default */
		case USER_CP_CIRCLE_HSV:
		case USER_CP_CIRCLE_HSL:
		default:
			but = uiDefButR_prop(block, UI_BTYPE_HSVCIRCLE, 0, "", 0, 0, WHEEL_SIZE, WHEEL_SIZE, ptr, prop,
			                     -1, 0.0, 0.0, 0, 0, "");
			break;

	}

	but->custom_data = cpicker;

	if (lock) {
		but->flag |= UI_BUT_COLOR_LOCK;
	}

	if (lock_luminosity) {
		float color[4]; /* in case of alpha */
		but->flag |= UI_BUT_VEC_SIZE_LOCK;
		RNA_property_float_get_array(ptr, prop, color);
		but->a2 = len_v3(color);
	}

	if (cubic)
		but->flag |= UI_BUT_COLOR_CUBIC;

	
	if (value_slider) {
		switch (U.color_picker_type) {
			case USER_CP_CIRCLE_HSL:
				uiItemS(row);
				but = uiDefButR_prop(block, UI_BTYPE_HSVCUBE, 0, "", WHEEL_SIZE + 6, 0, 14, WHEEL_SIZE, ptr, prop,
				                     -1, softmin, softmax, UI_GRAD_L_ALT, 0, "");
				break;
			case USER_CP_SQUARE_SV:
				uiItemS(col);
				but = uiDefButR_prop(block, UI_BTYPE_HSVCUBE, 0, "", 0, 4, WHEEL_SIZE, 18, ptr, prop,
				                     -1, softmin, softmax, UI_GRAD_SV + 3, 0, "");
				break;
			case USER_CP_SQUARE_HS:
				uiItemS(col);
				but = uiDefButR_prop(block, UI_BTYPE_HSVCUBE, 0, "", 0, 4, WHEEL_SIZE, 18, ptr, prop,
				                     -1, softmin, softmax, UI_GRAD_HS + 3, 0, "");
				break;
			case USER_CP_SQUARE_HV:
				uiItemS(col);
				but = uiDefButR_prop(block, UI_BTYPE_HSVCUBE, 0, "", 0, 4, WHEEL_SIZE, 18, ptr, prop,
				                     -1, softmin, softmax, UI_GRAD_HV + 3, 0, "");
				break;

			/* user default */
			case USER_CP_CIRCLE_HSV:
			default:
				uiItemS(row);
				but = uiDefButR_prop(block, UI_BTYPE_HSVCUBE, 0, "", WHEEL_SIZE + 6, 0, 14, WHEEL_SIZE, ptr, prop,
				                     -1, softmin, softmax, UI_GRAD_V_ALT, 0, "");
				break;
		}

		but->custom_data = cpicker;
	}
}

void uiTemplatePalette(uiLayout *layout, PointerRNA *ptr, const char *propname, int UNUSED(colors))
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
	PointerRNA cptr;
	Palette *palette;
	PaletteColor *color;
	uiBlock *block;
	uiLayout *col;
	int row_cols = 0, col_id = 0;
	int cols_per_row = MAX2(uiLayoutGetWidth(layout) / UI_UNIT_X, 1);

	if (!prop) {
		RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}

	cptr = RNA_property_pointer_get(ptr, prop);
	if (!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_Palette))
		return;

	block = uiLayoutGetBlock(layout);

	palette = cptr.data;

	/* first delete any pending colors */
	BKE_palette_cleanup(palette);

	color = palette->colors.first;

	col = uiLayoutColumn(layout, true);
	uiLayoutRow(col, true);
	uiDefIconButO(block, UI_BTYPE_BUT, "PALETTE_OT_color_add", WM_OP_INVOKE_DEFAULT, ICON_ZOOMIN, 0, 0, UI_UNIT_X, UI_UNIT_Y, NULL);
	uiDefIconButO(block, UI_BTYPE_BUT, "PALETTE_OT_color_delete", WM_OP_INVOKE_DEFAULT, ICON_ZOOMOUT, 0, 0, UI_UNIT_X, UI_UNIT_Y, NULL);

	col = uiLayoutColumn(layout, true);
	uiLayoutRow(col, true);

	for (; color; color = color->next) {
		PointerRNA ptr;

		if (row_cols >= cols_per_row) {
			uiLayoutRow(col, true);
			row_cols = 0;
		}

		RNA_pointer_create(&palette->id, &RNA_PaletteColor, color, &ptr);
		uiDefButR(block, UI_BTYPE_COLOR, 0, "", 0, 0, UI_UNIT_X, UI_UNIT_Y, &ptr, "color", -1, 0.0, 1.0,
		          UI_PALETTE_COLOR, col_id, "");
		row_cols++;
		col_id++;
	}
}


/********************* Layer Buttons Template ************************/

static void handle_layer_buttons(bContext *C, void *arg1, void *arg2)
{
	uiBut *but = arg1;
	int cur = GET_INT_FROM_POINTER(arg2);
	wmWindow *win = CTX_wm_window(C);
	int i, tot, shift = win->eventstate->shift;

	if (!shift) {
		tot = RNA_property_array_length(&but->rnapoin, but->rnaprop);
		
		/* Normally clicking only selects one layer */
		RNA_property_boolean_set_index(&but->rnapoin, but->rnaprop, cur, true);
		for (i = 0; i < tot; ++i) {
			if (i != cur)
				RNA_property_boolean_set_index(&but->rnapoin, but->rnaprop, i, false);
		}
	}

	/* view3d layer change should update depsgraph (invisible object changed maybe) */
	/* see view3d_header.c */
}

/* TODO:
 * - for now, grouping of layers is determined by dividing up the length of
 *   the array of layer bitflags */

void uiTemplateLayers(uiLayout *layout, PointerRNA *ptr, const char *propname,
                      PointerRNA *used_ptr, const char *used_propname, int active_layer)
{
	uiLayout *uRow, *uCol;
	PropertyRNA *prop, *used_prop = NULL;
	int groups, cols, layers;
	int group, col, layer, row;
	int cols_per_group = 5;

	prop = RNA_struct_find_property(ptr, propname);
	if (!prop) {
		RNA_warning("layers property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}
	
	/* the number of layers determines the way we group them 
	 *	- we want 2 rows only (for now)
	 *	- the number of columns (cols) is the total number of buttons per row
	 *	  the 'remainder' is added to this, as it will be ok to have first row slightly wider if need be
	 *	- for now, only split into groups if group will have at least 5 items
	 */
	layers = RNA_property_array_length(ptr, prop);
	cols = (layers / 2) + (layers % 2);
	groups = ((cols / 2) < cols_per_group) ? (1) : (cols / cols_per_group);

	if (used_ptr && used_propname) {
		used_prop = RNA_struct_find_property(used_ptr, used_propname);
		if (!used_prop) {
			RNA_warning("used layers property not found: %s.%s", RNA_struct_identifier(ptr->type), used_propname);
			return;
		}

		if (RNA_property_array_length(used_ptr, used_prop) < layers)
			used_prop = NULL;
	}
	
	/* layers are laid out going across rows, with the columns being divided into groups */
	
	for (group = 0; group < groups; group++) {
		uCol = uiLayoutColumn(layout, true);
		
		for (row = 0; row < 2; row++) {
			uiBlock *block;
			uiBut *but;

			uRow = uiLayoutRow(uCol, true);
			block = uiLayoutGetBlock(uRow);
			layer = groups * cols_per_group * row + cols_per_group * group;
			
			/* add layers as toggle buts */
			for (col = 0; (col < cols_per_group) && (layer < layers); col++, layer++) {
				int icon = 0;
				int butlay = 1 << layer;

				if (active_layer & butlay)
					icon = ICON_LAYER_ACTIVE;
				else if (used_prop && RNA_property_boolean_get_index(used_ptr, used_prop, layer))
					icon = ICON_LAYER_USED;
				
				but = uiDefAutoButR(block, ptr, prop, layer, "", icon, 0, 0, UI_UNIT_X / 2, UI_UNIT_Y / 2);
				UI_but_func_set(but, handle_layer_buttons, but, SET_INT_IN_POINTER(layer));
				but->type = UI_BTYPE_TOGGLE;
			}
		}
	}
}

void uiTemplateGameStates(uiLayout *layout, PointerRNA *ptr, const char *propname,
                          PointerRNA *used_ptr, const char *used_propname, int active_state)
{
	uiLayout *uRow, *uCol;
	PropertyRNA *prop, *used_prop = NULL;
	int groups, cols, states;
	int group, col, state, row;
	int cols_per_group = 5;
	Object *ob = (Object *)ptr->id.data;

	prop = RNA_struct_find_property(ptr, propname);
	if (!prop) {
		RNA_warning("states property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}
	
	/* the number of states determines the way we group them 
	 *	- we want 2 rows only (for now)
	 *	- the number of columns (cols) is the total number of buttons per row
	 *	  the 'remainder' is added to this, as it will be ok to have first row slightly wider if need be
	 *	- for now, only split into groups if group will have at least 5 items
	 */
	states = RNA_property_array_length(ptr, prop);
	cols = (states / 2) + (states % 2);
	groups = ((cols / 2) < cols_per_group) ? (1) : (cols / cols_per_group);

	if (used_ptr && used_propname) {
		used_prop = RNA_struct_find_property(used_ptr, used_propname);
		if (!used_prop) {
			RNA_warning("used layers property not found: %s.%s", RNA_struct_identifier(ptr->type), used_propname);
			return;
		}

		if (RNA_property_array_length(used_ptr, used_prop) < states)
			used_prop = NULL;
	}
	
	/* layers are laid out going across rows, with the columns being divided into groups */
	
	for (group = 0; group < groups; group++) {
		uCol = uiLayoutColumn(layout, true);
		
		for (row = 0; row < 2; row++) {
			uiBlock *block;
			uiBut *but;

			uRow = uiLayoutRow(uCol, true);
			block = uiLayoutGetBlock(uRow);
			state = groups * cols_per_group * row + cols_per_group * group;
			
			/* add layers as toggle buts */
			for (col = 0; (col < cols_per_group) && (state < states); col++, state++) {
				int icon = 0;
				int butlay = 1 << state;

				if (active_state & butlay)
					icon = ICON_LAYER_ACTIVE;
				else if (used_prop && RNA_property_boolean_get_index(used_ptr, used_prop, state))
					icon = ICON_LAYER_USED;
				
				but = uiDefIconButR_prop(block, UI_BTYPE_ICON_TOGGLE, 0, icon, 0, 0, UI_UNIT_X / 2, UI_UNIT_Y / 2, ptr, prop,
				                         state, 0, 0, -1, -1, sca_state_name_get(ob, state));
				UI_but_func_set(but, handle_layer_buttons, but, SET_INT_IN_POINTER(state));
				but->type = UI_BTYPE_TOGGLE;
			}
		}
	}
}


/************************* List Template **************************/
static void uilist_draw_item_default(struct uiList *ui_list, struct bContext *UNUSED(C), struct uiLayout *layout,
                                     struct PointerRNA *UNUSED(dataptr), struct PointerRNA *itemptr, int icon,
                                     struct PointerRNA *UNUSED(active_dataptr), const char *UNUSED(active_propname),
                                     int UNUSED(index), int UNUSED(flt_flag))
{
	PropertyRNA *nameprop = RNA_struct_name_property(itemptr->type);

	/* Simplest one! */
	switch (ui_list->layout_type) {
		case UILST_LAYOUT_GRID:
			uiItemL(layout, "", icon);
			break;
		case UILST_LAYOUT_DEFAULT:
		case UILST_LAYOUT_COMPACT:
		default:
			if (nameprop) {
				uiItemFullR(layout, itemptr, nameprop, RNA_NO_INDEX, 0, UI_ITEM_R_NO_BG, "", icon);
			}
			else {
				uiItemL(layout, "", icon);
			}
			break;
	}
}

static void uilist_draw_filter_default(struct uiList *ui_list, struct bContext *UNUSED(C), struct uiLayout *layout)
{
	PointerRNA listptr;
	uiLayout *row, *subrow;

	RNA_pointer_create(NULL, &RNA_UIList, ui_list, &listptr);

	row = uiLayoutRow(layout, false);

	subrow = uiLayoutRow(row, true);
	uiItemR(subrow, &listptr, "filter_name", 0, "", ICON_NONE);
	uiItemR(subrow, &listptr, "use_filter_invert", UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY, "",
	        (ui_list->filter_flag & UILST_FLT_EXCLUDE) ? ICON_ZOOM_OUT : ICON_ZOOM_IN);

	subrow = uiLayoutRow(row, true);
	uiItemR(subrow, &listptr, "use_filter_sort_alpha", UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
	uiItemR(subrow, &listptr, "use_filter_sort_reverse", UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY, "",
	        (ui_list->filter_sort_flag & UILST_FLT_SORT_REVERSE) ? ICON_TRIA_UP : ICON_TRIA_DOWN);
}

typedef struct {
	char name[MAX_IDPROP_NAME];
	int org_idx;
} StringCmp;

static int cmpstringp(const void *p1, const void *p2)
{
	/* Case-insensitive comparison. */
	return BLI_strcasecmp(((StringCmp *) p1)->name, ((StringCmp *) p2)->name);
}

static void uilist_filter_items_default(struct uiList *ui_list, struct bContext *UNUSED(C), struct PointerRNA *dataptr,
                                        const char *propname)
{
	uiListDyn *dyn_data = ui_list->dyn_data;
	PropertyRNA *prop = RNA_struct_find_property(dataptr, propname);

	const char *filter_raw = ui_list->filter_byname;
	char *filter = (char *)filter_raw, filter_buff[32], *filter_dyn = NULL;
	const bool filter_exclude = (ui_list->filter_flag & UILST_FLT_EXCLUDE) != 0;
	const bool order_by_name = (ui_list->filter_sort_flag & UILST_FLT_SORT_ALPHA) != 0;
	int len = RNA_property_collection_length(dataptr, prop);

	dyn_data->items_shown = dyn_data->items_len = len;

	if (len && (order_by_name || filter_raw[0])) {
		StringCmp *names = NULL;
		int order_idx = 0, i = 0;

		if (order_by_name) {
			names = MEM_callocN(sizeof(StringCmp) * len, "StringCmp");
		}
		if (filter_raw[0]) {
			size_t idx = 0, slen = strlen(filter_raw);

			dyn_data->items_filter_flags = MEM_callocN(sizeof(int) * len, "items_filter_flags");
			dyn_data->items_shown = 0;

			/* Implicitly add heading/trailing wildcards if needed. */
			if (slen + 3 <= sizeof(filter_buff)) {
				filter = filter_buff;
			}
			else {
				filter = filter_dyn = MEM_mallocN((slen + 3) * sizeof(char), "filter_dyn");
			}
			if (filter_raw[idx] != '*') {
				filter[idx++] = '*';
			}
			memcpy(filter + idx, filter_raw, slen);
			idx += slen;
			if (filter[idx - 1] != '*') {
				filter[idx++] = '*';
			}
			filter[idx] = '\0';
		}

		RNA_PROP_BEGIN (dataptr, itemptr, prop)
		{
			char *namebuf;
			const char *name;
			bool do_order = false;

			namebuf = RNA_struct_name_get_alloc(&itemptr, NULL, 0, NULL);
			name = namebuf ? namebuf : "";

			if (filter[0]) {
				/* Case-insensitive! */
				if (fnmatch(filter, name, FNM_CASEFOLD) == 0) {
					dyn_data->items_filter_flags[i] = UILST_FLT_ITEM;
					if (!filter_exclude) {
						dyn_data->items_shown++;
						do_order = order_by_name;
					}
					//printf("%s: '%s' matches '%s'\n", __func__, name, filter);
				}
				else if (filter_exclude) {
					dyn_data->items_shown++;
					do_order = order_by_name;
				}
			}
			else {
				do_order = order_by_name;
			}

			if (do_order) {
				names[order_idx].org_idx = order_idx;
				BLI_strncpy(names[order_idx++].name, name, MAX_IDPROP_NAME);
			}

			/* free name */
			if (namebuf) {
				MEM_freeN(namebuf);
			}
			i++;
		}
		RNA_PROP_END;

		if (order_by_name) {
			int new_idx;
			/* note: order_idx equals either to ui_list->items_len if no filtering done,
			 *       or to ui_list->items_shown if filter is enabled,
			 *       or to (ui_list->items_len - ui_list->items_shown) if filtered items are excluded.
			 *       This way, we only sort items we actually intend to draw!
			 */
			qsort(names, order_idx, sizeof(StringCmp), cmpstringp);

			dyn_data->items_filter_neworder = MEM_mallocN(sizeof(int) * order_idx, "items_filter_neworder");
			for (new_idx = 0; new_idx < order_idx; new_idx++) {
				dyn_data->items_filter_neworder[names[new_idx].org_idx] = new_idx;
			}
		}

		if (filter_dyn) {
			MEM_freeN(filter_dyn);
		}
		if (names) {
			MEM_freeN(names);
		}
	}
}

typedef struct {
	PointerRNA item;
	int org_idx;
	int flt_flag;
} _uilist_item;

typedef struct {
	int visual_items;  /* Visual number of items (i.e. number of items we have room to display). */
	int start_idx;     /* Index of first item to display. */
	int end_idx;       /* Index of last item to display + 1. */
} uiListLayoutdata;

static void uilist_prepare(uiList *ui_list, int len, int activei, int rows, int maxrows, int columns,
                         uiListLayoutdata *layoutdata)
{
	uiListDyn *dyn_data = ui_list->dyn_data;
	int activei_row, max_scroll;
	const bool use_auto_size = (ui_list->list_grip < (rows - UI_LIST_AUTO_SIZE_THRESHOLD));

	/* default rows */
	if (rows <= 0)
		rows = 5;
	dyn_data->visual_height_min = rows;
	if (maxrows < rows)
		maxrows = max_ii(rows, 5);
	if (columns <= 0)
		columns = 9;

	if (columns > 1) {
		dyn_data->height = (int)ceil((double)len / (double)columns);
		activei_row = (int)floor((double)activei / (double)columns);
	}
	else {
		dyn_data->height = len;
		activei_row = activei;
	}

	if (!use_auto_size) {
		/* No auto-size, yet we clamp at min size! */
		maxrows = rows = max_ii(ui_list->list_grip, rows);
	}
	else if ((rows != maxrows) && (dyn_data->height > rows)) {
		/* Expand size if needed and possible. */
		rows = min_ii(dyn_data->height, maxrows);
	}

	/* If list length changes or list is tagged to check this, and active is out of view, scroll to it .*/
	if (ui_list->list_last_len != len || ui_list->flag & UILST_SCROLL_TO_ACTIVE_ITEM) {
		if (activei_row < ui_list->list_scroll) {
			ui_list->list_scroll = activei_row;
		}
		else if (activei_row >= ui_list->list_scroll + rows) {
			ui_list->list_scroll = activei_row - rows + 1;
		}
		ui_list->flag &= ~UILST_SCROLL_TO_ACTIVE_ITEM;
	}

	max_scroll = max_ii(0, dyn_data->height - rows);
	CLAMP(ui_list->list_scroll, 0, max_scroll);
	ui_list->list_last_len = len;
	dyn_data->visual_height = rows;
	layoutdata->visual_items = rows * columns;
	layoutdata->start_idx = ui_list->list_scroll * columns;
	layoutdata->end_idx = min_ii(layoutdata->start_idx + rows * columns, len);
}

static void uilist_resize_update_cb(bContext *UNUSED(C), void *arg1, void *UNUSED(arg2))
{
	uiList *ui_list = arg1;
	uiListDyn *dyn_data = ui_list->dyn_data;

	/* This way we get diff in number of additional items to show (positive) or hide (negative). */
	const int diff = iroundf((float)(dyn_data->resize - dyn_data->resize_prev) / (float)UI_UNIT_Y);

	if (diff != 0) {
		ui_list->list_grip += diff;
		dyn_data->resize_prev += diff * UI_UNIT_Y;
		ui_list->flag |= UILST_SCROLL_TO_ACTIVE_ITEM;
	}
}

void uiTemplateList(uiLayout *layout, bContext *C, const char *listtype_name, const char *list_id,
                    PointerRNA *dataptr, const char *propname, PointerRNA *active_dataptr, const char *active_propname,
                    int rows, int maxrows, int layout_type, int columns)
{
	uiListType *ui_list_type;
	uiList *ui_list = NULL;
	uiListDyn *dyn_data;
	ARegion *ar;
	uiListDrawItemFunc draw_item;
	uiListDrawFilterFunc draw_filter;
	uiListFilterItemsFunc filter_items;

	PropertyRNA *prop = NULL, *activeprop;
	PropertyType type, activetype;
	_uilist_item *items_ptr = NULL;
	StructRNA *ptype;
	uiLayout *glob = NULL, *box, *row, *col, *subrow, *sub, *overlap;
	uiBlock *block, *subblock;
	uiBut *but;

	uiListLayoutdata layoutdata;
	char ui_list_id[UI_MAX_NAME_STR];
	char numstr[32];
	int rnaicon = ICON_NONE, icon = ICON_NONE;
	int i = 0, activei = 0;
	int len = 0;

	/* validate arguments */
	/* Forbid default UI_UL_DEFAULT_CLASS_NAME list class without a custom list_id! */
	if (!strcmp(UI_UL_DEFAULT_CLASS_NAME, listtype_name) && !(list_id && list_id[0])) {
		RNA_warning("template_list using default '%s' UIList class must provide a custom list_id",
		            UI_UL_DEFAULT_CLASS_NAME);
		return;
	}

	block = uiLayoutGetBlock(layout);

	if (!active_dataptr->data) {
		RNA_warning("No active data");
		return;
	}

	if (dataptr->data) {
		prop = RNA_struct_find_property(dataptr, propname);
		if (!prop) {
			RNA_warning("Property not found: %s.%s", RNA_struct_identifier(dataptr->type), propname);
			return;
		}
	}

	activeprop = RNA_struct_find_property(active_dataptr, active_propname);
	if (!activeprop) {
		RNA_warning("Property not found: %s.%s", RNA_struct_identifier(active_dataptr->type), active_propname);
		return;
	}

	if (prop) {
		type = RNA_property_type(prop);
		if (type != PROP_COLLECTION) {
			RNA_warning("Expected a collection data property");
			return;
		}
	}

	activetype = RNA_property_type(activeprop);
	if (activetype != PROP_INT) {
		RNA_warning("Expected an integer active data property");
		return;
	}

	/* get icon */
	if (dataptr->data && prop) {
		ptype = RNA_property_pointer_type(dataptr, prop);
		rnaicon = RNA_struct_ui_icon(ptype);
	}

	/* get active data */
	activei = RNA_property_int_get(active_dataptr, activeprop);

	/* Find the uiList type. */
	ui_list_type = WM_uilisttype_find(listtype_name, false);

	if (ui_list_type == NULL) {
		RNA_warning("List type %s not found", listtype_name);
		return;
	}

	draw_item = ui_list_type->draw_item ? ui_list_type->draw_item : uilist_draw_item_default;
	draw_filter = ui_list_type->draw_filter ? ui_list_type->draw_filter : uilist_draw_filter_default;
	filter_items = ui_list_type->filter_items ? ui_list_type->filter_items : uilist_filter_items_default;

	/* Find or add the uiList to the current Region. */
	/* We tag the list id with the list type... */
	BLI_snprintf(ui_list_id, sizeof(ui_list_id), "%s_%s", ui_list_type->idname, list_id ? list_id : "");

	ar = CTX_wm_region(C);
	ui_list = BLI_findstring(&ar->ui_lists, ui_list_id, offsetof(uiList, list_id));

	if (!ui_list) {
		ui_list = MEM_callocN(sizeof(uiList), "uiList");
		BLI_strncpy(ui_list->list_id, ui_list_id, sizeof(ui_list->list_id));
		BLI_addtail(&ar->ui_lists, ui_list);
		ui_list->list_grip = -UI_LIST_AUTO_SIZE_THRESHOLD;  /* Force auto size by default. */
	}

	if (!ui_list->dyn_data) {
		ui_list->dyn_data = MEM_callocN(sizeof(uiListDyn), "uiList.dyn_data");
	}
	dyn_data = ui_list->dyn_data;

	/* Because we can't actually pass type across save&load... */
	ui_list->type = ui_list_type;
	ui_list->layout_type = layout_type;

	/* Reset filtering data. */
	MEM_SAFE_FREE(dyn_data->items_filter_flags);
	MEM_SAFE_FREE(dyn_data->items_filter_neworder);
	dyn_data->items_len = dyn_data->items_shown = -1;

	/* When active item changed since last draw, scroll to it. */
	if (activei != ui_list->list_last_activei) {
		ui_list->flag |= UILST_SCROLL_TO_ACTIVE_ITEM;
		ui_list->list_last_activei = activei;
	}

	/* Filter list items! (not for compact layout, though) */
	if (dataptr->data && prop) {
		const int filter_exclude = ui_list->filter_flag & UILST_FLT_EXCLUDE;
		const bool order_reverse = (ui_list->filter_sort_flag & UILST_FLT_SORT_REVERSE) != 0;
		int items_shown, idx = 0;
#if 0
		int prev_ii = -1, prev_i;
#endif

		if (layout_type == UILST_LAYOUT_COMPACT) {
			dyn_data->items_len = dyn_data->items_shown = RNA_property_collection_length(dataptr, prop);
		}
		else {
			//printf("%s: filtering...\n", __func__);
			filter_items(ui_list, C, dataptr, propname);
			//printf("%s: filtering done.\n", __func__);
		}

		items_shown = dyn_data->items_shown;
		if (items_shown >= 0) {
			bool activei_mapping_pending = true;
			items_ptr = MEM_mallocN(sizeof(_uilist_item) * items_shown, __func__);
			//printf("%s: items shown: %d.\n", __func__, items_shown);
			RNA_PROP_BEGIN (dataptr, itemptr, prop)
			{
				if (!dyn_data->items_filter_flags ||
				    ((dyn_data->items_filter_flags[i] & UILST_FLT_ITEM) ^ filter_exclude))
				{
					int ii;
					if (dyn_data->items_filter_neworder) {
						ii = dyn_data->items_filter_neworder[idx++];
						ii = order_reverse ? items_shown - ii - 1 : ii;
					}
					else {
						ii = order_reverse ? items_shown - ++idx : idx++;
					}
					//printf("%s: ii: %d\n", __func__, ii);
					items_ptr[ii].item = itemptr;
					items_ptr[ii].org_idx = i;
					items_ptr[ii].flt_flag = dyn_data->items_filter_flags ? dyn_data->items_filter_flags[i] : 0;

					if (activei_mapping_pending && activei == i) {
						activei = ii;
						/* So that we do not map again activei! */
						activei_mapping_pending = false;
					}
# if 0 /* For now, do not alter active element, even if it will be hidden... */
					else if (activei < i) {
						/* We do not want an active but invisible item!
						 * Only exception is when all items are filtered out...
						 */
						if (prev_ii >= 0) {
							activei = prev_ii;
							RNA_property_int_set(active_dataptr, activeprop, prev_i);
						}
						else {
							activei = ii;
							RNA_property_int_set(active_dataptr, activeprop, i);
						}
					}
					prev_i = i;
					prev_ii = ii;
#endif
				}
				i++;
			}
			RNA_PROP_END;
		}
		if (dyn_data->items_shown >= 0) {
			len = dyn_data->items_shown;
		}
		else {
			len = dyn_data->items_len;
		}
	}

	switch (layout_type) {
		case UILST_LAYOUT_DEFAULT:
			/* layout */
			box = uiLayoutListBox(layout, ui_list, dataptr, prop, active_dataptr, activeprop);
			glob = uiLayoutColumn(box, true);
			row = uiLayoutRow(glob, false);
			col = uiLayoutColumn(row, true);

			/* init numbers */
			uilist_prepare(ui_list, len, activei, rows, maxrows, 1, &layoutdata);

			if (dataptr->data && prop) {
				/* create list items */
				for (i = layoutdata.start_idx; i < layoutdata.end_idx; i++) {
					PointerRNA *itemptr = &items_ptr[i].item;
					int org_i = items_ptr[i].org_idx;
					int flt_flag = items_ptr[i].flt_flag;
					subblock = uiLayoutGetBlock(col);

					overlap = uiLayoutOverlap(col);

					UI_block_flag_enable(subblock, UI_BLOCK_LIST_ITEM);

					/* list item behind label & other buttons */
					sub = uiLayoutRow(overlap, false);

					but = uiDefButR_prop(subblock, UI_BTYPE_LISTROW, 0, "", 0, 0, UI_UNIT_X * 10, UI_UNIT_Y,
					                     active_dataptr, activeprop, 0, 0, org_i, 0, 0, TIP_("Double click to rename"));

					sub = uiLayoutRow(overlap, false);

					icon = UI_rnaptr_icon_get(C, itemptr, rnaicon, false);
					if (icon == ICON_DOT)
						icon = ICON_NONE;
					draw_item(ui_list, C, sub, dataptr, itemptr, icon, active_dataptr, active_propname,
					          org_i, flt_flag);

					/* If we are "drawing" active item, set all labels as active. */
					if (i == activei) {
						ui_layout_list_set_labels_active(sub);
					}

					UI_block_flag_disable(subblock, UI_BLOCK_LIST_ITEM);
				}
			}

			/* add dummy buttons to fill space */
			for (; i < layoutdata.start_idx + layoutdata.visual_items; i++) {
				uiItemL(col, "", ICON_NONE);
			}

			/* add scrollbar */
			if (len > layoutdata.visual_items) {
				col = uiLayoutColumn(row, false);
				uiDefButI(block, UI_BTYPE_SCROLL, 0, "", 0, 0, UI_UNIT_X * 0.75, UI_UNIT_Y * dyn_data->visual_height,
				          &ui_list->list_scroll, 0, dyn_data->height - dyn_data->visual_height,
				          dyn_data->visual_height, 0, "");
			}
			break;
		case UILST_LAYOUT_COMPACT:
			row = uiLayoutRow(layout, true);

			if ((dataptr->data && prop) && (dyn_data->items_shown > 0) &&
			    (activei >= 0) && (activei < dyn_data->items_shown))
			{
				PointerRNA *itemptr = &items_ptr[activei].item;
				int org_i = items_ptr[activei].org_idx;

				icon = UI_rnaptr_icon_get(C, itemptr, rnaicon, false);
				if (icon == ICON_DOT)
					icon = ICON_NONE;
				draw_item(ui_list, C, row, dataptr, itemptr, icon, active_dataptr, active_propname, org_i, 0);
			}
			/* if list is empty, add in dummy button */
			else {
				uiItemL(row, "", ICON_NONE);
			}

			/* next/prev button */
			BLI_snprintf(numstr, sizeof(numstr), "%d :", dyn_data->items_shown);
			but = uiDefIconTextButR_prop(block, UI_BTYPE_NUM, 0, 0, numstr, 0, 0, UI_UNIT_X * 5, UI_UNIT_Y,
			                             active_dataptr, activeprop, 0, 0, 0, 0, 0, "");
			if (dyn_data->items_shown == 0)
				UI_but_flag_enable(but, UI_BUT_DISABLED);
			break;
		case UILST_LAYOUT_GRID:
			box = uiLayoutListBox(layout, ui_list, dataptr, prop, active_dataptr, activeprop);
			glob = uiLayoutColumn(box, true);
			row = uiLayoutRow(glob, false);
			col = uiLayoutColumn(row, true);
			subrow = NULL;  /* Quite gcc warning! */

			uilist_prepare(ui_list, len, activei, rows, maxrows, columns, &layoutdata);

			if (dataptr->data && prop) {
				/* create list items */
				for (i = layoutdata.start_idx; i < layoutdata.end_idx; i++) {
					PointerRNA *itemptr = &items_ptr[i].item;
					int org_i = items_ptr[i].org_idx;
					int flt_flag = items_ptr[i].flt_flag;

					/* create button */
					if (!(i % columns))
						subrow = uiLayoutRow(col, false);

					subblock = uiLayoutGetBlock(subrow);
					overlap = uiLayoutOverlap(subrow);

					UI_block_flag_enable(subblock, UI_BLOCK_LIST_ITEM);

					/* list item behind label & other buttons */
					sub = uiLayoutRow(overlap, false);

					but = uiDefButR_prop(subblock, UI_BTYPE_LISTROW, 0, "", 0, 0, UI_UNIT_X * 10, UI_UNIT_Y,
					                     active_dataptr, activeprop, 0, 0, org_i, 0, 0, NULL);
					UI_but_drawflag_enable(but, UI_BUT_NO_TOOLTIP);

					sub = uiLayoutRow(overlap, false);

					icon = UI_rnaptr_icon_get(C, itemptr, rnaicon, false);
					draw_item(ui_list, C, sub, dataptr, itemptr, icon, active_dataptr, active_propname,
					          org_i, flt_flag);

					/* If we are "drawing" active item, set all labels as active. */
					if (i == activei) {
						ui_layout_list_set_labels_active(sub);
					}

					UI_block_flag_disable(subblock, UI_BLOCK_LIST_ITEM);
				}
			}

			/* add dummy buttons to fill space */
			for (; i < layoutdata.start_idx + layoutdata.visual_items; i++) {
				if (!(i % columns)) {
					subrow = uiLayoutRow(col, false);
				}
				uiItemL(subrow, "", ICON_NONE);
			}

			/* add scrollbar */
			if (len > layoutdata.visual_items) {
				col = uiLayoutColumn(row, false);
				uiDefButI(block, UI_BTYPE_SCROLL, 0, "", 0, 0, UI_UNIT_X * 0.75, UI_UNIT_Y * dyn_data->visual_height,
				          &ui_list->list_scroll, 0, dyn_data->height - dyn_data->visual_height,
				          dyn_data->visual_height, 0, "");
			}
			break;
	}

	if (glob) {
		/* About UI_BTYPE_GRIP drag-resize:
		 * We can't directly use results from a grip button, since we have a rather complex behavior here
		 * (sizing by discrete steps and, overall, autosize feature).
		 * Since we *never* know whether we are grip-resizing or not (because there is no callback for when a
		 * button enters/leaves its "edit mode"), we use the fact that grip-controlled value (dyn_data->resize)
		 * is completely handled by the grip during the grab resize, so settings its value here has no effect
		 * at all.
		 * It is only meaningful when we are not resizing, in which case this gives us the correct "init drag" value.
		 * Note we cannot affect dyn_data->resize_prev here, since this value is not controlled by the grip!
		 */
		dyn_data->resize = dyn_data->resize_prev + (dyn_data->visual_height - ui_list->list_grip) * UI_UNIT_Y;

		row = uiLayoutRow(glob, true);
		subblock = uiLayoutGetBlock(row);
		UI_block_emboss_set(subblock, UI_EMBOSS_NONE);

		if (ui_list->filter_flag & UILST_FLT_SHOW) {
			but = uiDefIconButBitI(subblock, UI_BTYPE_TOGGLE, UILST_FLT_SHOW, 0, ICON_DISCLOSURE_TRI_DOWN, 0, 0,
			                       UI_UNIT_X, UI_UNIT_Y * 0.5f, &(ui_list->filter_flag), 0, 0, 0, 0,
			                       TIP_("Hide filtering options"));
			UI_but_flag_disable(but, UI_BUT_UNDO); /* skip undo on screen buttons */

			but = uiDefIconButI(subblock, UI_BTYPE_GRIP, 0, ICON_GRIP, 0, 0, UI_UNIT_X * 10.0f, UI_UNIT_Y * 0.5f,
			                    &dyn_data->resize, 0.0, 0.0, 0, 0, "");
			UI_but_func_set(but, uilist_resize_update_cb, ui_list, NULL);

			UI_block_emboss_set(subblock, UI_EMBOSS);

			col = uiLayoutColumn(glob, false);
			subblock = uiLayoutGetBlock(col);
			uiDefBut(subblock, UI_BTYPE_SEPR, 0, "", 0, 0, UI_UNIT_X, UI_UNIT_Y * 0.05f, NULL, 0.0, 0.0, 0, 0, "");

			draw_filter(ui_list, C, col);
		}
		else {
			but = uiDefIconButBitI(subblock, UI_BTYPE_TOGGLE, UILST_FLT_SHOW, 0, ICON_DISCLOSURE_TRI_RIGHT, 0, 0,
			                       UI_UNIT_X, UI_UNIT_Y * 0.5f, &(ui_list->filter_flag), 0, 0, 0, 0,
			                       TIP_("Show filtering options"));
			UI_but_flag_disable(but, UI_BUT_UNDO); /* skip undo on screen buttons */

			but = uiDefIconButI(subblock, UI_BTYPE_GRIP, 0, ICON_GRIP, 0, 0, UI_UNIT_X * 10.0f, UI_UNIT_Y * 0.5f,
			                    &dyn_data->resize, 0.0, 0.0, 0, 0, "");
			UI_but_func_set(but, uilist_resize_update_cb, ui_list, NULL);

			UI_block_emboss_set(subblock, UI_EMBOSS);
		}
	}

	if (items_ptr) {
		MEM_freeN(items_ptr);
	}
}

/************************* Operator Search Template **************************/

static void operator_call_cb(bContext *C, void *UNUSED(arg1), void *arg2)
{
	wmOperatorType *ot = arg2;
	
	if (ot)
		WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, NULL);
}

static void operator_search_cb(const bContext *C, void *UNUSED(arg), const char *str, uiSearchItems *items)
{
	GHashIterator iter;

	for (WM_operatortype_iter(&iter); !BLI_ghashIterator_done(&iter); BLI_ghashIterator_step(&iter)) {
		wmOperatorType *ot = BLI_ghashIterator_getValue(&iter);

		if ((ot->flag & OPTYPE_INTERNAL) && (G.debug & G_DEBUG_WM) == 0)
			continue;

		if (BLI_strcasestr(ot->name, str)) {
			if (WM_operator_poll((bContext *)C, ot)) {
				char name[256];
				int len = strlen(ot->name);
				
				/* display name for menu, can hold hotkey */
				BLI_strncpy(name, ot->name, sizeof(name));
				
				/* check for hotkey */
				if (len < sizeof(name) - 6) {
					if (WM_key_event_operator_string(C, ot->idname, WM_OP_EXEC_DEFAULT, NULL, true,
					                                 &name[len + 1], sizeof(name) - len - 1))
					{
						name[len] = UI_SEP_CHAR;
					}
				}
				
				if (false == UI_search_item_add(items, name, ot, 0))
					break;
			}
		}
	}
}

void UI_but_func_operator_search(uiBut *but)
{
	UI_but_func_search_set(but, operator_search_cb, NULL, operator_call_cb, NULL);
}

void uiTemplateOperatorSearch(uiLayout *layout)
{
	uiBlock *block;
	uiBut *but;
	static char search[256] = "";
		
	block = uiLayoutGetBlock(layout);
	UI_block_layout_set_current(block, layout);

	but = uiDefSearchBut(block, search, 0, ICON_VIEWZOOM, sizeof(search), 0, 0, UI_UNIT_X * 6, UI_UNIT_Y, 0, 0, "");
	UI_but_func_operator_search(but);
}

/************************* Running Jobs Template **************************/

#define B_STOPRENDER    1
#define B_STOPCAST      2
#define B_STOPANIM      3
#define B_STOPCOMPO     4
#define B_STOPSEQ       5
#define B_STOPCLIP      6
#define B_STOPOTHER     7

static void do_running_jobs(bContext *C, void *UNUSED(arg), int event)
{
	switch (event) {
		case B_STOPRENDER:
			G.is_break = true;
			break;
		case B_STOPCAST:
			WM_jobs_stop(CTX_wm_manager(C), CTX_wm_screen(C), NULL);
			break;
		case B_STOPANIM:
			WM_operator_name_call(C, "SCREEN_OT_animation_play", WM_OP_INVOKE_SCREEN, NULL);
			break;
		case B_STOPCOMPO:
			WM_jobs_stop(CTX_wm_manager(C), CTX_data_scene(C), NULL);
			break;
		case B_STOPSEQ:
			WM_jobs_stop(CTX_wm_manager(C), CTX_wm_area(C), NULL);
			break;
		case B_STOPCLIP:
			WM_jobs_stop(CTX_wm_manager(C), CTX_wm_area(C), NULL);
			break;
		case B_STOPOTHER:
			G.is_break = true;
			break;
	}
}

void uiTemplateRunningJobs(uiLayout *layout, bContext *C)
{
	bScreen *screen = CTX_wm_screen(C);
	wmWindowManager *wm = CTX_wm_manager(C);
	ScrArea *sa = CTX_wm_area(C);
	uiBlock *block;
	void *owner = NULL;
	int handle_event;
	
	block = uiLayoutGetBlock(layout);
	UI_block_layout_set_current(block, layout);

	UI_block_func_handle_set(block, do_running_jobs, NULL);

	if (sa->spacetype == SPACE_SEQ) {
		if (WM_jobs_test(wm, sa, WM_JOB_TYPE_ANY))
			owner = sa;
		handle_event = B_STOPSEQ;
	}
	else if (sa->spacetype == SPACE_CLIP) {
		if (WM_jobs_test(wm, sa, WM_JOB_TYPE_ANY))
			owner = sa;
		handle_event = B_STOPCLIP;
	}
	else {
		Scene *scene;
		/* another scene can be rendering too, for example via compositor */
		for (scene = CTX_data_main(C)->scene.first; scene; scene = scene->id.next) {
			if (WM_jobs_test(wm, scene, WM_JOB_TYPE_RENDER)) {
				handle_event = B_STOPRENDER;
				break;
			}
			else if (WM_jobs_test(wm, scene, WM_JOB_TYPE_COMPOSITE)) {
				handle_event = B_STOPCOMPO;
				break;
			}
			else if (WM_jobs_test(wm, scene, WM_JOB_TYPE_OBJECT_BAKE_TEXTURE) ||
			         WM_jobs_test(wm, scene, WM_JOB_TYPE_OBJECT_BAKE))
			{
				/* Skip bake jobs in compositor to avoid compo header displaying
				 * progress bar which is not being updated (bake jobs only need
				 * to update NC_IMAGE context.
				 */
				if (sa->spacetype != SPACE_NODE) {
					handle_event = B_STOPOTHER;
					break;
				}
			}
			else if (WM_jobs_test(wm, scene, WM_JOB_TYPE_ANY)) {
				handle_event = B_STOPOTHER;
				break;
			}
		}
		owner = scene;
	}

	if (owner) {
		uiLayout *ui_abs;
		
		ui_abs = uiLayoutAbsolute(layout, false);
		(void)ui_abs;  /* UNUSED */
		
		uiDefIconBut(block, UI_BTYPE_BUT, handle_event, ICON_PANEL_CLOSE, 0, UI_UNIT_Y * 0.1, UI_UNIT_X * 0.8, UI_UNIT_Y * 0.8,
		             NULL, 0.0f, 0.0f, 0, 0, TIP_("Stop this job"));
		uiDefBut(block, UI_BTYPE_PROGRESS_BAR, 0, WM_jobs_name(wm, owner), 
		         UI_UNIT_X, 0, UI_UNIT_X * 5.0f, UI_UNIT_Y, NULL, 0.0f, 0.0f, WM_jobs_progress(wm, owner), 0, TIP_("Progress"));
		
		uiLayoutRow(layout, false);
	}
	if (WM_jobs_test(wm, screen, WM_JOB_TYPE_SCREENCAST))
		uiDefIconTextBut(block, UI_BTYPE_BUT, B_STOPCAST, ICON_CANCEL, IFACE_("Capture"), 0, 0, UI_UNIT_X * 4.25f, UI_UNIT_Y,
		                 NULL, 0.0f, 0.0f, 0, 0, TIP_("Stop screencast"));
	if (screen->animtimer)
		uiDefIconTextBut(block, UI_BTYPE_BUT, B_STOPANIM, ICON_CANCEL, IFACE_("Anim Player"), 0, 0, UI_UNIT_X * 5.0f, UI_UNIT_Y,
		                 NULL, 0.0f, 0.0f, 0, 0, TIP_("Stop animation playback"));
}

/************************* Reports for Last Operator Template **************************/

void uiTemplateReportsBanner(uiLayout *layout, bContext *C)
{
	ReportList *reports = CTX_wm_reports(C);
	Report *report = BKE_reports_last_displayable(reports);
	ReportTimerInfo *rti;
	
	uiLayout *ui_abs;
	uiBlock *block;
	uiBut *but;
	uiStyle *style = UI_style_get();
	int width;
	int icon;
	
	/* if the report display has timed out, don't show */
	if (!reports->reporttimer) return;
	
	rti = (ReportTimerInfo *)reports->reporttimer->customdata;
	
	if (!rti || rti->widthfac == 0.0f || !report) return;
	
	ui_abs = uiLayoutAbsolute(layout, false);
	block = uiLayoutGetBlock(ui_abs);
	
	width = BLF_width(style->widget.uifont_id, report->message, report->len);
	width = min_ii((int)(rti->widthfac * width), width);
	width = max_ii(width, 10);
	
	/* make a box around the report to make it stand out */
	UI_block_align_begin(block);
	but = uiDefBut(block, UI_BTYPE_ROUNDBOX, 0, "", 0, 0, UI_UNIT_X + 10, UI_UNIT_Y, NULL, 0.0f, 0.0f, 0, 0, "");
	/* set the report's bg color in but->col - UI_BTYPE_ROUNDBOX feature */
	rgb_float_to_uchar(but->col, rti->col);
	but->col[3] = 255;

	but = uiDefBut(block, UI_BTYPE_ROUNDBOX, 0, "", UI_UNIT_X + 10, 0, UI_UNIT_X + width, UI_UNIT_Y,
	               NULL, 0.0f, 0.0f, 0, 0, "");
	but->col[0] = but->col[1] = but->col[2] = FTOCHAR(rti->grayscale);
	but->col[3] = 255;

	UI_block_align_end(block);
	
	
	/* icon and report message on top */
	icon = UI_icon_from_report_type(report->type);
	
	/* XXX: temporary operator to dump all reports to a text block, but only if more than 1 report 
	 * to be shown instead of icon when appropriate...
	 */
	UI_block_emboss_set(block, UI_EMBOSS_NONE);

	if (reports->list.first != reports->list.last)
		uiDefIconButO(block, UI_BTYPE_BUT, "UI_OT_reports_to_textblock", WM_OP_INVOKE_REGION_WIN, icon, 2, 0, UI_UNIT_X,
		              UI_UNIT_Y, TIP_("Click to see the remaining reports in text block: 'Recent Reports'"));
	else
		uiDefIconBut(block, UI_BTYPE_LABEL, 0, icon, 2, 0, UI_UNIT_X, UI_UNIT_Y, NULL, 0.0f, 0.0f, 0, 0, "");

	UI_block_emboss_set(block, UI_EMBOSS);
	
	uiDefBut(block, UI_BTYPE_LABEL, 0, report->message, UI_UNIT_X + 10, 0, UI_UNIT_X + width, UI_UNIT_Y,
	         NULL, 0.0f, 0.0f, 0, 0, "");
}

/********************************* Keymap *************************************/

static void keymap_item_modified(bContext *UNUSED(C), void *kmi_p, void *UNUSED(unused))
{
	wmKeyMapItem *kmi = (wmKeyMapItem *)kmi_p;
	WM_keyconfig_update_tag(NULL, kmi);
}

static void template_keymap_item_properties(uiLayout *layout, const char *title, PointerRNA *ptr)
{
	uiLayout *flow, *box, *row;

	uiItemS(layout);

	if (title)
		uiItemL(layout, title, ICON_NONE);
	
	flow = uiLayoutColumnFlow(layout, 2, false);

	RNA_STRUCT_BEGIN (ptr, prop)
	{
		const bool is_set = RNA_property_is_set(ptr, prop);
		uiBut *but;

		/* recurse for nested properties */
		if (RNA_property_type(prop) == PROP_POINTER) {
			PointerRNA propptr = RNA_property_pointer_get(ptr, prop);

			if (propptr.data && RNA_struct_is_a(propptr.type, &RNA_OperatorProperties)) {
				const char *name = RNA_property_ui_name(prop);
				template_keymap_item_properties(layout, name, &propptr);
				continue;
			}
		}

		box = uiLayoutBox(flow);
		uiLayoutSetActive(box, is_set);
		row = uiLayoutRow(box, false);

		/* property value */
		uiItemFullR(row, ptr, prop, -1, 0, 0, NULL, ICON_NONE);

		if (is_set) {
			/* unset operator */
			uiBlock *block = uiLayoutGetBlock(row);
			UI_block_emboss_set(block, UI_EMBOSS_NONE);
			but = uiDefIconButO(block, UI_BTYPE_BUT, "UI_OT_unset_property_button", WM_OP_EXEC_DEFAULT, ICON_X, 0, 0, UI_UNIT_X, UI_UNIT_Y, NULL);
			but->rnapoin = *ptr;
			but->rnaprop = prop;
			UI_block_emboss_set(block, UI_EMBOSS);
		}
	}
	RNA_STRUCT_END;
}

void uiTemplateKeymapItemProperties(uiLayout *layout, PointerRNA *ptr)
{
	PointerRNA propptr = RNA_pointer_get(ptr, "properties");

	if (propptr.data) {
		uiBut *but = uiLayoutGetBlock(layout)->buttons.last;

		template_keymap_item_properties(layout, NULL, &propptr);

		/* attach callbacks to compensate for missing properties update,
		 * we don't know which keymap (item) is being modified there */
		for (; but; but = but->next) {
			/* operator buttons may store props for use (file selector, [#36492]) */
			if (but->rnaprop) {
				UI_but_func_set(but, keymap_item_modified, ptr->data, NULL);
			}
		}
	}
}

/********************************* Color management *************************************/

void uiTemplateColorspaceSettings(uiLayout *layout, PointerRNA *ptr, const char *propname)
{
	PropertyRNA *prop;
	PointerRNA colorspace_settings_ptr;

	prop = RNA_struct_find_property(ptr, propname);

	if (!prop) {
		printf("%s: property not found: %s.%s\n",
		       __func__, RNA_struct_identifier(ptr->type), propname);
		return;
	}

	colorspace_settings_ptr = RNA_property_pointer_get(ptr, prop);

	uiItemL(layout, IFACE_("Input Color Space:"), ICON_NONE);
	uiItemR(layout, &colorspace_settings_ptr, "name", 0, "", ICON_NONE);
}

void uiTemplateColormanagedViewSettings(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr, const char *propname)
{
	PropertyRNA *prop;
	PointerRNA view_transform_ptr;
	uiLayout *col, *row;
	ColorManagedViewSettings *view_settings;

	prop = RNA_struct_find_property(ptr, propname);

	if (!prop) {
		printf("%s: property not found: %s.%s\n",
		       __func__, RNA_struct_identifier(ptr->type), propname);
		return;
	}

	view_transform_ptr = RNA_property_pointer_get(ptr, prop);
	view_settings = view_transform_ptr.data;

	col = uiLayoutColumn(layout, false);

	row = uiLayoutRow(col, false);
	uiItemR(row, &view_transform_ptr, "view_transform", 0, IFACE_("View"), ICON_NONE);

	col = uiLayoutColumn(layout, false);
	uiItemR(col, &view_transform_ptr, "exposure", 0, NULL, ICON_NONE);
	uiItemR(col, &view_transform_ptr, "gamma", 0, NULL, ICON_NONE);

	uiItemR(col, &view_transform_ptr, "look", 0, IFACE_("Look"), ICON_NONE);

	col = uiLayoutColumn(layout, false);
	uiItemR(col, &view_transform_ptr, "use_curve_mapping", 0, NULL, ICON_NONE);
	if (view_settings->flag & COLORMANAGE_VIEW_USE_CURVES)
		uiTemplateCurveMapping(col, &view_transform_ptr, "curve_mapping", 'c', true, false, false);
}

/********************************* Component Menu *************************************/

typedef struct ComponentMenuArgs {
	PointerRNA ptr;
	char propname[64];	/* XXX arbitrary */
} ComponentMenuArgs;
/* NOTE: this is a block-menu, needs 0 events, otherwise the menu closes */
static uiBlock *component_menu(bContext *C, ARegion *ar, void *args_v)
{
	ComponentMenuArgs *args = (ComponentMenuArgs *)args_v;
	uiBlock *block;
	uiLayout *layout;
	
	block = UI_block_begin(C, ar, __func__, UI_EMBOSS);
	UI_block_flag_enable(block, UI_BLOCK_KEEP_OPEN);
	
	layout = uiLayoutColumn(UI_block_layout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, UI_UNIT_X * 6, UI_UNIT_Y, 0, UI_style_get()), 0);
	
	uiItemR(layout, &args->ptr, args->propname, UI_ITEM_R_EXPAND, "", ICON_NONE);
	
	UI_block_bounds_set_normal(block, 6);
	UI_block_direction_set(block, UI_DIR_DOWN);
	
	return block;
}
void uiTemplateComponentMenu(uiLayout *layout, PointerRNA *ptr, const char *propname, const char *name)
{
	ComponentMenuArgs *args = MEM_callocN(sizeof(ComponentMenuArgs), "component menu template args");
	uiBlock *block;
	uiBut *but;
	
	args->ptr = *ptr;
	BLI_strncpy(args->propname, propname, sizeof(args->propname));
	
	block = uiLayoutGetBlock(layout);
	UI_block_align_begin(block);

	but = uiDefBlockButN(block, component_menu, args, name, 0, 0, UI_UNIT_X * 6, UI_UNIT_Y, "");
	/* set rna directly, uiDefBlockButN doesn't do this */
	but->rnapoin = *ptr;
	but->rnaprop = RNA_struct_find_property(ptr, propname);
	but->rnaindex = 0;
	
	UI_block_align_end(block);
}

/************************* Node Socket Icon **************************/

void uiTemplateNodeSocket(uiLayout *layout, bContext *UNUSED(C), float *color)
{
	uiBlock *block;
	uiBut *but;
	
	block = uiLayoutGetBlock(layout);
	UI_block_align_begin(block);
	
	/* XXX using explicit socket colors is not quite ideal.
	 * Eventually it should be possible to use theme colors for this purpose,
	 * but this requires a better design for extendable color palettes in user prefs.
	 */
	but = uiDefBut(block, UI_BTYPE_NODE_SOCKET, 0, "", 0, 0, UI_UNIT_X, UI_UNIT_Y, NULL, 0, 0, 0, 0, "");
	rgba_float_to_uchar(but->col, color);
	
	UI_block_align_end(block);
}
