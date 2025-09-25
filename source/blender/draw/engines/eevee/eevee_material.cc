/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 */

#include "BLI_time.h"
#include "DNA_material_types.h"

#include "BKE_lib_id.hh"
#include "BKE_material.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"

#include "NOD_shader.h"

#include "eevee_instance.hh"
#include "eevee_material.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Material
 *
 * \{ */

MaterialModule::MaterialModule(Instance &inst) : inst_(inst)
{
  {
    diffuse_mat = BKE_id_new_nomain<::Material>("EEVEE default diffuse");
    bNodeTree *ntree = bke::node_tree_add_tree_embedded(
        nullptr, &diffuse_mat->id, "Shader Nodetree", ntreeType_Shader->idname);
    diffuse_mat->surface_render_method = MA_SURFACE_METHOD_FORWARD;

    /* Use 0.18 as it is close to middle gray. Middle gray is typically defined as 18% reflectance
     * of visible light and commonly used for VFX balls. */
    bNode *bsdf = bke::node_add_static_node(nullptr, *ntree, SH_NODE_BSDF_DIFFUSE);
    bNodeSocket *base_color = bke::node_find_socket(*bsdf, SOCK_IN, "Color");
    copy_v3_fl(((bNodeSocketValueRGBA *)base_color->default_value)->value, 0.18f);

    bNode *output = bke::node_add_static_node(nullptr, *ntree, SH_NODE_OUTPUT_MATERIAL);

    bke::node_add_link(*ntree,
                       *bsdf,
                       *bke::node_find_socket(*bsdf, SOCK_OUT, "BSDF"),
                       *output,
                       *bke::node_find_socket(*output, SOCK_IN, "Surface"));

    bke::node_set_active(*ntree, *output);
  }
  {
    metallic_mat = BKE_id_new_nomain<::Material>("EEVEE default metal");
    bNodeTree *ntree = bke::node_tree_add_tree_embedded(
        nullptr, &metallic_mat->id, "Shader Nodetree", ntreeType_Shader->idname);
    metallic_mat->surface_render_method = MA_SURFACE_METHOD_FORWARD;

    bNode *bsdf = bke::node_add_static_node(nullptr, *ntree, SH_NODE_BSDF_GLOSSY);
    bNodeSocket *base_color = bke::node_find_socket(*bsdf, SOCK_IN, "Color");
    copy_v3_fl(((bNodeSocketValueRGBA *)base_color->default_value)->value, 1.0f);
    bNodeSocket *roughness = bke::node_find_socket(*bsdf, SOCK_IN, "Roughness");
    ((bNodeSocketValueFloat *)roughness->default_value)->value = 0.0f;

    bNode *output = bke::node_add_static_node(nullptr, *ntree, SH_NODE_OUTPUT_MATERIAL);

    bke::node_add_link(*ntree,
                       *bsdf,
                       *bke::node_find_socket(*bsdf, SOCK_OUT, "BSDF"),
                       *output,
                       *bke::node_find_socket(*output, SOCK_IN, "Surface"));

    bke::node_set_active(*ntree, *output);
  }
  {
    default_surface = reinterpret_cast<::Material *>(BKE_id_copy_ex(
        nullptr, &BKE_material_default_surface()->id, nullptr, LIB_ID_COPY_LOCALIZE));
    default_volume = reinterpret_cast<::Material *>(BKE_id_copy_ex(
        nullptr, &BKE_material_default_volume()->id, nullptr, LIB_ID_COPY_LOCALIZE));
  }
  {
    error_mat_ = BKE_id_new_nomain<::Material>("EEVEE default error");
    bNodeTree *ntree = bke::node_tree_add_tree_embedded(
        nullptr, &error_mat_->id, "Shader Nodetree", ntreeType_Shader->idname);

    /* Use emission and output material to be compatible with both World and Material. */
    bNode *bsdf = bke::node_add_static_node(nullptr, *ntree, SH_NODE_EMISSION);
    bNodeSocket *color = bke::node_find_socket(*bsdf, SOCK_IN, "Color");
    copy_v3_fl3(((bNodeSocketValueRGBA *)color->default_value)->value, 1.0f, 0.0f, 1.0f);

    bNode *output = bke::node_add_static_node(nullptr, *ntree, SH_NODE_OUTPUT_MATERIAL);

    bke::node_add_link(*ntree,
                       *bsdf,
                       *bke::node_find_socket(*bsdf, SOCK_OUT, "Emission"),
                       *output,
                       *bke::node_find_socket(*output, SOCK_IN, "Surface"));

    bke::node_set_active(*ntree, *output);
  }
}

MaterialModule::~MaterialModule()
{
  BKE_id_free(nullptr, metallic_mat);
  BKE_id_free(nullptr, diffuse_mat);
  BKE_id_free(nullptr, default_surface);
  BKE_id_free(nullptr, default_volume);
  BKE_id_free(nullptr, error_mat_);
}

void MaterialModule::begin_sync()
{
  queued_shaders_count = 0;
  queued_textures_count = 0;
  queued_optimize_shaders_count = 0;

  material_override = DEG_get_evaluated(inst_.depsgraph, inst_.view_layer->mat_override);

  uint64_t next_update = GPU_pass_global_compilation_count();
  gpu_pass_last_update_ = gpu_pass_next_update_;
  gpu_pass_next_update_ = next_update;

  texture_loading_queue_.clear();
  material_map_.clear();
  shader_map_.clear();
}

void MaterialModule::queue_texture_loading(GPUMaterial *material)
{
  ListBase textures = GPU_material_textures(material);
  for (GPUMaterialTexture *tex : ListBaseWrapper<GPUMaterialTexture>(textures)) {
    if (tex->ima) {
      const bool use_tile_mapping = tex->tiled_mapping_name[0];
      ImageUser *iuser = tex->iuser_available ? &tex->iuser : nullptr;
      ImageGPUTextures gputex = BKE_image_get_gpu_material_texture_try(
          tex->ima, iuser, use_tile_mapping);
      if (*gputex.texture == nullptr) {
        texture_loading_queue_.append(tex);
      }
    }
  }
}

void MaterialModule::end_sync()
{
  if (texture_loading_queue_.is_empty()) {
    return;
  }

  if (inst_.is_viewport()) {
    /* Avoid ghosting of textures. */
    inst_.sampling.reset();
  }

  GPU_debug_group_begin("Texture Loading");

  /* Load files from disk in a multithreaded manner. Allow better parallelism. */
  threading::parallel_for(texture_loading_queue_.index_range(), 1, [&](const IndexRange range) {
    for (auto i : range) {
      GPUMaterialTexture *tex = texture_loading_queue_[i];
      ImageUser *iuser = tex->iuser_available ? &tex->iuser : nullptr;
      BKE_image_get_tile(tex->ima, 0);
      threading::isolate_task([&]() {
        ImBuf *imbuf = BKE_image_acquire_ibuf(tex->ima, iuser, nullptr);
        BKE_image_release_ibuf(tex->ima, imbuf, nullptr);
      });
    }
  });

  /* Tag time is not thread-safe. */
  for (GPUMaterialTexture *tex : texture_loading_queue_) {
    BKE_image_tag_time(tex->ima);
  }

  /* Upload to the GPU (create gpu::Texture). This part still requires a valid GPU context and
   * is not easily parallelized. */
  for (GPUMaterialTexture *tex : texture_loading_queue_) {
    BLI_assert(tex->ima);
    GPU_debug_group_begin(tex->ima->id.name);

    const bool use_tile_mapping = tex->tiled_mapping_name[0];
    ImageUser *iuser = tex->iuser_available ? &tex->iuser : nullptr;
    ImageGPUTextures gputex = BKE_image_get_gpu_material_texture(
        tex->ima, iuser, use_tile_mapping);

    /* Acquire the textures since they were not existing inside `PassBase::material_set()`. */
    inst_.manager->acquire_texture(*gputex.texture);
    if (gputex.tile_mapping) {
      inst_.manager->acquire_texture(*gputex.tile_mapping);
    }

    GPU_debug_group_end();
  }
  GPU_debug_group_end();
  texture_loading_queue_.clear();
}

MaterialPass MaterialModule::material_pass_get(Object *ob,
                                               ::Material *blender_mat,
                                               eMaterialPipeline pipeline_type,
                                               eMaterialGeometry geometry_type,
                                               eMaterialProbe probe_capture)
{
  bNodeTree *ntree = (blender_mat->nodetree != nullptr) ? blender_mat->nodetree :
                                                          default_surface->nodetree;

  /* We can't defer compilation in viewport image render, since we can't re-sync.(See #130235) */
  bool use_deferred_compilation = !inst_.is_viewport_image_render;

  const bool is_volume = ELEM(pipeline_type, MAT_PIPE_VOLUME_OCCUPANCY, MAT_PIPE_VOLUME_MATERIAL);
  ::Material *default_mat = is_volume ? default_volume : default_surface;

  MaterialPass matpass = MaterialPass();
  matpass.gpumat = inst_.shaders.material_shader_get(
      blender_mat, ntree, pipeline_type, geometry_type, use_deferred_compilation, default_mat);

  queue_texture_loading(matpass.gpumat);

  const bool is_forward = ELEM(pipeline_type,
                               MAT_PIPE_FORWARD,
                               MAT_PIPE_PREPASS_FORWARD,
                               MAT_PIPE_PREPASS_FORWARD_VELOCITY,
                               MAT_PIPE_PREPASS_OVERLAP);

  switch (GPU_material_status(matpass.gpumat)) {
    case GPU_MAT_SUCCESS: {
      /* Determine optimization status for remaining compilations counter. */
      int optimization_status = GPU_material_optimization_status(matpass.gpumat);
      if (optimization_status == GPU_MAT_OPTIMIZATION_QUEUED) {
        queued_optimize_shaders_count++;
      }
      break;
    }
    case GPU_MAT_QUEUED:
      queued_shaders_count++;
      matpass.gpumat = inst_.shaders.material_shader_get(
          default_mat, default_mat->nodetree, pipeline_type, geometry_type, false, nullptr);
      break;
    case GPU_MAT_FAILED:
    default:
      matpass.gpumat = inst_.shaders.material_shader_get(
          error_mat_, error_mat_->nodetree, pipeline_type, geometry_type, false, nullptr);
      break;
  }
  /* Returned material should be ready to be drawn. */
  BLI_assert(GPU_material_status(matpass.gpumat) == GPU_MAT_SUCCESS);

  inst_.manager->register_layer_attributes(matpass.gpumat);

  const bool is_transparent = GPU_material_flag_get(matpass.gpumat, GPU_MATFLAG_TRANSPARENT);

  bool pass_updated = GPU_material_compilation_timestamp(matpass.gpumat) > gpu_pass_last_update_;

  if (inst_.is_viewport() && use_deferred_compilation && pass_updated) {
    inst_.sampling.reset();

    const bool has_displacement = GPU_material_has_displacement_output(matpass.gpumat) &&
                                  (blender_mat->displacement_method != MA_DISPLACEMENT_BUMP);
    const bool has_volume = GPU_material_has_volume_output(matpass.gpumat);

    if (((pipeline_type == MAT_PIPE_SHADOW) && (is_transparent || has_displacement)) || has_volume)
    {
      /* WORKAROUND: This is to avoid lingering shadows from default material.
       * Ideally, we should tag the caster object to update only the needed areas but that's a bit
       * more involved. */
      inst_.shadows.reset();
    }
  }

  if (is_volume || (is_forward && is_transparent)) {
    /* Sub pass is generated later. */
    matpass.sub_pass = nullptr;
  }
  else {
    ShaderKey shader_key(matpass.gpumat, blender_mat, probe_capture);

    PassMain::Sub *shader_sub = shader_map_.lookup_or_add_cb(shader_key, [&]() {
      /* First time encountering this shader. Create a sub that will contain materials using it. */
      return inst_.pipelines.material_add(
          ob, blender_mat, matpass.gpumat, pipeline_type, probe_capture);
    });

    if (shader_sub != nullptr) {
      /* Create a sub for this material as `shader_sub` is for sharing shader between materials. */
      matpass.sub_pass = &shader_sub->sub(GPU_material_get_name(matpass.gpumat));
      matpass.sub_pass->material_set(*inst_.manager, matpass.gpumat, true);
    }
    else {
      matpass.sub_pass = nullptr;
    }
  }

  return matpass;
}

Material &MaterialModule::material_sync(Object *ob,
                                        ::Material *blender_mat,
                                        eMaterialGeometry geometry_type,
                                        bool has_motion)
{
  bool hide_on_camera = ob->visibility_flag & OB_HIDE_CAMERA;

  if (geometry_type == MAT_GEOM_VOLUME) {
    MaterialKey material_key(
        blender_mat, geometry_type, MAT_PIPE_VOLUME_MATERIAL, ob->visibility_flag);
    Material &mat = material_map_.lookup_or_add_cb(material_key, [&]() {
      Material mat = {};
      mat.volume_occupancy = material_pass_get(
          ob, blender_mat, MAT_PIPE_VOLUME_OCCUPANCY, MAT_GEOM_VOLUME);
      mat.volume_material = material_pass_get(
          ob, blender_mat, MAT_PIPE_VOLUME_MATERIAL, MAT_GEOM_VOLUME);
      return mat;
    });

    /* Volume needs to use one sub pass per object to support layering. */
    VolumeLayer *layer = hide_on_camera ? nullptr :
                                          inst_.pipelines.volume.register_and_get_layer(ob);
    if (layer) {
      mat.volume_occupancy.sub_pass = layer->occupancy_add(
          ob, blender_mat, mat.volume_occupancy.gpumat);
      mat.volume_material.sub_pass = layer->material_add(
          ob, blender_mat, mat.volume_material.gpumat);
    }
    else {
      /* Culled volumes. */
      mat.volume_occupancy.sub_pass = nullptr;
      mat.volume_material.sub_pass = nullptr;
    }
    return mat;
  }

  const bool use_forward_pipeline = (blender_mat->surface_render_method ==
                                     MA_SURFACE_METHOD_FORWARD);
  eMaterialPipeline surface_pipe, prepass_pipe;
  if (use_forward_pipeline) {
    surface_pipe = MAT_PIPE_FORWARD;
    prepass_pipe = has_motion ? MAT_PIPE_PREPASS_FORWARD_VELOCITY : MAT_PIPE_PREPASS_FORWARD;
  }
  else {
    surface_pipe = MAT_PIPE_DEFERRED;
    prepass_pipe = has_motion ? MAT_PIPE_PREPASS_DEFERRED_VELOCITY : MAT_PIPE_PREPASS_DEFERRED;
  }

  MaterialKey material_key(blender_mat, geometry_type, surface_pipe, ob->visibility_flag);

  Material &mat = material_map_.lookup_or_add_cb(material_key, [&]() {
    Material mat;
    if (inst_.is_baking()) {
      if (ob->visibility_flag & OB_HIDE_PROBE_VOLUME) {
        mat.capture = MaterialPass();
      }
      else {
        mat.capture = material_pass_get(ob, blender_mat, MAT_PIPE_CAPTURE, geometry_type);
      }
      mat.prepass = MaterialPass();
      /* TODO(fclem): Still need the shading pass for correct attribute extraction. Would be better
       * to avoid this shader compilation in another context. */
      mat.shading = material_pass_get(ob, blender_mat, surface_pipe, geometry_type);
      mat.overlap_masking = MaterialPass();
      mat.lightprobe_sphere_prepass = MaterialPass();
      mat.lightprobe_sphere_shading = MaterialPass();
      mat.planar_probe_prepass = MaterialPass();
      mat.planar_probe_shading = MaterialPass();
      mat.volume_occupancy = MaterialPass();
      mat.volume_material = MaterialPass();
      mat.has_volume = false; /* TODO */
      mat.has_surface = GPU_material_has_surface_output(mat.shading.gpumat);
    }
    else {
      /* Order is important for transparent. */
      if (!hide_on_camera) {
        mat.prepass = material_pass_get(ob, blender_mat, prepass_pipe, geometry_type);
      }
      else {
        mat.prepass = MaterialPass();
      }

      mat.shading = material_pass_get(ob, blender_mat, surface_pipe, geometry_type);
      if (hide_on_camera) {
        /* Only null the sub_pass.
         * `mat.shading.gpumat` is always needed for using the GPU_material API. */
        mat.shading.sub_pass = nullptr;
      }

      mat.overlap_masking = MaterialPass();
      mat.capture = MaterialPass();

      if (inst_.needs_lightprobe_sphere_passes() && !(ob->visibility_flag & OB_HIDE_PROBE_CUBEMAP))
      {
        mat.lightprobe_sphere_prepass = material_pass_get(
            ob, blender_mat, MAT_PIPE_PREPASS_DEFERRED, geometry_type, MAT_PROBE_REFLECTION);
        mat.lightprobe_sphere_shading = material_pass_get(
            ob, blender_mat, MAT_PIPE_DEFERRED, geometry_type, MAT_PROBE_REFLECTION);
      }
      else {
        mat.lightprobe_sphere_prepass = MaterialPass();
        mat.lightprobe_sphere_shading = MaterialPass();
      }

      if (inst_.needs_planar_probe_passes() && !(ob->visibility_flag & OB_HIDE_PROBE_PLANAR)) {
        mat.planar_probe_prepass = material_pass_get(
            ob, blender_mat, MAT_PIPE_PREPASS_PLANAR, geometry_type, MAT_PROBE_PLANAR);
        mat.planar_probe_shading = material_pass_get(
            ob, blender_mat, MAT_PIPE_DEFERRED, geometry_type, MAT_PROBE_PLANAR);
      }
      else {
        mat.planar_probe_prepass = MaterialPass();
        mat.planar_probe_shading = MaterialPass();
      }

      mat.has_surface = GPU_material_has_surface_output(mat.shading.gpumat);
      mat.has_volume = GPU_material_has_volume_output(mat.shading.gpumat);
      if (mat.has_volume && !hide_on_camera) {
        mat.volume_occupancy = material_pass_get(
            ob, blender_mat, MAT_PIPE_VOLUME_OCCUPANCY, geometry_type);
        mat.volume_material = material_pass_get(
            ob, blender_mat, MAT_PIPE_VOLUME_MATERIAL, geometry_type);
      }
      else {
        mat.volume_occupancy = MaterialPass();
        mat.volume_material = MaterialPass();
      }
    }

    if (!(ob->visibility_flag & OB_HIDE_SHADOW)) {
      mat.shadow = material_pass_get(ob, blender_mat, MAT_PIPE_SHADOW, geometry_type);
    }
    else {
      mat.shadow = MaterialPass();
    }

    mat.is_alpha_blend_transparent = use_forward_pipeline &&
                                     GPU_material_flag_get(mat.shading.gpumat,
                                                           GPU_MATFLAG_TRANSPARENT);
    mat.has_transparent_shadows = blender_mat->blend_flag & MA_BL_TRANSPARENT_SHADOW &&
                                  GPU_material_flag_get(mat.shading.gpumat,
                                                        GPU_MATFLAG_TRANSPARENT);

    return mat;
  });

  if (mat.is_alpha_blend_transparent && !hide_on_camera) {
    /* Transparent needs to use one sub pass per object to support reordering.
     * NOTE: Pre-pass needs to be created first in order to be sorted first. */
    mat.overlap_masking.sub_pass = inst_.pipelines.forward.prepass_transparent_add(
        ob, blender_mat, mat.shading.gpumat);
    mat.shading.sub_pass = inst_.pipelines.forward.material_transparent_add(
        ob, blender_mat, mat.shading.gpumat);
  }

  if (mat.has_volume) {
    /* Volume needs to use one sub pass per object to support layering. */
    VolumeLayer *layer = hide_on_camera ? nullptr :
                                          inst_.pipelines.volume.register_and_get_layer(ob);
    if (layer) {
      mat.volume_occupancy.sub_pass = layer->occupancy_add(
          ob, blender_mat, mat.volume_occupancy.gpumat);
      mat.volume_material.sub_pass = layer->material_add(
          ob, blender_mat, mat.volume_material.gpumat);
    }
    else {
      /* Culled volumes. */
      mat.volume_occupancy.sub_pass = nullptr;
      mat.volume_material.sub_pass = nullptr;
    }
  }
  return mat;
}

::Material *MaterialModule::material_from_slot(Object *ob, int slot)
{
  ::Material *ma = BKE_object_material_get_eval(ob, slot + 1);
  if (ma == nullptr) {
    if (ob->type == OB_VOLUME) {
      return BKE_material_default_volume();
    }
    return BKE_material_default_surface();
  }
  return ma;
}

MaterialArray &MaterialModule::material_array_get(Object *ob, bool has_motion)
{
  material_array_.materials.clear();
  material_array_.gpu_materials.clear();

  const int materials_len = BKE_object_material_used_with_fallback_eval(*ob);

  for (auto i : IndexRange(materials_len)) {
    ::Material *blender_mat = (material_override) ? material_override : material_from_slot(ob, i);
    Material &mat = material_sync(ob, blender_mat, to_material_geometry(ob), has_motion);
    /* \note Perform a whole copy since next material_sync() can move the Material memory location
     * (i.e: because of its container growing) */
    material_array_.materials.append(mat);
    material_array_.gpu_materials.append(mat.shading.gpumat);
  }
  return material_array_;
}

Material &MaterialModule::material_get(Object *ob,
                                       bool has_motion,
                                       int mat_nr,
                                       eMaterialGeometry geometry_type)
{
  ::Material *blender_mat = (material_override) ? material_override :
                                                  material_from_slot(ob, mat_nr);
  Material &mat = material_sync(ob, blender_mat, geometry_type, has_motion);
  return mat;
}

ShaderGroups MaterialModule::default_materials_load(bool block_until_ready)
{
  bool shaders_are_ready = true;
  auto request_shader = [&](::Material *mat, eMaterialPipeline pipeline, eMaterialGeometry geom) {
    GPUMaterial *gpu_mat = inst_.shaders.material_shader_get(
        mat, mat->nodetree, pipeline, geom, !block_until_ready, nullptr);
    shaders_are_ready = shaders_are_ready && GPU_material_status(gpu_mat) == GPU_MAT_SUCCESS;
  };

  request_shader(default_surface, MAT_PIPE_PREPASS_DEFERRED, MAT_GEOM_MESH);
  request_shader(default_surface, MAT_PIPE_PREPASS_DEFERRED_VELOCITY, MAT_GEOM_MESH);
  request_shader(default_surface, MAT_PIPE_DEFERRED, MAT_GEOM_MESH);
  request_shader(default_surface, MAT_PIPE_SHADOW, MAT_GEOM_MESH);

  return shaders_are_ready ? DEFAULT_MATERIALS : NONE;
}

/** \} */

}  // namespace blender::eevee
