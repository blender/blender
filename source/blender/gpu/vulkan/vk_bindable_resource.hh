/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_shader_create_info.hh"

#include "BLI_utility_mixins.hh"

namespace blender::gpu {
class VKDescriptorSetTracker;
class VKShaderInterface;

namespace render_graph {
struct VKResourceAccessInfo;
}

/**
 * Access to the descriptor set, shader interface is needed when adding state manager bindings to a
 * descriptor set.
 *
 * When adding the bindings to the descriptor set we also record the access flag in
 * resource_access_info.\
 *
 * AddToDescriptorSetContext is a convenience structure so we don't need to pass the references to
 * the descriptor set, shader interface and resource access info to each method call.
 */
struct AddToDescriptorSetContext : NonCopyable {
  /** Descriptor set where to bind/add resources to. */
  VKDescriptorSetTracker &descriptor_set;

  /**
   * Shader interface of the active shader to query shader binding locations and the used access
   * flags.
   */
  const VKShaderInterface &shader_interface;

  /**
   * When adding resources to the descriptor set, its access info should be added to the
   * resource_access_info. When adding a dispatch/draw node to the render graph, this structure is
   * passed to make links with the resources and the exact access.
   */
  render_graph::VKResourceAccessInfo &resource_access_info;

  AddToDescriptorSetContext(VKDescriptorSetTracker &descriptor_set,
                            const VKShaderInterface &shader_interface,
                            render_graph::VKResourceAccessInfo &resource_access_info)
      : descriptor_set(descriptor_set),
        shader_interface(shader_interface),
        resource_access_info(resource_access_info)
  {
  }
};

/**
 * Super class for resources that can be bound to a shader.
 */
class VKBindableResource {
 protected:
  virtual ~VKBindableResource();

 public:
  /**
   * Add/bind a resource to a descriptor set (`data.descriptor_set`) and the access info
   * (`data.resource_access_info`).
   *
   * `binding` parameter is the binding as specified in the ShaderCreateInfo.
   * `bind_type` to make distinction between samples, image load/store, buffer texture binding.
   */
  virtual void add_to_descriptor_set(
      AddToDescriptorSetContext &data,
      int binding,
      shader::ShaderCreateInfo::Resource::BindType bind_type,
      const GPUSamplerState sampler_state = GPUSamplerState::default_sampler()) = 0;

 protected:
  void unbind_from_active_context();

 private:
  void unbind_from_all_contexts();
};

/**
 * Blender binds resources at context level (VKStateManager). The bindings are organized in
 * namespaces.
 */
template<shader::ShaderCreateInfo::Resource::BindType BindType> class VKBindSpace {
  class ResourceBinding {
   public:
    int binding;
    VKBindableResource *resource;
    GPUSamplerState sampler_state;
  };

  Vector<ResourceBinding> bindings_;

 public:
  /**
   * Register a binding to this namespace.
   */
  void bind(int binding,
            VKBindableResource &resource,
            const GPUSamplerState sampler_state = GPUSamplerState::default_sampler())
  {
    for (ResourceBinding &bind : bindings_) {
      if (bind.binding == binding) {
        bind.resource = &resource;
        bind.sampler_state = sampler_state;
        return;
      }
    }
    ResourceBinding bind = {binding, &resource, sampler_state};
    bindings_.append(bind);
  }

  /**
   * Apply registered bindings to the active shader.
   */
  void add_to_descriptor_set(AddToDescriptorSetContext &data)
  {
    for (ResourceBinding &binding : bindings_) {
      binding.resource->add_to_descriptor_set(
          data, binding.binding, BindType, binding.sampler_state);
    }
  }

  /**
   * Unregister the given resource from this namespace.
   */
  void unbind(VKBindableResource &resource)
  {
    bindings_.remove_if(
        [&resource](const ResourceBinding &binding) { return binding.resource == &resource; });
  }

  /**
   * Remove all bindings from this namespace.
   */
  void unbind_all()
  {
    bindings_.clear();
  }
};

}  // namespace blender::gpu
