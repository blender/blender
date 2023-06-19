/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/particles.h"
#include "device/device.h"
#include "scene/scene.h"
#include "scene/stats.h"

#include "util/foreach.h"
#include "util/hash.h"
#include "util/log.h"
#include "util/map.h"
#include "util/progress.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

/* Particle System */

NODE_DEFINE(ParticleSystem)
{
  NodeType *type = NodeType::add("particle_system", create);
  return type;
}

ParticleSystem::ParticleSystem() : Node(get_node_type()) {}

ParticleSystem::~ParticleSystem() {}

void ParticleSystem::tag_update(Scene *scene)
{
  scene->particle_system_manager->tag_update(scene);
}

/* Particle System Manager */

ParticleSystemManager::ParticleSystemManager()
{
  need_update_ = true;
}

ParticleSystemManager::~ParticleSystemManager() {}

void ParticleSystemManager::device_update_particles(Device *,
                                                    DeviceScene *dscene,
                                                    Scene *scene,
                                                    Progress &progress)
{
  /* count particles.
   * adds one dummy particle at the beginning to avoid invalid lookups,
   * in case a shader uses particle info without actual particle data. */
  int num_particles = 1;
  for (size_t j = 0; j < scene->particle_systems.size(); j++)
    num_particles += scene->particle_systems[j]->particles.size();

  KernelParticle *kparticles = dscene->particles.alloc(num_particles);

  /* dummy particle */
  memset(kparticles, 0, sizeof(KernelParticle));

  int i = 1;
  for (size_t j = 0; j < scene->particle_systems.size(); j++) {
    ParticleSystem *psys = scene->particle_systems[j];

    for (size_t k = 0; k < psys->particles.size(); k++) {
      /* pack in texture */
      Particle &pa = psys->particles[k];

      kparticles[i].index = pa.index;
      kparticles[i].age = pa.age;
      kparticles[i].lifetime = pa.lifetime;
      kparticles[i].size = pa.size;
      kparticles[i].rotation = pa.rotation;
      kparticles[i].location = float3_to_float4(pa.location);
      kparticles[i].velocity = float3_to_float4(pa.velocity);
      kparticles[i].angular_velocity = float3_to_float4(pa.angular_velocity);

      i++;

      if (progress.get_cancel())
        return;
    }
  }

  dscene->particles.copy_to_device();
}

void ParticleSystemManager::device_update(Device *device,
                                          DeviceScene *dscene,
                                          Scene *scene,
                                          Progress &progress)
{
  if (!need_update())
    return;

  scoped_callback_timer timer([scene](double time) {
    if (scene->update_stats) {
      scene->update_stats->particles.times.add_entry({"device_update", time});
    }
  });

  VLOG_INFO << "Total " << scene->particle_systems.size() << " particle systems.";

  device_free(device, dscene);

  progress.set_status("Updating Particle Systems", "Copying Particles to device");
  device_update_particles(device, dscene, scene, progress);

  if (progress.get_cancel())
    return;

  need_update_ = false;
}

void ParticleSystemManager::device_free(Device *, DeviceScene *dscene)
{
  dscene->particles.free();
}

void ParticleSystemManager::tag_update(Scene * /*scene*/)
{
  need_update_ = true;
}

bool ParticleSystemManager::need_update() const
{
  return need_update_;
}

CCL_NAMESPACE_END
