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

#pragma once

#include "MEM_guardedalloc.h"

namespace blender {
namespace deg {

struct Node;

/* Settings/Tags on Relationship.
 * NOTE: Is a bitmask, allowing accumulation. */
enum RelationFlag {
  /* "cyclic" link - when detecting cycles, this relationship was the one
   * which triggers a cyclic relationship to exist in the graph. */
  RELATION_FLAG_CYCLIC = (1 << 0),
  /* Update flush will not go through this relation. */
  RELATION_FLAG_NO_FLUSH = (1 << 1),
  /* Only flush along the relation is update comes from a node which was
   * affected by user input. */
  RELATION_FLAG_FLUSH_USER_EDIT_ONLY = (1 << 2),
  /* The relation can not be killed by the cyclic dependencies solver. */
  RELATION_FLAG_GODMODE = (1 << 4),
  /* Relation will check existence before being added. */
  RELATION_CHECK_BEFORE_ADD = (1 << 5),
};

/* B depends on A (A -> B) */
struct Relation {
  Relation(Node *from, Node *to, const char *description);
  ~Relation();

  void unlink();

  /* the nodes in the relationship (since this is shared between the nodes) */
  Node *from; /* A */
  Node *to;   /* B */

  /* relationship attributes */
  const char *name; /* label for debugging */
  int flag;         /* Bitmask of RelationFlag) */

  MEM_CXX_CLASS_ALLOC_FUNCS("Relation");
};

}  // namespace deg
}  // namespace blender
