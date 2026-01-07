/* SPDX-FileCopyrightText: 2011-2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "blender/light_linking.h"

#include "DNA_object_types.h"

#include "kernel/types.h"

CCL_NAMESPACE_BEGIN

static const blender::LightLinking *get_light_linking(const blender::Object &b_object)
{
  return b_object.light_linking;
}

uint64_t BlenderLightLink::get_light_set_membership(const blender::Object * /*parent*/,
                                                    const blender::Object &object)
{
  const blender::LightLinking *light_linking = get_light_linking(object);
  return (light_linking) ? light_linking->runtime.light_set_membership : LIGHT_LINK_MASK_ALL;
}

uint BlenderLightLink::get_receiver_light_set(const blender::Object *parent,
                                              const blender::Object &object)
{
  if (parent) {
    const blender::LightLinking *parent_light_linking = get_light_linking(*parent);
    if (parent_light_linking && parent_light_linking->runtime.receiver_light_set) {
      return parent_light_linking->runtime.receiver_light_set;
    }
  }

  const blender::LightLinking *light_linking = get_light_linking(object);
  return (light_linking) ? light_linking->runtime.receiver_light_set : 0;
}

uint64_t BlenderLightLink::get_shadow_set_membership(const blender::Object * /*parent*/,
                                                     const blender::Object &object)
{
  const blender::LightLinking *light_linking = get_light_linking(object);
  return (light_linking) ? light_linking->runtime.shadow_set_membership : LIGHT_LINK_MASK_ALL;
}

uint BlenderLightLink::get_blocker_shadow_set(const blender::Object *parent,
                                              const blender::Object &object)
{
  if (parent) {
    const blender::LightLinking *parent_light_linking = get_light_linking(*parent);
    if (parent_light_linking && parent_light_linking->runtime.blocker_shadow_set) {
      return parent_light_linking->runtime.blocker_shadow_set;
    }
  }

  const blender::LightLinking *light_linking = get_light_linking(object);
  return (light_linking) ? light_linking->runtime.blocker_shadow_set : 0;
}

CCL_NAMESPACE_END
