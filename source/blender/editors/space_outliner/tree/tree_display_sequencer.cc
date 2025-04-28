/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include <cstring>

#include "BLI_listbase_wrapper.hh"
#include "BLI_utildefines.h"

#include "DNA_sequence_types.h"
#include "DNA_space_types.h"

#include "SEQ_sequencer.hh"

#include "../outliner_intern.hh"
#include "tree_display.hh"

namespace blender::ed::outliner {

template<typename T> using List = ListBaseWrapper<T>;

TreeDisplaySequencer::TreeDisplaySequencer(SpaceOutliner &space_outliner)
    : AbstractTreeDisplay(space_outliner)
{
}

ListBase TreeDisplaySequencer::build_tree(const TreeSourceData &source_data)
{
  ListBase tree = {nullptr};

  Editing *ed = seq::editing_get(source_data.scene);
  if (ed == nullptr) {
    return tree;
  }

  for (Strip *strip : List<Strip>(ed->seqbasep)) {
    StripAddOp op = need_add_strip_dup(strip);
    if (op == StripAddOp::None) {
      add_element(&tree, nullptr, strip, nullptr, TSE_STRIP, 0);
    }
    else if (op == StripAddOp::Add) {
      TreeElement *te = add_element(&tree, nullptr, strip, nullptr, TSE_STRIP_DUP, 0);
      add_strip_dup(strip, te, 0);
    }
  }

  return tree;
}

StripAddOp TreeDisplaySequencer::need_add_strip_dup(Strip *strip) const
{
  if ((!strip->data) || (!strip->data->stripdata)) {
    return StripAddOp::None;
  }

  /*
   * First check backward, if we found a duplicate
   * sequence before this, don't need it, just return.
   */
  Strip *p = strip->prev;
  while (p) {
    if ((!p->data) || (!p->data->stripdata)) {
      p = p->prev;
      continue;
    }

    if (STREQ(p->data->stripdata->filename, strip->data->stripdata->filename)) {
      return StripAddOp::Noop;
    }
    p = p->prev;
  }

  p = strip->next;
  while (p) {
    if ((!p->data) || (!p->data->stripdata)) {
      p = p->next;
      continue;
    }

    if (STREQ(p->data->stripdata->filename, strip->data->stripdata->filename)) {
      return StripAddOp::Add;
    }
    p = p->next;
  }

  return StripAddOp::None;
}

void TreeDisplaySequencer::add_strip_dup(Strip *strip, TreeElement *te, short index)
{
  Strip *p = strip;
  while (p) {
    if ((!p->data) || (!p->data->stripdata) || (p->data->stripdata->filename[0] == '\0')) {
      p = p->next;
      continue;
    }

    if (STREQ(p->data->stripdata->filename, strip->data->stripdata->filename)) {
      add_element(&te->subtree, nullptr, (void *)p, te, TSE_STRIP, index);
    }
    p = p->next;
  }
}

}  // namespace blender::ed::outliner
