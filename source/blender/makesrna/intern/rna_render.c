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
 * Contributor(s): Blender Foundation (2009)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "DNA_scene_types.h"

#include "RNA_define.h"

#include "rna_internal.h"


#include "RE_pipeline.h"

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "BKE_context.h"
#include "BKE_report.h"


/* RenderEngine */

static RenderEngineType internal_render_type = {
	NULL, NULL, "BLENDER_RENDER", "Blender Render", RE_INTERNAL, NULL, {NULL, NULL, NULL, NULL}};
#if GAMEBLENDER == 1
static RenderEngineType internal_game_type = {
	NULL, NULL, "BLENDER_GAME", "Blender Game", RE_INTERNAL|RE_GAME, NULL, {NULL, NULL, NULL, NULL}};
#endif

ListBase R_engines = {NULL, NULL};

void RE_engines_init()
{
	BLI_addtail(&R_engines, &internal_render_type);
#if GAMEBLENDER == 1
	BLI_addtail(&R_engines, &internal_game_type);
#endif
}

void RE_engines_exit()
{
	RenderEngineType *type, *next;

	for(type=R_engines.first; type; type=next) {
		next= type->next;

		BLI_remlink(&R_engines, type);

		if(!(type->flag & RE_INTERNAL)) {
			if(type->ext.free)
				type->ext.free(type->ext.data);

			MEM_freeN(type);
		}
	}
}

static void engine_render(RenderEngine *engine, struct Scene *scene)
{
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	func= RNA_struct_find_function(&ptr, "render");

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "scene", &scene);
	engine->type->ext.call(&ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void rna_RenderEngine_unregister(const bContext *C, StructRNA *type)
{
	RenderEngineType *et= RNA_struct_blender_type_get(type);

	if(!et)
		return;
	
	RNA_struct_free_extension(type, &et->ext);
	BLI_freelinkN(&R_engines, et);
	RNA_struct_free(&BLENDER_RNA, type);
}

static StructRNA *rna_RenderEngine_register(const bContext *C, ReportList *reports, void *data, const char *identifier, StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	RenderEngineType *et, dummyet = {0};
	RenderEngine dummyengine= {0};
	PointerRNA dummyptr;
	int have_function[1];

	/* setup dummy engine & engine type to store static properties in */
	dummyengine.type= &dummyet;
	RNA_pointer_create(NULL, &RNA_RenderEngine, &dummyengine, &dummyptr);

	/* validate the python class */
	if(validate(&dummyptr, data, have_function) != 0)
		return NULL;

	if(strlen(identifier) >= sizeof(dummyet.idname)) {
		BKE_reportf(reports, RPT_ERROR, "registering render engine class: '%s' is too long, maximum length is %d.", identifier, sizeof(dummyet.idname));
		return NULL;
	}

	/* check if we have registered this engine type before, and remove it */
	for(et=R_engines.first; et; et=et->next) {
		if(strcmp(et->idname, dummyet.idname) == 0) {
			if(et->ext.srna)
				rna_RenderEngine_unregister(C, et->ext.srna);
			break;
		}
	}
	
	/* create a new engine type */
	et= MEM_callocN(sizeof(RenderEngineType), "python buttons engine");
	memcpy(et, &dummyet, sizeof(dummyet));

	et->ext.srna= RNA_def_struct(&BLENDER_RNA, et->idname, "RenderEngine"); 
	et->ext.data= data;
	et->ext.call= call;
	et->ext.free= free;
	RNA_struct_blender_type_set(et->ext.srna, et);

	et->render= (have_function[0])? engine_render: NULL;

	BLI_addtail(&R_engines, et);

	return et->ext.srna;
}

static StructRNA* rna_RenderEngine_refine(PointerRNA *ptr)
{
	RenderEngine *engine= (RenderEngine*)ptr->data;
	return (engine->type && engine->type->ext.srna)? engine->type->ext.srna: &RNA_RenderEngine;
}

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

static int rna_RenderLayer_rect_get_length(PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION])
{
	RenderLayer *rl= (RenderLayer*)ptr->data;

	length[0]= rl->rectx*rl->recty;
	length[1]= 4;

	return length[0]*length[1];
}

static void rna_RenderLayer_rect_get(PointerRNA *ptr, float *values)
{
	RenderLayer *rl= (RenderLayer*)ptr->data;
	memcpy(values, rl->rectf, sizeof(float)*rl->rectx*rl->recty*4);
}

static void rna_RenderLayer_rect_set(PointerRNA *ptr, const float *values)
{
	RenderLayer *rl= (RenderLayer*)ptr->data;
	memcpy(rl->rectf, values, sizeof(float)*rl->rectx*rl->recty*4);
}

static int rna_RenderPass_rect_get_length(PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION])
{
	RenderPass *rpass= (RenderPass*)ptr->data;

	length[0]= rpass->rectx*rpass->recty;
	length[1]= rpass->channels;

	return length[0]*length[1];
}

static void rna_RenderPass_rect_get(PointerRNA *ptr, float *values)
{
	RenderPass *rpass= (RenderPass*)ptr->data;
	printf("rect get\n");
	memcpy(values, rpass->rect, sizeof(float)*rpass->rectx*rpass->recty*rpass->channels);
}

static void rna_RenderPass_rect_set(PointerRNA *ptr, const float *values)
{
	RenderPass *rpass= (RenderPass*)ptr->data;
	printf("rect set\n");
	memcpy(rpass->rect, values, sizeof(float)*rpass->rectx*rpass->recty*rpass->channels);
}

#else // RNA_RUNTIME

static void rna_def_render_engine(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	
	srna= RNA_def_struct(brna, "RenderEngine", NULL);
	RNA_def_struct_sdna(srna, "RenderEngine");
	RNA_def_struct_ui_text(srna, "Render Engine", "Render engine");
	RNA_def_struct_refine_func(srna, "rna_RenderEngine_refine");
	RNA_def_struct_register_funcs(srna, "rna_RenderEngine_register", "rna_RenderEngine_unregister");

	/* render */
	func= RNA_def_function(srna, "render", NULL);
	RNA_def_function_ui_description(func, "Render scene into an image.");
	RNA_def_function_flag(func, FUNC_REGISTER);
	RNA_def_pointer(func, "scene", "Scene", "", "");

	func= RNA_def_function(srna, "begin_result", "RE_engine_begin_result");
	prop= RNA_def_int(func, "x", 0, 0, INT_MAX, "X", "", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop= RNA_def_int(func, "y", 0, 0, INT_MAX, "Y", "", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop= RNA_def_int(func, "w", 0, 0, INT_MAX, "Width", "", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop= RNA_def_int(func, "h", 0, 0, INT_MAX, "Height", "", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop= RNA_def_pointer(func, "result", "RenderResult", "Result", "");
	RNA_def_function_return(func, prop);

	func= RNA_def_function(srna, "update_result", "RE_engine_update_result");
	prop= RNA_def_pointer(func, "result", "RenderResult", "Result", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);

	func= RNA_def_function(srna, "end_result", "RE_engine_end_result");
	prop= RNA_def_pointer(func, "result", "RenderResult", "Result", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);

	func= RNA_def_function(srna, "test_break", "RE_engine_test_break");
	prop= RNA_def_boolean(func, "do_break", 0, "Break", "");
	RNA_def_function_return(func, prop);

	func= RNA_def_function(srna, "update_stats", "RE_engine_update_stats");
	prop= RNA_def_string(func, "stats", "", 0, "Stats", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop= RNA_def_string(func, "info", "", 0, "Info", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);

	/* registration */
	RNA_define_verify_sdna(0);

	prop= RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->idname");
	RNA_def_property_flag(prop, PROP_REGISTER);

	prop= RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->name");
	RNA_def_property_flag(prop, PROP_REGISTER);

	prop= RNA_def_property(srna, "bl_preview", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "type->flag", RE_DO_PREVIEW);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	RNA_define_verify_sdna(1);
}

static void rna_def_render_result(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	
	srna= RNA_def_struct(brna, "RenderResult", NULL);
	RNA_def_struct_ui_text(srna, "Render Result", "Result of rendering, including all layers and passes");

	func= RNA_def_function(srna, "load_from_file", "RE_result_load_from_file");
	RNA_def_function_ui_description(func, "Copies the pixels of this render result from an image file.");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	prop= RNA_def_string(func, "filename", "", 0, "Filename", "Filename to load into this render tile, must be no smaller then the render result");
	RNA_def_property_flag(prop, PROP_REQUIRED);

	RNA_define_verify_sdna(0);

	prop= RNA_def_property(srna, "resolution_x", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "rectx");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop= RNA_def_property(srna, "resolution_y", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "recty");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop= RNA_def_property(srna, "layers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "RenderLayer");
	RNA_def_property_collection_funcs(prop, "rna_RenderResult_layers_begin", "rna_iterator_listbase_next", "rna_iterator_listbase_end", "rna_iterator_listbase_get", 0, 0, 0);

	RNA_define_verify_sdna(1);
}

static void rna_def_render_layer(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	
	srna= RNA_def_struct(brna, "RenderLayer", NULL);
	RNA_def_struct_ui_text(srna, "Render Layer", "");

	func= RNA_def_function(srna, "load_from_file", "RE_layer_load_from_file");
	RNA_def_function_ui_description(func, "Copies the pixels of this renderlayer from an image file.");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	prop= RNA_def_string(func, "filename", "", 0, "Filename", "Filename to load into this render tile, must be no smaller then the renderlayer");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	
	RNA_define_verify_sdna(0);

	rna_def_render_layer_common(srna, 0);

	prop= RNA_def_property(srna, "passes", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "RenderPass");
	RNA_def_property_collection_funcs(prop, "rna_RenderLayer_passes_begin", "rna_iterator_listbase_next", "rna_iterator_listbase_end", "rna_iterator_listbase_get", 0, 0, 0);

	prop= RNA_def_property(srna, "rect", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_DYNAMIC);
	RNA_def_property_multi_array(prop, 2, NULL);
	RNA_def_property_dynamic_array_funcs(prop, "rna_RenderLayer_rect_get_length");
	RNA_def_property_float_funcs(prop, "rna_RenderLayer_rect_get", "rna_RenderLayer_rect_set", NULL);

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
		{SCE_PASS_VECTOR, "VECTOR", 0, "Vector", ""},
		{SCE_PASS_REFRACT, "REFRACTION", 0, "Refraction", ""},
		{SCE_PASS_INDEXOB, "OBJECT_INDEX", 0, "Object Index", ""},
		{SCE_PASS_UV, "UV", 0, "UV", ""},
		{SCE_PASS_MIST, "MIST", 0, "Mist", ""},
		{SCE_PASS_EMIT, "EMIT", 0, "Emit", ""},
		{SCE_PASS_ENVIRONMENT, "ENVIRONMENT", 0, "Environment", ""},
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

	prop= RNA_def_property(srna, "rect", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_DYNAMIC);
	RNA_def_property_multi_array(prop, 2, NULL);
	RNA_def_property_dynamic_array_funcs(prop, "rna_RenderPass_rect_get_length");
	RNA_def_property_float_funcs(prop, "rna_RenderPass_rect_get", "rna_RenderPass_rect_set", NULL);

	RNA_define_verify_sdna(1);
}

void RNA_def_render(BlenderRNA *brna)
{
	rna_def_render_engine(brna);
	rna_def_render_result(brna);
	rna_def_render_layer(brna);
	rna_def_render_pass(brna);
}

#endif // RNA_RUNTIME

