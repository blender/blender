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

#ifndef __SHADER_H__
#define __SHADER_H__

#include "attribute.h"
#include "kernel_types.h"

#include "util_map.h"
#include "util_param.h"
#include "util_string.h"
#include "util_types.h"

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
	   with and without bump,  because the displacement method is a mesh
	   level setting, so we need to handle both */
	ShaderGraph *graph_bump;

	/* sampling */
	bool sample_as_light;
	bool homogeneous_volume;

	/* synchronization */
	bool need_update;
	bool need_update_attributes;

	/* information about shader after compiling */
	bool has_surface;
	bool has_surface_emission;
	bool has_surface_transparent;
	bool has_volume;
	bool has_displacement;

	/* requested mesh attributes */
	AttributeRequestSet attributes;

	Shader();
	~Shader();

	void set_graph(ShaderGraph *graph);
	void tag_update(Scene *scene);
};

/* Shader Manager virtual base class
 * 
 * From this the SVM and OSL shader managers are derived, that do the actual
 * shader compiling and device updating. */

class ShaderManager {
public:
	bool need_update;

	static ShaderManager *create(Scene *scene);
	virtual ~ShaderManager();

	/* device update */
	virtual void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress) = 0;
	virtual void device_free(Device *device, DeviceScene *dscene) = 0;

	void device_update_common(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_free_common(Device *device, DeviceScene *dscene);

	/* get globally unique id for a type of attribute */
	uint get_attribute_id(ustring name);
	uint get_attribute_id(AttributeStandard std);

	/* get shader id for mesh faces */
	int get_shader_id(uint shader, Mesh *mesh = NULL, bool smooth = false);

	/* add default shaders to scene, to use as default for things that don't
	   have any shader assigned explicitly */
	static void add_default(Scene *scene);

protected:
	ShaderManager();

	typedef unordered_map<ustring, uint, ustringHash> AttributeIDMap;
	AttributeIDMap unique_attribute_id;
};

CCL_NAMESPACE_END

#endif /* __SHADER_H__ */

