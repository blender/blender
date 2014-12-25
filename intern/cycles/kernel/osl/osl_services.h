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

#ifdef WITH_PTEX
class PtexCache;
#endif

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
	
	void thread_init(KernelGlobals *kernel_globals, OSL::TextureSystem *ts);

	bool get_matrix(OSL::ShaderGlobals *sg, OSL::Matrix44 &result, OSL::TransformationPtr xform, float time);
	bool get_inverse_matrix(OSL::ShaderGlobals *sg, OSL::Matrix44 &result, OSL::TransformationPtr xform, float time);
	
	bool get_matrix(OSL::ShaderGlobals *sg, OSL::Matrix44 &result, ustring from, float time);
	bool get_inverse_matrix(OSL::ShaderGlobals *sg, OSL::Matrix44 &result, ustring to, float time);
	
	bool get_matrix(OSL::ShaderGlobals *sg, OSL::Matrix44 &result, OSL::TransformationPtr xform);
	bool get_inverse_matrix(OSL::ShaderGlobals *sg, OSL::Matrix44 &result, OSL::TransformationPtr xform);
	
	bool get_matrix(OSL::ShaderGlobals *sg, OSL::Matrix44 &result, ustring from);
	bool get_inverse_matrix(OSL::ShaderGlobals *sg, OSL::Matrix44 &result, ustring from);

	bool get_array_attribute(OSL::ShaderGlobals *sg, bool derivatives,
	                         ustring object, TypeDesc type, ustring name,
	                         int index, void *val);
	bool get_attribute(OSL::ShaderGlobals *sg, bool derivatives, ustring object,
	                   TypeDesc type, ustring name, void *val);
	bool get_attribute(ShaderData *sd, bool derivatives, ustring object_name,
	                   TypeDesc type, ustring name, void *val);

	bool get_userdata(bool derivatives, ustring name, TypeDesc type,
	                  OSL::ShaderGlobals *sg, void *val);
	bool has_userdata(ustring name, TypeDesc type, OSL::ShaderGlobals *sg);

	int pointcloud_search(OSL::ShaderGlobals *sg, ustring filename, const OSL::Vec3 &center,
	                      float radius, int max_points, bool sort, size_t *out_indices,
	                      float *out_distances, int derivs_offset);

	int pointcloud_get(OSL::ShaderGlobals *sg, ustring filename, size_t *indices, int count,
	                   ustring attr_name, TypeDesc attr_type, void *out_data);

	bool pointcloud_write(OSL::ShaderGlobals *sg,
	                      ustring filename, const OSL::Vec3 &pos,
	                      int nattribs, const ustring *names,
	                      const TypeDesc *types,
	                      const void **data);

	bool trace(TraceOpt &options, OSL::ShaderGlobals *sg,
	           const OSL::Vec3 &P, const OSL::Vec3 &dPdx,
	           const OSL::Vec3 &dPdy, const OSL::Vec3 &R,
	           const OSL::Vec3 &dRdx, const OSL::Vec3 &dRdy);

	bool getmessage(OSL::ShaderGlobals *sg, ustring source, ustring name,
	                TypeDesc type, void *val, bool derivatives);

	bool texture(ustring filename, TextureOpt &options,
	             OSL::ShaderGlobals *sg,
	             float s, float t, float dsdx, float dtdx,
	             float dsdy, float dtdy, int nchannels, float *result);

	bool texture3d(ustring filename, TextureOpt &options,
	               OSL::ShaderGlobals *sg, const OSL::Vec3 &P,
	               const OSL::Vec3 &dPdx, const OSL::Vec3 &dPdy,
	               const OSL::Vec3 &dPdz, int nchannels, float *result);

	bool environment(ustring filename, TextureOpt &options,
	                 OSL::ShaderGlobals *sg, const OSL::Vec3 &R,
	                 const OSL::Vec3 &dRdx, const OSL::Vec3 &dRdy,
	                 int nchannels, float *result);

	bool get_texture_info(OSL::ShaderGlobals *sg, ustring filename, int subimage,
	                      ustring dataname, TypeDesc datatype, void *data);

	static bool get_background_attribute(KernelGlobals *kg, ShaderData *sd, ustring name,
	                                     TypeDesc type, bool derivatives, void *val);
	static bool get_object_standard_attribute(KernelGlobals *kg, ShaderData *sd, ustring name,
	                                          TypeDesc type, bool derivatives, void *val);

	static ustring u_distance;
	static ustring u_index;
	static ustring u_world;
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
	static ustring u_is_smooth;
	static ustring u_is_curve;
	static ustring u_curve_thickness;
	static ustring u_curve_tangent_normal;
	static ustring u_path_ray_length;
	static ustring u_path_ray_depth;
	static ustring u_path_transparent_depth;
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

	/* Code to make OSL versions transition smooth. */

#if OSL_LIBRARY_VERSION_CODE < 10600
	inline bool texture(ustring filename, TextureOpt &options,
	                    OSL::ShaderGlobals *sg,
	                    float s, float t, float dsdx, float dtdx,
	                    float dsdy, float dtdy, float *result)
	{
		return texture(filename, options, sg, s, t, dsdx, dtdx, dsdy, dtdy,
		               options.nchannels, result);
	}

	inline bool texture3d(ustring filename, TextureOpt &options,
	                      OSL::ShaderGlobals *sg, const OSL::Vec3 &P,
	                      const OSL::Vec3 &dPdx, const OSL::Vec3 &dPdy,
	                      const OSL::Vec3 &dPdz, float *result)
	{
		return texture3d(filename, options, sg, P, dPdx, dPdy, dPdz,
		                 options.nchannels, result);
	}

	inline bool environment(ustring filename, TextureOpt &options,
	                        OSL::ShaderGlobals *sg, const OSL::Vec3 &R,
	                        const OSL::Vec3 &dRdx, const OSL::Vec3 &dRdy,
	                        float *result)
	{
		return environment(filename, options, sg, R, dRdx, dRdy,
		                   options.nchannels, result);
	}
#endif

private:
	KernelGlobals *kernel_globals;
	OSL::TextureSystem *osl_ts;
#ifdef WITH_PTEX
	PtexCache *ptex_cache;
#endif
};

CCL_NAMESPACE_END

#endif /* __OSL_SERVICES_H__  */

