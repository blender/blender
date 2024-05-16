/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 */

#include "DNA_material_types.h"

#include "BKE_lib_id.hh"
#include "BKE_material.h"
#include "BKE_node.hh"
#include "NOD_shader.h"

#include "eevee_instance.hh"

#include "eevee_material.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Default Material
 *
 * \{ */

DefaultSurfaceNodeTree::DefaultSurfaceNodeTree()
{
  bNodeTree *ntree = bke::ntreeAddTree(nullptr, "Shader Nodetree", ntreeType_Shader->idname);
  bNode *bsdf = bke::nodeAddStaticNode(nullptr, ntree, SH_NODE_BSDF_PRINCIPLED);
  bNode *output = bke::nodeAddStaticNode(nullptr, ntree, SH_NODE_OUTPUT_MATERIAL);
  bNodeSocket *bsdf_out = bke::nodeFindSocket(bsdf, SOCK_OUT, "BSDF");
  bNodeSocket *output_in = bke::nodeFindSocket(output, SOCK_IN, "Surface");
  bke::nodeAddLink(ntree, bsdf, bsdf_out, output, output_in);
  bke::nodeSetActive(ntree, output);

  color_socket_ =
      (bNodeSocketValueRGBA *)bke::nodeFindSocket(bsdf, SOCK_IN, "Base Color")->default_value;
  metallic_socket_ =
      (bNodeSocketValueFloat *)bke::nodeFindSocket(bsdf, SOCK_IN, "Metallic")->default_value;
  roughness_socket_ =
      (bNodeSocketValueFloat *)bke::nodeFindSocket(bsdf, SOCK_IN, "Roughness")->default_value;
  specular_socket_ = (bNodeSocketValueFloat *)bke::nodeFindSocket(
                         bsdf, SOCK_IN, "Specular IOR Level")
                         ->default_value;
  ntree_ = ntree;
}

DefaultSurfaceNodeTree::~DefaultSurfaceNodeTree()
{
  bke::ntreeFreeEmbeddedTree(ntree_);
  MEM_SAFE_FREE(ntree_);
}

bNodeTree *DefaultSurfaceNodeTree::nodetree_get(::Material *ma)
{
  /* WARNING: This function is not threadsafe. Which is not a problem for the moment. */
  copy_v3_fl3(color_socket_->value, ma->r, ma->g, ma->b);
  metallic_socket_->value = ma->metallic;
  roughness_socket_->value = ma->roughness;
  specular_socket_->value = ma->spec;

  return ntree_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material
 *
 * \{ */

MaterialModule::MaterialModule(Instance &inst) : inst_(inst)
{
  {
    diffuse_mat = (::Material *)BKE_id_new_nomain(ID_MA, "EEVEE default diffuse");
    bNodeTree *ntree = bke::ntreeAddTreeEmbedded(
        nullptr, &diffuse_mat->id, "Shader Nodetree", ntreeType_Shader->idname);
    diffuse_mat->use_nodes = true;
    diffuse_mat->surface_render_method = MA_SURFACE_METHOD_FORWARD;

    /* Use 0.18 as it is close to middle gray. Middle gray is typically defined as 18% reflectance
     * of visible light and commonly used for VFX balls. */
    bNode *bsdf = bke::nodeAddStaticNode(nullptr, ntree, SH_NODE_BSDF_DIFFUSE);
    bNodeSocket *base_color = bke::nodeFindSocket(bsdf, SOCK_IN, "Color");
    copy_v3_fl(((bNodeSocketValueRGBA *)base_color->default_value)->value, 0.18f);

    bNode *output = bke::nodeAddStaticNode(nullptr, ntree, SH_NODE_OUTPUT_MATERIAL);

    bke::nodeAddLink(ntree,
                     bsdf,
                     bke::nodeFindSocket(bsdf, SOCK_OUT, "BSDF"),
                     output,
                     bke::nodeFindSocket(output, SOCK_IN, "Surface"));

    bke::nodeSetActive(ntree, output);
  }
  {
    metallic_mat = (::Material *)BKE_id_new_nomain(ID_MA, "EEVEE default metal");
    bNodeTree *ntree = bke::ntreeAddTreeEmbedded(
        nullptr, &metallic_mat->id, "Shader Nodetree", ntreeType_Shader->idname);
    metallic_mat->use_nodes = true;
    metallic_mat->surface_render_method = MA_SURFACE_METHOD_FORWARD;

    bNode *bsdf = bke::nodeAddStaticNode(nullptr, ntree, SH_NODE_BSDF_GLOSSY);
    bNodeSocket *base_color = bke::nodeFindSocket(bsdf, SOCK_IN, "Color");
    copy_v3_fl(((bNodeSocketValueRGBA *)base_color->default_value)->value, 1.0f);
    bNodeSocket *roughness = bke::nodeFindSocket(bsdf, SOCK_IN, "Roughness");
    ((bNodeSocketValueFloat *)roughness->default_value)->value = 0.0f;

    bNode *output = bke::nodeAddStaticNode(nullptr, ntree, SH_NODE_OUTPUT_MATERIAL);

    bke::nodeAddLink(ntree,
                     bsdf,
                     bke::nodeFindSocket(bsdf, SOCK_OUT, "BSDF"),
                     output,
                     bke::nodeFindSocket(output, SOCK_IN, "Surface"));

    bke::nodeSetActive(ntree, output);
  }
  {
    error_mat_ = (::Material *)BKE_id_new_nomain(ID_MA, "EEVEE default error");
    bNodeTree *ntree = bke::ntreeAddTreeEmbedded(
        nullptr, &error_mat_->id, "Shader Nodetree", ntreeType_Shader->idname);
    error_mat_->use_nodes = true;

    /* Use emission and output material to be compatible with both World and Material. */
    bNode *bsdf = bke::nodeAddStaticNode(nullptr, ntree, SH_NODE_EMISSION);
    bNodeSocket *color = bke::nodeFindSocket(bsdf, SOCK_IN, "Color");
    copy_v3_fl3(((bNodeSocketValueRGBA *)color->default_value)->value, 1.0f, 0.0f, 1.0f);

    bNode *output = bke::nodeAddStaticNode(nullptr, ntree, SH_NODE_OUTPUT_MATERIAL);

    bke::nodeAddLink(ntree,
                     bsdf,
                     bke::nodeFindSocket(bsdf, SOCK_OUT, "Emission"),
                     output,
                     bke::nodeFindSocket(output, SOCK_IN, "Surface"));

    bke::nodeSetActive(ntree, output);
  }
}

MaterialModule::~MaterialModule()
{
  BKE_id_free(nullptr, metallic_mat);
  BKE_id_free(nullptr, diffuse_mat);
  BKE_id_free(nullptr, error_mat_);
}

void MaterialModule::begin_sync()
{
  queued_shaders_count = 0;
  queued_optimize_shaders_count = 0;

  material_map_.clear();
  shader_map_.clear();
}

MaterialPass MaterialModule::material_pass_get(Object *ob,
                                               ::Material *blender_mat,
                                               eMaterialPipeline pipeline_type,
                                               eMaterialGeometry geometry_type,
                                               eMaterialProbe probe_capture)
{
  bNodeTree *ntree = (blender_mat->use_nodes && blender_mat->nodetree != nullptr) ?
                         blender_mat->nodetree :
                         default_surface_ntree_.nodetree_get(blender_mat);

  bool use_deferred_compilation = inst_.is_viewport();

  MaterialPass matpass = MaterialPass();
  matpass.gpumat = inst_.shaders.material_shader_get(
      blender_mat, ntree, pipeline_type, geometry_type, use_deferred_compilation);

  const bool is_volume = ELEM(pipeline_type, MAT_PIPE_VOLUME_OCCUPANCY, MAT_PIPE_VOLUME_MATERIAL);
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
      matpass.gpumat = inst_.shaders.material_default_shader_get(pipeline_type, geometry_type);
      break;
    case GPU_MAT_FAILED:
    default:
      matpass.gpumat = inst_.shaders.material_shader_get(
          error_mat_, error_mat_->nodetree, pipeline_type, geometry_type, false);
      break;
  }
  /* Returned material should be ready to be drawn. */
  BLI_assert(GPU_material_status(matpass.gpumat) == GPU_MAT_SUCCESS);

  inst_.manager->register_layer_attributes(matpass.gpumat);

  const bool is_transparent = GPU_material_flag_get(matpass.gpumat, GPU_MATFLAG_TRANSPARENT);

  if (use_deferred_compilation && GPU_material_recalc_flag_get(matpass.gpumat)) {
    /* TODO(Miguel Pozo): This is broken, it consumes the flag,
     * but GPUMats can be shared across viewports. */
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
      matpass.sub_pass->material_set(*inst_.manager, matpass.gpumat);
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
    VolumeLayer *layer = inst_.pipelines.volume.register_and_get_layer(ob);
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
      mat.prepass = MaterialPass();
      /* TODO(fclem): Still need the shading pass for correct attribute extraction. Would be better
       * to avoid this shader compilation in another context. */
      mat.shading = material_pass_get(ob, blender_mat, surface_pipe, geometry_type);
      mat.capture = material_pass_get(ob, blender_mat, MAT_PIPE_CAPTURE, geometry_type);
      mat.overlap_masking = MaterialPass();
      mat.reflection_probe_prepass = MaterialPass();
      mat.reflection_probe_shading = MaterialPass();
      mat.planar_probe_prepass = MaterialPass();
      mat.planar_probe_shading = MaterialPass();
      mat.volume_occupancy = MaterialPass();
      mat.volume_material = MaterialPass();
      mat.has_volume = false; /* TODO */
      mat.has_surface = GPU_material_has_surface_output(mat.shading.gpumat);
    }
    else {
      /* Order is important for transparent. */
      mat.prepass = material_pass_get(ob, blender_mat, prepass_pipe, geometry_type);
      mat.shading = material_pass_get(ob, blender_mat, surface_pipe, geometry_type);
      mat.overlap_masking = MaterialPass();
      mat.capture = MaterialPass();

      if (inst_.do_reflection_probe_sync() && !(ob->visibility_flag & OB_HIDE_PROBE_CUBEMAP)) {
        mat.reflection_probe_prepass = material_pass_get(
            ob, blender_mat, MAT_PIPE_PREPASS_DEFERRED, geometry_type, MAT_PROBE_REFLECTION);
        mat.reflection_probe_shading = material_pass_get(
            ob, blender_mat, MAT_PIPE_DEFERRED, geometry_type, MAT_PROBE_REFLECTION);
      }
      else {
        mat.reflection_probe_prepass = MaterialPass();
        mat.reflection_probe_shading = MaterialPass();
      }

      if (inst_.do_planar_probe_sync() && !(ob->visibility_flag & OB_HIDE_PROBE_PLANAR)) {
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
      if (mat.has_volume) {
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

  if (mat.is_alpha_blend_transparent) {
    /* Transparent needs to use one sub pass per object to support reordering.
     * NOTE: Pre-pass needs to be created first in order to be sorted first. */
    mat.overlap_masking.sub_pass = inst_.pipelines.forward.prepass_transparent_add(
        ob, blender_mat, mat.shading.gpumat);
    mat.shading.sub_pass = inst_.pipelines.forward.material_transparent_add(
        ob, blender_mat, mat.shading.gpumat);
  }

  if (mat.has_volume) {
    /* Volume needs to use one sub pass per object to support layering. */
    VolumeLayer *layer = inst_.pipelines.volume.register_and_get_layer(ob);
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
  if (ob->base_flag & BASE_HOLDOUT) {
    return BKE_material_default_holdout();
  }
  ::Material *ma = BKE_object_material_get(ob, slot + 1);
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

  const int materials_len = DRW_cache_object_material_count_get(ob);

  for (auto i : IndexRange(materials_len)) {
    ::Material *blender_mat = material_from_slot(ob, i);
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
  ::Material *blender_mat = material_from_slot(ob, mat_nr);
  Material &mat = material_sync(ob, blender_mat, geometry_type, has_motion);
  return mat;
}

/** \} */

}  // namespace blender::eevee
