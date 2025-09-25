/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BKE_context.hh"

#include "DNA_space_types.h"

#include "outliner_intern.hh"
#include "tree/tree_iterator.hh"

namespace blender::ed::outliner {

static void outliner_context_selected_ids(const SpaceOutliner *space_outliner,
                                          bContextDataResult *result)
{
  tree_iterator::all(*space_outliner, [&](const TreeElement *te) {
    const TreeStoreElem *tse = TREESTORE(te);
    if ((tse->flag & TSE_SELECTED) && ELEM(tse->type, TSE_SOME_ID, TSE_LAYER_COLLECTION)) {
      CTX_data_id_list_add(result, tse->id);
    }
  });
  CTX_data_type_set(result, ContextDataType::Collection);
}

static const char *outliner_context_dir[] = {
    "id",
    "selected_ids",
    nullptr,
};

int /*eContextResult*/ outliner_main_region_context(const bContext *C,
                                                    const char *member,
                                                    bContextDataResult *result)
{
  SpaceOutliner *space_outliner = CTX_wm_space_outliner(C);

  if (CTX_data_dir(member)) {
    CTX_data_dir_set(result, outliner_context_dir);
    return CTX_RESULT_OK;
  }

  if (CTX_data_equals(member, "id")) {
    const TreeElement *active_element = outliner_find_element_with_flag(&space_outliner->tree,
                                                                        TSE_ACTIVE);
    if (!active_element) {
      return CTX_RESULT_NO_DATA;
    }
    const TreeStoreElem *active_tse = TREESTORE(active_element);
    if (!TSE_IS_REAL_ID(active_tse)) {
      return CTX_RESULT_NO_DATA;
    }

    CTX_data_id_pointer_set(result, active_tse->id);
    return CTX_RESULT_OK;
  }

  if (CTX_data_equals(member, "selected_ids")) {
    outliner_context_selected_ids(space_outliner, result);
    return CTX_RESULT_OK;
  }
  /* NOTE: Querying non-ID selection could also work if tree elements stored their matching RNA
   * struct type. */

  return CTX_RESULT_MEMBER_NOT_FOUND;
}

}  // namespace blender::ed::outliner
