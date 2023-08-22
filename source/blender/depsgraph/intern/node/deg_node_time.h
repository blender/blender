/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "intern/node/deg_node.h"

namespace blender::deg {

/* Time Source Node. */
struct TimeSourceNode : public Node {
  bool tagged_for_update = false;

  /* TODO: evaluate() operation needed */

  virtual void tag_update(Depsgraph *graph, eUpdateSource source) override;

  void flush_update_tag(Depsgraph *graph);

  DEG_DEPSNODE_DECLARE;
};

}  // namespace blender::deg
