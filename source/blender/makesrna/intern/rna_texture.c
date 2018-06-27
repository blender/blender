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

/** \file blender/makesrna/intern/rna_texture.c
 *  \ingroup RNA
 */

#include <float.h>
#include <stdio.h>
#include <stdlib.h>

#include "DNA_brush_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"
#include "DNA_node_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h" /* MAXFRAME only */

#include "BLI_utildefines.h"

#include "BKE_node.h"
#include "BKE_paint.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_api.h"
#include "WM_types.h"

#ifndef RNA_RUNTIME
static const EnumPropertyItem texture_filter_items[] = {
	{TXF_BOX, "BOX", 0, "Box", ""},
	{TXF_EWA, "EWA", 0, "EWA", ""},
	{TXF_FELINE, "FELINE", 0, "FELINE", ""},
	{TXF_AREA, "AREA", 0, "Area", ""},
	{0, NULL, 0, NULL, NULL}
};
#endif

const EnumPropertyItem rna_enum_texture_type_items[] = {
	{0, "NONE", 0, "None", ""},
	{TEX_BLEND, "BLEND", ICON_TEXTURE, "Blend", "Procedural - create a ramp texture"},
	{TEX_CLOUDS, "CLOUDS", ICON_TEXTURE, "Clouds", "Procedural - create a cloud-like fractal noise texture"},
	{TEX_DISTNOISE, "DISTORTED_NOISE", ICON_TEXTURE,
	                "Distorted Noise", "Procedural - noise texture distorted by two noise algorithms"},
	{TEX_IMAGE, "IMAGE", ICON_IMAGE_DATA, "Image or Movie", "Allow for images or movies to be used as textures"},
	{TEX_MAGIC, "MAGIC", ICON_TEXTURE, "Magic", "Procedural - color texture based on trigonometric functions"},
	{TEX_MARBLE, "MARBLE", ICON_TEXTURE, "Marble", "Procedural - marble-like noise texture with wave generated bands"},
	{TEX_MUSGRAVE, "MUSGRAVE", ICON_TEXTURE, "Musgrave", "Procedural - highly flexible fractal noise texture"},
	{TEX_NOISE, "NOISE", ICON_TEXTURE, "Noise",
	            "Procedural - random noise, gives a different result every time, for every frame, for every pixel"},
	{TEX_STUCCI, "STUCCI", ICON_TEXTURE, "Stucci", "Procedural - create a fractal noise texture"},
	{TEX_VORONOI, "VORONOI", ICON_TEXTURE, "Voronoi", "Procedural - create cell-like patterns based on Worley noise"},
	{TEX_WOOD, "WOOD", ICON_TEXTURE, "Wood", "Procedural - wave generated bands or rings, with optional noise"},
	{0, NULL, 0, NULL, NULL}
};

#ifndef RNA_RUNTIME
static const EnumPropertyItem blend_type_items[] = {
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
	{MTEX_SOFT_LIGHT, "SOFT_LIGHT", 0, "Soft Light", ""},
	{MTEX_LIN_LIGHT, "LINEAR_LIGHT", 0, "Linear Light", ""},
	{0, NULL, 0, NULL, NULL}
};
#endif

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "BKE_colorband.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_texture.h"
#include "BKE_main.h"

#include "DEG_depsgraph.h"

#include "ED_node.h"
#include "ED_render.h"

static StructRNA *rna_Texture_refine(struct PointerRNA *ptr)
{
	Tex *tex = (Tex *)ptr->data;

	switch (tex->type) {
		case TEX_BLEND:
			return &RNA_BlendTexture;
		case TEX_CLOUDS:
			return &RNA_CloudsTexture;
		case TEX_DISTNOISE:
			return &RNA_DistortedNoiseTexture;
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
		case TEX_STUCCI:
			return &RNA_StucciTexture;
		case TEX_VORONOI:
			return &RNA_VoronoiTexture;
		case TEX_WOOD:
			return &RNA_WoodTexture;
		default:
			return &RNA_Texture;
	}
}

static void rna_Texture_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
	ID *id = ptr->id.data;

	if (GS(id->name) == ID_TE) {
		Tex *tex = ptr->id.data;

		DEG_id_tag_update(&tex->id, 0);
		WM_main_add_notifier(NC_TEXTURE, tex);
		WM_main_add_notifier(NC_MATERIAL | ND_SHADING_DRAW, NULL);
	}
	else if (GS(id->name) == ID_NT) {
		bNodeTree *ntree = ptr->id.data;
		ED_node_tag_update_nodetree(bmain, ntree, NULL);
	}
}

static void rna_Texture_mapping_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	TexMapping *texmap = ptr->data;
	BKE_texture_mapping_init(texmap);
	rna_Texture_update(bmain, scene, ptr);
}

static void rna_Color_mapping_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *UNUSED(ptr))
{
	/* nothing to do */
}

/* Used for Texture Properties, used (also) for/in Nodes */
static void rna_Texture_nodes_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Tex *tex = ptr->id.data;

	DEG_id_tag_update(&tex->id, 0);
	WM_main_add_notifier(NC_TEXTURE | ND_NODES, tex);
}

static void rna_Texture_type_set(PointerRNA *ptr, int value)
{
	Tex *tex = (Tex *)ptr->data;

	BKE_texture_type_set(tex, value);
}

void rna_TextureSlot_update(bContext *C, PointerRNA *ptr)
{
	ID *id = ptr->id.data;

	DEG_id_tag_update(id, 0);

	switch (GS(id->name)) {
		case ID_MA:
			WM_main_add_notifier(NC_MATERIAL | ND_SHADING, id);
			WM_main_add_notifier(NC_MATERIAL | ND_SHADING_DRAW, id);
			break;
		case ID_WO:
			WM_main_add_notifier(NC_WORLD, id);
			break;
		case ID_LA:
			WM_main_add_notifier(NC_LAMP | ND_LIGHTING, id);
			WM_main_add_notifier(NC_LAMP | ND_LIGHTING_DRAW, id);
			break;
		case ID_BR:
		{
			Scene *scene = CTX_data_scene(C);
			MTex *mtex = ptr->data;
			ViewLayer *view_layer = CTX_data_view_layer(C);
			BKE_paint_invalidate_overlay_tex(scene, view_layer, mtex->tex);
			WM_main_add_notifier(NC_BRUSH, id);
			break;
		}
		case ID_LS:
			WM_main_add_notifier(NC_LINESTYLE, id);
			break;
		case ID_PA:
		{
			MTex *mtex = ptr->data;
			int recalc = OB_RECALC_DATA;

			if (mtex->mapto & PAMAP_INIT)
				recalc |= PSYS_RECALC_RESET;
			if (mtex->mapto & PAMAP_CHILD)
				recalc |= PSYS_RECALC_CHILD;

			DEG_id_tag_update(id, recalc);
			WM_main_add_notifier(NC_OBJECT | ND_PARTICLE | NA_EDITED, NULL);
			break;
		}
		default:
			break;
	}
}

char *rna_TextureSlot_path(PointerRNA *ptr)
{
	MTex *mtex = ptr->data;

	/* if there is ID-data, resolve the path using the index instead of by name,
	 * since the name used is the name of the texture assigned, but the texture
	 * may be used multiple times in the same stack
	 */
	if (ptr->id.data) {
		if (GS(((ID *)ptr->id.data)->name) == ID_BR) {
			return BLI_strdup("texture_slot");
		}
		else {
			PointerRNA id_ptr;
			PropertyRNA *prop;

			/* find the 'textures' property of the ID-struct */
			RNA_id_pointer_create(ptr->id.data, &id_ptr);
			prop = RNA_struct_find_property(&id_ptr, "texture_slots");

			/* get an iterator for this property, and try to find the relevant index */
			if (prop) {
				int index = RNA_property_collection_lookup_index(&id_ptr, prop, ptr);

				if (index != -1) {
					return BLI_sprintfN("texture_slots[%d]", index);
				}
			}
		}
	}

	/* this is a compromise for the remaining cases... */
	if (mtex->tex) {
		char name_esc[(sizeof(mtex->tex->id.name) - 2) * 2];

		BLI_strescape(name_esc, mtex->tex->id.name + 2, sizeof(name_esc));
		return BLI_sprintfN("texture_slots[\"%s\"]", name_esc);
	}
	else {
		return BLI_strdup("texture_slots[0]");
	}
}

static int rna_TextureSlot_name_length(PointerRNA *ptr)
{
	MTex *mtex = ptr->data;

	if (mtex->tex)
		return strlen(mtex->tex->id.name + 2);

	return 0;
}

static void rna_TextureSlot_name_get(PointerRNA *ptr, char *str)
{
	MTex *mtex = ptr->data;

	if (mtex->tex)
		strcpy(str, mtex->tex->id.name + 2);
	else
		str[0] = '\0';
}

static int rna_TextureSlot_output_node_get(PointerRNA *ptr)
{
	MTex *mtex = ptr->data;
	Tex *tex = mtex->tex;
	int cur = mtex->which_output;

	if (tex) {
		bNodeTree *ntree = tex->nodetree;
		bNode *node;
		if (ntree) {
			for (node = ntree->nodes.first; node; node = node->next) {
				if (node->type == TEX_NODE_OUTPUT) {
					if (cur == node->custom1)
						return cur;
				}
			}
		}
	}

	mtex->which_output = 0;
	return 0;
}


static const EnumPropertyItem *rna_TextureSlot_output_node_itemf(
        bContext *UNUSED(C), PointerRNA *ptr,
        PropertyRNA *UNUSED(prop), bool *r_free)
{
	MTex *mtex = ptr->data;
	Tex *tex = mtex->tex;
	EnumPropertyItem *item = NULL;
	int totitem = 0;

	if (tex) {
		bNodeTree *ntree = tex->nodetree;
		if (ntree) {
			EnumPropertyItem tmp = {0, "", 0, "", ""};
			bNode *node;

			tmp.value = 0;
			tmp.name = "Not Specified";
			tmp.identifier = "NOT_SPECIFIED";
			RNA_enum_item_add(&item, &totitem, &tmp);

			for (node = ntree->nodes.first; node; node = node->next) {
				if (node->type == TEX_NODE_OUTPUT) {
					tmp.value = node->custom1;
					tmp.name = ((TexNodeOutput *)node->storage)->name;
					tmp.identifier = tmp.name;
					RNA_enum_item_add(&item, &totitem, &tmp);
				}
			}
		}
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

static void rna_Texture_use_color_ramp_set(PointerRNA *ptr, bool value)
{
	Tex *tex = (Tex *)ptr->data;

	if (value) tex->flag |= TEX_COLORBAND;
	else tex->flag &= ~TEX_COLORBAND;

	if ((tex->flag & TEX_COLORBAND) && tex->coba == NULL)
		tex->coba = BKE_colorband_add(false);
}

static void rna_Texture_use_nodes_update(bContext *C, PointerRNA *ptr)
{
	Tex *tex = (Tex *)ptr->data;

	if (tex->use_nodes) {
		tex->type = 0;

		if (tex->nodetree == NULL)
			ED_node_texture_default(C, tex);
	}

	rna_Texture_nodes_update(CTX_data_main(C), CTX_data_scene(C), ptr);
}

static void rna_ImageTexture_mipmap_set(PointerRNA *ptr, bool value)
{
	Tex *tex = (Tex *)ptr->data;

	if (value) tex->imaflag |= TEX_MIPMAP;
	else tex->imaflag &= ~TEX_MIPMAP;
}

#else

static void rna_def_texmapping(BlenderRNA *brna)
{
	static const EnumPropertyItem prop_mapping_items[] = {
		{MTEX_FLAT, "FLAT", 0, "Flat", "Map X and Y coordinates directly"},
		{MTEX_CUBE, "CUBE", 0, "Cube", "Map using the normal vector"},
		{MTEX_TUBE, "TUBE", 0, "Tube", "Map with Z as central axis"},
		{MTEX_SPHERE, "SPHERE", 0, "Sphere", "Map with Z as central axis"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem prop_vect_type_items[] = {
		{TEXMAP_TYPE_TEXTURE, "TEXTURE", 0, "Texture", "Transform a texture by inverse mapping the texture coordinate"},
		{TEXMAP_TYPE_POINT,   "POINT",   0, "Point",   "Transform a point"},
		{TEXMAP_TYPE_VECTOR,  "VECTOR",  0, "Vector",  "Transform a direction vector"},
		{TEXMAP_TYPE_NORMAL,  "NORMAL",  0, "Normal",  "Transform a normal vector with unit length"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem prop_xyz_mapping_items[] = {
		{0, "NONE", 0, "None", ""},
		{1, "X", 0, "X", ""},
		{2, "Y", 0, "Y", ""},
		{3, "Z", 0, "Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "TexMapping", NULL);
	RNA_def_struct_ui_text(srna, "Texture Mapping", "Texture coordinate mapping settings");

	prop = RNA_def_property(srna, "vector_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_vect_type_items);
	RNA_def_property_ui_text(prop, "Type", "Type of vector that the mapping transforms");
	RNA_def_property_update(prop, 0, "rna_Texture_mapping_update");

	prop = RNA_def_property(srna, "translation", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "loc");
	RNA_def_property_ui_text(prop, "Location", "");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
	RNA_def_property_update(prop, 0, "rna_Texture_mapping_update");

	/* Not PROP_XYZ, this is now in radians, no more degrees */
	prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_EULER);
	RNA_def_property_float_sdna(prop, NULL, "rot");
	RNA_def_property_ui_text(prop, "Rotation", "");
	RNA_def_property_update(prop, 0, "rna_Texture_mapping_update");

	prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "size");
	RNA_def_property_flag(prop, PROP_PROPORTIONAL);
	RNA_def_property_ui_text(prop, "Scale", "");
	RNA_def_property_update(prop, 0, "rna_Texture_mapping_update");

	prop = RNA_def_property(srna, "min", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "min");
	RNA_def_property_ui_text(prop, "Minimum", "Minimum value for clipping");
	RNA_def_property_update(prop, 0, "rna_Texture_mapping_update");

	prop = RNA_def_property(srna, "max", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "max");
	RNA_def_property_ui_text(prop, "Maximum", "Maximum value for clipping");
	RNA_def_property_update(prop, 0, "rna_Texture_mapping_update");

	prop = RNA_def_property(srna, "use_min", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEXMAP_CLIP_MIN);
	RNA_def_property_ui_text(prop, "Has Minimum", "Whether to use minimum clipping value");
	RNA_def_property_update(prop, 0, "rna_Texture_mapping_update");

	prop = RNA_def_property(srna, "use_max", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEXMAP_CLIP_MAX);
	RNA_def_property_ui_text(prop, "Has Maximum", "Whether to use maximum clipping value");
	RNA_def_property_update(prop, 0, "rna_Texture_mapping_update");

	prop = RNA_def_property(srna, "mapping_x", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "projx");
	RNA_def_property_enum_items(prop, prop_xyz_mapping_items);
	RNA_def_property_ui_text(prop, "X Mapping", "");
	RNA_def_property_update(prop, 0, "rna_Texture_mapping_update");

	prop = RNA_def_property(srna, "mapping_y", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "projy");
	RNA_def_property_enum_items(prop, prop_xyz_mapping_items);
	RNA_def_property_ui_text(prop, "Y Mapping", "");
	RNA_def_property_update(prop, 0, "rna_Texture_mapping_update");

	prop = RNA_def_property(srna, "mapping_z", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "projz");
	RNA_def_property_enum_items(prop, prop_xyz_mapping_items);
	RNA_def_property_ui_text(prop, "Z Mapping", "");
	RNA_def_property_update(prop, 0, "rna_Texture_mapping_update");

	prop = RNA_def_property(srna, "mapping", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_mapping_items);
	RNA_def_property_ui_text(prop, "Mapping", "");
	RNA_def_property_update(prop, 0, "rna_Texture_mapping_update");
}

static void rna_def_colormapping(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ColorMapping", NULL);
	RNA_def_struct_ui_text(srna, "Color Mapping", "Color mapping settings");

	prop = RNA_def_property(srna, "use_color_ramp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", COLORMAP_USE_RAMP);
	RNA_def_property_ui_text(prop, "Use Color Ramp", "Toggle color ramp operations");
	RNA_def_property_update(prop, 0, "rna_Color_mapping_update");

	prop = RNA_def_property(srna, "color_ramp", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "coba");
	RNA_def_property_struct_type(prop, "ColorRamp");
	RNA_def_property_ui_text(prop, "Color Ramp", "");
	RNA_def_property_update(prop, 0, "rna_Color_mapping_update");

	prop = RNA_def_property(srna, "brightness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "bright");
	RNA_def_property_range(prop, 0, 2);
	RNA_def_property_ui_text(prop, "Brightness", "Adjust the brightness of the texture");
	RNA_def_property_update(prop, 0, "rna_Color_mapping_update");

	prop = RNA_def_property(srna, "contrast", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 5);
	RNA_def_property_ui_text(prop, "Contrast", "Adjust the contrast of the texture");
	RNA_def_property_update(prop, 0, "rna_Color_mapping_update");

	prop = RNA_def_property(srna, "saturation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0, 2);
	RNA_def_property_ui_text(prop, "Saturation", "Adjust the saturation of colors in the texture");
	RNA_def_property_update(prop, 0, "rna_Color_mapping_update");

	prop = RNA_def_property(srna, "blend_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, blend_type_items);
	RNA_def_property_ui_text(prop, "Blend Type", "Mode used to mix with texture output color");
	RNA_def_property_update(prop, 0, "rna_Color_mapping_update");

	prop = RNA_def_property(srna, "blend_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Color", "Blend color to mix with texture output color");
	RNA_def_property_update(prop, 0, "rna_Color_mapping_update");

	prop = RNA_def_property(srna, "blend_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Blend Factor", "");
	RNA_def_property_update(prop, 0, "rna_Color_mapping_update");
}

static void rna_def_mtex(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem output_node_items[] = {
		{0, "DUMMY", 0, "Dummy", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "TextureSlot", NULL);
	RNA_def_struct_sdna(srna, "MTex");
	RNA_def_struct_ui_text(srna, "Texture Slot", "Texture slot defining the mapping and influence of a texture");
	RNA_def_struct_path_func(srna, "rna_TextureSlot_path");
	RNA_def_struct_ui_icon(srna, ICON_TEXTURE_DATA);

	prop = RNA_def_property(srna, "texture", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tex");
	RNA_def_property_struct_type(prop, "Texture");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_CONTEXT_UPDATE);
	RNA_def_property_ui_text(prop, "Texture", "Texture data-block used by this texture slot");
	RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING_LINKS, "rna_TextureSlot_update");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_TextureSlot_name_get", "rna_TextureSlot_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "Texture slot name");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

	/* mapping */
	prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "ofs");
	RNA_def_property_ui_range(prop, -10, 10, 10, RNA_TRANSLATION_PREC_DEFAULT);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_ui_text(prop, "Offset", "Fine tune of the texture mapping X, Y and Z locations");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

	prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "size");
	RNA_def_property_flag(prop, PROP_PROPORTIONAL | PROP_CONTEXT_UPDATE);
	RNA_def_property_ui_range(prop, -100, 100, 10, 2);
	RNA_def_property_ui_text(prop, "Size", "Set scaling for the texture's X, Y and Z sizes");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

	prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "r");
	RNA_def_property_array(prop, 3);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_ui_text(prop, "Color",
	                         "Default color for textures that don't return RGB or when RGB to intensity is enabled");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

	prop = RNA_def_property(srna, "blend_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "blendtype");
	RNA_def_property_enum_items(prop, blend_type_items);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_ui_text(prop, "Blend Type", "Mode used to apply the texture");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

	prop = RNA_def_property(srna, "use_stencil", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "texflag", MTEX_STENCIL);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_ui_text(prop, "Stencil", "Use this texture as a blending value on the next texture");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

	prop = RNA_def_property(srna, "invert", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "texflag", MTEX_NEGATIVE);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_ui_text(prop, "Negate", "Invert the values of the texture to reverse its effect");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

	prop = RNA_def_property(srna, "use_rgb_to_intensity", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "texflag", MTEX_RGBTOINT);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_ui_text(prop, "RGB to Intensity", "Convert texture RGB values to intensity (gray) values");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

	prop = RNA_def_property(srna, "default_value", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "def_var");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Default Value",
	                         "Value to use for Ref, Spec, Amb, Emit, Alpha, RayMir, TransLu and Hard");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

	prop = RNA_def_property(srna, "output_node", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "which_output");
	RNA_def_property_enum_items(prop, output_node_items);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_enum_funcs(prop, "rna_TextureSlot_output_node_get", NULL, "rna_TextureSlot_output_node_itemf");
	RNA_def_property_ui_text(prop, "Output Node", "Which output node to use, for node-based textures");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_update");
}

static void rna_def_filter_common(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "use_mipmap", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "imaflag", TEX_MIPMAP);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_ImageTexture_mipmap_set");
	RNA_def_property_ui_text(prop, "MIP Map", "Use auto-generated MIP maps for the image");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "use_mipmap_gauss", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "imaflag", TEX_GAUSS_MIP);
	RNA_def_property_ui_text(prop, "MIP Map Gaussian filter", "Use Gauss filter to sample down MIP maps");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "texfilter");
	RNA_def_property_enum_items(prop, texture_filter_items);
	RNA_def_property_ui_text(prop, "Filter", "Texture filter to use for sampling image");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "filter_lightprobes", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "afmax");
	RNA_def_property_range(prop, 1, 256);
	RNA_def_property_ui_text(prop, "Filter Probes",
	                         "Maximum number of samples (higher gives less blur at distant/oblique angles, "
	                         "but is also slower)");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "filter_eccentricity", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "afmax");
	RNA_def_property_range(prop, 1, 256);
	RNA_def_property_ui_text(prop, "Filter Eccentricity",
	                         "Maximum eccentricity (higher gives less blur at distant/oblique angles, "
	                         "but is also slower)");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "use_filter_size_min", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "imaflag", TEX_FILTER_MIN);
	RNA_def_property_ui_text(prop, "Minimum Filter Size", "Use Filter Size as a minimal filter value in pixels");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "filter_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "filtersize");
	RNA_def_property_range(prop, 0.1, 50.0);
	RNA_def_property_ui_range(prop, 0.1, 50.0, 1, 2);
	RNA_def_property_ui_text(prop, "Filter Size", "Multiply the filter size used by MIP Map and Interpolation");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static const EnumPropertyItem prop_noise_basis_items[] = {
	{TEX_BLENDER, "BLENDER_ORIGINAL", 0, "Blender Original",
	              "Noise algorithm - Blender original: Smooth interpolated noise"},
	{TEX_STDPERLIN, "ORIGINAL_PERLIN", 0, "Original Perlin",
	                "Noise algorithm - Original Perlin: Smooth interpolated noise"},
	{TEX_NEWPERLIN, "IMPROVED_PERLIN", 0, "Improved Perlin",
	                "Noise algorithm - Improved Perlin: Smooth interpolated noise"},
	{TEX_VORONOI_F1, "VORONOI_F1", 0, "Voronoi F1",
	                 "Noise algorithm - Voronoi F1: Returns distance to the closest feature point"},
	{TEX_VORONOI_F2, "VORONOI_F2", 0, "Voronoi F2",
	                 "Noise algorithm - Voronoi F2: Returns distance to the 2nd closest feature point"},
	{TEX_VORONOI_F3, "VORONOI_F3", 0, "Voronoi F3",
	                 "Noise algorithm - Voronoi F3: Returns distance to the 3rd closest feature point"},
	{TEX_VORONOI_F4, "VORONOI_F4", 0, "Voronoi F4",
	                 "Noise algorithm - Voronoi F4: Returns distance to the 4th closest feature point"},
	{TEX_VORONOI_F2F1, "VORONOI_F2_F1", 0, "Voronoi F2-F1", "Noise algorithm - Voronoi F1-F2"},
	{TEX_VORONOI_CRACKLE, "VORONOI_CRACKLE", 0, "Voronoi Crackle",
	                      "Noise algorithm - Voronoi Crackle: Voronoi tessellation with sharp edges"},
	{TEX_CELLNOISE, "CELL_NOISE", 0, "Cell Noise",
	                "Noise algorithm - Cell Noise: Square cell tessellation"},
	{0, NULL, 0, NULL, NULL}
};

static const EnumPropertyItem prop_noise_type[] = {
	{TEX_NOISESOFT, "SOFT_NOISE", 0, "Soft", "Generate soft noise (smooth transitions)"},
	{TEX_NOISEPERL, "HARD_NOISE", 0, "Hard", "Generate hard noise (sharp transitions)"},
	{0, NULL, 0, NULL, NULL}
};


static void rna_def_texture_clouds(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem prop_clouds_stype[] = {
		{TEX_DEFAULT, "GRAYSCALE", 0, "Grayscale", ""},
		{TEX_COLOR, "COLOR", 0, "Color", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "CloudsTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Clouds Texture", "Procedural noise texture");
	RNA_def_struct_sdna(srna, "Tex");

	prop = RNA_def_property(srna, "noise_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "noisesize");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 2, 10, 2);
	RNA_def_property_ui_text(prop, "Noise Size", "Scaling for noise input");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "noise_depth", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "noisedepth");
	RNA_def_property_range(prop, 0, 30);
	RNA_def_property_ui_range(prop, 0, 24, 0, 2);
	RNA_def_property_ui_text(prop, "Noise Depth", "Depth of the cloud calculation");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop = RNA_def_property(srna, "noise_basis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisebasis");
	RNA_def_property_enum_items(prop, prop_noise_basis_items);
	RNA_def_property_ui_text(prop, "Noise Basis", "Noise basis used for turbulence");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop = RNA_def_property(srna, "noise_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisetype");
	RNA_def_property_enum_items(prop, prop_noise_type);
	RNA_def_property_ui_text(prop, "Noise Type", "");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop = RNA_def_property(srna, "cloud_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "stype");
	RNA_def_property_enum_items(prop, prop_clouds_stype);
	RNA_def_property_ui_text(prop, "Color", "Determine whether Noise returns grayscale or RGB values");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop = RNA_def_property(srna, "nabla", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.001, 0.1);
	RNA_def_property_ui_range(prop, 0.001, 0.1, 1, 2);
	RNA_def_property_ui_text(prop, "Nabla", "Size of derivative offset used for calculating normal");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static void rna_def_texture_wood(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem prop_wood_stype[] = {
		{TEX_BAND, "BANDS", 0, "Bands", "Use standard wood texture in bands"},
		{TEX_RING, "RINGS", 0, "Rings", "Use wood texture in rings"},
		{TEX_BANDNOISE, "BANDNOISE", 0, "Band Noise", "Add noise to standard wood"},
		{TEX_RINGNOISE, "RINGNOISE", 0, "Ring Noise", "Add noise to rings"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem prop_wood_noisebasis2[] = {
		{TEX_SIN, "SIN", 0, "Sine", "Use a sine wave to produce bands"},
		{TEX_SAW, "SAW", 0, "Saw", "Use a saw wave to produce bands"},
		{TEX_TRI, "TRI", 0, "Tri", "Use a triangle wave to produce bands"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "WoodTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Wood Texture", "Procedural noise texture");
	RNA_def_struct_sdna(srna, "Tex");

	prop = RNA_def_property(srna, "noise_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "noisesize");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 2, 10, 2);
	RNA_def_property_ui_text(prop, "Noise Size", "Scaling for noise input");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "turbulence", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "turbul");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 200, 10, 2);
	RNA_def_property_ui_text(prop, "Turbulence", "Turbulence of the bandnoise and ringnoise types");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "noise_basis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisebasis");
	RNA_def_property_enum_items(prop, prop_noise_basis_items);
	RNA_def_property_ui_text(prop, "Noise Basis", "Noise basis used for turbulence");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop = RNA_def_property(srna, "noise_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisetype");
	RNA_def_property_enum_items(prop, prop_noise_type);
	RNA_def_property_ui_text(prop, "Noise Type", "");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop = RNA_def_property(srna, "wood_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "stype");
	RNA_def_property_enum_items(prop, prop_wood_stype);
	RNA_def_property_ui_text(prop, "Pattern", "");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop = RNA_def_property(srna, "noise_basis_2", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisebasis2");
	RNA_def_property_enum_items(prop, prop_wood_noisebasis2);
	RNA_def_property_ui_text(prop, "Noise Basis 2", "");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop = RNA_def_property(srna, "nabla", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.001, 0.1);
	RNA_def_property_ui_range(prop, 0.001, 0.1, 1, 2);
	RNA_def_property_ui_text(prop, "Nabla", "Size of derivative offset used for calculating normal");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

}

static void rna_def_texture_marble(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem prop_marble_stype[] = {
		{TEX_SOFT, "SOFT", 0, "Soft", "Use soft marble"},
		{TEX_SHARP, "SHARP", 0, "Sharp", "Use more clearly defined marble"},
		{TEX_SHARPER, "SHARPER", 0, "Sharper", "Use very clearly defined marble"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem prop_marble_noisebasis2[] = {
		{TEX_SIN, "SIN", 0, "Sin", "Use a sine wave to produce bands"},
		{TEX_SAW, "SAW", 0, "Saw", "Use a saw wave to produce bands"},
		{TEX_TRI, "TRI", 0, "Tri", "Use a triangle wave to produce bands"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "MarbleTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Marble Texture", "Procedural noise texture");
	RNA_def_struct_sdna(srna, "Tex");

	prop = RNA_def_property(srna, "noise_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "noisesize");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 2, 10, 2);
	RNA_def_property_ui_text(prop, "Noise Size", "Scaling for noise input");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "turbulence", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "turbul");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 200, 10, 2);
	RNA_def_property_ui_text(prop, "Turbulence", "Turbulence of the bandnoise and ringnoise types");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "noise_depth", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "noisedepth");
	RNA_def_property_range(prop, 0, 30);
	RNA_def_property_ui_range(prop, 0, 24, 0, 2);
	RNA_def_property_ui_text(prop, "Noise Depth", "Depth of the cloud calculation");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "noise_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisetype");
	RNA_def_property_enum_items(prop, prop_noise_type);
	RNA_def_property_ui_text(prop, "Noise Type", "");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop = RNA_def_property(srna, "marble_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "stype");
	RNA_def_property_enum_items(prop, prop_marble_stype);
	RNA_def_property_ui_text(prop, "Pattern", "");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop = RNA_def_property(srna, "noise_basis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisebasis");
	RNA_def_property_enum_items(prop, prop_noise_basis_items);
	RNA_def_property_ui_text(prop, "Noise Basis", "Noise basis used for turbulence");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop = RNA_def_property(srna, "noise_basis_2", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisebasis2");
	RNA_def_property_enum_items(prop, prop_marble_noisebasis2);
	RNA_def_property_ui_text(prop, "Noise Basis 2", "");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop = RNA_def_property(srna, "nabla", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.001, 0.1);
	RNA_def_property_ui_range(prop, 0.001, 0.1, 1, 2);
	RNA_def_property_ui_text(prop, "Nabla", "Size of derivative offset used for calculating normal");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

}

static void rna_def_texture_magic(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MagicTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Magic Texture", "Procedural noise texture");
	RNA_def_struct_sdna(srna, "Tex");

	prop = RNA_def_property(srna, "turbulence", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "turbul");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 200, 10, 2);
	RNA_def_property_ui_text(prop, "Turbulence", "Turbulence of the noise");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "noise_depth", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "noisedepth");
	RNA_def_property_range(prop, 0, 30);
	RNA_def_property_ui_range(prop, 0, 24, 0, 2);
	RNA_def_property_ui_text(prop, "Noise Depth", "Depth of the noise");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static void rna_def_texture_blend(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem prop_blend_progression[] = {
		{TEX_LIN, "LINEAR", 0, "Linear", "Create a linear progression"},
		{TEX_QUAD, "QUADRATIC", 0, "Quadratic", "Create a quadratic progression"},
		{TEX_EASE, "EASING", 0, "Easing", "Create a progression easing from one step to the next"},
		{TEX_DIAG, "DIAGONAL", 0, "Diagonal", "Create a diagonal progression"},
		{TEX_SPHERE, "SPHERICAL", 0, "Spherical", "Create a spherical progression"},
		{TEX_HALO, "QUADRATIC_SPHERE", 0, "Quadratic sphere",
		           "Create a quadratic progression in the shape of a sphere"},
		{TEX_RAD, "RADIAL", 0, "Radial", "Create a radial progression"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem prop_flip_axis_items[] = {
		{0, "HORIZONTAL", 0, "Horizontal", "No flipping"},
		{TEX_FLIPBLEND, "VERTICAL", 0, "Vertical", "Flip the texture's X and Y axis"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "BlendTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Blend Texture", "Procedural color blending texture");
	RNA_def_struct_sdna(srna, "Tex");

	prop = RNA_def_property(srna, "progression", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "stype");
	RNA_def_property_enum_items(prop, prop_blend_progression);
	RNA_def_property_ui_text(prop, "Progression", "Style of the color blending");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop = RNA_def_property(srna, "use_flip_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, prop_flip_axis_items);
	RNA_def_property_ui_text(prop, "Flip Axis", "Flip the texture's X and Y axis");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

}

static void rna_def_texture_stucci(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem prop_stucci_stype[] = {
		{TEX_PLASTIC, "PLASTIC", 0, "Plastic", "Use standard stucci"},
		{TEX_WALLIN, "WALL_IN", 0, "Wall in", "Create Dimples"},
		{TEX_WALLOUT, "WALL_OUT", 0, "Wall out", "Create Ridges"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "StucciTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Stucci Texture", "Procedural noise texture");
	RNA_def_struct_sdna(srna, "Tex");

	prop = RNA_def_property(srna, "turbulence", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "turbul");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 200, 10, 2);
	RNA_def_property_ui_text(prop, "Turbulence", "Turbulence of the noise");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "noise_basis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisebasis");
	RNA_def_property_enum_items(prop, prop_noise_basis_items);
	RNA_def_property_ui_text(prop, "Noise Basis", "Noise basis used for turbulence");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "noise_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "noisesize");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 2, 10, 2);
	RNA_def_property_ui_text(prop, "Noise Size", "Scaling for noise input");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "noise_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisetype");
	RNA_def_property_enum_items(prop, prop_noise_type);
	RNA_def_property_ui_text(prop, "Noise Type", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "stucci_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "stype");
	RNA_def_property_enum_items(prop, prop_stucci_stype);
	RNA_def_property_ui_text(prop, "Pattern", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static void rna_def_texture_noise(BlenderRNA *brna)
{
	StructRNA *srna;

	srna = RNA_def_struct(brna, "NoiseTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Noise Texture", "Procedural noise texture");
	RNA_def_struct_sdna(srna, "Tex");
}

static void rna_def_texture_image(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem prop_image_extension[] = {
		{TEX_EXTEND, "EXTEND", 0, "Extend", "Extend by repeating edge pixels of the image"},
		{TEX_CLIP, "CLIP", 0, "Clip", "Clip to image size and set exterior pixels as transparent"},
		{TEX_CLIPCUBE, "CLIP_CUBE", 0, "Clip Cube",
		               "Clip to cubic-shaped area around the image and set exterior pixels as transparent"},
		{TEX_REPEAT, "REPEAT", 0, "Repeat", "Cause the image to repeat horizontally and vertically"},
		{TEX_CHECKER, "CHECKER", 0, "Checker", "Cause the image to repeat in checker board pattern"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "ImageTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Image Texture", "");
	RNA_def_struct_sdna(srna, "Tex");

	prop = RNA_def_property(srna, "use_interpolation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "imaflag", TEX_INTERPOL);
	RNA_def_property_ui_text(prop, "Interpolation", "Interpolate pixels using selected filter");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	/* XXX: I think flip_axis should be a generic Texture property, enabled for all the texture types */
	prop = RNA_def_property(srna, "use_flip_axis", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "imaflag", TEX_IMAROT);
	RNA_def_property_ui_text(prop, "Flip Axis", "Flip the texture's X and Y axis");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "use_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "imaflag", TEX_USEALPHA);
	RNA_def_property_ui_text(prop, "Use Alpha", "Use the alpha channel information in the image");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "use_calculate_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "imaflag", TEX_CALCALPHA);
	RNA_def_property_ui_text(prop, "Calculate Alpha", "Calculate an alpha channel based on RGB values in the image");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "invert_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEX_NEGALPHA);
	RNA_def_property_ui_text(prop, "Invert Alpha", "Invert all the alpha values in the image");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	rna_def_filter_common(srna);

	prop = RNA_def_property(srna, "extension", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "extend");
	RNA_def_property_enum_items(prop, prop_image_extension);
	RNA_def_property_ui_text(prop, "Extension", "How the image is extrapolated past its original bounds");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "repeat_x", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "xrepeat");
	RNA_def_property_range(prop, 1, 512);
	RNA_def_property_ui_text(prop, "Repeat X", "Repetition multiplier in the X direction");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "repeat_y", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "yrepeat");
	RNA_def_property_range(prop, 1, 512);
	RNA_def_property_ui_text(prop, "Repeat Y", "Repetition multiplier in the Y direction");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "use_mirror_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEX_REPEAT_XMIR);
	RNA_def_property_ui_text(prop, "Mirror X", "Mirror the image repetition on the X direction");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "use_mirror_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEX_REPEAT_YMIR);
	RNA_def_property_ui_text(prop, "Mirror Y", "Mirror the image repetition on the Y direction");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "use_checker_odd", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEX_CHECKER_ODD);
	RNA_def_property_ui_text(prop, "Checker Odd", "Odd checker tiles");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "use_checker_even", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEX_CHECKER_EVEN);
	RNA_def_property_ui_text(prop, "Checker Even", "Even checker tiles");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "checker_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "checkerdist");
	RNA_def_property_range(prop, 0.0, 0.99);
	RNA_def_property_ui_range(prop, 0.0, 0.99, 0.1, 2);
	RNA_def_property_ui_text(prop, "Checker Distance", "Distance between checker tiles");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

#if 0

	/* XXX: did this as an array, but needs better descriptions than "1 2 3 4"
	 * perhaps a new subtype could be added?
	 * --I actually used single values for this, maybe change later with a RNA_Rect thing? */
	prop = RNA_def_property(srna, "crop_rectangle", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cropxmin");
	RNA_def_property_array(prop, 4);
	RNA_def_property_range(prop, -10, 10);
	RNA_def_property_ui_text(prop, "Crop Rectangle", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

#endif

	prop = RNA_def_property(srna, "crop_min_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cropxmin");
	RNA_def_property_range(prop, -10.0, 10.0);
	RNA_def_property_ui_range(prop, -10.0, 10.0, 1, 2);
	RNA_def_property_ui_text(prop, "Crop Minimum X", "Minimum X value to crop the image");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "crop_min_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cropymin");
	RNA_def_property_range(prop, -10.0, 10.0);
	RNA_def_property_ui_range(prop, -10.0, 10.0, 1, 2);
	RNA_def_property_ui_text(prop, "Crop Minimum Y", "Minimum Y value to crop the image");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "crop_max_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cropxmax");
	RNA_def_property_range(prop, -10.0, 10.0);
	RNA_def_property_ui_range(prop, -10.0, 10.0, 1, 2);
	RNA_def_property_ui_text(prop, "Crop Maximum X", "Maximum X value to crop the image");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "crop_max_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cropymax");
	RNA_def_property_range(prop, -10.0, 10.0);
	RNA_def_property_ui_range(prop, -10.0, 10.0, 1, 2);
	RNA_def_property_ui_text(prop, "Crop Maximum Y", "Maximum Y value to crop the image");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ima");
	RNA_def_property_struct_type(prop, "Image");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Image", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "image_user", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "iuser");
	RNA_def_property_ui_text(prop, "Image User",
	                         "Parameters defining which layer, pass and frame of the image is displayed");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	/* Normal Map */
	prop = RNA_def_property(srna, "use_normal_map", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "imaflag", TEX_NORMALMAP);
	RNA_def_property_ui_text(prop, "Normal Map", "Use image RGB values for normal mapping");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static void rna_def_texture_musgrave(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem prop_musgrave_type[] = {
		{TEX_MFRACTAL, "MULTIFRACTAL", 0, "Multifractal", "Use Perlin noise as a basis"},
		{TEX_RIDGEDMF, "RIDGED_MULTIFRACTAL", 0, "Ridged Multifractal",
		               "Use Perlin noise with inflection as a basis"},
		{TEX_HYBRIDMF, "HYBRID_MULTIFRACTAL", 0, "Hybrid Multifractal",
		               "Use Perlin noise as a basis, with extended controls"},
		{TEX_FBM, "FBM", 0, "fBM", "Fractal Brownian Motion, use Brownian noise as a basis"},
		{TEX_HTERRAIN, "HETERO_TERRAIN", 0, "Hetero Terrain", "Similar to multifractal"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "MusgraveTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Musgrave", "Procedural musgrave texture");
	RNA_def_struct_sdna(srna, "Tex");

	prop = RNA_def_property(srna, "musgrave_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "stype");
	RNA_def_property_enum_items(prop, prop_musgrave_type);
	RNA_def_property_ui_text(prop, "Type", "Fractal noise algorithm");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "dimension_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mg_H");
	RNA_def_property_range(prop, 0.0001, 2);
	RNA_def_property_ui_text(prop, "Highest Dimension", "Highest fractal dimension");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "lacunarity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mg_lacunarity");
	RNA_def_property_range(prop, 0, 6);
	RNA_def_property_ui_text(prop, "Lacunarity", "Gap between successive frequencies");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "octaves", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mg_octaves");
	RNA_def_property_range(prop, 0, 8);
	RNA_def_property_ui_text(prop, "Octaves", "Number of frequencies used");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mg_offset");
	RNA_def_property_range(prop, 0, 6);
	RNA_def_property_ui_text(prop, "Offset", "The fractal offset");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "gain", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mg_gain");
	RNA_def_property_range(prop, 0, 6);
	RNA_def_property_ui_text(prop, "Gain", "The gain multiplier");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "noise_intensity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ns_outscale");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Noise Intensity", "Intensity of the noise");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "noise_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "noisesize");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 2, 10, 2);
	RNA_def_property_ui_text(prop, "Noise Size", "Scaling for noise input");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "noise_basis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisebasis");
	RNA_def_property_enum_items(prop, prop_noise_basis_items);
	RNA_def_property_ui_text(prop, "Noise Basis", "Noise basis used for turbulence");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "nabla", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.001, 0.1);
	RNA_def_property_ui_range(prop, 0.001, 0.1, 1, 2);
	RNA_def_property_ui_text(prop, "Nabla", "Size of derivative offset used for calculating normal");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static void rna_def_texture_voronoi(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static const EnumPropertyItem prop_distance_metric_items[] = {
		{TEX_DISTANCE, "DISTANCE", 0, "Actual Distance", "sqrt(x*x+y*y+z*z)"},
		{TEX_DISTANCE_SQUARED, "DISTANCE_SQUARED", 0, "Distance Squared", "(x*x+y*y+z*z)"},
		{TEX_MANHATTAN, "MANHATTAN", 0, "Manhattan",
		                "The length of the distance in axial directions"},
		{TEX_CHEBYCHEV, "CHEBYCHEV", 0, "Chebychev",
		                "The length of the longest Axial journey"},
		{TEX_MINKOVSKY_HALF, "MINKOVSKY_HALF", 0, "Minkowski 1/2",
		                     "Set Minkowski variable to 0.5"},
		{TEX_MINKOVSKY_FOUR, "MINKOVSKY_FOUR", 0, "Minkowski 4",
		                     "Set Minkowski variable to 4"},
		{TEX_MINKOVSKY, "MINKOVSKY", 0, "Minkowski",
		                "Use the Minkowski function to calculate distance "
		                "(exponent value determines the shape of the boundaries)"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem prop_coloring_items[] = {
		/* XXX: OK names / descriptions? */
		{TEX_INTENSITY, "INTENSITY", 0, "Intensity", "Only calculate intensity"},
		{TEX_COL1, "POSITION", 0, "Position", "Color cells by position"},
		{TEX_COL2, "POSITION_OUTLINE", 0, "Position and Outline", "Use position plus an outline based on F2-F1"},
		{TEX_COL3, "POSITION_OUTLINE_INTENSITY", 0, "Position, Outline, and Intensity",
		           "Multiply position and outline by intensity"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "VoronoiTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Voronoi", "Procedural voronoi texture");
	RNA_def_struct_sdna(srna, "Tex");

	prop = RNA_def_property(srna, "weight_1", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "vn_w1");
	RNA_def_property_range(prop, -2, 2);
	RNA_def_property_ui_text(prop, "Weight 1", "Voronoi feature weight 1");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "weight_2", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "vn_w2");
	RNA_def_property_range(prop, -2, 2);
	RNA_def_property_ui_text(prop, "Weight 2", "Voronoi feature weight 2");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "weight_3", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "vn_w3");
	RNA_def_property_range(prop, -2, 2);
	RNA_def_property_ui_text(prop, "Weight 3", "Voronoi feature weight 3");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "weight_4", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "vn_w4");
	RNA_def_property_range(prop, -2, 2);
	RNA_def_property_ui_text(prop, "Weight 4", "Voronoi feature weight 4");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "minkovsky_exponent", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "vn_mexp");
	RNA_def_property_range(prop, 0.01, 10);
	RNA_def_property_ui_text(prop, "Minkowski Exponent", "Minkowski exponent");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "distance_metric", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "vn_distm");
	RNA_def_property_enum_items(prop, prop_distance_metric_items);
	RNA_def_property_ui_text(prop, "Distance Metric",
	                         "Algorithm used to calculate distance of sample points to feature points");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "color_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "vn_coltype");
	RNA_def_property_enum_items(prop, prop_coloring_items);
	RNA_def_property_ui_text(prop, "Coloring", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "noise_intensity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ns_outscale");
	RNA_def_property_range(prop, 0.01, 10);
	RNA_def_property_ui_text(prop, "Noise Intensity", "Scales the intensity of the noise");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "noise_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "noisesize");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 2, 10, 2);
	RNA_def_property_ui_text(prop, "Noise Size", "Scaling for noise input");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "nabla", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.001, 0.1);
	RNA_def_property_ui_range(prop, 0.001, 0.1, 1, 2);
	RNA_def_property_ui_text(prop, "Nabla", "Size of derivative offset used for calculating normal");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static void rna_def_texture_distorted_noise(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "DistortedNoiseTexture", "Texture");
	RNA_def_struct_ui_text(srna, "Distorted Noise", "Procedural distorted noise texture");
	RNA_def_struct_sdna(srna, "Tex");

	prop = RNA_def_property(srna, "distortion", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dist_amount");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Distortion Amount", "Amount of distortion");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "noise_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "noisesize");
	RNA_def_property_range(prop, 0.0001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0001, 2, 10, 2);
	RNA_def_property_ui_text(prop, "Noise Size", "Scaling for noise input");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "noise_basis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisebasis2");
	RNA_def_property_enum_items(prop, prop_noise_basis_items);
	RNA_def_property_ui_text(prop, "Noise Basis", "Noise basis used for turbulence");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop = RNA_def_property(srna, "noise_distortion", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisebasis");
	RNA_def_property_enum_items(prop, prop_noise_basis_items);
	RNA_def_property_ui_text(prop, "Noise Distortion", "Noise basis for the distortion");
	RNA_def_property_update(prop, 0, "rna_Texture_nodes_update");

	prop = RNA_def_property(srna, "nabla", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.001, 0.1);
	RNA_def_property_ui_range(prop, 0.001, 0.1, 1, 2);
	RNA_def_property_ui_text(prop, "Nabla", "Size of derivative offset used for calculating normal");
	RNA_def_property_update(prop, 0, "rna_Texture_update");
}

static void rna_def_texture(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Texture", "ID");
	RNA_def_struct_sdna(srna, "Tex");
	RNA_def_struct_ui_text(srna, "Texture", "Texture data-block used by materials, lights, worlds and brushes");
	RNA_def_struct_ui_icon(srna, ICON_TEXTURE_DATA);
	RNA_def_struct_refine_func(srna, "rna_Texture_refine");

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	/*RNA_def_property_clear_flag(prop, PROP_EDITABLE); */
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, rna_enum_texture_type_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_Texture_type_set", NULL);
	RNA_def_property_ui_text(prop, "Type", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "use_clamp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", TEX_NO_CLAMP);
	RNA_def_property_ui_text(prop, "Clamp", "Set negative texture RGB and intensity values to zero, for some uses like displacement this option can be disabled to get the full range");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "use_color_ramp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEX_COLORBAND);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Texture_use_color_ramp_set");
	RNA_def_property_ui_text(prop, "Use Color Ramp", "Toggle color ramp operations");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "color_ramp", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "coba");
	RNA_def_property_struct_type(prop, "ColorRamp");
	RNA_def_property_ui_text(prop, "Color Ramp", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "intensity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "bright");
	RNA_def_property_range(prop, 0, 2);
	RNA_def_property_ui_text(prop, "Brightness", "Adjust the brightness of the texture");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "contrast", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 5);
	RNA_def_property_ui_text(prop, "Contrast", "Adjust the contrast of the texture");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "saturation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0, 2);
	RNA_def_property_ui_text(prop, "Saturation", "Adjust the saturation of colors in the texture");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	/* RGB Factor */
	prop = RNA_def_property(srna, "factor_red", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rfac");
	RNA_def_property_range(prop, 0, 2);
	RNA_def_property_ui_text(prop, "Factor Red", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "factor_green", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "gfac");
	RNA_def_property_range(prop, 0, 2);
	RNA_def_property_ui_text(prop, "Factor Green", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	prop = RNA_def_property(srna, "factor_blue", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "bfac");
	RNA_def_property_range(prop, 0, 2);
	RNA_def_property_ui_text(prop, "Factor Blue", "");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	/* Alpha for preview render */
	prop = RNA_def_property(srna, "use_preview_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEX_PRV_ALPHA);
	RNA_def_property_ui_text(prop, "Show Alpha", "Show Alpha in Preview Render");
	RNA_def_property_update(prop, 0, "rna_Texture_update");

	/* nodetree */
	prop = RNA_def_property(srna, "use_nodes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "use_nodes", 1);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_ui_text(prop, "Use Nodes", "Make this a node-based texture");
	RNA_def_property_update(prop, 0, "rna_Texture_use_nodes_update");

	prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
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
	rna_def_texture_musgrave(brna);
	rna_def_texture_voronoi(brna);
	rna_def_texture_distorted_noise(brna);
	/* XXX add more types here .. */

	RNA_api_texture(srna);
}

void RNA_def_texture(BlenderRNA *brna)
{
	rna_def_texture(brna);
	rna_def_mtex(brna);
	rna_def_texmapping(brna);
	rna_def_colormapping(brna);
}

#endif
