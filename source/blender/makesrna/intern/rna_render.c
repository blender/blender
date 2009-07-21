/**
 * $Id: rna_render.c 21648 2009-07-17 02:31:28Z campbellbarton $
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
 * Contributor(s): Blender Foundation (2009)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "DNA_scene_types.h"

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "WM_types.h"

#include "RE_pipeline.h"
#include "RE_render_ext.h"

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "BKE_context.h"
#include "BKE_report.h"

#include "WM_api.h"

static void rna_RenderResult_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	RenderResult *rr= (RenderResult*)ptr->data;
	rna_iterator_listbase_begin(iter, &rr->layers, NULL);
}

static void rna_RenderLayer_passes_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	RenderLayer *rl= (RenderLayer*)ptr->data;
	rna_iterator_listbase_begin(iter, &rl->passes, NULL);
}

static float rna_RenderValue_value_get(PointerRNA *ptr)
{
	return *(float*)ptr->data;
}

static void rna_RenderValue_value_set(PointerRNA *ptr, float value)
{
	*(float*)ptr->data= value;
}

static void rna_RenderLayer_rect_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	RenderLayer *rl= (RenderLayer*)ptr->data;
	rna_iterator_array_begin(iter, (void*)rl->rectf, sizeof(float), rl->rectx*rl->recty*4, 0, NULL);
}

static int rna_RenderLayer_rect_length(PointerRNA *ptr)
{
	RenderLayer *rl= (RenderLayer*)ptr->data;
	return rl->rectx*rl->recty*4;
}

static void rna_RenderPass_rect_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	RenderPass *rpass= (RenderPass*)ptr->data;
	rna_iterator_array_begin(iter, (void*)rpass->rect, sizeof(float), rpass->rectx*rpass->recty*rpass->channels, 0, NULL);
}

static int rna_RenderPass_rect_length(PointerRNA *ptr)
{
	RenderPass *rpass= (RenderPass*)ptr->data;
	return rpass->rectx*rpass->recty*rpass->channels;
}


#else // RNA_RUNTIME

static void rna_def_render_result(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "RenderResult", NULL);
	RNA_def_struct_ui_text(srna, "Render Result", "Result of rendering, including all layers and passes.");

	RNA_define_verify_sdna(0);

	prop= RNA_def_property(srna, "resolution_x", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "rectx");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop= RNA_def_property(srna, "resolution_y", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "recty");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop= RNA_def_property(srna, "layers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "RenderLayer");
	RNA_def_property_collection_funcs(prop, "rna_RenderResult_layers_begin", "rna_iterator_listbase_next", "rna_iterator_listbase_end", "rna_iterator_listbase_get", 0, 0, 0, 0, 0);

	RNA_define_verify_sdna(1);
}

static void rna_def_render_layer(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "RenderLayer", NULL);
	RNA_def_struct_ui_text(srna, "Render Layer", "");

	RNA_define_verify_sdna(0);

	rna_def_render_layer_common(srna, 0);

	prop= RNA_def_property(srna, "passes", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "RenderPass");
	RNA_def_property_collection_funcs(prop, "rna_RenderLayer_passes_begin", "rna_iterator_listbase_next", "rna_iterator_listbase_end", "rna_iterator_listbase_get", 0, 0, 0, 0, 0);

	prop= RNA_def_property(srna, "rect", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "RenderValue");
	RNA_def_property_collection_funcs(prop, "rna_RenderLayer_rect_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get", "rna_RenderLayer_rect_length", 0, 0, 0, 0);

	/* value */
	srna= RNA_def_struct(brna, "RenderValue", NULL);
	RNA_def_struct_ui_text(srna, "Render Value", "");

	prop= RNA_def_property(srna, "value", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_RenderValue_value_get", "rna_RenderValue_value_set", NULL);

	RNA_define_verify_sdna(1);
}

static void rna_def_render_pass(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem pass_type_items[]= {
		{SCE_PASS_COMBINED, "COMBINED", 0, "Combined", ""},
		{SCE_PASS_Z, "Z", 0, "Z", ""},
		{SCE_PASS_RGBA, "COLOR", 0, "Color", ""},
		{SCE_PASS_DIFFUSE, "DIFFUSE", 0, "Diffuse", ""},
		{SCE_PASS_SPEC, "SPECULAR", 0, "Specular", ""},
		{SCE_PASS_SHADOW, "SHADOW", 0, "Shadow", ""},
		{SCE_PASS_AO, "AO", 0, "AO", ""},
		{SCE_PASS_REFLECT, "REFLECTION", 0, "Reflection", ""},
		{SCE_PASS_NORMAL, "NORMAL", 0, "Normal", ""},
		{SCE_PASS_VECTOR, "VECTOR", 0, "Vecotr", ""},
		{SCE_PASS_REFRACT, "REFRACTION", 0, "Refraction", ""},
		{SCE_PASS_INDEXOB, "OBJECT_INDEX", 0, "Object Index", ""},
		{SCE_PASS_UV, "UV", 0, "UV", ""},
		{SCE_PASS_MIST, "MIST", 0, "Mist", ""},
		{0, NULL, 0, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "RenderPass", NULL);
	RNA_def_struct_ui_text(srna, "Render Pass", "");

	RNA_define_verify_sdna(0);

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_struct_name_property(srna, prop);

	prop= RNA_def_property(srna, "channel_id", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "chan_id");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop= RNA_def_property(srna, "channels", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "channels");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "passtype");
	RNA_def_property_enum_items(prop, pass_type_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop= RNA_def_property(srna, "rect", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "RenderValue");
	RNA_def_property_collection_funcs(prop, "rna_RenderPass_rect_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get", "rna_RenderPass_rect_length", 0, 0, 0, 0);

	RNA_define_verify_sdna(1);
}

void RNA_def_render(BlenderRNA *brna)
{
	rna_def_render_result(brna);
	rna_def_render_layer(brna);
	rna_def_render_pass(brna);
}

#endif // RNA_RUNTIME

