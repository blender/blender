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

#include "device.h"

#include "mesh.h"
#include "object.h"
#include "scene.h"
#include "shader.h"

#include "util_foreach.h"
#include "util_progress.h"

CCL_NAMESPACE_BEGIN

bool MeshManager::displace(Device *device, DeviceScene *dscene, Scene *scene, Mesh *mesh, Progress& progress)
{
	/* verify if we have a displacement shader */
	bool has_displacement = false;

	if(mesh->displacement_method != Mesh::DISPLACE_BUMP) {
		foreach(uint sindex, mesh->used_shaders)
			if(scene->shaders[sindex]->has_displacement)
				has_displacement = true;
	}
	
	if(!has_displacement)
		return false;

	string msg = string_printf("Computing Displacement %s", mesh->name.c_str());
	progress.set_status("Updating Mesh", msg);

	/* find object index. todo: is arbitrary */
	size_t object_index = ~0;

	for(size_t i = 0; i < scene->objects.size(); i++) {
		if(scene->objects[i]->mesh == mesh) {
			object_index = i;
			break;
		}
	}

	/* setup input for device task */
	vector<bool> done(mesh->verts.size(), false);
	device_vector<uint4> d_input;
	uint4 *d_input_data = d_input.resize(mesh->verts.size());
	size_t d_input_size = 0;

	for(size_t i = 0; i < mesh->triangles.size(); i++) {
		Mesh::Triangle t = mesh->triangles[i];
		Shader *shader = scene->shaders[mesh->shader[i]];

		if(!shader->has_displacement)
			continue;

		for(int j = 0; j < 3; j++) {
			if(done[t.v[j]])
				continue;

			done[t.v[j]] = true;

			/* set up object, primitive and barycentric coordinates */
			/* when used, non-instanced convention: object = ~object */
			int object = ~object_index;
			int prim = mesh->tri_offset + i;
			float u, v;
			
			switch (j) {
				case 0:
					u = 1.0f;
					v = 0.0f;
					break;
				case 1:
					u = 0.0f;
					v = 1.0f;
					break;
				default:
					u = 0.0f;
					v = 0.0f;
					break;
			}

			/* back */
			uint4 in = make_uint4(object, prim, __float_as_int(u), __float_as_int(v));
			d_input_data[d_input_size++] = in;
		}
	}

	if(d_input_size == 0)
		return false;
	
	/* run device task */
	device_vector<float4> d_output;
	d_output.resize(d_input_size);

	/* needs to be up to data for attribute access */
	device->const_copy_to("__data", &dscene->data, sizeof(dscene->data));

	device->mem_alloc(d_input, MEM_READ_ONLY);
	device->mem_copy_to(d_input);
	device->mem_alloc(d_output, MEM_WRITE_ONLY);

	DeviceTask task(DeviceTask::SHADER);
	task.shader_input = d_input.device_pointer;
	task.shader_output = d_output.device_pointer;
	task.shader_eval_type = SHADER_EVAL_DISPLACE;
	task.shader_x = 0;
	task.shader_w = d_output.size();

	device->task_add(task);
	device->task_wait();

	device->mem_copy_from(d_output, 0, 1, d_output.size(), sizeof(float4));
	device->mem_free(d_input);
	device->mem_free(d_output);

	if(progress.get_cancel())
		return false;

	/* read result */
	done.clear();
	done.resize(mesh->verts.size(), false);
	int k = 0;

	float4 *offset = (float4*)d_output.data_pointer;

	for(size_t i = 0; i < mesh->triangles.size(); i++) {
		Mesh::Triangle t = mesh->triangles[i];
		Shader *shader = scene->shaders[mesh->shader[i]];

		if(!shader->has_displacement)
			continue;

		for(int j = 0; j < 3; j++) {
			if(!done[t.v[j]]) {
				done[t.v[j]] = true;
				float3 off = float4_to_float3(offset[k++]);
				mesh->verts[t.v[j]] += off;
			}
		}
	}

	/* for displacement method both, we only need to recompute the face
	 * normals, as bump mapping in the shader will already alter the
	 * vertex normal, so we start from the non-displaced vertex normals
	 * to avoid applying the perturbation twice. */
	mesh->attributes.remove(ATTR_STD_FACE_NORMAL);
	mesh->add_face_normals();

	if(mesh->displacement_method == Mesh::DISPLACE_TRUE) {
		mesh->attributes.remove(ATTR_STD_VERTEX_NORMAL);
		mesh->add_vertex_normals();
	}

	return true;
}

CCL_NAMESPACE_END

