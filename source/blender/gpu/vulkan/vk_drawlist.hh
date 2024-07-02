/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_drawlist_private.hh"

#include "vk_buffer.hh"
#include "vk_resource_tracker.hh"

namespace blender::gpu {
class VKBatch;

class VKDrawList : public DrawList, VKResourceTracker<VKBuffer> {
 private:
  /**
   * Batch from who the commands are being recorded.
   */
  VKBatch *batch_ = nullptr;

  /**
   * Maximum number of commands that can be recorded per batch. Commands will be flushed when this
   * number of commands are added.
   */
  const int length_;

  /**
   * Current number of recorded commands.
   */
  int command_index_ = 0;

 public:
  VKDrawList(int list_length);

  /**
   * Append a new command for the given batch to the draw list.
   *
   * Will flush when batch is different than the previous one or when the command_buffer_ is full.
   */
  void append(Batch *batch, int instance_first, int instance_count) override;

  /**
   * Submit buffered commands to the GPU.
   *
   * NOTE: after calling this method the command_index_ and the batch_ are reset.
   */
  void submit() override;

 private:
  /**
   * Retrieve command to write to. The returned memory is part of the mapped memory of the
   * commands_buffer_.
   */
  template<typename CommandType> CommandType &get_command(VKBuffer &buffer) const
  {
    return MutableSpan<CommandType>(static_cast<CommandType *>(buffer.mapped_memory_get()),
                                    length_)[command_index_];
  }

 protected:
  std::unique_ptr<VKBuffer> create_resource(VKContext &context) override;
};

}  // namespace blender::gpu
