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
#include "particles.h"

#include "blender_sync.h"
#include "blender_util.h"

#include "util_foreach.h"

CCL_NAMESPACE_BEGIN

/* Utilities */


/* Particles Sync */

bool BlenderSync::psys_need_update(BL::ParticleSystem b_psys)
{
	/* Particle data is only needed for
	 * a) Billboard render mode if object's own material uses particle info
	 * b) object/group render mode if any dupli object's material uses particle info
	 *
	 * Note: Meshes have to be synced at this point!
	 */
	bool need_update = false;
	
	switch (b_psys.settings().render_type()) {
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
				need_update |= mesh->need_attribute(scene, ATTR_STD_PARTICLE) && mesh->need_update;
			}
			break;
		}
		#endif
		
		case BL::ParticleSettings::render_type_OBJECT: {
			BL::Object b_dupli_ob = b_psys.settings().dupli_object();
			if (b_dupli_ob) {
				BL::ID key = (BKE_object_is_modified(b_dupli_ob))? b_dupli_ob: b_dupli_ob.data();
				Mesh *mesh = mesh_map.find(key);
				if (mesh) {
					need_update |= mesh->need_attribute(scene, ATTR_STD_PARTICLE) && mesh->need_update;
				}
			}
			break;
		}
		
		case BL::ParticleSettings::render_type_GROUP: {
			BL::Group b_dupli_group = b_psys.settings().dupli_group();
			if (b_dupli_group) {
				BL::Group::objects_iterator b_gob;
				for (b_dupli_group.objects.begin(b_gob); b_gob != b_dupli_group.objects.end(); ++b_gob) {
					BL::ID key = (BKE_object_is_modified(*b_gob))? *b_gob: b_gob->data();
					Mesh *mesh = mesh_map.find(key);
					if (mesh) {
						need_update |= mesh->need_attribute(scene, ATTR_STD_PARTICLE) && mesh->need_update;
					}
				}
			}
			break;
		}
		
		default:
			/* avoid compiler warning */
			break;
	}
	
	return need_update;
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
	return b_pa.is_exist() && b_pa.is_visible() &&
	        (b_pa.alive_state()==BL::Particle::alive_state_ALIVE || b_pa.alive_state()==BL::Particle::alive_state_DYING);
}

static int psys_count_particles(BL::ParticleSystem b_psys)
{
	int tot = 0;
	BL::ParticleSystem::particles_iterator b_pa;
	for(b_psys.particles.begin(b_pa); b_pa != b_psys.particles.end(); ++b_pa) {
		if(use_particle(*b_pa))
			++tot;
	}
	return tot;
}

int BlenderSync::object_count_particles(BL::Object b_ob)
{
	int tot = 0;
	BL::Object::particle_systems_iterator b_psys;
	for(b_ob.particle_systems.begin(b_psys); b_psys != b_ob.particle_systems.end(); ++b_psys) {
		if (use_particle_system(*b_psys))
			tot += psys_count_particles(*b_psys);
	}
	return tot;
}

void BlenderSync::sync_particles(BL::Object b_ob, BL::ParticleSystem b_psys)
{
	/* depending on settings the psys may not even be rendered */
	if (!use_particle_system(b_psys))
		return;
	
	/* key to lookup particle system */
	ParticleSystemKey key(b_ob, b_psys);
	ParticleSystem *psys;
	
	/* test if we need to sync */
	bool object_updated = false;
	
	if(particle_system_map.sync(&psys, b_ob, b_ob, key))
		object_updated = true;
	
	bool need_update = psys_need_update(b_psys);
	
	if (object_updated || need_update) {
		int tot = psys_count_particles(b_psys);
		psys->particles.clear();
		psys->particles.reserve(tot);
		
		int index = 0;
		BL::ParticleSystem::particles_iterator b_pa;
		for(b_psys.particles.begin(b_pa); b_pa != b_psys.particles.end(); ++b_pa) {
			if(use_particle(*b_pa)) {
				Particle pa;
				
				pa.index = index;
				pa.age = b_scene.frame_current() - b_pa->birth_time();
				pa.lifetime = b_pa->lifetime();
				pa.location = get_float3(b_pa->location());
				pa.rotation = get_float4(b_pa->rotation());
				pa.size = b_pa->size();
				pa.velocity = get_float3(b_pa->velocity());
				pa.angular_velocity = get_float3(b_pa->angular_velocity());
				
				psys->particles.push_back(pa);
			}
			
			++index;
		}
		
		psys->tag_update(scene);
	}
}

void BlenderSync::sync_particle_systems()
{
	/* layer data */
	uint scene_layer = render_layer.scene_layer;
	
	particle_system_map.pre_sync();

	/* object loop */
	BL::Scene::objects_iterator b_ob;
	BL::Scene b_sce = b_scene;

	for(; b_sce; b_sce = b_sce.background_set()) {
		for(b_sce.objects.begin(b_ob); b_ob != b_sce.objects.end(); ++b_ob) {
			bool hide = (render_layer.use_viewport_visibility)? b_ob->hide(): b_ob->hide_render();
			uint ob_layer = get_layer(b_ob->layers(), b_ob->layers_local_view(), render_layer.use_localview, object_is_light(*b_ob));
			hide = hide || !(ob_layer & scene_layer);

			if(!hide) {
				BL::Object::particle_systems_iterator b_psys;
				for(b_ob->particle_systems.begin(b_psys); b_psys != b_ob->particle_systems.end(); ++b_psys)
					sync_particles(*b_ob, *b_psys);
			}
		}
	}

	/* handle removed data and modified pointers */
	if(particle_system_map.post_sync())
		scene->particle_system_manager->tag_update(scene);
}

CCL_NAMESPACE_END
