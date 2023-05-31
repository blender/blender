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
#include "BKE_ipo.h"
#include "BKE_keyconfig.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_override.h"
#include "BKE_main.h"
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
                                  const char *path_src)
{
  strcpy(path_dst, path_src);
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

    /* Security issue: any blend file could include a USER block.
     *
     * Currently we load prefs from BLENDER_STARTUP_FILE and later on load BLENDER_USERPREF_FILE,
     * to load the preferences defined in the users home dir.
     *
     * This means we will never accidentally (or maliciously)
     * enable scripts auto-execution by loading a '.blend' file.
     */
    U.flag |= USER_SCRIPT_AUTOEXEC_DISABLE;
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
                           const struct BlendFileReadParams *params,
                           BlendFileReadReport *reports)
{
  Main *bmain = G_MAIN;
  Scene *curscene = nullptr;
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

  /* The following code blocks performs complex window-manager matching. */

  /* no load screens? */
  if (mode != LOAD_UI) {
    /* Logic for 'track_undo_scene' is to keep using the scene which the active screen has,
     * as long as the scene associated with the undo operation is visible
     * in one of the open windows.
     *
     * - 'curscreen->scene' - scene the user is currently looking at.
     * - 'bfd->curscene' - scene undo-step was created in.
     *
     * This means users can have 2+ windows open and undo in both without screens switching.
     * But if they close one of the screens,
     * undo will ensure that the scene being operated on will be activated
     * (otherwise we'd be undoing on an off-screen scene which isn't acceptable).
     * see: #43424
     */
    wmWindow *win;
    bScreen *curscreen = nullptr;
    ViewLayer *cur_view_layer;
    bool track_undo_scene;

    /* comes from readfile.c */
    SWAP(ListBase, bmain->wm, bfd->main->wm);
    SWAP(ListBase, bmain->workspaces, bfd->main->workspaces);
    SWAP(ListBase, bmain->screens, bfd->main->screens);
    /* NOTE: UI IDs are assumed to be only local data-blocks, so no need to call
     * #BKE_main_namemap_clear here (otherwise, the swapping would fail in many funny ways). */
    if (bmain->name_map != nullptr) {
      BKE_main_namemap_destroy(&bmain->name_map);
    }
    if (bfd->main->name_map != nullptr) {
      BKE_main_namemap_destroy(&bfd->main->name_map);
    }

    /* In case of actual new file reading without loading UI, we need to regenerate the session
     * uuid of the UI-related datablocks we are keeping from previous session, otherwise their uuid
     * will collide with some generated for newly read data. */
    if (mode != LOAD_UNDO) {
      ID *id;
      FOREACH_MAIN_LISTBASE_ID_BEGIN (&bfd->main->wm, id) {
        BKE_lib_libblock_session_uuid_renew(id);
      }
      FOREACH_MAIN_LISTBASE_ID_END;

      FOREACH_MAIN_LISTBASE_ID_BEGIN (&bfd->main->workspaces, id) {
        BKE_lib_libblock_session_uuid_renew(id);
      }
      FOREACH_MAIN_LISTBASE_ID_END;

      FOREACH_MAIN_LISTBASE_ID_BEGIN (&bfd->main->screens, id) {
        BKE_lib_libblock_session_uuid_renew(id);
      }
      FOREACH_MAIN_LISTBASE_ID_END;
    }

    /* we re-use current window and screen */
    win = CTX_wm_window(C);
    curscreen = CTX_wm_screen(C);
    /* but use Scene pointer from new file */
    curscene = bfd->curscene;
    cur_view_layer = bfd->cur_view_layer;

    track_undo_scene = (mode == LOAD_UNDO && curscreen && curscene && bfd->main->wm.first);

    if (curscene == nullptr) {
      curscene = static_cast<Scene *>(bfd->main->scenes.first);
    }
    /* empty file, we add a scene to make Blender work */
    if (curscene == nullptr) {
      curscene = BKE_scene_add(bfd->main, "Empty");
    }
    if (cur_view_layer == nullptr) {
      /* fallback to scene layer */
      cur_view_layer = BKE_view_layer_default_view(curscene);
    }

    if (track_undo_scene) {
      /* keep the old (free'd) scene, let 'blo_lib_link_screen_restore'
       * replace it with 'curscene' if its needed */
    }
    /* and we enforce curscene to be in current screen */
    else if (win) { /* The window may be nullptr in background-mode. */
      win->scene = curscene;
    }

    /* BKE_blender_globals_clear will free G_MAIN, here we can still restore pointers */
    blo_lib_link_restore(bmain, bfd->main, CTX_wm_manager(C), curscene, cur_view_layer);
    if (win) {
      curscene = win->scene;
    }

    if (track_undo_scene) {
      wmWindowManager *wm = static_cast<wmWindowManager *>(bfd->main->wm.first);
      if (wm_scene_is_visible(wm, bfd->curscene) == false) {
        curscene = bfd->curscene;
        win->scene = curscene;
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

  BKE_blender_globals_main_replace(bfd->main);
  bmain = G_MAIN;
  bfd->main = nullptr;

  CTX_data_main_set(C, bmain);

  /* case G_FILE_NO_UI or no screens in file */
  if (mode != LOAD_UI) {
    /* leave entire context further unaltered? */
    CTX_data_scene_set(C, curscene);
  }
  else {
    CTX_wm_manager_set(C, static_cast<wmWindowManager *>(bmain->wm.first));
    CTX_wm_screen_set(C, bfd->curscreen);
    CTX_data_scene_set(C, bfd->curscene);
    CTX_wm_area_set(C, nullptr);
    CTX_wm_region_set(C, nullptr);
    CTX_wm_menu_set(C, nullptr);
    curscene = bfd->curscene;
  }

  /* Keep state from preferences. */
  const int fileflags_keep = G_FILE_FLAG_ALL_RUNTIME;
  G.fileflags = (G.fileflags & fileflags_keep) | (bfd->fileflags & ~fileflags_keep);

  /* this can happen when active scene was lib-linked, and doesn't exist anymore */
  if (CTX_data_scene(C) == nullptr) {
    wmWindow *win = CTX_wm_window(C);

    /* in case we don't even have a local scene, add one */
    if (!bmain->scenes.first) {
      BKE_scene_add(bmain, "Empty");
    }

    CTX_data_scene_set(C, static_cast<Scene *>(bmain->scenes.first));
    win->scene = CTX_data_scene(C);
    curscene = CTX_data_scene(C);
  }

  BLI_assert(curscene == CTX_data_scene(C));

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
  /* NOTE: Check bmain version (i.e. current blend file version), AND the versions of all the
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
    /* In case of autosave or quit.blend, use original filepath instead. */
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

  if (mode == LOAD_UNDO) {
    /* In undo/redo case, we do a whole lot of magic tricks to avoid having to re-read linked
     * data-blocks from libraries (since those are not supposed to change). Unfortunately, that
     * means that we do not reset their user count, however we do increase that one when doing
     * lib_link on local IDs using linked ones.
     * There is no real way to predict amount of changes here, so we have to fully redo
     * reference-counting.
     * Now that we re-use (and do not liblink in readfile.c) most local data-blocks as well,
     * we have to recompute reference-counts for all local IDs too. */
    BKE_main_id_refcount_recompute(bmain, false);
  }

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
                                      const struct BlendFileReadParams *params,
                                      BlendFileReadReport *reports)
{
  if ((params->skip_flags & BLO_READ_SKIP_USERDEF) == 0) {
    setup_app_userdef(bfd);
  }
  if ((params->skip_flags & BLO_READ_SKIP_DATA) == 0) {
    setup_app_data(C, bfd, params, reports);
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

void BKE_blendfile_read_setup_ex(bContext *C,
                                 BlendFileData *bfd,
                                 const struct BlendFileReadParams *params,
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
  setup_app_blend_file_data(C, bfd, params, reports);
  BLO_blendfiledata_free(bfd);
}

void BKE_blendfile_read_setup(bContext *C,
                              BlendFileData *bfd,
                              const struct BlendFileReadParams *params,
                              BlendFileReadReport *reports)
{
  BKE_blendfile_read_setup_ex(C, bfd, params, reports, false, nullptr);
}

struct BlendFileData *BKE_blendfile_read(const char *filepath,
                                         const struct BlendFileReadParams *params,
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

struct BlendFileData *BKE_blendfile_read_from_memory(const void *filebuf,
                                                     int filelength,
                                                     const struct BlendFileReadParams *params,
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

struct BlendFileData *BKE_blendfile_read_from_memfile(Main *bmain,
                                                      struct MemFile *memfile,
                                                      const struct BlendFileReadParams *params,
                                                      ReportList *reports)
{
  BlendFileData *bfd = BLO_read_from_memfile(
      bmain, BKE_main_blendfile_path(bmain), memfile, params, reports);
  if (bfd && bfd->main->is_read_invalid) {
    BLO_blendfiledata_free(bfd);
    bfd = nullptr;
  }
  if (bfd) {
    /* Removing the unused workspaces, screens and wm is useless here, setup_app_data will switch
     * those lists with the ones from old bmain, which freeing is much more efficient than
     * individual calls to `BKE_id_free()`.
     * Further more, those are expected to be empty anyway with new memfile reading code. */
    BLI_assert(BLI_listbase_is_empty(&bfd->main->wm));
    BLI_assert(BLI_listbase_is_empty(&bfd->main->workspaces));
    BLI_assert(BLI_listbase_is_empty(&bfd->main->screens));
  }
  else {
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

UserDef *BKE_blendfile_userdef_from_defaults(void)
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

  /* System-specific fonts directory. */
  BKE_appdir_font_folder_default(userdef->fontdir);

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

void BKE_blendfile_write_partial_begin(Main *bmain_src)
{
  BKE_main_id_tag_all(bmain_src, LIB_TAG_NEED_EXPAND | LIB_TAG_DOIT, false);
}

void BKE_blendfile_write_partial_tag_ID(ID *id, bool set)
{
  if (set) {
    id->tag |= LIB_TAG_NEED_EXPAND | LIB_TAG_DOIT;
  }
  else {
    id->tag &= ~(LIB_TAG_NEED_EXPAND | LIB_TAG_DOIT);
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
  BKE_main_id_tag_all(bmain_src, LIB_TAG_NEED_EXPAND | LIB_TAG_DOIT, false);
}

/** \} */
