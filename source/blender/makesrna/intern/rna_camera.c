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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_camera_types.h"

#ifdef RNA_RUNTIME

#else

void RNA_def_camera(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem prop_type_items[] = {
		{CAM_PERSP, "PERSP", "Perspective", ""},
		{CAM_ORTHO, "ORTHO", "Orthographic", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "Camera", "ID", "Camera");

	/* Enums */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Type", "Camera types.");

	/* Number values */

	prop= RNA_def_property(srna, "passepartout_alpha", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "passepartalpha");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Passepartout Alpha", "Opacity (alpha) of the passepartout area in Camera view.");

	prop= RNA_def_property(srna, "angle", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "angle");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Angle", "Perspective Camera lens value in degrees.");

	prop= RNA_def_property(srna, "clip_start", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clipsta");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Clip Start", "Camera clipping start.");

	prop= RNA_def_property(srna, "clip_end", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clipend");
	RNA_def_property_range(prop, 1.0f, 5000.0f);
	RNA_def_property_ui_text(prop, "Clip End", "Camera clipping end.");

	prop= RNA_def_property(srna, "lens", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "lens");
	RNA_def_property_range(prop, 1.0f, 250.0f);
	RNA_def_property_ui_text(prop, "Lens", "Perspective Camera lens value in mm.");

	prop= RNA_def_property(srna, "ortho_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ortho_scale");
	RNA_def_property_range(prop, 0.01f, 1000.0f);
	RNA_def_property_ui_text(prop, "Orthographic Scale", "Ortographic Camera scale.");

	prop= RNA_def_property(srna, "draw_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "drawsize");
	RNA_def_property_range(prop, 0.1f, 10.0f);
	RNA_def_property_ui_text(prop, "Draw Size", "Apparent size of the Camera object in the 3D View.");

	prop= RNA_def_property(srna, "shift_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "shiftx");
	RNA_def_property_range(prop, -2.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Shift X", "Perspective Camera horizontal shift.");

	prop= RNA_def_property(srna, "shift_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "shifty");
	RNA_def_property_range(prop, -2.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Shift Y", "Perspective Camera vertical shift.");

	prop= RNA_def_property(srna, "dof_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "YF_dofdist");
	RNA_def_property_range(prop, 0.0f, 5000.0f);
	RNA_def_property_ui_text(prop, "DOF Distance", "Depth of field distance.");

	/* flag */
	prop= RNA_def_property(srna, "show_limits", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_SHOWLIMITS);
	RNA_def_property_ui_text(prop, "Show Limits", "Draw the clipping range and focal point in Camera view.");

	prop= RNA_def_property(srna, "show_mist", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_SHOWMIST);
	RNA_def_property_ui_text(prop, "Show Mist", "Draw a line from the Camera to indicate the mist area.");

	prop= RNA_def_property(srna, "show_passepartout", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_SHOWPASSEPARTOUT);
	RNA_def_property_ui_text(prop, "Show Passepartout", "Enable passepartout mode in Camera view.");

	prop= RNA_def_property(srna, "show_title_safe", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_SHOWTITLESAFE);
	RNA_def_property_ui_text(prop, "Show Title Safe", "Draw a title safe zone in Camera view.");

	prop= RNA_def_property(srna, "show_name", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_SHOWNAME);
	RNA_def_property_ui_text(prop, "Show Name", "Draw the active Camera's name in Camera view.");

	prop= RNA_def_property(srna, "use_degrees", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_ANGLETOGGLE);
	RNA_def_property_ui_text(prop, "Use Degrees", "Use degrees instead of mm as the unit of the Camera lens.");

	/* Pointers */
	rna_def_ipo_common(srna);

	prop= RNA_def_property(srna, "dof_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_sdna(prop, NULL, "dof_ob");
	RNA_def_property_ui_text(prop, "DOF Object", "Use this object to define the depth of field focal point.");
}

#endif

