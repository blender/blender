/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "BKE_gpencil_legacy.h"
#include "BKE_image.h"
#include "DRW_gpu_wrapper.hh"
#include "DRW_render.hh"

#include "draw_manager.hh"
#include "draw_pass.hh"

namespace blender::draw::greasepencil {

using namespace draw;

class LightModule {
 private:
  /** Contains all lights in the scene. */
  StorageVectorBuffer<gpLight> lights_buf_ = "gp_lights_buf";

  float studiolight_intensity_ = 1.0f;
  bool use_scene_lights_ = true;
  bool use_scene_world_ = true;

 public:
  void init(const View3D *v3d)
  {
    if (v3d != nullptr) {
      use_scene_lights_ = V3D_USES_SCENE_LIGHTS(v3d);
      use_scene_world_ = V3D_USES_SCENE_WORLD(v3d);
      studiolight_intensity_ = v3d->shading.studiolight_intensity;
    }
  }

  void begin_sync(Depsgraph *depsgraph)
  {
    lights_buf_.clear();

    World *world = DEG_get_evaluated_scene(depsgraph)->world;
    if (world != nullptr && use_scene_world_) {
      ambient_sync(float3(world->horr, world->horg, world->horb));
    }
    else {
      ambient_sync(float3(studiolight_intensity_));
    }
  }

  void sync(ObjectRef &object_ref)
  {
    if (!use_scene_lights_) {
      return;
    }
    const Object *ob = object_ref.object;
    const Light *la = static_cast<Light *>(ob->data);

    float light_power;
    if (la->type == LA_AREA) {
      light_power = 1.0f / (4.0f * M_PI);
    }
    else if (ELEM(la->type, LA_SPOT, LA_LOCAL)) {
      light_power = 1.0f / (4.0f * M_PI * M_PI);
    }
    else {
      light_power = 1.0f / M_PI;
    }

    gpLight light;
    float4x4 &mat = *reinterpret_cast<float4x4 *>(&light.right);
    switch (la->type) {
      case LA_SPOT:
        light.type = GP_LIGHT_TYPE_SPOT;
        light.spot_size = cosf(la->spotsize * 0.5f);
        light.spot_blend = (1.0f - light.spot_size) * la->spotblend;
        mat = ob->world_to_object();
        break;
      case LA_AREA:
        /* Simulate area lights using a spot light. */
        light.type = GP_LIGHT_TYPE_SPOT;
        light.spot_size = cosf(M_PI_2);
        light.spot_blend = (1.0f - light.spot_size) * 1.0f;
        normalize_m4_m4(mat.ptr(), ob->object_to_world().ptr());
        invert_m4(mat.ptr());
        break;
      case LA_SUN:
        light.forward = math::normalize(float3(ob->object_to_world().ptr()[2]));
        light.type = GP_LIGHT_TYPE_SUN;
        break;
      default:
        light.type = GP_LIGHT_TYPE_POINT;
        break;
    }
    light.position = float3(object_ref.object->object_to_world().location());
    light.color = float3(la->r, la->g, la->b) * (la->energy * light_power);

    lights_buf_.append(light);
  }

  void end_sync()
  {
    /* Tag light list end. */
    gpLight light;
    light.color[0] = -1.0f;
    lights_buf_.append(light);

    lights_buf_.push_update();
  }

  void bind_resources(PassMain::Sub &sub)
  {
    sub.bind_ssbo(GPENCIL_LIGHT_SLOT, &lights_buf_);
  }

 private:
  void ambient_sync(float3 color)
  {
    gpLight light;
    light.type = GP_LIGHT_TYPE_AMBIENT;
    light.color = color;

    lights_buf_.append(light);
  }
};

}  // namespace blender::draw::greasepencil
