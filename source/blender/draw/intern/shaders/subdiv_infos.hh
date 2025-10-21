/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "BLI_utildefines_variadic.h"

#  include "gpu_shader_compat.hh"

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
STORAGE_BUF(SUBDIV_FACE_OFFSET_BUF_SLOT, read, uint, subdiv_face_offset[])
ADDITIONAL_INFO(subdiv_base)
GPU_SHADER_CREATE_END()

/* -------------------------------------------------------------------- */
/** \name Patch evaluation
 * \{ */

GPU_SHADER_CREATE_INFO(subdiv_patch_evaluation_basis)
#ifdef __APPLE__
/* Match definition from OPenSubdiv which defines OSD_PATCH_BASIS_METAL as 1. Matching it here
 * avoids possible re-definition warning at runtime. */
DEFINE_VALUE("OSD_PATCH_BASIS_METAL", "1")
#else
DEFINE("OSD_PATCH_BASIS_GLSL")
#endif
DEFINE("OPENSUBDIV_GLSL_COMPUTE_USE_1ST_DERIVATIVES")
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

/* -------------------------------------------------------------------- */
/** \name Loop Normals
 * \{ */

GPU_SHADER_CREATE_INFO(subdiv_loop_normals)
DO_STATIC_COMPILATION()
STORAGE_BUF(LOOP_NORMALS_POS_SLOT, read, Position, positions[])
STORAGE_BUF(LOOP_NORMALS_EXTRA_COARSE_FACE_DATA_BUF_SLOT, read, uint, extra_coarse_face_data[])
STORAGE_BUF(LOOP_NORMALS_VERT_NORMALS_BUF_SLOT, read, Normal, vert_normals[])
STORAGE_BUF(LOOP_NORMALS_VERTEX_LOOP_MAP_BUF_SLOT, read, uint, vert_loop_map[])
STORAGE_BUF(LOOP_NORMALS_OUTPUT_LNOR_BUF_SLOT, write, Normal, output_lnor[])
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
STORAGE_BUF(TRIS_EXTRA_COARSE_FACE_DATA_BUF_SLOT, read, uint, extra_coarse_face_data[])
STORAGE_BUF(TRIS_OUTPUT_TRIS_BUF_SLOT, write, uint, output_tris[])
COMPUTE_SOURCE("subdiv_ibo_tris_comp.glsl")
ADDITIONAL_INFO(subdiv_polygon_offset_base)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_tris_multiple_materials)
DO_STATIC_COMPILATION()
STORAGE_BUF(TRIS_EXTRA_COARSE_FACE_DATA_BUF_SLOT, read, uint, extra_coarse_face_data[])
STORAGE_BUF(TRIS_OUTPUT_TRIS_BUF_SLOT, write, uint, output_tris[])
STORAGE_BUF(TRIS_FACE_MAT_OFFSET, read, uint, face_mat_offset[])
COMPUTE_SOURCE("subdiv_ibo_tris_comp.glsl")
ADDITIONAL_INFO(subdiv_polygon_offset_base)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Line indices
 * \{ */

GPU_SHADER_CREATE_INFO(subdiv_lines)
DO_STATIC_COMPILATION()
STORAGE_BUF(LINES_INPUT_EDGE_DRAW_FLAG_BUF_SLOT, read, int, input_edge_draw_flag[])
STORAGE_BUF(LINES_EXTRA_COARSE_FACE_DATA_BUF_SLOT, read, uint, extra_coarse_face_data[])
STORAGE_BUF(LINES_OUTPUT_LINES_BUF_SLOT, write, uint, output_lines[])
COMPUTE_SOURCE("subdiv_ibo_lines_comp.glsl")
ADDITIONAL_INFO(subdiv_polygon_offset_base)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_lines_loose)
DO_STATIC_COMPILATION()
DEFINE("LINES_LOOSE")
STORAGE_BUF(LINES_OUTPUT_LINES_BUF_SLOT, write, uint, output_lines[])
STORAGE_BUF(LINES_LINES_LOOSE_FLAGS, read, uint, lines_loose_flags[])
COMPUTE_SOURCE("subdiv_ibo_lines_comp.glsl")
ADDITIONAL_INFO(subdiv_base)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge data for object mode wireframe
 * \{ */

GPU_SHADER_CREATE_INFO(subdiv_edge_fac)
ADDITIONAL_INFO(subdiv_base)
DO_STATIC_COMPILATION()
STORAGE_BUF(EDGE_FAC_POS_BUF_SLOT, read, Position, positions[])
STORAGE_BUF(EDGE_FAC_EDGE_DRAW_FLAG_BUF_SLOT, read, uint, input_edge_draw_flag[])
STORAGE_BUF(EDGE_FAC_POLY_OTHER_MAP_BUF_SLOT, read, int, input_poly_other_map[])
STORAGE_BUF(EDGE_FAC_EDGE_FAC_BUF_SLOT, write, float, output_edge_fac[])
COMPUTE_SOURCE("subdiv_vbo_edge_fac_comp.glsl")
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Custom data
 * \{ */

GPU_SHADER_CREATE_INFO(subdiv_custom_data_interp_base)
COMPUTE_SOURCE("subdiv_custom_data_interp_comp.glsl")
STORAGE_BUF(CUSTOM_DATA_FACE_PTEX_OFFSET_BUF_SLOT, read, uint, face_ptex_offset[])
STORAGE_BUF(CUSTOM_DATA_PATCH_COORDS_BUF_SLOT, read, BlenderPatchCoord, patch_coords[])
STORAGE_BUF(CUSTOM_DATA_EXTRA_COARSE_FACE_DATA_BUF_SLOT, read, uint, extra_coarse_face_data[])
ADDITIONAL_INFO(subdiv_polygon_offset_base)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_data_uint)
DEFINE("GPU_COMP_U16")
STORAGE_BUF(CUSTOM_DATA_SOURCE_DATA_BUF_SLOT, read, uint, src_data[])
STORAGE_BUF(CUSTOM_DATA_DESTINATION_DATA_BUF_SLOT, write, uint, dst_data[])
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_data_int)
DEFINE("GPU_COMP_I32")
STORAGE_BUF(CUSTOM_DATA_SOURCE_DATA_BUF_SLOT, read, int, src_data[])
STORAGE_BUF(CUSTOM_DATA_DESTINATION_DATA_BUF_SLOT, write, int, dst_data[])
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_data_float)
DEFINE("GPU_COMP_F32")
STORAGE_BUF(CUSTOM_DATA_SOURCE_DATA_BUF_SLOT, read, float, src_data[])
STORAGE_BUF(CUSTOM_DATA_DESTINATION_DATA_BUF_SLOT, write, float, dst_data[])
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_dimension_1)
DEFINE("DIMENSIONS_1")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_dimension_2)
DEFINE("DIMENSIONS_2")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_dimension_3)
DEFINE("DIMENSIONS_3")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_dimension_4)
DEFINE("DIMENSIONS_4")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_normalize)
DEFINE("NORMALIZE")
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(subdiv_custom_data_interp_4d_u16, subdiv_custom_data_interp_base, subdiv_data_uint, subdiv_dimension_4)
CREATE_INFO_VARIANT(subdiv_custom_data_interp_1d_i32, subdiv_custom_data_interp_base, subdiv_data_int, subdiv_dimension_1)
CREATE_INFO_VARIANT(subdiv_custom_data_interp_2d_i32, subdiv_custom_data_interp_base, subdiv_data_int, subdiv_dimension_2)
CREATE_INFO_VARIANT(subdiv_custom_data_interp_3d_i32, subdiv_custom_data_interp_base, subdiv_data_int, subdiv_dimension_3)
CREATE_INFO_VARIANT(subdiv_custom_data_interp_4d_i32, subdiv_custom_data_interp_base, subdiv_data_int, subdiv_dimension_4)
CREATE_INFO_VARIANT(subdiv_custom_data_interp_1d_f32, subdiv_custom_data_interp_base, subdiv_data_float, subdiv_dimension_1)
CREATE_INFO_VARIANT(subdiv_custom_data_interp_2d_f32, subdiv_custom_data_interp_base, subdiv_data_float, subdiv_dimension_2)
CREATE_INFO_VARIANT(subdiv_custom_data_interp_3d_f32, subdiv_custom_data_interp_base, subdiv_data_float, subdiv_dimension_3)
CREATE_INFO_VARIANT(subdiv_custom_data_interp_4d_f32, subdiv_custom_data_interp_base, subdiv_data_float, subdiv_dimension_4)
/* clang-format on */

CREATE_INFO_VARIANT(subdiv_custom_data_interp_3d_f32_normalize,
                    subdiv_custom_data_interp_base,
                    subdiv_data_float,
                    subdiv_dimension_3,
                    subdiv_normalize)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt data
 * \{ */

GPU_SHADER_CREATE_INFO(subdiv_sculpt_data)
DO_STATIC_COMPILATION()
STORAGE_BUF(SCULPT_DATA_SCULPT_MASK_BUF_SLOT, read, float, sculpt_mask[])
STORAGE_BUF(SCULPT_DATA_SCULPT_FACE_SET_COLOR_BUF_SLOT, read, uint, sculpt_face_set_color[])
STORAGE_BUF(SCULPT_DATA_SCULPT_DATA_BUF_SLOT, write, SculptData, sculpt_data[])
COMPUTE_SOURCE("subdiv_vbo_sculpt_data_comp.glsl")
ADDITIONAL_INFO(subdiv_base)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Stretch overlays
 * \{ */

GPU_SHADER_CREATE_INFO(subdiv_edituv_stretch_angle)
DO_STATIC_COMPILATION()
STORAGE_BUF(STRETCH_ANGLE_POS_BUF_SLOT, read, Position, positions[])
STORAGE_BUF(STRETCH_ANGLE_UVS_BUF_SLOT, read, packed_float2, uvs[])
STORAGE_BUF(STRETCH_ANGLE_UV_STRETCHES_BUF_SLOT, write, UVStretchAngle, uv_stretches[])
COMPUTE_SOURCE("subdiv_vbo_edituv_strech_angle_comp.glsl")
ADDITIONAL_INFO(subdiv_base)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(subdiv_edituv_stretch_area)
DO_STATIC_COMPILATION()
STORAGE_BUF(STRETCH_AREA_COARSE_STRETCH_AREA_BUF_SLOT, read, float, coarse_stretch_area[])
STORAGE_BUF(STRETCH_AREA_SUBDIV_STRETCH_AREA_BUF_SLOT, write, float, subdiv_stretch_area[])
COMPUTE_SOURCE("subdiv_vbo_edituv_strech_area_comp.glsl")
ADDITIONAL_INFO(subdiv_polygon_offset_base)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Normals
 * \{ */

GPU_SHADER_CREATE_INFO(subdiv_normals_accumulate)
DO_STATIC_COMPILATION()
STORAGE_BUF(NORMALS_ACCUMULATE_POS_BUF_SLOT, read, Position, positions[])
STORAGE_BUF(NORMALS_ACCUMULATE_FACE_ADJACENCY_OFFSETS_BUF_SLOT,
            read,
            uint,
            face_adjacency_offsets[])
STORAGE_BUF(NORMALS_ACCUMULATE_FACE_ADJACENCY_LISTS_BUF_SLOT, read, uint, face_adjacency_lists[])
STORAGE_BUF(NORMALS_ACCUMULATE_VERTEX_LOOP_MAP_BUF_SLOT, read, uint, vert_loop_map[])
STORAGE_BUF(NORMALS_ACCUMULATE_NORMALS_BUF_SLOT, write, Normal, vert_normals[])
COMPUTE_SOURCE("subdiv_normals_accumulate_comp.glsl")
ADDITIONAL_INFO(subdiv_base)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paint Overlay Flag
 * \{ */

GPU_SHADER_CREATE_INFO(subdiv_paint_overlay_flag)
DO_STATIC_COMPILATION()
STORAGE_BUF(PAINT_OVERLAY_EXTRA_COARSE_FACE_DATA_BUF_SLOT, read, uint, extra_coarse_face_data[])
STORAGE_BUF(PAINT_OVERLAY_EXTRA_INPUT_VERT_ORIG_INDEX_SLOT, read, int, input_vert_origindex[])
STORAGE_BUF(PAINT_OVERLAY_OUTPUT_FLAG_SLOT, write, int, flags[])
COMPUTE_SOURCE("subdiv_vbo_paint_overlay_flag_comp.glsl")
ADDITIONAL_INFO(subdiv_polygon_offset_base)
GPU_SHADER_CREATE_END()

/** \} */
