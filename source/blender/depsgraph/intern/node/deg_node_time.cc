/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include "intern/node/deg_node_time.h"

#include "DNA_scene_types.h"

#include "intern/depsgraph.h"
#include "intern/depsgraph_relation.h"

namespace blender::deg {

void TimeSourceNode::tag_update(Depsgraph * /*graph*/, eUpdateSource /*source*/)
{
  tagged_for_update = true;
}

void TimeSourceNode::flush_update_tag(Depsgraph *graph)
{
  if (!tagged_for_update) {
    return;
  }
  for (Relation *rel : outlinks) {
    Node *node = rel->to;
    node->tag_update(graph, DEG_UPDATE_SOURCE_TIME);
  }
}

}  // namespace blender::deg
