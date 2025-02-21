/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_subdiv_shader_shared.hh"
#endif

#include "gpu_shader_create_info.hh"

#include "draw_subdiv_defines.hh"

GPU_SHADER_CREATE_INFO(subdiv_base)
LOCAL_GROUP_SIZE(SUBDIV_GROUP_SIZE)
TYPEDEF_SOURCE("draw_subdiv_shader_shared.hh")
UNIFORM_BUF(SHADER_DATA_BUF_SLOT, DRWSubdivUboStorage, shader_data)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_polygon_offset_base)
DEFINE("SUBDIV_POLYGON_OFFSET")
STORAGE_BUF(SUBDIV_FACE_OFFSET_BUF_SLOT, READ, uint, subdiv_face_offset[])
ADDITIONAL_INFO(subdiv_base)
GPU_SHADER_CREATE_END()

/* -------------------------------------------------------------------- */
/** \name Line indices
 * \{ */

GPU_SHADER_CREATE_INFO(subdiv_lines)
DO_STATIC_COMPILATION()
STORAGE_BUF(LINES_INPUT_EDGE_DRAW_FLAG_BUF_SLOT, READ, int, input_edge_draw_flag[])
STORAGE_BUF(LINES_EXTRA_COARSE_FACE_DATA_BUF_SLOT, READ, uint, extra_coarse_face_data[])
STORAGE_BUF(LINES_OUTPUT_LINES_BUF_SLOT, WRITE, uint, output_lines[])
COMPUTE_SOURCE("subdiv_ibo_lines_comp.glsl")
ADDITIONAL_INFO(subdiv_polygon_offset_base)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_lines_loose)
DO_STATIC_COMPILATION()
DEFINE("LINES_LOOSE")
STORAGE_BUF(LINES_OUTPUT_LINES_BUF_SLOT, WRITE, uint, output_lines[])
STORAGE_BUF(LINES_LINES_LOOSE_FLAGS, READ, uint, lines_loose_flags[])
COMPUTE_SOURCE("subdiv_ibo_lines_comp.glsl")
ADDITIONAL_INFO(subdiv_base)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Normals
 * \{ */

GPU_SHADER_CREATE_INFO(subdiv_normals_accumulate)
DO_STATIC_COMPILATION()
STORAGE_BUF(NORMALS_ACCUMULATE_POS_NOR_BUF_SLOT, READ, PosNorLoop, pos_nor[])
STORAGE_BUF(NORMALS_ACCUMULATE_FACE_ADJACENCY_OFFSETS_BUF_SLOT,
            READ,
            uint,
            face_adjacency_offsets[])
STORAGE_BUF(NORMALS_ACCUMULATE_FACE_ADJACENCY_LISTS_BUF_SLOT, READ, uint, face_adjacency_lists[])
STORAGE_BUF(NORMALS_ACCUMULATE_VERTEX_LOOP_MAP_BUF_SLOT, READ, uint, vert_loop_map[])
STORAGE_BUF(NORMALS_ACCUMULATE_NORMALS_BUF_SLOT, WRITE, packed_float3, normals[])
COMPUTE_SOURCE("subdiv_normals_accumulate_comp.glsl")
ADDITIONAL_INFO(subdiv_base)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_normals_finalize)
DO_STATIC_COMPILATION()
STORAGE_BUF(NORMALS_FINALIZE_VERTEX_NORMALS_BUF_SLOT, READ, packed_float3, vertex_normals[])
STORAGE_BUF(NORMALS_FINALIZE_VERTEX_LOOP_MAP_BUF_SLOT, READ, uint, vert_loop_map[])
STORAGE_BUF(NORMALS_FINALIZE_POS_NOR_BUF_SLOT, READ_WRITE, PosNorLoop, pos_nor[])
COMPUTE_SOURCE("subdiv_normals_finalize_comp.glsl")
ADDITIONAL_INFO(subdiv_base)
GPU_SHADER_CREATE_END()

/* Meshes can have (custom) split normals as loop attribute. */
GPU_SHADER_CREATE_INFO(subdiv_custom_normals_finalize)
DO_STATIC_COMPILATION()
DEFINE("CUSTOM_NORMALS")
STORAGE_BUF(NORMALS_FINALIZE_CUSTOM_NORMALS_BUF_SLOT, READ, CustomNormal, custom_normals[])
STORAGE_BUF(NORMALS_FINALIZE_POS_NOR_BUF_SLOT, READ_WRITE, PosNorLoop, pos_nor[])
COMPUTE_SOURCE("subdiv_normals_finalize_comp.glsl")
ADDITIONAL_INFO(subdiv_base)
GPU_SHADER_CREATE_END()

/** \} */
