/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * A `blender::gpu::Texture` is a wrapper around backend specific texture objects.
 * It allows, creation of diverse texture format and types, update, read, reference counting,
 * internal sampler state tracking and texture binding.
 */

#pragma once

#include <string>

#include "BLI_assert.h"
#include "BLI_enum_flags.hh"

#include "GPU_format.hh"

namespace blender::gpu {
class VertBuf;
}

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Texture Formats
 * \{ */

/**
 * Formats compatible with read-only texture.
 */
enum class TextureFormat : uint8_t {
  Invalid = 0,

#define DECLARE(a, b, c, blender_enum, d, e, f, g, h) blender_enum = int(DataFormat::blender_enum),

#define GPU_TEXTURE_FORMAT_EXPAND(impl) \
  SNORM_8_(impl) \
  SNORM_8_8_(impl) \
  SNORM_8_8_8_(impl) /* TODO(fclem): Incompatible with metal, to remove. */ \
  SNORM_8_8_8_8_(impl) \
\
  SNORM_16_(impl) \
  SNORM_16_16_(impl) \
  SNORM_16_16_16_(impl) /* TODO(fclem): Incompatible with metal, to remove. */ \
  SNORM_16_16_16_16_(impl) \
\
  UNORM_8_(impl) \
  UNORM_8_8_(impl) \
  UNORM_8_8_8_(impl) /* TODO(fclem): Incompatible with metal, to remove. */ \
  UNORM_8_8_8_8_(impl) \
\
  UNORM_16_(impl) \
  UNORM_16_16_(impl) \
  UNORM_16_16_16_(impl) /* TODO(fclem): Incompatible with metal, to remove. */ \
  UNORM_16_16_16_16_(impl) \
\
  SINT_8_(impl) \
  SINT_8_8_(impl) \
  SINT_8_8_8_(impl) /* TODO(fclem): Incompatible with metal, to remove. */ \
  SINT_8_8_8_8_(impl) \
\
  SINT_16_(impl) \
  SINT_16_16_(impl) \
  SINT_16_16_16_(impl) /* TODO(fclem): Incompatible with metal, to remove. */ \
  SINT_16_16_16_16_(impl) \
\
  SINT_32_(impl) \
  SINT_32_32_(impl) \
  SINT_32_32_32_(impl) /* TODO(fclem): Incompatible with metal, to remove. */ \
  SINT_32_32_32_32_(impl) \
\
  UINT_8_(impl) \
  UINT_8_8_(impl) \
  UINT_8_8_8_(impl) /* TODO(fclem): Incompatible with metal, to remove. */ \
  UINT_8_8_8_8_(impl) \
\
  UINT_16_(impl) \
  UINT_16_16_(impl) \
  UINT_16_16_16_(impl) /* TODO(fclem): Incompatible with metal, to remove. */ \
  UINT_16_16_16_16_(impl) \
\
  UINT_32_(impl) \
  UINT_32_32_(impl) \
  UINT_32_32_32_(impl) /* TODO(fclem): Incompatible with metal, to remove. */ \
  UINT_32_32_32_32_(impl) \
\
  SFLOAT_16_(impl) \
  SFLOAT_16_16_(impl) \
  SFLOAT_16_16_16_(impl) /* TODO(fclem): Incompatible with metal, to remove. */ \
  SFLOAT_16_16_16_16_(impl) \
\
  SFLOAT_32_(impl) \
  SFLOAT_32_32_(impl) \
  SFLOAT_32_32_32_(impl) /* TODO(fclem): Incompatible with metal, to remove. */ \
  SFLOAT_32_32_32_32_(impl) \
\
  UNORM_10_10_10_2_(impl) \
  UINT_10_10_10_2_(impl) \
\
  UFLOAT_11_11_10_(impl) \
  UFLOAT_9_9_9_EXP_5_(impl) \
\
  UNORM_16_DEPTH_(impl) \
  SFLOAT_32_DEPTH_(impl) \
  SFLOAT_32_DEPTH_UINT_8_(impl) \
\
  SRGBA_8_8_8_(impl) /* TODO(fclem): Incompatible with metal, to remove. */ \
  SRGBA_8_8_8_8_(impl) \
\
  SNORM_DXT1_(impl) \
  SNORM_DXT3_(impl) \
  SNORM_DXT5_(impl) \
  SRGB_DXT1_(impl) \
  SRGB_DXT3_(impl) \
  SRGB_DXT5_(impl)

  GPU_TEXTURE_FORMAT_EXPAND(DECLARE)

#undef DECLARE
};

inline constexpr DataFormat to_data_format(TextureFormat format)
{
  return DataFormat(int(format));
}

/**
 * Formats compatible with frame-buffer attachments.
 */
enum class TextureTargetFormat : uint8_t {
  Invalid = 0,

#define DECLARE(a, b, c, blender_enum, d, e, f, g, h) \
  blender_enum = int(TextureFormat::blender_enum),

#define GPU_TEXTURE_TARGET_FORMAT_EXPAND(impl) \
  UNORM_8_(impl) \
  UNORM_8_8_(impl) \
  UNORM_8_8_8_8_(impl) \
\
  UNORM_16_(impl) \
  UNORM_16_16_(impl) \
  UNORM_16_16_16_16_(impl) \
\
  SINT_8_(impl) \
  SINT_8_8_(impl) \
  SINT_8_8_8_8_(impl) \
\
  SINT_16_(impl) \
  SINT_16_16_(impl) \
  SINT_16_16_16_16_(impl) \
\
  SINT_32_(impl) \
  SINT_32_32_(impl) \
  SINT_32_32_32_32_(impl) \
\
  UINT_8_(impl) \
  UINT_8_8_(impl) \
  UINT_8_8_8_8_(impl) \
\
  UINT_16_(impl) \
  UINT_16_16_(impl) \
  UINT_16_16_16_16_(impl) \
\
  UINT_32_(impl) \
  UINT_32_32_(impl) \
  UINT_32_32_32_32_(impl) \
\
  SFLOAT_16_(impl) \
  SFLOAT_16_16_(impl) \
  SFLOAT_16_16_16_16_(impl) \
\
  SFLOAT_32_(impl) \
  SFLOAT_32_32_(impl) \
  SFLOAT_32_32_32_32_(impl) \
\
  UNORM_10_10_10_2_(impl) \
  UINT_10_10_10_2_(impl) \
\
  UFLOAT_11_11_10_(impl) \
\
  UNORM_16_DEPTH_(impl) \
  SFLOAT_32_DEPTH_(impl) \
  SFLOAT_32_DEPTH_UINT_8_(impl) \
\
  SRGBA_8_8_8_8_(impl)

  GPU_TEXTURE_TARGET_FORMAT_EXPAND(DECLARE)

#undef DECLARE
};

inline constexpr TextureFormat to_texture_format(TextureTargetFormat format)
{
  return TextureFormat(int(format));
}

/**
 * Formats compatible with shader load/store.
 */
enum class TextureWriteFormat : uint8_t {
  Invalid = 0,

#define DECLARE(a, b, c, blender_enum, d, e, f, g, h) \
  blender_enum = int(TextureFormat::blender_enum),

#define GPU_TEXTURE_WRITE_FORMAT_EXPAND(impl) \
  UNORM_8_(impl) \
  UNORM_8_8_(impl) \
  UNORM_8_8_8_8_(impl) \
\
  UNORM_16_(impl) \
  UNORM_16_16_(impl) \
  UNORM_16_16_16_16_(impl) \
\
  SINT_8_(impl) \
  SINT_8_8_(impl) \
  SINT_8_8_8_8_(impl) \
\
  SINT_16_(impl) \
  SINT_16_16_(impl) \
  SINT_16_16_16_16_(impl) \
\
  SINT_32_(impl) \
  SINT_32_32_(impl) \
  SINT_32_32_32_32_(impl) \
\
  UINT_8_(impl) \
  UINT_8_8_(impl) \
  UINT_8_8_8_8_(impl) \
\
  UINT_16_(impl) \
  UINT_16_16_(impl) \
  UINT_16_16_16_16_(impl) \
\
  UINT_32_(impl) \
  UINT_32_32_(impl) \
  UINT_32_32_32_32_(impl) \
\
  SFLOAT_16_(impl) \
  SFLOAT_16_16_(impl) \
  SFLOAT_16_16_16_16_(impl) \
\
  SFLOAT_32_(impl) \
  SFLOAT_32_32_(impl) \
  SFLOAT_32_32_32_32_(impl) \
\
  UNORM_10_10_10_2_(impl) \
  UINT_10_10_10_2_(impl) \
\
  UFLOAT_11_11_10_(impl)

  GPU_TEXTURE_WRITE_FORMAT_EXPAND(DECLARE)

#undef DECLARE
};

inline constexpr TextureFormat to_texture_format(TextureWriteFormat format)
{
  return TextureFormat(int(format));
}

/** \} */

}  // namespace blender::gpu

/* -------------------------------------------------------------------- */
/** \name Sampler State
 * \{ */

/**
 * The `GPUSamplerFiltering` bit flag specifies the enabled filtering options of a texture
 * sampler.
 */
enum GPUSamplerFiltering {
  /**
   * Default sampler filtering with all options off.
   * It means no linear filtering, no mipmapping, and no anisotropic filtering.
   */
  GPU_SAMPLER_FILTERING_DEFAULT = 0,
  /**
   * Enables hardware linear filtering.
   * Also enables linear interpolation between MIPS if GPU_SAMPLER_FILTERING_MIPMAP is set.
   */
  GPU_SAMPLER_FILTERING_LINEAR = (1 << 0),
  /**
   * Enables mipmap access through shader samplers.
   * Also enables linear interpolation between mips if GPU_SAMPLER_FILTER is set, otherwise the mip
   * interpolation will be set to nearest.
   *
   * The following parameters are always left to their default values and can't be changed:
   * - TEXTURE_MIN_LOD is -1000.
   * - TEXTURE_MAX_LOD is 1000.
   * - TEXTURE_LOD_BIAS is 0.0f.
   */
  GPU_SAMPLER_FILTERING_MIPMAP = (1 << 1),
  /**
   * Enable Anisotropic filtering. This only has effect if `GPU_SAMPLER_FILTERING_MIPMAP` is set.
   * The filtered result is implementation dependent.
   *
   * The maximum amount of samples is always set to its maximum possible value and can't be
   * changed, except by the user through the user preferences, see the use of U.anisotropic_filter.
   */
  GPU_SAMPLER_FILTERING_ANISOTROPIC = (1 << 2),
};

ENUM_OPERATORS(GPUSamplerFiltering)

/** The number of every possible filtering configuration. */
static const int GPU_SAMPLER_FILTERING_TYPES_COUNT = (GPU_SAMPLER_FILTERING_LINEAR |
                                                      GPU_SAMPLER_FILTERING_MIPMAP |
                                                      GPU_SAMPLER_FILTERING_ANISOTROPIC) +
                                                     1;

/**
 * The `GPUSamplerExtendMode` specifies how the texture will be extrapolated for out-of-bound
 * texture sampling.
 */
enum GPUSamplerExtendMode {
  /**
   * Extrapolate by extending the edge pixels of the texture, in other words, the texture
   * coordinates are clamped.
   */
  GPU_SAMPLER_EXTEND_MODE_EXTEND = 0,
  /** Extrapolate by repeating the texture. */
  GPU_SAMPLER_EXTEND_MODE_REPEAT,
  /** Extrapolate by repeating the texture with mirroring in a ping-pong fashion. */
  GPU_SAMPLER_EXTEND_MODE_MIRRORED_REPEAT,
  /**
   * Extrapolate using the value of TEXTURE_BORDER_COLOR, which is always set to a transparent
   * black color (0, 0, 0, 0) and can't be changed.
   */
  GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER,
};

#define GPU_SAMPLER_EXTEND_MODES_COUNT (GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER + 1)

/**
 * The `GPUSamplerCustomType` specifies pre-defined sampler configurations with parameters that
 * are not controllable using the GPUSamplerFiltering and GPUSamplerExtendMode options. Hence, the
 * use of a custom sampler type is mutually exclusive with the use of the aforementioned enums.
 *
 * The parameters that needs to be set for those custom samplers are not added as yet another
 * option inside the GPUSamplerState structure because every possible configuration of sampler
 * states are generated, setup, and cached at startup, so adding yet another axis of variation will
 * multiply the number of configurations that needs to be cached, which is not worth it due to the
 * limited use of the parameters needed to setup those custom samplers.
 */
enum GPUSamplerCustomType {
  /**
   * Enable compare mode for depth texture. The depth texture must then be bound to a shadow
   * sampler. This is equivalent to:
   *
   * - GPU_SAMPLER_FILTERING_LINEAR.
   * - GPU_SAMPLER_EXTEND_MODE_EXTEND.
   *
   * And sets:
   *
   * - TEXTURE_COMPARE_MODE -> COMPARE_REF_TO_TEXTURE.
   * - TEXTURE_COMPARE_FUNC -> LEQUAL.
   */
  GPU_SAMPLER_CUSTOM_COMPARE = 0,
  /**
   * Special icon sampler with custom LOD bias and interpolation mode. This sets:
   *
   * - TEXTURE_MAG_FILTER -> LINEAR.
   * - TEXTURE_MIN_FILTER -> LINEAR_MIPMAP_NEAREST.
   * - TEXTURE_LOD_BIAS   -> -0.5.
   */
  GPU_SAMPLER_CUSTOM_ICON,
};

#define GPU_SAMPLER_CUSTOM_TYPES_COUNT (GPU_SAMPLER_CUSTOM_ICON + 1)

/**
 * The `GPUSamplerStateType` specifies how the GPUSamplerState structure should be interpreted
 * when passed around due to it being an overloaded type, see the documentation of each of the
 * types for more information.
 */
enum GPUSamplerStateType {
  /**
   * The filtering, extend_x, and extend_yz members of the GPUSamplerState structure will be used
   * in setting up the sampler state for the texture. The custom_type member will be ignored in
   * that case.
   */
  GPU_SAMPLER_STATE_TYPE_PARAMETERS = 0,
  /**
   * The filtering, extend_x, and extend_yz members of the GPUSamplerState structure will be
   * ignored, and the predefined custom parameters outlined in the documentation of
   * GPUSamplerCustomType will be used in setting up the sampler state for the texture.
   */
  GPU_SAMPLER_STATE_TYPE_CUSTOM,
  /**
   * The members of the GPUSamplerState structure will be ignored and the internal sampler state of
   * the texture will be used. In other words, this is a signal value and stores no useful or
   * actual data.
   */
  GPU_SAMPLER_STATE_TYPE_INTERNAL,
};

/**
 * The `GPUSamplerState` specifies the sampler state to bind a texture with.
 *
 * When the state type is set to GPU_SAMPLER_STATE_TYPE_CUSTOM or GPU_SAMPLER_STATE_TYPE_INTERNAL,
 * the rest of the members of the structure will be ignored. However, we can't turn this structure
 * into a union, because various functions merely temporally change the state type and expect the
 * rest of the members' values to be retained when the state type is changed back to
 * GPU_SAMPLER_STATE_TYPE_PARAMETERS. For the instance, a function might do the following and
 * expect the original sampler state of the texture to be retained after disabling comparison mode:
 *
 * GPU_texture_compare_mode(texture, true);
 * // Use the texture ...
 * GPU_texture_compare_mode(texture, false);
 */
struct GPUSamplerState {
  /** Specifies the enabled filtering options for the sampler. */
  GPUSamplerFiltering filtering : 8;
  /**
   * Specifies how the texture will be extrapolated for out-of-bound texture sampling along the x
   * axis.
   */
  GPUSamplerExtendMode extend_x : 4;
  /**
   * Specifies how the texture will be extrapolated for out-of-bound texture sampling along both
   * the y and z axis. There is no individual control for the z axis because 3D textures have
   * limited use, and when used, their extend mode is typically the same for all axis.
   */
  GPUSamplerExtendMode extend_yz : 4;
  /** Specifies the type of sampler if the state type is GPU_SAMPLER_STATE_TYPE_CUSTOM. */
  GPUSamplerCustomType custom_type : 8;
  /** Specifies how the GPUSamplerState structure should be interpreted when passed around. */
  GPUSamplerStateType type : 8;

  /**
   * Constructs a sampler state with default filtering and extended extend in both x and y axis.
   * See the documentation on GPU_SAMPLER_FILTERING_DEFAULT and GPU_SAMPLER_EXTEND_MODE_EXTEND for
   * more information.
   *
   * GPU_SAMPLER_STATE_TYPE_PARAMETERS is set in order to utilize the aforementioned parameters, so
   * GPU_SAMPLER_CUSTOM_COMPARE is arbitrary, ignored, and irrelevant.
   */
  static constexpr GPUSamplerState default_sampler()
  {
    return {GPU_SAMPLER_FILTERING_DEFAULT,
            GPU_SAMPLER_EXTEND_MODE_EXTEND,
            GPU_SAMPLER_EXTEND_MODE_EXTEND,
            GPU_SAMPLER_CUSTOM_COMPARE,
            GPU_SAMPLER_STATE_TYPE_PARAMETERS};
  }

  /**
   * Constructs a sampler state that can be used to signal that the internal sampler of the texture
   * should be used instead. See the documentation on GPU_SAMPLER_STATE_TYPE_INTERNAL for more
   * information.
   *
   * GPU_SAMPLER_STATE_TYPE_INTERNAL is set in order to signal the use of the internal sampler of
   * the texture, so the rest of the options before it are arbitrary, ignored, and irrelevant.
   */
  static constexpr GPUSamplerState internal_sampler()
  {
    return {GPU_SAMPLER_FILTERING_DEFAULT,
            GPU_SAMPLER_EXTEND_MODE_EXTEND,
            GPU_SAMPLER_EXTEND_MODE_EXTEND,
            GPU_SAMPLER_CUSTOM_COMPARE,
            GPU_SAMPLER_STATE_TYPE_INTERNAL};
  }

  /**
   * Constructs a special sampler state that can be used sampler icons. See the documentation on
   * GPU_SAMPLER_CUSTOM_ICON for more information.
   *
   * GPU_SAMPLER_STATE_TYPE_CUSTOM is set in order to specify a custom sampler type, so the rest of
   * the options before it are arbitrary, ignored, and irrelevant.
   */
  static constexpr GPUSamplerState icon_sampler()
  {
    return {GPU_SAMPLER_FILTERING_DEFAULT,
            GPU_SAMPLER_EXTEND_MODE_EXTEND,
            GPU_SAMPLER_EXTEND_MODE_EXTEND,
            GPU_SAMPLER_CUSTOM_ICON,
            GPU_SAMPLER_STATE_TYPE_CUSTOM};
  }

  /**
   * Constructs a special sampler state for depth comparison. See the documentation on
   * GPU_SAMPLER_CUSTOM_COMPARE for more information.
   *
   * GPU_SAMPLER_STATE_TYPE_CUSTOM is set in order to specify a custom sampler type, so the rest of
   * the options before it are ignored and irrelevant, but they are set to sensible defaults in
   * case comparison mode is turned off, in which case, the sampler state will become equivalent to
   * GPUSamplerState::default_sampler().
   */
  static constexpr GPUSamplerState compare_sampler()
  {
    return {GPU_SAMPLER_FILTERING_DEFAULT,
            GPU_SAMPLER_EXTEND_MODE_EXTEND,
            GPU_SAMPLER_EXTEND_MODE_EXTEND,
            GPU_SAMPLER_CUSTOM_COMPARE,
            GPU_SAMPLER_STATE_TYPE_CUSTOM};
  }

  /**
   * Enables the given filtering flags.
   */
  void enable_filtering_flag(GPUSamplerFiltering filtering_flags)
  {
    this->filtering = this->filtering | filtering_flags;
  }

  /**
   * Disables the given filtering flags.
   */
  void disable_filtering_flag(GPUSamplerFiltering filtering_flags)
  {
    this->filtering = this->filtering & ~filtering_flags;
  }

  /**
   * Enables the given filtering flags if the given test is true, otherwise, disables the given
   * filtering flags.
   */
  void set_filtering_flag_from_test(GPUSamplerFiltering filtering_flags, bool test)
  {
    if (test) {
      this->enable_filtering_flag(filtering_flags);
    }
    else {
      this->disable_filtering_flag(filtering_flags);
    }
  }

  std::string to_string() const
  {
    if (this->type == GPU_SAMPLER_STATE_TYPE_INTERNAL) {
      return "internal";
    }

    if (this->type == GPU_SAMPLER_STATE_TYPE_CUSTOM) {
      switch (this->custom_type) {
        case GPU_SAMPLER_CUSTOM_COMPARE:
          return "compare";
          break;
        case GPU_SAMPLER_CUSTOM_ICON:
          return "icon";
          break;
        default:
          BLI_assert_unreachable();
          return "";
      }
    }

    /* The sampler state is of type PARAMETERS, so serialize the parameters. */
    BLI_assert(this->type == GPU_SAMPLER_STATE_TYPE_PARAMETERS);
    std::string serialized_parameters;

    if (this->filtering & GPU_SAMPLER_FILTERING_LINEAR) {
      serialized_parameters += "linear-filter_";
    }

    if (this->filtering & GPU_SAMPLER_FILTERING_MIPMAP) {
      serialized_parameters += "mipmap_";
    }

    if (this->filtering & GPU_SAMPLER_FILTERING_ANISOTROPIC) {
      serialized_parameters += "anisotropic_";
    }

    switch (this->extend_x) {
      case GPU_SAMPLER_EXTEND_MODE_EXTEND:
        serialized_parameters += "extend-x_";
        break;
      case GPU_SAMPLER_EXTEND_MODE_REPEAT:
        serialized_parameters += "repeat-x_";
        break;
      case GPU_SAMPLER_EXTEND_MODE_MIRRORED_REPEAT:
        serialized_parameters += "mirrored-repeat-x_";
        break;
      case GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER:
        serialized_parameters += "clamp-to-border-x_";
        break;
      default:
        BLI_assert_unreachable();
    }

    switch (this->extend_yz) {
      case GPU_SAMPLER_EXTEND_MODE_EXTEND:
        serialized_parameters += "extend-y_";
        break;
      case GPU_SAMPLER_EXTEND_MODE_REPEAT:
        serialized_parameters += "repeat-y_";
        break;
      case GPU_SAMPLER_EXTEND_MODE_MIRRORED_REPEAT:
        serialized_parameters += "mirrored-repeat-y_";
        break;
      case GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER:
        serialized_parameters += "clamp-to-border-y_";
        break;
      default:
        BLI_assert_unreachable();
    }

    switch (this->extend_yz) {
      case GPU_SAMPLER_EXTEND_MODE_EXTEND:
        serialized_parameters += "extend-z";
        break;
      case GPU_SAMPLER_EXTEND_MODE_REPEAT:
        serialized_parameters += "repeat-z";
        break;
      case GPU_SAMPLER_EXTEND_MODE_MIRRORED_REPEAT:
        serialized_parameters += "mirrored-repeat-z";
        break;
      case GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER:
        serialized_parameters += "clamp-to-border-z";
        break;
      default:
        BLI_assert_unreachable();
    }

    return serialized_parameters;
  }

  uint32_t as_uint() const
  {
    uint32_t value = filtering;
    value = (value << 4) | extend_x;
    value = (value << 4) | extend_yz;
    value = (value << 8) | custom_type;
    value = (value << 8) | type;
    return value;
  }

  bool operator==(GPUSamplerState const &rhs) const
  {
    return this->filtering == rhs.filtering && this->extend_x == rhs.extend_x &&
           this->extend_yz == rhs.extend_yz && this->custom_type == rhs.custom_type &&
           this->type == rhs.type;
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Enums
 * \{ */

/**
 * Types of data for data specification.
 * Used for formatting upload and download of data.
 * When used with textures, they need to match or be compatible with the
 * `blender::gpu::TextureFormat` used. Check `validate_data_format` for compatibility list.
 */
/* TODO(fclem): Replace by gpu::DataFormat. */
enum eGPUDataFormat {
  GPU_DATA_FLOAT,
  GPU_DATA_HALF_FLOAT,
  GPU_DATA_INT,
  GPU_DATA_UINT,
  GPU_DATA_UBYTE,
  /** Special type used for depth-stencil textures. */
  /* GPU_DATA_UINT_24_8_DEPRECATED is deprecated since Blender 5.0. It is still here as python
   * add-ons can still use it. */
  GPU_DATA_UINT_24_8_DEPRECATED,
  /** Special type used for packed 32bit per pixel textures. Data is stored in reverse order. */
  GPU_DATA_10_11_11_REV,
  GPU_DATA_2_10_10_10_REV,
};

/**
 * Texture usage flags allow backend implementations to contextually optimize texture resources.
 * Any texture with an explicit flag should not perform operations which are not explicitly
 * specified in the usage flags. If usage is unknown upfront, then GPU_TEXTURE_USAGE_GENERAL can be
 * used.
 *
 * NOTE: These usage flags act as hints for the backend implementations. There may be no benefit in
 * some circumstances, and certain resource types may insert additional usage as required. However,
 * explicit usage can ensure that hardware features such as render target/texture compression can
 * be used. For explicit APIs such as Metal/Vulkan, texture usage needs to be specified up-front.
 */
enum eGPUTextureUsage {
  /* Whether texture is sampled or read during a shader. */
  GPU_TEXTURE_USAGE_SHADER_READ = (1 << 0),
  /* Whether the texture is written to by a shader using imageStore. */
  GPU_TEXTURE_USAGE_SHADER_WRITE = (1 << 1),
  /* Whether a texture is used as an attachment in a frame-buffer. */
  GPU_TEXTURE_USAGE_ATTACHMENT = (1 << 2),
  /* Whether a texture is used to create a texture view utilizing a different texture format to the
   * source textures format. This includes the use of stencil views. */
  GPU_TEXTURE_USAGE_FORMAT_VIEW = (1 << 3),
  /* Whether the texture needs to be read from by the CPU. */
  GPU_TEXTURE_USAGE_HOST_READ = (1 << 4),
  /* When used, the texture will not have any backing storage and can solely exist as a virtual
   * frame-buffer attachment. */
  GPU_TEXTURE_USAGE_MEMORYLESS = (1 << 5),
  /* Whether a texture can support atomic operations. */
  GPU_TEXTURE_USAGE_ATOMIC = (1 << 6),
  /* Whether a texture can be exported to other instances/processes. */
  GPU_TEXTURE_USAGE_MEMORY_EXPORT = (1 << 7),
  /* Create a texture whose usage cannot be defined prematurely.
   * This is unoptimized and should not be used. */
  GPU_TEXTURE_USAGE_GENERAL = (0xFF & (~(GPU_TEXTURE_USAGE_MEMORYLESS | GPU_TEXTURE_USAGE_ATOMIC |
                                         GPU_TEXTURE_USAGE_MEMORY_EXPORT))),
};

ENUM_OPERATORS(eGPUTextureUsage);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Creation
 * \{ */

namespace blender::gpu {
class Texture;
}  // namespace blender::gpu

/**
 * \note \a data is expected to be float. If the \a format is not compatible with float data or if
 * the data is not in float format, use GPU_texture_update to upload the data with the right data
 * format.
 *
 * Textures created via other means will either inherit usage from the source resource, or also
 * be initialized with `GPU_TEXTURE_USAGE_GENERAL`.
 *
 * flag. \a mips is the number of mip level to allocate. It must be >= 1.
 */
blender::gpu::Texture *GPU_texture_create_1d(const char *name,
                                             int width,
                                             int mip_len,
                                             blender::gpu::TextureFormat format,
                                             eGPUTextureUsage usage,
                                             const float *data);
blender::gpu::Texture *GPU_texture_create_1d_array(const char *name,
                                                   int width,
                                                   int layer_len,
                                                   int mip_len,
                                                   blender::gpu::TextureFormat format,
                                                   eGPUTextureUsage usage,
                                                   const float *data);
blender::gpu::Texture *GPU_texture_create_2d(const char *name,
                                             int width,
                                             int height,
                                             int mip_len,
                                             blender::gpu::TextureFormat format,
                                             eGPUTextureUsage usage,
                                             const float *data);
blender::gpu::Texture *GPU_texture_create_2d_array(const char *name,
                                                   int width,
                                                   int height,
                                                   int layer_len,
                                                   int mip_len,
                                                   blender::gpu::TextureFormat format,
                                                   eGPUTextureUsage usage,
                                                   const float *data);
blender::gpu::Texture *GPU_texture_create_3d(const char *name,
                                             int width,
                                             int height,
                                             int depth,
                                             int mip_len,
                                             blender::gpu::TextureFormat format,
                                             eGPUTextureUsage usage,
                                             const void *data);
blender::gpu::Texture *GPU_texture_create_cube(const char *name,
                                               int width,
                                               int mip_len,
                                               blender::gpu::TextureFormat format,
                                               eGPUTextureUsage usage,
                                               const float *data);
blender::gpu::Texture *GPU_texture_create_cube_array(const char *name,
                                                     int width,
                                                     int layer_len,
                                                     int mip_len,
                                                     blender::gpu::TextureFormat format,
                                                     eGPUTextureUsage usage,
                                                     const float *data);
/**
 * DDS texture loading. Return nullptr if compressed texture support is not available.
 * \a data should hold all the data for \a mip_len mipmaps.
 * The data is expected to be in compressed form. This isn't going to compress un-compress data.
 */
blender::gpu::Texture *GPU_texture_create_compressed_2d(const char *name,
                                                        int width,
                                                        int height,
                                                        int mip_len,
                                                        blender::gpu::TextureFormat format,
                                                        eGPUTextureUsage usage,
                                                        const void *data);

/**
 * Create a buffer texture that allow access to a buffer \a vertex_buf through a sampler of type
 * `(FLOAT/INT/UINT)_BUFFER`.
 */
blender::gpu::Texture *GPU_texture_create_from_vertbuf(const char *name,
                                                       blender::gpu::VertBuf *vertex_buf);

/**
 * Create an error texture that will bind an pink texture at draw time.
 * \a dimension is the number of number of dimension of the texture (1, 2, or 3).
 * \a array if set to true, will make the texture be an array (layered).
 */
blender::gpu::Texture *GPU_texture_create_error(int dimension, bool array);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Freeing
 * \{ */

/**
 * Add a reference to this texture for usage.
 * This internally increment the reference counter.
 * This avoids the texture being free between the time it is referenced by the drawing logic and
 * the time it is actually dereferenced.
 */
void GPU_texture_ref(blender::gpu::Texture *texture);

/**
 * This internally decrement the reference counter.
 * If the reference counter is 1 when calling this function the #blender::gpu::Texture will be
 * freed.
 */
void GPU_texture_free(blender::gpu::Texture *texture);

#define GPU_TEXTURE_FREE_SAFE(texture) \
  do { \
    if (texture != nullptr) { \
      GPU_texture_free(texture); \
      texture = nullptr; \
    } \
  } while (0)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture Views
 * \{ */

/**
 * Create an alias of the source texture data. A view can cover the whole texture or only a range
 * of mip levels and/or array layer range.
 *
 * \a view_format is the format in which the view will interpret the data of \a source_texture.
 * It must match the format of \a source_texture in size (ex: RGBA8 can be reinterpreted as R32UI).
 * See https://www.khronos.org/opengl/wiki/Texture_Storage#View_texture_aliases for an exhaustive
 * list.
 *
 * \note If \a source_texture is freed, the texture view will continue to be valid.
 * \note If \a mip_start or \a mip_len is bigger than available mips they will be clamped to the
 * source texture available range.
 * \note If \a cube_as_array is true, then the created view will be a 2D array texture instead of a
 * cube-map texture or cube-map-array texture.
 *
 * For Depth-Stencil texture view formats:
 * \note If \a use_stencil is true, the texture is expected to be bound to a UINT sampler and will
 * return the stencil value (in a range of [0..255]) as the first component.
 * \note If \a use_stencil is false (default), the texture is expected to be bound to a DEPTH
 * sampler and will return the normalized depth value (in a range of [0..1])  as the first
 * component.
 *
 * TODO(fclem): Target conversion (ex: Texture 2D as Texture 2D Array) is not implemented yet.
 */
blender::gpu::Texture *GPU_texture_create_view(const char *name,
                                               blender::gpu::Texture *source_texture,
                                               blender::gpu::TextureFormat view_format,
                                               int mip_start,
                                               int mip_len,
                                               int layer_start,
                                               int layer_len,
                                               bool cube_as_array,
                                               bool use_stencil);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modify & Update
 * \{ */

/**
 * Makes data interpretation aware of the source layout.
 * Skipping pixels correctly when changing rows when doing partial update.
 * This affects `GPU_texture_update`, `GPU_texture_update_sub`, `GPU_texture_update_mipmap`.
 * TODO(fclem): replace this by pixel buffer updates using a custom utility to do the line shifting
 * like Cycles does.
 */
void GPU_unpack_row_length_set(uint len);

/**
 * Update the content of a texture's base mip-map level (mip 0).
 * \a data_format is the format of the \a data . It needs to be compatible with the internal
 * texture storage.
 * The \a data should be the size of the entire mip 0 level.
 * \note This function only update the content of mip 0. Either specify other mips or use
 * `GPU_texture_update_mipmap_chain` to generate them if needed.
 */
void GPU_texture_update(blender::gpu::Texture *texture,
                        eGPUDataFormat data_format,
                        const void *data);

/**
 * Update the content of a region of a texture's base mip-map level (mip 0).
 * \a data_format is the format of the \a data . It needs to be compatible with the internal
 * texture storage.
 * The \a data should be the size of the mip 0 level region.
 * \note This function only update the content of mip 0. Either specify other mips or use
 * `GPU_texture_update_mipmap_chain` to generate them if needed.
 *
 * \a offset_x , \a offset_y , \a offset_z specify the bottom left corner of the updated region.
 * \a width , \a height , \a depth specify the extent of the updated region.
 */
void GPU_texture_update_sub(blender::gpu::Texture *texture,
                            eGPUDataFormat data_format,
                            const void *pixels,
                            int offset_x,
                            int offset_y,
                            int offset_z,
                            int width,
                            int height,
                            int depth);

/**
 * Update the content of a texture's specific mip-map level.
 * \a data_format is the format of the \a pixels . It needs to be compatible with the internal
 * texture storage.
 * The \a data should be the size of the entire \a mip_level.
 */
void GPU_texture_update_mipmap(blender::gpu::Texture *texture,
                               int mip_level,
                               eGPUDataFormat data_format,
                               const void *pixels);

/**
 * Fills the whole texture with the same data for all pixels.
 * \warning Only works for 2D and 3D textures.
 * \warning Only clears the MIP 0 of the texture.
 * \param data_format: data format of the pixel data.
 * \note The format is float for UNORM textures.
 * \param data: 1 pixel worth of data to fill the texture with.
 */
void GPU_texture_clear(blender::gpu::Texture *texture,
                       eGPUDataFormat data_format,
                       const void *data);

/**
 * Copy a \a src texture content to a similar \a dst texture. Only MIP 0 is copied.
 * Textures needs to match in size and format.
 */
void GPU_texture_copy(blender::gpu::Texture *dst, blender::gpu::Texture *src);

/**
 * Update the mip-map levels using the mip 0 data.
 * \note this doesn't work on depth or compressed textures.
 */
void GPU_texture_update_mipmap_chain(blender::gpu::Texture *texture);

/**
 * Read the content of a \a mip_level from a \a tex and returns a copy of its data.
 * \warning the texture must have been created using GPU_TEXTURE_USAGE_HOST_READ.
 * \note synchronization of shader writes via `imageStore()` needs to be explicitly done using
 * `GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH)`.
 */
void *GPU_texture_read(blender::gpu::Texture *texture, eGPUDataFormat data_format, int mip_level);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Binding
 * \{ */

/**
 * Bind a texture to a texture sampling image units using the texture internal sampler state.
 */
void GPU_texture_bind(blender::gpu::Texture *texture, int unit);
/**
 * Bind a texture to a texture sampling image units using the explicit sampler state.
 */
void GPU_texture_bind_ex(blender::gpu::Texture *texture, GPUSamplerState state, int unit);
/**
 * Unbind \a tex from a texture sampling image unit.
 * \note this isn't strictly required but it is better for debugging purpose.
 */
void GPU_texture_unbind(blender::gpu::Texture *texture);
/**
 * Unbind all texture from all texture sampling image units.
 */
void GPU_texture_unbind_all();

/**
 * Bind \a tex to an arbitrary load/store image unit.
 * It correspond to a `gpu::shader::ShaderCreateInfo::image()` declaration.
 * \note this overrides any previous bind on the same unit.
 */
void GPU_texture_image_bind(blender::gpu::Texture *texture, int unit);
/**
 * Unbind \a tex from an arbitrary load/store image unit.
 * \note this isn't strictly required but it is better for debugging purpose.
 */
void GPU_texture_image_unbind(blender::gpu::Texture *texture);
/**
 * Unbind all texture from all arbitrary load/store image units.
 */
void GPU_texture_image_unbind_all();

/** \} */

/* -------------------------------------------------------------------- */
/** \name State API
 * \{ */

/**
 * Set \a tex texture depth comparison mode. Only works on depth format.
 */
void GPU_texture_compare_mode(blender::gpu::Texture *texture, bool use_compare);

/**
 * Set \a tex texture filter usage.
 * If \a use_filter is true, the texture will use linear interpolation between neighboring texels.
 * \note Does not work on non-normalized integer textures.
 * \note Does not modify the mip-map usage state.
 */
void GPU_texture_filter_mode(blender::gpu::Texture *texture, bool use_filter);

/**
 * Set \a tex texture filter and mip-map usage.
 * If \a use_filter is true, the texture will use linear interpolation between neighboring texels.
 * If \a use_mipmap is true, the texture will use mip-mapping as anti-aliasing method.
 * If both are set to true, the texture will use linear interpolation between mip-map levels.
 * \note Does not work on non-normalized integer textures.
 */
void GPU_texture_mipmap_mode(blender::gpu::Texture *texture, bool use_mipmap, bool use_filter);

/**
 * Set anisotropic filter usage. Filter sample count is determined globally by
 * `U.anisotropic_filter` and updated when `GPU_samplers_update` is called.
 */
void GPU_texture_anisotropic_filter(blender::gpu::Texture *texture, bool use_aniso);

/**
 * Set \a tex texture sampling method for coordinates outside of the [0..1] uv range along the x
 * axis. See GPUSamplerExtendMode for the available and meaning of different extend modes.
 */
void GPU_texture_extend_mode_x(blender::gpu::Texture *texture, GPUSamplerExtendMode extend_mode);

/**
 * Set \a tex texture sampling method for coordinates outside of the [0..1] uv range along the y
 * axis. See GPUSamplerExtendMode for the available and meaning of different extend modes.
 */
void GPU_texture_extend_mode_y(blender::gpu::Texture *texture, GPUSamplerExtendMode extend_mode);

/**
 * Set \a tex texture sampling method for coordinates outside of the [0..1] uv range along both the
 * x and y axis. See GPUSamplerExtendMode for the available and meaning of different extend modes.
 */
void GPU_texture_extend_mode(blender::gpu::Texture *texture, GPUSamplerExtendMode extend_mode);

/**
 * Set \a tex texture swizzle state for swizzling sample components.
 *
 * A texture sample always return 4 components in the shader. If the texture has less than 4
 * components, the missing ones are replaced by the matching values in the following vector
 * (0, 0, 0, 1).
 *
 * \a swizzle contains 1 char per component representing the source of the data for each of the
 * component of a sample value. The possible values for each of these 4 characters are:
 * - 'r' or 'x': use the texture first component.
 * - 'g' or 'y': use the texture second component.
 * - 'b' or 'z': use the texture third component.
 * - 'a' or 'w': use the texture fourth component.
 * - '0': will make the component value to always return 0.
 * - '1': will make the component value to always return 1.
 */
void GPU_texture_swizzle_set(blender::gpu::Texture *texture, const char swizzle[4]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Introspection API
 * \{ */

/**
 * Return the number of dimensions of the texture ignoring dimension of layers (1, 2 or 3).
 * Cube textures are considered 2D.
 */
int GPU_texture_dimensions(const blender::gpu::Texture *texture);

/**
 * Return the width of \a tex.
 */
int GPU_texture_width(const blender::gpu::Texture *texture);

/**
 * Return the height of \a tex. Correspond to number of layers for 1D array texture.
 */
int GPU_texture_height(const blender::gpu::Texture *texture);

/**
 * Return the depth of \a tex . Correspond to number of layers for 2D array texture.
 * NOTE: return 0 for 1D & 2D textures.
 */
int GPU_texture_depth(const blender::gpu::Texture *texture);

/**
 * Return the number of layers of \a tex. Return 1 if the texture is not layered.
 */
int GPU_texture_layer_count(const blender::gpu::Texture *texture);

/**
 * Return the number of mip-map level inside this texture.
 */
int GPU_texture_mip_count(const blender::gpu::Texture *texture);

/**
 * Return the texture format of \a tex.
 */
blender::gpu::TextureFormat GPU_texture_format(const blender::gpu::Texture *texture);

/**
 * Return the usage flags of \a tex.
 */
eGPUTextureUsage GPU_texture_usage(const blender::gpu::Texture *texture);

/**
 * Return true if the texture is an array texture type (has layers).
 */
bool GPU_texture_is_array(const blender::gpu::Texture *texture);

/**
 * Return true if the texture is an cube-map texture type.
 */
bool GPU_texture_is_cube(const blender::gpu::Texture *texture);

/**
 * Return true if the texture format has a depth component.
 */
bool GPU_texture_has_depth_format(const blender::gpu::Texture *texture);

/**
 * Return true if the texture format has a stencil component.
 */
bool GPU_texture_has_stencil_format(const blender::gpu::Texture *texture);

/**
 * Return true if the texture format is an integer type (non-normalized integers).
 */
bool GPU_texture_has_integer_format(const blender::gpu::Texture *texture);

/**
 * Return true if the texture format is a float type.
 */
bool GPU_texture_has_float_format(const blender::gpu::Texture *texture);

/**
 * Return true if the texture format is an integer normalized type.
 */
bool GPU_texture_has_normalized_format(const blender::gpu::Texture *texture);

/**
 * Return true if the texture format is a signed type.
 */
bool GPU_texture_has_signed_format(const blender::gpu::Texture *texture);

/**
 * Returns the pixel dimensions of a texture's mip-map level.
 * \a size is expected to be a pointer to a vector of dimension matching the texture's dimension
 * (including the array dimension).
 */
void GPU_texture_get_mipmap_size(blender::gpu::Texture *texture, int mip_level, int *r_size);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Python API & meta-data
 *
 * These are not intrinsic properties of a texture but they are stored inside the gpu::Texture
 * structure for tracking purpose.
 * \{ */

/**
 * Width & Height (of source data), optional.
 * WORKAROUND: Calling #BKE_image_get_size may free the texture. Store the source image size
 * (before down-scaling) inside the #blender::gpu::Texture to retrieve the original size later (Ref
 * #59347).
 */
int GPU_texture_original_width(const blender::gpu::Texture *texture);
int GPU_texture_original_height(const blender::gpu::Texture *texture);
void GPU_texture_original_size_set(blender::gpu::Texture *texture, int width, int height);

/**
 * Reference of a pointer that needs to be cleaned when deallocating the texture.
 * Points to #BPyGPUTexture.tex
 */
#ifndef GPU_NO_USE_PY_REFERENCES
void **GPU_texture_py_reference_get(blender::gpu::Texture *texture);
void GPU_texture_py_reference_set(blender::gpu::Texture *texture, void **py_ref);
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

/**
 * Returns the number of components in a texture format.
 */
size_t GPU_texture_component_len(blender::gpu::TextureFormat format);

/**
 * Return the expected number of bytes for one pixel of \a data_format data.
 */
size_t GPU_texture_dataformat_size(eGPUDataFormat data_format);

/**
 * Return the texture format as a string for display purpose.
 * Example: `blender::gpu::TextureFormat::UNORM_8_8_8_8` returns as `"RGBA8"`.
 */
const char *GPU_texture_format_name(blender::gpu::TextureFormat format);

/**
 * Returns the memory usage of all currently allocated textures in bytes.
 * \note that does not mean all of the textures are inside VRAM. Drivers can swap the texture
 * memory back and forth depending on usage.
 */
unsigned int GPU_texture_memory_usage_get();

/**
 * Update sampler states depending on user settings.
 */
void GPU_samplers_update();

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pixel Buffer
 *
 * Used for interfacing with other graphic APIs using graphic interoperability.
 * It can also be used for more efficient partial update from CPU side.
 * \{ */

/** Opaque type hiding blender::gpu::PixelBuffer. */
struct GPUPixelBuffer;

/**
 * Creates a #GPUPixelBuffer object with \a byte_size worth of storage.
 */
GPUPixelBuffer *GPU_pixel_buffer_create(size_t byte_size);

/**
 * Free a #GPUPixelBuffer object.
 * The object should be unmapped before being freed.
 */
void GPU_pixel_buffer_free(GPUPixelBuffer *pixel_buf);

/**
 * Maps a pixel buffer to RAM, giving back access rights to CPU.
 * The returned pointer is only valid until `GPU_pixel_buffer_unmap` is called.
 * A #GPUPixelBuffer needs to be unmapped before being used for GPU side operation (like texture
 * update through `GPU_texture_update_sub_from_pixel_buffer`).
 */
void *GPU_pixel_buffer_map(GPUPixelBuffer *pixel_buf);

/**
 * Unmap a pixel buffer from RAM, giving back access rights to GPU.
 * Any pointer previously acquired by `GPU_pixel_buffer_map` becomes invalid.
 */
void GPU_pixel_buffer_unmap(GPUPixelBuffer *pixel_buf);

/**
 * Return size in bytes of the \a pix_buf.
 */
size_t GPU_pixel_buffer_size(GPUPixelBuffer *pixel_buf);

/**
 * Return the native handle of the \a pix_buf to use for graphic interoperability registration.
 *
 * - OpenGL: pixel buffer object ID.
 * - Vulkan on Windows: opaque handle for VkBuffer.
 * - Vulkan on Unix: opaque file descriptor for VkBuffer.
 * - Metal: MTLBuffer with unified memory.
 *
 * For Vulkan, the caller is responsible for closing the handle.
 */
struct GPUPixelBufferNativeHandle {
  int64_t handle = 0;
  size_t size = 0;
};

GPUPixelBufferNativeHandle GPU_pixel_buffer_get_native_handle(GPUPixelBuffer *pixel_buf);

/**
 * Update a sub-region of a texture using the data from a #GPUPixelBuffer as source data.
 * The \a pix_buf data is expected to be contiguous and big enough to fill the described
 * sub-region.
 */
void GPU_texture_update_sub_from_pixel_buffer(blender::gpu::Texture *texture,
                                              eGPUDataFormat data_format,
                                              GPUPixelBuffer *pixel_buf,
                                              int offset_x,
                                              int offset_y,
                                              int offset_z,
                                              int width,
                                              int height,
                                              int depth);
/** \} */
