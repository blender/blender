/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct ListBase;
struct bUserMenu;
struct bUserMenuItem;

struct bUserMenu *BKE_blender_user_menu_find(struct ListBase *lb,
                                             char space_type,
                                             const char *context);
struct bUserMenu *BKE_blender_user_menu_ensure(struct ListBase *lb,
                                               char space_type,
                                               const char *context);

struct bUserMenuItem *BKE_blender_user_menu_item_add(struct ListBase *lb, int type);
void BKE_blender_user_menu_item_free(struct bUserMenuItem *umi);
void BKE_blender_user_menu_item_free_list(struct ListBase *lb);

#ifdef __cplusplus
}
#endif
