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

#include "mesh.h"
#include "object.h"
#include "particles.h"

#include "blender_sync.h"
#include "blender_util.h"

#include "util_foreach.h"

CCL_NAMESPACE_BEGIN

/* Utilities */

bool BlenderSync::sync_dupli_particle(BL::Object b_ob, BL::DupliObject b_dup, Object *object)
{
	/* test if this dupli was generated from a particle sytem */
	BL::ParticleSystem b_psys = b_dup.particle_system();
	if(!b_psys)
		return false;

	/* test if we need particle data */
	if(!object->mesh->need_attribute(scene, ATTR_STD_PARTICLE))
		return false;

	/* don't handle child particles yet */
	BL::Array<int, OBJECT_PERSISTENT_ID_SIZE> persistent_id = b_dup.persistent_id();

	if(persistent_id[0] >= b_psys.particles.length())
		return false;

	/* find particle system */
	ParticleSystemKey key(b_ob, persistent_id);
	ParticleSystem *psys;

	bool first_use = !particle_system_map.is_used(key);
	bool need_update = particle_system_map.sync(&psys, b_ob, b_dup.object(), key);

	/* no update needed? */
	if(!need_update && !object->mesh->need_update && !scene->object_manager->need_update)
		return true;

	/* first time used in this sync loop? clear and tag update */
	if(first_use) {
		psys->particles.clear();
		psys->tag_update(scene);
	}

	/* add particle */
	BL::Particle b_pa = b_psys.particles[persistent_id[0]];
	Particle pa;
	
	pa.index = persistent_id[0];
	pa.age = b_scene.frame_current() - b_pa.birth_time();
	pa.lifetime = b_pa.lifetime();
	pa.location = get_float3(b_pa.location());
	pa.rotation = get_float4(b_pa.rotation());
	pa.size = b_pa.size();
	pa.velocity = get_float3(b_pa.velocity());
	pa.angular_velocity = get_float3(b_pa.angular_velocity());

	psys->particles.push_back(pa);

	if (object->particle_index != psys->particles.size() - 1)
		scene->object_manager->tag_update(scene);
	object->particle_system = psys;
	object->particle_index = psys->particles.size() - 1;

	/* return that this object has particle data */
	return true;
}

CCL_NAMESPACE_END
