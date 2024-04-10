/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 */

#pragma once

#include "DRW_render.hh"

#include "BLI_map.hh"
#include "BLI_vector.hh"
#include "GPU_material.hh"

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
  /* These all map to the depth shader. */
  MAT_PIPE_PREPASS_DEFERRED,
  MAT_PIPE_PREPASS_DEFERRED_VELOCITY,
  MAT_PIPE_PREPASS_OVERLAP,
  MAT_PIPE_PREPASS_FORWARD,
  MAT_PIPE_PREPASS_FORWARD_VELOCITY,
  MAT_PIPE_PREPASS_PLANAR,

  MAT_PIPE_VOLUME_MATERIAL,
  MAT_PIPE_VOLUME_OCCUPANCY,
  MAT_PIPE_SHADOW,
  MAT_PIPE_CAPTURE,
};

enum eMaterialGeometry {
  /* These maps directly to object types. */
  MAT_GEOM_MESH = 0,
  MAT_GEOM_POINT_CLOUD,
  MAT_GEOM_CURVES,
  MAT_GEOM_GPENCIL,
  MAT_GEOM_VOLUME,

  /* These maps to special shader. */
  MAT_GEOM_WORLD,
};

static inline bool geometry_type_has_surface(eMaterialGeometry geometry_type)
{
  return geometry_type < MAT_GEOM_VOLUME;
}

enum eMaterialDisplacement {
  MAT_DISPLACEMENT_BUMP = 0,
  MAT_DISPLACEMENT_VERTEX_WITH_BUMP,
};

static inline eMaterialDisplacement to_displacement_type(int displacement_method)
{
  switch (displacement_method) {
    case MA_DISPLACEMENT_DISPLACE:
      /* Currently unsupported. Revert to vertex displacement + bump. */
      ATTR_FALLTHROUGH;
    case MA_DISPLACEMENT_BOTH:
      return MAT_DISPLACEMENT_VERTEX_WITH_BUMP;
    default:
      return MAT_DISPLACEMENT_BUMP;
  }
}

enum eMaterialProbe {
  MAT_PROBE_NONE = 0,
  MAT_PROBE_REFLECTION,
  MAT_PROBE_PLANAR,
};

static inline void material_type_from_shader_uuid(uint64_t shader_uuid,
                                                  eMaterialPipeline &pipeline_type,
                                                  eMaterialGeometry &geometry_type,
                                                  eMaterialDisplacement &displacement_type,
                                                  bool &transparent_shadows)
{
  const uint64_t geometry_mask = ((1u << 4u) - 1u);
  const uint64_t pipeline_mask = ((1u << 4u) - 1u);
  const uint64_t displacement_mask = ((1u << 2u) - 1u);
  geometry_type = static_cast<eMaterialGeometry>(shader_uuid & geometry_mask);
  pipeline_type = static_cast<eMaterialPipeline>((shader_uuid >> 4u) & pipeline_mask);
  displacement_type = static_cast<eMaterialDisplacement>((shader_uuid >> 8u) & displacement_mask);
  transparent_shadows = (shader_uuid >> 10u) & 1u;
}

static inline uint64_t shader_uuid_from_material_type(
    eMaterialPipeline pipeline_type,
    eMaterialGeometry geometry_type,
    eMaterialDisplacement displacement_type = MAT_DISPLACEMENT_BUMP,
    char blend_flags = 0)
{
  BLI_assert(displacement_type < (1 << 2));
  BLI_assert(geometry_type < (1 << 4));
  BLI_assert(pipeline_type < (1 << 4));
  uint64_t transparent_shadows = blend_flags & MA_BL_TRANSPARENT_SHADOW ? 1 : 0;
  return geometry_type | (pipeline_type << 4) | (displacement_type << 8) |
         (transparent_shadows << 10);
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
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_TRANSLUCENT)) {
    closure_bits |= CLOSURE_TRANSLUCENT;
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_EMISSION)) {
    closure_bits |= CLOSURE_EMISSION;
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_GLOSSY)) {
    closure_bits |= CLOSURE_REFLECTION;
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_COAT)) {
    closure_bits |= CLOSURE_CLEARCOAT;
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
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_SHADER_TO_RGBA)) {
    closure_bits |= CLOSURE_SHADER_TO_RGBA;
  }
  return closure_bits;
}

static inline eMaterialGeometry to_material_geometry(const Object *ob)
{
  switch (ob->type) {
    case OB_CURVES:
      return MAT_GEOM_CURVES;
    case OB_VOLUME:
      return MAT_GEOM_VOLUME;
    case OB_GPENCIL_LEGACY:
      return MAT_GEOM_GPENCIL;
    case OB_POINTCLOUD:
      return MAT_GEOM_POINT_CLOUD;
    default:
      return MAT_GEOM_MESH;
  }
}

/**
 * Unique key to identify each material in the hash-map.
 * This is above the shader binning.
 */
struct MaterialKey {
  ::Material *mat;
  uint64_t options;

  MaterialKey(::Material *mat_,
              eMaterialGeometry geometry,
              eMaterialPipeline pipeline,
              short visibility_flags)
      : mat(mat_)
  {
    options = shader_uuid_from_material_type(
        pipeline, geometry, to_displacement_type(mat_->displacement_method), mat_->blend_flag);
    options = (options << 1) | (visibility_flags & OB_HIDE_SHADOW ? 0 : 1);
    options = (options << 1) | (visibility_flags & OB_HIDE_PROBE_CUBEMAP ? 0 : 1);
    options = (options << 1) | (visibility_flags & OB_HIDE_PROBE_PLANAR ? 0 : 1);
  }

  uint64_t hash() const
  {
    return uint64_t(mat) + options;
  }

  bool operator<(const MaterialKey &k) const
  {
    if (mat == k.mat) {
      return options < k.options;
    }
    return mat < k.mat;
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

/**
 * Key used to find the sub-pass that already renders objects with the same shader.
 * This avoids the cost associated with shader switching.
 * This is below the material binning.
 * Should only include pipeline options that are not baked in the shader itself.
 */
struct ShaderKey {
  GPUShader *shader;
  uint64_t options;

  ShaderKey(GPUMaterial *gpumat, eMaterialProbe probe_capture)
  {
    shader = GPU_material_get_shader(gpumat);
    options = uint64_t(probe_capture);
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
  bool has_transparent_shadows;
  bool has_surface;
  bool has_volume;
  MaterialPass shadow;
  MaterialPass shading;
  MaterialPass prepass;
  MaterialPass overlap_masking;
  MaterialPass capture;
  MaterialPass reflection_probe_prepass;
  MaterialPass reflection_probe_shading;
  MaterialPass planar_probe_prepass;
  MaterialPass planar_probe_shading;
  MaterialPass volume_occupancy;
  MaterialPass volume_material;
};

struct MaterialArray {
  Vector<Material> materials;
  Vector<GPUMaterial *> gpu_materials;
};

class MaterialModule {
 public:
  ::Material *diffuse_mat;
  ::Material *metallic_mat;

  int64_t queued_shaders_count = 0;
  int64_t queued_optimize_shaders_count = 0;

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
                                 eMaterialProbe probe_capture = MAT_PROBE_NONE);
};

/** \} */

}  // namespace blender::eevee
