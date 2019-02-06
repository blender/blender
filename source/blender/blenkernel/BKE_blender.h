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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */
#ifndef __BKE_BLENDER_H__
#define __BKE_BLENDER_H__

/** \file \ingroup bke
 *  \brief Blender util stuff
 */

#ifdef __cplusplus
extern "C" {
#endif

struct UserDef;

void BKE_blender_free(void);

void BKE_blender_globals_init(void);
void BKE_blender_globals_clear(void);
void BKE_blender_version_string(
        char *version_str, size_t maxncpy,
        short version, short subversion, bool v_prefix, bool include_subversion);

void BKE_blender_userdef_data_swap(struct UserDef *userdef_dst, struct UserDef *userdef_src);
void BKE_blender_userdef_data_set(struct UserDef *userdef);
void BKE_blender_userdef_data_set_and_free(struct UserDef *userdef);

void BKE_blender_userdef_app_template_data_swap(struct UserDef *userdef_dst, struct UserDef *userdef_src);
void BKE_blender_userdef_app_template_data_set(struct UserDef *userdef);
void BKE_blender_userdef_app_template_data_set_and_free(struct UserDef *userdef);

void BKE_blender_userdef_data_free(struct UserDef *userdef, bool clear_fonts);

/* set this callback when a UI is running */
void BKE_blender_callback_test_break_set(void (*func)(void));
int  BKE_blender_test_break(void);

/* Blenders' own atexit (avoids leaking) */
void BKE_blender_atexit_register(void (*func)(void *user_data), void *user_data);
void BKE_blender_atexit_unregister(void (*func)(void *user_data), const void *user_data);
void BKE_blender_atexit(void);

#ifdef __cplusplus
}
#endif

#endif  /* __BKE_BLENDER_H__ */
