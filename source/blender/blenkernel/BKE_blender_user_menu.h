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

#ifndef __BKE_BLENDER_USER_MENU_H__
#define __BKE_BLENDER_USER_MENU_H__

/** \file BKE_blender_user_menu.h
 *  \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct ListBase;
struct bUserMenu;
struct bUserMenuItem;

struct bUserMenu *BKE_blender_user_menu_find(
        struct ListBase *lb, char space_type, const char *context);
struct bUserMenu *BKE_blender_user_menu_ensure(
        struct ListBase *lb, char space_type, const char *context);

struct bUserMenuItem *BKE_blender_user_menu_item_add(struct ListBase *lb, int type);
void BKE_blender_user_menu_item_free(struct bUserMenuItem *umi);
void BKE_blender_user_menu_item_free_list(struct ListBase *lb);

#ifdef __cplusplus
}
#endif

#endif  /* __BKE_BLENDER_USER_MENU_H__ */
