/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#pragma once

#include "WM_types.hh"

struct bContext;
struct Main;
struct ReportList;
struct wmFileReadPost_Params;
struct wmGenericCallback;
struct wmOperator;
struct wmOperatorType;
struct wmWindow;
struct wmWindowManager;

/* `wm_files.cc`. */

void wm_history_file_read();

struct wmHomeFileRead_Params {
  /** Load data, disable when only loading user preferences. */
  unsigned int use_data : 1;
  /** Load factory settings as well as startup file (disabled for "File New"). */
  unsigned int use_userdef : 1;

  /**
   * Ignore on-disk startup file, use bundled `datatoc_startup_blend` instead.
   * Used for "Restore Factory Settings".
   */
  unsigned int use_factory_settings : 1;
  /** Read factory settings from the app-templates only (keep other defaults). */
  unsigned int use_factory_settings_app_template_only : 1;
  /**
   * Load the startup file without any data-blocks.
   * Useful for automated content generation, so the file starts without data.
   */
  unsigned int use_empty_data : 1;
  /**
   * When true, this is the first time the home file is read.
   * In this case resetting the previous state can be skipped.
   */
  unsigned int is_first_time : 1;
  /**
   * Optional path pointing to an alternative blend file (may be NULL).
   */
  const char *filepath_startup_override;
  /**
   * Template to use instead of the template defined in user-preferences.
   * When not-null, this is written into the user preferences.
   */
  const char *app_template_override;
};

/**
 * Called on startup, (context entirely filled with NULLs)
 * or called for 'New File' both `startup.blend` and `userpref.blend` are checked.
 *
 * \param r_params_file_read_post: Support postponed initialization,
 * needed for initial startup when only some sub-systems have been initialized.
 * When non-null, #wm_file_read_post doesn't run, instead it's arguments are stored
 * in this return argument.
 * The caller is responsible for calling #wm_homefile_read_post with this return argument.
 */
void wm_homefile_read_ex(bContext *C,
                         const wmHomeFileRead_Params *params_homefile,
                         ReportList *reports,
                         wmFileReadPost_Params **r_params_file_read_post);
void wm_homefile_read(bContext *C,
                      const wmHomeFileRead_Params *params_homefile,
                      ReportList *reports);

/**
 * Special case, support deferred execution of #wm_file_read_post,
 * Needed when loading for the first time to workaround order of initialization bug, see #89046.
 */
void wm_homefile_read_post(bContext *C, const wmFileReadPost_Params *params_file_read_post);

void wm_file_read_report(Main *bmain, wmWindow *win);

void wm_close_file_dialog(bContext *C, wmGenericCallback *post_action);
/**
 * \return True if the dialog was created, the calling operator should return #OPERATOR_INTERFACE
 *         then.
 */
bool wm_operator_close_file_dialog_if_needed(bContext *C,
                                             wmOperator *op,
                                             wmGenericCallbackFn post_action_fn);
/**
 * Check if there is data that would be lost when closing the current file without saving.
 */
bool wm_file_or_session_data_has_unsaved_changes(const Main *bmain, const wmWindowManager *wm);
/**
 * Confirmation dialog when user is about to save the current blend file, and it was previously
 * created by a newer version of Blender.
 *
 * Important to ask confirmation, as this is a very common scenario of data loss.
 */
void wm_save_file_overwrite_dialog(bContext *C, wmOperator *op);

void WM_OT_save_homefile(wmOperatorType *ot);
void WM_OT_save_userpref(wmOperatorType *ot);
void WM_OT_read_userpref(wmOperatorType *ot);
void WM_OT_read_factory_userpref(wmOperatorType *ot);
void WM_OT_read_history(wmOperatorType *ot);
void WM_OT_read_homefile(wmOperatorType *ot);
void WM_OT_read_factory_settings(wmOperatorType *ot);

void WM_OT_open_mainfile(wmOperatorType *ot);

void WM_OT_revert_mainfile(wmOperatorType *ot);
void WM_OT_recover_last_session(wmOperatorType *ot);
void WM_OT_recover_auto_save(wmOperatorType *ot);

void WM_OT_save_as_mainfile(wmOperatorType *ot);
void WM_OT_save_mainfile(wmOperatorType *ot);

void WM_OT_clear_recent_files(wmOperatorType *ot);

/* `wm_files_link.cc` */

void WM_OT_link(wmOperatorType *ot);
void WM_OT_append(wmOperatorType *ot);
void WM_OT_id_linked_relocate(wmOperatorType *ot);

void WM_OT_lib_relocate(wmOperatorType *ot);
void WM_OT_lib_reload(wmOperatorType *ot);

/* `wm_files_colorspace.cc` */

void WM_OT_set_working_color_space(wmOperatorType *ot);
