/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup wm
 */

#pragma once

struct Main;
struct wmFileReadPost_Params;
struct wmGenericCallback;
struct wmOperatorType;

#ifdef __cplusplus
extern "C" {
#endif

/* wm_files.c */
void wm_history_file_read(void);

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
  /**
   * Load the startup file without any data-blocks.
   * Useful for automated content generation, so the file starts without data.
   */
  unsigned int use_empty_data : 1;
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

void wm_homefile_read_ex(struct bContext *C,
                         const struct wmHomeFileRead_Params *params_homefile,
                         struct ReportList *reports,
                         struct wmFileReadPost_Params **r_params_file_read_post);
void wm_homefile_read(struct bContext *C,
                      const struct wmHomeFileRead_Params *params_homefile,
                      struct ReportList *reports);

void wm_homefile_read_post(struct bContext *C,
                           const struct wmFileReadPost_Params *params_file_read_post);

void wm_file_read_report(bContext *C, struct Main *bmain);

void wm_close_file_dialog(bContext *C, struct wmGenericCallback *post_action);
bool wm_operator_close_file_dialog_if_needed(bContext *C,
                                             wmOperator *op,
                                             wmGenericCallbackFn exec_fn);
bool wm_file_or_session_data_has_unsaved_changes(const Main *bmain, const wmWindowManager *wm);

void WM_OT_save_homefile(struct wmOperatorType *ot);
void WM_OT_save_userpref(struct wmOperatorType *ot);
void WM_OT_read_userpref(struct wmOperatorType *ot);
void WM_OT_read_factory_userpref(struct wmOperatorType *ot);
void WM_OT_read_history(struct wmOperatorType *ot);
void WM_OT_read_homefile(struct wmOperatorType *ot);
void WM_OT_read_factory_settings(struct wmOperatorType *ot);

void WM_OT_open_mainfile(struct wmOperatorType *ot);

void WM_OT_revert_mainfile(struct wmOperatorType *ot);
void WM_OT_recover_last_session(struct wmOperatorType *ot);
void WM_OT_recover_auto_save(struct wmOperatorType *ot);

void WM_OT_save_as_mainfile(struct wmOperatorType *ot);
void WM_OT_save_mainfile(struct wmOperatorType *ot);

/* wm_files_link.c */
void WM_OT_link(struct wmOperatorType *ot);
void WM_OT_append(struct wmOperatorType *ot);

void WM_OT_lib_relocate(struct wmOperatorType *ot);
void WM_OT_lib_reload(struct wmOperatorType *ot);

#ifdef __cplusplus
}
#endif
