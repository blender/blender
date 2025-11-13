/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Functions for dealing with append/link operators and helpers.
 */

#include <cctype>
#include <cerrno>
#include <cfloat>
#include <cstring>

#include "CLG_log.h"

#include "DNA_ID.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_fileops.h"
#include "BLI_map.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "BLO_readfile.hh"

#include "BKE_armature.hh"
#include "BKE_blendfile.hh"
#include "BKE_blendfile_link_append.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"
#include "BKE_rigidbody.h"
#include "BKE_scene.hh"

#include "BKE_idtype.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "IMB_colormanagement.hh"

#include "ED_datafiles.h"
#include "ED_screen.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "wm_files.hh"

static CLG_LogRef LOG = {"blend.link"};

/* -------------------------------------------------------------------- */
/** \name Link/Append Operator
 * \{ */

static bool wm_link_append_poll(bContext *C)
{
  if (WM_operator_winactive(C)) {
    /* NOTE(@sergey): Linking changes active object which is pretty useful in general,
     * but which totally confuses edit mode (i.e. it becoming not so obvious
     * to leave from edit mode and invalid tools in toolbar might be displayed)
     * so disable link/append when in edit mode. */
    if (CTX_data_edit_object(C)) {
      return false;
    }

    return true;
  }

  return false;
}

static wmOperatorStatus wm_link_append_invoke(bContext *C,
                                              wmOperator *op,
                                              const wmEvent * /*event*/)
{
  if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
    const char *blendfile_path = BKE_main_blendfile_path_from_global();
    if (G.filepath_last_library[0] != '\0') {
      RNA_string_set(op->ptr, "filepath", G.filepath_last_library);
    }
    else if (blendfile_path[0] != '\0') {
      char dirpath[FILE_MAX];
      BLI_path_split_dir_part(blendfile_path, dirpath, sizeof(dirpath));
      RNA_string_set(op->ptr, "filepath", dirpath);
    }
  }

  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static int wm_link_append_flag(wmOperator *op)
{
  /* NOTE: most options exposed here should also be available in the Python API
   * unless there are good reasons to have them only exposed to the operator.
   * see `bpy_lib_load`. */
  PropertyRNA *prop;
  int flag = 0;

  if (RNA_boolean_get(op->ptr, "autoselect")) {
    flag |= FILE_AUTOSELECT;
  }
  if (RNA_boolean_get(op->ptr, "active_collection")) {
    flag |= FILE_ACTIVE_COLLECTION;
  }
  if ((prop = RNA_struct_find_property(op->ptr, "relative_path")) &&
      RNA_property_boolean_get(op->ptr, prop))
  {
    flag |= FILE_RELPATH;
  }
  if (RNA_boolean_get(op->ptr, "link")) {
    flag |= FILE_LINK;
  }
  else {
    if (RNA_boolean_get(op->ptr, "use_recursive")) {
      flag |= BLO_LIBLINK_APPEND_RECURSIVE;
    }
    if (RNA_boolean_get(op->ptr, "set_fake")) {
      flag |= BLO_LIBLINK_APPEND_SET_FAKEUSER;
    }
    if (RNA_boolean_get(op->ptr, "do_reuse_local_id")) {
      flag |= BLO_LIBLINK_APPEND_LOCAL_ID_REUSE;
    }
    if (RNA_boolean_get(op->ptr, "clear_asset_data")) {
      flag |= BLO_LIBLINK_APPEND_ASSET_DATA_CLEAR;
    }
  }
  if (RNA_boolean_get(op->ptr, "instance_collections")) {
    flag |= BLO_LIBLINK_COLLECTION_INSTANCE;
  }
  if (RNA_boolean_get(op->ptr, "instance_object_data")) {
    flag |= BLO_LIBLINK_OBDATA_INSTANCE;
  }

  return flag;
}

/**
 * Check if an item defined by \a name and \a group can be appended/linked.
 *
 * \param reports: Optionally report an error when an item can't be appended/linked.
 */
static bool wm_link_append_item_poll(ReportList *reports,
                                     const char *filepath,
                                     const char *group,
                                     const char *name,
                                     const bool do_append)
{
  short idcode;

  if (!group || !name) {
    CLOG_WARN(&LOG, "Skipping %s", filepath);
    return false;
  }

  idcode = BKE_idtype_idcode_from_name(group);

  if (!BKE_idtype_idcode_is_linkable(idcode) ||
      (!do_append && BKE_idtype_idcode_is_only_appendable(idcode)))
  {
    if (reports) {
      if (do_append) {
        BKE_reportf(reports,
                    RPT_ERROR_INVALID_INPUT,
                    "Cannot append data-block '%s' of type '%s'",
                    name,
                    group);
      }
      else {
        BKE_reportf(reports,
                    RPT_ERROR_INVALID_INPUT,
                    "Cannot link data-block '%s' of type '%s'",
                    name,
                    group);
      }
    }
    return false;
  }

  return true;
}

static wmOperatorStatus wm_link_append_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const char *blendfile_path = BKE_main_blendfile_path(bmain);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  PropertyRNA *prop;
  BlendfileLinkAppendContext *lapp_context;
  char filepath[FILE_MAX_LIBEXTRA], root[FILE_MAXDIR], libname[FILE_MAX_LIBEXTRA],
      relname[FILE_MAX];
  char *group, *name;
  int totfiles = 0;

  RNA_string_get(op->ptr, "filename", relname);
  RNA_string_get(op->ptr, "directory", root);
  if (BLI_path_is_rel(root)) {
    BLI_path_abs(root, blendfile_path);
  }

  BLI_path_join(filepath, sizeof(filepath), root, relname);

  /* Test if we have a valid data. */
  const bool is_librarypath_valid = BKE_blendfile_library_path_explode(
      filepath, libname, &group, &name);

  {
    if (blendfile_path[0] != '\0') {
      /* NOTE: Need to also check `filepath`, as typically `libname` is an empty string here
       * (when trying to append from current file from the file-browser e.g.). */
      if (BLI_path_cmp(blendfile_path, filepath) == 0 ||
          BLI_path_cmp(blendfile_path, libname) == 0)
      {
        BKE_reportf(op->reports, RPT_ERROR, "'%s': cannot use current file as library", filepath);
        return OPERATOR_CANCELLED;
      }
    }
  }

  if (!group) {
    BKE_reportf(op->reports, RPT_ERROR, "'%s': nothing indicated", filepath);
    return OPERATOR_CANCELLED;
  }
  if (!is_librarypath_valid) {
    BKE_reportf(op->reports, RPT_ERROR, "'%s': not a library", filepath);
    return OPERATOR_CANCELLED;
  }

  /* Check if something is indicated for append/link. */
  prop = RNA_struct_find_property(op->ptr, "files");
  if (prop) {
    totfiles = RNA_property_collection_length(op->ptr, prop);
    if (totfiles == 0) {
      if (!name) {
        BKE_reportf(op->reports, RPT_ERROR, "'%s': nothing indicated", filepath);
        return OPERATOR_CANCELLED;
      }
    }
  }
  else if (!name) {
    BKE_reportf(op->reports, RPT_ERROR, "'%s': nothing indicated", filepath);
    return OPERATOR_CANCELLED;
  }

  int flag = wm_link_append_flag(op);
  const bool do_append = (flag & FILE_LINK) == 0;

  /* From here down, no error returns. */

  if (view_layer && RNA_boolean_get(op->ptr, "autoselect")) {
    BKE_view_layer_base_deselect_all(scene, view_layer);
  }

  /* Sanity checks for flag. */
  if (scene && scene->id.lib) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                "Scene '%s' is linked, instantiation of objects is disabled",
                scene->id.name + 2);
    flag &= ~(BLO_LIBLINK_COLLECTION_INSTANCE | BLO_LIBLINK_OBDATA_INSTANCE);
    scene = nullptr;
  }

  /* Tag everything, all untagged data can be made local
   * its also generally useful to know what is new.
   *
   * Take extra care `BKE_main_id_flag_all(bmain, ID_TAG_PRE_EXISTING, false)` is called after! */
  BKE_main_id_tag_all(bmain, ID_TAG_PRE_EXISTING, true);

  /* We define our working data...
   * Note that here, each item 'uses' one library, and only one. */
  LibraryLink_Params lapp_params;
  BLO_library_link_params_init_with_context(
      &lapp_params, bmain, flag, 0, scene, view_layer, CTX_wm_view3d(C));

  lapp_context = BKE_blendfile_link_append_context_new(&lapp_params);
  BKE_blendfile_link_append_context_embedded_blendfile_set(
      lapp_context, datatoc_startup_blend, datatoc_startup_blend_size);

  if (totfiles != 0) {
    blender::Map<std::string, int> libraries;
    int lib_idx = 0;

    RNA_BEGIN (op->ptr, itemptr, "files") {
      RNA_string_get(&itemptr, "name", relname);

      BLI_path_join(filepath, sizeof(filepath), root, relname);

      if (BKE_blendfile_library_path_explode(filepath, libname, &group, &name)) {
        if (!wm_link_append_item_poll(nullptr, filepath, group, name, do_append)) {
          continue;
        }

        if (libraries.add(libname, lib_idx)) {
          lib_idx++;
          BKE_blendfile_link_append_context_library_add(lapp_context, libname, nullptr);
        }
      }
    }
    RNA_END;

    RNA_BEGIN (op->ptr, itemptr, "files") {
      RNA_string_get(&itemptr, "name", relname);

      BLI_path_join(filepath, sizeof(filepath), root, relname);

      if (BKE_blendfile_library_path_explode(filepath, libname, &group, &name)) {
        BlendfileLinkAppendContextItem *item;

        if (!wm_link_append_item_poll(op->reports, filepath, group, name, do_append)) {
          continue;
        }

        lib_idx = libraries.lookup(libname);

        item = BKE_blendfile_link_append_context_item_add(
            lapp_context, name, BKE_idtype_idcode_from_name(group), nullptr);
        BKE_blendfile_link_append_context_item_library_index_enable(lapp_context, item, lib_idx);
      }
    }
    RNA_END;
  }
  else {
    BlendfileLinkAppendContextItem *item;

    BKE_blendfile_link_append_context_library_add(lapp_context, libname, nullptr);
    item = BKE_blendfile_link_append_context_item_add(
        lapp_context, name, BKE_idtype_idcode_from_name(group), nullptr);
    BKE_blendfile_link_append_context_item_library_index_enable(lapp_context, item, 0);
  }

  if (BKE_blendfile_link_append_context_is_empty(lapp_context)) {
    /* Early out in case there is nothing to link. */
    BKE_blendfile_link_append_context_free(lapp_context);
    /* Clear pre existing tag. */
    BKE_main_id_tag_all(bmain, ID_TAG_PRE_EXISTING, false);
    return OPERATOR_CANCELLED;
  }

  BKE_blendfile_link_append_context_init_done(lapp_context);

  /* XXX We'd need re-entrant locking on Main for this to work... */
  // BKE_main_lock(bmain);

  BKE_blendfile_link(lapp_context, op->reports);

  // BKE_main_unlock(bmain);

  /* Mark all library linked objects to be updated. */
  BKE_main_lib_objects_recalc_all(bmain);
  IMB_colormanagement_check_file_config(bmain);

  /* Append, rather than linking. */
  if (do_append) {
    BKE_blendfile_append(lapp_context, op->reports);
  }

  /* Instantiate loose data in the scene (e.g. add object to the active collection). */
  BKE_blendfile_link_append_instantiate_loose(lapp_context, op->reports);

  BKE_blendfile_link_append_context_finalize(lapp_context);

  BKE_blendfile_link_append_context_free(lapp_context);

  /* Important we unset, otherwise these object won't
   * link into other scenes from this blend file. */
  BKE_main_id_tag_all(bmain, ID_TAG_PRE_EXISTING, false);

  /* TODO(sergey): Use proper flag for tagging here. */

  /* TODO(dalai): Temporary solution!
   * Ideally we only need to tag the new objects themselves, not the scene.
   * This way we'll avoid flush of collection properties
   * to all objects and limit update to the particular object only.
   * But afraid first we need to change collection evaluation in DEG
   * according to depsgraph manifesto. */
  if (scene) {
    DEG_id_tag_update(&scene->id, 0);
  }

  /* Recreate dependency graph to include new objects. */
  DEG_relations_tag_update(bmain);

  /* TODO: align `G.filepath_last_library` with other directory storage
   * (like last opened image, etc). */
  STRNCPY(G.filepath_last_library, root);

  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  return OPERATOR_FINISHED;
}

static void wm_link_append_properties_common(wmOperatorType *ot,
                                             const bool is_link,
                                             const bool is_relocate)
{
  PropertyRNA *prop;

  /* Better not save _any_ settings for this operator. */

  /* Properties. */
  prop = RNA_def_boolean(ot->srna,
                         "link",
                         is_link || is_relocate,
                         "Link",
                         "Link the objects or data-blocks rather than appending");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);

  prop = RNA_def_boolean(
      ot->srna,
      "do_reuse_local_id",
      false,
      "Re-Use Local Data",
      "Try to re-use previously matching appended data-blocks instead of appending a new copy");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
  prop = RNA_def_boolean(ot->srna,
                         "clear_asset_data",
                         false,
                         "Clear Asset Data",
                         "Don't add asset meta-data or tags from the original data-block");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);

  prop = RNA_def_boolean(ot->srna, "autoselect", true, "Select", "Select new objects");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "active_collection",
                         !is_relocate,
                         "Active Collection",
                         "Put new objects on the active collection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  /* NOTE: do not force instancing when relocating, as direct data (the selected ID) status should
   * not change on that regard, and other dependencies would be indirectly linked and therefore
   * should not require any enforced instancing when linked. */
  prop = RNA_def_boolean(
      ot->srna,
      "instance_collections",
      is_link && !is_relocate,
      "Instance Collections",
      "Create instances for collections, rather than adding them directly to the scene");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna,
      "instance_object_data",
      !is_relocate,
      "Instance Object Data",
      "Create instances for object data which are not referenced by any objects");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

void WM_OT_link(wmOperatorType *ot)
{
  ot->name = "Link";
  ot->idname = "WM_OT_link";
  ot->description = "Link from a Library .blend file";

  ot->invoke = wm_link_append_invoke;
  ot->exec = wm_link_append_exec;
  ot->poll = wm_link_append_poll;

  ot->flag = OPTYPE_UNDO;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_BLENDER | FILE_TYPE_BLENDERLIB,
                                 FILE_LOADLIB,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_DIRECTORY | WM_FILESEL_FILENAME |
                                     WM_FILESEL_RELPATH | WM_FILESEL_FILES | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  wm_link_append_properties_common(ot, true, false);
}

void WM_OT_append(wmOperatorType *ot)
{
  ot->name = "Append";
  ot->idname = "WM_OT_append";
  ot->description = "Append from a Library .blend file";

  ot->invoke = wm_link_append_invoke;
  ot->exec = wm_link_append_exec;
  ot->poll = wm_link_append_poll;

  ot->flag = OPTYPE_UNDO;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_BLENDER | FILE_TYPE_BLENDERLIB,
                                 FILE_LOADLIB,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_DIRECTORY | WM_FILESEL_FILENAME |
                                     WM_FILESEL_FILES | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  wm_link_append_properties_common(ot, false, false);
  RNA_def_boolean(ot->srna,
                  "set_fake",
                  false,
                  "Fake User",
                  "Set \"Fake User\" for appended items (except objects and collections)");
  RNA_def_boolean(
      ot->srna,
      "use_recursive",
      true,
      "Localize All",
      "Localize all appended data, including those indirectly linked from other libraries");
}

static wmOperatorStatus wm_id_linked_relocate_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const char *blendfile_path = BKE_main_blendfile_path(bmain);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BlendfileLinkAppendContext *lapp_context;
  char filepath[FILE_MAX_LIBEXTRA], root[FILE_MAXDIR], libname[FILE_MAX_LIBEXTRA],
      relname[FILE_MAX];
  char *group, *name;

  RNA_string_get(op->ptr, "filename", relname);
  RNA_string_get(op->ptr, "directory", root);
  if (BLI_path_is_rel(root)) {
    BLI_path_abs(root, blendfile_path);
  }

  BLI_path_join(filepath, sizeof(filepath), root, relname);

  /* Test if we have a valid data. */
  const bool is_librarypath_valid = BKE_blendfile_library_path_explode(
      filepath, libname, &group, &name);

  {
    if (blendfile_path[0] != '\0') {
      /* NOTE: Need to also check `filepath`, as typically `libname` is an empty string here
       * (when trying to append from current file from the file-browser e.g.). */
      if (BLI_path_cmp(blendfile_path, filepath) == 0 ||
          BLI_path_cmp(blendfile_path, libname) == 0)
      {
        BKE_reportf(op->reports, RPT_ERROR, "'%s': cannot use current file as library", filepath);
        return OPERATOR_CANCELLED;
      }
    }
  }

  if (!is_librarypath_valid) {
    BKE_reportf(op->reports, RPT_ERROR, "'%s': not a library", filepath);
    return OPERATOR_CANCELLED;
  }
  if (!group || !name) {
    BKE_reportf(op->reports, RPT_ERROR, "'%s': nothing indicated", filepath);
    return OPERATOR_CANCELLED;
  }

  int flag = wm_link_append_flag(op);
  BLI_assert(flag & FILE_LINK);

  const short id_type_code = BKE_idtype_idcode_from_name(group);

  const int tmp_id_session_uid = RNA_int_get(op->ptr, "id_session_uid");
  const uint id_session_uid = *reinterpret_cast<const uint *>(&tmp_id_session_uid);
  /* NOTE: Creating a full ID map for a single lookup is not worth it. */
  ID *linked_id = BKE_libblock_find_session_uid(bmain, id_session_uid);

  {
    const char *linked_id_name = BKE_id_name(*linked_id);

    if (!linked_id || !ID_IS_LINKED(linked_id)) {
      BKE_reportf(op->reports, RPT_ERROR, "No valid existing linked ID given to relocate");
      return OPERATOR_CANCELLED;
    }
    if (GS(linked_id->name) != id_type_code) {
      BKE_reportf(op->reports,
                  RPT_ERROR,
                  "Selected ID '%s' is a %s, cannot be used to relocate existing linked ID '%s' "
                  "which is a %s",
                  name,
                  group,
                  linked_id_name,
                  BKE_idtype_idcode_to_name(GS(linked_id->name)));
      return OPERATOR_CANCELLED;
    }
    if (STREQ(linked_id_name, name) && STREQ(linked_id->lib->runtime->filepath_abs, libname)) {
      BKE_reportf(op->reports,
                  RPT_ERROR,
                  "Selected ID '%s' seems to be the same as the relocated ID '%s', use 'Reload' "
                  "operation instead",
                  name,
                  linked_id_name);
      return OPERATOR_CANCELLED;
    }
  }

  /* From here down, no error returns. */

  if (view_layer && RNA_boolean_get(op->ptr, "autoselect")) {
    BKE_view_layer_base_deselect_all(scene, view_layer);
  }

  /* Never enforce instantiation of anything when relocating. */
  flag &= ~(BLO_LIBLINK_COLLECTION_INSTANCE | BLO_LIBLINK_OBDATA_INSTANCE);

  /* Tag everything, its generally useful to know what is new.
   *
   * Take extra care `BKE_main_id_flag_all(bmain, ID_TAG_PRE_EXISTING, false)` is called after! */
  BKE_main_id_tag_all(bmain, ID_TAG_PRE_EXISTING, true);

  /* We define our working data...
   * Note that here, each item 'uses' one library, and only one. */
  LibraryLink_Params lapp_params;
  BLO_library_link_params_init_with_context(
      &lapp_params, bmain, flag, 0, scene, view_layer, CTX_wm_view3d(C));

  lapp_context = BKE_blendfile_link_append_context_new(&lapp_params);
  BKE_blendfile_link_append_context_embedded_blendfile_set(
      lapp_context, datatoc_startup_blend, datatoc_startup_blend_size);

  BKE_blendfile_link_append_context_library_add(lapp_context, libname, nullptr);
  BlendfileLinkAppendContextItem *item = BKE_blendfile_link_append_context_item_add(
      lapp_context, name, id_type_code, linked_id);
  BKE_blendfile_link_append_context_item_library_index_enable(lapp_context, item, 0);

  BKE_blendfile_link_append_context_init_done(lapp_context);

  BKE_blendfile_id_relocate(*lapp_context, op->reports);

  BKE_blendfile_link_append_context_finalize(lapp_context);

  BKE_blendfile_link_append_context_free(lapp_context);

  /* TODO(sergey): Use proper flag for tagging here. */

  /* TODO(dalai): Temporary solution!
   * Ideally we only need to tag the new objects themselves, not the scene.
   * This way we'll avoid flush of collection properties
   * to all objects and limit update to the particular object only.
   * But afraid first we need to change collection evaluation in DEG
   * according to depsgraph manifesto. */
  if (scene) {
    DEG_id_tag_update(&scene->id, 0);
  }

  /* Recreate dependency graph to include new objects. */
  DEG_relations_tag_update(bmain);

  /* TODO: align `G.filepath_last_library` with other directory storage
   * (like last opened image, etc). */
  STRNCPY(G.filepath_last_library, root);

  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  return OPERATOR_FINISHED;
}

void WM_OT_id_linked_relocate(wmOperatorType *ot)
{
  ot->name = "Relocate Linked ID";
  ot->idname = "WM_OT_id_linked_relocate";
  ot->description =
      "Relocate a linked ID, i.e. select another ID to link, and remap its local usages to that "
      "newly linked data-block). Currently only designed as an internal operator, not directly "
      "exposed to the user";

  ot->invoke = wm_link_append_invoke;
  ot->exec = wm_id_linked_relocate_exec;
  ot->poll = wm_link_append_poll;

  ot->flag = OPTYPE_INTERNAL | OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop = RNA_def_int(ot->srna,
                                  "id_session_uid",
                                  MAIN_ID_SESSION_UID_UNSET,
                                  0,
                                  INT_MAX,
                                  "Linked ID Session UID",
                                  "Unique runtime identifier for the linked ID to relocate",
                                  0,
                                  INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_BLENDER | FILE_TYPE_BLENDERLIB,
                                 FILE_LOADLIB,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_DIRECTORY | WM_FILESEL_FILENAME |
                                     WM_FILESEL_RELPATH | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  wm_link_append_properties_common(ot, true, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Link/Append Single Data-Block & Return it
 *
 * \{ */

static ID *wm_file_link_append_datablock_ex(Main *bmain,
                                            Scene *scene,
                                            ViewLayer *view_layer,
                                            View3D *v3d,
                                            const char *filepath,
                                            const short id_code,
                                            const char *id_name,
                                            const int flag)
{
  BLI_assert_msg(
      BLI_path_cmp(BKE_main_blendfile_path(bmain), filepath) != 0,
      "Calling code should ensure it does not attempt to link/append from current blendfile");

  const bool do_pack = (flag & BLO_LIBLINK_PACK) != 0;
  const bool do_append = (flag & FILE_LINK) == 0;
  /* Tag everything so we can make local only the new datablock. */
  BKE_main_id_tag_all(bmain, ID_TAG_PRE_EXISTING, true);

  /* Define working data, with just the one item we want to link. */
  LibraryLink_Params lapp_params;
  BLO_library_link_params_init_with_context(&lapp_params, bmain, flag, 0, scene, view_layer, v3d);

  BlendfileLinkAppendContext *lapp_context = BKE_blendfile_link_append_context_new(&lapp_params);
  BKE_blendfile_link_append_context_embedded_blendfile_set(
      lapp_context, datatoc_startup_blend, datatoc_startup_blend_size);

  BKE_blendfile_link_append_context_library_add(lapp_context, filepath, nullptr);
  BlendfileLinkAppendContextItem *item = BKE_blendfile_link_append_context_item_add(
      lapp_context, id_name, id_code, nullptr);
  BKE_blendfile_link_append_context_item_library_index_enable(lapp_context, item, 0);

  BKE_blendfile_link_append_context_init_done(lapp_context);

  /* Link datablock. */
  BKE_blendfile_link(lapp_context, nullptr);

  if (do_pack) {
    BKE_blendfile_link_pack(lapp_context, nullptr);
  }
  else if (do_append) {
    BKE_blendfile_append(lapp_context, nullptr);
  }

  BKE_blendfile_link_append_instantiate_loose(lapp_context, nullptr);

  BKE_blendfile_link_append_context_finalize(lapp_context);

  /* Get linked datablock and free working data. */
  ID *id = BKE_blendfile_link_append_context_item_newid_get(lapp_context, item);

  BKE_blendfile_link_append_context_free(lapp_context);

  BKE_main_id_tag_all(bmain, ID_TAG_PRE_EXISTING, false);

  return id;
}

ID *WM_file_link_datablock(Main *bmain,
                           Scene *scene,
                           ViewLayer *view_layer,
                           View3D *v3d,
                           const char *filepath,
                           const short id_code,
                           const char *id_name,
                           int flag)
{
  flag |= FILE_LINK;
  return wm_file_link_append_datablock_ex(
      bmain, scene, view_layer, v3d, filepath, id_code, id_name, flag);
}

ID *WM_file_append_datablock(Main *bmain,
                             Scene *scene,
                             ViewLayer *view_layer,
                             View3D *v3d,
                             const char *filepath,
                             const short id_code,
                             const char *id_name,
                             int flag)
{
  BLI_assert((flag & FILE_LINK) == 0);
  return wm_file_link_append_datablock_ex(
      bmain, scene, view_layer, v3d, filepath, id_code, id_name, flag);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Library Relocate Operator & Library Reload API
 * \{ */

static wmOperatorStatus wm_lib_relocate_invoke(bContext *C,
                                               wmOperator *op,
                                               const wmEvent * /*event*/)
{
  Library *lib;
  char lib_name[MAX_NAME];

  RNA_string_get(op->ptr, "library", lib_name);
  lib = (Library *)BKE_libblock_find_name(CTX_data_main(C), ID_LI, lib_name);

  if (lib) {
    if (lib->runtime->parent) {
      BKE_reportf(op->reports,
                  RPT_ERROR_INVALID_INPUT,
                  "Cannot relocate indirectly linked library '%s'",
                  lib->runtime->filepath_abs);
      return OPERATOR_CANCELLED;
    }
    RNA_string_set(op->ptr, "filepath", lib->runtime->filepath_abs);

    WM_event_add_fileselect(C, op);

    return OPERATOR_RUNNING_MODAL;
  }

  return OPERATOR_CANCELLED;
}

void WM_lib_reload(Library *lib, bContext *C, ReportList *reports)
{
  if (!BKE_blendfile_extension_check(lib->runtime->filepath_abs)) {
    BKE_reportf(
        reports, RPT_ERROR, "'%s' is not a valid library filepath", lib->runtime->filepath_abs);
    return;
  }

  if (!BLI_exists(lib->runtime->filepath_abs)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Trying to reload library '%s' from invalid path '%s'",
                lib->id.name,
                lib->runtime->filepath_abs);
    return;
  }

  Main *bmain = CTX_data_main(C);
  LibraryLink_Params lapp_params;
  BLO_library_link_params_init_with_context(
      &lapp_params,
      bmain,
      (BLO_LIBLINK_USE_PLACEHOLDERS | BLO_LIBLINK_FORCE_INDIRECT | FILE_LINK),
      0,
      CTX_data_scene(C),
      CTX_data_view_layer(C),
      nullptr);

  BlendfileLinkAppendContext *lapp_context = BKE_blendfile_link_append_context_new(&lapp_params);

  BKE_blendfile_link_append_context_library_add(lapp_context, lib->runtime->filepath_abs, nullptr);

  BKE_blendfile_library_relocate(lapp_context, reports, lib, true);

  BKE_blendfile_link_append_context_finalize(lapp_context);

  BKE_blendfile_link_append_context_free(lapp_context);

  BKE_main_lib_objects_recalc_all(bmain);
  IMB_colormanagement_check_file_config(bmain);

  /* Important we unset, otherwise these object won't link into other scenes from this blend file.
   */
  BKE_main_id_tag_all(bmain, ID_TAG_PRE_EXISTING, false);

  /* Recreate dependency graph to include new IDs. */
  DEG_relations_tag_update(bmain);

  WM_event_add_notifier(C, NC_WINDOW, nullptr);
}

static wmOperatorStatus wm_lib_relocate_exec_do(bContext *C, wmOperator *op, bool do_reload)
{
  Main *bmain = CTX_data_main(C);
  const char *blendfile_path = BKE_main_blendfile_path(bmain);
  char lib_name[MAX_NAME];

  RNA_string_get(op->ptr, "library", lib_name);
  Library *lib = (Library *)BKE_libblock_find_name(bmain, ID_LI, lib_name);
  if (lib == nullptr) {
    return OPERATOR_CANCELLED;
  }

  PropertyRNA *prop;
  BlendfileLinkAppendContext *lapp_context;

  char filepath[FILE_MAX], root[FILE_MAXDIR], libname[FILE_MAX], relname[FILE_MAX];

  if (lib->runtime->parent && !do_reload) {
    BKE_reportf(op->reports,
                RPT_ERROR_INVALID_INPUT,
                "Cannot relocate indirectly linked library '%s'",
                lib->runtime->filepath_abs);
    return OPERATOR_CANCELLED;
  }

  RNA_string_get(op->ptr, "filename", libname);
  RNA_string_get(op->ptr, "directory", root);
  if (BLI_path_is_rel(root)) {
    BLI_path_abs(root, blendfile_path);
  }

  if (!BKE_blendfile_extension_check(libname)) {
    BKE_report(op->reports, RPT_ERROR, "Not a library");
    return OPERATOR_CANCELLED;
  }

  BLI_path_join(filepath, sizeof(filepath), root, libname);

  if (!BLI_exists(filepath)) {
    BKE_reportf(op->reports,
                RPT_ERROR_INVALID_INPUT,
                "Trying to reload or relocate library '%s' to invalid path '%s'",
                lib->id.name,
                filepath);
    return OPERATOR_CANCELLED;
  }

  {
    if ((blendfile_path[0] != '\0') && (BLI_path_cmp(blendfile_path, filepath) == 0)) {
      BKE_reportf(op->reports,
                  RPT_ERROR_INVALID_INPUT,
                  "Cannot relocate library '%s' to current blend file '%s'",
                  lib->id.name,
                  filepath);
      return OPERATOR_CANCELLED;
    }
  }

  int flag = FILE_LINK;
  if (RNA_boolean_get(op->ptr, "relative_path")) {
    flag |= FILE_RELPATH;
  }
  flag |= BLO_LIBLINK_FORCE_INDIRECT;
  if (do_reload) {
    flag |= BLO_LIBLINK_USE_PLACEHOLDERS;
  }

  LibraryLink_Params lapp_params;
  BLO_library_link_params_init_with_context(
      &lapp_params, bmain, flag, 0, CTX_data_scene(C), CTX_data_view_layer(C), nullptr);

  if (BLI_path_cmp(lib->runtime->filepath_abs, filepath) == 0) {
    CLOG_DEBUG(&LOG, "We are supposed to reload '%s' lib (%d)", lib->filepath, lib->id.us);

    do_reload = true;

    lapp_context = BKE_blendfile_link_append_context_new(&lapp_params);
    BKE_blendfile_link_append_context_library_add(lapp_context, filepath, nullptr);
  }
  else {
    int totfiles = 0;

    CLOG_DEBUG(
        &LOG, "We are supposed to relocate '%s' lib to new '%s' one", lib->filepath, libname);

    /* Check if something is indicated for relocate. */
    prop = RNA_struct_find_property(op->ptr, "files");
    if (prop) {
      totfiles = RNA_property_collection_length(op->ptr, prop);
      if (totfiles == 0) {
        if (!libname[0]) {
          BKE_report(op->reports, RPT_ERROR, "Nothing indicated");
          return OPERATOR_CANCELLED;
        }
      }
    }

    lapp_context = BKE_blendfile_link_append_context_new(&lapp_params);

    if (totfiles) {
      RNA_BEGIN (op->ptr, itemptr, "files") {
        RNA_string_get(&itemptr, "name", relname);

        BLI_path_join(filepath, sizeof(filepath), root, relname);

        if (BLI_path_cmp(filepath, lib->runtime->filepath_abs) == 0 ||
            !BKE_blendfile_extension_check(relname))
        {
          continue;
        }

        CLOG_DEBUG(&LOG, "\tCandidate new lib to reload data-blocks from: %s", filepath);
        BKE_blendfile_link_append_context_library_add(lapp_context, filepath, nullptr);
      }
      RNA_END;
    }
    else {
      CLOG_DEBUG(&LOG, "\tCandidate new lib to reload data-blocks from: %s", filepath);
      BKE_blendfile_link_append_context_library_add(lapp_context, filepath, nullptr);
    }
  }

  BKE_blendfile_library_relocate(lapp_context, op->reports, lib, do_reload);

  BKE_blendfile_link_append_context_finalize(lapp_context);

  BKE_blendfile_link_append_context_free(lapp_context);

  /* TODO: align `G.filepath_last_library` with other directory storage
   * (like last opened image, etc). */
  STRNCPY(G.filepath_last_library, root);

  BKE_main_lib_objects_recalc_all(bmain);
  IMB_colormanagement_check_file_config(bmain);

  /* Important we unset, otherwise these object won't link into other scenes from this blend
   * file.
   */
  BKE_main_id_tag_all(bmain, ID_TAG_PRE_EXISTING, false);

  /* Recreate dependency graph to include new IDs. */
  DEG_relations_tag_update(bmain);

  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus wm_lib_relocate_exec(bContext *C, wmOperator *op)
{
  return wm_lib_relocate_exec_do(C, op, false);
}

void WM_OT_lib_relocate(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Relocate Library";
  ot->idname = "WM_OT_lib_relocate";
  ot->description = "Relocate the given library to one or several others";

  ot->invoke = wm_lib_relocate_invoke;
  ot->exec = wm_lib_relocate_exec;

  ot->flag = OPTYPE_UNDO;

  prop = RNA_def_string(ot->srna, "library", nullptr, MAX_NAME, "Library", "Library to relocate");
  RNA_def_property_flag(prop, PROP_HIDDEN);

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_BLENDER,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_DIRECTORY | WM_FILESEL_FILENAME |
                                     WM_FILESEL_FILES | WM_FILESEL_RELPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

static wmOperatorStatus wm_lib_reload_exec(bContext *C, wmOperator *op)
{
  return wm_lib_relocate_exec_do(C, op, true);
}

void WM_OT_lib_reload(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Reload Library";
  ot->idname = "WM_OT_lib_reload";
  ot->description = "Reload the given library";

  ot->exec = wm_lib_reload_exec;

  ot->flag = OPTYPE_UNDO;

  prop = RNA_def_string(ot->srna, "library", nullptr, MAX_NAME, "Library", "Library to reload");
  RNA_def_property_flag(prop, PROP_HIDDEN);

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_BLENDER,
                                 FILE_BLENDER,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_DIRECTORY | WM_FILESEL_FILENAME |
                                     WM_FILESEL_RELPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

/** \} */
