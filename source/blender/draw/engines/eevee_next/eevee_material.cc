/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup eevee
 */

#include "DNA_material_types.h"

#include "BKE_lib_id.h"
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
  bNodeTree *ntree = ntreeAddTree(nullptr, "Shader Nodetree", ntreeType_Shader->idname);
  bNode *bsdf = nodeAddStaticNode(nullptr, ntree, SH_NODE_BSDF_PRINCIPLED);
  bNode *output = nodeAddStaticNode(nullptr, ntree, SH_NODE_OUTPUT_MATERIAL);
  bNodeSocket *bsdf_out = nodeFindSocket(bsdf, SOCK_OUT, "BSDF");
  bNodeSocket *output_in = nodeFindSocket(output, SOCK_IN, "Surface");
  nodeAddLink(ntree, bsdf, bsdf_out, output, output_in);
  nodeSetActive(ntree, output);

  color_socket_ =
      (bNodeSocketValueRGBA *)nodeFindSocket(bsdf, SOCK_IN, "Base Color")->default_value;
  metallic_socket_ =
      (bNodeSocketValueFloat *)nodeFindSocket(bsdf, SOCK_IN, "Metallic")->default_value;
  roughness_socket_ =
      (bNodeSocketValueFloat *)nodeFindSocket(bsdf, SOCK_IN, "Roughness")->default_value;
  specular_socket_ =
      (bNodeSocketValueFloat *)nodeFindSocket(bsdf, SOCK_IN, "Specular")->default_value;
  ntree_ = ntree;
}

DefaultSurfaceNodeTree::~DefaultSurfaceNodeTree()
{
  ntreeFreeEmbeddedTree(ntree_);
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
    /* To use the forward pipeline. */
    diffuse_mat->blend_method = MA_BM_BLEND;

    bNode *bsdf = nodeAddStaticNode(nullptr, ntree, SH_NODE_BSDF_DIFFUSE);
    bNodeSocket *base_color = nodeFindSocket(bsdf, SOCK_IN, "Color");
    copy_v3_fl(((bNodeSocketValueRGBA *)base_color->default_value)->value, 0.8f);

    bNode *output = nodeAddStaticNode(nullptr, ntree, SH_NODE_OUTPUT_MATERIAL);

    nodeAddLink(ntree,
                bsdf,
                nodeFindSocket(bsdf, SOCK_OUT, "BSDF"),
                output,
                nodeFindSocket(output, SOCK_IN, "Surface"));

    nodeSetActive(ntree, output);
  }
  {
    glossy_mat = (::Material *)BKE_id_new_nomain(ID_MA, "EEVEE default metal");
    bNodeTree *ntree = bke::ntreeAddTreeEmbedded(
        nullptr, &glossy_mat->id, "Shader Nodetree", ntreeType_Shader->idname);
    glossy_mat->use_nodes = true;
    /* To use the forward pipeline. */
    glossy_mat->blend_method = MA_BM_BLEND;

    bNode *bsdf = nodeAddStaticNode(nullptr, ntree, SH_NODE_BSDF_GLOSSY);
    bNodeSocket *base_color = nodeFindSocket(bsdf, SOCK_IN, "Color");
    copy_v3_fl(((bNodeSocketValueRGBA *)base_color->default_value)->value, 1.0f);
    bNodeSocket *roughness = nodeFindSocket(bsdf, SOCK_IN, "Roughness");
    ((bNodeSocketValueFloat *)roughness->default_value)->value = 0.0f;

    bNode *output = nodeAddStaticNode(nullptr, ntree, SH_NODE_OUTPUT_MATERIAL);

    nodeAddLink(ntree,
                bsdf,
                nodeFindSocket(bsdf, SOCK_OUT, "BSDF"),
                output,
                nodeFindSocket(output, SOCK_IN, "Surface"));

    nodeSetActive(ntree, output);
  }
  {
    error_mat_ = (::Material *)BKE_id_new_nomain(ID_MA, "EEVEE default error");
    bNodeTree *ntree = bke::ntreeAddTreeEmbedded(
        nullptr, &error_mat_->id, "Shader Nodetree", ntreeType_Shader->idname);
    error_mat_->use_nodes = true;

    /* Use emission and output material to be compatible with both World and Material. */
    bNode *bsdf = nodeAddStaticNode(nullptr, ntree, SH_NODE_EMISSION);
    bNodeSocket *color = nodeFindSocket(bsdf, SOCK_IN, "Color");
    copy_v3_fl3(((bNodeSocketValueRGBA *)color->default_value)->value, 1.0f, 0.0f, 1.0f);

    bNode *output = nodeAddStaticNode(nullptr, ntree, SH_NODE_OUTPUT_MATERIAL);

    nodeAddLink(ntree,
                bsdf,
                nodeFindSocket(bsdf, SOCK_OUT, "Emission"),
                output,
                nodeFindSocket(output, SOCK_IN, "Surface"));

    nodeSetActive(ntree, output);
  }
}

MaterialModule::~MaterialModule()
{
  BKE_id_free(nullptr, glossy_mat);
  BKE_id_free(nullptr, diffuse_mat);
  BKE_id_free(nullptr, error_mat_);
}

void MaterialModule::begin_sync()
{
  queued_shaders_count = 0;

  material_map_.clear();
  shader_map_.clear();
}

MaterialPass MaterialModule::material_pass_get(Object *ob,
                                               ::Material *blender_mat,
                                               eMaterialPipeline pipeline_type,
                                               eMaterialGeometry geometry_type)
{
  bNodeTree *ntree = (blender_mat->use_nodes && blender_mat->nodetree != nullptr) ?
                         blender_mat->nodetree :
                         default_surface_ntree_.nodetree_get(blender_mat);

  MaterialPass matpass = MaterialPass();
  matpass.gpumat = inst_.shaders.material_shader_get(
      blender_mat, ntree, pipeline_type, geometry_type, true);

  switch (GPU_material_status(matpass.gpumat)) {
    case GPU_MAT_SUCCESS:
      break;
    case GPU_MAT_QUEUED:
      queued_shaders_count++;
      blender_mat = (geometry_type == MAT_GEOM_VOLUME) ? BKE_material_default_volume() :
                                                         BKE_material_default_surface();
      matpass.gpumat = inst_.shaders.material_shader_get(
          blender_mat, blender_mat->nodetree, pipeline_type, geometry_type, false);
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

  if (GPU_material_recalc_flag_get(matpass.gpumat)) {
    inst_.sampling.reset();
  }

  if (ELEM(pipeline_type,
           MAT_PIPE_FORWARD,
           MAT_PIPE_FORWARD_PREPASS,
           MAT_PIPE_FORWARD_PREPASS_VELOCITY) &&
      GPU_material_flag_get(matpass.gpumat, GPU_MATFLAG_TRANSPARENT))
  {
    /* Transparent pass is generated later. */
    matpass.sub_pass = nullptr;
  }
  else {
    ShaderKey shader_key(matpass.gpumat, geometry_type, pipeline_type);

    PassMain::Sub *shader_sub = shader_map_.lookup_or_add_cb(shader_key, [&]() {
      /* First time encountering this shader. Create a sub that will contain materials using it. */
      return inst_.pipelines.material_add(ob, blender_mat, matpass.gpumat, pipeline_type);
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
  eMaterialPipeline surface_pipe = (blender_mat->blend_method == MA_BM_BLEND) ? MAT_PIPE_FORWARD :
                                                                                MAT_PIPE_DEFERRED;
  eMaterialPipeline prepass_pipe = (blender_mat->blend_method == MA_BM_BLEND) ?
                                       (has_motion ? MAT_PIPE_FORWARD_PREPASS_VELOCITY :
                                                     MAT_PIPE_FORWARD_PREPASS) :
                                       (has_motion ? MAT_PIPE_DEFERRED_PREPASS_VELOCITY :
                                                     MAT_PIPE_DEFERRED_PREPASS);

  MaterialKey material_key(blender_mat, geometry_type, surface_pipe);

  Material &mat = material_map_.lookup_or_add_cb(material_key, [&]() {
    Material mat;
    /* Order is important for transparent. */
    mat.prepass = material_pass_get(ob, blender_mat, prepass_pipe, geometry_type);
    mat.shading = material_pass_get(ob, blender_mat, surface_pipe, geometry_type);
    if (blender_mat->blend_shadow == MA_BS_NONE) {
      mat.shadow = MaterialPass();
    }
    else {
      mat.shadow = material_pass_get(ob, blender_mat, MAT_PIPE_SHADOW, geometry_type);
    }
    mat.is_alpha_blend_transparent = (blender_mat->blend_method == MA_BM_BLEND) &&
                                     GPU_material_flag_get(mat.shading.gpumat,
                                                           GPU_MATFLAG_TRANSPARENT);
    return mat;
  });

  if (mat.is_alpha_blend_transparent) {
    /* Transparent needs to use one sub pass per object to support reordering.
     * NOTE: Pre-pass needs to be created first in order to be sorted first. */
    mat.prepass.sub_pass = inst_.pipelines.forward.prepass_transparent_add(
        ob, blender_mat, mat.shading.gpumat);
    mat.shading.sub_pass = inst_.pipelines.forward.material_transparent_add(
        ob, blender_mat, mat.shading.gpumat);
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
    /* \note: Perform a whole copy since next material_sync() can move the Material memory location
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
