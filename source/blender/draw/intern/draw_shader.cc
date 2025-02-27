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

namespace blender::draw::Shader {

class ShaderCache {
  static gpu::StaticShaderCache<ShaderCache> &get_static_cache()
  {
    static gpu::StaticShaderCache<ShaderCache> static_cache;
    return static_cache;
  }

 public:
  static ShaderCache &get()
  {
    return get_static_cache().get();
  }
  static void release()
  {
    get_static_cache().release();
  }

  gpu::StaticShader hair_refine = {"draw_hair_refine_compute"};
  gpu::StaticShader debug_draw_display = {"draw_debug_draw_display"};
  gpu::StaticShader draw_visibility_compute = {"draw_visibility_compute"};
  gpu::StaticShader draw_view_finalize = {"draw_view_finalize"};
  gpu::StaticShader draw_resource_finalize = {"draw_resource_finalize"};
  gpu::StaticShader draw_command_generate = {"draw_command_generate"};
};

}  // namespace blender::draw::Shader

using namespace blender::draw::Shader;

GPUShader *DRW_shader_hair_refine_get(ParticleRefineShader /*refinement*/)
{
  return ShaderCache::get().hair_refine.get();
}

GPUShader *DRW_shader_curves_refine_get(blender::draw::CurvesEvalShader /*type*/)
{
  /* TODO: Implement curves evaluation types (Bezier and Catmull Rom). */
  return ShaderCache::get().hair_refine.get();
}

GPUShader *DRW_shader_debug_draw_display_get()
{
  return ShaderCache::get().debug_draw_display.get();
}

GPUShader *DRW_shader_draw_visibility_compute_get()
{
  return ShaderCache::get().draw_visibility_compute.get();
}

GPUShader *DRW_shader_draw_view_finalize_get()
{
  return ShaderCache::get().draw_view_finalize.get();
}

GPUShader *DRW_shader_draw_resource_finalize_get()
{
  return ShaderCache::get().draw_resource_finalize.get();
}

GPUShader *DRW_shader_draw_command_generate_get()
{
  return ShaderCache::get().draw_command_generate.get();
}

/* -------------------------------------------------------------------- */
/** \name Subdivision
 * \{ */

#define SHADER_CUSTOM_DATA_INTERP_MAX_DIMENSIONS 4
static struct {
  GPUShader *subdiv_sh[SUBDIVISION_MAX_SHADERS];
  GPUShader *subdiv_custom_data_sh[SHADER_CUSTOM_DATA_INTERP_MAX_DIMENSIONS][GPU_COMP_MAX];
} e_data = {{nullptr}};

static blender::StringRefNull get_subdiv_shader_info_name(SubdivShaderType shader_type)
{
  switch (shader_type) {
    case SubdivShaderType::BUFFER_LINES:
      return "subdiv_lines";

    case SubdivShaderType::BUFFER_LINES_LOOSE:
      return "subdiv_lines_loose";

    case SubdivShaderType::BUFFER_TRIS:
      return "subdiv_tris_single_material";

    case SubdivShaderType::BUFFER_TRIS_MULTIPLE_MATERIALS:
      return "subdiv_tris_multiple_materials";

    case SubdivShaderType::BUFFER_EDGE_FAC:
      if (GPU_crappy_amd_driver()) {
        return "subdiv_edge_fac_amd_legacy";
      }
      return "subdiv_edge_fac";

    case SubdivShaderType::BUFFER_SCULPT_DATA:
      return "subdiv_sculpt_data";

    case SubdivShaderType::PATCH_EVALUATION:
      return "subdiv_patch_evaluation_verts";

    case SubdivShaderType::PATCH_EVALUATION_FVAR:
      return "subdiv_patch_evaluation_fvar";

    case SubdivShaderType::PATCH_EVALUATION_FACE_DOTS:
      return "subdiv_patch_evaluation_fdots";

    case SubdivShaderType::PATCH_EVALUATION_FACE_DOTS_WITH_NORMALS:
      return "subdiv_patch_evaluation_fdots_normals";

    case SubdivShaderType::PATCH_EVALUATION_ORCO:
      return "subdiv_patch_evaluation_verts_orcos";

    case SubdivShaderType::BUFFER_UV_STRETCH_ANGLE:
      return "subdiv_edituv_stretch_angle";

    case SubdivShaderType::BUFFER_UV_STRETCH_AREA:
      return "subdiv_edituv_stretch_area";

    case SubdivShaderType::BUFFER_NORMALS_ACCUMULATE:
      return "subdiv_normals_accumulate";

    case SubdivShaderType::BUFFER_NORMALS_FINALIZE:
      return "subdiv_normals_finalize";

    case SubdivShaderType::BUFFER_CUSTOM_NORMALS_FINALIZE:
      return "subdiv_custom_normals_finalize";

    case SubdivShaderType::BUFFER_LNOR:
      return "subdiv_loop_normals";

    case SubdivShaderType::COMP_CUSTOM_DATA_INTERP:
      break;

    default:
      break;
  }
  BLI_assert_unreachable();
  return "";
}

GPUShader *DRW_shader_subdiv_get(SubdivShaderType shader_type)
{
  BLI_assert(!ELEM(shader_type, SubdivShaderType::COMP_CUSTOM_DATA_INTERP));

  if (e_data.subdiv_sh[uint(shader_type)] == nullptr) {
    blender::StringRefNull create_info_name = get_subdiv_shader_info_name(shader_type);
    e_data.subdiv_sh[uint(shader_type)] = GPU_shader_create_from_info_name(
        create_info_name.c_str());
    BLI_assert(e_data.subdiv_sh[uint(shader_type)] != nullptr);
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
    std::string info_name = "subdiv_custom_data_interp";
    switch (dimensions) {
      case 1:
        info_name += "_1d";
        break;
      case 2:
        info_name += "_2d";
        break;
      case 3:
        info_name += "_3d";
        break;
      case 4:
        info_name += "_4d";
        break;
      default:
        BLI_assert_unreachable();
    }

    switch (comp_type) {
      case GPU_COMP_U16:
        info_name += "_u16";
        break;
      case GPU_COMP_I32:
        info_name += "_i32";
        break;
      case GPU_COMP_F32:
        info_name += "_f32";
        break;
      default:
        BLI_assert_unreachable();
    }
    shader = GPU_shader_create_from_info_name(info_name.c_str());
  }
  return shader;
}

/** \} */

void DRW_shaders_free()
{
  ShaderCache::release();

  for (int i = 0; i < SUBDIVISION_MAX_SHADERS; i++) {
    GPU_SHADER_FREE_SAFE(e_data.subdiv_sh[i]);
  }
  for (int i = 0; i < SHADER_CUSTOM_DATA_INTERP_MAX_DIMENSIONS; i++) {
    for (int j = 0; j < GPU_COMP_MAX; j++) {
      GPU_SHADER_FREE_SAFE(e_data.subdiv_custom_data_sh[i][j]);
    }
  }
}
