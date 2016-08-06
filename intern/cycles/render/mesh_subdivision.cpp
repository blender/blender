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

#include "mesh.h"
#include "attribute.h"

#include "subd_split.h"
#include "subd_patch.h"

#include "util_foreach.h"

CCL_NAMESPACE_BEGIN

void Mesh::tessellate(DiagSplit *split)
{
	int num_faces = subd_faces.size();

	Attribute *attr_vN = subd_attributes.find(ATTR_STD_VERTEX_NORMAL);
	float3* vN = attr_vN->data_float3();

	for(int f = 0; f < num_faces; f++) {
		SubdFace& face = subd_faces[f];

		if(face.is_quad()) {
			/* quad */
			LinearQuadPatch patch;
			float3 *hull = patch.hull;
			float3 *normals = patch.normals;

			patch.patch_index = face.ptex_offset;
			patch.shader = face.shader;

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

			/* Quad faces need to be split at least once to line up with split ngons, we do this
			 * here in this manner because if we do it later edge factors may end up slightly off.
			 */
			QuadDice::SubPatch subpatch;
			subpatch.patch = &patch;

			subpatch.P00 = make_float2(0.0f, 0.0f);
			subpatch.P10 = make_float2(0.5f, 0.0f);
			subpatch.P01 = make_float2(0.0f, 0.5f);
			subpatch.P11 = make_float2(0.5f, 0.5f);
			split->split_quad(&patch, &subpatch);

			subpatch.P00 = make_float2(0.5f, 0.0f);
			subpatch.P10 = make_float2(1.0f, 0.0f);
			subpatch.P01 = make_float2(0.5f, 0.5f);
			subpatch.P11 = make_float2(1.0f, 0.5f);
			split->split_quad(&patch, &subpatch);

			subpatch.P00 = make_float2(0.0f, 0.5f);
			subpatch.P10 = make_float2(0.5f, 0.5f);
			subpatch.P01 = make_float2(0.0f, 1.0f);
			subpatch.P11 = make_float2(0.5f, 1.0f);
			split->split_quad(&patch, &subpatch);

			subpatch.P00 = make_float2(0.5f, 0.5f);
			subpatch.P10 = make_float2(1.0f, 0.5f);
			subpatch.P01 = make_float2(0.5f, 1.0f);
			subpatch.P11 = make_float2(1.0f, 1.0f);
			split->split_quad(&patch, &subpatch);
		}
		else {
			/* ngon */
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

	/* interpolate center points for attributes */
	foreach(Attribute& attr, subd_attributes.attributes) {
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
}

CCL_NAMESPACE_END

