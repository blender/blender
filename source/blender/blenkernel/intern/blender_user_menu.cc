/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * User defined menu API.
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_userdef_types.h"

#include "BKE_blender_user_menu.hh"
#include "BKE_idprop.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Menu Type
 * \{ */

bUserMenu *BKE_blender_user_menu_find(ListBaseT<bUserMenu> *lb,
                                      char space_type,
                                      const char *context)
{
  for (bUserMenu &um : *lb) {
    if ((space_type == um.space_type) && STREQ(context, um.context)) {
      return &um;
    }
  }
  return nullptr;
}

bUserMenu *BKE_blender_user_menu_ensure(ListBaseT<bUserMenu> *lb,
                                        char space_type,
                                        const char *context)
{
  bUserMenu *um = BKE_blender_user_menu_find(lb, space_type, context);
  if (um == nullptr) {
    um = MEM_new_for_free<bUserMenu>(__func__);
    um->space_type = space_type;
    STRNCPY(um->context, context);
    BLI_addhead(lb, um);
  }
  return um;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Menu Item
 * \{ */

bUserMenuItem *BKE_blender_user_menu_item_add(ListBaseT<bUserMenuItem> *lb, int type)
{
  bUserMenuItem *umi;

  if (type == USER_MENU_TYPE_SEP) {
    umi = MEM_new_for_free<bUserMenuItem>(__func__);
  }
  else if (type == USER_MENU_TYPE_OPERATOR) {
    umi = reinterpret_cast<bUserMenuItem *>(MEM_new_for_free<bUserMenuItem_Op>(__func__));
  }
  else if (type == USER_MENU_TYPE_MENU) {
    umi = reinterpret_cast<bUserMenuItem *>(MEM_new_for_free<bUserMenuItem_Menu>(__func__));
  }
  else if (type == USER_MENU_TYPE_PROP) {
    umi = reinterpret_cast<bUserMenuItem *>(MEM_new_for_free<bUserMenuItem_Prop>(__func__));
  }
  else {
    umi = MEM_new_for_free<bUserMenuItem>(__func__);
    BLI_assert(0);
  }

  umi->type = type;
  BLI_addtail(lb, umi);
  return umi;
}

void BKE_blender_user_menu_item_free(bUserMenuItem *umi)
{
  if (umi->type == USER_MENU_TYPE_OPERATOR) {
    bUserMenuItem_Op *umi_op = reinterpret_cast<bUserMenuItem_Op *>(umi);
    if (umi_op->prop) {
      IDP_FreeProperty(umi_op->prop);
    }
  }
  MEM_freeN(umi);
}

void BKE_blender_user_menu_item_free_list(ListBaseT<bUserMenuItem> *lb)
{
  for (bUserMenuItem *umi = static_cast<bUserMenuItem *>(lb->first), *umi_next; umi;
       umi = umi_next)
  {
    umi_next = umi->next;
    BKE_blender_user_menu_item_free(umi);
  }
  BLI_listbase_clear(lb);
}

/** \} */

}  // namespace blender
