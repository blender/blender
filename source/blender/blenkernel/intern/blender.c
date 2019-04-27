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

/** \file
 * \ingroup bke
 *
 * Application level startup/shutdown functionality.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_string.h"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "BLI_callbacks.h"

#include "IMB_imbuf.h"
#include "IMB_moviecache.h"

#include "BKE_addon.h"
#include "BKE_blender.h"         /* own include */
#include "BKE_blender_version.h" /* own include */
#include "BKE_blender_user_menu.h"
#include "BKE_blendfile.h"
#include "BKE_brush.h"
#include "BKE_cachefile.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_image.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_sequencer.h"
#include "BKE_studiolight.h"

#include "DEG_depsgraph.h"

#include "RE_pipeline.h"
#include "RE_render_ext.h"

#include "BLF_api.h"

Global G;
UserDef U;

char versionstr[48] = "";

/* ********** free ********** */

/* only to be called on exit blender */
void BKE_blender_free(void)
{
  /* samples are in a global list..., also sets G_MAIN->sound->sample NULL */

  /* Needs to run before main free as wm is still referenced for icons preview jobs. */
  BKE_studiolight_free();

  BKE_main_free(G_MAIN);
  G_MAIN = NULL;

  if (G.log.file != NULL) {
    fclose(G.log.file);
  }

  BKE_spacetypes_free(); /* after free main, it uses space callbacks */

  IMB_exit();
  BKE_cachefiles_exit();
  BKE_images_exit();
  DEG_free_node_types();

  BKE_brush_system_exit();
  RE_texture_rng_exit();

  BLI_callback_global_finalize();

  BKE_sequencer_cache_destruct();
  IMB_moviecache_destruct();

  free_nodesystem();
}

void BKE_blender_version_string(char *version_str,
                                size_t maxncpy,
                                short version,
                                short subversion,
                                bool v_prefix,
                                bool include_subversion)
{
  const char *prefix = v_prefix ? "v" : "";

  if (include_subversion && subversion > 0) {
    BLI_snprintf(
        version_str, maxncpy, "%s%d.%02d.%d", prefix, version / 100, version % 100, subversion);
  }
  else {
    BLI_snprintf(version_str, maxncpy, "%s%d.%02d", prefix, version / 100, version % 100);
  }
}

void BKE_blender_globals_init(void)
{
  memset(&G, 0, sizeof(Global));

  U.savetime = 1;

  G_MAIN = BKE_main_new();

  strcpy(G.ima, "//");

  BKE_blender_version_string(
      versionstr, sizeof(versionstr), BLENDER_VERSION, BLENDER_SUBVERSION, true, true);

#ifndef WITH_PYTHON_SECURITY /* default */
  G.f |= G_FLAG_SCRIPT_AUTOEXEC;
#else
  G.f &= ~G_FLAG_SCRIPT_AUTOEXEC;
#endif

  G.log.level = 1;
}

void BKE_blender_globals_clear(void)
{
  BKE_main_free(G_MAIN); /* free all lib data */

  G_MAIN = NULL;
}

/***/

static void keymap_item_free(wmKeyMapItem *kmi)
{
  if (kmi->properties) {
    IDP_FreeProperty(kmi->properties);
    MEM_freeN(kmi->properties);
  }
  if (kmi->ptr) {
    MEM_freeN(kmi->ptr);
  }
}

void BKE_blender_userdef_data_swap(UserDef *userdef_a, UserDef *userdef_b)
{
  SWAP(UserDef, *userdef_a, *userdef_b);
}

void BKE_blender_userdef_data_set(UserDef *userdef)
{
  BKE_blender_userdef_data_swap(&U, userdef);
  BKE_blender_userdef_data_free(userdef, true);
}

void BKE_blender_userdef_data_set_and_free(UserDef *userdef)
{
  BKE_blender_userdef_data_set(userdef);
  MEM_freeN(userdef);
}

static void userdef_free_keymaps(UserDef *userdef)
{
  for (wmKeyMap *km = userdef->user_keymaps.first, *km_next; km; km = km_next) {
    km_next = km->next;
    for (wmKeyMapDiffItem *kmdi = km->diff_items.first; kmdi; kmdi = kmdi->next) {
      if (kmdi->add_item) {
        keymap_item_free(kmdi->add_item);
        MEM_freeN(kmdi->add_item);
      }
      if (kmdi->remove_item) {
        keymap_item_free(kmdi->remove_item);
        MEM_freeN(kmdi->remove_item);
      }
    }

    for (wmKeyMapItem *kmi = km->items.first; kmi; kmi = kmi->next) {
      keymap_item_free(kmi);
    }

    BLI_freelistN(&km->diff_items);
    BLI_freelistN(&km->items);

    MEM_freeN(km);
  }
  BLI_listbase_clear(&userdef->user_keymaps);
}

static void userdef_free_keyconfig_prefs(UserDef *userdef)
{
  for (wmKeyConfigPref *kpt = userdef->user_keyconfig_prefs.first, *kpt_next; kpt;
       kpt = kpt_next) {
    kpt_next = kpt->next;
    IDP_FreeProperty(kpt->prop);
    MEM_freeN(kpt->prop);
    MEM_freeN(kpt);
  }
  BLI_listbase_clear(&userdef->user_keyconfig_prefs);
}

static void userdef_free_user_menus(UserDef *userdef)
{
  for (bUserMenu *um = userdef->user_menus.first, *um_next; um; um = um_next) {
    um_next = um->next;
    BKE_blender_user_menu_item_free_list(&um->items);
    MEM_freeN(um);
  }
}

static void userdef_free_addons(UserDef *userdef)
{
  for (bAddon *addon = userdef->addons.first, *addon_next; addon; addon = addon_next) {
    addon_next = addon->next;
    BKE_addon_free(addon);
  }
  BLI_listbase_clear(&userdef->addons);
}

/**
 * When loading a new userdef from file,
 * or when exiting Blender.
 */
void BKE_blender_userdef_data_free(UserDef *userdef, bool clear_fonts)
{
#define U _invalid_access_ /* ensure no accidental global access */
#ifdef U                   /* quiet warning */
#endif

  userdef_free_keymaps(userdef);
  userdef_free_keyconfig_prefs(userdef);
  userdef_free_user_menus(userdef);
  userdef_free_addons(userdef);

  if (clear_fonts) {
    for (uiFont *font = userdef->uifonts.first; font; font = font->next) {
      BLF_unload_id(font->blf_id);
    }
    BLF_default_set(-1);
  }

  BLI_freelistN(&userdef->autoexec_paths);

  BLI_freelistN(&userdef->uistyles);
  BLI_freelistN(&userdef->uifonts);
  BLI_freelistN(&userdef->themes);

#undef U
}

/**
 * Write U from userdef.
 * This function defines which settings a template will override for the user preferences.
 */
void BKE_blender_userdef_app_template_data_swap(UserDef *userdef_a, UserDef *userdef_b)
{
  /* TODO:
   * - various minor settings (add as needed).
   */

#define DATA_SWAP(id) \
  { \
    UserDef userdef_tmp; \
    memcpy(&(userdef_tmp.id), &(userdef_a->id), sizeof(userdef_tmp.id)); \
    memcpy(&(userdef_a->id), &(userdef_b->id), sizeof(userdef_tmp.id)); \
    memcpy(&(userdef_b->id), &(userdef_tmp.id), sizeof(userdef_tmp.id)); \
  } \
  ((void)0)

#define LIST_SWAP(id) \
  { \
    SWAP(ListBase, userdef_a->id, userdef_b->id); \
  } \
  ((void)0)

#define FLAG_SWAP(id, ty, flags) \
  { \
    CHECK_TYPE(&(userdef_a->id), ty *); \
    const ty f = flags; \
    const ty a = userdef_a->id; \
    const ty b = userdef_b->id; \
    userdef_a->id = (userdef_a->id & ~f) | (b & f); \
    userdef_b->id = (userdef_b->id & ~f) | (a & f); \
  } \
  ((void)0)

  LIST_SWAP(uistyles);
  LIST_SWAP(uifonts);
  LIST_SWAP(themes);
  LIST_SWAP(addons);
  LIST_SWAP(user_keymaps);

  DATA_SWAP(font_path_ui);
  DATA_SWAP(font_path_ui_mono);
  DATA_SWAP(keyconfigstr);

  DATA_SWAP(gizmo_flag);
  DATA_SWAP(app_flag);

  /* We could add others. */
  FLAG_SWAP(uiflag, int, USER_SAVE_PROMPT);

#undef SWAP_TYPELESS
#undef DATA_SWAP
#undef LIST_SWAP
#undef FLAG_SWAP
}

void BKE_blender_userdef_app_template_data_set(UserDef *userdef)
{
  BKE_blender_userdef_app_template_data_swap(&U, userdef);
  BKE_blender_userdef_data_free(userdef, true);
}

void BKE_blender_userdef_app_template_data_set_and_free(UserDef *userdef)
{
  BKE_blender_userdef_app_template_data_set(userdef);
  MEM_freeN(userdef);
}

/** \name Blender's AtExit
 *
 * \note Don't use MEM_mallocN so functions can be registered at any time.
 * \{ */

static struct AtExitData {
  struct AtExitData *next;

  void (*func)(void *user_data);
  void *user_data;
} *g_atexit = NULL;

void BKE_blender_atexit_register(void (*func)(void *user_data), void *user_data)
{
  struct AtExitData *ae = malloc(sizeof(*ae));
  ae->next = g_atexit;
  ae->func = func;
  ae->user_data = user_data;
  g_atexit = ae;
}

void BKE_blender_atexit_unregister(void (*func)(void *user_data), const void *user_data)
{
  struct AtExitData *ae = g_atexit;
  struct AtExitData **ae_p = &g_atexit;

  while (ae) {
    if ((ae->func == func) && (ae->user_data == user_data)) {
      *ae_p = ae->next;
      free(ae);
      return;
    }
    ae_p = &ae;
    ae = ae->next;
  }
}

void BKE_blender_atexit(void)
{
  struct AtExitData *ae = g_atexit, *ae_next;
  while (ae) {
    ae_next = ae->next;

    ae->func(ae->user_data);

    free(ae);
    ae = ae_next;
  }
  g_atexit = NULL;
}

/** \} */
