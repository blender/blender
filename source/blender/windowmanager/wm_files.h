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

void		wm_read_history(void);
int			wm_file_write(struct bContext *C, const char *target, int fileflags, struct ReportList *reports);
int			wm_history_read_exec(bContext *C, wmOperator *op);
int			wm_homefile_read_exec(struct bContext *C, struct wmOperator *op);
int			wm_homefile_read(struct bContext *C, struct ReportList *reports, bool from_memory, const char *filepath);
int			wm_homefile_write_exec(struct bContext *C, struct wmOperator *op);
int			wm_userpref_write_exec(struct bContext *C, struct wmOperator *op);


#endif /* __WM_FILES_H__ */

