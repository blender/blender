/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_string_ref.hh"

/**
 * Describes the load operation of a frame-buffer attachment at the start of a render pass.
 */
enum eGPULoadOp {
  /**
   * Clear the frame-buffer attachment using the clear value.
   */
  GPU_LOADACTION_CLEAR = 0,
  /**
   * Load the value from the attached texture.
   * Cannot be used with memoryless attachments.
   * Slower than `GPU_LOADACTION_CLEAR` or `GPU_LOADACTION_DONT_CARE`.
   */
  GPU_LOADACTION_LOAD,
  /**
   * Do not care about the content of the attachment when the render pass starts.
   * Useful if only the values being written are important.
   * Faster than `GPU_LOADACTION_CLEAR`.
   */
  GPU_LOADACTION_DONT_CARE,
};

/**
 * Describes the store operation of a frame-buffer attachment at the end of a render pass.
 */
enum eGPUStoreOp {
  /**
   * Do not care about the content of the attachment when the render pass ends.
   * Useful if only the values being written are important.
   * Cannot be used with memoryless attachments.
   */
  GPU_STOREACTION_STORE = 0,
  /**
   * The result of the rendering for this attachment will be discarded.
   * No writes to the texture memory will be done which makes it faster than
   * `GPU_STOREACTION_STORE`.
   * IMPORTANT: The actual values of the attachment is to be considered undefined.
   * Only to be used on transient attachment that are only used within the boundaries of
   * a render pass (ex.: Unneeded depth buffer result).
   */
  GPU_STOREACTION_DONT_CARE,
};

/**
 * Describes the state of a frame-buffer attachment during a sub-pass.
 *
 * NOTE: Until this is correctly implemented in all backend, reading and writing from the
 * same attachment will not work. Although there is no case where it would currently be useful.
 */
enum GPUAttachmentState {
  /** Attachment will not be written during rendering. */
  GPU_ATTACHMENT_IGNORE = 0,
  /** Attachment will be written during render sub-pass. This also works with blending. */
  GPU_ATTACHMENT_WRITE,
  /** Attachment is used as input in the fragment shader. Incompatible with depth on Metal. */
  GPU_ATTACHMENT_READ,
};

enum eGPUFrontFace {
  GPU_CLOCKWISE,
  GPU_COUNTERCLOCKWISE,
};

namespace blender::gpu::shader {

enum class Type {
  /* Types supported natively across all GPU back-ends. */
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
  /* Additionally supported types to enable data optimization and native
   * support in some GPU back-ends.
   * NOTE: These types must be representable in all APIs. E.g. `VEC3_101010I2` is aliased as vec3
   * in the GL back-end, as implicit type conversions from packed normal attribute data to vec3 is
   * supported. UCHAR/CHAR types are natively supported in Metal and can be used to avoid
   * additional data conversions for `GPU_COMP_U8` vertex attributes. */
  VEC3_101010I2,
  UCHAR,
  UCHAR2,
  UCHAR3,
  UCHAR4,
  CHAR,
  CHAR2,
  CHAR3,
  CHAR4,
  USHORT,
  USHORT2,
  USHORT3,
  USHORT4,
  SHORT,
  SHORT2,
  SHORT3,
  SHORT4
};

BLI_INLINE int to_component_count(const Type &type)
{
  switch (type) {
    case Type::FLOAT:
    case Type::UINT:
    case Type::INT:
    case Type::BOOL:
      return 1;
    case Type::VEC2:
    case Type::UVEC2:
    case Type::IVEC2:
      return 2;
    case Type::VEC3:
    case Type::UVEC3:
    case Type::IVEC3:
      return 3;
    case Type::VEC4:
    case Type::UVEC4:
    case Type::IVEC4:
      return 4;
    case Type::MAT3:
      return 9;
    case Type::MAT4:
      return 16;
    /* Alias special types. */
    case Type::UCHAR:
    case Type::USHORT:
      return 1;
    case Type::UCHAR2:
    case Type::USHORT2:
      return 2;
    case Type::UCHAR3:
    case Type::USHORT3:
      return 3;
    case Type::UCHAR4:
    case Type::USHORT4:
      return 4;
    case Type::CHAR:
    case Type::SHORT:
      return 1;
    case Type::CHAR2:
    case Type::SHORT2:
      return 2;
    case Type::CHAR3:
    case Type::SHORT3:
      return 3;
    case Type::CHAR4:
    case Type::SHORT4:
      return 4;
    case Type::VEC3_101010I2:
      return 3;
  }
  BLI_assert_unreachable();
  return -1;
}

struct SpecializationConstant {
  struct Value {
    union {
      uint32_t u;
      int32_t i;
      float f;
    };

    inline bool operator==(const Value &other) const
    {
      return u == other.u;
    }
  };

  Type type;
  StringRefNull name;
  Value value;

  SpecializationConstant() = default;

  SpecializationConstant(const char *name, uint32_t value) : type(Type::UINT), name(name)
  {
    this->value.u = value;
  }

  SpecializationConstant(const char *name, int value) : type(Type::INT), name(name)
  {
    this->value.i = value;
  }

  SpecializationConstant(const char *name, float value) : type(Type::FLOAT), name(name)
  {
    this->value.f = value;
  }

  SpecializationConstant(const char *name, bool value) : type(Type::BOOL), name(name)
  {
    this->value.u = value ? 1 : 0;
  }

  inline bool operator==(const SpecializationConstant &b) const
  {
    return this->type == b.type && this->name == b.name && this->value == b.value;
  }
};

}  // namespace blender::gpu::shader
