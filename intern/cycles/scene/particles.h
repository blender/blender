/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __PARTICLES_H__
#define __PARTICLES_H__

#include "util/array.h"
#include "util/types.h"

#include "graph/node.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Progress;
class Scene;

/* Particle System */

struct Particle {
  int index;
  float age;
  float lifetime;
  float3 location;
  float4 rotation;
  float size;
  float3 velocity;
  float3 angular_velocity;
};

class ParticleSystem : public Node {
 public:
  NODE_DECLARE

  ParticleSystem();
  ~ParticleSystem();

  void tag_update(Scene *scene);

  array<Particle> particles;
};

/* ParticleSystem Manager */

class ParticleSystemManager {
  bool need_update_;

 public:
  ParticleSystemManager();
  ~ParticleSystemManager();

  void device_update_particles(Device *device,
                               DeviceScene *dscene,
                               Scene *scene,
                               Progress &progress);
  void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);
  void device_free(Device *device, DeviceScene *dscene);

  void tag_update(Scene *scene);

  bool need_update() const;
};

CCL_NAMESPACE_END

#endif /* __PARTICLES_H__ */
