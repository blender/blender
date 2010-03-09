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
 * Contributor(s): Blender Foundation (2009), Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

/* exported for use in API */
EnumPropertyItem keyingset_path_grouping_items[] = {
	{KSP_GROUP_NAMED, "NAMED", 0, "Named Group", ""},
	{KSP_GROUP_NONE, "NONE", 0, "None", ""},
	{KSP_GROUP_KSNAME, "KEYINGSET", 0, "Keying Set Name", ""},
	{KSP_GROUP_TEMPLATE_ITEM, "TEMPLATE", 0, "Innermost Context-Item Name", ""},
	{0, NULL, 0, NULL, NULL}};

#ifdef RNA_RUNTIME

static int rna_AnimData_action_editable(PointerRNA *ptr)
{
	AnimData *adt= (AnimData *)ptr->data;
	
	/* active action is only editable when it is not a tweaking strip */
	if ((adt->flag & ADT_NLA_EDIT_ON) || (adt->actstrip) || (adt->tmpact))
		return 0;
	else
		return 1;
}

static void rna_AnimData_action_set(PointerRNA *ptr, PointerRNA value)
{
	AnimData *adt= (AnimData*)(ptr->data);
	adt->action= value.data;
}

/* ****************************** */

static StructRNA *rna_ksPath_id_typef(PointerRNA *ptr)
{
	KS_Path *ksp= (KS_Path*)ptr->data;
	return ID_code_to_RNA_type(ksp->idtype);
}

static int rna_ksPath_id_editable(PointerRNA *ptr)
{
	KS_Path *ksp= (KS_Path*)ptr->data;
	return (ksp->idtype)? PROP_EDITABLE : 0;
}

static void rna_ksPath_id_type_set(PointerRNA *ptr, int value)
{
	KS_Path *data= (KS_Path*)(ptr->data);
	
	/* set the driver type, then clear the id-block if the type is invalid */
	data->idtype= value;
	if ((data->id) && (GS(data->id->name) != data->idtype))
		data->id= NULL;
}

static void rna_ksPath_RnaPath_get(PointerRNA *ptr, char *value)
{
	KS_Path *ksp= (KS_Path *)ptr->data;

	if (ksp->rna_path)
		strcpy(value, ksp->rna_path);
	else
		strcpy(value, "");
}

static int rna_ksPath_RnaPath_length(PointerRNA *ptr)
{
	KS_Path *ksp= (KS_Path *)ptr->data;
	
	if (ksp->rna_path)
		return strlen(ksp->rna_path);
	else
		return 0;
}

static void rna_ksPath_RnaPath_set(PointerRNA *ptr, const char *value)
{
	KS_Path *ksp= (KS_Path *)ptr->data;

	if (ksp->rna_path)
		MEM_freeN(ksp->rna_path);
	
	if (strlen(value))
		ksp->rna_path= BLI_strdup(value);
	else 
		ksp->rna_path= NULL;
}

/* ****************************** */

static int rna_KeyingSet_active_ksPath_editable(PointerRNA *ptr)
{
	KeyingSet *ks= (KeyingSet *)ptr->data;
	
	/* only editable if there are some paths to change to */
	return (ks->paths.first != NULL);
}

static PointerRNA rna_KeyingSet_active_ksPath_get(PointerRNA *ptr)
{
	KeyingSet *ks= (KeyingSet *)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_KeyingSetPath, BLI_findlink(&ks->paths, ks->active_path-1));
}

static void rna_KeyingSet_active_ksPath_set(PointerRNA *ptr, PointerRNA value)
{
	KeyingSet *ks= (KeyingSet *)ptr->data;
	KS_Path *ksp= (KS_Path*)value.data;
	ks->active_path= BLI_findindex(&ks->paths, ksp) + 1;
}

static int rna_KeyingSet_active_ksPath_index_get(PointerRNA *ptr)
{
	KeyingSet *ks= (KeyingSet *)ptr->data;
	return MAX2(ks->active_path-1, 0);
}

static void rna_KeyingSet_active_ksPath_index_set(PointerRNA *ptr, int value)
{
	KeyingSet *ks= (KeyingSet *)ptr->data;
	ks->active_path= value+1;
}

static void rna_KeyingSet_active_ksPath_index_range(PointerRNA *ptr, int *min, int *max)
{
	KeyingSet *ks= (KeyingSet *)ptr->data;

	*min= 0;
	*max= BLI_countlist(&ks->paths)-1;
	*max= MAX2(0, *max);
}

#else


static void rna_def_keyingset_path(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "KeyingSetPath", NULL);
	RNA_def_struct_sdna(srna, "KS_Path");
	RNA_def_struct_ui_text(srna, "Keying Set Path", "Path to a setting for use in a Keying Set");
	
	/* ID */
	prop= RNA_def_property(srna, "id", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ID");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_editable_func(prop, "rna_ksPath_id_editable");
	RNA_def_property_pointer_funcs(prop, NULL, NULL, "rna_ksPath_id_typef");
	RNA_def_property_ui_text(prop, "ID-Block", "ID-Block that keyframes for Keying Set should be added to (for Absolute Keying Sets only)");
	
	prop= RNA_def_property(srna, "id_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "idtype");
	RNA_def_property_enum_items(prop, id_type_items);
	RNA_def_property_enum_default(prop, ID_OB);
	RNA_def_property_enum_funcs(prop, NULL, "rna_ksPath_id_type_set", NULL);
	RNA_def_property_ui_text(prop, "ID Type", "Type of ID-block that can be used");
	
	/* Group */
	prop= RNA_def_property(srna, "group", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Group Name", "Name of Action Group to assign setting(s) for this path to");
	
	/* Grouping */
	prop= RNA_def_property(srna, "grouping", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "groupmode");
	RNA_def_property_enum_items(prop, keyingset_path_grouping_items);
	RNA_def_property_ui_text(prop, "Grouping Method", "Method used to define which Group-name to use");
	
	/* Path + Array Index */
	prop= RNA_def_property(srna, "data_path", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ksPath_RnaPath_get", "rna_ksPath_RnaPath_length", "rna_ksPath_RnaPath_set");
	RNA_def_property_ui_text(prop, "Data Path", "Path to property setting");
	RNA_def_struct_name_property(srna, prop); // XXX this is the best indicator for now...
	
	prop= RNA_def_property(srna, "array_index", PROP_INT, PROP_NONE);
	RNA_def_property_ui_text(prop, "RNA Array Index", "Index to the specific setting if applicable");
	
	/* Flags */
	prop= RNA_def_property(srna, "entire_array", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", KSP_FLAG_WHOLE_ARRAY);
	RNA_def_property_ui_text(prop, "Entire Array", "When an 'array/vector' type is chosen (Location, Rotation, Color, etc.), entire array is to be used");
}

static void rna_def_keyingset(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "KeyingSet", NULL);
	RNA_def_struct_ui_text(srna, "Keying Set", "Settings that should be keyframed together");
	
	/* Name */
	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);
	
	/* Paths */
	prop= RNA_def_property(srna, "paths", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "paths", NULL);
	RNA_def_property_struct_type(prop, "KeyingSetPath");
	RNA_def_property_ui_text(prop, "Paths", "Keying Set Paths to define settings that get keyframed together");
	
	prop= RNA_def_property(srna, "active_path", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "KeyingSetPath");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_editable_func(prop, "rna_KeyingSet_active_ksPath_editable");
	RNA_def_property_pointer_funcs(prop, "rna_KeyingSet_active_ksPath_get", "rna_KeyingSet_active_ksPath_set", NULL);
	RNA_def_property_ui_text(prop, "Active Keying Set", "Active Keying Set used to insert/delete keyframes");
	
	prop= RNA_def_property(srna, "active_path_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "active_path");
	RNA_def_property_int_funcs(prop, "rna_KeyingSet_active_ksPath_index_get", "rna_KeyingSet_active_ksPath_index_set", "rna_KeyingSet_active_ksPath_index_range");
	RNA_def_property_ui_text(prop, "Active Path Index", "Current Keying Set index");
	
	/* Flags */
		// XXX: depreceated
	prop= RNA_def_property(srna, "builtin", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", KEYINGSET_BUILTIN);
	RNA_def_property_ui_text(prop, "Built-In", "Keying Set is a built-in to Blender");
	
	prop= RNA_def_property(srna, "absolute", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", KEYINGSET_ABSOLUTE);
	RNA_def_property_ui_text(prop, "Absolute", "Keying Set defines specific paths/settings to be keyframed (i.e. is not reliant on context info)");
	
	/* Keyframing Flags */
	prop= RNA_def_property(srna, "insertkey_needed", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "keyingflag", INSERTKEY_NEEDED);
	RNA_def_property_ui_text(prop, "Insert Keyframes - Only Needed", "Only insert keyframes where they're needed in the relevant F-Curves");
	
	prop= RNA_def_property(srna, "insertkey_visual", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "keyingflag", INSERTKEY_MATRIX);
	RNA_def_property_ui_text(prop, "Insert Keyframes - Visual", "Insert keyframes based on 'visual transforms'");
	
	prop= RNA_def_property(srna, "insertkey_xyz_to_rgb", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "keyingflag", INSERTKEY_XYZ2RGB);
	RNA_def_property_ui_text(prop, "F-Curve Colors - XYZ to RGB", "Color for newly added transformation F-Curves (Location, Rotation, Scale) and also Color is based on the transform axis");
	
	/* Keying Set API */
	RNA_api_keyingset(srna);
}

/* --- */

void rna_def_animdata_common(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop= RNA_def_property(srna, "animation_data", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "adt");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Animation Data", "Animation data for this datablock");	
}

void rna_def_animdata(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "AnimData", NULL);
	RNA_def_struct_ui_text(srna, "Animation Data", "Animation data for datablock");
	
	/* NLA */
	prop= RNA_def_property(srna, "nla_tracks", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "nla_tracks", NULL);
	RNA_def_property_struct_type(prop, "NlaTrack");
	RNA_def_property_ui_text(prop, "NLA Tracks", "NLA Tracks (i.e. Animation Layers)");
	
	/* Active Action */
	prop= RNA_def_property(srna, "action", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_funcs(prop, NULL, "rna_AnimData_action_set", NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE); /* this flag as well as the dynamic test must be defined for this to be editable... */
	RNA_def_property_editable_func(prop, "rna_AnimData_action_editable");
	RNA_def_property_ui_text(prop, "Action", "Active Action for this datablock");

	
	/* Active Action Settings */
	prop= RNA_def_property(srna, "action_extrapolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "act_extendmode");
	RNA_def_property_enum_items(prop, nla_mode_extend_items);
	RNA_def_property_ui_text(prop, "Action Extrapolation", "Action to take for gaps past the Active Action's range (when evaluating with NLA)");
	
	prop= RNA_def_property(srna, "action_blending", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "act_blendmode");
	RNA_def_property_enum_items(prop, nla_mode_blend_items);
	RNA_def_property_ui_text(prop, "Action Blending", "Method used for combining Active Action's result with result of NLA stack");
	
	prop= RNA_def_property(srna, "action_influence", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "act_influence");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Action Influence", "Amount the Active Action contributes to the result of the NLA stack");
	
	/* Drivers */
	prop= RNA_def_property(srna, "drivers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "drivers", NULL);
	RNA_def_property_struct_type(prop, "FCurve");
	RNA_def_property_ui_text(prop, "Drivers", "The Drivers/Expressions for this datablock");
	
	/* General Settings */
	prop= RNA_def_property(srna, "nla_enabled", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", ADT_NLA_EVAL_OFF);
	RNA_def_property_ui_text(prop, "NLA Evaluation Enabled", "NLA stack is evaluated when evaluating this block");
}

/* --- */

void RNA_def_animation(BlenderRNA *brna)
{
	rna_def_animdata(brna);
	
	rna_def_keyingset(brna);
	rna_def_keyingset_path(brna);
}

#endif
