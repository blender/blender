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
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "WM_types.h"

EnumPropertyItem fmodifier_type_items[] = {
	{FMODIFIER_TYPE_NULL, "NULL", 0, "Invalid", ""},
	{FMODIFIER_TYPE_GENERATOR, "GENERATOR", 0, "Generator", ""},
	{FMODIFIER_TYPE_FN_GENERATOR, "FNGENERATOR", 0, "Built-In Function", ""},
	{FMODIFIER_TYPE_ENVELOPE, "ENVELOPE", 0, "Envelope", ""},
	{FMODIFIER_TYPE_CYCLES, "CYCLES", 0, "Cycles", ""},
	{FMODIFIER_TYPE_NOISE, "NOISE", 0, "Noise", ""},
	{FMODIFIER_TYPE_FILTER, "FILTER", 0, "Filter", ""},
	{FMODIFIER_TYPE_PYTHON, "PYTHON", 0, "Python", ""},
	{FMODIFIER_TYPE_LIMITS, "LIMITS", 0, "Limits", ""},
	{0, NULL, 0, NULL, NULL}};

#ifdef RNA_RUNTIME

#include "WM_api.h"

static StructRNA *rna_FModifierType_refine(struct PointerRNA *ptr)
{
	FModifier *fcm= (FModifier *)ptr->data;

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
		//case FMODIFIER_TYPE_FILTER:
		//	return &RNA_FModifierFilter;
		case FMODIFIER_TYPE_PYTHON:
			return &RNA_FModifierPython;
		case FMODIFIER_TYPE_LIMITS:
			return &RNA_FModifierLimits;
		default:
			return &RNA_UnknownType;
	}
}

/* ****************************** */

#include "BKE_fcurve.h"
#include "BKE_depsgraph.h"

static void rna_ChannelDriver_update_data(bContext *C, PointerRNA *ptr)
{
	ID *id= ptr->id.data;
	
	// TODO: this really needs an update guard...
	DAG_scene_sort(CTX_data_scene(C));
	DAG_id_flush_update(id, OB_RECALC_DATA);
	
	WM_event_add_notifier(C, NC_SCENE, id);
}

/* ----------- */

static StructRNA *rna_DriverTarget_id_typef(PointerRNA *ptr)
{
	DriverTarget *dtar= (DriverTarget*)ptr->data;
	return ID_code_to_RNA_type(dtar->idtype);
}

static int rna_DriverTarget_id_editable(PointerRNA *ptr)
{
	DriverTarget *dtar= (DriverTarget*)ptr->data;
	return (dtar->idtype)? PROP_EDITABLE : 0;
}

static void rna_DriverTarget_id_type_set(PointerRNA *ptr, int value)
{
	DriverTarget *data= (DriverTarget*)(ptr->data);
	
	/* set the driver type, then clear the id-block if the type is invalid */
	data->idtype= value;
	if ((data->id) && (GS(data->id->name) != data->idtype))
		data->id= NULL;
}

static void rna_DriverTarget_RnaPath_get(PointerRNA *ptr, char *value)
{
	DriverTarget *dtar= (DriverTarget *)ptr->data;

	if (dtar->rna_path)
		strcpy(value, dtar->rna_path);
	else
		strcpy(value, "");
}

static int rna_DriverTarget_RnaPath_length(PointerRNA *ptr)
{
	DriverTarget *dtar= (DriverTarget *)ptr->data;
	
	if (dtar->rna_path)
		return strlen(dtar->rna_path);
	else
		return 0;
}

static void rna_DriverTarget_RnaPath_set(PointerRNA *ptr, const char *value)
{
	DriverTarget *dtar= (DriverTarget *)ptr->data;
	
	// XXX in this case we need to be very careful, as this will require some new dependencies to be added!
	if (dtar->rna_path)
		MEM_freeN(dtar->rna_path);
	
	if (strlen(value))
		dtar->rna_path= BLI_strdup(value);
	else 
		dtar->rna_path= NULL;
}

/* ****************************** */

static void rna_FCurve_RnaPath_get(PointerRNA *ptr, char *value)
{
	FCurve *fcu= (FCurve *)ptr->data;

	if (fcu->rna_path)
		strcpy(value, fcu->rna_path);
	else
		strcpy(value, "");
}

static int rna_FCurve_RnaPath_length(PointerRNA *ptr)
{
	FCurve *fcu= (FCurve *)ptr->data;
	
	if (fcu->rna_path)
		return strlen(fcu->rna_path);
	else
		return 0;
}

static void rna_FCurve_RnaPath_set(PointerRNA *ptr, const char *value)
{
	FCurve *fcu= (FCurve *)ptr->data;

	if (fcu->rna_path)
		MEM_freeN(fcu->rna_path);
	
	if (strlen(value))
		fcu->rna_path= BLI_strdup(value);
	else 
		fcu->rna_path= NULL;
}

DriverTarget *rna_Driver_add_target(ChannelDriver *driver, char *name)
{
	DriverTarget *dtar= driver_add_new_target(driver);

	/* set the name if given */
	if (name && name[0]) {
		BLI_strncpy(dtar->name, name, 64);
		BLI_uniquename(&driver->targets, dtar, "var", '_', offsetof(DriverTarget, name), 64);
	}

	/* return this target for the users to play with */
	return dtar;
}

void rna_Driver_remove_target(ChannelDriver *driver, DriverTarget *dtar)
{
	/* call the API function for this */
	driver_free_target(driver, dtar);
}

#else


static void rna_def_fmodifier_generator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem generator_mode_items[] = {
		{FCM_GENERATOR_POLYNOMIAL, "POLYNOMIAL", 0, "Expanded Polynomial", ""},
		{FCM_GENERATOR_POLYNOMIAL_FACTORISED, "POLYNOMIAL_FACTORISED", 0, "Factorised Polynomial", ""},
		{0, NULL, 0, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "FModifierGenerator", "FModifier");
	RNA_def_struct_ui_text(srna, "Generator F-Curve Modifier", "Deterministically generates values for the modified F-Curve.");
	RNA_def_struct_sdna_from(srna, "FMod_Generator", "data");
	
	/* define common props */
	prop= RNA_def_property(srna, "additive", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FCM_GENERATOR_ADDITIVE);
	RNA_def_property_ui_text(prop, "Additive", "Values generated by this modifier are applied on top of the existing values instead of overwriting them.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
		// XXX this has a special validation func
	prop= RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, generator_mode_items);
	RNA_def_property_ui_text(prop, "Mode", "Type of generator to use.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	
	/* order of the polynomial */
		// XXX this has a special validation func
	prop= RNA_def_property(srna, "poly_order", PROP_INT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Polynomial Order", "The highest power of 'x' for this polynomial. (number of coefficients - 1)");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	/* coefficients array */
		// FIXME: this is quite difficult to try to wrap
	//prop= RNA_def_property(srna, "coefficients", PROP_COLLECTION, PROP_NONE);
	//RNA_def_property_collection_funcs(prop, "rna_FModifierGenerator_coefficients_begin", "rna_FModifierGenerator_coefficients_next", "rna_FModifierGenerator_coefficients_end", "rna_iterator_array_get", "rna_FModifierGenerator_coefficients_length", 0, 0, 0, 0);
	//RNA_def_property_ui_text(prop, "Coefficients", "Coefficients for 'x' (starting from lowest power of x^0).");
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
		{0, NULL, 0, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "FModifierFunctionGenerator", "FModifier");
	RNA_def_struct_ui_text(srna, "Built-In Function F-Modifier", "Generates values using a Built-In Function.");
	RNA_def_struct_sdna_from(srna, "FMod_FunctionGenerator", "data");
	
	/* coefficients */
	prop= RNA_def_property(srna, "amplitude", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Amplitude", "Scale factor determining the maximum/minimum values.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	prop= RNA_def_property(srna, "phase_multiplier", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Phase Multiplier", "Scale factor determining the 'speed' of the function.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	prop= RNA_def_property(srna, "phase_offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Phase Offset", "Constant factor to offset time by for function.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	prop= RNA_def_property(srna, "value_offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Value Offset", "Constant factor to offset values by.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	/* flags */
	prop= RNA_def_property(srna, "additive", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FCM_GENERATOR_ADDITIVE);
	RNA_def_property_ui_text(prop, "Additive", "Values generated by this modifier are applied on top of the existing values instead of overwriting them.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	prop= RNA_def_property(srna, "function_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Type", "Type of built-in function to use.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
}

/* --------- */

static void rna_def_fmodifier_envelope_ctrl(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "FModifierEnvelopeControlPoint", NULL);
	RNA_def_struct_ui_text(srna, "Envelope Control Point", "Control point for envelope F-Modifier.");
	RNA_def_struct_sdna(srna, "FCM_EnvelopeData");
	
	/* min/max extents 
	 *	- for now, these are allowed to go past each other, so that we can have inverted action
	 *	- technically, the range is limited by the settings in the envelope-modifier data, not here...
	 */
	prop= RNA_def_property(srna, "minimum", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "min");
	RNA_def_property_ui_text(prop, "Minimum Value", "Lower bound of envelope at this control-point.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	prop= RNA_def_property(srna, "maximum", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max");
	RNA_def_property_ui_text(prop, "Maximum Value", "Upper bound of envelope at this control-point.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	/* Frame */
	prop= RNA_def_property(srna, "frame", PROP_FLOAT, PROP_TIME);
	RNA_def_property_float_sdna(prop, NULL, "time");
	RNA_def_property_ui_text(prop, "Frame", "Frame this control-point occurs on.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	// TODO:
	//	- selection flags (not implemented in UI yet though)
}

static void rna_def_fmodifier_envelope(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "FModifierEnvelope", "FModifier");
	RNA_def_struct_ui_text(srna, "Envelope F-Modifier", "Scales the values of the modified F-Curve.");
	RNA_def_struct_sdna_from(srna, "FMod_Envelope", "data");
	
	/* Collections */
	prop= RNA_def_property(srna, "control_points", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "data", "totvert");
	RNA_def_property_struct_type(prop, "FModifierEnvelopeControlPoint");
	RNA_def_property_ui_text(prop, "Control Points", "Control points defining the shape of the envelope.");
	
	/* Range Settings */
	prop= RNA_def_property(srna, "reference_value", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "midval");
	RNA_def_property_ui_text(prop, "Reference Value", "Value that envelope's influence is centered around / based on.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	prop= RNA_def_property(srna, "default_minimum", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "min");
	RNA_def_property_ui_text(prop, "Default Minimum", "Lower distance from Reference Value for 1:1 default influence.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	prop= RNA_def_property(srna, "default_maximum", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max");
	RNA_def_property_ui_text(prop, "Default Maximum", "Upper distance from Reference Value for 1:1 default influence.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
}

/* --------- */

static void rna_def_fmodifier_cycles(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_type_items[] = {
		{FCM_EXTRAPOLATE_NONE, "NONE", 0, "No Cycles", "Don't do anything."},
		{FCM_EXTRAPOLATE_CYCLIC, "REPEAT", 0, "Repeat Motion", "Repeat keyframe range as-is."},
		{FCM_EXTRAPOLATE_CYCLIC_OFFSET, "REPEAT_OFFSET", 0, "Repeat with Offset", "Repeat keyframe range, but with offset based on gradient between values"},
		{FCM_EXTRAPOLATE_MIRROR, "MIRROR", 0, "Repeat Mirrored", "Alternate between forward and reverse playback of keyframe range"},
		{0, NULL, 0, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "FModifierCycles", "FModifier");
	RNA_def_struct_ui_text(srna, "Cycles F-Modifier", "Repeats the values of the modified F-Curve.");
	RNA_def_struct_sdna_from(srna, "FMod_Cycles", "data");
	
	/* before */
	prop= RNA_def_property(srna, "before_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Before Mode", "Cycling mode to use before first keyframe.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	prop= RNA_def_property(srna, "before_cycles", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Before Cycles", "Maximum number of cycles to allow before first keyframe. (0 = infinite)");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	/* after */
	prop= RNA_def_property(srna, "after_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "After Mode", "Cycling mode to use after last keyframe.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	prop= RNA_def_property(srna, "after_cycles", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "After Cycles", "Maximum number of cycles to allow after last keyframe. (0 = infinite)");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
}

/* --------- */

static void rna_def_fmodifier_python(BlenderRNA *brna)
{
	StructRNA *srna;
	//PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "FModifierPython", "FModifier");
	RNA_def_struct_ui_text(srna, "Python F-Modifier", "Performs user-defined operation on the modified F-Curve.");
	RNA_def_struct_sdna_from(srna, "FMod_Python", "data");
}

/* --------- */

static void rna_def_fmodifier_limits(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "FModifierLimits", "FModifier");
	RNA_def_struct_ui_text(srna, "Limits F-Modifier", "Limits the time/value ranges of the modified F-Curve.");
	RNA_def_struct_sdna_from(srna, "FMod_Limits", "data");
	
	prop= RNA_def_property(srna, "use_minimum_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FCM_LIMIT_XMIN);
	RNA_def_property_ui_text(prop, "Minimum X", "Use the minimum X value.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	prop= RNA_def_property(srna, "use_minimum_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FCM_LIMIT_YMIN);
	RNA_def_property_ui_text(prop, "Minimum Y", "Use the minimum Y value.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	prop= RNA_def_property(srna, "use_maximum_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FCM_LIMIT_XMAX);
	RNA_def_property_ui_text(prop, "Maximum X", "Use the maximum X value.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	prop= RNA_def_property(srna, "use_maximum_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FCM_LIMIT_YMAX);
	RNA_def_property_ui_text(prop, "Maximum Y", "Use the maximum Y value.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	prop= RNA_def_property(srna, "minimum_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rect.xmin");
	RNA_def_property_ui_text(prop, "Minimum X", "Lowest X value to allow.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	prop= RNA_def_property(srna, "minimum_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rect.ymin");
	RNA_def_property_ui_text(prop, "Minimum Y", "Lowest Y value to allow.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	prop= RNA_def_property(srna, "maximum_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rect.xmax");
	RNA_def_property_ui_text(prop, "Maximum X", "Highest X value to allow.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	prop= RNA_def_property(srna, "maximum_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rect.ymax");
	RNA_def_property_ui_text(prop, "Maximum Y", "Highest Y value to allow.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
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
		{0, NULL, 0, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "FModifierNoise", "FModifier");
	RNA_def_struct_ui_text(srna, "Noise F-Modifier", "Gives randomness to the modified F-Curve.");
	RNA_def_struct_sdna_from(srna, "FMod_Noise", "data");
	
	prop= RNA_def_property(srna, "modification", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_modification_items);
	RNA_def_property_ui_text(prop, "Modification", "Method of modifying the existing F-Curve.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	prop= RNA_def_property(srna, "size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "size");
	RNA_def_property_ui_text(prop, "Size", "Scaling (in time) of the noise");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	prop= RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "strength");
	RNA_def_property_ui_text(prop, "Strength", "Amplitude of the noise - the amount that it modifies the underlying curve");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	prop= RNA_def_property(srna, "phase", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "phase");
	RNA_def_property_ui_text(prop, "Phase", "A random seed for the noise effect");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	prop= RNA_def_property(srna, "depth", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "depth");
	RNA_def_property_ui_text(prop, "Depth", "Amount of fine level detail present in the noise");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);

}


/* --------- */

static void rna_def_fmodifier(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	/* base struct definition */
	srna= RNA_def_struct(brna, "FModifier", NULL);
	RNA_def_struct_refine_func(srna, "rna_FModifierType_refine");
	RNA_def_struct_ui_text(srna, "F-Modifier", "Modifier for values of F-Curve.");
	
#if 0 // XXX not used yet
	/* name */
	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_ui_text(prop, "Name", "Short description of F-Curve Modifier.");
#endif // XXX not used yet
	
	/* type */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, fmodifier_type_items);
	RNA_def_property_ui_text(prop, "Type", "F-Curve Modifier Type");
	
	/* settings */
	prop= RNA_def_property(srna, "expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FMODIFIER_FLAG_EXPANDED);
	RNA_def_property_ui_text(prop, "Expanded", "F-Curve Modifier's panel is expanded in UI.");
	
	prop= RNA_def_property(srna, "muted", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FMODIFIER_FLAG_MUTED);
	RNA_def_property_ui_text(prop, "Muted", "F-Curve Modifier will not be evaluated.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_PROP, NULL);
	
		// XXX this is really an internal flag, but it may be useful for some tools to be able to access this...
	prop= RNA_def_property(srna, "disabled", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FMODIFIER_FLAG_DISABLED);
	RNA_def_property_ui_text(prop, "Disabled", "F-Curve Modifier has invalid settings and will not be evaluated.");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_PROP, NULL);
	
		// TODO: setting this to true must ensure that all others in stack are turned off too...
	prop= RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FMODIFIER_FLAG_ACTIVE);
	RNA_def_property_ui_text(prop, "Active", "F-Curve Modifier is the one being edited ");
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME_PROP, NULL);
}	

/* *********************** */

static void rna_def_drivertarget(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "DriverTarget", NULL);
	RNA_def_struct_ui_text(srna, "Driver Target", "Variable from some source/target for driver relationship.");
	
	/* Variable Name */
	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_ui_text(prop, "Name", "Name to use in scripted expressions/functions. (No spaces or dots are allowed. Also, must not start with a symbol or digit)");
	//RNA_def_property_update(prop, 0, "rna_ChannelDriver_update_data"); // XXX disabled for now, until we can turn off auto updates
	
	/* Target Properties - ID-block to Drive */
	prop= RNA_def_property(srna, "id", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ID");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_editable_func(prop, "rna_DriverTarget_id_editable");
	RNA_def_property_pointer_funcs(prop, NULL, NULL, "rna_DriverTarget_id_typef");
	RNA_def_property_ui_text(prop, "ID", "ID-block that the specific property used can be found from");
	//RNA_def_property_update(prop, 0, "rna_ChannelDriver_update_data"); // XXX disabled for now, until we can turn off auto updates
	
	prop= RNA_def_property(srna, "id_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "idtype");
	RNA_def_property_enum_items(prop, id_type_items);
	RNA_def_property_enum_default(prop, ID_OB);
	RNA_def_property_enum_funcs(prop, NULL, "rna_DriverTarget_id_type_set", NULL);
	RNA_def_property_ui_text(prop, "ID Type", "Type of ID-block that can be used.");
	//RNA_def_property_update(prop, 0, "rna_ChannelDriver_update_data"); // XXX disabled for now, until we can turn off auto updates
	
	/* Target Properties - Property to Drive */
	prop= RNA_def_property(srna, "rna_path", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_DriverTarget_RnaPath_get", "rna_DriverTarget_RnaPath_length", "rna_DriverTarget_RnaPath_set");
	RNA_def_property_ui_text(prop, "RNA Path", "RNA Path (from Object) to property used");
	//RNA_def_property_update(prop, 0, "rna_ChannelDriver_update_data"); // XXX disabled for now, until we can turn off auto updates
	
	prop= RNA_def_property(srna, "array_index", PROP_INT, PROP_NONE);
	RNA_def_property_ui_text(prop, "RNA Array Index", "Index to the specific property used (if applicable)");
	//RNA_def_property_update(prop, 0, "rna_ChannelDriver_update_data"); // XXX disabled for now, until we can turn off auto updates
}


/* channeldriver.targets.* */
static void rna_def_channeldriver_targets(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
//	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	srna= RNA_def_struct(brna, "ChannelDriverTargets", NULL);
	RNA_def_struct_sdna(srna, "ChannelDriver");
	RNA_def_struct_ui_text(srna, "ChannelDriver Targets", "Collection of channel driver Targets.");

	RNA_def_property_srna(cprop, "ChannelDriverTargets");


	/* add target */
	func= RNA_def_function(srna, "add", "rna_Driver_add_target");
	RNA_def_function_ui_description(func, "Add a new target for the driver.");
		/* return type */
	parm= RNA_def_pointer(func, "target", "DriverTarget", "", "Newly created Driver Target.");
		RNA_def_function_return(func, parm);
		/* optional name parameter */
	parm= RNA_def_string(func, "name", "", 64, "Name", "Name to use in scripted expressions/functions. (No spaces or dots are allowed. Also, must not start with a symbol or digit)");

	/* remove target */
	func= RNA_def_function(srna, "remove", "rna_Driver_remove_target");
		RNA_def_function_ui_description(func, "Remove an existing target from the driver.");
		/* target to remove*/
	parm= RNA_def_pointer(func, "target", "DriverTarget", "", "Target to remove from the driver.");
		RNA_def_property_flag(parm, PROP_REQUIRED);

}

static void rna_def_channeldriver(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_type_items[] = {
		{DRIVER_TYPE_AVERAGE, "AVERAGE", 0, "Averaged Value", ""},
		{DRIVER_TYPE_PYTHON, "SCRIPTED", 0, "Scripted Expression", ""},
		{DRIVER_TYPE_ROTDIFF, "ROTDIFF", 0, "Rotational Difference", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "Driver", NULL);
	RNA_def_struct_sdna(srna, "ChannelDriver");
	RNA_def_struct_ui_text(srna, "Driver", "Driver for the value of a setting based on an external value.");

	/* Enums */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Type", "Driver types.");
	RNA_def_property_update(prop, 0, "rna_ChannelDriver_update_data");

	/* String values */
	prop= RNA_def_property(srna, "expression", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Expression", "Expression to use for Scripted Expression.");
	//RNA_def_property_update(prop, 0, "rna_ChannelDriver_update_data"); // XXX disabled for now, until we can turn off auto updates

	/* Collections */
	prop= RNA_def_property(srna, "targets", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "targets", NULL);
	RNA_def_property_struct_type(prop, "DriverTarget");
	RNA_def_property_ui_text(prop, "Target Variables", "Properties acting as targets for this driver.");
	rna_def_channeldriver_targets(brna, prop);
	
	/* Functions */
	RNA_api_drivers(srna);
}

/* *********************** */

static void rna_def_fpoint(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "FCurveSample", NULL);
	RNA_def_struct_sdna(srna, "FPoint");
	RNA_def_struct_ui_text(srna, "F-Curve Sample", "Sample point for F-Curve.");
	
	/* Boolean values */
	prop= RNA_def_property(srna, "selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", 1);
	RNA_def_property_ui_text(prop, "Selected", "Selection status");
	
	/* Vector value */
	prop= RNA_def_property(srna, "point", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_array(prop, 2);
	RNA_def_property_float_sdna(prop, NULL, "vec");
	RNA_def_property_ui_text(prop, "Point", "Point coordinates");
}

static void rna_def_fcurve(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_mode_extend_items[] = {
		{FCURVE_EXTRAPOLATE_CONSTANT, "CONSTANT", 0, "Constant", ""},
		{FCURVE_EXTRAPOLATE_LINEAR, "LINEAR", 0, "Linear", ""},
		{0, NULL, 0, NULL, NULL}};
	static EnumPropertyItem prop_mode_color_items[] = {
		{FCURVE_COLOR_AUTO_RAINBOW, "AUTO_RAINBOW", 0, "Auto Rainbow", ""},
		{FCURVE_COLOR_AUTO_RGB, "AUTO_RGB", 0, "Auto XYZ to RGB", ""},
		{FCURVE_COLOR_CUSTOM, "CUSTOM", 0, "User Defined", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "FCurve", NULL);
	RNA_def_struct_ui_text(srna, "F-Curve", "F-Curve defining values of a period of time.");
	RNA_def_struct_ui_icon(srna, ICON_ANIM_DATA);

	/* Enums */
	prop= RNA_def_property(srna, "extrapolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "extend");
	RNA_def_property_enum_items(prop, prop_mode_extend_items);
	RNA_def_property_ui_text(prop, "Extrapolation", "");

	/* Pointers */
	prop= RNA_def_property(srna, "driver", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Driver", "Channel Driver (only set for Driver F-Curves)");
	
	/* Path + Array Index */
	prop= RNA_def_property(srna, "rna_path", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_FCurve_RnaPath_get", "rna_FCurve_RnaPath_length", "rna_FCurve_RnaPath_set");
	RNA_def_property_ui_text(prop, "RNA Path", "RNA Path to property affected by F-Curve.");
	
	prop= RNA_def_property(srna, "array_index", PROP_INT, PROP_NONE);
	RNA_def_property_ui_text(prop, "RNA Array Index", "Index to the specific property affected by F-Curve if applicable.");
	
	/* Color */
	prop= RNA_def_property(srna, "color_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_mode_color_items);
	RNA_def_property_ui_text(prop, "Color Mode", "Method used to determine color of F-Curve in Graph Editor.");
	
	prop= RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Color", "Color of the F-Curve in the Graph Editor.");
	
	/* Collections */
	prop= RNA_def_property(srna, "sampled_points", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "fpt", "totvert");
	RNA_def_property_struct_type(prop, "FCurveSample");
	RNA_def_property_ui_text(prop, "Sampled Points", "Sampled animation data");

	prop= RNA_def_property(srna, "keyframe_points", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "bezt", "totvert");
	RNA_def_property_struct_type(prop, "BezierCurvePoint");
	RNA_def_property_ui_text(prop, "Keyframes", "User-editable keyframes");
	
	prop= RNA_def_property(srna, "modifiers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "FModifier");
	RNA_def_property_ui_text(prop, "Modifiers", "Modifiers affecting the shape of the F-Curve.");
}

/* *********************** */

void RNA_def_fcurve(BlenderRNA *brna)
{
	rna_def_fcurve(brna);
	rna_def_fpoint(brna);
	
	rna_def_drivertarget(brna);
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
}


#endif
