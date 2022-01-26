/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2021 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Descriptor type used to define shader structure, resources and interfaces.
 *
 * Some rule of thumb:
 * - Do not include anything else than this file in each info file.
 */

#pragma once

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"
#include "GPU_texture.h"

namespace blender::gpu::shader {

#ifndef GPU_SHADER_CREATE_INFO
/* Helps intelisense / auto-completion. */
#  define GPU_SHADER_INTERFACE_INFO(_interface, _inst_name) \
    StageInterfaceInfo _interface(#_interface, _inst_name); \
    _interface
#  define GPU_SHADER_CREATE_INFO(_info) \
    ShaderCreateInfo _info(#_info); \
    _info
#endif

enum class Type {
  FLOAT = 0,
  VEC2,
  VEC3,
  VEC4,
  MAT3,
  MAT4,
  UINT,
  UVEC2,
  UVEC3,
  UVEC4,
  INT,
  IVEC2,
  IVEC3,
  IVEC4,
  BOOL,
};

enum class BuiltinBits {
  /**
   * Allow getting barycentric coordinates inside the fragment shader.
   * \note Emulated on OpenGL.
   */
  BARYCENTRIC_COORD = (1 << 0),
  FRAG_COORD = (1 << 2),
  FRONT_FACING = (1 << 4),
  GLOBAL_INVOCATION_ID = (1 << 5),
  INSTANCE_ID = (1 << 6),
  LAYER = (1 << 7),
  LOCAL_INVOCATION_ID = (1 << 8),
  LOCAL_INVOCATION_INDEX = (1 << 9),
  NUM_WORK_GROUP = (1 << 10),
  POINT_COORD = (1 << 11),
  POINT_SIZE = (1 << 12),
  PRIMITIVE_ID = (1 << 13),
  VERTEX_ID = (1 << 14),
  WORK_GROUP_ID = (1 << 15),
  WORK_GROUP_SIZE = (1 << 16),
};
ENUM_OPERATORS(BuiltinBits, BuiltinBits::WORK_GROUP_SIZE);

/* Samplers & images. */
enum class ImageType {
  /** Color samplers/image. */
  FLOAT_BUFFER = 0,
  FLOAT_1D,
  FLOAT_1D_ARRAY,
  FLOAT_2D,
  FLOAT_2D_ARRAY,
  FLOAT_3D,
  FLOAT_CUBE,
  FLOAT_CUBE_ARRAY,
  INT_BUFFER,
  INT_1D,
  INT_1D_ARRAY,
  INT_2D,
  INT_2D_ARRAY,
  INT_3D,
  INT_CUBE,
  INT_CUBE_ARRAY,
  UINT_BUFFER,
  UINT_1D,
  UINT_1D_ARRAY,
  UINT_2D,
  UINT_2D_ARRAY,
  UINT_3D,
  UINT_CUBE,
  UINT_CUBE_ARRAY,
  /** Depth samplers (not supported as image). */
  SHADOW_2D,
  SHADOW_2D_ARRAY,
  SHADOW_CUBE,
  SHADOW_CUBE_ARRAY,
  DEPTH_2D,
  DEPTH_2D_ARRAY,
  DEPTH_CUBE,
  DEPTH_CUBE_ARRAY,
};

/* Storage qualifiers. */
enum class Qualifier {
  RESTRICT = (1 << 0),
  READ_ONLY = (1 << 1),
  WRITE_ONLY = (1 << 2),
  QUALIFIER_MAX = (WRITE_ONLY << 1) - 1,
};
ENUM_OPERATORS(Qualifier, Qualifier::QUALIFIER_MAX);

enum class Frequency {
  BATCH = 0,
  PASS,
};

/* Dual Source Blending Index. */
enum class DualBlend {
  NONE = 0,
  SRC_0,
  SRC_1,
};

/* Interpolation qualifiers. */
enum class Interpolation {
  SMOOTH = 0,
  FLAT,
  NO_PERSPECTIVE,
};

/** Input layout for geometry shader. */
enum class PrimitiveIn {
  POINTS = 0,
  LINES,
  LINES_ADJACENCY,
  TRIANGLES,
  TRIANGLES_ADJACENCY,
};

/** Output layout for geometry shader. */
enum class PrimitiveOut {
  POINTS = 0,
  LINE_STRIP,
  TRIANGLE_STRIP,
};

struct StageInterfaceInfo {
  struct InOut {
    Interpolation interp;
    Type type;
    StringRefNull name;
  };

  StringRefNull name;
  /** Name of the instance of the block (used to access).
   *  Can be empty string (i.e: "") only if not using geometry shader. */
  StringRefNull instance_name;
  /** List of all members of the interface. */
  Vector<InOut> inouts;

  StageInterfaceInfo(const char *name_, const char *instance_name_)
      : name(name_), instance_name(instance_name_){};
  ~StageInterfaceInfo(){};

  using Self = StageInterfaceInfo;

  Self &smooth(Type type, StringRefNull _name)
  {
    inouts.append({Interpolation::SMOOTH, type, _name});
    return *(Self *)this;
  }

  Self &flat(Type type, StringRefNull _name)
  {
    inouts.append({Interpolation::FLAT, type, _name});
    return *(Self *)this;
  }

  Self &no_perspective(Type type, StringRefNull _name)
  {
    inouts.append({Interpolation::NO_PERSPECTIVE, type, _name});
    return *(Self *)this;
  }
};

/**
 * @brief Describe inputs & outputs, stage interfaces, resources and sources of a shader.
 *        If all data is correctly provided, this is all that is needed to create and compile
 *        a GPUShader.
 *
 * IMPORTANT: All strings are references only. Make sure all the strings used by a
 *            ShaderCreateInfo are not freed until it is consumed or deleted.
 */
struct ShaderCreateInfo {
  /** Shader name for debugging. */
  StringRefNull name_;
  /** True if the shader is static and can be pre-compiled at compile time. */
  bool do_static_compilation_ = false;
  /** If true, all additionally linked create info will be merged into this one. */
  bool finalized_ = false;
  /**
   * Maximum length of all the resource names including each null terminator.
   * Only for names used by gpu::ShaderInterface.
   */
  size_t interface_names_size_ = 0;

  struct VertIn {
    int index;
    Type type;
    StringRefNull name;
  };
  Vector<VertIn> vertex_inputs_;

  struct GeometryStageLayout {
    PrimitiveIn primitive_in;
    int invocations;
    PrimitiveOut primitive_out;
    /** Set to -1 by default to check if used. */
    int max_vertices = -1;
  };
  GeometryStageLayout geometry_layout_;

  struct ComputeStageLayout {
    int local_size_x = -1;
    int local_size_y = -1;
    int local_size_z = -1;
  };

  ComputeStageLayout compute_layout_;

  struct FragOut {
    int index;
    Type type;
    DualBlend blend;
    StringRefNull name;
  };
  Vector<FragOut> fragment_outputs_;

  struct Sampler {
    ImageType type;
    eGPUSamplerState sampler;
    StringRefNull name;
  };

  struct Image {
    eGPUTextureFormat format;
    ImageType type;
    Qualifier qualifiers;
    StringRefNull name;
  };

  struct UniformBuf {
    StringRefNull type_name;
    StringRefNull name;
  };

  struct StorageBuf {
    Qualifier qualifiers;
    StringRefNull type_name;
    StringRefNull name;
  };

  struct Resource {
    enum BindType {
      UNIFORM_BUFFER = 0,
      STORAGE_BUFFER,
      SAMPLER,
      IMAGE,
    };

    BindType bind_type;
    int slot;
    union {
      Sampler sampler;
      Image image;
      UniformBuf uniformbuf;
      StorageBuf storagebuf;
    };

    Resource(BindType type, int _slot) : bind_type(type), slot(_slot){};
  };
  /**
   * Resources are grouped by frequency of change.
   * Pass resources are meant to be valid for the whole pass.
   * Batch resources can be changed in a more granular manner (per object/material).
   * Mis-usage will only produce suboptimal performance.
   */
  Vector<Resource> pass_resources_, batch_resources_;

  Vector<StageInterfaceInfo *> vertex_out_interfaces_;
  Vector<StageInterfaceInfo *> geometry_out_interfaces_;

  struct PushConst {
    int index;
    Type type;
    StringRefNull name;
    int array_size;
  };

  Vector<PushConst> push_constants_;

  /* Sources for resources type definitions. */
  Vector<StringRefNull> typedef_sources_;

  StringRefNull vertex_source_, geometry_source_, fragment_source_, compute_source_;

  Vector<std::array<StringRefNull, 2>> defines_;
  /**
   * Name of other infos to recursively merge with this one.
   * No data slot must overlap otherwise we throw an error.
   */
  Vector<StringRefNull> additional_infos_;

 public:
  ShaderCreateInfo(const char *name) : name_(name){};
  ~ShaderCreateInfo(){};

  using Self = ShaderCreateInfo;

  /* -------------------------------------------------------------------- */
  /** \name Shaders in/outs (fixed function pipeline config)
   * \{ */

  Self &vertex_in(int slot, Type type, StringRefNull name)
  {
    vertex_inputs_.append({slot, type, name});
    interface_names_size_ += name.size() + 1;
    return *(Self *)this;
  }

  Self &vertex_out(StageInterfaceInfo &interface)
  {
    vertex_out_interfaces_.append(&interface);
    return *(Self *)this;
  }

  /**
   * IMPORTANT: invocations count is only used if GL_ARB_gpu_shader5 is supported. On
   * implementations that do not supports it, the max_vertices will be be multiplied by
   * invocations. Your shader needs to account for this fact. Use `#ifdef GPU_ARB_gpu_shader5`
   * and make a code path that does not rely on gl_InvocationID.
   */
  Self &geometry_layout(PrimitiveIn prim_in,
                        PrimitiveOut prim_out,
                        int max_vertices,
                        int invocations = -1)
  {
    geometry_layout_.primitive_in = prim_in;
    geometry_layout_.primitive_out = prim_out;
    geometry_layout_.max_vertices = max_vertices;
    geometry_layout_.invocations = invocations;
    return *(Self *)this;
  }

  Self &local_group_size(int local_size_x = -1, int local_size_y = -1, int local_size_z = -1)
  {
    compute_layout_.local_size_x = local_size_x;
    compute_layout_.local_size_y = local_size_y;
    compute_layout_.local_size_z = local_size_z;
    return *(Self *)this;
  }

  /**
   * Only needed if geometry shader is enabled.
   * IMPORTANT: Input and output instance name will have respectively "_in" and "_out" suffix
   * appended in the geometry shader IF AND ONLY IF the vertex_out interface instance name matches
   * the geometry_out interface instance name.
   */
  Self &geometry_out(StageInterfaceInfo &interface)
  {
    geometry_out_interfaces_.append(&interface);
    return *(Self *)this;
  }

  Self &fragment_out(int slot, Type type, StringRefNull name, DualBlend blend = DualBlend::NONE)
  {
    fragment_outputs_.append({slot, type, blend, name});
    return *(Self *)this;
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Resources bindings points
   * \{ */

  Self &uniform_buf(int slot,
                    StringRefNull type_name,
                    StringRefNull name,
                    Frequency freq = Frequency::PASS)
  {
    Resource res(Resource::BindType::UNIFORM_BUFFER, slot);
    res.uniformbuf.name = name;
    res.uniformbuf.type_name = type_name;
    ((freq == Frequency::PASS) ? pass_resources_ : batch_resources_).append(res);
    interface_names_size_ += name.size() + 1;
    return *(Self *)this;
  }

  Self &storage_buf(int slot,
                    Qualifier qualifiers,
                    StringRefNull type_name,
                    StringRefNull name,
                    Frequency freq = Frequency::PASS)
  {
    Resource res(Resource::BindType::STORAGE_BUFFER, slot);
    res.storagebuf.qualifiers = qualifiers;
    res.storagebuf.type_name = type_name;
    res.storagebuf.name = name;
    ((freq == Frequency::PASS) ? pass_resources_ : batch_resources_).append(res);
    interface_names_size_ += name.size() + 1;
    return *(Self *)this;
  }

  Self &image(int slot,
              eGPUTextureFormat format,
              Qualifier qualifiers,
              ImageType type,
              StringRefNull name,
              Frequency freq = Frequency::PASS)
  {
    Resource res(Resource::BindType::IMAGE, slot);
    res.image.format = format;
    res.image.qualifiers = qualifiers;
    res.image.type = type;
    res.image.name = name;
    ((freq == Frequency::PASS) ? pass_resources_ : batch_resources_).append(res);
    interface_names_size_ += name.size() + 1;
    return *(Self *)this;
  }

  Self &sampler(int slot,
                ImageType type,
                StringRefNull name,
                Frequency freq = Frequency::PASS,
                eGPUSamplerState sampler = (eGPUSamplerState)-1)
  {
    Resource res(Resource::BindType::SAMPLER, slot);
    res.sampler.type = type;
    res.sampler.name = name;
    res.sampler.sampler = sampler;
    ((freq == Frequency::PASS) ? pass_resources_ : batch_resources_).append(res);
    interface_names_size_ += name.size() + 1;
    return *(Self *)this;
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Shader Source
   * \{ */

  Self &vertex_source(StringRefNull filename)
  {
    vertex_source_ = filename;
    return *(Self *)this;
  }

  Self &geometry_source(StringRefNull filename)
  {
    geometry_source_ = filename;
    return *(Self *)this;
  }

  Self &fragment_source(StringRefNull filename)
  {
    fragment_source_ = filename;
    return *(Self *)this;
  }

  Self &compute_source(StringRefNull filename)
  {
    compute_source_ = filename;
    return *(Self *)this;
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Push constants
   *
   * Data managed by GPUShader. Can be set through uniform functions. Must be less than 128bytes.
   * One slot represents 4bytes. Each element needs to have enough empty space left after it.
   * example:
   * [0] = PUSH_CONSTANT(MAT4, "ModelMatrix"),
   * ---- 16 slots occupied by ModelMatrix ----
   * [16] = PUSH_CONSTANT(VEC4, "color"),
   * ---- 4 slots occupied by color ----
   * [20] = PUSH_CONSTANT(BOOL, "srgbToggle"),
   * The maximum slot is 31.
   * \{ */

  Self &push_constant(int slot, Type type, StringRefNull name, int array_size = 0)
  {
    BLI_assert_msg(name.find("[") == -1,
                   "Array syntax is forbidden for push constants."
                   "Use the array_size parameter instead.");
    push_constants_.append({slot, type, name, array_size});
    interface_names_size_ += name.size() + 1;
    return *(Self *)this;
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Defines
   * \{ */

  Self &define(StringRefNull name, StringRefNull value = "")
  {
    defines_.append({name, value});
    return *(Self *)this;
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Defines
   * \{ */

  Self &do_static_compilation(bool value)
  {
    do_static_compilation_ = value;
    return *(Self *)this;
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Additional Create Info
   *
   * Used to share parts of the infos that are common to many shaders.
   * \{ */

  Self &additional_info(StringRefNull info_name0,
                        StringRefNull info_name1 = "",
                        StringRefNull info_name2 = "",
                        StringRefNull info_name3 = "",
                        StringRefNull info_name4 = "",
                        StringRefNull info_name5 = "",
                        StringRefNull info_name6 = "")
  {
    additional_infos_.append(info_name0);
    if (!info_name1.is_empty()) {
      additional_infos_.append(info_name1);
    }
    if (!info_name2.is_empty()) {
      additional_infos_.append(info_name2);
    }
    if (!info_name3.is_empty()) {
      additional_infos_.append(info_name3);
    }
    if (!info_name4.is_empty()) {
      additional_infos_.append(info_name4);
    }
    if (!info_name5.is_empty()) {
      additional_infos_.append(info_name5);
    }
    if (!info_name6.is_empty()) {
      additional_infos_.append(info_name6);
    }
    return *(Self *)this;
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Typedef Sources
   *
   * Some resource declarations might need some special structure defined.
   * Adding a file using typedef_source will include it before the resource
   * and interface definitions.
   * \{ */

  Self &typedef_source(StringRefNull filename)
  {
    typedef_sources_.append(filename);
    return *(Self *)this;
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Recursive evaluation.
   *
   * Flatten all dependency so that this descriptor contains all the data from the additional
   * descriptors. This avoids tedious traversal in shader source creation.
   * \{ */

  /* WARNING: Recursive. */
  void finalize();

  /** Error detection that some backend compilers do not complain about. */
  void validate(const ShaderCreateInfo &other_info);

  /** \} */
};

}  // namespace blender::gpu::shader
