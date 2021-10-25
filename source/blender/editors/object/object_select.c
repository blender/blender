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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2002-2008 full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_select.c
 *  \ingroup edobj
 */


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DNA_anim_types.h"
#include "DNA_group_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"
#include "DNA_property_types.h"
#include "DNA_scene_types.h"
#include "DNA_armature_types.h"
#include "DNA_lamp_types.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_rand.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_group.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_particle.h"
#include "BKE_property.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_library.h"
#include "BKE_deform.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_keyframing.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "object_intern.h"

/************************ Exported **************************/

/* simple API for object selection, rather than just using the flag
 * this takes into account the 'restrict selection in 3d view' flag.
 * deselect works always, the restriction just prevents selection */

/* Note: send a NC_SCENE|ND_OB_SELECT notifier yourself! (or 
 * or a NC_SCENE|ND_OB_VISIBLE in case of visibility toggling */

void ED_base_object_select(Base *base, short mode)
{
	if (base) {
		if (mode == BA_SELECT) {
			if (!(base->object->restrictflag & OB_RESTRICT_SELECT))
				base->flag |= SELECT;
		}
		else if (mode == BA_DESELECT) {
			base->flag &= ~SELECT;
		}
		base->object->flag = base->flag;
	}
}

/* also to set active NULL */
void ED_base_object_activate(bContext *C, Base *base)
{
	Scene *scene = CTX_data_scene(C);
	
	/* sets scene->basact */
	BASACT = base;
	
	if (base) {
		
		/* XXX old signals, remember to handle notifiers now! */
		//		select_actionchannel_by_name(base->object->action, "Object", 1);
		
		WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
	}
	else
		WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, NULL);
}

/********************** Selection Operators **********************/

static int objects_selectable_poll(bContext *C)
{
	/* we don't check for linked scenes here, selection is
	 * still allowed then for inspection of scene */
	Object *obact = CTX_data_active_object(C);

	if (CTX_data_edit_object(C))
		return 0;
	if (obact && obact->mode)
		return 0;
	
	return 1;
}

/************************ Select by Type *************************/

static int object_select_by_type_exec(bContext *C, wmOperator *op)
{
	short obtype, extend;
	
	obtype = RNA_enum_get(op->ptr, "type");
	extend = RNA_boolean_get(op->ptr, "extend");
		
	if (extend == 0) {
		CTX_DATA_BEGIN (C, Base *, base, visible_bases)
		{
			ED_base_object_select(base, BA_DESELECT);
		}
		CTX_DATA_END;
	}
	
	CTX_DATA_BEGIN (C, Base *, base, visible_bases)
	{
		if (base->object->type == obtype) {
			ED_base_object_select(base, BA_SELECT);
		}
	}
	CTX_DATA_END;
	
	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_select_by_type(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select By Type";
	ot->description = "Select all visible objects that are of a type";
	ot->idname = "OBJECT_OT_select_by_type";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = object_select_by_type_exec;
	ot->poll = objects_selectable_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend selection instead of deselecting everything first");
	ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_object_type_items, 1, "Type", "");
}

/*********************** Selection by Links *********************/

enum {
	OBJECT_SELECT_LINKED_IPO = 1,
	OBJECT_SELECT_LINKED_OBDATA,
	OBJECT_SELECT_LINKED_MATERIAL,
	OBJECT_SELECT_LINKED_TEXTURE,
	OBJECT_SELECT_LINKED_DUPGROUP,
	OBJECT_SELECT_LINKED_PARTICLE,
	OBJECT_SELECT_LINKED_LIBRARY,
	OBJECT_SELECT_LINKED_LIBRARY_OBDATA
};

static EnumPropertyItem prop_select_linked_types[] = {
	//{OBJECT_SELECT_LINKED_IPO, "IPO", 0, "Object IPO", ""}, // XXX deprecated animation system stuff...
	{OBJECT_SELECT_LINKED_OBDATA, "OBDATA", 0, "Object Data", ""},
	{OBJECT_SELECT_LINKED_MATERIAL, "MATERIAL", 0, "Material", ""},
	{OBJECT_SELECT_LINKED_TEXTURE, "TEXTURE", 0, "Texture", ""},
	{OBJECT_SELECT_LINKED_DUPGROUP, "DUPGROUP", 0, "Dupligroup", ""},
	{OBJECT_SELECT_LINKED_PARTICLE, "PARTICLE", 0, "Particle System", ""},
	{OBJECT_SELECT_LINKED_LIBRARY, "LIBRARY", 0, "Library", ""},
	{OBJECT_SELECT_LINKED_LIBRARY_OBDATA, "LIBRARY_OBDATA", 0, "Library (Object Data)", ""},
	{0, NULL, 0, NULL, NULL}
};

// XXX old animation system
#if 0
static int object_select_all_by_ipo(bContext *C, Ipo *ipo)
{
	bool changed = false;

	CTX_DATA_BEGIN (C, Base *, base, visible_bases)
	{
		if (base->object->ipo == ipo) {
			base->flag |= SELECT;
			base->object->flag = base->flag;

			changed = true;
		}
	}
	CTX_DATA_END;

	return changed;
}
#endif

static bool object_select_all_by_obdata(bContext *C, void *obdata)
{
	bool changed = false;

	CTX_DATA_BEGIN (C, Base *, base, visible_bases)
	{
		if ((base->flag & SELECT) == 0) {
			if (base->object->data == obdata) {
				base->flag |= SELECT;
				base->object->flag = base->flag;

				changed = true;
			}
		}
	}
	CTX_DATA_END;

	return changed;
}

static bool object_select_all_by_material_texture(bContext *C, int use_texture, Material *mat, Tex *tex)
{
	bool changed = false;

	CTX_DATA_BEGIN (C, Base *, base, visible_bases)
	{
		if ((base->flag & SELECT) == 0) {
			Object *ob = base->object;
			Material *mat1;
			int a, b;

			for (a = 1; a <= ob->totcol; a++) {
				mat1 = give_current_material(ob, a);

				if (!use_texture) {
					if (mat1 == mat) {
						base->flag |= SELECT;
						changed = true;
					}
				}
				else if (mat1 && use_texture) {
					for (b = 0; b < MAX_MTEX; b++) {
						if (mat1->mtex[b]) {
							if (tex == mat1->mtex[b]->tex) {
								base->flag |= SELECT;
								changed = true;
								break;
							}
						}
					}
				}
			}

			base->object->flag = base->flag;
		}
	}
	CTX_DATA_END;

	return changed;
}

static bool object_select_all_by_dup_group(bContext *C, Object *ob)
{
	bool changed = false;
	Group *dup_group = (ob->transflag & OB_DUPLIGROUP) ? ob->dup_group : NULL;

	CTX_DATA_BEGIN (C, Base *, base, visible_bases)
	{
		if ((base->flag & SELECT) == 0) {
			Group *dup_group_other = (base->object->transflag & OB_DUPLIGROUP) ? base->object->dup_group : NULL;
			if (dup_group == dup_group_other) {
				base->flag |= SELECT;
				base->object->flag = base->flag;

				changed = true;
			}
		}
	}
	CTX_DATA_END;

	return changed;
}

static bool object_select_all_by_particle(bContext *C, Object *ob)
{
	ParticleSystem *psys_act = psys_get_current(ob);
	bool changed = false;

	CTX_DATA_BEGIN (C, Base *, base, visible_bases)
	{
		if ((base->flag & SELECT) == 0) {
			/* loop through other particles*/
			ParticleSystem *psys;
			
			for (psys = base->object->particlesystem.first; psys; psys = psys->next) {
				if (psys->part == psys_act->part) {
					base->flag |= SELECT;
					changed = true;
					break;
				}

				if (base->flag & SELECT) {
					break;
				}
			}

			base->object->flag = base->flag;
		}
	}
	CTX_DATA_END;

	return changed;
}

static bool object_select_all_by_library(bContext *C, Library *lib)
{
	bool changed = false;

	CTX_DATA_BEGIN (C, Base *, base, visible_bases)
	{
		if ((base->flag & SELECT) == 0) {
			if (lib == base->object->id.lib) {
				base->flag |= SELECT;
				base->object->flag = base->flag;

				changed = true;
			}
		}
	}
	CTX_DATA_END;

	return changed;
}

static bool object_select_all_by_library_obdata(bContext *C, Library *lib)
{
	bool changed = false;

	CTX_DATA_BEGIN (C, Base *, base, visible_bases)
	{
		if ((base->flag & SELECT) == 0) {
			if (base->object->data && lib == ((ID *)base->object->data)->lib) {
				base->flag |= SELECT;
				base->object->flag = base->flag;

				changed = true;
			}
		}
	}
	CTX_DATA_END;

	return changed;
}

void ED_object_select_linked_by_id(bContext *C, ID *id)
{
	int idtype = GS(id->name);
	bool changed = false;

	if (OB_DATA_SUPPORT_ID(idtype)) {
		changed = object_select_all_by_obdata(C, id);
	}
	else if (idtype == ID_MA) {
		changed = object_select_all_by_material_texture(C, false, (Material *)id, NULL);
	}
	else if (idtype == ID_LI) {
		changed = object_select_all_by_library(C, (Library *) id);
	}

	if (changed) {
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, CTX_data_scene(C));
	}
}

static int object_select_linked_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob;
	int nr = RNA_enum_get(op->ptr, "type");
	bool changed = false, extend;

	extend = RNA_boolean_get(op->ptr, "extend");
	
	if (extend == 0) {
		CTX_DATA_BEGIN (C, Base *, base, visible_bases)
		{
			ED_base_object_select(base, BA_DESELECT);
		}
		CTX_DATA_END;
	}
	
	ob = OBACT;
	if (ob == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No active object");
		return OPERATOR_CANCELLED;
	}
	
	if (nr == OBJECT_SELECT_LINKED_IPO) {
		// XXX old animation system
		//if (ob->ipo == 0) return OPERATOR_CANCELLED;
		//object_select_all_by_ipo(C, ob->ipo)
		return OPERATOR_CANCELLED;
	}
	else if (nr == OBJECT_SELECT_LINKED_OBDATA) {
		if (ob->data == NULL)
			return OPERATOR_CANCELLED;

		changed = object_select_all_by_obdata(C, ob->data);
	}
	else if (nr == OBJECT_SELECT_LINKED_MATERIAL || nr == OBJECT_SELECT_LINKED_TEXTURE) {
		Material *mat = NULL;
		Tex *tex = NULL;
		bool use_texture = false;

		mat = give_current_material(ob, ob->actcol);
		if (mat == NULL) return OPERATOR_CANCELLED;
		if (nr == OBJECT_SELECT_LINKED_TEXTURE) {
			use_texture = true;

			if (mat->mtex[(int)mat->texact]) tex = mat->mtex[(int)mat->texact]->tex;
			if (tex == NULL) return OPERATOR_CANCELLED;
		}

		changed = object_select_all_by_material_texture(C, use_texture, mat, tex);
	}
	else if (nr == OBJECT_SELECT_LINKED_DUPGROUP) {
		if (ob->dup_group == NULL)
			return OPERATOR_CANCELLED;

		changed = object_select_all_by_dup_group(C, ob);
	}
	else if (nr == OBJECT_SELECT_LINKED_PARTICLE) {
		if (BLI_listbase_is_empty(&ob->particlesystem))
			return OPERATOR_CANCELLED;

		changed = object_select_all_by_particle(C, ob);
	}
	else if (nr == OBJECT_SELECT_LINKED_LIBRARY) {
		/* do nothing */
		changed = object_select_all_by_library(C, ob->id.lib);
	}
	else if (nr == OBJECT_SELECT_LINKED_LIBRARY_OBDATA) {
		if (ob->data == NULL)
			return OPERATOR_CANCELLED;

		changed = object_select_all_by_library_obdata(C, ((ID *) ob->data)->lib);
	}
	else
		return OPERATOR_CANCELLED;

	if (changed) {
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
		return OPERATOR_FINISHED;
	}
	
	return OPERATOR_CANCELLED;
}

void OBJECT_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked";
	ot->description = "Select all visible objects that are linked";
	ot->idname = "OBJECT_OT_select_linked";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = object_select_linked_exec;
	ot->poll = objects_selectable_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend selection instead of deselecting everything first");
	ot->prop = RNA_def_enum(ot->srna, "type", prop_select_linked_types, 0, "Type", "");
}

/*********************** Selected Grouped ********************/

enum {
	OBJECT_GRPSEL_CHILDREN_RECURSIVE =  0,
	OBJECT_GRPSEL_CHILDREN           =  1,
	OBJECT_GRPSEL_PARENT             =  2,
	OBJECT_GRPSEL_SIBLINGS           =  3,
	OBJECT_GRPSEL_TYPE               =  4,
	OBJECT_GRPSEL_LAYER              =  5,
	OBJECT_GRPSEL_GROUP              =  6,
	OBJECT_GRPSEL_HOOK               =  7,
	OBJECT_GRPSEL_PASS               =  8,
	OBJECT_GRPSEL_COLOR              =  9,
	OBJECT_GRPSEL_PROPERTIES         = 10,
	OBJECT_GRPSEL_KEYINGSET          = 11,
	OBJECT_GRPSEL_LAMP_TYPE          = 12,
};

static EnumPropertyItem prop_select_grouped_types[] = {
	{OBJECT_GRPSEL_CHILDREN_RECURSIVE, "CHILDREN_RECURSIVE", 0, "Children", ""},
	{OBJECT_GRPSEL_CHILDREN, "CHILDREN", 0, "Immediate Children", ""},
	{OBJECT_GRPSEL_PARENT, "PARENT", 0, "Parent", ""},
	{OBJECT_GRPSEL_SIBLINGS, "SIBLINGS", 0, "Siblings", "Shared Parent"},
	{OBJECT_GRPSEL_TYPE, "TYPE", 0, "Type", "Shared object type"},
	{OBJECT_GRPSEL_LAYER, "LAYER", 0, "Layer", "Shared layers"},
	{OBJECT_GRPSEL_GROUP, "GROUP", 0, "Group", "Shared group"},
	{OBJECT_GRPSEL_HOOK, "HOOK", 0, "Hook", ""},
	{OBJECT_GRPSEL_PASS, "PASS", 0, "Pass", "Render pass Index"},
	{OBJECT_GRPSEL_COLOR, "COLOR", 0, "Color", "Object Color"},
	{OBJECT_GRPSEL_PROPERTIES, "PROPERTIES", 0, "Properties", "Game Properties"},
	{OBJECT_GRPSEL_KEYINGSET, "KEYINGSET", 0, "Keying Set", "Objects included in active Keying Set"},
	{OBJECT_GRPSEL_LAMP_TYPE, "LAMP_TYPE", 0, "Lamp Type", "Matching lamp types"},
	{0, NULL, 0, NULL, NULL}
};

static bool select_grouped_children(bContext *C, Object *ob, const bool recursive)
{
	bool changed = false;

	CTX_DATA_BEGIN (C, Base *, base, selectable_bases)
	{
		if (ob == base->object->parent) {
			if (!(base->flag & SELECT)) {
				ED_base_object_select(base, BA_SELECT);
				changed = true;
			}

			if (recursive)
				changed |= select_grouped_children(C, base->object, 1);
		}
	}
	CTX_DATA_END;
	return changed;
}

static bool select_grouped_parent(bContext *C) /* Makes parent active and de-selected OBACT */
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);

	bool changed = false;
	Base *baspar, *basact = CTX_data_active_base(C);

	if (!basact || !(basact->object->parent)) return 0;  /* we know OBACT is valid */

	baspar = BKE_scene_base_find(scene, basact->object->parent);

	/* can be NULL if parent in other scene */
	if (baspar && BASE_SELECTABLE(v3d, baspar)) {
		ED_base_object_select(baspar, BA_SELECT);
		ED_base_object_activate(C, baspar);
		changed = true;
	}
	return changed;
}


#define GROUP_MENU_MAX  24
static bool select_grouped_group(bContext *C, Object *ob)  /* Select objects in the same group as the active */
{
	bool changed = false;
	Group *group, *ob_groups[GROUP_MENU_MAX];
	int group_count = 0, i;
	uiPopupMenu *pup;
	uiLayout *layout;

	for (group = CTX_data_main(C)->group.first; group && group_count < GROUP_MENU_MAX; group = group->id.next) {
		if (BKE_group_object_exists(group, ob)) {
			ob_groups[group_count] = group;
			group_count++;
		}
	}

	if (!group_count)
		return 0;
	else if (group_count == 1) {
		group = ob_groups[0];
		CTX_DATA_BEGIN (C, Base *, base, visible_bases)
		{
			if (!(base->flag & SELECT) && BKE_group_object_exists(group, base->object)) {
				ED_base_object_select(base, BA_SELECT);
				changed = true;
			}
		}
		CTX_DATA_END;
		return changed;
	}

	/* build the menu. */
	pup = UI_popup_menu_begin(C, IFACE_("Select Group"), ICON_NONE);
	layout = UI_popup_menu_layout(pup);

	for (i = 0; i < group_count; i++) {
		group = ob_groups[i];
		uiItemStringO(layout, group->id.name + 2, 0, "OBJECT_OT_select_same_group", "group", group->id.name + 2);
	}

	UI_popup_menu_end(C, pup);
	return changed;  /* The operator already handle this! */
}

static bool select_grouped_object_hooks(bContext *C, Object *ob)
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);

	bool changed = false;
	Base *base;
	ModifierData *md;
	HookModifierData *hmd;

	for (md = ob->modifiers.first; md; md = md->next) {
		if (md->type == eModifierType_Hook) {
			hmd = (HookModifierData *) md;
			if (hmd->object && !(hmd->object->flag & SELECT)) {
				base = BKE_scene_base_find(scene, hmd->object);
				if (base && (BASE_SELECTABLE(v3d, base))) {
					ED_base_object_select(base, BA_SELECT);
					changed = true;
				}
			}
		}
	}
	return changed;
}

/* Select objects with the same parent as the active (siblings),
 * parent can be NULL also */
static bool select_grouped_siblings(bContext *C, Object *ob)
{
	bool changed = false;

	CTX_DATA_BEGIN (C, Base *, base, selectable_bases)
	{
		if ((base->object->parent == ob->parent) && !(base->flag & SELECT)) {
			ED_base_object_select(base, BA_SELECT);
			changed = true;
		}
	}
	CTX_DATA_END;
	return changed;
}
static bool select_grouped_lamptype(bContext *C, Object *ob)
{
	Lamp *la = ob->data;

	bool changed = false;

	CTX_DATA_BEGIN (C, Base *, base, selectable_bases)
	{
		if (base->object->type == OB_LAMP) {
			Lamp *la_test = base->object->data;
			if ((la->type == la_test->type) && !(base->flag & SELECT)) {
				ED_base_object_select(base, BA_SELECT);
				changed = true;
			}
		}
	}
	CTX_DATA_END;
	return changed;
}
static bool select_grouped_type(bContext *C, Object *ob)
{
	bool changed = false;

	CTX_DATA_BEGIN (C, Base *, base, selectable_bases)
	{
		if ((base->object->type == ob->type) && !(base->flag & SELECT)) {
			ED_base_object_select(base, BA_SELECT);
			changed = true;
		}
	}
	CTX_DATA_END;
	return changed;
}

static bool select_grouped_layer(bContext *C, Object *ob)
{
	bool changed = false;

	CTX_DATA_BEGIN (C, Base *, base, selectable_bases)
	{
		if ((base->lay & ob->lay) && !(base->flag & SELECT)) {
			ED_base_object_select(base, BA_SELECT);
			changed = true;
		}
	}
	CTX_DATA_END;
	return changed;
}

static bool select_grouped_index_object(bContext *C, Object *ob)
{
	bool changed = false;

	CTX_DATA_BEGIN (C, Base *, base, selectable_bases)
	{
		if ((base->object->index == ob->index) && !(base->flag & SELECT)) {
			ED_base_object_select(base, BA_SELECT);
			changed = true;
		}
	}
	CTX_DATA_END;
	return changed;
}

static bool select_grouped_color(bContext *C, Object *ob)
{
	bool changed = false;

	CTX_DATA_BEGIN (C, Base *, base, selectable_bases)
	{
		if (!(base->flag & SELECT) && (compare_v3v3(base->object->col, ob->col, 0.005f))) {
			ED_base_object_select(base, BA_SELECT);
			changed = true;
		}
	}
	CTX_DATA_END;
	return changed;
}

static bool objects_share_gameprop(Object *a, Object *b)
{
	bProperty *prop;

	for (prop = a->prop.first; prop; prop = prop->next) {
		if (BKE_bproperty_object_get(b, prop->name)) {
			return 1;
		}
	}
	return 0;
}

static bool select_grouped_gameprops(bContext *C, Object *ob)
{
	bool changed = false;

	CTX_DATA_BEGIN (C, Base *, base, selectable_bases)
	{
		if (!(base->flag & SELECT) && (objects_share_gameprop(base->object, ob))) {
			ED_base_object_select(base, BA_SELECT);
			changed = true;
		}
	}
	CTX_DATA_END;
	return changed;
}

static bool select_grouped_keyingset(bContext *C, Object *UNUSED(ob), ReportList *reports)
{
	KeyingSet *ks = ANIM_scene_get_active_keyingset(CTX_data_scene(C));
	bool changed = false;
	
	/* firstly, validate KeyingSet */
	if (ks == NULL) {
		BKE_report(reports, RPT_ERROR, "No active Keying Set to use");
		return false;
	}
	else if (ANIM_validate_keyingset(C, NULL, ks) != 0) {
		if (ks->paths.first == NULL) {
			if ((ks->flag & KEYINGSET_ABSOLUTE) == 0) {
				BKE_report(reports, RPT_ERROR, 
				           "Use another Keying Set, as the active one depends on the currently "
				           "selected objects or cannot find any targets due to unsuitable context");
			}
			else {
				BKE_report(reports, RPT_ERROR, "Keying Set does not contain any paths");
			}
		}
		return false;
	}
	
	/* select each object that Keying Set refers to */
	/* TODO: perhaps to be more in line with the rest of these, we should only take objects
	 * if the passed in object is included in this too */
	CTX_DATA_BEGIN (C, Base *, base, selectable_bases)
	{
		/* only check for this object if it isn't selected already, to limit time wasted */
		if ((base->flag & SELECT) == 0) {
			KS_Path *ksp;
			
			/* this is the slow way... we could end up with > 500 items here, 
			 * with none matching, but end up doing this on 1000 objects...
			 */
			for (ksp = ks->paths.first; ksp; ksp = ksp->next) {
				/* if id matches, select then stop looping (match found) */
				if (ksp->id == (ID *)base->object) {
					ED_base_object_select(base, BA_SELECT);
					changed = true;
					break;
				}
			}
		}
	}
	CTX_DATA_END;
		
	return changed;
}

static int object_select_grouped_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob;
	const int type = RNA_enum_get(op->ptr, "type");
	bool changed = false, extend;

	extend = RNA_boolean_get(op->ptr, "extend");

	if (extend == 0) {
		CTX_DATA_BEGIN (C, Base *, base, visible_bases)
		{
			ED_base_object_select(base, BA_DESELECT);
			changed = true;
		}
		CTX_DATA_END;
	}

	ob = OBACT;
	if (ob == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No active object");
		return OPERATOR_CANCELLED;
	}

	switch (type) {
		case OBJECT_GRPSEL_CHILDREN_RECURSIVE:
			changed |= select_grouped_children(C, ob, true);
			break;
		case OBJECT_GRPSEL_CHILDREN:
			changed |= select_grouped_children(C, ob, false);
			break;
		case OBJECT_GRPSEL_PARENT:
			changed |= select_grouped_parent(C);
			break;
		case OBJECT_GRPSEL_SIBLINGS:
			changed |= select_grouped_siblings(C, ob);
			break;
		case OBJECT_GRPSEL_TYPE:
			changed |= select_grouped_type(C, ob);
			break;
		case OBJECT_GRPSEL_LAYER:
			changed |= select_grouped_layer(C, ob);
			break;
		case OBJECT_GRPSEL_GROUP:
			changed |= select_grouped_group(C, ob);
			break;
		case OBJECT_GRPSEL_HOOK:
			changed |= select_grouped_object_hooks(C, ob);
			break;
		case OBJECT_GRPSEL_PASS:
			changed |= select_grouped_index_object(C, ob);
			break;
		case OBJECT_GRPSEL_COLOR:
			changed |= select_grouped_color(C, ob);
			break;
		case OBJECT_GRPSEL_PROPERTIES:
			changed |= select_grouped_gameprops(C, ob);
			break;
		case OBJECT_GRPSEL_KEYINGSET:
			changed |= select_grouped_keyingset(C, ob, op->reports);
			break;
		case OBJECT_GRPSEL_LAMP_TYPE:
			if (ob->type != OB_LAMP) {
				BKE_report(op->reports, RPT_ERROR, "Active object must be a lamp");
				break;
			}
			changed |= select_grouped_lamptype(C, ob);
			break;
		default:
			break;
	}

	if (changed) {
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void OBJECT_OT_select_grouped(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Grouped";
	ot->description = "Select all visible objects grouped by various properties";
	ot->idname = "OBJECT_OT_select_grouped";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = object_select_grouped_exec;
	ot->poll = objects_selectable_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend selection instead of deselecting everything first");
	ot->prop = RNA_def_enum(ot->srna, "type", prop_select_grouped_types, 0, "Type", "");
}

/************************* Select by Layer **********************/
enum {
	OB_SEL_LAYERMATCH_EXACT = 1,
	OB_SEL_LAYERMATCH_SHARED = 2,
};

static int object_select_by_layer_exec(bContext *C, wmOperator *op)
{
	unsigned int layernum;
	bool extend;
	int match;
	
	extend = RNA_boolean_get(op->ptr, "extend");
	layernum = RNA_int_get(op->ptr, "layers");
	match = RNA_enum_get(op->ptr, "match");
	
	if (extend == false) {
		CTX_DATA_BEGIN (C, Base *, base, visible_bases)
		{
			ED_base_object_select(base, BA_DESELECT);
		}
		CTX_DATA_END;
	}
		
	CTX_DATA_BEGIN (C, Base *, base, visible_bases)
	{
		bool ok = false;

		switch (match) {
			case OB_SEL_LAYERMATCH_EXACT:
				/* Mask out bits used for local view, only work on real layer ones, see T45783. */
				ok = ((base->lay & ((1 << 20) - 1)) == (1 << (layernum - 1)));
				break;
			case OB_SEL_LAYERMATCH_SHARED:
				ok = (base->lay & (1 << (layernum - 1))) != 0;
				break;
			default:
				break;
		}

		if (ok) {
			ED_base_object_select(base, BA_SELECT);
		}
	}
	CTX_DATA_END;
	
	/* undo? */
	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_select_by_layer(wmOperatorType *ot)
{
	static EnumPropertyItem match_items[] = {
		{OB_SEL_LAYERMATCH_EXACT, "EXACT", 0, "Exact Match", ""},
		{OB_SEL_LAYERMATCH_SHARED, "SHARED", 0, "Shared Layers", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Select by Layer";
	ot->description = "Select all visible objects on a layer";
	ot->idname = "OBJECT_OT_select_by_layer";
	
	/* api callbacks */
	/*ot->invoke = XXX - need a int grid popup*/
	ot->exec = object_select_by_layer_exec;
	ot->poll = objects_selectable_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_enum(ot->srna, "match", match_items, OB_SEL_LAYERMATCH_EXACT, "Match", "");
	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend selection instead of deselecting everything first");
	RNA_def_int(ot->srna, "layers", 1, 1, 20, "Layer", "", 1, 20);
}

/**************************** (De)select All ****************************/

static int object_select_all_exec(bContext *C, wmOperator *op)
{
	int action = RNA_enum_get(op->ptr, "action");
	
	/* passthrough if no objects are visible */
	if (CTX_DATA_COUNT(C, visible_bases) == 0) return OPERATOR_PASS_THROUGH;

	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;
		CTX_DATA_BEGIN (C, Base *, base, visible_bases)
		{
			if (base->flag & SELECT) {
				action = SEL_DESELECT;
				break;
			}
		}
		CTX_DATA_END;
	}

	CTX_DATA_BEGIN (C, Base *, base, visible_bases)
	{
		switch (action) {
			case SEL_SELECT:
				ED_base_object_select(base, BA_SELECT);
				break;
			case SEL_DESELECT:
				ED_base_object_select(base, BA_DESELECT);
				break;
			case SEL_INVERT:
				if (base->flag & SELECT) {
					ED_base_object_select(base, BA_DESELECT);
				}
				else {
					ED_base_object_select(base, BA_SELECT);
				}
				break;
		}
	}
	CTX_DATA_END;
	
	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_select_all(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "(De)select All";
	ot->description = "Change selection of all visible objects in scene";
	ot->idname = "OBJECT_OT_select_all";
	
	/* api callbacks */
	ot->exec = object_select_all_exec;
	ot->poll = objects_selectable_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	WM_operator_properties_select_all(ot);
}

/**************************** Select In The Same Group ****************************/

static int object_select_same_group_exec(bContext *C, wmOperator *op)
{
	Group *group;
	char group_name[MAX_ID_NAME];

	/* passthrough if no objects are visible */
	if (CTX_DATA_COUNT(C, visible_bases) == 0) return OPERATOR_PASS_THROUGH;

	RNA_string_get(op->ptr, "group", group_name);

	group = (Group *)BKE_libblock_find_name(ID_GR, group_name);

	if (!group) {
		return OPERATOR_PASS_THROUGH;
	}

	CTX_DATA_BEGIN (C, Base *, base, visible_bases)
	{
		if (!(base->flag & SELECT) && BKE_group_object_exists(group, base->object))
			ED_base_object_select(base, BA_SELECT);
	}
	CTX_DATA_END;

	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_select_same_group(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Select Same Group";
	ot->description = "Select object in the same group";
	ot->idname = "OBJECT_OT_select_same_group";
	
	/* api callbacks */
	ot->exec = object_select_same_group_exec;
	ot->poll = objects_selectable_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_string(ot->srna, "group", NULL, MAX_ID_NAME, "Group", "Name of the group to select");
}

/**************************** Select Mirror ****************************/
static int object_select_mirror_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	bool extend;
	
	extend = RNA_boolean_get(op->ptr, "extend");
	
	CTX_DATA_BEGIN (C, Base *, primbase, selected_bases)
	{
		char name_flip[MAXBONENAME];

		BLI_string_flip_side_name(name_flip, primbase->object->id.name + 2, true, sizeof(name_flip));
		
		if (!STREQ(name_flip, primbase->object->id.name + 2)) {
			Object *ob = (Object *)BKE_libblock_find_name(ID_OB, name_flip);
			if (ob) {
				Base *secbase = BKE_scene_base_find(scene, ob);

				if (secbase) {
					ED_base_object_select(secbase, BA_SELECT);
				}
			}
		}
		
		if (extend == false) ED_base_object_select(primbase, BA_DESELECT);
		
	}
	CTX_DATA_END;
	
	/* undo? */
	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_select_mirror(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Select Mirror";
	ot->description = "Select the Mirror objects of the selected object eg. L.sword -> R.sword";
	ot->idname = "OBJECT_OT_select_mirror";
	
	/* api callbacks */
	ot->exec = object_select_mirror_exec;
	ot->poll = objects_selectable_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend selection instead of deselecting everything first");
}


/** \name Select More/Less
 * \{ */

static bool object_select_more_less(bContext *C, const bool select)
{
	Scene *scene = CTX_data_scene(C);

	for (Base *base = scene->base.first; base; base = base->next) {
		Object *ob = base->object;
		ob->flag &= ~OB_DONE;
		ob->id.tag &= ~LIB_TAG_DOIT;
		/* parent may be in another scene */
		if (ob->parent) {
			ob->parent->flag &= ~OB_DONE;
			ob->parent->id.tag &= ~LIB_TAG_DOIT;
		}
	}

	ListBase ctx_base_list;
	CollectionPointerLink *ctx_base;
	CTX_data_selectable_bases(C, &ctx_base_list);

	CTX_DATA_BEGIN (C, Object *, ob, selected_objects)
	{
		ob->flag |= OB_DONE;
	}
	CTX_DATA_END;



	for (ctx_base = ctx_base_list.first; ctx_base; ctx_base = ctx_base->next) {
		Object *ob = ((Base *)ctx_base->ptr.data)->object;
		if (ob->parent) {
			if ((ob->flag & OB_DONE) != (ob->parent->flag & OB_DONE)) {
				ob->id.tag         |= LIB_TAG_DOIT;
				ob->parent->id.tag |= LIB_TAG_DOIT;
			}
		}
	}

	bool changed = false;
	const short select_mode = select ? BA_SELECT : BA_DESELECT;
	const short select_flag = select ? SELECT : 0;

	for (ctx_base = ctx_base_list.first; ctx_base; ctx_base = ctx_base->next) {
		Base *base = ctx_base->ptr.data;
		Object *ob = base->object;
		if ((ob->id.tag & LIB_TAG_DOIT) && ((ob->flag & SELECT) != select_flag)) {
			ED_base_object_select(base, select_mode);
			changed = true;
		}
	}

	BLI_freelistN(&ctx_base_list);

	return changed;
}

static int object_select_more_exec(bContext *C, wmOperator *UNUSED(op))
{
	bool changed = object_select_more_less(C, true);

	if (changed) {
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, CTX_data_scene(C));
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void OBJECT_OT_select_more(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select More";
	ot->idname = "OBJECT_OT_select_more";
	ot->description = "Select connected parent/child objects";

	/* api callbacks */
	ot->exec = object_select_more_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int object_select_less_exec(bContext *C, wmOperator *UNUSED(op))
{
	bool changed = object_select_more_less(C, false);

	if (changed) {
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, CTX_data_scene(C));
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void OBJECT_OT_select_less(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Less";
	ot->idname = "OBJECT_OT_select_less";
	ot->description = "Deselect objects at the boundaries of parent/child relationships";

	/* api callbacks */
	ot->exec = object_select_less_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */


/**************************** Select Random ****************************/

static int object_select_random_exec(bContext *C, wmOperator *op)
{	
	const float randfac = RNA_float_get(op->ptr, "percent") / 100.0f;
	const int seed = WM_operator_properties_select_random_seed_increment_get(op);
	const bool select = (RNA_enum_get(op->ptr, "action") == SEL_SELECT);

	RNG *rng = BLI_rng_new_srandom(seed);

	CTX_DATA_BEGIN (C, Base *, base, selectable_bases)
	{
		if (BLI_rng_get_float(rng) < randfac) {
			ED_base_object_select(base, select);
		}
	}
	CTX_DATA_END;

	BLI_rng_free(rng);

	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_select_random(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Random";
	ot->description = "Set select on random visible objects";
	ot->idname = "OBJECT_OT_select_random";
	
	/* api callbacks */
	/*ot->invoke = object_select_random_invoke XXX - need a number popup ;*/
	ot->exec = object_select_random_exec;
	ot->poll = objects_selectable_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	WM_operator_properties_select_random(ot);
}
