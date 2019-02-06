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
 */
#ifndef __BKE_BLENDER_UNDO_H__
#define __BKE_BLENDER_UNDO_H__

/** \file \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Main;
struct MemFileUndoData;
struct Scene;
struct bContext;

#define BKE_UNDO_STR_MAX 64

struct MemFileUndoData *BKE_memfile_undo_encode(struct Main *bmain, struct MemFileUndoData *mfu_prev);
bool                    BKE_memfile_undo_decode(struct MemFileUndoData *mfu, struct bContext *C);
void                    BKE_memfile_undo_free(struct MemFileUndoData *mfu);

#ifdef __cplusplus
}
#endif

#endif  /* __BKE_BLENDER_UNDO_H__ */
