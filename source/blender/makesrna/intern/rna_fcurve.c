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

/** \file blender/makesrna/intern/rna_fcurve.c
 *  \ingroup RNA
 */


#include <stdlib.h>

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_action.h"

#include "WM_types.h"

#include "ED_keyframing.h"
#include "ED_keyframes_edit.h"

EnumPropertyItem fmodifier_type_items[] = {
	{FMODIFIER_TYPE_NULL, "NULL", 0, "Invalid", ""},
	{FMODIFIER_TYPE_GENERATOR, "GENERATOR", 0, "Generator", ""},
	{FMODIFIER_TYPE_FN_GENERATOR, "FNGENERATOR", 0, "Built-In Function", ""},
	{FMODIFIER_TYPE_ENVELOPE, "ENVELOPE", 0, "Envelope", ""},
	{FMODIFIER_TYPE_CYCLES, "CYCLES", 0, "Cycles", ""},
	{FMODIFIER_TYPE_NOISE, "NOISE", 0, "Noise", ""},
	{FMODIFIER_TYPE_FILTER, "FILTER", 0, "Filter", ""},
	/*{FMODIFIER_TYPE_PYTHON, "PYTHON", 0, "Python", ""},	 *//* FIXME: not implemented yet! */
	{FMODIFIER_TYPE_LIMITS, "LIMITS", 0, "Limits", ""},
	{FMODIFIER_TYPE_STEPPED, "STEPPED", 0, "Stepped Interpolation", ""},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem beztriple_keyframe_type_items[] = {
	{BEZT_KEYTYPE_KEYFRAME, "KEYFRAME", 0, "Keyframe", ""},
	{BEZT_KEYTYPE_BREAKDOWN, "BREAKDOWN", 0, "Breakdown", ""},
	{BEZT_KEYTYPE_EXTREME, "EXTREME", 0, "Extreme", ""},
	{BEZT_KEYTYPE_JITTER, "JITTER", 0, "Jitter", ""},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "WM_api.h"

static StructRNA *rna_FModifierType_refine(struct PointerRNA *ptr)
{
	FModifier *fcm = (FModifier *)ptr->data;

	switch (fcm->type) {
		case FMODIFIER_TYPE_GENERATOR:
			return &RNA_FModifierGenerator;
		case FMODIFIER_TYPE_FN_GENERATOR:
			return &RNA_FModifierFunctionGenerator;
		case FMODIFIER_TYPE_ENVELOPE:
			return &RNA_FModifierEnvelope;
		case FMODIFIER_TYPE_CYCLES:
			return &RNA_FModifierCycles;
		case FMODIFIER_TYPE_NOISE:
			return &RNA_FModifierNoise;
		/*case FMODIFIER_TYPE_FILTER: */
		/*	return &RNA_FModifierFilter; */
		case FMODIFIER_TYPE_PYTHON:
			return &RNA_FModifierPython;
		case FMODIFIER_TYPE_LIMITS:
			return &RNA_FModifierLimits;
		case FMODIFIER_TYPE_STEPPED:
			return &RNA_FModifierStepped;
		default:
			return &RNA_UnknownType;
	}
}

/* ****************************** */

#include "BKE_fcurve.h"
#include "BKE_depsgraph.h"
#include "BKE_animsys.h"

static void rna_ChannelDriver_update_data(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	ID *id = ptr->id.data;
	ChannelDriver *driver = ptr->data;

	driver->flag &= ~DRIVER_FLAG_INVALID;
	
	/* TODO: this really needs an update guard... */
	DAG_scene_sort(bmain, scene);
	DAG_id_tag_update(id, OB_RECALC_OB | OB_RECALC_DATA);
	
	WM_main_add_notifier(NC_SCENE | ND_FRAME, scene);
}

static void rna_ChannelDriver_update_expr(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	ChannelDriver *driver = ptr->data;
	driver->flag |= DRIVER_FLAG_RECOMPILE;
	rna_ChannelDriver_update_data(bmain, scene, ptr);
}

static void rna_DriverTarget_update_data(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	PointerRNA driverptr;
	ChannelDriver *driver;
	FCurve *fcu;
	AnimData *adt = BKE_animdata_from_id(ptr->id.data);

	/* find the driver this belongs to and update it */
	for (fcu = adt->drivers.first; fcu; fcu = fcu->next) {
		driver = fcu->driver;
		fcu->flag &= ~FCURVE_DISABLED;
		
		if (driver) {
			/* FIXME: need to be able to search targets for required one... */
			/*BLI_findindex(&driver->targets, ptr->data) != -1)  */
			RNA_pointer_create(ptr->id.data, &RNA_Driver, driver, &driverptr);
			rna_ChannelDriver_update_data(bmain, scene, &driverptr);
			return;
		}
	}
}

static void rna_DriverTarget_update_name(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	ChannelDriver *driver = ptr->data;
	rna_DriverTarget_update_data(bmain, scene, ptr);

	driver->flag |= DRIVER_FLAG_RENAMEVAR;

}

/* ----------- */

/* note: this function exists only to avoid id refcounting */
static void rna_DriverTarget_id_set(PointerRNA *ptr, PointerRNA value)
{
	DriverTarget *dtar = (DriverTarget *)ptr->data;
	dtar->id = value.data;
}

static StructRNA *rna_DriverTarget_id_typef(PointerRNA *ptr)
{
	DriverTarget *dtar = (DriverTarget *)ptr->data;
	return ID_code_to_RNA_type(dtar->idtype);
}

static int rna_DriverTarget_id_editable(PointerRNA *ptr)
{
	DriverTarget *dtar = (DriverTarget *)ptr->data;
	return (dtar->idtype) ? PROP_EDITABLE : 0;
}

static int rna_DriverTarget_id_type_editable(PointerRNA *ptr)
{
	DriverTarget *dtar = (DriverTarget *)ptr->data;
	
	/* when the id-type can only be object, don't allow editing
	 * otherwise, there may be strange crashes
	 */
	return ((dtar->flag & DTAR_FLAG_ID_OB_ONLY) == 0);
}

static void rna_DriverTarget_id_type_set(PointerRNA *ptr, int value)
{
	DriverTarget *data = (DriverTarget *)(ptr->data);
	
	/* check if ID-type is settable */
	if ((data->flag & DTAR_FLAG_ID_OB_ONLY) == 0) {
		/* change ID-type to the new type */
		data->idtype = value;
	}
	else {
		/* make sure ID-type is Object */
		data->idtype = ID_OB;
	}
	
	/* clear the id-block if the type is invalid */
	if ((data->id) && (GS(data->id->name) != data->idtype))
		data->id = NULL;
}

static void rna_DriverTarget_RnaPath_get(PointerRNA *ptr, char *value)
{
	DriverTarget *dtar = (DriverTarget *)ptr->data;

	if (dtar->rna_path)
		strcpy(value, dtar->rna_path);
	else
		value[0] = '\0';
}

static int rna_DriverTarget_RnaPath_length(PointerRNA *ptr)
{
	DriverTarget *dtar = (DriverTarget *)ptr->data;
	
	if (dtar->rna_path)
		return strlen(dtar->rna_path);
	else
		return 0;
}

static void rna_DriverTarget_RnaPath_set(PointerRNA *ptr, const char *value)
{
	DriverTarget *dtar = (DriverTarget *)ptr->data;
	
	/* XXX in this case we need to be very careful, as this will require some new dependencies to be added! */
	if (dtar->rna_path)
		MEM_freeN(dtar->rna_path);
	
	if (value[0])
		dtar->rna_path = BLI_strdup(value);
	else
		dtar->rna_path = NULL;
}

static void rna_DriverVariable_type_set(PointerRNA *ptr, int value)
{
	DriverVar *dvar = (DriverVar *)ptr->data;
	
	/* call the API function for this */
	driver_change_variable_type(dvar, value);
}

/* ****************************** */

static void rna_FKeyframe_handle1_get(PointerRNA *ptr, float *values)
{
	BezTriple *bezt = (BezTriple *)ptr->data;
	
	values[0] = bezt->vec[0][0];
	values[1] = bezt->vec[0][1];
}

static void rna_FKeyframe_handle1_set(PointerRNA *ptr, const float *values)
{
	BezTriple *bezt = (BezTriple *)ptr->data;
	
	bezt->vec[0][0] = values[0];
	bezt->vec[0][1] = values[1];
}

static void rna_FKeyframe_handle2_get(PointerRNA *ptr, float *values)
{
	BezTriple *bezt = (BezTriple *)ptr->data;
	
	values[0] = bezt->vec[2][0];
	values[1] = bezt->vec[2][1];
}

static void rna_FKeyframe_handle2_set(PointerRNA *ptr, const float *values)
{
	BezTriple *bezt = (BezTriple *)ptr->data;
	
	bezt->vec[2][0] = values[0];
	bezt->vec[2][1] = values[1];
}

static void rna_FKeyframe_ctrlpoint_get(PointerRNA *ptr, float *values)
{
	BezTriple *bezt = (BezTriple *)ptr->data;
	
	values[0] = bezt->vec[1][0];
	values[1] = bezt->vec[1][1];
}

static void rna_FKeyframe_ctrlpoint_set(PointerRNA *ptr, const float *values)
{
	BezTriple *bezt = (BezTriple *)ptr->data;
	
	bezt->vec[1][0] = values[0];
	bezt->vec[1][1] = values[1];
}

/* ****************************** */

static void rna_FCurve_RnaPath_get(PointerRNA *ptr, char *value)
{
	FCurve *fcu = (FCurve *)ptr->data;

	if (fcu->rna_path)
		strcpy(value, fcu->rna_path);
	else
		value[0] = '\0';
}

static int rna_FCurve_RnaPath_length(PointerRNA *ptr)
{
	FCurve *fcu = (FCurve *)ptr->data;
	
	if (fcu->rna_path)
		return strlen(fcu->rna_path);
	else
		return 0;
}

static void rna_FCurve_RnaPath_set(PointerRNA *ptr, const char *value)
{
	FCurve *fcu = (FCurve *)ptr->data;

	if (fcu->rna_path)
		MEM_freeN(fcu->rna_path);
	
	if (value[0]) {
		fcu->rna_path = BLI_strdup(value);
		fcu->flag &= ~FCURVE_DISABLED;
	}
	else
		fcu->rna_path = NULL;
}

static void rna_FCurve_group_set(PointerRNA *ptr, PointerRNA value)
{
	ID *pid = (ID *)ptr->id.data;
	ID *vid = (ID *)value.id.data;
	FCurve *fcu = ptr->data;
	bAction *act = NULL;
	
	/* get action */
	if (ELEM(NULL, pid, vid)) {
		printf("ERROR: one of the ID's for the groups to assign to is invalid (ptr=%p, val=%p)\n", pid, vid);
		return;
	}
	else if (value.data && (pid != vid)) {
		/* id's differ, cant do this, should raise an error */
		printf("ERROR: ID's differ - ptr=%p vs value=%p\n", pid, vid);
		return;
	}
	
	if (GS(pid->name) == ID_AC && GS(vid->name) == ID_AC) {
		/* the ID given is the action already - usually when F-Curve is obtained from an action's pointer */
		act = (bAction *)pid;
	}
	else {
		/* the ID given is the owner of the F-Curve (for drivers) */
		AnimData *adt = BKE_animdata_from_id(ptr->id.data);
		act = (adt) ? adt->action : NULL;
	}
	
	/* already belongs to group? */
	if (fcu->grp == value.data) {
		/* nothing to do */
		printf("ERROR: F-Curve already belongs to this group\n");
		return;
	}
	
	/* can only change group if we have info about the action the F-Curve is in
	 * (i.e. for drivers or random F-Curves, this cannot be done)
	 */
	if (act == NULL) {
		/* can't change the grouping of F-Curve when it doesn't belong to an action */
		printf("ERROR: cannot assign F-Curve to group, since F-Curve is not attached to any ID\n");
		return;
	}
	/* make sure F-Curve exists in this action first, otherwise we could still have been tricked */
	else if (BLI_findindex(&act->curves, fcu) == -1) {
		printf("ERROR: F-Curve (%p) doesn't exist in action '%s'\n", fcu, act->id.name);
		return;
	}
	
	/* try to remove F-Curve from action (including from any existing groups) */
	action_groups_remove_channel(act, fcu);
	
	/* add the F-Curve back to the action now in the right place */
	/* TODO: make the api function handle the case where there isn't any group to assign to  */
	if (value.data) {
		/* add to its group using API function, which makes sure everything goes ok */
		action_groups_add_channel(act, value.data, fcu);
	}
	else {
		/* need to add this back, but it can only go at the end of the list (or else will corrupt groups) */
		BLI_addtail(&act->curves, fcu);
	}
}

DriverVar *rna_Driver_new_variable(ChannelDriver *driver)
{
	/* call the API function for this */
	return driver_add_new_variable(driver);
}

void rna_Driver_remove_variable(ChannelDriver *driver, ReportList *reports, DriverVar *dvar)
{
	if (BLI_findindex(&driver->variables, dvar) == -1) {
		BKE_report(reports, RPT_ERROR, "Variable does not exist in this driver");
		return;
	}

	driver_free_variable(driver, dvar);
}


static PointerRNA rna_FCurve_active_modifier_get(PointerRNA *ptr)
{
	FCurve *fcu = (FCurve *)ptr->data;
	FModifier *fcm = find_active_fmodifier(&fcu->modifiers);
	return rna_pointer_inherit_refine(ptr, &RNA_FModifier, fcm);
}

static void rna_FCurve_active_modifier_set(PointerRNA *ptr, PointerRNA value)
{
	FCurve *fcu = (FCurve *)ptr->data;
	set_active_fmodifier(&fcu->modifiers, (FModifier *)value.data);
}

static FModifier *rna_FCurve_modifiers_new(FCurve *fcu, int type)
{
	return add_fmodifier(&fcu->modifiers, type);
}

static void rna_FCurve_modifiers_remove(FCurve *fcu, ReportList *reports, FModifier *fcm)
{
	if (BLI_findindex(&fcu->modifiers, fcm) == -1) {
		BKE_reportf(reports, RPT_ERROR, "F-Curve modifier '%s' not found in F-Curve", fcm->name);
		return;
	}
	remove_fmodifier(&fcu->modifiers, fcm);
}

static void rna_FModifier_active_set(PointerRNA *ptr, int UNUSED(value))
{
	FModifier *fm = (FModifier *)ptr->data;

	/* don't toggle, always switch on */
	fm->flag |= FMODIFIER_FLAG_ACTIVE;
}

static void rna_FModifier_start_frame_range(PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
	FModifier *fcm = (FModifier *)ptr->data;
	
	*min = MINAFRAMEF;
	*max = (fcm->flag & FMODIFIER_FLAG_RANGERESTRICT) ? fcm->efra : MAXFRAMEF;
}

static void rna_FModifier_end_frame_range(PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
	FModifier *fcm = (FModifier *)ptr->data;

	*min = (fcm->flag & FMODIFIER_FLAG_RANGERESTRICT) ? fcm->sfra : MINAFRAMEF;
	*max = MAXFRAMEF;
}

static void rna_FModifier_blending_range(PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
	FModifier *fcm = (FModifier *)ptr->data;

	*min = 0.0f;
	*max = fcm->efra - fcm->sfra;
}

static void rna_FModifier_active_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	FModifier *fm, *fmo = (FModifier *)ptr->data;

	/* clear active state of other FModifiers in this list */
	for (fm = fmo->prev; fm; fm = fm->prev) {
		fm->flag &= ~FMODIFIER_FLAG_ACTIVE;
	}
	for (fm = fmo->next; fm; fm = fm->next) {
		fm->flag &= ~FMODIFIER_FLAG_ACTIVE;
	}
	
}

static int rna_FModifierGenerator_coefficients_get_length(PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION])
{
	FModifier *fcm = (FModifier *)ptr->data;
	FMod_Generator *gen = fcm->data;

	if (gen)
		length[0] = gen->arraysize;
	else
		length[0] = 100;  /* for raw_access, untested */

	return length[0];
}

static void rna_FModifierGenerator_coefficients_get(PointerRNA *ptr, float *values)
{
	FModifier *fcm = (FModifier *)ptr->data;
	FMod_Generator *gen = fcm->data;
	memcpy(values, gen->coefficients, gen->arraysize * sizeof(float));
}

static void rna_FModifierGenerator_coefficients_set(PointerRNA *ptr, const float *values)
{
	FModifier *fcm = (FModifier *)ptr->data;
	FMod_Generator *gen = fcm->data;
	memcpy(gen->coefficients, values, gen->arraysize * sizeof(float));
}

static void rna_FModifierLimits_minx_range(PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
	FModifier *fcm = (FModifier *)ptr->data;
	FMod_Limits *data = fcm->data;

	*min = MINAFRAMEF;
	*max = (data->flag & FCM_LIMIT_XMAX) ? data->rect.xmax : MAXFRAMEF;
}

static void rna_FModifierLimits_maxx_range(PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
	FModifier *fcm = (FModifier *)ptr->data;
	FMod_Limits *data = fcm->data;

	*min = (data->flag & FCM_LIMIT_XMIN) ? data->rect.xmin : MINAFRAMEF;
	*max = MAXFRAMEF;
}

static void rna_FModifierLimits_miny_range(PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
	FModifier *fcm = (FModifier *)ptr->data;
	FMod_Limits *data = fcm->data;

	*min = -FLT_MAX;
	*max = (data->flag & FCM_LIMIT_YMAX) ? data->rect.ymax : FLT_MAX;
}

static void rna_FModifierLimits_maxy_range(PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
	FModifier *fcm = (FModifier *)ptr->data;
	FMod_Limits *data = fcm->data;

	*min = (data->flag & FCM_LIMIT_YMIN) ? data->rect.ymin : -FLT_MAX;
	*max = FLT_MAX;
}


static void rna_FModifierStepped_start_frame_range(PointerRNA *ptr, float *min, float *max,
                                                   float *softmin, float *softmax)
{
	FModifier *fcm = (FModifier *)ptr->data;
	FMod_Stepped *data = fcm->data;
	
	*min = MINAFRAMEF;
	*max = (data->flag & FCM_STEPPED_NO_AFTER) ? data->end_frame : MAXFRAMEF;
}

static void rna_FModifierStepped_end_frame_range(PointerRNA *ptr, float *min, float *max,
                                                 float *softmin, float *softmax)
{
	FModifier *fcm = (FModifier *)ptr->data;
	FMod_Stepped *data = fcm->data;

	*min = (data->flag & FCM_STEPPED_NO_BEFORE) ? data->start_frame : MINAFRAMEF;
	*max = MAXFRAMEF;
}

static BezTriple *rna_FKeyframe_points_insert(FCurve *fcu, float frame, float value, int flag)
{
	int index = insert_vert_fcurve(fcu, frame, value, flag);
	return ((fcu->bezt) && (index >= 0)) ? (fcu->bezt + index) : NULL;
}

static void rna_FKeyframe_points_add(FCurve *fcu, int tot)
{
	if (tot > 0) {
		BezTriple *bezt;
		if (fcu->totvert) {
			BezTriple *nbezt = MEM_callocN(sizeof(BezTriple) * (fcu->totvert + tot), "rna_FKeyframe_points_add");
			memcpy(nbezt, fcu->bezt, sizeof(BezTriple) * fcu->totvert);
			MEM_freeN(fcu->bezt);
			fcu->bezt = nbezt;
		}
		else {
			fcu->bezt = MEM_callocN(sizeof(BezTriple) * tot, "rna_FKeyframe_points_add");
		}
		
		bezt = fcu->bezt + fcu->totvert;
		fcu->totvert += tot;
		
		while (tot--) {
			/* defaults, no userprefs gives predictable results for API */
			bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
			bezt->ipo = BEZT_IPO_BEZ;
			bezt->h1 = bezt->h2 = HD_AUTO_ANIM;
			bezt++;
		}
	}
}

static void rna_FKeyframe_points_remove(FCurve *fcu, ReportList *reports, BezTriple *bezt, int do_fast)
{
	int index = (int)(bezt - fcu->bezt);
	if (index < 0 || index >= fcu->totvert) {
		BKE_report(reports, RPT_ERROR, "Keyframe not in F-Curve");
		return;
	}

	delete_fcurve_key(fcu, index, !do_fast);
}

static void rna_fcurve_range(FCurve *fcu, float range[2])
{
	calc_fcurve_range(fcu, range, range + 1, FALSE, FALSE);
}

#else

static void rna_def_fmodifier_generator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem generator_mode_items[] = {
		{FCM_GENERATOR_POLYNOMIAL, "POLYNOMIAL", 0, "Expanded Polynomial", ""},
		{FCM_GENERATOR_POLYNOMIAL_FACTORISED, "POLYNOMIAL_FACTORISED", 0, "Factorized Polynomial", ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "FModifierGenerator", "FModifier");
	RNA_def_struct_ui_text(srna, "Generator F-Modifier", "Deterministically generate values for the modified F-Curve");
	RNA_def_struct_sdna_from(srna, "FMod_Generator", "data");
	
	/* define common props */
	prop = RNA_def_property(srna, "use_additive", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FCM_GENERATOR_ADDITIVE);
	RNA_def_property_ui_text(prop, "Additive",
	                         "Values generated by this modifier are applied on top of "
	                         "the existing values instead of overwriting them");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	/* XXX this has a special validation func */
	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, generator_mode_items);
	RNA_def_property_ui_text(prop, "Mode", "Type of generator to use");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	
	/* order of the polynomial */
	/* XXX this has a special validation func */
	prop = RNA_def_property(srna, "poly_order", PROP_INT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Polynomial Order",
	                         "The highest power of 'x' for this polynomial (number of coefficients - 1)");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	/* coefficients array */
	prop = RNA_def_property(srna, "coefficients", PROP_FLOAT, PROP_NONE);
	RNA_def_property_array(prop, 32);
	RNA_def_property_flag(prop, PROP_DYNAMIC);
	RNA_def_property_dynamic_array_funcs(prop, "rna_FModifierGenerator_coefficients_get_length");
	RNA_def_property_float_funcs(prop, "rna_FModifierGenerator_coefficients_get",
	                             "rna_FModifierGenerator_coefficients_set", NULL);
	RNA_def_property_ui_text(prop, "Coefficients", "Coefficients for 'x' (starting from lowest power of x^0)");
}

/* --------- */

static void rna_def_fmodifier_function_generator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_type_items[] = {
		{0, "SIN", 0, "Sine", ""},
		{1, "COS", 0, "Cosine", ""},
		{2, "TAN", 0, "Tangent", ""},
		{3, "SQRT", 0, "Square Root", ""},
		{4, "LN", 0, "Natural Logarithm", ""},
		{5, "SINC", 0, "Normalised Sine", "sin(x) / x"},
		{0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "FModifierFunctionGenerator", "FModifier");
	RNA_def_struct_ui_text(srna, "Built-In Function F-Modifier", "Generate values using a Built-In Function");
	RNA_def_struct_sdna_from(srna, "FMod_FunctionGenerator", "data");
	
	/* coefficients */
	prop = RNA_def_property(srna, "amplitude", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Amplitude", "Scale factor determining the maximum/minimum values");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "phase_multiplier", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Phase Multiplier", "Scale factor determining the 'speed' of the function");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "phase_offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Phase Offset", "Constant factor to offset time by for function");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "value_offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Value Offset", "Constant factor to offset values by");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	/* flags */
	prop = RNA_def_property(srna, "use_additive", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FCM_GENERATOR_ADDITIVE);
	RNA_def_property_ui_text(prop, "Additive",
	                         "Values generated by this modifier are applied on top of "
	                         "the existing values instead of overwriting them");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "function_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Type", "Type of built-in function to use");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
}

/* --------- */

static void rna_def_fmodifier_envelope_ctrl(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "FModifierEnvelopeControlPoint", NULL);
	RNA_def_struct_ui_text(srna, "Envelope Control Point", "Control point for envelope F-Modifier");
	RNA_def_struct_sdna(srna, "FCM_EnvelopeData");
	
	/* min/max extents
	 *	- for now, these are allowed to go past each other, so that we can have inverted action
	 *	- technically, the range is limited by the settings in the envelope-modifier data, not here...
	 */
	prop = RNA_def_property(srna, "min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "min");
	RNA_def_property_ui_text(prop, "Minimum Value", "Lower bound of envelope at this control-point");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max");
	RNA_def_property_ui_text(prop, "Maximum Value", "Upper bound of envelope at this control-point");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	/* Frame */
	prop = RNA_def_property(srna, "frame", PROP_FLOAT, PROP_TIME);
	RNA_def_property_float_sdna(prop, NULL, "time");
	RNA_def_property_ui_text(prop, "Frame", "Frame this control-point occurs on");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	/* TODO: */
	/*	- selection flags (not implemented in UI yet though) */
}

static void rna_def_fmodifier_envelope(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "FModifierEnvelope", "FModifier");
	RNA_def_struct_ui_text(srna, "Envelope F-Modifier", "Scale the values of the modified F-Curve");
	RNA_def_struct_sdna_from(srna, "FMod_Envelope", "data");
	
	/* Collections */
	prop = RNA_def_property(srna, "control_points", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "data", "totvert");
	RNA_def_property_struct_type(prop, "FModifierEnvelopeControlPoint");
	RNA_def_property_ui_text(prop, "Control Points", "Control points defining the shape of the envelope");
	
	/* Range Settings */
	prop = RNA_def_property(srna, "reference_value", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "midval");
	RNA_def_property_ui_text(prop, "Reference Value", "Value that envelope's influence is centered around / based on");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "default_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "min");
	RNA_def_property_ui_text(prop, "Default Minimum", "Lower distance from Reference Value for 1:1 default influence");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "default_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max");
	RNA_def_property_ui_text(prop, "Default Maximum", "Upper distance from Reference Value for 1:1 default influence");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
}

/* --------- */

static void rna_def_fmodifier_cycles(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_type_items[] = {
		{FCM_EXTRAPOLATE_NONE, "NONE", 0, "No Cycles", "Don't do anything"},
		{FCM_EXTRAPOLATE_CYCLIC, "REPEAT", 0, "Repeat Motion", "Repeat keyframe range as-is"},
		{FCM_EXTRAPOLATE_CYCLIC_OFFSET, "REPEAT_OFFSET", 0, "Repeat with Offset",
		                                "Repeat keyframe range, but with offset based on gradient between "
		                                "start and end values"},
		{FCM_EXTRAPOLATE_MIRROR, "MIRROR", 0, "Repeat Mirrored",
		                         "Alternate between forward and reverse playback of keyframe range"},
		{0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "FModifierCycles", "FModifier");
	RNA_def_struct_ui_text(srna, "Cycles F-Modifier", "Repeat the values of the modified F-Curve");
	RNA_def_struct_sdna_from(srna, "FMod_Cycles", "data");
	
	/* before */
	prop = RNA_def_property(srna, "mode_before", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "before_mode");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Before Mode", "Cycling mode to use before first keyframe");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "cycles_before", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "before_cycles");
	RNA_def_property_ui_text(prop, "Before Cycles",
	                         "Maximum number of cycles to allow before first keyframe (0 = infinite)");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	/* after */
	prop = RNA_def_property(srna, "mode_after", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "after_mode");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "After Mode", "Cycling mode to use after last keyframe");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "cycles_after", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "after_cycles");
	RNA_def_property_ui_text(prop, "After Cycles",
	                         "Maximum number of cycles to allow after last keyframe (0 = infinite)");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
}

/* --------- */

static void rna_def_fmodifier_python(BlenderRNA *brna)
{
	StructRNA *srna;
	/*PropertyRNA *prop; */
	
	srna = RNA_def_struct(brna, "FModifierPython", "FModifier");
	RNA_def_struct_ui_text(srna, "Python F-Modifier", "Perform user-defined operation on the modified F-Curve");
	RNA_def_struct_sdna_from(srna, "FMod_Python", "data");
}

/* --------- */

static void rna_def_fmodifier_limits(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "FModifierLimits", "FModifier");
	RNA_def_struct_ui_text(srna, "Limit F-Modifier", "Limit the time/value ranges of the modified F-Curve");
	RNA_def_struct_sdna_from(srna, "FMod_Limits", "data");
	
	prop = RNA_def_property(srna, "use_min_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FCM_LIMIT_XMIN);
	RNA_def_property_ui_text(prop, "Minimum X", "Use the minimum X value");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "use_min_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FCM_LIMIT_YMIN);
	RNA_def_property_ui_text(prop, "Minimum Y", "Use the minimum Y value");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "use_max_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FCM_LIMIT_XMAX);
	RNA_def_property_ui_text(prop, "Maximum X", "Use the maximum X value");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "use_max_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FCM_LIMIT_YMAX);
	RNA_def_property_ui_text(prop, "Maximum Y", "Use the maximum Y value");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "min_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rect.xmin");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_FModifierLimits_minx_range");
	RNA_def_property_ui_text(prop, "Minimum X", "Lowest X value to allow");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "min_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rect.ymin");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_FModifierLimits_miny_range");
	RNA_def_property_ui_text(prop, "Minimum Y", "Lowest Y value to allow");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "max_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rect.xmax");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_FModifierLimits_maxx_range");
	RNA_def_property_ui_text(prop, "Maximum X", "Highest X value to allow");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "max_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rect.ymax");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_FModifierLimits_maxy_range");
	RNA_def_property_ui_text(prop, "Maximum Y", "Highest Y value to allow");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
}

/* --------- */

static void rna_def_fmodifier_noise(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_modification_items[] = {
		{FCM_NOISE_MODIF_REPLACE, "REPLACE", 0, "Replace", ""},
		{FCM_NOISE_MODIF_ADD, "ADD", 0, "Add", ""},
		{FCM_NOISE_MODIF_SUBTRACT, "SUBTRACT", 0, "Subtract", ""},
		{FCM_NOISE_MODIF_MULTIPLY, "MULTIPLY", 0, "Multiply", ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "FModifierNoise", "FModifier");
	RNA_def_struct_ui_text(srna, "Noise F-Modifier", "Give randomness to the modified F-Curve");
	RNA_def_struct_sdna_from(srna, "FMod_Noise", "data");
	
	prop = RNA_def_property(srna, "blend_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "modification");
	RNA_def_property_enum_items(prop, prop_modification_items);
	RNA_def_property_ui_text(prop, "Blend Type", "Method of modifying the existing F-Curve");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "size");
	RNA_def_property_ui_text(prop, "Scale", "Scaling (in time) of the noise");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "strength");
	RNA_def_property_ui_text(prop, "Strength",
	                         "Amplitude of the noise - the amount that it modifies the underlying curve");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "phase", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "phase");
	RNA_def_property_ui_text(prop, "Phase", "A random seed for the noise effect");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "depth", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "depth");
	RNA_def_property_ui_text(prop, "Depth", "Amount of fine level detail present in the noise");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);

}

/* --------- */

static void rna_def_fmodifier_stepped(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "FModifierStepped", "FModifier");
	RNA_def_struct_ui_text(srna, "Stepped Interpolation F-Modifier",
	                       "Hold each interpolated value from the F-Curve for several frames without "
	                       "changing the timing");
	RNA_def_struct_sdna_from(srna, "FMod_Stepped", "data");
	
	/* properties */
	prop = RNA_def_property(srna, "frame_step", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "step_size");
	RNA_def_property_ui_text(prop, "Step Size", "Number of frames to hold each value");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "frame_offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "offset");
	RNA_def_property_ui_text(prop, "Offset",
	                         "Reference number of frames before frames get held "
	                         "(use to get hold for '1-3' vs '5-7' holding patterns)");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "use_frame_start", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FCM_STEPPED_NO_BEFORE);
	RNA_def_property_ui_text(prop, "Use Start Frame", "Restrict modifier to only act after its 'start' frame");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "use_frame_end", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FCM_STEPPED_NO_AFTER);
	RNA_def_property_ui_text(prop, "Use End Frame", "Restrict modifier to only act before its 'end' frame");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "frame_start", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "start_frame");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_FModifierStepped_start_frame_range");
	RNA_def_property_ui_text(prop, "Start Frame", "Frame that modifier's influence starts (if applicable)");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "frame_end", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "end_frame");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_FModifierStepped_end_frame_range");
	RNA_def_property_ui_text(prop, "End Frame", "Frame that modifier's influence ends (if applicable)");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
}

/* --------- */


static void rna_def_fmodifier(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	/* base struct definition */
	srna = RNA_def_struct(brna, "FModifier", NULL);
	RNA_def_struct_refine_func(srna, "rna_FModifierType_refine");
	RNA_def_struct_ui_text(srna, "F-Modifier", "Modifier for values of F-Curve");
	
#if 0 /* XXX not used yet */
	  /* name */
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_ui_text(prop, "Name", "Short description of F-Curve Modifier");
#endif /* XXX not used yet */
	
	/* type */
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, fmodifier_type_items);
	RNA_def_property_ui_text(prop, "Type", "F-Curve Modifier Type");
	
	/* settings */
	prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FMODIFIER_FLAG_EXPANDED);
	RNA_def_property_ui_text(prop, "Expanded", "F-Curve Modifier's panel is expanded in UI");
	RNA_def_property_ui_icon(prop, ICON_TRIA_RIGHT, 1);
	
	prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FMODIFIER_FLAG_MUTED);
	RNA_def_property_ui_text(prop, "Muted", "F-Curve Modifier will not be evaluated");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	RNA_def_property_ui_icon(prop, ICON_MUTE_IPO_OFF, 1);
	
	prop = RNA_def_property(srna, "is_valid", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", FMODIFIER_FLAG_DISABLED);
	RNA_def_property_ui_text(prop, "Disabled", "F-Curve Modifier has invalid settings and will not be evaluated");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	
	/* TODO: setting this to true must ensure that all others in stack are turned off too... */
	prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FMODIFIER_FLAG_ACTIVE);
	RNA_def_property_ui_text(prop, "Active", "F-Curve Modifier is the one being edited ");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_FModifier_active_set");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME_PROP, "rna_FModifier_active_update");
	RNA_def_property_ui_icon(prop, ICON_RADIOBUT_OFF, 1);
	
	/* restricted range */
	prop = RNA_def_property(srna, "use_restricted_range", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FMODIFIER_FLAG_RANGERESTRICT);
	RNA_def_property_ui_text(prop, "Restrict Frame Range",
	                         "F-Curve Modifier is only applied for the specified frame range to help "
	                         "mask off effects in order to chain them");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	RNA_def_property_ui_icon(prop, ICON_TRIA_RIGHT, 1); /* XXX: depends on UI implementation */
	
	prop = RNA_def_property(srna, "frame_start", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sfra");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_FModifier_start_frame_range");
	RNA_def_property_ui_text(prop, "Start Frame",
	                         "Frame that modifier's influence starts (if Restrict Frame Range is in use)");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	
	prop = RNA_def_property(srna, "frame_end", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "efra");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_FModifier_end_frame_range");
	RNA_def_property_ui_text(prop, "End Frame",
	                         "Frame that modifier's influence ends (if Restrict Frame Range is in use)");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	
	prop = RNA_def_property(srna, "blend_in", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "blendin");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_FModifier_blending_range");
	RNA_def_property_ui_text(prop, "Blend In", "Number of frames from start frame for influence to take effect");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	
	prop = RNA_def_property(srna, "blend_out", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "blendout");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_FModifier_blending_range");
	RNA_def_property_ui_text(prop, "Blend Out", "Number of frames from end frame for influence to fade out");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	
	/* influence */
	prop = RNA_def_property(srna, "use_influence", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FMODIFIER_FLAG_USEINFLUENCE);
	RNA_def_property_ui_text(prop, "Use Influence", "F-Curve Modifier's effects will be tempered by a default factor");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	RNA_def_property_ui_icon(prop, ICON_TRIA_RIGHT, 1); /* XXX: depends on UI implementation */
	
	prop = RNA_def_property(srna, "influence", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "influence");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_ui_text(prop, "Influence",
	                         "Amount of influence F-Curve Modifier will have when not fading in/out");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
}

/* *********************** */

static void rna_def_drivertarget(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_trans_chan_items[] = {
		{DTAR_TRANSCHAN_LOCX, "LOC_X", 0, "X Location", ""},
		{DTAR_TRANSCHAN_LOCY, "LOC_Y", 0, "Y Location", ""},
		{DTAR_TRANSCHAN_LOCZ, "LOC_Z", 0, "Z Location", ""},
		{DTAR_TRANSCHAN_ROTX, "ROT_X", 0, "X Rotation", ""},
		{DTAR_TRANSCHAN_ROTY, "ROT_Y", 0, "Y Rotation", ""},
		{DTAR_TRANSCHAN_ROTZ, "ROT_Z", 0, "Z Rotation", ""},
		{DTAR_TRANSCHAN_SCALEX, "SCALE_X", 0, "X Scale", ""},
		{DTAR_TRANSCHAN_SCALEY, "SCALE_Y", 0, "Y Scale", ""},
		{DTAR_TRANSCHAN_SCALEZ, "SCALE_Z", 0, "Z Scale", ""},
		{0, NULL, 0, NULL, NULL}
	};
		
	static EnumPropertyItem prop_local_space_items[] = {
		{0, "WORLD_SPACE", 0, "World Space", "Transforms include effects of parenting/restpose and constraints"},
		{DTAR_FLAG_LOCALSPACE, "TRANSFORM_SPACE", 0, "Transform Space",
		                       "Transforms don't include parenting/restpose or constraints"},
		{DTAR_FLAG_LOCALSPACE | DTAR_FLAG_LOCAL_CONSTS, "LOCAL_SPACE", 0, "Local Space",
		                                                "Transforms include effects of constraints but not "
		                                                "parenting/restpose"},
		{0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "DriverTarget", NULL);
	RNA_def_struct_ui_text(srna, "Driver Target", "Source of input values for driver variables");
	
	/* Target Properties - ID-block to Drive */
	prop = RNA_def_property(srna, "id", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ID");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_editable_func(prop, "rna_DriverTarget_id_editable");
	/* note: custom set function is ONLY to avoid rna setting a user for this. */
	RNA_def_property_pointer_funcs(prop, NULL, "rna_DriverTarget_id_set", "rna_DriverTarget_id_typef", NULL);
	RNA_def_property_ui_text(prop, "ID",
	                         "ID-block that the specific property used can be found from "
	                         "(id_type property must be set first)");
	RNA_def_property_update(prop, 0, "rna_DriverTarget_update_data");
	
	prop = RNA_def_property(srna, "id_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "idtype");
	RNA_def_property_enum_items(prop, id_type_items);
	RNA_def_property_enum_default(prop, ID_OB);
	RNA_def_property_enum_funcs(prop, NULL, "rna_DriverTarget_id_type_set", NULL);
	RNA_def_property_editable_func(prop, "rna_DriverTarget_id_type_editable");
	RNA_def_property_ui_text(prop, "ID Type", "Type of ID-block that can be used");
	RNA_def_property_update(prop, 0, "rna_DriverTarget_update_data");
	
	/* Target Properties - Property to Drive */
	prop = RNA_def_property(srna, "data_path", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_DriverTarget_RnaPath_get", "rna_DriverTarget_RnaPath_length",
	                              "rna_DriverTarget_RnaPath_set");
	RNA_def_property_ui_text(prop, "Data Path", "RNA Path (from ID-block) to property used");
	RNA_def_property_update(prop, 0, "rna_DriverTarget_update_data");
	
	prop = RNA_def_property(srna, "bone_target", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "pchan_name");
	RNA_def_property_ui_text(prop, "Bone Name", "Name of PoseBone to use as target");
	RNA_def_property_update(prop, 0, "rna_DriverTarget_update_data");
	
	prop = RNA_def_property(srna, "transform_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "transChan");
	RNA_def_property_enum_items(prop, prop_trans_chan_items);
	RNA_def_property_ui_text(prop, "Type", "Driver variable type");
	RNA_def_property_update(prop, 0, "rna_DriverTarget_update_data");
	
	prop = RNA_def_property(srna, "transform_space", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, prop_local_space_items);
	RNA_def_property_ui_text(prop, "Transform Space", "Space in which transforms are used");
	RNA_def_property_update(prop, 0, "rna_DriverTarget_update_data");
}

static void rna_def_drivervar(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_type_items[] = {
		{DVAR_TYPE_SINGLE_PROP, "SINGLE_PROP", 0, "Single Property", "Use the value from some RNA property (Default)"},
		{DVAR_TYPE_TRANSFORM_CHAN, "TRANSFORMS", 0, "Transform Channel",
		                           "Final transformation value of object or bone"},
		{DVAR_TYPE_ROT_DIFF, "ROTATION_DIFF", 0, "Rotational Difference", "Use the angle between two bones"},
		{DVAR_TYPE_LOC_DIFF, "LOC_DIFF", 0, "Distance", "Distance between two bones or objects"},
		{0, NULL, 0, NULL, NULL}
	};
		
	
	srna = RNA_def_struct(brna, "DriverVariable", NULL);
	RNA_def_struct_sdna(srna, "DriverVar");
	RNA_def_struct_ui_text(srna, "Driver Variable", "Variable from some source/target for driver relationship");
	
	/* Variable Name */
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_ui_text(prop, "Name",
	                         "Name to use in scripted expressions/functions (no spaces or dots are allowed, "
	                         "and must start with a letter)");
	RNA_def_property_update(prop, 0, "rna_DriverTarget_update_name"); /* XXX */
	
	/* Enums */
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_DriverVariable_type_set", NULL);
	RNA_def_property_ui_text(prop, "Type", "Driver variable type");
	RNA_def_property_update(prop, 0, "rna_ChannelDriver_update_data"); /* XXX */
	
	/* Targets */
	/* TODO: for nicer api, only expose the relevant props via subclassing,
	 *       instead of exposing the collection of targets */
	prop = RNA_def_property(srna, "targets", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "targets", "num_targets");
	RNA_def_property_struct_type(prop, "DriverTarget");
	RNA_def_property_ui_text(prop, "Targets", "Sources of input data for evaluating this variable");
}


/* channeldriver.variables.* */
static void rna_def_channeldriver_variables(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
/*	PropertyRNA *prop; */
	
	FunctionRNA *func;
	PropertyRNA *parm;
	
	RNA_def_property_srna(cprop, "ChannelDriverVariables");
	srna = RNA_def_struct(brna, "ChannelDriverVariables", NULL);
	RNA_def_struct_sdna(srna, "ChannelDriver");
	RNA_def_struct_ui_text(srna, "ChannelDriver Variables", "Collection of channel driver Variables");
	
	
	/* add variable */
	func = RNA_def_function(srna, "new", "rna_Driver_new_variable");
	RNA_def_function_ui_description(func, "Add a new variable for the driver");
	/* return type */
	parm = RNA_def_pointer(func, "var", "DriverVariable", "", "Newly created Driver Variable");
	RNA_def_function_return(func, parm);

	/* remove variable */
	func = RNA_def_function(srna, "remove", "rna_Driver_remove_variable");
	RNA_def_function_ui_description(func, "Remove an existing variable from the driver");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	/* target to remove */
	parm = RNA_def_pointer(func, "variable", "DriverVariable", "", "Variable to remove from the driver");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
}

static void rna_def_channeldriver(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_type_items[] = {
		{DRIVER_TYPE_AVERAGE, "AVERAGE", 0, "Averaged Value", ""},
		{DRIVER_TYPE_SUM, "SUM", 0, "Sum Values", ""},
		{DRIVER_TYPE_PYTHON, "SCRIPTED", 0, "Scripted Expression", ""},
		{DRIVER_TYPE_MIN, "MIN", 0, "Minimum Value", ""},
		{DRIVER_TYPE_MAX, "MAX", 0, "Maximum Value", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "Driver", NULL);
	RNA_def_struct_sdna(srna, "ChannelDriver");
	RNA_def_struct_ui_text(srna, "Driver", "Driver for the value of a setting based on an external value");
	RNA_def_struct_ui_icon(srna, ICON_DRIVER);

	/* Enums */
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Type", "Driver type");
	RNA_def_property_update(prop, 0, "rna_ChannelDriver_update_data");

	/* String values */
	prop = RNA_def_property(srna, "expression", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Expression", "Expression to use for Scripted Expression");
	RNA_def_property_update(prop, 0, "rna_ChannelDriver_update_expr");

	/* Collections */
	prop = RNA_def_property(srna, "variables", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "variables", NULL);
	RNA_def_property_struct_type(prop, "DriverVariable");
	RNA_def_property_ui_text(prop, "Variables", "Properties acting as inputs for this driver");
	rna_def_channeldriver_variables(brna, prop);
	
	/* Settings */
	prop = RNA_def_property(srna, "show_debug_info", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", DRIVER_FLAG_SHOWDEBUG);
	RNA_def_property_ui_text(prop, "Show Debug Info",
	                         "Show intermediate values for the driver calculations to allow debugging of drivers");
	
	/* State Info (for Debugging) */
	prop = RNA_def_property(srna, "is_valid", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", DRIVER_FLAG_INVALID);
	RNA_def_property_ui_text(prop, "Invalid", "Driver could not be evaluated in past, so should be skipped");
	
	
	/* Functions */
	RNA_api_drivers(srna);
}

/* *********************** */

static void rna_def_fpoint(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "FCurveSample", NULL);
	RNA_def_struct_sdna(srna, "FPoint");
	RNA_def_struct_ui_text(srna, "F-Curve Sample", "Sample point for F-Curve");
	
	/* Boolean values */
	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", 1);
	RNA_def_property_ui_text(prop, "Select", "Selection status");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, NULL);
	
	/* Vector value */
	prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_COORDS); /* keyframes are dimensionless */
	RNA_def_property_float_sdna(prop, NULL, "vec");
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_text(prop, "Point", "Point coordinates");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
}


/* duplicate of BezTriple in rna_curve.c
 * but with F-Curve specific options updates/functionality
 */
static void rna_def_fkeyframe(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "Keyframe", NULL);
	RNA_def_struct_sdna(srna, "BezTriple");
	RNA_def_struct_ui_text(srna, "Keyframe", "Bezier curve point with two handles defining a Keyframe on an F-Curve");
	
	/* Boolean values */
	prop = RNA_def_property(srna, "select_left_handle", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "f1", 0);
	RNA_def_property_ui_text(prop, "Handle 1 selected", "Left handle selection status");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, NULL);
	
	prop = RNA_def_property(srna, "select_right_handle", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "f3", 0);
	RNA_def_property_ui_text(prop, "Handle 2 selected", "Right handle selection status");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, NULL);
	
	prop = RNA_def_property(srna, "select_control_point", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "f2", 0);
	RNA_def_property_ui_text(prop, "Select", "Control point selection status");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, NULL);
	
	/* Enums */
	prop = RNA_def_property(srna, "handle_left_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "h1");
	RNA_def_property_enum_items(prop, keyframe_handle_type_items);
	RNA_def_property_ui_text(prop, "Left Handle Type", "Handle types");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	
	prop = RNA_def_property(srna, "handle_right_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "h2");
	RNA_def_property_enum_items(prop, keyframe_handle_type_items);
	RNA_def_property_ui_text(prop, "Right Handle Type", "Handle types");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	
	prop = RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "ipo");
	RNA_def_property_enum_items(prop, beztriple_interpolation_mode_items);
	RNA_def_property_ui_text(prop, "Interpolation",
	                         "Interpolation method to use for segment of the F-Curve from "
	                         "this Keyframe until the next Keyframe");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "hide");
	RNA_def_property_enum_items(prop, beztriple_keyframe_type_items);
	RNA_def_property_ui_text(prop, "Type", "Type of keyframe (for visual purposes only)");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	
	/* Vector values */
	prop = RNA_def_property(srna, "handle_left", PROP_FLOAT, PROP_COORDS); /* keyframes are dimensionless */
	RNA_def_property_array(prop, 2);
	RNA_def_property_float_funcs(prop, "rna_FKeyframe_handle1_get", "rna_FKeyframe_handle1_set", NULL);
	RNA_def_property_ui_text(prop, "Left Handle", "Coordinates of the left handle (before the control point)");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_COORDS); /* keyframes are dimensionless */
	RNA_def_property_array(prop, 2);
	RNA_def_property_float_funcs(prop, "rna_FKeyframe_ctrlpoint_get", "rna_FKeyframe_ctrlpoint_set", NULL);
	RNA_def_property_ui_text(prop, "Control Point", "Coordinates of the control point");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "handle_right", PROP_FLOAT, PROP_COORDS); /* keyframes are dimensionless */
	RNA_def_property_array(prop, 2);
	RNA_def_property_float_funcs(prop, "rna_FKeyframe_handle2_get", "rna_FKeyframe_handle2_set", NULL);
	RNA_def_property_ui_text(prop, "Right Handle", "Coordinates of the right handle (after the control point)");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
}

static void rna_def_fcurve_modifiers(BlenderRNA *brna, PropertyRNA *cprop)
{
	/* add modifiers */
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "FCurveModifiers");
	srna = RNA_def_struct(brna, "FCurveModifiers", NULL);
	RNA_def_struct_sdna(srna, "FCurve");
	RNA_def_struct_ui_text(srna, "F-Curve Modifiers", "Collection of F-Curve Modifiers");


	/* Collection active property */
	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "FModifier");
	RNA_def_property_pointer_funcs(prop, "rna_FCurve_active_modifier_get",
	                               "rna_FCurve_active_modifier_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Active F-Curve Modifier", "Active F-Curve Modifier");

	/* Constraint collection */
	func = RNA_def_function(srna, "new", "rna_FCurve_modifiers_new");
	RNA_def_function_ui_description(func, "Add a constraint to this object");
	/* return type */
	parm = RNA_def_pointer(func, "fmodifier", "FModifier", "", "New fmodifier");
	RNA_def_function_return(func, parm);
	/* object to add */
	parm = RNA_def_enum(func, "type", fmodifier_type_items, 1, "", "Constraint type to add");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "remove", "rna_FCurve_modifiers_remove");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove a modifier from this F-Curve");
	/* modifier to remove */
	parm = RNA_def_pointer(func, "modifier", "FModifier", "", "Removed modifier");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
}

/* fcurve.keyframe_points */
static void rna_def_fcurve_keyframe_points(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	FunctionRNA *func;
	PropertyRNA *parm;

	static EnumPropertyItem keyframe_flag_items[] = {
		{INSERTKEY_REPLACE, "REPLACE", 0, "Replace", "Don't add any new keyframes, but just replace existing ones"},
		{INSERTKEY_NEEDED, "NEEDED", 0, "Needed", "Only adds keyframes that are needed"},
		{INSERTKEY_FAST, "FAST", 0, "Fast", "Fast keyframe insertion to avoid recalculating the curve each time"},
		{0, NULL, 0, NULL, NULL}
	};

	RNA_def_property_srna(cprop, "FCurveKeyframePoints");
	srna = RNA_def_struct(brna, "FCurveKeyframePoints", NULL);
	RNA_def_struct_sdna(srna, "FCurve");
	RNA_def_struct_ui_text(srna, "Keyframe Points", "Collection of keyframe points");

	func = RNA_def_function(srna, "insert", "rna_FKeyframe_points_insert");
	RNA_def_function_ui_description(func, "Add a keyframe point to a F-Curve");
	parm = RNA_def_float(func, "frame", 0.0f, -FLT_MAX, FLT_MAX, "",
	                     "X Value of this keyframe point", -FLT_MAX, FLT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_float(func, "value", 0.0f, -FLT_MAX, FLT_MAX, "",
	                     "Y Value of this keyframe point", -FLT_MAX, FLT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);

	RNA_def_enum_flag(func, "options", keyframe_flag_items, 0, "", "Keyframe options");

	parm = RNA_def_pointer(func, "keyframe", "Keyframe", "", "Newly created keyframe");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "add", "rna_FKeyframe_points_add");
	RNA_def_function_ui_description(func, "Add a keyframe point to a F-Curve");
	RNA_def_int(func, "count", 1, 1, INT_MAX, "Number", "Number of points to add to the spline", 1, INT_MAX);

	func = RNA_def_function(srna, "remove", "rna_FKeyframe_points_remove");
	RNA_def_function_ui_description(func, "Remove keyframe from an F-Curve");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "keyframe", "Keyframe", "", "Keyframe to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	/* optional */
	RNA_def_boolean(func, "fast", 0, "Fast", "Fast keyframe removal to avoid recalculating the curve each time");
}

static void rna_def_fcurve(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	PropertyRNA *parm;

	static EnumPropertyItem prop_mode_extend_items[] = {
		{FCURVE_EXTRAPOLATE_CONSTANT, "CONSTANT", 0, "Constant", ""},
		{FCURVE_EXTRAPOLATE_LINEAR, "LINEAR", 0, "Linear", ""},
		{0, NULL, 0, NULL, NULL}
	};
	static EnumPropertyItem prop_mode_color_items[] = {
		{FCURVE_COLOR_AUTO_RAINBOW, "AUTO_RAINBOW", 0, "Auto Rainbow", ""},
		{FCURVE_COLOR_AUTO_RGB, "AUTO_RGB", 0, "Auto XYZ to RGB", ""},
		{FCURVE_COLOR_CUSTOM, "CUSTOM", 0, "User Defined", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "FCurve", NULL);
	RNA_def_struct_ui_text(srna, "F-Curve", "F-Curve defining values of a period of time");
	RNA_def_struct_ui_icon(srna, ICON_ANIM_DATA);

	/* Enums */
	prop = RNA_def_property(srna, "extrapolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "extend");
	RNA_def_property_enum_items(prop, prop_mode_extend_items);
	RNA_def_property_ui_text(prop, "Extrapolation", "");
	/* XXX need an update callback for this so that animation gets evaluated */
	RNA_def_property_update(prop, NC_ANIMATION, NULL);

	/* Pointers */
	prop = RNA_def_property(srna, "driver", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Driver", "Channel Driver (only set for Driver F-Curves)");
	
	prop = RNA_def_property(srna, "group", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "grp");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Group", "Action Group that this F-Curve belongs to");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_FCurve_group_set", NULL, NULL);
	RNA_def_property_update(prop, NC_ANIMATION, NULL);

	/* Path + Array Index */
	prop = RNA_def_property(srna, "data_path", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_FCurve_RnaPath_get", "rna_FCurve_RnaPath_length",
	                              "rna_FCurve_RnaPath_set");
	RNA_def_property_ui_text(prop, "Data Path", "RNA Path to property affected by F-Curve");
	/* XXX need an update callback for this to that animation gets evaluated */
	RNA_def_property_update(prop, NC_ANIMATION, NULL);

	/* called 'index' when given as function arg */
	prop = RNA_def_property(srna, "array_index", PROP_INT, PROP_NONE);
	RNA_def_property_ui_text(prop, "RNA Array Index",
	                         "Index to the specific property affected by F-Curve if applicable");
	/* XXX need an update callback for this so that animation gets evaluated */
	RNA_def_property_update(prop, NC_ANIMATION, NULL);
	
	/* Color */
	prop = RNA_def_property(srna, "color_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_mode_color_items);
	RNA_def_property_ui_text(prop, "Color Mode", "Method used to determine color of F-Curve in Graph Editor");
	RNA_def_property_update(prop, NC_ANIMATION, NULL);
	
	prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Color", "Color of the F-Curve in the Graph Editor");
	RNA_def_property_update(prop, NC_ANIMATION, NULL);
	
	/* Flags */
	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FCURVE_SELECTED);
	RNA_def_property_ui_text(prop, "Select", "F-Curve is selected for editing");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, NULL);
	
	prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FCURVE_PROTECTED);
	RNA_def_property_ui_text(prop, "Lock", "F-Curve's settings cannot be edited");
	RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FCURVE_MUTED);
	RNA_def_property_ui_text(prop, "Muted", "F-Curve is not evaluated");
	RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", FCURVE_VISIBLE);
	RNA_def_property_ui_text(prop, "Hide", "F-Curve and its keyframes are hidden in the Graph Editor graphs");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);

	/* State Info (for Debugging) */
	prop = RNA_def_property(srna, "is_valid", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", FCURVE_DISABLED);
	RNA_def_property_ui_text(prop, "Valid",
	                         "False when F-Curve could not be evaluated in past, so should be skipped "
	                         "when evaluating");
	RNA_def_property_update(prop, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	
	/* Collections */
	prop = RNA_def_property(srna, "sampled_points", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "fpt", "totvert");
	RNA_def_property_struct_type(prop, "FCurveSample");
	RNA_def_property_ui_text(prop, "Sampled Points", "Sampled animation data");

	prop = RNA_def_property(srna, "keyframe_points", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "bezt", "totvert");
	RNA_def_property_struct_type(prop, "Keyframe");
	RNA_def_property_ui_text(prop, "Keyframes", "User-editable keyframes");
	rna_def_fcurve_keyframe_points(brna, prop);
	
	prop = RNA_def_property(srna, "modifiers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "FModifier");
	RNA_def_property_ui_text(prop, "Modifiers", "Modifiers affecting the shape of the F-Curve");
	rna_def_fcurve_modifiers(brna, prop);

	/* Functions */
	func = RNA_def_function(srna, "evaluate", "evaluate_fcurve"); /* calls the C/API direct */
	RNA_def_function_ui_description(func, "Evaluate F-Curve");
	parm = RNA_def_float(func, "frame", 1.0f, -FLT_MAX, FLT_MAX, "Frame",
	                     "Evaluate F-Curve at given frame", -FLT_MAX, FLT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return value */
	parm = RNA_def_float(func, "position", 0, -FLT_MAX, FLT_MAX, "Position", "F-Curve position", -FLT_MAX, FLT_MAX);
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "range", "rna_fcurve_range");
	RNA_def_function_ui_description(func, "Get the time extents for F-Curve");
	/* return value */
	parm = RNA_def_float_vector(func, "range", 2, NULL, -FLT_MAX, FLT_MAX, "Range",
	                            "Min/Max values", -FLT_MAX, FLT_MAX);
	RNA_def_property_flag(parm, PROP_THICK_WRAP);
	RNA_def_function_output(func, parm);
}

/* *********************** */

void RNA_def_fcurve(BlenderRNA *brna)
{
	rna_def_fcurve(brna);
	rna_def_fkeyframe(brna);
	rna_def_fpoint(brna);
	
	rna_def_drivertarget(brna);
	rna_def_drivervar(brna);
	rna_def_channeldriver(brna);
	
	rna_def_fmodifier(brna);
	
	rna_def_fmodifier_generator(brna);
	rna_def_fmodifier_function_generator(brna);

	rna_def_fmodifier_envelope(brna);
	rna_def_fmodifier_envelope_ctrl(brna);

	rna_def_fmodifier_cycles(brna);
	rna_def_fmodifier_python(brna);
	rna_def_fmodifier_limits(brna);
	rna_def_fmodifier_noise(brna);
	rna_def_fmodifier_stepped(brna);
}


#endif
