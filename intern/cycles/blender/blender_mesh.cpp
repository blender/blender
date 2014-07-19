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
	MikkUserData(const BL::Mesh mesh_, BL::MeshTextureFaceLayer *layer_, int num_faces_)
	: mesh(mesh_), layer(layer_), num_faces(num_faces_)
	{
		tangent.resize(num_faces*4);
	}

	BL::Mesh mesh;
	BL::MeshTextureFaceLayer *layer;
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
	if(userdata->layer != NULL) {
		BL::MeshTextureFace tf = userdata->layer->data[face_num];
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
	else {
		int vert_idx = userdata->mesh.tessfaces[face_num].vertices()[vert_num];
		float3 orco =
			get_float3(userdata->mesh.vertices[vert_idx].undeformed_co());
		map_to_sphere(&uv[0], &uv[1], orco[0], orco[1], orco[2]);
	}
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

static void mikk_compute_tangents(BL::Mesh b_mesh, BL::MeshTextureFaceLayer *b_layer, Mesh *mesh, vector<int>& nverts, bool need_sign, bool active_render)
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
	ustring name;
	if(b_layer != NULL)
		name = ustring((string(b_layer->name().c_str()) + ".tangent").c_str());
	else
		name = ustring("orco.tangent");

	if(active_render)
		attr = mesh->attributes.add(ATTR_STD_UV_TANGENT, name);
	else
		attr = mesh->attributes.add(name, TypeDesc::TypeVector, ATTR_ELEMENT_CORNER);

	float3 *tangent = attr->data_float3();

	/* create bitangent sign attribute */
	float *tangent_sign = NULL;

	if(need_sign) {
		Attribute *attr_sign;
		ustring name_sign;
		if(b_layer != NULL)
			name_sign = ustring((string(b_layer->name().c_str()) + ".tangent_sign").c_str());
		else
			name_sign = ustring("orco.tangent_sign");

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

/* Create Volume Attribute */

static void create_mesh_volume_attribute(BL::Object b_ob, Mesh *mesh, ImageManager *image_manager, AttributeStandard std, float frame)
{
	BL::SmokeDomainSettings b_domain = object_smoke_domain_find(b_ob);

	if(!b_domain)
		return;
	
	Attribute *attr = mesh->attributes.add(std);
	VoxelAttribute *volume_data = attr->data_voxel();
	bool is_float, is_linear;
	bool animated = false;

	volume_data->manager = image_manager;
	volume_data->slot = image_manager->add_image(Attribute::standard_name(std),
		b_ob.ptr.data, animated, frame, is_float, is_linear, INTERPOLATION_LINEAR, true);
}

static void create_mesh_volume_attributes(Scene *scene, BL::Object b_ob, Mesh *mesh, float frame)
{
	/* for smoke volume rendering */
	if(mesh->need_attribute(scene, ATTR_STD_VOLUME_DENSITY))
		create_mesh_volume_attribute(b_ob, mesh, scene->image_manager, ATTR_STD_VOLUME_DENSITY, frame);
	if(mesh->need_attribute(scene, ATTR_STD_VOLUME_COLOR))
		create_mesh_volume_attribute(b_ob, mesh, scene->image_manager, ATTR_STD_VOLUME_COLOR, frame);
	if(mesh->need_attribute(scene, ATTR_STD_VOLUME_FLAME))
		create_mesh_volume_attribute(b_ob, mesh, scene->image_manager, ATTR_STD_VOLUME_FLAME, frame);
	if(mesh->need_attribute(scene, ATTR_STD_VOLUME_HEAT))
		create_mesh_volume_attribute(b_ob, mesh, scene->image_manager, ATTR_STD_VOLUME_HEAT, frame);
	if(mesh->need_attribute(scene, ATTR_STD_VOLUME_VELOCITY))
		create_mesh_volume_attribute(b_ob, mesh, scene->image_manager, ATTR_STD_VOLUME_VELOCITY, frame);
}

/* Create Mesh */

static void create_mesh(Scene *scene, Mesh *mesh, BL::Mesh b_mesh, const vector<uint>& used_shaders)
{
	/* count vertices and faces */
	int numverts = b_mesh.vertices.length();
	int numfaces = b_mesh.tessfaces.length();
	int numtris = 0;
	bool use_loop_normals = b_mesh.use_auto_smooth();

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
	N = attr_N->data_float3();

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

	/* create faces */
	vector<int> nverts(numfaces);
	int fi = 0, ti = 0;

	for(b_mesh.tessfaces.begin(f); f != b_mesh.tessfaces.end(); ++f, ++fi) {
		int4 vi = get_int4(f->vertices_raw());
		int n = (vi[3] == 0)? 3: 4;
		int mi = clamp(f->material_index(), 0, used_shaders.size()-1);
		int shader = used_shaders[mi];
		bool smooth = f->use_smooth();

		/* split vertices if normal is different
		 *
		 * note all vertex attributes must have been set here so we can split
		 * and copy attributes in split_vertex without remapping later */
		if(use_loop_normals) {
			BL::Array<float, 12> loop_normals = f->split_normals();

			for(int i = 0; i < n; i++) {
				float3 loop_N = make_float3(loop_normals[i * 3], loop_normals[i * 3 + 1], loop_normals[i * 3 + 2]);

				if(N[vi[i]] != loop_N) {
					int new_vi = mesh->split_vertex(vi[i]);

					/* set new normal and vertex index */
					N = attr_N->data_float3();
					N[new_vi] = loop_N;
					vi[i] = new_vi;
				}
			}
		}

		/* create triangles */
		if(n == 4) {
			if(is_zero(cross(mesh->verts[vi[1]] - mesh->verts[vi[0]], mesh->verts[vi[2]] - mesh->verts[vi[0]])) ||
			   is_zero(cross(mesh->verts[vi[2]] - mesh->verts[vi[0]], mesh->verts[vi[3]] - mesh->verts[vi[0]])))
			{
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
				ustring(l->name().c_str()), TypeDesc::TypeColor, ATTR_ELEMENT_CORNER_BYTE);

			BL::MeshColorLayer::data_iterator c;
			uchar4 *cdata = attr->data_uchar4();
			size_t i = 0;

			for(l->data.begin(c); c != l->data.end(); ++c, ++i) {
				cdata[0] = color_float_to_byte(color_srgb_to_scene_linear(get_float3(c->color1())));
				cdata[1] = color_float_to_byte(color_srgb_to_scene_linear(get_float3(c->color2())));
				cdata[2] = color_float_to_byte(color_srgb_to_scene_linear(get_float3(c->color3())));

				if(nverts[i] == 4) {
					cdata[3] = cdata[0];
					cdata[4] = cdata[2];
					cdata[5] = color_float_to_byte(color_srgb_to_scene_linear(get_float3(c->color4())));
					cdata += 6;
				}
				else
					cdata += 3;
			}
		}
	}

	/* create uv map attributes */
	if (b_mesh.tessface_uv_textures.length() != 0) {
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

				std::cout << "name " << name << std::endl;
				for(l->data.begin(t); t != l->data.end(); ++t, ++i) {
					fdata[0] =  get_float3(t->uv1());
					fdata[1] =  get_float3(t->uv2());
					fdata[2] =  get_float3(t->uv3());
					fdata += 3;
#if 0
					std::cout << "  1st "
					          << "( " << t->uv1()[0] << ", " << t->uv1()[1] << ") "
					          << "( " << t->uv2()[0] << ", " << t->uv2()[1] << ") "
					          << "( " << t->uv3()[0] << ", " << t->uv3()[1] << ") " << std::endl;
#endif

					if(nverts[i] == 4) {
						fdata[0] =  get_float3(t->uv1());
						fdata[1] =  get_float3(t->uv3());
						fdata[2] =  get_float3(t->uv4());
						fdata += 3;
#if 0
						std::cout << "  2nd "
						          << "( " << t->uv1()[0] << ", " << t->uv1()[1] << ") "
						          << "( " << t->uv3()[0] << ", " << t->uv3()[1] << ") "
						          << "( " << t->uv4()[0] << ", " << t->uv4()[1] << ") " << std::endl;
#endif
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

				mikk_compute_tangents(b_mesh, &(*l), mesh, nverts, need_sign, active_render);
			}
		}
	}
	else if(mesh->need_attribute(scene, ATTR_STD_UV_TANGENT)) {
		bool need_sign = mesh->need_attribute(scene, ATTR_STD_UV_TANGENT_SIGN);
		mikk_compute_tangents(b_mesh, NULL, mesh, nverts, need_sign, true);
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
	DiagSplit dsplit(sdparams);
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
	vector<float4> oldcurve_keys = mesh->curve_keys;

	mesh->clear();
	mesh->used_shaders = used_shaders;
	mesh->name = ustring(b_ob_data.name().c_str());

	if(render_layer.use_surfaces || render_layer.use_hair) {
		/* mesh objects does have special handle in the dependency graph,
		 * they're ensured to have properly updated.
		 *
		 * updating meshes here will end up having derived mesh referencing
		 * freed data from the blender side.
		 */
		if(preview && b_ob.type() != BL::Object::type_MESH)
			b_ob.update_from_editmode();

		bool need_undeformed = mesh->need_attribute(scene, ATTR_STD_GENERATED);
		BL::Mesh b_mesh = object_to_mesh(b_data, b_ob, b_scene, true, !preview, need_undeformed);

		if(b_mesh) {
			if(render_layer.use_surfaces && !hide_tris) {
				if(cmesh.data && experimental && RNA_boolean_get(&cmesh, "use_subdivision"))
					create_subd_mesh(scene, mesh, b_mesh, &cmesh, used_shaders);
				else
					create_mesh(scene, mesh, b_mesh, used_shaders);

				create_mesh_volume_attributes(scene, b_ob, mesh, b_scene.frame_current());
			}

			if(render_layer.use_hair)
				sync_curves(mesh, b_mesh, b_ob, false);

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
		if(memcmp(&oldcurve_keys[0], &mesh->curve_keys[0], sizeof(float4)*oldcurve_keys.size()) != 0)
			rebuild = true;
	}
	
	mesh->tag_update(scene, rebuild);

	return mesh;
}

void BlenderSync::sync_mesh_motion(BL::Object b_ob, Object *object, float motion_time)
{
	/* ensure we only sync instanced meshes once */
	Mesh *mesh = object->mesh;

	if(mesh_motion_synced.find(mesh) != mesh_motion_synced.end())
		return;

	mesh_motion_synced.insert(mesh);

	/* ensure we only motion sync meshes that also had mesh synced, to avoid
	 * unnecessary work and to ensure that its attributes were clear */
	if(mesh_synced.find(mesh) == mesh_synced.end())
		return;

	/* for motion pass always compute, for motion blur it can be disabled */
	int time_index = 0;

	if(scene->need_motion() == Scene::MOTION_BLUR) {
		if(!mesh->use_motion_blur)
			return;
		
		/* see if this mesh needs motion data at this time */
		vector<float> object_times = object->motion_times();
		bool found = false;

		foreach(float object_time, object_times) {
			if(motion_time == object_time) {
				found = true;
				break;
			}
			else
				time_index++;
		}

		if(!found)
			return;
	}
	else {
		if(motion_time == -1.0f)
			time_index = 0;
		else if(motion_time == 1.0f)
			time_index = 1;
		else
			return;
	}

	/* skip empty meshes */
	size_t numverts = mesh->verts.size();
	size_t numkeys = mesh->curve_keys.size();

	if(!numverts && !numkeys)
		return;
	
	/* skip objects without deforming modifiers. this is not totally reliable,
	 * would need a more extensive check to see which objects are animated */
	BL::Mesh b_mesh(PointerRNA_NULL);

	if(ccl::BKE_object_is_deform_modified(b_ob, b_scene, preview)) {
		/* get derived mesh */
		b_mesh = object_to_mesh(b_data, b_ob, b_scene, true, !preview, false);
	}

	if(!b_mesh) {
		/* if we have no motion blur on this frame, but on other frames, copy */
		if(numverts) {
			/* triangles */
			Attribute *attr_mP = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

			if(attr_mP) {
				Attribute *attr_mN = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_NORMAL);
				Attribute *attr_N = mesh->attributes.find(ATTR_STD_VERTEX_NORMAL);
				float3 *P = &mesh->verts[0];
				float3 *N = (attr_N)? attr_N->data_float3(): NULL;

				memcpy(attr_mP->data_float3() + time_index*numverts, P, sizeof(float3)*numverts);
				if(attr_mN)
					memcpy(attr_mN->data_float3() + time_index*numverts, N, sizeof(float3)*numverts);
			}
		}

		if(numkeys) {
			/* curves */
			Attribute *attr_mP = mesh->curve_attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

			if(attr_mP) {
				float4 *keys = &mesh->curve_keys[0];
				memcpy(attr_mP->data_float4() + time_index*numkeys, keys, sizeof(float4)*numkeys);
			}
		}

		return;
	}

	if(numverts) {
		/* find attributes */
		Attribute *attr_mP = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
		Attribute *attr_mN = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_NORMAL);
		Attribute *attr_N = mesh->attributes.find(ATTR_STD_VERTEX_NORMAL);
		bool new_attribute = false;

		/* add new attributes if they don't exist already */
		if(!attr_mP) {
			attr_mP = mesh->attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
			if(attr_N)
				attr_mN = mesh->attributes.add(ATTR_STD_MOTION_VERTEX_NORMAL);

			new_attribute = true;
		}

		/* load vertex data from mesh */
		float3 *mP = attr_mP->data_float3() + time_index*numverts;
		float3 *mN = (attr_mN)? attr_mN->data_float3() + time_index*numverts: NULL;

		BL::Mesh::vertices_iterator v;
		int i = 0;

		for(b_mesh.vertices.begin(v); v != b_mesh.vertices.end() && i < numverts; ++v, ++i) {
			mP[i] = get_float3(v->co());
			if(mN)
				mN[i] = get_float3(v->normal());
		}

		/* in case of new attribute, we verify if there really was any motion */
		if(new_attribute) {
			if(i != numverts || memcmp(mP, &mesh->verts[0], sizeof(float3)*numverts) == 0) {
				/* no motion, remove attributes again */
				mesh->attributes.remove(ATTR_STD_MOTION_VERTEX_POSITION);
				if(attr_mN)
					mesh->attributes.remove(ATTR_STD_MOTION_VERTEX_NORMAL);
			}
			else if(time_index > 0) {
				/* motion, fill up previous steps that we might have skipped because
				 * they had no motion, but we need them anyway now */
				float3 *P = &mesh->verts[0];
				float3 *N = (attr_N)? attr_N->data_float3(): NULL;

				for(int step = 0; step < time_index; step++) {
					memcpy(attr_mP->data_float3() + step*numverts, P, sizeof(float3)*numverts);
					if(attr_mN)
						memcpy(attr_mN->data_float3() + step*numverts, N, sizeof(float3)*numverts);
				}
			}
		}
	}

	/* hair motion */
	if(numkeys)
		sync_curves(mesh, b_mesh, b_ob, true, time_index);

	/* free derived mesh */
	b_data.meshes.remove(b_mesh);
}

CCL_NAMESPACE_END

