/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Push constants is a way to quickly provide a small amount of uniform data to shaders. It should
 * be much quicker than UBOs but a huge limitation is the size of data - spec requires 128 bytes to
 * be available for a push constant range. Hardware vendors may support more, but compared to other
 * means it is still very little (for example 256 bytes).
 *
 * Due to this size requirements we try to use push constants when it fits on the device. If it
 * doesn't fit we fall back to use an uniform buffer.
 *
 * Shader developers are responsible to fine-tune the performance of the shader. One way to do this
 * is to tailor what will be sent as a push constant to keep the push constants within the limits.
 */

#pragma once

#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "gpu_shader_create_info.hh"

#include "vk_common.hh"
#include "vk_descriptor_set.hh"

namespace blender::gpu {
class VKShaderInterface;
class VKUniformBuffer;
class VKContext;
class VKDevice;

/**
 * Container to store push constants in a buffer.
 *
 * Can handle buffers with different memory layouts (std140/std430)
 * Which memory layout is used is based on the storage type.
 *
 * VKPushConstantsLayout only describes the buffer, an instance of this
 * class can handle setting/modifying/duplicating push constants.
 *
 * It should also keep track of the submissions in order to reuse the allocated
 * data.
 */
class VKPushConstants {
  friend class VKContext;

 public:
  /** Different methods to store push constants. */
  enum class StorageType {
    /** Push constants aren't in use. */
    NONE,

    /** Store push constants as regular vulkan push constants. */
    PUSH_CONSTANTS,

    /**
     * Fallback when push constants doesn't meet the device requirements.
     */
    UNIFORM_BUFFER,
  };

  /**
   * Describe the layout of the push constants and the storage type that should be used.
   */
  struct Layout {
    static constexpr StorageType STORAGE_TYPE_DEFAULT = StorageType::PUSH_CONSTANTS;
    static constexpr StorageType STORAGE_TYPE_FALLBACK = StorageType::UNIFORM_BUFFER;

    struct PushConstant {
      /* Used as lookup based on ShaderInput. */
      int32_t location;

      /** Offset in the push constant data (in bytes). */
      uint32_t offset;
      shader::Type type;
      int array_size;
      uint inner_row_padding;
    };

   private:
    Vector<PushConstant> push_constants;
    uint32_t size_in_bytes_ = 0;
    StorageType storage_type_ = StorageType::NONE;
    /**
     * Binding index in the descriptor set when the push constants use an uniform buffer.
     */
    VKDescriptorSet::Location descriptor_set_location_;

   public:
    /**
     * Return the desired storage type that can fit the push constants of the given shader create
     * info, matching the limits of the given device.
     *
     * Returns:
     * - StorageType::NONE: No push constants are needed.
     * - StorageType::PUSH_CONSTANTS: Regular vulkan push constants can be used.
     * - StorageType::UNIFORM_BUFFER: The push constants don't fit in the limits of the given
     *   device. A uniform buffer should be used as a fallback method.
     */
    static StorageType determine_storage_type(const shader::ShaderCreateInfo &info,
                                              const VKDevice &device);

    /**
     * Initialize the push constants of the given shader create info with the
     * binding location.
     *
     * interface: Uniform locations of the interface are used as lookup key.
     * storage_type: The type of storage for push constants to use.
     * location: When storage_type=StorageType::UNIFORM_BUFFER this contains
     *    the location in the descriptor set where the uniform buffer can be
     *    bound.
     */
    void init(const shader::ShaderCreateInfo &info,
              const VKShaderInterface &interface,
              StorageType storage_type,
              VKDescriptorSet::Location location);

    /**
     * Return the storage type that is used.
     */
    StorageType storage_type_get() const
    {
      return storage_type_;
    }

    /**
     * Get the binding location for the uniform buffer.
     *
     * Only valid when storage_type=StorageType::UNIFORM_BUFFER.
     */
    VKDescriptorSet::Location descriptor_set_location_get() const
    {
      return descriptor_set_location_;
    }

    /**
     * Get the size needed to store the push constants.
     */
    uint32_t size_in_bytes() const
    {
      return size_in_bytes_;
    }

    /**
     * Find the push constant layout for the given location.
     * Location = ShaderInput.location.
     */
    const PushConstant *find(int32_t location) const;

    void debug_print() const;
  };

 private:
  const Layout *layout_ = nullptr;
  void *data_ = nullptr;

  /** Uniform buffer used to store the push constants when they don't fit. */
  std::unique_ptr<VKUniformBuffer> uniform_buffer_;

 public:
  VKPushConstants();
  VKPushConstants(const Layout *layout);
  VKPushConstants(VKPushConstants &&other);
  virtual ~VKPushConstants();

  VKPushConstants &operator=(VKPushConstants &&other);

  size_t offset() const
  {
    return 0;
  }

  const Layout &layout_get() const
  {
    return *layout_;
  }

  /**
   * Get the reference to the active data.
   *
   * Data can get inactive when push constants are modified, after being added to the command
   * queue. We still keep track of the old data for reuse and make sure we don't overwrite data
   * that is still not on the GPU.
   */
  const void *data() const
  {
    return data_;
  }

  /**
   * Modify a push constant.
   *
   * location: ShaderInput.location of the push constant to update.
   * comp_len: number of components has the data type that is being updated.
   * array_size: number of elements when an array to update. (0=no array)
   * input_data: packed source data to use.
   */
  template<typename T>
  void push_constant_set(int32_t location,
                         int32_t comp_len,
                         int32_t array_size,
                         const T *input_data)
  {
    const Layout::PushConstant *push_constant_layout = layout_->find(location);
    if (push_constant_layout == nullptr) {
      /* Legacy code can still try to update push constants when they don't exist. For example
       * `immDrawPixelsTexSetup` will bind an image slot manually. This works in OpenGL, but in
       * vulkan images aren't stored as push constants. */
      return;
    }

    uint8_t *bytes = static_cast<uint8_t *>(data_);
    T *dst = static_cast<T *>(static_cast<void *>(&bytes[push_constant_layout->offset]));
    const int inner_row_padding = push_constant_layout->inner_row_padding;
    const bool is_tightly_std140_packed = (comp_len % 4) == 0;
    /* Vec3[] are not tightly packed in std430. */
    const bool is_tightly_std430_packed = comp_len != 3 || array_size == 0;
    if (inner_row_padding == 0 &&
        ((layout_->storage_type_get() == StorageType::PUSH_CONSTANTS &&
          is_tightly_std430_packed) ||
         array_size == 0 || push_constant_layout->array_size == 0 || is_tightly_std140_packed))
    {
      const size_t copy_size_in_bytes = comp_len * max_ii(array_size, 1) * sizeof(T);
      BLI_assert_msg(push_constant_layout->offset + copy_size_in_bytes <= layout_->size_in_bytes(),
                     "Tried to write outside the push constant allocated memory.");
      memcpy(dst, input_data, copy_size_in_bytes);
      return;
    }

    /* Store elements in uniform buffer as array. In Std140 arrays have an element stride of 16
     * bytes. */
    BLI_assert(sizeof(T) == 4);
    const T *src = input_data;
    if (inner_row_padding == 0) {
      for (const int i : IndexRange(array_size)) {
        UNUSED_VARS(i);
        memcpy(dst, src, comp_len * sizeof(T));
        src += comp_len;
        dst += 4;
      }
    }
    else {
      BLI_assert_msg(array_size == 1, "No support for MAT3 arrays, but can be added when needed");
      for (const int component_index : IndexRange(comp_len)) {
        *dst = *src;
        dst += 1;
        src += 1;
        if ((component_index % inner_row_padding) == (inner_row_padding - 1)) {
          dst += 1;
        }
      }
    }
  }

  /**
   * When storage type = StorageType::UNIFORM_BUFFER use this method to update the uniform
   * buffer.
   *
   * It must be called just before adding a draw/compute command to the command queue.
   */
  void update_uniform_buffer();

  /**
   * Get a reference to the uniform buffer.
   *
   * Only valid when storage type = StorageType::UNIFORM_BUFFER.
   */
  std::unique_ptr<VKUniformBuffer> &uniform_buffer_get();
};

}  // namespace blender::gpu
