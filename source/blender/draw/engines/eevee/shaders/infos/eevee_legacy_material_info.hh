/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_legacy_volume_info.hh"
#include "gpu_shader_create_info.hh"

/* For EEVEE Materials prepared in `eevee_shader_material_create_info_amend`,
 * differing permutations are generated based on material options.
 *
 * Sources, e.g.
 * -> datatoc_volumetric_vert_glsl
 * -> datatoc_world_vert_glsl
 * -> datatoc_surface_vert_glsl
 *
 * Are not included in the create-infos, but should have a corresponding
 * Create info block, which defines bindings and other library requirements.
 */

/*** EMPTY EEVEE STUB COMMON INCLUDES following 'eevee_empty.glsl' and
 * 'eevee_empty_volume.glsl'****/
GPU_SHADER_CREATE_INFO(eevee_legacy_material_empty_base)
    .additional_info("eevee_legacy_closure_type_lib")
    .additional_info("eevee_legacy_common_lib")
    .additional_info("draw_modelmat")
    .additional_info("draw_view");

/* Volumetrics skips uniform bindings in `closure_type_lib`. */
GPU_SHADER_CREATE_INFO(eevee_legacy_material_empty_base_volume)
    .additional_info("eevee_legacy_common_lib")
    .additional_info("draw_modelmat")
    .additional_info("draw_view");

/**** MATERIAL VERTEX SHADER PERMUTATIONS ****/

/** -- Volumetric -- **/
GPU_SHADER_CREATE_INFO(eevee_legacy_material_volumetric_vert)
    .additional_info("eevee_legacy_material_empty_base_volume")
    .vertex_out(legacy_volume_vert_geom_iface)
    .additional_info("draw_resource_id_varying");

#ifdef WITH_METAL_BACKEND
GPU_SHADER_CREATE_INFO(eevee_legacy_material_volumetric_vert_no_geom)
    .additional_info("eevee_legacy_material_empty_base_volume")
    .vertex_out(legacy_volume_vert_geom_iface)
    .vertex_out(legacy_volume_geom_frag_iface)
    .additional_info("draw_resource_id_varying");
#endif

/** -- World Shader -- **/
GPU_SHADER_CREATE_INFO(eevee_legacy_material_world_vert)
    .additional_info("eevee_legacy_material_empty_base")
    .additional_info("eevee_legacy_common_utiltex_lib")
    .additional_info("eevee_legacy_closure_eval_surface_lib")
    .additional_info("eevee_legacy_surface_lib_common")
    .additional_info("draw_resource_id_varying")
    .vertex_in(0, Type::VEC2, "pos");

/** -- Surface Shader -- **/
GPU_SHADER_CREATE_INFO(eevee_legacy_material_surface_vert_common)
    .additional_info("eevee_legacy_material_empty_base")
    .additional_info("draw_resource_id_varying")
    .additional_info("eevee_legacy_common_utiltex_lib")
    .additional_info("eevee_legacy_closure_eval_surface_lib");

GPU_SHADER_CREATE_INFO(eevee_legacy_material_surface_vert)
    .additional_info("eevee_legacy_material_surface_vert_common")
    .additional_info("eevee_legacy_surface_lib_common")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC3, "nor");

GPU_SHADER_CREATE_INFO(eevee_legacy_mateiral_surface_vert_hair)
    .additional_info("eevee_legacy_material_surface_vert_common")
    .additional_info("eevee_legacy_surface_lib_hair")
    .additional_info("eevee_legacy_hair_lib");

GPU_SHADER_CREATE_INFO(eevee_legacy_mateiral_surface_vert_pointcloud)
    .additional_info("draw_pointcloud")
    .additional_info("eevee_legacy_material_surface_vert_common")
    .additional_info("eevee_legacy_surface_lib_pointcloud")
    .auto_resource_location(true);

/**** MATERIAL GEOMETRY SHADER PERMUTATIONS ****/
/** -- Volumetric -- **/
GPU_SHADER_CREATE_INFO(eevee_legacy_material_volumetric_geom)
    .additional_info("eevee_legacy_common_lib")
    .additional_info("draw_view")
    .geometry_out(legacy_volume_geom_frag_iface)
    .geometry_layout(PrimitiveIn::TRIANGLES, PrimitiveOut::TRIANGLE_STRIP, 3)
    .additional_info("draw_resource_id_varying");

/**** MATERIAL FRAGMENT SHADER PERMUTATIONS ****/

/** -- Volumetric Shader -- **/
GPU_SHADER_CREATE_INFO(eevee_legacy_material_volumetric_frag)
    .additional_info("eevee_legacy_common_lib")
    .additional_info("draw_view")
    .additional_info("draw_resource_id_varying")
    .additional_info("eevee_legacy_volumetric_lib")
    .fragment_out(0, Type::VEC4, "volumeScattering")
    .fragment_out(1, Type::VEC4, "volumeExtinction")
    .fragment_out(2, Type::VEC4, "volumeEmissive")
    .fragment_out(3, Type::VEC4, "volumePhase");

/** -- Prepass Shader -- **/

/* Common info for all `prepass_frag` variants. */
GPU_SHADER_CREATE_INFO(eevee_legacy_material_prepass_frag_common)
    .additional_info("eevee_legacy_common_lib")
    .additional_info("eevee_legacy_common_utiltex_lib")
    .additional_info("draw_view")
    .additional_info("eevee_legacy_closure_eval_surface_lib");

/* Common info for all `prepass_frag_opaque` variants. */
GPU_SHADER_CREATE_INFO(eevee_legacy_material_prepass_frag_opaque_common)
    .additional_info("eevee_legacy_material_prepass_frag_common");

GPU_SHADER_CREATE_INFO(eevee_legacy_material_prepass_frag_opaque)
    .additional_info("eevee_legacy_surface_lib_common")
    .additional_info("eevee_legacy_material_prepass_frag_opaque_common");

GPU_SHADER_CREATE_INFO(eevee_legacy_material_prepass_frag_opaque_hair)
    .additional_info("eevee_legacy_surface_lib_hair")
    .additional_info("eevee_legacy_material_prepass_frag_opaque_common")
    .additional_info("draw_hair");

GPU_SHADER_CREATE_INFO(eevee_legacy_material_prepass_frag_opaque_pointcloud)
    .additional_info("eevee_legacy_material_prepass_frag_opaque_common")
    .additional_info("draw_pointcloud");

/* Common info for all `prepass_frag_alpha_hash` variants. */
GPU_SHADER_CREATE_INFO(eevee_legacy_material_prepass_frag_alpha_hash_common)
    .define("USE_ALPHA_HASH")
    .additional_info("eevee_legacy_material_prepass_frag_common")
    .push_constant(Type::FLOAT, "alphaClipThreshold");

GPU_SHADER_CREATE_INFO(eevee_legacy_material_prepass_frag_alpha_hash)
    .additional_info("eevee_legacy_surface_lib_common")
    .additional_info("eevee_legacy_material_prepass_frag_alpha_hash_common");

GPU_SHADER_CREATE_INFO(eevee_legacy_material_prepass_frag_alpha_hash_hair)
    .additional_info("eevee_legacy_surface_lib_hair")
    .additional_info("eevee_legacy_material_prepass_frag_alpha_hash_common")
    .additional_info("draw_hair");

GPU_SHADER_CREATE_INFO(eevee_legacy_material_prepass_frag_alpha_hash_pointcloud)
    .additional_info("eevee_legacy_surface_lib_pointcloud")
    .additional_info("eevee_legacy_material_prepass_frag_alpha_hash_common")
    .additional_info("draw_pointcloud");

/** -- Surface Shader -- **/

GPU_SHADER_CREATE_INFO(eevee_legacy_material_surface_frag_common)
    .additional_info("eevee_legacy_common_lib")
    .additional_info("eevee_legacy_common_utiltex_lib")
    .additional_info("eevee_legacy_closure_eval_surface_lib")
    .additional_info("eevee_legacy_renderpass_lib")
    .additional_info("eevee_legacy_volumetric_lib")
    .push_constant(Type::FLOAT, "backgroundAlpha");

GPU_SHADER_CREATE_INFO(eevee_legacy_material_surface_frag_opaque)
    .additional_info("eevee_legacy_material_surface_frag_common")
    .fragment_out(0, Type::VEC4, "outRadiance")
    .fragment_out(1, Type::VEC2, "ssrNormals")
    .fragment_out(2, Type::VEC4, "ssrData")
    .fragment_out(3, Type::VEC3, "sssIrradiance")
    .fragment_out(4, Type::FLOAT, "sssRadius")
    .fragment_out(5, Type::VEC3, "sssAlbedo");

GPU_SHADER_CREATE_INFO(eevee_legacy_material_surface_frag_alpha_blend)
    .define("USE_ALPHA_BLEND")
    .additional_info("eevee_legacy_material_surface_frag_common")
    .fragment_out(0, Type::VEC4, "outRadiance", DualBlend::SRC_0)
    .fragment_out(0, Type::VEC4, "outTransmittance", DualBlend::SRC_1);

/* hair_refine_shader_transform_feedback_create */

GPU_SHADER_INTERFACE_INFO(legacy_hair_refine_shader_transform_feedback_iface, "")
    .smooth(Type::VEC4, "finalColor");

GPU_SHADER_CREATE_INFO(legacy_hair_refine_shader_transform_feedback)
    .define("HAIR_PHASE_SUBDIV")
    .define("USE_TF")
    .additional_info("eevee_legacy_hair_lib")
    .vertex_source("common_hair_refine_vert.glsl")
    .vertex_out(legacy_hair_refine_shader_transform_feedback_iface)
    .transform_feedback_mode(GPU_SHADER_TFB_POINTS)
    .transform_feedback_output_name("finalColor")
    .do_static_compilation(true);
