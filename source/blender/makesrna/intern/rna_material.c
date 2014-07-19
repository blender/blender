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
 * Contributor(s): Blender Foundation (2008), Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_material.c
 *  \ingroup RNA
 */

#include <float.h>
#include <stdlib.h>

#include "DNA_material_types.h"
#include "DNA_texture_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_api.h"
#include "WM_types.h"

static EnumPropertyItem prop_texture_coordinates_items[] = {
	{TEXCO_GLOB, "GLOBAL", 0, "Global", "Use global coordinates for the texture coordinates"},
	{TEXCO_OBJECT, "OBJECT", 0, "Object", "Use linked object's coordinates for texture coordinates"},
	{TEXCO_UV, "UV", 0, "UV", "Use UV coordinates for texture coordinates"},
	{TEXCO_ORCO, "ORCO", 0, "Generated", "Use the original undeformed coordinates of the object"},
	{TEXCO_STRAND, "STRAND", 0, "Strand / Particle",
	               "Use normalized strand texture coordinate (1D) or particle age (X) and trail position (Y)"},
	{TEXCO_WINDOW, "WINDOW", 0, "Window", "Use screen coordinates as texture coordinates"},
	{TEXCO_NORM, "NORMAL", 0, "Normal", "Use normal vector as texture coordinates"},
	{TEXCO_REFL, "REFLECTION", 0, "Reflection", "Use reflection vector as texture coordinates"},
	{TEXCO_STRESS, "STRESS", 0, "Stress",
	               "Use the difference of edge lengths compared to original coordinates of the mesh"},
	{TEXCO_TANGENT, "TANGENT", 0, "Tangent", "Use the optional tangent vector as texture coordinates"},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem ramp_blend_items[] = {
	{MA_RAMP_BLEND, "MIX", 0, "Mix", ""},
	{MA_RAMP_ADD, "ADD", 0, "Add", ""},
	{MA_RAMP_MULT, "MULTIPLY", 0, "Multiply", ""},
	{MA_RAMP_SUB, "SUBTRACT", 0, "Subtract", ""},
	{MA_RAMP_SCREEN, "SCREEN", 0, "Screen", ""},
	{MA_RAMP_DIV, "DIVIDE", 0, "Divide", ""},
	{MA_RAMP_DIFF, "DIFFERENCE", 0, "Difference", ""},
	{MA_RAMP_DARK, "DARKEN", 0, "Darken", ""},
	{MA_RAMP_LIGHT, "LIGHTEN", 0, "Lighten", ""},
	{MA_RAMP_OVERLAY, "OVERLAY", 0, "Overlay", ""},
	{MA_RAMP_DODGE, "DODGE", 0, "Dodge", ""},
	{MA_RAMP_BURN, "BURN", 0, "Burn", ""},
	{MA_RAMP_HUE, "HUE", 0, "Hue", ""},
	{MA_RAMP_SAT, "SATURATION", 0, "Saturation", ""},
	{MA_RAMP_VAL, "VALUE", 0, "Value", ""},
	{MA_RAMP_COLOR, "COLOR", 0, "Color", ""},
	{MA_RAMP_SOFT, "SOFT_LIGHT", 0, "Soft Light", ""},
	{MA_RAMP_LINEAR, "LINEAR_LIGHT", 0, "Linear Light", ""},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_texture.h"
#include "BKE_node.h"
#include "BKE_paint.h"

#include "ED_node.h"

static void rna_Material_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Material *ma = ptr->id.data;

	DAG_id_tag_update(&ma->id, 0);
	WM_main_add_notifier(NC_MATERIAL | ND_SHADING, ma);
}

static void rna_Material_update_previews(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Material *ma = ptr->id.data;
	
	if (ma->nodetree)
		BKE_node_preview_clear_tree(ma->nodetree);
		
	WM_main_add_notifier(NC_MATERIAL | ND_SHADING_PREVIEW, ma);
}

static void rna_Material_draw_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Material *ma = ptr->id.data;

	DAG_id_tag_update(&ma->id, 0);
	WM_main_add_notifier(NC_MATERIAL | ND_SHADING_DRAW, ma);
}

static PointerRNA rna_Material_mirror_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_MaterialRaytraceMirror, ptr->id.data);
}

static PointerRNA rna_Material_transp_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_MaterialRaytraceTransparency, ptr->id.data);
}

static PointerRNA rna_Material_halo_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_MaterialHalo, ptr->id.data);
}

static PointerRNA rna_Material_sss_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_MaterialSubsurfaceScattering, ptr->id.data);
}

static PointerRNA rna_Material_strand_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_MaterialStrand, ptr->id.data);
}

static PointerRNA rna_Material_physics_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_MaterialPhysics, ptr->id.data);
}

static void rna_Material_type_set(PointerRNA *ptr, int value)
{
	Material *ma = (Material *)ptr->data;

	if (ma->material_type == MA_TYPE_HALO && value != MA_TYPE_HALO)
		ma->mode &= ~(MA_STAR | MA_HALO_XALPHA | MA_ZINV | MA_ENV);

	ma->material_type = value;
}

static void rna_Material_mtex_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Material *ma = (Material *)ptr->data;
	rna_iterator_array_begin(iter, (void *)ma->mtex, sizeof(MTex *), MAX_MTEX, 0, NULL);
}

static PointerRNA rna_Material_active_texture_get(PointerRNA *ptr)
{
	Material *ma = (Material *)ptr->data;
	Tex *tex;

	tex = give_current_material_texture(ma);
	return rna_pointer_inherit_refine(ptr, &RNA_Texture, tex);
}

static void rna_Material_active_texture_set(PointerRNA *ptr, PointerRNA value)
{
	Material *ma = (Material *)ptr->data;

	set_current_material_texture(ma, value.data);
}

static int rna_Material_active_texture_editable(PointerRNA *ptr)
{
	Material *ma = (Material *)ptr->id.data;

	return has_current_material_texture(ma);
}

static PointerRNA rna_Material_active_node_material_get(PointerRNA *ptr)
{
	Material *ma = give_node_material((Material *)ptr->data);
	return rna_pointer_inherit_refine(ptr, &RNA_Material, ma);
}

static void rna_Material_active_node_material_set(PointerRNA *ptr, PointerRNA value)
{
	Material *ma = (Material *)ptr->data;
	Material *ma_act = value.data;

	nodeSetActiveID(ma->nodetree, ID_MA, &ma_act->id);
}

static void rna_MaterialStrand_start_size_range(PointerRNA *ptr, float *min, float *max,
                                                float *UNUSED(softmin), float *UNUSED(softmax))
{
	Material *ma = (Material *)ptr->id.data;

	if (ma->mode & MA_STR_B_UNITS) {
		*min = 0.0001f;
		*max = 2.0f;
	}
	else {
		*min = 0.25f;
		*max = 20.0f;
	}
}

static void rna_MaterialStrand_end_size_range(PointerRNA *ptr, float *min, float *max,
                                              float *UNUSED(softmin), float *UNUSED(softmax))
{
	Material *ma = (Material *)ptr->id.data;

	if (ma->mode & MA_STR_B_UNITS) {
		*min = 0.0001f;
		*max = 1.0f;
	}
	else {
		*min = 0.25f;
		*max = 10.0f;
	}
}

static int rna_MaterialTextureSlot_use_get(PointerRNA *ptr)
{
	Material *ma = (Material *)ptr->id.data;
	MTex *mtex = (MTex *)ptr->data;
	int a;

	for (a = 0; a < MAX_MTEX; a++)
		if (ma->mtex[a] == mtex)
			return (ma->septex & (1 << a)) == 0;
	
	return 0;
}

static void rna_MaterialTextureSlot_use_set(PointerRNA *ptr, int value)
{
	Material *ma = (Material *)ptr->id.data;
	MTex *mtex = (MTex *)ptr->data;
	int a;

	for (a = 0; a < MAX_MTEX; a++) {
		if (ma->mtex[a] == mtex) {
			if (value)
				ma->septex &= ~(1 << a);
			else
				ma->septex |= (1 << a);
		}
	}
}

static void rna_Material_use_diffuse_ramp_set(PointerRNA *ptr, int value)
{
	Material *ma = (Material *)ptr->data;

	if (value) ma->mode |= MA_RAMP_COL;
	else ma->mode &= ~MA_RAMP_COL;

	if ((ma->mode & MA_RAMP_COL) && ma->ramp_col == NULL)
		ma->ramp_col = add_colorband(false);
}

static void rna_Material_use_specular_ramp_set(PointerRNA *ptr, int value)
{
	Material *ma = (Material *)ptr->data;

	if (value) ma->mode |= MA_RAMP_SPEC;
	else ma->mode &= ~MA_RAMP_SPEC;

	if ((ma->mode & MA_RAMP_SPEC) && ma->ramp_spec == NULL)
		ma->ramp_spec = add_colorband(false);
}

static void rna_Material_use_nodes_update(bContext *C, PointerRNA *ptr)
{
	Material *ma = (Material *)ptr->data;

	if (ma->use_nodes && ma->nodetree == NULL)
		ED_node_shader_default(C, &ma->id);
	
	rna_Material_update(CTX_data_main(C), CTX_data_scene(C), ptr);
}

static EnumPropertyItem *rna_Material_texture_coordinates_itemf(bContext *UNUSED(C), PointerRNA *ptr,
                                                                PropertyRNA *UNUSED(prop), bool *r_free)
{
	Material *ma = (Material *)ptr->id.data;
	EnumPropertyItem *item = NULL;
	int totitem = 0;
	
	RNA_enum_items_add_value(&item, &totitem, prop_texture_coordinates_items, TEXCO_GLOB);
	RNA_enum_items_add_value(&item, &totitem, prop_texture_coordinates_items, TEXCO_OBJECT);
	RNA_enum_items_add_value(&item, &totitem, prop_texture_coordinates_items, TEXCO_ORCO);
	
	if (ma->material_type == MA_TYPE_VOLUME) {
		
	}
	else if (ELEM(ma->material_type, MA_TYPE_SURFACE, MA_TYPE_HALO, MA_TYPE_WIRE)) {
		RNA_enum_items_add_value(&item, &totitem, prop_texture_coordinates_items, TEXCO_UV);
		RNA_enum_items_add_value(&item, &totitem, prop_texture_coordinates_items, TEXCO_STRAND);
		RNA_enum_items_add_value(&item, &totitem, prop_texture_coordinates_items, TEXCO_WINDOW);
		RNA_enum_items_add_value(&item, &totitem, prop_texture_coordinates_items, TEXCO_NORM);
		RNA_enum_items_add_value(&item, &totitem, prop_texture_coordinates_items, TEXCO_REFL);
		RNA_enum_items_add_value(&item, &totitem, prop_texture_coordinates_items, TEXCO_STRESS);
		RNA_enum_items_add_value(&item, &totitem, prop_texture_coordinates_items, TEXCO_TANGENT);
	}
	
	RNA_enum_item_end(&item, &totitem);
	*r_free = true;
	
	return item;
}

MTex *rna_mtex_texture_slots_add(ID *self_id, struct bContext *C, ReportList *reports)
{
	MTex *mtex = add_mtex_id(self_id, -1);
	if (mtex == NULL) {
		BKE_reportf(reports, RPT_ERROR, "Maximum number of textures added %d", MAX_MTEX);
		return NULL;
	}

	/* for redraw only */
	WM_event_add_notifier(C, NC_TEXTURE, CTX_data_scene(C));

	return mtex;
}

MTex *rna_mtex_texture_slots_create(ID *self_id, struct bContext *C, ReportList *reports, int index)
{
	MTex *mtex;

	if (index < 0 || index >= MAX_MTEX) {
		BKE_reportf(reports, RPT_ERROR, "Index %d is invalid", index);
		return NULL;
	}

	mtex = add_mtex_id(self_id, index);

	/* for redraw only */
	WM_event_add_notifier(C, NC_TEXTURE, CTX_data_scene(C));

	return mtex;
}

void rna_mtex_texture_slots_clear(ID *self_id, struct bContext *C, ReportList *reports, int index)
{
	MTex **mtex_ar;
	short act;

	give_active_mtex(self_id, &mtex_ar, &act);

	if (mtex_ar == NULL) {
		BKE_report(reports, RPT_ERROR, "Mtex not found for this type");
		return;
	}
	
	if (index < 0 || index >= MAX_MTEX) {
		BKE_reportf(reports, RPT_ERROR, "Index %d is invalid", index);
		return;
	}

	if (mtex_ar[index]) {
		id_us_min((ID *)mtex_ar[index]->tex);
		MEM_freeN(mtex_ar[index]);
		mtex_ar[index] = NULL;
		DAG_id_tag_update(self_id, 0);
	}

	/* for redraw only */
	WM_event_add_notifier(C, NC_TEXTURE, CTX_data_scene(C));
}

#else

static void rna_def_material_mtex(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_mapping_items[] = {
		{MTEX_FLAT, "FLAT", 0, "Flat", "Map X and Y coordinates directly"},
		{MTEX_CUBE, "CUBE", 0, "Cube", "Map using the normal vector"},
		{MTEX_TUBE, "TUBE", 0, "Tube", "Map with Z as central axis"},
		{MTEX_SPHERE, "SPHERE", 0, "Sphere", "Map with Z as central axis"},
		{0, NULL, 0, NULL, NULL}
	};
		
	static EnumPropertyItem prop_x_mapping_items[] = {
		{0, "NONE", 0, "None", ""},
		{1, "X", 0, "X", ""},
		{2, "Y", 0, "Y", ""},
		{3, "Z", 0, "Z", ""},
		{0, NULL, 0, NULL, NULL}
	};
		
	static EnumPropertyItem prop_y_mapping_items[] = {
		{0, "NONE", 0, "None", ""},
		{1, "X", 0, "X", ""},
		{2, "Y", 0, "Y", ""},
		{3, "Z", 0, "Z", ""},
		{0, NULL, 0, NULL, NULL}
	};
		
	static EnumPropertyItem prop_z_mapping_items[] = {
		{0, "NONE", 0, "None", ""},
		{1, "X", 0, "X", ""},
		{2, "Y", 0, "Y", ""},
		{3, "Z", 0, "Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_normal_map_space_items[] = {
		{MTEX_NSPACE_CAMERA, "CAMERA", 0, "Camera", ""},
		{MTEX_NSPACE_WORLD, "WORLD", 0, "World", ""},
		{MTEX_NSPACE_OBJECT, "OBJECT", 0, "Object", ""},
		{MTEX_NSPACE_TANGENT, "TANGENT", 0, "Tangent", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_bump_method_items[] = {
		{0, "BUMP_ORIGINAL", 0, "Original", ""},
		{MTEX_COMPAT_BUMP, "BUMP_COMPATIBLE", 0, "Compatible", ""},
		{MTEX_3TAP_BUMP, "BUMP_LOW_QUALITY", 0, "Low Quality", "Use 3 tap filtering"},
		{MTEX_5TAP_BUMP, "BUMP_MEDIUM_QUALITY", 0, "Medium Quality", "Use 5 tap filtering"},
		{MTEX_BICUBIC_BUMP, "BUMP_BEST_QUALITY", 0,
		 "Best Quality", "Use bicubic filtering (requires OpenGL 3.0+, it will fall back on "
		 "medium setting for other systems)"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_bump_space_items[] = {
		{0, "BUMP_VIEWSPACE", 0, "ViewSpace", ""},
		{MTEX_BUMP_OBJECTSPACE, "BUMP_OBJECTSPACE", 0, "ObjectSpace", ""},
		{MTEX_BUMP_TEXTURESPACE, "BUMP_TEXTURESPACE", 0, "TextureSpace", ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "MaterialTextureSlot", "TextureSlot");
	RNA_def_struct_sdna(srna, "MTex");
	RNA_def_struct_ui_text(srna, "Material Texture Slot", "Texture slot for textures in a Material datablock");

	prop = RNA_def_property(srna, "texture_coords", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "texco");
	RNA_def_property_enum_items(prop, prop_texture_coordinates_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Material_texture_coordinates_itemf");
	RNA_def_property_ui_text(prop, "Texture Coordinates", "");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "object");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object", "Object to use for mapping with Object texture coordinates");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "uvname");
	RNA_def_property_ui_text(prop, "UV Map", "UV map to use for mapping with UV texture coordinates");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "use_from_dupli", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "texflag", MTEX_DUPLI_MAPTO);
	RNA_def_property_ui_text(prop, "From Dupli",
	                         "Dupli's instanced from verts, faces or particles, inherit texture coordinate "
	                         "from their parent");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "use_map_to_bounds", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "texflag", MTEX_MAPTO_BOUNDS);
	RNA_def_property_ui_text(prop, "Map to Bounds",
	                         "Map coordinates in object bounds");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "use_from_original", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "texflag", MTEX_OB_DUPLI_ORIG);
	RNA_def_property_ui_text(prop, "From Original",
	                         "Dupli's derive their object coordinates from the original object's transformation");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "use_map_color_diffuse", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_COL);
	RNA_def_property_ui_text(prop, "Diffuse Color", "The texture affects basic color of the material");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_map_normal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_NORM);
	RNA_def_property_ui_text(prop, "Normal", "The texture affects the rendered normal");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_map_color_spec", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_COLSPEC);
	RNA_def_property_ui_text(prop, "Specular Color", "The texture affects the specularity color");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_map_mirror", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_COLMIR);
	RNA_def_property_ui_text(prop, "Mirror", "The texture affects the mirror color");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_map_diffuse", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_REF);
	RNA_def_property_ui_text(prop, "Diffuse", "The texture affects the value of diffuse reflectivity");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_map_specular", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_SPEC);
	RNA_def_property_ui_text(prop, "Specular", "The texture affects the value of specular reflectivity");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_map_ambient", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_AMB);
	RNA_def_property_ui_text(prop, "Ambient", "The texture affects the value of ambient");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_map_hardness", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_HAR);
	RNA_def_property_ui_text(prop, "Hardness", "The texture affects the hardness value");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_map_raymir", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_RAYMIRR);
	RNA_def_property_ui_text(prop, "Ray-Mirror", "The texture affects the ray-mirror value");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_map_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_ALPHA);
	RNA_def_property_ui_text(prop, "Alpha", "The texture affects the alpha value");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_map_emit", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_EMIT);
	RNA_def_property_ui_text(prop, "Emit", "The texture affects the emit value");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_map_translucency", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_TRANSLU);
	RNA_def_property_ui_text(prop, "Translucency", "The texture affects the translucency value");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_map_displacement", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_DISPLACE);
	RNA_def_property_ui_text(prop, "Displacement", "Let the texture displace the surface");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_map_warp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_WARP);
	RNA_def_property_ui_text(prop, "Warp", "Let the texture warp texture coordinates of next channels");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "mapping_x", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "projx");
	RNA_def_property_enum_items(prop, prop_x_mapping_items);
	RNA_def_property_ui_text(prop, "X Mapping", "");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "mapping_y", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "projy");
	RNA_def_property_enum_items(prop, prop_y_mapping_items);
	RNA_def_property_ui_text(prop, "Y Mapping", "");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "mapping_z", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "projz");
	RNA_def_property_enum_items(prop, prop_z_mapping_items);
	RNA_def_property_ui_text(prop, "Z Mapping", "");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "mapping", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_mapping_items);
	RNA_def_property_ui_text(prop, "Mapping", "");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "normal_map_space", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "normapspace");
	RNA_def_property_enum_items(prop, prop_normal_map_space_items);
	RNA_def_property_ui_text(prop, "Normal Map Space", "Set space of normal map image");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "normal_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "norfac");
	RNA_def_property_ui_range(prop, -5, 5, 10, 3);
	RNA_def_property_ui_text(prop, "Normal Factor", "Amount texture affects normal values");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "displacement_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dispfac");
	RNA_def_property_ui_range(prop, -1, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Displacement Factor", "Amount texture displaces the surface");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "warp_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "warpfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Warp Factor", "Amount texture affects texture coordinates of next channels");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "specular_color_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "colspecfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Specular Color Factor", "Amount texture affects specular color");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "diffuse_color_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "colfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Diffuse Color Factor", "Amount texture affects diffuse color");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "mirror_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mirrfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Mirror Factor", "Amount texture affects mirror color");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "alpha_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "alphafac");
	RNA_def_property_ui_range(prop, -1, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Alpha Factor", "Amount texture affects alpha");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "diffuse_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "difffac");
	RNA_def_property_ui_range(prop, -1, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Diffuse Factor", "Amount texture affects diffuse reflectivity");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "specular_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "specfac");
	RNA_def_property_ui_range(prop, -1, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Specular Factor", "Amount texture affects specular reflectivity");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "emit_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "emitfac");
	RNA_def_property_ui_range(prop, -1, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Emit Factor", "Amount texture affects emission");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "hardness_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "hardfac");
	RNA_def_property_ui_range(prop, -1, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Hardness Factor", "Amount texture affects hardness");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "raymir_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "raymirrfac");
	RNA_def_property_ui_range(prop, -1, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Ray Mirror Factor", "Amount texture affects ray mirror");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "translucency_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "translfac");
	RNA_def_property_ui_range(prop, -1, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Translucency Factor", "Amount texture affects translucency");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "ambient_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ambfac");
	RNA_def_property_ui_range(prop, -1, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Ambient Factor", "Amount texture affects ambient");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	/* volume material */
	prop = RNA_def_property(srna, "use_map_color_emission", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_EMISSION_COL);
	RNA_def_property_ui_text(prop, "Emission Color", "The texture affects the color of emission");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_map_color_reflection", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_REFLECTION_COL);
	RNA_def_property_ui_text(prop, "Reflection Color", "The texture affects the color of scattered light");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_map_color_transmission", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_TRANSMISSION_COL);
	RNA_def_property_ui_text(prop, "Transmission Color",
	                         "The texture affects the result color after other light has been scattered/absorbed");
	RNA_def_property_update(prop, NC_TEXTURE, NULL);
	
	prop = RNA_def_property(srna, "use_map_density", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_DENSITY);
	RNA_def_property_ui_text(prop, "Density", "The texture affects the volume's density");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_map_emission", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_EMISSION);
	RNA_def_property_ui_text(prop, "Emission", "The texture affects the volume's emission");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_map_scatter", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_SCATTERING);
	RNA_def_property_ui_text(prop, "Scattering", "The texture affects the volume's scattering");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_map_reflect", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_REFLECTION);
	RNA_def_property_ui_text(prop, "Reflection", "The texture affects the reflected light's brightness");
	RNA_def_property_update(prop, NC_TEXTURE, NULL);
	
	prop = RNA_def_property(srna, "emission_color_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "colemitfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Emission Color Factor", "Amount texture affects emission color");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "reflection_color_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "colreflfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Reflection Color Factor", "Amount texture affects color of out-scattered light");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "transmission_color_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "coltransfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Transmission Color Factor",
	                         "Amount texture affects result color after light has been scattered/absorbed");
	RNA_def_property_update(prop, NC_TEXTURE, NULL);
	
	prop = RNA_def_property(srna, "density_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "densfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Density Factor", "Amount texture affects density");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "emission_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "emitfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Emission Factor", "Amount texture affects emission");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "scattering_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "scatterfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Scattering Factor", "Amount texture affects scattering");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "reflection_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "reflfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Reflection Factor", "Amount texture affects brightness of out-scattered light");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	/* end volume material */
	
	prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_MaterialTextureSlot_use_get", "rna_MaterialTextureSlot_use_set");
	RNA_def_property_ui_text(prop, "Enabled", "Enable this material texture slot");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "bump_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "texflag");
	RNA_def_property_enum_items(prop, prop_bump_method_items);
	RNA_def_property_ui_text(prop, "Bump Method", "Method to use for bump mapping");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "bump_objectspace", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "texflag");
	RNA_def_property_enum_items(prop, prop_bump_space_items);
	RNA_def_property_ui_text(prop, "Bump Space", "Space to apply bump mapping in");
	RNA_def_property_update(prop, 0, "rna_Material_update");
}

static void rna_def_material_gamesettings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_alpha_blend_items[] = {
		{GEMAT_SOLID, "OPAQUE", 0, "Opaque", "Render color of textured face as color"},
		{GEMAT_ADD, "ADD", 0, "Add", "Render face transparent and add color of face"},
		{GEMAT_CLIP, "CLIP", 0, "Alpha Clip", "Use the image alpha values clipped with no blending (binary alpha)"},
		{GEMAT_ALPHA, "ALPHA", 0, "Alpha Blend",
		              "Render polygon transparent, depending on alpha channel of the texture"},
		{GEMAT_ALPHA_SORT, "ALPHA_SORT", 0, "Alpha Sort",
		                   "Sort faces for correct alpha drawing (slow, use Alpha Clip instead when possible)"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_face_orientation_items[] = {
		{GEMAT_NORMAL, "NORMAL", 0, "Normal", "No transformation"},
		{GEMAT_HALO, "HALO", 0, "Halo", "Screen aligned billboard"},
		{GEMAT_BILLBOARD, "BILLBOARD", 0, "Billboard", "Billboard with Z-axis constraint"},
		{GEMAT_SHADOW, "SHADOW", 0, "Shadow", "Faces are used for shadow"},
		{0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "MaterialGameSettings", NULL);
	RNA_def_struct_sdna(srna, "GameSettings");
	RNA_def_struct_nested(brna, srna, "Material");
	RNA_def_struct_ui_text(srna, "Material Game Settings", "Game Engine settings for a Material datablock");
	
	prop = RNA_def_property(srna, "use_backface_culling", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GEMAT_BACKCULL); /* use bitflags */
	RNA_def_property_ui_text(prop, "Backface Culling", "Hide Back of the face in Game Engine ");
	RNA_def_property_update(prop, 0, "rna_Material_draw_update");

	prop = RNA_def_property(srna, "text", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GEMAT_TEXT); /* use bitflags */
	RNA_def_property_ui_text(prop, "Text", "Use material as text in Game Engine ");
	RNA_def_property_update(prop, 0, "rna_Material_draw_update");

	prop = RNA_def_property(srna, "invisible", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GEMAT_INVISIBLE); /* use bitflags */
	RNA_def_property_ui_text(prop, "Invisible", "Make face invisible");
	RNA_def_property_update(prop, 0, "rna_Material_draw_update");

	prop = RNA_def_property(srna, "alpha_blend", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "alpha_blend");
	RNA_def_property_enum_items(prop, prop_alpha_blend_items);
	RNA_def_property_ui_text(prop, "Blend Mode", "Blend Mode for Transparent Faces");
	RNA_def_property_update(prop, 0, "rna_Material_draw_update");

	prop = RNA_def_property(srna, "face_orientation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_face_orientation_items);
	RNA_def_property_ui_text(prop, "Face Orientations", "Especial face orientation options");

	prop = RNA_def_property(srna, "physics", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", GEMAT_NOPHYSICS); /* use bitflags */
	RNA_def_property_ui_text(prop, "Physics", "Use physics properties of materials ");
}

static void rna_def_material_colors(StructRNA *srna)
{
	PropertyRNA *prop;

	static EnumPropertyItem prop_ramp_input_items[] = {
		{MA_RAMP_IN_SHADER, "SHADER", 0, "Shader", ""},
		{MA_RAMP_IN_ENERGY, "ENERGY", 0, "Energy", ""},
		{MA_RAMP_IN_NOR, "NORMAL", 0, "Normal", ""},
		{MA_RAMP_IN_RESULT, "RESULT", 0, "Result", ""},
		{0, NULL, 0, NULL, NULL}
	};

	prop = RNA_def_property(srna, "diffuse_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "r");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Diffuse Color", "Diffuse color of the material");
	RNA_def_property_update(prop, 0, "rna_Material_draw_update");
	
	prop = RNA_def_property(srna, "specular_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "specr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Specular Color", "Specular color of the material");
	RNA_def_property_update(prop, 0, "rna_Material_draw_update");
	
	prop = RNA_def_property(srna, "mirror_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "mirr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Mirror Color", "Mirror color of the material");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "alpha", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Alpha", "Alpha transparency of the material");
	RNA_def_property_update(prop, 0, "rna_Material_draw_update");

	prop = RNA_def_property(srna, "specular_alpha", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "spectra");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Specular Alpha", "Alpha transparency for specular areas");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	/* Color bands */
	prop = RNA_def_property(srna, "use_diffuse_ramp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_RAMP_COL);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Material_use_diffuse_ramp_set");
	RNA_def_property_ui_text(prop, "Use Diffuse Ramp", "Toggle diffuse ramp operations");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "diffuse_ramp", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ramp_col");
	RNA_def_property_struct_type(prop, "ColorRamp");
	RNA_def_property_ui_text(prop, "Diffuse Ramp", "Color ramp used to affect diffuse shading");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "use_specular_ramp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_RAMP_SPEC);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Material_use_specular_ramp_set");
	RNA_def_property_ui_text(prop, "Use Specular Ramp", "Toggle specular ramp operations");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "specular_ramp", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ramp_spec");
	RNA_def_property_struct_type(prop, "ColorRamp");
	RNA_def_property_ui_text(prop, "Specular Ramp", "Color ramp used to affect specular shading");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "diffuse_ramp_blend", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "rampblend_col");
	RNA_def_property_enum_items(prop, ramp_blend_items);
	RNA_def_property_ui_text(prop, "Diffuse Ramp Blend", "Blending method of the ramp and the diffuse color");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "specular_ramp_blend", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "rampblend_spec");
	RNA_def_property_enum_items(prop, ramp_blend_items);
	RNA_def_property_ui_text(prop, "Specular Ramp Blend", "Blending method of the ramp and the specular color");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "diffuse_ramp_input", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "rampin_col");
	RNA_def_property_enum_items(prop, prop_ramp_input_items);
	RNA_def_property_ui_text(prop, "Diffuse Ramp Input", "How the ramp maps on the surface");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "specular_ramp_input", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "rampin_spec");
	RNA_def_property_enum_items(prop, prop_ramp_input_items);
	RNA_def_property_ui_text(prop, "Specular Ramp Input", "How the ramp maps on the surface");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "diffuse_ramp_factor", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "rampfac_col");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Diffuse Ramp Factor", "Blending factor (also uses alpha in Colorband)");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "specular_ramp_factor", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "rampfac_spec");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Specular Ramp Factor", "Blending factor (also uses alpha in Colorband)");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	/* Freestyle line color */
	prop = RNA_def_property(srna, "line_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "line_col");
	RNA_def_property_array(prop, 4);
	RNA_def_property_ui_text(prop, "Line Color", "Line color used for Freestyle line rendering");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "line_priority", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "line_priority");
	RNA_def_property_range(prop, 0, 32767);
	RNA_def_property_ui_text(prop, "Line Priority",
	                         "The line color of a higher priority is used at material boundaries");
	RNA_def_property_update(prop, 0, "rna_Material_update");
}

static void rna_def_material_diffuse(StructRNA *srna)
{
	PropertyRNA *prop;

	static EnumPropertyItem prop_diff_shader_items[] = {
		{MA_DIFF_LAMBERT, "LAMBERT", 0, "Lambert", "Use a Lambertian shader"},
		{MA_DIFF_ORENNAYAR, "OREN_NAYAR", 0, "Oren-Nayar", "Use an Oren-Nayar shader"},
		{MA_DIFF_TOON, "TOON", 0, "Toon", "Use a toon shader"},
		{MA_DIFF_MINNAERT, "MINNAERT", 0, "Minnaert", "Use a Minnaert shader"},
		{MA_DIFF_FRESNEL, "FRESNEL", 0, "Fresnel", "Use a Fresnel shader"},
		{0, NULL, 0, NULL, NULL}
	};
	
	prop = RNA_def_property(srna, "diffuse_shader", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "diff_shader");
	RNA_def_property_enum_items(prop, prop_diff_shader_items);
	RNA_def_property_ui_text(prop, "Diffuse Shader Model", "");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "diffuse_intensity", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "ref");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Diffuse Intensity", "Amount of diffuse reflection");
	RNA_def_property_update(prop, 0, "rna_Material_draw_update");
	
	prop = RNA_def_property(srna, "roughness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 3.14f);
	RNA_def_property_ui_text(prop, "Roughness", "Oren-Nayar Roughness");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "diffuse_toon_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "param[0]");
	RNA_def_property_range(prop, 0.0f, 3.14f);
	RNA_def_property_ui_text(prop, "Diffuse Toon Size", "Size of diffuse toon area");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "diffuse_toon_smooth", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "param[1]");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Diffuse Toon Smooth", "Smoothness of diffuse toon area");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "diffuse_fresnel", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "param[1]");
	RNA_def_property_range(prop, 0.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Diffuse Fresnel", "Power of Fresnel");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "diffuse_fresnel_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "param[0]");
	RNA_def_property_range(prop, 0.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Diffuse Fresnel Factor", "Blending factor of Fresnel");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "darkness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Darkness", "Minnaert darkness");
	RNA_def_property_update(prop, 0, "rna_Material_update");
}

static void rna_def_material_raymirror(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_fadeto_mir_items[] = {
		{MA_RAYMIR_FADETOSKY, "FADE_TO_SKY", 0, "Sky", ""},
		{MA_RAYMIR_FADETOMAT, "FADE_TO_MATERIAL", 0, "Material", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "MaterialRaytraceMirror", NULL);
	RNA_def_struct_sdna(srna, "Material");
	RNA_def_struct_nested(brna, srna, "Material");
	RNA_def_struct_ui_text(srna, "Material Raytrace Mirror", "Raytraced reflection settings for a Material datablock");

	prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_RAYMIRROR); /* use bitflags */
	RNA_def_property_ui_text(prop, "Enabled", "Enable raytraced reflections");
	RNA_def_property_update(prop, 0, "rna_Material_update");
		
	prop = RNA_def_property(srna, "reflect_factor", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "ray_mirror");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Reflectivity", "Amount of mirror reflection for raytrace");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "fresnel", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fresnel_mir");
	RNA_def_property_range(prop, 0.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Fresnel", "Power of Fresnel for mirror reflection");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "fresnel_factor", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "fresnel_mir_i");
	RNA_def_property_range(prop, 0.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Fresnel Factor", "Blending factor for Fresnel");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "gloss_factor", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "gloss_mir");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Gloss Amount",
	                         "The shininess of the reflection (values < 1.0 give diffuse, blurry reflections)");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "gloss_anisotropic", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "aniso_gloss_mir");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Gloss Anisotropy",
	                         "The shape of the reflection, from 0.0 (circular) to 1.0 "
	                         "(fully stretched along the tangent");
	RNA_def_property_update(prop, 0, "rna_Material_update");
		
	prop = RNA_def_property(srna, "gloss_samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "samp_gloss_mir");
	RNA_def_property_range(prop, 0, 1024);
	RNA_def_property_ui_text(prop, "Gloss Samples", "Number of cone samples averaged for blurry reflections");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "gloss_threshold", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "adapt_thresh_mir");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Gloss Threshold",
	                         "Threshold for adaptive sampling (if a sample contributes less than "
	                         "this amount [as a percentage], sampling is stopped)");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "depth", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "ray_depth");
	RNA_def_property_ui_range(prop, 0, 100, 1, 3);
	RNA_def_property_ui_text(prop, "Depth", "Maximum allowed number of light inter-reflections");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "dist_mir");
	RNA_def_property_range(prop, 0.0f, 10000.0f);
	RNA_def_property_ui_text(prop, "Maximum Distance",
	                         "Maximum distance of reflected rays (reflections further than this "
	                         "range fade to sky color or material color)");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "fade_to", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "fadeto_mir");
	RNA_def_property_enum_items(prop, prop_fadeto_mir_items);
	RNA_def_property_ui_text(prop, "Fade-out Color",
	                         "The color that rays with no intersection within the Max Distance take "
	                         "(material color can be best for indoor scenes, sky color for outdoor)");
	RNA_def_property_update(prop, 0, "rna_Material_update");
}

static void rna_def_material_raytra(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MaterialRaytraceTransparency", NULL);
	RNA_def_struct_sdna(srna, "Material");
	RNA_def_struct_nested(brna, srna, "Material");
	RNA_def_struct_ui_text(srna, "Material Raytrace Transparency",
	                       "Raytraced refraction settings for a Material datablock");

	prop = RNA_def_property(srna, "ior", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ang");
	RNA_def_property_range(prop, 0.25f, 4.0f);
	RNA_def_property_ui_text(prop, "IOR", "Angular index of refraction for raytraced refraction");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "fresnel", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fresnel_tra");
	RNA_def_property_range(prop, 0.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Fresnel", "Power of Fresnel for transparency (Ray or ZTransp)");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "fresnel_factor", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "fresnel_tra_i");
	RNA_def_property_range(prop, 1.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Fresnel Factor", "Blending factor for Fresnel");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "gloss_factor", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "gloss_tra");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Gloss Amount",
	                         "The clarity of the refraction. Values < 1.0 give diffuse, blurry refractions");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "gloss_samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "samp_gloss_tra");
	RNA_def_property_range(prop, 0, 1024);
	RNA_def_property_ui_text(prop, "Gloss Samples", "Number of cone samples averaged for blurry refractions");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "gloss_threshold", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "adapt_thresh_tra");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Gloss Threshold",
	                         "Threshold for adaptive sampling. If a sample contributes less than "
	                         "this amount (as a percentage), sampling is stopped");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "depth", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "ray_depth_tra");
	RNA_def_property_ui_range(prop, 0, 100, 1, 3);
	RNA_def_property_ui_text(prop, "Depth", "Maximum allowed number of light inter-refractions");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "filter", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "filter");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Filter",
	                         "Amount to blend in the material's diffuse color in raytraced "
	                         "transparency (simulating absorption)");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "depth_max", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "tx_limit");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Limit",
	                         "Maximum depth for light to travel through the transparent material "
	                         "before becoming fully filtered (0.0 is disabled)");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "falloff", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "tx_falloff");
	RNA_def_property_range(prop, 0.1f, 10.0f);
	RNA_def_property_ui_text(prop, "Falloff", "Falloff power for transmissivity filter effect (1.0 is linear)");
	RNA_def_property_update(prop, 0, "rna_Material_update");
}

static void rna_def_material_volume(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_lighting_items[] = {
		{MA_VOL_SHADE_SHADELESS, "SHADELESS", 0, "Shadeless", "Do not calculate lighting and shadows"},
		{MA_VOL_SHADE_SHADOWED, "SHADOWED", 0, "Shadowed", ""},
		{MA_VOL_SHADE_SHADED, "SHADED", 0, "Shaded", ""},
		{MA_VOL_SHADE_MULTIPLE, "MULTIPLE_SCATTERING", 0, "Multiple Scattering", ""},
		{MA_VOL_SHADE_SHADEDPLUSMULTIPLE, "SHADED_PLUS_MULTIPLE_SCATTERING", 0, "Shaded + Multiple Scattering", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_stepsize_items[] = {
		{MA_VOL_STEP_RANDOMIZED, "RANDOMIZED", 0, "Randomized", ""},
		{MA_VOL_STEP_CONSTANT, "CONSTANT", 0, "Constant", ""},
		/*{MA_VOL_STEP_ADAPTIVE, "ADAPTIVE", 0, "Adaptive", ""}, */
		{0, NULL, 0, NULL, NULL}
	};
		
	srna = RNA_def_struct(brna, "MaterialVolume", NULL);
	RNA_def_struct_sdna(srna, "VolumeSettings");
	RNA_def_struct_nested(brna, srna, "Material");
	RNA_def_struct_ui_text(srna, "Material Volume", "Volume rendering settings for a Material datablock");
	
	prop = RNA_def_property(srna, "step_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "stepsize_type");
	RNA_def_property_enum_items(prop, prop_stepsize_items);
	RNA_def_property_ui_text(prop, "Step Calculation", "Method of calculating the steps through the volume");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "step_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "stepsize");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.001f, 1.0f, 1, 3);
	RNA_def_property_ui_text(prop, "Step Size", "Distance between subsequent volume depth samples");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "light_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "shade_type");
	RNA_def_property_enum_items(prop, prop_lighting_items);
	RNA_def_property_ui_text(prop, "Lighting Mode",
	                         "Method of shading, attenuating, and scattering light through the volume");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_external_shadows", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shadeflag", MA_VOL_RECV_EXT_SHADOW); /* use bitflags */
	RNA_def_property_ui_text(prop, "External Shadows", "Receive shadows from sources outside the volume (temporary)");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_light_cache", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shadeflag", MA_VOL_PRECACHESHADING); /* use bitflags */
	RNA_def_property_ui_text(prop, "Light Cache",
	                         "Pre-calculate the shading information into a voxel grid, "
	                         "speeds up shading at slightly less accuracy");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "cache_resolution", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "precache_resolution");
	RNA_def_property_range(prop, 1, 1024);
	RNA_def_property_ui_text(prop, "Resolution",
	                         "Resolution of the voxel grid, low resolutions are faster, "
	                         "high resolutions use more memory");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "ms_diffusion", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ms_diff");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_text(prop, "Diffusion", "Diffusion factor, the strength of the blurring effect");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "ms_spread", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ms_spread");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1, 3);
	RNA_def_property_ui_text(prop, "Spread", "Proportional distance over which the light is diffused");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "ms_intensity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ms_intensity");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_text(prop, "Intensity", "Multiplier for multiple scattered light energy");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "depth_threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "depth_cutoff");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Depth Cutoff",
	                         "Stop ray marching early if transmission drops below this luminance - "
	                         "higher values give speedups in dense volumes at the expense of accuracy");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "density", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "density");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Density", "The base density of the volume");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "density_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "density_scale");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
	RNA_def_property_ui_text(prop, "Density Scale", "Multiplier for the material's density");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "scattering", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "scattering");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
	RNA_def_property_ui_text(prop, "Scattering",
	                         "Amount of light that gets scattered out by the volume - "
	                         "the more out-scattering, the shallower the light will penetrate");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "transmission_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "transmission_col");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Transmission Color",
	                         "Result color of the volume, after other light has been scattered/absorbed");
	RNA_def_property_update(prop, 0, "rna_Material_draw_update");
	
	prop = RNA_def_property(srna, "reflection_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "reflection_col");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Reflection Color",
	                         "Color of light scattered out of the volume (does not affect transmission)");
	RNA_def_property_update(prop, 0, "rna_Material_draw_update");
	
	prop = RNA_def_property(srna, "reflection", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "reflection");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 100.0f, 1, 3);
	RNA_def_property_ui_text(prop, "Reflection",
	                         "Multiplier to make out-scattered light brighter or darker (non-physically correct)");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "emission_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "emission_col");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Emission Color", "Color of emitted light");
	RNA_def_property_update(prop, 0, "rna_Material_draw_update");
	
	prop = RNA_def_property(srna, "emission", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "emission");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
	RNA_def_property_ui_text(prop, "Emission", "Amount of light that gets emitted by the volume");
	RNA_def_property_update(prop, 0, "rna_Material_update");
		
	prop = RNA_def_property(srna, "asymmetry", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "asymmetry");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Asymmetry",
	                         "Back scattering (-1.0) to Forward scattering (1.0) and the range in between");
	RNA_def_property_update(prop, 0, "rna_Material_update");
}


static void rna_def_material_halo(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MaterialHalo", NULL);
	RNA_def_struct_sdna(srna, "Material");
	RNA_def_struct_nested(brna, srna, "Material");
	RNA_def_struct_ui_text(srna, "Material Halo", "Halo particle effect settings for a Material datablock");

	prop = RNA_def_property(srna, "size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "hasize");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Size", "Dimension of the halo");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "hardness", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "har");
	RNA_def_property_range(prop, 0, 127);
	RNA_def_property_ui_text(prop, "Hardness", "Hardness of the halo");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "add", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "add");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Add", "Strength of the add effect");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "ring_count", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ringc");
	RNA_def_property_range(prop, 0, 24);
	RNA_def_property_ui_text(prop, "Rings", "Number of rings rendered over the halo");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "line_count", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "linec");
	RNA_def_property_range(prop, 0, 250);
	RNA_def_property_ui_text(prop, "Line Number", "Number of star shaped lines rendered over the halo");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "star_tip_count", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "starc");
	RNA_def_property_range(prop, 3, 50);
	RNA_def_property_ui_text(prop, "Star Tips", "Number of points on the star shaped halo");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "seed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "seed1");
	RNA_def_property_range(prop, 0, 255);
	RNA_def_property_ui_text(prop, "Seed", "Randomize ring dimension and line location");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_flare_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALO_FLARE); /* use bitflags */
	RNA_def_property_ui_text(prop, "Flare", "Render halo as a lens flare");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "flare_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "flaresize");
	RNA_def_property_range(prop, 0.1f, 25.0f);
	RNA_def_property_ui_text(prop, "Flare Size", "Factor by which the flare is larger than the halo");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "flare_subflare_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "subsize");
	RNA_def_property_range(prop, 0.1f, 25.0f);
	RNA_def_property_ui_text(prop, "Flare Subsize", "Dimension of the sub-flares, dots and circles");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "flare_boost", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "flareboost");
	RNA_def_property_range(prop, 0.1f, 10.0f);
	RNA_def_property_ui_text(prop, "Flare Boost", "Give the flare extra strength");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "flare_seed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "seed2");
	RNA_def_property_range(prop, 0, 255);
	RNA_def_property_ui_text(prop, "Flare Seed", "Offset in the flare seed table");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "flare_subflare_count", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "flarec");
	RNA_def_property_range(prop, 1, 32);
	RNA_def_property_ui_text(prop, "Flares Sub", "Number of sub-flares");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_ring", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALO_RINGS);
	RNA_def_property_ui_text(prop, "Rings", "Render rings over halo");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_lines", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALO_LINES);
	RNA_def_property_ui_text(prop, "Lines", "Render star shaped lines over halo");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_star", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_STAR);
	RNA_def_property_ui_text(prop, "Star", "Render halo as a star");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_texture", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALOTEX);
	RNA_def_property_ui_text(prop, "Texture", "Give halo a texture");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_vertex_normal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALOPUNO);
	RNA_def_property_ui_text(prop, "Vertex Normal", "Use the vertex normal to specify the dimension of the halo");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_extreme_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALO_XALPHA);
	RNA_def_property_ui_text(prop, "Extreme Alpha", "Use extreme alpha");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_shaded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALO_SHADE);
	RNA_def_property_ui_text(prop, "Shaded", "Let halo receive light and shadows from external objects");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_soft", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALO_SOFT);
	RNA_def_property_ui_text(prop, "Soft", "Soften the edges of halos at intersections with other geometry");
	RNA_def_property_update(prop, 0, "rna_Material_update");
}

static void rna_def_material_sss(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MaterialSubsurfaceScattering", NULL);
	RNA_def_struct_sdna(srna, "Material");
	RNA_def_struct_nested(brna, srna, "Material");
	RNA_def_struct_ui_text(srna, "Material Subsurface Scattering",
	                       "Diffuse subsurface scattering settings for a Material datablock");

	prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_COLOR | PROP_UNIT_LENGTH);
	RNA_def_property_float_sdna(prop, NULL, "sss_radius");
	RNA_def_property_range(prop, 0.001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.001, 10000, 1, 3);
	RNA_def_property_ui_text(prop, "Radius", "Mean red/green/blue scattering path length");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "sss_col");
	RNA_def_property_ui_text(prop, "Color", "Scattering color");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "error_threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sss_error");
	RNA_def_property_ui_range(prop, 0.0001, 10, 1, 3);
	RNA_def_property_ui_text(prop, "Error Tolerance", "Error tolerance (low values are slower and higher quality)");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sss_scale");
	RNA_def_property_ui_range(prop, 0.001, 1000, 1, 3);
	RNA_def_property_ui_text(prop, "Scale", "Object scale factor");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "ior", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sss_ior");
	RNA_def_property_ui_range(prop, 0.1, 2, 1, 3);
	RNA_def_property_ui_text(prop, "IOR", "Index of refraction (higher values are denser)");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "color_factor", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "sss_colfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Color Factor", "Blend factor for SSS colors");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "texture_factor", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "sss_texfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Texture Factor", "Texture scattering blend factor");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "front", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sss_front");
	RNA_def_property_range(prop, 0, 2);
	RNA_def_property_ui_text(prop, "Front", "Front scattering weight");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "back", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sss_back");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Back", "Back scattering weight");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "sss_flag", MA_DIFF_SSS);
	RNA_def_property_ui_text(prop, "Enabled", "Enable diffuse subsurface scattering effects in a material");
	RNA_def_property_update(prop, 0, "rna_Material_update");
}

static void rna_def_material_specularity(StructRNA *srna)
{
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_specular_shader_items[] = {
		{MA_SPEC_COOKTORR, "COOKTORR", 0, "CookTorr", "Use a Cook-Torrance shader"},
		{MA_SPEC_PHONG, "PHONG", 0, "Phong", "Use a Phong shader"},
		{MA_SPEC_BLINN, "BLINN", 0, "Blinn", "Use a Blinn shader"},
		{MA_SPEC_TOON, "TOON", 0, "Toon", "Use a toon shader"},
		{MA_SPEC_WARDISO, "WARDISO", 0, "WardIso", "Use a Ward anisotropic shader"},
		{0, NULL, 0, NULL, NULL}
	};
	
	prop = RNA_def_property(srna, "specular_shader", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "spec_shader");
	RNA_def_property_enum_items(prop, prop_specular_shader_items);
	RNA_def_property_ui_text(prop, "Specular Shader Model", "");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "specular_intensity", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "spec");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Specular Intensity", "How intense (bright) the specular reflection is");
	RNA_def_property_update(prop, 0, "rna_Material_draw_update");

	/* NOTE: "har", "param", etc are used for multiple purposes depending on
	 * settings. This should be fixed in DNA once, for RNA we just expose them
	 * multiple times, which may give somewhat strange changes in the outliner,
	 * but in the UI they are never visible at the same time. */

	prop = RNA_def_property(srna, "specular_hardness", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "har");
	RNA_def_property_range(prop, 1, 511);
	RNA_def_property_ui_text(prop, "Specular Hardness", "How hard (sharp) the specular reflection is");
	RNA_def_property_update(prop, 0, "rna_Material_draw_update");

	prop = RNA_def_property(srna, "specular_ior", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "refrac");
	RNA_def_property_range(prop, 1, 10);
	RNA_def_property_ui_text(prop, "Specular IOR", "Specular index of refraction");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "specular_toon_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "param[2]");
	RNA_def_property_range(prop, 0.0f, 1.53f);
	RNA_def_property_ui_text(prop, "Specular Toon Size", "Size of specular toon area");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "specular_toon_smooth", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "param[3]");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Specular Toon Smooth", "Smoothness of specular toon area");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "specular_slope", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "rms");
	RNA_def_property_range(prop, 0, 0.4);
	RNA_def_property_ui_text(prop, "Specular Slope", "The standard deviation of surface slope");
	RNA_def_property_update(prop, 0, "rna_Material_update");
}

static void rna_def_material_strand(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MaterialStrand", NULL);
	RNA_def_struct_sdna(srna, "Material");
	RNA_def_struct_nested(brna, srna, "Material");
	RNA_def_struct_ui_text(srna, "Material Strand", "Strand settings for a Material datablock");

	prop = RNA_def_property(srna, "use_tangent_shading", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_TANGENT_STR);
	RNA_def_property_ui_text(prop, "Tangent Shading", "Use direction of strands as normal for tangent-shading");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	/* this flag is only set when rendering, not to be edited manually */
	prop = RNA_def_property(srna, "use_surface_diffuse", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_STR_SURFDIFF);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Surface Diffuse", "Make diffuse shading more similar to shading the surface");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "blend_distance", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "strand_surfnor");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Blend Distance", "Worldspace distance over which to blend in the surface normal");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "use_blender_units", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_STR_B_UNITS);
	RNA_def_property_ui_text(prop, "Blender Units", "Use Blender units for widths instead of pixels");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "root_size", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "strand_sta");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_MaterialStrand_start_size_range");
	RNA_def_property_ui_range(prop, 0, 10.0f, 10, 5);
	RNA_def_property_ui_text(prop, "Root Size", "Start size of strands in pixels or Blender units");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "tip_size", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "strand_end");
	RNA_def_property_ui_range(prop, 0, 10.0f, 10, 5);
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_MaterialStrand_end_size_range");
	RNA_def_property_ui_text(prop, "Tip Size", "End size of strands in pixels or Blender units");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "size_min", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "strand_min");
	RNA_def_property_range(prop, 0.001, 10);
	RNA_def_property_ui_text(prop, "Minimum Size", "Minimum size of strands in pixels");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "shape", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "strand_ease");
	RNA_def_property_range(prop, -0.9, 0.9);
	RNA_def_property_ui_text(prop, "Shape", "Positive values make strands rounder, negative ones make strands spiky");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "width_fade", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "strand_widthfade");
	RNA_def_property_range(prop, 0, 2);
	RNA_def_property_ui_text(prop, "Width Fade", "Transparency along the width of the strand");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "strand_uvname");
	RNA_def_property_ui_text(prop, "UV Map", "Name of UV map to override");
	RNA_def_property_update(prop, 0, "rna_Material_update");
}

static void rna_def_material_physics(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "MaterialPhysics", NULL);
	RNA_def_struct_sdna(srna, "Material");
	RNA_def_struct_nested(brna, srna, "Material");
	RNA_def_struct_ui_text(srna, "Material Physics", "Physics settings for a Material datablock");
	
	prop = RNA_def_property(srna, "friction", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "friction");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Friction", "Coulomb friction coefficient, when inside the physics distance area");

	prop = RNA_def_property(srna, "elasticity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "reflect");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Elasticity", "Elasticity of collisions");

	/* FH/Force Field Settings */
	prop = RNA_def_property(srna, "use_fh_normal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "dynamode", MA_FH_NOR);
	RNA_def_property_ui_text(prop, "Align to Normal",
	                         "Align dynamic game objects along the surface normal, "
	                         "when inside the physics distance area");

	prop = RNA_def_property(srna, "fh_force", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fh");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 10, 2);
	RNA_def_property_ui_text(prop, "Force", "Upward spring force, when inside the physics distance area");
	
	prop = RNA_def_property(srna, "fh_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fhdist");
	RNA_def_property_range(prop, 0, 20);
	RNA_def_property_ui_text(prop, "Distance", "Distance of the physics area");
	
	prop = RNA_def_property(srna, "fh_damping", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "xyfrict");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Damping", "Damping of the spring force, when inside the physics distance area");
}

void RNA_def_material(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_type_items[] = {
		{MA_TYPE_SURFACE, "SURFACE", 0, "Surface", "Render object as a surface"},
		{MA_TYPE_WIRE, "WIRE", 0, "Wire", "Render the edges of faces as wires (not supported in raytracing)"},
		{MA_TYPE_VOLUME, "VOLUME", 0, "Volume", "Render object as a volume"},
		{MA_TYPE_HALO, "HALO", 0, "Halo", "Render object as halo particles"},
		{0, NULL, 0, NULL, NULL}
	};
	static EnumPropertyItem transparency_items[] = {
		{0, "MASK", 0, "Mask", "Mask the background"},
		{MA_ZTRANSP, "Z_TRANSPARENCY", 0, "Z Transparency", "Use alpha buffer for transparent faces"},
		{MA_RAYTRANSP, "RAYTRACE", 0, "Raytrace", "Use raytracing for transparent refraction rendering"},
		{0, NULL, 0, NULL, NULL}
	};
	
	/* Render Preview Types */
	static EnumPropertyItem preview_type_items[] = {
		{MA_FLAT, "FLAT", ICON_MATPLANE, "Flat", "Flat XY plane"},
		{MA_SPHERE, "SPHERE", ICON_MATSPHERE, "Sphere", "Sphere"},
		{MA_CUBE, "CUBE", ICON_MATCUBE, "Cube", "Cube"},
		{MA_MONKEY, "MONKEY", ICON_MONKEY, "Monkey", "Monkey"},
		{MA_HAIR, "HAIR", ICON_HAIR, "Hair", "Hair strands"},
		{MA_SPHERE_A, "SPHERE_A", ICON_MAT_SPHERE_SKY, "World Sphere", "Large sphere with sky"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_shadows_only_items[] = {
		{MA_SO_OLD, "SHADOW_ONLY_OLD", 0, "Shadow and Distance", "Old shadow only method"},
		{MA_SO_SHADOW, "SHADOW_ONLY", 0, "Shadow Only", "Improved shadow only method"},
		{MA_SO_SHADED, "SHADOW_ONLY_SHADED", 0, "Shadow and Shading",
		 "Improved shadow only method which also renders lightless areas as shadows"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "Material", "ID");
	RNA_def_struct_ui_text(srna, "Material",
	                       "Material datablock to define the appearance of geometric objects for rendering");
	RNA_def_struct_ui_icon(srna, ICON_MATERIAL_DATA);
	
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "material_type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Type", "Material type defining how the object is rendered");
	RNA_def_property_enum_funcs(prop, NULL, "rna_Material_type_set", NULL);
	RNA_def_property_update(prop, 0, "rna_Material_draw_update");

	prop = RNA_def_property(srna, "use_transparency", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_TRANSP);
	RNA_def_property_ui_text(prop, "Transparency", "Render material as transparent");
	RNA_def_property_update(prop, 0, "rna_Material_draw_update");

	prop = RNA_def_property(srna, "transparency_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, transparency_items);
	RNA_def_property_ui_text(prop, "Transparency Method", "Method to use for rendering transparency");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	/* For Preview Render */
	prop = RNA_def_property(srna, "preview_render_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "pr_type");
	RNA_def_property_enum_items(prop, preview_type_items);
	RNA_def_property_ui_text(prop, "Preview render type", "Type of preview render");
	RNA_def_property_update(prop, 0, "rna_Material_update_previews");
	
	prop = RNA_def_property(srna, "ambient", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "amb");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Ambient", "Amount of global ambient color the material receives");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "emit", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 2.0f, 1, 2);
	RNA_def_property_ui_text(prop, "Emit", "Amount of light to emit");
	RNA_def_property_update(prop, 0, "rna_Material_draw_update");

	prop = RNA_def_property(srna, "translucency", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Translucency", "Amount of diffuse shading on the back side");
	RNA_def_property_update(prop, 0, "rna_Material_update");
		
	prop = RNA_def_property(srna, "use_cubic", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shade_flag", MA_CUBIC);
	RNA_def_property_ui_text(prop, "Cubic Interpolation",
	                         "Use cubic interpolation for diffuse values, for smoother transitions");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_object_color", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shade_flag", MA_OBCOLOR);
	RNA_def_property_ui_text(prop, "Object Color", "Modulate the result with a per-object color");
	RNA_def_property_update(prop, 0, "rna_Material_draw_update");

	prop = RNA_def_property(srna, "shadow_ray_bias", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sbias");
	RNA_def_property_range(prop, 0, 0.25);
	RNA_def_property_ui_text(prop, "Shadow Ray Bias",
	                         "Shadow raytracing bias to prevent terminator problems on shadow boundary");

	prop = RNA_def_property(srna, "shadow_buffer_bias", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "lbias");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Shadow Buffer Bias", "Factor to multiply shadow buffer bias with (0 is ignore)");

	prop = RNA_def_property(srna, "shadow_cast_alpha", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "shad_alpha");
	RNA_def_property_range(prop, 0.001, 1);
	RNA_def_property_ui_text(prop, "Shadow Casting Alpha",
	                         "Shadow casting alpha, in use for Irregular and Deep shadow buffer");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "light_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "group");
	RNA_def_property_struct_type(prop, "Group");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Light Group", "Limit lighting to lamps in this Group");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "index");
	RNA_def_property_ui_text(prop, "Pass Index", "Index number for the IndexMA render pass");
	RNA_def_property_update(prop, NC_OBJECT, "rna_Material_update");

	/* flags */
	
	prop = RNA_def_property(srna, "use_light_group_exclusive", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_GROUP_NOLAY);
	RNA_def_property_ui_text(prop, "Light Group Exclusive",
	                         "Material uses the light group exclusively - these lamps are excluded "
	                         "from other scene lighting");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "use_light_group_local", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shade_flag", MA_GROUP_LOCAL);
	RNA_def_property_ui_text(prop, "Light Group Local", "When linked in, material uses local light group with the same name");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "use_raytrace", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_TRACEBLE);
	RNA_def_property_ui_text(prop, "Traceable",
	                         "Include this material and geometry that uses it in raytracing calculations");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_shadows", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_SHADOW);
	RNA_def_property_ui_text(prop, "Shadows", "Allow this material to receive shadows");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_shadeless", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_SHLESS);
	RNA_def_property_ui_text(prop, "Shadeless", "Make this material insensitive to light or shadow");
	RNA_def_property_update(prop, 0, "rna_Material_draw_update");
	
	prop = RNA_def_property(srna, "use_vertex_color_light", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_VERTEXCOL);
	RNA_def_property_ui_text(prop, "Vertex Color Light", "Add vertex colors as additional lighting");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "use_vertex_color_paint", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_VERTEXCOLP);
	RNA_def_property_ui_text(prop, "Vertex Color Paint",
	                         "Replace object base color with vertex colors (multiply with "
	                         "'texture face' face assigned textures)");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "invert_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_ZINV);
	RNA_def_property_ui_text(prop, "Invert Z Depth",
	                         "Render material's faces with an inverted Z buffer (scanline only)");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "offset_z", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "zoffs");
	RNA_def_property_ui_text(prop, "Z Offset", "Give faces an artificial offset in the Z buffer for Z transparency");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_sky", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_ENV);
	RNA_def_property_ui_text(prop, "Sky",
	                         "Render this material with zero alpha, with sky background in place (scanline only)");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_only_shadow", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_ONLYSHADOW);
	RNA_def_property_ui_text(prop, "Only Shadow",
	                         "Render shadows as the material's alpha value, making the material "
	                         "transparent except for shadowed areas");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "shadow_only_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "shadowonly_flag");
	RNA_def_property_enum_items(prop, prop_shadows_only_items);
	RNA_def_property_ui_text(prop, "Shadow Type", "How to draw shadows");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_face_texture", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_FACETEXTURE);
	RNA_def_property_ui_text(prop, "Face Textures",
	                         "Replace the object's base color with color from UV map image textures");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_face_texture_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_FACETEXTURE_ALPHA);
	RNA_def_property_ui_text(prop, "Face Textures Alpha",
	                         "Replace the object's base alpha value with alpha from UV map image textures");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_cast_shadows", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode2", MA_CASTSHADOW);
	RNA_def_property_ui_text(prop, "Cast Shadows",
	                         "Allow this material to cast shadows");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_cast_shadows_only", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_ONLYCAST);
	RNA_def_property_ui_text(prop, "Cast Shadows Only",
	                         "Make objects with this material appear invisible (not rendered), only casting shadows");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_mist", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "mode", MA_NOMIST);
	RNA_def_property_ui_text(prop, "Use Mist", "Use mist with this material (in world settings)");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_transparent_shadows", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_SHADOW_TRA);
	RNA_def_property_ui_text(prop, "Receive Transparent Shadows",
	                         "Allow this object to receive transparent shadows cast through other objects");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_ray_shadow_bias", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_RAYBIAS);
	RNA_def_property_ui_text(prop, "Ray Shadow Bias",
	                         "Prevent raytraced shadow errors on surfaces with smooth shaded normals "
	                         "(terminator problem)");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_full_oversampling", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_FULL_OSA);
	RNA_def_property_ui_text(prop, "Full Oversampling",
	                         "Force this material to render full shading/textures for all anti-aliasing samples");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "use_cast_buffer_shadows", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_SHADBUF);
	RNA_def_property_ui_text(prop, "Cast Buffer Shadows",
	                         "Allow this material to cast shadows from shadow buffer lamps");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	prop = RNA_def_property(srna, "use_cast_approximate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "shade_flag", MA_APPROX_OCCLUSION);
	RNA_def_property_ui_text(prop, "Cast Approximate",
	                         "Allow this material to cast shadows when using approximate ambient occlusion");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_tangent_shading", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_TANGENT_V);
	RNA_def_property_ui_text(prop, "Tangent Shading",
	                         "Use the material's tangent vector instead of the normal for shading "
	                         "- for anisotropic shading effects");
	RNA_def_property_update(prop, 0, "rna_Material_update");
	
	prop = RNA_def_property(srna, "use_uv_project", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapflag", MA_MAPFLAG_UVPROJECT);
	RNA_def_property_ui_text(prop, "UV Project",
	                         "Use to ensure UV interpolation is correct for camera projections (use with UV project modifier)");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	/* nested structs */
	prop = RNA_def_property(srna, "raytrace_mirror", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "MaterialRaytraceMirror");
	RNA_def_property_pointer_funcs(prop, "rna_Material_mirror_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Raytrace Mirror", "Raytraced reflection settings for the material");

	prop = RNA_def_property(srna, "raytrace_transparency", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "MaterialRaytraceTransparency");
	RNA_def_property_pointer_funcs(prop, "rna_Material_transp_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Raytrace Transparency", "Raytraced transparency settings for the material");

	prop = RNA_def_property(srna, "volume", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "vol");
	RNA_def_property_struct_type(prop, "MaterialVolume");
	RNA_def_property_ui_text(prop, "Volume", "Volume settings for the material");

	prop = RNA_def_property(srna, "halo", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "MaterialHalo");
	RNA_def_property_pointer_funcs(prop, "rna_Material_halo_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Halo", "Halo settings for the material");

	prop = RNA_def_property(srna, "subsurface_scattering", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "MaterialSubsurfaceScattering");
	RNA_def_property_pointer_funcs(prop, "rna_Material_sss_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Subsurface Scattering", "Subsurface scattering settings for the material");

	prop = RNA_def_property(srna, "strand", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "MaterialStrand");
	RNA_def_property_pointer_funcs(prop, "rna_Material_strand_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Strand", "Strand settings for the material");
	
	prop = RNA_def_property(srna, "physics", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "MaterialPhysics");
	RNA_def_property_pointer_funcs(prop, "rna_Material_physics_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Physics", "Game physics settings");

	/* game settings */
	prop = RNA_def_property(srna, "game_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "game");
	RNA_def_property_struct_type(prop, "MaterialGameSettings");
	RNA_def_property_ui_text(prop, "Game Settings", "Game material settings");

	/* nodetree */
	prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "nodetree");
	RNA_def_property_ui_text(prop, "Node Tree", "Node tree for node based materials");

	prop = RNA_def_property(srna, "use_nodes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "use_nodes", 1);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_ui_text(prop, "Use Nodes", "Use shader nodes to render the material");
	RNA_def_property_update(prop, 0, "rna_Material_use_nodes_update");

	prop = RNA_def_property(srna, "active_node_material", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Material");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, "rna_Material_active_node_material_get",
	                               "rna_Material_active_node_material_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Material", "Active node material");
	RNA_def_property_update(prop, NC_MATERIAL, NULL);

	/* common */
	rna_def_animdata_common(srna);
	rna_def_mtex_common(brna, srna, "rna_Material_mtex_begin", "rna_Material_active_texture_get",
	                    "rna_Material_active_texture_set", "rna_Material_active_texture_editable",
	                    "MaterialTextureSlot", "MaterialTextureSlots", "rna_Material_update", "rna_Material_update");

	/* only material has this one */
	prop = RNA_def_property(srna, "use_textures", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "septex", 1);
	RNA_def_property_array(prop, 18);
	RNA_def_property_ui_text(prop, "Use Textures", "Enable/Disable each texture");
	RNA_def_property_update(prop, 0, "rna_Material_update");

	rna_def_material_colors(srna);
	rna_def_material_diffuse(srna);
	rna_def_material_specularity(srna);

	/* nested structs */
	rna_def_material_raymirror(brna);
	rna_def_material_raytra(brna);
	rna_def_material_volume(brna);
	rna_def_material_halo(brna);
	rna_def_material_sss(brna);
	rna_def_material_mtex(brna);
	rna_def_material_strand(brna);
	rna_def_material_physics(brna);
	rna_def_material_gamesettings(brna);

	RNA_api_material(srna);
}


static void rna_def_texture_slots(BlenderRNA *brna, PropertyRNA *cprop, const char *structname,
                                  const char *structname_slots)
{
	StructRNA *srna;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, structname_slots);
	srna = RNA_def_struct(brna, structname_slots, NULL);
	RNA_def_struct_sdna(srna, "ID");
	RNA_def_struct_ui_text(srna, "Texture Slots", "Collection of texture slots");

	/* functions */
	func = RNA_def_function(srna, "add", "rna_mtex_texture_slots_add");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_NO_SELF | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "mtex", structname, "", "The newly initialized mtex");
	RNA_def_function_return(func, parm);
	
	func = RNA_def_function(srna, "create", "rna_mtex_texture_slots_create");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_NO_SELF | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	parm = RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "Slot index to initialize", 0, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "mtex", structname, "", "The newly initialized mtex");
	RNA_def_function_return(func, parm);
	
	func = RNA_def_function(srna, "clear", "rna_mtex_texture_slots_clear");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_NO_SELF | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	parm = RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "Slot index to clear", 0, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

void rna_def_mtex_common(BlenderRNA *brna, StructRNA *srna, const char *begin,
                         const char *activeget, const char *activeset, const char *activeeditable,
                         const char *structname, const char *structname_slots, const char *update, const char *update_index)
{
	PropertyRNA *prop;

	/* mtex */
	prop = RNA_def_property(srna, "texture_slots", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, structname);
	RNA_def_property_collection_funcs(prop, begin, "rna_iterator_array_next", "rna_iterator_array_end",
	                                  "rna_iterator_array_dereference_get", NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Textures", "Texture slots defining the mapping and influence of textures");
	rna_def_texture_slots(brna, prop, structname, structname_slots);

	prop = RNA_def_property(srna, "active_texture", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Texture");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	if (activeeditable)
		RNA_def_property_editable_func(prop, activeeditable);
	RNA_def_property_pointer_funcs(prop, activeget, activeset, NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Texture", "Active texture slot being displayed");
	RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING_LINKS, update);

	prop = RNA_def_property(srna, "active_texture_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "texact");
	RNA_def_property_range(prop, 0, MAX_MTEX - 1);
	RNA_def_property_ui_text(prop, "Active Texture Index", "Index of active texture slot");
	RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING_LINKS, update_index);
}

#endif
