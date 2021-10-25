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

#include "render/mesh.h"
#include "render/object.h"
#include "render/scene.h"
#include "render/camera.h"

#include "blender/blender_sync.h"
#include "blender/blender_session.h"
#include "blender/blender_util.h"

#include "subd/subd_patch.h"
#include "subd/subd_split.h"

#include "util/util_algorithm.h"
#include "util/util_foreach.h"
#include "util/util_logging.h"
#include "util/util_math.h"

#include "mikktspace.h"

CCL_NAMESPACE_BEGIN

/* Per-face bit flags. */
enum {
	/* Face has no special flags. */
	FACE_FLAG_NONE      = (0 << 0),
	/* Quad face was split using 1-3 diagonal. */
	FACE_FLAG_DIVIDE_13 = (1 << 0),
	/* Quad face was split using 2-4 diagonal. */
	FACE_FLAG_DIVIDE_24 = (1 << 1),
};

/* Get vertex indices to create triangles from a given face.
 *
 * Two triangles has vertex indices in the original Blender-side face.
 * If face is already a quad tri_b will not be initialized.
 */
inline void face_split_tri_indices(const int face_flag,
                                   int tri_a[3],
                                   int tri_b[3])
{
	if(face_flag & FACE_FLAG_DIVIDE_24) {
		tri_a[0] = 0;
		tri_a[1] = 1;
		tri_a[2] = 3;

		tri_b[0] = 2;
		tri_b[1] = 3;
		tri_b[2] = 1;
	}
	else /*if(face_flag & FACE_FLAG_DIVIDE_13)*/ {
		tri_a[0] = 0;
		tri_a[1] = 1;
		tri_a[2] = 2;

		tri_b[0] = 0;
		tri_b[1] = 2;
		tri_b[2] = 3;
	}
}

/* Tangent Space */

struct MikkUserData {
	MikkUserData(const BL::Mesh& mesh_,
	             BL::MeshTextureFaceLayer *layer_,
	             int num_faces_)
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

		switch(vert_num) {
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
		float2 tmp = map_to_sphere(make_float3(orco[0], orco[1], orco[2]));
		uv[0] = tmp.x;
		uv[1] = tmp.y;
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

static void mikk_compute_tangents(BL::Mesh& b_mesh,
                                  BL::MeshTextureFaceLayer *b_layer,
                                  Mesh *mesh,
                                  const vector<int>& nverts,
                                  const vector<int>& face_flags,
                                  bool need_sign,
                                  bool active_render)
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
		int tri_a[3], tri_b[3];
		face_split_tri_indices(face_flags[i], tri_a, tri_b);

		tangent[0] = float4_to_float3(userdata.tangent[i*4 + tri_a[0]]);
		tangent[1] = float4_to_float3(userdata.tangent[i*4 + tri_a[1]]);
		tangent[2] = float4_to_float3(userdata.tangent[i*4 + tri_a[2]]);
		tangent += 3;

		if(tangent_sign) {
			tangent_sign[0] = userdata.tangent[i*4 + tri_a[0]].w;
			tangent_sign[1] = userdata.tangent[i*4 + tri_a[1]].w;
			tangent_sign[2] = userdata.tangent[i*4 + tri_a[2]].w;
			tangent_sign += 3;
		}

		if(nverts[i] == 4) {
			tangent[0] = float4_to_float3(userdata.tangent[i*4 + tri_b[0]]);
			tangent[1] = float4_to_float3(userdata.tangent[i*4 + tri_b[1]]);
			tangent[2] = float4_to_float3(userdata.tangent[i*4 + tri_b[2]]);
			tangent += 3;

			if(tangent_sign) {
				tangent_sign[0] = userdata.tangent[i*4 + tri_b[0]].w;
				tangent_sign[1] = userdata.tangent[i*4 + tri_b[1]].w;
				tangent_sign[2] = userdata.tangent[i*4 + tri_b[2]].w;
				tangent_sign += 3;
			}
		}
	}
}

/* Create Volume Attribute */

static void create_mesh_volume_attribute(BL::Object& b_ob,
                                         Mesh *mesh,
                                         ImageManager *image_manager,
                                         AttributeStandard std,
                                         float frame)
{
	BL::SmokeDomainSettings b_domain = object_smoke_domain_find(b_ob);

	if(!b_domain)
		return;

	Attribute *attr = mesh->attributes.add(std);
	VoxelAttribute *volume_data = attr->data_voxel();
	bool is_float, is_linear;
	bool animated = false;

	volume_data->manager = image_manager;
	volume_data->slot = image_manager->add_image(
	        Attribute::standard_name(std),
	        b_ob.ptr.data,
	        animated,
	        frame,
	        is_float,
	        is_linear,
	        INTERPOLATION_LINEAR,
	        EXTENSION_CLIP,
	        true);
}

static void create_mesh_volume_attributes(Scene *scene,
                                          BL::Object& b_ob,
                                          Mesh *mesh,
                                          float frame)
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

/* Create vertex color attributes. */
static void attr_create_vertex_color(Scene *scene,
                                     Mesh *mesh,
                                     BL::Mesh& b_mesh,
                                     const vector<int>& nverts,
                                     const vector<int>& face_flags,
                                     bool subdivision)
{
	if(subdivision) {
		BL::Mesh::vertex_colors_iterator l;

		for(b_mesh.vertex_colors.begin(l); l != b_mesh.vertex_colors.end(); ++l) {
			if(!mesh->need_attribute(scene, ustring(l->name().c_str())))
				continue;

			Attribute *attr = mesh->subd_attributes.add(ustring(l->name().c_str()),
			                                            TypeDesc::TypeColor,
			                                            ATTR_ELEMENT_CORNER_BYTE);

			BL::Mesh::polygons_iterator p;
			uchar4 *cdata = attr->data_uchar4();

			for(b_mesh.polygons.begin(p); p != b_mesh.polygons.end(); ++p) {
				int n = p->loop_total();
				for(int i = 0; i < n; i++) {
					float3 color = get_float3(l->data[p->loop_start() + i].color());
					*(cdata++) = color_float_to_byte(color_srgb_to_scene_linear_v3(color));
				}
			}
		}
	}
	else {
		BL::Mesh::tessface_vertex_colors_iterator l;
		for(b_mesh.tessface_vertex_colors.begin(l); l != b_mesh.tessface_vertex_colors.end(); ++l) {
			if(!mesh->need_attribute(scene, ustring(l->name().c_str())))
				continue;

			Attribute *attr = mesh->attributes.add(ustring(l->name().c_str()),
			                                       TypeDesc::TypeColor,
			                                       ATTR_ELEMENT_CORNER_BYTE);

			BL::MeshColorLayer::data_iterator c;
			uchar4 *cdata = attr->data_uchar4();
			size_t i = 0;

			for(l->data.begin(c); c != l->data.end(); ++c, ++i) {
				int tri_a[3], tri_b[3];
				face_split_tri_indices(face_flags[i], tri_a, tri_b);

				uchar4 colors[4];
				colors[0] = color_float_to_byte(color_srgb_to_scene_linear_v3(get_float3(c->color1())));
				colors[1] = color_float_to_byte(color_srgb_to_scene_linear_v3(get_float3(c->color2())));
				colors[2] = color_float_to_byte(color_srgb_to_scene_linear_v3(get_float3(c->color3())));
				if(nverts[i] == 4) {
					colors[3] = color_float_to_byte(color_srgb_to_scene_linear_v3(get_float3(c->color4())));
				}

				cdata[0] = colors[tri_a[0]];
				cdata[1] = colors[tri_a[1]];
				cdata[2] = colors[tri_a[2]];

				if(nverts[i] == 4) {
					cdata[3] = colors[tri_b[0]];
					cdata[4] = colors[tri_b[1]];
					cdata[5] = colors[tri_b[2]];
					cdata += 6;
				}
				else
					cdata += 3;
			}
		}
	}
}

/* Create uv map attributes. */
static void attr_create_uv_map(Scene *scene,
                               Mesh *mesh,
                               BL::Mesh& b_mesh,
                               const vector<int>& nverts,
                               const vector<int>& face_flags,
                               bool subdivision,
                               bool subdivide_uvs)
{
	if(subdivision) {
		BL::Mesh::uv_layers_iterator l;
		int i = 0;

		for(b_mesh.uv_layers.begin(l); l != b_mesh.uv_layers.end(); ++l, ++i) {
			bool active_render = b_mesh.uv_textures[i].active_render();
			AttributeStandard std = (active_render)? ATTR_STD_UV: ATTR_STD_NONE;
			ustring name = ustring(l->name().c_str());

			/* UV map */
			if(mesh->need_attribute(scene, name) || mesh->need_attribute(scene, std)) {
				Attribute *attr;

				if(active_render)
					attr = mesh->subd_attributes.add(std, name);
				else
					attr = mesh->subd_attributes.add(name, TypeDesc::TypePoint, ATTR_ELEMENT_CORNER);

				if(subdivide_uvs) {
					attr->flags |= ATTR_SUBDIVIDED;
				}

				BL::Mesh::polygons_iterator p;
				float3 *fdata = attr->data_float3();

				for(b_mesh.polygons.begin(p); p != b_mesh.polygons.end(); ++p) {
					int n = p->loop_total();
					for(int j = 0; j < n; j++) {
						*(fdata++) = get_float3(l->data[p->loop_start() + j].uv());
					}
				}
			}
		}
	}
	else if(b_mesh.tessface_uv_textures.length() != 0) {
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
					int tri_a[3], tri_b[3];
					face_split_tri_indices(face_flags[i], tri_a, tri_b);

					float3 uvs[4];
					uvs[0] = get_float3(t->uv1());
					uvs[1] = get_float3(t->uv2());
					uvs[2] = get_float3(t->uv3());
					if(nverts[i] == 4) {
						uvs[3] = get_float3(t->uv4());
					}

					fdata[0] = uvs[tri_a[0]];
					fdata[1] = uvs[tri_a[1]];
					fdata[2] = uvs[tri_a[2]];
					fdata += 3;

					if(nverts[i] == 4) {
						fdata[0] = uvs[tri_b[0]];
						fdata[1] = uvs[tri_b[1]];
						fdata[2] = uvs[tri_b[2]];
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

				mikk_compute_tangents(b_mesh,
				                      &(*l),
				                      mesh,
				                      nverts,
				                      face_flags,
				                      need_sign,
				                      active_render);
			}
		}
	}
	else if(mesh->need_attribute(scene, ATTR_STD_UV_TANGENT)) {
		bool need_sign = mesh->need_attribute(scene, ATTR_STD_UV_TANGENT_SIGN);
		mikk_compute_tangents(b_mesh,
		                      NULL,
		                      mesh,
		                      nverts,
		                      face_flags,
		                      need_sign,
		                      true);
	}
}

/* Create vertex pointiness attributes. */

/* Compare vertices by sum of their coordinates. */
class VertexAverageComparator {
public:
	VertexAverageComparator(const array<float3>& verts)
	        : verts_(verts) {
	}

	bool operator()(const int& vert_idx_a, const int& vert_idx_b)
	{
		const float3 &vert_a = verts_[vert_idx_a];
		const float3 &vert_b = verts_[vert_idx_b];
		if(vert_a == vert_b) {
			/* Special case for doubles, so we ensure ordering. */
			return vert_idx_a > vert_idx_b;
		}
		const float x1 = vert_a.x + vert_a.y + vert_a.z;
		const float x2 = vert_b.x + vert_b.y + vert_b.z;
		return x1 < x2;
	}

protected:
	const array<float3>& verts_;
};

static void attr_create_pointiness(Scene *scene,
                                   Mesh *mesh,
                                   BL::Mesh& b_mesh,
                                   bool subdivision)
{
	if(!mesh->need_attribute(scene, ATTR_STD_POINTINESS)) {
		return;
	}
	const int num_verts = b_mesh.vertices.length();
	if(num_verts == 0) {
		return;
	}
	/* STEP 1: Find out duplicated vertices and point duplicates to a single
	 *         original vertex.
	 */
	vector<int> sorted_vert_indeices(num_verts);
	for(int vert_index = 0; vert_index < num_verts; ++vert_index) {
		sorted_vert_indeices[vert_index] = vert_index;
	}
	VertexAverageComparator compare(mesh->verts);
	sort(sorted_vert_indeices.begin(), sorted_vert_indeices.end(), compare);
	/* This array stores index of the original vertex for the given vertex
	 * index.
	 */
	vector<int> vert_orig_index(num_verts);
	for(int sorted_vert_index = 0;
	    sorted_vert_index < num_verts;
	    ++sorted_vert_index)
	{
		const int vert_index = sorted_vert_indeices[sorted_vert_index];
		const float3 &vert_co = mesh->verts[vert_index];
		bool found = false;
		for(int other_sorted_vert_index = sorted_vert_index + 1;
		    other_sorted_vert_index < num_verts;
		    ++other_sorted_vert_index)
		{
			const int other_vert_index =
			        sorted_vert_indeices[other_sorted_vert_index];
			const float3 &other_vert_co = mesh->verts[other_vert_index];
			/* We are too far away now, we wouldn't have duplicate. */
			if((other_vert_co.x + other_vert_co.y + other_vert_co.z) -
			   (vert_co.x + vert_co.y + vert_co.z) > 3 * FLT_EPSILON)
			{
				break;
			}
			/* Found duplicate. */
			if(len_squared(other_vert_co - vert_co) < FLT_EPSILON) {
				found = true;
				vert_orig_index[vert_index] = other_vert_index;
				break;
			}
		}
		if(!found) {
			vert_orig_index[vert_index] = vert_index;
		}
	}
	/* Make sure we always points to the very first orig vertex. */
	for(int vert_index = 0; vert_index < num_verts; ++vert_index) {
		int orig_index = vert_orig_index[vert_index];
		while(orig_index != vert_orig_index[orig_index]) {
			orig_index = vert_orig_index[orig_index];
		}
		vert_orig_index[vert_index] = orig_index;
	}
	sorted_vert_indeices.free_memory();
	/* STEP 2: Calculate vertex normals taking into account their possible
	 *         duplicates which gets "welded" together.
	 */
	vector<float3> vert_normal(num_verts, make_float3(0.0f, 0.0f, 0.0f));
	/* First we accumulate all vertex normals in the original index. */
	for(int vert_index = 0; vert_index < num_verts; ++vert_index) {
		const float3 normal = get_float3(b_mesh.vertices[vert_index].normal());
		const int orig_index = vert_orig_index[vert_index];
		vert_normal[orig_index] += normal;
	}
	/* Then we normalize the accumulated result and flush it to all duplicates
	 * as well.
	 */
	for(int vert_index = 0; vert_index < num_verts; ++vert_index) {
		const int orig_index = vert_orig_index[vert_index];
		vert_normal[vert_index] = normalize(vert_normal[orig_index]);
	}
	/* STEP 3: Calculate pointiness using single ring neighborhood. */
	vector<int> counter(num_verts, 0);
	vector<float> raw_data(num_verts, 0.0f);
	vector<float3> edge_accum(num_verts, make_float3(0.0f, 0.0f, 0.0f));
	BL::Mesh::edges_iterator e;
	EdgeMap visited_edges;
	int edge_index = 0;
	memset(&counter[0], 0, sizeof(int) * counter.size());
	for(b_mesh.edges.begin(e); e != b_mesh.edges.end(); ++e, ++edge_index) {
		const int v0 = vert_orig_index[b_mesh.edges[edge_index].vertices()[0]],
		          v1 = vert_orig_index[b_mesh.edges[edge_index].vertices()[1]];
		if(visited_edges.exists(v0, v1)) {
			continue;
		}
		visited_edges.insert(v0, v1);
		float3 co0 = get_float3(b_mesh.vertices[v0].co()),
		       co1 = get_float3(b_mesh.vertices[v1].co());
		float3 edge = normalize(co1 - co0);
		edge_accum[v0] += edge;
		edge_accum[v1] += -edge;
		++counter[v0];
		++counter[v1];
	}
	for(int vert_index = 0; vert_index < num_verts; ++vert_index) {
		const int orig_index = vert_orig_index[vert_index];
		if(orig_index != vert_index) {
			/* Skip duplicates, they'll be overwritten later on. */
			continue;
		}
		if(counter[vert_index] > 0) {
			const float3 normal = vert_normal[vert_index];
			const float angle =
			        safe_acosf(dot(normal,
			                       edge_accum[vert_index] / counter[vert_index]));
			raw_data[vert_index] = angle * M_1_PI_F;
		}
		else {
			raw_data[vert_index] = 0.0f;
		}
	}
	/* STEP 3: Blur vertices to approximate 2 ring neighborhood. */
	AttributeSet& attributes = (subdivision)? mesh->subd_attributes: mesh->attributes;
	Attribute *attr = attributes.add(ATTR_STD_POINTINESS);
	float *data = attr->data_float();
	memcpy(data, &raw_data[0], sizeof(float) * raw_data.size());
	memset(&counter[0], 0, sizeof(int) * counter.size());
	edge_index = 0;
	visited_edges.clear();
	for(b_mesh.edges.begin(e); e != b_mesh.edges.end(); ++e, ++edge_index) {
		const int v0 = vert_orig_index[b_mesh.edges[edge_index].vertices()[0]],
		          v1 = vert_orig_index[b_mesh.edges[edge_index].vertices()[1]];
		if(visited_edges.exists(v0, v1)) {
			continue;
		}
		visited_edges.insert(v0, v1);
		data[v0] += raw_data[v1];
		data[v1] += raw_data[v0];
		++counter[v0];
		++counter[v1];
	}
	for(int vert_index = 0; vert_index < num_verts; ++vert_index) {
		data[vert_index] /= counter[vert_index] + 1;
	}
	/* STEP 4: Copy attribute to the duplicated vertices. */
	for(int vert_index = 0; vert_index < num_verts; ++vert_index) {
		const int orig_index = vert_orig_index[vert_index];
		data[vert_index] = data[orig_index];
	}
}

/* Create Mesh */

static void create_mesh(Scene *scene,
                        Mesh *mesh,
                        BL::Mesh& b_mesh,
                        const vector<Shader*>& used_shaders,
                        bool subdivision = false,
                        bool subdivide_uvs = true)
{
	/* count vertices and faces */
	int numverts = b_mesh.vertices.length();
	int numfaces = (!subdivision) ? b_mesh.tessfaces.length() : b_mesh.polygons.length();
	int numtris = 0;
	int numcorners = 0;
	int numngons = 0;
	bool use_loop_normals = b_mesh.use_auto_smooth() && (mesh->subdivision_type != Mesh::SUBDIVISION_CATMULL_CLARK);

	BL::Mesh::vertices_iterator v;
	BL::Mesh::tessfaces_iterator f;
	BL::Mesh::polygons_iterator p;

	if(!subdivision) {
		for(b_mesh.tessfaces.begin(f); f != b_mesh.tessfaces.end(); ++f) {
			int4 vi = get_int4(f->vertices_raw());
			numtris += (vi[3] == 0)? 1: 2;
		}
	}
	else {
		for(b_mesh.polygons.begin(p); p != b_mesh.polygons.end(); ++p) {
			numngons += (p->loop_total() == 4)? 0: 1;
			numcorners += p->loop_total();
		}
	}

	/* allocate memory */
	mesh->reserve_mesh(numverts, numtris);
	mesh->reserve_subd_faces(numfaces, numngons, numcorners);

	/* create vertex coordinates and normals */
	for(b_mesh.vertices.begin(v); v != b_mesh.vertices.end(); ++v)
		mesh->add_vertex(get_float3(v->co()));

	AttributeSet& attributes = (subdivision)? mesh->subd_attributes: mesh->attributes;
	Attribute *attr_N = attributes.add(ATTR_STD_VERTEX_NORMAL);
	float3 *N = attr_N->data_float3();

	for(b_mesh.vertices.begin(v); v != b_mesh.vertices.end(); ++v, ++N)
		*N = get_float3(v->normal());
	N = attr_N->data_float3();

	/* create generated coordinates from undeformed coordinates */
	if(mesh->need_attribute(scene, ATTR_STD_GENERATED)) {
		Attribute *attr = attributes.add(ATTR_STD_GENERATED);
		attr->flags |= ATTR_SUBDIVIDED;

		float3 loc, size;
		mesh_texture_space(b_mesh, loc, size);

		float3 *generated = attr->data_float3();
		size_t i = 0;

		for(b_mesh.vertices.begin(v); v != b_mesh.vertices.end(); ++v)
			generated[i++] = get_float3(v->undeformed_co())*size - loc;
	}

	/* create faces */
	vector<int> nverts(numfaces);
	vector<int> face_flags(numfaces, FACE_FLAG_NONE);
	int fi = 0;

	if(!subdivision) {
		for(b_mesh.tessfaces.begin(f); f != b_mesh.tessfaces.end(); ++f, ++fi) {
			int4 vi = get_int4(f->vertices_raw());
			int n = (vi[3] == 0)? 3: 4;
			int shader = clamp(f->material_index(), 0, used_shaders.size()-1);
			bool smooth = f->use_smooth() || use_loop_normals;

			if(use_loop_normals) {
				BL::Array<float, 12> loop_normals = f->split_normals();
				for(int i = 0; i < n; i++) {
					N[vi[i]] = make_float3(loop_normals[i * 3],
					                       loop_normals[i * 3 + 1],
					                       loop_normals[i * 3 + 2]);
				}
			}

			/* Create triangles.
			 *
			 * NOTE: Autosmooth is already taken care about.
			 */
			if(n == 4) {
				if(is_zero(cross(mesh->verts[vi[1]] - mesh->verts[vi[0]], mesh->verts[vi[2]] - mesh->verts[vi[0]])) ||
				   is_zero(cross(mesh->verts[vi[2]] - mesh->verts[vi[0]], mesh->verts[vi[3]] - mesh->verts[vi[0]])))
				{
					mesh->add_triangle(vi[0], vi[1], vi[3], shader, smooth);
					mesh->add_triangle(vi[2], vi[3], vi[1], shader, smooth);
					face_flags[fi] |= FACE_FLAG_DIVIDE_24;
				}
				else {
					mesh->add_triangle(vi[0], vi[1], vi[2], shader, smooth);
					mesh->add_triangle(vi[0], vi[2], vi[3], shader, smooth);
					face_flags[fi] |= FACE_FLAG_DIVIDE_13;
				}
			}
			else {
				mesh->add_triangle(vi[0], vi[1], vi[2], shader, smooth);
			}

			nverts[fi] = n;
		}
	}
	else {
		vector<int> vi;

		for(b_mesh.polygons.begin(p); p != b_mesh.polygons.end(); ++p) {
			int n = p->loop_total();
			int shader = clamp(p->material_index(), 0, used_shaders.size()-1);
			bool smooth = p->use_smooth() || use_loop_normals;

			vi.resize(n);
			for(int i = 0; i < n; i++) {
				/* NOTE: Autosmooth is already taken care about. */
				vi[i] = b_mesh.loops[p->loop_start() + i].vertex_index();
			}

			/* create subd faces */
			mesh->add_subd_face(&vi[0], n, shader, smooth);
		}
	}

	/* Create all needed attributes.
	 * The calculate functions will check whether they're needed or not.
	 */
	attr_create_pointiness(scene, mesh, b_mesh, subdivision);
	attr_create_vertex_color(scene, mesh, b_mesh, nverts, face_flags, subdivision);
	attr_create_uv_map(scene, mesh, b_mesh, nverts, face_flags, subdivision, subdivide_uvs);

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

static void create_subd_mesh(Scene *scene,
                             Mesh *mesh,
                             BL::Object& b_ob,
                             BL::Mesh& b_mesh,
                             const vector<Shader*>& used_shaders,
                             float dicing_rate,
                             int max_subdivisions)
{
	BL::SubsurfModifier subsurf_mod(b_ob.modifiers[b_ob.modifiers.length()-1]);
	bool subdivide_uvs = subsurf_mod.use_subsurf_uv();

	create_mesh(scene, mesh, b_mesh, used_shaders, true, subdivide_uvs);

	/* export creases */
	size_t num_creases = 0;
	BL::Mesh::edges_iterator e;

	for(b_mesh.edges.begin(e); e != b_mesh.edges.end(); ++e) {
		if(e->crease() != 0.0f) {
			num_creases++;
		}
	}

	mesh->subd_creases.resize(num_creases);

	Mesh::SubdEdgeCrease* crease = mesh->subd_creases.data();
	for(b_mesh.edges.begin(e); e != b_mesh.edges.end(); ++e) {
		if(e->crease() != 0.0f) {
			crease->v[0] = e->vertices()[0];
			crease->v[1] = e->vertices()[1];
			crease->crease = e->crease();

			crease++;
		}
	}

	/* set subd params */
	if(!mesh->subd_params) {
		mesh->subd_params = new SubdParams(mesh);
	}
	SubdParams& sdparams = *mesh->subd_params;

	PointerRNA cobj = RNA_pointer_get(&b_ob.ptr, "cycles");

	sdparams.dicing_rate = max(0.1f, RNA_float_get(&cobj, "dicing_rate") * dicing_rate);
	sdparams.max_level = max_subdivisions;

	scene->camera->update();
	sdparams.camera = scene->camera;
	sdparams.objecttoworld = get_transform(b_ob.matrix_world());
}

/* Sync */

static void sync_mesh_fluid_motion(BL::Object& b_ob, Scene *scene, Mesh *mesh)
{
	if(scene->need_motion() == Scene::MOTION_NONE)
		return;

	BL::DomainFluidSettings b_fluid_domain = object_fluid_domain_find(b_ob);

	if(!b_fluid_domain)
		return;

	/* If the mesh has modifiers following the fluid domain we can't export motion. */
	if(b_fluid_domain.fluid_mesh_vertices.length() != mesh->verts.size())
		return;

	/* Find or add attribute */
	float3 *P = &mesh->verts[0];
	Attribute *attr_mP = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

	if(!attr_mP) {
		attr_mP = mesh->attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
	}

	/* Only export previous and next frame, we don't have any in between data. */
	float motion_times[2] = {-1.0f, 1.0f};
	for(int step = 0; step < 2; step++) {
		float relative_time = motion_times[step] * scene->motion_shutter_time() * 0.5f;
		float3 *mP = attr_mP->data_float3() + step*mesh->verts.size();

		BL::DomainFluidSettings::fluid_mesh_vertices_iterator fvi;
		int i = 0;

		for(b_fluid_domain.fluid_mesh_vertices.begin(fvi); fvi != b_fluid_domain.fluid_mesh_vertices.end(); ++fvi, ++i) {
			mP[i] = P[i] + get_float3(fvi->velocity()) * relative_time;
		}
	}
}

Mesh *BlenderSync::sync_mesh(BL::Object& b_ob,
                             bool object_updated,
                             bool hide_tris)
{
	/* When viewport display is not needed during render we can force some
	 * caches to be releases from blender side in order to reduce peak memory
	 * footprint during synchronization process.
	 */
	const bool is_interface_locked = b_engine.render() &&
	                                 b_engine.render().use_lock_interface();
	const bool can_free_caches = BlenderSession::headless || is_interface_locked;

	/* test if we can instance or if the object is modified */
	BL::ID b_ob_data = b_ob.data();
	BL::ID key = (BKE_object_is_modified(b_ob))? b_ob: b_ob_data;
	BL::Material material_override = render_layer.material_override;

	/* find shader indices */
	vector<Shader*> used_shaders;

	BL::Object::material_slots_iterator slot;
	for(b_ob.material_slots.begin(slot); slot != b_ob.material_slots.end(); ++slot) {
		if(material_override) {
			find_shader(material_override, used_shaders, scene->default_surface);
		}
		else {
			BL::ID b_material(slot->material());
			find_shader(b_material, used_shaders, scene->default_surface);
		}
	}

	if(used_shaders.size() == 0) {
		if(material_override)
			find_shader(material_override, used_shaders, scene->default_surface);
		else
			used_shaders.push_back(scene->default_surface);
	}

	/* test if we need to sync */
	int requested_geometry_flags = Mesh::GEOMETRY_NONE;
	if(render_layer.use_surfaces) {
		requested_geometry_flags |= Mesh::GEOMETRY_TRIANGLES;
	}
	if(render_layer.use_hair) {
		requested_geometry_flags |= Mesh::GEOMETRY_CURVES;
	}
	Mesh *mesh;

	if(!mesh_map.sync(&mesh, key)) {
		/* if transform was applied to mesh, need full update */
		if(object_updated && mesh->transform_applied);
		/* test if shaders changed, these can be object level so mesh
		 * does not get tagged for recalc */
		else if(mesh->used_shaders != used_shaders);
		else if(requested_geometry_flags != mesh->geometry_flags);
		else {
			/* even if not tagged for recalc, we may need to sync anyway
			 * because the shader needs different mesh attributes */
			bool attribute_recalc = false;

			foreach(Shader *shader, mesh->used_shaders)
				if(shader->need_update_attributes)
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
	array<int> oldtriangle = mesh->triangles;

	/* compares curve_keys rather than strands in order to handle quick hair
	 * adjustments in dynamic BVH - other methods could probably do this better*/
	array<float3> oldcurve_keys = mesh->curve_keys;
	array<float> oldcurve_radius = mesh->curve_radius;

	mesh->clear();
	mesh->used_shaders = used_shaders;
	mesh->name = ustring(b_ob_data.name().c_str());

	if(requested_geometry_flags != Mesh::GEOMETRY_NONE) {
		/* mesh objects does have special handle in the dependency graph,
		 * they're ensured to have properly updated.
		 *
		 * updating meshes here will end up having derived mesh referencing
		 * freed data from the blender side.
		 */
		if(preview && b_ob.type() != BL::Object::type_MESH)
			b_ob.update_from_editmode();

		bool need_undeformed = mesh->need_attribute(scene, ATTR_STD_GENERATED);

		mesh->subdivision_type = object_subdivision_type(b_ob, preview, experimental);

		/* Disable adaptive subdivision while baking as the baking system
		 * currently doesnt support the topology and will crash.
		 */
		if(scene->bake_manager->get_baking()) {
			mesh->subdivision_type = Mesh::SUBDIVISION_NONE;
		}

		BL::Mesh b_mesh = object_to_mesh(b_data,
		                                 b_ob,
		                                 b_scene,
		                                 true,
		                                 !preview,
		                                 need_undeformed,
		                                 mesh->subdivision_type);

		if(b_mesh) {
			if(render_layer.use_surfaces && !hide_tris) {
				if(mesh->subdivision_type != Mesh::SUBDIVISION_NONE)
					create_subd_mesh(scene, mesh, b_ob, b_mesh, used_shaders,
					                 dicing_rate, max_subdivisions);
				else
					create_mesh(scene, mesh, b_mesh, used_shaders, false);

				create_mesh_volume_attributes(scene, b_ob, mesh, b_scene.frame_current());
			}

			if(render_layer.use_hair && mesh->subdivision_type == Mesh::SUBDIVISION_NONE)
				sync_curves(mesh, b_mesh, b_ob, false);

			if(can_free_caches) {
				b_ob.cache_release();
			}

			/* free derived mesh */
			b_data.meshes.remove(b_mesh, false, true, false);
		}
	}
	mesh->geometry_flags = requested_geometry_flags;

	/* fluid motion */
	sync_mesh_fluid_motion(b_ob, scene, mesh);

	/* tag update */
	bool rebuild = false;

	if(oldtriangle.size() != mesh->triangles.size())
		rebuild = true;
	else if(oldtriangle.size()) {
		if(memcmp(&oldtriangle[0], &mesh->triangles[0], sizeof(int)*oldtriangle.size()) != 0)
			rebuild = true;
	}

	if(oldcurve_keys.size() != mesh->curve_keys.size())
		rebuild = true;
	else if(oldcurve_keys.size()) {
		if(memcmp(&oldcurve_keys[0], &mesh->curve_keys[0], sizeof(float3)*oldcurve_keys.size()) != 0)
			rebuild = true;
	}

	if(oldcurve_radius.size() != mesh->curve_radius.size())
		rebuild = true;
	else if(oldcurve_radius.size()) {
		if(memcmp(&oldcurve_radius[0], &mesh->curve_radius[0], sizeof(float)*oldcurve_radius.size()) != 0)
			rebuild = true;
	}

	mesh->tag_update(scene, rebuild);

	return mesh;
}

void BlenderSync::sync_mesh_motion(BL::Object& b_ob,
                                   Object *object,
                                   float motion_time)
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
	const size_t numverts = mesh->verts.size();
	const size_t numkeys = mesh->curve_keys.size();

	if(!numverts && !numkeys)
		return;

	/* skip objects without deforming modifiers. this is not totally reliable,
	 * would need a more extensive check to see which objects are animated */
	BL::Mesh b_mesh(PointerRNA_NULL);

	/* fluid motion is exported immediate with mesh, skip here */
	BL::DomainFluidSettings b_fluid_domain = object_fluid_domain_find(b_ob);
	if(b_fluid_domain)
		return;

	if(ccl::BKE_object_is_deform_modified(b_ob, b_scene, preview)) {
		/* get derived mesh */
		b_mesh = object_to_mesh(b_data,
		                        b_ob,
		                        b_scene,
		                        true,
		                        !preview,
		                        false,
		                        Mesh::SUBDIVISION_NONE);
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
				float3 *keys = &mesh->curve_keys[0];
				memcpy(attr_mP->data_float3() + time_index*numkeys, keys, sizeof(float3)*numkeys);
			}
		}

		return;
	}

	/* TODO(sergey): Perform preliminary check for number of verticies. */
	if(numverts) {
		/* Find attributes. */
		Attribute *attr_mP = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
		Attribute *attr_mN = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_NORMAL);
		Attribute *attr_N = mesh->attributes.find(ATTR_STD_VERTEX_NORMAL);
		bool new_attribute = false;
		/* Add new attributes if they don't exist already. */
		if(!attr_mP) {
			attr_mP = mesh->attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
			if(attr_N)
				attr_mN = mesh->attributes.add(ATTR_STD_MOTION_VERTEX_NORMAL);

			new_attribute = true;
		}
		/* Load vertex data from mesh. */
		float3 *mP = attr_mP->data_float3() + time_index*numverts;
		float3 *mN = (attr_mN)? attr_mN->data_float3() + time_index*numverts: NULL;
		/* NOTE: We don't copy more that existing amount of vertices to prevent
		 * possible memory corruption.
		 */
		BL::Mesh::vertices_iterator v;
		int i = 0;
		for(b_mesh.vertices.begin(v); v != b_mesh.vertices.end() && i < numverts; ++v, ++i) {
			mP[i] = get_float3(v->co());
			if(mN)
				mN[i] = get_float3(v->normal());
		}
		if(new_attribute) {
			/* In case of new attribute, we verify if there really was any motion. */
			if(b_mesh.vertices.length() != numverts ||
			   memcmp(mP, &mesh->verts[0], sizeof(float3)*numverts) == 0)
			{
				/* no motion, remove attributes again */
				if(b_mesh.vertices.length() != numverts) {
					VLOG(1) << "Topology differs, disabling motion blur for object "
					        << b_ob.name();
				}
				else {
					VLOG(1) << "No actual deformation motion for object "
					        << b_ob.name();
				}
				mesh->attributes.remove(ATTR_STD_MOTION_VERTEX_POSITION);
				if(attr_mN)
					mesh->attributes.remove(ATTR_STD_MOTION_VERTEX_NORMAL);
			}
			else if(time_index > 0) {
				VLOG(1) << "Filling deformation motion for object " << b_ob.name();
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
		else {
			if(b_mesh.vertices.length() != numverts) {
				VLOG(1) << "Topology differs, discarding motion blur for object "
				        << b_ob.name() << " at time " << time_index;
				memcpy(mP, &mesh->verts[0], sizeof(float3)*numverts);
				if(mN != NULL) {
					memcpy(mN, attr_N->data_float3(), sizeof(float3)*numverts);
				}
			}
		}
	}

	/* hair motion */
	if(numkeys)
		sync_curves(mesh, b_mesh, b_ob, true, time_index);

	/* free derived mesh */
	b_data.meshes.remove(b_mesh, false, true, false);
}

CCL_NAMESPACE_END
