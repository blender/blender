/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * UI List Registry.
 */

#include <cstdio>
#include <cstring>

#include "BLI_listbase.h"
#include "BLI_sys_types.h"

#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.hh"

#include "BLI_ghash.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_main.hh"
#include "BKE_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

static GHash *uilisttypes_hash = nullptr;

uiListType *WM_uilisttype_find(const char *idname, bool quiet)
{
  if (idname[0]) {
    uiListType *ult = static_cast<uiListType *>(BLI_ghash_lookup(uilisttypes_hash, idname));
    if (ult) {
      return ult;
    }
  }

  if (!quiet) {
    printf("search for unknown uilisttype %s\n", idname);
  }

  return nullptr;
}

bool WM_uilisttype_add(uiListType *ult)
{
  BLI_ghash_insert(uilisttypes_hash, ult->idname, ult);
  return true;
}

static void wm_uilisttype_unlink_from_region(const uiListType *ult, ARegion *region)
{
  LISTBASE_FOREACH (uiList *, list, &region->ui_lists) {
    if (list->type == ult) {
      /* Don't delete the list, it's not just runtime data but stored in files. Freeing would make
       * that data get lost. */
      list->type = nullptr;
    }
  }
}

static void wm_uilisttype_unlink_from_area(const uiListType *ult, ScrArea *area)
{
  LISTBASE_FOREACH (SpaceLink *, space_link, &area->spacedata) {
    ListBase *regionbase = (space_link == area->spacedata.first) ? &area->regionbase :
                                                                   &space_link->regionbase;
    LISTBASE_FOREACH (ARegion *, region, regionbase) {
      wm_uilisttype_unlink_from_region(ult, region);
    }
  }
}

/**
 * For all lists representing \a ult, clear their `uiListType` pointer. Use when a list-type is
 * deleted, so that the UI doesn't keep references to it.
 *
 * This is a common pattern for unregistering (usually .py defined) types at runtime, e.g. see
 * #WM_gizmomaptype_group_unlink().
 * Note that unlike in some other cases using this pattern, we don't actually free the lists with
 * type \a ult, we just clear the reference to the type. That's because UI-Lists are written to
 * files and we don't want them to get lost together with their (user visible) settings.
 */
static void wm_uilisttype_unlink(Main *bmain, const uiListType *ult)
{
  for (wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first); wm != nullptr;
       wm = static_cast<wmWindowManager *>(wm->id.next))
  {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      LISTBASE_FOREACH (ScrArea *, global_area, &win->global_areas.areabase) {
        wm_uilisttype_unlink_from_area(ult, global_area);
      }
    }
  }

  for (bScreen *screen = static_cast<bScreen *>(bmain->screens.first); screen != nullptr;
       screen = static_cast<bScreen *>(screen->id.next))
  {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      wm_uilisttype_unlink_from_area(ult, area);
    }

    LISTBASE_FOREACH (ARegion *, region, &screen->regionbase) {
      wm_uilisttype_unlink_from_region(ult, region);
    }
  }
}

void WM_uilisttype_remove_ptr(Main *bmain, uiListType *ult)
{
  wm_uilisttype_unlink(bmain, ult);

  bool ok = BLI_ghash_remove(uilisttypes_hash, ult->idname, nullptr, MEM_freeN);

  BLI_assert(ok);
  UNUSED_VARS_NDEBUG(ok);
}

void WM_uilisttype_init()
{
  uilisttypes_hash = BLI_ghash_str_new_ex("uilisttypes_hash gh", 16);
}

void WM_uilisttype_free()
{
  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, uilisttypes_hash) {
    uiListType *ult = static_cast<uiListType *>(BLI_ghashIterator_getValue(&gh_iter));
    if (ult->rna_ext.free) {
      ult->rna_ext.free(ult->rna_ext.data);
    }
  }

  BLI_ghash_free(uilisttypes_hash, nullptr, MEM_freeN);
  uilisttypes_hash = nullptr;
}

void WM_uilisttype_to_full_list_id(const uiListType *ult,
                                   const char *list_id,
                                   char r_full_list_id[/*UI_MAX_NAME_STR*/])
{
  /* We tag the list id with the list type... */
  BLI_snprintf(r_full_list_id, UI_MAX_NAME_STR, "%s_%s", ult->idname, list_id ? list_id : "");
}

const char *WM_uilisttype_list_id_get(const uiListType *ult, uiList *list)
{
  /* Some sanity check for the assumed behavior of #WM_uilisttype_to_full_list_id(). */
  BLI_assert((list->list_id + strlen(ult->idname))[0] == '_');
  /* +1 to skip the '_' */
  return list->list_id + strlen(ult->idname) + 1;
}
