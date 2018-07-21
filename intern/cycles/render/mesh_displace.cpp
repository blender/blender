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

#include "device/device.h"

#include "render/mesh.h"
#include "render/object.h"
#include "render/scene.h"
#include "render/shader.h"

#include "util/util_foreach.h"
#include "util/util_progress.h"

CCL_NAMESPACE_BEGIN

static float3 compute_face_normal(const Mesh::Triangle& t, float3 *verts)
{
	float3 v0 = verts[t.v[0]];
	float3 v1 = verts[t.v[1]];
	float3 v2 = verts[t.v[2]];

	float3 norm = cross(v1 - v0, v2 - v0);
	float normlen = len(norm);

	if(normlen == 0.0f)
		return make_float3(1.0f, 0.0f, 0.0f);

	return norm / normlen;
}

bool MeshManager::displace(Device *device, DeviceScene *dscene, Scene *scene, Mesh *mesh, Progress& progress)
{
	/* verify if we have a displacement shader */
	if(!mesh->has_true_displacement()) {
		return false;
	}

	string msg = string_printf("Computing Displacement %s", mesh->name.c_str());
	progress.set_status("Updating Mesh", msg);

	/* find object index. todo: is arbitrary */
	size_t object_index = OBJECT_NONE;

	for(size_t i = 0; i < scene->objects.size(); i++) {
		if(scene->objects[i]->mesh == mesh) {
			object_index = i;
			break;
		}
	}

	/* setup input for device task */
	const size_t num_verts = mesh->verts.size();
	vector<bool> done(num_verts, false);
	device_vector<uint4> d_input(device, "displace_input", MEM_READ_ONLY);
	uint4 *d_input_data = d_input.alloc(num_verts);
	size_t d_input_size = 0;

	size_t num_triangles = mesh->num_triangles();
	for(size_t i = 0; i < num_triangles; i++) {
		Mesh::Triangle t = mesh->get_triangle(i);
		int shader_index = mesh->shader[i];
		Shader *shader = (shader_index < mesh->used_shaders.size()) ?
			mesh->used_shaders[shader_index] : scene->default_surface;

		if(!shader->has_displacement || shader->displacement_method == DISPLACE_BUMP) {
			continue;
		}

		for(int j = 0; j < 3; j++) {
			if(done[t.v[j]])
				continue;

			done[t.v[j]] = true;

			/* set up object, primitive and barycentric coordinates */
			int object = object_index;
			int prim = mesh->tri_offset + i;
			float u, v;

			switch(j) {
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
	device_vector<float4> d_output(device, "displace_output", MEM_READ_WRITE);
	d_output.alloc(d_input_size);
	d_output.zero_to_device();
	d_input.copy_to_device();

	/* needs to be up to data for attribute access */
	device->const_copy_to("__data", &dscene->data, sizeof(dscene->data));

	DeviceTask task(DeviceTask::SHADER);
	task.shader_input = d_input.device_pointer;
	task.shader_output = d_output.device_pointer;
	task.shader_eval_type = SHADER_EVAL_DISPLACE;
	task.shader_x = 0;
	task.shader_w = d_output.size();
	task.num_samples = 1;
	task.get_cancel = function_bind(&Progress::get_cancel, &progress);

	device->task_add(task);
	device->task_wait();

	if(progress.get_cancel()) {
		d_input.free();
		d_output.free();
		return false;
	}

	d_output.copy_from_device(0, 1, d_output.size());
	d_input.free();

	/* read result */
	done.clear();
	done.resize(num_verts, false);
	int k = 0;

	float4 *offset = d_output.data();

	Attribute *attr_mP = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
	for(size_t i = 0; i < num_triangles; i++) {
		Mesh::Triangle t = mesh->get_triangle(i);
		int shader_index = mesh->shader[i];
		Shader *shader = (shader_index < mesh->used_shaders.size()) ?
			mesh->used_shaders[shader_index] : scene->default_surface;

		if(!shader->has_displacement || shader->displacement_method == DISPLACE_BUMP) {
			continue;
		}

		for(int j = 0; j < 3; j++) {
			if(!done[t.v[j]]) {
				done[t.v[j]] = true;
				float3 off = float4_to_float3(offset[k++]);
				/* Avoid illegal vertex coordinates. */
				off = ensure_finite3(off);
				mesh->verts[t.v[j]] += off;
				if(attr_mP != NULL) {
					for(int step = 0; step < mesh->motion_steps - 1; step++) {
						float3 *mP = attr_mP->data_float3() + step*num_verts;
						mP[t.v[j]] += off;
					}
				}
			}
		}
	}

	d_output.free();

	/* for displacement method both, we only need to recompute the face
	 * normals, as bump mapping in the shader will already alter the
	 * vertex normal, so we start from the non-displaced vertex normals
	 * to avoid applying the perturbation twice. */
	mesh->attributes.remove(ATTR_STD_FACE_NORMAL);
	mesh->add_face_normals();

	bool need_recompute_vertex_normals = false;

	foreach(Shader *shader, mesh->used_shaders) {
		if(shader->has_displacement && shader->displacement_method == DISPLACE_TRUE) {
			need_recompute_vertex_normals = true;
			break;
		}
	}

	if(need_recompute_vertex_normals) {
		bool flip = mesh->transform_negative_scaled;
		vector<bool> tri_has_true_disp(num_triangles, false);

		for(size_t i = 0; i < num_triangles; i++) {
			int shader_index = mesh->shader[i];
			Shader *shader = (shader_index < mesh->used_shaders.size()) ?
				mesh->used_shaders[shader_index] : scene->default_surface;

			tri_has_true_disp[i] = shader->has_displacement && shader->displacement_method == DISPLACE_TRUE;
		}

		/* static vertex normals */

		/* get attributes */
		Attribute *attr_fN = mesh->attributes.find(ATTR_STD_FACE_NORMAL);
		Attribute *attr_vN = mesh->attributes.find(ATTR_STD_VERTEX_NORMAL);

		float3 *fN = attr_fN->data_float3();
		float3 *vN = attr_vN->data_float3();

		/* compute vertex normals */

		/* zero vertex normals on triangles with true displacement */
		for(size_t i = 0; i < num_triangles; i++) {
			if(tri_has_true_disp[i]) {
				for(size_t j = 0; j < 3; j++) {
					vN[mesh->get_triangle(i).v[j]] = make_float3(0.0f, 0.0f, 0.0f);
				}
			}
		}

		/* add face normals to vertex normals */
		for(size_t i = 0; i < num_triangles; i++) {
			if(tri_has_true_disp[i]) {
				for(size_t j = 0; j < 3; j++) {
					vN[mesh->get_triangle(i).v[j]] += fN[i];
				}
			}
		}

		/* normalize vertex normals */
		done.clear();
		done.resize(num_verts, false);

		for(size_t i = 0; i < num_triangles; i++) {
			if(tri_has_true_disp[i]) {
				for(size_t j = 0; j < 3; j++) {
					int vert = mesh->get_triangle(i).v[j];

					if(done[vert]) {
						continue;
					}

					vN[vert] = normalize(vN[vert]);
					if(flip)
						vN[vert] = -vN[vert];

					done[vert] = true;
				}
			}
		}

		/* motion vertex normals */
		Attribute *attr_mP = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
		Attribute *attr_mN = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_NORMAL);

		if(mesh->has_motion_blur() && attr_mP && attr_mN) {
			for(int step = 0; step < mesh->motion_steps - 1; step++) {
				float3 *mP = attr_mP->data_float3() + step*mesh->verts.size();
				float3 *mN = attr_mN->data_float3() + step*mesh->verts.size();

				/* compute */

				/* zero vertex normals on triangles with true displacement */
				for(size_t i = 0; i < num_triangles; i++) {
					if(tri_has_true_disp[i]) {
						for(size_t j = 0; j < 3; j++) {
							mN[mesh->get_triangle(i).v[j]] = make_float3(0.0f, 0.0f, 0.0f);
						}
					}
				}

				/* add face normals to vertex normals */
				for(size_t i = 0; i < num_triangles; i++) {
					if(tri_has_true_disp[i]) {
						for(size_t j = 0; j < 3; j++) {
							float3 fN = compute_face_normal(mesh->get_triangle(i), mP);
							mN[mesh->get_triangle(i).v[j]] += fN;
						}
					}
				}

				/* normalize vertex normals */
				done.clear();
				done.resize(num_verts, false);

				for(size_t i = 0; i < num_triangles; i++) {
					if(tri_has_true_disp[i]) {
						for(size_t j = 0; j < 3; j++) {
							int vert = mesh->get_triangle(i).v[j];

							if(done[vert]) {
								continue;
							}

							mN[vert] = normalize(mN[vert]);
							if(flip)
								mN[vert] = -mN[vert];

							done[vert] = true;
						}
					}
				}
			}
		}
	}

	return true;
}

CCL_NAMESPACE_END
