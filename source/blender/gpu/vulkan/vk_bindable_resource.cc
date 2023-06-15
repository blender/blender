/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_bindable_resource.hh"
#include "vk_backend.hh"
#include "vk_context.hh"
#include "vk_device.hh"
#include "vk_state_manager.hh"

namespace blender::gpu {

VKBindableResource::~VKBindableResource()
{
  unbind_from_all_contexts();
}

void VKBindableResource::unbind_from_active_context()
{
  const VKContext *context = VKContext::get();
  if (context != nullptr) {
    VKStateManager &state_manager = context->state_manager_get();
    state_manager.unbind_from_all_namespaces(*this);
  }
}

void VKBindableResource::unbind_from_all_contexts()
{
  for (const VKContext &context : VKBackend::get().device_get().contexts_get()) {
    VKStateManager &state_manager = context.state_manager_get();
    state_manager.unbind_from_all_namespaces(*this);
  }
}

}  // namespace blender::gpu