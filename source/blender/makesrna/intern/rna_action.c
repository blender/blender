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
 * Contributor(s): Blender Foundation (2008), Roland Hess
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#ifdef RNA_RUNTIME

static void rna_Driver_RnaPath_get(PointerRNA *ptr, char *value)
{
	ChannelDriver *driver= (ChannelDriver *)ptr->data;

	if (driver->rna_path)
		strcpy(value, driver->rna_path);
	else
		strcpy(value, "");
}

static int rna_Driver_RnaPath_length(PointerRNA *ptr)
{
	ChannelDriver *driver= (ChannelDriver *)ptr->data;
	
	if (driver->rna_path)
		return strlen(driver->rna_path);
	else
		return 0;
}

#if 0
static void rna_Driver_RnaPath_set(PointerRNA *ptr, const char *value)
{
	ChannelDriver *driver= (ChannelDriver *)ptr->data;

	if (driver->rna_path)
		MEM_freeN(driver->rna_path);
	
	if (strlen(value))
		driver->rna_path= BLI_strdup(value);
	else 
		driver->rna_path= NULL;
}
#endif

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

#if 0
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
#endif

#else

// XXX maybe this should be in a separate file?
void rna_def_channeldriver(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_type_items[] = {
		{DRIVER_TYPE_CHANNEL, "NORMAL", "Normal", ""},
		{DRIVER_TYPE_PYTHON, "SCRIPTED", "Scripted Expression", ""},
		{DRIVER_TYPE_ROTDIFF, "ROTDIFF", "Rotational Difference", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "ChannelDriver", NULL);
	RNA_def_struct_ui_text(srna, "Driver", "Driver for the value of a setting based on an external value.");

	/* Enums */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Type", "Driver types.");

	/* String values */
	prop= RNA_def_property(srna, "expression", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Expression", "Expression to use for Scripted Expression.");

	/* Pointers */
	prop= RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_ui_text(prop, "Driver Object", "Object that controls this Driver.");
	
	prop= RNA_def_property(srna, "rna_path", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Driver_RnaPath_get", "rna_Driver_RnaPath_length", "rna_Driver_RnaPath_set");
	RNA_def_property_ui_text(prop, "Driver RNA Path", "RNA Path (from Driver Object) to property used as Driver.");
	
	prop= RNA_def_property(srna, "array_index", PROP_INT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Driver RNA Array Index", "Index to the specific property used as Driver if applicable.");
}

// XXX maybe this should be in a separate file?
void rna_def_fcurve(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_mode_extend_items[] = {
		{FCURVE_EXTRAPOLATE_CONSTANT, "CONSTANT", "Constant", ""},
		{FCURVE_EXTRAPOLATE_LINEAR, "LINEAR", "Linear", ""},
		{0, NULL, NULL, NULL}};
	static EnumPropertyItem prop_mode_color_items[] = {
		{FCURVE_COLOR_AUTO_RAINBOW, "AUTO_RAINBOW", "Automatic Rainbow", ""},
		{FCURVE_COLOR_AUTO_RGB, "AUTO_RGB", "Automatic XYZ to RGB", ""},
		{FCURVE_COLOR_CUSTOM, "CUSTOM", "User Defined", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "FCurve", NULL);
	RNA_def_struct_ui_text(srna, "F-Curve", "F-Curve defining values of a period of time.");

	/* Enums */
	prop= RNA_def_property(srna, "extrapolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "extend");
	RNA_def_property_enum_items(prop, prop_mode_extend_items);
	RNA_def_property_ui_text(prop, "Extrapolation", "");

	/* Pointers */
	prop= RNA_def_property(srna, "driver", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); // xxx?
	RNA_def_property_ui_text(prop, "Driver", "Channel Driver (only set for Driver F-Curves)");
	
	/* Path + Array Index */
	prop= RNA_def_property(srna, "rna_path", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
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
	RNA_def_property_struct_type(prop, "CurvePoint");
	RNA_def_property_ui_text(prop, "Sampled Points", "Sampled animation data");

	prop= RNA_def_property(srna, "keyframe_points", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "bezt", "totvert");
	RNA_def_property_struct_type(prop, "BezierCurvePoint");
	RNA_def_property_ui_text(prop, "Keyframes", "User-editable keyframes");
	
	// XXX to add modifiers...
}

/* --- */

void rna_def_action_group(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ActionGroup", NULL);
	RNA_def_struct_sdna(srna, "bActionGroup");
	RNA_def_struct_ui_text(srna, "Action Group", "Groups of F-Curves.");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);

	/* dna warns not to treat the Action Channel listbase in the Action Group struct like a
	   normal listbase. I'll leave this here but comment out, for Joshua to review. He can 
 	   probably shed some more light on why this is */
	/*prop= RNA_def_property(srna, "channels", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "channels", NULL);
	RNA_def_property_struct_type(prop, "FCurve");
	RNA_def_property_ui_text(prop, "Channels", "F-Curves in this group.");*/

	prop= RNA_def_property(srna, "selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", AGRP_SELECTED);
	RNA_def_property_ui_text(prop, "Selected", "Action Group is selected.");

	prop= RNA_def_property(srna, "locked", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", AGRP_PROTECTED);
	RNA_def_property_ui_text(prop, "Locked", "Action Group is locked.");

	prop= RNA_def_property(srna, "expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", AGRP_EXPANDED);
	RNA_def_property_ui_text(prop, "Expanded", "Action Group is expanded.");

	prop= RNA_def_property(srna, "custom_color", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "customCol");
	RNA_def_property_ui_text(prop, "Custom Color", "Index of custom color set.");
}

void rna_def_action(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "Action", "ID");
	RNA_def_struct_sdna(srna, "bAction");
	RNA_def_struct_ui_text(srna, "Action", "A collection of F-Curves for animation.");

	prop= RNA_def_property(srna, "fcurves", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "curves", NULL);
	RNA_def_property_struct_type(prop, "FCurve");
	RNA_def_property_ui_text(prop, "F-Curves", "The individual F-Curves that make up the Action.");

	prop= RNA_def_property(srna, "groups", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "groups", NULL);
	RNA_def_property_struct_type(prop, "ActionGroup");
	RNA_def_property_ui_text(prop, "Groups", "Convenient groupings of F-Curves.");

	prop= RNA_def_property(srna, "pose_markers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "markers", NULL);
	RNA_def_property_struct_type(prop, "TimelineMarker");
	RNA_def_property_ui_text(prop, "Pose Markers", "Markers specific to this Action, for labeling poses.");
}

/* --------- */

void RNA_def_action(BlenderRNA *brna)
{
	rna_def_action(brna);
	rna_def_action_group(brna);
	
	// should these be in their own file, or is that overkill?
	rna_def_fcurve(brna);
	rna_def_channeldriver(brna);
}


#endif
