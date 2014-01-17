/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. 
 *
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

/** \file blender/editors/render/render_shading.c
 *  \ingroup edrend
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_curve_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_font.h"
#include "BKE_freestyle.h"
#include "BKE_global.h"
#include "BKE_icons.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_linestyle.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_texture.h"
#include "BKE_world.h"
#include "BKE_editmesh.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "GPU_material.h"

#ifdef WITH_FREESTYLE
#  include "FRS_freestyle.h"
#endif

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_curve.h"
#include "ED_mesh.h"
#include "ED_node.h"
#include "ED_render.h"
#include "ED_screen.h"

#include "RNA_define.h"

#include "UI_interface.h"

#include "RE_pipeline.h"

#include "render_intern.h"  // own include

/********************** material slot operators *********************/

static int material_slot_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);

	if (!ob)
		return OPERATOR_CANCELLED;

	object_add_material_slot(ob);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, ob);
	WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_PREVIEW, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_material_slot_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Material Slot";
	ot->idname = "OBJECT_OT_material_slot_add";
	ot->description = "Add a new material slot";
	
	/* api callbacks */
	ot->exec = material_slot_add_exec;
	ot->poll = ED_operator_object_active_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int material_slot_remove_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);

	if (!ob)
		return OPERATOR_CANCELLED;

	/* Removing material slots in edit mode screws things up, see bug #21822.*/
	if (ob == CTX_data_edit_object(C)) {
		BKE_report(op->reports, RPT_ERROR, "Unable to remove material slot in edit mode");
		return OPERATOR_CANCELLED;
	}

	object_remove_material_slot(ob);
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, ob);
	WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_PREVIEW, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_material_slot_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Material Slot";
	ot->idname = "OBJECT_OT_material_slot_remove";
	ot->description = "Remove the selected material slot";
	
	/* api callbacks */
	ot->exec = material_slot_remove_exec;
	ot->poll = ED_operator_object_active_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int material_slot_assign_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);

	if (!ob)
		return OPERATOR_CANCELLED;

	if (ob && ob->actcol > 0) {
		if (ob->type == OB_MESH) {
			BMEditMesh *em = BKE_editmesh_from_object(ob);
			BMFace *efa;
			BMIter iter;

			if (em) {
				BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
					if (BM_elem_flag_test(efa, BM_ELEM_SELECT))
						efa->mat_nr = ob->actcol - 1;
				}
			}
		}
		else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
			Nurb *nu;
			ListBase *nurbs = BKE_curve_editNurbs_get((Curve *)ob->data);

			if (nurbs) {
				for (nu = nurbs->first; nu; nu = nu->next)
					if (isNurbsel(nu))
						nu->mat_nr = nu->charidx = ob->actcol - 1;
			}
		}
		else if (ob->type == OB_FONT) {
			EditFont *ef = ((Curve *)ob->data)->editfont;
			int i, selstart, selend;

			if (ef && BKE_vfont_select_get(ob, &selstart, &selend)) {
				for (i = selstart; i <= selend; i++)
					ef->textbufinfo[i].mat_nr = ob->actcol;
			}
		}
	}

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_material_slot_assign(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Assign Material Slot";
	ot->idname = "OBJECT_OT_material_slot_assign";
	ot->description = "Assign active material slot to selection";
	
	/* api callbacks */
	ot->exec = material_slot_assign_exec;
	ot->poll = ED_operator_object_active_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int material_slot_de_select(bContext *C, int select)
{
	Object *ob = ED_object_context(C);

	if (!ob)
		return OPERATOR_CANCELLED;

	if (ob->type == OB_MESH) {
		BMEditMesh *em = BKE_editmesh_from_object(ob);

		if (em) {
			EDBM_deselect_by_material(em, ob->actcol - 1, select);
		}
	}
	else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
		ListBase *nurbs = BKE_curve_editNurbs_get((Curve *)ob->data);
		Nurb *nu;
		BPoint *bp;
		BezTriple *bezt;
		int a;

		if (nurbs) {
			for (nu = nurbs->first; nu; nu = nu->next) {
				if (nu->mat_nr == ob->actcol - 1) {
					if (nu->bezt) {
						a = nu->pntsu;
						bezt = nu->bezt;
						while (a--) {
							if (bezt->hide == 0) {
								if (select) {
									bezt->f1 |= SELECT;
									bezt->f2 |= SELECT;
									bezt->f3 |= SELECT;
								}
								else {
									bezt->f1 &= ~SELECT;
									bezt->f2 &= ~SELECT;
									bezt->f3 &= ~SELECT;
								}
							}
							bezt++;
						}
					}
					else if (nu->bp) {
						a = nu->pntsu * nu->pntsv;
						bp = nu->bp;
						while (a--) {
							if (bp->hide == 0) {
								if (select) bp->f1 |= SELECT;
								else bp->f1 &= ~SELECT;
							}
							bp++;
						}
					}
				}
			}
		}
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);

	return OPERATOR_FINISHED;
}

static int material_slot_select_exec(bContext *C, wmOperator *UNUSED(op))
{
	return material_slot_de_select(C, 1);
}

void OBJECT_OT_material_slot_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Material Slot";
	ot->idname = "OBJECT_OT_material_slot_select";
	ot->description = "Select by active material slot";
	
	/* api callbacks */
	ot->exec = material_slot_select_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int material_slot_deselect_exec(bContext *C, wmOperator *UNUSED(op))
{
	return material_slot_de_select(C, 0);
}

void OBJECT_OT_material_slot_deselect(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Deselect Material Slot";
	ot->idname = "OBJECT_OT_material_slot_deselect";
	ot->description = "Deselect by active material slot";
	
	/* api callbacks */
	ot->exec = material_slot_deselect_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


static int material_slot_copy_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	Material ***matar;

	if (!ob || !(matar = give_matarar(ob)))
		return OPERATOR_CANCELLED;

	CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects)
	{
		if (ob != ob_iter && give_matarar(ob_iter)) {
			if (ob->data != ob_iter->data)
				assign_matarar(ob_iter, matar, ob->totcol);
			
			if (ob_iter->totcol == ob->totcol) {
				ob_iter->actcol = ob->actcol;
				DAG_id_tag_update(&ob_iter->id, OB_RECALC_DATA);
				WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob_iter);
			}
		}
	}
	CTX_DATA_END;

	return OPERATOR_FINISHED;
}


void OBJECT_OT_material_slot_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy Material to Others";
	ot->idname = "OBJECT_OT_material_slot_copy";
	ot->description = "Copies materials to other selected objects";

	/* api callbacks */
	ot->exec = material_slot_copy_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** new material operator *********************/

static int new_material_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	Material *ma = CTX_data_pointer_get_type(C, "material", &RNA_Material).data;
	Main *bmain = CTX_data_main(C);
	PointerRNA ptr, idptr;
	PropertyRNA *prop;

	/* add or copy material */
	if (ma) {
		ma = BKE_material_copy(ma);
	}
	else {
		ma = BKE_material_add(bmain, DATA_("Material"));

		if (BKE_scene_use_new_shading_nodes(scene)) {
			ED_node_shader_default(C, &ma->id);
			ma->use_nodes = TRUE;
		}
	}

	/* hook into UI */
	uiIDContextProperty(C, &ptr, &prop);

	if (prop) {
		/* when creating new ID blocks, use is already 1, but RNA
		 * pointer se also increases user, so this compensates it */
		ma->id.us--;

		RNA_id_pointer_create(&ma->id, &idptr);
		RNA_property_pointer_set(&ptr, prop, idptr);
		RNA_property_update(C, &ptr, prop);
	}

	WM_event_add_notifier(C, NC_MATERIAL | NA_ADDED, ma);
	
	return OPERATOR_FINISHED;
}

void MATERIAL_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Material";
	ot->idname = "MATERIAL_OT_new";
	ot->description = "Add a new material";
	
	/* api callbacks */
	ot->exec = new_material_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** new texture operator *********************/

static int new_texture_exec(bContext *C, wmOperator *UNUSED(op))
{
	Tex *tex = CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data;
	Main *bmain = CTX_data_main(C);
	PointerRNA ptr, idptr;
	PropertyRNA *prop;

	/* add or copy texture */
	if (tex) {
		tex = BKE_texture_copy(tex);
	}
	else {
		tex = add_texture(bmain, DATA_("Texture"));
	}

	/* hook into UI */
	uiIDContextProperty(C, &ptr, &prop);

	if (prop) {
		/* when creating new ID blocks, use is already 1, but RNA
		 * pointer se also increases user, so this compensates it */
		tex->id.us--;

		RNA_id_pointer_create(&tex->id, &idptr);
		RNA_property_pointer_set(&ptr, prop, idptr);
		RNA_property_update(C, &ptr, prop);
	}

	WM_event_add_notifier(C, NC_TEXTURE | NA_ADDED, tex);
	
	return OPERATOR_FINISHED;
}

void TEXTURE_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Texture";
	ot->idname = "TEXTURE_OT_new";
	ot->description = "Add a new texture";
	
	/* api callbacks */
	ot->exec = new_texture_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** new world operator *********************/

static int new_world_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	World *wo = CTX_data_pointer_get_type(C, "world", &RNA_World).data;
	Main *bmain = CTX_data_main(C);
	PointerRNA ptr, idptr;
	PropertyRNA *prop;

	/* add or copy world */
	if (wo) {
		wo = BKE_world_copy(wo);
	}
	else {
		wo = add_world(bmain, DATA_("World"));

		if (BKE_scene_use_new_shading_nodes(scene)) {
			ED_node_shader_default(C, &wo->id);
			wo->use_nodes = TRUE;
		}
	}

	/* hook into UI */
	uiIDContextProperty(C, &ptr, &prop);

	if (prop) {
		/* when creating new ID blocks, use is already 1, but RNA
		 * pointer se also increases user, so this compensates it */
		wo->id.us--;

		RNA_id_pointer_create(&wo->id, &idptr);
		RNA_property_pointer_set(&ptr, prop, idptr);
		RNA_property_update(C, &ptr, prop);
	}

	WM_event_add_notifier(C, NC_WORLD | NA_ADDED, wo);
	
	return OPERATOR_FINISHED;
}

void WORLD_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New World";
	ot->idname = "WORLD_OT_new";
	ot->description = "Add a new world";
	
	/* api callbacks */
	ot->exec = new_world_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** render layer operators *********************/

static int render_layer_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);

	BKE_scene_add_render_layer(scene, NULL);
	scene->r.actlay = BLI_countlist(&scene->r.layers) - 1;

	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);
	
	return OPERATOR_FINISHED;
}

void SCENE_OT_render_layer_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Render Layer";
	ot->idname = "SCENE_OT_render_layer_add";
	ot->description = "Add a render layer";
	
	/* api callbacks */
	ot->exec = render_layer_add_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int render_layer_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *rl = BLI_findlink(&scene->r.layers, scene->r.actlay);

	if (!BKE_scene_remove_render_layer(CTX_data_main(C), scene, rl))
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);
	
	return OPERATOR_FINISHED;
}

void SCENE_OT_render_layer_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Render Layer";
	ot->idname = "SCENE_OT_render_layer_remove";
	ot->description = "Remove the selected render layer";
	
	/* api callbacks */
	ot->exec = render_layer_remove_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

#ifdef WITH_FREESTYLE

static bool freestyle_linestyle_check_report(FreestyleLineSet *lineset, ReportList *reports)
{
	if (!lineset) {
		BKE_report(reports, RPT_ERROR, "No active lineset and associated line style to add the modifier to");
		return false;
	}
	if (!lineset->linestyle) {
		BKE_report(reports, RPT_ERROR, "The active lineset does not have a line style (indicating data corruption)");
		return false;
	}

	return true;
}

static int freestyle_active_module_poll(bContext *C)
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "freestyle_module", &RNA_FreestyleModuleSettings);
	FreestyleModuleConfig *module = ptr.data;

	return module != NULL;
}

static int freestyle_module_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *srl = BLI_findlink(&scene->r.layers, scene->r.actlay);

	BKE_freestyle_module_add(&srl->freestyleConfig);

	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

	return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_module_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Freestyle Module";
	ot->idname = "SCENE_OT_freestyle_module_add";
	ot->description = "Add a style module into the list of modules";

	/* api callbacks */
	ot->exec = freestyle_module_add_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int freestyle_module_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *srl = BLI_findlink(&scene->r.layers, scene->r.actlay);
	PointerRNA ptr = CTX_data_pointer_get_type(C, "freestyle_module", &RNA_FreestyleModuleSettings);
	FreestyleModuleConfig *module = ptr.data;

	BKE_freestyle_module_delete(&srl->freestyleConfig, module);

	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

	return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_module_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Freestyle Module";
	ot->idname = "SCENE_OT_freestyle_module_remove";
	ot->description = "Remove the style module from the stack";

	/* api callbacks */
	ot->poll = freestyle_active_module_poll;
	ot->exec = freestyle_module_remove_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int freestyle_module_move_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *srl = BLI_findlink(&scene->r.layers, scene->r.actlay);
	PointerRNA ptr = CTX_data_pointer_get_type(C, "freestyle_module", &RNA_FreestyleModuleSettings);
	FreestyleModuleConfig *module = ptr.data;
	int dir = RNA_enum_get(op->ptr, "direction");

	if (dir == 1) {
		BKE_freestyle_module_move_up(&srl->freestyleConfig, module);
	}
	else {
		BKE_freestyle_module_move_down(&srl->freestyleConfig, module);
	}
	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

	return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_module_move(wmOperatorType *ot)
{
	static EnumPropertyItem direction_items[] = {
		{1, "UP", 0, "Up", ""},
		{-1, "DOWN", 0, "Down", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Move Freestyle Module";
	ot->idname = "SCENE_OT_freestyle_module_move";
	ot->description = "Change the position of the style module within in the list of style modules";

	/* api callbacks */
	ot->poll = freestyle_active_module_poll;
	ot->exec = freestyle_module_move_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_enum(ot->srna, "direction", direction_items, 0, "Direction", "Direction to move, UP or DOWN");
}

static int freestyle_lineset_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *srl = BLI_findlink(&scene->r.layers, scene->r.actlay);

	BKE_freestyle_lineset_add(&srl->freestyleConfig);

	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

	return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_lineset_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Line Set";
	ot->idname = "SCENE_OT_freestyle_lineset_add";
	ot->description = "Add a line set into the list of line sets";

	/* api callbacks */
	ot->exec = freestyle_lineset_add_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int freestyle_active_lineset_poll(bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *srl = BLI_findlink(&scene->r.layers, scene->r.actlay);

	if (!srl) {
		return FALSE;
	}

	return BKE_freestyle_lineset_get_active(&srl->freestyleConfig) != NULL;
}

static int freestyle_lineset_copy_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *srl = BLI_findlink(&scene->r.layers, scene->r.actlay);

	FRS_copy_active_lineset(&srl->freestyleConfig);

	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

	return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_lineset_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy Line Set";
	ot->idname = "SCENE_OT_freestyle_lineset_copy";
	ot->description = "Copy the active line set to a buffer";

	/* api callbacks */
	ot->exec = freestyle_lineset_copy_exec;
	ot->poll = freestyle_active_lineset_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int freestyle_lineset_paste_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *srl = BLI_findlink(&scene->r.layers, scene->r.actlay);

	FRS_paste_active_lineset(&srl->freestyleConfig);

	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

	return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_lineset_paste(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Paste Line Set";
	ot->idname = "SCENE_OT_freestyle_lineset_paste";
	ot->description = "Paste the buffer content to the active line set";

	/* api callbacks */
	ot->exec = freestyle_lineset_paste_exec;
	ot->poll = freestyle_active_lineset_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int freestyle_lineset_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *srl = BLI_findlink(&scene->r.layers, scene->r.actlay);

	FRS_delete_active_lineset(&srl->freestyleConfig);

	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

	return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_lineset_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Line Set";
	ot->idname = "SCENE_OT_freestyle_lineset_remove";
	ot->description = "Remove the active line set from the list of line sets";

	/* api callbacks */
	ot->exec = freestyle_lineset_remove_exec;
	ot->poll = freestyle_active_lineset_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int freestyle_lineset_move_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *srl = BLI_findlink(&scene->r.layers, scene->r.actlay);
	int dir = RNA_enum_get(op->ptr, "direction");

	if (dir == 1) {
		FRS_move_active_lineset_up(&srl->freestyleConfig);
	}
	else {
		FRS_move_active_lineset_down(&srl->freestyleConfig);
	}
	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

	return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_lineset_move(wmOperatorType *ot)
{
	static EnumPropertyItem direction_items[] = {
		{1, "UP", 0, "Up", ""},
		{-1, "DOWN", 0, "Down", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Move Line Set";
	ot->idname = "SCENE_OT_freestyle_lineset_move";
	ot->description = "Change the position of the active line set within the list of line sets";

	/* api callbacks */
	ot->exec = freestyle_lineset_move_exec;
	ot->poll = freestyle_active_lineset_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_enum(ot->srna, "direction", direction_items, 0, "Direction", "Direction to move, UP or DOWN");
}

static int freestyle_linestyle_new_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *srl = BLI_findlink(&scene->r.layers, scene->r.actlay);
	FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&srl->freestyleConfig);

	if (!lineset) {
		BKE_report(op->reports, RPT_ERROR, "No active lineset to add a new line style to");
		return OPERATOR_CANCELLED;
	}
	if (lineset->linestyle) {
		lineset->linestyle->id.us--;
		lineset->linestyle = BKE_copy_linestyle(lineset->linestyle);
	}
	else {
		lineset->linestyle = BKE_new_linestyle("LineStyle", NULL);
	}

	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

	return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_linestyle_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Line Style";
	ot->idname = "SCENE_OT_freestyle_linestyle_new";
	ot->description = "Create a new line style, reusable by multiple line sets";

	/* api callbacks */
	ot->exec = freestyle_linestyle_new_exec;
	ot->poll = freestyle_active_lineset_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int freestyle_color_modifier_add_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *srl = BLI_findlink(&scene->r.layers, scene->r.actlay);
	FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&srl->freestyleConfig);
	int type = RNA_enum_get(op->ptr, "type");

	if (!freestyle_linestyle_check_report(lineset, op->reports)) {
		return OPERATOR_CANCELLED;
	}

	if (BKE_add_linestyle_color_modifier(lineset->linestyle, type) == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Unknown line color modifier type");
		return OPERATOR_CANCELLED;
	}
	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

	return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_color_modifier_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Line Color Modifier";
	ot->idname = "SCENE_OT_freestyle_color_modifier_add";
	ot->description = "Add a line color modifier to the line style associated with the active lineset";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = freestyle_color_modifier_add_exec;
	ot->poll = freestyle_active_lineset_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", linestyle_color_modifier_type_items, 0, "Type", "");
}

static int freestyle_alpha_modifier_add_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *srl = BLI_findlink(&scene->r.layers, scene->r.actlay);
	FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&srl->freestyleConfig);
	int type = RNA_enum_get(op->ptr, "type");

	if (!freestyle_linestyle_check_report(lineset, op->reports)) {
		return OPERATOR_CANCELLED;
	}

	if (BKE_add_linestyle_alpha_modifier(lineset->linestyle, type) == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Unknown alpha transparency modifier type");
		return OPERATOR_CANCELLED;
	}
	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

	return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_alpha_modifier_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Alpha Transparency Modifier";
	ot->idname = "SCENE_OT_freestyle_alpha_modifier_add";
	ot->description = "Add an alpha transparency modifier to the line style associated with the active lineset";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = freestyle_alpha_modifier_add_exec;
	ot->poll = freestyle_active_lineset_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", linestyle_alpha_modifier_type_items, 0, "Type", "");
}

static int freestyle_thickness_modifier_add_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *srl = BLI_findlink(&scene->r.layers, scene->r.actlay);
	FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&srl->freestyleConfig);
	int type = RNA_enum_get(op->ptr, "type");

	if (!freestyle_linestyle_check_report(lineset, op->reports)) {
		return OPERATOR_CANCELLED;
	}

	if (BKE_add_linestyle_thickness_modifier(lineset->linestyle, type) == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Unknown line thickness modifier type");
		return OPERATOR_CANCELLED;
	}
	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

	return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_thickness_modifier_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Line Thickness Modifier";
	ot->idname = "SCENE_OT_freestyle_thickness_modifier_add";
	ot->description = "Add a line thickness modifier to the line style associated with the active lineset";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = freestyle_thickness_modifier_add_exec;
	ot->poll = freestyle_active_lineset_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", linestyle_thickness_modifier_type_items, 0, "Type", "");
}

static int freestyle_geometry_modifier_add_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *srl = BLI_findlink(&scene->r.layers, scene->r.actlay);
	FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&srl->freestyleConfig);
	int type = RNA_enum_get(op->ptr, "type");

	if (!freestyle_linestyle_check_report(lineset, op->reports)) {
		return OPERATOR_CANCELLED;
	}

	if (BKE_add_linestyle_geometry_modifier(lineset->linestyle, type) == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Unknown stroke geometry modifier type");
		return OPERATOR_CANCELLED;
	}
	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

	return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_geometry_modifier_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Stroke Geometry Modifier";
	ot->idname = "SCENE_OT_freestyle_geometry_modifier_add";
	ot->description = "Add a stroke geometry modifier to the line style associated with the active lineset";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = freestyle_geometry_modifier_add_exec;
	ot->poll = freestyle_active_lineset_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", linestyle_geometry_modifier_type_items, 0, "Type", "");
}

static int freestyle_get_modifier_type(PointerRNA *ptr)
{
	if (RNA_struct_is_a(ptr->type, &RNA_LineStyleColorModifier))
		return LS_MODIFIER_TYPE_COLOR;
	else if (RNA_struct_is_a(ptr->type, &RNA_LineStyleAlphaModifier))
		return LS_MODIFIER_TYPE_ALPHA;
	else if (RNA_struct_is_a(ptr->type, &RNA_LineStyleThicknessModifier))
		return LS_MODIFIER_TYPE_THICKNESS;
	else if (RNA_struct_is_a(ptr->type, &RNA_LineStyleGeometryModifier))
		return LS_MODIFIER_TYPE_GEOMETRY;
	return -1;
}

static int freestyle_modifier_remove_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *srl = BLI_findlink(&scene->r.layers, scene->r.actlay);
	FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&srl->freestyleConfig);
	PointerRNA ptr = CTX_data_pointer_get_type(C, "modifier", &RNA_LineStyleModifier);
	LineStyleModifier *modifier = ptr.data;

	if (!freestyle_linestyle_check_report(lineset, op->reports)) {
		return OPERATOR_CANCELLED;
	}

	switch (freestyle_get_modifier_type(&ptr)) {
		case LS_MODIFIER_TYPE_COLOR:
			BKE_remove_linestyle_color_modifier(lineset->linestyle, modifier);
			break;
		case LS_MODIFIER_TYPE_ALPHA:
			BKE_remove_linestyle_alpha_modifier(lineset->linestyle, modifier);
			break;
		case LS_MODIFIER_TYPE_THICKNESS:
			BKE_remove_linestyle_thickness_modifier(lineset->linestyle, modifier);
			break;
		case LS_MODIFIER_TYPE_GEOMETRY:
			BKE_remove_linestyle_geometry_modifier(lineset->linestyle, modifier);
			break;
		default:
			BKE_report(op->reports, RPT_ERROR, "The object the data pointer refers to is not a valid modifier");
			return OPERATOR_CANCELLED;
	}
	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

	return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_modifier_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Modifier";
	ot->idname = "SCENE_OT_freestyle_modifier_remove";
	ot->description = "Remove the modifier from the list of modifiers";

	/* api callbacks */
	ot->exec = freestyle_modifier_remove_exec;
	ot->poll = freestyle_active_lineset_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int freestyle_modifier_copy_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *srl = BLI_findlink(&scene->r.layers, scene->r.actlay);
	FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&srl->freestyleConfig);
	PointerRNA ptr = CTX_data_pointer_get_type(C, "modifier", &RNA_LineStyleModifier);
	LineStyleModifier *modifier = ptr.data;

	if (!freestyle_linestyle_check_report(lineset, op->reports)) {
		return OPERATOR_CANCELLED;
	}

	switch (freestyle_get_modifier_type(&ptr)) {
		case LS_MODIFIER_TYPE_COLOR:
			BKE_copy_linestyle_color_modifier(lineset->linestyle, modifier);
			break;
		case LS_MODIFIER_TYPE_ALPHA:
			BKE_copy_linestyle_alpha_modifier(lineset->linestyle, modifier);
			break;
		case LS_MODIFIER_TYPE_THICKNESS:
			BKE_copy_linestyle_thickness_modifier(lineset->linestyle, modifier);
			break;
		case LS_MODIFIER_TYPE_GEOMETRY:
			BKE_copy_linestyle_geometry_modifier(lineset->linestyle, modifier);
			break;
		default:
			BKE_report(op->reports, RPT_ERROR, "The object the data pointer refers to is not a valid modifier");
			return OPERATOR_CANCELLED;
	}
	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

	return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_modifier_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy Modifier";
	ot->idname = "SCENE_OT_freestyle_modifier_copy";
	ot->description = "Duplicate the modifier within the list of modifiers";

	/* api callbacks */
	ot->exec = freestyle_modifier_copy_exec;
	ot->poll = freestyle_active_lineset_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int freestyle_modifier_move_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *srl = BLI_findlink(&scene->r.layers, scene->r.actlay);
	FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(&srl->freestyleConfig);
	PointerRNA ptr = CTX_data_pointer_get_type(C, "modifier", &RNA_LineStyleModifier);
	LineStyleModifier *modifier = ptr.data;
	int dir = RNA_enum_get(op->ptr, "direction");

	if (!freestyle_linestyle_check_report(lineset, op->reports)) {
		return OPERATOR_CANCELLED;
	}

	switch (freestyle_get_modifier_type(&ptr)) {
		case LS_MODIFIER_TYPE_COLOR:
			BKE_move_linestyle_color_modifier(lineset->linestyle, modifier, dir);
			break;
		case LS_MODIFIER_TYPE_ALPHA:
			BKE_move_linestyle_alpha_modifier(lineset->linestyle, modifier, dir);
			break;
		case LS_MODIFIER_TYPE_THICKNESS:
			BKE_move_linestyle_thickness_modifier(lineset->linestyle, modifier, dir);
			break;
		case LS_MODIFIER_TYPE_GEOMETRY:
			BKE_move_linestyle_geometry_modifier(lineset->linestyle, modifier, dir);
			break;
		default:
			BKE_report(op->reports, RPT_ERROR, "The object the data pointer refers to is not a valid modifier");
			return OPERATOR_CANCELLED;
	}
	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);

	return OPERATOR_FINISHED;
}

void SCENE_OT_freestyle_modifier_move(wmOperatorType *ot)
{
	static EnumPropertyItem direction_items[] = {
		{1, "UP", 0, "Up", ""},
		{-1, "DOWN", 0, "Down", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Move Modifier";
	ot->idname = "SCENE_OT_freestyle_modifier_move";
	ot->description = "Move the modifier within the list of modifiers";

	/* api callbacks */
	ot->exec = freestyle_modifier_move_exec;
	ot->poll = freestyle_active_lineset_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_enum(ot->srna, "direction", direction_items, 0, "Direction", "Direction to move, UP or DOWN");
}

#endif /* WITH_FREESTYLE */

static int texture_slot_move_exec(bContext *C, wmOperator *op)
{
	ID *id = CTX_data_pointer_get_type(C, "texture_slot", &RNA_TextureSlot).id.data;

	if (id) {
		MTex **mtex_ar, *mtexswap;
		short act;
		int type = RNA_enum_get(op->ptr, "type");
		struct AnimData *adt = BKE_animdata_from_id(id);

		give_active_mtex(id, &mtex_ar, &act);

		if (type == -1) { /* Up */
			if (act > 0) {
				mtexswap = mtex_ar[act];
				mtex_ar[act] = mtex_ar[act - 1];
				mtex_ar[act - 1] = mtexswap;
				
				BKE_animdata_fix_paths_rename(id, adt, NULL, "texture_slots", NULL, NULL, act - 1, -1, 0);
				BKE_animdata_fix_paths_rename(id, adt, NULL, "texture_slots", NULL, NULL, act, act - 1, 0);
				BKE_animdata_fix_paths_rename(id, adt, NULL, "texture_slots", NULL, NULL, -1, act, 0);

				if (GS(id->name) == ID_MA) {
					Material *ma = (Material *)id;
					int mtexuse = ma->septex & (1 << act);
					ma->septex &= ~(1 << act);
					ma->septex |= (ma->septex & (1 << (act - 1))) << 1;
					ma->septex &= ~(1 << (act - 1));
					ma->septex |= mtexuse >> 1;
				}
				
				set_active_mtex(id, act - 1);
			}
		}
		else { /* Down */
			if (act < MAX_MTEX - 1) {
				mtexswap = mtex_ar[act];
				mtex_ar[act] = mtex_ar[act + 1];
				mtex_ar[act + 1] = mtexswap;
				
				BKE_animdata_fix_paths_rename(id, adt, NULL, "texture_slots", NULL, NULL, act + 1, -1, 0);
				BKE_animdata_fix_paths_rename(id, adt, NULL, "texture_slots", NULL, NULL, act, act + 1, 0);
				BKE_animdata_fix_paths_rename(id, adt, NULL, "texture_slots", NULL, NULL, -1, act, 0);

				if (GS(id->name) == ID_MA) {
					Material *ma = (Material *)id;
					int mtexuse = ma->septex & (1 << act);
					ma->septex &= ~(1 << act);
					ma->septex |= (ma->septex & (1 << (act + 1))) >> 1;
					ma->septex &= ~(1 << (act + 1));
					ma->septex |= mtexuse << 1;
				}
				
				set_active_mtex(id, act + 1);
			}
		}

		DAG_id_tag_update(id, 0);
		WM_event_add_notifier(C, NC_TEXTURE, CTX_data_scene(C));
	}

	return OPERATOR_FINISHED;
}

void TEXTURE_OT_slot_move(wmOperatorType *ot)
{
	static EnumPropertyItem slot_move[] = {
		{-1, "UP", 0, "Up", ""},
		{1, "DOWN", 0, "Down", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Move Texture Slot";
	ot->idname = "TEXTURE_OT_slot_move";
	ot->description = "Move texture slots up and down";

	/* api callbacks */
	ot->exec = texture_slot_move_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "type", slot_move, 0, "Type", "");
}



/********************** environment map operators *********************/

static int save_envmap(wmOperator *op, Scene *scene, EnvMap *env, char *path, const char imtype)
{
	float layout[12];
	if (RNA_struct_find_property(op->ptr, "layout") )
		RNA_float_get_array(op->ptr, "layout", layout);
	else
		memcpy(layout, default_envmap_layout, sizeof(layout));

	if (RE_WriteEnvmapResult(op->reports, scene, env, path, imtype, layout)) {
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}

}

static int envmap_save_exec(bContext *C, wmOperator *op)
{
	Tex *tex = CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data;
	Scene *scene = CTX_data_scene(C);
	//int imtype = RNA_enum_get(op->ptr, "file_type");
	char imtype = scene->r.im_format.imtype;
	char path[FILE_MAX];
	
	RNA_string_get(op->ptr, "filepath", path);
	
	if (scene->r.scemode & R_EXTENSION) {
		BKE_add_image_extension(path, &scene->r.im_format);
	}
	
	WM_cursor_wait(1);
	
	save_envmap(op, scene, tex->env, path, imtype);
	
	WM_cursor_wait(0);
	
	WM_event_add_notifier(C, NC_TEXTURE, tex);
	
	return OPERATOR_FINISHED;
}

static int envmap_save_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	//Scene *scene= CTX_data_scene(C);
	
	if (RNA_struct_property_is_set(op->ptr, "filepath"))
		return envmap_save_exec(C, op);

	//RNA_enum_set(op->ptr, "file_type", scene->r.im_format.imtype);
	RNA_string_set(op->ptr, "filepath", G.main->name);
	WM_event_add_fileselect(C, op);
	
	return OPERATOR_RUNNING_MODAL;
}

static int envmap_save_poll(bContext *C)
{
	Tex *tex = CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data;

	if (!tex) 
		return 0;
	if (!tex->env || !tex->env->ok)
		return 0;
	if (tex->env->cube[1] == NULL)
		return 0;
	
	return 1;
}

void TEXTURE_OT_envmap_save(wmOperatorType *ot)
{
	PropertyRNA *prop;
	/* identifiers */
	ot->name = "Save Environment Map";
	ot->idname = "TEXTURE_OT_envmap_save";
	ot->description = "Save the current generated Environment map to an image file";
	
	/* api callbacks */
	ot->exec = envmap_save_exec;
	ot->invoke = envmap_save_invoke;
	ot->poll = envmap_save_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER; /* no undo since this doesnt modify the env-map */
	
	/* properties */
	prop = RNA_def_float_array(ot->srna, "layout", 12, default_envmap_layout, 0.0f, 0.0f,
	                           "File layout",
	                           "Flat array describing the X,Y position of each cube face in the output image, "
	                           "where 1 is the size of a face - order is [+Z -Z +Y -X -Y +X] "
	                           "(use -1 to skip a face)", 0.0f, 0.0f);
	RNA_def_property_flag(prop, PROP_HIDDEN);

	WM_operator_properties_filesel(ot, FOLDERFILE | IMAGEFILE | MOVIEFILE, FILE_SPECIAL, FILE_SAVE,
	                               WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);
}

static int envmap_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
	Tex *tex = CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data;
	
	BKE_free_envmapdata(tex->env);
	
	WM_event_add_notifier(C, NC_TEXTURE | NA_EDITED, tex);
	
	return OPERATOR_FINISHED;
}

static int envmap_clear_poll(bContext *C)
{
	Tex *tex = CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data;
	
	if (!tex) 
		return 0;
	if (!tex->env || !tex->env->ok)
		return 0;
	if (tex->env->cube[1] == NULL)
		return 0;
	
	return 1;
}

void TEXTURE_OT_envmap_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Environment Map";
	ot->idname = "TEXTURE_OT_envmap_clear";
	ot->description = "Discard the environment map and free it from memory";
	
	/* api callbacks */
	ot->exec = envmap_clear_exec;
	ot->poll = envmap_clear_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int envmap_clear_all_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	Tex *tex;
	
	for (tex = bmain->tex.first; tex; tex = tex->id.next)
		if (tex->env)
			BKE_free_envmapdata(tex->env);
	
	WM_event_add_notifier(C, NC_TEXTURE | NA_EDITED, tex);
	
	return OPERATOR_FINISHED;
}

void TEXTURE_OT_envmap_clear_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear All Environment Maps";
	ot->idname = "TEXTURE_OT_envmap_clear_all";
	ot->description = "Discard all environment maps in the .blend file and free them from memory";
	
	/* api callbacks */
	ot->exec = envmap_clear_all_exec;
	ot->poll = envmap_clear_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** material operators *********************/

/* material copy/paste */
static int copy_material_exec(bContext *C, wmOperator *UNUSED(op))
{
	Material *ma = CTX_data_pointer_get_type(C, "material", &RNA_Material).data;

	if (ma == NULL)
		return OPERATOR_CANCELLED;

	copy_matcopybuf(ma);

	return OPERATOR_FINISHED;
}

void MATERIAL_OT_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy Material";
	ot->idname = "MATERIAL_OT_copy";
	ot->description = "Copy the material settings and nodes";

	/* api callbacks */
	ot->exec = copy_material_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER; /* no undo needed since no changes are made to the material */
}

static int paste_material_exec(bContext *C, wmOperator *UNUSED(op))
{
	Material *ma = CTX_data_pointer_get_type(C, "material", &RNA_Material).data;

	if (ma == NULL)
		return OPERATOR_CANCELLED;

	paste_matcopybuf(ma);

	WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_LINKS, ma);

	return OPERATOR_FINISHED;
}

void MATERIAL_OT_paste(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Paste Material";
	ot->idname = "MATERIAL_OT_paste";
	ot->description = "Paste the material settings and nodes";

	/* api callbacks */
	ot->exec = paste_material_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


static short mtexcopied = 0; /* must be reset on file load */
static MTex mtexcopybuf;

void ED_render_clear_mtex_copybuf(void)
{   /* use for file reload */
	mtexcopied = 0;
}

static void copy_mtex_copybuf(ID *id)
{
	MTex **mtex = NULL;
	
	switch (GS(id->name)) {
		case ID_MA:
			mtex = &(((Material *)id)->mtex[(int)((Material *)id)->texact]);
			break;
		case ID_LA:
			mtex = &(((Lamp *)id)->mtex[(int)((Lamp *)id)->texact]);
			// la->mtex[(int)la->texact] // TODO
			break;
		case ID_WO:
			mtex = &(((World *)id)->mtex[(int)((World *)id)->texact]);
			// mtex= wrld->mtex[(int)wrld->texact]; // TODO
			break;
		case ID_PA:
			mtex = &(((ParticleSettings *)id)->mtex[(int)((ParticleSettings *)id)->texact]);
			break;
	}
	
	if (mtex && *mtex) {
		memcpy(&mtexcopybuf, *mtex, sizeof(MTex));
		mtexcopied = 1;
	}
	else {
		mtexcopied = 0;
	}
}

static void paste_mtex_copybuf(ID *id)
{
	MTex **mtex = NULL;
	
	if (mtexcopied == 0 || mtexcopybuf.tex == NULL)
		return;
	
	switch (GS(id->name)) {
		case ID_MA:
			mtex = &(((Material *)id)->mtex[(int)((Material *)id)->texact]);
			break;
		case ID_LA:
			mtex = &(((Lamp *)id)->mtex[(int)((Lamp *)id)->texact]);
			// la->mtex[(int)la->texact] // TODO
			break;
		case ID_WO:
			mtex = &(((World *)id)->mtex[(int)((World *)id)->texact]);
			// mtex= wrld->mtex[(int)wrld->texact]; // TODO
			break;
		case ID_PA:
			mtex = &(((ParticleSettings *)id)->mtex[(int)((ParticleSettings *)id)->texact]);
			break;
		default:
			BLI_assert("invalid id type");
			return;
	}
	
	if (mtex) {
		if (*mtex == NULL) {
			*mtex = MEM_mallocN(sizeof(MTex), "mtex copy");
		}
		else if ((*mtex)->tex) {
			(*mtex)->tex->id.us--;
		}
		
		memcpy(*mtex, &mtexcopybuf, sizeof(MTex));
		
		id_us_plus((ID *)mtexcopybuf.tex);
	}
}


static int copy_mtex_exec(bContext *C, wmOperator *UNUSED(op))
{
	ID *id = CTX_data_pointer_get_type(C, "texture_slot", &RNA_TextureSlot).id.data;

	if (id == NULL) {
		/* copying empty slot */
		ED_render_clear_mtex_copybuf();
		return OPERATOR_CANCELLED;
	}

	copy_mtex_copybuf(id);

	return OPERATOR_FINISHED;
}

static int copy_mtex_poll(bContext *C)
{
	ID *id = CTX_data_pointer_get_type(C, "texture_slot", &RNA_TextureSlot).id.data;
	
	return (id != NULL);
}

void TEXTURE_OT_slot_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy Texture Slot Settings";
	ot->idname = "TEXTURE_OT_slot_copy";
	ot->description = "Copy the material texture settings and nodes";

	/* api callbacks */
	ot->exec = copy_mtex_exec;
	ot->poll = copy_mtex_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER; /* no undo needed since no changes are made to the mtex */
}

static int paste_mtex_exec(bContext *C, wmOperator *UNUSED(op))
{
	ID *id = CTX_data_pointer_get_type(C, "texture_slot", &RNA_TextureSlot).id.data;

	if (id == NULL) {
		Material *ma = CTX_data_pointer_get_type(C, "material", &RNA_Material).data;
		Lamp *la = CTX_data_pointer_get_type(C, "lamp", &RNA_Lamp).data;
		World *wo = CTX_data_pointer_get_type(C, "world", &RNA_World).data;
		ParticleSystem *psys = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem).data;
		
		if (ma)
			id = &ma->id;
		else if (la)
			id = &la->id;
		else if (wo)
			id = &wo->id;
		else if (psys)
			id = &psys->part->id;
		
		if (id == NULL)
			return OPERATOR_CANCELLED;
	}

	paste_mtex_copybuf(id);

	WM_event_add_notifier(C, NC_TEXTURE | ND_SHADING_LINKS, NULL);

	return OPERATOR_FINISHED;
}

void TEXTURE_OT_slot_paste(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Paste Texture Slot Settings";
	ot->idname = "TEXTURE_OT_slot_paste";
	ot->description = "Copy the texture settings and nodes";

	/* api callbacks */
	ot->exec = paste_mtex_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

