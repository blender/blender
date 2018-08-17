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

#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BLI_utildefines.h"
#include "BLI_path_util.h"

#include "BKE_scene.h"
#include "BKE_image.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "ED_render.h"

/* Deprecated, only provided for API compatibility. */
const EnumPropertyItem rna_enum_render_pass_type_items[] = {
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
	{SCE_PASS_SUBSURFACE_DIRECT, "SUBSURFACE_DIRECT", 0, "Subsurface Direct", ""},
	{SCE_PASS_SUBSURFACE_INDIRECT, "SUBSURFACE_INDIRECT", 0, "Subsurface Indirect", ""},
	{SCE_PASS_SUBSURFACE_COLOR, "SUBSURFACE_COLOR", 0, "Subsurface Color", ""},
#ifdef WITH_CYCLES_DEBUG
	{SCE_PASS_DEBUG, "DEBUG", 0, "Pass used for render engine debugging", ""},
#endif
	{0, NULL, 0, NULL, NULL}
};

const EnumPropertyItem rna_enum_bake_pass_type_items[] = {
	{SCE_PASS_COMBINED, "COMBINED", 0, "Combined", ""},
	{SCE_PASS_AO, "AO", 0, "AO", ""},
	{SCE_PASS_SHADOW, "SHADOW", 0, "Shadow", ""},
	{SCE_PASS_NORMAL, "NORMAL", 0, "Normal", ""},
	{SCE_PASS_UV, "UV", 0, "UV", ""},
	{SCE_PASS_ROUGHNESS, "ROUGHNESS", 0, "ROUGHNESS", ""},
	{SCE_PASS_EMIT, "EMIT", 0, "Emit", ""},
	{SCE_PASS_ENVIRONMENT, "ENVIRONMENT", 0, "Environment", ""},
	{SCE_PASS_DIFFUSE_COLOR, "DIFFUSE", 0, "Diffuse", ""},
	{SCE_PASS_GLOSSY_COLOR, "GLOSSY", 0, "Glossy", ""},
	{SCE_PASS_TRANSM_COLOR, "TRANSMISSION", 0, "Transmission", ""},
	{SCE_PASS_SUBSURFACE_COLOR, "SUBSURFACE", 0, "Subsurface", ""},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "BKE_context.h"
#include "BKE_report.h"

#include "IMB_colormanagement.h"
#include "GPU_extensions.h"

/* RenderEngine Callbacks */

static void engine_tag_redraw(RenderEngine *engine)
{
	engine->flag |= RE_ENGINE_DO_DRAW;
}

static void engine_tag_update(RenderEngine *engine)
{
	engine->flag |= RE_ENGINE_DO_UPDATE;
}

static bool engine_support_display_space_shader(RenderEngine *UNUSED(engine), Scene *scene)
{
	return IMB_colormanagement_support_glsl_draw(&scene->view_settings);
}

static int engine_get_preview_pixel_size(RenderEngine *UNUSED(engine), Scene *scene)
{
	return BKE_render_preview_pixel_size(&scene->r);
}

static void engine_bind_display_space_shader(RenderEngine *UNUSED(engine), Scene *scene)
{
	IMB_colormanagement_setup_glsl_draw(&scene->view_settings,
	                                    &scene->display_settings,
	                                    scene->r.dither_intensity,
	                                    false);
}

static void engine_unbind_display_space_shader(RenderEngine *UNUSED(engine))
{
	IMB_colormanagement_finish_glsl_draw();
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

static void engine_bake(RenderEngine *engine, struct Scene *scene,
                        struct Object *object, const int pass_type, const int pass_filter,
                        const int object_id, const struct BakePixel *pixel_array,
                        const int num_pixels, const int depth, void *result)
{
	extern FunctionRNA rna_RenderEngine_bake_func;
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	func = &rna_RenderEngine_bake_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "scene", &scene);
	RNA_parameter_set_lookup(&list, "object", &object);
	RNA_parameter_set_lookup(&list, "pass_type", &pass_type);
	RNA_parameter_set_lookup(&list, "pass_filter", &pass_filter);
	RNA_parameter_set_lookup(&list, "object_id", &object_id);
	RNA_parameter_set_lookup(&list, "pixel_array", &pixel_array);
	RNA_parameter_set_lookup(&list, "num_pixels", &num_pixels);
	RNA_parameter_set_lookup(&list, "depth", &depth);
	RNA_parameter_set_lookup(&list, "result", &result);
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

static void engine_update_script_node(RenderEngine *engine, struct bNodeTree *ntree, struct bNode *node)
{
	extern FunctionRNA rna_RenderEngine_update_script_node_func;
	PointerRNA ptr, nodeptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	RNA_pointer_create((ID *)ntree, &RNA_Node, node, &nodeptr);
	func = &rna_RenderEngine_update_script_node_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "node", &nodeptr);
	engine->type->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void engine_update_render_passes(RenderEngine *engine, struct Scene *scene, struct SceneRenderLayer *srl)
{
	extern FunctionRNA rna_RenderEngine_update_render_passes_func;
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	func = &rna_RenderEngine_update_render_passes_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "scene", &scene);
	RNA_parameter_set_lookup(&list, "renderlayer", &srl);
	engine->type->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

/* RenderEngine registration */

static void rna_RenderEngine_unregister(Main *bmain, StructRNA *type)
{
	RenderEngineType *et = RNA_struct_blender_type_get(type);

	if (!et)
		return;

	RNA_struct_free_extension(type, &et->ext);
	RNA_struct_free(&BLENDER_RNA, type);
	BLI_freelinkN(&R_engines, et);

	/* Stop all renders in case we were using this one. */
	ED_render_engine_changed(bmain);
}

static StructRNA *rna_RenderEngine_register(
        Main *bmain, ReportList *reports, void *data, const char *identifier,
        StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	RenderEngineType *et, dummyet = {NULL};
	RenderEngine dummyengine = {NULL};
	PointerRNA dummyptr;
	int have_function[7];

	/* setup dummy engine & engine type to store static properties in */
	dummyengine.type = &dummyet;
	dummyet.flag |= RE_USE_SHADING_NODES_CUSTOM;
	RNA_pointer_create(NULL, &RNA_RenderEngine, &dummyengine, &dummyptr);

	/* validate the python class */
	if (validate(&dummyptr, data, have_function) != 0)
		return NULL;

	if (strlen(identifier) >= sizeof(dummyet.idname)) {
		BKE_reportf(reports, RPT_ERROR, "Registering render engine class: '%s' is too long, maximum length is %d",
		            identifier, (int)sizeof(dummyet.idname));
		return NULL;
	}

	/* check if we have registered this engine type before, and remove it */
	for (et = R_engines.first; et; et = et->next) {
		if (STREQ(et->idname, dummyet.idname)) {
			if (et->ext.srna)
				rna_RenderEngine_unregister(bmain, et->ext.srna);
			break;
		}
	}

	/* create a new engine type */
	et = MEM_callocN(sizeof(RenderEngineType), "python render engine");
	memcpy(et, &dummyet, sizeof(dummyet));

	et->ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, et->idname, &RNA_RenderEngine);
	et->ext.data = data;
	et->ext.call = call;
	et->ext.free = free;
	RNA_struct_blender_type_set(et->ext.srna, et);

	et->update = (have_function[0]) ? engine_update : NULL;
	et->render = (have_function[1]) ? engine_render : NULL;
	et->bake = (have_function[2]) ? engine_bake : NULL;
	et->view_update = (have_function[3]) ? engine_view_update : NULL;
	et->view_draw = (have_function[4]) ? engine_view_draw : NULL;
	et->update_script_node = (have_function[5]) ? engine_update_script_node : NULL;
	et->update_render_passes = (have_function[6]) ? engine_update_render_passes : NULL;

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

static PointerRNA rna_RenderEngine_render_get(PointerRNA *ptr)
{
	RenderEngine *engine = (RenderEngine *)ptr->data;

	if (engine->re) {
		RenderData *r = RE_engine_get_render_data(engine->re);

		return rna_pointer_inherit_refine(ptr, &RNA_RenderSettings, r);
	}
	else {
		return rna_pointer_inherit_refine(ptr, &RNA_RenderSettings, NULL);
	}
}

static PointerRNA rna_RenderEngine_camera_override_get(PointerRNA *ptr)
{
	RenderEngine *engine = (RenderEngine *)ptr->data;

	if (engine->re) {
		Object *cam = RE_GetCamera(engine->re);
		return rna_pointer_inherit_refine(ptr, &RNA_Object, cam);
	}
	else {
		return rna_pointer_inherit_refine(ptr, &RNA_Object, engine->camera_override);
	}
}

static void rna_RenderResult_views_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	RenderResult *rr = (RenderResult *)ptr->data;
	rna_iterator_listbase_begin(iter, &rr->views, NULL);
}

static void rna_RenderResult_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	RenderResult *rr = (RenderResult *)ptr->data;
	rna_iterator_listbase_begin(iter, &rr->layers, NULL);
}

static void rna_RenderResult_stamp_data_add_field(RenderResult *rr, const char *field, const char *value)
{
	BKE_render_result_stamp_data(rr, field, value);
}


static void rna_RenderLayer_passes_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	RenderLayer *rl = (RenderLayer *)ptr->data;
	rna_iterator_listbase_begin(iter, &rl->passes, NULL);
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

static PointerRNA rna_BakePixel_next_get(PointerRNA *ptr)
{
	BakePixel *bp = ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_BakePixel, bp + 1);
}

static RenderPass *rna_RenderPass_find_by_type(RenderLayer *rl, int passtype, const char *view)
{
	return RE_pass_find_by_type(rl, passtype, view);
}

static RenderPass *rna_RenderPass_find_by_name(RenderLayer *rl, const char *name, const char *view)
{
	return RE_pass_find_by_name(rl, name, view);
}

#else /* RNA_RUNTIME */

static void rna_def_render_engine(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	static const EnumPropertyItem render_pass_type_items[] = {
	        {SOCK_FLOAT,   "VALUE",     0,    "Value",     ""},
	        {SOCK_VECTOR,  "VECTOR",    0,    "Vector",    ""},
	        {SOCK_RGBA,    "COLOR",     0,    "Color",     ""},
	        {0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "RenderEngine", NULL);
	RNA_def_struct_sdna(srna, "RenderEngine");
	RNA_def_struct_ui_text(srna, "Render Engine", "Render engine");
	RNA_def_struct_refine_func(srna, "rna_RenderEngine_refine");
	RNA_def_struct_register_funcs(srna, "rna_RenderEngine_register", "rna_RenderEngine_unregister",
	                              "rna_RenderEngine_instance");

	/* final render callbacks */
	func = RNA_def_function(srna, "update", NULL);
	RNA_def_function_ui_description(func, "Export scene data for render");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	RNA_def_pointer(func, "data", "BlendData", "", "");
	RNA_def_pointer(func, "scene", "Scene", "", "");

	func = RNA_def_function(srna, "render", NULL);
	RNA_def_function_ui_description(func, "Render scene into an image");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	RNA_def_pointer(func, "scene", "Scene", "", "");

	func = RNA_def_function(srna, "bake", NULL);
	RNA_def_function_ui_description(func, "Bake passes");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	parm = RNA_def_pointer(func, "scene", "Scene", "", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_pointer(func, "object", "Object", "", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_enum(func, "pass_type", rna_enum_bake_pass_type_items, 0, "Pass", "Pass to bake");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_int(func, "pass_filter", 0, 0, INT_MAX, "Pass Filter", "Filter to combined, diffuse, glossy, transmission and subsurface passes", 0, INT_MAX);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_int(func, "object_id", 0, 0, INT_MAX, "Object Id", "Id of the current object being baked in relation to the others", 0, INT_MAX);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_pointer(func, "pixel_array", "BakePixel", "", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_int(func, "num_pixels", 0, 0, INT_MAX, "Number of Pixels", "Size of the baking batch", 0, INT_MAX);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_int(func, "depth", 0, 0, INT_MAX, "Pixels depth", "Number of channels", 1, INT_MAX);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	/* TODO, see how array size of 0 works, this shouldnt be used */
	parm = RNA_def_pointer(func, "result", "AnyType", "", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

	/* viewport render callbacks */
	func = RNA_def_function(srna, "view_update", NULL);
	RNA_def_function_ui_description(func, "Update on data changes for viewport render");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	RNA_def_pointer(func, "context", "Context", "", "");

	func = RNA_def_function(srna, "view_draw", NULL);
	RNA_def_function_ui_description(func, "Draw viewport render");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	RNA_def_pointer(func, "context", "Context", "", "");

	/* shader script callbacks */
	func = RNA_def_function(srna, "update_script_node", NULL);
	RNA_def_function_ui_description(func, "Compile shader script node");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	parm = RNA_def_pointer(func, "node", "Node", "", "");
	RNA_def_parameter_flags(parm, 0, PARM_RNAPTR);

	/* tag for redraw */
	func = RNA_def_function(srna, "tag_redraw", "engine_tag_redraw");
	RNA_def_function_ui_description(func, "Request redraw for viewport rendering");

	/* tag for update */
	func = RNA_def_function(srna, "tag_update", "engine_tag_update");
	RNA_def_function_ui_description(func, "Request update call for viewport rendering");

	func = RNA_def_function(srna, "update_render_passes", NULL);
	RNA_def_function_ui_description(func, "Update the render passes that will be generated");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	parm = RNA_def_pointer(func, "scene", "Scene", "", "");
	parm = RNA_def_pointer(func, "renderlayer", "SceneRenderLayer", "", "");

	func = RNA_def_function(srna, "begin_result", "RE_engine_begin_result");
	RNA_def_function_ui_description(func, "Create render result to write linear floating point render layers and passes");
	parm = RNA_def_int(func, "x", 0, 0, INT_MAX, "X", "", 0, INT_MAX);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_int(func, "y", 0, 0, INT_MAX, "Y", "", 0, INT_MAX);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_int(func, "w", 0, 0, INT_MAX, "Width", "", 0, INT_MAX);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_int(func, "h", 0, 0, INT_MAX, "Height", "", 0, INT_MAX);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	RNA_def_string(func, "layer", NULL, 0, "Layer", "Single layer to get render result for");  /* NULL ok here */
	RNA_def_string(func, "view", NULL, 0, "View", "Single view to get render result for");  /* NULL ok here */
	parm = RNA_def_pointer(func, "result", "RenderResult", "Result", "");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "update_result", "RE_engine_update_result");
	RNA_def_function_ui_description(func, "Signal that pixels have been updated and can be redrawn in the user interface");
	parm = RNA_def_pointer(func, "result", "RenderResult", "Result", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

	func = RNA_def_function(srna, "end_result", "RE_engine_end_result");
	RNA_def_function_ui_description(func, "All pixels in the render result have been set and are final");
	parm = RNA_def_pointer(func, "result", "RenderResult", "Result", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	RNA_def_boolean(func, "cancel", 0, "Cancel", "Don't mark tile as done, don't merge results unless forced");
	RNA_def_boolean(func, "highlight", 0, "Highlight", "Don't mark tile as done yet");
	RNA_def_boolean(func, "do_merge_results", 0, "Merge Results", "Merge results even if cancel=true");

	func = RNA_def_function(srna, "add_pass", "RE_engine_add_pass");
	RNA_def_function_ui_description(func, "Add a pass to the render layer");
	parm = RNA_def_string(func, "name", NULL, 0, "Name", "Name of the Pass, without view or channel tag");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_int(func, "channels", 0, 0, INT_MAX, "Channels", "", 0, INT_MAX);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_string(func, "chan_id", NULL, 0, "Channel IDs", "Channel names, one character per channel");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	RNA_def_string(func, "layer", NULL, 0, "Layer", "Single layer to add render pass to");  /* NULL ok here */

	func = RNA_def_function(srna, "get_result", "RE_engine_get_result");
	RNA_def_function_ui_description(func, "Get final result for non-pixel operations");
	parm = RNA_def_pointer(func, "result", "RenderResult", "Result", "");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "test_break", "RE_engine_test_break");
	RNA_def_function_ui_description(func, "Test if the render operation should been canceled, this is a fast call that should be used regularly for responsiveness");
	parm = RNA_def_boolean(func, "do_break", 0, "Break", "");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "active_view_get", "RE_engine_active_view_get");
	parm = RNA_def_string(func, "view", NULL, 0, "View", "Single view active");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "active_view_set", "RE_engine_active_view_set");
	parm = RNA_def_string(func, "view", NULL, 0, "View", "Single view to set as active");  /* NULL ok here */
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

	func = RNA_def_function(srna, "camera_shift_x", "RE_engine_get_camera_shift_x");
	parm = RNA_def_pointer(func, "camera", "Object", "", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	RNA_def_boolean(func, "use_spherical_stereo", 0, "Spherical Stereo", "");
	parm = RNA_def_float(func, "shift_x", 0.0f, 0.0f, FLT_MAX, "Shift X", "", 0.0f, FLT_MAX);
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "camera_model_matrix", "RE_engine_get_camera_model_matrix");
	parm = RNA_def_pointer(func, "camera", "Object", "", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	RNA_def_boolean(func, "use_spherical_stereo", 0, "Spherical Stereo", "");
	parm = RNA_def_float_matrix(func, "r_model_matrix", 4, 4, NULL, 0.0f, 0.0f, "Model Matrix", "Normalized camera model matrix", 0.0f, 0.0f);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	RNA_def_function_output(func, parm);

	func = RNA_def_function(srna, "use_spherical_stereo", "RE_engine_get_spherical_stereo");
	parm = RNA_def_pointer(func, "camera", "Object", "", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_boolean(func, "use_spherical_stereo", 0, "Spherical Stereo", "");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "update_stats", "RE_engine_update_stats");
	RNA_def_function_ui_description(func, "Update and signal to redraw render status text");
	parm = RNA_def_string(func, "stats", NULL, 0, "Stats", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_string(func, "info", NULL, 0, "Info", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

	func = RNA_def_function(srna, "frame_set", "RE_engine_frame_set");
	RNA_def_function_ui_description(func, "Evaluate scene at a different frame (for motion blur)");
	parm = RNA_def_int(func, "frame", 0, INT_MIN, INT_MAX, "Frame", "", INT_MIN, INT_MAX);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_float(func, "subframe", 0.0f, 0.0f, 1.0f, "Subframe", "", 0.0f, 1.0f);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

	func = RNA_def_function(srna, "update_progress", "RE_engine_update_progress");
	RNA_def_function_ui_description(func, "Update progress percentage of render");
	parm = RNA_def_float(func, "progress", 0, 0.0f, 1.0f, "", "Percentage of render that's done", 0.0f, 1.0f);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

	func = RNA_def_function(srna, "update_memory_stats", "RE_engine_update_memory_stats");
	RNA_def_function_ui_description(func, "Update memory usage statistics");
	RNA_def_float(func, "memory_used", 0, 0.0f, FLT_MAX, "", "Current memory usage in megabytes", 0.0f, FLT_MAX);
	RNA_def_float(func, "memory_peak", 0, 0.0f, FLT_MAX, "", "Peak memory usage in megabytes", 0.0f, FLT_MAX);

	func = RNA_def_function(srna, "report", "RE_engine_report");
	RNA_def_function_ui_description(func, "Report info, warning or error messages");
	parm = RNA_def_enum_flag(func, "type", rna_enum_wm_report_items, 0, "Type", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_string(func, "message", NULL, 0, "Report Message", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

	func = RNA_def_function(srna, "error_set", "RE_engine_set_error_message");
	RNA_def_function_ui_description(func, "Set error message displaying after the render is finished");
	parm = RNA_def_string(func, "message", NULL, 0, "Report Message", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

	func = RNA_def_function(srna, "bind_display_space_shader", "engine_bind_display_space_shader");
	RNA_def_function_ui_description(func, "Bind GLSL fragment shader that converts linear colors to display space colors using scene color management settings");
	parm = RNA_def_pointer(func, "scene", "Scene", "", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

	func = RNA_def_function(srna, "unbind_display_space_shader", "engine_unbind_display_space_shader");
	RNA_def_function_ui_description(func, "Unbind GLSL display space shader, must always be called after binding the shader");

	func = RNA_def_function(srna, "support_display_space_shader", "engine_support_display_space_shader");
	RNA_def_function_ui_description(func, "Test if GLSL display space shader is supported for the combination of graphics card and scene settings");
	parm = RNA_def_pointer(func, "scene", "Scene", "", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_boolean(func, "supported", 0, "Supported", "");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "get_preview_pixel_size", "engine_get_preview_pixel_size");
	RNA_def_function_ui_description(func, "Get the pixel size that should be used for preview rendering");
	parm = RNA_def_pointer(func, "scene", "Scene", "", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_int(func, "pixel_size", 0, 1, 8, "Pixel Size", "", 1, 8);
	RNA_def_function_return(func, parm);

	RNA_define_verify_sdna(0);

	prop = RNA_def_property(srna, "is_animation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RE_ENGINE_ANIMATION);

	prop = RNA_def_property(srna, "is_preview", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RE_ENGINE_PREVIEW);

	prop = RNA_def_property(srna, "camera_override", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_funcs(prop, "rna_RenderEngine_camera_override_get", NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "Object");

	prop = RNA_def_property(srna, "layer_override", PROP_BOOLEAN, PROP_LAYER_MEMBER);
	RNA_def_property_boolean_sdna(prop, NULL, "layer_override", 1);
	RNA_def_property_array(prop, 20);

	prop = RNA_def_property(srna, "tile_x", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "tile_x");
	prop = RNA_def_property(srna, "tile_y", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "tile_y");

	prop = RNA_def_property(srna, "resolution_x", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_sdna(prop, NULL, "resolution_x");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "resolution_y", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_sdna(prop, NULL, "resolution_y");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	/* Render Data */
	prop = RNA_def_property(srna, "render", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "RenderSettings");
	RNA_def_property_pointer_funcs(prop, "rna_RenderEngine_render_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Render Data", "");

	prop = RNA_def_property(srna, "use_highlight_tiles", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RE_ENGINE_HIGHLIGHT_TILES);

	func = RNA_def_function(srna, "register_pass", "RE_engine_register_pass");
	RNA_def_function_ui_description(func, "Register a render pass that will be part of the render with the current settings");
	prop = RNA_def_pointer(func, "scene", "Scene", "", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	prop = RNA_def_pointer(func, "srl", "SceneRenderLayer", "", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	prop = RNA_def_string(func, "name", NULL, MAX_NAME, "Name", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	prop = RNA_def_int(func, "channels", 1, 1, 8, "Channels", "", 1, 4);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	prop = RNA_def_string(func, "chanid", NULL, 8, "Channel IDs", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	prop = RNA_def_enum(func, "type", render_pass_type_items, SOCK_FLOAT, "Type", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

	/* registration */

	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->idname");
	RNA_def_property_flag(prop, PROP_REGISTER);

	prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->name");
	RNA_def_property_flag(prop, PROP_REGISTER);

	prop = RNA_def_property(srna, "bl_use_preview", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "type->flag", RE_USE_PREVIEW);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_use_texture_preview", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "type->flag", RE_USE_TEXTURE_PREVIEW);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_use_postprocess", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "type->flag", RE_USE_POSTPROCESS);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_use_shading_nodes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "type->flag", RE_USE_SHADING_NODES);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_use_shading_nodes_custom", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "type->flag", RE_USE_SHADING_NODES_CUSTOM);
	RNA_def_property_boolean_default(prop, true);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_use_exclude_layers", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "type->flag", RE_USE_EXCLUDE_LAYERS);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_use_save_buffers", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "type->flag", RE_USE_SAVE_BUFFERS);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_use_spherical_stereo", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "type->flag", RE_USE_SPHERICAL_STEREO);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	RNA_define_verify_sdna(1);
}

static void rna_def_render_result(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	srna = RNA_def_struct(brna, "RenderResult", NULL);
	RNA_def_struct_ui_text(srna, "Render Result", "Result of rendering, including all layers and passes");

	func = RNA_def_function(srna, "load_from_file", "RE_result_load_from_file");
	RNA_def_function_ui_description(func, "Copies the pixels of this render result from an image file");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_string_file_name(func, "filename", NULL, FILE_MAX, "File Name",
	                                "Filename to load into this render tile, must be no smaller than "
	                                "the render result");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

	func = RNA_def_function(srna, "stamp_data_add_field", "rna_RenderResult_stamp_data_add_field");
	RNA_def_function_ui_description(func, "Add engine-specific stamp data to the result");
	parm = RNA_def_string(func, "field", NULL, 1024, "Field", "Name of the stamp field to add");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_string(func, "value", NULL, 1024, "Value", "Value of the stamp data");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

	RNA_define_verify_sdna(0);

	prop = RNA_def_property(srna, "resolution_x", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_sdna(prop, NULL, "rectx");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "resolution_y", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_sdna(prop, NULL, "recty");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "layers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "RenderLayer");
	RNA_def_property_collection_funcs(prop, "rna_RenderResult_layers_begin", "rna_iterator_listbase_next",
	                                  "rna_iterator_listbase_end", "rna_iterator_listbase_get",
	                                  NULL, NULL, NULL, NULL);

	prop = RNA_def_property(srna, "views", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "RenderView");
	RNA_def_property_collection_funcs(prop, "rna_RenderResult_views_begin", "rna_iterator_listbase_next",
	                                  "rna_iterator_listbase_end", "rna_iterator_listbase_get",
	                                  NULL, NULL, NULL, NULL);

	RNA_define_verify_sdna(1);
}

static void rna_def_render_view(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "RenderView", NULL);
	RNA_def_struct_ui_text(srna, "Render View", "");

	RNA_define_verify_sdna(0);

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_struct_name_property(srna, prop);

	RNA_define_verify_sdna(1);
}

static void rna_def_render_passes(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "RenderPasses");
	srna = RNA_def_struct(brna, "RenderPasses", NULL);
	RNA_def_struct_sdna(srna, "RenderLayer");
	RNA_def_struct_ui_text(srna, "Render Passes", "Collection of render passes");

	func = RNA_def_function(srna, "find_by_type", "rna_RenderPass_find_by_type");
	RNA_def_function_ui_description(func, "Get the render pass for a given type and view");
	parm = RNA_def_enum(func, "pass_type", rna_enum_render_pass_type_items, SCE_PASS_COMBINED, "Pass", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_string(func, "view", NULL, 0, "View", "Render view to get pass from");  /* NULL ok here */
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_pointer(func, "render_pass", "RenderPass", "", "The matching render pass");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "find_by_name", "rna_RenderPass_find_by_name");
	RNA_def_function_ui_description(func, "Get the render pass for a given name and view");
	parm = RNA_def_string(func, "name", RE_PASSNAME_COMBINED, 0, "Pass", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_string(func, "view", NULL, 0, "View", "Render view to get pass from");  /* NULL ok here */
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_pointer(func, "render_pass", "RenderPass", "", "The matching render pass");
	RNA_def_function_return(func, parm);
}

static void rna_def_render_layer(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	srna = RNA_def_struct(brna, "RenderLayer", NULL);
	RNA_def_struct_ui_text(srna, "Render Layer", "");

	func = RNA_def_function(srna, "load_from_file", "RE_layer_load_from_file");
	RNA_def_function_ui_description(func, "Copies the pixels of this renderlayer from an image file");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_string(func, "filename", NULL, 0, "Filename",
	                      "Filename to load into this render tile, must be no smaller than the renderlayer");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
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
	rna_def_render_passes(brna, prop);

	RNA_define_verify_sdna(1);
}

static void rna_def_render_pass(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "RenderPass", NULL);
	RNA_def_struct_ui_text(srna, "Render Pass", "");

	RNA_define_verify_sdna(0);

	prop = RNA_def_property(srna, "fullname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "fullname");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_struct_name_property(srna, prop);

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

	prop = RNA_def_property(srna, "rect", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_DYNAMIC);
	RNA_def_property_multi_array(prop, 2, NULL);
	RNA_def_property_dynamic_array_funcs(prop, "rna_RenderPass_rect_get_length");
	RNA_def_property_float_funcs(prop, "rna_RenderPass_rect_get", "rna_RenderPass_rect_set", NULL);

	prop = RNA_def_property(srna, "view_id", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "view_id");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	RNA_define_verify_sdna(1);
}

static void rna_def_render_bake_pixel(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "BakePixel", NULL);
	RNA_def_struct_ui_text(srna, "Bake Pixel", "");

	RNA_define_verify_sdna(0);

	prop = RNA_def_property(srna, "primitive_id", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "primitive_id");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "object_id", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "object_id");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "uv", PROP_FLOAT, PROP_NONE);
	RNA_def_property_array(prop, 2);
	RNA_def_property_float_sdna(prop, NULL, "uv");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "du_dx", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "du_dx");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "du_dy", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "du_dy");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "dv_dx", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dv_dx");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "dv_dy", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dv_dy");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "next", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "BakePixel");
	RNA_def_property_pointer_funcs(prop, "rna_BakePixel_next_get", NULL, NULL, NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	RNA_define_verify_sdna(1);
}

void RNA_def_render(BlenderRNA *brna)
{
	rna_def_render_engine(brna);
	rna_def_render_result(brna);
	rna_def_render_view(brna);
	rna_def_render_layer(brna);
	rna_def_render_pass(brna);
	rna_def_render_bake_pixel(brna);
}

#endif /* RNA_RUNTIME */
