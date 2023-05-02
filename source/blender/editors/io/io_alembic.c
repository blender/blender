/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2016 Blender Foundation */

/** \file
 * \ingroup editor/io
 */

#ifdef WITH_ALEMBIC

/* needed for directory lookup */
#  ifndef WIN32
#    include <dirent.h>
#  else
#    include "BLI_winstuff.h"
#  endif

#  include <errno.h>
#  include <string.h>

#  include "MEM_guardedalloc.h"

#  include "DNA_modifier_types.h"
#  include "DNA_object_types.h"
#  include "DNA_scene_types.h"
#  include "DNA_space_types.h"

#  include "BKE_context.h"
#  include "BKE_main.h"
#  include "BKE_report.h"

#  include "BLI_listbase.h"
#  include "BLI_path_util.h"
#  include "BLI_string.h"
#  include "BLI_utildefines.h"

#  include "BLT_translation.h"

#  include "RNA_access.h"
#  include "RNA_define.h"
#  include "RNA_enum_types.h"

#  include "ED_fileselect.h"
#  include "ED_object.h"

#  include "UI_interface.h"
#  include "UI_resources.h"

#  include "WM_api.h"
#  include "WM_types.h"

#  include "DEG_depsgraph.h"

#  include "io_alembic.h"

#  include "ABC_alembic.h"

const EnumPropertyItem rna_enum_abc_export_evaluation_mode_items[] = {
    {DAG_EVAL_RENDER,
     "RENDER",
     0,
     "Render",
     "Use Render settings for object visibility, modifier settings, etc"},
    {DAG_EVAL_VIEWPORT,
     "VIEWPORT",
     0,
     "Viewport",
     "Use Viewport settings for object visibility, modifier settings, etc"},
    {0, NULL, 0, NULL, NULL},
};

static int wm_alembic_export_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (!RNA_struct_property_is_set(op->ptr, "as_background_job")) {
    RNA_boolean_set(op->ptr, "as_background_job", true);
  }

  RNA_boolean_set(op->ptr, "init_scene_frame_range", true);

  ED_fileselect_ensure_default_filepath(C, op, ".abc");

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;

  UNUSED_VARS(event);
}

static int wm_alembic_export_exec(bContext *C, wmOperator *op)
{
  if (!RNA_struct_property_is_set_ex(op->ptr, "filepath", false)) {
    BKE_report(op->reports, RPT_ERROR, "No filename given");
    return OPERATOR_CANCELLED;
  }

  char filename[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filename);

  struct AlembicExportParams params = {
      .frame_start = RNA_int_get(op->ptr, "start"),
      .frame_end = RNA_int_get(op->ptr, "end"),

      .frame_samples_xform = RNA_int_get(op->ptr, "xsamples"),
      .frame_samples_shape = RNA_int_get(op->ptr, "gsamples"),

      .shutter_open = RNA_float_get(op->ptr, "sh_open"),
      .shutter_close = RNA_float_get(op->ptr, "sh_close"),

      .selected_only = RNA_boolean_get(op->ptr, "selected"),
      .uvs = RNA_boolean_get(op->ptr, "uvs"),
      .normals = RNA_boolean_get(op->ptr, "normals"),
      .vcolors = RNA_boolean_get(op->ptr, "vcolors"),
      .orcos = RNA_boolean_get(op->ptr, "orcos"),
      .apply_subdiv = RNA_boolean_get(op->ptr, "apply_subdiv"),
      .curves_as_mesh = RNA_boolean_get(op->ptr, "curves_as_mesh"),
      .flatten_hierarchy = RNA_boolean_get(op->ptr, "flatten"),
      .visible_objects_only = RNA_boolean_get(op->ptr, "visible_objects_only"),
      .face_sets = RNA_boolean_get(op->ptr, "face_sets"),
      .use_subdiv_schema = RNA_boolean_get(op->ptr, "subdiv_schema"),
      .export_hair = RNA_boolean_get(op->ptr, "export_hair"),
      .export_particles = RNA_boolean_get(op->ptr, "export_particles"),
      .export_custom_properties = RNA_boolean_get(op->ptr, "export_custom_properties"),
      .use_instancing = RNA_boolean_get(op->ptr, "use_instancing"),
      .packuv = RNA_boolean_get(op->ptr, "packuv"),
      .triangulate = RNA_boolean_get(op->ptr, "triangulate"),
      .quad_method = RNA_enum_get(op->ptr, "quad_method"),
      .ngon_method = RNA_enum_get(op->ptr, "ngon_method"),
      .evaluation_mode = RNA_enum_get(op->ptr, "evaluation_mode"),

      .global_scale = RNA_float_get(op->ptr, "global_scale"),
  };

  /* Take some defaults from the scene, if not specified explicitly. */
  Scene *scene = CTX_data_scene(C);
  if (params.frame_start == INT_MIN) {
    params.frame_start = scene->r.sfra;
  }
  if (params.frame_end == INT_MIN) {
    params.frame_end = scene->r.efra;
  }

  const bool as_background_job = RNA_boolean_get(op->ptr, "as_background_job");
  bool ok = ABC_export(scene, C, filename, &params, as_background_job);

  return as_background_job || ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void ui_alembic_export_settings(uiLayout *layout, PointerRNA *imfptr)
{
  uiLayout *box, *row, *col, *sub;

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  box = uiLayoutBox(layout);
  uiItemL(box, IFACE_("Manual Transform"), ICON_NONE);

  uiItemR(box, imfptr, "global_scale", 0, NULL, ICON_NONE);

  /* Scene Options */
  box = uiLayoutBox(layout);
  row = uiLayoutRow(box, false);
  uiItemL(row, IFACE_("Scene Options"), ICON_SCENE_DATA);

  col = uiLayoutColumn(box, false);

  sub = uiLayoutColumn(col, true);
  uiItemR(sub, imfptr, "start", 0, IFACE_("Frame Start"), ICON_NONE);
  uiItemR(sub, imfptr, "end", 0, IFACE_("End"), ICON_NONE);

  uiItemR(col, imfptr, "xsamples", 0, IFACE_("Samples Transform"), ICON_NONE);
  uiItemR(col, imfptr, "gsamples", 0, IFACE_("Geometry"), ICON_NONE);

  sub = uiLayoutColumn(col, true);
  uiItemR(sub, imfptr, "sh_open", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(sub, imfptr, "sh_close", UI_ITEM_R_SLIDER, IFACE_("Close"), ICON_NONE);

  uiItemS(col);

  uiItemR(col, imfptr, "flatten", 0, NULL, ICON_NONE);
  uiItemR(sub, imfptr, "use_instancing", 0, IFACE_("Use Instancing"), ICON_NONE);
  uiItemR(sub, imfptr, "export_custom_properties", 0, IFACE_("Custom Properties"), ICON_NONE);

  sub = uiLayoutColumnWithHeading(col, true, IFACE_("Only"));
  uiItemR(sub, imfptr, "selected", 0, IFACE_("Selected Objects"), ICON_NONE);
  uiItemR(sub, imfptr, "visible_objects_only", 0, IFACE_("Visible Objects"), ICON_NONE);

  col = uiLayoutColumn(box, true);
  uiItemR(col, imfptr, "evaluation_mode", 0, NULL, ICON_NONE);

  /* Object Data */
  box = uiLayoutBox(layout);
  row = uiLayoutRow(box, false);
  uiItemL(row, IFACE_("Object Options"), ICON_OBJECT_DATA);

  col = uiLayoutColumn(box, false);

  uiItemR(col, imfptr, "uvs", 0, NULL, ICON_NONE);
  row = uiLayoutRow(col, false);
  uiLayoutSetActive(row, RNA_boolean_get(imfptr, "uvs"));
  uiItemR(row, imfptr, "packuv", 0, NULL, ICON_NONE);

  uiItemR(col, imfptr, "normals", 0, NULL, ICON_NONE);
  uiItemR(col, imfptr, "vcolors", 0, NULL, ICON_NONE);
  uiItemR(col, imfptr, "orcos", 0, NULL, ICON_NONE);
  uiItemR(col, imfptr, "face_sets", 0, NULL, ICON_NONE);
  uiItemR(col, imfptr, "curves_as_mesh", 0, NULL, ICON_NONE);

  uiItemS(col);

  sub = uiLayoutColumnWithHeading(col, true, IFACE_("Subdivisions"));
  uiItemR(sub, imfptr, "apply_subdiv", 0, IFACE_("Apply"), ICON_NONE);
  uiItemR(sub, imfptr, "subdiv_schema", 0, IFACE_("Use Schema"), ICON_NONE);

  uiItemS(col);

  col = uiLayoutColumn(box, false);
  uiItemR(col, imfptr, "triangulate", 0, NULL, ICON_NONE);
  sub = uiLayoutColumn(col, false);
  uiLayoutSetActive(sub, RNA_boolean_get(imfptr, "triangulate"));
  uiItemR(sub, imfptr, "quad_method", 0, IFACE_("Method Quads"), ICON_NONE);
  uiItemR(sub, imfptr, "ngon_method", 0, IFACE_("Polygons"), ICON_NONE);

  /* Particle Data */
  box = uiLayoutBox(layout);
  row = uiLayoutRow(box, false);
  uiItemL(row, IFACE_("Particle Systems"), ICON_PARTICLE_DATA);

  col = uiLayoutColumn(box, true);
  uiItemR(col, imfptr, "export_hair", 0, NULL, ICON_NONE);
  uiItemR(col, imfptr, "export_particles", 0, NULL, ICON_NONE);
}

static void wm_alembic_export_draw(bContext *C, wmOperator *op)
{
  /* Conveniently set start and end frame to match the scene's frame range. */
  Scene *scene = CTX_data_scene(C);

  if (scene != NULL && RNA_boolean_get(op->ptr, "init_scene_frame_range")) {
    RNA_int_set(op->ptr, "start", scene->r.sfra);
    RNA_int_set(op->ptr, "end", scene->r.efra);

    RNA_boolean_set(op->ptr, "init_scene_frame_range", false);
  }

  ui_alembic_export_settings(op->layout, op->ptr);
}

static bool wm_alembic_export_check(bContext *UNUSED(C), wmOperator *op)
{
  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);

  if (!BLI_path_extension_check(filepath, ".abc")) {
    BLI_path_extension_ensure(filepath, FILE_MAX, ".abc");
    RNA_string_set(op->ptr, "filepath", filepath);
    return true;
  }

  return false;
}

void WM_OT_alembic_export(wmOperatorType *ot)
{
  ot->name = "Export Alembic";
  ot->description = "Export current scene in an Alembic archive";
  ot->idname = "WM_OT_alembic_export";

  ot->invoke = wm_alembic_export_invoke;
  ot->exec = wm_alembic_export_exec;
  ot->poll = WM_operator_winactive;
  ot->ui = wm_alembic_export_draw;
  ot->check = wm_alembic_export_check;
  ot->flag = OPTYPE_PRESET;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_ALEMBIC,
                                 FILE_BLENDER,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  PropertyRNA *prop = RNA_def_string(ot->srna, "filter_glob", "*.abc", 0, "", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);

  RNA_def_int(ot->srna,
              "start",
              INT_MIN,
              INT_MIN,
              INT_MAX,
              "Start Frame",
              "Start frame of the export, use the default value to "
              "take the start frame of the current scene",
              INT_MIN,
              INT_MAX);

  RNA_def_int(ot->srna,
              "end",
              INT_MIN,
              INT_MIN,
              INT_MAX,
              "End Frame",
              "End frame of the export, use the default value to "
              "take the end frame of the current scene",
              INT_MIN,
              INT_MAX);

  RNA_def_int(ot->srna,
              "xsamples",
              1,
              1,
              128,
              "Transform Samples",
              "Number of times per frame transformations are sampled",
              1,
              128);

  RNA_def_int(ot->srna,
              "gsamples",
              1,
              1,
              128,
              "Geometry Samples",
              "Number of times per frame object data are sampled",
              1,
              128);

  RNA_def_float(ot->srna,
                "sh_open",
                0.0f,
                -1.0f,
                1.0f,
                "Shutter Open",
                "Time at which the shutter is open",
                -1.0f,
                1.0f);

  RNA_def_float(ot->srna,
                "sh_close",
                1.0f,
                -1.0f,
                1.0f,
                "Shutter Close",
                "Time at which the shutter is closed",
                -1.0f,
                1.0f);

  RNA_def_boolean(
      ot->srna, "selected", 0, "Selected Objects Only", "Export only selected objects");

  RNA_def_boolean(ot->srna,
                  "visible_objects_only",
                  0,
                  "Visible Objects Only",
                  "Export only objects that are visible");

  RNA_def_boolean(ot->srna,
                  "flatten",
                  0,
                  "Flatten Hierarchy",
                  "Do not preserve objects' parent/children relationship");

  RNA_def_boolean(ot->srna, "uvs", 1, "UVs", "Export UVs");

  RNA_def_boolean(ot->srna, "packuv", 1, "Pack UV Islands", "Export UVs with packed island");

  RNA_def_boolean(ot->srna, "normals", 1, "Normals", "Export normals");

  RNA_def_boolean(ot->srna, "vcolors", 0, "Color Attributes", "Export color attributes");

  RNA_def_boolean(ot->srna,
                  "orcos",
                  true,
                  "Generated Coordinates",
                  "Export undeformed mesh vertex coordinates");

  RNA_def_boolean(
      ot->srna, "face_sets", 0, "Face Sets", "Export per face shading group assignments");

  RNA_def_boolean(ot->srna,
                  "subdiv_schema",
                  0,
                  "Use Subdivision Schema",
                  "Export meshes using Alembic's subdivision schema");

  RNA_def_boolean(ot->srna,
                  "apply_subdiv",
                  0,
                  "Apply Subdivision Surface",
                  "Export subdivision surfaces as meshes");

  RNA_def_boolean(ot->srna,
                  "curves_as_mesh",
                  false,
                  "Curves as Mesh",
                  "Export curves and NURBS surfaces as meshes");

  RNA_def_boolean(ot->srna,
                  "use_instancing",
                  true,
                  "Use Instancing",
                  "Export data of duplicated objects as Alembic instances; speeds up the export "
                  "and can be disabled for compatibility with other software");

  RNA_def_float(
      ot->srna,
      "global_scale",
      1.0f,
      0.0001f,
      1000.0f,
      "Scale",
      "Value by which to enlarge or shrink the objects with respect to the world's origin",
      0.0001f,
      1000.0f);

  RNA_def_boolean(ot->srna,
                  "triangulate",
                  false,
                  "Triangulate",
                  "Export polygons (quads and n-gons) as triangles");

  RNA_def_enum(ot->srna,
               "quad_method",
               rna_enum_modifier_triangulate_quad_method_items,
               MOD_TRIANGULATE_QUAD_SHORTEDGE,
               "Quad Method",
               "Method for splitting the quads into triangles");

  RNA_def_enum(ot->srna,
               "ngon_method",
               rna_enum_modifier_triangulate_ngon_method_items,
               MOD_TRIANGULATE_NGON_BEAUTY,
               "N-gon Method",
               "Method for splitting the n-gons into triangles");

  RNA_def_boolean(ot->srna,
                  "export_hair",
                  1,
                  "Export Hair",
                  "Exports hair particle systems as animated curves");
  RNA_def_boolean(
      ot->srna, "export_particles", 1, "Export Particles", "Exports non-hair particle systems");

  RNA_def_boolean(ot->srna,
                  "export_custom_properties",
                  true,
                  "Export Custom Properties",
                  "Export custom properties to Alembic .userProperties");

  RNA_def_boolean(
      ot->srna,
      "as_background_job",
      false,
      "Run as Background Job",
      "Enable this to run the import in the background, disable to block Blender while importing. "
      "This option is deprecated; EXECUTE this operator to run in the foreground, and INVOKE it "
      "to run as a background job");

  RNA_def_enum(ot->srna,
               "evaluation_mode",
               rna_enum_abc_export_evaluation_mode_items,
               DAG_EVAL_RENDER,
               "Use Settings for",
               "Determines visibility of objects, modifier settings, and other areas where there "
               "are different settings for viewport and rendering");

  /* This dummy prop is used to check whether we need to init the start and
   * end frame values to that of the scene's, otherwise they are reset at
   * every change, draw update. */
  RNA_def_boolean(ot->srna, "init_scene_frame_range", true, "", "");
}

/* ************************************************************************** */

/* TODO(kevin): check on de-duplicating all this with code in image_ops.c */

typedef struct CacheFrame {
  struct CacheFrame *next, *prev;
  int framenr;
} CacheFrame;

static int cmp_frame(const void *a, const void *b)
{
  const CacheFrame *frame_a = a;
  const CacheFrame *frame_b = b;

  if (frame_a->framenr < frame_b->framenr) {
    return -1;
  }
  if (frame_a->framenr > frame_b->framenr) {
    return 1;
  }
  return 0;
}

static int get_sequence_len(const char *filename, int *ofs)
{
  int frame;
  int numdigit;

  if (!BLI_path_frame_get(filename, &frame, &numdigit)) {
    return 1;
  }

  char path[FILE_MAX];
  BLI_path_split_dir_part(filename, path, FILE_MAX);

  if (path[0] == '\0') {
    /* The filename had no path, so just use the blend file path. */
    BLI_path_split_dir_part(BKE_main_blendfile_path_from_global(), path, FILE_MAX);
  }
  else {
    BLI_path_abs(path, BKE_main_blendfile_path_from_global());
  }

  DIR *dir = opendir(path);
  if (dir == NULL) {
    fprintf(stderr,
            "Error opening directory '%s': %s\n",
            path,
            errno ? strerror(errno) : "unknown error");
    return -1;
  }

  const char *ext = ".abc";
  const char *basename = BLI_path_basename(filename);
  const int len = strlen(basename) - (numdigit + strlen(ext));

  ListBase frames;
  BLI_listbase_clear(&frames);

  struct dirent *fname;
  while ((fname = readdir(dir)) != NULL) {
    /* do we have the right extension? */
    if (!strstr(fname->d_name, ext)) {
      continue;
    }

    if (!STREQLEN(basename, fname->d_name, len)) {
      continue;
    }

    CacheFrame *cache_frame = MEM_callocN(sizeof(CacheFrame), "abc_frame");

    BLI_path_frame_get(fname->d_name, &cache_frame->framenr, &numdigit);

    BLI_addtail(&frames, cache_frame);
  }

  closedir(dir);

  BLI_listbase_sort(&frames, cmp_frame);

  CacheFrame *cache_frame = frames.first;

  if (cache_frame) {
    int frame_curr = cache_frame->framenr;
    (*ofs) = frame_curr;

    while (cache_frame && (cache_frame->framenr == frame_curr)) {
      frame_curr++;
      cache_frame = cache_frame->next;
    }

    BLI_freelistN(&frames);

    return frame_curr - (*ofs);
  }

  return 1;
}

/* ************************************************************************** */

static void ui_alembic_import_settings(uiLayout *layout, PointerRNA *imfptr)
{

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiLayout *box = uiLayoutBox(layout);
  uiLayout *row = uiLayoutRow(box, false);
  uiItemL(row, IFACE_("Manual Transform"), ICON_NONE);

  uiItemR(box, imfptr, "scale", 0, NULL, ICON_NONE);

  box = uiLayoutBox(layout);
  row = uiLayoutRow(box, false);
  uiItemL(row, IFACE_("Options"), ICON_NONE);

  uiLayout *col = uiLayoutColumn(box, false);
  uiItemR(col, imfptr, "relative_path", 0, NULL, ICON_NONE);
  uiItemR(col, imfptr, "set_frame_range", 0, NULL, ICON_NONE);
  uiItemR(col, imfptr, "is_sequence", 0, NULL, ICON_NONE);
  uiItemR(col, imfptr, "validate_meshes", 0, NULL, ICON_NONE);
  uiItemR(col, imfptr, "always_add_cache_reader", 0, NULL, ICON_NONE);
}

static void wm_alembic_import_draw(bContext *UNUSED(C), wmOperator *op)
{
  ui_alembic_import_settings(op->layout, op->ptr);
}

/* op->invoke, opens fileselect if path property not set, otherwise executes */
static int wm_alembic_import_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (!RNA_struct_property_is_set(op->ptr, "as_background_job")) {
    RNA_boolean_set(op->ptr, "as_background_job", true);
  }
  return WM_operator_filesel(C, op, event);
}

static int wm_alembic_import_exec(bContext *C, wmOperator *op)
{
  if (!RNA_struct_property_is_set_ex(op->ptr, "filepath", false)) {
    BKE_report(op->reports, RPT_ERROR, "No filename given");
    return OPERATOR_CANCELLED;
  }

  char filename[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filename);

  const float scale = RNA_float_get(op->ptr, "scale");
  const bool is_sequence = RNA_boolean_get(op->ptr, "is_sequence");
  const bool set_frame_range = RNA_boolean_get(op->ptr, "set_frame_range");
  const bool validate_meshes = RNA_boolean_get(op->ptr, "validate_meshes");
  const bool always_add_cache_reader = RNA_boolean_get(op->ptr, "always_add_cache_reader");
  const bool as_background_job = RNA_boolean_get(op->ptr, "as_background_job");

  int offset = 0;
  int sequence_len = 1;

  if (is_sequence) {
    sequence_len = get_sequence_len(filename, &offset);
    if (sequence_len < 0) {
      BKE_report(op->reports, RPT_ERROR, "Unable to determine ABC sequence length");
      return OPERATOR_CANCELLED;
    }
  }

  /* Switch out of edit mode to avoid being stuck in it (#54326). */
  Object *obedit = CTX_data_edit_object(C);
  if (obedit) {
    ED_object_mode_set(C, OB_MODE_OBJECT);
  }

  struct AlembicImportParams params = {0};
  params.global_scale = scale;
  params.sequence_len = sequence_len;
  params.sequence_offset = offset;
  params.is_sequence = is_sequence;
  params.set_frame_range = set_frame_range;
  params.validate_meshes = validate_meshes;
  params.always_add_cache_reader = always_add_cache_reader;

  bool ok = ABC_import(C, filename, &params, as_background_job);

  return as_background_job || ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void WM_OT_alembic_import(wmOperatorType *ot)
{
  ot->name = "Import Alembic";
  ot->description = "Load an Alembic archive";
  ot->idname = "WM_OT_alembic_import";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_PRESET;

  ot->invoke = wm_alembic_import_invoke;
  ot->exec = wm_alembic_import_exec;
  ot->poll = WM_operator_winactive;
  ot->ui = wm_alembic_import_draw;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_ALEMBIC,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  PropertyRNA *prop = RNA_def_string(ot->srna, "filter_glob", "*.abc", 0, "", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);

  RNA_def_float(
      ot->srna,
      "scale",
      1.0f,
      0.0001f,
      1000.0f,
      "Scale",
      "Value by which to enlarge or shrink the objects with respect to the world's origin",
      0.0001f,
      1000.0f);

  RNA_def_boolean(
      ot->srna,
      "set_frame_range",
      true,
      "Set Frame Range",
      "If checked, update scene's start and end frame to match those of the Alembic archive");

  RNA_def_boolean(ot->srna,
                  "validate_meshes",
                  0,
                  "Validate Meshes",
                  "Check imported mesh objects for invalid data (slow)");

  RNA_def_boolean(ot->srna,
                  "always_add_cache_reader",
                  false,
                  "Always Add Cache Reader",
                  "Add cache modifiers and constraints to imported objects even if they are not "
                  "animated so that they can be updated when reloading the Alembic archive");

  RNA_def_boolean(ot->srna,
                  "is_sequence",
                  false,
                  "Is Sequence",
                  "Set to true if the cache is split into separate files");

  RNA_def_boolean(
      ot->srna,
      "as_background_job",
      false,
      "Run as Background Job",
      "Enable this to run the export in the background, disable to block Blender while exporting. "
      "This option is deprecated; EXECUTE this operator to run in the foreground, and INVOKE it "
      "to run as a background job");
}

#endif
