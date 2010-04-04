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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <float.h>
#include <stdio.h>
#include <stdlib.h>

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_brush_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"
#include "DNA_node_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h" /* MAXFRAME only */

#include "BKE_node.h"

EnumPropertyItem texture_filter_items[] = {
	{TXF_BOX, "BOX", 0, "Box", ""},
	{TXF_EWA, "EWA", 0, "EWA", ""},
	{TXF_FELINE, "FELINE", 0, "FELINE", ""},
	{TXF_AREA, "AREA", 0, "Area", ""},
	{0, NULL, 0, NULL, NULL}};

EnumPropertyItem texture_type_items[] = {
	{0, "NONE", 0, "None", ""},
	{TEX_BLEND, "BLEND", ICON_TEXTURE, "Blend", ""},
	{TEX_CLOUDS, "CLOUDS", ICON_TEXTURE, "Clouds", ""},
	{TEX_DISTNOISE, "DISTORTED_NOISE", ICON_TEXTURE, "Distorted Noise", ""},
	{TEX_ENVMAP, "ENVIRONMENT_MAP", ICON_IMAGE_DATA, "Environment Map", ""},
	{TEX_IMAGE, "IMAGE", ICON_IMAGE_DATA, "Image or Movie", ""},
	{TEX_MAGIC, "MAGIC", ICON_TEXTURE, "Magic", ""},
	{TEX_MARBLE, "MARBLE", ICON_TEXTURE, "Marble", ""},
	{TEX_MUSGRAVE, "MUSGRAVE", ICON_TEXTURE, "Musgrave", ""},
	{TEX_NOISE, "NOISE", ICON_TEXTURE, "Noise", ""},
	{TEX_PLUGIN, "PLUGIN", ICON_PLUGIN, "Plugin", ""},
	{TEX_POINTDENSITY, "POINT_DENSITY", ICON_TEXTURE, "Point Density", ""},
	{TEX_STUCCI, "STUCCI", ICON_TEXTURE, "Stucci", ""},
	{TEX_VORONOI, "VORONOI", ICON_TEXTURE, "Voronoi", ""},
	{TEX_VOXELDATA, "VOXEL_DATA", ICON_TEXTURE, "Voxel Data", ""},
	{TEX_WOOD, "WOOD", ICON_TEXTURE, "Wood", ""},
	{0, NULL, 0, NULL, NULL}};

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "BKE_depsgraph.h"
#include "BKE_texture.h"
#include "BKE_main.h"

#include "ED_node.h"

#include "WM_api.h"
#include "WM_types.h"

static StructRNA *rna_Texture_refine(struct PointerRNA *ptr)
{
	Tex *tex= (Tex*)ptr->data;

	switch(tex->type) {
		case TEX_BLEND:
			return &RNA_BlendTexture; 
		case TEX_CLOUDS:
			return &RNA_CloudsTexture;
		case TEX_DISTNOISE:
			return &RNA_DistortedNoiseTexture;
		case TEX_ENVMAP:
			return &RNA_EnvironmentMapTexture;
		case TEX_IMAGE:
			return &RNA_ImageTexture;
		case TEX_MAGIC:
			return &RNA_MagicTexture;
		case TEX_MARBLE:
			return &RNA_MarbleTexture;
		case TEX_MUSGRAVE:
			return &RNA_MusgraveTexture;
		case TEX_NOISE:
			return &RNA_NoiseTexture;
		case TEX_PLUGIN:
			return &RNA_PluginTexture;
		case TEX_POINTDENSITY:
			return &RNA_PointDensityTexture;
		case TEX_STUCCI:
			return &RNA_StucciTexture;
		case TEX_VORONOI:
			return &RNA_VoronoiTexture;
		case TEX_VOXELDATA:
			return &RNA_VoxelDataTexture;
		case TEX_WOOD:
			return &RNA_WoodTexture;
		default:
			return &RNA_Texture;
	}
}

static void rna_Texture_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Tex *tex= ptr->id.data;

	DAG_id_flush_update(&tex->id, 0);
	WM_main_add_notifier(NC_TEXTURE, tex);
}

/* Used for Texture Properties, used (also) for/in Nodes */
static void rna_Texture_nodes_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Tex *tex= ptr->id.data;

	DAG_id_flush_update(&tex->id, 0);
	WM_main_add_notifier(NC_TEXTURE|ND_NODES, tex);
}

static void rna_Texture_type_set(PointerRNA *ptr, int value)
{
	Tex *tex= (Tex*)ptr->data;

	switch(value) {

		case TEX_VOXELDATA:
			if (tex->vd == NULL)
				tex->vd = BKE_add_voxeldata();
			break;
		case TEX_POINTDENSITY:
			if (tex->pd == NULL)
				tex->pd = BKE_add_pointdensity();
			break;
		case TEX_ENVMAP:
			if (tex->env == NULL)
				tex->env = BKE_add_envmap();
			break;
	}
	
	tex->type = value;
}

void rna_TextureSlot_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	ID *id= ptr->id.data;

	DAG_id_flush_update(id, 0);

	switch(GS(id->name)) {
		case ID_MA: 
			WM_main_add_notifier(NC_MATERIAL|ND_SHADING, id);
			break;
		case ID_WO: 
			WM_main_add_notifier(NC_WORLD, id);
			break;
		case ID_LA: 
			WM_main_add_notifier(NC_LAMP|ND_LIGHTING, id);
			break;
		case ID_BR: 
			WM_main_add_notifier(NC_BRUSH, id);
			break;
	}
}

char *rna_TextureSlot_path(PointerRNA *ptr)
{
	MTex *mtex= ptr->data;
	
	/* if there is ID-data, resolve the path using the index instead of by name,
	 * since the name used is the name of the texture assigned, but the texture
	 * may be used multiple times in the same stack
	 */
	if (ptr->id.data) {
		PointerRNA id_ptr;
		PropertyRNA *prop;
		
		/* find the 'textures' property of the ID-struct */
		RNA_id_pointer_create(ptr->id.data, &id_ptr);
		prop= RNA_struct_find_property(&id_ptr, "texture_slots");
		
		/* get an iterator for this property, and try to find the relevant index */
		if (prop) {
			int index= RNA_property_collection_lookup_index(&id_ptr, prop, ptr);
			
			if (index >= 0)
				return BLI_sprintfN("texture_slots[%d]", index);
		}
	}
	
	/* this is a compromise for the remaining cases... */
	if (mtex->tex)
		return BLI_sprintfN("texture_slots[\"%s\"]", mtex->tex->id.name+2);
	else
		return BLI_strdup("texture_slots[0]");
}

static int rna_TextureSlot_name_length(PointerRNA *ptr)
{
	MTex *mtex= ptr->data;

	if(mtex->tex)
		return strlen(mtex->tex->id.name+2);
	
	return 0;
}

static void rna_TextureSlot_name_get(PointerRNA *ptr, char *str)
{
	MTex *mtex= ptr->data;

	if(mtex->tex)
		strcpy(str, mtex->tex->id.name+2);
	else
		strcpy(str, "");
}

static int rna_TextureSlot_output_node_get(PointerRNA *ptr)
{
	MTex *mtex= ptr->data;
	Tex *tex= mtex->tex;
	int cur= mtex->which_output;
	
	if(tex) {
		bNodeTree *ntree= tex->nodetree;
		bNode *node;
		if(ntree) {
			for(node= ntree->nodes.first; node; node= node->next) {
				if(node->type == TEX_NODE_OUTPUT) {
					if(cur == node->custom1)
						return cur;
				}
			}
		}
	}
	
	mtex->which_output= 0;
	return 0;
}


static EnumPropertyItem *rna_TextureSlot_output_node_itemf(bContext *C, PointerRNA *ptr, int *free)
{
	MTex *mtex= ptr->data;
	Tex *tex= mtex->tex;
	EnumPropertyItem *item= NULL;
	int totitem= 0;
	
	if(tex) {
		bNodeTree *ntree= tex->nodetree;
		if(ntree) {
			EnumPropertyItem tmp= {0, "", 0, "", ""};
			bNode *node;
			
			tmp.value = 0;
			tmp.name = "Not Specified";
			tmp.identifier = "NOT_SPECIFIED";
			RNA_enum_item_add(&item, &totitem, &tmp);
			
			for(node= ntree->nodes.first; node; node= node->next) {
				if(node->type == TEX_NODE_OUTPUT) {
					tmp.value= node->custom1;
					tmp.name= ((TexNodeOutput*)node->storage)->name;
					tmp.identifier = tmp.name;
					RNA_enum_item_add(&item, &totitem, &tmp);
				}
			}
		}
	}
	
	RNA_enum_item_end(&item, &totitem);
	*free = 1;

	return item;
}

static void rna_Texture_use_color_ramp_set(PointerRNA *ptr, int value)
{
	Tex *tex= (Tex*)ptr->data;

	if(value) tex->flag |= TEX_COLORBAND;
	else tex->flag &= ~TEX_COLORBAND;

	if((tex->flag & TEX_COLORBAND) && tex->coba == NULL)
		tex->coba= add_colorband(0);
}

static void rna_Texture_use_nodes_set(PointerRNA *ptr, int v)
{
	Tex *tex= (Tex*)ptr->data;
	
	tex->use_nodes = v;
	tex->type = 0;
	
	if(v && tex->nodetree==NULL)
		ED_node_texture_default(tex);
}

static void rna_ImageTexture_mipmap_set(PointerRNA *ptr, int value)
{
	Tex *tex= (Tex*)ptr->data;

	if(value) tex->imaflag |= TEX_MIPMAP;
	else tex->imaflag &= ~TEX_MIPMAP;

	if(tex->imaflag & TEX_MIPMAP)
		tex->texfilter = TXF_EWA;
}

static void rna_Envmap_source_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Tex *tex= ptr->id.data;
	
	if (tex->env)
		BKE_free_envmapdata(tex->env);
	
	rna_Texture_update(bmain, scene, ptr);
}

static PointerRNA rna_PointDensity_psys_get(PointerRNA *ptr)
{
	PointDensity *pd= ptr->data;
	Object *ob= pd->object;
	ParticleSystem *psys= NULL;
	PointerRNA value;

	if(ob && pd->psys)
		psys= BLI_findlink(&ob->particlesystem, pd->psys-1);

	RNA_pointer_create(&ob->id, &RNA_ParticleSystem, psys, &value);
	return value;
}

static void rna_PointDensity_psys_set(PointerRNA *ptr, PointerRNA value)
{
	PointDensity *pd= ptr->data;
	Object *ob= pd->object;

	if(ob && value.id.data == ob)
		pd->psys= BLI_findindex(&ob->particlesystem, value.data) + 1;
}

#else

static void rna_def_texmapping(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "TexMapping", NULL);
	RNA_def_struct_ui_text(srna, "Texture Mapping", "Mapping settings");

	prop= RNA_def_property(srna, "location", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "loc");
	RNA_def_property_ui_text(prop, "Location", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_EULER);
	RNA_def_property_float_sdna(prop, NULL, "rot");
	RNA_def_property_ui_text(prop, "Rotation", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "scale", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "size");
	RNA_def_property_ui_text(prop, "Scale", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "minimum", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "min");
	RNA_def_property_ui_text(prop, "Minimum", "Minimum value for clipping");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "maximum", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "max");
	RNA_def_property_ui_text(prop, "Maximum", "Maximum value for clipping");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "has_minimum", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEXMAP_CLIP_MIN);
	RNA_def_property_ui_text(prop, "Has Minimum", "Whether to use minimum clipping value");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "has_maximum", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEXMAP_CLIP_MAX);
	RNA_def_property_ui_text(prop, "Has Maximum", "Whether to use maximum clipping value");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static void rna_def_mtex(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_blend_type_items[] = {
		{MTEX_BLEND, "MIX", 0, "Mix", ""},
		{MTEX_ADD, "ADD", 0, "Add", ""},
		{MTEX_SUB, "SUBTRACT", 0, "Subtract", ""},
		{MTEX_MUL, "MULTIPLY", 0, "Multiply", ""},
		{MTEX_SCREEN, "SCREEN", 0, "Screen", ""},
		{MTEX_OVERLAY, "OVERLAY", 0, "Overlay", ""},
		{MTEX_DIFF, "DIFFERENCE", 0, "Difference", ""},
		{MTEX_DIV, "DIVIDE", 0, "Divide", ""},
		{MTEX_DARK, "DARKEN", 0, "Darken", ""},
		{MTEX_LIGHT, "LIGHTEN", 0, "Lighten", ""},
		{MTEX_BLEND_HUE, "HUE", 0, "Hue", ""},
		{MTEX_BLEND_SAT, "SATURATION", 0, "Saturation", ""},
		{MTEX_BLEND_VAL, "VALUE", 0, "Value", ""},
		{MTEX_BLEND_COLOR, "COLOR", 0, "Color", ""},
		{MTEX_SOFT_LIGHT, "SOFT LIGHT", 0, "Soft Light", ""}, 
		{MTEX_LIN_LIGHT    , "LINEAR LIGHT", 0, "Linear Light", ""}, 
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem output_node_items[] = {
		{0, "DUMMY", 0, "Dummy", ""},
		{0, NULL, 0, NULL, NULL}};
		
	srna= RNA_def_struct(brna, "TextureSlot", NULL);
	RNA_def_struct_sdna(srna, "MTex");
	RNA_def_struct_ui_text(srna, "Texture Slot", "Texture slot defining the mapping and influence of a texture");
	RNA_def_struct_path_func(srna, "rna_TextureSlot_path");
	RNA_def_struct_ui_icon(srna, ICON_TEXTURE_DATA);

	prop= RNA_def_property(srna, "texture", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tex");
	RNA_def_property_struct_type(prop, "Texture");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Texture", "Texture datablock used by this texture slot");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_TextureSlot_name_get", "rna_TextureSlot_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "Texture slot name");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

	/* mapping */
	prop= RNA_def_property(srna, "offset", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "ofs");
	RNA_def_property_ui_range(prop, -10, 10, 10, 2);
	RNA_def_property_ui_text(prop, "Offset", "Fine tunes texture mapping X, Y and Z locations");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

	prop= RNA_def_property(srna, "size", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_ui_range(prop, -100, 100, 10, 2);
	RNA_def_property_ui_text(prop, "Size", "Sets scaling for the texture's X, Y and Z sizes");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

	prop= RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "r");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Color", "The default color for textures that don't return RGB");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

	prop= RNA_def_property(srna, "blend_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "blendtype");
	RNA_def_property_enum_items(prop, prop_blend_type_items);
	RNA_def_property_ui_text(prop, "Blend Type", "");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

	prop= RNA_def_property(srna, "stencil", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "texflag", MTEX_STENCIL);
	RNA_def_property_ui_text(prop, "Stencil", "Use this texture as a blending value on the next texture");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

	prop= RNA_def_property(srna, "negate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "texflag", MTEX_NEGATIVE);
	RNA_def_property_ui_text(prop, "Negate", "Inverts the values of the texture to reverse its effect");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

	prop= RNA_def_property(srna, "rgb_to_intensity", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "texflag", MTEX_RGBTOINT);
	RNA_def_property_ui_text(prop, "RGB to Intensity", "Converts texture RGB values to intensity (gray) values");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

	prop= RNA_def_property(srna, "default_value", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "def_var");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Default Value", "Value to use for Ref, Spec, Amb, Emit, Alpha, RayMir, TransLu and Hard");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");
	
	prop= RNA_def_property(srna, "output_node", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "which_output");
	RNA_def_property_enum_items(prop, output_node_items);
	RNA_def_property_enum_funcs(prop, "rna_TextureSlot_output_node_get", NULL, "rna_TextureSlot_output_node_itemf");
	RNA_def_property_ui_text(prop, "Output Node", "Which output node to use, for node-based textures");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");
}

static void rna_def_filter_common(StructRNA *srna) 
{
	PropertyRNA *prop;
	
	prop= RNA_def_property(srna, "mipmap", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "imaflag", TEX_MIPMAP);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_ImageTexture_mipmap_set");
	RNA_def_property_ui_text(prop, "MIP Map", "Uses auto-generated MIP maps for the image");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "mipmap_gauss", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "imaflag", TEX_GAUSS_MIP);
	RNA_def_property_ui_text(prop, "MIP Map Gaussian filter", "Uses Gauss filter to sample down MIP maps");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "filter", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "texfilter");
	RNA_def_property_enum_items(prop, texture_filter_items);
	RNA_def_property_ui_text(prop, "Filter", "Texture filter to use for sampling image");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "filter_probes", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "afmax");
	RNA_def_property_range(prop, 1, 256);
	RNA_def_property_ui_text(prop, "Filter Probes", "Maximum number of samples. Higher gives less blur at distant/oblique angles, but is also slower");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "filter_eccentricity", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "afmax");
	RNA_def_property_range(prop, 1, 256);
	RNA_def_property_ui_text(prop, "Filter Eccentricity", "Maximum eccentricity. Higher gives less blur at distant/oblique angles, but is also slower");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "filter_size_minimum", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "imaflag", TEX_FILTER_MIN);
	RNA_def_property_ui_text(prop, "Minimum Filter Size", "Use Filter Size as a minimal filter value in pixels");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "filter_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "filtersize");
	RNA_def_property_range(prop, 0.1, 50.0);
	RNA_def_property_ui_range(prop, 0.1, 50.0, 1, 0.2);
	RNA_def_property_ui_text(prop, "Filter Size", "Multiplies the filter size used by MIP Map and Interpolation");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static void rna_def_environment_map(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_source_items[] = {
		{ENV_STATIC, "STATIC", 0, "Static", "Calculates environment map only once"},
		{ENV_ANIM, "ANIMATED", 0, "Animated", "Calculates environment map at each rendering"},
		{ENV_LOAD, "IMAGE_FILE", 0, "Image File", "Loads a saved environment map image from disk"},
		{0, NULL, 0, NULL, NULL}};
	
	static EnumPropertyItem prop_mapping_items[] = {
		{ENV_CUBE, "CUBE", 0, "Cube", "Use environment map with six cube sides"},
		{ENV_PLANE, "PLANE", 0, "Plane", "Only one side is rendered, with Z axis pointing in direction of image"},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "EnvironmentMap", NULL);
	RNA_def_struct_sdna(srna, "EnvMap");
	RNA_def_struct_ui_text(srna, "EnvironmentMap", "Environment map created by the renderer and cached for subsequent renders");
	
	prop= RNA_def_property(srna, "source", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "stype");
	RNA_def_property_enum_items(prop, prop_source_items);
	RNA_def_property_ui_text(prop, "Source", "");
	RNA_def_property_update(prop, 0, "rna_Envmap_source_update");

	prop= RNA_def_property(srna, "viewpoint_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "object");
	RNA_def_property_ui_text(prop, "Viewpoint Object", "Object to use as the environment map's viewpoint location");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "mapping", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_mapping_items);
	RNA_def_property_ui_text(prop, "Mapping", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "clip_start", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clipsta");
	RNA_def_property_range(prop, 0.01, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.01, 50, 100, 2);
	RNA_def_property_ui_text(prop, "Clip Start", "Objects nearer than this are not visible to map");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "clip_end", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clipend");
	RNA_def_property_range(prop, 0.01, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.10, 20000, 100, 2);
	RNA_def_property_ui_text(prop, "Clip End", "Objects further than this are not visible to map");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "zoom", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "viewscale");
	RNA_def_property_range(prop, 0.1, 5.0);
	RNA_def_property_ui_range(prop, 0.5, 1.5, 1, 2);
	RNA_def_property_ui_text(prop, "Zoom", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "ignore_layers", PROP_BOOLEAN, PROP_LAYER_MEMBER);
	RNA_def_property_boolean_sdna(prop, NULL, "notlay", 1);
	RNA_def_property_array(prop, 20);
	RNA_def_property_ui_text(prop, "Ignore Layers", "Hide objects on these layers when generating the Environment Map");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "resolution", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "cuberes");
	RNA_def_property_range(prop, 50, 4096);
	RNA_def_property_ui_text(prop, "Resolution", "Pixel resolution of the rendered environment map");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "depth", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 0, 5);
	RNA_def_property_ui_text(prop, "Depth", "Number of times a map will be rendered recursively (mirror effects.)");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static EnumPropertyItem prop_noise_basis_items[] = {
	{TEX_BLENDER, "BLENDER_ORIGINAL", 0, "Blender Original", ""},
	{TEX_STDPERLIN, "ORIGINAL_PERLIN", 0, "Original Perlin", ""},
	{TEX_NEWPERLIN, "IMPROVED_PERLIN", 0, "Improved Perlin", ""},
	{TEX_VORONOI_F1, "VORONOI_F1", 0, "Voronoi F1", ""},
	{TEX_VORONOI_F2, "VORONOI_F2", 0, "Voronoi F2", ""},
	{TEX_VORONOI_F3, "VORONOI_F3", 0, "Voronoi F3", ""},
	{TEX_VORONOI_F4, "VORONOI_F4", 0, "Voronoi F4", ""},
	{TEX_VORONOI_F2F1, "VORONOI_F2_F1", 0, "Voronoi F2-F1", ""},
	{TEX_VORONOI_CRACKLE, "VORONOI_CRACKLE", 0, "Voronoi Crackle", ""},
	{TEX_CELLNOISE, "CELL_NOISE", 0, "Cell Noise", ""},
	{0, NULL, 0, NULL, NULL}};

static EnumPropertyItem prop_noise_type[] = {
	{TEX_NOISESOFT, "SOFT_NOISE", 0, "Soft", ""},
	{TEX_NOISEPERL, "HARD_NOISE", 0, "Hard", ""},
	{0, NULL, 0, NULL, NULL}};


static void rna_def_texture_clouds(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_clouds_stype[] = {
	{TEX_DEFAULT, "GREYSCALE", 0, "Greyscale", ""},
	{TEX_COLOR, "COLOR", 0, "Color", ""},
	{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "CloudsTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Clouds Texture", "Procedural noise texture");
	RNA_def_struct_sdna(srna, "Tex");

	prop= RNA_def_property(srna, "noise_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "noisesize");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 2, 10, 2);
	RNA_def_property_ui_text(prop, "Noise Size", "Sets scaling for noise input");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "noise_depth", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "noisedepth");
	RNA_def_property_range(prop, 0, INT_MAX);
	RNA_def_property_ui_range(prop, 0, 6, 0, 2);
	RNA_def_property_ui_text(prop, "Noise Depth", "Sets the depth of the cloud calculation");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop= RNA_def_property(srna, "noise_basis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisebasis");
	RNA_def_property_enum_items(prop, prop_noise_basis_items);
	RNA_def_property_ui_text(prop, "Noise Basis", "Sets the noise basis used for turbulence");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop= RNA_def_property(srna, "noise_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisetype");
	RNA_def_property_enum_items(prop, prop_noise_type);
	RNA_def_property_ui_text(prop, "Noise Type", "");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop= RNA_def_property(srna, "stype", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "stype");
	RNA_def_property_enum_items(prop, prop_clouds_stype);
	RNA_def_property_ui_text(prop, "Color", "");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop= RNA_def_property(srna, "nabla", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.001, 0.1);
	RNA_def_property_ui_range(prop, 0.001, 0.1, 1, 2);
	RNA_def_property_ui_text(prop, "Nabla", "Size of derivative offset used for calculating normal");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static void rna_def_texture_wood(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_wood_stype[] = {
	{TEX_BAND, "BANDS", 0, "Bands", "Uses standard wood texture in bands"},
	{TEX_RING, "RINGS", 0, "Rings", "Uses wood texture in rings"},
	{TEX_BANDNOISE, "BANDNOISE", 0, "Band Noise", "Adds noise to standard wood"},
	{TEX_RINGNOISE, "RINGNOISE", 0, "Ring Noise", "Adds noise to rings"},
	{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem prop_wood_noisebasis2[] = {
	{TEX_SIN, "SIN", 0, "Sine", "Uses a sine wave to produce bands"},
	{TEX_SAW, "SAW", 0, "Saw", "Uses a saw wave to produce bands"},
	{TEX_TRI, "TRI", 0, "Tri", "Uses a triangle wave to produce bands"},
	{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "WoodTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Wood Texture", "Procedural noise texture");
	RNA_def_struct_sdna(srna, "Tex");

	prop= RNA_def_property(srna, "noise_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "noisesize");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 2, 10, 2);
	RNA_def_property_ui_text(prop, "Noise Size", "Sets scaling for noise input");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "turbulence", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "turbul");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 200, 10, 2);
	RNA_def_property_ui_text(prop, "Turbulence", "Sets the turbulence of the bandnoise and ringnoise types");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "noise_basis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisebasis");
	RNA_def_property_enum_items(prop, prop_noise_basis_items);
	RNA_def_property_ui_text(prop, "Noise Basis", "Sets the noise basis used for turbulence");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop= RNA_def_property(srna, "noise_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisetype");
	RNA_def_property_enum_items(prop, prop_noise_type);
	RNA_def_property_ui_text(prop, "Noise Type", "");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop= RNA_def_property(srna, "stype", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "stype");
	RNA_def_property_enum_items(prop, prop_wood_stype);
	RNA_def_property_ui_text(prop, "Pattern", "");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop= RNA_def_property(srna, "noisebasis2", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisebasis2");
	RNA_def_property_enum_items(prop, prop_wood_noisebasis2);
	RNA_def_property_ui_text(prop, "Noise Basis 2", "");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop= RNA_def_property(srna, "nabla", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.001, 0.1);
	RNA_def_property_ui_range(prop, 0.001, 0.1, 1, 2);
	RNA_def_property_ui_text(prop, "Nabla", "Size of derivative offset used for calculating normal");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

}

static void rna_def_texture_marble(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_marble_stype[] = {
	{TEX_SOFT, "SOFT", 0, "Soft", "Uses soft marble"},
	{TEX_SHARP, "SHARP", 0, "Sharp", "Uses more clearly defined marble"},
	{TEX_SHARPER, "SHARPER", 0, "Sharper", "Uses very clearly defined marble"},
	{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem prop_marble_noisebasis2[] = {
	{TEX_SIN, "SIN", 0, "Sin", "Uses a sine wave to produce bands"},
	{TEX_SAW, "SAW", 0, "Saw", "Uses a saw wave to produce bands"},
	{TEX_TRI, "TRI", 0, "Tri", "Uses a triangle wave to produce bands"},
	{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "MarbleTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Marble Texture", "Procedural noise texture");
	RNA_def_struct_sdna(srna, "Tex");

	prop= RNA_def_property(srna, "noise_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "noisesize");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 2, 10, 2);
	RNA_def_property_ui_text(prop, "Noise Size", "Sets scaling for noise input");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "turbulence", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "turbul");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 200, 10, 2);
	RNA_def_property_ui_text(prop, "Turbulence", "Sets the turbulence of the bandnoise and ringnoise types");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "noise_depth", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "noisedepth");
	RNA_def_property_range(prop, 0, INT_MAX);
	RNA_def_property_ui_range(prop, 0, 6, 0, 2);
	RNA_def_property_ui_text(prop, "Noise Depth", "Sets the depth of the cloud calculation");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "noise_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisetype");
	RNA_def_property_enum_items(prop, prop_noise_type);
	RNA_def_property_ui_text(prop, "Noise Type", "");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop= RNA_def_property(srna, "stype", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "stype");
	RNA_def_property_enum_items(prop, prop_marble_stype);
	RNA_def_property_ui_text(prop, "Pattern", "");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");
	
	prop= RNA_def_property(srna, "noise_basis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisebasis");
	RNA_def_property_enum_items(prop, prop_noise_basis_items);
	RNA_def_property_ui_text(prop, "Noise Basis", "Sets the noise basis used for turbulence");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop= RNA_def_property(srna, "noisebasis2", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisebasis2");
	RNA_def_property_enum_items(prop, prop_marble_noisebasis2);
	RNA_def_property_ui_text(prop, "Noise Basis 2", "");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop= RNA_def_property(srna, "nabla", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.001, 0.1);
	RNA_def_property_ui_range(prop, 0.001, 0.1, 1, 2);
	RNA_def_property_ui_text(prop, "Nabla", "Size of derivative offset used for calculating normal");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

}

static void rna_def_texture_magic(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MagicTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Magic Texture", "Procedural noise texture");
	RNA_def_struct_sdna(srna, "Tex");

	prop= RNA_def_property(srna, "turbulence", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "turbul");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 200, 10, 2);
	RNA_def_property_ui_text(prop, "Turbulence", "Sets the turbulence of the bandnoise and ringnoise types");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "noise_depth", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "noisedepth");
	RNA_def_property_range(prop, 0, INT_MAX);
	RNA_def_property_ui_range(prop, 0, 6, 0, 2);
	RNA_def_property_ui_text(prop, "Noise Depth", "Sets the depth of the cloud calculation");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static void rna_def_texture_blend(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_blend_progression[] = {
		{TEX_LIN, "LINEAR", 0, "Linear", "Creates a linear progression"},
		{TEX_QUAD, "QUADRATIC", 0, "Quadratic", "Creates a quadratic progression"},
		{TEX_EASE, "EASING", 0, "Easing", "Creates a progression easing from one step to the next"},
		{TEX_DIAG, "DIAGONAL", 0, "Diagonal", "Creates a diagonal progression"},
		{TEX_SPHERE, "SPHERICAL", 0, "Spherical", "Creates a spherical progression"},
		{TEX_HALO, "QUADRATIC_SPHERE", 0, "Quadratic sphere", "Creates a quadratic progression in the shape of a sphere"},
		{TEX_RAD, "RADIAL", 0, "Radial", "Creates a radial progression"},
		{0, NULL, 0, NULL, NULL}};

	static const EnumPropertyItem prop_flip_axis_items[]= {
		{0, "HORIZONTAL", 0, "Horizontal", "Flips the texture's X and Y axis"},
		{TEX_FLIPBLEND, "VERTICAL", 0, "Vertical", "Flips the texture's X and Y axis"},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "BlendTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Blend Texture", "Procedural color blending texture");
	RNA_def_struct_sdna(srna, "Tex");

	prop= RNA_def_property(srna, "progression", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "stype");
	RNA_def_property_enum_items(prop, prop_blend_progression);
	RNA_def_property_ui_text(prop, "Progression", "Sets the style of the color blending");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop= RNA_def_property(srna, "flip_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, prop_flip_axis_items);
	RNA_def_property_ui_text(prop, "Flip Axis", "Flips the texture's X and Y axis");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

}

static void rna_def_texture_stucci(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_stucci_stype[] = {
	{TEX_PLASTIC, "PLASTIC", 0, "Plastic", "Uses standard stucci"},
	{TEX_WALLIN, "WALL_IN", 0, "Wall in", "Creates Dimples"},
	{TEX_WALLOUT, "WALL_OUT", 0, "Wall out", "Creates Ridges"},
	{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "StucciTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Stucci Texture", "Procedural noise texture");
	RNA_def_struct_sdna(srna, "Tex");

	prop= RNA_def_property(srna, "turbulence", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "turbul");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 200, 10, 2);
	RNA_def_property_ui_text(prop, "Turbulence", "Sets the turbulence of the bandnoise and ringnoise types");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "noise_basis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisebasis");
	RNA_def_property_enum_items(prop, prop_noise_basis_items);
	RNA_def_property_ui_text(prop, "Noise Basis", "Sets the noise basis used for turbulence");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "noise_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "noisesize");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 2, 10, 2);
	RNA_def_property_ui_text(prop, "Noise Size", "Sets scaling for noise input");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "noise_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisetype");
	RNA_def_property_enum_items(prop, prop_noise_type);
	RNA_def_property_ui_text(prop, "Noise Type", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "stype", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "stype");
	RNA_def_property_enum_items(prop, prop_stucci_stype);
	RNA_def_property_ui_text(prop, "Pattern", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static void rna_def_texture_noise(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "NoiseTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Noise Texture", "Procedural noise texture");
	RNA_def_struct_sdna(srna, "Tex");
}

static void rna_def_texture_image(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_image_extension[] = {
		{TEX_EXTEND, "EXTEND", 0, "Extend", "Extends by repeating edge pixels of the image"},
		{TEX_CLIP, "CLIP", 0, "Clip", "Clips to image size and sets exterior pixels as transparent"},
		{TEX_CLIPCUBE, "CLIP_CUBE", 0, "Clip Cube", "Clips to cubic-shaped area around the image and sets exterior pixels as transparent"},
		{TEX_REPEAT, "REPEAT", 0, "Repeat", "Causes the image to repeat horizontally and vertically"},
		{TEX_CHECKER, "CHECKER", 0, "Checker", "Causes the image to repeat in checker board pattern"},
		{0, NULL, 0, NULL, NULL}};
		
	static EnumPropertyItem prop_normal_space[] = {
		{MTEX_NSPACE_CAMERA, "CAMERA", 0, "Camera", ""},
		{MTEX_NSPACE_WORLD, "WORLD", 0, "World", ""},
		{MTEX_NSPACE_OBJECT, "OBJECT", 0, "Object", ""},
		{MTEX_NSPACE_TANGENT, "TANGENT", 0, "Tangent", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "ImageTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Image Texture", "");
	RNA_def_struct_sdna(srna, "Tex");

	prop= RNA_def_property(srna, "interpolation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "imaflag", TEX_INTERPOL);
	RNA_def_property_ui_text(prop, "Interpolation", "Interpolates pixels using Area filter");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	/* XXX: I think flip_axis should be a generic Texture property, enabled for all the texture types */
	prop= RNA_def_property(srna, "flip_axis", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "imaflag", TEX_IMAROT);
	RNA_def_property_ui_text(prop, "Flip Axis", "Flips the texture's X and Y axis");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "use_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "imaflag", TEX_USEALPHA);
	RNA_def_property_ui_text(prop, "Use Alpha", "Uses the alpha channel information in the image");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "calculate_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "imaflag", TEX_CALCALPHA);
	RNA_def_property_ui_text(prop, "Calculate Alpha", "Calculates an alpha channel based on RGB values in the image");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "invert_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEX_NEGALPHA);
	RNA_def_property_ui_text(prop, "Invert Alpha", "Inverts all the alpha values in the image");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	rna_def_filter_common(srna);

	prop= RNA_def_property(srna, "extension", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "extend");
	RNA_def_property_enum_items(prop, prop_image_extension);
	RNA_def_property_ui_text(prop, "Extension", "Sets how the image is extrapolated past its original bounds");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "repeat_x", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "xrepeat");
	RNA_def_property_range(prop, 1, 512);
	RNA_def_property_ui_text(prop, "Repeat X", "Sets a repetition multiplier in the X direction");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "repeat_y", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "yrepeat");
	RNA_def_property_range(prop, 1, 512);
	RNA_def_property_ui_text(prop, "Repeat Y", "Sets a repetition multiplier in the Y direction");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "mirror_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEX_REPEAT_XMIR);
	RNA_def_property_ui_text(prop, "Mirror X", "Mirrors the image repetition on the X direction");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "mirror_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEX_REPEAT_YMIR);
	RNA_def_property_ui_text(prop, "Mirror Y", "Mirrors the image repetition on the Y direction");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "checker_odd", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEX_CHECKER_ODD);
	RNA_def_property_ui_text(prop, "Checker Odd", "Sets odd checker tiles");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "checker_even", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEX_CHECKER_EVEN);
	RNA_def_property_ui_text(prop, "Checker Even", "Sets even checker tiles");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "checker_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "checkerdist");
	RNA_def_property_range(prop, 0.0, 0.99);
	RNA_def_property_ui_range(prop, 0.0, 0.99, 0.1, 0.01);
	RNA_def_property_ui_text(prop, "Checker Distance", "Sets distance between checker tiles");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

#if 0

	/* XXX: did this as an array, but needs better descriptions than "1 2 3 4"
	perhaps a new subtype could be added? 
	--I actually used single values for this, maybe change later with a RNA_Rect thing? */
	prop= RNA_def_property(srna, "crop_rectangle", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cropxmin");
	RNA_def_property_array(prop, 4);
	RNA_def_property_range(prop, -10, 10);
	RNA_def_property_ui_text(prop, "Crop Rectangle", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

#endif

	prop= RNA_def_property(srna, "crop_min_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cropxmin");
	RNA_def_property_range(prop, -10.0, 10.0);
	RNA_def_property_ui_range(prop, -10.0, 10.0, 1, 0.2);
	RNA_def_property_ui_text(prop, "Crop Minimum X", "Sets minimum X value to crop the image");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "crop_min_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cropymin");
	RNA_def_property_range(prop, -10.0, 10.0);
	RNA_def_property_ui_range(prop, -10.0, 10.0, 1, 0.2);
	RNA_def_property_ui_text(prop, "Crop Minimum Y", "Sets minimum Y value to crop the image");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "crop_max_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cropxmax");
	RNA_def_property_range(prop, -10.0, 10.0);
	RNA_def_property_ui_range(prop, -10.0, 10.0, 1, 0.2);
	RNA_def_property_ui_text(prop, "Crop Maximum X", "Sets maximum X value to crop the image");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "crop_max_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cropymax");
	RNA_def_property_range(prop, -10.0, 10.0);
	RNA_def_property_ui_range(prop, -10.0, 10.0, 1, 0.2);
	RNA_def_property_ui_text(prop, "Crop Maximum Y", "Sets maximum Y value to crop the image");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ima");
	RNA_def_property_struct_type(prop, "Image");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Image", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "image_user", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "iuser");
	RNA_def_property_ui_text(prop, "Image User", "Parameters defining which layer, pass and frame of the image is displayed");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	/* Normal Map */
	prop= RNA_def_property(srna, "normal_map", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "imaflag", TEX_NORMALMAP);
	RNA_def_property_ui_text(prop, "Normal Map", "Uses image RGB values for normal mapping");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	/*	not sure why this goes in mtex instead of texture directly? */
	RNA_def_struct_sdna(srna, "MTex");
	
	prop= RNA_def_property(srna, "normal_space", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "normapspace");
	RNA_def_property_enum_items(prop, prop_normal_space);
	RNA_def_property_ui_text(prop, "Normal Space", "Sets space of normal map image");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static void rna_def_texture_plugin(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "PluginTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Plugin", "External plugin texture");
	RNA_def_struct_sdna(srna, "Tex");

	/* XXX: todo */
}

static void rna_def_texture_environment_map(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "EnvironmentMapTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Environment Map", "Environment map texture");
	RNA_def_struct_sdna(srna, "Tex");

	prop= RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ima");
	RNA_def_property_struct_type(prop, "Image");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Image", "Source image file to read the environment map from");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "image_user", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "iuser");
	RNA_def_property_ui_text(prop, "Image User", "Parameters defining which layer, pass and frame of the image is displayed");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	rna_def_filter_common(srna);
	
	prop= RNA_def_property(srna, "environment_map", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "env");
	RNA_def_property_struct_type(prop, "EnvironmentMap");
	RNA_def_property_ui_text(prop, "Environment Map", "Gets the environment map associated with this texture");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static void rna_def_texture_musgrave(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_musgrave_type[] = {
		{TEX_MFRACTAL, "MULTIFRACTAL", 0, "Multifractal", ""},
		{TEX_RIDGEDMF, "RIDGED_MULTIFRACTAL", 0, "Ridged Multifractal", ""},
		{TEX_HYBRIDMF, "HYBRID_MULTIFRACTAL", 0, "Hybrid Multifractal", ""},
		{TEX_FBM, "FBM", 0, "fBM", ""},
		{TEX_HTERRAIN, "HETERO_TERRAIN", 0, "Hetero Terrain", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "MusgraveTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Musgrave", "Procedural musgrave texture");
	RNA_def_struct_sdna(srna, "Tex");

	prop= RNA_def_property(srna, "musgrave_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "stype");
	RNA_def_property_enum_items(prop, prop_musgrave_type);
	RNA_def_property_ui_text(prop, "Type", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "highest_dimension", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mg_H");
	RNA_def_property_range(prop, 0.0001, 2);
	RNA_def_property_ui_text(prop, "Highest Dimension", "Highest fractal dimension");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "lacunarity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mg_lacunarity");
	RNA_def_property_range(prop, 0, 6);
	RNA_def_property_ui_text(prop, "Lacunarity", "Gap between succesive frequencies");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "octaves", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mg_octaves");
	RNA_def_property_range(prop, 0, 8);
	RNA_def_property_ui_text(prop, "Octaves", "Number of frequencies used");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mg_offset");
	RNA_def_property_range(prop, 0, 6);
	RNA_def_property_ui_text(prop, "Offset", "The fractal offset");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "gain", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mg_gain");
	RNA_def_property_range(prop, 0, 6);
	RNA_def_property_ui_text(prop, "Gain", "The gain multiplier");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "noise_intensity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ns_outscale");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Noise Intensity", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "noise_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "noisesize");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 2, 10, 2);
	RNA_def_property_ui_text(prop, "Noise Size", "Sets scaling for noise input");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "noise_basis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisebasis");
	RNA_def_property_enum_items(prop, prop_noise_basis_items);
	RNA_def_property_ui_text(prop, "Noise Basis", "Sets the noise basis used for turbulence");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "nabla", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.001, 0.1);
	RNA_def_property_ui_range(prop, 0.001, 0.1, 1, 2);
	RNA_def_property_ui_text(prop, "Nabla", "Size of derivative offset used for calculating normal");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static void rna_def_texture_voronoi(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_distance_metric_items[] = {
		{TEX_DISTANCE, "DISTANCE", 0, "Actual Distance", ""},
		{TEX_DISTANCE_SQUARED, "DISTANCE_SQUARED", 0, "Distance Squared", ""},
		{TEX_MANHATTAN, "MANHATTAN", 0, "Manhattan", ""},
		{TEX_CHEBYCHEV, "CHEBYCHEV", 0, "Chebychev", ""},
		{TEX_MINKOVSKY_HALF, "MINKOVSKY_HALF", 0, "Minkovsky 1/2", ""},
		{TEX_MINKOVSKY_FOUR, "MINKOVSKY_FOUR", 0, "Minkovsky 4", ""},
		{TEX_MINKOVSKY, "MINKOVSKY", 0, "Minkovsky", ""},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem prop_coloring_items[] = {
		/* XXX: OK names / descriptions? */
		{TEX_INTENSITY, "INTENSITY", 0, "Intensity", "Only calculate intensity"},
		{TEX_COL1, "POSITION", 0, "Position", "Color cells by position"},
		{TEX_COL2, "POSITION_OUTLINE", 0, "Position and Outline", "Use position plus an outline based on F2-F.1"},
		{TEX_COL3, "POSITION_OUTLINE_INTENSITY", 0, "Position, Outline, and Intensity", "Multiply position and outline by intensity"},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "VoronoiTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Voronoi", "Procedural voronoi texture");
	RNA_def_struct_sdna(srna, "Tex");

	prop= RNA_def_property(srna, "weight_1", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "vn_w1");
	RNA_def_property_range(prop, -2, 2);
	RNA_def_property_ui_text(prop, "Weight 1", "Voronoi feature weight 1");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "weight_2", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "vn_w2");
	RNA_def_property_range(prop, -2, 2);
	RNA_def_property_ui_text(prop, "Weight 2", "Voronoi feature weight 2");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "weight_3", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "vn_w3");
	RNA_def_property_range(prop, -2, 2);
	RNA_def_property_ui_text(prop, "Weight 3", "Voronoi feature weight 3");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "weight_4", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "vn_w4");
	RNA_def_property_range(prop, -2, 2);
	RNA_def_property_ui_text(prop, "Weight 4", "Voronoi feature weight 4");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "minkovsky_exponent", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "vn_mexp");
	RNA_def_property_range(prop, 0.01, 10);
	RNA_def_property_ui_text(prop, "Minkovsky Exponent", "Minkovsky exponent");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "distance_metric", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "vn_distm");
	RNA_def_property_enum_items(prop, prop_distance_metric_items);
	RNA_def_property_ui_text(prop, "Distance Metric", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "coloring", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "vn_coltype");
	RNA_def_property_enum_items(prop, prop_coloring_items);
	RNA_def_property_ui_text(prop, "Coloring", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "noise_intensity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ns_outscale");
	RNA_def_property_range(prop, 0.01, 10);
	RNA_def_property_ui_text(prop, "Noise Intensity", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "noise_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "noisesize");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 2, 10, 2);
	RNA_def_property_ui_text(prop, "Noise Size", "Sets scaling for noise input");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "nabla", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.001, 0.1);
	RNA_def_property_ui_range(prop, 0.001, 0.1, 1, 2);
	RNA_def_property_ui_text(prop, "Nabla", "Size of derivative offset used for calculating normal");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static void rna_def_texture_distorted_noise(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "DistortedNoiseTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Distorted Noise", "Procedural distorted noise texture");
	RNA_def_struct_sdna(srna, "Tex");

	prop= RNA_def_property(srna, "distortion", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dist_amount");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Distortion Amount", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "noise_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "noisesize");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 2, 10, 2);
	RNA_def_property_ui_text(prop, "Noise Size", "Sets scaling for noise input");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "noise_basis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisebasis2");
	RNA_def_property_enum_items(prop, prop_noise_basis_items);
	RNA_def_property_ui_text(prop, "Noise Basis", "Sets the noise basis used for turbulence");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop= RNA_def_property(srna, "noise_distortion", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisebasis");
	RNA_def_property_enum_items(prop, prop_noise_basis_items);
	RNA_def_property_ui_text(prop, "Noise Distortion", "Sets the noise basis for the distortion");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop= RNA_def_property(srna, "nabla", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.001, 0.1);
	RNA_def_property_ui_range(prop, 0.001, 0.1, 1, 2);
	RNA_def_property_ui_text(prop, "Nabla", "Size of derivative offset used for calculating normal");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static void rna_def_texture_pointdensity(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem point_source_items[] = {
		{TEX_PD_PSYS, "PARTICLE_SYSTEM", 0, "Particle System", "Generate point density from a particle system"},
		{TEX_PD_OBJECT, "OBJECT", 0, "Object Vertices", "Generate point density from an object's vertices"},
		//{TEX_PD_FILE, "FILE", 0 , "File", ""},
		{0, NULL, 0, NULL, NULL}};
	
	static EnumPropertyItem particle_cache_items[] = {
		{TEX_PD_OBJECTLOC, "OBJECT_LOCATION", 0, "Emit Object Location", ""},
		{TEX_PD_OBJECTSPACE, "OBJECT_SPACE", 0, "Emit Object Space", ""},
		{TEX_PD_WORLDSPACE, "WORLD_SPACE", 0 , "Global Space", ""},
		{0, NULL, 0, NULL, NULL}};
		
	static EnumPropertyItem vertice_cache_items[] = {
		{TEX_PD_OBJECTLOC, "OBJECT_LOCATION", 0, "Object Location", ""},
		{TEX_PD_OBJECTSPACE, "OBJECT_SPACE", 0, "Object Space", ""},
		{TEX_PD_WORLDSPACE, "WORLD_SPACE", 0 , "Global Space", ""},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem falloff_items[] = {
		{TEX_PD_FALLOFF_STD, "STANDARD", 0, "Standard", ""},
		{TEX_PD_FALLOFF_SMOOTH, "SMOOTH", 0, "Smooth", ""},
		{TEX_PD_FALLOFF_SOFT, "SOFT", 0, "Soft", ""},
		{TEX_PD_FALLOFF_CONSTANT, "CONSTANT", 0, "Constant", "Density is constant within lookup radius"},
		{TEX_PD_FALLOFF_ROOT, "ROOT", 0, "Root", ""},
		{0, NULL, 0, NULL, NULL}};
	
	static EnumPropertyItem color_source_items[] = {
		{TEX_PD_COLOR_CONSTANT, "CONSTANT", 0, "Constant", ""},
		{TEX_PD_COLOR_PARTAGE, "PARTICLE_AGE", 0, "Particle Age", "Lifetime mapped as 0.0 - 1.0 intensity"},
		{TEX_PD_COLOR_PARTSPEED, "PARTICLE_SPEED", 0, "Particle Speed", "Particle speed (absolute magnitude of velocity) mapped as 0.0-1.0 intensity"},
		{TEX_PD_COLOR_PARTVEL, "PARTICLE_VELOCITY", 0, "Particle Velocity", "XYZ velocity mapped to RGB colors"},
		{0, NULL, 0, NULL, NULL}};
	
	static EnumPropertyItem turbulence_influence_items[] = {
		{TEX_PD_NOISE_STATIC, "STATIC", 0, "Static", "Noise patterns will remain unchanged, faster and suitable for stills"},
		{TEX_PD_NOISE_VEL, "PARTICLE_VELOCITY", 0, "Particle Velocity", "Turbulent noise driven by particle velocity"},
		{TEX_PD_NOISE_AGE, "PARTICLE_AGE", 0, "Particle Age", "Turbulent noise driven by the particle's age between birth and death"},
		{TEX_PD_NOISE_TIME, "GLOBAL_TIME", 0, "Global Time", "Turbulent noise driven by the global current frame"},
		{0, NULL, 0, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "PointDensity", NULL);
	RNA_def_struct_sdna(srna, "PointDensity");
	RNA_def_struct_ui_text(srna, "PointDensity", "Point density settings");
	
	prop= RNA_def_property(srna, "point_source", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "source");
	RNA_def_property_enum_items(prop, point_source_items);
	RNA_def_property_ui_text(prop, "Point Source", "Point data to use as renderable point density");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "object");
	RNA_def_property_ui_text(prop, "Object", "Object to take point data from");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "particle_system", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Particle System", "Particle System to render as points");
	RNA_def_property_struct_type(prop, "ParticleSystem");
	RNA_def_property_pointer_funcs(prop, "rna_PointDensity_psys_get", "rna_PointDensity_psys_set", NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "particle_cache", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "psys_cache_space");
	RNA_def_property_enum_items(prop, particle_cache_items);
	RNA_def_property_ui_text(prop, "Particle Cache", "Co-ordinate system to cache particles in");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "vertices_cache", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "ob_cache_space");
	RNA_def_property_enum_items(prop, vertice_cache_items);
	RNA_def_property_ui_text(prop, "Vertices Cache", "Co-ordinate system to cache vertices in");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "radius");
	RNA_def_property_range(prop, 0.001, FLT_MAX);
	RNA_def_property_ui_text(prop, "Radius", "Radius from the shaded sample to look for points within");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "falloff", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "falloff_type");
	RNA_def_property_enum_items(prop, falloff_items);
	RNA_def_property_ui_text(prop, "Falloff", "Method of attenuating density by distance from the point");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "falloff_softness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "falloff_softness");
	RNA_def_property_range(prop, 0.01, FLT_MAX);
	RNA_def_property_ui_text(prop, "Softness", "Softness of the 'soft' falloff option");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "color_source", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "color_source");
	RNA_def_property_enum_items(prop, color_source_items);
	RNA_def_property_ui_text(prop, "Color Source", "Data to derive color results from");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "speed_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "speed_scale");
	RNA_def_property_range(prop, 0.001, 100.0);
	RNA_def_property_ui_text(prop, "Scale", "Multipler to bring particle speed within an acceptable range");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "color_ramp", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "coba");
	RNA_def_property_struct_type(prop, "ColorRamp");
	RNA_def_property_ui_text(prop, "Color Ramp", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	/* Turbulence */
	prop= RNA_def_property(srna, "turbulence", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEX_PD_TURBULENCE);
	RNA_def_property_ui_text(prop, "Turbulence", "Add directed noise to the density at render-time");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "turbulence_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "noise_size");
	RNA_def_property_range(prop, 0.01, FLT_MAX);
	RNA_def_property_ui_text(prop, "Size", "Scale of the added turbulent noise");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "turbulence_strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "noise_fac");
	RNA_def_property_range(prop, 0.01, FLT_MAX);
	RNA_def_property_ui_text(prop, "Strength", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "turbulence_depth", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "noise_depth");
	RNA_def_property_range(prop, 0, INT_MAX);
	RNA_def_property_ui_text(prop, "Depth", "Level of detail in the added turbulent noise");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "turbulence_influence", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noise_influence");
	RNA_def_property_enum_items(prop, turbulence_influence_items);
	RNA_def_property_ui_text(prop, "Turbulence Influence", "Method for driving added turbulent noise");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "noise_basis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noise_basis");
	RNA_def_property_enum_items(prop, prop_noise_basis_items);
	RNA_def_property_ui_text(prop, "Noise Basis", "Noise formula used for tubulence");
	RNA_def_property_update(prop, 0, "rna_Texture_update");


	srna= RNA_def_struct(brna, "PointDensityTexture", "Texture");
	RNA_def_struct_sdna(srna, "Tex");
	RNA_def_struct_ui_text(srna, "Point Density", "Settings for the Point Density texture");
	
	prop= RNA_def_property(srna, "pointdensity", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "pd");
	RNA_def_property_struct_type(prop, "PointDensity");
	RNA_def_property_ui_text(prop, "Point Density", "The point density settings associated with this texture");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static void rna_def_texture_voxeldata(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem interpolation_type_items[] = {
		{TEX_VD_NEARESTNEIGHBOR, "NEREASTNEIGHBOR", 0, "Nearest Neighbor", "No interpolation, fast but blocky and low quality"},
		{TEX_VD_LINEAR, "TRILINEAR", 0, "Linear", "Good smoothness and speed"},
		{TEX_VD_QUADRATIC, "QUADRATIC", 0, "Quadratic", "Mid-range quality and speed"},
		{TEX_VD_TRICUBIC_CATROM, "TRICUBIC_CATROM", 0, "Cubic Catmull-Rom", "High quality interpolation, but slower"},
		{TEX_VD_TRICUBIC_BSPLINE, "TRICUBIC_BSPLINE", 0, "Cubic B-Spline", "Smoothed high quality interpolation, but slower"},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem file_format_items[] = {
		{TEX_VD_BLENDERVOXEL, "BLENDER_VOXEL", 0, "Blender Voxel", "Default binary voxel file format"},
		{TEX_VD_RAW_8BIT, "RAW_8BIT", 0, "8 bit RAW", "8 bit greyscale binary data"},
		//{TEX_VD_RAW_16BIT, "RAW_16BIT", 0, "16 bit RAW", ""},
		{TEX_VD_IMAGE_SEQUENCE, "IMAGE_SEQUENCE", 0, "Image Sequence", "Generate voxels from a sequence of image slices"},
		{TEX_VD_SMOKE, "SMOKE", 0, "Smoke", "Render voxels from a Blender smoke simulation"},
		{0, NULL, 0, NULL, NULL}};
	
	static EnumPropertyItem voxeldata_extension[] = {
		{TEX_EXTEND, "EXTEND", 0, "Extend", "Extends by repeating edge pixels of the image"},
		{TEX_CLIP, "CLIP", 0, "Clip", "Clips to image size and sets exterior pixels as transparent"},
		{TEX_REPEAT, "REPEAT", 0, "Repeat", "Causes the image to repeat horizontally and vertically"},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem smoked_type_items[] = {
		{TEX_VD_SMOKEDENSITY, "SMOKEDENSITY", 0, "Density", "Use smoke density as texture data"},
		{TEX_VD_SMOKEHEAT, "SMOKEHEAT", 0, "Heat", "Use smoke heat as texture data. Values from -2.0 to 2.0 are used"},
		{TEX_VD_SMOKEVEL, "SMOKEVEL", 0, "Velocity", "Use smoke velocity as texture data"},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "VoxelData", NULL);
	RNA_def_struct_sdna(srna, "VoxelData");
	RNA_def_struct_ui_text(srna, "VoxelData", "Voxel data settings");
	
	prop= RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "interp_type");
	RNA_def_property_enum_items(prop, interpolation_type_items);
	RNA_def_property_ui_text(prop, "Interpolation", "Method to interpolate/smooth values between voxel cells");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "smoke_data_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "smoked_type");
	RNA_def_property_enum_items(prop, smoked_type_items);
	RNA_def_property_ui_text(prop, "Source", "Simulation value to be used as a texture");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "extension", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "extend");
	RNA_def_property_enum_items(prop, voxeldata_extension);
	RNA_def_property_ui_text(prop, "Extension", "Sets how the texture is extrapolated past its original bounds");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "intensity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "int_multiplier");
	RNA_def_property_range(prop, 0.01, FLT_MAX);
	RNA_def_property_ui_text(prop, "Intensity", "Multiplier for intensity values");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "file_format", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "file_format");
	RNA_def_property_enum_items(prop, file_format_items);
	RNA_def_property_ui_text(prop, "File Format", "Format of the source data set to render	");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "source_path", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_sdna(prop, NULL, "source_path");
	RNA_def_property_ui_text(prop, "Source Path", "The external source data file to use");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "resolution", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "resol");
	RNA_def_property_ui_text(prop, "Resolution", "Resolution of the voxel grid");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "still", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEX_VD_STILL);
	RNA_def_property_ui_text(prop, "Still Frame Only", "Always render a still frame from the voxel data sequence");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "still_frame_number", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "still_frame");
	RNA_def_property_range(prop, -MAXFRAME, MAXFRAME);
	RNA_def_property_ui_text(prop, "Still Frame Number", "The frame number to always use");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "domain_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "object");
	RNA_def_property_ui_text(prop, "Domain Object", "Object used as the smoke simulation domain");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	
	srna= RNA_def_struct(brna, "VoxelDataTexture", "Texture");
	RNA_def_struct_sdna(srna, "Tex");
	RNA_def_struct_ui_text(srna, "Voxel Data", "Settings for the Voxel Data texture");
	
	prop= RNA_def_property(srna, "voxeldata", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "vd");
	RNA_def_property_struct_type(prop, "VoxelData");
	RNA_def_property_ui_text(prop, "Voxel Data", "The voxel data associated with this texture");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ima");
	RNA_def_property_struct_type(prop, "Image");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Image", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "image_user", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "iuser");
	RNA_def_property_ui_text(prop, "Image User", "Parameters defining which layer, pass and frame of the image is displayed");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static void rna_def_texture(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "Texture", "ID");
	RNA_def_struct_sdna(srna, "Tex");
	RNA_def_struct_ui_text(srna, "Texture", "Texture datablock used by materials, lamps, worlds and brushes");
	RNA_def_struct_ui_icon(srna, ICON_TEXTURE_DATA);
	RNA_def_struct_refine_func(srna, "rna_Texture_refine");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	//RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, texture_type_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_Texture_type_set", NULL);
	RNA_def_property_ui_text(prop, "Type", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "use_color_ramp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEX_COLORBAND);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Texture_use_color_ramp_set");
	RNA_def_property_ui_text(prop, "Use Color Ramp", "Toggle color ramp operations");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "color_ramp", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "coba");
	RNA_def_property_struct_type(prop, "ColorRamp");
	RNA_def_property_ui_text(prop, "Color Ramp", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "brightness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "bright");
	RNA_def_property_range(prop, 0, 2);
	RNA_def_property_ui_text(prop, "Brightness", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop= RNA_def_property(srna, "contrast", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.01, 5);
	RNA_def_property_ui_text(prop, "Contrast", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	/* RGB Factor */
	prop= RNA_def_property(srna, "factor_red", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rfac");
	RNA_def_property_range(prop, 0, 2);
	RNA_def_property_ui_text(prop, "Factor Red", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "factor_green", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "gfac");
	RNA_def_property_range(prop, 0, 2);
	RNA_def_property_ui_text(prop, "Factor Green", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	prop= RNA_def_property(srna, "factor_blue", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "bfac");
	RNA_def_property_range(prop, 0, 2);
	RNA_def_property_ui_text(prop, "Factor Blue", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	/* Alpha for preview render */
	prop= RNA_def_property(srna, "use_preview_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEX_PRV_ALPHA);
	RNA_def_property_ui_text(prop, "Show Alpha", "Show Alpha in Preview Render");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
	
	/* nodetree */
	prop= RNA_def_property(srna, "use_nodes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "use_nodes", 1);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Texture_use_nodes_set");
	RNA_def_property_ui_text(prop, "Use Nodes", "Make this a node-based texture");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");
	
	prop= RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "nodetree");
	RNA_def_property_ui_text(prop, "Node Tree", "Node tree for node-based textures");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");
	
	rna_def_animdata_common(srna);

	/* specific types */
	rna_def_texture_clouds(brna);
	rna_def_texture_wood(brna);
	rna_def_texture_marble(brna);
	rna_def_texture_magic(brna);
	rna_def_texture_blend(brna);
	rna_def_texture_stucci(brna);
	rna_def_texture_noise(brna);
	rna_def_texture_image(brna);
	rna_def_texture_plugin(brna);
	rna_def_texture_environment_map(brna);
	rna_def_texture_musgrave(brna);
	rna_def_texture_voronoi(brna);
	rna_def_texture_distorted_noise(brna);
	rna_def_texture_pointdensity(brna);
	rna_def_texture_voxeldata(brna);
	/* XXX add more types here .. */
}

void RNA_def_texture(BlenderRNA *brna)
{
	rna_def_texture(brna);
	rna_def_mtex(brna);
	rna_def_environment_map(brna);
	rna_def_texmapping(brna);
}

#endif
