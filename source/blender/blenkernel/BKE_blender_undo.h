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
#ifndef __BKE_BLENDER_UNDO_H__
#define __BKE_BLENDER_UNDO_H__

/** \file BKE_blender_undo.h
 *  \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct bContext;
struct Scene;
struct Main;

#define BKE_UNDO_STR_MAX 64

/* global undo */
extern void          BKE_undo_write(struct bContext *C, const char *name);
extern void          BKE_undo_step(struct bContext *C, int step);
extern void          BKE_undo_name(struct bContext *C, const char *name);
extern bool          BKE_undo_is_valid(const char *name);
extern void          BKE_undo_reset(void);
extern void          BKE_undo_number(struct bContext *C, int nr);
extern const char   *BKE_undo_get_name(int nr, bool *r_active);
extern const char   *BKE_undo_get_name_last(void);
extern bool          BKE_undo_save_file(const char *filename);
extern struct Main  *BKE_undo_get_main(struct Scene **r_scene);

extern void          BKE_undo_callback_wm_kill_jobs_set(void (*callback)(struct bContext *C));

#ifdef __cplusplus
}
#endif

#endif  /* __BKE_BLENDER_UNDO_H__ */
