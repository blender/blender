/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once

#  include "gpu_shader_compat.hh"

#  include "draw_subdiv_shader_shared.hh"

/* `osd_patch_defines.glsl` must be included before `osd_patch_basis.glsl` */
#  include "osd_patch_defines.glsl"

#  include "osd_patch_basis.glsl"
#endif

#include "gpu_shader_create_info.hh"

#include "draw_subdiv_defines.hh"

/* -------------------------------------------------------------------- */
/** \name Patch evaluation
 * \{ */

GPU_SHADER_CREATE_INFO(subdiv_patch_evaluation_basis)
DEFINE("OPENSUBDIV_GLSL_COMPUTE_USE_1ST_DERIVATIVES")
TYPEDEF_SOURCE("osd_patch_defines.glsl")
TYPEDEF_SOURCE("osd_patch_basis.glsl")
COMPUTE_SOURCE("subdiv_patch_evaluation_comp.glsl")
STORAGE_BUF(PATCH_EVALUATION_SOURCE_VERTEX_BUFFER_BUF_SLOT, read, float, srcVertexBuffer[])
STORAGE_BUF(PATCH_EVALUATION_INPUT_PATCH_HANDLES_BUF_SLOT,
            read,
            PatchHandle,
            input_patch_handles[])
STORAGE_BUF(PATCH_EVALUATION_QUAD_NODES_BUF_SLOT, read, QuadNode, quad_nodes[])
STORAGE_BUF(PATCH_EVALUATION_PATCH_COORDS_BUF_SLOT, read, BlenderPatchCoord, patch_coords[])
STORAGE_BUF(PATCH_EVALUATION_PATCH_ARRAY_BUFFER_BUF_SLOT, read, OsdPatchArray, patchArrayBuffer[])
STORAGE_BUF(PATCH_EVALUATION_PATCH_INDEX_BUFFER_BUF_SLOT, read, int, patchIndexBuffer[])
STORAGE_BUF(PATCH_EVALUATION_PATCH_PARAM_BUFFER_BUF_SLOT, read, OsdPatchParam, patchParamBuffer[])
ADDITIONAL_INFO(subdiv_base)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_patch_evaluation_fdots)
DEFINE("FDOTS_EVALUATION")
STORAGE_BUF(PATCH_EVALUATION_OUTPUT_FDOTS_VERTEX_BUFFER_BUF_SLOT, write, FDotVert, output_verts[])
STORAGE_BUF(PATCH_EVALUATION_OUTPUT_INDICES_BUF_SLOT, write, uint, output_indices[])
STORAGE_BUF(PATCH_EVALUATION_EXTRA_COARSE_FACE_DATA_BUF_SLOT, read, uint, extra_coarse_face_data[])
ADDITIONAL_INFO(subdiv_patch_evaluation_basis)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_patch_evaluation_verts)
DO_STATIC_COMPILATION()
DEFINE("VERTS_EVALUATION")
STORAGE_BUF(PATCH_EVALUATION_OUTPUT_POS_BUF_SLOT, write, Position, positions[])
ADDITIONAL_INFO(subdiv_patch_evaluation_basis)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_patch_evaluation_fvar)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(subdiv_patch_evaluation_basis)
DEFINE("FVAR_EVALUATION")
STORAGE_BUF(PATCH_EVALUATION_OUTPUT_FVAR_BUF_SLOT, write, packed_float2, output_fvar[])
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_patch_evaluation_fdots_normals)
DO_STATIC_COMPILATION()
DEFINE("FDOTS_NORMALS")
STORAGE_BUF(PATCH_EVALUATION_OUTPUT_NORMALS_BUF_SLOT, write, FDotNor, output_nors[])
ADDITIONAL_INFO(subdiv_patch_evaluation_fdots)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_patch_evaluation_verts_orcos)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(subdiv_patch_evaluation_verts)
DEFINE("ORCO_EVALUATION")
STORAGE_BUF(PATCH_EVALUATION_SOURCE_EXTRA_VERTEX_BUFFER_BUF_SLOT,
            read,
            float,
            srcExtraVertexBuffer[])
STORAGE_BUF(PATCH_EVALUATION_OUTPUT_ORCOS_BUF_SLOT, write, float4, output_orcos[])
GPU_SHADER_CREATE_END()

/** \} */
