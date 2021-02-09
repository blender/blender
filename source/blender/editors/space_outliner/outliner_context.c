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
 * The Original Code is Copyright (C) 2017 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spoutliner
 */

#include "BLI_listbase.h"

#include "BKE_context.h"

#include "DNA_space_types.h"

#include "outliner_intern.h"

static void outliner_context_selected_ids_recursive(const ListBase *subtree,
                                                    bContextDataResult *result)
{
  LISTBASE_FOREACH (const TreeElement *, te, subtree) {
    const TreeStoreElem *tse = TREESTORE(te);
    if ((tse->flag & TSE_SELECTED) && (ELEM(tse->type, 0, TSE_LAYER_COLLECTION))) {
      CTX_data_id_list_add(result, tse->id);
    }
    outliner_context_selected_ids_recursive(&te->subtree, result);
  }
}

static void outliner_context_selected_ids(const SpaceOutliner *space_outliner,
                                          bContextDataResult *result)
{
  outliner_context_selected_ids_recursive(&space_outliner->tree, result);
  CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
}

static const char *outliner_context_dir[] = {"selected_ids", NULL};

int /*eContextResult*/ outliner_context(const bContext *C,
                                        const char *member,
                                        bContextDataResult *result)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);

  if (CTX_data_dir(member)) {
    CTX_data_dir_set(result, outliner_context_dir);
    return CTX_RESULT_OK;
  }
  if (CTX_data_equals(member, "selected_ids")) {
    outliner_context_selected_ids(space_outliner, result);
    return CTX_RESULT_OK;
  }
  /* Note: Querying non-ID selection could also work if tree elements stored their matching RNA
   * struct type. */

  return CTX_RESULT_MEMBER_NOT_FOUND;
}
