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
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_BLENDER_COPYBUFFER_H__
#define __BKE_BLENDER_COPYBUFFER_H__

/** \file BKE_blender_copybuffer.h
 *  \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct bContext;
struct ReportList;
struct Main;
struct ID;

/* copybuffer (wrapper for BKE_blendfile_write_partial) */
void BKE_copybuffer_begin(struct Main *bmain_src);
void BKE_copybuffer_tag_ID(struct ID *id);
bool BKE_copybuffer_save(struct Main *bmain_src, const char *filename, struct ReportList *reports);
bool BKE_copybuffer_read(struct Main *bmain_dst, const char *libname, struct ReportList *reports);
bool BKE_copybuffer_paste(struct bContext *C, const char *libname, const short flag, struct ReportList *reports);

#ifdef __cplusplus
}
#endif

#endif  /* __BKE_BLENDER_COPYBUFFER_H__ */
