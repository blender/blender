/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edproject
 */

#include <climits>

#include "BKE_asset_library_custom.h"
#include "BKE_blender_project.h"
#include "BKE_context.h"
#include "BKE_report.h"

#include "BLI_path_util.h"

#include "DNA_space_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_project.h"

using namespace blender;

/* -------------------------------------------------------------------- */
/** \name Add Custom Asset Library
 * \{ */

static int custom_asset_library_add_exec(bContext *UNUSED(C), wmOperator *op)
{
  BlenderProject *project = CTX_wm_project();
  if (!project) {
    BKE_report(op->reports,
               RPT_ERROR,
               "Couldn't create project asset library, there is no active project");
    return OPERATOR_CANCELLED;
  }

  char path[FILE_MAXDIR];
  char dirname[FILE_MAXFILE];

  RNA_string_get(op->ptr, "directory", path);

  BLI_path_slash_rstrip(path);
  /* Always keep project paths relative for now. Adds the "//" prefix which usually denotes a path
   * that's relative to the current .blend, for now use it for project relative paths as well. */
  BLI_path_rel(path, BKE_project_root_path_get(project));
  BLI_split_file_part(path, dirname, sizeof(dirname));

  ListBase *asset_libraries = BKE_project_custom_asset_libraries_get(project);
  /* NULL is a valid directory path here. A library without path will be created then. */
  BKE_asset_library_custom_add(asset_libraries, dirname, path);
  BKE_project_tag_has_unsaved_changes(project);

  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIBRARY, NULL);
  WM_main_add_notifier(NC_PROJECT, NULL);

  return OPERATOR_FINISHED;
}

static int custom_asset_library_add_invoke(bContext *C,
                                           wmOperator *op,
                                           const wmEvent *UNUSED(event))
{
  if (!RNA_struct_property_is_set(op->ptr, "directory")) {
    WM_event_add_fileselect(C, op);
    return OPERATOR_RUNNING_MODAL;
  }

  return custom_asset_library_add_exec(C, op);
}

/* Similar to #PREFERENCES_OT_asset_library_add. */
static void PROJECT_OT_custom_asset_library_add(wmOperatorType *ot)
{
  ot->name = "Add Asset Library";
  ot->idname = "PROJECT_OT_custom_asset_library_add";
  ot->description = "Register a directory to be used by the Asset Browser as source of assets";

  ot->exec = custom_asset_library_add_exec;
  ot->invoke = custom_asset_library_add_invoke;

  ot->flag = OPTYPE_INTERNAL;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_DIRECTORY,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Custom Asset Library
 * \{ */

static int custom_asset_library_remove_exec(bContext *UNUSED(C), wmOperator *op)
{
  const int index = RNA_int_get(op->ptr, "index");

  BlenderProject *project = CTX_wm_project();
  if (!project) {
    BKE_report(op->reports,
               RPT_ERROR,
               "Couldn't remove project asset library, there is no active project");
    return OPERATOR_CANCELLED;
  }

  ListBase *asset_libraries = BKE_project_custom_asset_libraries_get(project);
  CustomAssetLibraryDefinition *library = BKE_asset_library_custom_find_from_index(asset_libraries,
                                                                                   index);
  BKE_asset_library_custom_remove(asset_libraries, library);
  BKE_project_tag_has_unsaved_changes(project);

  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIBRARY, NULL);
  WM_main_add_notifier(NC_PROJECT, NULL);

  return OPERATOR_FINISHED;
}

/* Similar to #PREFERENCES_OT_asset_library_remove. */
static void PROJECT_OT_custom_asset_library_remove(wmOperatorType *ot)
{
  ot->name = "Remove Asset Library";
  ot->idname = "PROJECT_OT_custom_asset_library_remove";
  ot->description =
      "Unregister a path to a .blend file, so the Asset Browser will not attempt to show it "
      "anymore";

  ot->exec = custom_asset_library_remove_exec;

  ot->flag = OPTYPE_INTERNAL;

  RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "", 0, 1000);
}

/** \} */

void ED_operatortypes_project()
{
  WM_operatortype_append(PROJECT_OT_custom_asset_library_add);
  WM_operatortype_append(PROJECT_OT_custom_asset_library_remove);
}
