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
 * Contributor(s): Blender Foundation (2008), Juho Vepsäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_curve_types.h"

#ifdef RNA_RUNTIME

static void rna_BezTriple_handle1_get(PointerRNA *ptr, float *values)
{
	BezTriple *bt= (BezTriple*)ptr->data;

	values[0]= bt->vec[0][0];
	values[1]= bt->vec[0][1];
	values[2]= bt->vec[0][2];
}

static void rna_BezTriple_handle1_set(PointerRNA *ptr, const float *values)
{
	BezTriple *bt= (BezTriple*)ptr->data;

	bt->vec[0][0]= values[0];
	bt->vec[0][1]= values[1];
	bt->vec[0][2]= values[2];
}

static void rna_BezTriple_handle2_get(PointerRNA *ptr, float *values)
{
	BezTriple *bt= (BezTriple*)ptr->data;

	values[0]= bt->vec[2][0];
	values[1]= bt->vec[2][1];
	values[2]= bt->vec[2][2];
}

static void rna_BezTriple_handle2_set(PointerRNA *ptr, const float *values)
{
	BezTriple *bt= (BezTriple*)ptr->data;

	bt->vec[2][0]= values[0];
	bt->vec[2][1]= values[1];
	bt->vec[2][2]= values[2];
}

static void rna_BezTriple_ctrlpoint_get(PointerRNA *ptr, float *values)
{
	BezTriple *bt= (BezTriple*)ptr->data;

	values[0]= bt->vec[1][0];
	values[1]= bt->vec[1][1];
	values[2]= bt->vec[1][2];
}

static void rna_BezTriple_ctrlpoint_set(PointerRNA *ptr, const float *values)
{
	BezTriple *bt= (BezTriple*)ptr->data;

	bt->vec[1][0]= values[0];
	bt->vec[1][1]= values[1];
	bt->vec[1][2]= values[2];
}

static int rna_Curve_texspace_editable(PointerRNA *ptr)
{
	Curve *cu= (Curve*)ptr->data;
	return (cu->texflag & CU_AUTOSPACE)? 0: PROP_EDITABLE;
}

#else

static void rna_def_bpoint(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "CurvePoint", NULL);
	RNA_def_struct_sdna(srna, "BPoint");
	RNA_def_struct_ui_text(srna, "CurvePoint", "Curve point without handles.");

	/* Boolean values */
	prop= RNA_def_property(srna, "selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "f1", 0);
	RNA_def_property_ui_text(prop, "Selected", "Selection status");

	prop= RNA_def_property(srna, "hidden", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "hide", 0);
	RNA_def_property_ui_text(prop, "Hidden", "Visibility status");

	/* Vector value */
	prop= RNA_def_property(srna, "point", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_array(prop, 4);
	RNA_def_property_float_sdna(prop, NULL, "vec");
	RNA_def_property_ui_text(prop, "Point", "Point coordinates");

	/* Number values */
	prop= RNA_def_property(srna, "tilt", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "alfa");
	/*RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);*/
	RNA_def_property_ui_text(prop, "Tilt", "Tilt in 3d View");

	prop= RNA_def_property(srna, "weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.01f, 100.0f);
	RNA_def_property_ui_text(prop, "Weight", "Softbody goal weight");

	prop= RNA_def_property(srna, "bevel_radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "radius");
	/*RNA_def_property_range(prop, 0.0f, 1.0f);*/
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Bevel Radius", "Radius for bevelling");
}

static void rna_def_beztriple(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem prop_handle_type_items[] = {
		{HD_FREE, "FREE", "Free", ""},
		{HD_AUTO, "AUTO", "Auto", ""},
		{HD_VECT, "VECTOR", "Vector", ""},
		{HD_ALIGN, "ALIGNED", "Aligned", ""},
		{HD_AUTO_ANIM, "AUTO_CLAMPED", "Auto Clamped", ""},
		{0, NULL, NULL, NULL}};
	static EnumPropertyItem prop_mode_interpolation_items[] = {
		{BEZT_IPO_CONST, "CONSTANT", "Constant", ""},
		{BEZT_IPO_LIN, "LINEAR", "Linear", ""},
		{BEZT_IPO_BEZ, "BEZIER", "Bezier", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "BezierCurvePoint", NULL);
	RNA_def_struct_sdna(srna, "BezTriple");
	RNA_def_struct_ui_text(srna, "Bezier Curve Point", "Bezier curve point with two handles.");

	/* Boolean values */
	prop= RNA_def_property(srna, "selected_handle1", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "f1", 0);
	RNA_def_property_ui_text(prop, "Handle 1 selected", "Handle 1 selection status");

	prop= RNA_def_property(srna, "selected_handle2", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "f3", 0);
	RNA_def_property_ui_text(prop, "Handle 2 selected", "Handle 2 selection status");

	prop= RNA_def_property(srna, "selected_control_point", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "f2", 0);
	RNA_def_property_ui_text(prop, "Control Point selected", "Control point selection status");

	prop= RNA_def_property(srna, "hidden", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "hide", 0);
	RNA_def_property_ui_text(prop, "Hidden", "Visibility status");

	/* Enums */
	prop= RNA_def_property(srna, "handle1_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "h1");
	RNA_def_property_enum_items(prop, prop_handle_type_items);
	RNA_def_property_ui_text(prop, "Handle 1 Type", "Handle types");

	prop= RNA_def_property(srna, "handle2_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "h2");
	RNA_def_property_enum_items(prop, prop_handle_type_items);
	RNA_def_property_ui_text(prop, "Handle 2 Type", "Handle types");

	prop= RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "ipo");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, prop_mode_interpolation_items);
	RNA_def_property_ui_text(prop, "Interpolation", "(For F-Curves Only) Interpolation to use for segment of curve starting from current BezTriple.");

	/* Vector values */
	prop= RNA_def_property(srna, "handle1", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_BezTriple_handle1_get", "rna_BezTriple_handle1_set", NULL);
	RNA_def_property_ui_text(prop, "Handle 1", "Coordinates of the first handle");

	prop= RNA_def_property(srna, "control_point", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_BezTriple_ctrlpoint_get", "rna_BezTriple_ctrlpoint_set", NULL);
	RNA_def_property_ui_text(prop, "Control Point", "Coordinates of the control point");

	prop= RNA_def_property(srna, "handle2", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_BezTriple_handle2_get", "rna_BezTriple_handle2_set", NULL);
	RNA_def_property_ui_text(prop, "Handle 2", "Coordinates of the second handle");

	/* Number values */
	prop= RNA_def_property(srna, "tilt", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "alfa");
	/*RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);*/
	RNA_def_property_ui_text(prop, "Tilt", "Tilt in 3d View");

	prop= RNA_def_property(srna, "weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.01f, 100.0f);
	RNA_def_property_ui_text(prop, "Weight", "Softbody goal weight");

	prop= RNA_def_property(srna, "bevel_radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "radius");
	/*RNA_def_property_range(prop, 0.0f, 1.0f);*/
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Bevel Radius", "Radius for bevelling");
}

static void rna_def_path(BlenderRNA *brna, StructRNA *srna)
{
	PropertyRNA *prop;
	
	/* number values */
	prop= RNA_def_property(srna, "path_length", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pathlen");
	RNA_def_property_range(prop, 1, 32767);
	RNA_def_property_ui_text(prop, "Path Length", "If no speed IPO was set, the length of path in frames.");
	
	/* flags */
	prop= RNA_def_property(srna, "path", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_PATH);
	RNA_def_property_ui_text(prop, "Path", "Enable the curve to become a translation path.");
	
	prop= RNA_def_property(srna, "follow", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_FOLLOW);
	RNA_def_property_ui_text(prop, "Follow", "Make curve path children to rotate along the path.");
	
	prop= RNA_def_property(srna, "stretch", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_STRETCH);
	RNA_def_property_ui_text(prop, "Stretch", "Option for curve-deform: makes deformed child to stretch along entire path.");
	
	prop= RNA_def_property(srna, "offset_path_distance", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_OFFS_PATHDIST);
	RNA_def_property_ui_text(prop, "Offset Path Distance", "Children will use TimeOffs value as path distance offset.");
}

static void rna_def_nurbs(BlenderRNA *brna, StructRNA *srna)
{
	PropertyRNA *prop;
	
	/* flags */
	prop= RNA_def_property(srna, "uv_orco", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_UV_ORCO);
	RNA_def_property_ui_text(prop, "UV Orco", "Forces to use UV coordinates for texture mapping 'orco'.");
	
	prop= RNA_def_property(srna, "vertex_normal_flip", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", CU_NOPUNOFLIP);
	RNA_def_property_ui_text(prop, "Vertex Normal Flip", "Flip vertex normals towards the camera during render");
}

static void rna_def_font(BlenderRNA *brna, StructRNA *srna)
{
	PropertyRNA *prop;
	
	/* number values */
	prop= RNA_def_property(srna, "text_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fsize");
	RNA_def_property_range(prop, 0.1f, 10.0f);
	RNA_def_property_ui_text(prop, "Font size", "");
	
	prop= RNA_def_property(srna, "line_dist", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "linedist");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Distance between lines of text", "");
	
	prop= RNA_def_property(srna, "word_spacing", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "wordspace");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Spacing between words", "");
	
	prop= RNA_def_property(srna, "spacing", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "spacing");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Global spacing between characters", "");
	
	prop= RNA_def_property(srna, "shear", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "shear");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Shear", "Italic angle of the characters");
	
	prop= RNA_def_property(srna, "x_offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "xof");
	RNA_def_property_range(prop, -50.0f, 50.0f);
	RNA_def_property_ui_text(prop, "X Offset", "Horizontal offset from the object center");
	
	prop= RNA_def_property(srna, "y_offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "yof");
	RNA_def_property_range(prop, -50.0f, 50.0f);
	RNA_def_property_ui_text(prop, "Y Offset", "Vertical offset from the object center");
	
	prop= RNA_def_property(srna, "ul_position", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ulpos");
	RNA_def_property_range(prop, -0.2f, 0.8f);
	RNA_def_property_ui_text(prop, "Underline position", "Vertical position of underline");
	
	prop= RNA_def_property(srna, "ul_height", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ulheight");
	RNA_def_property_range(prop, -0.2f, 0.8f);
	RNA_def_property_ui_text(prop, "Underline thickness", "");
	
	prop= RNA_def_property(srna, "active_textbox", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "actbox");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "The active text box", "");
	
	/* strings */
	prop= RNA_def_property(srna, "family", PROP_STRING, PROP_NONE);
	RNA_def_property_string_maxlength(prop, 21);
	RNA_def_property_ui_text(prop, "Family", "Blender uses font from selfmade objects.");
	
	prop= RNA_def_property(srna, "str", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "str");
	RNA_def_property_ui_text(prop, "String", "");
	RNA_def_property_string_funcs(prop, "rna_ID_name_get", "rna_ID_name_length", "rna_ID_name_set");
	RNA_def_property_string_maxlength(prop, 8192); /* note that originally str did not have a limit! */
	RNA_def_struct_name_property(srna, prop);
	
	/* pointers */
	prop= RNA_def_property(srna, "text_on_curve", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "textoncurve");
	RNA_def_property_ui_text(prop, "Text on Curve", "Curve deforming text object.");
	
	prop= RNA_def_property(srna, "font", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "vfont");
	RNA_def_property_ui_text(prop, "Font", "");
	
	prop= RNA_def_property(srna, "textbox", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tb");
	RNA_def_property_ui_text(prop, "Textbox", "");
	
	prop= RNA_def_property(srna, "edit_format", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "curinfo");
	RNA_def_property_ui_text(prop, "Edit Format", "Editing settings character formatting.");
	
	/* flags */
	prop= RNA_def_property(srna, "fast", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_FAST);
	RNA_def_property_ui_text(prop, "Fast", "Don't fill polygons while editing.");
	
	prop= RNA_def_property(srna, "left_align", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "spacemode", CU_LEFT);
	RNA_def_property_ui_text(prop, "Left Align", "Left align the text from the object center.");
	
	prop= RNA_def_property(srna, "middle_align", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "spacemode", CU_MIDDLE);
	RNA_def_property_ui_text(prop, "Middle Align", "Middle align the text from the object center.");
	
	prop= RNA_def_property(srna, "right_align", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "spacemode", CU_RIGHT);
	RNA_def_property_ui_text(prop, "Right Align", "Right align the text from the object center.");
	
	prop= RNA_def_property(srna, "justify", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "spacemode", CU_JUSTIFY);
	RNA_def_property_ui_text(prop, "Justify", "Fill complete lines to maximum textframe width.");
	
	prop= RNA_def_property(srna, "flush", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "spacemode", CU_FLUSH);
	RNA_def_property_ui_text(prop, "Left Align", "Fill every line to maximum textframe width distributing space among all characters.");
}

void rna_def_textbox(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "TextBox", NULL);
	RNA_def_struct_ui_text(srna, "Text Box", "Text bounding box for layout.");
	
	/* number values */
	prop= RNA_def_property(srna, "x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "x");
	RNA_def_property_range(prop, -50.0f, 50.0f);
	RNA_def_property_ui_text(prop, "Textbox X Offset", "");
	
	prop= RNA_def_property(srna, "y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "y");
	RNA_def_property_range(prop, -50.0f, 50.0f);
	RNA_def_property_ui_text(prop, "Textbox Y Offset", "");

	prop= RNA_def_property(srna, "width", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "w");
	RNA_def_property_range(prop, 0.0f, 50.0f);
	RNA_def_property_ui_text(prop, "Textbox Width", "");

	prop= RNA_def_property(srna, "height", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "h");
	RNA_def_property_range(prop, 0.0f, 50.0f);
	RNA_def_property_ui_text(prop, "Textbox Height", "");
}

void rna_def_charinfo(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "TextCharacterFormat", NULL);
	RNA_def_struct_sdna(srna, "CharInfo");
	RNA_def_struct_ui_text(srna, "Text Character Format", "Text character formatting settings.");
	
	/* flags */
	prop= RNA_def_property(srna, "style", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_STYLE);
	RNA_def_property_ui_text(prop, "Style", "");
	
	prop= RNA_def_property(srna, "bold", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_BOLD);
	RNA_def_property_ui_text(prop, "Bold", "");
	
	prop= RNA_def_property(srna, "italic", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_ITALIC);
	RNA_def_property_ui_text(prop, "Italic", "");
	
	prop= RNA_def_property(srna, "underline", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_UNDERLINE);
	RNA_def_property_ui_text(prop, "Underline", "");
	
	prop= RNA_def_property(srna, "wrap", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_WRAP);
	RNA_def_property_ui_text(prop, "Wrap", "");
}

void rna_def_curve(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "Curve", "ID");
	RNA_def_struct_ui_text(srna, "Curve", "Curve datablock storing curves, splines and NURBS.");
	
	rna_def_animdata_common(srna);
	rna_def_texmat_common(srna, "rna_Curve_texspace_editable");

	prop= RNA_def_property(srna, "shape_keys", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "key");
	RNA_def_property_ui_text(prop, "Shape Keys", "");
	
	rna_def_path(brna, srna);
	rna_def_nurbs(brna, srna);
	rna_def_font(brna, srna);
	
	/* Number values */
	prop= RNA_def_property(srna, "bevel_resolution", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "bevresol");
	RNA_def_property_range(prop, 0, 32);
	RNA_def_property_ui_text(prop, "Bevel Resolution", "Bevel resolution when depth is non-zero and no specific bevel object has been defined.");
	
	prop= RNA_def_property(srna, "width", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "width");
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Width", "Scale the original width (1.0) based on given factor.");
	
	prop= RNA_def_property(srna, "extrude", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ext1");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Extrude", "Amount of curve extrusion when not using a bevel object.");
	
	prop= RNA_def_property(srna, "bevel_depth", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ext2");
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Bevel Depth", "Bevel depth when not using a bevel object.");
	
	prop= RNA_def_property(srna, "resolution_u", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "resolu");
	RNA_def_property_range(prop, 1, 1024);
	RNA_def_property_ui_text(prop, "Resolution U", "Surface resolution in U direction.");
	
	prop= RNA_def_property(srna, "resolution_v", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "resolv");
	RNA_def_property_range(prop, 1, 1024);
	RNA_def_property_ui_text(prop, "Resolution V", "Surface resolution in V direction.");
	
	prop= RNA_def_property(srna, "render_resolution_u", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "resolu_ren");
	RNA_def_property_range(prop, 0, 1024);
	RNA_def_property_ui_text(prop, "Render Resolution U", "Surface resolution in U direction used while rendering. Zero skips this property.");
	
	prop= RNA_def_property(srna, "render_resolution_v", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "resolv_ren");
	RNA_def_property_range(prop, 0, 1024);
	RNA_def_property_ui_text(prop, "Render Resolution V", "Surface resolution in V direction used while rendering. Zero skips this property.");
	
	/* pointers */
	prop= RNA_def_property(srna, "bevel_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "bevobj");
	RNA_def_property_ui_text(prop, "Bevel Object", "Curve object name that defines the bevel shape.");
	
	prop= RNA_def_property(srna, "taper_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "taperobj");
	RNA_def_property_ui_text(prop, "Taper Object", "Curve object name that defines the taper (width).");
	
	/* Flags */
	prop= RNA_def_property(srna, "curve_2d", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", CU_3D);
	RNA_def_property_ui_text(prop, "2D Curve", "Define curve in two dimensions only. Note that fill only works when this is enabled.");
	
	prop= RNA_def_property(srna, "front", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_FRONT);
	RNA_def_property_ui_text(prop, "Front", "Draw filled front for extruded/beveled curves.");
	
	prop= RNA_def_property(srna, "back", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_BACK);
	RNA_def_property_ui_text(prop, "Back", "Draw filled back for extruded/beveled curves.");
	
	prop= RNA_def_property(srna, "retopo", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_RETOPO);
	RNA_def_property_ui_text(prop, "Retopo", "Turn on the re-topology tool.");
}

void RNA_def_curve(BlenderRNA *brna)
{
	rna_def_curve(brna);
	rna_def_textbox(brna);
	rna_def_charinfo(brna);
	rna_def_bpoint(brna);
	rna_def_beztriple(brna);
}

#endif

