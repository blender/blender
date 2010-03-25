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
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "ED_keyframing.h"

/* exported for use in API */
EnumPropertyItem keyingset_path_grouping_items[] = {
	{KSP_GROUP_NAMED, "NAMED", 0, "Named Group", ""},
	{KSP_GROUP_NONE, "NONE", 0, "None", ""},
	{KSP_GROUP_KSNAME, "KEYINGSET", 0, "Keying Set Name", ""},
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

/* wrapper for poll callback */
static int RKS_POLL_rna_internal(KeyingSetInfo *ksi, bContext *C)
{
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;
	void *ret;
	int ok;

	RNA_pointer_create(NULL, ksi->ext.srna, ksi, &ptr);
	func= RNA_struct_find_function(&ptr, "poll");

	RNA_parameter_list_create(&list, &ptr, func);
		/* hook up arguments */
		RNA_parameter_set_lookup(&list, "ksi", &ksi);
		RNA_parameter_set_lookup(&list, "context", &C);
		
		/* execute the function */
		ksi->ext.call(&ptr, func, &list);
		
		/* read the result */
		RNA_parameter_get_lookup(&list, "ok", &ret);
		ok= *(int*)ret;
	RNA_parameter_list_free(&list);
	
	return ok;
}

/* wrapper for iterator callback */
static void RKS_ITER_rna_internal(KeyingSetInfo *ksi, bContext *C, KeyingSet *ks)
{
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, ksi->ext.srna, ksi, &ptr);
	func= RNA_struct_find_function(&ptr, "iterator");

	RNA_parameter_list_create(&list, &ptr, func);
		/* hook up arguments */
		RNA_parameter_set_lookup(&list, "ksi", &ksi);
		RNA_parameter_set_lookup(&list, "context", &C);
		RNA_parameter_set_lookup(&list, "ks", &ks);
		
		/* execute the function */
		ksi->ext.call(&ptr, func, &list);
	RNA_parameter_list_free(&list);
}

/* wrapper for generator callback */
static void RKS_GEN_rna_internal(KeyingSetInfo *ksi, bContext *C, KeyingSet *ks, PointerRNA *data)
{
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, ksi->ext.srna, ksi, &ptr);
	func= RNA_struct_find_function(&ptr, "generate");

	RNA_parameter_list_create(&list, &ptr, func);
		/* hook up arguments */
		RNA_parameter_set_lookup(&list, "ksi", &ksi);
		RNA_parameter_set_lookup(&list, "context", &C);
		RNA_parameter_set_lookup(&list, "ks", &ks);
		RNA_parameter_set_lookup(&list, "data", data);
		
		/* execute the function */
		ksi->ext.call(&ptr, func, &list);
	RNA_parameter_list_free(&list);
}

/* ------ */

// XXX: the exact purpose of this is not too clear... maybe we want to revise this at some point?
static StructRNA *rna_KeyingSetInfo_refine(PointerRNA *ptr)
{
	KeyingSetInfo *ksi= (KeyingSetInfo *)ptr->data;
	return (ksi->ext.srna)? ksi->ext.srna: &RNA_KeyingSetInfo;
}

static void rna_KeyingSetInfo_unregister(const bContext *C, StructRNA *type)
{
	KeyingSetInfo *ksi= RNA_struct_blender_type_get(type);

	if (ksi == NULL)
		return;
	
	/* free RNA data referencing this */
	RNA_struct_free_extension(type, &ksi->ext);
	RNA_struct_free(&BLENDER_RNA, type);
	
	/* unlink Blender-side data */
	ANIM_keyingset_info_unregister(C, ksi);
}

static StructRNA *rna_KeyingSetInfo_register(const bContext *C, ReportList *reports, void *data, const char *identifier, StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	KeyingSetInfo dummyksi = {0};
	KeyingSetInfo *ksi;
	PointerRNA dummyptr;
	int have_function[3];

	/* setup dummy type info to store static properties in */
	// TODO: perhaps we want to get users to register as if they're using 'KeyingSet' directly instead?
	RNA_pointer_create(NULL, &RNA_KeyingSetInfo, &dummyksi, &dummyptr);
	
	/* validate the python class */
	if (validate(&dummyptr, data, have_function) != 0)
		return NULL;
	
	if (strlen(identifier) >= sizeof(dummyksi.idname)) {
		BKE_reportf(reports, RPT_ERROR, "registering keying set info class: '%s' is too long, maximum length is %d.", identifier, sizeof(dummyksi.idname));
		return NULL;
	}
	
	/* check if we have registered this info before, and remove it */
	ksi = ANIM_keyingset_info_find_named(dummyksi.idname);
	if (ksi && ksi->ext.srna)
		rna_KeyingSetInfo_unregister(C, ksi->ext.srna);
	
	/* create a new KeyingSetInfo type */
	ksi= MEM_callocN(sizeof(KeyingSetInfo), "python keying set info");
	memcpy(ksi, &dummyksi, sizeof(KeyingSetInfo));
	
	/* set RNA-extensions info */
	ksi->ext.srna= RNA_def_struct(&BLENDER_RNA, ksi->idname, "KeyingSetInfo"); 
	ksi->ext.data= data;
	ksi->ext.call= call;
	ksi->ext.free= free;
	RNA_struct_blender_type_set(ksi->ext.srna, ksi);
	
	/* set callbacks */
	// NOTE: we really should have all of these... 
	ksi->poll= (have_function[0])? RKS_POLL_rna_internal: NULL;
	ksi->iter= (have_function[1])? RKS_ITER_rna_internal: NULL;
	ksi->generate= (have_function[2])? RKS_GEN_rna_internal: NULL;

	/* add and register with other info as needed */
	ANIM_keyingset_info_register(C, ksi);
	
	/* return the struct-rna added */
	return ksi->ext.srna;
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

static PointerRNA rna_KeyingSet_typeinfo_get(PointerRNA *ptr)
{
	KeyingSet *ks= (KeyingSet *)ptr->data;
	KeyingSetInfo *ksi = NULL;
	
	/* keying set info is only for builtin Keying Sets */
	if ((ks->flag & KEYINGSET_ABSOLUTE)==0)
		ksi = ANIM_keyingset_info_find_named(ks->typeinfo);
	return rna_pointer_inherit_refine(ptr, &RNA_KeyingSetInfo, ksi);
}

#else

/* helper function for Keying Set -> keying settings */
static void rna_def_common_keying_flags(StructRNA *srna, short reg)
{
	PropertyRNA *prop;
	
	prop= RNA_def_property(srna, "insertkey_needed", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "keyingflag", INSERTKEY_NEEDED);
	RNA_def_property_ui_text(prop, "Insert Keyframes - Only Needed", "Only insert keyframes where they're needed in the relevant F-Curves");
	if (reg) RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
	
	prop= RNA_def_property(srna, "insertkey_visual", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "keyingflag", INSERTKEY_MATRIX);
	RNA_def_property_ui_text(prop, "Insert Keyframes - Visual", "Insert keyframes based on 'visual transforms'");
	if (reg) RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
	
	prop= RNA_def_property(srna, "insertkey_xyz_to_rgb", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "keyingflag", INSERTKEY_XYZ2RGB);
	RNA_def_property_ui_text(prop, "F-Curve Colors - XYZ to RGB", "Color for newly added transformation F-Curves (Location, Rotation, Scale) and also Color is based on the transform axis");
	if (reg) RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
}

/* --- */

static void rna_def_keyingset_info(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	PropertyRNA *parm;
	
	srna= RNA_def_struct(brna, "KeyingSetInfo", NULL);
	RNA_def_struct_sdna(srna, "KeyingSetInfo");
	RNA_def_struct_ui_text(srna, "Keying Set Info", "Callback function defines for builtin Keying Sets");
	RNA_def_struct_refine_func(srna, "rna_KeyingSetInfo_refine");
	RNA_def_struct_register_funcs(srna, "rna_KeyingSetInfo_register", "rna_KeyingSetInfo_unregister");
	
	/* Properties --------------------- */
	
	RNA_define_verify_sdna(0); // not in sdna
		
	prop= RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "idname");
	RNA_def_property_flag(prop, PROP_REGISTER);
		
	/* Name */
	prop= RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_flag(prop, PROP_REGISTER);
	
	rna_def_common_keying_flags(srna, 1); /* '1' arg here is to indicate that we need these to be set on registering */
	
	RNA_define_verify_sdna(1);
	
	/* Function Callbacks ------------- */
		/* poll */
	func= RNA_def_function(srna, "poll", NULL);
	RNA_def_function_ui_description(func, "Test if Keying Set can be used or not");
	RNA_def_function_flag(func, FUNC_REGISTER);
	RNA_def_function_return(func, RNA_def_boolean(func, "ok", 1, "", ""));
	parm= RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	
		/* iterator */
	func= RNA_def_function(srna, "iterator", NULL);
	RNA_def_function_ui_description(func, "Call generate() on the structs which have properties to be keyframed");
	RNA_def_function_flag(func, FUNC_REGISTER);
	parm= RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "ks", "KeyingSet", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	
		/* generate */
	func= RNA_def_function(srna, "generate", NULL);
	RNA_def_function_ui_description(func, "Add Paths to the Keying Set to keyframe the properties of the given data");
	RNA_def_function_flag(func, FUNC_REGISTER);
	parm= RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "ks", "KeyingSet", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "data", "AnyType", "", ""); 
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);
}

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
	
	/* Keyframing Settings */
	rna_def_common_keying_flags(srna, 0);
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
	RNA_def_struct_ui_icon(srna, ICON_KEY_HLT); // TODO: we need a dedicated icon
	RNA_def_struct_name_property(srna, prop);
	
	/* KeyingSetInfo (Type Info) for Builtin Sets only  */
	prop= RNA_def_property(srna, "type_info", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "KeyingSetInfo");
	RNA_def_property_pointer_funcs(prop, "rna_KeyingSet_typeinfo_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Type Info", "Callback function defines for builtin Keying Sets");
	
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
	prop= RNA_def_property(srna, "absolute", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", KEYINGSET_ABSOLUTE);
	RNA_def_property_ui_text(prop, "Absolute", "Keying Set defines specific paths/settings to be keyframed (i.e. is not reliant on context info)");	
	
	/* Keyframing Flags */
	rna_def_common_keying_flags(srna, 0);
	
	
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
	rna_def_keyingset_info(brna);
}

#endif
