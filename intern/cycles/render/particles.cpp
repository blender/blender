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

#include "device/device.h"
#include "render/particles.h"
#include "render/scene.h"

#include "util/util_foreach.h"
#include "util/util_hash.h"
#include "util/util_logging.h"
#include "util/util_map.h"
#include "util/util_progress.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

/* Particle System */

ParticleSystem::ParticleSystem()
{
}

ParticleSystem::~ParticleSystem()
{
}

void ParticleSystem::tag_update(Scene *scene)
{
	scene->particle_system_manager->need_update = true;
}

/* Particle System Manager */

ParticleSystemManager::ParticleSystemManager()
{
	need_update = true;
}

ParticleSystemManager::~ParticleSystemManager()
{
}

void ParticleSystemManager::device_update_particles(Device *, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	/* count particles.
	 * adds one dummy particle at the beginning to avoid invalid lookups,
	 * in case a shader uses particle info without actual particle data. */
	int num_particles = 1;
	for(size_t j = 0; j < scene->particle_systems.size(); j++)
		num_particles += scene->particle_systems[j]->particles.size();

	KernelParticle *kparticles = dscene->particles.alloc(num_particles);

	/* dummy particle */
	memset(kparticles, 0, sizeof(KernelParticle));

	int i = 1;
	for(size_t j = 0; j < scene->particle_systems.size(); j++) {
		ParticleSystem *psys = scene->particle_systems[j];

		for(size_t k = 0; k < psys->particles.size(); k++) {
			/* pack in texture */
			Particle& pa = psys->particles[k];

			kparticles[i].index = pa.index;
			kparticles[i].age = pa.age;
			kparticles[i].lifetime = pa.lifetime;
			kparticles[i].size = pa.size;
			kparticles[i].rotation = pa.rotation;
			kparticles[i].location = float3_to_float4(pa.location);
			kparticles[i].velocity = float3_to_float4(pa.velocity);
			kparticles[i].angular_velocity = float3_to_float4(pa.angular_velocity);

			i++;

			if(progress.get_cancel()) return;
		}
	}

	dscene->particles.copy_to_device();
}

void ParticleSystemManager::device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	if(!need_update)
		return;

	VLOG(1) << "Total " << scene->particle_systems.size()
	        << " particle systems.";

	device_free(device, dscene);

	progress.set_status("Updating Particle Systems", "Copying Particles to device");
	device_update_particles(device, dscene, scene, progress);

	if(progress.get_cancel()) return;

	need_update = false;
}

void ParticleSystemManager::device_free(Device *, DeviceScene *dscene)
{
	dscene->particles.free();
}

void ParticleSystemManager::tag_update(Scene * /*scene*/)
{
	need_update = true;
}

CCL_NAMESPACE_END
