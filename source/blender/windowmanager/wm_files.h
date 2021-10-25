/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/wm_files.h
 *  \ingroup wm
 */

#ifndef __WM_FILES_H__
#define __WM_FILES_H__

struct wmOperatorType;

/* wm_files.c */
void		wm_history_file_read(void);
int			wm_homefile_read(
        struct bContext *C, struct ReportList *reports,
        bool use_factory_settings, bool use_empty_data, bool use_userdef,
        const char *filepath_startup_override, const char *app_template_override);
void		wm_file_read_report(bContext *C);

void        WM_OT_save_homefile(struct wmOperatorType *ot);
void        WM_OT_userpref_autoexec_path_add(struct wmOperatorType *ot);
void        WM_OT_userpref_autoexec_path_remove(struct wmOperatorType *ot);
void        WM_OT_save_userpref(struct wmOperatorType *ot);
void        WM_OT_read_history(struct wmOperatorType *ot);
void        WM_OT_read_homefile(struct wmOperatorType *ot);
void        WM_OT_read_factory_settings(struct wmOperatorType *ot);

void        WM_OT_open_mainfile(struct wmOperatorType *ot);

void        WM_OT_revert_mainfile(struct wmOperatorType *ot);
void        WM_OT_recover_last_session(struct wmOperatorType *ot);
void        WM_OT_recover_auto_save(struct wmOperatorType *ot);

void        WM_OT_save_as_mainfile(struct wmOperatorType *ot);
void        WM_OT_save_mainfile(struct wmOperatorType *ot);

/* wm_files_link.c */
void        WM_OT_link(struct wmOperatorType *ot);
void        WM_OT_append(struct wmOperatorType *ot);

void        WM_OT_lib_relocate(struct wmOperatorType *ot);
void        WM_OT_lib_reload(struct wmOperatorType *ot);

#endif /* __WM_FILES_H__ */

