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
 * \ingroup spoutliner
 */

#include <cstring>

#include "BLI_listbase.h"
#include "BLI_listbase_wrapper.hh"
#include "BLI_utildefines.h"

#include "DNA_sequence_types.h"
#include "DNA_space_types.h"

#include "SEQ_sequencer.h"

#include "../outliner_intern.hh"
#include "tree_display.hh"
#include "tree_element.hh"

namespace blender::ed::outliner {

template<typename T> using List = ListBaseWrapper<T>;

TreeDisplaySequencer::TreeDisplaySequencer(SpaceOutliner &space_outliner)
    : AbstractTreeDisplay(space_outliner)
{
}

ListBase TreeDisplaySequencer::buildTree(const TreeSourceData &source_data)
{
  ListBase tree = {nullptr};

  Editing *ed = SEQ_editing_get(source_data.scene);
  if (ed == nullptr) {
    return tree;
  }

  for (Sequence *seq : List<Sequence>(ed->seqbasep)) {
    SequenceAddOp op = need_add_seq_dup(seq);
    if (op == SEQUENCE_DUPLICATE_NONE) {
      outliner_add_element(&space_outliner_, &tree, seq, nullptr, TSE_SEQUENCE, 0);
    }
    else if (op == SEQUENCE_DUPLICATE_ADD) {
      TreeElement *te = outliner_add_element(
          &space_outliner_, &tree, seq, nullptr, TSE_SEQUENCE_DUP, 0);
      add_seq_dup(seq, te, 0);
    }
  }

  return tree;
}

SequenceAddOp TreeDisplaySequencer::need_add_seq_dup(Sequence *seq) const
{
  if ((!seq->strip) || (!seq->strip->stripdata)) {
    return SEQUENCE_DUPLICATE_NONE;
  }

  /*
   * First check backward, if we found a duplicate
   * sequence before this, don't need it, just return.
   */
  Sequence *p = seq->prev;
  while (p) {
    if ((!p->strip) || (!p->strip->stripdata)) {
      p = p->prev;
      continue;
    }

    if (STREQ(p->strip->stripdata->name, seq->strip->stripdata->name)) {
      return SEQUENCE_DUPLICATE_NOOP;
    }
    p = p->prev;
  }

  p = seq->next;
  while (p) {
    if ((!p->strip) || (!p->strip->stripdata)) {
      p = p->next;
      continue;
    }

    if (STREQ(p->strip->stripdata->name, seq->strip->stripdata->name)) {
      return SEQUENCE_DUPLICATE_ADD;
    }
    p = p->next;
  }

  return SEQUENCE_DUPLICATE_NONE;
}

void TreeDisplaySequencer::add_seq_dup(Sequence *seq, TreeElement *te, short index) const
{
  Sequence *p = seq;
  while (p) {
    if ((!p->strip) || (!p->strip->stripdata) || (p->strip->stripdata->name[0] == '\0')) {
      p = p->next;
      continue;
    }

    if (STREQ(p->strip->stripdata->name, seq->strip->stripdata->name)) {
      outliner_add_element(&space_outliner_, &te->subtree, (void *)p, te, TSE_SEQUENCE, index);
    }
    p = p->next;
  }
}

}  // namespace blender::ed::outliner
