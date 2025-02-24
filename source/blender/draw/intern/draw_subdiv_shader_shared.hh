/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "draw_subdiv_defines.hh"

#ifndef GPU_SHADER
#  include "GPU_shader_shared_utils.hh"

#endif

struct DRWSubdivUboStorage {
  /* Offsets in the buffers data where the source and destination data start. */
  int src_offset;
  int dst_offset;

  /* Parameters for the DRWPatchMap. */
  int min_patch_face;
  int max_patch_face;
  int max_depth;
  bool32_t patches_are_triangular;

  /* Coarse topology information. */
  int coarse_face_count;
  uint edge_loose_offset;

  /* Refined topology information. */
  uint num_subdiv_loops;

  /* The sculpt mask data layer may be null. */
  bool32_t has_sculpt_mask;

  /* Masks for the extra coarse face data. */
  uint coarse_face_select_mask;
  uint coarse_face_smooth_mask;
  uint coarse_face_active_mask;
  uint coarse_face_hidden_mask;
  uint coarse_face_loopstart_mask;

  /* Number of elements to process in the compute shader (can be the coarse quad count, or the
   * final vertex count, depending on which compute pass we do). This is used to early out in case
   * of out of bond accesses as compute dispatch are of fixed size. */
  uint total_dispatch_size;

  bool32_t is_edit_mode;
  bool32_t use_hide;
  int _pad3;
  int _pad4;
};
BLI_STATIC_ASSERT_ALIGN(DRWSubdivUboStorage, 16)

struct SculptData {
  uint face_set_color;
  float mask;
};

/* Duplicate of #PosNorLoop from the mesh extract CPU code.
 * We do not use a vec3 for the position as it will be padded to a vec4 which is incompatible with
 * the format. */
struct PosNorLoop {
  float x, y, z;
  float nx, ny, nz;
  float flag;
};

struct LoopNormal {
  float nx, ny, nz;
  float flag;
};

struct CustomNormal {
  float x;
  float y;
  float z;
};
