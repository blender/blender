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
 */

/** \file
 * \ingroup wm
 *
 * UI List Registry.
 */

#include <stdio.h>
#include <string.h>

#include "BLI_sys_types.h"

#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.h"

#include "BLI_ghash.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_screen.h"

#include "WM_api.h"
#include "WM_types.h"

static GHash *uilisttypes_hash = NULL;

uiListType *WM_uilisttype_find(const char *idname, bool quiet)
{
  if (idname[0]) {
    uiListType *ult = BLI_ghash_lookup(uilisttypes_hash, idname);
    if (ult) {
      return ult;
    }
  }

  if (!quiet) {
    printf("search for unknown uilisttype %s\n", idname);
  }

  return NULL;
}

bool WM_uilisttype_add(uiListType *ult)
{
  BLI_ghash_insert(uilisttypes_hash, ult->idname, ult);
  return 1;
}

void WM_uilisttype_freelink(uiListType *ult)
{

  bool ok = BLI_ghash_remove(uilisttypes_hash, ult->idname, NULL, MEM_freeN);

  BLI_assert(ok);
  UNUSED_VARS_NDEBUG(ok);
}

/* called on initialize WM_init() */
void WM_uilisttype_init(void)
{
  uilisttypes_hash = BLI_ghash_str_new_ex("uilisttypes_hash gh", 16);
}

void WM_uilisttype_free(void)
{
  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, uilisttypes_hash) {
    uiListType *ult = BLI_ghashIterator_getValue(&gh_iter);
    if (ult->rna_ext.free) {
      ult->rna_ext.free(ult->rna_ext.data);
    }
  }

  BLI_ghash_free(uilisttypes_hash, NULL, MEM_freeN);
  uilisttypes_hash = NULL;
}

/**
 * The "full" list-ID is an internal name used for storing and identifying a list. It is built like
 * this:
 * "{uiListType.idname}_{list_id}", wherby "list_id" is an optional parameter passed to
 * `UILayout.template_list()`. If it is not set, the full list-ID is just "{uiListType.idname}_".
 *
 * Note that whenever the Python API refers to the list-ID, it's the short, "non-full" one it
 * passed to `UILayout.template_list()`. C code can query that through
 * #WM_uilisttype_list_id_get().
 */
void WM_uilisttype_to_full_list_id(const uiListType *ult,
                                   const char *list_id,
                                   char r_full_list_id[UI_MAX_NAME_STR])
{
  /* We tag the list id with the list type... */
  BLI_snprintf(r_full_list_id, UI_MAX_NAME_STR, "%s_%s", ult->idname, list_id ? list_id : "");
}

/**
 * Get the "non-full" list-ID, see #WM_uilisttype_to_full_list_id() for details.
 *
 * \note Assumes `uiList.list_id` was set using #WM_uilisttype_to_full_list_id()!
 */
const char *WM_uilisttype_list_id_get(const uiListType *ult, uiList *list)
{
  /* Some sanity check for the assumed behavior of #WM_uilisttype_to_full_list_id(). */
  BLI_assert((list->list_id + strlen(ult->idname))[0] == '_');
  /* +1 to skip the '_' */
  return list->list_id + strlen(ult->idname) + 1;
}
