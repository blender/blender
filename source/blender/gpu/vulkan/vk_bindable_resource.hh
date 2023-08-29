/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_array.hh"

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
template<shader::ShaderCreateInfo::Resource::BindType BindType, int MaxBindings = 16>
class VKBindSpace {
  Array<VKBindableResource *> bindings_ = Array<VKBindableResource *>(MaxBindings);

 public:
  VKBindSpace()
  {
    bindings_.fill(nullptr);
  }

  /**
   * Register a binding to this namespace.
   */
  void bind(int binding, VKBindableResource &resource)
  {
    bindings_[binding] = &resource;
  }

  /**
   * Apply registered bindings to the active shader.
   */
  void apply_bindings()
  {
    for (int binding : IndexRange(MaxBindings)) {
      if (bindings_[binding] != nullptr) {
        bindings_[binding]->bind(binding, BindType);
      }
    }
  }

  /**
   * Unregister the given resource from this namespace.
   */
  void unbind(VKBindableResource &resource)
  {
    for (int binding : IndexRange(MaxBindings)) {
      if (bindings_[binding] == &resource) {
        bindings_[binding] = nullptr;
      }
    }
  }

  /**
   * Remove all bindings from this namespace.
   */
  void unbind_all()
  {
    bindings_.fill(nullptr);
  }
};

}  // namespace blender::gpu
