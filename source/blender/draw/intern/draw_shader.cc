/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "draw_shader.hh"

#include "GPU_batch.hh"
#include "GPU_capabilities.hh"

#include "opensubdiv_capi_type.hh"
#include "opensubdiv_evaluator_capi.hh"

#include "DRW_render.hh"

extern "C" char datatoc_common_hair_lib_glsl[];
extern "C" char datatoc_subdiv_custom_data_interp_comp_glsl[];
extern "C" char datatoc_subdiv_ibo_lines_comp_glsl[];
extern "C" char datatoc_subdiv_ibo_tris_comp_glsl[];
extern "C" char datatoc_subdiv_lib_glsl[];
extern "C" char datatoc_subdiv_normals_accumulate_comp_glsl[];
extern "C" char datatoc_subdiv_normals_finalize_comp_glsl[];
extern "C" char datatoc_subdiv_patch_evaluation_comp_glsl[];
extern "C" char datatoc_subdiv_vbo_edge_fac_comp_glsl[];
extern "C" char datatoc_subdiv_vbo_lnor_comp_glsl[];
extern "C" char datatoc_subdiv_vbo_sculpt_data_comp_glsl[];
extern "C" char datatoc_subdiv_vbo_edituv_strech_angle_comp_glsl[];
extern "C" char datatoc_subdiv_vbo_edituv_strech_area_comp_glsl[];

#define SHADER_CUSTOM_DATA_INTERP_MAX_DIMENSIONS 4
static struct {
  GPUShader *hair_refine_sh[PART_REFINE_MAX_SHADER];
  GPUShader *debug_print_display_sh;
  GPUShader *debug_draw_display_sh;
  GPUShader *draw_visibility_compute_sh;
  GPUShader *draw_view_finalize_sh;
  GPUShader *draw_resource_finalize_sh;
  GPUShader *draw_command_generate_sh;

  GPUShader *subdiv_sh[SUBDIVISION_MAX_SHADERS];
  GPUShader *subdiv_custom_data_sh[SHADER_CUSTOM_DATA_INTERP_MAX_DIMENSIONS][GPU_COMP_MAX];
} e_data = {{nullptr}};

/* -------------------------------------------------------------------- */
/** \name Hair refinement
 * \{ */

static GPUShader *hair_refine_shader_compute_create(ParticleRefineShader /*refinement*/)
{
  return GPU_shader_create_from_info_name("draw_hair_refine_compute");
}

GPUShader *DRW_shader_hair_refine_get(ParticleRefineShader refinement)
{
  if (e_data.hair_refine_sh[refinement] == nullptr) {
    GPUShader *sh = hair_refine_shader_compute_create(refinement);
    e_data.hair_refine_sh[refinement] = sh;
  }

  return e_data.hair_refine_sh[refinement];
}

GPUShader *DRW_shader_curves_refine_get(blender::draw::CurvesEvalShader type)
{
  /* TODO: Implement curves evaluation types (Bezier and Catmull Rom). */
  if (e_data.hair_refine_sh[type] == nullptr) {
    GPUShader *sh = hair_refine_shader_compute_create(PART_REFINE_CATMULL_ROM);
    e_data.hair_refine_sh[type] = sh;
  }

  return e_data.hair_refine_sh[type];
}

GPUShader *DRW_shader_debug_draw_display_get()
{
  if (e_data.debug_draw_display_sh == nullptr) {
    e_data.debug_draw_display_sh = GPU_shader_create_from_info_name("draw_debug_draw_display");
  }
  return e_data.debug_draw_display_sh;
}

GPUShader *DRW_shader_draw_visibility_compute_get()
{
  if (e_data.draw_visibility_compute_sh == nullptr) {
    e_data.draw_visibility_compute_sh = GPU_shader_create_from_info_name(
        "draw_visibility_compute");
  }
  return e_data.draw_visibility_compute_sh;
}

GPUShader *DRW_shader_draw_view_finalize_get()
{
  if (e_data.draw_view_finalize_sh == nullptr) {
    e_data.draw_view_finalize_sh = GPU_shader_create_from_info_name("draw_view_finalize");
  }
  return e_data.draw_view_finalize_sh;
}

GPUShader *DRW_shader_draw_resource_finalize_get()
{
  if (e_data.draw_resource_finalize_sh == nullptr) {
    e_data.draw_resource_finalize_sh = GPU_shader_create_from_info_name("draw_resource_finalize");
  }
  return e_data.draw_resource_finalize_sh;
}

GPUShader *DRW_shader_draw_command_generate_get()
{
  if (e_data.draw_command_generate_sh == nullptr) {
    e_data.draw_command_generate_sh = GPU_shader_create_from_info_name("draw_command_generate");
  }
  return e_data.draw_command_generate_sh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Subdivision
 * \{ */

static blender::StringRefNull get_subdiv_shader_info_name(SubdivShaderType shader_type)
{
  switch (shader_type) {
    case SubdivShaderType::BUFFER_NORMALS_FINALIZE:
      return "subdiv_normals_finalize";

    case SubdivShaderType::BUFFER_CUSTOM_NORMALS_FINALIZE:
      return "subdiv_custom_normals_finalize";

    default:
      break;
  }
  BLI_assert_unreachable();
  return "";
}
static blender::StringRefNull get_subdiv_shader_name(SubdivShaderType shader_type)
{
  switch (shader_type) {
    case SubdivShaderType::BUFFER_LINES: {
      return "subdiv lines build";
    }
    case SubdivShaderType::BUFFER_LINES_LOOSE: {
      return "subdiv lines loose build";
    }
    case SubdivShaderType::BUFFER_LNOR: {
      return "subdiv lnor build";
    }
    case SubdivShaderType::BUFFER_EDGE_FAC: {
      return "subdiv edge fac build";
    }
    case SubdivShaderType::BUFFER_TRIS:
    case SubdivShaderType::BUFFER_TRIS_MULTIPLE_MATERIALS: {
      return "subdiv tris";
    }
    case SubdivShaderType::BUFFER_NORMALS_ACCUMULATE: {
      return "subdiv normals accumulate";
    }
    case SubdivShaderType::BUFFER_NORMALS_FINALIZE: {
      return "subdiv normals finalize";
    }
    case SubdivShaderType::BUFFER_CUSTOM_NORMALS_FINALIZE: {
      return "subdiv custom normals finalize";
    }
    case SubdivShaderType::PATCH_EVALUATION: {
      return "subdiv patch evaluation";
    }
    case SubdivShaderType::PATCH_EVALUATION_FVAR: {
      return "subdiv patch evaluation face-varying";
    }
    case SubdivShaderType::PATCH_EVALUATION_FACE_DOTS: {
      return "subdiv patch evaluation face dots";
    }
    case SubdivShaderType::PATCH_EVALUATION_FACE_DOTS_WITH_NORMALS: {
      return "subdiv patch evaluation face dots with normals";
    }
    case SubdivShaderType::PATCH_EVALUATION_ORCO: {
      return "subdiv patch evaluation orco";
    }
    case SubdivShaderType::COMP_CUSTOM_DATA_INTERP_1D: {
      return "subdiv custom data interp 1D";
    }
    case SubdivShaderType::COMP_CUSTOM_DATA_INTERP_2D: {
      return "subdiv custom data interp 2D";
    }
    case SubdivShaderType::COMP_CUSTOM_DATA_INTERP_3D: {
      return "subdiv custom data interp 3D";
    }
    case SubdivShaderType::COMP_CUSTOM_DATA_INTERP_4D: {
      return "subdiv custom data interp 4D";
    }
    case SubdivShaderType::BUFFER_SCULPT_DATA: {
      return "subdiv sculpt data";
    }
    case SubdivShaderType::BUFFER_UV_STRETCH_ANGLE: {
      return "subdiv uv stretch angle";
    }
    case SubdivShaderType::BUFFER_UV_STRETCH_AREA: {
      return "subdiv uv stretch area";
    }
  }
  BLI_assert_unreachable();
  return "";
}
static blender::StringRefNull get_subdiv_shader_code(SubdivShaderType shader_type)
{
  switch (shader_type) {
    case SubdivShaderType::BUFFER_LINES:
    case SubdivShaderType::BUFFER_LINES_LOOSE: {
      return datatoc_subdiv_ibo_lines_comp_glsl;
    }
    case SubdivShaderType::BUFFER_EDGE_FAC: {
      return datatoc_subdiv_vbo_edge_fac_comp_glsl;
    }
    case SubdivShaderType::BUFFER_LNOR: {
      return datatoc_subdiv_vbo_lnor_comp_glsl;
    }
    case SubdivShaderType::BUFFER_TRIS:
    case SubdivShaderType::BUFFER_TRIS_MULTIPLE_MATERIALS: {
      return datatoc_subdiv_ibo_tris_comp_glsl;
    }
    case SubdivShaderType::BUFFER_NORMALS_ACCUMULATE: {
      return datatoc_subdiv_normals_accumulate_comp_glsl;
    }
    case SubdivShaderType::BUFFER_NORMALS_FINALIZE:
    case SubdivShaderType::BUFFER_CUSTOM_NORMALS_FINALIZE: {
      return datatoc_subdiv_normals_finalize_comp_glsl;
    }
    case SubdivShaderType::PATCH_EVALUATION:
    case SubdivShaderType::PATCH_EVALUATION_FVAR:
    case SubdivShaderType::PATCH_EVALUATION_FACE_DOTS:
    case SubdivShaderType::PATCH_EVALUATION_FACE_DOTS_WITH_NORMALS:
    case SubdivShaderType::PATCH_EVALUATION_ORCO: {
      return datatoc_subdiv_patch_evaluation_comp_glsl;
    }
    case SubdivShaderType::COMP_CUSTOM_DATA_INTERP_1D:
    case SubdivShaderType::COMP_CUSTOM_DATA_INTERP_2D:
    case SubdivShaderType::COMP_CUSTOM_DATA_INTERP_3D:
    case SubdivShaderType::COMP_CUSTOM_DATA_INTERP_4D: {
      return datatoc_subdiv_custom_data_interp_comp_glsl;
    }
    case SubdivShaderType::BUFFER_SCULPT_DATA: {
      return datatoc_subdiv_vbo_sculpt_data_comp_glsl;
    }
    case SubdivShaderType::BUFFER_UV_STRETCH_ANGLE: {
      return datatoc_subdiv_vbo_edituv_strech_angle_comp_glsl;
    }
    case SubdivShaderType::BUFFER_UV_STRETCH_AREA: {
      return datatoc_subdiv_vbo_edituv_strech_area_comp_glsl;
    }
  }
  BLI_assert_unreachable();
  return "";
}

static GPUShader *draw_shader_subdiv_patch_evaluation_get(SubdivShaderType shader_type)
{
  if (e_data.subdiv_sh[uint(shader_type)] == nullptr) {
    const blender::StringRefNull compute_code = get_subdiv_shader_code(shader_type);

    std::optional<blender::StringRefNull> defines;
    if (shader_type == SubdivShaderType::PATCH_EVALUATION) {
      defines =
          "#define OSD_PATCH_BASIS_GLSL\n"
          "#define OPENSUBDIV_GLSL_COMPUTE_USE_1ST_DERIVATIVES\n";
    }
    else if (shader_type == SubdivShaderType::PATCH_EVALUATION_FVAR) {
      defines =
          "#define OSD_PATCH_BASIS_GLSL\n"
          "#define OPENSUBDIV_GLSL_COMPUTE_USE_1ST_DERIVATIVES\n"
          "#define FVAR_EVALUATION\n";
    }
    else if (shader_type == SubdivShaderType::PATCH_EVALUATION_FACE_DOTS) {
      defines =
          "#define OSD_PATCH_BASIS_GLSL\n"
          "#define OPENSUBDIV_GLSL_COMPUTE_USE_1ST_DERIVATIVES\n"
          "#define FDOTS_EVALUATION\n";
    }
    else if (shader_type == SubdivShaderType::PATCH_EVALUATION_FACE_DOTS_WITH_NORMALS) {
      defines =
          "#define OSD_PATCH_BASIS_GLSL\n"
          "#define OPENSUBDIV_GLSL_COMPUTE_USE_1ST_DERIVATIVES\n"
          "#define FDOTS_EVALUATION\n"
          "#define FDOTS_NORMALS\n";
    }
    else if (shader_type == SubdivShaderType::PATCH_EVALUATION_ORCO) {
      defines =
          "#define OSD_PATCH_BASIS_GLSL\n"
          "#define OPENSUBDIV_GLSL_COMPUTE_USE_1ST_DERIVATIVES\n"
          "#define ORCO_EVALUATION\n";
    }
    else {
      BLI_assert_unreachable();
    }

    /* Merge OpenSubdiv library code with our own library code. */
    const blender::StringRefNull patch_basis_source = openSubdiv_getGLSLPatchBasisSource();
    const blender::StringRefNull subdiv_lib_code = datatoc_subdiv_lib_glsl;
    std::string library_code = patch_basis_source + subdiv_lib_code;
    e_data.subdiv_sh[uint(shader_type)] = GPU_shader_create_compute(
        compute_code, library_code, defines, get_subdiv_shader_name(shader_type));
  }

  return e_data.subdiv_sh[uint(shader_type)];
}

GPUShader *DRW_shader_subdiv_get(SubdivShaderType shader_type)
{
  if (ELEM(shader_type,
           SubdivShaderType::PATCH_EVALUATION,
           SubdivShaderType::PATCH_EVALUATION_FVAR,
           SubdivShaderType::PATCH_EVALUATION_FACE_DOTS,
           SubdivShaderType::PATCH_EVALUATION_FACE_DOTS_WITH_NORMALS,
           SubdivShaderType::PATCH_EVALUATION_ORCO))
  {
    return draw_shader_subdiv_patch_evaluation_get(shader_type);
  }

  BLI_assert(!ELEM(shader_type,
                   SubdivShaderType::COMP_CUSTOM_DATA_INTERP_1D,
                   SubdivShaderType::COMP_CUSTOM_DATA_INTERP_2D,
                   SubdivShaderType::COMP_CUSTOM_DATA_INTERP_3D,
                   SubdivShaderType::COMP_CUSTOM_DATA_INTERP_4D));

  if (e_data.subdiv_sh[uint(shader_type)] == nullptr &&
      ELEM(shader_type,
           SubdivShaderType::BUFFER_NORMALS_FINALIZE,
           SubdivShaderType::BUFFER_CUSTOM_NORMALS_FINALIZE))
  {
    blender::StringRefNull create_info_name = get_subdiv_shader_info_name(shader_type);
    e_data.subdiv_sh[uint(shader_type)] = GPU_shader_create_from_info_name(
        create_info_name.c_str());
    BLI_assert(e_data.subdiv_sh[uint(shader_type)] != nullptr);
  }
  else if (e_data.subdiv_sh[uint(shader_type)] == nullptr) {
    const blender::StringRefNull compute_code = get_subdiv_shader_code(shader_type);
    std::optional<blender::StringRefNull> defines;

    if (ELEM(shader_type,
             SubdivShaderType::BUFFER_LINES,
             SubdivShaderType::BUFFER_LNOR,
             SubdivShaderType::BUFFER_TRIS_MULTIPLE_MATERIALS,
             SubdivShaderType::BUFFER_UV_STRETCH_AREA))
    {
      defines = "#define SUBDIV_POLYGON_OFFSET\n";
    }
    else if (shader_type == SubdivShaderType::BUFFER_TRIS) {
      defines =
          "#define SUBDIV_POLYGON_OFFSET\n"
          "#define SINGLE_MATERIAL\n";
    }
    else if (shader_type == SubdivShaderType::BUFFER_LINES_LOOSE) {
      defines = "#define LINES_LOOSE\n";
    }
    else if (shader_type == SubdivShaderType::BUFFER_EDGE_FAC) {
      /* No separate shader for the AMD driver case as we assume that the GPU will not change
       * during the execution of the program. */
      if (GPU_crappy_amd_driver()) {
        defines = "#define GPU_AMD_DRIVER_BYTE_BUG\n";
      }
    }
    else if (shader_type == SubdivShaderType::BUFFER_CUSTOM_NORMALS_FINALIZE) {
      defines = "#define CUSTOM_NORMALS\n";
    }

    e_data.subdiv_sh[uint(shader_type)] = GPU_shader_create_compute(
        compute_code, datatoc_subdiv_lib_glsl, defines, get_subdiv_shader_name(shader_type));
  }
  return e_data.subdiv_sh[uint(shader_type)];
}

GPUShader *DRW_shader_subdiv_custom_data_get(GPUVertCompType comp_type, int dimensions)
{
  BLI_assert(dimensions >= 1 && dimensions <= SHADER_CUSTOM_DATA_INTERP_MAX_DIMENSIONS);
  if (comp_type == GPU_COMP_U16) {
    BLI_assert(dimensions == 4);
  }

  GPUShader *&shader = e_data.subdiv_custom_data_sh[dimensions - 1][comp_type];

  if (shader == nullptr) {
    SubdivShaderType shader_type = SubdivShaderType(
        uint(SubdivShaderType::COMP_CUSTOM_DATA_INTERP_1D) + dimensions - 1);
    const blender::StringRefNull compute_code = get_subdiv_shader_code(shader_type);

    std::string defines = "#define SUBDIV_POLYGON_OFFSET\n";
    defines += "#define DIMENSIONS " + std::to_string(dimensions) + "\n";
    switch (comp_type) {
      case GPU_COMP_U16:
        defines += "#define GPU_COMP_U16\n";
        break;
      case GPU_COMP_I32:
        defines += "#define GPU_COMP_I32\n";
        break;
      case GPU_COMP_F32:
        /* float is the default */
        break;
      default:
        BLI_assert_unreachable();
        break;
    }

    shader = GPU_shader_create_compute(
        compute_code, datatoc_subdiv_lib_glsl, defines, get_subdiv_shader_name(shader_type));
  }
  return shader;
}

/** \} */

void DRW_shaders_free()
{
  for (int i = 0; i < PART_REFINE_MAX_SHADER; i++) {
    GPU_SHADER_FREE_SAFE(e_data.hair_refine_sh[i]);
  }
  GPU_SHADER_FREE_SAFE(e_data.debug_print_display_sh);
  GPU_SHADER_FREE_SAFE(e_data.debug_draw_display_sh);
  GPU_SHADER_FREE_SAFE(e_data.draw_visibility_compute_sh);
  GPU_SHADER_FREE_SAFE(e_data.draw_view_finalize_sh);
  GPU_SHADER_FREE_SAFE(e_data.draw_resource_finalize_sh);
  GPU_SHADER_FREE_SAFE(e_data.draw_command_generate_sh);

  for (int i = 0; i < SUBDIVISION_MAX_SHADERS; i++) {
    GPU_SHADER_FREE_SAFE(e_data.subdiv_sh[i]);
  }
  for (int i = 0; i < SHADER_CUSTOM_DATA_INTERP_MAX_DIMENSIONS; i++) {
    for (int j = 0; j < GPU_COMP_MAX; j++) {
      GPU_SHADER_FREE_SAFE(e_data.subdiv_custom_data_sh[i][j]);
    }
  }
}
