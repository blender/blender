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

#include "device.h"

#include "mesh.h"
#include "scene.h"
#include "shader.h"

#include "util_foreach.h"
#include "util_progress.h"

CCL_NAMESPACE_BEGIN

bool MeshManager::displace(Device *device, Scene *scene, Mesh *mesh, Progress& progress)
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

	/* setup input for device task */
	vector<bool> done(mesh->verts.size(), false);
	device_vector<uint4> d_input;
	uint4 *d_input_data = d_input.resize(mesh->verts.size());
	size_t d_input_offset = 0;

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
			/* when used, non-instanced convention: object = -object-1; */
			int object = ~0; /* todo */
			int prim = mesh->tri_offset + i;
			float u, v;

			if(j == 0) {
				u = 1.0f;
				v = 0.0f;
			}
			else if(j == 1) {
				u = 0.0f;
				v = 1.0f;
			}
			else {
				u = 0.0f;
				v = 0.0f;
			}

			/* back */
			uint4 in = make_uint4(object, prim, __float_as_int(u), __float_as_int(v));
			d_input_data[d_input_offset++] = in;
		}
	}

	if(d_input_offset == 0)
		return false;
	
	/* run device task */
	device_vector<float3> d_output;
	d_output.resize(d_input.size());

	device->mem_alloc(d_input, MEM_READ_ONLY);
	device->mem_copy_to(d_input);
	device->mem_alloc(d_output, MEM_WRITE_ONLY);

	DeviceTask task(DeviceTask::SHADER);
	task.shader_input = d_input.device_pointer;
	task.shader_output = d_output.device_pointer;
	task.shader_eval_type = SHADER_EVAL_DISPLACE;
	task.shader_x = 0;
	task.shader_w = d_input.size();

	device->task_add(task);
	device->task_wait();

	device->mem_copy_from(d_output, 0, 1, d_output.size(), sizeof(float3));
	device->mem_free(d_input);
	device->mem_free(d_output);

	if(progress.get_cancel())
		return false;

	/* read result */
	done.clear();
	done.resize(mesh->verts.size(), false);
	int k = 0;

	float3 *offset = (float3*)d_output.data_pointer;

	for(size_t i = 0; i < mesh->triangles.size(); i++) {
		Mesh::Triangle t = mesh->triangles[i];
		Shader *shader = scene->shaders[mesh->shader[i]];

		if(!shader->has_displacement)
			continue;

		for(int j = 0; j < 3; j++) {
			if(!done[t.v[j]]) {
				done[t.v[j]] = true;
				mesh->verts[t.v[j]] += offset[k++];
			}
		}
	}

	/* for displacement method both, we only need to recompute the face
	 * normals, as bump mapping in the shader will already alter the
	 * vertex normal, so we start from the non-displaced vertex normals
	 * to avoid applying the perturbation twice. */
	mesh->attributes.remove(Attribute::STD_FACE_NORMAL);
	mesh->add_face_normals();

	if(mesh->displacement_method == Mesh::DISPLACE_TRUE) {
		mesh->attributes.remove(Attribute::STD_VERTEX_NORMAL);
		mesh->add_vertex_normals();
	}

	return true;
}

CCL_NAMESPACE_END

