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

#define _USE_MATH_DEFINES
#include <stdlib.h>
#include <math.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_camera_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

static void rna_Camera_angle_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Camera *cam= (Camera*)ptr->id.data;
	cam->lens = 16.0f / tan(M_PI*cam->angle/360.0f);
}

static void rna_Camera_lens_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Camera *cam= (Camera*)ptr->id.data;
	cam->angle= 360.0f * atan(16.0f/cam->lens) / M_PI;
}


#else

void RNA_def_camera(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem prop_type_items[] = {
		{CAM_PERSP, "PERSP", 0, "Perspective", ""},
		{CAM_ORTHO, "ORTHO", 0, "Orthographic", ""},
		{0, NULL, 0, NULL, NULL}};
	static EnumPropertyItem prop_lens_unit_items[] = {
		{0, "MILLIMETERS", 0, "Millimeters", ""},
		{CAM_ANGLETOGGLE, "DEGREES", 0, "Degrees", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "Camera", "ID");
	RNA_def_struct_ui_text(srna, "Camera", "Camera datablock for storing camera settings.");
	RNA_def_struct_ui_icon(srna, ICON_CAMERA_DATA);

	/* Enums */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Type", "Camera types.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);
	
	/* Number values */

	prop= RNA_def_property(srna, "passepartout_alpha", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "passepartalpha");
	RNA_def_property_ui_text(prop, "Passepartout Alpha", "Opacity (alpha) of the darkened overlay in Camera view.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop= RNA_def_property(srna, "angle", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "angle");
	RNA_def_property_range(prop, 0.0f, 172.85f);
	RNA_def_property_ui_text(prop, "Angle", "Perspective Camera lend field of view in degrees.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Camera_angle_update");

	prop= RNA_def_property(srna, "clip_start", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clipsta");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_text(prop, "Clip Start", "Camera near clipping distance.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop= RNA_def_property(srna, "clip_end", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clipend");
	RNA_def_property_range(prop, 1.0f, FLT_MAX);
	RNA_def_property_ui_text(prop, "Clip End", "Camera far clipping distance.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop= RNA_def_property(srna, "lens", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "lens");
	RNA_def_property_range(prop, 1.0f, 250.0f);
	RNA_def_property_ui_text(prop, "Lens", "Perspective Camera lens value in mm.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_Camera_lens_update");

	prop= RNA_def_property(srna, "ortho_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ortho_scale");
	RNA_def_property_range(prop, 0.01f, 1000.0f);
	RNA_def_property_ui_text(prop, "Orthographic Scale", "Orthographic Camera scale (similar to zoom).");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop= RNA_def_property(srna, "draw_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "drawsize");
	RNA_def_property_range(prop, 0.1f, 10.0f);
	RNA_def_property_ui_text(prop, "Draw Size", "Apparent size of the Camera object in the 3D View.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop= RNA_def_property(srna, "shift_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "shiftx");
	RNA_def_property_range(prop, -2.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Shift X", "Perspective Camera horizontal shift.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop= RNA_def_property(srna, "shift_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "shifty");
	RNA_def_property_range(prop, -2.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Shift Y", "Perspective Camera vertical shift.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop= RNA_def_property(srna, "dof_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "YF_dofdist");
	RNA_def_property_range(prop, 0.0f, 5000.0f);
	RNA_def_property_ui_text(prop, "DOF Distance", "Distance to the focus point for depth of field.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	/* flag */
	prop= RNA_def_property(srna, "show_limits", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_SHOWLIMITS);
	RNA_def_property_ui_text(prop, "Show Limits", "Draw the clipping range and focus point on the camera.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop= RNA_def_property(srna, "show_mist", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_SHOWMIST);
	RNA_def_property_ui_text(prop, "Show Mist", "Draw a line from the Camera to indicate the mist area.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop= RNA_def_property(srna, "show_passepartout", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_SHOWPASSEPARTOUT);
	RNA_def_property_ui_text(prop, "Show Passepartout", "Show a darkened overlay outside the image area in Camera view.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop= RNA_def_property(srna, "show_title_safe", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_SHOWTITLESAFE);
	RNA_def_property_ui_text(prop, "Show Title Safe", "Show indicators for the title safe zone in Camera view.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop= RNA_def_property(srna, "show_name", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_SHOWNAME);
	RNA_def_property_ui_text(prop, "Show Name", "Show the active Camera's name in Camera view.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop= RNA_def_property(srna, "lens_unit", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, prop_lens_unit_items);
	RNA_def_property_ui_text(prop, "Lens Unit", "Unit to edit lens in for the user interface.");

	prop= RNA_def_property(srna, "panorama", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_PANORAMA);
	RNA_def_property_ui_text(prop, "Panorama", "Render the scene with a cylindrical camera for pseudo-fisheye lens effects.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	/* pointers */
	rna_def_animdata_common(srna);

	prop= RNA_def_property(srna, "dof_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_sdna(prop, NULL, "dof_ob");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "DOF Object", "Use this object to define the depth of field focal point.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);
}

#endif

