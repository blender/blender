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

static float3 tri_calc_tangent(float3 v0, float3 v1, float3 v2, float3 tx0, float3 tx1, float3 tx2)
{
	float3 duv1 = tx2 - tx0;
	float3 duv2 = tx2 - tx1;
	float3 dp1 = v2 - v0;
	float3 dp2 = v2 - v1;
	float det = duv1[0] * duv2[1] - duv1[1] * duv2[0];

	if(det != 0.0f) {
		return normalize(dp1 * duv2[1] - dp2 * duv1[1]);
	}
	else {
		/* give back a sane default, using a valid edge as a fallback */
		float3 edge = v1 - v0;

		if(len(edge) == 0.0f)
			edge = v2 - v0;

		return normalize(edge);
	}
}

static void create_mesh(Scene *scene, Mesh *mesh, BL::Mesh b_mesh, const vector<uint>& used_shaders)
{
	/* create vertices */
	BL::Mesh::vertices_iterator v;

	for(b_mesh.vertices.begin(v); v != b_mesh.vertices.end(); ++v)
		mesh->verts.push_back(get_float3(v->co()));

	/* create vertex normals */
	Attribute *attr_N = mesh->attributes.add(ATTR_STD_VERTEX_NORMAL);
	float3 *N = attr_N->data_float3();

	for(b_mesh.vertices.begin(v); v != b_mesh.vertices.end(); ++v, ++N)
		*N = get_float3(v->normal());

	/* create faces */
	BL::Mesh::tessfaces_iterator f;
	vector<int> nverts;

	for(b_mesh.tessfaces.begin(f); f != b_mesh.tessfaces.end(); ++f) {
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
	 * coordinates from modifiers, for now we use texspace loc/size which
	 * is available in the api. */
	if(mesh->need_attribute(scene, ATTR_STD_GENERATED)) {
		Attribute *attr = mesh->attributes.add(ATTR_STD_GENERATED);
		float3 loc = get_float3(b_mesh.texspace_location());
		float3 size = get_float3(b_mesh.texspace_size());

		if(size.x != 0.0f) size.x = 0.5f/size.x;
		if(size.y != 0.0f) size.y = 0.5f/size.y;
		if(size.z != 0.0f) size.z = 0.5f/size.z;

		loc = loc*size - make_float3(0.5f, 0.5f, 0.5f);

		float3 *fdata = attr->data_float3();
		size_t i = 0;

		for(b_mesh.vertices.begin(v); v != b_mesh.vertices.end(); ++v)
			fdata[i++] = get_float3(v->co())*size - loc;
	}

	/* create vertex color attributes */
	{
		BL::Mesh::tessface_vertex_colors_iterator l;

		for(b_mesh.tessface_vertex_colors.begin(l); l != b_mesh.tessface_vertex_colors.end(); ++l) {
			if(!mesh->need_attribute(scene, ustring(l->name().c_str())))
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
			AttributeStandard std = (l->active_render())? ATTR_STD_UV: ATTR_STD_NONE;
			ustring name = ustring(l->name().c_str());

			if(!(mesh->need_attribute(scene, name) || mesh->need_attribute(scene, std)))
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

	/* create texcoord-based tangent attributes */
	{
		BL::Mesh::tessface_uv_textures_iterator l;

		for(b_mesh.tessface_uv_textures.begin(l); l != b_mesh.tessface_uv_textures.end(); ++l) {
			AttributeStandard std = (l->active_render())? ATTR_STD_TANGENT: ATTR_STD_NONE;

			if(!mesh->need_attribute(scene, std))
				continue;

			Attribute *attr = mesh->attributes.add(std, ustring("Tangent"));

			/* compute average tangents per vertex */
			float3 *tangents = attr->data_float3();
			memset(tangents, 0, sizeof(float3)*mesh->verts.size());

			BL::MeshTextureFaceLayer::data_iterator t;

			size_t fi = 0; /* face index */
			b_mesh.tessfaces.begin(f);
			for(l->data.begin(t); t != l->data.end() && f != b_mesh.tessfaces.end(); ++t, ++fi, ++f) {
				int4 vi = get_int4(f->vertices_raw());

				float3 tx0 = get_float3(t->uv1());
				float3 tx1 = get_float3(t->uv2());
				float3 tx2 = get_float3(t->uv3());

				float3 v0 = mesh->verts[vi[0]];
				float3 v1 = mesh->verts[vi[1]];
				float3 v2 = mesh->verts[vi[2]];

				/* calculate tangent for the triangle;
				 * get vertex positions, and find change in position with respect
				 * to the texture coords in the first texture coord dimension */
				float3 tangent0 = tri_calc_tangent(v0, v1, v2, tx0, tx1, tx2);

				if(nverts[fi] == 4) {
					/* quad tangent */
					float3 tx3 = get_float3(t->uv4());
					float3 v3 = mesh->verts[vi[3]];
					float3 tangent1 = tri_calc_tangent(v0, v2, v3, tx0, tx2, tx3);

					tangents[vi[0]] += 0.5f*(tangent0 + tangent1);
					tangents[vi[1]] += tangent0;
					tangents[vi[2]] += 0.5f*(tangent0 + tangent1);
					tangents[vi[3]] += tangent1;
				}
				else {
					/* triangle tangent */
					tangents[vi[0]] += tangent0;
					tangents[vi[1]] += tangent0;
					tangents[vi[2]] += tangent0;
				}
			}

			/* normalize tangent vectors */
			for(int i = 0; i < mesh->verts.size(); i++)
				tangents[i] = normalize(tangents[i]);
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
	BL::Mesh::tessfaces_iterator f;

	for(b_mesh.tessfaces.begin(f); f != b_mesh.tessfaces.end(); ++f) {
		int4 vi = get_int4(f->vertices_raw());
		int n = (vi[3] == 0) ? 3: 4;
		//int shader = used_shaders[f->material_index()];

		if(n == 4)
			sdmesh.add_face(vi[0], vi[1], vi[2], vi[3]);
#if 0
		else
			sdmesh.add_face(vi[0], vi[1], vi[2]);
#endif
	}

	/* finalize subd mesh */
	sdmesh.link_boundary();

	/* subdivide */
	DiagSplit dsplit;
	dsplit.camera = NULL;
	dsplit.dicing_rate = RNA_float_get(cmesh, "dicing_rate");

	sdmesh.tessellate(&dsplit, false, mesh, used_shaders[0], true);
}

/* Sync */

Mesh *BlenderSync::sync_mesh(BL::Object b_ob, bool object_updated)
{
	/* test if we can instance or if the object is modified */
	BL::ID b_ob_data = b_ob.data();
	BL::ID key = (BKE_object_is_modified(b_ob))? b_ob: b_ob_data;
	BL::Material material_override = render_layer.material_override;

	/* find shader indices */
	vector<uint> used_shaders;

	BL::Object::material_slots_iterator slot;
	for(b_ob.material_slots.begin(slot); slot != b_ob.material_slots.end(); ++slot) {
		if(material_override)
			find_shader(material_override, used_shaders, scene->default_surface);
		else
			find_shader(slot->material(), used_shaders, scene->default_surface);
	}

	if(used_shaders.size() == 0) {
		if(material_override)
			find_shader(material_override, used_shaders, scene->default_surface);
		else
			used_shaders.push_back(scene->default_surface);
	}
	
	/* test if we need to sync */
	Mesh *mesh;

	if(!mesh_map.sync(&mesh, key)) {
		/* if transform was applied to mesh, need full update */
		if(object_updated && mesh->transform_applied);
		/* test if shaders changed, these can be object level so mesh
		 * does not get tagged for recalc */
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

void BlenderSync::sync_mesh_motion(BL::Object b_ob, Mesh *mesh, int motion)
{
	/* todo: displacement, subdivision */
	size_t size = mesh->verts.size();

	/* skip objects without deforming modifiers. this is not a totally reliable,
	 * would need a more extensive check to see which objects are animated */
	if(!size || !ccl::BKE_object_is_deform_modified(b_ob, b_scene, preview))
		return;

	/* get derived mesh */
	BL::Mesh b_mesh = object_to_mesh(b_ob, b_scene, true, !preview);

	if(b_mesh) {
		BL::Mesh::vertices_iterator v;
		AttributeStandard std = (motion == -1)? ATTR_STD_MOTION_PRE: ATTR_STD_MOTION_POST;
		Attribute *attr_M = mesh->attributes.add(std);
		float3 *M = attr_M->data_float3(), *cur_M;
		size_t i = 0;

		for(b_mesh.vertices.begin(v), cur_M = M; v != b_mesh.vertices.end() && i < size; ++v, cur_M++, i++)
			*cur_M = get_float3(v->co());

		/* if number of vertices changed, or if coordinates stayed the same, drop it */
		if(i != size || memcmp(M, &mesh->verts[0], sizeof(float3)*size) == 0)
			mesh->attributes.remove(std);

		/* free derived mesh */
		object_remove_mesh(b_data, b_mesh);
	}
}

CCL_NAMESPACE_END

