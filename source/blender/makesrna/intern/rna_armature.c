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
 * Contributor(s): Blender Foundation (2008), Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "WM_api.h"
#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_idprop.h"
#include "BKE_main.h"

#include "ED_armature.h"
#include "BKE_armature.h"

static void rna_Armature_update_data(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	ID *id= ptr->id.data;

	DAG_id_flush_update(id, OB_RECALC_DATA);
	WM_main_add_notifier(NC_GEOM|ND_DATA, id);
	//WM_main_add_notifier(NC_OBJECT|ND_POSE, NULL);
}


static void rna_Armature_act_bone_set(PointerRNA *ptr, PointerRNA value)
{
	bArmature *arm= (bArmature*)ptr->data;

	if(value.id.data==NULL && value.data==NULL) {
		arm->act_bone= NULL;
	}
	else {
		if(value.id.data != arm) {
			/* raise an error! */
		}
		else {
			arm->act_bone= value.data;
			arm->act_bone->flag |= BONE_SELECTED;
		}
	}
}

static void rna_Armature_act_edit_bone_set(PointerRNA *ptr, PointerRNA value)
{
	bArmature *arm= (bArmature*)ptr->data;

	if(value.id.data==NULL && value.data==NULL) {
		arm->act_edbone= NULL;
	}
	else {
		if(value.id.data != arm) {
			/* raise an error! */
		}
		else {
			arm->act_edbone= value.data;
			((EditBone *)arm->act_edbone)->flag |= BONE_SELECTED;
		}
	}
}

EditBone *rna_Armature_edit_bone_new(bArmature *arm, ReportList *reports, char *name)
{
	if(arm->edbo==NULL) {
		BKE_reportf(reports, RPT_ERROR, "Armature '%s' not in editmode, cant add an editbone.", arm->id.name+2);
		return NULL;
	}
	return ED_armature_edit_bone_add(arm, name);
}

void rna_Armature_edit_bone_remove(bArmature *arm, ReportList *reports, EditBone *ebone)
{
	if(arm->edbo==NULL) {
		BKE_reportf(reports, RPT_ERROR, "Armature '%s' not in editmode, cant remove an editbone.", arm->id.name+2);
		return;
	}
	ED_armature_edit_bone_remove(arm, ebone);
}

static void rna_Armature_redraw_data(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	ID *id= ptr->id.data;

	WM_main_add_notifier(NC_GEOM|ND_DATA, id);
}

static char *rna_Bone_path(PointerRNA *ptr)
{
	return BLI_sprintfN("bones[\"%s\"]", ((Bone*)ptr->data)->name);
}

static IDProperty *rna_Bone_idproperties(PointerRNA *ptr, int create)
{
	Bone *bone= ptr->data;

	if(create && !bone->prop) {
		IDPropertyTemplate val = {0};
		bone->prop= IDP_New(IDP_GROUP, val, "RNA_Bone ID properties");
	}

	return bone->prop;
}

static IDProperty *rna_EditBone_idproperties(PointerRNA *ptr, int create)
{
	EditBone *ebone= ptr->data;

	if(create && !ebone->prop) {
		IDPropertyTemplate val = {0};
		ebone->prop= IDP_New(IDP_GROUP, val, "RNA_EditBone ID properties");
	}

	return ebone->prop;
}

static void rna_bone_layer_set(int *layer, const int *values)
{
	int i, tot= 0;

	/* ensure we always have some layer selected */
	for(i=0; i<32; i++)
		if(values[i])
			tot++;
	
	if(tot==0)
		return;

	for(i=0; i<32; i++) {
		if(values[i]) *layer |= (1<<i);
		else *layer &= ~(1<<i);
	}
}

static void rna_Bone_layer_set(PointerRNA *ptr, const int *values)
{
	Bone *bone= (Bone*)ptr->data;
	rna_bone_layer_set(&bone->layer, values);
}

static void rna_Armature_layer_set(PointerRNA *ptr, const int *values)
{
	bArmature *arm= (bArmature*)ptr->data;
	int i, tot= 0;

	/* ensure we always have some layer selected */
	for(i=0; i<32; i++)
		if(values[i])
			tot++;
	
	if(tot==0)
		return;

	for(i=0; i<32; i++) {
		if(values[i]) arm->layer |= (1<<i);
		else arm->layer &= ~(1<<i);
	}
}

// XXX depreceated.... old armature only animviz
static void rna_Armature_ghost_start_frame_set(PointerRNA *ptr, int value)
{
	bArmature *data= (bArmature*)ptr->data;
	CLAMP(value, 1, data->ghostef);
	data->ghostsf= value;
}

static void rna_Armature_ghost_end_frame_set(PointerRNA *ptr, int value)
{
	bArmature *data= (bArmature*)ptr->data;
	CLAMP(value, data->ghostsf, (int)(MAXFRAMEF/2));
	data->ghostef= value;
}
// XXX depreceated... old armature only animviz

static void rna_EditBone_name_set(PointerRNA *ptr, const char *value)
{
	bArmature *arm= (bArmature*)ptr->id.data;
	EditBone *ebone= (EditBone*)ptr->data;
	char oldname[32], newname[32];
	
	/* need to be on the stack */
	BLI_strncpy(newname, value, 32);
	BLI_strncpy(oldname, ebone->name, 32);
	
	ED_armature_bone_rename(arm, oldname, newname);
}

static void rna_Bone_name_set(PointerRNA *ptr, const char *value)
{
	bArmature *arm= (bArmature*)ptr->id.data;
	Bone *bone= (Bone*)ptr->data;
	char oldname[32], newname[32];
	
	/* need to be on the stack */
	BLI_strncpy(newname, value, 32);
	BLI_strncpy(oldname, bone->name, 32);
	
	ED_armature_bone_rename(arm, oldname, newname);
}

static void rna_EditBone_layer_set(PointerRNA *ptr, const int values[])
{
	EditBone *data= (EditBone*)(ptr->data);
	rna_bone_layer_set(&data->layer, values);
}

static void rna_EditBone_connected_check(EditBone *ebone)
{
	if(ebone->parent) {
		if(ebone->flag & BONE_CONNECTED) {
			/* Attach this bone to its parent */
			VECCOPY(ebone->head, ebone->parent->tail);

			if(ebone->flag & BONE_ROOTSEL)
				ebone->parent->flag |= BONE_TIPSEL;
		}
		else if(!(ebone->parent->flag & BONE_ROOTSEL)) {
			ebone->parent->flag &= ~BONE_TIPSEL;
		}
	}
}

static void rna_EditBone_connected_set(PointerRNA *ptr, int value)
{
	EditBone *ebone= (EditBone*)(ptr->data);

	if(value) ebone->flag |= BONE_CONNECTED;
	else ebone->flag &= ~BONE_CONNECTED;

	rna_EditBone_connected_check(ebone);
}

static PointerRNA rna_EditBone_parent_get(PointerRNA *ptr)
{
	EditBone *data= (EditBone*)(ptr->data);
	return rna_pointer_inherit_refine(ptr, &RNA_EditBone, data->parent);
}

static void rna_EditBone_parent_set(PointerRNA *ptr, PointerRNA value)
{
	EditBone *ebone= (EditBone*)(ptr->data);
	EditBone *pbone, *parbone= (EditBone*)value.data;

	if(parbone == NULL) {
		if(ebone->parent && !(ebone->parent->flag & BONE_ROOTSEL))
			ebone->parent->flag &= ~BONE_TIPSEL;

		ebone->parent = NULL;
		ebone->flag &= ~BONE_CONNECTED;
	}
	else {
		/* within same armature */
		if(value.id.data != ptr->id.data)
			return;

		/* make sure this is a valid child */
		if(parbone == ebone)
			return;
			
		for(pbone= parbone->parent; pbone; pbone=pbone->parent)
			if(pbone == ebone)
				return;

		ebone->parent = parbone;
		rna_EditBone_connected_check(ebone);
	}
}

static void rna_EditBone_matrix_get(PointerRNA *ptr, float *values)
{
	EditBone *ebone= (EditBone*)(ptr->data);

	float	delta[3], tmat[3][3], mat[4][4];

	/* Find the current bone matrix */
	sub_v3_v3v3(delta, ebone->tail, ebone->head);
	vec_roll_to_mat3(delta, ebone->roll, tmat);
	copy_m4_m3(mat, tmat);
	VECCOPY(mat[3], ebone->head);

	memcpy(values, mat, 16 * sizeof(float));
}

static void rna_Armature_editbone_transform_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	bArmature *arm= (bArmature*)ptr->id.data;
	EditBone *ebone= (EditBone*)ptr->data;
	EditBone *child, *eboflip;
	
	/* update our parent */
	if(ebone->parent && ebone->flag & BONE_CONNECTED)
		VECCOPY(ebone->parent->tail, ebone->head)

	/* update our children if necessary */
	for(child = arm->edbo->first; child; child=child->next)
		if(child->parent == ebone && (child->flag & BONE_CONNECTED))
			VECCOPY(child->head, ebone->tail);

	if(arm->flag & ARM_MIRROR_EDIT) {
		eboflip= ED_armature_bone_get_mirrored(arm->edbo, ebone);

		if(eboflip) {
			eboflip->roll= -ebone->roll;

			eboflip->head[0]= -ebone->head[0];
			eboflip->tail[0]= -ebone->tail[0];
			
			/* update our parent */
			if(eboflip->parent && eboflip->flag & BONE_CONNECTED)
				VECCOPY(eboflip->parent->tail, eboflip->head);
			
			/* update our children if necessary */
			for(child = arm->edbo->first; child; child=child->next)
				if(child->parent == eboflip && (child->flag & BONE_CONNECTED))
					VECCOPY (child->head, eboflip->tail);
		}
	}

	rna_Armature_update_data(bmain, scene, ptr);
}

static void rna_Armature_bones_next(CollectionPropertyIterator *iter)
{
	ListBaseIterator *internal= iter->internal;
	Bone *bone= (Bone*)internal->link;

	if(bone->childbase.first)
		internal->link= (Link*)bone->childbase.first;
	else if(bone->next)
		internal->link= (Link*)bone->next;
	else {
		internal->link= NULL;

		do {
			bone= bone->parent;
			if(bone && bone->next) {
				internal->link= (Link*)bone->next;
				break;
			}
		} while(bone);
	}

	iter->valid= (internal->link != NULL);
}

#else

static void rna_def_bone_common(StructRNA *srna, int editbone)
{
	PropertyRNA *prop;

	/* strings */
	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);
	if(editbone) RNA_def_property_string_funcs(prop, NULL, NULL, "rna_EditBone_name_set");
	else RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Bone_name_set");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

	/* flags */
	prop= RNA_def_property(srna, "layer", PROP_BOOLEAN, PROP_LAYER_MEMBER);
	RNA_def_property_boolean_sdna(prop, NULL, "layer", 1);
	RNA_def_property_array(prop, 32);
	if(editbone) RNA_def_property_boolean_funcs(prop, NULL, "rna_EditBone_layer_set");
	else RNA_def_property_boolean_funcs(prop, NULL, "rna_Bone_layer_set");
	RNA_def_property_ui_text(prop, "Layers", "Layers bone exists in");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

	prop= RNA_def_property(srna, "connected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_CONNECTED);
	if(editbone) RNA_def_property_boolean_funcs(prop, NULL, "rna_EditBone_connected_set");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Connected", "When bone has a parent, bone's head is struck to the parent's tail.");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop= RNA_def_property(srna, "hinge", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", BONE_HINGE);
	RNA_def_property_ui_text(prop, "Inherit Rotation", "Bone inherits rotation or scale from parent bone.");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop= RNA_def_property(srna, "multiply_vertexgroup_with_envelope", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_MULT_VG_ENV);
	RNA_def_property_ui_text(prop, "Multiply Vertex Group with Envelope", "When deforming bone, multiply effects of Vertex Group weights with Envelope influence.");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop= RNA_def_property(srna, "deform", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", BONE_NO_DEFORM);
	RNA_def_property_ui_text(prop, "Deform", "Bone does not deform any geometry.");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop= RNA_def_property(srna, "inherit_scale", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Inherit Scale", "Bone inherits scaling from parent bone.");
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", BONE_NO_SCALE);
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");

	prop= RNA_def_property(srna, "local_location", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Local Location", "Bone location is set in local space.");
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", BONE_NO_LOCAL_LOCATION);
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop= RNA_def_property(srna, "draw_wire", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_DRAWWIRE);
	RNA_def_property_ui_text(prop, "Draw Wire", "Bone is always drawn as Wireframe regardless of viewport draw mode. Useful for non-obstructive custom bone shapes.");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	
	prop= RNA_def_property(srna, "cyclic_offset", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", BONE_NO_CYCLICOFFSET);
	RNA_def_property_ui_text(prop, "Cyclic Offset", "When bone doesn't have a parent, it receives cyclic offset effects.");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop= RNA_def_property(srna, "restrict_select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_UNSELECTABLE);
	RNA_def_property_ui_text(prop, "Selectable", "Bone is able to be selected");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

	/* Number values */
		/* envelope deform settings */
	prop= RNA_def_property(srna, "envelope_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dist");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Envelope Deform Distance", "Bone deformation distance (for Envelope deform only).");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop= RNA_def_property(srna, "envelope_weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Envelope Deform Weight", "Bone deformation weight (for Envelope deform only).");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop= RNA_def_property(srna, "head_radius", PROP_FLOAT, PROP_NONE);
	if(editbone) RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update");
	RNA_def_property_float_sdna(prop, NULL, "rad_head");
	//RNA_def_property_range(prop, 0, 1000);  // XXX range is 0 to lim, where lim= 10000.0f*MAX2(1.0, view3d->grid);
	RNA_def_property_ui_text(prop, "Envelope Head Radius", "Radius of head of bone (for Envelope deform only).");
	
	prop= RNA_def_property(srna, "tail_radius", PROP_FLOAT, PROP_NONE);
	if(editbone) RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update");
	RNA_def_property_float_sdna(prop, NULL, "rad_tail");
	//RNA_def_property_range(prop, 0, 1000);  // XXX range is 0 to lim, where lim= 10000.0f*MAX2(1.0, view3d->grid);
	RNA_def_property_ui_text(prop, "Envelope Tail Radius", "Radius of tail of bone (for Envelope deform only).");
	
		/* b-bones deform settings */
	prop= RNA_def_property(srna, "bbone_segments", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "segments");
	RNA_def_property_range(prop, 1, 32);
	RNA_def_property_ui_text(prop, "B-Bone Segments", "Number of subdivisions of bone (for B-Bones only).");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop= RNA_def_property(srna, "bbone_in", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ease1");
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "B-Bone Ease In", "Length of first Bezier Handle (for B-Bones only).");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop= RNA_def_property(srna, "bbone_out", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ease2");
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "B-Bone Ease Out", "Length of second Bezier Handle (for B-Bones only).");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
}

// err... bones should not be directly edited (only editbones should be...)
static void rna_def_bone(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "Bone", NULL);
	RNA_def_struct_ui_text(srna, "Bone", "Bone in an Armature datablock.");
	RNA_def_struct_ui_icon(srna, ICON_BONE_DATA);
	RNA_def_struct_path_func(srna, "rna_Bone_path");
	RNA_def_struct_idproperties_func(srna, "rna_Bone_idproperties");
	
	/* pointers/collections */
		/* parent (pointer) */
	prop= RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Bone");
	RNA_def_property_pointer_sdna(prop, NULL, "parent");
	RNA_def_property_ui_text(prop, "Parent", "Parent bone (in same Armature).");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	
		/* children (collection) */
	prop= RNA_def_property(srna, "children", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "childbase", NULL);
	RNA_def_property_struct_type(prop, "Bone");
	RNA_def_property_ui_text(prop, "Children", "Bones which are children of this bone");

	rna_def_bone_common(srna, 0);

		// XXX should we define this in PoseChannel wrapping code instead? but PoseChannels directly get some of their flags from here...
	prop= RNA_def_property(srna, "hidden", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_HIDDEN_P);
	RNA_def_property_ui_text(prop, "Hidden", "Bone is not visible when it is not in Edit Mode (i.e. in Object or Pose Modes).");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

	prop= RNA_def_property(srna, "selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_SELECTED);
	RNA_def_property_ui_text(prop, "Selected", "");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

	/* XXX better matrix descriptions possible (Arystan) */
	prop= RNA_def_property(srna, "matrix", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_float_sdna(prop, NULL, "bone_mat");
	RNA_def_property_array(prop, 9);
	RNA_def_property_ui_text(prop, "Bone Matrix", "3x3 bone matrix.");

	prop= RNA_def_property(srna, "matrix_local", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_float_sdna(prop, NULL, "arm_mat");
	RNA_def_property_array(prop, 16);
	RNA_def_property_ui_text(prop, "Bone Armature-Relative Matrix", "4x4 bone matrix relative to armature.");

	prop= RNA_def_property(srna, "tail", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "tail");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Tail", "Location of tail end of the bone.");

	prop= RNA_def_property(srna, "tail_local", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "arm_tail");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Armature-Relative Tail", "Location of tail end of the bone relative to armature.");

	prop= RNA_def_property(srna, "head", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "head");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Head", "Location of head end of the bone relative to its parent.");

	prop= RNA_def_property(srna, "head_local", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "arm_head");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Armature-Relative Head", "Location of head end of the bone relative to armature.");
}

static void rna_def_edit_bone(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "EditBone", NULL);
	RNA_def_struct_sdna(srna, "EditBone");
	RNA_def_struct_idproperties_func(srna, "rna_EditBone_idproperties");
	RNA_def_struct_ui_text(srna, "Edit Bone", "Editmode bone in an Armature datablock.");
	RNA_def_struct_ui_icon(srna, ICON_BONE_DATA);
	
	RNA_define_verify_sdna(0); // not in sdna

	prop= RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "EditBone");
	RNA_def_property_pointer_funcs(prop, "rna_EditBone_parent_get", "rna_EditBone_parent_set", NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Parent", "Parent edit bone (in same Armature).");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	
	prop= RNA_def_property(srna, "roll", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "roll");
	RNA_def_property_ui_text(prop, "Roll", "Bone rotation around head-tail axis.");
	RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update");

	prop= RNA_def_property(srna, "head", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "head");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Head", "Location of head end of the bone.");
	RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update");

	prop= RNA_def_property(srna, "tail", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "tail");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Tail", "Location of tail end of the bone.");
	RNA_def_property_update(prop, 0, "rna_Armature_editbone_transform_update");

	rna_def_bone_common(srna, 1);

	prop= RNA_def_property(srna, "hidden", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_HIDDEN_A);
	RNA_def_property_ui_text(prop, "Hidden", "Bone is not visible when in Edit Mode");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

	prop= RNA_def_property(srna, "locked", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_EDITMODE_LOCKED);
	RNA_def_property_ui_text(prop, "Locked", "Bone is not able to be transformed when in Edit Mode.");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

	prop= RNA_def_property(srna, "selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_SELECTED);
	RNA_def_property_ui_text(prop, "Selected", "");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

	prop= RNA_def_property(srna, "selected_head", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_ROOTSEL);
	RNA_def_property_ui_text(prop, "Head Selected", "");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	
	prop= RNA_def_property(srna, "selected_tail", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BONE_TIPSEL);
	RNA_def_property_ui_text(prop, "Tail Selected", "");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");

	/* calculated and read only, not actual data access */
	prop= RNA_def_property(srna, "matrix", PROP_FLOAT, PROP_MATRIX);
	//RNA_def_property_float_sdna(prop, NULL, ""); // doesnt access any real data
	RNA_def_property_array(prop, 16);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_flag(prop, PROP_THICK_WRAP); /* no reference to original data */
	RNA_def_property_ui_text(prop, "Editbone Matrix", "Read-only matrix calculated from the roll (armature space).");
	RNA_def_property_float_funcs(prop, "rna_EditBone_matrix_get", NULL, NULL); // TODO - this could be made writable also

	RNA_api_armature_edit_bone(srna);

	RNA_define_verify_sdna(1);
}


/* armature.bones.* */
static void rna_def_armature_bones(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

//	FunctionRNA *func;
//	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "ArmatureBones");
	srna= RNA_def_struct(brna, "ArmatureBones", NULL);
	RNA_def_struct_sdna(srna, "bArmature");
	RNA_def_struct_ui_text(srna, "Armature Bones", "Collection of armature bones.");


	prop= RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Bone");
	RNA_def_property_pointer_sdna(prop, NULL, "act_bone");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Active Bone", "Armatures active bone.");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Armature_act_bone_set", NULL);

	/* todo, redraw */
//		RNA_def_property_collection_active(prop, prop_act);
}

/* armature.bones.* */
static void rna_def_armature_edit_bones(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "ArmatureEditBones");
	srna= RNA_def_struct(brna, "ArmatureEditBones", NULL);
	RNA_def_struct_sdna(srna, "bArmature");
	RNA_def_struct_ui_text(srna, "Armature EditBones", "Collection of armature edit bones.");

	prop= RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "EditBone");
	RNA_def_property_pointer_sdna(prop, NULL, "act_edbone");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Active EditBone", "Armatures active edit bone.");
	//RNA_def_property_update(prop, 0, "rna_Armature_act_editbone_update");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Armature_act_edit_bone_set", NULL);

	/* todo, redraw */
//		RNA_def_property_collection_active(prop, prop_act);

	/* add target */
	func= RNA_def_function(srna, "new", "rna_Armature_edit_bone_new");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Add a new bone.");
	parm= RNA_def_string(func, "name", "Object", 0, "", "New name for the bone.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	/* return type */
	parm= RNA_def_pointer(func, "bone", "EditBone", "", "Newly created edit bone.");
	RNA_def_function_return(func, parm);

	/* remove target */
	func= RNA_def_function(srna, "remove", "rna_Armature_edit_bone_remove");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove an existing bone from the armature.");
	/* target to remove*/
	parm= RNA_def_pointer(func, "bone", "EditBone", "", "EditBone to remove.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

static void rna_def_armature(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_drawtype_items[] = {
		{ARM_OCTA, "OCTAHEDRAL", 0, "Octahedral", "Display bones as octahedral shape (default)."},
		{ARM_LINE, "STICK", 0, "Stick", "Display bones as simple 2D lines with dots."},
		{ARM_B_BONE, "BBONE", 0, "B-Bone", "Display bones as boxes, showing subdivision and B-Splines"},
		{ARM_ENVELOPE, "ENVELOPE", 0, "Envelope", "Display bones as extruded spheres, showing defomation influence volume."},
		{0, NULL, 0, NULL, NULL}};
	static EnumPropertyItem prop_ghost_type_items[] = {
		{ARM_GHOST_CUR, "CURRENT_FRAME", 0, "Around Frame", "Display Ghosts of poses within a fixed number of frames around the current frame."},
		{ARM_GHOST_RANGE, "RANGE", 0, "In Range", "Display Ghosts of poses within specified range."},
		{ARM_GHOST_KEYS, "KEYS", 0, "On Keyframes", "Display Ghosts of poses on Keyframes."},
		{0, NULL, 0, NULL, NULL}};
	static const EnumPropertyItem prop_pose_position_items[]= {
		{0, "POSE", 0, "Pose Position", "Show armature in posed state."},
		{ARM_RESTPOS, "REST", 0, "Rest Position", "Show Armature in binding pose state. No posing possible."},
		{0, NULL, 0, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "Armature", "ID");
	RNA_def_struct_ui_text(srna, "Armature", "Armature datablock containing a hierarchy of bones, usually used for rigging characters.");
	RNA_def_struct_ui_icon(srna, ICON_ARMATURE_DATA);
	RNA_def_struct_sdna(srna, "bArmature");
	
	/* Animation Data */
	rna_def_animdata_common(srna);
	
	/* Collections */
	prop= RNA_def_property(srna, "bones", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "bonebase", NULL);
	RNA_def_property_collection_funcs(prop, 0, "rna_Armature_bones_next", 0, 0, 0, 0, 0);
	RNA_def_property_struct_type(prop, "Bone");
	RNA_def_property_ui_text(prop, "Bones", "");
	rna_def_armature_bones(brna, prop);

	prop= RNA_def_property(srna, "edit_bones", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "edbo", NULL);
	RNA_def_property_struct_type(prop, "EditBone");
	RNA_def_property_ui_text(prop, "Edit Bones", "");
	rna_def_armature_edit_bones(brna, prop);

	/* Enum values */
	prop= RNA_def_property(srna, "pose_position", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, prop_pose_position_items);
	RNA_def_property_ui_text(prop, "Pose Position", "Show armature in binding pose or final posed state.");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop= RNA_def_property(srna, "drawtype", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_drawtype_items);
	RNA_def_property_ui_text(prop, "Draw Type", "");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	
// XXX depreceated ....... old animviz for armatures only
	prop= RNA_def_property(srna, "ghost_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "ghosttype");
	RNA_def_property_enum_items(prop, prop_ghost_type_items);
	RNA_def_property_ui_text(prop, "Ghost Type", "Method of Onion-skinning for active Action");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
// XXX depreceated ....... old animviz for armatures only	

	/* Boolean values */
		/* layer */
	prop= RNA_def_property(srna, "layer", PROP_BOOLEAN, PROP_LAYER_MEMBER);
	RNA_def_property_boolean_sdna(prop, NULL, "layer", 1);
	RNA_def_property_array(prop, 32);
	RNA_def_property_ui_text(prop, "Visible Layers", "Armature layer visibility.");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Armature_layer_set");
	RNA_def_property_update(prop, NC_OBJECT|ND_POSE, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	
		/* layer protection */
	prop= RNA_def_property(srna, "layer_protection", PROP_BOOLEAN, PROP_LAYER);
	RNA_def_property_boolean_sdna(prop, NULL, "layer_protected", 1);
	RNA_def_property_array(prop, 32);
	RNA_def_property_ui_text(prop, "Layer Proxy Protection", "Protected layers in Proxy Instances are restored to Proxy settings on file reload and undo.");	
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
		
		/* flag */
	prop= RNA_def_property(srna, "draw_axes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_DRAWAXES);
	RNA_def_property_ui_text(prop, "Draw Axes", "Draw bone axes.");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	
	prop= RNA_def_property(srna, "draw_names", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_DRAWNAMES);
	RNA_def_property_ui_text(prop, "Draw Names", "Draw bone names.");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	
	prop= RNA_def_property(srna, "delay_deform", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_DELAYDEFORM);
	RNA_def_property_ui_text(prop, "Delay Deform", "Don't deform children when manipulating bones in Pose Mode");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop= RNA_def_property(srna, "x_axis_mirror", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_MIRROR_EDIT);
	RNA_def_property_ui_text(prop, "X-Axis Mirror", "Apply changes to matching bone on opposite side of X-Axis.");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	
	prop= RNA_def_property(srna, "auto_ik", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_AUTO_IK);
	RNA_def_property_ui_text(prop, "Auto IK", "Add temporaral IK constraints while grabbing bones in Pose Mode.");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	
	prop= RNA_def_property(srna, "draw_custom_bone_shapes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", ARM_NO_CUSTOM);
	RNA_def_property_ui_text(prop, "Draw Custom Bone Shapes", "Draw bones with their custom shapes.");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	
	prop= RNA_def_property(srna, "draw_group_colors", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_COL_CUSTOM);
	RNA_def_property_ui_text(prop, "Draw Bone Group Colors", "Draw bone group colors.");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	
// XXX depreceated ....... old animviz for armatures only
	prop= RNA_def_property(srna, "ghost_only_selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ARM_GHOST_ONLYSEL);
	RNA_def_property_ui_text(prop, "Draw Ghosts on Selected Bones Only", "");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
// XXX depreceated ....... old animviz for armatures only
	
		/* deformflag */
	prop= RNA_def_property(srna, "deform_vertexgroups", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_VGROUP);
	RNA_def_property_ui_text(prop, "Deform Vertex Groups", "Enable Vertex Groups when defining deform");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop= RNA_def_property(srna, "deform_envelope", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_ENVELOPE);
	RNA_def_property_ui_text(prop, "Deform Envelopes", "Enable Bone Envelopes when defining deform");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop= RNA_def_property(srna, "deform_quaternion", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_QUATERNION);
	RNA_def_property_ui_text(prop, "Use Dual Quaternion Deformation", "Enable deform rotation with Quaternions");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	prop= RNA_def_property(srna, "deform_bbone_rest", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_B_BONE_REST);
	RNA_def_property_ui_text(prop, "B-Bones Deform in Rest Position", "Make B-Bones deform already in Rest Position");
	RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	//prop= RNA_def_property(srna, "deform_invert_vertexgroups", PROP_BOOLEAN, PROP_NONE);
	//RNA_def_property_boolean_negative_sdna(prop, NULL, "deformflag", ARM_DEF_INVERT_VGROUP);
	//RNA_def_property_ui_text(prop, "Invert Vertex Group Influence", "Invert Vertex Group influence (only for Modifiers)");
	//RNA_def_property_update(prop, 0, "rna_Armature_update_data");
	
	/* Number fields */
// XXX depreceated ....... old animviz for armatures only
		/* ghost/onionskining settings */
	prop= RNA_def_property(srna, "ghost_step", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ghostep");
	RNA_def_property_range(prop, 0, 30);
	RNA_def_property_ui_text(prop, "Ghosting Step", "Number of frame steps on either side of current frame to show as ghosts (only for 'Around Current Frame' Onion-skining method).");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	
	prop= RNA_def_property(srna, "ghost_size", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ghostsize");
	RNA_def_property_range(prop, 1, 20);
	RNA_def_property_ui_text(prop, "Ghosting Frame Step", "Frame step for Ghosts (not for 'On Keyframes' Onion-skining method).");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	
	prop= RNA_def_property(srna, "ghost_start_frame", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "ghostsf");
	RNA_def_property_int_funcs(prop, NULL, "rna_Armature_ghost_start_frame_set", NULL);
	RNA_def_property_ui_text(prop, "Ghosting Start Frame", "Starting frame of range of Ghosts to display (not for 'Around Current Frame' Onion-skinning method).");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	
	prop= RNA_def_property(srna, "ghost_end_frame", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "ghostef");
	RNA_def_property_int_funcs(prop, NULL, "rna_Armature_ghost_end_frame_set", NULL);
	RNA_def_property_ui_text(prop, "Ghosting End Frame", "End frame of range of Ghosts to display (not for 'Around Current Frame' Onion-skinning method).");
	RNA_def_property_update(prop, 0, "rna_Armature_redraw_data");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
// XXX depreceated ....... old animviz for armatures only	
}

void RNA_def_armature(BlenderRNA *brna)
{
	rna_def_armature(brna);
	rna_def_bone(brna);
	rna_def_edit_bone(brna);
}

#endif
