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
 * limitations under the License
 */

#include "device.h"
#include "particles.h"
#include "scene.h"

#include "util_foreach.h"
#include "util_map.h"
#include "util_progress.h"
#include "util_vector.h"

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

void ParticleSystemManager::device_update_particles(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	/* count particles.
	 * adds one dummy particle at the beginning to avoid invalid lookups,
	 * in case a shader uses particle info without actual particle data. */
	int num_particles = 1;
	foreach(ParticleSystem *psys, scene->particle_systems)
		num_particles += psys->particles.size();
	
	float4 *particles = dscene->particles.resize(PARTICLE_SIZE*num_particles);
	
	/* dummy particle */
	particles[0] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
	particles[1] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
	particles[2] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
	particles[3] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
	particles[4] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
	
	int i = 1;
	foreach(ParticleSystem *psys, scene->particle_systems) {
		foreach(Particle &pa, psys->particles) {
			/* pack in texture */
			int offset = i*PARTICLE_SIZE;
			
			particles[offset] = make_float4(pa.index, pa.age, pa.lifetime, pa.size);
			particles[offset+1] = pa.rotation;
			particles[offset+2] = make_float4(pa.location.x, pa.location.y, pa.location.z, pa.velocity.x);
			particles[offset+3] = make_float4(pa.velocity.y, pa.velocity.z, pa.angular_velocity.x, pa.angular_velocity.y);
			particles[offset+4] = make_float4(pa.angular_velocity.z, 0.0f, 0.0f, 0.0f);
			
			i++;
			
			if(progress.get_cancel()) return;
		}
	}
	
	device->tex_alloc("__particles", dscene->particles);
}

void ParticleSystemManager::device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	if(!need_update)
		return;
	
	device_free(device, dscene);

	progress.set_status("Updating Particle Systems", "Copying Particles to device");
	device_update_particles(device, dscene, scene, progress);
	
	if(progress.get_cancel()) return;
	
	need_update = false;
}

void ParticleSystemManager::device_free(Device *device, DeviceScene *dscene)
{
	device->tex_free(dscene->particles);
	dscene->particles.clear();
}

void ParticleSystemManager::tag_update(Scene *scene)
{
	need_update = true;
}

CCL_NAMESPACE_END

