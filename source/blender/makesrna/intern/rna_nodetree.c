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
 * Contributor(s): Blender Foundation (2008), Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_node_types.h"
#include "BKE_node.h"

#ifdef RNA_RUNTIME

StructRNA *rna_Node_refine(struct PointerRNA *ptr)
{
	bNode *node= (bNode*)ptr->data;

	switch(node->type) {
		case SH_NODE_OUTPUT:
			return &RNA_ShaderNodeOutput;
		case SH_NODE_MATERIAL:
			return &RNA_ShaderNodeMaterial;
		/* XXX complete here */
		default:
			return &RNA_Node;
	}
}

#else

static void rna_def_shader_node_output(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "ShaderNodeOutput", "ShaderNode");
	RNA_def_struct_ui_text(srna, "Shader Node Output", "");
	RNA_def_struct_sdna(srna, "bNode");
}

static void rna_def_shader_node_material(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ShaderNodeMaterial", "ShaderNode");
	RNA_def_struct_ui_text(srna, "Shader Node Material", "");
	RNA_def_struct_sdna(srna, "bNode");

	prop= RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "Material");
	RNA_def_property_ui_text(prop, "Material", "");

	prop= RNA_def_property(srna, "diffuse", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", SH_NODE_MAT_DIFF);
	RNA_def_property_ui_text(prop, "Diffuse", "Material Node outputs Diffuse");

	/* XXX add specular, negate normal */
}

static void rna_def_shader_node(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem node_type_items[] ={
		{SH_NODE_OUTPUT, "OUTPUT", "Output", ""},
		{SH_NODE_MATERIAL, "MATERIAL", "Material", ""},
		{SH_NODE_RGB, "RGB", "RGB", ""},
		{SH_NODE_VALUE, "VALUE", "Value", ""},
		{SH_NODE_MIX_RGB, "MIX_RGB", "Mix RGB", ""},
		{SH_NODE_VALTORGB, "VALUE_TO_RGB", "Value to RGB", ""},
		{SH_NODE_RGBTOBW, "RGB_TO_BW", "RGB to BW", ""},
		{SH_NODE_TEXTURE, "TEXTURE", "Texture", ""},
		{SH_NODE_NORMAL, "NORMAL", "Normal", ""},
		{SH_NODE_GEOMETRY, "GEOMETRY", "Geometry", ""},
		{SH_NODE_MAPPING, "MAPPING", "Mapping", ""},
		{SH_NODE_CURVE_VEC, "VECTOR_CURVES", "Vector Curves", ""},
		{SH_NODE_CURVE_RGB, "RGB_CURVES", "RGB Curves", ""},
		{SH_NODE_CAMERA, "CAMERA", "Camera", ""},
		{SH_NODE_MATH, "MATH", "Math", ""},
		{SH_NODE_VECT_MATH, "VECTOR_MATH", "Vector Math", ""},
		{SH_NODE_SQUEEZE, "SQUEEZE", "Squeeze", ""},
		{SH_NODE_MATERIAL_EXT, "MATERIAL_EXTENDED", "Material Extended", ""},
		{SH_NODE_INVERT, "INVERT", "Invert", ""},
		{SH_NODE_SEPRGB, "SEPARATE_RGB", "Seperate RGB", ""},
		{SH_NODE_COMBRGB, "COMBINE_RGB", "Combine RGB", ""},
		{SH_NODE_HUE_SAT, "HUE_SATURATION", "Hue/Saturation", ""},
		{NODE_DYNAMIC, "SCRIPT", "Script", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "ShaderNode", "Node");
	RNA_def_struct_ui_text(srna, "Shader Node", "Material shader node.");
	RNA_def_struct_sdna(srna, "bNode");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_enum_items(prop, node_type_items);
	RNA_def_property_ui_text(prop, "Type", "");

	/* specific types */
	rna_def_shader_node_output(brna);
	rna_def_shader_node_material(brna);
}

static void rna_def_node(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "Node", NULL);
	RNA_def_struct_ui_text(srna, "Node", "Node in a node tree.");
	RNA_def_struct_sdna(srna, "bNode");
	RNA_def_struct_refine_func(srna, "rna_Node_refine");
	
	prop= RNA_def_property(srna, "location", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "locx");
	RNA_def_property_array(prop, 2);
	RNA_def_property_range(prop, -10000.0f, 10000.0f);
	RNA_def_property_ui_text(prop, "Location", "");
	
	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Node name.");
	RNA_def_struct_name_property(srna, prop);
}

static void rna_def_nodetree(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "NodeTree", "ID");
	RNA_def_struct_ui_text(srna, "Node Tree", "Node tree consisting of linked nodes used for materials, textures and compositing.");
	RNA_def_struct_sdna(srna, "bNodeTree");

	prop= RNA_def_property(srna, "nodes", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "nodes", NULL);
	RNA_def_property_struct_type(prop, "Node");
	RNA_def_property_ui_text(prop, "Nodes", "");
}

void RNA_def_nodetree(BlenderRNA *brna)
{
	rna_def_nodetree(brna);
	rna_def_node(brna);
	rna_def_shader_node(brna);
}

#endif

