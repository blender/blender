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
 * Contributor(s): Blender Foundation (2008), Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_armature.c
 *  \ingroup RNA
 */


#include <stdlib.h>

#include "BLI_math.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "WM_api.h"
#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_idprop.h"
#include "BKE_main.h"

#include "ED_armature.h"
#include "BKE_armature.h"

static void rna_Armature_update_data(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	ID *id = ptr->id.data;

	DAG_id_tag_update(id, 0);
	WM_main_add_notifier(NC_GEOM | ND_DATA, id);
	/*WM_main_add_notifier(NC_OBJECT|ND_POSE, NULL); */
}


static void rna_Armature_act_bone_set(PointerRNA *ptr, PointerRNA value)
{
	bArmature *arm = (bArmature *)ptr->data;

	if (value.id.data == NULL && value.data == NULL) {
		arm->act_bone = NULL;
	}
	else {
		if (value.id.data != arm) {
			Object *ob = (Object *)value.id.data;
			
			if (GS(ob->id.name) != ID_OB || (ob->data != arm)) {
				printf("ERROR: armature set active bone - new active doesn't come from this armature\n");
				return;
			}
		}
		
		arm->act_bone = value.data;
		arm->act_bone->flag |= BONE_SELECTED;
	}
}

static void rna_Armature_act_edit_bone_set(PointerRNA *ptr, PointerRNA value)
{
	bArmature *arm = (bArmature *)ptr->data;

	if (value.id.data == NULL && value.data == NULL) {
		arm->act_edbone = NULL;
	}
	else {
		if (value.id.data != arm) {
			/* raise an error! */
		}
		else {
			arm->act_edbone = value.data;
			((EditBone *)arm->act_edbone)->flag |= BONE_SELECTED;
		}
	}
}

static EditBone *rna_Armature_edit_bone_new(bArmature *arm, ReportList *reports, const char *name)
{
	if (arm->edbo == NULL) {
		BKE_reportf(reports, RPT_ERROR, "Armature '%s' not in edit mode, cannot add an editbone", arm->id.name + 2);
		return NULL;
	}
	return ED_armature_edit_bone_add(arm, name);
}

static void rna_Armature_edit_bone_remove(bArmature *arm, ReportList *reports, PointerRNA *ebone_ptr)
{
	EditBone *ebone = ebone_ptr->data;
	if (arm->edbo == NULL) {
		BKE_reportf(reports, RPT_ERROR, "Armature '%s' not in edit mode, cannot remove an editbone", arm->id.name + 2);
		return;
	}

	if (BLI_findindex(arm->edbo, ebone) == -1) {
		BKE_reportf(reports, RPT_ERROR, "Armature '%s' does not contain bone '%s'", arm->id.name + 2, ebone->name);
		return;
	}

	ED_armature_edit_bone_remove(arm, ebone);
	RNA_POINTER_INVALIDATE(ebone_ptr);
}

static void rna_Armature_update_layers(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
	bArmature *arm = ptr->id.data;
	Object *ob;

	/* proxy lib exception, store it here so we can restore layers on file
	 * load, since it would otherwise get lost due to being linked data */
	for (ob = bmain->object.first; ob; ob = ob->id.next) {
		if (ob->data == arm && ob->pose)
			ob->pose->proxy_layer = arm->layer;
	}

	WM_main_add_notifier(NC_GEOM | ND_DATA, arm);
}

static void rna_Armature_redraw_data(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	ID *id = ptr->id.data;
	
	WM_main_add_notifier(NC_GEOM | ND_DATA, id);
}

/* called whenever a bone is renamed */
static void rna_Bone_update_renamed(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	ID *id = ptr->id.data;
	
	/* redraw view */
	WM_main_add_notifier(NC_GEOM | ND_DATA, id);
	
	/* update animation channels */
	WM_main_add_notifier(NC_ANIMATION | ND_ANIMCHAN, id);
}

static void rna_Bone_select_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	ID *id = ptr->id.data;
	
	/* special updates for cases where rigs try to hook into armature drawing stuff 
	 * e.g. Mask Modifier - 'Armature' option
	 */
	if (id) {
		if (GS(id->name) == ID_AR) {
			bArmature *arm = (bArmature *)id;
			
			if (arm->flag & ARM_HAS_VIZ_DEPS) {
				DAG_id_tag_update(id, OB_RECALC_DATA);
			}
		}
		else if (GS(id->name) == ID_OB) {
			Object *ob = (Object *)id;
			bArmature *arm = (bArmature *)ob->data;
			
			if (arm->flag & ARM_HAS_VIZ_DEPS) {
				DAG_id_tag_update(id, OB_RECALC_DATA);
			}
		}
	}
	
	WM_main_add_notifier(NC_GEOM | ND_DATA, id);

	/* spaces that show animation data of the selected bone need updating */
	WM_main_add_notifier(NC_ANIMATION | ND_ANIMCHAN, id);
}

static char *rna_Bone_path(PointerRNA *ptr)
{
	ID *id = ptr->id.data;
	Bone *bone = (Bone *)ptr->data;
	char name_esc[sizeof(bone->name) * 2];
	
	BLI_strescape(name_esc, bone->name, sizeof(name_esc));

	/* special exception for trying to get the path where ID-block is Object
	 * - this will be assumed to be from a Pose Bone...
	 */
	if (id) {
		if (GS(id->name) == ID_OB) {
			return BLI_sprintfN("pose.bones[\"%s\"].bone", name_esc);
		}
	}
	
	/* from armature... */
	return BLI_sprintfN("bones[\"%s\"]", name_esc);
}

static IDProperty *rna_Bone_idprops(PointerRNA *ptr, bool create)
{
	Bone *bone = ptr->data;

	if (create && !bone->prop) {
		IDPropertyTemplate val = {0};
		bone->prop = IDP_New(IDP_GROUP, &val, "RNA_Bone ID properties");
	}

	return bone->prop;
}

static IDProperty *rna_EditBone_idprops(PointerRNA *ptr, bool create)
{
	EditBone *ebone = ptr->data;

	if (create && !ebone->prop) {
		IDPropertyTemplate val = {0};
		ebone->prop = IDP_New(IDP_GROUP, &val, "RNA_EditBone ID properties");
	}

	return ebone->prop;
}

static void rna_bone_layer_set(int *layer, const int *values)
{
	int i, tot = 0;

	/* ensure we always have some layer selected */
	for (i = 0; i < 32; i++)
		if (values[i])
			tot++;
	
	if (tot == 0)
		return;

	for (i = 0; i < 32; i++) {
		if (values[i]) *layer |= (1u << i);
		else *layer &= ~(1u << i);
	}
}

static void rna_Bone_layer_set(PointerRNA *ptr, const int *values)
{
	Bone *bone = (Bone *)ptr->data;
	rna_bone_layer_set(&bone->layer, values);
}

static void rna_Armature_layer_set(PointerRNA *ptr, const int *values)
{
	bArmature *arm = (bArmature *)ptr->data;
	int i, tot = 0;

	/* ensure we always have some layer selected */
	for (i = 0; i < 32; i++)
		if (values[i])
			tot++;
	
	if (tot == 0)
		return;

	for (i = 0; i < 32; i++) {
		if (values[i]) arm->layer |= (1u << i);
		else arm->layer &= ~(1u << i);
	}
}

/* XXX deprecated.... old armature only animviz */
static void rna_Armature_ghost_start_frame_set(PointerRNA *ptr, int value)
{
	bArmature *data = (bArmature *)ptr->data;
	CLAMP(value, 1, (int)(MAXFRAMEF / 2));
	data->ghostsf = value;

	if (data->ghostsf >= data->ghostef) {
		data->ghostef = MIN2(data->ghostsf, (int)(MAXFRAMEF / 2));
	}
}

static void rna_Armature_ghost_end_frame_set(PointerRNA *ptr, int value)
{
	bArmature *data = (bArmature *)ptr->data;
	CLAMP(value, 1, (int)(MAXFRAMEF / 2));
	data->ghostef = value;

	if (data->ghostsf >= data->ghostef) {
		data->ghostsf = MAX2(data->ghostef, 1);
	}
}
/* XXX deprecated... old armature only animviz */

static void rna_EditBone_name_set(PointerRNA *ptr, const char *value)
{
	bArmature *arm = (bArmature *)ptr->id.data;
	EditBone *ebone = (EditBone *)ptr->data;
	char oldname[sizeof(ebone->name)], newname[sizeof(ebone->name)];
	
	/* need to be on the stack */
	BLI_strncpy_utf8(newname, value, sizeof(ebone->name));
	BLI_strncpy(oldname, ebone->name, sizeof(ebone->name));
	
	ED_armature_bone_rename(arm, oldname, newname);
}

static void rna_Bone_name_set(PointerRNA *ptr, const char *value)
{
	bArmature *arm = (bArmature *)ptr->id.data;
	Bone *bone = (Bone *)ptr->data;
	char oldname[sizeof(bone->name)], newname[sizeof(bone->name)];
	
	/* need to be on the stack */
	BLI_strncpy_utf8(newname, value, sizeof(bone->name));
	BLI_strncpy(oldname, bone->name, sizeof(bone->name));

	ED_armature_bone_rename(arm, oldname, newname);
}

static void rna_EditBone_layer_set(PointerRNA *ptr, const int values[])
{
	EditBone *data = (EditBone *)(ptr->data);
	rna_bone_layer_set(&data->layer, values);
}

static void rna_EditBone_connected_check(EditBone *ebone)
{
	if (ebone->parent) {
		if (ebone->flag & BONE_CONNECTED) {
			/* Attach this bone to its parent */
			copy_v3_v3(ebone->head, ebone->parent->tail);

			if (ebone->flag & BONE_ROOTSEL)
				ebone->parent->flag |= BONE_TIPSEL;
		}
		else if (!(ebone->parent->flag & BONE_ROOTSEL)) {
			ebone->parent->flag &= ~BONE_TIPSEL;
		}
	}
}

static void rna_EditBone_connected_set(PointerRNA *ptr, int value)
{
	EditBone *ebone = (EditBone *)(ptr->data);

	if (value) ebone->flag |= BONE_CONNECTED;
	else ebone->flag &= ~BONE_CONNECTED;

	rna_EditBone_connected_check(ebone);
}

static PointerRNA rna_EditBone_parent_get(PointerRNA *ptr)
{
	EditBone *data = (EditBone *)(ptr->data);
	return rna_pointer_inherit_refine(ptr, &RNA_EditBone, data->parent);
}

static void rna_EditBone_parent_set(PointerRNA *ptr, PointerRNA value)
{
	EditBone *ebone = (EditBone *)(ptr->data);
	EditBone *pbone, *parbone = (EditBone *)value.data;

	if (parbone == NULL) {
		if (ebone->parent && !(ebone->parent->flag & BONE_ROOTSEL))
			ebone->parent->flag &= ~BONE_TIPSEL;

		ebone->parent = NULL;
		ebone->flag &= ~BONE_CONNECTED;
	}
	else {
		/* within same armature */
		if (value.id.data != ptr->id.data)
			return;

		/* make sure this is a valid child */
		if (parbone == ebone)
			return;
			
		for (pbone = parbone->parent; pbone; pbone = pbone->parent)
			if (pbone == ebone)
				return;

		ebone->parent = parbone;
		rna_EditBone_connected_check(ebone);
	}
}

static void rna_EditBone_matrix_get(PointerRNA *ptr, float *values)
{
	EditBone *ebone = (EditBone *)(ptr->data);
	ED_armature_ebone_to_mat4(ebone, (float(*)[4])values);
}

static void rna_EditBone_matrix_set(PointerRNA *ptr, const float *values)
{
	EditBone *ebone = (EditBone *)(ptr->data);
	ED_armature_ebone_from_mat4(ebone, (float(*)[4])values);
}

static void rna_Armature_editbone_transform_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	bArmature *arm = (bArmature *)ptr->id.data;
	EditBone *ebone = (EditBone *)ptr->data;
	EditBone *child, *eboflip;
	
	/* update our parent */
	if (ebone->parent && ebone->flag & BONE_CONNECTED)
		copy_v3_v3(ebone->parent->tail, ebone->head);

	/* update our children if necessary */
	for (child = arm->edbo->first; child; child = child->next)
		if (child->parent == ebone && (child->flag & BONE_CONNECTED))
			copy_v3_v3(child->head, ebone->tail);

	if (arm->flag & ARM_MIRROR_EDIT) {
		eboflip = ED_armature_bone_get_mirrored(arm->edbo, ebone);

		if (eboflip) {
			eboflip->roll = -ebone->roll;

			eboflip->head[0] = -ebone->head[0];
			eboflip->tail[0] = -ebone->tail[0];
			
			/* update our parent */
			if (eboflip->parent && eboflip->flag & BONE_CONNECTED)
				copy_v3_v3(eboflip->parent->tail, eboflip->head);
			
			/* update our children if necessary */
			for (child = arm->edbo->first; child; child = child->next)
				if (child->parent == eboflip && (child->flag & BONE_CONNECTED))
					copy_v3_v3(child->head, eboflip->tail);
		}
	}

	rna_Armature_update_data(bmain, scene, ptr);
}

static void rna_Armature_bones_next(CollectionPropertyIterator *iter)
{
	ListBaseIterator *internal = &iter->internal.listbase;
	Bone *bone = (Bone *)internal->link;

	if (bone->childbase.first)
		internal->link = (Link *)bone->childbase.first;
	else if (bone->next)
		internal->link = (Link *)bone->next;
	else {
		internal->link = NULL;

		do {
			bone = bone->parent;
			if (bone && bone->next) {
				internal->link = (Link *)bone->next;
				break;
			}
		} while (bone);
	}

	iter->valid = (internal->link != NULL);
}

static int rna_Armature_is_editmode_get(PointerRNA *ptr)
{
	bArmature *arm = (bArmature *)ptr->id.data;
	return (arm->edbo != NULL);
}

static void rna_Armature_transform(struct bArmature *arm, float *mat)
{
	ED_armature_transform(arm, (float (*)[4])mat);
}

#else

/* Settings for curved bbone settings - The posemode values get applied over the top of the editmode ones */
void rna_def_bone_curved_common(StructRNA *srna, bool is_posebone)
{
#define RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone)                                \
	{                                                                              \
		if (is_posebone)                                                           \
			RNA_def_property_update(prop, NC_OBJECT | ND_POSE, "rna_Pose_update"); \
		else                                                                       \
			RNA_def_property_update(prop, 0, "rna_Armature_update_data");          \
	} (void)0;
	
	PropertyRNA *prop;
	
	/* Roll In/Out */
	prop = RNA_def_property(srna, "bbone_rollin", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "roll1");
	RNA_def_property_range(prop, -M_PI * 2.0, M_PI * 2.0);
	RNA_def_property_ui_text(prop, "Roll In", "Roll offset for the start of the B-Bone, adjusts twist");
	RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone);
	
	prop = RNA_def_property(srna, "bbone_rollout", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "roll2");
	RNA_def_property_range(prop, -M_PI * 2.0, M_PI * 2.0);
	RNA_def_property_ui_text(prop, "Roll Out", "Roll offset for the end of the B-Bone, adjusts twist");
	RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone);
	
	if (is_posebone == false) {
		prop = RNA_def_property(srna, "use_endroll_as_inroll", PROP_BOOLEAN, PROP_NONE);
		RNA_def_property_ui_text(prop, "Inherit End Roll", "Use Roll Out of parent bone as Roll In of its children");
		RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_ADD_PARENT_END_ROLL);
		RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	}
	
	/* Curve X/Y Offsets */
	prop = RNA_def_property(srna, "bbone_curveinx", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "curveInX");
	RNA_def_property_range(prop, -5.0f, 5.0f);
	RNA_def_property_ui_text(prop, "In X", "X-axis handle offset for start of the B-Bone's curve, adjusts curvature");
	RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone);
	
	prop = RNA_def_property(srna, "bbone_curveiny", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "curveInY");
	RNA_def_property_range(prop, -5.0f, 5.0f);
	RNA_def_property_ui_text(prop, "In Y", "Y-axis handle offset for start of the B-Bone's curve, adjusts curvature");
	RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone);
	
	prop = RNA_def_property(srna, "bbone_curveoutx", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "curveOutX");
	RNA_def_property_range(prop, -5.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Out X", "X-axis handle offset for end of the B-Bone's curve, adjusts curvature");
	RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone);
	
	prop = RNA_def_property(srna, "bbone_curveouty", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "curveOutY");
	RNA_def_property_range(prop, -5.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Out Y", "Y-axis handle offset for end of the B-Bone's curve, adjusts curvature");
	RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone);
	
	/* Scale In/Out */
	prop = RNA_def_property(srna, "bbone_scalein", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "scaleIn");
	RNA_def_property_range(prop, 0.0f, 5.0f);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_ui_text(prop, "Scale In", "Scale factor for start of the B-Bone, adjusts thickness (for tapering effects)");
	RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone);
	
	prop = RNA_def_property(srna, "bbone_scaleout", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "scaleOut");
	RNA_def_property_range(prop, 0.0f, 5.0f);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_ui_text(prop, "Scale Out", "Scale factor for end of the B-Bone, adjusts thickness (for tapering effects)");
	RNA_DEF_CURVEBONE_UPDATE(prop, is_posebone);
	
#undef RNA_DEF_CURVEBONE_UPDATE
}

static void rna_def_bone_common(StructRNA *srna, int editbone)
{
	PropertyRNA *prop;

	/* strings */
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);
	if (editbone) RNA_def_property_string_funcs(prop, NULL, NULL, "rna_EditBone_name_set");
	else RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Bone_name_set");
	RNA_def_property_update(prop, 0, "rna_Bone_update_renamed");

	/* flags */
	prop = RNA_def_property(srna, "layers", PROP_BOOLEAN, PROP_LAYER_MEMBER);
	RNA_def_property_boolean_sdna(prop, NULL, "layer", 1);
	RNA_def_property_array(prop, 32);
	if (editbone) RNA_def_property_boolean_funcs(prop, NULL, "rna_EditBone_layer_set");
	else RNA_def_property_boolean_funcs(prop, NULL, "rna_Bone_layer_set");
	RNA_def_property_ui_text(prop, "Layers", "Layers bone exists in");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

	prop = RNA_def_property(srna, "use_connect", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_CONNECTED);
	if (editbone) RNA_def_property_boolean_funcs(prop, NULL, "rna_EditBone_connected_set");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Connected", "When bone has a parent, bone's head is stuck to the parent's tail");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop = RNA_def_property(srna, "use_inherit_rotation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", BONE_HINGE);
	RNA_def_property_ui_text(prop, "Inherit Rotation", "Bone inherits rotation or scale from parent bone");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop = RNA_def_property(srna, "use_envelope_multiply", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_MULT_VG_ENV);
	RNA_def_property_ui_text(prop, "Multiply Vertex Group with Envelope",
	                         "When deforming bone, multiply effects of Vertex Group weights with Envelope influence");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop = RNA_def_property(srna, "use_deform", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", BONE_NO_DEFORM);
	RNA_def_property_ui_text(prop, "Deform", "Enable Bone to deform geometry");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop = RNA_def_property(srna, "use_inherit_scale", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Inherit Scale", "Bone inherits scaling from parent bone");
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", BONE_NO_SCALE);
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");

	prop = RNA_def_property(srna, "use_local_location", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Local Location", "Bone location is set in local space");
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", BONE_NO_LOCAL_LOCATION);
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop = RNA_def_property(srna, "use_relative_parent", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Relative Parenting", "Object children will use relative transform, like deform");
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_RELATIVE_PARENTING);
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop = RNA_def_property(srna, "show_wire", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_DRAWWIRE);
	RNA_def_property_ui_text(prop, "Draw Wire",
	                         "Bone is always drawn as Wireframe regardless of viewport draw mode "
	                         "(useful for non-obstructive custom bone shapes)");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	
	/* XXX: use_cyclic_offset is deprecated in 2.5. May/may not return */
	prop = RNA_def_property(srna, "use_cyclic_offset", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", BONE_NO_CYCLICOFFSET);
	RNA_def_property_ui_text(prop, "Cyclic Offset",
	                         "When bone doesn't have a parent, it receives cyclic offset effects (Deprecated)");
	//                         "When bone doesn't have a parent, it receives cyclic offset effects");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop = RNA_def_property(srna, "hide_select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_UNSELECTABLE);
	RNA_def_property_ui_text(prop, "Selectable", "Bone is able to be selected");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

	/* Number values */
	/* envelope deform settings */
	prop = RNA_def_property(srna, "envelope_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dist");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Envelope Deform Distance", "Bone deformation distance (for Envelope deform only)");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop = RNA_def_property(srna, "envelope_weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Envelope Deform Weight", "Bone deformation weight (for Envelope deform only)");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop = RNA_def_property(srna, "head_radius", PROP_FLOAT, PROP_UNSIGNED);
	if (editbone) RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update");
	else RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	RNA_def_property_float_sdna(prop, NULL, "rad_head");
	/* XXX range is 0 to lim, where lim = 10000.0f * MAX2(1.0, view3d->grid); */
	/*RNA_def_property_range(prop, 0, 1000); */
	RNA_def_property_ui_range(prop, 0.01, 100, 0.1, 3);
	RNA_def_property_ui_text(prop, "Envelope Head Radius", "Radius of head of bone (for Envelope deform only)");
	
	prop = RNA_def_property(srna, "tail_radius", PROP_FLOAT, PROP_UNSIGNED);
	if (editbone) RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update");
	else RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	RNA_def_property_float_sdna(prop, NULL, "rad_tail");
	/* XXX range is 0 to lim, where lim = 10000.0f * MAX2(1.0, view3d->grid); */
	/*RNA_def_property_range(prop, 0, 1000); */
	RNA_def_property_ui_range(prop, 0.01, 100, 0.1, 3);
	RNA_def_property_ui_text(prop, "Envelope Tail Radius", "Radius of tail of bone (for Envelope deform only)");
	
	/* b-bones deform settings */
	prop = RNA_def_property(srna, "bbone_segments", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "segments");
	RNA_def_property_range(prop, 1, 32);
	RNA_def_property_ui_text(prop, "B-Bone Segments", "Number of subdivisions of bone (for B-Bones only)");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop = RNA_def_property(srna, "bbone_in", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ease1");
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "B-Bone Ease In", "Length of first Bezier Handle (for B-Bones only)");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop = RNA_def_property(srna, "bbone_out", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ease2");
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "B-Bone Ease Out", "Length of second Bezier Handle (for B-Bones only)");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");

	prop = RNA_def_property(srna, "bbone_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "xwidth");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "B-Bone Display X Width", "B-Bone X size");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop = RNA_def_property(srna, "bbone_z", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "zwidth");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "B-Bone Display Z Width", "B-Bone Z size");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
}

/* err... bones should not be directly edited (only editbones should be...) */
static void rna_def_bone(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "Bone", NULL);
	RNA_def_struct_ui_text(srna, "Bone", "Bone in an Armature data-block");
	RNA_def_struct_ui_icon(srna, ICON_BONE_DATA);
	RNA_def_struct_path_func(srna, "rna_Bone_path");
	RNA_def_struct_idprops_func(srna, "rna_Bone_idprops");
	
	/* pointers/collections */
	/* parent (pointer) */
	prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Bone");
	RNA_def_property_pointer_sdna(prop, NULL, "parent");
	RNA_def_property_ui_text(prop, "Parent", "Parent bone (in same Armature)");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	
	/* children (collection) */
	prop = RNA_def_property(srna, "children", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "childbase", NULL);
	RNA_def_property_struct_type(prop, "Bone");
	RNA_def_property_ui_text(prop, "Children", "Bones which are children of this bone");

	rna_def_bone_common(srna, 0);
	rna_def_bone_curved_common(srna, 0);

	/* XXX should we define this in PoseChannel wrapping code instead?
	 *     But PoseChannels directly get some of their flags from here... */
	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_HIDDEN_P);
	RNA_def_property_ui_text(prop, "Hide",
	                         "Bone is not visible when it is not in Edit Mode (i.e. in Object or Pose Modes)");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_SELECTED);
	RNA_def_property_ui_text(prop, "Select", "");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE); /* XXX: review whether this could be used for interesting effects... */
	RNA_def_property_update(prop, 0, "rna_Bone_select_update");
	
	prop = RNA_def_property(srna, "select_head", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_ROOTSEL);
	RNA_def_property_ui_text(prop, "Select Head", "");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	
	prop = RNA_def_property(srna, "select_tail", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_TIPSEL);
	RNA_def_property_ui_text(prop, "Select Tail", "");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

	/* XXX better matrix descriptions possible (Arystan) */
	prop = RNA_def_property(srna, "matrix", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_float_sdna(prop, NULL, "bone_mat");
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_3x3);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Bone Matrix", "3x3 bone matrix");

	prop = RNA_def_property(srna, "matrix_local", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_float_sdna(prop, NULL, "arm_mat");
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Bone Armature-Relative Matrix", "4x4 bone matrix relative to armature");

	prop = RNA_def_property(srna, "tail", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "tail");
	RNA_def_property_array(prop, 3);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Tail", "Location of tail end of the bone");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);

	prop = RNA_def_property(srna, "tail_local", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "arm_tail");
	RNA_def_property_array(prop, 3);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Armature-Relative Tail", "Location of tail end of the bone relative to armature");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);

	prop = RNA_def_property(srna, "head", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "head");
	RNA_def_property_array(prop, 3);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Head", "Location of head end of the bone relative to its parent");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);

	prop = RNA_def_property(srna, "head_local", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "arm_head");
	RNA_def_property_array(prop, 3);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Armature-Relative Head", "Location of head end of the bone relative to armature");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);

	RNA_api_bone(srna);
}

static void rna_def_edit_bone(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "EditBone", NULL);
	RNA_def_struct_sdna(srna, "EditBone");
	RNA_def_struct_idprops_func(srna, "rna_EditBone_idprops");
	RNA_def_struct_ui_text(srna, "Edit Bone", "Editmode bone in an Armature data-block");
	RNA_def_struct_ui_icon(srna, ICON_BONE_DATA);
	
	RNA_define_verify_sdna(0); /* not in sdna */

	prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "EditBone");
	RNA_def_property_pointer_funcs(prop, "rna_EditBone_parent_get", "rna_EditBone_parent_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Parent", "Parent edit bone (in same Armature)");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	
	prop = RNA_def_property(srna, "roll", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "roll");
	RNA_def_property_ui_range(prop, -M_PI * 2, M_PI * 2, 10, 2);
	RNA_def_property_ui_text(prop, "Roll", "Bone rotation around head-tail axis");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update");

	prop = RNA_def_property(srna, "head", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "head");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Head", "Location of head end of the bone");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update");

	prop = RNA_def_property(srna, "tail", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "tail");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Tail", "Location of tail end of the bone");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update");

	rna_def_bone_common(srna, 1);
	rna_def_bone_curved_common(srna, 0);

	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_HIDDEN_A);
	RNA_def_property_ui_text(prop, "Hide", "Bone is not visible when in Edit Mode");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

	prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_EDITMODE_LOCKED);
	RNA_def_property_ui_text(prop, "Lock", "Bone is not able to be transformed when in Edit Mode");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_SELECTED);
	RNA_def_property_ui_text(prop, "Select", "");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

	prop = RNA_def_property(srna, "select_head", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_ROOTSEL);
	RNA_def_property_ui_text(prop, "Head Select", "");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	
	prop = RNA_def_property(srna, "select_tail", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_TIPSEL);
	RNA_def_property_ui_text(prop, "Tail Select", "");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

	/* calculated and read only, not actual data access */
	prop = RNA_def_property(srna, "matrix", PROP_FLOAT, PROP_MATRIX);
	/*RNA_def_property_float_sdna(prop, NULL, "");  *//* doesnt access any real data */
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	//RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_flag(prop, PROP_THICK_WRAP); /* no reference to original data */
	RNA_def_property_ui_text(prop, "Editbone Matrix",
	                         "Matrix combining loc/rot of the bone (head position, direction and roll), "
	                         "in armature space (WARNING: does not include/support bone's length/size)");
	RNA_def_property_float_funcs(prop, "rna_EditBone_matrix_get", "rna_EditBone_matrix_set", NULL);

	RNA_api_armature_edit_bone(srna);

	RNA_define_verify_sdna(1);
}


/* armature.bones.* */
static void rna_def_armature_bones(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

/*	FunctionRNA *func; */
/*	PropertyRNA *parm; */

	RNA_def_property_srna(cprop, "ArmatureBones");
	srna = RNA_def_struct(brna, "ArmatureBones", NULL);
	RNA_def_struct_sdna(srna, "bArmature");
	RNA_def_struct_ui_text(srna, "Armature Bones", "Collection of armature bones");


	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Bone");
	RNA_def_property_pointer_sdna(prop, NULL, "act_bone");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Active Bone", "Armature's active bone");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Armature_act_bone_set", NULL, NULL);

	/* todo, redraw */
/*		RNA_def_property_collection_active(prop, prop_act); */
}

/* armature.bones.* */
static void rna_def_armature_edit_bones(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "ArmatureEditBones");
	srna = RNA_def_struct(brna, "ArmatureEditBones", NULL);
	RNA_def_struct_sdna(srna, "bArmature");
	RNA_def_struct_ui_text(srna, "Armature EditBones", "Collection of armature edit bones");

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "EditBone");
	RNA_def_property_pointer_sdna(prop, NULL, "act_edbone");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Active EditBone", "Armatures active edit bone");
	/*RNA_def_property_update(prop, 0, "rna_Armature_act_editbone_update"); */
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Armature_act_edit_bone_set", NULL, NULL);

	/* todo, redraw */
/*		RNA_def_property_collection_active(prop, prop_act); */

	/* add target */
	func = RNA_def_function(srna, "new", "rna_Armature_edit_bone_new");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Add a new bone");
	parm = RNA_def_string(func, "name", "Object", 0, "", "New name for the bone");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	/* return type */
	parm = RNA_def_pointer(func, "bone", "EditBone", "", "Newly created edit bone");
	RNA_def_function_return(func, parm);

	/* remove target */
	func = RNA_def_function(srna, "remove", "rna_Armature_edit_bone_remove");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove an existing bone from the armature");
	/* target to remove*/
	parm = RNA_def_pointer(func, "bone", "EditBone", "", "EditBone to remove");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
	RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
}

static void rna_def_armature(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	static EnumPropertyItem prop_drawtype_items[] = {
		{ARM_OCTA, "OCTAHEDRAL", 0, "Octahedral", "Display bones as octahedral shape (default)"},
		{ARM_LINE, "STICK", 0, "Stick", "Display bones as simple 2D lines with dots"},
		{ARM_B_BONE, "BBONE", 0, "B-Bone", "Display bones as boxes, showing subdivision and B-Splines"},
		{ARM_ENVELOPE, "ENVELOPE", 0, "Envelope",
		               "Display bones as extruded spheres, showing deformation influence volume"},
		{ARM_WIRE, "WIRE", 0, "Wire", "Display bones as thin wires, showing subdivision and B-Splines"},
		{0, NULL, 0, NULL, NULL}
	};
	static EnumPropertyItem prop_vdeformer[] = {
		{ARM_VDEF_BLENDER, "BLENDER", 0, "Blender", "Use Blender's armature vertex deformation"},
		{ARM_VDEF_BGE_CPU, "BGE_CPU", 0, "BGE", "Use vertex deformation code optimized for the BGE"},
		{0, NULL, 0, NULL, NULL}
	};
	static EnumPropertyItem prop_ghost_type_items[] = {
		{ARM_GHOST_CUR, "CURRENT_FRAME", 0, "Around Frame",
		                "Display Ghosts of poses within a fixed number of frames around the current frame"},
		{ARM_GHOST_RANGE, "RANGE", 0, "In Range", "Display Ghosts of poses within specified range"},
		{ARM_GHOST_KEYS, "KEYS", 0, "On Keyframes", "Display Ghosts of poses on Keyframes"},
		{0, NULL, 0, NULL, NULL}
	};
	static const EnumPropertyItem prop_pose_position_items[] = {
		{0, "POSE", 0, "Pose Position", "Show armature in posed state"},
		{ARM_RESTPOS, "REST", 0, "Rest Position", "Show Armature in binding pose state (no posing possible)"},
		{0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "Armature", "ID");
	RNA_def_struct_ui_text(srna, "Armature",
	                       "Armature data-block containing a hierarchy of bones, usually used for rigging characters");
	RNA_def_struct_ui_icon(srna, ICON_ARMATURE_DATA);
	RNA_def_struct_sdna(srna, "bArmature");

	func = RNA_def_function(srna, "transform", "rna_Armature_transform");
	RNA_def_function_ui_description(func, "Transform armature bones by a matrix");
	parm = RNA_def_float_matrix(func, "matrix", 4, 4, NULL, 0.0f, 0.0f, "", "Matrix", 0.0f, 0.0f);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

	/* Animation Data */
	rna_def_animdata_common(srna);
	
	/* Collections */
	prop = RNA_def_property(srna, "bones", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "bonebase", NULL);
	RNA_def_property_collection_funcs(prop, NULL, "rna_Armature_bones_next", NULL, NULL, NULL, NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "Bone");
	RNA_def_property_ui_text(prop, "Bones", "");
	rna_def_armature_bones(brna, prop);

	prop = RNA_def_property(srna, "edit_bones", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "edbo", NULL);
	RNA_def_property_struct_type(prop, "EditBone");
	RNA_def_property_ui_text(prop, "Edit Bones", "");
	rna_def_armature_edit_bones(brna, prop);

	/* Enum values */
	prop = RNA_def_property(srna, "pose_position", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, prop_pose_position_items);
	RNA_def_property_ui_text(prop, "Pose Position", "Show armature in binding pose or final posed state");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	
	prop = RNA_def_property(srna, "draw_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "drawtype");
	RNA_def_property_enum_items(prop, prop_drawtype_items);
	RNA_def_property_ui_text(prop, "Draw Type", "");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);

	prop = RNA_def_property(srna, "deform_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "gevertdeformer");
	RNA_def_property_enum_items(prop, prop_vdeformer);
	RNA_def_property_ui_text(prop, "Vertex Deformer", "Vertex Deformer Method (Game Engine only)");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	
/* XXX deprecated ....... old animviz for armatures only */
	prop = RNA_def_property(srna, "ghost_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "ghosttype");
	RNA_def_property_enum_items(prop, prop_ghost_type_items);
	RNA_def_property_ui_text(prop, "Ghost Type", "Method of Onion-skinning for active Action");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
/* XXX deprecated ....... old animviz for armatures only	 */

	/* Boolean values */
	/* layer */
	prop = RNA_def_property(srna, "layers", PROP_BOOLEAN, PROP_LAYER_MEMBER);
	RNA_def_property_boolean_sdna(prop, NULL, "layer", 1);
	RNA_def_property_array(prop, 32);
	RNA_def_property_ui_text(prop, "Visible Layers", "Armature layer visibility");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Armature_layer_set");
	RNA_def_property_update(prop, NC_OBJECT | ND_POSE, "rna_Armature_update_layers");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	
	/* layer protection */
	prop = RNA_def_property(srna, "layers_protected", PROP_BOOLEAN, PROP_LAYER);
	RNA_def_property_boolean_sdna(prop, NULL, "layer_protected", 1);
	RNA_def_property_array(prop, 32);
	RNA_def_property_ui_text(prop, "Layer Proxy Protection",
	                         "Protected layers in Proxy Instances are restored to Proxy settings "
	                         "on file reload and undo");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
		
	/* flag */
	prop = RNA_def_property(srna, "show_axes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_DRAWAXES);
	RNA_def_property_ui_text(prop, "Draw Axes", "Draw bone axes");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	
	prop = RNA_def_property(srna, "show_names", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_DRAWNAMES);
	RNA_def_property_ui_text(prop, "Draw Names", "Draw bone names");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	
	prop = RNA_def_property(srna, "use_deform_delay", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_DELAYDEFORM);
	RNA_def_property_ui_text(prop, "Delay Deform", "Don't deform children when manipulating bones in Pose Mode");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop = RNA_def_property(srna, "use_mirror_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_MIRROR_EDIT);
	RNA_def_property_ui_text(prop, "X-Axis Mirror", "Apply changes to matching bone on opposite side of X-Axis");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	
	prop = RNA_def_property(srna, "use_auto_ik", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_AUTO_IK);
	RNA_def_property_ui_text(prop, "Auto IK", "Add temporary IK constraints while grabbing bones in Pose Mode");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	
	prop = RNA_def_property(srna, "show_bone_custom_shapes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", ARM_NO_CUSTOM);
	RNA_def_property_ui_text(prop, "Draw Custom Bone Shapes", "Draw bones with their custom shapes");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	
	prop = RNA_def_property(srna, "show_group_colors", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_COL_CUSTOM);
	RNA_def_property_ui_text(prop, "Draw Bone Group Colors", "Draw bone group colors");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	
/* XXX deprecated ....... old animviz for armatures only */
	prop = RNA_def_property(srna, "show_only_ghost_selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_GHOST_ONLYSEL);
	RNA_def_property_ui_text(prop, "Draw Ghosts on Selected Bones Only", "");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
/* XXX deprecated ....... old animviz for armatures only */

	/* Number fields */
/* XXX deprecated ....... old animviz for armatures only */
	/* ghost/onionskining settings */
	prop = RNA_def_property(srna, "ghost_step", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ghostep");
	RNA_def_property_range(prop, 0, 30);
	RNA_def_property_ui_text(prop, "Ghosting Step",
	                         "Number of frame steps on either side of current frame to show as ghosts "
	                         "(only for 'Around Current Frame' Onion-skinning method)");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	
	prop = RNA_def_property(srna, "ghost_size", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ghostsize");
	RNA_def_property_range(prop, 1, 20);
	RNA_def_property_ui_text(prop, "Ghosting Frame Step",
	                         "Frame step for Ghosts (not for 'On Keyframes' Onion-skinning method)");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	
	prop = RNA_def_property(srna, "ghost_frame_start", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "ghostsf");
	RNA_def_property_int_funcs(prop, NULL, "rna_Armature_ghost_start_frame_set", NULL);
	RNA_def_property_ui_text(prop, "Ghosting Start Frame",
	                         "Starting frame of range of Ghosts to display (not for "
	                         "'Around Current Frame' Onion-skinning method)");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	
	prop = RNA_def_property(srna, "ghost_frame_end", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "ghostef");
	RNA_def_property_int_funcs(prop, NULL, "rna_Armature_ghost_end_frame_set", NULL);
	RNA_def_property_ui_text(prop, "Ghosting End Frame",
	                         "End frame of range of Ghosts to display "
	                         "(not for 'Around Current Frame' Onion-skinning method)");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
/* XXX deprecated ....... old animviz for armatures only */


	prop = RNA_def_property(srna, "is_editmode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_Armature_is_editmode_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Is Editmode", "True when used in editmode");
}

void RNA_def_armature(BlenderRNA *brna)
{
	rna_def_armature(brna);
	rna_def_bone(brna);
	rna_def_edit_bone(brna);
}

#endif
