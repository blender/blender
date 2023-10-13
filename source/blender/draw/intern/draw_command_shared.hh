/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#ifndef GPU_SHADER
#  include "BLI_span.hh"
#  include "GPU_shader_shared_utils.h"

namespace blender::draw::command {

#endif

/* -------------------------------------------------------------------- */
/** \name Multi Draw
 * \{ */

/**
 * A DrawGroup allow to split the command stream into batch-able chunks of commands with
 * the same render state.
 */
struct DrawGroup {
  /** Index of next #DrawGroup from the same header. */
  uint next;

  /** Index of the first instances after sorting. */
  uint start;
  /** Total number of instances (including inverted facing). Needed to issue the draw call. */
  uint len;
  /** Number of non inverted scaling instances in this Group. */
  uint front_facing_len;

  /** #GPUBatch values to be copied to #DrawCommand after sorting (if not overridden). */
  int vertex_len;
  int vertex_first;
  int base_index;

  /** Atomic counters used during command sorting. */
  uint total_counter;

#ifndef GPU_SHADER
  /* NOTE: Union just to make sure the struct has always the same size on all platform. */
  union {
    struct {
      /** For debug printing only. */
      uint front_proto_len;
      uint back_proto_len;
      /** Needed to create the correct draw call. */
      GPUBatch *gpu_batch;
    };
    struct {
#endif
      uint front_facing_counter;
      uint back_facing_counter;
      uint _pad0, _pad1;
#ifndef GPU_SHADER
    };
  };
#endif
};
BLI_STATIC_ASSERT_ALIGN(DrawGroup, 16)

/**
 * Representation of a future draw call inside a DrawGroup. This #DrawPrototype is then
 * converted into #DrawCommand on GPU after visibility and compaction. Multiple
 * #DrawPrototype might get merged into the same final #DrawCommand.
 */
struct DrawPrototype {
  /* Reference to parent DrawGroup to get the GPUBatch vertex / instance count. */
  uint group_id;
  /* Resource handle associated with this call. Also reference visibility. */
  uint resource_handle;
  /* Custom extra value to be used by the engines. */
  uint custom_id;
  /* Number of instances. */
  uint instance_len;
};
BLI_STATIC_ASSERT_ALIGN(DrawPrototype, 16)

/** \} */

#ifndef GPU_SHADER
};  // namespace blender::draw::command
#endif
