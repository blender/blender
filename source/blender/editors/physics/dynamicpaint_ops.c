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
 * ***** END GPL LICENSE BLOCK *****
 */

#include "DNA_dynamicpaint_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_dynamicpaint.h"
#include "BKE_modifier.h"

#include "ED_mesh.h"
#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_types.h"
#include "WM_api.h"

static int dynamicpaint_bake_exec(bContext *C, wmOperator *op)
{
	/* Bake dynamic paint */
	if(!dynamicPaint_initBake(C, op)) {
		return OPERATOR_CANCELLED;}

	return OPERATOR_FINISHED;
}

void DPAINT_OT_bake(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Dynamic Paint Bake";
	ot->description= "Bake dynamic paint image sequence surface";
	ot->idname= "DPAINT_OT_bake";
	
	/* api callbacks */
	ot->exec= dynamicpaint_bake_exec;
	ot->poll= ED_operator_object_active_editable;
}

static int surface_slot_add_exec(bContext *C, wmOperator *op)
{
	DynamicPaintModifierData *pmd = 0;
	Object *cObject = CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	DynamicPaintSurface *surface;

	/* Make sure we're dealing with a canvas */
	pmd = (DynamicPaintModifierData *)modifiers_findByType(cObject, eModifierType_DynamicPaint);
	if (!pmd) return OPERATOR_CANCELLED;
	if (!pmd->canvas) return OPERATOR_CANCELLED;

	surface = dynamicPaint_createNewSurface(pmd->canvas, CTX_data_scene(C));

	if (!surface) return OPERATOR_CANCELLED;

	/* set preview for this surface only and set active */
	pmd->canvas->active_sur = 0;
	for(surface=surface->prev; surface; surface=surface->prev) {
				surface->flags &= ~MOD_DPAINT_PREVIEW;
				pmd->canvas->active_sur++;
	}

	return OPERATOR_FINISHED;
}

/* add surface slot */
void DPAINT_OT_surface_slot_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Surface Slot";
	ot->idname= "DPAINT_OT_surface_slot_add";
	ot->description="Add a new Dynamic Paint surface slot";
	
	/* api callbacks */
	ot->exec= surface_slot_add_exec;
	ot->poll= ED_operator_object_active_editable;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int surface_slot_remove_exec(bContext *C, wmOperator *op)
{
	DynamicPaintModifierData *pmd = 0;
	Object *cObject = CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	DynamicPaintSurface *surface;
	int id=0;

	/* Make sure we're dealing with a canvas */
	pmd = (DynamicPaintModifierData *)modifiers_findByType(cObject, eModifierType_DynamicPaint);
	if (!pmd) return OPERATOR_CANCELLED;
	if (!pmd->canvas) return OPERATOR_CANCELLED;

	surface = pmd->canvas->surfaces.first;

	/* find active surface and remove it */
	for(; surface; surface=surface->next) {
		if(id == pmd->canvas->active_sur) {
				pmd->canvas->active_sur -= 1;
				dynamicPaint_freeSurface(surface);
				break;
			}
		id++;
	}

	dynamicPaint_resetPreview(pmd->canvas);
	DAG_id_tag_update(&cObject->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, cObject);

	return OPERATOR_FINISHED;
}

/* remove surface slot */
void DPAINT_OT_surface_slot_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Surface Slot";
	ot->idname= "DPAINT_OT_surface_slot_remove";
	ot->description="Remove the selected surface slot";
	
	/* api callbacks */
	ot->exec= surface_slot_remove_exec;
	ot->poll= ED_operator_object_active_editable;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int type_toggle_exec(bContext *C, wmOperator *op)
{

	Object *cObject = CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	Scene *scene = CTX_data_scene(C);
	DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)modifiers_findByType(cObject, eModifierType_DynamicPaint);
	int type= RNA_enum_get(op->ptr, "type");

	if (!pmd) return OPERATOR_CANCELLED;

	/* if type is already enabled, toggle it off */
	if (type == MOD_DYNAMICPAINT_TYPE_CANVAS && pmd->canvas) {
			dynamicPaint_freeCanvas(pmd);
	}
	else if (type == MOD_DYNAMICPAINT_TYPE_BRUSH && pmd->brush) {
			dynamicPaint_freeBrush(pmd);
	}
	/* else create a new type */
	else {
		if (!dynamicPaint_createType(pmd, type, scene))
			return OPERATOR_CANCELLED;
	}
	
	/* update dependancy */
	DAG_id_tag_update(&cObject->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, cObject);
	DAG_scene_sort(CTX_data_main(C), scene);

	return OPERATOR_FINISHED;
}

void DPAINT_OT_type_toggle(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name= "Toggle Type Active";
	ot->idname= "DPAINT_OT_type_toggle";
	ot->description = "Toggles whether given type is active or not";
	
	/* api callbacks */
	ot->exec= type_toggle_exec;
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	prop= RNA_def_enum(ot->srna, "type", prop_dynamicpaint_type_items, MOD_DYNAMICPAINT_TYPE_CANVAS, "Type", "");
	ot->prop= prop;
}

static int output_toggle_exec(bContext *C, wmOperator *op)
{

	Object *ob = CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	Scene *scene = CTX_data_scene(C);
	DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)modifiers_findByType(ob, eModifierType_DynamicPaint);
	int index= RNA_int_get(op->ptr, "index");

	if (!pmd) return OPERATOR_CANCELLED;


	/* if type is already enabled, toggle it off */
	if (pmd->canvas) {
			DynamicPaintSurface *surface = get_activeSurface(pmd->canvas);

			if (surface->format == MOD_DPAINT_SURFACE_F_VERTEX) {
				int exists = dynamicPaint_outputLayerExists(surface, ob, index);
				char *name;
				
				if (index == 0)
					name = surface->output_name;
				else if (index == 1)
					name = surface->output_name2;

				/* Vertex Color Layer */
				if (surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
					if (!exists)
						ED_mesh_color_add(C, scene, ob, ob->data, name, 1);
					else 
						ED_mesh_color_remove_named(C, ob, ob->data, name);
				}
				/* Vertex Weight Layer */
				else if (surface->type == MOD_DPAINT_SURFACE_T_WEIGHT) {
					if (!exists)
						ED_vgroup_add_name(ob, name);
					else {
						bDeformGroup *defgroup = defgroup_find_name(ob, name);
						if (defgroup) ED_vgroup_delete(ob, defgroup);
					}
				}
			}
	}

	return OPERATOR_FINISHED;
}

void DPAINT_OT_output_toggle(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name= "Toggle Output Layer";
	ot->idname= "DPAINT_OT_output_toggle";
	ot->description = "Adds or removes Dynamic Paint output data layer.";
	
	/* api callbacks */
	ot->exec= output_toggle_exec;
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	prop= RNA_def_int(ot->srna, "index", 0, 0, 1, "Index", "", 0, 1);
	ot->prop= prop;
}
