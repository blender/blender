/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

struct ListBase;
struct bUserMenu;
struct bUserMenuItem;

bUserMenu *BKE_blender_user_menu_find(ListBase *lb, char space_type, const char *context);
bUserMenu *BKE_blender_user_menu_ensure(ListBase *lb, char space_type, const char *context);

bUserMenuItem *BKE_blender_user_menu_item_add(ListBase *lb, int type);
void BKE_blender_user_menu_item_free(bUserMenuItem *umi);
void BKE_blender_user_menu_item_free_list(ListBase *lb);
