/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

#  include <cerrno>
#  include <cstring>

#  include "MEM_guardedalloc.h"

#  include "DNA_modifier_types.h"
#  include "DNA_object_types.h"
#  include "DNA_scene_types.h"
#  include "DNA_space_types.h"

#  include "BKE_context.hh"
#  include "BKE_file_handler.hh"
#  include "BKE_main.hh"
#  include "BKE_report.hh"

#  include "BLI_path_util.h"
#  include "BLI_string.h"
#  include "BLI_utildefines.h"
#  include "BLI_vector.hh"

#  include "BLT_translation.hh"

#  include "RNA_access.hh"
#  include "RNA_define.hh"
#  include "RNA_enum_types.hh"

#  include "ED_fileselect.hh"
#  include "ED_object.hh"

#  include "UI_interface.hh"
#  include "UI_resources.hh"

#  include "WM_api.hh"
#  include "WM_types.hh"

#  include "DEG_depsgraph.hh"

#  include "io_alembic.hh"
#  include "io_utils.hh"

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
    {0, nullptr, 0, nullptr, nullptr},
};

static int wm_alembic_export_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  if (!RNA_struct_property_is_set(op->ptr, "as_background_job")) {
    RNA_boolean_set(op->ptr, "as_background_job", true);
  }

  RNA_boolean_set(op->ptr, "init_scene_frame_range", true);

  ED_fileselect_ensure_default_filepath(C, op, ".abc");

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static int wm_alembic_export_exec(bContext *C, wmOperator *op)
{
  if (!RNA_struct_property_is_set_ex(op->ptr, "filepath", false)) {
    BKE_report(op->reports, RPT_ERROR, "No filepath given");
    return OPERATOR_CANCELLED;
  }

  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);

  AlembicExportParams params{};
  params.frame_start = RNA_int_get(op->ptr, "start");
  params.frame_end = RNA_int_get(op->ptr, "end");

  params.frame_samples_xform = RNA_int_get(op->ptr, "xsamples");
  params.frame_samples_shape = RNA_int_get(op->ptr, "gsamples");

  params.shutter_open = RNA_float_get(op->ptr, "sh_open");
  params.shutter_close = RNA_float_get(op->ptr, "sh_close");

  params.selected_only = RNA_boolean_get(op->ptr, "selected");
  params.uvs = RNA_boolean_get(op->ptr, "uvs");
  params.normals = RNA_boolean_get(op->ptr, "normals");
  params.vcolors = RNA_boolean_get(op->ptr, "vcolors");
  params.orcos = RNA_boolean_get(op->ptr, "orcos");
  params.apply_subdiv = RNA_boolean_get(op->ptr, "apply_subdiv");
  params.curves_as_mesh = RNA_boolean_get(op->ptr, "curves_as_mesh");
  params.flatten_hierarchy = RNA_boolean_get(op->ptr, "flatten");
  params.visible_objects_only = RNA_boolean_get(op->ptr, "visible_objects_only");
  params.face_sets = RNA_boolean_get(op->ptr, "face_sets");
  params.use_subdiv_schema = RNA_boolean_get(op->ptr, "subdiv_schema");
  params.export_hair = RNA_boolean_get(op->ptr, "export_hair");
  params.export_particles = RNA_boolean_get(op->ptr, "export_particles");
  params.export_custom_properties = RNA_boolean_get(op->ptr, "export_custom_properties");
  params.use_instancing = RNA_boolean_get(op->ptr, "use_instancing");
  params.packuv = RNA_boolean_get(op->ptr, "packuv");
  params.triangulate = RNA_boolean_get(op->ptr, "triangulate");
  params.quad_method = RNA_enum_get(op->ptr, "quad_method");
  params.ngon_method = RNA_enum_get(op->ptr, "ngon_method");
  params.evaluation_mode = eEvaluationMode(RNA_enum_get(op->ptr, "evaluation_mode"));

  params.global_scale = RNA_float_get(op->ptr, "global_scale");

  /* Take some defaults from the scene, if not specified explicitly. */
  Scene *scene = CTX_data_scene(C);
  if (params.frame_start == INT_MIN) {
    params.frame_start = scene->r.sfra;
  }
  if (params.frame_end == INT_MIN) {
    params.frame_end = scene->r.efra;
  }

  const bool as_background_job = RNA_boolean_get(op->ptr, "as_background_job");
  bool ok = ABC_export(scene, C, filepath, &params, as_background_job);

  return as_background_job || ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void ui_alembic_export_settings(uiLayout *layout, PointerRNA *imfptr)
{
  uiLayout *box, *row, *col, *sub;

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  box = uiLayoutBox(layout);
  uiItemL(box, IFACE_("Manual Transform"), ICON_NONE);

  uiItemR(box, imfptr, "global_scale", UI_ITEM_NONE, nullptr, ICON_NONE);

  /* Scene Options */
  box = uiLayoutBox(layout);
  row = uiLayoutRow(box, false);
  uiItemL(row, IFACE_("Scene Options"), ICON_SCENE_DATA);

  col = uiLayoutColumn(box, false);

  sub = uiLayoutColumn(col, true);
  uiItemR(sub, imfptr, "start", UI_ITEM_NONE, IFACE_("Frame Start"), ICON_NONE);
  uiItemR(sub, imfptr, "end", UI_ITEM_NONE, IFACE_("End"), ICON_NONE);

  uiItemR(col, imfptr, "xsamples", UI_ITEM_NONE, IFACE_("Samples Transform"), ICON_NONE);
  uiItemR(col, imfptr, "gsamples", UI_ITEM_NONE, IFACE_("Geometry"), ICON_NONE);

  sub = uiLayoutColumn(col, true);
  uiItemR(sub, imfptr, "sh_open", UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  uiItemR(sub, imfptr, "sh_close", UI_ITEM_R_SLIDER, IFACE_("Close"), ICON_NONE);

  uiItemS(col);

  uiItemR(col, imfptr, "flatten", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(sub, imfptr, "use_instancing", UI_ITEM_NONE, IFACE_("Use Instancing"), ICON_NONE);
  uiItemR(sub,
          imfptr,
          "export_custom_properties",
          UI_ITEM_NONE,
          IFACE_("Custom Properties"),
          ICON_NONE);

  sub = uiLayoutColumnWithHeading(col, true, IFACE_("Only"));
  uiItemR(sub, imfptr, "selected", UI_ITEM_NONE, IFACE_("Selected Objects"), ICON_NONE);
  uiItemR(sub, imfptr, "visible_objects_only", UI_ITEM_NONE, IFACE_("Visible Objects"), ICON_NONE);

  col = uiLayoutColumn(box, true);
  uiItemR(col, imfptr, "evaluation_mode", UI_ITEM_NONE, nullptr, ICON_NONE);

  /* Object Data */
  box = uiLayoutBox(layout);
  row = uiLayoutRow(box, false);
  uiItemL(row, IFACE_("Object Options"), ICON_OBJECT_DATA);

  col = uiLayoutColumn(box, false);

  uiItemR(col, imfptr, "uvs", UI_ITEM_NONE, nullptr, ICON_NONE);
  row = uiLayoutRow(col, false);
  uiLayoutSetActive(row, RNA_boolean_get(imfptr, "uvs"));
  uiItemR(row, imfptr, "packuv", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiItemR(col, imfptr, "normals", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, imfptr, "vcolors", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, imfptr, "orcos", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, imfptr, "face_sets", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, imfptr, "curves_as_mesh", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiItemS(col);

  sub = uiLayoutColumnWithHeading(col, true, IFACE_("Subdivisions"));
  uiItemR(sub, imfptr, "apply_subdiv", UI_ITEM_NONE, IFACE_("Apply"), ICON_NONE);
  uiItemR(sub, imfptr, "subdiv_schema", UI_ITEM_NONE, IFACE_("Use Schema"), ICON_NONE);

  uiItemS(col);

  col = uiLayoutColumn(box, false);
  uiItemR(col, imfptr, "triangulate", UI_ITEM_NONE, nullptr, ICON_NONE);
  sub = uiLayoutColumn(col, false);
  uiLayoutSetActive(sub, RNA_boolean_get(imfptr, "triangulate"));
  uiItemR(sub, imfptr, "quad_method", UI_ITEM_NONE, IFACE_("Method Quads"), ICON_NONE);
  uiItemR(sub, imfptr, "ngon_method", UI_ITEM_NONE, IFACE_("Polygons"), ICON_NONE);

  /* Particle Data */
  box = uiLayoutBox(layout);
  row = uiLayoutRow(box, false);
  uiItemL(row, IFACE_("Particle Systems"), ICON_PARTICLE_DATA);

  col = uiLayoutColumn(box, true);
  uiItemR(col, imfptr, "export_hair", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, imfptr, "export_particles", UI_ITEM_NONE, nullptr, ICON_NONE);
}

static void wm_alembic_export_draw(bContext *C, wmOperator *op)
{
  /* Conveniently set start and end frame to match the scene's frame range. */
  Scene *scene = CTX_data_scene(C);

  if (scene != nullptr && RNA_boolean_get(op->ptr, "init_scene_frame_range")) {
    RNA_int_set(op->ptr, "start", scene->r.sfra);
    RNA_int_set(op->ptr, "end", scene->r.efra);

    RNA_boolean_set(op->ptr, "init_scene_frame_range", false);
  }

  ui_alembic_export_settings(op->layout, op->ptr);
}

static bool wm_alembic_export_check(bContext * /*C*/, wmOperator *op)
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
      ot->srna, "selected", false, "Selected Objects Only", "Export only selected objects");

  RNA_def_boolean(ot->srna,
                  "visible_objects_only",
                  false,
                  "Visible Objects Only",
                  "Export only objects that are visible");

  RNA_def_boolean(ot->srna,
                  "flatten",
                  false,
                  "Flatten Hierarchy",
                  "Do not preserve objects' parent/children relationship");

  RNA_def_boolean(ot->srna, "uvs", true, "UVs", "Export UVs");

  RNA_def_boolean(ot->srna, "packuv", true, "Pack UV Islands", "Export UVs with packed island");

  RNA_def_boolean(ot->srna, "normals", true, "Normals", "Export normals");

  RNA_def_boolean(ot->srna, "vcolors", false, "Color Attributes", "Export color attributes");

  RNA_def_boolean(ot->srna,
                  "orcos",
                  true,
                  "Generated Coordinates",
                  "Export undeformed mesh vertex coordinates");

  RNA_def_boolean(
      ot->srna, "face_sets", false, "Face Sets", "Export per face shading group assignments");

  RNA_def_boolean(ot->srna,
                  "subdiv_schema",
                  false,
                  "Use Subdivision Schema",
                  "Export meshes using Alembic's subdivision schema");

  RNA_def_boolean(ot->srna,
                  "apply_subdiv",
                  false,
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
                  true,
                  "Export Hair",
                  "Exports hair particle systems as animated curves");
  RNA_def_boolean(
      ot->srna, "export_particles", true, "Export Particles", "Exports non-hair particle systems");

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

/* TODO(kevin): check on de-duplicating all this with code in `image_ops.cc` */

struct CacheFrame {
  CacheFrame *next, *prev;
  int framenr;
};

static int get_sequence_len(const char *filepath, int *ofs)
{
  int frame;
  int numdigit;

  if (!BLI_path_frame_get(filepath, &frame, &numdigit)) {
    return 1;
  }

  char dirpath[FILE_MAX];
  BLI_path_split_dir_part(filepath, dirpath, FILE_MAX);

  if (dirpath[0] == '\0') {
    /* The `filepath` had no directory component, so just use the blend files directory. */
    BLI_path_split_dir_part(BKE_main_blendfile_path_from_global(), dirpath, sizeof(dirpath));
  }
  else {
    BLI_path_abs(dirpath, BKE_main_blendfile_path_from_global());
  }

  DIR *dir = opendir(dirpath);
  if (dir == nullptr) {
    fprintf(stderr,
            "Error opening directory '%s': %s\n",
            dirpath,
            errno ? strerror(errno) : "unknown error");
    return -1;
  }

  const char *ext = ".abc";
  const char *basename = BLI_path_basename(filepath);
  const int len = strlen(basename) - (numdigit + strlen(ext));

  blender::Vector<CacheFrame> frames;

  dirent *fname;
  while ((fname = readdir(dir)) != nullptr) {
    /* do we have the right extension? */
    if (!strstr(fname->d_name, ext)) {
      continue;
    }

    if (!STREQLEN(basename, fname->d_name, len)) {
      continue;
    }

    CacheFrame cache_frame{};

    BLI_path_frame_get(fname->d_name, &cache_frame.framenr, &numdigit);

    frames.append(cache_frame);
  }

  closedir(dir);

  std::sort(frames.begin(), frames.end(), [](const CacheFrame &a, const CacheFrame &b) {
    return a.framenr < b.framenr;
  });

  if (frames.is_empty()) {
    return -1;
  }

  int frame_curr = frames.first().framenr;
  (*ofs) = frame_curr;

  for (CacheFrame &cache_frame : frames) {
    if (cache_frame.framenr != frame_curr) {
      break;
    }
    frame_curr++;
  }

  return frame_curr - (*ofs);
}

/* ************************************************************************** */

static void ui_alembic_import_settings(uiLayout *layout, PointerRNA *imfptr)
{

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiLayout *box = uiLayoutBox(layout);
  uiLayout *row = uiLayoutRow(box, false);
  uiItemL(row, IFACE_("Manual Transform"), ICON_NONE);

  uiItemR(box, imfptr, "scale", UI_ITEM_NONE, nullptr, ICON_NONE);

  box = uiLayoutBox(layout);
  row = uiLayoutRow(box, false);
  uiItemL(row, IFACE_("Options"), ICON_NONE);

  uiLayout *col = uiLayoutColumn(box, false);
  uiItemR(col, imfptr, "relative_path", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, imfptr, "set_frame_range", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, imfptr, "is_sequence", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, imfptr, "validate_meshes", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, imfptr, "always_add_cache_reader", UI_ITEM_NONE, nullptr, ICON_NONE);
}

static void wm_alembic_import_draw(bContext * /*C*/, wmOperator *op)
{
  ui_alembic_import_settings(op->layout, op->ptr);
}

/* op->invoke, opens fileselect if path property not set, otherwise executes */
static int wm_alembic_import_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (!RNA_struct_property_is_set(op->ptr, "as_background_job")) {
    RNA_boolean_set(op->ptr, "as_background_job", true);
  }
  return blender::ed::io::filesel_drop_import_invoke(C, op, event);
}

static int wm_alembic_import_exec(bContext *C, wmOperator *op)
{
  if (!RNA_struct_property_is_set_ex(op->ptr, "filepath", false)) {
    BKE_report(op->reports, RPT_ERROR, "No filepath given");
    return OPERATOR_CANCELLED;
  }

  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);

  const float scale = RNA_float_get(op->ptr, "scale");
  const bool is_sequence = RNA_boolean_get(op->ptr, "is_sequence");
  const bool set_frame_range = RNA_boolean_get(op->ptr, "set_frame_range");
  const bool validate_meshes = RNA_boolean_get(op->ptr, "validate_meshes");
  const bool always_add_cache_reader = RNA_boolean_get(op->ptr, "always_add_cache_reader");
  const bool as_background_job = RNA_boolean_get(op->ptr, "as_background_job");

  int offset = 0;
  int sequence_len = 1;

  if (is_sequence) {
    sequence_len = get_sequence_len(filepath, &offset);
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

  AlembicImportParams params = {0};
  params.global_scale = scale;
  params.sequence_len = sequence_len;
  params.sequence_offset = offset;
  params.is_sequence = is_sequence;
  params.set_frame_range = set_frame_range;
  params.validate_meshes = validate_meshes;
  params.always_add_cache_reader = always_add_cache_reader;

  bool ok = ABC_import(C, filepath, &params, as_background_job);

  return as_background_job || ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void WM_OT_alembic_import(wmOperatorType *ot)
{
  ot->name = "Import Alembic";
  ot->description = "Load an Alembic archive";
  ot->idname = "WM_OT_alembic_import";
  ot->flag = OPTYPE_UNDO | OPTYPE_PRESET;

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
                  false,
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

namespace blender::ed::io {
void alembic_file_handler_add()
{
  auto fh = std::make_unique<blender::bke::FileHandlerType>();
  STRNCPY(fh->idname, "IO_FH_alembic");
  STRNCPY(fh->import_operator, "WM_OT_alembic_import");
  STRNCPY(fh->label, "Alembic");
  STRNCPY(fh->file_extensions_str, ".abc");
  fh->poll_drop = poll_file_object_drop;
  bke::file_handler_add(std::move(fh));
}
}  // namespace blender::ed::io

#endif
