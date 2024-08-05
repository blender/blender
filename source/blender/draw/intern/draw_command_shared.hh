/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#ifndef GPU_SHADER
#  include "BLI_span.hh"
#  include "GPU_shader_shared_utils.hh"

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

  /** #gpu::Batch values (or subrange of) copied to #DrawCommand after sorting. */
  int vertex_len;
  int vertex_first;
  /* Set to -1 if not an indexed draw. */
  int base_index;

  /** Atomic counters used during command sorting. */
  uint total_counter;

  uint front_facing_counter;
  uint back_facing_counter;

  /* CPU specific region of the struct. Should be kept constant after recording.
   * Can be used by GPU but needs to be initialized by GPU before usage. */
#ifdef GPU_SHADER
  uint _cpu_reserved_1;
  uint _cpu_reserved_2;

  uint _cpu_reserved_3;
  uint _cpu_reserved_4;
  uint _cpu_reserved_5;
  uint _cpu_reserved_6;

#else
  struct {
    /* Specific range of vertex to draw from the #gpu::Batch. */
    uint32_t vertex_first;
    /* Ugly packing to support expanded draws without inflating the struct.
     * Makes vertex range restricted to smaller range for expanded draw. */
    uint32_t expand_prim_type : 4;
    uint32_t expand_prim_len : 3;
    uint32_t vertex_len : 25;

    /** Needed to create the correct draw call. */
    gpu::Batch *gpu_batch;
#  ifdef WITH_METAL_BACKEND
    GPUShader *gpu_shader;
#  else
    uint64_t _cpu_pad0;
#  endif
  } desc;
#endif
};
BLI_STATIC_ASSERT_ALIGN(DrawGroup, 16)

/**
 * Representation of a future draw call inside a DrawGroup. This #DrawPrototype is then
 * converted into #DrawCommand on GPU after visibility and compaction. Multiple
 * #DrawPrototype might get merged into the same final #DrawCommand.
 */
struct DrawPrototype {
  /* Reference to parent DrawGroup to get the gpu::Batch vertex / instance count. */
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
