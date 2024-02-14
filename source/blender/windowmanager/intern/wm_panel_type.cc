/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Panel Registry.
 *
 * \note Unlike menu, and other registries, this doesn't *own* the PanelType.
 *
 * For popups/popovers only, regions handle panel types by including them in local lists.
 */

#include <cstdio>

#include "BLI_sys_types.h"

#include "DNA_windowmanager_types.h"

#include "BLI_ghash.h"
#include "BLI_utildefines.h"

#include "BKE_screen.hh"

#include "WM_api.hh"

static GHash *g_paneltypes_hash = nullptr;

PanelType *WM_paneltype_find(const char *idname, bool quiet)
{
  if (idname[0]) {
    PanelType *pt = static_cast<PanelType *>(BLI_ghash_lookup(g_paneltypes_hash, idname));
    if (pt) {
      return pt;
    }
  }

  if (!quiet) {
    printf("search for unknown paneltype %s\n", idname);
  }

  return nullptr;
}

bool WM_paneltype_add(PanelType *pt)
{
  BLI_ghash_insert(g_paneltypes_hash, pt->idname, pt);
  return true;
}

void WM_paneltype_remove(PanelType *pt)
{
  const bool ok = BLI_ghash_remove(g_paneltypes_hash, pt->idname, nullptr, nullptr);

  BLI_assert(ok);
  UNUSED_VARS_NDEBUG(ok);
}

void WM_paneltype_init()
{
  /* reserve size is set based on blender default setup */
  g_paneltypes_hash = BLI_ghash_str_new_ex("g_paneltypes_hash gh", 512);
}

void WM_paneltype_clear()
{
  BLI_ghash_free(g_paneltypes_hash, nullptr, nullptr);
}

void WM_paneltype_idname_visit_for_search(
    const bContext * /*C*/,
    PointerRNA * /*ptr*/,
    PropertyRNA * /*prop*/,
    const char * /*edit_text*/,
    blender::FunctionRef<void(StringPropertySearchVisitParams)> visit_fn)
{
  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, g_paneltypes_hash) {
    PanelType *pt = static_cast<PanelType *>(BLI_ghashIterator_getValue(&gh_iter));

    StringPropertySearchVisitParams visit_params{};
    visit_params.text = pt->idname;
    visit_params.info = pt->label;
    visit_fn(visit_params);
  }
}
