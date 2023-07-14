/* SPDX-FileCopyrightText: 2009 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <stdio.h>
#include <stdlib.h>

#include "BLI_kdopbvh.h"
#include "BLI_path_util.h"
#include "BLI_utildefines.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "rna_internal.h" /* own include */

#ifdef WITH_ALEMBIC
#  include "ABC_alembic.h"
#endif

#ifdef RNA_RUNTIME

#  include "BKE_editmesh.h"
#  include "BKE_global.h"
#  include "BKE_image.h"
#  include "BKE_scene.h"
#  include "BKE_writeavi.h"

#  include "DEG_depsgraph_query.h"

#  include "ED_transform.h"
#  include "ED_transform_snap_object_context.h"
#  include "ED_uvedit.h"

#  ifdef WITH_PYTHON
#    include "BPY_extern.h"
#  endif

static void rna_Scene_frame_set(Scene *scene, Main *bmain, int frame, float subframe)
{
  double cfra = double(frame) + double(subframe);

  CLAMP(cfra, MINAFRAME, MAXFRAME);
  BKE_scene_frame_set(scene, cfra);

#  ifdef WITH_PYTHON
  BPy_BEGIN_ALLOW_THREADS;
#  endif

  for (ViewLayer *view_layer = static_cast<ViewLayer *>(scene->view_layers.first);
       view_layer != nullptr;
       view_layer = view_layer->next)
  {
    Depsgraph *depsgraph = BKE_scene_ensure_depsgraph(bmain, scene, view_layer);
    BKE_scene_graph_update_for_newframe(depsgraph);
  }

#  ifdef WITH_PYTHON
  BPy_END_ALLOW_THREADS;
#  endif

  if (BKE_scene_camera_switch_update(scene)) {
    for (bScreen *screen = static_cast<bScreen *>(bmain->screens.first); screen;
         screen = static_cast<bScreen *>(screen->id.next))
    {
      BKE_screen_view3d_scene_sync(screen, scene);
    }
  }

  /* don't do notifier when we're rendering, avoid some viewport crashes
   * redrawing while the data is being modified for render */
  if (!G.is_rendering) {
    /* can't use NC_SCENE|ND_FRAME because this causes wm_event_do_notifiers to call
     * BKE_scene_graph_update_for_newframe which will lose any un-keyed changes #24690. */
    // WM_main_add_notifier(NC_SCENE|ND_FRAME, scene);

    /* instead just redraw the views */
    WM_main_add_notifier(NC_WINDOW, nullptr);
  }
}

static void rna_Scene_uvedit_aspect(Scene * /*scene*/, Object *ob, float aspect[2])
{
  if ((ob->type == OB_MESH) && (ob->mode == OB_MODE_EDIT)) {
    BMEditMesh *em;
    em = BKE_editmesh_from_object(ob);
    if (EDBM_uv_check(em)) {
      ED_uvedit_get_aspect(ob, aspect, aspect + 1);
      return;
    }
  }

  aspect[0] = aspect[1] = 1.0f;
}

static void rna_SceneRender_get_frame_path(
    RenderData *rd, Main *bmain, int frame, bool preview, const char *view, char *filepath)
{
  const char *suffix = BKE_scene_multiview_view_suffix_get(rd, view);

  /* avoid nullptr pointer */
  if (!suffix) {
    suffix = "";
  }

  if (BKE_imtype_is_movie(rd->im_format.imtype)) {
    BKE_movie_filepath_get(filepath, rd, preview != 0, suffix);
  }
  else {
    BKE_image_path_from_imformat(filepath,
                                 rd->pic,
                                 BKE_main_blendfile_path(bmain),
                                 (frame == INT_MIN) ? rd->cfra : frame,
                                 &rd->im_format,
                                 (rd->scemode & R_EXTENSION) != 0,
                                 true,
                                 suffix);
  }
}

static void rna_Scene_ray_cast(Scene *scene,
                               Depsgraph *depsgraph,
                               float origin[3],
                               float direction[3],
                               float ray_dist,
                               bool *r_success,
                               float r_location[3],
                               float r_normal[3],
                               int *r_index,
                               Object **r_ob,
                               float r_obmat[16])
{
  normalize_v3(direction);
  SnapObjectContext *sctx = ED_transform_snap_object_context_create(scene, 0);

  SnapObjectParams snap_object_params{};
  snap_object_params.snap_target_select = SCE_SNAP_TARGET_ALL;

  bool ret = ED_transform_snap_object_project_ray_ex(sctx,
                                                     depsgraph,
                                                     nullptr,
                                                     &snap_object_params,
                                                     origin,
                                                     direction,
                                                     &ray_dist,
                                                     r_location,
                                                     r_normal,
                                                     r_index,
                                                     r_ob,
                                                     (float(*)[4])r_obmat);

  ED_transform_snap_object_context_destroy(sctx);

  if (r_ob != nullptr && *r_ob != nullptr) {
    *r_ob = DEG_get_original_object(*r_ob);
  }

  if (ret) {
    *r_success = true;
  }
  else {
    *r_success = false;

    unit_m4((float(*)[4])r_obmat);
    zero_v3(r_location);
    zero_v3(r_normal);
  }
}

static void rna_Scene_sequencer_editing_free(Scene *scene)
{
  SEQ_editing_free(scene, true);
}

#  ifdef WITH_ALEMBIC

static void rna_Scene_alembic_export(Scene *scene,
                                     bContext *C,
                                     const char *filepath,
                                     int frame_start,
                                     int frame_end,
                                     int xform_samples,
                                     int geom_samples,
                                     float shutter_open,
                                     float shutter_close,
                                     bool selected_only,
                                     bool uvs,
                                     bool normals,
                                     bool vcolors,
                                     bool apply_subdiv,
                                     bool flatten_hierarchy,
                                     bool visible_objects_only,
                                     bool face_sets,
                                     bool use_subdiv_schema,
                                     bool export_hair,
                                     bool export_particles,
                                     bool packuv,
                                     float scale,
                                     bool triangulate,
                                     int quad_method,
                                     int ngon_method)
{
/* We have to enable allow_threads, because we may change scene frame number
 * during export. */
#    ifdef WITH_PYTHON
  BPy_BEGIN_ALLOW_THREADS;
#    endif

  AlembicExportParams params{};
  params.frame_start = frame_start;
  params.frame_end = frame_end;

  params.frame_samples_xform = xform_samples;
  params.frame_samples_shape = geom_samples;

  params.shutter_open = shutter_open;
  params.shutter_close = shutter_close;

  params.selected_only = selected_only;
  params.uvs = uvs;
  params.normals = normals;
  params.vcolors = vcolors;
  params.apply_subdiv = apply_subdiv;
  params.flatten_hierarchy = flatten_hierarchy;
  params.visible_objects_only = visible_objects_only;
  params.face_sets = face_sets;
  params.use_subdiv_schema = use_subdiv_schema;
  params.export_hair = export_hair;
  params.export_particles = export_particles;
  params.packuv = packuv;
  params.triangulate = triangulate;
  params.quad_method = quad_method;
  params.ngon_method = ngon_method;

  params.global_scale = scale;

  ABC_export(scene, C, filepath, &params, true);

#    ifdef WITH_PYTHON
  BPy_END_ALLOW_THREADS;
#    endif
}

#  endif

#else

void RNA_api_scene(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "frame_set", "rna_Scene_frame_set");
  RNA_def_function_ui_description(func, "Set scene frame updating all objects immediately");
  parm = RNA_def_int(
      func, "frame", 0, MINAFRAME, MAXFRAME, "", "Frame number to set", MINAFRAME, MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_float(
      func, "subframe", 0.0, 0.0, 1.0, "", "Subframe time, between 0.0 and 1.0", 0.0, 1.0);
  RNA_def_function_flag(func, FUNC_USE_MAIN);

  func = RNA_def_function(srna, "uvedit_aspect", "rna_Scene_uvedit_aspect");
  RNA_def_function_ui_description(func, "Get uv aspect for current object");
  parm = RNA_def_pointer(func, "object", "Object", "", "Object");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_float_vector(
      func, "result", 2, nullptr, 0.0f, FLT_MAX, "", "aspect", 0.0f, FLT_MAX);
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_function_output(func, parm);

  /* Ray Cast */
  func = RNA_def_function(srna, "ray_cast", "rna_Scene_ray_cast");
  RNA_def_function_ui_description(func, "Cast a ray onto in object space");

  parm = RNA_def_pointer(func, "depsgraph", "Depsgraph", "", "The current dependency graph");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  /* ray start and end */
  parm = RNA_def_float_vector(func, "origin", 3, nullptr, -FLT_MAX, FLT_MAX, "", "", -1e4, 1e4);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_float_vector(func, "direction", 3, nullptr, -FLT_MAX, FLT_MAX, "", "", -1e4, 1e4);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_float(func,
                "distance",
                BVH_RAYCAST_DIST_MAX,
                0.0,
                BVH_RAYCAST_DIST_MAX,
                "",
                "Maximum distance",
                0.0,
                BVH_RAYCAST_DIST_MAX);
  /* return location and normal */
  parm = RNA_def_boolean(func, "result", 0, "", "");
  RNA_def_function_output(func, parm);
  parm = RNA_def_float_vector(func,
                              "location",
                              3,
                              nullptr,
                              -FLT_MAX,
                              FLT_MAX,
                              "Location",
                              "The hit location of this ray cast",
                              -1e4,
                              1e4);
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_function_output(func, parm);
  parm = RNA_def_float_vector(func,
                              "normal",
                              3,
                              nullptr,
                              -FLT_MAX,
                              FLT_MAX,
                              "Normal",
                              "The face normal at the ray cast hit location",
                              -1e4,
                              1e4);
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_function_output(func, parm);
  parm = RNA_def_int(
      func, "index", 0, 0, 0, "", "The face index, -1 when original data isn't available", 0, 0);
  RNA_def_function_output(func, parm);
  parm = RNA_def_pointer(func, "object", "Object", "", "Ray cast object");
  RNA_def_function_output(func, parm);
  parm = RNA_def_float_matrix(func, "matrix", 4, 4, nullptr, 0.0f, 0.0f, "", "Matrix", 0.0f, 0.0f);
  RNA_def_function_output(func, parm);

  /* Sequencer. */
  func = RNA_def_function(srna, "sequence_editor_create", "SEQ_editing_ensure");
  RNA_def_function_ui_description(func, "Ensure sequence editor is valid in this scene");
  parm = RNA_def_pointer(
      func, "sequence_editor", "SequenceEditor", "", "New sequence editor data or nullptr");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "sequence_editor_clear", "rna_Scene_sequencer_editing_free");
  RNA_def_function_ui_description(func, "Clear sequence editor in this scene");

#  ifdef WITH_ALEMBIC
  /* XXX Deprecated, will be removed in 2.8 in favor of calling the export operator. */
  func = RNA_def_function(srna, "alembic_export", "rna_Scene_alembic_export");
  RNA_def_function_ui_description(
      func, "Export to Alembic file (deprecated, use the Alembic export operator)");

  parm = RNA_def_string(
      func, "filepath", nullptr, FILE_MAX, "File Path", "File path to write Alembic file");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_property_subtype(parm, PROP_FILEPATH); /* allow non utf8 */

  RNA_def_int(func, "frame_start", 1, INT_MIN, INT_MAX, "Start", "Start Frame", INT_MIN, INT_MAX);
  RNA_def_int(func, "frame_end", 1, INT_MIN, INT_MAX, "End", "End Frame", INT_MIN, INT_MAX);
  RNA_def_int(
      func, "xform_samples", 1, 1, 128, "Xform samples", "Transform samples per frame", 1, 128);
  RNA_def_int(
      func, "geom_samples", 1, 1, 128, "Geom samples", "Geometry samples per frame", 1, 128);
  RNA_def_float(func, "shutter_open", 0.0f, -1.0f, 1.0f, "Shutter open", "", -1.0f, 1.0f);
  RNA_def_float(func, "shutter_close", 1.0f, -1.0f, 1.0f, "Shutter close", "", -1.0f, 1.0f);
  RNA_def_boolean(func, "selected_only", 0, "Selected only", "Export only selected objects");
  RNA_def_boolean(func, "uvs", 1, "UVs", "Export UVs");
  RNA_def_boolean(func, "normals", 1, "Normals", "Export normals");
  RNA_def_boolean(func, "vcolors", 0, "Color Attributes", "Export color attributes");
  RNA_def_boolean(
      func, "apply_subdiv", 1, "Subsurfs as meshes", "Export subdivision surfaces as meshes");
  RNA_def_boolean(func, "flatten", 0, "Flatten hierarchy", "Flatten hierarchy");
  RNA_def_boolean(func,
                  "visible_objects_only",
                  0,
                  "Visible layers only",
                  "Export only objects in visible layers");
  RNA_def_boolean(func, "face_sets", 0, "Facesets", "Export face sets");
  RNA_def_boolean(func,
                  "subdiv_schema",
                  0,
                  "Use Alembic subdivision Schema",
                  "Use Alembic subdivision Schema");
  RNA_def_boolean(
      func, "export_hair", 1, "Export Hair", "Exports hair particle systems as animated curves");
  RNA_def_boolean(
      func, "export_particles", 1, "Export Particles", "Exports non-hair particle systems");
  RNA_def_boolean(
      func, "packuv", 0, "Export with packed UV islands", "Export with packed UV islands");
  RNA_def_float(
      func,
      "scale",
      1.0f,
      0.0001f,
      1000.0f,
      "Scale",
      "Value by which to enlarge or shrink the objects with respect to the world's origin",
      0.0001f,
      1000.0f);
  RNA_def_boolean(
      func, "triangulate", 0, "Triangulate", "Export polygons (quads and n-gons) as triangles");
  RNA_def_enum(func,
               "quad_method",
               rna_enum_modifier_triangulate_quad_method_items,
               0,
               "Quad Method",
               "Method for splitting the quads into triangles");
  RNA_def_enum(func,
               "ngon_method",
               rna_enum_modifier_triangulate_ngon_method_items,
               0,
               "N-gon Method",
               "Method for splitting the n-gons into triangles");

  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
#  endif
}

void RNA_api_scene_render(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "frame_path", "rna_SceneRender_get_frame_path");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  RNA_def_function_ui_description(
      func, "Return the absolute path to the filename to be written for a given frame");
  RNA_def_int(func,
              "frame",
              INT_MIN,
              INT_MIN,
              INT_MAX,
              "",
              "Frame number to use, if unset the current frame will be used",
              MINAFRAME,
              MAXFRAME);
  RNA_def_boolean(func, "preview", 0, "Preview", "Use preview range");
  RNA_def_string_file_path(func,
                           "view",
                           nullptr,
                           FILE_MAX,
                           "View",
                           "The name of the view to use to replace the \"%\" chars");
  parm = RNA_def_string_file_path(func,
                                  "filepath",
                                  nullptr,
                                  FILE_MAX,
                                  "File Path",
                                  "The resulting filepath from the scenes render settings");
  RNA_def_parameter_flags(
      parm, PROP_THICK_WRAP, ParameterFlag(0)); /* needed for string return value */
  RNA_def_function_output(func, parm);
}

#endif
