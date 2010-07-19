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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2002-2008 full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_group_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"
#include "DNA_property_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_rand.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_group.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_particle.h"
#include "BKE_property.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"

#include "UI_interface.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "object_intern.h"

/************************ Exported **************************/

/* simple API for object selection, rather than just using the flag
 * this takes into account the 'restrict selection in 3d view' flag.
 * deselect works always, the restriction just prevents selection */

/* Note: send a NC_SCENE|ND_OB_SELECT notifier yourself! */

void ED_base_object_select(Base *base, short mode)
{
	if (base) {
		if (mode==BA_SELECT) {
			if (!(base->object->restrictflag & OB_RESTRICT_SELECT))
				base->flag |= SELECT;
		}
		else if (mode==BA_DESELECT) {
			base->flag &= ~SELECT;
		}
		base->object->flag= base->flag;
	}
}

/* also to set active NULL */
void ED_base_object_activate(bContext *C, Base *base)
{
	Scene *scene= CTX_data_scene(C);
	
	/* sets scene->basact */
	BASACT= base;
	
	if(base) {
		
		/* XXX old signals, remember to handle notifiers now! */
		//		select_actionchannel_by_name(base->object->action, "Object", 1);
		
		WM_event_add_notifier(C, NC_SCENE|ND_OB_ACTIVE, scene);
	}
	else
		WM_event_add_notifier(C, NC_SCENE|ND_OB_ACTIVE, NULL);
}

/********************** Selection Operators **********************/

/************************ Select by Type *************************/

static int object_select_by_type_exec(bContext *C, wmOperator *op)
{
	short obtype, extend;
	
	obtype = RNA_enum_get(op->ptr, "type");
	extend= RNA_boolean_get(op->ptr, "extend");
		
	if (extend == 0) {
		CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
			ED_base_object_select(base, BA_DESELECT);
		}
		CTX_DATA_END;
	}
	
	CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
		if(base->object->type==obtype) {
			ED_base_object_select(base, BA_SELECT);
		}
	}
	CTX_DATA_END;
	
	WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_select_by_type(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select By Type";
	ot->description = "Select all visible objects that are of a type";
	ot->idname= "OBJECT_OT_select_by_type";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= object_select_by_type_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "extend", FALSE, "Extend", "Extend selection instead of deselecting everything first.");
	ot->prop= RNA_def_enum(ot->srna, "type", object_type_items, 1, "Type", "");
}

/*********************** Selection by Links *********************/

static EnumPropertyItem prop_select_linked_types[] = {
	//{1, "IPO", 0, "Object IPO", ""}, // XXX depreceated animation system stuff...
	{2, "OBDATA", 0, "Object Data", ""},
	{3, "MATERIAL", 0, "Material", ""},
	{4, "TEXTURE", 0, "Texture", ""},
	{5, "DUPGROUP", 0, "Dupligroup", ""},
	{6, "PARTICLE", 0, "Particle System", ""},
	{7, "LIBRARY", 0, "Library", ""},
	{8, "LIBRARY_OBDATA", 0, "Library (Object Data)", ""},
	{0, NULL, 0, NULL, NULL}
};

static int object_select_linked_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob;
	void *obdata = NULL;
	Material *mat = NULL, *mat1;
	Tex *tex=0;
	int a, b;
	int nr = RNA_enum_get(op->ptr, "type");
	short changed = 0, extend;
	/* events (nr):
	 * Object Ipo: 1
	 * ObData: 2
	 * Current Material: 3
	 * Current Texture: 4
	 * DupliGroup: 5
	 * PSys: 6
	 */

	extend= RNA_boolean_get(op->ptr, "extend");
	
	if (extend == 0) {
		CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
			ED_base_object_select(base, BA_DESELECT);
		}
		CTX_DATA_END;
	}
	
	ob= OBACT;
	if(ob==NULL){ 
		BKE_report(op->reports, RPT_ERROR, "No Active Object");
		return OPERATOR_CANCELLED;
	}
	
	if(nr==1) {	
			// XXX old animation system
		//ipo= ob->ipo;
		//if(ipo==0) return OPERATOR_CANCELLED;
		return OPERATOR_CANCELLED;
	}
	else if(nr==2) {
		if(ob->data==0) return OPERATOR_CANCELLED;
		obdata= ob->data;
	}
	else if(nr==3 || nr==4) {
		mat= give_current_material(ob, ob->actcol);
		if(mat==0) return OPERATOR_CANCELLED;
		if(nr==4) {
			if(mat->mtex[ (int)mat->texact ]) tex= mat->mtex[ (int)mat->texact ]->tex;
			if(tex==0) return OPERATOR_CANCELLED;
		}
	}
	else if(nr==5) {
		if(ob->dup_group==NULL) return OPERATOR_CANCELLED;
	}
	else if(nr==6) {
		if(ob->particlesystem.first==NULL) return OPERATOR_CANCELLED;
	}
	else if(nr==7) {
		/* do nothing */
	}
	else if(nr==8) {
		if(ob->data==NULL) return OPERATOR_CANCELLED;
	}
	else
		return OPERATOR_CANCELLED;
	
	CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
		if(nr==1) {
				// XXX old animation system
			//if(base->object->ipo==ipo) base->flag |= SELECT;
			//changed = 1;
		}
		else if(nr==2) {
			if(base->object->data==obdata) base->flag |= SELECT;
			changed = 1;
		}
		else if(nr==3 || nr==4) {
			ob= base->object;
			
			for(a=1; a<=ob->totcol; a++) {
				mat1= give_current_material(ob, a);
				if(nr==3) {
					if(mat1==mat) base->flag |= SELECT;
					changed = 1;
				}
				else if(mat1 && nr==4) {
					for(b=0; b<MAX_MTEX; b++) {
						if(mat1->mtex[b]) {
							if(tex==mat1->mtex[b]->tex) {
								base->flag |= SELECT;
								changed = 1;
								break;
							}
						}
					}
				}
			}
		}
		else if(nr==5) {
			if(base->object->dup_group==ob->dup_group) {
				 base->flag |= SELECT;
				 changed = 1;
			}
		}
		else if(nr==6) {
			/* loop through other, then actives particles*/
			ParticleSystem *psys;
			ParticleSystem *psys_act;
			
			for(psys=base->object->particlesystem.first; psys; psys=psys->next) {
				for(psys_act=ob->particlesystem.first; psys_act; psys_act=psys_act->next) {
					if (psys->part == psys_act->part) {
						base->flag |= SELECT;
						changed = 1;
						break;
					}
				}
				
				if (base->flag & SELECT) {
					break;
				}
			}
		}
		else if(nr==7) {
			if(ob->id.lib == base->object->id.lib) {
				base->flag |= SELECT;
				changed= 1;
			}
		}
		else if(nr==8) {
			if(base->object->data && ((ID *)ob->data)->lib == ((ID *)base->object->data)->lib) {
				base->flag |= SELECT;
				changed= 1;
			}
		}
		base->object->flag= base->flag;
	}
	CTX_DATA_END;
	
	if (changed) {
		WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
		return OPERATOR_FINISHED;
	}
	
	return OPERATOR_CANCELLED;
}

void OBJECT_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Linked";
	ot->description = "Select all visible objects that are linked";
	ot->idname= "OBJECT_OT_select_linked";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= object_select_linked_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "extend", FALSE, "Extend", "Extend selection instead of deselecting everything first.");
	ot->prop= RNA_def_enum(ot->srna, "type", prop_select_linked_types, 0, "Type", "");
}

/*********************** Selected Grouped ********************/

static EnumPropertyItem prop_select_grouped_types[] = {
	{1, "CHILDREN_RECURSIVE", 0, "Children", ""},
	{2, "CHILDREN", 0, "Immediate Children", ""},
	{3, "PARENT", 0, "Parent", ""},
	{4, "SIBLINGS", 0, "Siblings", "Shared Parent"},
	{5, "TYPE", 0, "Type", "Shared object type"},
	{6, "LAYER", 0, "Layer", "Shared layers"},
	{7, "GROUP", 0, "Group", "Shared group"},
	{8, "HOOK", 0, "Hook", ""},
	{9, "PASS", 0, "Pass", "Render pass Index"},
	{10, "COLOR", 0, "Color", "Object Color"},
	{11, "PROPERTIES", 0, "Properties", "Game Properties"},
	{0, NULL, 0, NULL, NULL}
};

static short select_grouped_children(bContext *C, Object *ob, int recursive)
{
	short changed = 0;

	CTX_DATA_BEGIN(C, Base*, base, selectable_bases) {
		if (ob == base->object->parent) {
			if (!(base->flag & SELECT)) {
				ED_base_object_select(base, BA_SELECT);
				changed = 1;
			}

			if (recursive)
				changed |= select_grouped_children(C, base->object, 1);
		}
	}
	CTX_DATA_END;
	return changed;
}

static short select_grouped_parent(bContext *C)	/* Makes parent active and de-selected OBACT */
{
	Scene *scene= CTX_data_scene(C);
	View3D *v3d= CTX_wm_view3d(C);

	short changed = 0;
	Base *baspar, *basact= CTX_data_active_base(C);

	if (!basact || !(basact->object->parent)) return 0; /* we know OBACT is valid */

	baspar= object_in_scene(basact->object->parent, scene);

	/* can be NULL if parent in other scene */
	if(baspar && BASE_SELECTABLE(v3d, baspar)) {
		ED_base_object_select(basact, BA_DESELECT);
		ED_base_object_select(baspar, BA_SELECT);
		ED_base_object_activate(C, baspar);
		changed = 1;
	}
	return changed;
}


#define GROUP_MENU_MAX	24
static short select_grouped_group(bContext *C, Object *ob)	/* Select objects in the same group as the active */
{
	short changed = 0;
	Group *group, *ob_groups[GROUP_MENU_MAX];
	int group_count=0, i;
	uiPopupMenu *pup;
	uiLayout *layout;

	for (group=CTX_data_main(C)->group.first; group && group_count < GROUP_MENU_MAX; group=group->id.next) {
		if (object_in_group (ob, group)) {
			ob_groups[group_count] = group;
			group_count++;
		}
	}

	if (!group_count)
		return 0;
	else if (group_count == 1) {
		group = ob_groups[0];
		CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
			if (!(base->flag & SELECT) && object_in_group(base->object, group)) {
				ED_base_object_select(base, BA_SELECT);
				changed = 1;
			}
		}
		CTX_DATA_END;
		return changed;
	}

	/* build the menu. */
	pup= uiPupMenuBegin(C, "Select Group", 0);
	layout= uiPupMenuLayout(pup);

	for (i=0; i<group_count; i++) {
		group = ob_groups[i];
		uiItemStringO(layout, group->id.name+2, 0, "OBJECT_OT_select_same_group", "group", group->id.name);
	}

	uiPupMenuEnd(C, pup);
	return changed; // The operator already handle this!
}

static short select_grouped_object_hooks(bContext *C, Object *ob)
{
	Scene *scene= CTX_data_scene(C);
	View3D *v3d= CTX_wm_view3d(C);

	short changed = 0;
	Base *base;
	ModifierData *md;
	HookModifierData *hmd;

	for (md = ob->modifiers.first; md; md=md->next) {
		if (md->type==eModifierType_Hook) {
			hmd= (HookModifierData*) md;
			if (hmd->object && !(hmd->object->flag & SELECT)) {
				base= object_in_scene(hmd->object, scene);
				if (base && (BASE_SELECTABLE(v3d, base))) {
					ED_base_object_select(base, BA_SELECT);
					changed = 1;
				}
			}
		}
	}
	return changed;
}

/* Select objects woth the same parent as the active (siblings),
 * parent can be NULL also */
static short select_grouped_siblings(bContext *C, Object *ob)
{
	short changed = 0;

	CTX_DATA_BEGIN(C, Base*, base, selectable_bases) {
		if ((base->object->parent==ob->parent)  && !(base->flag & SELECT)) {
			ED_base_object_select(base, BA_SELECT);
			changed = 1;
		}
	}
	CTX_DATA_END;
	return changed;
}

static short select_grouped_type(bContext *C, Object *ob)
{
	short changed = 0;

	CTX_DATA_BEGIN(C, Base*, base, selectable_bases) {
		if ((base->object->type == ob->type) && !(base->flag & SELECT)) {
			ED_base_object_select(base, BA_SELECT);
			changed = 1;
		}
	}
	CTX_DATA_END;
	return changed;
}

static short select_grouped_layer(bContext *C, Object *ob)
{
	char changed = 0;

	CTX_DATA_BEGIN(C, Base*, base, selectable_bases) {
		if ((base->lay & ob->lay) && !(base->flag & SELECT)) {
			ED_base_object_select(base, BA_SELECT);
			changed = 1;
		}
	}
	CTX_DATA_END;
	return changed;
}

static short select_grouped_index_object(bContext *C, Object *ob)
{
	char changed = 0;

	CTX_DATA_BEGIN(C, Base*, base, selectable_bases) {
		if ((base->object->index == ob->index) && !(base->flag & SELECT)) {
			ED_base_object_select(base, BA_SELECT);
			changed = 1;
		}
	}
	CTX_DATA_END;
	return changed;
}

static short select_grouped_color(bContext *C, Object *ob)
{
	char changed = 0;

	CTX_DATA_BEGIN(C, Base*, base, selectable_bases) {
		if (!(base->flag & SELECT) && (compare_v3v3(base->object->col, ob->col, 0.005f))) {
			ED_base_object_select(base, BA_SELECT);
			changed = 1;
		}
	}
	CTX_DATA_END;
	return changed;
}

static short objects_share_gameprop(Object *a, Object *b)
{
	bProperty *prop;
	/*make a copy of all its properties*/

	for( prop= a->prop.first; prop; prop = prop->next ) {
		if ( get_ob_property(b, prop->name) )
			return 1;
	}
	return 0;
}

static short select_grouped_gameprops(bContext *C, Object *ob)
{
	char changed = 0;

	CTX_DATA_BEGIN(C, Base*, base, selectable_bases) {
		if (!(base->flag & SELECT) && (objects_share_gameprop(base->object, ob))) {
			ED_base_object_select(base, BA_SELECT);
			changed = 1;
		}
	}
	CTX_DATA_END;
	return changed;
}

static int object_select_grouped_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob;
	int nr = RNA_enum_get(op->ptr, "type");
	short changed = 0, extend;

	extend= RNA_boolean_get(op->ptr, "extend");
	
	if (extend == 0) {
		CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
			ED_base_object_select(base, BA_DESELECT);
			changed = 1;
		}
		CTX_DATA_END;
	}
	
	ob= OBACT;
	if(ob==0){ 
		BKE_report(op->reports, RPT_ERROR, "No Active Object");
		return OPERATOR_CANCELLED;
	}
	
	if(nr==1)		changed |= select_grouped_children(C, ob, 1);
	else if(nr==2)	changed |= select_grouped_children(C, ob, 0);
	else if(nr==3)	changed |= select_grouped_parent(C);
	else if(nr==4)	changed |= select_grouped_siblings(C, ob);
	else if(nr==5)	changed |= select_grouped_type(C, ob);
	else if(nr==6)	changed |= select_grouped_layer(C, ob);
	else if(nr==7)	changed |= select_grouped_group(C, ob);
	else if(nr==8)	changed |= select_grouped_object_hooks(C, ob);
	else if(nr==9)	changed |= select_grouped_index_object(C, ob);
	else if(nr==10)	changed |= select_grouped_color(C, ob);
	else if(nr==11)	changed |= select_grouped_gameprops(C, ob);
	
	if (changed) {
		WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
		return OPERATOR_FINISHED;
	}
	
	return OPERATOR_CANCELLED;
}

void OBJECT_OT_select_grouped(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Grouped";
	ot->description = "Select all visible objects grouped by various properties";
	ot->idname= "OBJECT_OT_select_grouped";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= object_select_grouped_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "extend", FALSE, "Extend", "Extend selection instead of deselecting everything first.");
	ot->prop= RNA_def_enum(ot->srna, "type", prop_select_grouped_types, 0, "Type", "");
}

/************************* Select by Layer **********************/

static int object_select_by_layer_exec(bContext *C, wmOperator *op)
{
	unsigned int layernum;
	short extend;
	
	extend= RNA_boolean_get(op->ptr, "extend");
	layernum = RNA_int_get(op->ptr, "layer");
	
	if (extend == 0) {
		CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
			ED_base_object_select(base, BA_DESELECT);
		}
		CTX_DATA_END;
	}
		
	CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
		if(base->lay == (1<< (layernum -1)))
			ED_base_object_select(base, BA_SELECT);
	}
	CTX_DATA_END;
	
	/* undo? */
	WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_select_by_layer(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "select by layer";
	ot->description = "Select all visible objects on a layer";
	ot->idname= "OBJECT_OT_select_by_layer";
	
	/* api callbacks */
	/*ot->invoke = XXX - need a int grid popup*/
	ot->exec= object_select_by_layer_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "extend", FALSE, "Extend", "Extend selection instead of deselecting everything first.");
	RNA_def_int(ot->srna, "layer", 1, 1, 20, "Layer", "", 1, 20);
}

/************************** Select Inverse *************************/

static int object_select_inverse_exec(bContext *C, wmOperator *op)
{
	CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
		if (base->flag & SELECT)
			ED_base_object_select(base, BA_DESELECT);
		else
			ED_base_object_select(base, BA_SELECT);
	}
	CTX_DATA_END;
	
	/* undo? */
	WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_select_inverse(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Select Inverse";
	ot->description = "Invert selection of all visible objects";
	ot->idname= "OBJECT_OT_select_inverse";
	
	/* api callbacks */
	ot->exec= object_select_inverse_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
}

/**************************** (De)select All ****************************/

static int object_select_all_exec(bContext *C, wmOperator *op)
{
	int action = RNA_enum_get(op->ptr, "action");
	
	/* passthrough if no objects are visible */
	if (CTX_DATA_COUNT(C, visible_bases) == 0) return OPERATOR_PASS_THROUGH;

	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;
		CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
			if (base->flag & SELECT) {
				action = SEL_DESELECT;
				break;
			}
		}
		CTX_DATA_END;
	}

	CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
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
			} else {
				ED_base_object_select(base, BA_SELECT);
			}
			break;
		}
	}
	CTX_DATA_END;
	
	WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_select_all(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "deselect all";
	ot->description = "Change selection of all visible objects in scene";
	ot->idname= "OBJECT_OT_select_all";
	
	/* api callbacks */
	ot->exec= object_select_all_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	WM_operator_properties_select_all(ot);
}

/**************************** Select In The Same Group ****************************/

static int object_select_same_group_exec(bContext *C, wmOperator *op)
{
	Group *group;
	char group_name[32];

	/* passthrough if no objects are visible */
	if (CTX_DATA_COUNT(C, visible_bases) == 0) return OPERATOR_PASS_THROUGH;

	RNA_string_get(op->ptr, "group", group_name);

	for (group=CTX_data_main(C)->group.first;	group; group=group->id.next) {
		if (!strcmp(group->id.name, group_name))
			break;
	}

	if (!group)
		return OPERATOR_PASS_THROUGH;

	CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
		if (!(base->flag & SELECT) && object_in_group(base->object, group))
			ED_base_object_select(base, BA_SELECT);
	}
	CTX_DATA_END;

	WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_select_same_group(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "select same group";
	ot->description = "Select object in the same group";
	ot->idname= "OBJECT_OT_select_same_group";
	
	/* api callbacks */
	ot->exec= object_select_same_group_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_string(ot->srna, "group", "", 32, "Group", "Name of the group to select.");
}

/**************************** Select Mirror ****************************/

/* finds the best possible flipped name. For renaming; check for unique names afterwards */
/* if strip_number: removes number extensions */
void object_flip_name (char *name)
{
	int     len;
	char    prefix[128]={""};   /* The part before the facing */
	char    suffix[128]={""};   /* The part after the facing */
	char    replace[128]={""};  /* The replacement string */
	char    number[128]={""};   /* The number extension string */
	char    *index=NULL;

	len= strlen(name);
	if(len<3) return; // we don't do names like .R or .L

	/* We first check the case with a .### extension, let's find the last period */
	if(isdigit(name[len-1])) {
		index= strrchr(name, '.'); // last occurrence
		if (index && isdigit(index[1]) ) { // doesnt handle case bone.1abc2 correct..., whatever!
			strcpy(number, index);
			*index= 0;
			len= strlen(name);
		}
	}

	strcpy (prefix, name);

#define IS_SEPARATOR(a) ((a)=='.' || (a)==' ' || (a)=='-' || (a)=='_')

	/* first case; separator . - _ with extensions r R l L  */
	if( IS_SEPARATOR(name[len-2]) ) {
		switch(name[len-1]) {
			case 'l':
				prefix[len-1]= 0;
				strcpy(replace, "r");
				break;
			case 'r':
				prefix[len-1]= 0;
				strcpy(replace, "l");
				break;
			case 'L':
				prefix[len-1]= 0;
				strcpy(replace, "R");
				break;
			case 'R':
				prefix[len-1]= 0;
				strcpy(replace, "L");
				break;
		}
	}
	/* case; beginning with r R l L , with separator after it */
	else if( IS_SEPARATOR(name[1]) ) {
		switch(name[0]) {
			case 'l':
				strcpy(replace, "r");
				strcpy(suffix, name+1);
				prefix[0]= 0;
				break;
			case 'r':
				strcpy(replace, "l");
				strcpy(suffix, name+1);
				prefix[0]= 0;
				break;
			case 'L':
				strcpy(replace, "R");
				strcpy(suffix, name+1);
				prefix[0]= 0;
				break;
			case 'R':
				strcpy(replace, "L");
				strcpy(suffix, name+1);
				prefix[0]= 0;
				break;
		}
	}
	else if(len > 5) {
		/* hrms, why test for a separator? lets do the rule 'ultimate left or right' */
		index = BLI_strcasestr(prefix, "right");
		if (index==prefix || index==prefix+len-5) {
			if(index[0]=='r') 
				strcpy (replace, "left");
			else {
				if(index[1]=='I') 
					strcpy (replace, "LEFT");
				else
					strcpy (replace, "Left");
			}
			*index= 0;
			strcpy (suffix, index+5);
		}
		else {
			index = BLI_strcasestr(prefix, "left");
			if (index==prefix || index==prefix+len-4) {
				if(index[0]=='l') 
					strcpy (replace, "right");
				else {
					if(index[1]=='E') 
						strcpy (replace, "RIGHT");
					else
						strcpy (replace, "Right");
				}
				*index= 0;
				strcpy (suffix, index+4);
			}
		}
	}

#undef IS_SEPARATOR

	sprintf (name, "%s%s%s%s", prefix, replace, suffix, number);
}

static int object_select_mirror_exec(bContext *C, wmOperator *op)
{
	char tmpname[32];
	short extend;
	
	extend= RNA_boolean_get(op->ptr, "extend");
	
	CTX_DATA_BEGIN(C, Base*, primbase, selected_bases) {

		strcpy(tmpname, primbase->object->id.name+2);
		object_flip_name(tmpname);
		
		CTX_DATA_BEGIN(C, Base*, secbase, visible_bases) {
			if(!strcmp(secbase->object->id.name+2, tmpname)) {
				ED_base_object_select(secbase, BA_SELECT);
			}
		}
		CTX_DATA_END;
		
		if (extend == 0) ED_base_object_select(primbase, BA_DESELECT);
		
	}
	CTX_DATA_END;
	
	/* undo? */
	WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_select_mirror(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Select Mirror";
	ot->description = "Select the Mirror objects of the selected object eg. L.sword -> R.sword";
	ot->idname= "OBJECT_OT_select_mirror";
	
	/* api callbacks */
	ot->exec= object_select_mirror_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend selection instead of deselecting everything first.");
}


static int object_select_name_exec(bContext *C, wmOperator *op)
{
	char *name= RNA_string_get_alloc(op->ptr, "name", NULL, 0);
	short extend= RNA_boolean_get(op->ptr, "extend");
	short changed = 0;

	if(!extend) {
		CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
			ED_base_object_select(base, BA_DESELECT);
		}
		CTX_DATA_END;
	}

	CTX_DATA_BEGIN(C, Base*, base, selectable_bases) {
		/* this is a bit dodjy, there should only be ONE object with this name, but library objects can mess this up */
		if(strcmp(name, base->object->id.name+2)==0) {
			ED_base_object_activate(C, base);
			ED_base_object_select(base, BA_SELECT);
			changed= 1;
		}
	}
	CTX_DATA_END;

	MEM_freeN(name);

	/* undo? */
	if(changed) {
		WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void OBJECT_OT_select_name(wmOperatorType *ot)
{

	/* identifiers */
	ot->name= "Select Name";
	ot->description = "Select an object with this name";
	ot->idname= "OBJECT_OT_select_name";

	/* api callbacks */
	ot->exec= object_select_name_exec;
	ot->poll= ED_operator_scene_editable;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_string(ot->srna, "name", "", 0, "Name", "Object name to select.");
	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend selection instead of deselecting everything first.");
}

/**************************** Select Random ****************************/

static int object_select_random_exec(bContext *C, wmOperator *op)
{	
	float percent;
	short extend;
	
	extend= RNA_boolean_get(op->ptr, "extend");
	
	if (extend == 0) {
		CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
			ED_base_object_select(base, BA_DESELECT);
		}
		CTX_DATA_END;
	}
	percent = RNA_float_get(op->ptr, "percent")/100.0f;
		
	CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
		if (BLI_frand() < percent) {
			ED_base_object_select(base, BA_SELECT);
		}
	}
	CTX_DATA_END;
	
	WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_select_random(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Random";
	ot->description = "Set select on random visible objects";
	ot->idname= "OBJECT_OT_select_random";
	
	/* api callbacks */
	/*ot->invoke= object_select_random_invoke XXX - need a number popup ;*/
	ot->exec = object_select_random_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_float_percentage(ot->srna, "percent", 50.f, 0.0f, 100.0f, "Percent", "Percentage of objects to select randomly", 0.f, 100.0f);
	RNA_def_boolean(ot->srna, "extend", FALSE, "Extend Selection", "Extend selection instead of deselecting everything first.");
}


