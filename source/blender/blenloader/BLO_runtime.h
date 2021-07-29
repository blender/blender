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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * 
 */

#ifndef __BLO_RUNTIME_H__
#define __BLO_RUNTIME_H__

/** \file BLO_runtime.h
 *  \ingroup blenloader
 */

#ifdef __cplusplus
extern "C" {
#endif

struct BlendFileData;
struct ReportList;

int BLO_is_a_runtime(const char *file);
struct BlendFileData *BLO_read_runtime(const char *file, struct ReportList *reports);

#ifdef __cplusplus
}
#endif

#endif /* __BLO_RUNTIME_H__ */

