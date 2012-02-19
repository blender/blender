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

#include "mesh.h"
#include "object.h"
#include "scene.h"

#include "blender_sync.h"
#include "blender_util.h"

#include "subd_mesh.h"
#include "subd_patch.h"
#include "subd_split.h"

#include "util_foreach.h"

CCL_NAMESPACE_BEGIN

/* Find/Add */

static bool mesh_need_attribute(Scene *scene, Mesh *mesh, Attribute::Standard std)
{
	if(std == Attribute::STD_NONE)
		return false;

	foreach(uint shader, mesh->used_shaders)
		if(scene->shaders[shader]->attributes.find(std))
			return true;
	
	return false;
}

static bool mesh_need_attribute(Scene *scene, Mesh *mesh, ustring name)
{
	if(name == ustring())
		return false;

	foreach(uint shader, mesh->used_shaders)
		if(scene->shaders[shader]->attributes.find(name))
			return true;
	
	return false;
}

static void create_mesh(Scene *scene, Mesh *mesh, BL::Mesh b_mesh, const vector<uint>& used_shaders)
{
	/* create vertices */
	BL::Mesh::vertices_iterator v;

	for(b_mesh.vertices.begin(v); v != b_mesh.vertices.end(); ++v)
		mesh->verts.push_back(get_float3(v->co()));

	/* create vertex normals */
	Attribute *attr_N = mesh->attributes.add(Attribute::STD_VERTEX_NORMAL);
	float3 *N = attr_N->data_float3();

	for(b_mesh.vertices.begin(v); v != b_mesh.vertices.end(); ++v, ++N)
		*N= get_float3(v->normal());

	/* create faces */
	BL::Mesh::faces_iterator f;
	vector<int> nverts;

	for(b_mesh.faces.begin(f); f != b_mesh.faces.end(); ++f) {
		int4 vi = get_int4(f->vertices_raw());
		int n = (vi[3] == 0)? 3: 4;
		int mi = clamp(f->material_index(), 0, used_shaders.size()-1);
		int shader = used_shaders[mi];
		bool smooth = f->use_smooth();

		mesh->add_triangle(vi[0], vi[1], vi[2], shader, smooth);

		if(n == 4)
			mesh->add_triangle(vi[0], vi[2], vi[3], shader, smooth);

		nverts.push_back(n);
	}

	/* create generated coordinates. todo: we should actually get the orco
	   coordinates from modifiers, for now we use texspace loc/size which
	   is available in the api. */
	if(mesh_need_attribute(scene, mesh, Attribute::STD_GENERATED)) {
		Attribute *attr = mesh->attributes.add(Attribute::STD_GENERATED);
		float3 loc = get_float3(b_mesh.texspace_location());
		float3 size = get_float3(b_mesh.texspace_size());

		if(size.x != 0.0f) size.x = 0.5f/size.x;
		if(size.y != 0.0f) size.y = 0.5f/size.y;
		if(size.z != 0.0f) size.z = 0.5f/size.z;

		loc = loc*size - make_float3(0.5f, 0.5f, 0.5f);

		float3 *fdata = attr->data_float3();
		BL::Mesh::vertices_iterator v;
		size_t i = 0;

		for(b_mesh.vertices.begin(v); v != b_mesh.vertices.end(); ++v)
			fdata[i++] = get_float3(v->co())*size - loc;
	}

	/* create vertex color attributes */
	{
		BL::Mesh::tessface_vertex_colors_iterator l;

		for(b_mesh.tessface_vertex_colors.begin(l); l != b_mesh.tessface_vertex_colors.end(); ++l) {
			if(!mesh_need_attribute(scene, mesh, ustring(l->name().c_str())))
				continue;

			Attribute *attr = mesh->attributes.add(
				ustring(l->name().c_str()), TypeDesc::TypeColor, Attribute::CORNER);

			BL::MeshColorLayer::data_iterator c;
			float3 *fdata = attr->data_float3();
			size_t i = 0;

			for(l->data.begin(c); c != l->data.end(); ++c, ++i) {
				fdata[0] = color_srgb_to_scene_linear(get_float3(c->color1()));
				fdata[1] = color_srgb_to_scene_linear(get_float3(c->color2()));
				fdata[2] = color_srgb_to_scene_linear(get_float3(c->color3()));

				if(nverts[i] == 4) {
					fdata[3] = fdata[0];
					fdata[4] = fdata[2];
					fdata[5] = color_srgb_to_scene_linear(get_float3(c->color4()));
					fdata += 6;
				}
				else
					fdata += 3;
			}
		}
	}

	/* create uv map attributes */
	{
		BL::Mesh::tessface_uv_textures_iterator l;

		for(b_mesh.tessface_uv_textures.begin(l); l != b_mesh.tessface_uv_textures.end(); ++l) {
			Attribute::Standard std = (l->active_render())? Attribute::STD_UV: Attribute::STD_NONE;
			ustring name = ustring(l->name().c_str());

			if(!(mesh_need_attribute(scene, mesh, name) || mesh_need_attribute(scene, mesh, std)))
				continue;

			Attribute *attr;

			if(l->active_render())
				attr = mesh->attributes.add(std, name);
			else
				attr = mesh->attributes.add(name, TypeDesc::TypePoint, Attribute::CORNER);

			BL::MeshTextureFaceLayer::data_iterator t;
			float3 *fdata = attr->data_float3();
			size_t i = 0;

			for(l->data.begin(t); t != l->data.end(); ++t, ++i) {
				fdata[0] =  get_float3(t->uv1());
				fdata[1] =  get_float3(t->uv2());
				fdata[2] =  get_float3(t->uv3());
				fdata += 3;

				if(nverts[i] == 4) {
					fdata[0] =  get_float3(t->uv1());
					fdata[1] =  get_float3(t->uv3());
					fdata[2] =  get_float3(t->uv4());
					fdata += 3;
				}
			}
		}
	}
}

static void create_subd_mesh(Mesh *mesh, BL::Mesh b_mesh, PointerRNA *cmesh, const vector<uint>& used_shaders)
{
	/* create subd mesh */
	SubdMesh sdmesh;

	/* create vertices */
	BL::Mesh::vertices_iterator v;

	for(b_mesh.vertices.begin(v); v != b_mesh.vertices.end(); ++v)
		sdmesh.add_vert(get_float3(v->co()));

	/* create faces */
	BL::Mesh::faces_iterator f;

	for(b_mesh.faces.begin(f); f != b_mesh.faces.end(); ++f) {
		int4 vi = get_int4(f->vertices_raw());
		int n= (vi[3] == 0)? 3: 4;
		//int shader = used_shaders[f->material_index()];

		if(n == 4)
			sdmesh.add_face(vi[0], vi[1], vi[2], vi[3]);
		/*else
			sdmesh.add_face(vi[0], vi[1], vi[2]);*/
	}

	/* finalize subd mesh */
	sdmesh.link_boundary();

	/* subdivide */
	DiagSplit dsplit;
	dsplit.camera = NULL;
	dsplit.dicing_rate = RNA_float_get(cmesh, "dicing_rate");

	sdmesh.tesselate(&dsplit, false, mesh, used_shaders[0], true);
}

/* Sync */

Mesh *BlenderSync::sync_mesh(BL::Object b_ob, bool object_updated)
{
	/* test if we can instance or if the object is modified */
	BL::ID b_ob_data = b_ob.data();
	BL::ID key = (object_is_modified(b_ob))? b_ob: b_ob_data;

	/* find shader indices */
	vector<uint> used_shaders;

	BL::Object::material_slots_iterator slot;
	for(b_ob.material_slots.begin(slot); slot != b_ob.material_slots.end(); ++slot) {
		BL::Material material_override = render_layers.front().material_override;

		if(material_override)
			find_shader(material_override, used_shaders, scene->default_surface);
		else
			find_shader(slot->material(), used_shaders, scene->default_surface);
	}

	if(used_shaders.size() == 0)
		used_shaders.push_back(scene->default_surface);
	
	/* test if we need to sync */
	Mesh *mesh;

	if(!mesh_map.sync(&mesh, key)) {
		/* if transform was applied to mesh, need full update */
		if(object_updated && mesh->transform_applied);
		/* test if shaders changed, these can be object level so mesh
		   does not get tagged for recalc */
		else if(mesh->used_shaders != used_shaders);
		else {
			/* even if not tagged for recalc, we may need to sync anyway
			 * because the shader needs different mesh attributes */
			bool attribute_recalc = false;

			foreach(uint shader, mesh->used_shaders)
				if(scene->shaders[shader]->need_update_attributes)
					attribute_recalc = true;

			if(!attribute_recalc)
				return mesh;
		}
	}

	/* ensure we only sync instanced meshes once */
	if(mesh_synced.find(mesh) != mesh_synced.end())
		return mesh;
	
	mesh_synced.insert(mesh);

	/* create derived mesh */
	BL::Mesh b_mesh = object_to_mesh(b_ob, b_scene, true, !preview);
	PointerRNA cmesh = RNA_pointer_get(&b_ob_data.ptr, "cycles");

	vector<Mesh::Triangle> oldtriangle = mesh->triangles;

	mesh->clear();
	mesh->used_shaders = used_shaders;
	mesh->name = ustring(b_ob_data.name().c_str());

	if(b_mesh) {
		if(cmesh.data && experimental && RNA_boolean_get(&cmesh, "use_subdivision"))
			create_subd_mesh(mesh, b_mesh, &cmesh, used_shaders);
		else
			create_mesh(scene, mesh, b_mesh, used_shaders);

		/* free derived mesh */
		object_remove_mesh(b_data, b_mesh);
	}

	/* displacement method */
	if(cmesh.data) {
		int method = RNA_enum_get(&cmesh, "displacement_method");

		if(method == 0 || !experimental)
			mesh->displacement_method = Mesh::DISPLACE_BUMP;
		else if(method == 1)
			mesh->displacement_method = Mesh::DISPLACE_TRUE;
		else
			mesh->displacement_method = Mesh::DISPLACE_BOTH;
	}

	/* tag update */
	bool rebuild = false;

	if(oldtriangle.size() != mesh->triangles.size())
		rebuild = true;
	else if(oldtriangle.size()) {
		if(memcmp(&oldtriangle[0], &mesh->triangles[0], sizeof(Mesh::Triangle)*oldtriangle.size()) != 0)
			rebuild = true;
	}
	
	mesh->tag_update(scene, rebuild);

	return mesh;
}

CCL_NAMESPACE_END

