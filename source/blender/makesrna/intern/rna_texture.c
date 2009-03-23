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

#include <float.h>
#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_material_types.h"
#include "DNA_texture_types.h"

#ifdef RNA_RUNTIME

StructRNA *rna_Texture_refine(struct PointerRNA *ptr)
{
	Tex *tex= (Tex*)ptr->data;

	switch(tex->type) {
		/*case TEX_CLOUDS:
			return &RNA_CloudsTexture;
		case TEX_WOOD:
			return &RNA_WoodTexture;
		case TEX_MARBLE:
			return &RNA_MarbleTexture;
		case TEX_MAGIC:
			return &RNA_MagicTexture;
		case TEX_BLEND:
			return &RNA_BlendTexture;
		case TEX_STUCCI:
			return &RNA_StucciTexture;
		case TEX_NOISE:
			return &RNA_NoiseTexture;
		case TEX_IMAGE:
			return &RNA_ImageTexture;
		case TEX_PLUGIN:
			return &RNA_PluginTexture;
		case TEX_ENVMAP:
			return &RNA_EnvironmentMapTexture;
		case TEX_MUSGRAVE:
			return &RNA_MusgraveTexture;
		case TEX_VORONOI:
			return &RNA_VoronoiTexture;
		case TEX_DISTNOISE:
			return &RNA_DistortedNoiseTexture; */
		default:
			return &RNA_Texture;
	}
}

#else

static void rna_def_color_ramp_element(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ColorRampElement", NULL);
	RNA_def_struct_sdna(srna, "CBData");
	RNA_def_struct_ui_text(srna, "Color Ramp Element", "Element defining a color at a position in the color ramp.");

	prop= RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "r");
	RNA_def_property_array(prop, 4);
	RNA_def_property_ui_text(prop, "Color", "");

	prop= RNA_def_property(srna, "position", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "pos");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Position", "");
}

static void rna_def_color_ramp(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_interpolation_items[] = {
		{1, "EASE", "Ease", ""},
		{3, "CARDINAL", "Cardinal", ""},
		{0, "LINEAR", "Linear", ""},
		{2, "B_SPLINE", "B-Spline", ""},
		{4, "CONSTANT", "Constant", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "ColorRamp", NULL);
	RNA_def_struct_sdna(srna, "ColorBand");
	RNA_def_struct_ui_text(srna, "Color Ramp", "Color ramp mapping a scalar value to a color.");

	prop= RNA_def_property(srna, "elements", PROP_COLLECTION, PROP_COLOR);
	RNA_def_property_collection_sdna(prop, NULL, "data", "tot");
	RNA_def_property_struct_type(prop, "ColorRampElement");
	RNA_def_property_ui_text(prop, "Elements", "");

	prop= RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "ipotype");
	RNA_def_property_enum_items(prop, prop_interpolation_items);
	RNA_def_property_ui_text(prop, "Interpolation", "");
}

static void rna_def_mtex(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_blend_type_items[] = {
		{MTEX_BLEND, "MIX", "Mix", ""},
		{MTEX_ADD, "ADD", "Add", ""},
		{MTEX_SUB, "SUBTRACT", "Subtract", ""},
		{MTEX_MUL, "MULTIPLY", "Multiply", ""},
		{MTEX_SCREEN, "SCREEN", "Screen", ""},
		{MTEX_OVERLAY, "OVERLAY", "Overlay", ""},
		{MTEX_DIFF, "DIFFERENCE", "Difference", ""},
		{MTEX_DIV, "DIVIDE", "Divide", ""},
		{MTEX_DARK, "DARKEN", "Darken", ""},
		{MTEX_LIGHT, "LIGHTEN", "Lighten", ""},
		{MTEX_BLEND_HUE, "HUE", "Hue", ""},
		{MTEX_BLEND_SAT, "SATURATION", "Saturation", ""},
		{MTEX_BLEND_VAL, "VALUE", "Value", ""},
		{MTEX_BLEND_COLOR, "COLOR", "Color", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "TextureSlot", NULL);
	RNA_def_struct_sdna(srna, "MTex");
	RNA_def_struct_ui_text(srna, "Texture Slot", "Texture slot defining the mapping and influence of a texture.");

	prop= RNA_def_property(srna, "texture", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tex");
	RNA_def_property_struct_type(prop, "Texture");
	RNA_def_property_ui_text(prop, "Texture", "Texture datablock used by this texture slot.");

	/* mapping */
	prop= RNA_def_property(srna, "offset", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "ofs");
	RNA_def_property_ui_range(prop, -10, 10, 10, 2);
	RNA_def_property_ui_text(prop, "Offset", "Fine tunes texture mapping X, Y and Z locations.");

	prop= RNA_def_property(srna, "size", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_ui_range(prop, -100, 100, 10, 2);
	RNA_def_property_ui_text(prop, "Size", "Sets scaling for the texture's X, Y and Z sizes.");

	prop= RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "r");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Color", "The default color for textures that don't return RGB.");

	prop= RNA_def_property(srna, "blend_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "blendtype");
	RNA_def_property_enum_items(prop, prop_blend_type_items);
	RNA_def_property_ui_text(prop, "Blend Type", "");

	prop= RNA_def_property(srna, "stencil", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "texflag", MTEX_STENCIL);
	RNA_def_property_ui_text(prop, "Stencil", "Use this texture as a blending value on the next texture.");

	prop= RNA_def_property(srna, "negate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "texflag", MTEX_NEGATIVE);
	RNA_def_property_ui_text(prop, "Negate", "Inverts the values of the texture to reverse its effect.");

	prop= RNA_def_property(srna, "no_rgb", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "texflag", MTEX_RGBTOINT);
	RNA_def_property_ui_text(prop, "No RGB", "Converts texture RGB values to intensity (gray) values.");

	prop= RNA_def_property(srna, "default_value", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "def_var");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Default Value", "Value to use for Ref, Spec, Amb, Emit, Alpha, RayMir, TransLu and Hard.");

	prop= RNA_def_property(srna, "variable_factor", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "varfac");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Variable Factor", "Amount texture affects other values.");

	prop= RNA_def_property(srna, "color_factor", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "colfac");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Color Factor", "Amount texture affects color values.");

	prop= RNA_def_property(srna, "normal_factor", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "norfac");
	RNA_def_property_range(prop, 0, 25);
	RNA_def_property_ui_text(prop, "Normal Factor", "Amount texture affects normal values.");
}

static void rna_def_environment_map(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_source_items[] = {
		{ENV_STATIC, "STATIC", "Static", ""},
		{ENV_ANIM, "ANIMATED", "Animated", ""},
		{ENV_LOAD, "LOAD", "Load", ""},
		{0, NULL, NULL, NULL}};

	static EnumPropertyItem prop_type_items[] = {
		{ENV_CUBE, "CUBE", "Cube", "Use environment map with six cube sides."},
		{ENV_PLANE, "PLANE", "Plane", "Only one side is rendered, with Z axis pointing in direction of image."},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "EnvironmentMap", NULL);
	RNA_def_struct_sdna(srna, "EnvMap");
	RNA_def_struct_ui_text(srna, "EnvironmentMap", "Environment map created by the renderer and cached for subsequent renders.");
	
	prop= RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ima");
	RNA_def_property_struct_type(prop, "Image");
	RNA_def_property_ui_text(prop, "Image", "");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Type", "");

	prop= RNA_def_property(srna, "source", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "stype");
	RNA_def_property_enum_items(prop, prop_source_items);
	RNA_def_property_ui_text(prop, "Source", "");

	prop= RNA_def_property(srna, "clip_start", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clipsta");
	RNA_def_property_range(prop, 0.01, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.01, 50, 100, 2);
	RNA_def_property_ui_text(prop, "Clip Start", "Objects nearer than this are not visible to map.");

	prop= RNA_def_property(srna, "clip_end", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clipend");
	RNA_def_property_range(prop, 0.01, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.01, 50, 100, 2);
	RNA_def_property_ui_text(prop, "Clip Start", "Objects further than this are not visible to map.");

	prop= RNA_def_property(srna, "zoom", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "viewscale");
	RNA_def_property_range(prop, 0.01, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.5, 5, 100, 2);
	RNA_def_property_ui_text(prop, "Zoom", "");

	/* XXX: EnvMap.notlay */
	
	prop= RNA_def_property(srna, "resolution", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "cuberes");
	RNA_def_property_range(prop, 50, 4096);
	RNA_def_property_ui_text(prop, "Resolution", "Pixel resolution of the rendered environment map.");

	prop= RNA_def_property(srna, "depth", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 0, 5);
	RNA_def_property_ui_text(prop, "Depth", "Number of times a map will be rendered recursively (mirror effects.)");
}

static void rna_def_texture(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_type_items[] = {
		{0, "NONE", "None", ""},
		{TEX_CLOUDS, "CLOUDS", "Clouds", ""},
		{TEX_WOOD, "WOOD", "Wood", ""},
		{TEX_MARBLE, "MARBLE", "Marble", ""},
		{TEX_MAGIC, "MAGIC", "Magic", ""},
		{TEX_BLEND, "BLEND", "Blend", ""},
		{TEX_STUCCI, "STUCCI", "Stucci", ""},
		{TEX_NOISE, "NOISE", "Noise", ""},
		{TEX_IMAGE, "IMAGE", "Image", ""},
		{TEX_PLUGIN, "PLUGIN", "Plugin", ""},
		{TEX_ENVMAP, "ENVIRONMENT_MAP", "Environment Map", ""},
		{TEX_MUSGRAVE, "MUSGRAVE", "Musgrave", ""},
		{TEX_VORONOI, "VORONOI", "Voronoi", ""},
		{TEX_DISTNOISE, "DISTORTED_NOISE", "Distorted Noise", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "Texture", "ID");
	RNA_def_struct_sdna(srna, "Tex");
	RNA_def_struct_ui_text(srna, "Texture", "Texture datablock used by materials, lamps, worlds and brushes.");
	RNA_def_struct_refine_func(srna, "rna_Texture_refine");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Type", "");

	prop= RNA_def_property(srna, "color_ramp", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "coba");
	RNA_def_property_struct_type(prop, "ColorRamp");
	RNA_def_property_ui_text(prop, "Color Ramp", "");

	prop= RNA_def_property(srna, "brightness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "bright");
	RNA_def_property_range(prop, 0, 2);
	RNA_def_property_ui_text(prop, "Brightness", "");

	prop= RNA_def_property(srna, "contrast", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.01, 5);
	RNA_def_property_ui_text(prop, "Contrast", "");

	/* XXX: would be nicer to have this as a color selector?
	   but the values can go past [0,1]. */
	prop= RNA_def_property(srna, "rgb_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rfac");
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, 0, 2);
	RNA_def_property_ui_text(prop, "RGB Factor", "");

	rna_def_animdata_common(srna);

	/* specific types */
	/* XXX add more types here .. */

	/* ********** XXX these should be moved to the specific types *****************/

#if 0
	static EnumPropertyItem prop_distance_metric_items[] = {
		{TEX_DISTANCE, "DISTANCE", "Actual Distance", ""},
		{TEX_DISTANCE_SQUARED, "DISTANCE_SQUARED", "Distance Squared", ""},
		{TEX_MANHATTAN, "MANHATTAN", "Manhattan", ""},
		{TEX_CHEBYCHEV, "CHEBYCHEV", "Chebychev", ""},
		{TEX_MINKOVSKY_HALF, "MINKOVSKY_HALF", "Minkovsky 1/2", ""},
		{TEX_MINKOVSKY_FOUR, "MINKOVSKY_FOUR", "Minkovsky 4", ""},
		{TEX_MINKOVSKY, "MINKOVSKY", "Minkovsky", ""},
		{0, NULL, NULL, NULL}};

	static EnumPropertyItem prop_color_type_items[] = {
		/* XXX: OK names / descriptions? */
		{TEX_INTENSITY, "INTENSITY", "Intensity", "Only calculate intensity."},
		{TEX_COL1, "POSITION", "Position", "Color cells by position."},
		{TEX_COL2, "POSITION_OUTLINE", "Position and Outline", "Use position plus an outline based on F2-F.1"},
		{TEX_COL3, "POSITION_OUTLINE_INTENSITY", "Position, Outline, and Intensity", "Multiply position and outline by intensity."},
		{0, NULL, NULL, NULL}};


	prop= RNA_def_property(srna, "turbulence", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "turbul");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 200, 10, 2);
	RNA_def_property_ui_text(prop, "Turbulence", "");

	/* XXX: tex->filtersize */

	/* Musgrave */
	prop= RNA_def_property(srna, "highest_dimension", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mg_H");
	RNA_def_property_range(prop, 0.0001, 2);
	RNA_def_property_ui_text(prop, "Highest Dimension", "Highest fractal dimension for musgrave.");

	prop= RNA_def_property(srna, "lacunarity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mg_lacunarity");
	RNA_def_property_range(prop, 0, 6);
	RNA_def_property_ui_text(prop, "Lacunarity", "Gap between succesive frequencies for musgrave.");

	prop= RNA_def_property(srna, "octaves", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mg_octaves");
	RNA_def_property_range(prop, 0, 8);
	RNA_def_property_ui_text(prop, "Octaves", "Number of frequencies used for musgrave.");

	prop= RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mg_offset");
	RNA_def_property_range(prop, 0, 6);
	RNA_def_property_ui_text(prop, "Offset", "The fractal offset for musgrave.");

	prop= RNA_def_property(srna, "gain", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mg_gain");
	RNA_def_property_range(prop, 0, 6);
	RNA_def_property_ui_text(prop, "Gain", "The gain multiplier for musgrave.");

	/* Distorted Noise */
	prop= RNA_def_property(srna, "distortion_amount", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dist_amount");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Distortion Amount", "");

	/* Musgrave / Voronoi */
	prop= RNA_def_property(srna, "noise_intensity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ns_outscale");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Noise Intensity", "");

	/* Voronoi */
	prop= RNA_def_property(srna, "feature_weights", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "vn_w1");
	RNA_def_property_array(prop, 4);
	RNA_def_property_range(prop, -2, 2);
	RNA_def_property_ui_text(prop, "Feature Weights", "");

	prop= RNA_def_property(srna, "minkovsky_exponent", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "vn_mexp");
	RNA_def_property_range(prop, 0.01, 10);
	RNA_def_property_ui_text(prop, "Minkovsky Exponent", "");

	prop= RNA_def_property(srna, "distance_metric", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "vn_distm");
	RNA_def_property_enum_items(prop, prop_distance_metric_items);
	RNA_def_property_ui_text(prop, "Distance Metric", "");

	prop= RNA_def_property(srna, "color_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "vn_coltype");
	RNA_def_property_enum_items(prop, prop_color_type_items);
	RNA_def_property_ui_text(prop, "Color Type", "");


	/* XXX: noisebasis2 */
	/* XXX: imaflag */
	/* XXX: flag */
	/* XXX: stype */

	/* XXX: did this as an array, but needs better descriptions than "1 2 3 4"
	        perhaps a new subtype could be added? */
	prop= RNA_def_property(srna, "crop_rectangle", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cropxmin");
	RNA_def_property_array(prop, 4);
	RNA_def_property_range(prop, -10, 10);
	RNA_def_property_ui_text(prop, "Crop Rectangle", "");
	
	prop= RNA_def_property(srna, "checker_separation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "checkerdist");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Checker Separation", "");
	

	prop= RNA_def_property(srna, "normal_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "norfac");
	RNA_def_property_range(prop, 0, 25);
	RNA_def_property_ui_text(prop, "Normal Factor", "Amount the texture affects normal values.");
       
	prop= RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ima");
	RNA_def_property_struct_type(prop, "Image");
	RNA_def_property_ui_text(prop, "Image", "");

	/* XXX: plugin */

	prop= RNA_def_property(srna, "environment_map", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "env");
	RNA_def_property_struct_type(prop, "EnvironmentMap");
	RNA_def_property_ui_text(prop, "Environment Map", "");
#endif
}

void RNA_def_texture(BlenderRNA *brna)
{
	rna_def_texture(brna);
	rna_def_mtex(brna);
	rna_def_environment_map(brna);
	rna_def_color_ramp(brna);
	rna_def_color_ramp_element(brna);
}

#endif
