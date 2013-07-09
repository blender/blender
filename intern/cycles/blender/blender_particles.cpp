/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

	/* return that this object has particle data */
	return true;
}

CCL_NAMESPACE_END
