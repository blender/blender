/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_shader_create_info.hh"

namespace blender::gpu {

/**
 * Super class for resources that can be bound to a shader.
 */
class VKBindableResource {
 protected:
  virtual ~VKBindableResource();

 public:
  /**
   * Bind the resource to the shader.
   */
  virtual void bind(int binding, shader::ShaderCreateInfo::Resource::BindType bind_type) = 0;

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
  };

  Vector<ResourceBinding> bindings_;

 public:
  /**
   * Register a binding to this namespace.
   */
  void bind(int binding, VKBindableResource &resource)
  {
    for (ResourceBinding &bind : bindings_) {
      if (bind.binding == binding) {
        bind.resource = &resource;
        return;
      }
    }
    ResourceBinding bind = {binding, &resource};
    bindings_.append(bind);
  }

  /**
   * Apply registered bindings to the active shader.
   */
  void apply_bindings()
  {
    for (ResourceBinding &binding : bindings_) {
      binding.resource->bind(binding.binding, BindType);
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
