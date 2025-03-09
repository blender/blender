/* SPDX-FileCopyrightText: 2011-2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/attribute.h"
#include "scene/mesh.h"

#include "subd/osd.h"
#include "subd/patch.h"
#include "subd/patch_table.h"
#include "subd/split.h"

#include "util/algorithm.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

void Mesh::tessellate(DiagSplit *split)
{
  /* reset the number of subdivision vertices, in case the Mesh was not cleared
   * between calls or data updates */
  num_subd_verts = 0;

#ifdef WITH_OPENSUBDIV
  OsdMesh osd_mesh(*this);
  OsdData osd_data;
  bool need_packed_patch_table = false;

  if (subdivision_type == SUBDIVISION_CATMULL_CLARK) {
    if (get_num_subd_faces()) {
      osd_data.build(osd_mesh);
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
    for (Attribute &attr : subd_attributes.attributes) {
      attr.flags &= ~ATTR_SUBDIVIDED;
    }
  }

  const int num_faces = get_num_subd_faces();

  Attribute *attr_vN = subd_attributes.find(ATTR_STD_VERTEX_NORMAL);
  float3 *vN = (attr_vN) ? attr_vN->data_float3() : nullptr;

  /* count patches */
  int num_patches = 0;
  for (int f = 0; f < num_faces; f++) {
    SubdFace face = get_subd_face(f);

    if (face.is_quad()) {
      num_patches++;
    }
    else {
      num_patches += face.num_corners;
    }
  }

  /* build patches from faces */
#ifdef WITH_OPENSUBDIV
  if (subdivision_type == SUBDIVISION_CATMULL_CLARK) {
    vector<OsdPatch> osd_patches(num_patches, OsdPatch(osd_data));
    OsdPatch *patch = osd_patches.data();

    for (int f = 0; f < num_faces; f++) {
      SubdFace face = get_subd_face(f);

      if (face.is_quad()) {
        patch->patch_index = face.ptex_offset;
        patch->from_ngon = false;
        patch->shader = face.shader;
        patch++;
      }
      else {
        for (int corner = 0; corner < face.num_corners; corner++) {
          patch->patch_index = face.ptex_offset + corner;
          patch->from_ngon = true;
          patch->shader = face.shader;
          patch++;
        }
      }
    }

    /* split patches */
    split->split_patches(osd_patches.data(), sizeof(OsdPatch));
  }
  else
#endif
  {
    vector<LinearQuadPatch> linear_patches(num_patches);
    LinearQuadPatch *patch = linear_patches.data();

    for (int f = 0; f < num_faces; f++) {
      SubdFace face = get_subd_face(f);

      if (face.is_quad()) {
        float3 *hull = patch->hull;
        float3 *normals = patch->normals;

        patch->patch_index = face.ptex_offset;
        patch->from_ngon = false;

        for (int i = 0; i < 4; i++) {
          hull[i] = verts[subd_face_corners[face.start_corner + i]];
        }

        if (face.smooth) {
          for (int i = 0; i < 4; i++) {
            normals[i] = vN[subd_face_corners[face.start_corner + i]];
          }
        }
        else {
          const float3 N = face.normal(this);
          for (int i = 0; i < 4; i++) {
            normals[i] = N;
          }
        }

        swap(hull[2], hull[3]);
        swap(normals[2], normals[3]);

        patch->shader = face.shader;
        patch++;
      }
      else {
        /* ngon */
        float3 center_vert = zero_float3();
        float3 center_normal = zero_float3();

        const float inv_num_corners = 1.0f / float(face.num_corners);
        for (int corner = 0; corner < face.num_corners; corner++) {
          center_vert += verts[subd_face_corners[face.start_corner + corner]] * inv_num_corners;
          center_normal += vN[subd_face_corners[face.start_corner + corner]] * inv_num_corners;
        }

        for (int corner = 0; corner < face.num_corners; corner++) {
          float3 *hull = patch->hull;
          float3 *normals = patch->normals;

          patch->patch_index = face.ptex_offset + corner;
          patch->from_ngon = true;

          patch->shader = face.shader;

          hull[0] =
              verts[subd_face_corners[face.start_corner + mod(corner + 0, face.num_corners)]];
          hull[1] =
              verts[subd_face_corners[face.start_corner + mod(corner + 1, face.num_corners)]];
          hull[2] =
              verts[subd_face_corners[face.start_corner + mod(corner - 1, face.num_corners)]];
          hull[3] = center_vert;

          hull[1] = (hull[1] + hull[0]) * 0.5;
          hull[2] = (hull[2] + hull[0]) * 0.5;

          if (face.smooth) {
            normals[0] =
                vN[subd_face_corners[face.start_corner + mod(corner + 0, face.num_corners)]];
            normals[1] =
                vN[subd_face_corners[face.start_corner + mod(corner + 1, face.num_corners)]];
            normals[2] =
                vN[subd_face_corners[face.start_corner + mod(corner - 1, face.num_corners)]];
            normals[3] = center_normal;

            normals[1] = (normals[1] + normals[0]) * 0.5;
            normals[2] = (normals[2] + normals[0]) * 0.5;
          }
          else {
            const float3 N = face.normal(this);
            for (int i = 0; i < 4; i++) {
              normals[i] = N;
            }
          }

          patch++;
        }
      }
    }

    /* split patches */
    split->split_patches(linear_patches.data(), sizeof(LinearQuadPatch));
  }

  /* interpolate center points for attributes */
  for (Attribute &attr : subd_attributes.attributes) {
#ifdef WITH_OPENSUBDIV
    if (subdivision_type == SUBDIVISION_CATMULL_CLARK && attr.flags & ATTR_SUBDIVIDED) {
      if (attr.element == ATTR_ELEMENT_CORNER || attr.element == ATTR_ELEMENT_CORNER_BYTE) {
        /* keep subdivision for corner attributes disabled for now */
        attr.flags &= ~ATTR_SUBDIVIDED;
      }
      else if (get_num_subd_faces()) {
        osd_data.subdivide_attribute(attr);

        need_packed_patch_table = true;
        continue;
      }
    }
#endif

    char *data = attr.data();
    const size_t stride = attr.data_sizeof();
    int ngons = 0;

    switch (attr.element) {
      case ATTR_ELEMENT_VERTEX: {
        for (int f = 0; f < num_faces; f++) {
          SubdFace face = get_subd_face(f);

          if (!face.is_quad()) {
            char *center = data + (verts.size() - num_subd_verts + ngons) * stride;
            attr.zero_data(center);

            const float inv_num_corners = 1.0f / float(face.num_corners);

            for (int corner = 0; corner < face.num_corners; corner++) {
              attr.add_with_weight(center,
                                   data + subd_face_corners[face.start_corner + corner] * stride,
                                   inv_num_corners);
            }

            ngons++;
          }
        }
        break;
      }
      case ATTR_ELEMENT_VERTEX_MOTION: {
        // TODO(mai): implement
        break;
      }
      case ATTR_ELEMENT_CORNER: {
        for (int f = 0; f < num_faces; f++) {
          SubdFace face = get_subd_face(f);

          if (!face.is_quad()) {
            char *center = data + (subd_face_corners.size() + ngons) * stride;
            attr.zero_data(center);

            const float inv_num_corners = 1.0f / float(face.num_corners);

            for (int corner = 0; corner < face.num_corners; corner++) {
              attr.add_with_weight(
                  center, data + (face.start_corner + corner) * stride, inv_num_corners);
            }

            ngons++;
          }
        }
        break;
      }
      case ATTR_ELEMENT_CORNER_BYTE: {
        for (int f = 0; f < num_faces; f++) {
          SubdFace face = get_subd_face(f);

          if (!face.is_quad()) {
            uchar *center = (uchar *)data + (subd_face_corners.size() + ngons) * stride;

            const float inv_num_corners = 1.0f / float(face.num_corners);
            float4 val = zero_float4();

            for (int corner = 0; corner < face.num_corners; corner++) {
              for (int i = 0; i < 4; i++) {
                val[i] += float(*(data + (face.start_corner + corner) * stride + i)) *
                          inv_num_corners;
              }
            }

            for (int i = 0; i < 4; i++) {
              center[i] = uchar(min(max(val[i], 0.0f), 255.0f));
            }

            ngons++;
          }
        }
        break;
      }
      default:
        break;
    }
  }

#ifdef WITH_OPENSUBDIV
  /* pack patch tables */
  if (need_packed_patch_table) {
    patch_table = make_unique<PackedPatchTable>();
    patch_table->pack(osd_data.patch_table.get());
  }
#endif
}

CCL_NAMESPACE_END
