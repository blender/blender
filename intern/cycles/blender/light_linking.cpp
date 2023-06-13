/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2023 Blender Foundation */

#include "blender/light_linking.h"

#include "scene/object.h"

#include "DNA_object_types.h"

CCL_NAMESPACE_BEGIN

static const ::Object *get_blender_object(const BL::Object &object)
{
  return reinterpret_cast<::Object *>(object.ptr.data);
}

static const ::LightLinking *get_light_linking(const BL::Object &object)
{
  const ::Object *blender_object = get_blender_object(object);
  return blender_object->light_linking;
}

uint64_t BlenderLightLink::get_light_set_membership(const BL::Object & /*parent*/,
                                                    const BL::Object &object)
{
  const ::LightLinking *light_linking = get_light_linking(object);
  return (light_linking) ? light_linking->runtime.light_set_membership : LIGHT_LINK_MASK_ALL;
}

uint BlenderLightLink::get_receiver_light_set(const BL::Object &parent, const BL::Object &object)
{
  if (parent) {
    const ::LightLinking *parent_light_linking = get_light_linking(parent);
    if (parent_light_linking && parent_light_linking->runtime.receiver_light_set) {
      return parent_light_linking->runtime.receiver_light_set;
    }
  }

  const ::LightLinking *light_linking = get_light_linking(object);
  return (light_linking) ? light_linking->runtime.receiver_light_set : 0;
}

uint64_t BlenderLightLink::get_shadow_set_membership(const BL::Object & /*parent*/,
                                                     const BL::Object &object)
{
  const ::LightLinking *light_linking = get_light_linking(object);
  return (light_linking) ? light_linking->runtime.shadow_set_membership : LIGHT_LINK_MASK_ALL;
}

uint BlenderLightLink::get_blocker_shadow_set(const BL::Object &parent, const BL::Object &object)
{
  if (parent) {
    const ::LightLinking *parent_light_linking = get_light_linking(parent);
    if (parent_light_linking && parent_light_linking->runtime.blocker_shadow_set) {
      return parent_light_linking->runtime.blocker_shadow_set;
    }
  }

  const ::LightLinking *light_linking = get_light_linking(object);
  return (light_linking) ? light_linking->runtime.blocker_shadow_set : 0;
}

CCL_NAMESPACE_END
