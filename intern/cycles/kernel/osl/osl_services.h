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

#ifndef __OSL_SERVICES_H__
#define __OSL_SERVICES_H__

/* OSL Render Services
 *
 * Implementation of OSL render services, to retriever matrices, attributes,
 * textures and point clouds. In principle this should only be accessing
 * kernel data, but currently we also reach back into the Scene to retrieve
 * attributes.
 */

#include <OSL/oslexec.h>
#include <OSL/oslclosure.h>

CCL_NAMESPACE_BEGIN

class Object;
class Scene;
class Shader;
struct ShaderData;
struct float3;
struct KernelGlobals;

class OSLRenderServices : public OSL::RendererServices
{
public:
	OSLRenderServices();
	~OSLRenderServices();
	
	void thread_init(KernelGlobals *kernel_globals);

	bool get_matrix(OSL::Matrix44 &result, OSL::TransformationPtr xform, float time);
	bool get_inverse_matrix(OSL::Matrix44 &result, OSL::TransformationPtr xform, float time);
	
	bool get_matrix(OSL::Matrix44 &result, ustring from, float time);
	bool get_inverse_matrix(OSL::Matrix44 &result, ustring to, float time);
	
	bool get_matrix(OSL::Matrix44 &result, OSL::TransformationPtr xform);
	bool get_inverse_matrix(OSL::Matrix44 &result, OSL::TransformationPtr xform);
	
	bool get_matrix(OSL::Matrix44 &result, ustring from);
	bool get_inverse_matrix(OSL::Matrix44 &result, ustring from);

	bool get_array_attribute(void *renderstate, bool derivatives,
	                         ustring object, TypeDesc type, ustring name,
	                         int index, void *val);
	bool get_attribute(void *renderstate, bool derivatives, ustring object,
	                   TypeDesc type, ustring name, void *val);

	bool get_userdata(bool derivatives, ustring name, TypeDesc type,
	                  void *renderstate, void *val);
	bool has_userdata(ustring name, TypeDesc type, void *renderstate);

	int pointcloud_search(OSL::ShaderGlobals *sg, ustring filename, const OSL::Vec3 &center,
	                      float radius, int max_points, bool sort, size_t *out_indices,
	                      float *out_distances, int derivs_offset);

	int pointcloud_get(ustring filename, size_t *indices, int count, ustring attr_name,
	                   TypeDesc attr_type, void *out_data);

	bool trace(TraceOpt &options, OSL::ShaderGlobals *sg,
	           const OSL::Vec3 &P, const OSL::Vec3 &dPdx,
	           const OSL::Vec3 &dPdy, const OSL::Vec3 &R,
	           const OSL::Vec3 &dRdx, const OSL::Vec3 &dRdy);

	bool getmessage(OSL::ShaderGlobals *sg, ustring source, ustring name,
	                TypeDesc type, void *val, bool derivatives);

	bool texture(ustring filename, TextureOpt &options,
	             OSL::ShaderGlobals *sg,
	             float s, float t, float dsdx, float dtdx,
	             float dsdy, float dtdy, float *result);

	bool texture3d(ustring filename, TextureOpt &options,
	               OSL::ShaderGlobals *sg, const OSL::Vec3 &P,
	               const OSL::Vec3 &dPdx, const OSL::Vec3 &dPdy,
	               const OSL::Vec3 &dPdz, float *result);

	bool environment(ustring filename, TextureOpt &options,
	                 OSL::ShaderGlobals *sg, const OSL::Vec3 &R,
	                 const OSL::Vec3 &dRdx, const OSL::Vec3 &dRdy, float *result);

	bool get_texture_info(ustring filename, int subimage,
	                      ustring dataname, TypeDesc datatype, void *data);

	static bool get_background_attribute(KernelGlobals *kg, ShaderData *sd, ustring name,
			TypeDesc type, bool derivatives, void *val);
	static bool get_object_standard_attribute(KernelGlobals *kg, ShaderData *sd, ustring name,
			TypeDesc type, bool derivatives, void *val);

	static ustring u_distance;
	static ustring u_index;
	static ustring u_camera;
	static ustring u_screen;
	static ustring u_raster;
	static ustring u_ndc;
	static ustring u_object_location;
	static ustring u_object_index;
	static ustring u_geom_dupli_generated;
	static ustring u_geom_dupli_uv;
	static ustring u_material_index;
	static ustring u_object_random;
	static ustring u_particle_index;
	static ustring u_particle_age;
	static ustring u_particle_lifetime;
	static ustring u_particle_location;
	static ustring u_particle_rotation;
	static ustring u_particle_size;
	static ustring u_particle_velocity;
	static ustring u_particle_angular_velocity;
	static ustring u_geom_numpolyvertices;
	static ustring u_geom_trianglevertices;
	static ustring u_geom_polyvertices;
	static ustring u_geom_name;
	static ustring u_path_ray_length;
	static ustring u_trace;
	static ustring u_hit;
	static ustring u_hitdist;
	static ustring u_N;
	static ustring u_Ng;
	static ustring u_P;
	static ustring u_I;
	static ustring u_u;
	static ustring u_v;
	static ustring u_empty;

private:
	KernelGlobals *kernel_globals;
};

CCL_NAMESPACE_END

#endif /* __OSL_SERVICES_H__  */

