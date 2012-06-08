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

#include "object.h"

#include "mesh.h"
#include "blender_sync.h"
#include "blender_util.h"

#include "util_foreach.h"

CCL_NAMESPACE_BEGIN

/* Utilities */


/* Particles Sync */

bool BlenderSync::object_use_particles(BL::Object b_ob)
{
	/* Particle data is only needed for
	 * a) Billboard render mode if object's own material uses particle info
	 * b) object/group render mode if any dupli object's material uses particle info
	 *
	 * Note: Meshes have to be synced at this point!
	 */
	bool use_particles = false;
	
	BL::Object::particle_systems_iterator b_psys;
	for (b_ob.particle_systems.begin(b_psys); b_psys != b_ob.particle_systems.end(); ++b_psys) {
		switch (b_psys->settings().render_type()) {
		/* XXX not implemented yet! 
		 * billboards/strands would become part of the mesh data (?),
		 * so the mesh attributes would store whether particle info is required.
		 */
		#if 0
		case BL::ParticleSettings::render_type_BILLBOARD:
		case BL::ParticleSettings::render_type_PATH: {	/* for strand rendering */
			BL::ID key = (BKE_object_is_modified(b_ob))? b_ob: b_ob.data();
			Mesh *mesh = mesh_map.find(key);
			if (mesh) {
				use_particles |= mesh->need_attribute(scene, ATTR_STD_PARTICLE);
			}
			break;
		}
		#endif
		
		case BL::ParticleSettings::render_type_OBJECT: {
			BL::Object b_dupli_ob = b_psys->settings().dupli_object();
			if (b_dupli_ob) {
				BL::ID key = (BKE_object_is_modified(b_dupli_ob))? b_dupli_ob: b_dupli_ob.data();
				Mesh *mesh = mesh_map.find(key);
				if (mesh) {
					use_particles |= mesh->need_attribute(scene, ATTR_STD_PARTICLE);
				}
			}
			break;
		}
		
		case BL::ParticleSettings::render_type_GROUP: {
			BL::Group b_dupli_group = b_psys->settings().dupli_group();
			if (b_dupli_group) {
				BL::Group::objects_iterator b_gob;
				for (b_dupli_group.objects.begin(b_gob); b_gob != b_dupli_group.objects.end(); ++b_gob) {
					BL::ID key = (BKE_object_is_modified(*b_gob))? *b_gob: b_gob->data();
					Mesh *mesh = mesh_map.find(key);
					if (mesh) {
						use_particles |= mesh->need_attribute(scene, ATTR_STD_PARTICLE);
					}
				}
			}
			break;
		}
		
		default:
			/* avoid compiler warning */
			break;
		}
	}
	
	return use_particles;
}

static bool use_particle_system(BL::ParticleSystem b_psys)
{
	/* only use duplicator particles? disabled particle info for
	 * halo and billboard to reduce particle count.
	 * Probably not necessary since particles don't contain a huge amount
	 * of data compared to other textures.
	 */
	#if 0
	int render_type = b_psys->settings().render_type();
	return (render_type == BL::ParticleSettings::render_type_OBJECT
	        || render_type == BL::ParticleSettings::render_type_GROUP);
	#endif
	
	return true;
}

static bool use_particle(BL::Particle b_pa)
{
	return b_pa.is_exist() && b_pa.is_visible() && b_pa.alive_state()==BL::Particle::alive_state_ALIVE;
}

int BlenderSync::object_count_particles(BL::Object b_ob)
{
	int tot = 0;
	BL::Object::particle_systems_iterator b_psys;
	for(b_ob.particle_systems.begin(b_psys); b_psys != b_ob.particle_systems.end(); ++b_psys) {
		if (use_particle_system(*b_psys)) {
			BL::ParticleSystem::particles_iterator b_pa;
			for(b_psys->particles.begin(b_pa); b_pa != b_psys->particles.end(); ++b_pa) {
				if(use_particle(*b_pa))
					++tot;
			}
		}
	}
	return tot;
}

void BlenderSync::sync_particles(Object *ob, BL::Object b_ob)
{
	int tot = object_count_particles(b_ob);
	
	ob->particles.clear();
	ob->particles.reserve(tot);
	
	int index;
	BL::Object::particle_systems_iterator b_psys;
	for(b_ob.particle_systems.begin(b_psys); b_psys != b_ob.particle_systems.end(); ++b_psys) {
		if (use_particle_system(*b_psys)) {
			BL::ParticleSystem::particles_iterator b_pa;
			for(b_psys->particles.begin(b_pa), index=0; b_pa != b_psys->particles.end(); ++b_pa, ++index) {
				if(use_particle(*b_pa)) {
					Particle pa;
					
					pa.age = b_scene.frame_current() - b_pa->birth_time();
					pa.lifetime = b_pa->lifetime();
					
					ob->particles.push_back(pa);
				}
			}
		}
	}
}

CCL_NAMESPACE_END
