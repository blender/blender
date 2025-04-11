/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include "intern/depsgraph_relation.hh" /* own include */

#include "intern/node/deg_node.hh"

namespace blender::deg {

void Relation::unlink()
{
  /* Sanity check. */
  BLI_assert(from != nullptr && to != nullptr);
  from->outlinks.remove_first_occurrence_and_reorder(this);
  to->inlinks.remove_first_occurrence_and_reorder(this);
}

}  // namespace blender::deg
