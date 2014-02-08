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
 * Contributor(s): Blender Foundation (2009), Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_animation.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_types.h"

#include "ED_keyframing.h"

/* exported for use in API */
EnumPropertyItem keyingset_path_grouping_items[] = {
	{KSP_GROUP_NAMED, "NAMED", 0, "Named Group", ""},
	{KSP_GROUP_NONE, "NONE", 0, "None", ""},
	{KSP_GROUP_KSNAME, "KEYINGSET", 0, "Keying Set Name", ""},
	{0, NULL, 0, NULL, NULL}
};

/* It would be cool to get rid of this 'INSERTKEY_' prefix in 'py strings' values, but it would break existing
 * exported keyingset... :/
 */
EnumPropertyItem keying_flag_items[] = {
	{INSERTKEY_NEEDED, "INSERTKEY_NEEDED", 0, "Only Needed",
	                   "Only insert keyframes where they're needed in the relevant F-Curves"},
	{INSERTKEY_MATRIX, "INSERTKEY_VISUAL", 0, "Visual Keying",
	                   "Insert keyframes based on 'visual transforms'"},
	{INSERTKEY_XYZ2RGB, "INSERTKEY_XYZ_TO_RGB", 0, "XYZ=RGB Colors",
	                    "Color for newly added transformation F-Curves (Location, Rotation, Scale) "
	                    "and also Color is based on the transform axis"},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "BLI_math_base.h"

#include "BKE_animsys.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_nla.h"

#include "DNA_object_types.h"

#include "WM_api.h"

static void rna_AnimData_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	ID *id = ptr->id.data;
	
	/* tag for refresh so that scheduled updates (e.g. action changed) will 
	 * get computed and reflected in the scene [#34869] 
	 */
	DAG_id_tag_update(id, OB_RECALC_OB | OB_RECALC_DATA);
}

static int rna_AnimData_action_editable(PointerRNA *ptr)
{
	AnimData *adt = (AnimData *)ptr->data;
	
	/* active action is only editable when it is not a tweaking strip */
	if ((adt->flag & ADT_NLA_EDIT_ON) || (adt->actstrip) || (adt->tmpact))
		return 0;
	else
		return 1;
}

static void rna_AnimData_action_set(PointerRNA *ptr, PointerRNA value)
{
	ID *ownerId = (ID *)ptr->id.data;
	AnimData *adt;
	
	/* set action */
	BKE_animdata_set_action(NULL, ownerId, value.data);
	
	/* force action to get evaluated [#34869] */
	adt = BKE_animdata_from_id(ownerId);
	if (adt) {
		adt->recalc |= ADT_RECALC_ANIM;
	}
}

/* ****************************** */

/* wrapper for poll callback */
static int RKS_POLL_rna_internal(KeyingSetInfo *ksi, bContext *C)
{
	extern FunctionRNA rna_KeyingSetInfo_poll_func;

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;
	void *ret;
	int ok;

	RNA_pointer_create(NULL, ksi->ext.srna, ksi, &ptr);
	func = &rna_KeyingSetInfo_poll_func; /* RNA_struct_find_function(&ptr, "poll"); */

	RNA_parameter_list_create(&list, &ptr, func);
	{
		/* hook up arguments */
		RNA_parameter_set_lookup(&list, "ksi", &ksi);
		RNA_parameter_set_lookup(&list, "context", &C);
		
		/* execute the function */
		ksi->ext.call(C, &ptr, func, &list);
		
		/* read the result */
		RNA_parameter_get_lookup(&list, "ok", &ret);
		ok = *(int *)ret;
	}
	RNA_parameter_list_free(&list);
	
	return ok;
}

/* wrapper for iterator callback */
static void RKS_ITER_rna_internal(KeyingSetInfo *ksi, bContext *C, KeyingSet *ks)
{
	extern FunctionRNA rna_KeyingSetInfo_iterator_func;

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, ksi->ext.srna, ksi, &ptr);
	func = &rna_KeyingSetInfo_iterator_func; /* RNA_struct_find_function(&ptr, "poll"); */

	RNA_parameter_list_create(&list, &ptr, func);
	{
		/* hook up arguments */
		RNA_parameter_set_lookup(&list, "ksi", &ksi);
		RNA_parameter_set_lookup(&list, "context", &C);
		RNA_parameter_set_lookup(&list, "ks", &ks);
		
		/* execute the function */
		ksi->ext.call(C, &ptr, func, &list);
	}
	RNA_parameter_list_free(&list);
}

/* wrapper for generator callback */
static void RKS_GEN_rna_internal(KeyingSetInfo *ksi, bContext *C, KeyingSet *ks, PointerRNA *data)
{
	extern FunctionRNA rna_KeyingSetInfo_generate_func;

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, ksi->ext.srna, ksi, &ptr);
	func = &rna_KeyingSetInfo_generate_func; /* RNA_struct_find_generate(&ptr, "poll"); */

	RNA_parameter_list_create(&list, &ptr, func);
	{
		/* hook up arguments */
		RNA_parameter_set_lookup(&list, "ksi", &ksi);
		RNA_parameter_set_lookup(&list, "context", &C);
		RNA_parameter_set_lookup(&list, "ks", &ks);
		RNA_parameter_set_lookup(&list, "data", data);
		
		/* execute the function */
		ksi->ext.call(C, &ptr, func, &list);
	}
	RNA_parameter_list_free(&list);
}

/* ------ */

/* XXX: the exact purpose of this is not too clear... maybe we want to revise this at some point? */
static StructRNA *rna_KeyingSetInfo_refine(PointerRNA *ptr)
{
	KeyingSetInfo *ksi = (KeyingSetInfo *)ptr->data;
	return (ksi->ext.srna) ? ksi->ext.srna : &RNA_KeyingSetInfo;
}

static void rna_KeyingSetInfo_unregister(Main *bmain, StructRNA *type)
{
	KeyingSetInfo *ksi = RNA_struct_blender_type_get(type);

	if (ksi == NULL)
		return;
	
	/* free RNA data referencing this */
	RNA_struct_free_extension(type, &ksi->ext);
	RNA_struct_free(&BLENDER_RNA, type);
	
	WM_main_add_notifier(NC_WINDOW, NULL);

	/* unlink Blender-side data */
	ANIM_keyingset_info_unregister(bmain, ksi);
}

static StructRNA *rna_KeyingSetInfo_register(Main *bmain, ReportList *reports, void *data, const char *identifier,
                                             StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	KeyingSetInfo dummyksi = {NULL};
	KeyingSetInfo *ksi;
	PointerRNA dummyptr = {{NULL}};
	int have_function[3];

	/* setup dummy type info to store static properties in */
	/* TODO: perhaps we want to get users to register as if they're using 'KeyingSet' directly instead? */
	RNA_pointer_create(NULL, &RNA_KeyingSetInfo, &dummyksi, &dummyptr);
	
	/* validate the python class */
	if (validate(&dummyptr, data, have_function) != 0)
		return NULL;
	
	if (strlen(identifier) >= sizeof(dummyksi.idname)) {
		BKE_reportf(reports, RPT_ERROR, "Registering keying set info class: '%s' is too long, maximum length is %d",
		            identifier, (int)sizeof(dummyksi.idname));
		return NULL;
	}
	
	/* check if we have registered this info before, and remove it */
	ksi = ANIM_keyingset_info_find_name(dummyksi.idname);
	if (ksi && ksi->ext.srna)
		rna_KeyingSetInfo_unregister(bmain, ksi->ext.srna);
	
	/* create a new KeyingSetInfo type */
	ksi = MEM_callocN(sizeof(KeyingSetInfo), "python keying set info");
	memcpy(ksi, &dummyksi, sizeof(KeyingSetInfo));
	
	/* set RNA-extensions info */
	ksi->ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, ksi->idname, &RNA_KeyingSetInfo);
	ksi->ext.data = data;
	ksi->ext.call = call;
	ksi->ext.free = free;
	RNA_struct_blender_type_set(ksi->ext.srna, ksi);
	
	/* set callbacks */
	/* NOTE: we really should have all of these...  */
	ksi->poll = (have_function[0]) ? RKS_POLL_rna_internal : NULL;
	ksi->iter = (have_function[1]) ? RKS_ITER_rna_internal : NULL;
	ksi->generate = (have_function[2]) ? RKS_GEN_rna_internal : NULL;
	
	/* add and register with other info as needed */
	ANIM_keyingset_info_register(ksi);
	
	WM_main_add_notifier(NC_WINDOW, NULL);

	/* return the struct-rna added */
	return ksi->ext.srna;
}

/* ****************************** */

static StructRNA *rna_ksPath_id_typef(PointerRNA *ptr)
{
	KS_Path *ksp = (KS_Path *)ptr->data;
	return ID_code_to_RNA_type(ksp->idtype);
}

static int rna_ksPath_id_editable(PointerRNA *ptr)
{
	KS_Path *ksp = (KS_Path *)ptr->data;
	return (ksp->idtype) ? PROP_EDITABLE : 0;
}

static void rna_ksPath_id_type_set(PointerRNA *ptr, int value)
{
	KS_Path *data = (KS_Path *)(ptr->data);
	
	/* set the driver type, then clear the id-block if the type is invalid */
	data->idtype = value;
	if ((data->id) && (GS(data->id->name) != data->idtype))
		data->id = NULL;
}

static void rna_ksPath_RnaPath_get(PointerRNA *ptr, char *value)
{
	KS_Path *ksp = (KS_Path *)ptr->data;

	if (ksp->rna_path)
		strcpy(value, ksp->rna_path);
	else
		value[0] = '\0';
}

static int rna_ksPath_RnaPath_length(PointerRNA *ptr)
{
	KS_Path *ksp = (KS_Path *)ptr->data;
	
	if (ksp->rna_path)
		return strlen(ksp->rna_path);
	else
		return 0;
}

static void rna_ksPath_RnaPath_set(PointerRNA *ptr, const char *value)
{
	KS_Path *ksp = (KS_Path *)ptr->data;

	if (ksp->rna_path)
		MEM_freeN(ksp->rna_path);
	
	if (value[0])
		ksp->rna_path = BLI_strdup(value);
	else
		ksp->rna_path = NULL;
}

/* ****************************** */

static void rna_KeyingSet_name_set(PointerRNA *ptr, const char *value)
{
	KeyingSet *ks = (KeyingSet *)ptr->data;
	
	/* update names of corresponding groups if name changes */
	if (strcmp(ks->name, value)) {
		KS_Path *ksp;
		
		for (ksp = ks->paths.first; ksp; ksp = ksp->next) {
			if ((ksp->groupmode == KSP_GROUP_KSNAME) && (ksp->id)) {
				AnimData *adt = BKE_animdata_from_id(ksp->id);
				
				/* TODO: NLA strips? */
				if (adt && adt->action) {
					bActionGroup *agrp;
					
					/* lazy check - should really find the F-Curve for the affected path and check its group 
					 * but this way should be faster and work well for most cases, as long as there are no
					 * conflicts
					 */
					for (agrp = adt->action->groups.first; agrp; agrp = agrp->next) {
						if (strcmp(ks->name, agrp->name) == 0) {
							/* there should only be one of these in the action, so can stop... */
							BLI_strncpy(agrp->name, value, sizeof(agrp->name));
							break;
						}
					}
				}
			}
		}
	}
	
	/* finally, update name to new value */
	BLI_strncpy(ks->name, value, sizeof(ks->name));
}


static int rna_KeyingSet_active_ksPath_editable(PointerRNA *ptr)
{
	KeyingSet *ks = (KeyingSet *)ptr->data;
	
	/* only editable if there are some paths to change to */
	return (BLI_listbase_is_empty(&ks->paths) == false);
}

static PointerRNA rna_KeyingSet_active_ksPath_get(PointerRNA *ptr)
{
	KeyingSet *ks = (KeyingSet *)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_KeyingSetPath, BLI_findlink(&ks->paths, ks->active_path - 1));
}

static void rna_KeyingSet_active_ksPath_set(PointerRNA *ptr, PointerRNA value)
{
	KeyingSet *ks = (KeyingSet *)ptr->data;
	KS_Path *ksp = (KS_Path *)value.data;
	ks->active_path = BLI_findindex(&ks->paths, ksp) + 1;
}

static int rna_KeyingSet_active_ksPath_index_get(PointerRNA *ptr)
{
	KeyingSet *ks = (KeyingSet *)ptr->data;
	return MAX2(ks->active_path - 1, 0);
}

static void rna_KeyingSet_active_ksPath_index_set(PointerRNA *ptr, int value)
{
	KeyingSet *ks = (KeyingSet *)ptr->data;
	ks->active_path = value + 1;
}

static void rna_KeyingSet_active_ksPath_index_range(PointerRNA *ptr, int *min, int *max,
                                                    int *UNUSED(softmin), int *UNUSED(softmax))
{
	KeyingSet *ks = (KeyingSet *)ptr->data;

	*min = 0;
	*max = max_ii(0, BLI_countlist(&ks->paths) - 1);
}

static PointerRNA rna_KeyingSet_typeinfo_get(PointerRNA *ptr)
{
	KeyingSet *ks = (KeyingSet *)ptr->data;
	KeyingSetInfo *ksi = NULL;
	
	/* keying set info is only for builtin Keying Sets */
	if ((ks->flag & KEYINGSET_ABSOLUTE) == 0)
		ksi = ANIM_keyingset_info_find_name(ks->typeinfo);
	return rna_pointer_inherit_refine(ptr, &RNA_KeyingSetInfo, ksi);
}



static KS_Path *rna_KeyingSet_paths_add(KeyingSet *keyingset, ReportList *reports,
                                        ID *id, const char rna_path[], int index, int group_method, const char group_name[])
{
	KS_Path *ksp = NULL;
	short flag = 0;
	
	/* special case when index = -1, we key the whole array (as with other places where index is used) */
	if (index == -1) {
		flag |= KSP_FLAG_WHOLE_ARRAY;
		index = 0;
	}
	
	/* if data is valid, call the API function for this */
	if (keyingset) {
		ksp = BKE_keyingset_add_path(keyingset, id, group_name, rna_path, index, flag, group_method);
		keyingset->active_path = BLI_countlist(&keyingset->paths);
	}
	else {
		BKE_report(reports, RPT_ERROR, "Keying set path could not be added");
	}
	
	/* return added path */
	return ksp;
}

static void rna_KeyingSet_paths_remove(KeyingSet *keyingset, ReportList *reports, PointerRNA *ksp_ptr)
{
	KS_Path *ksp = ksp_ptr->data;

	/* if data is valid, call the API function for this */
	if ((keyingset && ksp) == FALSE) {
		BKE_report(reports, RPT_ERROR, "Keying set path could not be removed");
		return;
	}

	/* remove the active path from the KeyingSet */
	BKE_keyingset_free_path(keyingset, ksp);
	RNA_POINTER_INVALIDATE(ksp_ptr);

	/* the active path number will most likely have changed */
	/* TODO: we should get more fancy and actually check if it was removed, but this will do for now */
	keyingset->active_path = 0;
}

static void rna_KeyingSet_paths_clear(KeyingSet *keyingset, ReportList *reports)
{
	/* if data is valid, call the API function for this */
	if (keyingset) {
		KS_Path *ksp, *kspn;
		
		/* free each path as we go to avoid looping twice */
		for (ksp = keyingset->paths.first; ksp; ksp = kspn) {
			kspn = ksp->next;
			BKE_keyingset_free_path(keyingset, ksp);
		}
			
		/* reset the active path, since there aren't any left */
		keyingset->active_path = 0;
	}
	else {
		BKE_report(reports, RPT_ERROR, "Keying set paths could not be removed");
	}
}

/* needs wrapper function to push notifier */
static NlaTrack *rna_NlaTrack_new(AnimData *adt, bContext *C, NlaTrack *track)
{
	NlaTrack *new_track = add_nlatrack(adt, track);

	WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_ADDED, NULL);

	return new_track;
}

static void rna_NlaTrack_remove(AnimData *adt, bContext *C, ReportList *reports, PointerRNA *track_ptr)
{
	NlaTrack *track = track_ptr->data;

	if (BLI_findindex(&adt->nla_tracks, track) == -1) {
		BKE_reportf(reports, RPT_ERROR, "NlaTrack '%s' cannot be removed", track->name);
		return;
	}

	free_nlatrack(&adt->nla_tracks, track);
	RNA_POINTER_INVALIDATE(track_ptr);

	WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_REMOVED, NULL);
}

static PointerRNA rna_NlaTrack_active_get(PointerRNA *ptr)
{
	AnimData *adt = (AnimData *)ptr->data;
	NlaTrack *track = BKE_nlatrack_find_active(&adt->nla_tracks);
	return rna_pointer_inherit_refine(ptr, &RNA_NlaTrack, track);
}

static void rna_NlaTrack_active_set(PointerRNA *ptr, PointerRNA value)
{
	AnimData *adt = (AnimData *)ptr->data;
	NlaTrack *track = (NlaTrack *)value.data;
	BKE_nlatrack_set_active(&adt->nla_tracks, track);
}


static FCurve *rna_Driver_from_existing(AnimData *adt, bContext *C, FCurve *src_driver)
{
	/* verify that we've got a driver to duplicate */
	if (ELEM(NULL, src_driver, src_driver->driver)) {
		BKE_report(CTX_wm_reports(C), RPT_ERROR, "No valid driver data to create copy of");
		return NULL;
	}
	else {
		/* just make a copy of the existing one and add to self */
		FCurve *new_fcu = copy_fcurve(src_driver);
		
		/* XXX: if we impose any ordering on these someday, this will be problematic */
		BLI_addtail(&adt->drivers, new_fcu);
		return new_fcu;
	}
}

#else

/* helper function for Keying Set -> keying settings */
/* TODO: use reg option! */
static void rna_def_common_keying_flags(StructRNA *srna, short UNUSED(reg))
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "bl_options", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "keyingflag");
	RNA_def_property_enum_items(prop, keying_flag_items);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL | PROP_ENUM_FLAG);
	RNA_def_property_ui_text(prop, "Options",  "Keying set options");
}

/* --- */

/* To avoid repeating it twice! */
#define KEYINGSET_IDNAME_DOC \
    "If this is set, the Keying Set gets a custom ID, otherwise it takes " \
    "the name of the class used to define the Keying Set (for example, "   \
    "if the class name is \"BUILTIN_KSI_location\", and bl_idname is not " \
    "set by the script, then bl_idname = \"BUILTIN_KSI_location\")"


static void rna_def_keyingset_info(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	PropertyRNA *parm;
	
	srna = RNA_def_struct(brna, "KeyingSetInfo", NULL);
	RNA_def_struct_sdna(srna, "KeyingSetInfo");
	RNA_def_struct_ui_text(srna, "Keying Set Info", "Callback function defines for builtin Keying Sets");
	RNA_def_struct_refine_func(srna, "rna_KeyingSetInfo_refine");
	RNA_def_struct_register_funcs(srna, "rna_KeyingSetInfo_register", "rna_KeyingSetInfo_unregister", NULL);
	
	/* Properties --------------------- */
	
	RNA_define_verify_sdna(0); /* not in sdna */
	
	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "idname");
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "ID Name", KEYINGSET_IDNAME_DOC);
	
	prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "UI Name", "");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_flag(prop, PROP_REGISTER);
	
	prop = RNA_def_property(srna, "bl_description", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "description");
	RNA_def_property_string_maxlength(prop, RNA_DYN_DESCR_MAX); /* else it uses the pointer size! */
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
	RNA_def_property_ui_text(prop, "Description", "A short description of the keying set");
	
	rna_def_common_keying_flags(srna, 1); /* '1' arg here is to indicate that we need these to be set on registering */
	
	RNA_define_verify_sdna(1);
	
	/* Function Callbacks ------------- */
	/* poll */
	func = RNA_def_function(srna, "poll", NULL);
	RNA_def_function_ui_description(func, "Test if Keying Set can be used or not");
	RNA_def_function_flag(func, FUNC_REGISTER);
	RNA_def_function_return(func, RNA_def_boolean(func, "ok", 1, "", ""));
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	
	/* iterator */
	func = RNA_def_function(srna, "iterator", NULL);
	RNA_def_function_ui_description(func, "Call generate() on the structs which have properties to be keyframed");
	RNA_def_function_flag(func, FUNC_REGISTER);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "ks", "KeyingSet", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	
	/* generate */
	func = RNA_def_function(srna, "generate", NULL);
	RNA_def_function_ui_description(func, "Add Paths to the Keying Set to keyframe the properties of the given data");
	RNA_def_function_flag(func, FUNC_REGISTER);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "ks", "KeyingSet", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "data", "AnyType", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR | PROP_NEVER_NULL);
}

static void rna_def_keyingset_path(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "KeyingSetPath", NULL);
	RNA_def_struct_sdna(srna, "KS_Path");
	RNA_def_struct_ui_text(srna, "Keying Set Path", "Path to a setting for use in a Keying Set");
	
	/* ID */
	prop = RNA_def_property(srna, "id", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ID");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_editable_func(prop, "rna_ksPath_id_editable");
	RNA_def_property_pointer_funcs(prop, NULL, NULL, "rna_ksPath_id_typef", NULL);
	RNA_def_property_ui_text(prop, "ID-Block",
	                         "ID-Block that keyframes for Keying Set should be added to "
	                         "(for Absolute Keying Sets only)");
	RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, NULL); /* XXX: maybe a bit too noisy */
	
	prop = RNA_def_property(srna, "id_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "idtype");
	RNA_def_property_enum_items(prop, id_type_items);
	RNA_def_property_enum_default(prop, ID_OB);
	RNA_def_property_enum_funcs(prop, NULL, "rna_ksPath_id_type_set", NULL);
	RNA_def_property_ui_text(prop, "ID Type", "Type of ID-block that can be used");
	RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, NULL); /* XXX: maybe a bit too noisy */
	
	/* Group */
	prop = RNA_def_property(srna, "group", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Group Name", "Name of Action Group to assign setting(s) for this path to");
	RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, NULL); /* XXX: maybe a bit too noisy */
	
	/* Grouping */
	prop = RNA_def_property(srna, "group_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "groupmode");
	RNA_def_property_enum_items(prop, keyingset_path_grouping_items);
	RNA_def_property_ui_text(prop, "Grouping Method", "Method used to define which Group-name to use");
	RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, NULL); /* XXX: maybe a bit too noisy */
	
	/* Path + Array Index */
	prop = RNA_def_property(srna, "data_path", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ksPath_RnaPath_get", "rna_ksPath_RnaPath_length",
	                              "rna_ksPath_RnaPath_set");
	RNA_def_property_ui_text(prop, "Data Path", "Path to property setting");
	RNA_def_struct_name_property(srna, prop); /* XXX this is the best indicator for now... */
	RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, NULL);

	/* called 'index' when given as function arg */
	prop = RNA_def_property(srna, "array_index", PROP_INT, PROP_NONE);
	RNA_def_property_ui_text(prop, "RNA Array Index", "Index to the specific setting if applicable");
	RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, NULL); /* XXX: maybe a bit too noisy */
	
	/* Flags */
	prop = RNA_def_property(srna, "use_entire_array", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", KSP_FLAG_WHOLE_ARRAY);
	RNA_def_property_ui_text(prop, "Entire Array",
	                         "When an 'array/vector' type is chosen (Location, Rotation, Color, etc.), "
	                         "entire array is to be used");
	RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, NULL); /* XXX: maybe a bit too noisy */
	
	/* Keyframing Settings */
	rna_def_common_keying_flags(srna, 0);
}


/* keyingset.paths */
static void rna_def_keyingset_paths(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	FunctionRNA *func;
	PropertyRNA *parm;
	
	PropertyRNA *prop;

	RNA_def_property_srna(cprop, "KeyingSetPaths");
	srna = RNA_def_struct(brna, "KeyingSetPaths", NULL);
	RNA_def_struct_sdna(srna, "KeyingSet");
	RNA_def_struct_ui_text(srna, "Keying set paths", "Collection of keying set paths");

	
	/* Add Path */
	func = RNA_def_function(srna, "add", "rna_KeyingSet_paths_add");
	RNA_def_function_ui_description(func, "Add a new path for the Keying Set");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	/* return arg */
	parm = RNA_def_pointer(func, "ksp", "KeyingSetPath", "New Path", "Path created and added to the Keying Set");
	RNA_def_function_return(func, parm);
	/* ID-block for target */
	parm = RNA_def_pointer(func, "target_id", "ID", "Target ID", "ID-Datablock for the destination");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* rna-path */
	/* XXX hopefully this is long enough */
	parm = RNA_def_string(func, "data_path", NULL, 256, "Data-Path", "RNA-Path to destination property");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* index (defaults to -1 for entire array) */
	RNA_def_int(func, "index", -1, -1, INT_MAX, "Index",
	            "The index of the destination property (i.e. axis of Location/Rotation/etc.), "
	            "or -1 for the entire array", 0, INT_MAX);
	/* grouping */
	RNA_def_enum(func, "group_method", keyingset_path_grouping_items, KSP_GROUP_KSNAME,
	             "Grouping Method", "Method used to define which Group-name to use");
	RNA_def_string(func, "group_name", NULL, 64, "Group Name",
	               "Name of Action Group to assign destination to (only if grouping mode is to use this name)");


	/* Remove Path */
	func = RNA_def_function(srna, "remove", "rna_KeyingSet_paths_remove");
	RNA_def_function_ui_description(func, "Remove the given path from the Keying Set");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	/* path to remove */
	parm = RNA_def_pointer(func, "path", "KeyingSetPath", "Path", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);


	/* Remove All Paths */
	func = RNA_def_function(srna, "clear", "rna_KeyingSet_paths_clear");
	RNA_def_function_ui_description(func, "Remove all the paths from the Keying Set");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	
	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "KeyingSetPath");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_editable_func(prop, "rna_KeyingSet_active_ksPath_editable");
	RNA_def_property_pointer_funcs(prop, "rna_KeyingSet_active_ksPath_get",
	                               "rna_KeyingSet_active_ksPath_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Keying Set", "Active Keying Set used to insert/delete keyframes");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "active_path");
	RNA_def_property_int_funcs(prop, "rna_KeyingSet_active_ksPath_index_get", "rna_KeyingSet_active_ksPath_index_set",
	                           "rna_KeyingSet_active_ksPath_index_range");
	RNA_def_property_ui_text(prop, "Active Path Index", "Current Keying Set index");
}

static void rna_def_keyingset(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "KeyingSet", NULL);
	RNA_def_struct_ui_text(srna, "Keying Set", "Settings that should be keyframed together");
	
	/* Id/Label */
	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "idname");
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "ID Name", KEYINGSET_IDNAME_DOC);
/*	RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET | NA_RENAME, NULL); */ /* NOTE: disabled, as ID name shouldn't be editable */
	
	prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_KeyingSet_name_set");
	RNA_def_property_ui_text(prop, "UI Name", "");
	RNA_def_struct_ui_icon(srna, ICON_KEYINGSET);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET | NA_RENAME, NULL);
	
	prop = RNA_def_property(srna, "bl_description", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "description");
	RNA_def_property_string_maxlength(prop, RNA_DYN_DESCR_MAX); /* else it uses the pointer size! */
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
	RNA_def_property_ui_text(prop, "Description", "A short description of the keying set");
	
	/* KeyingSetInfo (Type Info) for Builtin Sets only  */
	prop = RNA_def_property(srna, "type_info", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "KeyingSetInfo");
	RNA_def_property_pointer_funcs(prop, "rna_KeyingSet_typeinfo_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Type Info", "Callback function defines for built-in Keying Sets");
	
	/* Paths */
	prop = RNA_def_property(srna, "paths", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "paths", NULL);
	RNA_def_property_struct_type(prop, "KeyingSetPath");
	RNA_def_property_ui_text(prop, "Paths", "Keying Set Paths to define settings that get keyframed together");
	rna_def_keyingset_paths(brna, prop);

	/* Flags */
	prop = RNA_def_property(srna, "is_path_absolute", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", KEYINGSET_ABSOLUTE);
	RNA_def_property_ui_text(prop, "Absolute",
	                         "Keying Set defines specific paths/settings to be keyframed "
	                         "(i.e. is not reliant on context info)");
	
	/* Keyframing Flags */
	rna_def_common_keying_flags(srna, 0);
	
	
	/* Keying Set API */
	RNA_api_keyingset(srna);
}

#undef KEYINGSET_IDNAME_DOC
/* --- */

static void rna_api_animdata_nla_tracks(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *parm;
	FunctionRNA *func;

	PropertyRNA *prop;
	
	RNA_def_property_srna(cprop, "NlaTracks");
	srna = RNA_def_struct(brna, "NlaTracks", NULL);
	RNA_def_struct_sdna(srna, "AnimData");
	RNA_def_struct_ui_text(srna, "NLA Tracks", "Collection of NLA Tracks");
	
	func = RNA_def_function(srna, "new", "rna_NlaTrack_new");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Add a new NLA Track");
	RNA_def_pointer(func, "prev", "NlaTrack", "", "NLA Track to add the new one after");
	/* return type */
	parm = RNA_def_pointer(func, "track", "NlaTrack", "", "New NLA Track");
	RNA_def_function_return(func, parm);
	
	func = RNA_def_function(srna, "remove", "rna_NlaTrack_remove");
	RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Remove a NLA Track");
	parm = RNA_def_pointer(func, "track", "NlaTrack", "", "NLA Track to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "NlaTrack");
	RNA_def_property_pointer_funcs(prop, "rna_NlaTrack_active_get", "rna_NlaTrack_active_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Active Constraint", "Active Object constraint");
	/* XXX: should (but doesn't) update the active track in the NLA window */
	RNA_def_property_update(prop, NC_ANIMATION | ND_NLA | NA_SELECTED, NULL);
}

static void rna_api_animdata_drivers(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *parm;
	FunctionRNA *func;

	/* PropertyRNA *prop; */
	
	RNA_def_property_srna(cprop, "AnimDataDrivers");
	srna = RNA_def_struct(brna, "AnimDataDrivers", NULL);
	RNA_def_struct_sdna(srna, "AnimData");
	RNA_def_struct_ui_text(srna, "Drivers", "Collection of Driver F-Curves");
	
	func = RNA_def_function(srna, "from_existing", "rna_Driver_from_existing");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Add a new driver given an existing one");
	RNA_def_pointer(func, "src_driver", "FCurve", "", "Existing Driver F-Curve to use as template for a new one");
	/* return type */
	parm = RNA_def_pointer(func, "driver", "FCurve", "", "New Driver F-Curve");
	RNA_def_function_return(func, parm);
}

void rna_def_animdata_common(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "animation_data", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "adt");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Animation Data", "Animation data for this datablock");
}

static void rna_def_animdata(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "AnimData", NULL);
	RNA_def_struct_ui_text(srna, "Animation Data", "Animation data for datablock");
	RNA_def_struct_ui_icon(srna, ICON_ANIM_DATA);
	
	/* NLA */
	prop = RNA_def_property(srna, "nla_tracks", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "nla_tracks", NULL);
	RNA_def_property_struct_type(prop, "NlaTrack");
	RNA_def_property_ui_text(prop, "NLA Tracks", "NLA Tracks (i.e. Animation Layers)");

	rna_api_animdata_nla_tracks(brna, prop);
	
	/* Active Action */
	prop = RNA_def_property(srna, "action", PROP_POINTER, PROP_NONE);
	/* this flag as well as the dynamic test must be defined for this to be editable... */
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
	RNA_def_property_pointer_funcs(prop, NULL, "rna_AnimData_action_set", NULL, "rna_Action_id_poll");
	RNA_def_property_editable_func(prop, "rna_AnimData_action_editable");
	RNA_def_property_ui_text(prop, "Action", "Active Action for this datablock");
	RNA_def_property_update(prop, NC_ANIMATION | ND_NLA_ACTCHANGE, "rna_AnimData_update");

	/* Active Action Settings */
	prop = RNA_def_property(srna, "action_extrapolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "act_extendmode");
	RNA_def_property_enum_items(prop, nla_mode_extend_items);
	RNA_def_property_ui_text(prop, "Action Extrapolation",
	                         "Action to take for gaps past the Active Action's range (when evaluating with NLA)");
	RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, NULL);
	
	prop = RNA_def_property(srna, "action_blend_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "act_blendmode");
	RNA_def_property_enum_items(prop, nla_mode_blend_items);
	RNA_def_property_ui_text(prop, "Action Blending",
	                         "Method used for combining Active Action's result with result of NLA stack");
	RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, NULL); /* this will do? */
	
	prop = RNA_def_property(srna, "action_influence", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "act_influence");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Action Influence",
	                         "Amount the Active Action contributes to the result of the NLA stack");
	RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, NULL); /* this will do? */
	
	/* Drivers */
	prop = RNA_def_property(srna, "drivers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "drivers", NULL);
	RNA_def_property_struct_type(prop, "FCurve");
	RNA_def_property_ui_text(prop, "Drivers", "The Drivers/Expressions for this datablock");
	
	rna_api_animdata_drivers(brna, prop);
	
	/* General Settings */
	prop = RNA_def_property(srna, "use_nla", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", ADT_NLA_EVAL_OFF);
	RNA_def_property_ui_text(prop, "NLA Evaluation Enabled", "NLA stack is evaluated when evaluating this block");
	RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, NULL); /* this will do? */
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
