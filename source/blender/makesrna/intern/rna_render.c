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
 * Contributor(s): Blender Foundation (2009)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_render.c
 *  \ingroup RNA
 */


#include <stdlib.h>

#include "DNA_scene_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "BKE_utildefines.h"

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "BKE_context.h"
#include "BKE_report.h"

/* RenderEngine Callbacks */

void engine_tag_redraw(RenderEngine *engine)
{
	engine->flag |= RE_ENGINE_DO_DRAW;
}

void engine_tag_update(RenderEngine *engine)
{
	engine->flag |= RE_ENGINE_DO_UPDATE;
}

static void engine_update(RenderEngine *engine, Main *bmain, Scene *scene)
{
	extern FunctionRNA rna_RenderEngine_update_func;
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	func = &rna_RenderEngine_update_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "data", &bmain);
	RNA_parameter_set_lookup(&list, "scene", &scene);
	engine->type->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void engine_render(RenderEngine *engine, struct Scene *scene)
{
	extern FunctionRNA rna_RenderEngine_render_func;
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	func = &rna_RenderEngine_render_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "scene", &scene);
	engine->type->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void engine_view_update(RenderEngine *engine, const struct bContext *context)
{
	extern FunctionRNA rna_RenderEngine_view_update_func;
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	func = &rna_RenderEngine_view_update_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "context", &context);
	engine->type->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void engine_view_draw(RenderEngine *engine, const struct bContext *context)
{
	extern FunctionRNA rna_RenderEngine_view_draw_func;
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	func = &rna_RenderEngine_view_draw_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "context", &context);
	engine->type->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

/* RenderEngine registration */

static void rna_RenderEngine_unregister(Main *UNUSED(bmain), StructRNA *type)
{
	RenderEngineType *et = RNA_struct_blender_type_get(type);

	if (!et)
		return;
	
	RNA_struct_free_extension(type, &et->ext);
	BLI_freelinkN(&R_engines, et);
	RNA_struct_free(&BLENDER_RNA, type);
}

static StructRNA *rna_RenderEngine_register(Main *bmain, ReportList *reports, void *data, const char *identifier,
                                            StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	RenderEngineType *et, dummyet = {NULL};
	RenderEngine dummyengine = {NULL};
	PointerRNA dummyptr;
	int have_function[4];

	/* setup dummy engine & engine type to store static properties in */
	dummyengine.type = &dummyet;
	RNA_pointer_create(NULL, &RNA_RenderEngine, &dummyengine, &dummyptr);

	/* validate the python class */
	if (validate(&dummyptr, data, have_function) != 0)
		return NULL;

	if (strlen(identifier) >= sizeof(dummyet.idname)) {
		BKE_reportf(reports, RPT_ERROR, "registering render engine class: '%s' is too long, maximum length is %d",
		            identifier, (int)sizeof(dummyet.idname));
		return NULL;
	}

	/* check if we have registered this engine type before, and remove it */
	for (et = R_engines.first; et; et = et->next) {
		if (strcmp(et->idname, dummyet.idname) == 0) {
			if (et->ext.srna)
				rna_RenderEngine_unregister(bmain, et->ext.srna);
			break;
		}
	}
	
	/* create a new engine type */
	et = MEM_callocN(sizeof(RenderEngineType), "python render engine");
	memcpy(et, &dummyet, sizeof(dummyet));

	et->ext.srna = RNA_def_struct(&BLENDER_RNA, et->idname, "RenderEngine");
	et->ext.data = data;
	et->ext.call = call;
	et->ext.free = free;
	RNA_struct_blender_type_set(et->ext.srna, et);

	et->update = (have_function[0]) ? engine_update : NULL;
	et->render = (have_function[1]) ? engine_render : NULL;
	et->view_update = (have_function[2]) ? engine_view_update : NULL;
	et->view_draw = (have_function[3]) ? engine_view_draw : NULL;

	BLI_addtail(&R_engines, et);

	return et->ext.srna;
}

static void **rna_RenderEngine_instance(PointerRNA *ptr)
{
	RenderEngine *engine = ptr->data;
	return &engine->py_instance;
}

static StructRNA *rna_RenderEngine_refine(PointerRNA *ptr)
{
	RenderEngine *engine = (RenderEngine *)ptr->data;
	return (engine->type && engine->type->ext.srna) ? engine->type->ext.srna : &RNA_RenderEngine;
}

static void rna_RenderResult_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	RenderResult *rr = (RenderResult *)ptr->data;
	rna_iterator_listbase_begin(iter, &rr->layers, NULL);
}

static void rna_RenderLayer_passes_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	RenderLayer *rl = (RenderLayer *)ptr->data;
	rna_iterator_listbase_begin(iter, &rl->passes, NULL);
}

static int rna_RenderLayer_rect_get_length(PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION])
{
	RenderLayer *rl = (RenderLayer *)ptr->data;

	length[0] = rl->rectx * rl->recty;
	length[1] = 4;

	return length[0] * length[1];
}

static void rna_RenderLayer_rect_get(PointerRNA *ptr, float *values)
{
	RenderLayer *rl = (RenderLayer *)ptr->data;
	memcpy(values, rl->rectf, sizeof(float) * rl->rectx * rl->recty * 4);
}

void rna_RenderLayer_rect_set(PointerRNA *ptr, const float *values)
{
	RenderLayer *rl = (RenderLayer *)ptr->data;
	memcpy(rl->rectf, values, sizeof(float) * rl->rectx * rl->recty * 4);
}

static int rna_RenderPass_rect_get_length(PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION])
{
	RenderPass *rpass = (RenderPass *)ptr->data;

	length[0] = rpass->rectx * rpass->recty;
	length[1] = rpass->channels;

	return length[0] * length[1];
}

static void rna_RenderPass_rect_get(PointerRNA *ptr, float *values)
{
	RenderPass *rpass = (RenderPass *)ptr->data;
	memcpy(values, rpass->rect, sizeof(float) * rpass->rectx * rpass->recty * rpass->channels);
}

void rna_RenderPass_rect_set(PointerRNA *ptr, const float *values)
{
	RenderPass *rpass = (RenderPass *)ptr->data;
	memcpy(rpass->rect, values, sizeof(float) * rpass->rectx * rpass->recty * rpass->channels);
}

#else /* RNA_RUNTIME */

static void rna_def_render_engine(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	
	srna = RNA_def_struct(brna, "RenderEngine", NULL);
	RNA_def_struct_sdna(srna, "RenderEngine");
	RNA_def_struct_ui_text(srna, "Render Engine", "Render engine");
	RNA_def_struct_refine_func(srna, "rna_RenderEngine_refine");
	RNA_def_struct_register_funcs(srna, "rna_RenderEngine_register", "rna_RenderEngine_unregister",
	                              "rna_RenderEngine_instance");

	/* final render callbacks */
	func = RNA_def_function(srna, "update", NULL);
	RNA_def_function_ui_description(func, "Export scene data for render");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	RNA_def_pointer(func, "data", "BlendData", "", "");
	RNA_def_pointer(func, "scene", "Scene", "", "");

	func = RNA_def_function(srna, "render", NULL);
	RNA_def_function_ui_description(func, "Render scene into an image");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	RNA_def_pointer(func, "scene", "Scene", "", "");

	/* viewport render callbacks */
	func = RNA_def_function(srna, "view_update", NULL);
	RNA_def_function_ui_description(func, "Update on data changes for viewport render");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	RNA_def_pointer(func, "context", "Context", "", "");

	func = RNA_def_function(srna, "view_draw", NULL);
	RNA_def_function_ui_description(func, "Draw viewport render");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	RNA_def_pointer(func, "context", "Context", "", "");

	/* tag for redraw */
	RNA_def_function(srna, "tag_redraw", "engine_tag_redraw");
	RNA_def_function_ui_description(func, "Request redraw for viewport rendering");

	/* tag for update */
	RNA_def_function(srna, "tag_update", "engine_tag_update");
	RNA_def_function_ui_description(func, "Request update call for viewport rendering");

	func = RNA_def_function(srna, "begin_result", "RE_engine_begin_result");
	prop = RNA_def_int(func, "x", 0, 0, INT_MAX, "X", "", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_int(func, "y", 0, 0, INT_MAX, "Y", "", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_int(func, "w", 0, 0, INT_MAX, "Width", "", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_int(func, "h", 0, 0, INT_MAX, "Height", "", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_pointer(func, "result", "RenderResult", "Result", "");
	RNA_def_function_return(func, prop);

	func = RNA_def_function(srna, "update_result", "RE_engine_update_result");
	prop = RNA_def_pointer(func, "result", "RenderResult", "Result", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);

	func = RNA_def_function(srna, "end_result", "RE_engine_end_result");
	prop = RNA_def_pointer(func, "result", "RenderResult", "Result", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);

	func = RNA_def_function(srna, "test_break", "RE_engine_test_break");
	prop = RNA_def_boolean(func, "do_break", 0, "Break", "");
	RNA_def_function_return(func, prop);

	func = RNA_def_function(srna, "update_stats", "RE_engine_update_stats");
	prop = RNA_def_string(func, "stats", "", 0, "Stats", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_string(func, "info", "", 0, "Info", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);

	func = RNA_def_function(srna, "update_progress", "RE_engine_update_progress");
	prop = RNA_def_float(func, "progress", 0, 0.0f, 1.0f, "", "Percentage of render that's done", 0.0f, 1.0f);
	RNA_def_property_flag(prop, PROP_REQUIRED);

	func = RNA_def_function(srna, "report", "RE_engine_report");
	prop = RNA_def_enum_flag(func, "type", wm_report_items, 0, "Type", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_string(func, "message", "", 0, "Report Message", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);

	RNA_define_verify_sdna(0);

	prop = RNA_def_property(srna, "is_animation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RE_ENGINE_ANIMATION);

	prop = RNA_def_property(srna, "is_preview", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RE_ENGINE_PREVIEW);

	prop = RNA_def_property(srna, "camera_override", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "camera_override");
	RNA_def_property_struct_type(prop, "Object");

	/* registration */

	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->idname");
	RNA_def_property_flag(prop, PROP_REGISTER | PROP_NEVER_CLAMP);

	prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_TRANSLATE);
	RNA_def_property_string_sdna(prop, NULL, "type->name");
	RNA_def_property_flag(prop, PROP_REGISTER);

	prop = RNA_def_property(srna, "bl_use_preview", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "type->flag", RE_USE_PREVIEW);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_use_postprocess", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "type->flag", RE_USE_POSTPROCESS);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_use_shading_nodes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "type->flag", RE_USE_SHADING_NODES);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	RNA_define_verify_sdna(1);
}

static void rna_def_render_result(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;
	
	srna = RNA_def_struct(brna, "RenderResult", NULL);
	RNA_def_struct_ui_text(srna, "Render Result", "Result of rendering, including all layers and passes");

	func = RNA_def_function(srna, "load_from_file", "RE_result_load_from_file");
	RNA_def_function_ui_description(func, "Copies the pixels of this render result from an image file");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_string_file_name(func, "filename", "", FILE_MAX, "File Name",
	                                "Filename to load into this render tile, must be no smaller than "
	                                "the render result");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	RNA_define_verify_sdna(0);

	parm = RNA_def_property(srna, "resolution_x", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(parm, NULL, "rectx");
	RNA_def_property_clear_flag(parm, PROP_EDITABLE);

	parm = RNA_def_property(srna, "resolution_y", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(parm, NULL, "recty");
	RNA_def_property_clear_flag(parm, PROP_EDITABLE);

	parm = RNA_def_property(srna, "layers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(parm, "RenderLayer");
	RNA_def_property_collection_funcs(parm, "rna_RenderResult_layers_begin", "rna_iterator_listbase_next",
	                                  "rna_iterator_listbase_end", "rna_iterator_listbase_get",
	                                  NULL, NULL, NULL, NULL);

	RNA_define_verify_sdna(1);
}

static void rna_def_render_layer(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	
	srna = RNA_def_struct(brna, "RenderLayer", NULL);
	RNA_def_struct_ui_text(srna, "Render Layer", "");

	func = RNA_def_function(srna, "load_from_file", "RE_layer_load_from_file");
	RNA_def_function_ui_description(func, "Copies the pixels of this renderlayer from an image file");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	prop = RNA_def_string(func, "filename", "", 0, "Filename",
	                      "Filename to load into this render tile, must be no smaller than the renderlayer");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	RNA_def_int(func, "x", 0, 0, INT_MAX, "Offset X",
	            "Offset the position to copy from if the image is larger than the render layer", 0, INT_MAX);
	RNA_def_int(func, "y", 0, 0, INT_MAX, "Offset Y",
	            "Offset the position to copy from if the image is larger than the render layer", 0, INT_MAX);

	RNA_define_verify_sdna(0);

	rna_def_render_layer_common(srna, 0);

	prop = RNA_def_property(srna, "passes", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "RenderPass");
	RNA_def_property_collection_funcs(prop, "rna_RenderLayer_passes_begin", "rna_iterator_listbase_next",
	                                  "rna_iterator_listbase_end", "rna_iterator_listbase_get",
	                                  NULL, NULL, NULL, NULL);

	prop = RNA_def_property(srna, "rect", PROP_FLOAT, PROP_NONE);
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

	static EnumPropertyItem pass_type_items[] = {
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
		{SCE_PASS_INDEXMA, "MATERIAL_INDEX", 0, "Material Index", ""},
		{SCE_PASS_DIFFUSE_DIRECT, "DIFFUSE_DIRECT", 0, "Diffuse Direct", ""},
		{SCE_PASS_DIFFUSE_INDIRECT, "DIFFUSE_INDIRECT", 0, "Diffuse Indirect", ""},
		{SCE_PASS_DIFFUSE_COLOR, "DIFFUSE_COLOR", 0, "Diffuse Color", ""},
		{SCE_PASS_GLOSSY_DIRECT, "GLOSSY_DIRECT", 0, "Glossy Direct", ""},
		{SCE_PASS_GLOSSY_INDIRECT, "GLOSSY_INDIRECT", 0, "Glossy Indirect", ""},
		{SCE_PASS_GLOSSY_COLOR, "GLOSSY_COLOR", 0, "Glossy Color", ""},
		{SCE_PASS_TRANSM_DIRECT, "TRANSMISSION_DIRECT", 0, "Transmission Direct", ""},
		{SCE_PASS_TRANSM_INDIRECT, "TRANSMISSION_INDIRECT", 0, "Transmission Indirect", ""},
		{SCE_PASS_TRANSM_COLOR, "TRANSMISSION_COLOR", 0, "Transmission Color", ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "RenderPass", NULL);
	RNA_def_struct_ui_text(srna, "Render Pass", "");

	RNA_define_verify_sdna(0);

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_struct_name_property(srna, prop);

	prop = RNA_def_property(srna, "channel_id", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "chan_id");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "channels", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "channels");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "passtype");
	RNA_def_property_enum_items(prop, pass_type_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "rect", PROP_FLOAT, PROP_NONE);
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

#endif /* RNA_RUNTIME */
