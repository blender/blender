/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 */

#pragma once

#include "DRW_render.h"

#include "BLI_map.hh"
#include "BLI_vector.hh"
#include "GPU_material.h"

#include "eevee_sync.hh"

namespace blender::eevee {

class Instance;

/* -------------------------------------------------------------------- */
/** \name MaterialKey
 *
 * \{ */

enum eMaterialPipeline {
  MAT_PIPE_DEFERRED = 0,
  MAT_PIPE_FORWARD,
  MAT_PIPE_DEFERRED_PREPASS,
  MAT_PIPE_DEFERRED_PREPASS_VELOCITY,
  MAT_PIPE_FORWARD_PREPASS,
  MAT_PIPE_FORWARD_PREPASS_VELOCITY,
  MAT_PIPE_VOLUME,
  MAT_PIPE_SHADOW,
  MAT_PIPE_CAPTURE,
};

enum eMaterialGeometry {
  MAT_GEOM_MESH = 0,
  MAT_GEOM_POINT_CLOUD,
  MAT_GEOM_CURVES,
  MAT_GEOM_GPENCIL,
  MAT_GEOM_VOLUME_OBJECT,
  MAT_GEOM_VOLUME_WORLD,
  MAT_GEOM_WORLD,
};

static inline void material_type_from_shader_uuid(uint64_t shader_uuid,
                                                  eMaterialPipeline &pipeline_type,
                                                  eMaterialGeometry &geometry_type)
{
  const uint64_t geometry_mask = ((1u << 4u) - 1u);
  const uint64_t pipeline_mask = ((1u << 4u) - 1u);
  geometry_type = static_cast<eMaterialGeometry>(shader_uuid & geometry_mask);
  pipeline_type = static_cast<eMaterialPipeline>((shader_uuid >> 4u) & pipeline_mask);
}

static inline uint64_t shader_uuid_from_material_type(eMaterialPipeline pipeline_type,
                                                      eMaterialGeometry geometry_type)
{
  return geometry_type | (pipeline_type << 4);
}

ENUM_OPERATORS(eClosureBits, CLOSURE_AMBIENT_OCCLUSION)

static inline eClosureBits shader_closure_bits_from_flag(const GPUMaterial *gpumat)
{
  eClosureBits closure_bits = eClosureBits(0);
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_DIFFUSE)) {
    closure_bits |= CLOSURE_DIFFUSE;
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_TRANSPARENT)) {
    closure_bits |= CLOSURE_TRANSPARENCY;
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_EMISSION)) {
    closure_bits |= CLOSURE_EMISSION;
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_GLOSSY)) {
    closure_bits |= CLOSURE_REFLECTION;
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_SUBSURFACE)) {
    closure_bits |= CLOSURE_SSS;
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_REFRACT)) {
    closure_bits |= CLOSURE_REFRACTION;
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_HOLDOUT)) {
    closure_bits |= CLOSURE_HOLDOUT;
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_AO)) {
    closure_bits |= CLOSURE_AMBIENT_OCCLUSION;
  }
  return closure_bits;
}

static inline eMaterialGeometry to_material_geometry(const Object *ob)
{
  switch (ob->type) {
    case OB_CURVES:
      return MAT_GEOM_CURVES;
    case OB_VOLUME:
      return MAT_GEOM_VOLUME_OBJECT;
    case OB_GPENCIL_LEGACY:
      return MAT_GEOM_GPENCIL;
    case OB_POINTCLOUD:
      return MAT_GEOM_POINT_CLOUD;
    default:
      return MAT_GEOM_MESH;
  }
}

/** Unique key to identify each material in the hash-map. */
struct MaterialKey {
  ::Material *mat;
  uint64_t options;

  MaterialKey(::Material *mat_, eMaterialGeometry geometry, eMaterialPipeline surface_pipeline)
      : mat(mat_)
  {
    options = shader_uuid_from_material_type(surface_pipeline, geometry);
  }

  uint64_t hash() const
  {
    BLI_assert(options < sizeof(*mat));
    return uint64_t(mat) + options;
  }

  bool operator<(const MaterialKey &k) const
  {
    return (mat < k.mat) || (options < k.options);
  }

  bool operator==(const MaterialKey &k) const
  {
    return (mat == k.mat) && (options == k.options);
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name ShaderKey
 *
 * \{ */

struct ShaderKey {
  GPUShader *shader;
  uint64_t options;

  ShaderKey(GPUMaterial *gpumat,
            eMaterialGeometry geometry,
            eMaterialPipeline pipeline,
            char blend_flags,
            bool probe_capture)
  {
    shader = GPU_material_get_shader(gpumat);
    options = blend_flags;
    options = (options << 6u) | shader_uuid_from_material_type(pipeline, geometry);
    options = (options << 16u) | shader_closure_bits_from_flag(gpumat);
    options = (options << 1u) | uint64_t(probe_capture);
  }

  uint64_t hash() const
  {
    return uint64_t(shader) + options;
  }

  bool operator<(const ShaderKey &k) const
  {
    return (shader == k.shader) ? (options < k.options) : (shader < k.shader);
  }

  bool operator==(const ShaderKey &k) const
  {
    return (shader == k.shader) && (options == k.options);
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Default Material Node-Tree
 *
 * In order to support materials without nodetree we reuse and configure a standalone nodetree that
 * we pass for shader generation. The GPUMaterial is still stored inside the Material even if
 * it does not use the same nodetree.
 *
 * \{ */

class DefaultSurfaceNodeTree {
 private:
  bNodeTree *ntree_;
  bNodeSocketValueRGBA *color_socket_;
  bNodeSocketValueFloat *metallic_socket_;
  bNodeSocketValueFloat *roughness_socket_;
  bNodeSocketValueFloat *specular_socket_;

 public:
  DefaultSurfaceNodeTree();
  ~DefaultSurfaceNodeTree();

  /** Configure a default node-tree with the given material. */
  bNodeTree *nodetree_get(::Material *ma);
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material
 *
 * \{ */

struct MaterialPass {
  GPUMaterial *gpumat;
  PassMain::Sub *sub_pass;
};

struct Material {
  bool is_alpha_blend_transparent;
  MaterialPass shadow, shading, prepass, capture, probe_prepass, probe_shading, volume;
};

struct MaterialArray {
  Vector<Material> materials;
  Vector<GPUMaterial *> gpu_materials;
};

class MaterialModule {
 public:
  ::Material *diffuse_mat;
  ::Material *glossy_mat;

  int64_t queued_shaders_count = 0;

 private:
  Instance &inst_;

  Map<MaterialKey, Material> material_map_;
  Map<ShaderKey, PassMain::Sub *> shader_map_;

  MaterialArray material_array_;

  DefaultSurfaceNodeTree default_surface_ntree_;

  ::Material *error_mat_;

 public:
  MaterialModule(Instance &inst);
  ~MaterialModule();

  void begin_sync();

  /**
   * Returned Material references are valid until the next call to this function or material_get().
   */
  MaterialArray &material_array_get(Object *ob, bool has_motion);
  /**
   * Returned Material references are valid until the next call to this function or
   * material_array_get().
   */
  Material &material_get(Object *ob, bool has_motion, int mat_nr, eMaterialGeometry geometry_type);

 private:
  Material &material_sync(Object *ob,
                          ::Material *blender_mat,
                          eMaterialGeometry geometry_type,
                          bool has_motion);

  /** Return correct material or empty default material if slot is empty. */
  ::Material *material_from_slot(Object *ob, int slot);
  MaterialPass material_pass_get(Object *ob,
                                 ::Material *blender_mat,
                                 eMaterialPipeline pipeline_type,
                                 eMaterialGeometry geometry_type,
                                 bool probe_capture = false);
};

/** \} */

}  // namespace blender::eevee
