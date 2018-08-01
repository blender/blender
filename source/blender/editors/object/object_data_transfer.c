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
 * The Original Code is Copyright (C) 2014 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Bastien Montagne.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_data_transfer.c
 *  \ingroup edobj
 */

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_data_transfer.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_remap.h"
#include "BKE_mesh_runtime.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"

#include "UI_interface.h"

#include "object_intern.h"

/* All possible data to transfer.
 * Note some are 'fake' ones, i.e. they are not hold by real CDLayers. */
/* Not shared with modifier, since we use a usual enum here, not a multi-choice one. */
static const EnumPropertyItem DT_layer_items[] = {
	{0, "", 0, "Vertex Data", ""},
	{DT_TYPE_MDEFORMVERT, "VGROUP_WEIGHTS", 0, "Vertex Group(s)", "Transfer active or all vertex groups"},
#if 0  /* XXX For now, would like to finish/merge work from 2014 gsoc first. */
	{DT_TYPE_SHAPEKEY, "SHAPEKEYS", 0, "Shapekey(s)", "Transfer active or all shape keys"},
#endif
#if 0  /* XXX When SkinModifier is enabled, it seems to erase its own CD_MVERT_SKIN layer from final DM :( */
	{DT_TYPE_SKIN, "SKIN", 0, "Skin Weight", "Transfer skin weights"},
#endif
	{DT_TYPE_BWEIGHT_VERT, "BEVEL_WEIGHT_VERT", 0, "Bevel Weight", "Transfer bevel weights"},
	{0, "", 0, "Edge Data", ""},
	{DT_TYPE_SHARP_EDGE, "SHARP_EDGE", 0, "Sharp", "Transfer sharp mark"},
	{DT_TYPE_SEAM, "SEAM", 0, "UV Seam", "Transfer UV seam mark"},
	{DT_TYPE_CREASE, "CREASE", 0, "Subsurf Crease", "Transfer crease values"},
	{DT_TYPE_BWEIGHT_EDGE, "BEVEL_WEIGHT_EDGE", 0, "Bevel Weight", "Transfer bevel weights"},
	{DT_TYPE_FREESTYLE_EDGE, "FREESTYLE_EDGE", 0, "Freestyle Mark", "Transfer Freestyle edge mark"},
	{0, "", 0, "Face Corner Data", ""},
	{DT_TYPE_LNOR, "CUSTOM_NORMAL", 0, "Custom Normals", "Transfer custom normals"},
	{DT_TYPE_VCOL, "VCOL", 0, "VCol", "Vertex (face corners) colors"},
	{DT_TYPE_UV, "UV", 0, "UVs", "Transfer UV layers"},
	{0, "", 0, "Face Data", ""},
	{DT_TYPE_SHARP_FACE, "SMOOTH", 0, "Smooth", "Transfer flat/smooth mark"},
	{DT_TYPE_FREESTYLE_FACE, "FREESTYLE_FACE", 0, "Freestyle Mark", "Transfer Freestyle face mark"},
	{0, NULL, 0, NULL, NULL}
};

/* Note: rna_enum_dt_layers_select_src_items enum is from rna_modifier.c */
static const EnumPropertyItem *dt_layers_select_src_itemf(
        bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
	EnumPropertyItem *item = NULL, tmp_item = {0};
	int totitem = 0;
	const int data_type = RNA_enum_get(ptr, "data_type");

	if (!C) {  /* needed for docs and i18n tools */
		return rna_enum_dt_layers_select_src_items;
	}

	Depsgraph *depsgraph = CTX_data_depsgraph(C);

	RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_layers_select_src_items, DT_LAYERS_ACTIVE_SRC);
	RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_layers_select_src_items, DT_LAYERS_ALL_SRC);

	if (data_type == DT_TYPE_MDEFORMVERT) {
		Object *ob_src = CTX_data_active_object(C);

		if (BKE_object_pose_armature_get(ob_src)) {
			RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_layers_select_src_items, DT_LAYERS_VGROUP_SRC_BONE_SELECT);
			RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_layers_select_src_items, DT_LAYERS_VGROUP_SRC_BONE_DEFORM);
		}

		if (ob_src) {
			bDeformGroup *dg;
			int i;

			RNA_enum_item_add_separator(&item, &totitem);

			for (i = 0, dg = ob_src->defbase.first; dg; i++, dg = dg->next) {
				tmp_item.value = i;
				tmp_item.identifier = tmp_item.name = dg->name;
				RNA_enum_item_add(&item, &totitem, &tmp_item);
			}
		}
	}
	else if (data_type == DT_TYPE_SHAPEKEY) {
		/* TODO */
	}
	else if (data_type == DT_TYPE_UV) {
		Object *ob_src = CTX_data_active_object(C);
		Scene *scene = CTX_data_scene(C);

		if (ob_src) {
			Mesh *me_eval;
			int num_data, i;

			me_eval = mesh_get_eval_final(depsgraph, scene, ob_src, CD_MASK_BAREMESH | CD_MLOOPUV);
			num_data = CustomData_number_of_layers(&me_eval->ldata, CD_MLOOPUV);

			RNA_enum_item_add_separator(&item, &totitem);

			for (i = 0; i < num_data; i++) {
				tmp_item.value = i;
				tmp_item.identifier = tmp_item.name = CustomData_get_layer_name(&me_eval->ldata, CD_MLOOPUV, i);
				RNA_enum_item_add(&item, &totitem, &tmp_item);
			}
		}
	}
	else if (data_type == DT_TYPE_VCOL) {
		Object *ob_src = CTX_data_active_object(C);
		Scene *scene = CTX_data_scene(C);

		if (ob_src) {
			Mesh *me_eval;
			int num_data, i;

			me_eval = mesh_get_eval_final(depsgraph, scene, ob_src, CD_MASK_BAREMESH | CD_MLOOPCOL);
			num_data = CustomData_number_of_layers(&me_eval->ldata, CD_MLOOPCOL);

			RNA_enum_item_add_separator(&item, &totitem);

			for (i = 0; i < num_data; i++) {
				tmp_item.value = i;
				tmp_item.identifier = tmp_item.name = CustomData_get_layer_name(&me_eval->ldata, CD_MLOOPCOL, i);
				RNA_enum_item_add(&item, &totitem, &tmp_item);
			}
		}
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

/* Note: rna_enum_dt_layers_select_dst_items enum is from rna_modifier.c */
static const EnumPropertyItem *dt_layers_select_dst_itemf(
        bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
	EnumPropertyItem *item = NULL;
	int totitem = 0;

	const int layers_select_src = RNA_enum_get(ptr, "layers_select_src");

	if (!C) {  /* needed for docs and i18n tools */
		return rna_enum_dt_layers_select_dst_items;
	}

	if (layers_select_src == DT_LAYERS_ACTIVE_SRC || layers_select_src >= 0) {
		RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_layers_select_dst_items, DT_LAYERS_ACTIVE_DST);
	}
	RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_layers_select_dst_items, DT_LAYERS_NAME_DST);
	RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_layers_select_dst_items, DT_LAYERS_INDEX_DST);

	/* No 'specific' to-layers here, since we may transfer to several objects at once! */

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

static const EnumPropertyItem *dt_layers_select_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *prop, bool *r_free)
{
	const bool reverse_transfer = RNA_boolean_get(ptr, "use_reverse_transfer");

	if (STREQ(RNA_property_identifier(prop), "layers_select_dst")) {
		if (reverse_transfer) {
			return dt_layers_select_src_itemf(C, ptr, prop, r_free);
		}
		else {
			return dt_layers_select_dst_itemf(C, ptr, prop, r_free);
		}
	}
	else if (reverse_transfer) {
		return dt_layers_select_dst_itemf(C, ptr, prop, r_free);
	}
	else {
		return dt_layers_select_src_itemf(C, ptr, prop, r_free);
	}
}

/* Note: rna_enum_dt_mix_mode_items enum is from rna_modifier.c */
static const EnumPropertyItem *dt_mix_mode_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
	EnumPropertyItem *item = NULL;
	int totitem = 0;

	const int dtdata_type = RNA_enum_get(ptr, "data_type");
	bool support_advanced_mixing, support_threshold;

	if (!C) {  /* needed for docs and i18n tools */
		return rna_enum_dt_mix_mode_items;
	}

	RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_TRANSFER);

	BKE_object_data_transfer_get_dttypes_capacity(dtdata_type, &support_advanced_mixing, &support_threshold);

	if (support_threshold) {
		RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_REPLACE_ABOVE_THRESHOLD);
		RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_REPLACE_BELOW_THRESHOLD);
	}

	if (support_advanced_mixing) {
		RNA_enum_item_add_separator(&item, &totitem);
		RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_MIX);
		RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_ADD);
		RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_SUB);
		RNA_enum_items_add_value(&item, &totitem, rna_enum_dt_mix_mode_items, CDT_MIX_MUL);
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

static bool data_transfer_check(bContext *UNUSED(C), wmOperator *op)
{
	const int layers_select_src = RNA_enum_get(op->ptr, "layers_select_src");
	PropertyRNA *prop = RNA_struct_find_property(op->ptr, "layers_select_dst");
	const int layers_select_dst = RNA_property_enum_get(op->ptr, prop);

	/* TODO: check for invalid layers_src select modes too! */

	if ((layers_select_src != DT_LAYERS_ACTIVE_SRC) && (layers_select_dst == DT_LAYERS_ACTIVE_DST)) {
		RNA_property_enum_set(op->ptr, prop, DT_LAYERS_NAME_DST);
		return true;
	}

	return false;
}

/* Helper, used by both data_transfer_exec and datalayout_transfer_exec. */
static void data_transfer_exec_preprocess_objects(
        bContext *C, wmOperator *op, Object *ob_src, ListBase *ctx_objects, const bool reverse_transfer)
{
	CollectionPointerLink *ctx_ob;
	CTX_data_selected_editable_objects(C, ctx_objects);

	if (reverse_transfer) {
		return;  /* Nothing else to do in this case... */
	}

	for (ctx_ob = ctx_objects->first; ctx_ob; ctx_ob = ctx_ob->next) {
		Object *ob = ctx_ob->ptr.data;
		Mesh *me;
		if ((ob == ob_src) || (ob->type != OB_MESH)) {
			continue;
		}

		me = ob->data;
		if (ID_IS_LINKED(me)) {
			/* Do not transfer to linked data, not supported. */
			BKE_reportf(op->reports, RPT_WARNING, "Skipping object '%s', linked data '%s' cannot be modified",
			            ob->id.name + 2, me->id.name + 2);
			me->id.tag &= ~LIB_TAG_DOIT;
			continue;
		}

		me->id.tag |= LIB_TAG_DOIT;
	}
}

/* Helper, used by both data_transfer_exec and datalayout_transfer_exec. */
static bool data_transfer_exec_is_object_valid(
        wmOperator *op, Object *ob_src, Object *ob_dst, const bool reverse_transfer)
{
	Mesh *me;
	if ((ob_dst == ob_src) || (ob_src->type != OB_MESH) || (ob_dst->type != OB_MESH)) {
		return false;
	}

	if (reverse_transfer) {
		return true;
	}

	me = ob_dst->data;
	if (me->id.tag & LIB_TAG_DOIT) {
		me->id.tag &= ~LIB_TAG_DOIT;
		return true;
	}
	else if (!ID_IS_LINKED(me)) {
		/* Do not transfer apply operation more than once. */
		/* XXX This is not nice regarding vgroups, which are half-Object data... :/ */
		BKE_reportf(op->reports, RPT_WARNING,
		            "Skipping object '%s', data '%s' has already been processed with a previous object",
		            ob_dst->id.name + 2, me->id.name + 2);
	}
	return false;
}

static int data_transfer_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob_src = ED_object_active_context(C);
	Depsgraph *depsgraph = CTX_data_depsgraph(C);

	ListBase ctx_objects;
	CollectionPointerLink *ctx_ob_dst;

	bool changed = false;

	const bool is_frozen = RNA_boolean_get(op->ptr, "use_freeze");

	const bool reverse_transfer = RNA_boolean_get(op->ptr, "use_reverse_transfer");

	const int data_type = RNA_enum_get(op->ptr, "data_type");
	const bool use_create = RNA_boolean_get(op->ptr, "use_create");

	const int map_vert_mode = RNA_enum_get(op->ptr, "vert_mapping");
	const int map_edge_mode = RNA_enum_get(op->ptr, "edge_mapping");
	const int map_loop_mode = RNA_enum_get(op->ptr, "loop_mapping");
	const int map_poly_mode = RNA_enum_get(op->ptr, "poly_mapping");

	const bool use_auto_transform = RNA_boolean_get(op->ptr, "use_auto_transform");
	const bool use_object_transform = RNA_boolean_get(op->ptr, "use_object_transform");
	const bool use_max_distance = RNA_boolean_get(op->ptr, "use_max_distance");
	const float max_distance = use_max_distance ? RNA_float_get(op->ptr, "max_distance") : FLT_MAX;
	const float ray_radius = RNA_float_get(op->ptr, "ray_radius");
	const float islands_precision = RNA_float_get(op->ptr, "islands_precision");

	const int layers_src = RNA_enum_get(op->ptr, "layers_select_src");
	const int layers_dst = RNA_enum_get(op->ptr, "layers_select_dst");
	int layers_select_src[DT_MULTILAYER_INDEX_MAX] = {0};
	int layers_select_dst[DT_MULTILAYER_INDEX_MAX] = {0};
	const int fromto_idx = BKE_object_data_transfer_dttype_to_srcdst_index(data_type);

	const int mix_mode = RNA_enum_get(op->ptr, "mix_mode");
	const float mix_factor = RNA_float_get(op->ptr, "mix_factor");

	SpaceTransform space_transform_data;
	SpaceTransform *space_transform = (use_object_transform && !use_auto_transform) ? &space_transform_data : NULL;

	if (is_frozen) {
		BKE_report(op->reports, RPT_INFO,
		           "Operator is frozen, changes to its settings won't take effect until you unfreeze it");
		return OPERATOR_FINISHED;
	}

	if (reverse_transfer && ID_IS_LINKED(ob_src->data)) {
		/* Do not transfer to linked data, not supported. */
		return OPERATOR_CANCELLED;
	}

	if (fromto_idx != DT_MULTILAYER_INDEX_INVALID) {
		layers_select_src[fromto_idx] = layers_src;
		layers_select_dst[fromto_idx] = layers_dst;
	}

	data_transfer_exec_preprocess_objects(C, op, ob_src, &ctx_objects, reverse_transfer);

	for (ctx_ob_dst = ctx_objects.first; ctx_ob_dst; ctx_ob_dst = ctx_ob_dst->next) {
		Object *ob_dst = ctx_ob_dst->ptr.data;

		if (reverse_transfer) {
			SWAP(Object *, ob_src, ob_dst);
		}

		if (data_transfer_exec_is_object_valid(op, ob_src, ob_dst, reverse_transfer)) {
			if (space_transform) {
				BLI_SPACE_TRANSFORM_SETUP(space_transform, ob_dst, ob_src);
			}

			if (BKE_object_data_transfer_mesh(
			        depsgraph, scene, ob_src, ob_dst, data_type, use_create,
			        map_vert_mode, map_edge_mode, map_loop_mode, map_poly_mode,
			        space_transform, use_auto_transform,
			        max_distance, ray_radius, islands_precision,
			        layers_select_src, layers_select_dst,
			        mix_mode, mix_factor, NULL, false, op->reports))
			{
				changed = true;
			}
		}

		DEG_id_tag_update(&ob_dst->id, OB_RECALC_DATA);

		if (reverse_transfer) {
			SWAP(Object *, ob_src, ob_dst);
		}
	}

	BLI_freelistN(&ctx_objects);

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, NULL);

#if 0  /* TODO */
	/* Note: issue with that is that if canceled, operator cannot be redone... Nasty in our case. */
	return changed ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
#else
	(void)changed;
	return OPERATOR_FINISHED;
#endif
}

/* Used by both OBJECT_OT_data_transfer and OBJECT_OT_datalayout_transfer */
/* Note this context poll is only really partial, it cannot check for all possible invalid cases. */
static bool data_transfer_poll(bContext *C)
{
	Object *ob = ED_object_active_context(C);
	ID *data = (ob) ? ob->data : NULL;
	return (ob && ob->type == OB_MESH && data);
}

/* Used by both OBJECT_OT_data_transfer and OBJECT_OT_datalayout_transfer */
static bool data_transfer_poll_property(const bContext *UNUSED(C), wmOperator *op, const PropertyRNA *prop)
{
	PointerRNA *ptr = op->ptr;
	PropertyRNA *prop_other;

	const char *prop_id = RNA_property_identifier(prop);
	const int data_type = RNA_enum_get(ptr, "data_type");
	bool use_auto_transform = false;
	bool use_max_distance = false;
	bool use_modifier = false;

	if ((prop_other = RNA_struct_find_property(ptr, "use_auto_transform"))) {
		use_auto_transform = RNA_property_boolean_get(ptr, prop_other);
	}
	if ((prop_other = RNA_struct_find_property(ptr, "use_max_distance"))) {
		use_max_distance = RNA_property_boolean_get(ptr, prop_other);
	}
	if ((prop_other = RNA_struct_find_property(ptr, "modifier"))) {
		use_modifier = RNA_property_is_set(ptr, prop_other);
	}

	if (STREQ(prop_id, "modifier")) {
		return use_modifier;
	}

	if (use_modifier) {
		/* Hide everything but 'modifier' property, if set. */
		return false;
	}

	if (STREQ(prop_id, "use_object_transform") && use_auto_transform) {
		return false;
	}
	if (STREQ(prop_id, "max_distance") && !use_max_distance) {
		return false;
	}
	if (STREQ(prop_id, "islands_precision") && !DT_DATATYPE_IS_LOOP(data_type)) {
		return false;
	}

	if (STREQ(prop_id, "vert_mapping") && !DT_DATATYPE_IS_VERT(data_type)) {
		return false;
	}
	if (STREQ(prop_id, "edge_mapping") && !DT_DATATYPE_IS_EDGE(data_type)) {
		return false;
	}
	if (STREQ(prop_id, "loop_mapping") && !DT_DATATYPE_IS_LOOP(data_type)) {
		return false;
	}
	if (STREQ(prop_id, "poly_mapping") && !DT_DATATYPE_IS_POLY(data_type)) {
		return false;
	}

	if ((STREQ(prop_id, "layers_select_src") || STREQ(prop_id, "layers_select_dst")) &&
	    !DT_DATATYPE_IS_MULTILAYERS(data_type))
	{
		return false;
	}

	/* Else, show it! */
	return true;
}

/* transfers weight from active to selected */
void OBJECT_OT_data_transfer(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* Identifiers.*/
	ot->name = "Transfer Mesh Data";
	ot->idname = "OBJECT_OT_data_transfer";
	ot->description = "Transfer data layer(s) (weights, edge sharp, ...) from active to selected meshes";

	/* API callbacks.*/
	ot->poll = data_transfer_poll;
	ot->poll_property = data_transfer_poll_property;
	ot->invoke = WM_menu_invoke;
	ot->exec = data_transfer_exec;
	ot->check = data_transfer_check;

	/* Flags.*/
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* Properties.*/
	prop = RNA_def_boolean(ot->srna, "use_reverse_transfer", false, "Reverse Transfer",
	                       "Transfer from selected objects to active one");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

	RNA_def_boolean(ot->srna, "use_freeze", false, "Freeze Operator",
	                "Prevent changes to settings to re-run the operator, "
	                "handy to change several things at once with heavy geometry");

	/* Data type to transfer. */
	ot->prop = RNA_def_enum(ot->srna, "data_type", DT_layer_items, 0, "Data Type", "Which data to transfer");
	RNA_def_boolean(ot->srna, "use_create", true, "Create Data", "Add data layers on destination meshes if needed");

	/* Mapping methods. */
	RNA_def_enum(ot->srna, "vert_mapping", rna_enum_dt_method_vertex_items, MREMAP_MODE_VERT_NEAREST, "Vertex Mapping",
	             "Method used to map source vertices to destination ones");
	RNA_def_enum(ot->srna, "edge_mapping", rna_enum_dt_method_edge_items, MREMAP_MODE_EDGE_NEAREST, "Edge Mapping",
	             "Method used to map source edges to destination ones");
	RNA_def_enum(ot->srna, "loop_mapping", rna_enum_dt_method_loop_items, MREMAP_MODE_LOOP_NEAREST_POLYNOR,
	             "Face Corner Mapping", "Method used to map source faces' corners to destination ones");
	RNA_def_enum(ot->srna, "poly_mapping", rna_enum_dt_method_poly_items, MREMAP_MODE_POLY_NEAREST, "Face Mapping",
	             "Method used to map source faces to destination ones");

	/* Mapping options and filtering. */
	RNA_def_boolean(ot->srna, "use_auto_transform", false, "Auto Transform",
	                "Automatically compute transformation to get the best possible match between source and "
	                "destination meshes (WARNING: results will never be as good as manual matching of objects)");
	RNA_def_boolean(ot->srna, "use_object_transform", true, "Object Transform",
	                "Evaluate source and destination meshes in global space");
	RNA_def_boolean(ot->srna, "use_max_distance", false, "Only Neighbor Geometry",
	                "Source elements must be closer than given distance from destination one");
	prop = RNA_def_float(ot->srna, "max_distance", 1.0f, 0.0f, FLT_MAX, "Max Distance",
	                     "Maximum allowed distance between source and destination element, for non-topology mappings",
	                     0.0f, 100.0f);
	RNA_def_property_subtype(prop, PROP_DISTANCE);
	prop = RNA_def_float(ot->srna, "ray_radius", 0.0f, 0.0f, FLT_MAX, "Ray Radius",
	                     "'Width' of rays (especially useful when raycasting against vertices or edges)",
	                     0.0f, 10.0f);
	RNA_def_property_subtype(prop, PROP_DISTANCE);
	prop = RNA_def_float(ot->srna, "islands_precision", 0.1f, 0.0f, 10.0f, "Islands Precision",
	                     "Factor controlling precision of islands handling (the higher, the better the results)",
	                     0.0f, 1.0f);
	RNA_def_property_subtype(prop, PROP_FACTOR);

	/* How to handle multi-layers types of data. */
	prop = RNA_def_enum(ot->srna, "layers_select_src", rna_enum_dt_layers_select_src_items, DT_LAYERS_ACTIVE_SRC,
	                    "Source Layers Selection", "Which layers to transfer, in case of multi-layers types");
	RNA_def_property_enum_funcs_runtime(prop, NULL, NULL, dt_layers_select_itemf);

	prop = RNA_def_enum(ot->srna, "layers_select_dst", rna_enum_dt_layers_select_dst_items, DT_LAYERS_ACTIVE_DST,
	                    "Destination Layers Matching", "How to match source and destination layers");
	RNA_def_property_enum_funcs_runtime(prop, NULL, NULL, dt_layers_select_itemf);

	prop = RNA_def_enum(ot->srna, "mix_mode", rna_enum_dt_mix_mode_items, CDT_MIX_TRANSFER, "Mix Mode",
	                   "How to affect destination elements with source values");
	RNA_def_property_enum_funcs_runtime(prop, NULL, NULL, dt_mix_mode_itemf);
	RNA_def_float(ot->srna, "mix_factor", 1.0f, 0.0f, 1.0f, "Mix Factor",
	              "Factor to use when applying data to destination (exact behavior depends on mix mode)", 0.0f, 1.0f);
}

/******************************************************************************/
/* Note: This operator is hybrid, it can work as a usual standalone Object operator,
 *       or as a DataTransfer modifier tool.
 */

static bool datalayout_transfer_poll(bContext *C)
{
	return (edit_modifier_poll_generic(C, &RNA_DataTransferModifier, (1 << OB_MESH)) || data_transfer_poll(C));
}

static int datalayout_transfer_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob_act = ED_object_active_context(C);
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	DataTransferModifierData *dtmd;

	dtmd = (DataTransferModifierData *)edit_modifier_property_get(op, ob_act, eModifierType_DataTransfer);

	/* If we have a modifier, we transfer data layout from this modifier's source object to active one.
	 * Else, we transfer data layout from active object to all selected ones. */
	if (dtmd) {
		Object *ob_src = dtmd->ob_source;
		Object *ob_dst = ob_act;

		const bool use_delete = false;  /* Never when used from modifier, for now. */

		if (!ob_src) {
			return OPERATOR_CANCELLED;
		}

		BKE_object_data_transfer_layout(depsgraph, scene, ob_src, ob_dst, dtmd->data_types, use_delete,
		                                dtmd->layers_select_src, dtmd->layers_select_dst);

		DEG_id_tag_update(&ob_dst->id, OB_RECALC_DATA);
	}
	else {
		Object *ob_src = ob_act;

		ListBase ctx_objects;
		CollectionPointerLink *ctx_ob_dst;

		const int data_type = RNA_enum_get(op->ptr, "data_type");
		const bool use_delete = RNA_boolean_get(op->ptr, "use_delete");

		const int layers_src = RNA_enum_get(op->ptr, "layers_select_src");
		const int layers_dst = RNA_enum_get(op->ptr, "layers_select_dst");
		int layers_select_src[DT_MULTILAYER_INDEX_MAX] = {0};
		int layers_select_dst[DT_MULTILAYER_INDEX_MAX] = {0};
		const int fromto_idx = BKE_object_data_transfer_dttype_to_srcdst_index(data_type);

		if (fromto_idx != DT_MULTILAYER_INDEX_INVALID) {
			layers_select_src[fromto_idx] = layers_src;
			layers_select_dst[fromto_idx] = layers_dst;
		}

		data_transfer_exec_preprocess_objects(C, op, ob_src, &ctx_objects, false);

		for (ctx_ob_dst = ctx_objects.first; ctx_ob_dst; ctx_ob_dst = ctx_ob_dst->next) {
			Object *ob_dst = ctx_ob_dst->ptr.data;
			if (data_transfer_exec_is_object_valid(op, ob_src, ob_dst, false)) {
				BKE_object_data_transfer_layout(depsgraph, scene, ob_src, ob_dst, data_type, use_delete,
				                                layers_select_src, layers_select_dst);
			}

			DEG_id_tag_update(&ob_dst->id, OB_RECALC_DATA);
		}

		BLI_freelistN(&ctx_objects);
	}

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, NULL);

	return OPERATOR_FINISHED;
}

static int datalayout_transfer_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	if (edit_modifier_invoke_properties(C, op)) {
		return datalayout_transfer_exec(C, op);
	}
	else {
		return WM_menu_invoke(C, op, event);
	}
}

void OBJECT_OT_datalayout_transfer(wmOperatorType *ot)
{
	PropertyRNA *prop;

	ot->name = "Transfer Mesh Data Layout";
	ot->description = "Transfer layout of data layer(s) from active to selected meshes";
	ot->idname = "OBJECT_OT_datalayout_transfer";

	ot->poll = datalayout_transfer_poll;
	ot->poll_property = data_transfer_poll_property;
	ot->invoke = datalayout_transfer_invoke;
	ot->exec = datalayout_transfer_exec;
	ot->check = data_transfer_check;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* Properties.*/
	edit_modifier_properties(ot);

	/* Data type to transfer. */
	ot->prop = RNA_def_enum(ot->srna, "data_type", DT_layer_items, 0, "Data Type", "Which data to transfer");
	RNA_def_boolean(ot->srna, "use_delete", false, "Exact Match",
	                "Also delete some data layers from destination if necessary, so that it matches exactly source");

	/* How to handle multi-layers types of data. */
	prop = RNA_def_enum(ot->srna, "layers_select_src", rna_enum_dt_layers_select_src_items, DT_LAYERS_ACTIVE_SRC,
	                    "Source Layers Selection", "Which layers to transfer, in case of multi-layers types");
	RNA_def_property_enum_funcs_runtime(prop, NULL, NULL, dt_layers_select_src_itemf);

	prop = RNA_def_enum(ot->srna, "layers_select_dst", rna_enum_dt_layers_select_dst_items, DT_LAYERS_ACTIVE_DST,
	                    "Destination Layers Matching", "How to match source and destination layers");
	RNA_def_property_enum_funcs_runtime(prop, NULL, NULL, dt_layers_select_dst_itemf);
}
