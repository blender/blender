/*
 * Copyright 2011-2015 Blender Foundation
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

#include "blender_texture.h"

CCL_NAMESPACE_BEGIN

namespace {

/* Point density helpers. */

static void density_texture_space_invert(float3& loc,
                                         float3& size)
{
	if(size.x != 0.0f) size.x = 0.5f/size.x;
	if(size.y != 0.0f) size.y = 0.5f/size.y;
	if(size.z != 0.0f) size.z = 0.5f/size.z;

	loc = loc*size - make_float3(0.5f, 0.5f, 0.5f);
}

static void density_object_texture_space(BL::Object b_ob,
                                         float radius,
                                         float3& loc,
                                         float3& size)
{
	if(b_ob.type() == BL::Object::type_MESH) {
		BL::Mesh b_mesh(b_ob.data());
		loc = get_float3(b_mesh.texspace_location());
		size = get_float3(b_mesh.texspace_size());
	}
	else {
		/* TODO(sergey): Not supported currently. */
	}
	/* Adjust texture space to include density points on the boundaries. */
	size = size + make_float3(radius, radius, radius);
	density_texture_space_invert(loc, size);
}

static void density_particle_system_texture_space(
        BL::Object b_ob,
        BL::ParticleSystem b_particle_system,
        float radius,
        float3& loc,
        float3& size)
{
	if(b_particle_system.settings().type() == BL::ParticleSettings::type_HAIR) {
		/* TODO(sergey): Not supported currently. */
		return;
	}
	Transform tfm = get_transform(b_ob.matrix_world());
	Transform itfm = transform_inverse(tfm);
	float3 min = make_float3(FLT_MAX, FLT_MAX, FLT_MAX),
	       max = make_float3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	float3 particle_size = make_float3(radius, radius, radius);
	for(int i = 0; i < b_particle_system.particles.length(); ++i) {
		BL::Particle particle = b_particle_system.particles[i];
		if(particle.alive_state() == BL::Particle::alive_state_ALIVE) {
			float3 location = get_float3(particle.location());
			location = transform_point(&itfm, location);
			min = ccl::min(min, location - particle_size);
			max = ccl::max(max, location + particle_size);
		}
	}
	/* Calculate texture space from the particle bounds.  */
	loc = (min + max) * 0.5f;
	size = (max - min) * 0.5f;
	density_texture_space_invert(loc, size);
}

}  /* namespace */

void point_density_texture_space(BL::ShaderNodeTexPointDensity b_point_density_node,
                                 float3& loc,
                                 float3& size)
{
	/* Fallback values. */
	loc = make_float3(0.0f, 0.0f, 0.0f);
	size = make_float3(0.0f, 0.0f, 0.0f);
	BL::Object b_ob(b_point_density_node.object());
	if(!b_ob) {
		return;
	}
	if(b_point_density_node.point_source() ==
	   BL::ShaderNodeTexPointDensity::point_source_PARTICLE_SYSTEM)
	{
		BL::ParticleSystem b_particle_system(
		        b_point_density_node.particle_system());
		if(b_particle_system) {
			density_particle_system_texture_space(b_ob,
			                                      b_particle_system,
			                                      b_point_density_node.radius(),
			                                      loc,
			                                      size);
		}
	}
	else {
		density_object_texture_space(b_ob,
		                             b_point_density_node.radius(),
		                             loc,
		                             size);
	}
}

CCL_NAMESPACE_END
