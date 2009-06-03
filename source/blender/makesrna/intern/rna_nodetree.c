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
 * Contributor(s): Blender Foundation (2008), Nathan Letwory, Robin Allen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "BKE_node.h"

#ifdef RNA_RUNTIME

StructRNA *rna_Node_refine(struct PointerRNA *ptr)
{
	bNode *node = (bNode*)ptr->data;

	switch(node->type) {
		
		#define DefNode(Category, ID, DefFunc, EnumName, StructName, UIName, UIDesc) \
			case ID: return &RNA_##Category##StructName;
				
		#include "rna_nodetree_types.h"
		
		#undef DefNode
		
		default:
			return &RNA_Node;
	}
}

#else

#define MaxNodes 1000

enum
{
	Category_NoCategory,
	Category_ShaderNode,
	Category_CompositorNode,
	Category_TextureNode
};

typedef struct NodeInfo
{
	int defined;
	int category;
	const char *enum_name;
	const char *struct_name;
	const char *base_name;
	const char *ui_name;
	const char *ui_desc;
} NodeInfo;

static NodeInfo nodes[MaxNodes];

static void reg_node(
	int ID, 
	int category,
	const char *enum_name,
	const char *struct_name,
	const char *base_name,
	const char *ui_name,
	const char *ui_desc
){
	NodeInfo *ni = nodes + ID;
	
	ni->defined = 1;
	ni->category = category;
	ni->enum_name = enum_name;
	ni->struct_name = struct_name;
	ni->base_name = base_name;
	ni->ui_name = ui_name;
	ni->ui_desc = ui_desc;
}

static void init(void)
{
	memset(nodes, 0, sizeof nodes);
	
	#define Str(x) #x
	
	#define DefNode(Category, ID, DefFunc, EnumName, StructName, UIName, UIDesc) \
		reg_node(ID, Category_##Category, EnumName, Str(Category##StructName), #Category, UIName, UIDesc);
		
	#include "rna_nodetree_types.h"
	
	#undef DefNode
	#undef Str
}

static StructRNA* def_node(BlenderRNA *brna, int node_id)
{
	StructRNA *srna;
	NodeInfo *node = nodes + node_id;
	
	srna = RNA_def_struct(brna, node->struct_name, node->base_name);
	RNA_def_struct_ui_text(srna, node->ui_name, node->ui_desc);
	RNA_def_struct_sdna(srna, "bNode");
	
	return srna;
}

static EnumPropertyItem* alloc_node_type_items(int category)
{
	int i;
	int count = 2;
	EnumPropertyItem *item, *items;
	
	for(i=0; i<MaxNodes; i++)
		if(nodes[i].defined && nodes[i].category == category)
			count++;
		
	item = items = malloc(count * sizeof(EnumPropertyItem));
	
	for(i=0; i<MaxNodes; i++) {
		NodeInfo *node = nodes + i;
		if(node->defined && node->category == category) {
			item->value = i;
			item->identifier = node->enum_name;
			item->name = node->ui_name;
			item->description = node->ui_desc;
		
			item++;
		}
	}
	
	item->value = NODE_DYNAMIC;
	item->identifier = "SCRIPT";
	item->name = "Script";
	item->description = "";
	
	item++;
	
	memset(item, 0, sizeof(EnumPropertyItem));
	
	return items;
}


/* -- Common nodes ---------------------------------------------------------- */

static void def_math(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem items[] ={
		{ 0, "ADD",          "Add",          ""},
		{ 1, "SUBTRACT",     "Subtract",     ""},
		{ 2, "MULTIPLY",     "Multiply",     ""},
		{ 3, "DIVIDE",       "Divide",       ""},
		{ 4, "SINE",         "Sine",         ""},
		{ 5, "COSINE",       "Cosine",       ""},
		{ 6, "TANGENT",      "Tangent",      ""},
		{ 7, "ARCSINE",      "Arcsine",      ""},
		{ 8, "ARCCOSINE",    "Arccosine",    ""},
		{ 9, "ARCTANGENT",   "Arctangent",   ""},
		{10, "POWER",        "Power",        ""},
		{11, "LOGARITHM",    "Logarithm",    ""},
		{12, "MINIMUM",      "Minimum",      ""},
		{13, "MAXIMUM",      "Maximum",      ""},
		{14, "ROUND",        "Round",        ""},
		{15, "LESS_THAN",    "Less Than",    ""},
		{16, "GREATER_THAN", "Greater Than", ""},
		
		{0, NULL, NULL, NULL}
	};
	
	srna = def_node(brna, id);
	
	prop = RNA_def_property(srna, "operation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, items);
	RNA_def_property_ui_text(prop, "Operation", "");
}

static void def_vector_math(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem items[] ={
		{0, "ADD",           "Add",           ""},
		{1, "SUBTRACT",      "Subtract",      ""},
		{2, "AVERAGE",       "Average",       ""},
		{3, "DOT_PRODUCT",   "Dot Product",   ""},
		{4, "CROSS_PRODUCT", "Cross Product", ""},
		{5, "NORMALIZE",     "Normalize",     ""},
		
		{0, NULL, NULL, NULL}
	};
	
	srna = def_node(brna, id);
	
	prop = RNA_def_property(srna, "operation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, items);
	RNA_def_property_ui_text(prop, "Operation", "");
}

static void def_rgb_curve(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
	prop = RNA_def_property(srna, "mapping", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "storage");
	RNA_def_property_struct_type(prop, "CurveMapping");
	RNA_def_property_ui_text(prop, "Mapping", "");
}

static void def_vector_curve(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
	prop = RNA_def_property(srna, "mapping", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "storage");
	RNA_def_property_struct_type(prop, "CurveMapping");
	RNA_def_property_ui_text(prop, "Mapping", "");
}

static void def_time(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
	prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "storage");
	RNA_def_property_struct_type(prop, "CurveMapping");
	RNA_def_property_ui_text(prop, "Curve", "");
	
	prop = RNA_def_property(srna, "start", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom1");
	RNA_def_property_ui_text(prop, "Start Frame", "");
	
	prop = RNA_def_property(srna, "end", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom2");
	RNA_def_property_ui_text(prop, "End Frame", "");
}

static void def_val_to_rgb(BlenderRNA *brna, int id)
{
	StructRNA *srna;
//	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
	/* TODO: uncomment when ColorBand is wrapped */
	/*prop = RNA_def_property(srna, "color_band", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "storage");
	RNA_def_property_struct_type(prop, "ColorBand");
	RNA_def_property_ui_text(prop, "Color Band", "");*/
}

static void def_mix_rgb(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem blend_type_items[] ={
		{ 0, "MIX",        "Mix",         ""},
		{ 1, "ADD",        "Add",         ""},
		{ 3, "SUBTRACT",   "Subtract",    ""},
		{ 2, "MULTIPLY",   "Multiply",    ""},
		{ 4, "SCREEN",     "Screen",      ""},
		{ 9, "OVERLAY",    "Overlay",     ""},
		{ 5, "DIVIDE",     "Divide",      ""},
		{ 6, "DIFFERENCE", "Difference",  ""},
		{ 7, "DARKEN",     "Darken",      ""},
		{ 8, "LIGHTEN",    "Lighten",     ""},
		{10, "DODGE",      "Dodge",       ""},
		{11, "BURN",       "Burn",        ""},
		{15, "COLOR",      "Color",       ""},
		{14, "VALUE",      "Value",       ""},
		{13, "SATURATION", "Saturation",  ""},
		{12, "HUE",        "Hue",         ""},
		{0, NULL, NULL, NULL}
	};
	
	srna = def_node(brna, id);
	
	prop = RNA_def_property(srna, "blend_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, blend_type_items);
	RNA_def_property_ui_text(prop, "Blend Type", "");
	
	prop = RNA_def_property(srna, "alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom2", 1);
	RNA_def_property_ui_text(prop, "Diffuse", "Include alpha of second input in this operation");
}

static void def_texture(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
	prop = RNA_def_property(srna, "texture", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "Texture");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Texture", "");
	
	prop = RNA_def_property(srna, "node_output", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom1");
	RNA_def_property_ui_text(prop, "Node Output", "For node-based textures, which output node to use");
}


/* -- Shader Nodes ---------------------------------------------------------- */

static void def_sh_material(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = def_node(brna, id);

	prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "Material");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Material", "");

	prop = RNA_def_property(srna, "diffuse", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", SH_NODE_MAT_DIFF);
	RNA_def_property_ui_text(prop, "Diffuse", "Material Node outputs Diffuse");

	prop = RNA_def_property(srna, "specular", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", SH_NODE_MAT_SPEC);
	RNA_def_property_ui_text(prop, "Specular", "Material Node outputs Specular");
	
	prop = RNA_def_property(srna, "invert_normal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", SH_NODE_MAT_NEG);
	RNA_def_property_ui_text(prop, "Invert Normal", "Material Node uses inverted normal");
}

static void def_sh_mapping(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
	prop = RNA_def_property(srna, "mapping", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "storage");
	RNA_def_property_struct_type(prop, "TexMapping");
	RNA_def_property_ui_text(prop, "Mapping", "");
}

static void def_sh_geometry(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	RNA_def_struct_sdna_from(srna, "NodeGeometry", "storage");
	
	prop = RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "uvname");
	RNA_def_property_ui_text(prop, "UV Layer", "");
	
	prop = RNA_def_property(srna, "color_layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "colname");
	RNA_def_property_ui_text(prop, "Vertex Color Layer", "");
}


/* -- Compositor Nodes ------------------------------------------------------ */

static void def_cmp_alpha_over(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = def_node(brna, id);

	prop = RNA_def_property(srna, "convert_premul", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", 1);
	RNA_def_property_ui_text(prop, "convert_premul", "TODO: don't know what this is");
	
	/* TODO: uses NodeTwoFloats storage */
}

static void def_cmp_blur(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem filter_type_items[] ={
		{R_FILTER_BOX,        "FLAT",       "Flat",          ""},
		{R_FILTER_TENT,       "TENT",       "Tent",          ""},
		{R_FILTER_QUAD,       "QUAD",       "Quadratic",     ""},
		{R_FILTER_CUBIC,      "CUBIC",      "Cubic",         ""},
		{R_FILTER_GAUSS,      "GAUSS",      "Gaussian",      ""},
		{R_FILTER_FAST_GAUSS, "FAST_GAUSS", "Fast Gaussian", ""},
		{R_FILTER_CATROM,     "CATROM",     "Catrom",        ""},
		{R_FILTER_MITCH,      "MITCH",      "Mitch",         ""},
		{0, NULL, NULL, NULL}
	};

	srna = def_node(brna, id);
	RNA_def_struct_sdna_from(srna, "NodeBlurData", "storage");
	
	prop = RNA_def_property(srna, "sizex", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "sizex");
	RNA_def_property_ui_text(prop, "Size X", "");
	
	prop = RNA_def_property(srna, "sizey", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "sizey");
	RNA_def_property_ui_text(prop, "Size Y", "");
	
	prop = RNA_def_property(srna, "samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "samples");
	RNA_def_property_ui_text(prop, "Samples", "");
	
	prop = RNA_def_property(srna, "max_speed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "maxspeed");
	RNA_def_property_ui_text(prop, "Max Speed", "");
	
	prop = RNA_def_property(srna, "min_speed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "minspeed");
	RNA_def_property_ui_text(prop, "Min Speed", "");
	
	prop = RNA_def_property(srna, "relative", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "relative", 1);
	RNA_def_property_ui_text(prop, "Relative", "");
	
	prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fac");
	RNA_def_property_ui_text(prop, "Factor", "");
	
	prop = RNA_def_property(srna, "factor_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "percentx");
	RNA_def_property_ui_text(prop, "Relative Size X", "");
	
	prop = RNA_def_property(srna, "factor_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "percenty");
	RNA_def_property_ui_text(prop, "Relative Size Y", "");
	
	prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "filtertype");
	RNA_def_property_enum_items(prop, filter_type_items);
	RNA_def_property_ui_text(prop, "Filter Type", "");
	
	prop = RNA_def_property(srna, "bokeh", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "bokeh", 1);
	RNA_def_property_ui_text(prop, "Bokeh", "");
	
	prop = RNA_def_property(srna, "gamma", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gamma", 1);
	RNA_def_property_ui_text(prop, "Gamma", "");
	
	/*
		TODO:
			curved
			image_in_width
			image_in_height
			
		Don't know if these need wrapping
	*/
	
}

static void def_cmp_filter(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem type_items[] ={
		{0, "SOFTEN",  "Soften",  ""},
		{1, "SHARPEN", "Sharpen", ""},
		{2, "LAPLACE", "Laplace", ""},
		{3, "SOBEL",   "Sobel",   ""},
		{4, "PREWITT", "Prewitt", ""},
		{5, "KIRSCH",  "Kirsch",  ""},
		{6, "SHADOW",  "Shadow",  ""},
		{0, NULL, NULL, NULL}
	};
	
	srna = def_node(brna, id);
	
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, type_items);
	RNA_def_property_ui_text(prop, "Type", "");
}

static void def_cmp_map_value(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	RNA_def_struct_sdna_from(srna, "TexMapping", "storage");
	
	prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "loc");
	RNA_def_property_ui_text(prop, "Offset", "");
	
	prop = RNA_def_property(srna, "size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "size");
	RNA_def_property_ui_text(prop, "Size", "");
	
	prop = RNA_def_property(srna, "use_min", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEXMAP_CLIP_MIN);
	RNA_def_property_ui_text(prop, "Use Minimum", "");
	
	prop = RNA_def_property(srna, "use_max", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEXMAP_CLIP_MAX);
	RNA_def_property_ui_text(prop, "Use Maximum", "");
	
	prop = RNA_def_property(srna, "min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "min");
	RNA_def_property_ui_text(prop, "Minimum", "");
	
	prop = RNA_def_property(srna, "max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max");
	RNA_def_property_ui_text(prop, "Maximum", "");
}

static void def_cmp_vector_blur(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	RNA_def_struct_sdna_from(srna, "NodeBlurData", "storage");
	
	prop = RNA_def_property(srna, "samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "samples");
	RNA_def_property_ui_text(prop, "Samples", "");
	
	prop = RNA_def_property(srna, "min_speed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "minspeed");
	RNA_def_property_ui_text(prop, "Min Speed", "Minimum speed for a pixel to be blurred; used to separate background from foreground");
		
	prop = RNA_def_property(srna, "max_speed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "maxspeed");
	RNA_def_property_ui_text(prop, "Min Speed", "Maximum speed, or zero for none");
	
	prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fac");
	RNA_def_property_ui_text(prop, "Blur Factor", "Scaling factor for motion vectors; actually 'shutter speed' in frames");
	
	prop = RNA_def_property(srna, "curved", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "curved", 1);
	RNA_def_property_ui_text(prop, "Curved", "Interpolate between frames in a bezier curve, rather than linearly");
}

static void def_cmp_image(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	RNA_def_struct_sdna_from(srna, "ImageUser", "storage");
	
	/* TODO. uses storage and id. */
	
}

static void def_cmp_render_layers(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
	/* TODO. users customx and id. */
	
}

static void def_cmp_output_file(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
	/* TODO. */
	
}

static void def_cmp_texture(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
	/* TODO. */
	
}

static void def_cmp_dilate_erode(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
		
	prop = RNA_def_property(srna, "distance", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom2");
	RNA_def_property_ui_text(prop, "Distance", "Distance to grow/shrink (number of iterations)");
}

static void def_cmp_scale(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem space_items[] ={
		{0, "RELATIVE",   "Relative",   ""},
		{1, "ABSOLUTE",   "Absolute",   ""},
		{2, "SCENE_SIZE", "Scene Size", ""},
		{0, NULL, NULL, NULL}
	};
	
	srna = def_node(brna, id);
	
	prop = RNA_def_property(srna, "space", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, space_items);
	RNA_def_property_ui_text(prop, "Space", "Coordinate space to scale relative to");
}

static void def_cmp_diff_matte(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem color_space_items[] ={
		{1, "RGB", "RGB",   ""},
		{2, "HSV", "HSV",   ""},
		{3, "YUV", "YUV",   ""},
		{4, "YCC", "YCbCr", ""},
		{0, NULL, NULL, NULL}
	};
	
	srna = def_node(brna, id);
	
	prop = RNA_def_property(srna, "color_space", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, color_space_items);
	RNA_def_property_ui_text(prop, "Color Space", "");
	
	RNA_def_struct_sdna_from(srna, "NodeChroma", "storage");
	
	/* TODO: nicer wrapping for tolerances */
	
	prop = RNA_def_property(srna, "tolerance1", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t1");
	RNA_def_property_ui_text(prop, "Channel 1 Tolerance", "");
	
	prop = RNA_def_property(srna, "tolerance2", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t2");
	RNA_def_property_ui_text(prop, "Channel 2 Tolerance", "");
	
	prop = RNA_def_property(srna, "tolerance3", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t3");
	RNA_def_property_ui_text(prop, "Channel 3 Tolerance", "");
	
	
	prop = RNA_def_property(srna, "falloff", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fstrength");
	RNA_def_property_ui_text(prop, "Falloff", "");
}

static void def_cmp_color_spill(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem channel_items[] ={
		{1, "R", "Red",   ""},
		{2, "G", "Green", ""},
		{3, "B", "Blue",  ""},
		{0, NULL, NULL, NULL}
	};
	
	srna = def_node(brna, id);
	
	prop = RNA_def_property(srna, "channel", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, channel_items);
	RNA_def_property_ui_text(prop, "Channel", "");
	
	RNA_def_struct_sdna_from(srna, "NodeChroma", "storage");
	
	prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t1");
	RNA_def_property_ui_text(prop, "Amount", "How much the selected channel is affected by");
}

static void def_cmp_chroma(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
	RNA_def_struct_sdna_from(srna, "NodeChroma", "storage");
	
	prop = RNA_def_property(srna, "acceptance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t1");
	RNA_def_property_ui_text(prop, "Acceptance", "Tolerance for a color to be considered a keying color");
	
	prop = RNA_def_property(srna, "cutoff", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t2");
	RNA_def_property_ui_text(prop, "Cutoff", "Tolerance below which colors will be considered as exact matches");

	prop = RNA_def_property(srna, "lift", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fsize");
	RNA_def_property_ui_text(prop, "Lift", "Alpha lift");
	
	prop = RNA_def_property(srna, "gain", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fstrength");
	RNA_def_property_ui_text(prop, "Gain", "Alpha gain");
	
	prop = RNA_def_property(srna, "shadow_adjust", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t3");
	RNA_def_property_ui_text(prop, "Shadow Adjust", "Adjusts the brightness of any shadows captured");
	
	/* TODO: 
		if(c->t2 > c->t1)
			c->t2=c->t1;
	*/
}

static void def_cmp_channel_matte(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem color_space_items[] ={
		{1, "RGB", "RGB",   ""},
		{2, "HSV", "HSV",   ""},
		{3, "YUV", "YUV",   ""},
		{4, "YCC", "YCbCr", ""},
		{0, NULL, NULL, NULL}
	};
	
	srna = def_node(brna, id);
	
	prop = RNA_def_property(srna, "color_space", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, color_space_items);
	RNA_def_property_ui_text(prop, "Color Space", "");
	
	/* TODO: channel must be 1, 2 or 3 */
	prop = RNA_def_property(srna, "channel", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom2");
	RNA_def_property_ui_text(prop, "Channel", "");
	
	RNA_def_struct_sdna_from(srna, "NodeChroma", "storage");
	
	prop = RNA_def_property(srna, "high", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t1");
	RNA_def_property_ui_text(prop, "High", "Values higher than this setting are 100% opaque");
	
	prop = RNA_def_property(srna, "low", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t2");
	RNA_def_property_ui_text(prop, "Low", "Values lower than this setting are 100% keyed");
	
	/* TODO:
		if(c->t2 > c->t1)
			c->t2=c->t1;
	*/
}

static void def_cmp_flip(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem axis_items[] ={
		{0, "X",  "X",     ""},
		{1, "Y",  "Y",     ""},
		{2, "XY", "X & Y", ""},
		{0, NULL, NULL, NULL}
	};
	
	srna = def_node(brna, id);
	
	prop = RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, axis_items);
	RNA_def_property_ui_text(prop, "Axis", "");
}

static void def_cmp_splitviewer(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem axis_items[] ={
		{0, "X",  "X",     ""},
		{1, "Y",  "Y",     ""},
		{0, NULL, NULL, NULL}
	};
	
	srna = def_node(brna, id);
	
	prop = RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom2");
	RNA_def_property_enum_items(prop, axis_items);
	RNA_def_property_ui_text(prop, "Axis", "");
	
	/* TODO: percentage */
	prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "custom1");
	RNA_def_property_ui_text(prop, "Factor", "");
}

static void def_cmp_id_mask(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
	prop = RNA_def_property(srna, "index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom1");
	RNA_def_property_ui_text(prop, "Index", "Pass index number to convert to alpha");
}

static void def_cmp_map_uv(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
	/* TODO: percentage */
	prop = RNA_def_property(srna, "alpha", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom1");
	RNA_def_property_ui_text(prop, "Alpha", "");
}

static void def_cmp_defocus(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem bokeh_items[] ={
		{8, "Octagon",  "OCTAGON",  "8 sides"},
		{7, "Heptagon", "HEPTAGON", "7 sides"},
		{6, "Hexagon",  "HEXAGON",  "6 sides"},
		{5, "Pentagon", "PENTAGON", "5 sides"},
		{4, "Square",   "SQUARE",   "4 sides"},
		{3, "Triangle", "TRIANGLE", "3 sides"},
		{0, "Circle",   "CIRCLE",   ""},
		{0, NULL, NULL, NULL}
	};
	
	srna = def_node(brna, id);
	
	RNA_def_struct_sdna_from(srna, "NodeDefocus", "storage");
	
	prop = RNA_def_property(srna, "bokeh", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "bktype");
	RNA_def_property_enum_items(prop, bokeh_items);
	RNA_def_property_ui_text(prop, "Bokeh Type", "");

	/* TODO: angle in degrees */		
	prop = RNA_def_property(srna, "angle", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "rotation");
	RNA_def_property_ui_text(prop, "Angle", "Bokeh shape rotation offset in degrees");
	
	prop = RNA_def_property(srna, "gamma_correction", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gamco", 1);
	RNA_def_property_ui_text(prop, "Gamma Correction", "Enable gamma correction before and after main process");

	/* TODO */
	prop = RNA_def_property(srna, "f_stop", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fstop");
	RNA_def_property_ui_text(prop, "fStop", "Amount of focal blur, 128=infinity=perfect focus, half the value doubles the blur radius");
	
	prop = RNA_def_property(srna, "max_blur", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "maxblur");
	RNA_def_property_ui_text(prop, "Max Blur", "blur limit, maximum CoC radius, 0=no limit");
	
	prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "bthresh");
	RNA_def_property_ui_text(prop, "Threshold", "CoC radius threshold, prevents background bleed on in-focus midground, 0=off");
	
	prop = RNA_def_property(srna, "preview", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "preview", 1);
	RNA_def_property_ui_text(prop, "Preview", "Enable sampling mode, useful for preview when using low samplecounts");
	
	prop = RNA_def_property(srna, "samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "samples");
	RNA_def_property_ui_text(prop, "Samples", "Number of samples (16=grainy, higher=less noise)");
	
	prop = RNA_def_property(srna, "use_zbuffer", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "no_zbuf", 1);
	RNA_def_property_ui_text(prop, "Use Z-Buffer", "Disable when using an image as input instead of actual zbuffer (auto enabled if node not image based, eg. time node)");
	
	prop = RNA_def_property(srna, "z_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "scale");
	RNA_def_property_ui_text(prop, "Z-Scale", "Scales the Z input when not using a zbuffer, controls maximum blur designated by the color white or input value 1");
	
}

static void def_cmp_luma_matte(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
}

static void def_cmp_invert(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
}

static void def_cmp_crop(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
}

static void def_cmp_dblur(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
}

static void def_cmp_bilateral_blur(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
}

static void def_cmp_premul_key(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
}

static void def_cmp_glare(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
}

static void def_cmp_tonemap(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
}

static void def_cmp_lensdist(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = def_node(brna, id);
	
}
	


/* -- Texture Nodes --------------------------------------------------------- */

static void def_tex_output(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = def_node(brna, id);
	RNA_def_struct_sdna_from(srna, "TexNodeOutput", "storage");
	
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Name", "");
}

static void def_tex_image(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = def_node(brna, id);
	
	prop = RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "storage");
	RNA_def_property_struct_type(prop, "ImageUser");
	RNA_def_property_ui_text(prop, "Settings", "");
}

static void def_tex_bricks(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = def_node(brna, id);
	
	prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "custom3");
	RNA_def_property_ui_text(prop, "Offset Amount", "");
	
	prop = RNA_def_property(srna, "offset_frequency", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom1");
	RNA_def_property_ui_text(prop, "Offset Frequency", "Offset every N rows");
	
	prop = RNA_def_property(srna, "squash", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "custom4");
	RNA_def_property_ui_text(prop, "Squash Amount", "");
	
	prop = RNA_def_property(srna, "squash_frequency", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom2");
	RNA_def_property_ui_text(prop, "Squash Frequency", "Squash every N rows");
}

/* -------------------------------------------------------------------------- */

static void rna_def_shader_node(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	EnumPropertyItem *node_type_items;
	
	node_type_items = alloc_node_type_items(Category_ShaderNode);

	srna = RNA_def_struct(brna, "ShaderNode", "Node");
	RNA_def_struct_ui_text(srna, "Shader Node", "Material shader node.");
	RNA_def_struct_sdna(srna, "bNode");

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, node_type_items);
	RNA_def_property_ui_text(prop, "Type", "");
}

static void rna_def_compositor_node(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	EnumPropertyItem *node_type_items;
	
	node_type_items = alloc_node_type_items(Category_CompositorNode);
	
	srna = RNA_def_struct(brna, "CompositorNode", "Node");
	RNA_def_struct_ui_text(srna, "Compositor Node", "");
	RNA_def_struct_sdna(srna, "bNode");

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, node_type_items);
	RNA_def_property_ui_text(prop, "Type", "");
}

static void rna_def_texture_node(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	EnumPropertyItem *node_type_items;
	
	node_type_items = alloc_node_type_items(Category_TextureNode);
	
	srna = RNA_def_struct(brna, "TextureNode", "Node");
	RNA_def_struct_ui_text(srna, "Texture Node", "");
	RNA_def_struct_sdna(srna, "bNode");

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, node_type_items);
	RNA_def_property_ui_text(prop, "Type", "");
}

/* -------------------------------------------------------------------------- */

static void rna_def_node(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "Node", NULL);
	RNA_def_struct_ui_text(srna, "Node", "Node in a node tree.");
	RNA_def_struct_sdna(srna, "bNode");
	RNA_def_struct_refine_func(srna, "rna_Node_refine");
	
	prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "locx");
	RNA_def_property_array(prop, 2);
	RNA_def_property_range(prop, -10000.0f, 10000.0f);
	RNA_def_property_ui_text(prop, "Location", "");
	
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Node name.");
	RNA_def_struct_name_property(srna, prop);
}

static void rna_def_nodetree(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "NodeTree", "ID");
	RNA_def_struct_ui_text(srna, "Node Tree", "Node tree consisting of linked nodes used for materials, textures and compositing.");
	RNA_def_struct_sdna(srna, "bNodeTree");
	RNA_def_struct_ui_icon(srna, ICON_NODE);

	prop = RNA_def_property(srna, "nodes", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "nodes", NULL);
	RNA_def_property_struct_type(prop, "Node");
	RNA_def_property_ui_text(prop, "Nodes", "");
}

static void define_simple_node(BlenderRNA *brna, int id)
{
	def_node(brna, id);
}

static void define_specific_node(BlenderRNA *brna, int id, void (*func)(BlenderRNA*, int))
{
	func(brna, id);
}

void RNA_def_nodetree(BlenderRNA *brna)
{
	init();
	rna_def_nodetree(brna);
	rna_def_node(brna);
	rna_def_shader_node(brna);
	rna_def_compositor_node(brna);
	rna_def_texture_node(brna);
	
	#define DefNode(Category, ID, DefFunc, EnumName, StructName, UIName, UIDesc) \
		define_specific_node(brna, ID, DefFunc != 0 ? DefFunc : define_simple_node);
		
	#include "rna_nodetree_types.h"
	
	#undef DefNode
}

#endif

