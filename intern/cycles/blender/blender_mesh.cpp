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

 
#include "mesh.h"
#include "object.h"
#include "scene.h"

#include "blender_sync.h"
#include "blender_util.h"

#include "subd_mesh.h"
#include "subd_patch.h"
#include "subd_split.h"

#include "util_foreach.h"

#include "mikktspace.h"

CCL_NAMESPACE_BEGIN

/* Tangent Space */

struct MikkUserData {
	MikkUserData(const BL::Mesh mesh_, const BL::MeshTextureFaceLayer layer_, int num_faces_)
	: mesh(mesh_), layer(layer_), num_faces(num_faces_)
	{
		tangent.resize(num_faces*4);
	}

	BL::Mesh mesh;
	BL::MeshTextureFaceLayer layer;
	int num_faces;
	vector<float4> tangent;
};

static int mikk_get_num_faces(const SMikkTSpaceContext *context)
{
	MikkUserData *userdata = (MikkUserData*)context->m_pUserData;
	return userdata->num_faces;
}

static int mikk_get_num_verts_of_face(const SMikkTSpaceContext *context, const int face_num)
{
	MikkUserData *userdata = (MikkUserData*)context->m_pUserData;
	BL::MeshTessFace f = userdata->mesh.tessfaces[face_num];
	int4 vi = get_int4(f.vertices_raw());

	return (vi[3] == 0)? 3: 4;
}

static void mikk_get_position(const SMikkTSpaceContext *context, float P[3], const int face_num, const int vert_num)
{
	MikkUserData *userdata = (MikkUserData*)context->m_pUserData;
	BL::MeshTessFace f = userdata->mesh.tessfaces[face_num];
	int4 vi = get_int4(f.vertices_raw());
	BL::MeshVertex v = userdata->mesh.vertices[vi[vert_num]];
	float3 vP = get_float3(v.co());

	P[0] = vP.x;
	P[1] = vP.y;
	P[2] = vP.z;
}

static void mikk_get_texture_coordinate(const SMikkTSpaceContext *context, float uv[2], const int face_num, const int vert_num)
{
	MikkUserData *userdata = (MikkUserData*)context->m_pUserData;
	BL::MeshTextureFace tf = userdata->layer.data[face_num];
	float3 tfuv;
	
	switch (vert_num) {
		case 0:
			tfuv = get_float3(tf.uv1());
			break;
		case 1:
			tfuv = get_float3(tf.uv2());
			break;
		case 2:
			tfuv = get_float3(tf.uv3());
			break;
		default:
			tfuv = get_float3(tf.uv4());
			break;
	}
	
	uv[0] = tfuv.x;
	uv[1] = tfuv.y;
}

static void mikk_get_normal(const SMikkTSpaceContext *context, float N[3], const int face_num, const int vert_num)
{
	MikkUserData *userdata = (MikkUserData*)context->m_pUserData;
	BL::MeshTessFace f = userdata->mesh.tessfaces[face_num];
	float3 vN;

	if(f.use_smooth()) {
		int4 vi = get_int4(f.vertices_raw());
		BL::MeshVertex v = userdata->mesh.vertices[vi[vert_num]];
		vN = get_float3(v.normal());
	}
	else {
		vN = get_float3(f.normal());
	}

	N[0] = vN.x;
	N[1] = vN.y;
	N[2] = vN.z;
}

static void mikk_set_tangent_space(const SMikkTSpaceContext *context, const float T[], const float sign, const int face, const int vert)
{
	MikkUserData *userdata = (MikkUserData*)context->m_pUserData;

	userdata->tangent[face*4 + vert] = make_float4(T[0], T[1], T[2], sign);
}

static void mikk_compute_tangents(BL::Mesh b_mesh, BL::MeshTextureFaceLayer b_layer, Mesh *mesh, vector<int>& nverts, bool need_sign, bool active_render)
{
	/* setup userdata */
	MikkUserData userdata(b_mesh, b_layer, nverts.size());

	/* setup interface */
	SMikkTSpaceInterface sm_interface;
	memset(&sm_interface, 0, sizeof(sm_interface));
	sm_interface.m_getNumFaces = mikk_get_num_faces;
	sm_interface.m_getNumVerticesOfFace = mikk_get_num_verts_of_face;
	sm_interface.m_getPosition = mikk_get_position;
	sm_interface.m_getTexCoord = mikk_get_texture_coordinate;
	sm_interface.m_getNormal = mikk_get_normal;
	sm_interface.m_setTSpaceBasic = mikk_set_tangent_space;

	/* setup context */
	SMikkTSpaceContext context;
	memset(&context, 0, sizeof(context));
	context.m_pUserData = &userdata;
	context.m_pInterface = &sm_interface;

	/* compute tangents */
	genTangSpaceDefault(&context);

	/* create tangent attributes */
	Attribute *attr;
	ustring name = ustring((string(b_layer.name().c_str()) + ".tangent").c_str());

	if(active_render)
		attr = mesh->attributes.add(ATTR_STD_UV_TANGENT, name);
	else
		attr = mesh->attributes.add(name, TypeDesc::TypeVector, ATTR_ELEMENT_CORNER);

	float3 *tangent = attr->data_float3();

	/* create bitangent sign attribute */
	float *tangent_sign = NULL;

	if(need_sign) {
		Attribute *attr_sign;
		ustring name_sign = ustring((string(b_layer.name().c_str()) + ".tangent_sign").c_str());

		if(active_render)
			attr_sign = mesh->attributes.add(ATTR_STD_UV_TANGENT_SIGN, name_sign);
		else
			attr_sign = mesh->attributes.add(name_sign, TypeDesc::TypeFloat, ATTR_ELEMENT_CORNER);

		tangent_sign = attr_sign->data_float();
	}

	for(int i = 0; i < nverts.size(); i++) {
		tangent[0] = float4_to_float3(userdata.tangent[i*4 + 0]);
		tangent[1] = float4_to_float3(userdata.tangent[i*4 + 1]);
		tangent[2] = float4_to_float3(userdata.tangent[i*4 + 2]);
		tangent += 3;

		if(tangent_sign) {
			tangent_sign[0] = userdata.tangent[i*4 + 0].w;
			tangent_sign[1] = userdata.tangent[i*4 + 1].w;
			tangent_sign[2] = userdata.tangent[i*4 + 2].w;
			tangent_sign += 3;
		}

		if(nverts[i] == 4) {
			tangent[0] = float4_to_float3(userdata.tangent[i*4 + 0]);
			tangent[1] = float4_to_float3(userdata.tangent[i*4 + 2]);
			tangent[2] = float4_to_float3(userdata.tangent[i*4 + 3]);
			tangent += 3;

			if(tangent_sign) {
				tangent_sign[0] = userdata.tangent[i*4 + 0].w;
				tangent_sign[1] = userdata.tangent[i*4 + 2].w;
				tangent_sign[2] = userdata.tangent[i*4 + 3].w;
				tangent_sign += 3;
			}
		}
	}
}

/* Create Mesh */

static void create_mesh(Scene *scene, Mesh *mesh, BL::Mesh b_mesh, const vector<uint>& used_shaders)
{
	/* count vertices and faces */
	int numverts = b_mesh.vertices.length();
	int numfaces = b_mesh.tessfaces.length();
	int numtris = 0;

	BL::Mesh::vertices_iterator v;
	BL::Mesh::tessfaces_iterator f;

	for(b_mesh.tessfaces.begin(f); f != b_mesh.tessfaces.end(); ++f) {
		int4 vi = get_int4(f->vertices_raw());
		numtris += (vi[3] == 0)? 1: 2;
	}

	/* reserve memory */
	mesh->reserve(numverts, numtris, 0, 0);

	/* create vertex coordinates and normals */
	int i = 0;
	for(b_mesh.vertices.begin(v); v != b_mesh.vertices.end(); ++v, ++i)
		mesh->verts[i] = get_float3(v->co());

	Attribute *attr_N = mesh->attributes.add(ATTR_STD_VERTEX_NORMAL);
	float3 *N = attr_N->data_float3();

	for(b_mesh.vertices.begin(v); v != b_mesh.vertices.end(); ++v, ++N)
		*N = get_float3(v->normal());

	/* create faces */
	vector<int> nverts(numfaces);
	int fi = 0, ti = 0;

	for(b_mesh.tessfaces.begin(f); f != b_mesh.tessfaces.end(); ++f, ++fi) {
		int4 vi = get_int4(f->vertices_raw());
		int n = (vi[3] == 0)? 3: 4;
		int mi = clamp(f->material_index(), 0, used_shaders.size()-1);
		int shader = used_shaders[mi];
		bool smooth = f->use_smooth();

		if(n == 4) {
			if(is_zero(cross(mesh->verts[vi[1]] - mesh->verts[vi[0]], mesh->verts[vi[2]] - mesh->verts[vi[0]])) ||
				is_zero(cross(mesh->verts[vi[2]] - mesh->verts[vi[0]], mesh->verts[vi[3]] - mesh->verts[vi[0]]))) {
				mesh->set_triangle(ti++, vi[0], vi[1], vi[3], shader, smooth);
				mesh->set_triangle(ti++, vi[2], vi[3], vi[1], shader, smooth);
			}
			else {
				mesh->set_triangle(ti++, vi[0], vi[1], vi[2], shader, smooth);
				mesh->set_triangle(ti++, vi[0], vi[2], vi[3], shader, smooth);
			}
		}
		else
			mesh->set_triangle(ti++, vi[0], vi[1], vi[2], shader, smooth);

		nverts[fi] = n;
	}

	/* create vertex color attributes */
	{
		BL::Mesh::tessface_vertex_colors_iterator l;

		for(b_mesh.tessface_vertex_colors.begin(l); l != b_mesh.tessface_vertex_colors.end(); ++l) {
			if(!mesh->need_attribute(scene, ustring(l->name().c_str())))
				continue;

			Attribute *attr = mesh->attributes.add(
				ustring(l->name().c_str()), TypeDesc::TypeColor, ATTR_ELEMENT_CORNER);

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
			bool active_render = l->active_render();
			AttributeStandard std = (active_render)? ATTR_STD_UV: ATTR_STD_NONE;
			ustring name = ustring(l->name().c_str());

			/* UV map */
			if(mesh->need_attribute(scene, name) || mesh->need_attribute(scene, std)) {
				Attribute *attr;

				if(active_render)
					attr = mesh->attributes.add(std, name);
				else
					attr = mesh->attributes.add(name, TypeDesc::TypePoint, ATTR_ELEMENT_CORNER);

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

			/* UV tangent */
			std = (active_render)? ATTR_STD_UV_TANGENT: ATTR_STD_NONE;
			name = ustring((string(l->name().c_str()) + ".tangent").c_str());

			if(mesh->need_attribute(scene, name) || (active_render && mesh->need_attribute(scene, std))) {
				std = (active_render)? ATTR_STD_UV_TANGENT_SIGN: ATTR_STD_NONE;
				name = ustring((string(l->name().c_str()) + ".tangent_sign").c_str());
				bool need_sign = (mesh->need_attribute(scene, name) || mesh->need_attribute(scene, std));

				mikk_compute_tangents(b_mesh, *l, mesh, nverts, need_sign, active_render);
			}
		}
	}

	/* create generated coordinates from undeformed coordinates */
	if(mesh->need_attribute(scene, ATTR_STD_GENERATED)) {
		Attribute *attr = mesh->attributes.add(ATTR_STD_GENERATED);

		float3 loc, size;
		mesh_texture_space(b_mesh, loc, size);

		float3 *generated = attr->data_float3();
		size_t i = 0;

		for(b_mesh.vertices.begin(v); v != b_mesh.vertices.end(); ++v)
			generated[i++] = get_float3(v->undeformed_co())*size - loc;
	}

	/* for volume objects, create a matrix to transform from object space to
	 * mesh texture space. this does not work with deformations but that can
	 * probably only be done well with a volume grid mapping of coordinates */
	if(mesh->need_attribute(scene, ATTR_STD_GENERATED_TRANSFORM)) {
		Attribute *attr = mesh->attributes.add(ATTR_STD_GENERATED_TRANSFORM);
		Transform *tfm = attr->data_transform();

		float3 loc, size;
		mesh_texture_space(b_mesh, loc, size);

		*tfm = transform_translate(-loc)*transform_scale(size);
	}
}

static void create_subd_mesh(Scene *scene, Mesh *mesh, BL::Mesh b_mesh, PointerRNA *cmesh, const vector<uint>& used_shaders)
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
		else
			sdmesh.add_face(vi[0], vi[1], vi[2]);
	}

	/* finalize subd mesh */
	sdmesh.finish();

	/* parameters */
	bool need_ptex = mesh->need_attribute(scene, ATTR_STD_PTEX_FACE_ID) ||
	                 mesh->need_attribute(scene, ATTR_STD_PTEX_UV);

	SubdParams sdparams(mesh, used_shaders[0], true, need_ptex);
	sdparams.dicing_rate = RNA_float_get(cmesh, "dicing_rate");
	//scene->camera->update();
	//sdparams.camera = scene->camera;

	/* tesselate */
	DiagSplit dsplit(sdparams);;
	sdmesh.tessellate(&dsplit);
}

/* Sync */

Mesh *BlenderSync::sync_mesh(BL::Object b_ob, bool object_updated, bool hide_tris)
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
	PointerRNA cmesh = RNA_pointer_get(&b_ob_data.ptr, "cycles");

	vector<Mesh::Triangle> oldtriangle = mesh->triangles;
	
	/* compares curve_keys rather than strands in order to handle quick hair
	 * adjustsments in dynamic BVH - other methods could probably do this better*/
	vector<Mesh::CurveKey> oldcurve_keys = mesh->curve_keys;

	mesh->clear();
	mesh->used_shaders = used_shaders;
	mesh->name = ustring(b_ob_data.name().c_str());

	if(render_layer.use_surfaces || render_layer.use_hair) {
		if(preview)
			b_ob.update_from_editmode();

		bool need_undeformed = mesh->need_attribute(scene, ATTR_STD_GENERATED);
		BL::Mesh b_mesh = object_to_mesh(b_data, b_ob, b_scene, true, !preview, need_undeformed);

		if(b_mesh) {
			if(render_layer.use_surfaces && !hide_tris) {
				if(cmesh.data && experimental && RNA_boolean_get(&cmesh, "use_subdivision"))
					create_subd_mesh(scene, mesh, b_mesh, &cmesh, used_shaders);
				else
					create_mesh(scene, mesh, b_mesh, used_shaders);
			}

			if(render_layer.use_hair)
				sync_curves(mesh, b_mesh, b_ob, 0);

			/* free derived mesh */
			b_data.meshes.remove(b_mesh);
		}
	}

	/* displacement method */
	if(cmesh.data) {
		const int method = RNA_enum_get(&cmesh, "displacement_method");

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

	if(oldcurve_keys.size() != mesh->curve_keys.size())
		rebuild = true;
	else if(oldcurve_keys.size()) {
		if(memcmp(&oldcurve_keys[0], &mesh->curve_keys[0], sizeof(Mesh::CurveKey)*oldcurve_keys.size()) != 0)
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

	/* ensure we only sync instanced meshes once */
	if(mesh_motion_synced.find(mesh) != mesh_motion_synced.end())
		return;

	mesh_motion_synced.insert(mesh);

	/* get derived mesh */
	BL::Mesh b_mesh = object_to_mesh(b_data, b_ob, b_scene, true, !preview, false);

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

		/* hair motion */
		if(render_layer.use_hair)
			sync_curves(mesh, b_mesh, b_ob, motion);

		/* free derived mesh */
		b_data.meshes.remove(b_mesh);
	}
}

CCL_NAMESPACE_END

