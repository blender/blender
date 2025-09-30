/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_state_private.hh"

#include "BLI_array.hh"

#include "render_graph/vk_resource_access_info.hh"

namespace blender::gpu {
class VKTexture;
class VKUniformBuffer;
class VKVertexBuffer;
class VKStorageBuffer;
class VKIndexBuffer;
class VKContext;
class VKDescriptorSetTracker;

/**
 * Offset when searching for bindings.
 *
 * When shaders combine images and samplers, the images have to be offset to find the correct
 * shader input. Both textures and images are stored in the uniform list and their ID can be
 * overlapping.
 */
static constexpr int BIND_SPACE_IMAGE_OFFSET = 512;

/** Bind space for a uniform buffers. */
class BindSpaceUniformBuffers {
 public:
  Vector<VKUniformBuffer *> bound_resources;

  void bind(VKUniformBuffer *resource, int binding)
  {
    if (bound_resources.size() <= binding) {
      bound_resources.resize(binding + 1);
    }
    bound_resources[binding] = resource;
  }

  VKUniformBuffer *get(int binding) const
  {
    return bound_resources[binding];
  }

  void unbind(void *resource)
  {
    for (int index : IndexRange(bound_resources.size())) {
      if (bound_resources[index] == resource) {
        bound_resources[index] = nullptr;
      }
    }
  }

  void unbind_all()
  {
    bound_resources.clear();
  }
};

/**
 * Bind space for image resources.
 */
template<int Offset> class BindSpaceImages {
 public:
  Vector<VKTexture *> bound_resources;

  void bind(VKTexture *resource,
            int binding,
            TextureWriteFormat format,
            StateManager *state_manager)
  {
    if (binding >= Offset) {
      binding -= Offset;
    }
    if (bound_resources.size() <= binding) {
      bound_resources.resize(binding + 1);
    }
    bound_resources[binding] = resource;
    state_manager->image_formats[binding] = format;
  }

  VKTexture *get(int binding) const
  {
    if (binding >= Offset) {
      binding -= Offset;
    }
    return bound_resources[binding];
  }

  void unbind(void *resource, StateManager *state_manager)
  {
    for (int index : IndexRange(bound_resources.size())) {
      if (bound_resources[index] == resource) {
        bound_resources[index] = nullptr;
        state_manager->image_formats[index] = TextureWriteFormat::Invalid;
      }
    }
  }

  void unbind_all()
  {
    bound_resources.clear();
  }
};

/** Bind space for storage buffers. */
class BindSpaceStorageBuffers {
 public:
  enum class Type {
    Unused,
    UniformBuffer,
    VertexBuffer,
    IndexBuffer,
    StorageBuffer,
    Buffer,
  };
  struct Elem {
    Type resource_type;
    void *resource;
    VkDeviceSize offset;
  };
  Vector<Elem> bound_resources;

  void bind(Type resource_type, void *resource, int binding, VkDeviceSize offset)
  {
    if (bound_resources.size() <= binding) {
      bound_resources.resize(binding + 1);
    }
    bound_resources[binding].resource_type = resource_type;
    bound_resources[binding].resource = resource;
    bound_resources[binding].offset = offset;
  }

  const Elem &get(int binding) const
  {
    return bound_resources[binding];
  }

  void unbind(void *resource)
  {
    for (int index : IndexRange(bound_resources.size())) {
      if (bound_resources[index].resource == resource) {
        bound_resources[index].resource = nullptr;
        bound_resources[index].resource_type = Type::Unused;
        bound_resources[index].offset = 0u;
      }
    }
  }

  void unbind_all()
  {
    bound_resources.clear();
  }
};

/** Bind space for textures. */
class BindSpaceTextures {
 public:
  enum class Type {
    Unused,
    Texture,
    VertexBuffer,
  };
  struct Elem {
    Type resource_type;
    void *resource;
    GPUSamplerState sampler;
  };
  Vector<Elem> bound_resources;

  void bind(Type resource_type, void *resource, GPUSamplerState sampler, int binding)
  {
    if (bound_resources.size() <= binding) {
      bound_resources.resize(binding + 1, {});
    }
    bound_resources[binding].resource_type = resource_type;
    bound_resources[binding].resource = resource;
    bound_resources[binding].sampler = sampler;
  }

  const Elem *get(int binding) const
  {
    if (binding >= bound_resources.size()) {
      /* TODO: Check with @Jeroen-Bakker.
       * Could we ensure state_manager adds default initialized bindings for each ShaderInterface
       * resource? (See #142097). */
      return nullptr;
    }
    return &bound_resources[binding];
  }

  void unbind(void *resource)
  {
    for (int index : IndexRange(bound_resources.size())) {
      if (bound_resources[index].resource == resource) {
        bound_resources[index].resource = nullptr;
        bound_resources[index].resource_type = Type::Unused;
        bound_resources[index].sampler = GPUSamplerState::default_sampler();
      }
    }
  }

  void unbind_all()
  {
    bound_resources.clear();
  }
};

class VKStateManager : public StateManager {
  friend class VKDescriptorSetUpdator;

  uint texture_unpack_row_length_ = 0;

  BindSpaceTextures textures_;
  BindSpaceImages<BIND_SPACE_IMAGE_OFFSET> images_;
  BindSpaceUniformBuffers uniform_buffers_;
  BindSpaceStorageBuffers storage_buffers_;

 public:
  bool is_dirty = false;

  void apply_state() override;
  void force_state() override;

  void issue_barrier(GPUBarrier barrier_bits) override;

  void texture_bind(Texture *tex, GPUSamplerState sampler, int unit) override;
  void texture_unbind(Texture *tex) override;
  void texture_unbind_all() override;

  void image_bind(Texture *tex, int unit) override;
  void image_unbind(Texture *tex) override;
  void image_unbind_all() override;

  void uniform_buffer_bind(VKUniformBuffer *uniform_buffer, int slot);
  void uniform_buffer_unbind(VKUniformBuffer *uniform_buffer);
  void uniform_buffer_unbind_all();

  void texel_buffer_bind(VKVertexBuffer &vertex_buffer, int slot);
  void texel_buffer_unbind(VKVertexBuffer &vertex_buffer);

  void storage_buffer_bind(BindSpaceStorageBuffers::Type resource_type,
                           void *resource,
                           int binding)
  {
    storage_buffer_bind(resource_type, resource, binding, 0);
  }
  void storage_buffer_bind(BindSpaceStorageBuffers::Type resource_type,
                           void *resource,
                           int binding,
                           VkDeviceSize offset);
  void storage_buffer_unbind(void *resource);
  void storage_buffer_unbind_all();

  void texture_unpack_row_length_set(uint len) override;

  /**
   * Row length for unpacking host data when uploading texture data.
   *
   * When set to zero (0) host data can be assumed to be stored sequential.
   */
  uint texture_unpack_row_length_get() const;
};
}  // namespace blender::gpu
