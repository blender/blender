/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * High level `.blend` file read/write,
 * and functions for writing *partial* files (only selected data-blocks).
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_workspace_types.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_system.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "IMB_colormanagement.h"

#include "BKE_addon.h"
#include "BKE_appdir.h"
#include "BKE_blender.h"
#include "BKE_blender_version.h"
#include "BKE_blendfile.h"
#include "BKE_bpath.h"
#include "BKE_colorband.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_idtype.h"
#include "BKE_ipo.h"
#include "BKE_keyconfig.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_override.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_main_idmap.h"
#include "BKE_main_namemap.h"
#include "BKE_preferences.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_studiolight.h"
#include "BKE_undo_system.h"
#include "BKE_workspace.h"

#include "BLO_readfile.h"
#include "BLO_writefile.h"

#include "RNA_access.h"

#include "RE_pipeline.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blend File IO (High Level)
 * \{ */

static bool blendfile_or_libraries_versions_atleast(Main *bmain,
                                                    const short versionfile,
                                                    const short subversionfile)
{
  if (!MAIN_VERSION_ATLEAST(bmain, versionfile, subversionfile)) {
    return false;
  }

  LISTBASE_FOREACH (Library *, library, &bmain->libraries) {
    if (!MAIN_VERSION_ATLEAST(library, versionfile, subversionfile)) {
      return false;
    }
  }

  return true;
}

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
  wmWindow *win;
  for (win = static_cast<wmWindow *>(wm->windows.first); win; win = win->next) {
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

  /** Data generated and used by calling WM code to handle keeping WM and UI IDs as best as
   * possible across file reading.
   *
   * \note: May be null in undo (memfile) case.. */
  BlendFileReadWMSetupData *wm_setup_data;

  /** Storage for all remapping rules (old_id -> new_id) required by the preservation of old IDs
   * into the new Main. */
  IDRemapper *remapper;
  bool is_libraries_remapped;

  /** Used to find matching IDs by name/lib in new main, to remap ID usages of data ported over
   * from old main. */
  IDNameLib_Map *id_map;
};

/** Search for all libraries in `old_bmain` that are also in `new_bmain` (i.e. different Library
 * IDs having the same absolute filepath), and create a remapping rule for these.
 *
 * NOTE: The case where the `old_bmain` would be a library in the newly read one is not handled
 * here, as it does not create explicit issues. The local data from `old_bmain` is either
 * discarded, or added to the `new_bmain` as local data as well. Worst case, there will be a
 * double of a linked data as a local one, without any known relationships between them. In
 * practice, this latter case is not expected to commonly happen.
 */
static IDRemapper *reuse_bmain_data_remapper_ensure(ReuseOldBMainData *reuse_data)
{
  if (reuse_data->is_libraries_remapped) {
    return reuse_data->remapper;
  }

  if (reuse_data->remapper == nullptr) {
    reuse_data->remapper = BKE_id_remapper_create();
  }

  Main *new_bmain = reuse_data->new_bmain;
  Main *old_bmain = reuse_data->old_bmain;
  IDRemapper *remapper = reuse_data->remapper;

  LISTBASE_FOREACH (Library *, old_lib_iter, &old_bmain->libraries) {
    /* In case newly opened `new_bmain` is a library of the `old_bmain`, remap it to NULL, since a
     * file should never ever have linked data from itself. */
    if (STREQ(old_lib_iter->filepath_abs, new_bmain->filepath)) {
      BKE_id_remapper_add(remapper, &old_lib_iter->id, nullptr);
      continue;
    }

    /* NOTE: Although this is quadratic complexity, it is not expected to be an issue in practice:
     *  - Files using more than a few tens of libraries are extremely rare.
     *  - This code is only executed once for every file reading (not on undos).
     */
    LISTBASE_FOREACH (Library *, new_lib_iter, &new_bmain->libraries) {
      if (!STREQ(old_lib_iter->filepath_abs, new_lib_iter->filepath_abs)) {
        continue;
      }

      BKE_id_remapper_add(remapper, &old_lib_iter->id, &new_lib_iter->id);
      break;
    }
  }

  reuse_data->is_libraries_remapped = true;
  return reuse_data->remapper;
}

static bool reuse_bmain_data_remapper_is_id_remapped(IDRemapper *remapper, ID *id)
{
  IDRemapperApplyResult result = BKE_id_remapper_get_mapping_result(
      remapper, id, ID_REMAP_APPLY_DEFAULT, nullptr);
  if (ELEM(result, ID_REMAP_RESULT_SOURCE_REMAPPED, ID_REMAP_RESULT_SOURCE_UNASSIGNED)) {
    /* ID is already remapped to its matching ID in the new main, or explicitly remapped to NULL,
     * nothing else to do here. */
    return true;
  }
  BLI_assert_msg(result != ID_REMAP_RESULT_SOURCE_NOT_MAPPABLE,
                 "There should never be a non-mappable (i.e. NULL) input here.");
  BLI_assert(result == ID_REMAP_RESULT_SOURCE_UNAVAILABLE);
  return false;
}

/** Does a complete replacement of data in `new_bmain` by data from `old_bmain. Original new data
 * are moved to the `old_bmain`, and will be freed together with it.
 *
 * WARNING: Currently only expects to work on local data, won't work properly if some of the IDs of
 * given type are linked.
 *
 * NOTE: There is no support at all for potential dependencies of the IDs moved around. This is not
 * expected to be necessary for the current use cases (UI-related IDs). */
static void swap_old_bmain_data_for_blendfile(ReuseOldBMainData *reuse_data, const short id_code)
{
  Main *new_bmain = reuse_data->new_bmain;
  Main *old_bmain = reuse_data->old_bmain;

  ListBase *new_lb = which_libbase(new_bmain, id_code);
  ListBase *old_lb = which_libbase(old_bmain, id_code);

  IDRemapper *remapper = reuse_bmain_data_remapper_ensure(reuse_data);

  /* NOTE: Full swapping is only supported for ID types that are assumed to be only local
   * data-blocks (like UI-like ones). Otherwise, the swapping could fail in many funny ways. */
  BLI_assert(BLI_listbase_is_empty(old_lb) || !ID_IS_LINKED(old_lb->last));
  BLI_assert(BLI_listbase_is_empty(new_lb) || !ID_IS_LINKED(new_lb->last));

  SWAP(ListBase, *new_lb, *old_lb);

  /* Since all IDs here are supposed to be local, no need to call #BKE_main_namemap_clear. */
  /* TODO: Could add per-IDType control over namemaps clearing, if this becomes a performances
   * concern. */
  if (old_bmain->name_map != nullptr) {
    BKE_main_namemap_destroy(&old_bmain->name_map);
  }
  if (new_bmain->name_map != nullptr) {
    BKE_main_namemap_destroy(&new_bmain->name_map);
  }

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
      BKE_id_remapper_add(remapper, discarded_id_iter, reused_id_iter);

      discarded_id_iter = static_cast<ID *>(discarded_id_iter->next);
      reused_id_iter = static_cast<ID *>(reused_id_iter->next);
    }
    else if (strcmp_result < 0) {
      /* No matching reused 'old' ID for this discarded 'new' one. */
      BKE_id_remapper_add(remapper, discarded_id_iter, nullptr);

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
    BKE_id_remapper_add(remapper, discarded_id_iter, nullptr);
  }

  FOREACH_MAIN_LISTBASE_ID_BEGIN (new_lb, reused_id_iter) {
    /* Necessary as all `session_uuid` are renewed on blendfile loading. */
    BKE_lib_libblock_session_uuid_renew(reused_id_iter);

    /* Ensure that the reused ID is remapped to itself, since it is known to be in the `new_bmain`.
     */
    BKE_id_remapper_add_overwrite(remapper, reused_id_iter, reused_id_iter);
  }
  FOREACH_MAIN_LISTBASE_ID_END;
}

/** Similar to #swap_old_bmain_data_for_blendfile, but with special handling for WM ID. Tightly
 * related to further WM post-processing from calling WM code (see #WM_file_read and
 * #wm_homefile_read_ex). */
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

    IDRemapper *remapper = reuse_bmain_data_remapper_ensure(reuse_data);
    BKE_id_remapper_add(remapper, &old_wm->id, &new_wm->id);
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
  IDRemapper *remapper = reuse_bmain_data_remapper_ensure(reuse_data);
  if (reuse_bmain_data_remapper_is_id_remapped(remapper, id)) {
    return IDWALK_RET_NOP;
  }

  IDNameLib_Map *id_map = reuse_data->id_map;
  BLI_assert(id_map != nullptr);

  ID *id_new = BKE_main_idmap_lookup_id(id_map, id);
  BKE_id_remapper_add(remapper, id, id_new);

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

/** Detect and fix invalid usages of locale IDs by linked ones (or as reference of liboverrides).
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
        new_bmain, id_iter, reuse_bmain_data_invalid_local_usages_fix_cb, reuse_data, 0);

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
        if (base->local_view_bits & v3d->local_view_uuid) {
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
      v3d->local_view_uuid = 0;

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
      STRNCPY(win->view_layer_name, cur_view_layer->name);
    }

    view3d_data_consistency_ensure(win, win->scene, cur_view_layer);
  }
}

/**
 * Context matching, handle no-UI case.
 *
 * \note this is called on Undo so any slow conversion functions here
 * should be avoided or check (mode != LOAD_UNDO).
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
  const bool is_startup = params->is_startup;
  enum {
    LOAD_UI = 1,
    LOAD_UI_OFF,
    LOAD_UNDO,
  } mode;

  if (params->undo_direction != STEP_INVALID) {
    BLI_assert(bfd->curscene != nullptr);
    mode = LOAD_UNDO;
  }
  /* may happen with library files - UNDO file should never have nullptr curscene (but may have a
   * nullptr curscreen)... */
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

  /* Free all render results, without this stale data gets displayed after loading files */
  if (mode != LOAD_UNDO) {
    RE_FreeAllRenderResults();
  }

  /* Only make filepaths compatible when loading for real (not undo) */
  if (mode != LOAD_UNDO) {
    clean_paths(bfd->main);
  }

  BLI_assert(BKE_main_namemap_validate(bfd->main));

  /* Temp data to handle swapping around IDs between old and new mains, and accumulate the
   * required remapping accordingly. */
  ReuseOldBMainData reuse_data = {nullptr};
  reuse_data.new_bmain = bfd->main;
  reuse_data.old_bmain = bmain;
  reuse_data.wm_setup_data = wm_setup_data;

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
  }

  /* Logic for 'track_undo_scene' is to keep using the scene which the active screen has, as long
   * as the scene associated with the undo operation is visible in one of the open windows.
   *
   * - 'curscreen->scene' - scene the user is currently looking at.
   * - 'bfd->curscene' - scene undo-step was created in.
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

  /* If UI is not loaded when opening actual .blend file, and always in case of undo memfile
   * reading. */
  if (mode != LOAD_UI) {
    /* Re-use current window and screen. */
    win = CTX_wm_window(C);
    curscreen = CTX_wm_screen(C);

    track_undo_scene = (mode == LOAD_UNDO && curscreen && curscene && bfd->main->wm.first);

    if (track_undo_scene) {
      /* Keep the old (to-be-freed) scene, remapping below will ensure it's remapped to the
       * matching new scene if available, or NULL otherwise, in which case
       * #wm_data_consistency_ensure will define `curscene` as the active one. */
    }
    /* Enforce curscene to be in current screen. */
    else if (win) { /* The window may be nullptr in background-mode. */
      win->scene = curscene;
    }
  }

  BLI_assert(BKE_main_namemap_validate(bfd->main));

  /* Apply remapping of ID pointers caused by re-using part of the data from the 'old' main into
   * the new one. */
  if (reuse_data.remapper != nullptr) {
    /* In undo case all 'keeping old data' and remapping logic is now handled in readfile code
     * itself, so there should never be any remapping to do here. */
    BLI_assert(mode != LOAD_UNDO);

    /* Handle all pending remapping from swapping old and new IDs around. */
    BKE_libblock_remap_multiple_raw(bfd->main,
                                    reuse_data.remapper,
                                    (ID_REMAP_FORCE_UI_POINTERS | ID_REMAP_SKIP_USER_REFCOUNT |
                                     ID_REMAP_SKIP_UPDATE_TAGGING | ID_REMAP_SKIP_USER_CLEAR));

    /* Fix potential invalid usages of now-locale-data created by remapping above. Should never
     * be needed in undo case, this is to address cases like 'opening a blendfile that was a
     * library of the previous opened blendfile'. */
    reuse_bmain_data_invalid_local_usages_fix(&reuse_data);

    BKE_id_remapper_free(reuse_data.remapper);
    reuse_data.remapper = nullptr;

    wm_data_consistency_ensure(CTX_wm_manager(C), curscene, cur_view_layer);
  }

  BLI_assert(BKE_main_namemap_validate(bfd->main));

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

  BLI_assert(BKE_main_namemap_validate(bfd->main));

  /* This frees the `old_bmain`. */
  BKE_blender_globals_main_replace(bfd->main);
  bmain = G_MAIN;
  bfd->main = nullptr;
  CTX_data_main_set(C, bmain);

  BLI_assert(BKE_main_namemap_validate(bmain));

  /* These context data should remain valid if old UI is being re-used. */
  if (mode == LOAD_UI) {
    /* Setting WindowManager in context clears all other Context UI data (window, area, etc.). So
     * only do it when effectively loading a new WM, otherwise just assert that the WM from context
     * is still the same as in `new_bmain`. */
    CTX_wm_manager_set(C, static_cast<wmWindowManager *>(bmain->wm.first));
    CTX_wm_screen_set(C, bfd->curscreen);
    CTX_wm_area_set(C, nullptr);
    CTX_wm_region_set(C, nullptr);
    CTX_wm_menu_set(C, nullptr);
  }
  BLI_assert(CTX_wm_manager(C) == static_cast<wmWindowManager *>(bmain->wm.first));

  /* Keep state from preferences. */
  const int fileflags_keep = G_FILE_FLAG_ALL_RUNTIME;
  G.fileflags = (G.fileflags & fileflags_keep) | (bfd->fileflags & ~fileflags_keep);

  /* special cases, override loaded flags: */
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

  /* FIXME: this version patching should really be part of the file-reading code,
   * but we still get too many unrelated data-corruption crashes otherwise... */
  if (bmain->versionfile < 250) {
    do_versions_ipos_to_animato(bmain);
  }

  /* NOTE: readfile's `do_versions` does not allow to create new IDs, and only operates on a single
   * library at a time. This code needs to operate on the whole Main at once. */
  /* NOTE: Check Main version (i.e. current blend file version), AND the versions of all the
   * linked libraries. */
  if (mode != LOAD_UNDO && !blendfile_or_libraries_versions_atleast(bmain, 302, 1)) {
    BKE_lib_override_library_main_proxy_convert(bmain, reports);
    /* Currently liboverride code can generate invalid namemap. This is a known issue, requires
     * #107847 to be properly fixed. */
    BKE_main_namemap_validate_and_fix(bmain);
  }

  if (mode != LOAD_UNDO && !blendfile_or_libraries_versions_atleast(bmain, 302, 3)) {
    BKE_lib_override_library_main_hierarchy_root_ensure(bmain);
  }

  bmain->recovered = false;

  /* startup.blend or recovered startup */
  if (is_startup) {
    bmain->filepath[0] = '\0';
  }
  else if (recover) {
    /* In case of auto-save or quit.blend, use original filepath instead. */
    bmain->recovered = true;
    STRNCPY(bmain->filepath, bfd->filepath);
  }

  /* Base-flags, groups, make depsgraph, etc. */
  /* first handle case if other windows have different scenes visible */
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

  /* Setting scene might require having a dependency graph, with copy on write
   * we need to make sure we ensure scene has correct color management before
   * constructing dependency graph.
   */
  if (mode != LOAD_UNDO) {
    IMB_colormanagement_check_file_config(bmain);
  }

  BKE_scene_set_background(bmain, curscene);

  if (mode != LOAD_UNDO) {
    /* TODO(sergey): Can this be also move above? */
    RE_FreeAllPersistentData();
  }

  /* Both undo and regular file loading can perform some fairly complex ID manipulation, simpler
   * and safer to fully redo reference-counting. This is a relatively cheap process anyway. */
  BKE_main_id_refcount_recompute(bmain, false);

  BLI_assert(BKE_main_namemap_validate(bmain));

  if (mode != LOAD_UNDO && !USER_EXPERIMENTAL_TEST(&U, no_override_auto_resync)) {
    reports->duration.lib_overrides_resync = PIL_check_seconds_timer();

    BKE_lib_override_library_main_resync(
        bmain,
        curscene,
        bfd->cur_view_layer ? bfd->cur_view_layer : BKE_view_layer_default_view(curscene),
        reports);

    reports->duration.lib_overrides_resync = PIL_check_seconds_timer() -
                                             reports->duration.lib_overrides_resync;

    /* We need to rebuild some of the deleted override rules (for UI feedback purpose). */
    BKE_lib_override_library_main_operations_create(bmain, true, nullptr);
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
  if (main->minversionfile > BLENDER_FILE_VERSION ||
      (main->minversionfile == BLENDER_FILE_VERSION &&
       main->minsubversionfile > BLENDER_FILE_SUBVERSION))
  {
    BKE_reportf(reports->reports,
                RPT_WARNING,
                "File written by newer Blender binary (%d.%d), expect loss of data!",
                main->minversionfile,
                main->minsubversionfile);
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

BlendFileData *BKE_blendfile_read(const char *filepath,
                                  const BlendFileReadParams *params,
                                  BlendFileReadReport *reports)
{
  /* Don't print startup file loading. */
  if (params->is_startup == false) {
    printf("Read blend: \"%s\"\n", filepath);
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

BlendFileData *BKE_blendfile_read_from_memory(const void *filebuf,
                                              int filelength,
                                              const BlendFileReadParams *params,
                                              ReportList *reports)
{
  BlendFileData *bfd = BLO_read_from_memory(
      filebuf, filelength, eBLOReadSkip(params->skip_flags), reports);
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
 *   undo-memory uses the regular preferences (for e.g.).
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
    MEM_freeN(bfd);
  }

  return userdef;
}

UserDef *BKE_blendfile_userdef_read_from_memory(const void *filebuf,
                                                int filelength,
                                                ReportList *reports)
{
  BlendFileData *bfd;
  UserDef *userdef = nullptr;

  bfd = BLO_read_from_memory(
      filebuf, filelength, BLO_READ_SKIP_ALL & ~BLO_READ_SKIP_USERDEF, reports);
  if (bfd) {
    if (bfd->user) {
      userdef = bfd->user;
    }
    BKE_main_free(bfd->main);
    MEM_freeN(bfd);
  }
  else {
    BKE_reports_prepend(reports, "Loading failed: ");
  }

  return userdef;
}

UserDef *BKE_blendfile_userdef_from_defaults()
{
  UserDef *userdef = static_cast<UserDef *>(MEM_callocN(sizeof(UserDef), __func__));
  *userdef = blender::dna::shallow_copy(U_default);

  /* Add-ons. */
  {
    const char *addons[] = {
        "io_anim_bvh",
        "io_curve_svg",
        "io_mesh_stl",
        "io_mesh_uv_layout",
        "io_scene_fbx",
        "io_scene_gltf2",
        "io_scene_x3d",
        "cycles",
        "pose_library",
    };
    for (int i = 0; i < ARRAY_SIZE(addons); i++) {
      bAddon *addon = BKE_addon_new();
      STRNCPY(addon->module, addons[i]);
      BLI_addtail(&userdef->addons, addon);
    }
  }

  /* Theme. */
  {
    bTheme *btheme = static_cast<bTheme *>(MEM_mallocN(sizeof(*btheme), __func__));
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

  return userdef;
}

bool BKE_blendfile_userdef_write(const char *filepath, ReportList *reports)
{
  Main *mainb = MEM_cnew<Main>("empty main");
  bool ok = false;

  BlendFileWriteParams params{};
  params.use_userdef = true;

  if (BLO_write_file(mainb, filepath, 0, &params, reports)) {
    ok = true;
  }

  MEM_freeN(mainb);

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
  const char *cfgdir;
  bool ok = true;
  const bool use_template_userpref = BKE_appdir_app_template_has_userpref(U.app_template);

  if ((cfgdir = BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, nullptr))) {
    bool ok_write;
    BLI_path_join(filepath, sizeof(filepath), cfgdir, BLENDER_USERPREF_FILE);

    printf("Writing userprefs: \"%s\" ", filepath);
    if (use_template_userpref) {
      ok_write = BKE_blendfile_userdef_write_app_template(filepath, reports);
    }
    else {
      ok_write = BKE_blendfile_userdef_write(filepath, reports);
    }

    if (ok_write) {
      printf("ok\n");
      BKE_report(reports, RPT_INFO, "Preferences saved");
    }
    else {
      printf("fail\n");
      ok = false;
      BKE_report(reports, RPT_ERROR, "Saving preferences failed");
    }
  }
  else {
    BKE_report(reports, RPT_ERROR, "Unable to create userpref path");
  }

  if (use_template_userpref) {
    if ((cfgdir = BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, U.app_template))) {
      /* Also save app-template prefs */
      BLI_path_join(filepath, sizeof(filepath), cfgdir, BLENDER_USERPREF_FILE);

      printf("Writing userprefs app-template: \"%s\" ", filepath);
      if (BKE_blendfile_userdef_write(filepath, reports) != 0) {
        printf("ok\n");
      }
      else {
        printf("fail\n");
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
                                                             const void *filebuf,
                                                             int filelength,
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
    bfd = BLO_read_from_memory(filebuf, filelength, BLO_READ_SKIP_USERDEF, reports);
  }

  if (bfd) {
    workspace_config = MEM_cnew<WorkspaceConfigFileData>(__func__);
    workspace_config->main = bfd->main;

    /* Only 2.80+ files have actual workspaces, don't try to use screens
     * from older versions. */
    if (bfd->main->versionfile >= 280) {
      workspace_config->workspaces = bfd->main->workspaces;
    }

    MEM_freeN(bfd);
  }

  return workspace_config;
}

bool BKE_blendfile_workspace_config_write(Main *bmain, const char *filepath, ReportList *reports)
{
  const int fileflags = G.fileflags & ~G_FILE_NO_UI;
  bool retval = false;

  BKE_blendfile_write_partial_begin(bmain);

  for (WorkSpace *workspace = static_cast<WorkSpace *>(bmain->workspaces.first); workspace;
       workspace = static_cast<WorkSpace *>(workspace->id.next))
  {
    BKE_blendfile_write_partial_tag_ID(&workspace->id, true);
  }

  if (BKE_blendfile_write_partial(bmain, filepath, fileflags, BLO_WRITE_PATH_REMAP_NONE, reports))
  {
    retval = true;
  }

  BKE_blendfile_write_partial_end(bmain);

  return retval;
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

static void blendfile_write_partial_clear_flags(Main *bmain_src)
{
  ListBase *lbarray[INDEX_ID_MAX];
  int a = set_listbasepointers(bmain_src, lbarray);
  while (a--) {
    LISTBASE_FOREACH (ID *, id, lbarray[a]) {
      id->tag &= ~(LIB_TAG_NEED_EXPAND | LIB_TAG_DOIT);
      id->flag &= ~(LIB_CLIPBOARD_MARK);
    }
  }
}

void BKE_blendfile_write_partial_begin(Main *bmain)
{
  blendfile_write_partial_clear_flags(bmain);
}

void BKE_blendfile_write_partial_tag_ID(ID *id, bool set)
{
  if (set) {
    id->tag |= LIB_TAG_NEED_EXPAND | LIB_TAG_DOIT;
    id->flag |= LIB_CLIPBOARD_MARK;
  }
  else {
    id->tag &= ~(LIB_TAG_NEED_EXPAND | LIB_TAG_DOIT);
    id->flag &= ~LIB_CLIPBOARD_MARK;
  }
}

static void blendfile_write_partial_cb(void * /*handle*/, Main * /*bmain*/, void *vid)
{
  if (vid) {
    ID *id = static_cast<ID *>(vid);
    /* only tag for need-expand if not done, prevents eternal loops */
    if ((id->tag & LIB_TAG_DOIT) == 0) {
      id->tag |= LIB_TAG_NEED_EXPAND | LIB_TAG_DOIT;
    }

    if (id->lib && (id->lib->id.tag & LIB_TAG_DOIT) == 0) {
      id->lib->id.tag |= LIB_TAG_DOIT;
    }
  }
}

bool BKE_blendfile_write_partial(Main *bmain_src,
                                 const char *filepath,
                                 const int write_flags,
                                 const int remap_mode,
                                 ReportList *reports)
{
  Main *bmain_dst = MEM_cnew<Main>("copybuffer");
  ListBase *lbarray_dst[INDEX_ID_MAX], *lbarray_src[INDEX_ID_MAX];
  int a, retval;

  void *path_list_backup = nullptr;
  const eBPathForeachFlag path_list_flag = (BKE_BPATH_FOREACH_PATH_SKIP_LINKED |
                                            BKE_BPATH_FOREACH_PATH_SKIP_MULTIFILE);

  /* This is needed to be able to load that file as a real one later
   * (otherwise `main->filepath` will not be set at read time). */
  STRNCPY(bmain_dst->filepath, bmain_src->filepath);

  BLO_main_expander(blendfile_write_partial_cb);
  BLO_expand_main(nullptr, bmain_src);

  /* move over all tagged blocks */
  set_listbasepointers(bmain_src, lbarray_src);
  a = set_listbasepointers(bmain_dst, lbarray_dst);
  while (a--) {
    ID *id, *nextid;
    ListBase *lb_dst = lbarray_dst[a], *lb_src = lbarray_src[a];

    for (id = static_cast<ID *>(lb_src->first); id; id = nextid) {
      nextid = static_cast<ID *>(id->next);
      if (id->tag & LIB_TAG_DOIT) {
        BLI_remlink(lb_src, id);
        BLI_addtail(lb_dst, id);
      }
    }
  }

  /* Backup paths because remap relative will overwrite them.
   *
   * NOTE: we do this only on the list of data-blocks that we are writing
   * because the restored full list is not guaranteed to be in the same
   * order as before, as expected by BKE_bpath_list_restore.
   *
   * This happens because id_sort_by_name does not take into account
   * string case or the library name, so the order is not strictly
   * defined for two linked data-blocks with the same name! */
  if (remap_mode != BLO_WRITE_PATH_REMAP_NONE) {
    path_list_backup = BKE_bpath_list_backup(bmain_dst, path_list_flag);
  }

  /* save the buffer */
  BlendFileWriteParams blend_file_write_params{};
  blend_file_write_params.remap_mode = eBLO_WritePathRemap(remap_mode);
  retval = BLO_write_file(bmain_dst, filepath, write_flags, &blend_file_write_params, reports);

  if (path_list_backup) {
    BKE_bpath_list_restore(bmain_dst, path_list_flag, path_list_backup);
    BKE_bpath_list_free(path_list_backup);
  }

  /* move back the main, now sorted again */
  set_listbasepointers(bmain_src, lbarray_dst);
  a = set_listbasepointers(bmain_dst, lbarray_src);
  while (a--) {
    ID *id;
    ListBase *lb_dst = lbarray_dst[a], *lb_src = lbarray_src[a];

    while ((id = static_cast<ID *>(BLI_pophead(lb_src)))) {
      BLI_addtail(lb_dst, id);
      id_sort_by_name(lb_dst, id, nullptr);
    }
  }

  MEM_freeN(bmain_dst);

  return retval;
}

void BKE_blendfile_write_partial_end(Main *bmain_src)
{
  blendfile_write_partial_clear_flags(bmain_src);
}

/** \} */
