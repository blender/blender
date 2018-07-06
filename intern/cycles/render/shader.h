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

#ifndef __SHADER_H__
#define __SHADER_H__

#ifdef WITH_OSL
/* So no context pollution happens from indirectly included windows.h */
#  include "util/util_windows.h"
#  include <OSL/oslexec.h>
#endif

#include "render/attribute.h"
#include "kernel/kernel_types.h"

#include "graph/node.h"

#include "util/util_map.h"
#include "util/util_param.h"
#include "util/util_string.h"
#include "util/util_thread.h"
#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class DeviceRequestedFeatures;
class Mesh;
class Progress;
class Scene;
class ShaderGraph;
struct float3;

enum ShadingSystem {
	SHADINGSYSTEM_OSL,
	SHADINGSYSTEM_SVM
};

/* Keep those in sync with the python-defined enum. */
enum VolumeSampling {
	VOLUME_SAMPLING_DISTANCE = 0,
	VOLUME_SAMPLING_EQUIANGULAR = 1,
	VOLUME_SAMPLING_MULTIPLE_IMPORTANCE = 2,

	VOLUME_NUM_SAMPLING,
};

enum VolumeInterpolation {
	VOLUME_INTERPOLATION_LINEAR = 0,
	VOLUME_INTERPOLATION_CUBIC = 1,

	VOLUME_NUM_INTERPOLATION,
};

enum DisplacementMethod {
	DISPLACE_BUMP = 0,
	DISPLACE_TRUE = 1,
	DISPLACE_BOTH = 2,

	DISPLACE_NUM_METHODS,
};

/* Shader describing the appearance of a Mesh, Light or Background.
 *
 * While there is only a single shader graph, it has three outputs: surface,
 * volume and displacement, that the shader manager will compile and execute
 * separately. */

class Shader : public Node {
public:
	NODE_DECLARE

	int pass_id;

	/* shader graph */
	ShaderGraph *graph;

	/* sampling */
	bool use_mis;
	bool use_transparent_shadow;
	bool heterogeneous_volume;
	VolumeSampling volume_sampling_method;
	int volume_interpolation_method;

	/* synchronization */
	bool need_update;
	bool need_update_mesh;
	bool need_sync_object;

	/* If the shader has only volume components, the surface is assumed to
	 * be transparent.
	 * However, graph optimization might remove the volume subgraph, but
	 * since the user connected something to the volume output the surface
	 * should still be transparent.
	 * Therefore, has_volume_connected stores whether some volume subtree
	 * was connected before optimization. */
	bool has_volume_connected;

	/* information about shader after compiling */
	bool has_surface;
	bool has_surface_emission;
	bool has_surface_transparent;
	bool has_volume;
	bool has_displacement;
	bool has_surface_bssrdf;
	bool has_bump;
	bool has_bssrdf_bump;
	bool has_surface_spatial_varying;
	bool has_volume_spatial_varying;
	bool has_object_dependency;
	bool has_attribute_dependency;
	bool has_integrator_dependency;

	/* displacement */
	DisplacementMethod displacement_method;

	/* requested mesh attributes */
	AttributeRequestSet attributes;

	/* determined before compiling */
	uint id;
	bool used;

#ifdef WITH_OSL
	/* osl shading state references */
	OSL::ShaderGroupRef osl_surface_ref;
	OSL::ShaderGroupRef osl_surface_bump_ref;
	OSL::ShaderGroupRef osl_volume_ref;
	OSL::ShaderGroupRef osl_displacement_ref;
#endif

	Shader();
	~Shader();

	/* Checks whether the shader consists of just a emission node with fixed inputs that's connected directly to the output.
	 * If yes, it sets the content of emission to the constant value (color * strength), which is then used for speeding up light evaluation. */
	bool is_constant_emission(float3* emission);

	void set_graph(ShaderGraph *graph);
	void tag_update(Scene *scene);
	void tag_used(Scene *scene);
};

/* Shader Manager virtual base class
 *
 * From this the SVM and OSL shader managers are derived, that do the actual
 * shader compiling and device updating. */

class ShaderManager {
public:
	bool need_update;

	static ShaderManager *create(Scene *scene, int shadingsystem);
	virtual ~ShaderManager();

	virtual void reset(Scene *scene) = 0;

	virtual bool use_osl() { return false; }

	/* device update */
	virtual void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress) = 0;
	virtual void device_free(Device *device, DeviceScene *dscene, Scene *scene) = 0;

	void device_update_shaders_used(Scene *scene);
	void device_update_common(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_free_common(Device *device, DeviceScene *dscene, Scene *scene);

	/* get globally unique id for a type of attribute */
	uint get_attribute_id(ustring name);
	uint get_attribute_id(AttributeStandard std);

	/* get shader id for mesh faces */
	int get_shader_id(Shader *shader, bool smooth = false);

	/* add default shaders to scene, to use as default for things that don't
	 * have any shader assigned explicitly */
	static void add_default(Scene *scene);

	/* Selective nodes compilation. */
	void get_requested_features(Scene *scene,
	                            DeviceRequestedFeatures *requested_features);

	static void free_memory();

	float linear_rgb_to_gray(float3 c);

protected:
	ShaderManager();

	typedef unordered_map<ustring, uint, ustringHash> AttributeIDMap;
	AttributeIDMap unique_attribute_id;

	static thread_mutex lookup_table_mutex;
	static vector<float> beckmann_table;
	static bool beckmann_table_ready;

	size_t beckmann_table_offset;

	void get_requested_graph_features(ShaderGraph *graph,
	                                  DeviceRequestedFeatures *requested_features);

	thread_spin_lock attribute_lock_;

	float3 xyz_to_r;
	float3 xyz_to_g;
	float3 xyz_to_b;
	float3 rgb_to_y;
};

CCL_NAMESPACE_END

#endif /* __SHADER_H__ */
