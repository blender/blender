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
#include "BKE_idprop.h"

/* -------------------------------------------------------------------- */
/** \name Menu Type
 * \{ */

bUserMenu *BKE_blender_user_menu_find(ListBase *lb, char space_type, const char *context)
{
  LISTBASE_FOREACH (bUserMenu *, um, lb) {
    if ((space_type == um->space_type) && STREQ(context, um->context)) {
      return um;
    }
  }
  return nullptr;
}

bUserMenu *BKE_blender_user_menu_ensure(ListBase *lb, char space_type, const char *context)
{
  bUserMenu *um = BKE_blender_user_menu_find(lb, space_type, context);
  if (um == nullptr) {
    um = static_cast<bUserMenu *>(MEM_callocN(sizeof(bUserMenu), __func__));
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

bUserMenuItem *BKE_blender_user_menu_item_add(ListBase *lb, int type)
{
  uint size;

  if (type == USER_MENU_TYPE_SEP) {
    size = sizeof(bUserMenuItem);
  }
  else if (type == USER_MENU_TYPE_OPERATOR) {
    size = sizeof(bUserMenuItem_Op);
  }
  else if (type == USER_MENU_TYPE_MENU) {
    size = sizeof(bUserMenuItem_Menu);
  }
  else if (type == USER_MENU_TYPE_PROP) {
    size = sizeof(bUserMenuItem_Prop);
  }
  else {
    size = sizeof(bUserMenuItem);
    BLI_assert(0);
  }

  bUserMenuItem *umi = static_cast<bUserMenuItem *>(MEM_callocN(size, __func__));
  umi->type = type;
  BLI_addtail(lb, umi);
  return umi;
}

void BKE_blender_user_menu_item_free(bUserMenuItem *umi)
{
  if (umi->type == USER_MENU_TYPE_OPERATOR) {
    bUserMenuItem_Op *umi_op = (bUserMenuItem_Op *)umi;
    if (umi_op->prop) {
      IDP_FreeProperty(umi_op->prop);
    }
  }
  MEM_freeN(umi);
}

void BKE_blender_user_menu_item_free_list(ListBase *lb)
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
