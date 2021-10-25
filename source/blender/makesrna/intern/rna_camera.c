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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_camera.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "DNA_camera_types.h"

#include "BLI_math.h"

#include "RNA_define.h"

#include "rna_internal.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "BKE_camera.h"
#include "BKE_object.h"
#include "BKE_depsgraph.h"

static float rna_Camera_angle_get(PointerRNA *ptr)
{
	Camera *cam = ptr->id.data;
	float sensor = BKE_camera_sensor_size(cam->sensor_fit, cam->sensor_x, cam->sensor_y);
	return focallength_to_fov(cam->lens, sensor);
}

static void rna_Camera_angle_set(PointerRNA *ptr, float value)
{
	Camera *cam = ptr->id.data;
	float sensor = BKE_camera_sensor_size(cam->sensor_fit, cam->sensor_x, cam->sensor_y);
	cam->lens = fov_to_focallength(value, sensor);
}

static float rna_Camera_angle_x_get(PointerRNA *ptr)
{
	Camera *cam = ptr->id.data;
	return focallength_to_fov(cam->lens, cam->sensor_x);
}

static void rna_Camera_angle_x_set(PointerRNA *ptr, float value)
{
	Camera *cam = ptr->id.data;
	cam->lens = fov_to_focallength(value, cam->sensor_x);
}

static float rna_Camera_angle_y_get(PointerRNA *ptr)
{
	Camera *cam = ptr->id.data;
	return focallength_to_fov(cam->lens, cam->sensor_y);
}

static void rna_Camera_angle_y_set(PointerRNA *ptr, float value)
{
	Camera *cam = ptr->id.data;
	cam->lens = fov_to_focallength(value, cam->sensor_y);
}

static void rna_Camera_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Camera *camera = (Camera *)ptr->id.data;

	DAG_id_tag_update(&camera->id, 0);
}

static void rna_Camera_dependency_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
	Camera *camera = (Camera *)ptr->id.data;
	DAG_relations_tag_update(bmain);
	DAG_id_tag_update(&camera->id, 0);
}

#else

static void rna_def_camera_stereo_data(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem convergence_mode_items[] = {
		{CAM_S3D_OFFAXIS, "OFFAXIS", 0, "Off-Axis", "Off-axis frustums converging in a plane"},
		{CAM_S3D_PARALLEL, "PARALLEL", 0, "Parallel", "Parallel cameras with no convergence"},
		{CAM_S3D_TOE, "TOE", 0, "Toe-in", "Rotated cameras, looking at the convergence distance"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem pivot_items[] = {
		{CAM_S3D_PIVOT_LEFT, "LEFT", 0, "Left", ""},
		{CAM_S3D_PIVOT_RIGHT, "RIGHT", 0, "Right", ""},
		{CAM_S3D_PIVOT_CENTER, "CENTER", 0, "Center", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "CameraStereoData", NULL);
	RNA_def_struct_sdna(srna, "CameraStereoSettings");
	RNA_def_struct_nested(brna, srna, "Camera");
	RNA_def_struct_ui_text(srna, "Stereo", "Stereoscopy settings for a Camera data-block");

	prop = RNA_def_property(srna, "convergence_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, convergence_mode_items);
	RNA_def_property_ui_text(prop, "Mode", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

	prop = RNA_def_property(srna, "pivot", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, pivot_items);
	RNA_def_property_ui_text(prop, "Pivot", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

	prop = RNA_def_property(srna, "interocular_distance", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 1e4f, 1, 3);
	RNA_def_property_ui_text(prop, "Interocular Distance",
	                         "Set the distance between the eyes - the stereo plane distance / 30 should be fine");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

	prop = RNA_def_property(srna, "convergence_distance", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_range(prop, 0.00001f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.00001f, 15.f, 1, 3);
	RNA_def_property_ui_text(prop, "Convergence Plane Distance",
	                         "The converge point for the stereo cameras "
	                         "(often the distance between a projector and the projection screen)");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

	prop = RNA_def_property(srna, "use_spherical_stereo", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_S3D_SPHERICAL);
	RNA_def_property_ui_text(prop, "Spherical Stereo",
	                         "Render every pixel rotating the camera around the "
	                         "middle of the interocular distance");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

	prop = RNA_def_property(srna, "use_pole_merge", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_S3D_POLE_MERGE);
	RNA_def_property_ui_text(prop, "Use Pole Merge",
	                         "Fade interocular distance to 0 after the given cutoff angle");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

	prop = RNA_def_property(srna, "pole_merge_angle_from", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_range(prop, 0.0f, M_PI / 2.0);
	RNA_def_property_ui_text(prop, "Pole Merge Start Angle",
	                         "Angle at which interocular distance starts to fade to 0");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

	prop = RNA_def_property(srna, "pole_merge_angle_to", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_range(prop, 0.0f, M_PI / 2.0);
	RNA_def_property_ui_text(prop, "Pole Merge End Angle",
	                         "Angle at which interocular distance is 0");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);
}

void RNA_def_camera(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem prop_type_items[] = {
		{CAM_PERSP, "PERSP", 0, "Perspective", ""},
		{CAM_ORTHO, "ORTHO", 0, "Orthographic", ""},
		{CAM_PANO, "PANO", 0, "Panoramic", ""},
		{0, NULL, 0, NULL, NULL}
	};
	static EnumPropertyItem prop_draw_type_extra_items[] = {
		{CAM_DTX_CENTER, "CENTER", 0, "Center", ""},
		{CAM_DTX_CENTER_DIAG, "CENTER_DIAGONAL", 0, "Center Diagonal", ""},
		{CAM_DTX_THIRDS, "THIRDS", 0, "Thirds", ""},
		{CAM_DTX_GOLDEN, "GOLDEN", 0, "Golden", ""},
		{CAM_DTX_GOLDEN_TRI_A, "GOLDEN_TRIANGLE_A", 0, "Golden Triangle A", ""},
		{CAM_DTX_GOLDEN_TRI_B, "GOLDEN_TRIANGLE_B", 0, "Golden Triangle B", ""},
		{CAM_DTX_HARMONY_TRI_A, "HARMONY_TRIANGLE_A", 0, "Harmonious Triangle A", ""},
		{CAM_DTX_HARMONY_TRI_B, "HARMONY_TRIANGLE_B", 0, "Harmonious Triangle B", ""},
		{0, NULL, 0, NULL, NULL}
	};
	static EnumPropertyItem prop_lens_unit_items[] = {
		{0, "MILLIMETERS", 0, "Millimeters", "Specify the lens in millimeters"},
		{CAM_ANGLETOGGLE, "FOV", 0, "Field of View", "Specify the lens as the field of view's angle"},
		{0, NULL, 0, NULL, NULL}
	};
	static EnumPropertyItem sensor_fit_items[] = {
		{CAMERA_SENSOR_FIT_AUTO, "AUTO", 0, "Auto", "Fit to the sensor width or height depending on image resolution"},
		{CAMERA_SENSOR_FIT_HOR, "HORIZONTAL", 0, "Horizontal", "Fit to the sensor width"},
		{CAMERA_SENSOR_FIT_VERT, "VERTICAL", 0, "Vertical", "Fit to the sensor height"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "Camera", "ID");
	RNA_def_struct_ui_text(srna, "Camera", "Camera data-block for storing camera settings");
	RNA_def_struct_ui_icon(srna, ICON_CAMERA_DATA);

	/* Enums */
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Type", "Camera types");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Camera_update");

	prop = RNA_def_property(srna, "show_guide", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "dtx");
	RNA_def_property_enum_items(prop, prop_draw_type_extra_items);
	RNA_def_property_flag(prop, PROP_ENUM_FLAG);
	RNA_def_property_ui_text(prop, "Composition Guides",  "Draw overlay");
	RNA_def_property_update(prop, NC_CAMERA | ND_DRAW_RENDER_VIEWPORT, NULL);

	prop = RNA_def_property(srna, "sensor_fit", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "sensor_fit");
	RNA_def_property_enum_items(prop, sensor_fit_items);
	RNA_def_property_ui_text(prop, "Sensor Fit", "Method to fit image and field of view angle inside the sensor");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Camera_update");

	/* Number values */

	prop = RNA_def_property(srna, "passepartout_alpha", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "passepartalpha");
	RNA_def_property_ui_text(prop, "Passepartout Alpha", "Opacity (alpha) of the darkened overlay in Camera view");
	RNA_def_property_update(prop, NC_CAMERA | ND_DRAW_RENDER_VIEWPORT, NULL);

	prop = RNA_def_property(srna, "angle_x", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_range(prop, DEG2RAD(0.367), DEG2RAD(172.847));
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Horizontal FOV", "Camera lens horizontal field of view");
	RNA_def_property_float_funcs(prop, "rna_Camera_angle_x_get", "rna_Camera_angle_x_set", NULL);
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Camera_update");

	prop = RNA_def_property(srna, "angle_y", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_range(prop, DEG2RAD(0.367), DEG2RAD(172.847));
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Vertical FOV", "Camera lens vertical field of view");
	RNA_def_property_float_funcs(prop, "rna_Camera_angle_y_get", "rna_Camera_angle_y_set", NULL);
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Camera_update");

	prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_range(prop, DEG2RAD(0.367), DEG2RAD(172.847));
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Field of View", "Camera lens field of view");
	RNA_def_property_float_funcs(prop, "rna_Camera_angle_get", "rna_Camera_angle_set", NULL);
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Camera_update");

	prop = RNA_def_property(srna, "clip_start", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "clipsta");
	RNA_def_property_range(prop, 1e-6f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 10, 3);
	RNA_def_property_ui_text(prop, "Clip Start", "Camera near clipping distance");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

	prop = RNA_def_property(srna, "clip_end", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "clipend");
	RNA_def_property_range(prop, 1e-6f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 10, 3);
	RNA_def_property_ui_text(prop, "Clip End", "Camera far clipping distance");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

	prop = RNA_def_property(srna, "lens", PROP_FLOAT, PROP_DISTANCE_CAMERA);
	RNA_def_property_float_sdna(prop, NULL, "lens");
	RNA_def_property_range(prop, 1.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 1.0f, 5000.0f, 1, 2);
	RNA_def_property_ui_text(prop, "Focal Length", "Perspective Camera lens value in millimeters");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Camera_update");

	prop = RNA_def_property(srna, "sensor_width", PROP_FLOAT, PROP_DISTANCE_CAMERA);
	RNA_def_property_float_sdna(prop, NULL, "sensor_x");
	RNA_def_property_range(prop, 1.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 1.0f, 100.f, 1, 2);
	RNA_def_property_ui_text(prop, "Sensor Width", "Horizontal size of the image sensor area in millimeters");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Camera_update");

	prop = RNA_def_property(srna, "sensor_height", PROP_FLOAT, PROP_DISTANCE_CAMERA);
	RNA_def_property_float_sdna(prop, NULL, "sensor_y");
	RNA_def_property_range(prop, 1.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 1.0f, 100.f, 1, 2);
	RNA_def_property_ui_text(prop, "Sensor Height", "Vertical size of the image sensor area in millimeters");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Camera_update");

	prop = RNA_def_property(srna, "ortho_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ortho_scale");
	RNA_def_property_range(prop, FLT_MIN, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.001f, 10000.0f, 10, 3);
	RNA_def_property_ui_text(prop, "Orthographic Scale", "Orthographic Camera scale (similar to zoom)");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Camera_update");

	prop = RNA_def_property(srna, "draw_size", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "drawsize");
	RNA_def_property_range(prop, 0.01f, 1000.0f);
	RNA_def_property_ui_range(prop, 0.01, 100, 1, 2);
	RNA_def_property_ui_text(prop, "Draw Size", "Apparent size of the Camera object in the 3D View");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

	prop = RNA_def_property(srna, "shift_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "shiftx");
	RNA_def_property_range(prop, -10.0f, 10.0f);
	RNA_def_property_ui_range(prop, -2.0, 2.0, 1, 3);
	RNA_def_property_ui_text(prop, "Shift X", "Camera horizontal shift");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Camera_update");

	prop = RNA_def_property(srna, "shift_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "shifty");
	RNA_def_property_range(prop, -10.0f, 10.0f);
	RNA_def_property_ui_range(prop, -2.0, 2.0, 1, 3);
	RNA_def_property_ui_text(prop, "Shift Y", "Camera vertical shift");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Camera_update");

	prop = RNA_def_property(srna, "dof_distance", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "YF_dofdist");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 5000.0f, 1, 2);
	RNA_def_property_ui_text(prop, "DOF Distance", "Distance to the focus point for depth of field");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

	/* Stereo Settings */
	prop = RNA_def_property(srna, "stereo", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "stereo");
	RNA_def_property_struct_type(prop, "CameraStereoData");
	RNA_def_property_ui_text(prop, "Stereo", "");

	/* flag */
	prop = RNA_def_property(srna, "show_limits", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_SHOWLIMITS);
	RNA_def_property_ui_text(prop, "Show Limits", "Draw the clipping range and focus point on the camera");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

	prop = RNA_def_property(srna, "show_mist", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_SHOWMIST);
	RNA_def_property_ui_text(prop, "Show Mist", "Draw a line from the Camera to indicate the mist area");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

	prop = RNA_def_property(srna, "show_passepartout", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_SHOWPASSEPARTOUT);
	RNA_def_property_ui_text(prop, "Show Passepartout",
	                         "Show a darkened overlay outside the image area in Camera view");
	RNA_def_property_update(prop, NC_CAMERA | ND_DRAW_RENDER_VIEWPORT, NULL);

	prop = RNA_def_property(srna, "show_safe_areas", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_SHOW_SAFE_MARGINS);
	RNA_def_property_ui_text(prop, "Show Safe Areas", "Show TV title safe and action safe areas in Camera view");
	RNA_def_property_update(prop, NC_CAMERA | ND_DRAW_RENDER_VIEWPORT, NULL);

	prop = RNA_def_property(srna, "show_safe_center", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_SHOW_SAFE_CENTER);
	RNA_def_property_ui_text(prop, "Show Center-cut safe areas",
	                         "Show safe areas to fit content in a different aspect ratio");
	RNA_def_property_update(prop, NC_CAMERA | ND_DRAW_RENDER_VIEWPORT, NULL);

	prop = RNA_def_property(srna, "show_name", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_SHOWNAME);
	RNA_def_property_ui_text(prop, "Show Name", "Show the active Camera's name in Camera view");
	RNA_def_property_update(prop, NC_CAMERA | ND_DRAW_RENDER_VIEWPORT, NULL);

	prop = RNA_def_property(srna, "show_sensor", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CAM_SHOWSENSOR);
	RNA_def_property_ui_text(prop, "Show Sensor Size", "Show sensor size (film gate) in Camera view");
	RNA_def_property_update(prop, NC_CAMERA | ND_DRAW_RENDER_VIEWPORT, NULL);

	prop = RNA_def_property(srna, "lens_unit", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, prop_lens_unit_items);
	RNA_def_property_ui_text(prop, "Lens Unit", "Unit to edit lens in for the user interface");

	/* pointers */
	prop = RNA_def_property(srna, "dof_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_sdna(prop, NULL, "dof_ob");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "DOF Object", "Use this object to define the depth of field focal point");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Camera_dependency_update");

	prop = RNA_def_property(srna, "gpu_dof", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "GPUDOFSettings");
	RNA_def_property_ui_text(prop, "GPU Depth Of Field", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

	rna_def_animdata_common(srna);

	/* Nested Data  */
	RNA_define_animate_sdna(true);

	/* *** Animated *** */
	rna_def_camera_stereo_data(brna);

	/* Camera API */
	RNA_api_camera(srna);
}

#endif
