/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup eevee
 */

#include "DNA_material_types.h"

#include "BKE_lib_id.h"
#include "BKE_material.h"
#include "BKE_node.h"
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
    bNodeTree *ntree = ntreeAddTree(nullptr, "Shader Nodetree", ntreeType_Shader->idname);

    diffuse_mat = (::Material *)BKE_id_new_nomain(ID_MA, "EEVEE default diffuse");
    diffuse_mat->nodetree = ntree;
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
    bNodeTree *ntree = ntreeAddTree(nullptr, "Shader Nodetree", ntreeType_Shader->idname);

    glossy_mat = (::Material *)BKE_id_new_nomain(ID_MA, "EEVEE default metal");
    glossy_mat->nodetree = ntree;
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
    bNodeTree *ntree = ntreeAddTree(nullptr, "Shader Nodetree", ntreeType_Shader->idname);

    error_mat_ = (::Material *)BKE_id_new_nomain(ID_MA, "EEVEE default error");
    error_mat_->nodetree = ntree;
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
  for (Material *mat : material_map_.values()) {
    delete mat;
  }
  BKE_id_free(nullptr, glossy_mat);
  BKE_id_free(nullptr, diffuse_mat);
  BKE_id_free(nullptr, error_mat_);
}

void MaterialModule::begin_sync()
{
  queued_shaders_count = 0;

  for (Material *mat : material_map_.values()) {
    mat->init = false;
  }
  shader_map_.clear();
}

MaterialPass MaterialModule::material_pass_get(::Material *blender_mat,
                                               eMaterialPipeline pipeline_type,
                                               eMaterialGeometry geometry_type)
{
  bNodeTree *ntree = (blender_mat->use_nodes && blender_mat->nodetree != nullptr) ?
                         blender_mat->nodetree :
                         default_surface_ntree_.nodetree_get(blender_mat);

  MaterialPass matpass;
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

  if (GPU_material_recalc_flag_get(matpass.gpumat)) {
    // inst_.sampling.reset();
  }

  if ((pipeline_type == MAT_PIPE_DEFERRED) &&
      GPU_material_flag_get(matpass.gpumat, GPU_MATFLAG_SHADER_TO_RGBA)) {
    pipeline_type = MAT_PIPE_FORWARD;
  }

  if ((pipeline_type == MAT_PIPE_FORWARD) &&
      GPU_material_flag_get(matpass.gpumat, GPU_MATFLAG_TRANSPARENT)) {
    /* Transparent needs to use one shgroup per object to support reordering. */
    matpass.shgrp = inst_.pipelines.material_add(blender_mat, matpass.gpumat, pipeline_type);
  }
  else {
    ShaderKey shader_key(matpass.gpumat, geometry_type, pipeline_type);

    auto add_cb = [&]() -> DRWShadingGroup * {
      /* First time encountering this shader. Create a shading group. */
      return inst_.pipelines.material_add(blender_mat, matpass.gpumat, pipeline_type);
    };
    DRWShadingGroup *grp = shader_map_.lookup_or_add_cb(shader_key, add_cb);

    if (grp != nullptr) {
      /* Shading group for this shader already exists. Create a sub one for this material. */
      /* IMPORTANT: We always create a subgroup so that all subgroups are inserted after the
       * first "empty" shgroup. This avoids messing the order of subgroups when there is more
       * nested subgroup (i.e: hair drawing). */
      /* TODO(@fclem): Remove material resource binding from the first group creation. */
      matpass.shgrp = DRW_shgroup_create_sub(grp);
      DRW_shgroup_add_material_resources(matpass.shgrp, matpass.gpumat);
    }
  }

  return matpass;
}

Material &MaterialModule::material_sync(::Material *blender_mat,
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

  /* TEST until we have deferred pipeline up and running. */
  surface_pipe = MAT_PIPE_FORWARD;
  prepass_pipe = has_motion ? MAT_PIPE_FORWARD_PREPASS_VELOCITY : MAT_PIPE_FORWARD_PREPASS;

  MaterialKey material_key(blender_mat, geometry_type, surface_pipe);

  /* TODO: allocate in blocks to avoid memory fragmentation. */
  auto add_cb = [&]() { return new Material(); };
  Material &mat = *material_map_.lookup_or_add_cb(material_key, add_cb);

  /* Forward pipeline needs to use one shgroup per object. */
  if (mat.init == false || (surface_pipe == MAT_PIPE_FORWARD)) {
    mat.init = true;
    /* Order is important for transparent. */
    mat.prepass = material_pass_get(blender_mat, prepass_pipe, geometry_type);
    mat.shading = material_pass_get(blender_mat, surface_pipe, geometry_type);
    if (blender_mat->blend_shadow == MA_BS_NONE) {
      mat.shadow = MaterialPass();
    }
    else {
      mat.shadow = material_pass_get(blender_mat, MAT_PIPE_SHADOW, geometry_type);
    }

    mat.is_alpha_blend_transparent = (blender_mat->blend_method == MA_BM_BLEND) &&
                                     GPU_material_flag_get(mat.prepass.gpumat,
                                                           GPU_MATFLAG_TRANSPARENT);
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
    Material &mat = material_sync(blender_mat, to_material_geometry(ob), has_motion);
    material_array_.materials.append(&mat);
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
  Material &mat = material_sync(blender_mat, geometry_type, has_motion);
  return mat;
}

/** \} */

}  // namespace blender::eevee
