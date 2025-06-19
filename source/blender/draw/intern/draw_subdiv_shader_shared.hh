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

/* Mirror of #UVStretchAngle in the C++ code, but using floats until proper data compression
 * is implemented for all subdivision data. */
struct UVStretchAngle {
  float angle;
  float uv_angle0;
  float uv_angle1;
};

struct Position {
  float x;
  float y;
  float z;
};

struct Normal {
  float x;
  float y;
  float z;
};

/* Structure for #CompressedPatchCoord. */
struct BlenderPatchCoord {
  int patch_index;
  uint encoded_uv;
};

/* Patch evaluation - F-dots. */
/* float3 is padded to float4, but the format used for face-dots does not have any padding. */
struct FDotVert {
  float x, y, z;
};

/* Same here, do not use float3. */
struct FDotNor {
  float x, y, z;
  float flag;
};

/* This structure is a carbon copy of OpenSubDiv's #PatchTable::PatchHandle. */
struct PatchHandle {
  int array_index;
  int patch_index;
  int vertex_index;
};

/* This structure is a carbon copy of OpenSubDiv's #PatchCoord. */
struct PatchCoord {
  int array_index;
  int patch_index;
  int vertex_index;
  float u;
  float v;
};

/* This structure is a carbon copy of OpenSubDiv's #PatchCoord.QuadNode.
 * Each child is a bit-field. */
struct QuadNode {
  uint4 child;
};

/* When not using OSD we need to defined the structs as they subdiv_info still refer to them. */
#if !defined(USE_GPU_SHADER_CREATE_INFO) || \
    (!defined(OSD_PATCH_BASIS_GLSL) && !defined(OSD_PATCH_BASIS_METAL))
/* This structure is a carbon copy of OpenSubDiv's #Osd::PatchParam. */
struct OsdPatchParam {
  int field0;
  int field1;
  float sharpness;
};

/* This structure is a carbon copy of OpenSubDiv's #Osd::PatchArray. */
struct OsdPatchArray {
  int regDesc;
  int desc;
  int numPatches;
  int indexBase;
  int stride;
  int primitiveIdBase;
};

/* This structure is a carbon copy of OpenSubDiv's #Osd::PatchCoord. */
struct OsdPatchCoord {
  int arrayIndex;
  int patchIndex;
  int vertIndex;
  float s;
  float t;
};

#endif
