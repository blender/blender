/*
 * Copyright 2011-2016 Blender Foundation
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
#include "render/attribute.h"
#include "render/camera.h"

#include "subd/subd_split.h"
#include "subd/subd_patch.h"
#include "subd/subd_patch_table.h"

#include "util/util_foreach.h"
#include "util/util_algorithm.h"

CCL_NAMESPACE_BEGIN

#ifdef WITH_OPENSUBDIV

CCL_NAMESPACE_END

#include <opensubdiv/far/topologyRefinerFactory.h>
#include <opensubdiv/far/primvarRefiner.h>
#include <opensubdiv/far/patchTableFactory.h>
#include <opensubdiv/far/patchMap.h>

/* specializations of TopologyRefinerFactory for ccl::Mesh */

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {
namespace Far {
	template<>
	bool TopologyRefinerFactory<ccl::Mesh>::resizeComponentTopology(TopologyRefiner& refiner, ccl::Mesh const& mesh)
	{
		setNumBaseVertices(refiner, mesh.verts.size());
		setNumBaseFaces(refiner, mesh.subd_faces.size());

		const ccl::Mesh::SubdFace* face = mesh.subd_faces.data();

		for(int i = 0; i < mesh.subd_faces.size(); i++, face++) {
			setNumBaseFaceVertices(refiner, i, face->num_corners);
		}

		return true;
	}

	template<>
	bool TopologyRefinerFactory<ccl::Mesh>::assignComponentTopology(TopologyRefiner& refiner, ccl::Mesh const& mesh)
	{
		const ccl::Mesh::SubdFace* face = mesh.subd_faces.data();

		for(int i = 0; i < mesh.subd_faces.size(); i++, face++) {
			IndexArray face_verts = getBaseFaceVertices(refiner, i);

			int* corner = &mesh.subd_face_corners[face->start_corner];

			for(int j = 0; j < face->num_corners; j++, corner++) {
				face_verts[j] = *corner;
			}
		}

		return true;
	}

	template<>
	bool TopologyRefinerFactory<ccl::Mesh>::assignComponentTags(TopologyRefiner& refiner, ccl::Mesh const& mesh)
	{
		const ccl::Mesh::SubdEdgeCrease* crease = mesh.subd_creases.data();

		for(int i = 0; i < mesh.subd_creases.size(); i++, crease++) {
			Index edge = findBaseEdge(refiner, crease->v[0], crease->v[1]);

			if(edge != INDEX_INVALID) {
				setBaseEdgeSharpness(refiner, edge, crease->crease * 10.0f);
			}
		}

		for(int i = 0; i < mesh.verts.size(); i++) {
			ConstIndexArray vert_edges = getBaseVertexEdges(refiner, i);

			if(vert_edges.size() == 2) {
				float sharpness = refiner.getLevel(0).getEdgeSharpness(vert_edges[0]);
				sharpness = ccl::min(sharpness, refiner.getLevel(0).getEdgeSharpness(vert_edges[1]));

				setBaseVertexSharpness(refiner, i, sharpness);
			}
		}

		return true;
	}

	template<>
	bool TopologyRefinerFactory<ccl::Mesh>::assignFaceVaryingTopology(TopologyRefiner& /*refiner*/, ccl::Mesh const& /*mesh*/)
	{
		return true;
	}

	template<>
	void TopologyRefinerFactory<ccl::Mesh>::reportInvalidTopology(TopologyError /*err_code*/,
		char const * /*msg*/, ccl::Mesh const& /*mesh*/)
	{
	}
} /* namespace Far */
} /* namespace OPENSUBDIV_VERSION */
} /* namespace OpenSubdiv */

CCL_NAMESPACE_BEGIN

using namespace OpenSubdiv;

/* struct that implements OpenSubdiv's vertex interface */

template<typename T>
struct OsdValue {
	T value;

	OsdValue() {}

	void Clear(void* = 0) {
		memset(&value, 0, sizeof(T));
	}

	void AddWithWeight(OsdValue<T> const& src, float weight) {
		value += src.value * weight;
	}
};

template<>
void OsdValue<uchar4>::AddWithWeight(OsdValue<uchar4> const& src, float weight)
{
	for(int i = 0; i < 4; i++) {
		value[i] += (uchar)(src.value[i] * weight);
	}
}

/* class for holding OpenSubdiv data used during tessellation */

class OsdData {
	Mesh* mesh;
	vector<OsdValue<float3> > verts;
	Far::TopologyRefiner* refiner;
	Far::PatchTable* patch_table;
	Far::PatchMap* patch_map;

public:
	OsdData() : mesh(NULL), refiner(NULL), patch_table(NULL), patch_map(NULL) {}

	~OsdData()
	{
		delete refiner;
		delete patch_table;
		delete patch_map;
	}

	void build_from_mesh(Mesh* mesh_)
	{
		mesh = mesh_;

		/* type and options */
		Sdc::SchemeType type = Sdc::SCHEME_CATMARK;

		Sdc::Options options;
		options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_ONLY);

		/* create refiner */
		refiner = Far::TopologyRefinerFactory<Mesh>::Create(*mesh,
				Far::TopologyRefinerFactory<Mesh>::Options(type, options));

		/* adaptive refinement */
		int max_isolation = calculate_max_isolation();
		refiner->RefineAdaptive(Far::TopologyRefiner::AdaptiveOptions(max_isolation));

		/* create patch table */
		Far::PatchTableFactory::Options patch_options;
		patch_options.endCapType = Far::PatchTableFactory::Options::ENDCAP_GREGORY_BASIS;

		patch_table = Far::PatchTableFactory::Create(*refiner, patch_options);

		/* interpolate verts */
		int num_refiner_verts = refiner->GetNumVerticesTotal();
		int num_local_points = patch_table->GetNumLocalPoints();

		verts.resize(num_refiner_verts + num_local_points);
		for(int i = 0; i < mesh->verts.size(); i++) {
			verts[i].value = mesh->verts[i];
		}

		OsdValue<float3>* src = verts.data();
		for(int i = 0; i < refiner->GetMaxLevel(); i++) {
			OsdValue<float3>* dest = src + refiner->GetLevel(i).GetNumVertices();
			Far::PrimvarRefiner(*refiner).Interpolate(i+1, src, dest);
			src = dest;
		}

		if(num_local_points) {
			patch_table->ComputeLocalPointValues(&verts[0], &verts[num_refiner_verts]);
		}

		/* create patch map */
		patch_map = new Far::PatchMap(*patch_table);
	}

	void subdivide_attribute(Attribute& attr)
	{
		Far::PrimvarRefiner primvar_refiner(*refiner);

		if(attr.element == ATTR_ELEMENT_VERTEX) {
			int num_refiner_verts = refiner->GetNumVerticesTotal();
			int num_local_points = patch_table->GetNumLocalPoints();

			attr.resize(num_refiner_verts + num_local_points);
			attr.flags |= ATTR_FINAL_SIZE;

			char* src = attr.buffer.data();

			for(int i = 0; i < refiner->GetMaxLevel(); i++) {
				char* dest = src + refiner->GetLevel(i).GetNumVertices() * attr.data_sizeof();

				if(attr.same_storage(attr.type, TypeDesc::TypeFloat)) {
					primvar_refiner.Interpolate(i+1, (OsdValue<float>*)src, (OsdValue<float>*&)dest);
				}
				else {
					primvar_refiner.Interpolate(i+1, (OsdValue<float4>*)src, (OsdValue<float4>*&)dest);
				}

				src = dest;
			}

			if(num_local_points) {
				if(attr.same_storage(attr.type, TypeDesc::TypeFloat)) {
					patch_table->ComputeLocalPointValues((OsdValue<float>*)&attr.buffer[0],
							                             (OsdValue<float>*)&attr.buffer[num_refiner_verts * attr.data_sizeof()]);
				}
				else {
					patch_table->ComputeLocalPointValues((OsdValue<float4>*)&attr.buffer[0],
							                             (OsdValue<float4>*)&attr.buffer[num_refiner_verts * attr.data_sizeof()]);
				}
			}
		}
		else if(attr.element == ATTR_ELEMENT_CORNER || attr.element == ATTR_ELEMENT_CORNER_BYTE) {
			// TODO(mai): fvar interpolation
		}
	}

	int calculate_max_isolation()
	{
		/* loop over all edges to find longest in screen space */
		const Far::TopologyLevel& level = refiner->GetLevel(0);
		Transform objecttoworld = mesh->subd_params->objecttoworld;
		Camera* cam = mesh->subd_params->camera;

		float longest_edge = 0.0f;

		for(size_t i = 0; i < level.GetNumEdges(); i++) {
			Far::ConstIndexArray verts = level.GetEdgeVertices(i);

			float3 a = mesh->verts[verts[0]];
			float3 b = mesh->verts[verts[1]];

			float edge_len;

			if(cam) {
				a = transform_point(&objecttoworld, a);
				b = transform_point(&objecttoworld, b);

				edge_len = len(a - b) / cam->world_to_raster_size((a + b) * 0.5f);
			}
			else {
				edge_len = len(a - b);
			}

			longest_edge = max(longest_edge, edge_len);
		}

		/* calculate isolation level */
		int isolation = (int)(log2f(max(longest_edge / mesh->subd_params->dicing_rate, 1.0f)) + 1.0f);

		return min(isolation, 10);
	}

	friend struct OsdPatch;
	friend class Mesh;
};

/* ccl::Patch implementation that uses OpenSubdiv for eval */

struct OsdPatch : Patch {
	OsdData* osd_data;

	OsdPatch(OsdData* data) : osd_data(data) {}

	void eval(float3 *P, float3 *dPdu, float3 *dPdv, float3 *N, float u, float v)
	{
		const Far::PatchTable::PatchHandle* handle = osd_data->patch_map->FindPatch(patch_index, u, v);
		assert(handle);

		float p_weights[20], du_weights[20], dv_weights[20];
		osd_data->patch_table->EvaluateBasis(*handle, u, v, p_weights, du_weights, dv_weights);

		Far::ConstIndexArray cv = osd_data->patch_table->GetPatchVertices(*handle);

		float3 du, dv;
		if(P) *P = make_float3(0.0f, 0.0f, 0.0f);
		du = make_float3(0.0f, 0.0f, 0.0f);
		dv = make_float3(0.0f, 0.0f, 0.0f);

		for(int i = 0; i < cv.size(); i++) {
			float3 p = osd_data->verts[cv[i]].value;

			if(P) *P += p * p_weights[i];
			du += p * du_weights[i];
			dv += p * dv_weights[i];
		}

		if(dPdu) *dPdu = du;
		if(dPdv) *dPdv = dv;
		if(N) {
			*N = cross(du, dv);

			float t = len(*N);
			*N = (t != 0.0f) ? *N/t : make_float3(0.0f, 0.0f, 1.0f);
		}
	}

	BoundBox bound() { return BoundBox::empty; }
};

#endif

void Mesh::tessellate(DiagSplit *split)
{
#ifdef WITH_OPENSUBDIV
	OsdData osd_data;
	bool need_packed_patch_table = false;

	if(subdivision_type == SUBDIVISION_CATMULL_CLARK) {
		if(subd_faces.size()) {
			osd_data.build_from_mesh(this);
		}
	}
	else
#endif
	{
		/* force linear subdivision if OpenSubdiv is unavailable to avoid
		 * falling into catmull-clark code paths by accident
		 */
		subdivision_type = SUBDIVISION_LINEAR;

		/* force disable attribute subdivision for same reason as above */
		foreach(Attribute& attr, subd_attributes.attributes) {
			attr.flags &= ~ATTR_SUBDIVIDED;
		}
	}

	int num_faces = subd_faces.size();

	Attribute *attr_vN = subd_attributes.find(ATTR_STD_VERTEX_NORMAL);
	float3* vN = attr_vN->data_float3();

	for(int f = 0; f < num_faces; f++) {
		SubdFace& face = subd_faces[f];

		if(face.is_quad()) {
			/* quad */
			QuadDice::SubPatch subpatch;

			LinearQuadPatch quad_patch;
#ifdef WITH_OPENSUBDIV
			OsdPatch osd_patch(&osd_data);

			if(subdivision_type == SUBDIVISION_CATMULL_CLARK) {
				osd_patch.patch_index = face.ptex_offset;

				subpatch.patch = &osd_patch;
			}
			else
#endif
			{
				float3 *hull = quad_patch.hull;
				float3 *normals = quad_patch.normals;

				quad_patch.patch_index = face.ptex_offset;

				for(int i = 0; i < 4; i++) {
					hull[i] = verts[subd_face_corners[face.start_corner+i]];
				}

				if(face.smooth) {
					for(int i = 0; i < 4; i++) {
						normals[i] = vN[subd_face_corners[face.start_corner+i]];
					}
				}
				else {
					float3 N = face.normal(this);
					for(int i = 0; i < 4; i++) {
						normals[i] = N;
					}
				}

				swap(hull[2], hull[3]);
				swap(normals[2], normals[3]);

				subpatch.patch = &quad_patch;
			}

			subpatch.patch->shader = face.shader;

			/* Quad faces need to be split at least once to line up with split ngons, we do this
			 * here in this manner because if we do it later edge factors may end up slightly off.
			 */
			subpatch.P00 = make_float2(0.0f, 0.0f);
			subpatch.P10 = make_float2(0.5f, 0.0f);
			subpatch.P01 = make_float2(0.0f, 0.5f);
			subpatch.P11 = make_float2(0.5f, 0.5f);
			split->split_quad(subpatch.patch, &subpatch);

			subpatch.P00 = make_float2(0.5f, 0.0f);
			subpatch.P10 = make_float2(1.0f, 0.0f);
			subpatch.P01 = make_float2(0.5f, 0.5f);
			subpatch.P11 = make_float2(1.0f, 0.5f);
			split->split_quad(subpatch.patch, &subpatch);

			subpatch.P00 = make_float2(0.0f, 0.5f);
			subpatch.P10 = make_float2(0.5f, 0.5f);
			subpatch.P01 = make_float2(0.0f, 1.0f);
			subpatch.P11 = make_float2(0.5f, 1.0f);
			split->split_quad(subpatch.patch, &subpatch);

			subpatch.P00 = make_float2(0.5f, 0.5f);
			subpatch.P10 = make_float2(1.0f, 0.5f);
			subpatch.P01 = make_float2(0.5f, 1.0f);
			subpatch.P11 = make_float2(1.0f, 1.0f);
			split->split_quad(subpatch.patch, &subpatch);
		}
		else {
			/* ngon */
#ifdef WITH_OPENSUBDIV
			if(subdivision_type == SUBDIVISION_CATMULL_CLARK) {
				OsdPatch patch(&osd_data);

				patch.shader = face.shader;

				for(int corner = 0; corner < face.num_corners; corner++) {
					patch.patch_index = face.ptex_offset + corner;

					split->split_quad(&patch);
				}
			}
			else
#endif
			{
				float3 center_vert = make_float3(0.0f, 0.0f, 0.0f);
				float3 center_normal = make_float3(0.0f, 0.0f, 0.0f);

				float inv_num_corners = 1.0f/float(face.num_corners);
				for(int corner = 0; corner < face.num_corners; corner++) {
					center_vert += verts[subd_face_corners[face.start_corner + corner]] * inv_num_corners;
					center_normal += vN[subd_face_corners[face.start_corner + corner]] * inv_num_corners;
				}

				for(int corner = 0; corner < face.num_corners; corner++) {
					LinearQuadPatch patch;
					float3 *hull = patch.hull;
					float3 *normals = patch.normals;

					patch.patch_index = face.ptex_offset + corner;

					patch.shader = face.shader;

					hull[0] = verts[subd_face_corners[face.start_corner + mod(corner + 0, face.num_corners)]];
					hull[1] = verts[subd_face_corners[face.start_corner + mod(corner + 1, face.num_corners)]];
					hull[2] = verts[subd_face_corners[face.start_corner + mod(corner - 1, face.num_corners)]];
					hull[3] = center_vert;

					hull[1] = (hull[1] + hull[0]) * 0.5;
					hull[2] = (hull[2] + hull[0]) * 0.5;

					if(face.smooth) {
						normals[0] = vN[subd_face_corners[face.start_corner + mod(corner + 0, face.num_corners)]];
						normals[1] = vN[subd_face_corners[face.start_corner + mod(corner + 1, face.num_corners)]];
						normals[2] = vN[subd_face_corners[face.start_corner + mod(corner - 1, face.num_corners)]];
						normals[3] = center_normal;

						normals[1] = (normals[1] + normals[0]) * 0.5;
						normals[2] = (normals[2] + normals[0]) * 0.5;
					}
					else {
						float3 N = face.normal(this);
						for(int i = 0; i < 4; i++) {
							normals[i] = N;
						}
					}

					split->split_quad(&patch);
				}
			}
		}
	}

	/* interpolate center points for attributes */
	foreach(Attribute& attr, subd_attributes.attributes) {
#ifdef WITH_OPENSUBDIV
		if(subdivision_type == SUBDIVISION_CATMULL_CLARK && attr.flags & ATTR_SUBDIVIDED) {
			if(attr.element == ATTR_ELEMENT_CORNER || attr.element == ATTR_ELEMENT_CORNER_BYTE) {
				/* keep subdivision for corner attributes disabled for now */
				attr.flags &= ~ATTR_SUBDIVIDED;
			}
			else if(subd_faces.size()) {
				osd_data.subdivide_attribute(attr);

				need_packed_patch_table = true;
				continue;
			}
		}
#endif

		char* data = attr.data();
		size_t stride = attr.data_sizeof();
		int ngons = 0;

		switch(attr.element) {
			case ATTR_ELEMENT_VERTEX: {
				for(int f = 0; f < num_faces; f++) {
					SubdFace& face = subd_faces[f];

					if(!face.is_quad()) {
						char* center = data + (verts.size() - num_subd_verts + ngons) * stride;
						attr.zero_data(center);

						float inv_num_corners = 1.0f / float(face.num_corners);

						for(int corner = 0; corner < face.num_corners; corner++) {
							attr.add_with_weight(center,
							                     data + subd_face_corners[face.start_corner + corner] * stride,
							                     inv_num_corners);
						}

						ngons++;
					}
				}
			} break;
			case ATTR_ELEMENT_VERTEX_MOTION: {
				// TODO(mai): implement
			} break;
			case ATTR_ELEMENT_CORNER: {
				for(int f = 0; f < num_faces; f++) {
					SubdFace& face = subd_faces[f];

					if(!face.is_quad()) {
						char* center = data + (subd_face_corners.size() + ngons) * stride;
						attr.zero_data(center);

						float inv_num_corners = 1.0f / float(face.num_corners);

						for(int corner = 0; corner < face.num_corners; corner++) {
							attr.add_with_weight(center,
							                     data + (face.start_corner + corner) * stride,
							                     inv_num_corners);
						}

						ngons++;
					}
				}
			} break;
			case ATTR_ELEMENT_CORNER_BYTE: {
				for(int f = 0; f < num_faces; f++) {
					SubdFace& face = subd_faces[f];

					if(!face.is_quad()) {
						uchar* center = (uchar*)data + (subd_face_corners.size() + ngons) * stride;

						float inv_num_corners = 1.0f / float(face.num_corners);
						float4 val = make_float4(0.0f, 0.0f, 0.0f, 0.0f);

						for(int corner = 0; corner < face.num_corners; corner++) {
							for(int i = 0; i < 4; i++) {
								val[i] += float(*(data + (face.start_corner + corner) * stride + i)) * inv_num_corners;
							}
						}

						for(int i = 0; i < 4; i++) {
							center[i] = uchar(min(max(val[i], 0.0f), 255.0f));
						}

						ngons++;
					}
				}
			} break;
			default: break;
		}
	}

#ifdef WITH_OPENSUBDIV
	/* pack patch tables */
	if(need_packed_patch_table) {
		delete patch_table;
		patch_table = new PackedPatchTable;
		patch_table->pack(osd_data.patch_table);
	}
#endif
}

CCL_NAMESPACE_END
