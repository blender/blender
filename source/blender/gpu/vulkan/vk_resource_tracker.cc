/* SPDX-FileCopyrightText: 2023 Blender Foundation
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
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  const VKSubmissionID &current_id = command_buffer.submission_id_get();
  if (last_known_id_ != current_id) {
    last_known_id_ = current_id;
    return true;
  }
  return false;
}

}  // namespace blender::gpu
