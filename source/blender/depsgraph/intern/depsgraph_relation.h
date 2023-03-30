/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "MEM_guardedalloc.h"

namespace blender::deg {

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
  /* The relation does not participate in visibility checks. */
  RELATION_NO_VISIBILITY_CHANGE = (1 << 6),
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

}  // namespace blender::deg
