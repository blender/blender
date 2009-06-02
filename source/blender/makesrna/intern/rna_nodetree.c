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


/* -- Shader Node Storage Types --------------------------------------------- */

static void rna_def_storage_node_geometry(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "NodeGeometry", NULL);
	RNA_def_struct_ui_text(srna, "Node Geometry", "");
	
	prop = RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "uvname");
	RNA_def_property_ui_text(prop, "UV Layer", "");
	
	prop = RNA_def_property(srna, "color_layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "colname");
	RNA_def_property_ui_text(prop, "Vertex Color Layer", "");
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
	
	prop = RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "storage");
	RNA_def_property_struct_type(prop, "NodeGeometry");
	RNA_def_property_ui_text(prop, "Settings", "");
}


/* -- Compositor Node Storage Types ----------------------------------------- */

static void rna_def_storage_node_blur_data(BlenderRNA *brna)
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
	
	srna = RNA_def_struct(brna, "NodeBlurData", NULL);
	RNA_def_struct_ui_text(srna, "Node Blur Data", "");
	
	/**/
	
	prop = RNA_def_property(srna, "sizex", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "sizex");
	RNA_def_property_ui_text(prop, "Size X", "");
	
	prop = RNA_def_property(srna, "sizey", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "sizey");
	RNA_def_property_ui_text(prop, "Size Y", "");
	
	/**/
	
	prop = RNA_def_property(srna, "samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "samples");
	RNA_def_property_ui_text(prop, "Samples", "");
	
	/**/
	
	prop = RNA_def_property(srna, "max_speed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "maxspeed");
	RNA_def_property_ui_text(prop, "Max Speed", "");
	
	prop = RNA_def_property(srna, "min_speed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "minspeed");
	RNA_def_property_ui_text(prop, "Min Speed", "");
	
	/**/
	
	prop = RNA_def_property(srna, "relative", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "relative", 1);
	RNA_def_property_ui_text(prop, "Relative", "");
	
	/**/
	
	prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fac");
	RNA_def_property_ui_text(prop, "Factor", "");
	
	/* These aren't percentages */
	prop = RNA_def_property(srna, "factor_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "percentx");
	RNA_def_property_ui_text(prop, "Relative Size X", "");
	
	prop = RNA_def_property(srna, "factor_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "percenty");
	RNA_def_property_ui_text(prop, "Relative Size Y", "");
	
	/**/
	
	prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "filtertype");
	RNA_def_property_enum_items(prop, filter_type_items);
	RNA_def_property_ui_text(prop, "Filter Type", "");
	
	/**/
	
	prop = RNA_def_property(srna, "bokeh", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "bokeh", 1);
	RNA_def_property_ui_text(prop, "Bokeh", "");
	
	prop = RNA_def_property(srna, "gamma", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gamma", 1);
	RNA_def_property_ui_text(prop, "Gamma", "");
	
	/*
		Also:
			curved
			image_in_width
			image_in_height
			
		Don't know if these need wrapping
	*/
	
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

	srna = def_node(brna, id);
	
	prop = RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "storage");
	RNA_def_property_struct_type(prop, "NodeBlurData");
	RNA_def_property_ui_text(prop, "Settings", "");
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


/* -- Texture Node Storage Types --------------------------------------------- */

static void rna_def_storage_tex_node_output(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "TexNodeOutput", NULL);
	RNA_def_struct_ui_text(srna, "Texture Node Output", "");
	
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Name", "");
}


/* -- Texture Nodes --------------------------------------------------------- */

static void def_tex_output(BlenderRNA *brna, int id)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = def_node(brna, id);
	
	prop = RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "storage");
	RNA_def_property_struct_type(prop, "TexNodeOutput");
	RNA_def_property_ui_text(prop, "Settings", "");
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

	/* Shader storage types */
	rna_def_storage_node_geometry(brna);
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
	
	/* Compositor storage types */
	rna_def_storage_node_blur_data(brna);
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
	
	/* Texture storage types */
	rna_def_storage_tex_node_output(brna);
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

