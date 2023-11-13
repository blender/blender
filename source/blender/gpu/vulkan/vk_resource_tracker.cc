/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_resource_tracker.hh"
#include "vk_context.hh"

namespace blender::gpu {
bool VKSubmissionTracker::is_changed(VKContext &context)
{
  VKCommandBuffers &command_buffers = context.command_buffers_get();
  const VKSubmissionID &current_id = command_buffers.submission_id_get();
  if (last_known_id_ != current_id) {
    last_known_id_ = current_id;
    return true;
  }
  return false;
}

}  // namespace blender::gpu
