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
/** \name Patch evaluation
 * \{ */

#ifdef __APPLE__
#  define SUBDIV_PATCH_EVALUATION_BASIS_DEFINES() DEFINE("OSD_PATCH_BASIS_METAL")
#else
#  define SUBDIV_PATCH_EVALUATION_BASIS_DEFINES() DEFINE("OSD_PATCH_BASIS_GLSL")
#endif

#define SUBDIV_PATCH_EVALUATION_BASIS() \
  SUBDIV_PATCH_EVALUATION_BASIS_DEFINES() \
  DEFINE("OPENSUBDIV_GLSL_COMPUTE_USE_1ST_DERIVATIVES") \
  TYPEDEF_SOURCE("osd_patch_basis.glsl") \
  COMPUTE_SOURCE("subdiv_patch_evaluation_comp.glsl") \
  STORAGE_BUF(PATCH_EVALUATION_SOURCE_VERTEX_BUFFER_BUF_SLOT, READ, float, srcVertexBuffer[]) \
  STORAGE_BUF( \
      PATCH_EVALUATION_INPUT_PATCH_HANDLES_BUF_SLOT, READ, PatchHandle, input_patch_handles[]) \
  STORAGE_BUF(PATCH_EVALUATION_QUAD_NODES_BUF_SLOT, READ, QuadNode, quad_nodes[]) \
  STORAGE_BUF(PATCH_EVALUATION_PATCH_COORDS_BUF_SLOT, READ, BlenderPatchCoord, patch_coords[]) \
  STORAGE_BUF( \
      PATCH_EVALUATION_INPUT_VERTEX_ORIG_INDEX_BUF_SLOT, READ, int, input_vert_origindex[]) \
  STORAGE_BUF( \
      PATCH_EVALUATION_PATCH_ARRAY_BUFFER_BUF_SLOT, READ, OsdPatchArray, patchArrayBuffer[]) \
  STORAGE_BUF(PATCH_EVALUATION_PATCH_INDEX_BUFFER_BUF_SLOT, READ, int, patchIndexBuffer[]) \
  STORAGE_BUF( \
      PATCH_EVALUATION_PATCH_PARAM_BUFFER_BUF_SLOT, READ, OsdPatchParam, patchParamBuffer[]) \
  ADDITIONAL_INFO(subdiv_base)

#define SUBDIV_PATCH_EVALUATION_FDOTS() \
  SUBDIV_PATCH_EVALUATION_BASIS() \
  DEFINE("FDOTS_EVALUATION") \
  STORAGE_BUF( \
      PATCH_EVALUATION_OUTPUT_FDOTS_VERTEX_BUFFER_BUF_SLOT, WRITE, FDotVert, output_verts[]) \
  STORAGE_BUF(PATCH_EVALUATION_OUTPUT_INDICES_BUF_SLOT, WRITE, uint, output_indices[]) \
  STORAGE_BUF( \
      PATCH_EVALUATION_EXTRA_COARSE_FACE_DATA_BUF_SLOT, READ, uint, extra_coarse_face_data[])

#define SUBDIV_PATCH_EVALUATION_VERTS() \
  SUBDIV_PATCH_EVALUATION_BASIS() \
  DEFINE("VERTS_EVALUATION") \
  STORAGE_BUF(PATCH_EVALUATION_FLAGS_BUFFER_BUF_SLOT, READ, int, flags_buffer[]) \
  STORAGE_BUF(PATCH_EVALUATION_OUTPUT_VERTS_BUF_SLOT, WRITE, PosNorLoop, output_verts[])

GPU_SHADER_CREATE_INFO(subdiv_patch_evaluation_fvar)
DO_STATIC_COMPILATION()
SUBDIV_PATCH_EVALUATION_BASIS()
DEFINE("FVAR_EVALUATION")
STORAGE_BUF(PATCH_EVALUATION_OUTPUT_FVAR_BUF_SLOT, WRITE, packed_float2, output_fvar[])
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_patch_evaluation_fdots)
DO_STATIC_COMPILATION()
SUBDIV_PATCH_EVALUATION_FDOTS()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_patch_evaluation_fdots_normals)
DO_STATIC_COMPILATION()
SUBDIV_PATCH_EVALUATION_FDOTS()
DEFINE("FDOTS_NORMALS")
STORAGE_BUF(PATCH_EVALUATION_OUTPUT_NORMALS_BUF_SLOT, WRITE, FDotNor, output_nors[])
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_patch_evaluation_verts)
DO_STATIC_COMPILATION()
SUBDIV_PATCH_EVALUATION_VERTS()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_patch_evaluation_verts_orcos)
DO_STATIC_COMPILATION()
SUBDIV_PATCH_EVALUATION_VERTS()
DEFINE("ORCO_EVALUATION")
STORAGE_BUF(PATCH_EVALUATION_SOURCE_EXTRA_VERTEX_BUFFER_BUF_SLOT,
            READ,
            float,
            srcExtraVertexBuffer[])
STORAGE_BUF(PATCH_EVALUATION_OUTPUT_ORCOS_BUF_SLOT, WRITE, float4, output_orcos[])
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Loop Normals
 * \{ */

GPU_SHADER_CREATE_INFO(subdiv_loop_normals)
DO_STATIC_COMPILATION()
STORAGE_BUF(LOOP_NORMALS_POS_NOR_BUF_SLOT, READ, PosNorLoop, pos_nor[])
STORAGE_BUF(LOOP_NORMALS_EXTRA_COARSE_FACE_DATA_BUF_SLOT, READ, uint, extra_coarse_face_data[])
STORAGE_BUF(LOOP_NORMALS_INPUT_VERT_ORIG_INDEX_BUF_SLOT, READ, int, input_vert_origindex[])
STORAGE_BUF(LOOP_NORMALS_OUTPUT_LNOR_BUF_SLOT, WRITE, LoopNormal, output_lnor[])
COMPUTE_SOURCE("subdiv_vbo_lnor_comp.glsl")
ADDITIONAL_INFO(subdiv_polygon_offset_base)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Triangle indices
 * \{ */

GPU_SHADER_CREATE_INFO(subdiv_tris_single_material)
DO_STATIC_COMPILATION()
DEFINE("SINGLE_MATERIAL")
STORAGE_BUF(TRIS_EXTRA_COARSE_FACE_DATA_BUF_SLOT, READ, uint, extra_coarse_face_data[])
STORAGE_BUF(TRIS_OUTPUT_TRIS_BUF_SLOT, WRITE, uint, output_tris[])
COMPUTE_SOURCE("subdiv_ibo_tris_comp.glsl")
ADDITIONAL_INFO(subdiv_polygon_offset_base)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_tris_multiple_materials)
DO_STATIC_COMPILATION()
STORAGE_BUF(TRIS_EXTRA_COARSE_FACE_DATA_BUF_SLOT, READ, uint, extra_coarse_face_data[])
STORAGE_BUF(TRIS_OUTPUT_TRIS_BUF_SLOT, WRITE, uint, output_tris[])
STORAGE_BUF(TRIS_FACE_MAT_OFFSET, READ, uint, face_mat_offset[])
COMPUTE_SOURCE("subdiv_ibo_tris_comp.glsl")
ADDITIONAL_INFO(subdiv_polygon_offset_base)
GPU_SHADER_CREATE_END()

/** \} */

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
/** \name Edge data for object mode wireframe
 * \{ */

GPU_SHADER_CREATE_INFO(subdiv_edge_fac_base)
ADDITIONAL_INFO(subdiv_base)
STORAGE_BUF(EDGE_FAC_POS_NOR_BUF_SLOT, READ, PosNorLoop, pos_nor[])
STORAGE_BUF(EDGE_FAC_EDGE_DRAW_FLAG_BUF_SLOT, READ, uint, input_edge_draw_flag[])
STORAGE_BUF(EDGE_FAC_POLY_OTHER_MAP_BUF_SLOT, READ, int, input_poly_other_map[])
COMPUTE_SOURCE("subdiv_vbo_edge_fac_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_edge_fac)
DO_STATIC_COMPILATION()
STORAGE_BUF(EDGE_FAC_EDGE_FAC_BUF_SLOT, WRITE, uint, output_edge_fac[])
ADDITIONAL_INFO(subdiv_edge_fac_base)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_edge_fac_amd_legacy)
DO_STATIC_COMPILATION()
DEFINE("GPU_AMD_DRIVER_BYTE_BUG")
STORAGE_BUF(EDGE_FAC_EDGE_FAC_BUF_SLOT, WRITE, float, output_edge_fac[])
ADDITIONAL_INFO(subdiv_edge_fac_base)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Custom data
 * \{ */

#define SUBDIV_CUSTOM_DATA_VARIANT(suffix, gpu_comp_type, data_type, dimension) \
  GPU_SHADER_CREATE_INFO(subdiv_custom_data_interp_##suffix) \
  DO_STATIC_COMPILATION() \
  DEFINE(gpu_comp_type) \
  DEFINE(dimension) \
  COMPUTE_SOURCE("subdiv_custom_data_interp_comp.glsl") \
  STORAGE_BUF(CUSTOM_DATA_FACE_PTEX_OFFSET_BUF_SLOT, READ, uint, face_ptex_offset[]) \
  STORAGE_BUF(CUSTOM_DATA_PATCH_COORDS_BUF_SLOT, READ, BlenderPatchCoord, patch_coords[]) \
  STORAGE_BUF(CUSTOM_DATA_EXTRA_COARSE_FACE_DATA_BUF_SLOT, READ, uint, extra_coarse_face_data[]) \
  STORAGE_BUF(CUSTOM_DATA_SOURCE_DATA_BUF_SLOT, READ, data_type, src_data[]) \
  STORAGE_BUF(CUSTOM_DATA_DESTINATION_DATA_BUF_SLOT, WRITE, data_type, dst_data[]) \
  ADDITIONAL_INFO(subdiv_polygon_offset_base) \
  GPU_SHADER_CREATE_END()

SUBDIV_CUSTOM_DATA_VARIANT(4d_u16, "GPU_COMP_U16", uint, "DIMENSIONS_4")
SUBDIV_CUSTOM_DATA_VARIANT(1d_i32, "GPU_COMP_I32", int, "DIMENSIONS_1")
SUBDIV_CUSTOM_DATA_VARIANT(2d_i32, "GPU_COMP_I32", int, "DIMENSIONS_2")
SUBDIV_CUSTOM_DATA_VARIANT(3d_i32, "GPU_COMP_I32", int, "DIMENSIONS_3")
SUBDIV_CUSTOM_DATA_VARIANT(4d_i32, "GPU_COMP_I32", int, "DIMENSIONS_4")
SUBDIV_CUSTOM_DATA_VARIANT(1d_f32, "GPU_COMP_F32", float, "DIMENSIONS_1")
SUBDIV_CUSTOM_DATA_VARIANT(2d_f32, "GPU_COMP_F32", float, "DIMENSIONS_2")
SUBDIV_CUSTOM_DATA_VARIANT(3d_f32, "GPU_COMP_F32", float, "DIMENSIONS_3")
SUBDIV_CUSTOM_DATA_VARIANT(4d_f32, "GPU_COMP_F32", float, "DIMENSIONS_4")

#undef SUBDIV_CUSTOM_DATA_VARIANT

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt data
 * \{ */

GPU_SHADER_CREATE_INFO(subdiv_sculpt_data)
DO_STATIC_COMPILATION()
STORAGE_BUF(SCULPT_DATA_SCULPT_MASK_BUF_SLOT, READ, float, sculpt_mask[])
STORAGE_BUF(SCULPT_DATA_SCULPT_FACE_SET_COLOR_BUF_SLOT, READ, uint, sculpt_face_set_color[])
STORAGE_BUF(SCULPT_DATA_SCULPT_DATA_BUF_SLOT, WRITE, SculptData, sculpt_data[])
COMPUTE_SOURCE("subdiv_vbo_sculpt_data_comp.glsl")
ADDITIONAL_INFO(subdiv_base)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Stretch overlays
 * \{ */

GPU_SHADER_CREATE_INFO(subdiv_edituv_stretch_angle)
DO_STATIC_COMPILATION()
STORAGE_BUF(STRETCH_ANGLE_POS_NOR_BUF_SLOT, READ, PosNorLoop, pos_nor[])
STORAGE_BUF(STRETCH_ANGLE_UVS_BUF_SLOT, READ, packed_float2, uvs[])
STORAGE_BUF(STRETCH_ANGLE_UV_STRETCHES_BUF_SLOT, WRITE, UVStretchAngle, uv_stretches[])
COMPUTE_SOURCE("subdiv_vbo_edituv_strech_angle_comp.glsl")
ADDITIONAL_INFO(subdiv_base)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_edituv_stretch_area)
DO_STATIC_COMPILATION()
STORAGE_BUF(STRETCH_AREA_COARSE_STRETCH_AREA_BUF_SLOT, READ, float, coarse_stretch_area[])
STORAGE_BUF(STRETCH_AREA_SUBDIV_STRETCH_AREA_BUF_SLOT, WRITE, float, subdiv_stretch_area[])
COMPUTE_SOURCE("subdiv_vbo_edituv_strech_area_comp.glsl")
ADDITIONAL_INFO(subdiv_polygon_offset_base)
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
