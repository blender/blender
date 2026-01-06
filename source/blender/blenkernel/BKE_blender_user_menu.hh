/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "DNA_listBase.h"

namespace blender {

struct bUserMenu;
struct bUserMenuItem;

bUserMenu *BKE_blender_user_menu_find(ListBaseT<bUserMenu> *lb,
                                      char space_type,
                                      const char *context);
bUserMenu *BKE_blender_user_menu_ensure(ListBaseT<bUserMenu> *lb,
                                        char space_type,
                                        const char *context);

bUserMenuItem *BKE_blender_user_menu_item_add(ListBaseT<bUserMenuItem> *lb, int type);
void BKE_blender_user_menu_item_free(bUserMenuItem *umi);
void BKE_blender_user_menu_item_free_list(ListBaseT<bUserMenuItem> *lb);

}  // namespace blender
