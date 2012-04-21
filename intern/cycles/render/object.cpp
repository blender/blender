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
#include "light.h"
#include "mesh.h"
#include "object.h"
#include "scene.h"

#include "util_foreach.h"
#include "util_map.h"
#include "util_progress.h"

CCL_NAMESPACE_BEGIN

/* Object */

Object::Object()
{
	name = "";
	mesh = NULL;
	tfm = transform_identity();
	visibility = ~0;
	pass_id = 0;
}

Object::~Object()
{
}

void Object::compute_bounds()
{
	bounds = mesh->bounds.transformed(&tfm);
}

void Object::apply_transform()
{
	if(!mesh || tfm == transform_identity())
		return;
	
	for(size_t i = 0; i < mesh->verts.size(); i++)
		mesh->verts[i] = transform_point(&tfm, mesh->verts[i]);

	Attribute *attr_fN = mesh->attributes.find(Attribute::STD_FACE_NORMAL);
	Attribute *attr_vN = mesh->attributes.find(Attribute::STD_VERTEX_NORMAL);

	Transform ntfm = transform_transpose(transform_inverse(tfm));

	/* we keep normals pointing in same direction on negative scale, notify
	   mesh about this in it (re)calculates normals */
	if(transform_negative_scale(tfm))
		mesh->transform_negative_scaled = true;

	if(attr_fN) {
		float3 *fN = attr_fN->data_float3();

		for(size_t i = 0; i < mesh->triangles.size(); i++)
			fN[i] = transform_direction(&ntfm, fN[i]);
	}

	if(attr_vN) {
		float3 *vN = attr_vN->data_float3();

		for(size_t i = 0; i < mesh->verts.size(); i++)
			vN[i] = transform_direction(&ntfm, vN[i]);
	}

	if(bounds.valid()) {
		mesh->compute_bounds();
		compute_bounds();
	}
	
	tfm = transform_identity();
}

void Object::tag_update(Scene *scene)
{
	if(mesh) {
		if(mesh->transform_applied)
			mesh->need_update = true;

		foreach(uint sindex, mesh->used_shaders) {
			Shader *shader = scene->shaders[sindex];

			if(shader->sample_as_light && shader->has_surface_emission)
				scene->light_manager->need_update = true;
		}
	}

	scene->mesh_manager->need_update = true;
	scene->object_manager->need_update = true;
}

/* Object Manager */

ObjectManager::ObjectManager()
{
	need_update = true;
}

ObjectManager::~ObjectManager()
{
}

void ObjectManager::device_update_transforms(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	float4 *objects = dscene->objects.resize(OBJECT_SIZE*scene->objects.size());
	int i = 0;
	map<Mesh*, float> surface_area_map;

	foreach(Object *ob, scene->objects) {
		Mesh *mesh = ob->mesh;

		/* compute transformations */
		Transform tfm = ob->tfm;
		Transform itfm = transform_inverse(tfm);
		Transform ntfm = transform_transpose(itfm);

		/* compute surface area. for uniform scale we can do avoid the many
		   transform calls and share computation for instances */
		/* todo: correct for displacement, and move to a better place */
		float uniform_scale;
		float surface_area = 0.0f;
		float pass_id = ob->pass_id;
		
		if(transform_uniform_scale(tfm, uniform_scale)) {
			map<Mesh*, float>::iterator it = surface_area_map.find(mesh);

			if(it == surface_area_map.end()) {
				foreach(Mesh::Triangle& t, mesh->triangles) {
					float3 p1 = mesh->verts[t.v[0]];
					float3 p2 = mesh->verts[t.v[1]];
					float3 p3 = mesh->verts[t.v[2]];

					surface_area += triangle_area(p1, p2, p3);
				}

				surface_area_map[mesh] = surface_area;
			}
			else
				surface_area = it->second;

			surface_area *= uniform_scale;
		}
		else {
			foreach(Mesh::Triangle& t, mesh->triangles) {
				float3 p1 = transform_point(&tfm, mesh->verts[t.v[0]]);
				float3 p2 = transform_point(&tfm, mesh->verts[t.v[1]]);
				float3 p3 = transform_point(&tfm, mesh->verts[t.v[2]]);

				surface_area += triangle_area(p1, p2, p3);
			}
		}

		/* pack in texture */
		int offset = i*OBJECT_SIZE;

		memcpy(&objects[offset], &tfm, sizeof(float4)*4);
		memcpy(&objects[offset+4], &itfm, sizeof(float4)*4);
		memcpy(&objects[offset+8], &ntfm, sizeof(float4)*4);
		objects[offset+12] = make_float4(surface_area, pass_id, 0.0f, 0.0f);

		i++;

		if(progress.get_cancel()) return;
	}

	device->tex_alloc("__objects", dscene->objects);
}

void ObjectManager::device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	if(!need_update)
		return;
	
	device_free(device, dscene);

	if(scene->objects.size() == 0)
		return;

	/* set object transform matrices, before applying static transforms */
	progress.set_status("Updating Objects", "Copying Transformations to device");
	device_update_transforms(device, dscene, scene, progress);

	if(progress.get_cancel()) return;

	/* prepare for static BVH building */
	/* todo: do before to support getting object level coords? */
	if(scene->params.bvh_type == SceneParams::BVH_STATIC) {
		progress.set_status("Updating Objects", "Applying Static Transformations");
		apply_static_transforms(scene, progress);
	}

	if(progress.get_cancel()) return;

	need_update = false;
}

void ObjectManager::device_free(Device *device, DeviceScene *dscene)
{
	device->tex_free(dscene->objects);
	dscene->objects.clear();
}

void ObjectManager::apply_static_transforms(Scene *scene, Progress& progress)
{
	/* todo: normals and displacement should be done before applying transform! */
	/* todo: create objects/meshes in right order! */

	/* counter mesh users */
	map<Mesh*, int> mesh_users;

	foreach(Object *object, scene->objects) {
		map<Mesh*, int>::iterator it = mesh_users.find(object->mesh);

		if(it == mesh_users.end())
			mesh_users[object->mesh] = 1;
		else
			it->second++;
	}

	if(progress.get_cancel()) return;

	/* apply transforms for objects with single user meshes */
	foreach(Object *object, scene->objects) {
		if(mesh_users[object->mesh] == 1) {
			if(!object->mesh->transform_applied) {
				object->apply_transform();
				object->mesh->transform_applied = true;
			}

			if(progress.get_cancel()) return;
		}
	}
}

void ObjectManager::tag_update(Scene *scene)
{
	need_update = true;
	scene->mesh_manager->need_update = true;
	scene->light_manager->need_update = true;
}

CCL_NAMESPACE_END

