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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#include "intern/depsgraph_relation.h" /* own include */

#include "BLI_utildefines.h"

#include "intern/depsgraph_type.h"
#include "intern/node/deg_node.h"

namespace blender::deg {

Relation::Relation(Node *from, Node *to, const char *description)
    : from(from), to(to), name(description), flag(0)
{
  /* Hook it up to the nodes which use it.
   *
   * NOTE: We register relation in the nodes which this link connects to here
   * in constructor but we don't un-register it in the destructor.
   *
   * Reasoning:
   *
   * - Destructor is currently used on global graph destruction, so there's no
   *   real need in avoiding dangling pointers, all the memory is to be freed
   *   anyway.
   *
   * - Un-registering relation is not a cheap operation, so better to have it
   *   as an explicit call if we need this. */
  from->outlinks.append(this);
  to->inlinks.append(this);
}

Relation::~Relation()
{
  /* Sanity check. */
  BLI_assert(from != nullptr && to != nullptr);
}

void Relation::unlink()
{
  /* Sanity check. */
  BLI_assert(from != nullptr && to != nullptr);
  from->outlinks.remove_first_occurrence_and_reorder(this);
  to->inlinks.remove_first_occurrence_and_reorder(this);
}

}  // namespace blender::deg
