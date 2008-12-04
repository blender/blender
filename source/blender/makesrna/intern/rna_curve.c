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

static int rna_Curve_texspace_editable(PointerRNA *ptr)
{
	Curve *cu= (Curve*)ptr->data;
	return (cu->texflag & CU_AUTOSPACE)? PROP_NOT_EDITABLE: 0;
}

#else

static void rna_def_path(BlenderRNA *brna, StructRNA *srna);
static void rna_def_nurbs(BlenderRNA *brna, StructRNA *srna);
static void rna_def_font(BlenderRNA *brna, StructRNA *srna);

void rna_def_curve(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "Curve", "ID", "Curve");

	rna_def_ipo_common(srna);
	rna_def_texmat_common(srna, "rna_Curve_texspace_editable");

	prop= RNA_def_property(srna, "key", PROP_POINTER, PROP_NONE);
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
	RNA_def_property_ui_text(prop, "U Resolution", "Surface resolution in U direction.");
	
	prop= RNA_def_property(srna, "resolution_v", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "resolv");
	RNA_def_property_range(prop, 1, 1024);
	RNA_def_property_ui_text(prop, "V Resolution", "Surface resolution in V direction.");
	
	prop= RNA_def_property(srna, "resolution_u_rendering", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "resolu_ren");
	RNA_def_property_range(prop, 0, 1024);
	RNA_def_property_ui_text(prop, "U Resolution (Rendering)", "Surface resolution in U direction used while rendering. Zero skips this property.");
	
	prop= RNA_def_property(srna, "resolution_v_rendering", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "resolv_ren");
	RNA_def_property_range(prop, 0, 1024);
	RNA_def_property_ui_text(prop, "V Resolution (Rendering)", "Surface resolution in V direction used while rendering. Zero skips this property.");
	
	/* pointers */
	prop= RNA_def_property(srna, "bevel_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "bevobj");
	RNA_def_property_ui_text(prop, "Bevel Object", "Curve object name that defines the bevel shape.");
	
	prop= RNA_def_property(srna, "taper_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "taperobj");
	RNA_def_property_ui_text(prop, "Taper Object", "Curve object name that defines the taper (width).");
	
	/* Flags */
	prop= RNA_def_property(srna, "3d", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_3D);
	RNA_def_property_ui_text(prop, "3D Curve", "Define curve in three dimensions. Note that in this case fill won't work.");
	
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
	RNA_def_property_ui_text(prop, "Text Size", "");
	
	prop= RNA_def_property(srna, "line_dist", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "linedist");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Distance Between Lines of Text", "");
	
	prop= RNA_def_property(srna, "word_spacing", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "wordspace");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Word Spacing", "");
	
	prop= RNA_def_property(srna, "spacing", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "spacing");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Character Spacing", "");
	
	prop= RNA_def_property(srna, "shear", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "shear");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Shear", "Italic angle of the characters.");
	
	prop= RNA_def_property(srna, "x_offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "xof");
	RNA_def_property_range(prop, -50.0f, 50.0f);
	RNA_def_property_ui_text(prop, "X Offset", "Horizontal offset from the object center.");
	
	prop= RNA_def_property(srna, "y_offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "yof");
	RNA_def_property_range(prop, -50.0f, 50.0f);
	RNA_def_property_ui_text(prop, "Y Offset", "Vertical offset from the object center.");
	
	prop= RNA_def_property(srna, "ul_position", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ulpos");
	RNA_def_property_range(prop, -0.2f, 0.8f);
	RNA_def_property_ui_text(prop, "Underline Position", "Vertical position of underline.");
	
	prop= RNA_def_property(srna, "ul_height", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ulheight");
	RNA_def_property_range(prop, -0.2f, 0.8f);
	RNA_def_property_ui_text(prop, "Underline Thickness", "");
	
	prop= RNA_def_property(srna, "active_textbox", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "actbox");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Active Textbox", "");
	
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
	
	/*
	TODO: struct CharInfo curinfo;
	
	Obviously a pointer won't work in this case.
	*/
	/*
	prop= RNA_def_property(srna, "curinfo", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "curinfo");
	RNA_def_property_ui_text(prop, "curinfo", "");
	*/
	
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
	
	srna= RNA_def_struct(brna, "TextBox", NULL, "TextBox");
	
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
	
	srna= RNA_def_struct(brna, "CharInfo", NULL, "CharInfo");
	
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

void RNA_def_curve(BlenderRNA *brna)
{
	rna_def_curve(brna);
	rna_def_textbox(brna);
	rna_def_charinfo(brna);
}

#endif
