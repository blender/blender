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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spview3d
 */

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "BKE_blender_user_menu.h"
#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_idprop.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

/* -------------------------------------------------------------------- */
/** \name Menu Type
 * \{ */

bUserMenu **ED_screen_user_menus_find(const bContext *C, uint *r_len)
{
  SpaceLink *sl = CTX_wm_space_data(C);
  const char *context = CTX_data_mode_string(C);

  if (sl == NULL) {
    *r_len = 0;
    return NULL;
  }

  uint array_len = 3;
  bUserMenu **um_array = MEM_calloc_arrayN(array_len, sizeof(*um_array), __func__);
  um_array[0] = BKE_blender_user_menu_find(&U.user_menus, sl->spacetype, context);
  um_array[1] = (sl->spacetype != SPACE_TOPBAR) ?
                    BKE_blender_user_menu_find(&U.user_menus, SPACE_TOPBAR, context) :
                    NULL;
  um_array[2] = (sl->spacetype == SPACE_VIEW3D) ?
                    BKE_blender_user_menu_find(&U.user_menus, SPACE_PROPERTIES, context) :
                    NULL;

  *r_len = array_len;
  return um_array;
}

bUserMenu *ED_screen_user_menu_ensure(bContext *C)
{
  SpaceLink *sl = CTX_wm_space_data(C);
  const char *context = CTX_data_mode_string(C);
  return BKE_blender_user_menu_ensure(&U.user_menus, sl->spacetype, context);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Menu Item
 * \{ */

bUserMenuItem_Op *ED_screen_user_menu_item_find_operator(ListBase *lb,
                                                         const wmOperatorType *ot,
                                                         IDProperty *prop,
                                                         short opcontext)
{
  for (bUserMenuItem *umi = lb->first; umi; umi = umi->next) {
    if (umi->type == USER_MENU_TYPE_OPERATOR) {
      bUserMenuItem_Op *umi_op = (bUserMenuItem_Op *)umi;
      if (STREQ(ot->idname, umi_op->op_idname) && (opcontext == umi_op->opcontext) &&
          (IDP_EqualsProperties(prop, umi_op->prop))) {
        return umi_op;
      }
    }
  }
  return NULL;
}

struct bUserMenuItem_Menu *ED_screen_user_menu_item_find_menu(struct ListBase *lb,
                                                              const struct MenuType *mt)
{
  for (bUserMenuItem *umi = lb->first; umi; umi = umi->next) {
    if (umi->type == USER_MENU_TYPE_MENU) {
      bUserMenuItem_Menu *umi_mt = (bUserMenuItem_Menu *)umi;
      if (STREQ(mt->idname, umi_mt->mt_idname)) {
        return umi_mt;
      }
    }
  }
  return NULL;
}

struct bUserMenuItem_Prop *ED_screen_user_menu_item_find_prop(struct ListBase *lb,
                                                              const char *context_data_path,
                                                              const char *prop_id,
                                                              int prop_index)
{
  for (bUserMenuItem *umi = lb->first; umi; umi = umi->next) {
    if (umi->type == USER_MENU_TYPE_PROP) {
      bUserMenuItem_Prop *umi_pr = (bUserMenuItem_Prop *)umi;
      if (STREQ(context_data_path, umi_pr->context_data_path) && STREQ(prop_id, umi_pr->prop_id) &&
          (prop_index == umi_pr->prop_index)) {
        return umi_pr;
      }
    }
  }
  return NULL;
}

void ED_screen_user_menu_item_add_operator(ListBase *lb,
                                           const char *ui_name,
                                           const wmOperatorType *ot,
                                           const IDProperty *prop,
                                           short opcontext)
{
  bUserMenuItem_Op *umi_op = (bUserMenuItem_Op *)BKE_blender_user_menu_item_add(
      lb, USER_MENU_TYPE_OPERATOR);
  umi_op->opcontext = opcontext;
  if (!STREQ(ui_name, ot->name)) {
    STRNCPY(umi_op->item.ui_name, ui_name);
  }
  STRNCPY(umi_op->op_idname, ot->idname);
  umi_op->prop = prop ? IDP_CopyProperty(prop) : NULL;
}

void ED_screen_user_menu_item_add_menu(ListBase *lb, const char *ui_name, const MenuType *mt)
{
  bUserMenuItem_Menu *umi_mt = (bUserMenuItem_Menu *)BKE_blender_user_menu_item_add(
      lb, USER_MENU_TYPE_MENU);
  if (!STREQ(ui_name, mt->label)) {
    STRNCPY(umi_mt->item.ui_name, ui_name);
  }
  STRNCPY(umi_mt->mt_idname, mt->idname);
}

void ED_screen_user_menu_item_add_prop(ListBase *lb,
                                       const char *ui_name,
                                       const char *context_data_path,
                                       const char *prop_id,
                                       int prop_index)
{
  bUserMenuItem_Prop *umi_pr = (bUserMenuItem_Prop *)BKE_blender_user_menu_item_add(
      lb, USER_MENU_TYPE_PROP);
  STRNCPY(umi_pr->item.ui_name, ui_name);
  STRNCPY(umi_pr->context_data_path, context_data_path);
  STRNCPY(umi_pr->prop_id, prop_id);
  umi_pr->prop_index = prop_index;
}

void ED_screen_user_menu_item_remove(ListBase *lb, bUserMenuItem *umi)
{
  BLI_remlink(lb, umi);
  BKE_blender_user_menu_item_free(umi);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Menu Definition
 * \{ */

static void screen_user_menu_draw(const bContext *C, Menu *menu)
{
  /* Enable when we have the ability to edit menus. */
  const bool show_missing = false;
  char label[512];

  uint um_array_len;
  bUserMenu **um_array = ED_screen_user_menus_find(C, &um_array_len);
  bool is_empty = true;
  for (int um_index = 0; um_index < um_array_len; um_index++) {
    bUserMenu *um = um_array[um_index];
    if (um == NULL) {
      continue;
    }
    for (bUserMenuItem *umi = um->items.first; umi; umi = umi->next) {
      const char *ui_name = umi->ui_name[0] ? umi->ui_name : NULL;
      if (umi->type == USER_MENU_TYPE_OPERATOR) {
        bUserMenuItem_Op *umi_op = (bUserMenuItem_Op *)umi;
        wmOperatorType *ot = WM_operatortype_find(umi_op->op_idname, false);
        if (ot != NULL) {
          IDProperty *prop = umi_op->prop ? IDP_CopyProperty(umi_op->prop) : NULL;
          uiItemFullO_ptr(menu->layout, ot, ui_name, ICON_NONE, prop, umi_op->opcontext, 0, NULL);
          is_empty = false;
        }
        else {
          if (show_missing) {
            SNPRINTF(label, "Missing: %s", umi_op->op_idname);
            uiItemL(menu->layout, label, ICON_NONE);
          }
        }
      }
      else if (umi->type == USER_MENU_TYPE_MENU) {
        bUserMenuItem_Menu *umi_mt = (bUserMenuItem_Menu *)umi;
        MenuType *mt = WM_menutype_find(umi_mt->mt_idname, false);
        if (mt != NULL) {
          uiItemM_ptr(menu->layout, mt, ui_name, ICON_NONE);
          is_empty = false;
        }
        else {
          if (show_missing) {
            SNPRINTF(label, "Missing: %s", umi_mt->mt_idname);
            uiItemL(menu->layout, label, ICON_NONE);
          }
        }
      }
      else if (umi->type == USER_MENU_TYPE_PROP) {
        bUserMenuItem_Prop *umi_pr = (bUserMenuItem_Prop *)umi;

        char *data_path = strchr(umi_pr->context_data_path, '.');
        if (data_path) {
          *data_path = '\0';
        }
        PointerRNA ptr = CTX_data_pointer_get(C, umi_pr->context_data_path);
        if (ptr.type == NULL) {
          PointerRNA ctx_ptr;
          RNA_pointer_create(NULL, &RNA_Context, (void *)C, &ctx_ptr);
          if (!RNA_path_resolve_full(&ctx_ptr, umi_pr->context_data_path, &ptr, NULL, NULL)) {
            ptr.type = NULL;
          }
        }
        if (data_path) {
          *data_path = '.';
          data_path += 1;
        }

        bool ok = false;
        if (ptr.type != NULL) {
          PropertyRNA *prop = NULL;
          PointerRNA prop_ptr = ptr;
          if ((data_path == NULL) ||
              RNA_path_resolve_full(&ptr, data_path, &prop_ptr, NULL, NULL)) {
            prop = RNA_struct_find_property(&prop_ptr, umi_pr->prop_id);
            if (prop) {
              ok = true;
              uiItemFullR(
                  menu->layout, &prop_ptr, prop, umi_pr->prop_index, 0, 0, ui_name, ICON_NONE);
              is_empty = false;
            }
          }
        }
        if (!ok) {
          if (show_missing) {
            SNPRINTF(label, "Missing: %s.%s", umi_pr->context_data_path, umi_pr->prop_id);
            uiItemL(menu->layout, label, ICON_NONE);
          }
        }
      }
      else if (umi->type == USER_MENU_TYPE_SEP) {
        uiItemS(menu->layout);
      }
    }
  }
  if (um_array) {
    MEM_freeN(um_array);
  }

  if (is_empty) {
    uiItemL(menu->layout, IFACE_("No menu items found"), ICON_NONE);
    uiItemL(menu->layout, IFACE_("Right click on buttons to add them to this menu"), ICON_NONE);
  }
}

void ED_screen_user_menu_register(void)
{
  MenuType *mt = MEM_callocN(sizeof(MenuType), __func__);
  strcpy(mt->idname, "SCREEN_MT_user_menu");
  strcpy(mt->label, "Quick Favorites");
  strcpy(mt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  mt->draw = screen_user_menu_draw;
  WM_menutype_add(mt);
}

/** \} */
