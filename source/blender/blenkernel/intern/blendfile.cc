/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * High level `.blend` file read/write,
 * and functions for writing *partial* files (only selected data-blocks).
 */

#include <cstdlib>
#include <cstring>
#include <optional>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_fileops.h"
#include "BLI_function_ref.hh"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_system.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "BLT_translation.hh"

#include "IMB_colormanagement.hh"

#include "BKE_addon.h"
#include "BKE_appdir.hh"
#include "BKE_blender.hh"
#include "BKE_blender_version.h"
#include "BKE_blendfile.hh"
#include "BKE_bpath.hh"
#include "BKE_colorband.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_idmap.hh"
#include "BKE_main_namemap.hh"
#include "BKE_preferences.h"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BKE_studiolight.h"
#include "BKE_undo_system.hh"
#include "BKE_workspace.hh"

#include "BLO_read_write.hh"
#include "BLO_readfile.hh"
#include "BLO_userdef_default.h"
#include "BLO_writefile.hh"

#include "RE_pipeline.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.hh"
#endif

using namespace blender::bke;

/* -------------------------------------------------------------------- */
/** \name Blend/Library Paths
 * \{ */

bool BKE_blendfile_extension_check(const char *str)
{
  const char *ext_test[4] = {".blend", ".ble", ".blend.gz", nullptr};
  return BLI_path_extension_check_array(str, ext_test);
}

bool BKE_blendfile_library_path_explode(const char *path,
                                        char *r_dir,
                                        char **r_group,
                                        char **r_name)
{
  /* We might get some data names with slashes,
   * so we have to go up in path until we find blend file itself,
   * then we know next path item is group, and everything else is data name. */
  char *slash = nullptr, *prev_slash = nullptr, c = '\0';

  r_dir[0] = '\0';
  if (r_group) {
    *r_group = nullptr;
  }
  if (r_name) {
    *r_name = nullptr;
  }

  /* if path leads to an existing directory, we can be sure we're not (in) a library */
  if (BLI_is_dir(path)) {
    return false;
  }

  BLI_strncpy(r_dir, path, FILE_MAX_LIBEXTRA);

  while ((slash = (char *)BLI_path_slash_rfind(r_dir))) {
    char tc = *slash;
    *slash = '\0';
    if (BKE_blendfile_extension_check(r_dir) && BLI_is_file(r_dir)) {
      break;
    }
    if (STREQ(r_dir, BLO_EMBEDDED_STARTUP_BLEND)) {
      break;
    }

    if (prev_slash) {
      *prev_slash = c;
    }
    prev_slash = slash;
    c = tc;
  }

  if (!slash) {
    return false;
  }

  if (slash[1] != '\0') {
    BLI_assert(strlen(slash + 1) < BLO_GROUP_MAX);
    if (r_group) {
      *r_group = slash + 1;
    }
  }

  if (prev_slash && (prev_slash[1] != '\0')) {
    BLI_assert(strlen(prev_slash + 1) < MAX_ID_NAME - 2);
    if (r_name) {
      *r_name = prev_slash + 1;
    }
  }

  return true;
}

bool BKE_blendfile_is_readable(const char *path, ReportList *reports)
{
  BlendFileReadReport readfile_reports;
  readfile_reports.reports = reports;
  BlendHandle *bh = BLO_blendhandle_from_file(path, &readfile_reports);
  if (bh != nullptr) {
    BLO_blendhandle_close(bh);
    return true;
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blend File IO (High Level)
 * \{ */

static bool foreach_path_clean_cb(BPathForeachPathData * /*bpath_data*/,
                                  char *path_dst,
                                  size_t path_dst_maxncpy,
                                  const char *path_src)
{
  BLI_strncpy(path_dst, path_src, path_dst_maxncpy);
  BLI_path_slash_native(path_dst);
  return !STREQ(path_dst, path_src);
}

/* make sure path names are correct for OS */
static void clean_paths(Main *bmain)
{
  BPathForeachPathData foreach_path_data{};
  foreach_path_data.bmain = bmain;
  foreach_path_data.callback_function = foreach_path_clean_cb;
  foreach_path_data.flag = BKE_BPATH_FOREACH_PATH_SKIP_MULTIFILE;
  foreach_path_data.user_data = nullptr;

  BKE_bpath_foreach_path_main(&foreach_path_data);

  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    BLI_path_slash_native(scene->r.pic);
  }
}

static bool wm_scene_is_visible(wmWindowManager *wm, Scene *scene)
{
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    if (win->scene == scene) {
      return true;
    }
  }
  return false;
}

static void setup_app_userdef(BlendFileData *bfd)
{
  if (bfd->user) {
    /* only here free userdef themes... */
    BKE_blender_userdef_data_set_and_free(bfd->user);
    bfd->user = nullptr;

    /* Security issue: any blend file could include a #BLO_CODE_USER block.
     *
     * Preferences are loaded from #BLENDER_STARTUP_FILE and later on load #BLENDER_USERPREF_FILE,
     * to load the preferences defined in the users home directory.
     *
     * This means we will never accidentally (or maliciously)
     * enable scripts auto-execution by loading a `.blend` file. */
    U.flag |= USER_SCRIPT_AUTOEXEC_DISABLE;
  }
}

/**
 * Helper struct to manage IDs that are re-used across blend-file loading (i.e. moved from the old
 * Main the new one).
 *
 * NOTE: this is only used when actually loading a real `.blend` file,
 * loading of memfile undo steps does not need it.
 */
struct ReuseOldBMainData {
  Main *new_bmain;
  Main *old_bmain;

  /**
   * Data generated and used by calling WM code to handle keeping WM and UI IDs as best as
   * possible across file reading.
   *
   * \note May be null in undo (memfile) case.
   */
  BlendFileReadWMSetupData *wm_setup_data;

  /**
   * Storage for all remapping rules (old_id -> new_id) required by the preservation of old IDs
   * into the new Main.
   */
  id::IDRemapper *remapper;
  bool is_libraries_remapped;

  /**
   * Used to find matching IDs by name/lib in new main, to remap ID usages of data ported over
   * from old main.
   */
  IDNameLib_Map *id_map;
};

/**
 * Search for all libraries in `old_bmain` that are also in `new_bmain` (i.e. different Library
 * IDs having the same absolute filepath), and create a remapping rule for these.
 *
 * NOTE: The case where the `old_bmain` would be a library in the newly read one is not handled
 * here, as it does not create explicit issues. The local data from `old_bmain` is either
 * discarded, or added to the `new_bmain` as local data as well. Worst case, there will be a
 * double of a linked data as a local one, without any known relationships between them. In
 * practice, this latter case is not expected to commonly happen.
 */
static id::IDRemapper &reuse_bmain_data_remapper_ensure(ReuseOldBMainData *reuse_data)
{
  if (reuse_data->is_libraries_remapped) {
    return *reuse_data->remapper;
  }

  if (reuse_data->remapper == nullptr) {
    reuse_data->remapper = MEM_new<id::IDRemapper>(__func__);
  }

  Main *new_bmain = reuse_data->new_bmain;
  Main *old_bmain = reuse_data->old_bmain;
  id::IDRemapper &remapper = *reuse_data->remapper;

  LISTBASE_FOREACH (Library *, old_lib_iter, &old_bmain->libraries) {
    /* In case newly opened `new_bmain` is a library of the `old_bmain`, remap it to null, since a
     * file should never ever have linked data from itself. */
    if (STREQ(old_lib_iter->runtime->filepath_abs, new_bmain->filepath)) {
      remapper.add(&old_lib_iter->id, nullptr);
      continue;
    }

    /* NOTE: Although this is quadratic complexity, it is not expected to be an issue in practice:
     *  - Files using more than a few tens of libraries are extremely rare.
     *  - This code is only executed once for every file reading (not on undos).
     */
    LISTBASE_FOREACH (Library *, new_lib_iter, &new_bmain->libraries) {
      if (!STREQ(old_lib_iter->runtime->filepath_abs, new_lib_iter->runtime->filepath_abs)) {
        continue;
      }

      remapper.add(&old_lib_iter->id, &new_lib_iter->id);
      break;
    }
  }

  reuse_data->is_libraries_remapped = true;
  return *reuse_data->remapper;
}

static bool reuse_bmain_data_remapper_is_id_remapped(id::IDRemapper &remapper, ID *id)
{
  IDRemapperApplyResult result = remapper.get_mapping_result(id, ID_REMAP_APPLY_DEFAULT, nullptr);
  if (ELEM(result, ID_REMAP_RESULT_SOURCE_REMAPPED, ID_REMAP_RESULT_SOURCE_UNASSIGNED)) {
    /* ID is already remapped to its matching ID in the new main, or explicitly remapped to null,
     * nothing else to do here. */
    return true;
  }
  BLI_assert_msg(result != ID_REMAP_RESULT_SOURCE_NOT_MAPPABLE,
                 "There should never be a non-mappable (i.e. null) input here.");
  BLI_assert(result == ID_REMAP_RESULT_SOURCE_UNAVAILABLE);
  return false;
}

static bool reuse_bmain_move_id(ReuseOldBMainData *reuse_data,
                                ID *id,
                                Library *lib,
                                const bool reuse_existing)
{
  id::IDRemapper &remapper = reuse_bmain_data_remapper_ensure(reuse_data);
  /* Nothing to move for embedded ID. */
  if (id->flag & ID_FLAG_EMBEDDED_DATA) {
    remapper.add(id, id);
    return true;
  }

  Main *new_bmain = reuse_data->new_bmain;
  Main *old_bmain = reuse_data->old_bmain;
  ListBase *new_lb = which_libbase(new_bmain, GS(id->name));
  ListBase *old_lb = which_libbase(old_bmain, GS(id->name));

  if (reuse_existing) {
    /* A 'new' version of the same data may already exist in new_bmain, in the rare case
     * that the same asset blend file was linked explicitly into the blend file we are loading.
     * Don't move the old linked ID, but remap its usages to the new one instead. */
    LISTBASE_FOREACH_BACKWARD (ID *, id_iter, new_lb) {
      if (!ELEM(id_iter->lib, id->lib, lib)) {
        continue;
      }
      if (!STREQ(id_iter->name + 2, id->name + 2)) {
        continue;
      }

      remapper.add(id, id_iter);
      return false;
    }
  }

  /* If ID is already in the new_bmain, this should not have been called. */
  BLI_assert(BLI_findindex(new_lb, id) < 0);
  BLI_assert(BLI_findindex(old_lb, id) >= 0);

  /* Move from one list to another, and ensure name is valid. */
  BLI_remlink_safe(old_lb, id);

  /* In case the ID is linked and its library ID is re-used from the old Main, it is not possible
   * to handle name_map (and ensure name uniqueness).
   * This is because IDs are moved one by one from old Main's lists to new ones, while the re-used
   * library's name_map would be built only from IDs in the new list, leading to incomplete/invalid
   * states.
   * Currently, such name uniqueness checks should not be needed, as no new name would be expected
   * in the re-used library. Should this prove to be wrong at some point, the name check will have
   * to happen at the end of #reuse_editable_asset_bmain_data_for_blendfile, in a separate loop
   * over Main IDs.
   */
  const bool handle_name_map_updates = !ID_IS_LINKED(id) || id->lib != lib;
  if (handle_name_map_updates) {
    BKE_main_namemap_remove_id(*old_bmain, *id);
  }

  id->lib = lib;
  BLI_addtail(new_lb, id);
  if (handle_name_map_updates) {
    BKE_id_new_name_validate(
        *new_bmain, *new_lb, *id, nullptr, IDNewNameMode::RenameExistingNever, true);
  }
  else {
    id_sort_by_name(new_lb, id, nullptr);
  }
  BKE_lib_libblock_session_uid_renew(id);

  /* Remap to itself, to avoid re-processing this ID again. */
  remapper.add(id, id);
  return true;
}

static Library *reuse_bmain_data_dependencies_new_library_get(ReuseOldBMainData *reuse_data,
                                                              Library *old_lib)
{
  const id::IDRemapper &remapper = reuse_bmain_data_remapper_ensure(reuse_data);
  Library *new_lib = old_lib;
  IDRemapperApplyResult result = remapper.apply(reinterpret_cast<ID **>(&new_lib),
                                                ID_REMAP_APPLY_DEFAULT);

  switch (result) {
    case ID_REMAP_RESULT_SOURCE_UNAVAILABLE: {
      /* Move library to new bmain.
       * There should be no filepath conflicts, as #reuse_bmain_data_remapper_ensure has
       * already remapped existing libraries with matching filepath. */
      reuse_bmain_move_id(reuse_data, &old_lib->id, nullptr, false);
      /* Clear the name_map of the library, as not all of its IDs are guaranteed reused. The name
       * map cannot be used/kept in valid state while some IDs are moved from old to new main. See
       * also #reuse_bmain_move_id code. */
      BKE_main_namemap_destroy(&old_lib->runtime->name_map);
      return old_lib;
    }
    case ID_REMAP_RESULT_SOURCE_NOT_MAPPABLE: {
      BLI_assert_unreachable();
      return nullptr;
    }
    case ID_REMAP_RESULT_SOURCE_REMAPPED: {
      /* Already in new bmain, only transfer flags. */
      new_lib->runtime->tag |= old_lib->runtime->tag &
                               (LIBRARY_ASSET_EDITABLE | LIBRARY_ASSET_FILE_WRITABLE);
      return new_lib;
    }
    case ID_REMAP_RESULT_SOURCE_UNASSIGNED: {
      /* Happens when the library is the newly opened blend file. */
      return nullptr;
    }
  }

  BLI_assert_unreachable();
  return nullptr;
}

static int reuse_editable_asset_bmain_data_dependencies_process_cb(
    LibraryIDLinkCallbackData *cb_data)
{
  ID *id = *cb_data->id_pointer;

  if (id == nullptr) {
    return IDWALK_RET_NOP;
  }

  if (GS(id->name) == ID_LI) {
    /* Libraries are handled separately. */
    return IDWALK_RET_STOP_RECURSION;
  }

  ReuseOldBMainData *reuse_data = static_cast<ReuseOldBMainData *>(cb_data->user_data);

  /* First check if it has already been remapped. */
  id::IDRemapper &remapper = reuse_bmain_data_remapper_ensure(reuse_data);
  if (reuse_bmain_data_remapper_is_id_remapped(remapper, id)) {
    return IDWALK_RET_STOP_RECURSION;
  }

  if (id->lib == nullptr) {
    /* There should be no links to local datablocks from linked editable data. */
    remapper.add(id, nullptr);
    BLI_assert_unreachable();
    return IDWALK_RET_STOP_RECURSION;
  }

  /* Only preserve specific datablock types. */
  if (!ID_TYPE_SUPPORTS_ASSET_EDITABLE(GS(id->name))) {
    remapper.add(id, nullptr);
    return IDWALK_RET_STOP_RECURSION;
  }

  /* There may be a new library pointer in new_bmain, matching a library in old_bmain, even
   * though pointer values are not the same. So we need to check new linked IDs in new_bmain
   * against both potential library pointers. */
  Library *old_id_new_lib = reuse_bmain_data_dependencies_new_library_get(reuse_data, id->lib);

  /* Happens when the library is the newly opened blend file. */
  if (old_id_new_lib == nullptr) {
    remapper.add(id, nullptr);
    return IDWALK_RET_STOP_RECURSION;
  }

  /* Move to new main database. */
  return reuse_bmain_move_id(reuse_data, id, old_id_new_lib, true) ? IDWALK_RET_STOP_RECURSION :
                                                                     IDWALK_RET_NOP;
}

static bool reuse_editable_asset_needed(ReuseOldBMainData *reuse_data)
{
  Main *old_bmain = reuse_data->old_bmain;
  LISTBASE_FOREACH (Library *, lib, &old_bmain->libraries) {
    if (lib->runtime->tag & LIBRARY_ASSET_EDITABLE) {
      return true;
    }
  }
  return false;
}

/**
 * Selectively 'import' data from old Main into new Main, provided it does not conflict with data
 * already present in the new Main (name-wise and library-wise).
 *
 * Dependencies from moved over old data are also imported into the new Main, (unless, in case of
 * linked data, a matching linked ID is already available in new Main).
 *
 * When a conflict is found, usages of the conflicted ID by the old data are stored in the
 * `remapper` of `ReuseOldBMainData` to be remapped to the matching data in the new Main later.
 *
 * NOTE: This function will never remove any original new data from the new Main, it only moves
 * (some of) the old data to the new Main.
 */
static void reuse_editable_asset_bmain_data_for_blendfile(ReuseOldBMainData *reuse_data,
                                                          const short idcode)
{
  Main *new_bmain = reuse_data->new_bmain;
  Main *old_bmain = reuse_data->old_bmain;

  id::IDRemapper &remapper = reuse_bmain_data_remapper_ensure(reuse_data);

  ListBase *old_lb = which_libbase(old_bmain, idcode);
  ID *old_id_iter;

  FOREACH_MAIN_LISTBASE_ID_BEGIN (old_lb, old_id_iter) {
    /* Keep any datablocks from libraries marked as LIBRARY_ASSET_EDITABLE. */
    if (!(ID_IS_LINKED(old_id_iter) && old_id_iter->lib->runtime->tag & LIBRARY_ASSET_EDITABLE)) {
      continue;
    }

    Library *old_id_new_lib = reuse_bmain_data_dependencies_new_library_get(reuse_data,
                                                                            old_id_iter->lib);

    /* Happens when the library is the newly opened blend file. */
    if (old_id_new_lib == nullptr) {
      remapper.add(old_id_iter, nullptr);
      continue;
    }

    if (reuse_bmain_move_id(reuse_data, old_id_iter, old_id_new_lib, true)) {
      /* Port over dependencies of re-used ID, unless matching already existing ones in
       * new_bmain can be found.
       *
       * NOTE : No pointers are remapped here, this code only moves dependencies from old_bmain
       * to new_bmain if needed, and add necessary remapping rules to the reuse_data.remapper. */
      BKE_library_foreach_ID_link(new_bmain,
                                  old_id_iter,
                                  reuse_editable_asset_bmain_data_dependencies_process_cb,
                                  reuse_data,
                                  IDWALK_RECURSE | IDWALK_DO_LIBRARY_POINTER);
    }
  }
  FOREACH_MAIN_LISTBASE_ID_END;
}

/**
 * Grease pencil brushes may have a material pinned that is from the current file. Moving local
 * scene data to a different #Main is tricky, so in that case, unpin the material.
 */
static void unpin_file_local_grease_pencil_brush_materials(const ReuseOldBMainData *reuse_data)
{
  ID *old_id_iter;
  FOREACH_MAIN_LISTBASE_ID_BEGIN (&reuse_data->old_bmain->brushes, old_id_iter) {
    const Brush *brush = reinterpret_cast<Brush *>(old_id_iter);
    if (brush->gpencil_settings && brush->gpencil_settings->material &&
        /* Don't unpin if this material is linked, then it can be preserved for the new file. */
        !ID_IS_LINKED(brush->gpencil_settings->material))
    {
      /* Unpin material and clear pointer. */
      brush->gpencil_settings->flag &= ~GP_BRUSH_MATERIAL_PINNED;
      brush->gpencil_settings->material = nullptr;
    }
  }
  FOREACH_MAIN_LISTBASE_ID_END;
}

/**
 * Does a complete replacement of data in `new_bmain` by data from `old_bmain`. Original new data
 * are moved to the `old_bmain`, and will be freed together with it.
 *
 * WARNING: Currently only expects to work on local data, won't work properly if some of the IDs of
 * given type are linked.
 *
 * NOTE: Unlike with #reuse_editable_asset_bmain_data_for_blendfile, there is no support at all for
 * potential dependencies of the IDs moved around. This is not expected to be necessary for the
 * current use cases (UI-related IDs).
 */
static void swap_old_bmain_data_for_blendfile(ReuseOldBMainData *reuse_data, const short id_code)
{
  Main *new_bmain = reuse_data->new_bmain;
  Main *old_bmain = reuse_data->old_bmain;

  ListBase *new_lb = which_libbase(new_bmain, id_code);
  ListBase *old_lb = which_libbase(old_bmain, id_code);

  id::IDRemapper &remapper = reuse_bmain_data_remapper_ensure(reuse_data);

  /* NOTE: Full swapping is only supported for ID types that are assumed to be only local
   * data-blocks (like UI-like ones). Otherwise, the swapping could fail in many funny ways. */
  BLI_assert(BLI_listbase_is_empty(old_lb) || !ID_IS_LINKED(old_lb->last));
  BLI_assert(BLI_listbase_is_empty(new_lb) || !ID_IS_LINKED(new_lb->last));

  std::swap(*new_lb, *old_lb);

  /* TODO: Could add per-IDType control over name-maps clearing, if this becomes a performances
   * concern. */
  BKE_main_namemap_clear(*old_bmain);
  BKE_main_namemap_clear(*new_bmain);

  /* Original 'new' IDs have been moved into the old listbase and will be discarded (deleted).
   * Original 'old' IDs have been moved into the new listbase and are being reused (kept).
   * The discarded ones need to be remapped to a matching reused one, based on their names, if
   * possible.
   *
   * Since both lists are ordered, and they are all local, we can do a smart parallel processing of
   * both lists here instead of doing complete full list searches. */
  ID *discarded_id_iter = static_cast<ID *>(old_lb->first);
  ID *reused_id_iter = static_cast<ID *>(new_lb->first);
  while (!ELEM(nullptr, discarded_id_iter, reused_id_iter)) {
    const int strcmp_result = strcmp(discarded_id_iter->name + 2, reused_id_iter->name + 2);
    if (strcmp_result == 0) {
      /* Matching IDs, we can remap the discarded 'new' one to the re-used 'old' one. */
      remapper.add(discarded_id_iter, reused_id_iter);

      discarded_id_iter = static_cast<ID *>(discarded_id_iter->next);
      reused_id_iter = static_cast<ID *>(reused_id_iter->next);
    }
    else if (strcmp_result < 0) {
      /* No matching reused 'old' ID for this discarded 'new' one. */
      remapper.add(discarded_id_iter, nullptr);

      discarded_id_iter = static_cast<ID *>(discarded_id_iter->next);
    }
    else {
      reused_id_iter = static_cast<ID *>(reused_id_iter->next);
    }
  }
  /* Also remap all remaining non-compared discarded 'new' IDs to null. */
  for (; discarded_id_iter != nullptr;
       discarded_id_iter = static_cast<ID *>(discarded_id_iter->next))
  {
    remapper.add(discarded_id_iter, nullptr);
  }

  FOREACH_MAIN_LISTBASE_ID_BEGIN (new_lb, reused_id_iter) {
    /* Necessary as all `session_uid` are renewed on blendfile loading. */
    BKE_lib_libblock_session_uid_renew(reused_id_iter);

    /* Ensure that the reused ID is remapped to itself, since it is known to be in the `new_bmain`.
     */
    remapper.add_overwrite(reused_id_iter, reused_id_iter);
  }
  FOREACH_MAIN_LISTBASE_ID_END;
}

/**
 * Similar to #swap_old_bmain_data_for_blendfile, but with special handling for WM ID. Tightly
 * related to further WM post-processing from calling WM code (see #WM_file_read and
 * #wm_homefile_read_ex).
 */
static void swap_wm_data_for_blendfile(ReuseOldBMainData *reuse_data, const bool load_ui)
{
  Main *old_bmain = reuse_data->old_bmain;
  Main *new_bmain = reuse_data->new_bmain;
  ListBase *old_wm_list = &old_bmain->wm;
  ListBase *new_wm_list = &new_bmain->wm;

  /* Currently there should never be more than one WM in a main. */
  BLI_assert(BLI_listbase_count_at_most(new_wm_list, 2) <= 1);
  BLI_assert(BLI_listbase_count_at_most(old_wm_list, 2) <= 1);

  wmWindowManager *old_wm = static_cast<wmWindowManager *>(old_wm_list->first);
  wmWindowManager *new_wm = static_cast<wmWindowManager *>(new_wm_list->first);

  if (old_wm == nullptr) {
    /* No current (old) WM. Either (new) WM from file is used, or if none, WM code is responsible
     * to add a new default WM. Nothing to do here. */
    return;
  }

  /* Current (old) WM, and (new) WM in file, and loading UI: use WM from file, keep old WM around
   * for further processing in WM code. */
  if (load_ui && new_wm != nullptr) {
    /* Support window-manager ID references being held between file load operations by keeping
     * #Main.wm.first memory address in-place, while swapping all of its contents.
     *
     * This is needed so items such as key-maps can be held by an add-on,
     * without it pointing to invalid memory, see: #86431. */
    BLI_remlink(old_wm_list, old_wm);
    BLI_remlink(new_wm_list, new_wm);
    BKE_lib_id_swap_full(nullptr,
                         &old_wm->id,
                         &new_wm->id,
                         true,
                         (ID_REMAP_SKIP_NEVER_NULL_USAGE | ID_REMAP_SKIP_UPDATE_TAGGING |
                          ID_REMAP_SKIP_USER_REFCOUNT | ID_REMAP_FORCE_UI_POINTERS));
    /* Not strictly necessary, but helps for readability. */
    std::swap<wmWindowManager *>(old_wm, new_wm);
    BLI_addhead(new_wm_list, new_wm);
    /* Do not add old WM back to `old_bmain`, so that it does not get freed when `old_bmain` is
     * freed. Calling WM code will need this old WM to restore some windows etc. data into the
     * new WM, and is responsible to free it properly. */
    reuse_data->wm_setup_data->old_wm = old_wm;

    id::IDRemapper &remapper = reuse_bmain_data_remapper_ensure(reuse_data);
    remapper.add(&old_wm->id, &new_wm->id);
  }
  /* Current (old) WM, but no (new) one in file (should only happen when reading pre 2.5 files, no
   * WM back then), or not loading UI: Keep current WM. */
  else {
    swap_old_bmain_data_for_blendfile(reuse_data, ID_WM);
    old_wm->init_flag &= ~WM_INIT_FLAG_WINDOW;
    reuse_data->wm_setup_data->old_wm = old_wm;
  }
}

static int swap_old_bmain_data_for_blendfile_dependencies_process_cb(
    LibraryIDLinkCallbackData *cb_data)
{
  ID *id = *cb_data->id_pointer;

  if (id == nullptr) {
    return IDWALK_RET_NOP;
  }

  ReuseOldBMainData *reuse_data = static_cast<ReuseOldBMainData *>(cb_data->user_data);

  /* First check if it has already been remapped. */
  id::IDRemapper &remapper = reuse_bmain_data_remapper_ensure(reuse_data);
  if (reuse_bmain_data_remapper_is_id_remapped(remapper, id)) {
    return IDWALK_RET_NOP;
  }

  IDNameLib_Map *id_map = reuse_data->id_map;
  BLI_assert(id_map != nullptr);

  ID *id_new = BKE_main_idmap_lookup_id(id_map, id);
  remapper.add(id, id_new);

  return IDWALK_RET_NOP;
}

static void swap_old_bmain_data_dependencies_process(ReuseOldBMainData *reuse_data,
                                                     const short id_code)
{
  Main *new_bmain = reuse_data->new_bmain;
  ListBase *new_lb = which_libbase(new_bmain, id_code);

  BLI_assert(reuse_data->id_map != nullptr);

  ID *new_id_iter;
  FOREACH_MAIN_LISTBASE_ID_BEGIN (new_lb, new_id_iter) {
    /* Check all ID usages and find a matching new ID to remap them to in `new_bmain` if possible
     * (matching by names and libraries).
     *
     * Note that this call does not do any effective remapping, it only adds required remapping
     * operations to the remapper. */
    BKE_library_foreach_ID_link(new_bmain,
                                new_id_iter,
                                swap_old_bmain_data_for_blendfile_dependencies_process_cb,
                                reuse_data,
                                IDWALK_READONLY | IDWALK_INCLUDE_UI | IDWALK_DO_LIBRARY_POINTER);
  }
  FOREACH_MAIN_LISTBASE_ID_END;
}

static int reuse_bmain_data_invalid_local_usages_fix_cb(LibraryIDLinkCallbackData *cb_data)
{
  ID *id = *cb_data->id_pointer;

  if (id == nullptr) {
    return IDWALK_RET_NOP;
  }

  /* Embedded data cannot (yet) be fully trusted to have the same lib pointer as their owner ID, so
   * for now ignore them. This code should never have anything to fix for them anyway, otherwise
   * there is something extremely wrong going on. */
  if ((cb_data->cb_flag & (IDWALK_CB_EMBEDDED | IDWALK_CB_EMBEDDED_NOT_OWNING)) != 0) {
    return IDWALK_RET_NOP;
  }

  if (!ID_IS_LINKED(id)) {
    ID *owner_id = cb_data->owner_id;

    /* Do not allow linked data to use local data. */
    if (ID_IS_LINKED(owner_id)) {
      if (cb_data->cb_flag & IDWALK_CB_USER) {
        id_us_min(id);
      }
      *cb_data->id_pointer = nullptr;
    }
    /* Do not allow local liboverride data to use local data as reference. */
    else if (ID_IS_OVERRIDE_LIBRARY_REAL(owner_id) &&
             &owner_id->override_library->reference == cb_data->id_pointer)
    {
      if (cb_data->cb_flag & IDWALK_CB_USER) {
        id_us_min(id);
      }
      *cb_data->id_pointer = nullptr;
    }
  }

  return IDWALK_RET_NOP;
}

/**
 * Detect and fix invalid usages of locale IDs by linked ones (or as reference of liboverrides).
 */
static void reuse_bmain_data_invalid_local_usages_fix(ReuseOldBMainData *reuse_data)
{
  Main *new_bmain = reuse_data->new_bmain;
  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (new_bmain, id_iter) {
    if (!ID_IS_LINKED(id_iter) && !ID_IS_OVERRIDE_LIBRARY_REAL(id_iter)) {
      continue;
    }

    ID *liboverride_reference = ID_IS_OVERRIDE_LIBRARY_REAL(id_iter) ?
                                    id_iter->override_library->reference :
                                    nullptr;

    BKE_library_foreach_ID_link(
        new_bmain, id_iter, reuse_bmain_data_invalid_local_usages_fix_cb, reuse_data, IDWALK_NOP);

    /* Liboverrides who lost their reference should not be liboverrides anymore, but regular IDs.
     */
    if (ID_IS_OVERRIDE_LIBRARY_REAL(id_iter) &&
        id_iter->override_library->reference != liboverride_reference)
    {
      BKE_lib_override_library_free(&id_iter->override_library, true);
    }
  }
  FOREACH_MAIN_ID_END;
}

/* Post-remapping helpers to ensure validity of the UI data. */

static void view3d_data_consistency_ensure(wmWindow *win, Scene *scene, ViewLayer *view_layer)
{
  bScreen *screen = BKE_workspace_active_screen_get(win->workspace_hook);

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
      if (sl->spacetype != SPACE_VIEW3D) {
        continue;
      }

      View3D *v3d = reinterpret_cast<View3D *>(sl);
      if (v3d->camera == nullptr || v3d->scenelock) {
        v3d->camera = scene->camera;
      }
      if (v3d->localvd == nullptr) {
        continue;
      }

      if (v3d->localvd->camera == nullptr || v3d->scenelock) {
        v3d->localvd->camera = v3d->camera;
      }
      /* Local-view can become invalid during undo/redo steps, exit it when no valid object could
       * be found. */
      Base *base;
      for (base = static_cast<Base *>(view_layer->object_bases.first); base; base = base->next) {
        if (base->local_view_bits & v3d->local_view_uid) {
          break;
        }
      }
      if (base != nullptr) {
        /* The local view3D still has a valid object, nothing else to do. */
        continue;
      }

      /* No valid object found for the local view3D, it has to be cleared off. */
      MEM_freeN(v3d->localvd);
      v3d->localvd = nullptr;
      v3d->local_view_uid = 0;

      /* Region-base storage is different depending on whether the space is active or not. */
      ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase : &sl->regionbase;
      LISTBASE_FOREACH (ARegion *, region, regionbase) {
        if (region->regiontype != RGN_TYPE_WINDOW) {
          continue;
        }

        RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
        MEM_SAFE_FREE(rv3d->localvd);
      }
    }
  }
}

static void wm_data_consistency_ensure(wmWindowManager *curwm,
                                       Scene *cur_scene,
                                       ViewLayer *cur_view_layer)
{
  /* There may not be any available WM (e.g. when reading `userpref.blend`). */
  if (curwm == nullptr) {
    return;
  }

  LISTBASE_FOREACH (wmWindow *, win, &curwm->windows) {
    if (win->scene == nullptr) {
      win->scene = cur_scene;
    }
    if (BKE_view_layer_find(win->scene, win->view_layer_name) == nullptr) {
      STRNCPY_UTF8(win->view_layer_name, cur_view_layer->name);
    }

    view3d_data_consistency_ensure(win, win->scene, cur_view_layer);
  }
}

/**
 * This function runs after loading blend-file data and is responsible
 * for setting up the context (the active view-layer, scene & work-space).
 *
 * When loading a blend-file without it's UI (#G_FILE_NO_UI),
 * the existing screen-data needs to point to the newly loaded blend-file data.
 *
 * \note this is called on undo so any slow conversion functions here
 * should be avoided or check `mode != LOAD_UNDO`.
 *
 * \param bfd: Blend file data, freed by this function on exit.
 */
static void setup_app_data(bContext *C,
                           BlendFileData *bfd,
                           const BlendFileReadParams *params,
                           BlendFileReadWMSetupData *wm_setup_data,
                           BlendFileReadReport *reports)
{
  Main *bmain = G_MAIN;
  const bool recover = (G.fileflags & G_FILE_RECOVER_READ) != 0;
  enum {
    LOAD_UI = 1,
    LOAD_UI_OFF,
    LOAD_UNDO,
  } mode;

  if (params->undo_direction != STEP_INVALID) {
    BLI_assert(bfd->curscene != nullptr);
    mode = LOAD_UNDO;
  }
  else if (bfd->fileflags & G_FILE_ASSET_EDIT_FILE) {
    BKE_report(reports->reports,
               RPT_WARNING,
               "This file is managed by the asset system, you cannot overwrite it (using \"Save "
               "As\" is possible)");
    /* From now on the file in memory is a normal file, further saving it will contain a
     * window-manager, scene, ... and potentially user created data. Use #Main.is_asset_edit_file
     * to detect if saving this file needs extra protections. */
    bfd->fileflags &= ~G_FILE_ASSET_EDIT_FILE;
    BLI_assert(bfd->main->is_asset_edit_file);
    mode = LOAD_UI_OFF;
  }
  /* May happen with library files, loading undo-data should never have a null `curscene`
   * (but may have a null `curscreen`). */
  else if (ELEM(nullptr, bfd->curscreen, bfd->curscene)) {
    BKE_report(reports->reports, RPT_WARNING, "Library file, loading empty scene");
    mode = LOAD_UI_OFF;
  }
  else if (G.fileflags & G_FILE_NO_UI) {
    mode = LOAD_UI_OFF;
  }
  else {
    mode = LOAD_UI;
  }

  /* Free all render results and interactive compositor renders, without this stale data gets
   * displayed after loading files */
  if (mode != LOAD_UNDO) {
    RE_FreeAllRenderResults();
    RE_FreeInteractiveCompositorRenders();
  }

  /* Only make file-paths compatible when loading for real (not undo). */
  if (mode != LOAD_UNDO) {
    clean_paths(bfd->main);
  }

  BLI_assert(BKE_main_namemap_validate(*bfd->main));

  /* Temporary data to handle swapping around IDs between old and new mains,
   * and accumulate the required remapping accordingly. */
  ReuseOldBMainData reuse_data = {nullptr};
  reuse_data.new_bmain = bfd->main;
  reuse_data.old_bmain = bmain;
  reuse_data.wm_setup_data = wm_setup_data;

  const bool reuse_editable_assets = mode != LOAD_UNDO && !params->is_factory_settings &&
                                     reuse_editable_asset_needed(&reuse_data);

  if (mode != LOAD_UNDO) {
    const short ui_id_codes[]{ID_WS, ID_SCR};

    /* WM needs special complex handling, regardless of whether UI is kept or loaded from file. */
    swap_wm_data_for_blendfile(&reuse_data, mode == LOAD_UI);
    if (mode != LOAD_UI) {
      /* Re-use UI data from `old_bmain` if keeping existing UI. */
      for (auto id_code : ui_id_codes) {
        swap_old_bmain_data_for_blendfile(&reuse_data, id_code);
      }
    }

    /* Needs to happen after all data from `old_bmain` has been moved into new one. */
    BLI_assert(reuse_data.id_map == nullptr);
    reuse_data.id_map = BKE_main_idmap_create(
        reuse_data.new_bmain, true, reuse_data.old_bmain, MAIN_IDMAP_TYPE_NAME);

    swap_old_bmain_data_dependencies_process(&reuse_data, ID_WM);
    if (mode != LOAD_UI) {
      for (auto id_code : ui_id_codes) {
        swap_old_bmain_data_dependencies_process(&reuse_data, id_code);
      }
    }

    BKE_main_idmap_destroy(reuse_data.id_map);

    if (reuse_editable_assets) {
      unpin_file_local_grease_pencil_brush_materials(&reuse_data);
      /* Keep linked brush asset data, similar to UI data. Only does a known
       * subset know. Could do everything, but that risks dragging along more
       * scene data than we want. */
      for (short idtype_index = 0; idtype_index < INDEX_ID_MAX; idtype_index++) {
        const IDTypeInfo *idtype_info = BKE_idtype_get_info_from_idtype_index(idtype_index);
        if (ID_TYPE_SUPPORTS_ASSET_EDITABLE(idtype_info->id_code)) {
          reuse_editable_asset_bmain_data_for_blendfile(&reuse_data, idtype_info->id_code);
        }
      }
    }

    if (mode != LOAD_UI) {
      LISTBASE_FOREACH (bScreen *, screen, &bfd->main->screens) {
        BKE_screen_runtime_refresh_for_blendfile(screen);
      }
    }
  }

  /* Logic for 'track_undo_scene' is to keep using the scene which the active screen has, as long
   * as the scene associated with the undo operation is visible in one of the open windows.
   *
   * - 'curscreen->scene': Scene the user is currently looking at.
   * - 'bfd->curscene': Scene undo-step was created in.
   *
   * This means that users can have 2 or more windows open and undo in both without screens
   * switching. But if they close one of the screens, undo will ensure that the scene being
   * operated on will be activated (otherwise we'd be undoing on an off-screen scene which isn't
   * acceptable). See: #43424. */
  bool track_undo_scene = false;

  /* Always use the Scene and ViewLayer pointers from new file, if possible. */
  ViewLayer *cur_view_layer = bfd->cur_view_layer;
  Scene *curscene = bfd->curscene;

  wmWindow *win = nullptr;
  bScreen *curscreen = nullptr;

  /* Ensure that there is a valid scene and view-layer. */
  if (curscene == nullptr) {
    curscene = static_cast<Scene *>(bfd->main->scenes.first);
  }
  /* Empty file, add a scene to make Blender work. */
  if (curscene == nullptr) {
    curscene = BKE_scene_add(bfd->main, "Empty");
  }
  if (cur_view_layer == nullptr) {
    /* Fallback to the active scene view layer. */
    cur_view_layer = BKE_view_layer_default_view(curscene);
  }

  /* If UI is not loaded when opening actual `.blend` file,
   * and always in case of undo MEMFILE reading. */
  if (mode != LOAD_UI) {
    /* Re-use current window and screen. */
    win = CTX_wm_window(C);
    curscreen = CTX_wm_screen(C);

    track_undo_scene = (mode == LOAD_UNDO && curscreen && curscene && bfd->main->wm.first);

    if (track_undo_scene) {
      /* Keep the old (to-be-freed) scene, remapping below will ensure it's remapped to the
       * matching new scene if available, or null otherwise, in which case
       * #wm_data_consistency_ensure will define `curscene` as the active one. */
    }
    /* Enforce `curscene` to be in current screen. */
    else if (win) { /* The window may be null in background-mode. */
      win->scene = curscene;
    }
  }

  BLI_assert(BKE_main_namemap_validate(*bfd->main));

  /* Apply remapping of ID pointers caused by re-using part of the data from the 'old' main into
   * the new one. */
  if (reuse_data.remapper != nullptr) {
    /* In undo case all "keeping old data" and remapping logic is now handled
     * in file reading code itself, so there should never be any remapping to do here. */
    BLI_assert(mode != LOAD_UNDO);

    /* Handle all pending remapping from swapping old and new IDs around. */
    BKE_libblock_remap_multiple_raw(bfd->main,
                                    *reuse_data.remapper,
                                    (ID_REMAP_FORCE_UI_POINTERS | ID_REMAP_SKIP_USER_REFCOUNT |
                                     ID_REMAP_SKIP_UPDATE_TAGGING | ID_REMAP_SKIP_USER_CLEAR));

    /* Fix potential invalid usages of now-locale-data created by remapping above. Should never
     * be needed in undo case, this is to address cases like:
     * "opening a blend-file that was a library of the previous opened blend-file". */
    reuse_bmain_data_invalid_local_usages_fix(&reuse_data);

    MEM_delete(reuse_data.remapper);
    reuse_data.remapper = nullptr;

    wm_data_consistency_ensure(CTX_wm_manager(C), curscene, cur_view_layer);
  }

  if (mode == LOAD_UNDO) {
    /* It's possible to undo into a time before the scene existed, in this case the window's scene
     * will be null. Since it doesn't make sense to remove the window, set it to the current scene.
     *
     * NOTE: Redo will restore the active scene to the window so a reasonably consistent state
     * is maintained. We could do better by keeping a window/scene map for each undo step.
     *
     * Another source of potential inconsistency is undoing into a step where the active camera
     * object does not exist (see e.g. #125636).
     */
    wm_data_consistency_ensure(CTX_wm_manager(C), curscene, cur_view_layer);
  }

  BLI_assert(BKE_main_namemap_validate(*bfd->main));

  if (mode != LOAD_UI) {
    if (win) {
      curscene = win->scene;
    }

    if (track_undo_scene) {
      wmWindowManager *wm = static_cast<wmWindowManager *>(bfd->main->wm.first);
      if (!wm_scene_is_visible(wm, bfd->curscene)) {
        curscene = bfd->curscene;
        if (win) {
          win->scene = curscene;
        }
        BKE_screen_view3d_scene_sync(curscreen, curscene);
      }
    }

    /* We need to tag this here because events may be handled immediately after.
     * only the current screen is important because we won't have to handle
     * events from multiple screens at once. */
    if (curscreen) {
      BKE_screen_gizmo_tag_refresh(curscreen);
    }
  }
  CTX_data_scene_set(C, curscene);

  BLI_assert(BKE_main_namemap_validate(*bfd->main));

  /* This frees the `old_bmain`. */
  BKE_blender_globals_main_replace(bfd->main);
  bmain = G_MAIN;
  bfd->main = nullptr;
  CTX_data_main_set(C, bmain);

  BLI_assert(BKE_main_namemap_validate(*bmain));

  /* These context data should remain valid if old UI is being re-used. */
  if (mode == LOAD_UI) {
    /* Setting a window-manger clears all other windowing members (window, screen, area, etc).
     * So only do it when effectively loading a new #wmWindowManager
     * otherwise just assert that the WM from context is still the same as in `new_bmain`. */
    CTX_wm_manager_set(C, static_cast<wmWindowManager *>(bmain->wm.first));
    CTX_wm_screen_set(C, bfd->curscreen);
    CTX_wm_area_set(C, nullptr);
    CTX_wm_region_set(C, nullptr);
    CTX_wm_region_popup_set(C, nullptr);
  }
  BLI_assert(CTX_wm_manager(C) == static_cast<wmWindowManager *>(bmain->wm.first));

  /* Keep state from preferences. */
  const int fileflags_keep = G_FILE_FLAG_ALL_RUNTIME;
  G.fileflags = (G.fileflags & fileflags_keep) | (bfd->fileflags & ~fileflags_keep);

  /* Special cases, override any #G_FLAG_ALL_READFILE flags from the blend-file. */
  if (G.f != bfd->globalf) {
    const int flags_keep = G_FLAG_ALL_RUNTIME;
    bfd->globalf &= G_FLAG_ALL_READFILE;
    bfd->globalf = (bfd->globalf & ~flags_keep) | (G.f & flags_keep);
  }

  G.f = bfd->globalf;

#ifdef WITH_PYTHON
  /* let python know about new main */
  if (CTX_py_init_get(C)) {
    BPY_context_update(C);
  }
#endif

  if (mode != LOAD_UNDO) {
    /* Perform complex versioning that involves adding or removing IDs,
     * and/or needs to operate over the whole Main data-base
     * (versioning done in file reading code only operates on a per-library basis). */
    BLO_read_do_version_after_setup(bmain, nullptr, reports);
    BLO_readfile_id_runtime_data_free_all(*bmain);
  }

  bmain->recovered = false;

  /* `startup.blend` or recovered startup. */
  if (params->is_startup) {
    bmain->filepath[0] = '\0';
  }
  else if (recover) {
    /* In case of auto-save or `quit.blend`, use original file-path instead
     * (see also #read_global in `readfile.cc`). */
    bmain->recovered = true;
    STRNCPY(bmain->filepath, bfd->filepath);
  }

  /* Set the loaded .blend file path for crash recovery. */
  STRNCPY(G.filepath_last_blend, bmain->filepath);

  /* Base-flags, groups, make depsgraph, etc. */
  /* first handle case if other windows have different scenes visible. */
  if (mode == LOAD_UI) {
    wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
    if (wm) {
      LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
        if (win->scene && win->scene != curscene) {
          BKE_scene_set_background(bmain, win->scene);
        }
      }
    }
  }

  /* Setting scene might require having a dependency graph, with copy-on-eval
   * we need to make sure we ensure scene has correct color management before
   * constructing dependency graph. */
  if (params->is_startup) {
    IMB_colormanagement_working_space_init_startup(bmain);
  }
  IMB_colormanagement_working_space_check(bmain, mode == LOAD_UNDO, reuse_editable_assets);
  IMB_colormanagement_check_file_config(bmain);

  BKE_scene_set_background(bmain, curscene);

  if (mode != LOAD_UNDO) {
    /* TODO(@sergey): Can this be also move above? */
    RE_FreeAllPersistentData();
  }

  /* Both undo and regular file loading can perform some fairly complex ID manipulation, simpler
   * and safer to fully redo reference-counting. This is a relatively cheap process anyway. */
  BKE_main_id_refcount_recompute(bmain, false);

  BLI_assert(BKE_main_namemap_validate(*bmain));

  if (mode != LOAD_UNDO && liboverride::is_auto_resync_enabled()) {
    reports->duration.lib_overrides_resync = BLI_time_now_seconds();

    BKE_lib_override_library_main_resync(
        bmain,
        nullptr,
        curscene,
        bfd->cur_view_layer ? bfd->cur_view_layer : BKE_view_layer_default_view(curscene),
        reports);

    reports->duration.lib_overrides_resync = BLI_time_now_seconds() -
                                             reports->duration.lib_overrides_resync;

    /* We need to rebuild some of the deleted override rules (for UI feedback purpose). */
    BKE_lib_override_library_main_operations_create(bmain, true, nullptr);
  }

  /* Now that liboverrides have been resynced and 'irrelevant' missing linked IDs has been removed,
   * report actual missing linked data. */
  if (mode != LOAD_UNDO) {
    ID *id_iter;
    int missing_linked_ids_num = 0;
    FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
      if (ID_IS_LINKED(id_iter) && (id_iter->tag & ID_TAG_MISSING)) {
        missing_linked_ids_num++;
        BLO_reportf_wrap(reports,
                         RPT_INFO,
                         RPT_("LIB: %s: '%s' missing from '%s', parent '%s'"),
                         BKE_idtype_idcode_to_name(GS(id_iter->name)),
                         id_iter->name + 2,
                         id_iter->lib->runtime->filepath_abs,
                         id_iter->lib->runtime->parent ?
                             id_iter->lib->runtime->parent->runtime->filepath_abs :
                             "<direct>");
      }
    }
    FOREACH_MAIN_ID_END;
    reports->count.missing_linked_id = missing_linked_ids_num;
  }
}

static void setup_app_blend_file_data(bContext *C,
                                      BlendFileData *bfd,
                                      const BlendFileReadParams *params,
                                      BlendFileReadWMSetupData *wm_setup_data,
                                      BlendFileReadReport *reports)
{
  if ((params->skip_flags & BLO_READ_SKIP_USERDEF) == 0) {
    setup_app_userdef(bfd);
  }
  if ((params->skip_flags & BLO_READ_SKIP_DATA) == 0) {
    setup_app_data(C, bfd, params, wm_setup_data, reports);
  }
}

static void handle_subversion_warning(Main *main, BlendFileReadReport *reports)
{
  if (main->versionfile > BLENDER_FILE_VERSION || (main->versionfile == BLENDER_FILE_VERSION &&
                                                   main->subversionfile > BLENDER_FILE_SUBVERSION))
  {
    BKE_reportf(reports->reports,
                RPT_WARNING,
                "File written by newer Blender binary (%d.%d), expect loss of data!",
                main->versionfile,
                main->subversionfile);
  }
}

void BKE_blendfile_read_setup_readfile(bContext *C,
                                       BlendFileData *bfd,
                                       const BlendFileReadParams *params,
                                       BlendFileReadWMSetupData *wm_setup_data,
                                       BlendFileReadReport *reports,
                                       /* Extra args. */
                                       const bool startup_update_defaults,
                                       const char *startup_app_template)
{
  if (bfd->main->is_read_invalid) {
    BKE_reports_prepend(reports->reports,
                        "File could not be read, critical data corruption detected");
    BLO_blendfiledata_free(bfd);
    return;
  }

  if (startup_update_defaults) {
    if ((params->skip_flags & BLO_READ_SKIP_DATA) == 0) {
      BLO_update_defaults_startup_blend(bfd->main, startup_app_template);
    }
  }
  setup_app_blend_file_data(C, bfd, params, wm_setup_data, reports);
  BLO_blendfiledata_free(bfd);
}

void BKE_blendfile_read_setup_undo(bContext *C,
                                   BlendFileData *bfd,
                                   const BlendFileReadParams *params,
                                   BlendFileReadReport *reports)
{
  BKE_blendfile_read_setup_readfile(C, bfd, params, nullptr, reports, false, nullptr);
}

static CLG_LogRef LOG_BLEND = {"blend"};

BlendFileData *BKE_blendfile_read(const char *filepath,
                                  const BlendFileReadParams *params,
                                  BlendFileReadReport *reports)
{
  /* Don't print startup file loading. */
  if (params->is_startup == false) {
    CLOG_INFO_NOCHECK(&LOG_BLEND, "Read blend: \"%s\"", filepath);
  }

  BlendFileData *bfd = BLO_read_from_file(filepath, eBLOReadSkip(params->skip_flags), reports);
  if (bfd && bfd->main->is_read_invalid) {
    BLO_blendfiledata_free(bfd);
    bfd = nullptr;
  }
  if (bfd) {
    handle_subversion_warning(bfd->main, reports);
  }
  else {
    BKE_reports_prependf(reports->reports, "Loading \"%s\" failed: ", filepath);
  }
  return bfd;
}

BlendFileData *BKE_blendfile_read_from_memory(const void *file_buf,
                                              int file_buf_size,
                                              const BlendFileReadParams *params,
                                              ReportList *reports)
{
  BlendFileData *bfd = BLO_read_from_memory(
      file_buf, file_buf_size, eBLOReadSkip(params->skip_flags), reports);
  if (bfd && bfd->main->is_read_invalid) {
    BLO_blendfiledata_free(bfd);
    bfd = nullptr;
  }
  if (bfd) {
    /* Pass. */
  }
  else {
    BKE_reports_prepend(reports, "Loading failed: ");
  }
  return bfd;
}

BlendFileData *BKE_blendfile_read_from_memfile(Main *bmain,
                                               MemFile *memfile,
                                               const BlendFileReadParams *params,
                                               ReportList *reports)
{
  BlendFileData *bfd = BLO_read_from_memfile(
      bmain, BKE_main_blendfile_path(bmain), memfile, params, reports);
  if (bfd && bfd->main->is_read_invalid) {
    BLO_blendfiledata_free(bfd);
    bfd = nullptr;
  }
  if (bfd == nullptr) {
    BKE_reports_prepend(reports, "Loading failed: ");
  }
  return bfd;
}

void BKE_blendfile_read_make_empty(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  ListBase *lb;
  ID *id;

  FOREACH_MAIN_LISTBASE_BEGIN (bmain, lb) {
    FOREACH_MAIN_LISTBASE_ID_BEGIN (lb, id) {
      if (ELEM(GS(id->name), ID_SCE, ID_SCR, ID_WM, ID_WS)) {
        break;
      }
      BKE_id_delete(bmain, id);
    }
    FOREACH_MAIN_LISTBASE_ID_END;
  }
  FOREACH_MAIN_LISTBASE_END;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blend File IO (Preferences)
 *
 * Application Templates
 * =====================
 *
 * When using app-templates, both regular & app-template preferences are used.
 * Note that "regular" preferences refers to the preferences used with no app-template is active.
 *
 * - Reading preferences is performed for both the app-template & regular preferences.
 *
 *   The preferences are merged by using some from the app-template and other settings from the
 *   regular preferences (add-ons from the app-template for example are used),
 *   undo-memory uses the regular preferences (for example).
 *
 * - Writing preferences is performed for both the app-template & regular preferences.
 *
 *   Writing unmodified preference (#U) into the regular preferences
 *   would loose any settings the app-template overrides.
 *   To keep default settings the regular preferences is read, add-ons etc temporarily swapped
 *   into #U for writing, then swapped back out so as not to change the run-time preferences.
 *
 * \note The function #BKE_blender_userdef_app_template_data_swap determines which settings
 * the app-template overrides.
 * \{ */

UserDef *BKE_blendfile_userdef_read(const char *filepath, ReportList *reports)
{
  BlendFileData *bfd;
  UserDef *userdef = nullptr;

  BlendFileReadReport blend_file_read_reports{};
  blend_file_read_reports.reports = reports;

  bfd = BLO_read_from_file(
      filepath, BLO_READ_SKIP_ALL & ~BLO_READ_SKIP_USERDEF, &blend_file_read_reports);
  if (bfd) {
    if (bfd->user) {
      userdef = bfd->user;
    }
    BKE_main_free(bfd->main);
    MEM_delete(bfd);
  }

  return userdef;
}

UserDef *BKE_blendfile_userdef_read_from_memory(const void *file_buf,
                                                int file_buf_size,
                                                ReportList *reports)
{
  BlendFileData *bfd;
  UserDef *userdef = nullptr;

  bfd = BLO_read_from_memory(
      file_buf, file_buf_size, BLO_READ_SKIP_ALL & ~BLO_READ_SKIP_USERDEF, reports);
  if (bfd) {
    if (bfd->user) {
      userdef = bfd->user;
    }
    BKE_main_free(bfd->main);
    MEM_delete(bfd);
  }
  else {
    BKE_reports_prepend(reports, "Loading failed: ");
  }

  return userdef;
}

UserDef *BKE_blendfile_userdef_from_defaults()
{
  UserDef *userdef = MEM_callocN<UserDef>(__func__);
  *userdef = blender::dna::shallow_copy(U_default);

  /* Add-ons. */
  {
    const char *addons[] = {
        "io_anim_bvh",
        "io_curve_svg",
        "io_mesh_uv_layout",
        "io_scene_fbx",
        "io_scene_gltf2",
        "cycles",
        "pose_library",
        "bl_pkg",
    };
    for (int i = 0; i < ARRAY_SIZE(addons); i++) {
      bAddon *addon = BKE_addon_new();
      STRNCPY_UTF8(addon->module, addons[i]);
      BLI_addtail(&userdef->addons, addon);
    }
  }

  /* Theme. */
  {
    bTheme *btheme = MEM_mallocN<bTheme>(__func__);
    memcpy(btheme, &U_theme_default, sizeof(*btheme));

    BLI_addtail(&userdef->themes, btheme);
  }

#ifdef WITH_PYTHON_SECURITY
  /* use alternative setting for security nuts
   * otherwise we'd need to patch the binary blob - startup.blend.c */
  userdef->flag |= USER_SCRIPT_AUTOEXEC_DISABLE;
#else
  userdef->flag &= ~USER_SCRIPT_AUTOEXEC_DISABLE;
#endif

  /* System-specific fonts directory.
   * NOTE: when not found, leaves as-is (`//` for the blend-file directory). */
  if (BKE_appdir_font_folder_default(userdef->fontdir, sizeof(userdef->fontdir))) {
    /* Not actually needed, just a convention that directory selection
     * adds a trailing slash. */
    BLI_path_slash_ensure(userdef->fontdir, sizeof(userdef->fontdir));
  }

  userdef->memcachelimit = min_ii(BLI_system_memory_max_in_megabytes_int() / 2,
                                  userdef->memcachelimit);

  /* Init weight paint range. */
  BKE_colorband_init(&userdef->coba_weight, true);

  /* Default studio light. */
  BKE_studiolight_default(userdef->light_param, userdef->light_ambient);

  BKE_preferences_asset_library_default_add(userdef);

  BKE_preferences_extension_repo_add_defaults_all(userdef);

  {
    BKE_preferences_asset_shelf_settings_ensure_catalog_path_enabled(
        userdef, "VIEW3D_AST_brush_sculpt", "Brushes/Mesh Sculpt/General");
    BKE_preferences_asset_shelf_settings_ensure_catalog_path_enabled(
        userdef, "VIEW3D_AST_brush_sculpt", "Brushes/Mesh Sculpt/Paint");
    BKE_preferences_asset_shelf_settings_ensure_catalog_path_enabled(
        userdef, "VIEW3D_AST_brush_sculpt", "Brushes/Mesh Sculpt/Simulation");

    BKE_preferences_asset_shelf_settings_ensure_catalog_path_enabled(
        userdef, "IMAGE_AST_brush_paint", "Brushes/Mesh Texture Paint/Basic");
    BKE_preferences_asset_shelf_settings_ensure_catalog_path_enabled(
        userdef, "IMAGE_AST_brush_paint", "Brushes/Mesh Texture Paint/Erase");
    BKE_preferences_asset_shelf_settings_ensure_catalog_path_enabled(
        userdef, "IMAGE_AST_brush_paint", "Brushes/Mesh Texture Paint/Pixel Art");
    BKE_preferences_asset_shelf_settings_ensure_catalog_path_enabled(
        userdef, "IMAGE_AST_brush_paint", "Brushes/Mesh Texture Paint/Utilities");

    BKE_preferences_asset_shelf_settings_ensure_catalog_path_enabled(
        userdef, "VIEW3D_AST_brush_texture_paint", "Brushes/Mesh Texture Paint/Basic");
    BKE_preferences_asset_shelf_settings_ensure_catalog_path_enabled(
        userdef, "VIEW3D_AST_brush_texture_paint", "Brushes/Mesh Texture Paint/Erase");
    BKE_preferences_asset_shelf_settings_ensure_catalog_path_enabled(
        userdef, "VIEW3D_AST_brush_texture_paint", "Brushes/Mesh Texture Paint/Pixel Art");
    BKE_preferences_asset_shelf_settings_ensure_catalog_path_enabled(
        userdef, "VIEW3D_AST_brush_texture_paint", "Brushes/Mesh Texture Paint/Utilities");

    BKE_preferences_asset_shelf_settings_ensure_catalog_path_enabled(
        userdef, "VIEW3D_AST_brush_gpencil_paint", "Brushes/Grease Pencil Draw/Draw");
    BKE_preferences_asset_shelf_settings_ensure_catalog_path_enabled(
        userdef, "VIEW3D_AST_brush_gpencil_paint", "Brushes/Grease Pencil Draw/Erase");
    BKE_preferences_asset_shelf_settings_ensure_catalog_path_enabled(
        userdef, "VIEW3D_AST_brush_gpencil_paint", "Brushes/Grease Pencil Draw/Utilities");

    BKE_preferences_asset_shelf_settings_ensure_catalog_path_enabled(
        userdef, "VIEW3D_AST_brush_gpencil_sculpt", "Brushes/Grease Pencil Sculpt/Contrast");
    BKE_preferences_asset_shelf_settings_ensure_catalog_path_enabled(
        userdef, "VIEW3D_AST_brush_gpencil_sculpt", "Brushes/Grease Pencil Sculpt/Transform");
    BKE_preferences_asset_shelf_settings_ensure_catalog_path_enabled(
        userdef, "VIEW3D_AST_brush_gpencil_sculpt", "Brushes/Grease Pencil Sculpt/Utilities");
  }

  return userdef;
}

bool BKE_blendfile_userdef_write(const char *filepath, ReportList *reports)
{
  Main *mainb = MEM_new<Main>(__func__);
  bool ok = false;

  BlendFileWriteParams params{};
  params.use_userdef = true;

  if (BLO_write_file(mainb, filepath, 0, &params, reports)) {
    ok = true;
  }

  MEM_delete(mainb);

  return ok;
}

bool BKE_blendfile_userdef_write_app_template(const char *filepath, ReportList *reports)
{
  /* Checking that `filepath` exists is not essential, it just avoids printing a warning that
   * the file can't be found. In this case it's not an error - as the file is used if it exists,
   * falling back to the defaults.
   * If the preferences exists but file reading fails - the file can be assumed corrupt
   * so overwriting the file is OK. */
  UserDef *userdef_default = BLI_exists(filepath) ? BKE_blendfile_userdef_read(filepath, nullptr) :
                                                    nullptr;
  if (userdef_default == nullptr) {
    userdef_default = BKE_blendfile_userdef_from_defaults();
  }

  BKE_blender_userdef_app_template_data_swap(&U, userdef_default);
  bool ok = BKE_blendfile_userdef_write(filepath, reports);
  BKE_blender_userdef_app_template_data_swap(&U, userdef_default);
  BKE_blender_userdef_data_free(userdef_default, false);
  MEM_freeN(userdef_default);
  return ok;
}

bool BKE_blendfile_userdef_write_all(ReportList *reports)
{
  char filepath[FILE_MAX];
  bool ok = true;
  const bool use_template_userpref = BKE_appdir_app_template_has_userpref(U.app_template);
  std::optional<std::string> cfgdir = BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, nullptr);

  if (cfgdir) {
    bool ok_write;
    BLI_path_join(filepath, sizeof(filepath), cfgdir->c_str(), BLENDER_USERPREF_FILE);
    CLOG_INFO_NOCHECK(&LOG_BLEND, "Writing user preferences: \"%s\" ", filepath);
    if (use_template_userpref) {
      ok_write = BKE_blendfile_userdef_write_app_template(filepath, reports);
    }
    else {
      ok_write = BKE_blendfile_userdef_write(filepath, reports);
    }

    if (ok_write) {
      BKE_report(reports, RPT_INFO, "Preferences saved");
    }
    else {
      CLOG_WARN(&LOG_BLEND, "Failed to write user preferences");
      ok = false;
      BKE_report(reports, RPT_ERROR, "Saving preferences failed");
    }
  }
  else {
    BKE_report(reports, RPT_ERROR, "Unable to create userpref path");
  }

  if (use_template_userpref) {
    cfgdir = BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, U.app_template);
    if (cfgdir) {
      /* Also save app-template preferences. */
      BLI_path_join(filepath, sizeof(filepath), cfgdir->c_str(), BLENDER_USERPREF_FILE);

      CLOG_INFO_NOCHECK(&LOG_BLEND, "Writing user preferences app-template: \"%s\" ", filepath);
      if (BKE_blendfile_userdef_write(filepath, reports) != 0) {
      }
      else {
        CLOG_WARN(&LOG_BLEND, "Failed to write user preferences");
        ok = false;
      }
    }
    else {
      BKE_report(reports, RPT_ERROR, "Unable to create app-template userpref path");
      ok = false;
    }
  }

  if (ok) {
    U.runtime.is_dirty = false;
  }
  return ok;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blend File IO (WorkSpace)
 * \{ */

WorkspaceConfigFileData *BKE_blendfile_workspace_config_read(const char *filepath,
                                                             const void *file_buf,
                                                             int file_buf_size,
                                                             ReportList *reports)
{
  BlendFileData *bfd;
  WorkspaceConfigFileData *workspace_config = nullptr;

  if (filepath) {
    BlendFileReadReport blend_file_read_reports{};
    blend_file_read_reports.reports = reports;
    bfd = BLO_read_from_file(filepath, BLO_READ_SKIP_USERDEF, &blend_file_read_reports);
  }
  else {
    bfd = BLO_read_from_memory(file_buf, file_buf_size, BLO_READ_SKIP_USERDEF, reports);
  }

  if (bfd) {
    workspace_config = MEM_callocN<WorkspaceConfigFileData>(__func__);
    workspace_config->main = bfd->main;

    /* Only 2.80+ files have actual workspaces, don't try to use screens
     * from older versions. */
    if (bfd->main->versionfile >= 280) {
      workspace_config->workspaces = bfd->main->workspaces;
    }

    MEM_delete(bfd);
  }

  return workspace_config;
}

void BKE_blendfile_workspace_config_data_free(WorkspaceConfigFileData *workspace_config)
{
  BKE_main_free(workspace_config->main);
  MEM_freeN(workspace_config);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blend File Write (Partial)
 * \{ */

static CLG_LogRef LOG_PARTIALWRITE = {"blend.partial_write"};

namespace blender::bke::blendfile {

PartialWriteContext::PartialWriteContext(Main &reference_main)
    : reference_root_filepath_(BKE_main_blendfile_path(&reference_main))
{
  if (!reference_root_filepath_.empty()) {
    STRNCPY(this->bmain.filepath, reference_root_filepath_.c_str());
  }
  this->bmain.colorspace = reference_main.colorspace;
  /* Only for IDs matching existing data in current G_MAIN. */
  matching_uid_map_ = BKE_main_idmap_create(&this->bmain, false, nullptr, MAIN_IDMAP_TYPE_UID);
  /* For all IDs existing in the context. */
  this->bmain.id_map = BKE_main_idmap_create(
      &this->bmain, false, nullptr, MAIN_IDMAP_TYPE_UID | MAIN_IDMAP_TYPE_NAME);
};

PartialWriteContext::~PartialWriteContext()
{
  BKE_main_idmap_destroy(matching_uid_map_);
};

void PartialWriteContext::preempt_session_uid(ID *ctx_id, uint session_uid)
{
  /* If there is already an existing ID in the 'matching' set with that UID, it should be the same
   * as the given ctx_id. */
  ID *matching_ctx_id = BKE_main_idmap_lookup_uid(matching_uid_map_, session_uid);
  if (matching_ctx_id == ctx_id) {
    /* That ID has already been added to the context, nothing to do. */
    BLI_assert(matching_ctx_id->session_uid == session_uid);
    return;
  }
  if (matching_ctx_id != nullptr) {
    /* Another ID in the context, who has a matching ID in current G_MAIN, is sharing the same
     * session UID. This marks a critical corruption somewhere! */
    CLOG_FATAL(
        &LOG_PARTIALWRITE,
        "Different matching IDs sharing the same session UID in the partial write context.");
    return;
  }
  /* No ID with this session UID in the context, who's matching a current ID in G_MAIN. Check if a
   * non-matching context ID is already using that UID, if yes, regenerate a new one for it, such
   * that given `ctx_id` can use the desired UID. */
  /* NOTE: In theory, there should never be any session uid collision currently, since these are
   * generated session-wide, regardless of the type/source of the IDs. */
  matching_ctx_id = BKE_main_idmap_lookup_uid(this->bmain.id_map, session_uid);
  BLI_assert(matching_ctx_id != ctx_id);
  if (matching_ctx_id) {
    CLOG_DEBUG(&LOG_PARTIALWRITE,
               "Non-matching IDs sharing the same session UID in the partial write context.");
    BKE_main_idmap_remove_id(this->bmain.id_map, matching_ctx_id);
    /* FIXME: Allow #BKE_lib_libblock_session_uid_renew to work with temp IDs? */
    matching_ctx_id->tag &= ~ID_TAG_TEMP_MAIN;
    BKE_lib_libblock_session_uid_renew(matching_ctx_id);
    matching_ctx_id->tag |= ID_TAG_TEMP_MAIN;
    BKE_main_idmap_insert_id(this->bmain.id_map, matching_ctx_id);
    BLI_assert(BKE_main_idmap_lookup_uid(this->bmain.id_map, session_uid) == nullptr);
  }
  ctx_id->session_uid = session_uid;
}

void PartialWriteContext::process_added_id(ID *ctx_id,
                                           const PartialWriteContext::IDAddOperations operations)
{
  const bool set_fake_user = (operations & SET_FAKE_USER) != 0;
  const bool set_clipboard_mark = (operations & SET_CLIPBOARD_MARK) != 0;

  if (set_fake_user) {
    id_fake_user_set(ctx_id);
  }
  else {
    /* NOTE: Using this tag will ensure that this ID is written on disk in current state (current
     * context session). However, reloading the blendfile will clear this tag. */
    id_us_ensure_real(ctx_id);
  }

  if (set_clipboard_mark) {
    ctx_id->flag |= ID_FLAG_CLIPBOARD_MARK;
  }
}

ID *PartialWriteContext::id_add_copy(const ID *id, const bool regenerate_session_uid)
{
  ID *ctx_root_id = nullptr;
  BLI_assert(BKE_main_idmap_lookup_uid(matching_uid_map_, id->session_uid) == nullptr);
  const int copy_flags = (LIB_ID_CREATE_NO_MAIN | LIB_ID_CREATE_NO_USER_REFCOUNT |
                          /* NOTE: Could make this an option if needed in the future */
                          LIB_ID_COPY_ASSET_METADATA);
  ctx_root_id = BKE_id_copy_in_lib(nullptr, id->lib, id, std::nullopt, nullptr, copy_flags);
  if (!ctx_root_id) {
    return ctx_root_id;
  }
  ctx_root_id->tag |= ID_TAG_TEMP_MAIN;
  /* It is critical to preserve the deep hash here, as the copy put in the partial write context is
   * expected to be a perfect duplicate of the packed ID (including all of its dependencies). This
   * will also be used on paste for deduplication. */
  ctx_root_id->deep_hash = id->deep_hash;
  /* Ensure that the newly copied ID has a library in temp local bmain if it was linked.
   * While this could be optimized out in case the ID is made local in the context, this adds
   * complexity as default ID management code like 'make local' code will create invalid bmain
   * namemap data. */
  this->ensure_library(ctx_root_id);
  if (regenerate_session_uid) {
    /* Calling #BKE_lib_libblock_session_uid_renew is not needed here, copying already generated a
     * new one. */
    BLI_assert(BKE_main_idmap_lookup_uid(matching_uid_map_, id->session_uid) == nullptr);
  }
  else {
    this->preempt_session_uid(ctx_root_id, id->session_uid);
    BKE_main_idmap_insert_id(matching_uid_map_, ctx_root_id);
  }
  BKE_main_idmap_insert_id(this->bmain.id_map, ctx_root_id);
  BKE_libblock_management_main_add(&this->bmain, ctx_root_id);
  /* Note: remapping of external file relative paths is done as part of the 'write' process. */
  return ctx_root_id;
}

void PartialWriteContext::make_local(ID *ctx_id, const int make_local_flags)
{
  /* Making an ID local typically resets its session UID, here we want to keep the same value. */
  const uint ctx_id_session_uid = ctx_id->session_uid;
  BKE_main_idmap_remove_id(this->bmain.id_map, ctx_id);
  BKE_main_idmap_remove_id(matching_uid_map_, ctx_id);

  if (ID_IS_LINKED(ctx_id)) {
    BKE_lib_id_make_local(&this->bmain, ctx_id, make_local_flags);
  }
  /* NOTE: Cannot rely only on `ID_IS_OVERRIDE_LIBRARY` here, as the reference pointer to the
   * linked data may have already been cleared out by dependency management in code above that
   * call. */
  else if ((ctx_id->override_library || ID_IS_OVERRIDE_LIBRARY(ctx_id)) &&
           (make_local_flags & LIB_ID_MAKELOCAL_LIBOVERRIDE_CLEAR) != 0)

  {
    BKE_lib_override_library_make_local(&this->bmain, ctx_id);
  }

  this->preempt_session_uid(ctx_id, ctx_id_session_uid);
  BKE_main_idmap_insert_id(this->bmain.id_map, ctx_id);
  BKE_main_idmap_insert_id(matching_uid_map_, ctx_id);
}

Library *PartialWriteContext::ensure_library(ID *ctx_id)
{
  if (!ID_IS_LINKED(ctx_id)) {
    return nullptr;
  }

  Library *src_lib = ctx_id->lib;
  const bool is_archive_lib = (src_lib->flag & LIBRARY_FLAG_IS_ARCHIVE) != 0;
  Library *src_base_lib = is_archive_lib ? src_lib->archive_parent_library : src_lib;
  BLI_assert(src_base_lib);
  BLI_assert((is_archive_lib && src_lib != src_base_lib && !ctx_id->deep_hash.is_null()) ||
             (!is_archive_lib && src_lib == src_base_lib && ctx_id->deep_hash.is_null()));

  blender::StringRefNull lib_path = src_base_lib->runtime->filepath_abs;
  Library *ctx_base_lib = this->libraries_map_.lookup_default(lib_path, nullptr);
  if (!ctx_base_lib) {
    ctx_base_lib = reinterpret_cast<Library *>(id_add_copy(&src_base_lib->id, true));
    BLI_assert(ctx_base_lib);
    this->libraries_map_.add(lib_path, ctx_base_lib);
  }
  /* The mapping should only contain real libraries, never packed ones. */
  BLI_assert(!ctx_base_lib || (ctx_base_lib->flag & LIBRARY_FLAG_IS_ARCHIVE) == 0);

  /* There is a valid context base library, now find or create a valid archived library if needed.
   */
  Library *ctx_lib = ctx_base_lib;
  if (is_archive_lib) {
    /* Leave the creation of a new archive library to the Library code, when needed, instead of
     * using the write context's own `id_add_copy` util. Both are doing different and complex
     * things, but for archive libraries the Library code should be mostly usable 'as-is'. */
    bool is_new = false;
    ctx_lib = blender::bke::library::ensure_archive_library(
        this->bmain, *ctx_id, *ctx_lib, ctx_id->deep_hash, is_new);
    if (is_new) {
      ctx_lib->id.tag |= ID_TAG_TEMP_MAIN;
      BKE_main_idmap_insert_id(this->bmain.id_map, &ctx_lib->id);
    }
  }

  ctx_id->lib = ctx_lib;
  return ctx_lib;
}
Library *PartialWriteContext::ensure_library(blender::StringRefNull library_absolute_path)
{
  Library *ctx_lib = this->libraries_map_.lookup_default(library_absolute_path, nullptr);
  if (!ctx_lib) {
    const char *library_name = BLI_path_basename(library_absolute_path.c_str());
    ctx_lib = static_cast<Library *>(
        BKE_id_new_in_lib(&this->bmain, nullptr, ID_LI, library_name));
    ctx_lib->id.tag |= ID_TAG_TEMP_MAIN;
    id_us_min(&ctx_lib->id);
    this->libraries_map_.add(library_absolute_path, ctx_lib);
  }
  return ctx_lib;
}

ID *PartialWriteContext::id_add(
    const ID *id,
    PartialWriteContext::IDAddOptions options,
    blender::FunctionRef<PartialWriteContext::IDAddOperations(
        LibraryIDLinkCallbackData *cb_data, PartialWriteContext::IDAddOptions options)>
        dependencies_filter_cb)
{
  constexpr int make_local_flags = (LIB_ID_MAKELOCAL_INDIRECT | LIB_ID_MAKELOCAL_FORCE_LOCAL |
                                    LIB_ID_MAKELOCAL_LIBOVERRIDE_CLEAR);

  const bool add_dependencies = (options.operations & ADD_DEPENDENCIES) != 0;
  const bool clear_dependencies = (options.operations & CLEAR_DEPENDENCIES) != 0;
  const bool duplicate_dependencies = (options.operations & DUPLICATE_DEPENDENCIES) != 0;
  BLI_assert(clear_dependencies || add_dependencies || dependencies_filter_cb);
  BLI_assert(!clear_dependencies || !(add_dependencies || duplicate_dependencies));
  UNUSED_VARS_NDEBUG(add_dependencies, clear_dependencies, duplicate_dependencies);

  /* Do not directly add an embedded ID. Add its owner instead. */
  if (id->flag & ID_FLAG_EMBEDDED_DATA) {
    id = BKE_id_owner_get(const_cast<ID *>(id), true);
  }

  /* The given ID may have already been added (either explicitly or as a dependency) before. */
  /* NOTE: This should not be needed currently (as this is only used as temporary partial copy of
   * the current main data-base, so ID's runtime `session_uid` should be enough), but in the
   * future it might also be good to lookup by ID deep hash for packed data? */
  ID *ctx_root_id = BKE_main_idmap_lookup_uid(matching_uid_map_, id->session_uid);
  if (ctx_root_id) {
    /* If the root orig ID is already in the context, assume all of its dependencies are as well.
     */
    BLI_assert(ctx_root_id->session_uid == id->session_uid);
    this->process_added_id(ctx_root_id, options.operations);
    return ctx_root_id;
  }

  /* Local mapping, such that even in case dependencies are duplicated for this specific added ID,
   * once a dependency has been duplicated, it can be re-used for other ID usages within the
   * dependencies of the added ID. */
  blender::Map<const ID *, ID *> local_ctx_id_map;
  /* A list of IDs to post-process. Only contains IDs that were actually added to the context (not
   * the ones that were already there and were re-used). The #IDAddOperations item of the pair
   * stores the returned value from the given #dependencies_filter_cb (or given global #options
   * parameter otherwise). */
  blender::Vector<std::pair<ID *, PartialWriteContext::IDAddOperations>> post_process_ids_todo;

  ctx_root_id = id_add_copy(id, false);
  if (!ctx_root_id) {
    CLOG_ERROR(&LOG_PARTIALWRITE,
               "Failed to copy ID '%s', could not add it to the partial write context",
               id->name);
    return ctx_root_id;
  }

  BLI_assert(ctx_root_id->session_uid == id->session_uid);
  local_ctx_id_map.add(id, ctx_root_id);
  post_process_ids_todo.append({ctx_root_id, options.operations});
  this->process_added_id(ctx_root_id, options.operations);

  blender::VectorSet<ID *> ids_to_process{ctx_root_id};
  auto dependencies_cb = [this,
                          options,
                          &local_ctx_id_map,
                          &ids_to_process,
                          &post_process_ids_todo,
                          dependencies_filter_cb](LibraryIDLinkCallbackData *cb_data) -> int {
    ID **id_ptr = cb_data->id_pointer;
    const ID *orig_deps_id = *id_ptr;

    if (cb_data->cb_flag & (IDWALK_CB_EMBEDDED | IDWALK_CB_EMBEDDED_NOT_OWNING)) {
      return IDWALK_RET_NOP;
    }
    if (!orig_deps_id) {
      return IDWALK_RET_NOP;
    }

    if (cb_data->cb_flag & IDWALK_CB_INTERNAL) {
      /* Cleanup internal ID pointers. */
      *id_ptr = nullptr;
      return IDWALK_RET_NOP;
    }

    PartialWriteContext::IDAddOperations operations_final = (options.operations & MASK_INHERITED);
    if (dependencies_filter_cb) {
      const PartialWriteContext::IDAddOperations operations_per_id = dependencies_filter_cb(
          cb_data, options);
      operations_final = ((operations_per_id & MASK_PER_ID_USAGE) |
                          (operations_final & ~MASK_PER_ID_USAGE));
      if (ID_IS_PACKED(orig_deps_id) && (operations_final & MAKE_LOCAL) == 0) {
        /* To ensure that their deep hash still matches with their 'context' copy, packed IDs that
         * are not made local (i.e. 'unpacked'):
         *  - Must also include all of their dependencies.
         *  - Should never duplicate or clear their dependencies. */
        operations_final |= ADD_DEPENDENCIES;
        operations_final &= ~(DUPLICATE_DEPENDENCIES | CLEAR_DEPENDENCIES);
      }
    }

    const bool add_dependencies = (operations_final & ADD_DEPENDENCIES) != 0;
    const bool clear_dependencies = (operations_final & CLEAR_DEPENDENCIES) != 0;
    const bool duplicate_dependencies = (operations_final & DUPLICATE_DEPENDENCIES) != 0;
    BLI_assert(clear_dependencies || add_dependencies);
    BLI_assert(!clear_dependencies || !(add_dependencies || duplicate_dependencies));
    UNUSED_VARS_NDEBUG(add_dependencies);

    if (clear_dependencies) {
      if (cb_data->cb_flag & IDWALK_CB_NEVER_NULL) {
        CLOG_WARN(&LOG_PARTIALWRITE,
                  "Clearing a 'never null' ID usage of '%s' by '%s', this is likely not a "
                  "desired action",
                  (*id_ptr)->name,
                  cb_data->owner_id->name);
      }
      /* Owner ID should be a 'context-main' duplicate of a real Main ID, as such there should be
       * no need to decrease ID usages refcount here. */
      *id_ptr = nullptr;
      return IDWALK_RET_NOP;
    }
    /* else if (add_dependencies) */
    /* The given ID may have already been added (either explicitly or as a dependency) before. */
    ID *ctx_deps_id = nullptr;
    if (duplicate_dependencies) {
      ctx_deps_id = local_ctx_id_map.lookup(orig_deps_id);
    }
    else {
      ctx_deps_id = BKE_main_idmap_lookup_uid(matching_uid_map_, orig_deps_id->session_uid);
    }
    if (!ctx_deps_id) {
      if (cb_data->cb_flag & IDWALK_CB_LOOPBACK) {
        /* Do not follow 'loop back' pointers. */
        /* NOTE: Not sure whether this should be considered an error or not. Typically hitting such
         * a case is bad practice. On the other hand, some of these pointers are present in
         * 'normal' IDs, like e.g. the parent collections ones. This implies that currently, all
         * attempt to adding a collection to a partial write context should make usage of a custom
         * `dependencies_filter_cb` function to explicitly clear these pointers. */
        CLOG_ERROR(&LOG_PARTIALWRITE,
                   "First dependency to ID '%s' found through a 'loopback' usage from ID '%s', "
                   "this should never happen",
                   (*id_ptr)->name,
                   cb_data->owner_id->name);
        *id_ptr = nullptr;
        return IDWALK_RET_NOP;
      }
      ctx_deps_id = this->id_add_copy(orig_deps_id, duplicate_dependencies);
      local_ctx_id_map.add(orig_deps_id, ctx_deps_id);
      if (!ctx_deps_id) {
        CLOG_ERROR(&LOG_PARTIALWRITE,
                   "Failed to copy ID '%s' (used by ID '%s'), could not add it to the partial "
                   "write context",
                   (*id_ptr)->name,
                   cb_data->owner_id->name);
        *id_ptr = nullptr;
        return IDWALK_RET_NOP;
      }
      ids_to_process.add(ctx_deps_id);
      post_process_ids_todo.append({ctx_deps_id, operations_final});
    }
    if (duplicate_dependencies) {
      BLI_assert(ctx_deps_id->session_uid != orig_deps_id->session_uid);
    }
    else {
      BLI_assert(ctx_deps_id->session_uid == orig_deps_id->session_uid);
    }
    this->process_added_id(ctx_deps_id, operations_final);
    /* In-place remapping. */
    *id_ptr = ctx_deps_id;
    return IDWALK_RET_NOP;
  };
  while (!ids_to_process.is_empty()) {
    ID *ctx_id = ids_to_process.pop();
    BKE_library_foreach_ID_link(
        &this->bmain, ctx_id, dependencies_cb, &options, IDWALK_DO_INTERNAL_RUNTIME_POINTERS);
  }

  /* Post process all newly added IDs in the context:
   *   - Make them local or ensure that their library reference is also in the context.
   */
  for (auto [ctx_id, options_final] : post_process_ids_todo) {
    const bool do_make_local = (options_final & MAKE_LOCAL) != 0;
    if (do_make_local) {
      this->make_local(ctx_id, make_local_flags);
    }
  }

  return ctx_root_id;
}

ID *PartialWriteContext::id_create(const short id_type,
                                   const blender::StringRefNull id_name,
                                   Library *library,
                                   PartialWriteContext::IDAddOptions options)
{
  Library *ctx_library = nullptr;
  if (library) {
    ctx_library = this->ensure_library(library->runtime->filepath_abs);
  }
  ID *ctx_id = static_cast<ID *>(
      BKE_id_new_in_lib(&this->bmain, ctx_library, id_type, id_name.c_str()));
  ctx_id->tag |= ID_TAG_TEMP_MAIN;
  id_us_min(ctx_id);
  this->process_added_id(ctx_id, options.operations);
  /* See function doc about why handling of #matching_uid_map_ can be skipped here. */
  BKE_main_idmap_insert_id(this->bmain.id_map, ctx_id);
  return ctx_id;
}

void PartialWriteContext::id_delete(const ID *id)
{
  if (ID *ctx_id = BKE_main_idmap_lookup_uid(matching_uid_map_, id->session_uid)) {
    BKE_main_idmap_remove_id(matching_uid_map_, ctx_id);
    BKE_id_delete(&this->bmain, ctx_id);
  }
}

void PartialWriteContext::remove_unused(const bool clear_extra_user)
{
  LibQueryUnusedIDsData parameters;
  parameters.do_local_ids = true;
  parameters.do_linked_ids = true;
  parameters.do_recursive = true;

  if (clear_extra_user) {
    ID *id_iter;
    FOREACH_MAIN_ID_BEGIN (&this->bmain, id_iter) {
      id_us_clear_real(id_iter);
    }
    FOREACH_MAIN_ID_END;
  }
  BKE_lib_query_unused_ids_tag(&this->bmain, ID_TAG_DOIT, parameters);

  CLOG_DEBUG(&LOG_PARTIALWRITE,
             "Removing %d unused IDs from current partial write context",
             parameters.num_total[INDEX_ID_NULL]);
  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (&this->bmain, id_iter) {
    if ((id_iter->tag & ID_TAG_DOIT) != 0) {
      BKE_main_idmap_remove_id(matching_uid_map_, id_iter);
    }
  }
  FOREACH_MAIN_ID_END;
  BKE_id_multi_tagged_delete(&this->bmain);
}

void PartialWriteContext::clear()
{
  BKE_main_idmap_clear(*matching_uid_map_);
  BKE_main_clear(this->bmain);
}

bool PartialWriteContext::is_valid()
{
  blender::Set<ID *> ids_in_context;
  blender::Set<uint> session_uids_in_context;
  bool is_valid = true;

  ID *id_iter;

  /* Fill `ids_in_context`, check uniqueness of session_uid's. */
  FOREACH_MAIN_ID_BEGIN (&this->bmain, id_iter) {
    ids_in_context.add(id_iter);
    if (session_uids_in_context.contains(id_iter->session_uid)) {
      CLOG_ERROR(&LOG_PARTIALWRITE, "ID %s does not have a unique session_uid", id_iter->name);
      is_valid = false;
    }
    else {
      session_uids_in_context.add(id_iter->session_uid);
    }
  }
  FOREACH_MAIN_ID_END;

  /* Check that no ID uses IDs from outside this context. */
  auto id_validate_dependencies_cb = [&ids_in_context,
                                      &is_valid](LibraryIDLinkCallbackData *cb_data) -> int {
    ID **id_p = cb_data->id_pointer;
    ID *owner_id = cb_data->owner_id;
    ID *self_id = cb_data->self_id;

    /* By definition, embedded IDs are not in Main, so they are not listed in this context either.
     */
    if (cb_data->cb_flag & (IDWALK_CB_EMBEDDED | IDWALK_CB_EMBEDDED_NOT_OWNING)) {
      return IDWALK_RET_NOP;
    }

    if (*id_p && !ids_in_context.contains(*id_p)) {
      if (owner_id != self_id) {
        CLOG_ERROR(
            &LOG_PARTIALWRITE,
            "ID %s (used by ID '%s', embedded ID '%s') is not in current partial write context",
            (*id_p)->name,
            owner_id->name,
            self_id->name);
      }
      else {
        CLOG_ERROR(&LOG_PARTIALWRITE,
                   "ID %s (used by ID '%s') is not in current partial write context",
                   (*id_p)->name,
                   owner_id->name);
      }
      is_valid = false;
    }
    return IDWALK_RET_NOP;
  };
  FOREACH_MAIN_ID_BEGIN (&this->bmain, id_iter) {
    BKE_library_foreach_ID_link(
        &this->bmain, id_iter, id_validate_dependencies_cb, nullptr, IDWALK_READONLY);
  }
  FOREACH_MAIN_ID_END;

  return is_valid;
}

bool PartialWriteContext::write(const char *write_filepath,
                                const int write_flags,
                                const int remap_mode,
                                ReportList &reports)
{
  BLI_assert_msg(write_filepath != reference_root_filepath_,
                 "A library blendfile should not overwrite currently edited blendfile");

  /* In case the write path is the same as one of the libraries used by this context, make this
   * library local, and delete it (and all of its potentially remaining linked data). */
  blender::Vector<Library *> make_local_libs;
  LISTBASE_FOREACH (Library *, library, &this->bmain.libraries) {
    if (STREQ(write_filepath, library->runtime->filepath_abs)) {
      make_local_libs.append(library);
    }
  }
  /* Will likely change in the near future (embedded linked IDs, virtual libraries...), but
   * currently this should never happen. */
  if (make_local_libs.size() > 1) {
    CLOG_WARN(&LOG_PARTIALWRITE,
              "%d libraries found using the same filepath as destination one ('%s'), should "
              "never happen.",
              int32_t(make_local_libs.size()),
              write_filepath);
  }
  for (Library *lib : make_local_libs) {
    BKE_library_make_local(&this->bmain, lib, nullptr, false, false, false);
    BKE_id_delete(&this->bmain, lib);
  }
  make_local_libs.clear();

  BLI_assert(this->is_valid());

  BlendFileWriteParams blend_file_write_params{};
  blend_file_write_params.remap_mode = eBLO_WritePathRemap(remap_mode);
  return BLO_write_file(
      &this->bmain, write_filepath, write_flags, &blend_file_write_params, &reports);
}

bool PartialWriteContext::write(const char *write_filepath, ReportList &reports)
{
  return this->write(write_filepath, 0, BLO_WRITE_PATH_REMAP_RELATIVE, reports);
}

}  // namespace blender::bke::blendfile

/** \} */
