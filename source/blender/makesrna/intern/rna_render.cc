/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "BLI_path_utils.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern.hh"
#endif

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "RE_engine.h"

const EnumPropertyItem rna_enum_bake_pass_type_items[] = {
    {SCE_PASS_COMBINED, "COMBINED", 0, "Combined", ""},
    {SCE_PASS_AO, "AO", 0, "Ambient Occlusion", ""},
    {SCE_PASS_SHADOW, "SHADOW", 0, "Shadow", ""},
    {SCE_PASS_POSITION, "POSITION", 0, "Position", ""},
    {SCE_PASS_NORMAL, "NORMAL", 0, "Normal", ""},
    {SCE_PASS_UV, "UV", 0, "UV", ""},
    {int(SCE_PASS_ROUGHNESS), "ROUGHNESS", 0, "ROUGHNESS", ""},
    {SCE_PASS_EMIT, "EMIT", 0, "Emission", ""},
    {SCE_PASS_ENVIRONMENT, "ENVIRONMENT", 0, "Environment", ""},
    {SCE_PASS_DIFFUSE_COLOR, "DIFFUSE", 0, "Diffuse", ""},
    {SCE_PASS_GLOSSY_COLOR, "GLOSSY", 0, "Glossy", ""},
    {SCE_PASS_TRANSM_COLOR, "TRANSMISSION", 0, "Transmission", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include "MEM_guardedalloc.h"

#  include "RNA_access.hh"

#  include "BKE_appdir.hh"
#  include "BKE_context.hh"
#  include "BKE_image.hh"
#  include "BKE_report.hh"
#  include "BKE_scene.hh"

#  include "GPU_capabilities.hh"
#  include "GPU_shader.hh"
#  include "IMB_colormanagement.hh"
#  include "IMB_imbuf_types.hh"

#  include "DEG_depsgraph_query.hh"

#  include "ED_render.hh"

/* RenderEngine Callbacks */

static void engine_tag_redraw(RenderEngine *engine)
{
  engine->flag |= RE_ENGINE_DO_DRAW;
}

static void engine_tag_update(RenderEngine *engine)
{
  engine->flag |= RE_ENGINE_DO_UPDATE;
}

static bool engine_support_display_space_shader(RenderEngine * /*engine*/, Scene * /*scene*/)
{
  return true;
}

static int engine_get_preview_pixel_size(RenderEngine * /*engine*/, Scene *scene)
{
  return BKE_render_preview_pixel_size(&scene->r);
}

static void engine_bind_display_space_shader(RenderEngine * /*engine*/, Scene * /*scene*/)
{
  blender::gpu::Shader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_3D_IMAGE);
  GPU_shader_bind(shader);
  /** \note "image" binding slot is 0. */
}

static void engine_unbind_display_space_shader(RenderEngine * /*engine*/)
{
  GPU_shader_unbind();
}

static void engine_update(RenderEngine *engine, Main *bmain, Depsgraph *depsgraph)
{
  extern FunctionRNA rna_RenderEngine_update_func;
  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, engine->type->rna_ext.srna, engine);
  func = &rna_RenderEngine_update_func;

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "data", &bmain);
  RNA_parameter_set_lookup(&list, "depsgraph", &depsgraph);
  engine->type->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void engine_render(RenderEngine *engine, Depsgraph *depsgraph)
{
  extern FunctionRNA rna_RenderEngine_render_func;
  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, engine->type->rna_ext.srna, engine);
  func = &rna_RenderEngine_render_func;

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "depsgraph", &depsgraph);
  engine->type->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void engine_render_frame_finish(RenderEngine *engine)
{
  extern FunctionRNA rna_RenderEngine_render_frame_finish_func;
  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, engine->type->rna_ext.srna, engine);
  func = &rna_RenderEngine_render_frame_finish_func;

  RNA_parameter_list_create(&list, &ptr, func);
  engine->type->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void engine_draw(RenderEngine *engine, const bContext *context, Depsgraph *depsgraph)
{
  extern FunctionRNA rna_RenderEngine_draw_func;
  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, engine->type->rna_ext.srna, engine);
  func = &rna_RenderEngine_draw_func;

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &context);
  RNA_parameter_set_lookup(&list, "depsgraph", &depsgraph);
  engine->type->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void engine_bake(RenderEngine *engine,
                        Depsgraph *depsgraph,
                        Object *object,
                        const int pass_type,
                        const int pass_filter,
                        const int width,
                        const int height)
{
  extern FunctionRNA rna_RenderEngine_bake_func;
  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, engine->type->rna_ext.srna, engine);
  func = &rna_RenderEngine_bake_func;

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "depsgraph", &depsgraph);
  RNA_parameter_set_lookup(&list, "object", &object);
  RNA_parameter_set_lookup(&list, "pass_type", &pass_type);
  RNA_parameter_set_lookup(&list, "pass_filter", &pass_filter);
  RNA_parameter_set_lookup(&list, "width", &width);
  RNA_parameter_set_lookup(&list, "height", &height);
  engine->type->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void engine_view_update(RenderEngine *engine, const bContext *context, Depsgraph *depsgraph)
{
  extern FunctionRNA rna_RenderEngine_view_update_func;
  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, engine->type->rna_ext.srna, engine);
  func = &rna_RenderEngine_view_update_func;

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &context);
  RNA_parameter_set_lookup(&list, "depsgraph", &depsgraph);
  engine->type->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void engine_view_draw(RenderEngine *engine, const bContext *context, Depsgraph *depsgraph)
{
  extern FunctionRNA rna_RenderEngine_view_draw_func;
  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, engine->type->rna_ext.srna, engine);
  func = &rna_RenderEngine_view_draw_func;

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &context);
  RNA_parameter_set_lookup(&list, "depsgraph", &depsgraph);
  engine->type->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void engine_update_script_node(RenderEngine *engine, bNodeTree *ntree, bNode *node)
{
  extern FunctionRNA rna_RenderEngine_update_script_node_func;
  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, engine->type->rna_ext.srna, engine);
  PointerRNA nodeptr = RNA_pointer_create_discrete((ID *)ntree, &RNA_Node, node);
  func = &rna_RenderEngine_update_script_node_func;

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "node", &nodeptr);
  engine->type->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void engine_update_render_passes(RenderEngine *engine, Scene *scene, ViewLayer *view_layer)
{
  extern FunctionRNA rna_RenderEngine_update_render_passes_func;
  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, engine->type->rna_ext.srna, engine);
  func = &rna_RenderEngine_update_render_passes_func;

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "scene", &scene);
  RNA_parameter_set_lookup(&list, "renderlayer", &view_layer);
  engine->type->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void engine_update_custom_camera(RenderEngine *engine, Camera *cam)
{
  extern FunctionRNA rna_RenderEngine_update_custom_camera_func;
  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, engine->type->rna_ext.srna, engine);
  func = &rna_RenderEngine_update_custom_camera_func;

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "cam", &cam);
  engine->type->rna_ext.call(nullptr, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

/* RenderEngine registration */

static bool rna_RenderEngine_unregister(Main *bmain, StructRNA *type)
{
  RenderEngineType *et = static_cast<RenderEngineType *>(RNA_struct_blender_type_get(type));

  if (!et) {
    return false;
  }

  /* Stop all renders in case we were using this one. */
  ED_render_engine_changed(bmain, false);
  RE_FreeAllPersistentData();

  RNA_struct_free_extension(type, &et->rna_ext);
  RNA_struct_free(&BLENDER_RNA, type);
  BLI_freelinkN(&R_engines, et);
  return true;
}

static StructRNA *rna_RenderEngine_register(Main *bmain,
                                            ReportList *reports,
                                            void *data,
                                            const char *identifier,
                                            StructValidateFunc validate,
                                            StructCallbackFunc call,
                                            StructFreeFunc free)
{
  const char *error_prefix = "Registering render engine class:";
  RenderEngineType *et, dummy_et = {nullptr};
  RenderEngine dummy_engine = {nullptr};
  bool have_function[10];

  /* setup dummy engine & engine type to store static properties in */
  dummy_engine.type = &dummy_et;
  dummy_et.flag |= RE_USE_SHADING_NODES_CUSTOM;
  PointerRNA dummy_engine_ptr = RNA_pointer_create_discrete(
      nullptr, &RNA_RenderEngine, &dummy_engine);

  /* validate the python class */
  if (validate(&dummy_engine_ptr, data, have_function) != 0) {
    return nullptr;
  }

  if (strlen(identifier) >= sizeof(dummy_et.idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s '%s' is too long, maximum length is %d",
                error_prefix,
                identifier,
                int(sizeof(dummy_et.idname)));
    return nullptr;
  }

  /* Check if we have registered this engine type before, and remove it. */
  et = static_cast<RenderEngineType *>(
      BLI_findstring(&R_engines, dummy_et.idname, offsetof(RenderEngineType, idname)));
  if (et) {
    BKE_reportf(reports,
                RPT_INFO,
                "%s '%s', bl_idname '%s' has been registered before, unregistering previous",
                error_prefix,
                identifier,
                dummy_et.idname);

    StructRNA *srna = et->rna_ext.srna;
    if (!(srna && rna_RenderEngine_unregister(bmain, srna))) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "%s '%s', bl_idname '%s' %s",
                  error_prefix,
                  identifier,
                  dummy_et.idname,
                  srna ? "is built-in" : "could not be unregistered");
      return nullptr;
    }
  }

  /* create a new engine type */
  et = MEM_mallocN<RenderEngineType>("Python render engine");
  memcpy(et, &dummy_et, sizeof(dummy_et));

  et->rna_ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, et->idname, &RNA_RenderEngine);
  et->rna_ext.data = data;
  et->rna_ext.call = call;
  et->rna_ext.free = free;
  RNA_struct_blender_type_set(et->rna_ext.srna, et);

  et->update = (have_function[0]) ? engine_update : nullptr;
  et->render = (have_function[1]) ? engine_render : nullptr;
  et->render_frame_finish = (have_function[2]) ? engine_render_frame_finish : nullptr;
  et->draw = (have_function[3]) ? engine_draw : nullptr;
  et->bake = (have_function[4]) ? engine_bake : nullptr;
  et->view_update = (have_function[5]) ? engine_view_update : nullptr;
  et->view_draw = (have_function[6]) ? engine_view_draw : nullptr;
  et->update_script_node = (have_function[7]) ? engine_update_script_node : nullptr;
  et->update_render_passes = (have_function[8]) ? engine_update_render_passes : nullptr;
  et->update_custom_camera = (have_function[9]) ? engine_update_custom_camera : nullptr;

  RE_engines_register(et);

  return et->rna_ext.srna;
}

static void **rna_RenderEngine_instance(PointerRNA *ptr)
{
  RenderEngine *engine = static_cast<RenderEngine *>(ptr->data);
  return &engine->py_instance;
}

static StructRNA *rna_RenderEngine_refine(PointerRNA *ptr)
{
  RenderEngine *engine = (RenderEngine *)ptr->data;
  return (engine->type && engine->type->rna_ext.srna) ? engine->type->rna_ext.srna :
                                                        &RNA_RenderEngine;
}

static void rna_RenderEngine_tempdir_get(PointerRNA * /*ptr*/, char *value)
{
  strcpy(value, BKE_tempdir_session());
}

static int rna_RenderEngine_tempdir_length(PointerRNA * /*ptr*/)
{
  return strlen(BKE_tempdir_session());
}

static PointerRNA rna_RenderEngine_render_get(PointerRNA *ptr)
{
  RenderEngine *engine = (RenderEngine *)ptr->data;

  if (engine->re) {
    RenderData *r = RE_engine_get_render_data(engine->re);

    return RNA_pointer_create_with_parent(*ptr, &RNA_RenderSettings, r);
  }
  return PointerRNA_NULL;
}

static PointerRNA rna_RenderEngine_camera_override_get(PointerRNA *ptr)
{
  RenderEngine *engine = (RenderEngine *)ptr->data;
  /* TODO(sergey): Shouldn't engine point to an evaluated datablocks already? */
  if (engine->re) {
    Object *cam = RE_GetCamera(engine->re);
    Object *cam_eval = DEG_get_evaluated(engine->depsgraph, cam);
    return RNA_id_pointer_create(reinterpret_cast<ID *>(cam_eval));
  }
  else {
    return RNA_id_pointer_create(reinterpret_cast<ID *>(engine->camera_override));
  }
}

static void rna_RenderEngine_engine_frame_set(RenderEngine *engine, int frame, float subframe)
{
#  ifdef WITH_PYTHON
  BPy_BEGIN_ALLOW_THREADS;
#  endif

  RE_engine_frame_set(engine, frame, subframe);

#  ifdef WITH_PYTHON
  BPy_END_ALLOW_THREADS;
#  endif
}

static void rna_RenderResult_views_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  RenderResult *rr = (RenderResult *)ptr->data;
  rna_iterator_listbase_begin(iter, ptr, &rr->views, nullptr);
}

static void rna_RenderResult_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  RenderResult *rr = (RenderResult *)ptr->data;
  rna_iterator_listbase_begin(iter, ptr, &rr->layers, nullptr);
}

static void rna_RenderResult_stamp_data_add_field(RenderResult *rr,
                                                  const char *field,
                                                  const char *value)
{
  BKE_render_result_stamp_data(rr, field, value);
}

static void rna_RenderLayer_passes_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  RenderLayer *rl = (RenderLayer *)ptr->data;
  rna_iterator_listbase_begin(iter, ptr, &rl->passes, nullptr);
}

static int rna_RenderPass_rect_get_length(const PointerRNA *ptr,
                                          int length[RNA_MAX_ARRAY_DIMENSION])
{
  const RenderPass *rpass = (RenderPass *)ptr->data;

  length[0] = rpass->rectx * rpass->recty;
  length[1] = rpass->channels;

  return length[0] * length[1];
}

static void rna_RenderPass_rect_get(PointerRNA *ptr, float *values)
{
  RenderPass *rpass = (RenderPass *)ptr->data;
  const size_t size_in_bytes = sizeof(float) * rpass->rectx * rpass->recty * rpass->channels;
  const float *buffer = rpass->ibuf ? rpass->ibuf->float_buffer.data : nullptr;

  if (!buffer) {
    /* No float buffer to read from, initialize to all zeroes. */
    memset(values, 0, size_in_bytes);
    return;
  }

  memcpy(values, buffer, size_in_bytes);
}

void rna_RenderPass_rect_set(PointerRNA *ptr, const float *values)
{
  RenderPass *rpass = (RenderPass *)ptr->data;
  float *buffer = rpass->ibuf ? rpass->ibuf->float_buffer.data : nullptr;

  if (!buffer) {
    /* Only writing to an already existing buffer is supported. */
    return;
  }

  const size_t size_in_bytes = sizeof(float) * rpass->rectx * rpass->recty * rpass->channels;
  memcpy(buffer, values, size_in_bytes);
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
      {SOCK_FLOAT, "VALUE", 0, "Value", ""},
      {SOCK_VECTOR, "VECTOR", 0, "Vector", ""},
      {SOCK_RGBA, "COLOR", 0, "Color", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "RenderEngine", nullptr);
  RNA_def_struct_sdna(srna, "RenderEngine");
  RNA_def_struct_ui_text(srna, "Render Engine", "Render engine");
  RNA_def_struct_refine_func(srna, "rna_RenderEngine_refine");
  RNA_def_struct_register_funcs(srna,
                                "rna_RenderEngine_register",
                                "rna_RenderEngine_unregister",
                                "rna_RenderEngine_instance");

  /* final render callbacks */
  func = RNA_def_function(srna, "update", nullptr);
  RNA_def_function_ui_description(func, "Export scene data for render");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  RNA_def_pointer(func, "data", "BlendData", "", "");
  RNA_def_pointer(func, "depsgraph", "Depsgraph", "", "");

  func = RNA_def_function(srna, "render", nullptr);
  RNA_def_function_ui_description(func, "Render scene into an image");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "depsgraph", "Depsgraph", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "render_frame_finish", nullptr);
  RNA_def_function_ui_description(
      func, "Perform finishing operations after all view layers in a frame were rendered");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);

  func = RNA_def_function(srna, "draw", nullptr);
  RNA_def_function_ui_description(func, "Draw render image");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "depsgraph", "Depsgraph", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "bake", nullptr);
  RNA_def_function_ui_description(func, "Bake passes");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "depsgraph", "Depsgraph", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "object", "Object", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(func, "pass_type", rna_enum_bake_pass_type_items, 0, "Pass", "Pass to bake");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "pass_filter",
                     0,
                     0,
                     INT_MAX,
                     "Pass Filter",
                     "Filter to combined, diffuse, glossy and transmission passes",
                     0,
                     INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "width", 0, 0, INT_MAX, "Width", "Image width", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "height", 0, 0, INT_MAX, "Height", "Image height", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* viewport render callbacks */
  func = RNA_def_function(srna, "view_update", nullptr);
  RNA_def_function_ui_description(func, "Update on data changes for viewport render");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "depsgraph", "Depsgraph", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "view_draw", nullptr);
  RNA_def_function_ui_description(func, "Draw viewport render");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "depsgraph", "Depsgraph", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* shader script callbacks */
  func = RNA_def_function(srna, "update_script_node", nullptr);
  RNA_def_function_ui_description(func, "Compile shader script node");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "node", "Node", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);

  func = RNA_def_function(srna, "update_render_passes", nullptr);
  RNA_def_function_ui_description(func, "Update the render passes that will be generated");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "scene", "Scene", "", "");
  parm = RNA_def_pointer(func, "renderlayer", "ViewLayer", "", "");

  func = RNA_def_function(srna, "update_custom_camera", nullptr);
  RNA_def_function_ui_description(func, "Compile custom camera");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "cam", "Camera", "", "");

  /* tag for redraw */
  func = RNA_def_function(srna, "tag_redraw", "engine_tag_redraw");
  RNA_def_function_ui_description(func, "Request redraw for viewport rendering");

  /* tag for update */
  func = RNA_def_function(srna, "tag_update", "engine_tag_update");
  RNA_def_function_ui_description(func, "Request update call for viewport rendering");

  func = RNA_def_function(srna, "begin_result", "RE_engine_begin_result");
  RNA_def_function_ui_description(
      func, "Create render result to write linear floating-point render layers and passes");
  parm = RNA_def_int(func, "x", 0, 0, INT_MAX, "X", "", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "y", 0, 0, INT_MAX, "Y", "", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "w", 0, 0, INT_MAX, "Width", "", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "h", 0, 0, INT_MAX, "Height", "", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_string(func,
                 "layer",
                 nullptr,
                 0,
                 "Layer",
                 "Single layer to get render result for"); /* nullptr ok here */
  RNA_def_string(func,
                 "view",
                 nullptr,
                 0,
                 "View",
                 "Single view to get render result for"); /* nullptr ok here */
  parm = RNA_def_pointer(func, "result", "RenderResult", "Result", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "update_result", "RE_engine_update_result");
  RNA_def_function_ui_description(
      func, "Signal that pixels have been updated and can be redrawn in the user interface");
  parm = RNA_def_pointer(func, "result", "RenderResult", "Result", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "end_result", "RE_engine_end_result");
  RNA_def_function_ui_description(func,
                                  "All pixels in the render result have been set and are final");
  parm = RNA_def_pointer(func, "result", "RenderResult", "Result", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_boolean(func,
                  "cancel",
                  false,
                  "Cancel",
                  "Don't mark tile as done, don't merge results unless forced");
  RNA_def_boolean(func, "highlight", false, "Highlight", "Don't mark tile as done yet");
  RNA_def_boolean(
      func, "do_merge_results", false, "Merge Results", "Merge results even if cancel=true");

  func = RNA_def_function(srna, "add_pass", "RE_engine_add_pass");
  RNA_def_function_ui_description(func, "Add a pass to the render layer");
  parm = RNA_def_string(
      func, "name", nullptr, 0, "Name", "Name of the Pass, without view or channel tag");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "channels", 0, 0, INT_MAX, "Channels", "", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(
      func, "chan_id", nullptr, 0, "Channel IDs", "Channel names, one character per channel");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_string(func,
                 "layer",
                 nullptr,
                 0,
                 "Layer",
                 "Single layer to add render pass to"); /* nullptr ok here */

  func = RNA_def_function(srna, "get_result", "RE_engine_get_result");
  RNA_def_function_ui_description(func, "Get final result for non-pixel operations");
  parm = RNA_def_pointer(func, "result", "RenderResult", "Result", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "test_break", "RE_engine_test_break");
  RNA_def_function_ui_description(func,
                                  "Test if the render operation should been canceled, this is a "
                                  "fast call that should be used regularly for responsiveness");
  parm = RNA_def_boolean(func, "do_break", false, "Break", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "pass_by_index_get", "RE_engine_pass_by_index_get");
  parm = RNA_def_string(
      func, "layer", nullptr, 0, "Layer", "Name of render layer to get pass for");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "Index of pass to get", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "render_pass", "RenderPass", "Index", "Index of pass to get");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "active_view_get", "RE_engine_active_view_get");
  parm = RNA_def_string(func, "view", nullptr, 0, "View", "Single view active");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "active_view_set", "RE_engine_active_view_set");
  parm = RNA_def_string(
      func, "view", nullptr, 0, "View", "Single view to set as active"); /* nullptr ok here */
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "camera_shift_x", "RE_engine_get_camera_shift_x");
  parm = RNA_def_pointer(func, "camera", "Object", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_boolean(func, "use_spherical_stereo", false, "Spherical Stereo", "");
  parm = RNA_def_float(func, "shift_x", 0.0f, 0.0f, FLT_MAX, "Shift X", "", 0.0f, FLT_MAX);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "camera_model_matrix", "RE_engine_get_camera_model_matrix");
  parm = RNA_def_pointer(func, "camera", "Object", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_boolean(func, "use_spherical_stereo", false, "Spherical Stereo", "");
  parm = RNA_def_float_matrix(func,
                              "r_model_matrix",
                              4,
                              4,
                              nullptr,
                              0.0f,
                              0.0f,
                              "Model Matrix",
                              "Normalized camera model matrix",
                              0.0f,
                              0.0f);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_function_output(func, parm);

  func = RNA_def_function(srna, "use_spherical_stereo", "RE_engine_get_spherical_stereo");
  parm = RNA_def_pointer(func, "camera", "Object", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_boolean(func, "use_spherical_stereo", false, "Spherical Stereo", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "update_stats", "RE_engine_update_stats");
  RNA_def_function_ui_description(func, "Update and signal to redraw render status text");
  parm = RNA_def_string(func, "stats", nullptr, 0, "Stats", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "info", nullptr, 0, "Info", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "frame_set", "rna_RenderEngine_engine_frame_set");
  RNA_def_function_ui_description(func, "Evaluate scene at a different frame (for motion blur)");
  parm = RNA_def_int(func, "frame", 0, INT_MIN, INT_MAX, "Frame", "", INT_MIN, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_float(func, "subframe", 0.0f, 0.0f, 1.0f, "Subframe", "", 0.0f, 1.0f);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "update_progress", "RE_engine_update_progress");
  RNA_def_function_ui_description(func, "Update progress percentage of render");
  parm = RNA_def_float(
      func, "progress", 0, 0.0f, 1.0f, "", "Percentage of render that's done", 0.0f, 1.0f);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "update_memory_stats", "RE_engine_update_memory_stats");
  RNA_def_function_ui_description(func, "Update memory usage statistics");
  RNA_def_float(func,
                "memory_used",
                0,
                0.0f,
                FLT_MAX,
                "",
                "Current memory usage in megabytes",
                0.0f,
                FLT_MAX);
  RNA_def_float(
      func, "memory_peak", 0, 0.0f, FLT_MAX, "", "Peak memory usage in megabytes", 0.0f, FLT_MAX);

  func = RNA_def_function(srna, "report", "RE_engine_report");
  RNA_def_function_ui_description(func, "Report info, warning or error messages");
  parm = RNA_def_enum_flag(func, "type", rna_enum_wm_report_items, 0, "Type", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "message", nullptr, 0, "Report Message", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "error_set", "RE_engine_set_error_message");
  RNA_def_function_ui_description(func,
                                  "Set error message displaying after the render is finished");
  parm = RNA_def_string(func, "message", nullptr, 0, "Report Message", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "bind_display_space_shader", "engine_bind_display_space_shader");
  RNA_def_function_ui_description(func,
                                  "Bind GLSL fragment shader that converts linear colors to "
                                  "display space colors using scene color management settings");
  parm = RNA_def_pointer(func, "scene", "Scene", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(
      srna, "unbind_display_space_shader", "engine_unbind_display_space_shader");
  RNA_def_function_ui_description(
      func, "Unbind GLSL display space shader, must always be called after binding the shader");

  func = RNA_def_function(
      srna, "support_display_space_shader", "engine_support_display_space_shader");
  RNA_def_function_ui_description(func,
                                  "Test if GLSL display space shader is supported for the "
                                  "combination of graphics card and scene settings");
  parm = RNA_def_pointer(func, "scene", "Scene", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_boolean(func, "supported", false, "Supported", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "get_preview_pixel_size", "engine_get_preview_pixel_size");
  RNA_def_function_ui_description(func,
                                  "Get the pixel size that should be used for preview rendering");
  parm = RNA_def_pointer(func, "scene", "Scene", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "pixel_size", 0, 1, 8, "Pixel Size", "", 1, 8);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "free_blender_memory", "RE_engine_free_blender_memory");
  RNA_def_function_ui_description(func, "Free Blender side memory of render engine");

  func = RNA_def_function(srna, "tile_highlight_set", "RE_engine_tile_highlight_set");
  RNA_def_function_ui_description(func, "Set highlighted state of the given tile");
  parm = RNA_def_int(func, "x", 0, 0, INT_MAX, "X", "", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "y", 0, 0, INT_MAX, "Y", "", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "width", 0, 0, INT_MAX, "Width", "", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "height", 0, 0, INT_MAX, "Height", "", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_boolean(func, "highlight", false, "Highlight", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "tile_highlight_clear_all", "RE_engine_tile_highlight_clear_all");
  RNA_def_function_ui_description(func, "Clear highlight from all tiles");

  RNA_define_verify_sdna(false);

  prop = RNA_def_property(srna, "is_animation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", RE_ENGINE_ANIMATION);

  prop = RNA_def_property(srna, "is_preview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", RE_ENGINE_PREVIEW);

  prop = RNA_def_property(srna, "camera_override", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_funcs(
      prop, "rna_RenderEngine_camera_override_get", nullptr, nullptr, nullptr);
  RNA_def_property_struct_type(prop, "Object");

  prop = RNA_def_property(srna, "layer_override", PROP_BOOLEAN, PROP_LAYER_MEMBER);
  RNA_def_property_boolean_sdna(prop, nullptr, "layer_override", 1);
  RNA_def_property_array(prop, 20);

  prop = RNA_def_property(srna, "resolution_x", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "resolution_x");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "resolution_y", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "resolution_y");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "temporary_directory", PROP_STRING, PROP_NONE);
  RNA_def_function_ui_description(func, "The temp directory used by Blender");
  RNA_def_property_string_funcs(
      prop, "rna_RenderEngine_tempdir_get", "rna_RenderEngine_tempdir_length", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* Render Data */
  prop = RNA_def_property(srna, "render", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "RenderSettings");
  RNA_def_property_pointer_funcs(prop, "rna_RenderEngine_render_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Render Data", "");

  prop = RNA_def_property(srna, "use_highlight_tiles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", RE_ENGINE_HIGHLIGHT_TILES);

  func = RNA_def_function(srna, "register_pass", "RE_engine_register_pass");
  RNA_def_function_ui_description(
      func, "Register a render pass that will be part of the render with the current settings");
  parm = RNA_def_pointer(func, "scene", "Scene", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "view_layer", "ViewLayer", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "name", nullptr, MAX_NAME, "Name", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "channels", 1, 1, 8, "Channels", "", 1, 4);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "chanid", nullptr, 8, "Channel IDs", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(func, "type", render_pass_type_items, SOCK_FLOAT, "Type", "");
  RNA_def_property_enum_native_type(parm, "eNodeSocketDatatype");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* registration */

  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->idname");
  RNA_def_property_flag(prop, PROP_REGISTER);

  prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->name");
  RNA_def_property_flag(prop, PROP_REGISTER);

  prop = RNA_def_property(srna, "bl_use_preview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "type->flag", RE_USE_PREVIEW);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(
      prop,
      "Use Preview Render",
      "Render engine supports being used for rendering previews of materials, lights and worlds");

  prop = RNA_def_property(srna, "bl_use_postprocess", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "type->flag", RE_USE_POSTPROCESS);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(prop, "Use Post Processing", "Apply compositing on render results");

  prop = RNA_def_property(srna, "bl_use_eevee_viewport", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "type->flag", RE_USE_EEVEE_VIEWPORT);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(prop,
                           "Use EEVEE Viewport",
                           "Uses EEVEE for viewport shading in Material Preview shading mode");

  prop = RNA_def_property(srna, "bl_use_custom_freestyle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "type->flag", RE_USE_CUSTOM_FREESTYLE);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(
      prop,
      "Use Custom Freestyle",
      "Handles freestyle rendering on its own, instead of delegating it to EEVEE");

  prop = RNA_def_property(srna, "bl_use_image_save", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "type->flag", RE_USE_NO_IMAGE_SAVE);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(
      prop,
      "Use Image Save",
      "Save images/movie to disk while rendering an animation. "
      "Disabling image saving is only supported when bl_use_postprocess is also disabled.");

  prop = RNA_def_property(srna, "bl_use_gpu_context", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "type->flag", RE_USE_GPU_CONTEXT);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(
      prop,
      "Use GPU Context",
      "Enable OpenGL context for the render method, for engines that render using OpenGL");

  prop = RNA_def_property(srna, "bl_use_shading_nodes_custom", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "type->flag", RE_USE_SHADING_NODES_CUSTOM);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(prop,
                           "Use Custom Shading Nodes",
                           "Don't expose Cycles and EEVEE shading nodes in the node editor user "
                           "interface, so separate nodes can be used instead");

  prop = RNA_def_property(srna, "bl_use_spherical_stereo", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "type->flag", RE_USE_SPHERICAL_STEREO);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(prop, "Use Spherical Stereo", "Support spherical stereo camera models");

  prop = RNA_def_property(srna, "bl_use_stereo_viewport", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "type->flag", RE_USE_STEREO_VIEWPORT);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(prop, "Use Stereo Viewport", "Support rendering stereo 3D viewport");

  prop = RNA_def_property(srna, "bl_use_materialx", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "type->flag", RE_USE_MATERIALX);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(
      prop, "Use MaterialX", "Use MaterialX for exporting materials to Hydra");

  RNA_define_verify_sdna(true);
}

static void rna_def_hydra_render_engine(BlenderRNA *brna)
{
  /* This is implemented in Python. */
  StructRNA *srna = RNA_def_struct(brna, "HydraRenderEngine", "RenderEngine");
  RNA_def_struct_sdna(srna, "RenderEngine");
  RNA_def_struct_ui_text(srna, "Hydra Render Engine", "Base class from USD Hydra based renderers");
}

static void rna_def_render_result(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "RenderResult", nullptr);
  RNA_def_struct_ui_text(
      srna, "Render Result", "Result of rendering, including all layers and passes");

  func = RNA_def_function(srna, "load_from_file", "RE_result_load_from_file");
  RNA_def_function_ui_description(func,
                                  "Copies the pixels of this render result from an image file");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string_file_name(
      func,
      "filepath",
      nullptr,
      FILE_MAX,
      "File Name",
      "Filename to load into this render tile, must be no smaller than "
      "the render result");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "stamp_data_add_field", "rna_RenderResult_stamp_data_add_field");
  RNA_def_function_ui_description(func, "Add engine-specific stamp data to the result");
  parm = RNA_def_string(func, "field", nullptr, 1024, "Field", "Name of the stamp field to add");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "value", nullptr, 0, "Value", "Value of the stamp data");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  RNA_define_verify_sdna(false);

  prop = RNA_def_property(srna, "resolution_x", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "rectx");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "resolution_y", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "recty");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "layers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "RenderLayer");
  RNA_def_property_collection_funcs(prop,
                                    "rna_RenderResult_layers_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);

  prop = RNA_def_property(srna, "views", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "RenderView");
  RNA_def_property_collection_funcs(prop,
                                    "rna_RenderResult_views_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);

  RNA_define_verify_sdna(true);
}

static void rna_def_render_view(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "RenderView", nullptr);
  RNA_def_struct_ui_text(srna, "Render View", "");

  RNA_define_verify_sdna(false);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "name");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_struct_name_property(srna, prop);

  RNA_define_verify_sdna(true);
}

static void rna_def_render_passes(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "RenderPasses");
  srna = RNA_def_struct(brna, "RenderPasses", nullptr);
  RNA_def_struct_sdna(srna, "RenderLayer");
  RNA_def_struct_ui_text(srna, "Render Passes", "Collection of render passes");

  func = RNA_def_function(srna, "find_by_name", "rna_RenderPass_find_by_name");
  RNA_def_function_ui_description(func, "Get the render pass for a given name and view");
  parm = RNA_def_string(func, "name", RE_PASSNAME_COMBINED, 0, "Pass", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(
      func, "view", nullptr, 0, "View", "Render view to get pass from"); /* nullptr ok here */
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "render_pass", "RenderPass", "", "The matching render pass");
  RNA_def_function_return(func, parm);
}

static void rna_def_render_layer(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "RenderLayer", nullptr);
  RNA_def_struct_ui_text(srna, "Render Layer", "");

  func = RNA_def_function(srna, "load_from_file", "RE_layer_load_from_file");
  RNA_def_function_ui_description(func,
                                  "Copies the pixels of this renderlayer from an image file");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(
      func,
      "filepath",
      nullptr,
      0,
      "File Path",
      "File path to load into this render tile, must be no smaller than the renderlayer");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_int(func,
              "x",
              0,
              0,
              INT_MAX,
              "Offset X",
              "Offset the position to copy from if the image is larger than the render layer",
              0,
              INT_MAX);
  RNA_def_int(func,
              "y",
              0,
              0,
              INT_MAX,
              "Offset Y",
              "Offset the position to copy from if the image is larger than the render layer",
              0,
              INT_MAX);

  RNA_define_verify_sdna(false);

  rna_def_view_layer_common(brna, srna, false);

  prop = RNA_def_property(srna, "passes", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "RenderPass");
  RNA_def_property_collection_funcs(prop,
                                    "rna_RenderLayer_passes_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  rna_def_render_passes(brna, prop);

  RNA_define_verify_sdna(true);
}

static void rna_def_render_pass(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "RenderPass", nullptr);
  RNA_def_struct_ui_text(srna, "Render Pass", "");

  RNA_define_verify_sdna(false);

  prop = RNA_def_property(srna, "fullname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "fullname");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "name");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "channel_id", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "chan_id");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "channels", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "channels");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "rect", PROP_FLOAT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  RNA_def_property_multi_array(prop, 2, nullptr);
  RNA_def_property_dynamic_array_funcs(prop, "rna_RenderPass_rect_get_length");
  RNA_def_property_float_funcs(
      prop, "rna_RenderPass_rect_get", "rna_RenderPass_rect_set", nullptr);

  prop = RNA_def_property(srna, "view_id", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "view_id");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  RNA_define_verify_sdna(true);
}

void RNA_def_render(BlenderRNA *brna)
{
  rna_def_render_engine(brna);
  rna_def_hydra_render_engine(brna);
  rna_def_render_result(brna);
  rna_def_render_view(brna);
  rna_def_render_layer(brna);
  rna_def_render_pass(brna);
}

#endif /* RNA_RUNTIME */
