/**
 * $Id$
 *
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_INFO_INTERN_H
#define ED_INFO_INTERN_H

/* internal exports only */

struct wmOperatorType;

void FILE_OT_pack_all(struct wmOperatorType *ot);
void FILE_OT_unpack_all(struct wmOperatorType *ot);
void FILE_OT_make_paths_relative(struct wmOperatorType *ot);
void FILE_OT_make_paths_absolute(struct wmOperatorType *ot);
void FILE_OT_report_missing_files(struct wmOperatorType *ot);
void FILE_OT_find_missing_files(struct wmOperatorType *ot);

#endif /* ED_INFO_INTERN_H */

