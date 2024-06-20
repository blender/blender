/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_resource_tracker.hh"
#include "vk_context.hh"

namespace blender::gpu {
bool VKSubmissionTracker::is_changed(const VKContext &context)
{
  const VKSubmissionID &current_id = context.render_graph.submission_id;
  if (last_known_id_ != current_id) {
    last_known_id_ = current_id;
    return true;
  }
  return false;
}

}  // namespace blender::gpu
