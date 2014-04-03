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

#ifndef __SHADER_H__
#define __SHADER_H__

#include "attribute.h"
#include "kernel_types.h"

#include "util_map.h"
#include "util_param.h"
#include "util_string.h"
#include "util_types.h"

#ifdef WITH_OSL
#include <OSL/oslexec.h>
#endif

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Mesh;
class Progress;
class Scene;
class ShaderGraph;
struct float3;

/* Shader describing the appearance of a Mesh, Light or Background.
 *
 * While there is only a single shader graph, it has three outputs: surface,
 * volume and displacement, that the shader manager will compile and execute
 * separately. */

class Shader {
public:
	/* name */
	string name;
	int pass_id;

	/* shader graph */
	ShaderGraph *graph;

	/* shader graph with auto bump mapping included, we compile two shaders,
	 * with and without bump,  because the displacement method is a mesh
	 * level setting, so we need to handle both */
	ShaderGraph *graph_bump;

	/* sampling */
	bool use_mis;
	bool use_transparent_shadow;
	bool heterogeneous_volume;

	/* synchronization */
	bool need_update;
	bool need_update_attributes;

	/* information about shader after compiling */
	bool has_surface;
	bool has_surface_emission;
	bool has_surface_transparent;
	bool has_volume;
	bool has_displacement;
	bool has_surface_bssrdf;
	bool has_converter_blackbody;
	bool has_bssrdf_bump;
	bool has_heterogeneous_volume;

	/* requested mesh attributes */
	AttributeRequestSet attributes;

	/* determined before compiling */
	bool used;

#ifdef WITH_OSL
	/* osl shading state references */
	OSL::ShadingAttribStateRef osl_surface_ref;
	OSL::ShadingAttribStateRef osl_surface_bump_ref;
	OSL::ShadingAttribStateRef osl_volume_ref;
	OSL::ShadingAttribStateRef osl_displacement_ref;
#endif

	Shader();
	~Shader();

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
	int get_shader_id(uint shader, Mesh *mesh = NULL, bool smooth = false);

	/* add default shaders to scene, to use as default for things that don't
	 * have any shader assigned explicitly */
	static void add_default(Scene *scene);

protected:
	ShaderManager();

	typedef unordered_map<ustring, uint, ustringHash> AttributeIDMap;
	AttributeIDMap unique_attribute_id;

	size_t blackbody_table_offset;
};

CCL_NAMESPACE_END

#endif /* __SHADER_H__ */

