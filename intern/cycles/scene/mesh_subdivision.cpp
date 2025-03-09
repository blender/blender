/* SPDX-FileCopyrightText: 2011-2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/attribute.h"
#include "scene/mesh.h"

#include "subd/interpolation.h"
#include "subd/osd.h"
#include "subd/patch.h"
#include "subd/split.h"

#include "util/algorithm.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

void Mesh::tessellate(DiagSplit *split)
{
  /* reset the number of subdivision vertices, in case the Mesh was not cleared
   * between calls or data updates */
  num_subd_added_verts = 0;

#ifdef WITH_OPENSUBDIV
  OsdMesh osd_mesh(*this);
  OsdData osd_data;

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
  }

  /* count patches */
  const int num_faces = get_num_subd_faces();
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

    /* Split patches. */
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
        /* Simple quad case. */
        float3 *hull = patch->hull;

        patch->patch_index = face.ptex_offset;
        patch->from_ngon = false;

        hull[0] = verts[subd_face_corners[face.start_corner + 0]];
        hull[1] = verts[subd_face_corners[face.start_corner + 1]];
        hull[2] = verts[subd_face_corners[face.start_corner + 3]];
        hull[3] = verts[subd_face_corners[face.start_corner + 2]];

        patch->shader = face.shader;
        patch++;
      }
      else {
        /* N-gon split into N quads. */
        float3 center_vert = zero_float3();

        const float inv_num_corners = 1.0f / float(face.num_corners);
        for (int corner = 0; corner < face.num_corners; corner++) {
          center_vert += verts[subd_face_corners[face.start_corner + corner]] * inv_num_corners;
        }

        for (int corner = 0; corner < face.num_corners; corner++) {
          float3 *hull = patch->hull;

          patch->patch_index = face.ptex_offset + corner;
          patch->from_ngon = true;

          patch->shader = face.shader;

          const int v0 = subd_face_corners[face.start_corner + mod(corner + 0, face.num_corners)];
          const int v1 = subd_face_corners[face.start_corner + mod(corner + 1, face.num_corners)];
          const int v3 = subd_face_corners[face.start_corner +
                                           mod(corner + face.num_corners - 1, face.num_corners)];

          hull[0] = verts[v0];
          hull[1] = 0.5f * (verts[v0] + verts[v1]);
          hull[2] = 0.5f * (verts[v3] + verts[v0]);
          hull[3] = center_vert;

          patch++;
        }
      }
    }

    /* Split patches. */
    split->split_patches(linear_patches.data(), sizeof(LinearQuadPatch));
  }

  if (get_num_subd_faces()) {
    /* Create a tessellated mesh attributes from subd base mesh attributes. */
#ifdef WITH_OPENSUBDIV
    SubdAttributeInterpolation interpolation(*this, osd_mesh, osd_data, num_patches);
#else
    SubdAttributeInterpolation interpolation(*this, num_patches);
#endif

    for (const Attribute &subd_attr : subd_attributes.attributes) {
      if (!interpolation.support_interp_attribute(subd_attr)) {
        continue;
      }
      Attribute &mesh_attr = attributes.copy(subd_attr);
      interpolation.interp_attribute(subd_attr, mesh_attr);
    }
  }

  // TODO: Free subd base data? Or will this break interactive updates?

  // TODO: Use ATTR_STD_PTEX attributes instead, and create only for lifetime of this function
  // if there are attributes to interpolation. And then keep only if needed for ptex texturing

  /* Clear temporary buffers needed for interpolation. */
  subd_triangle_patch_index.clear();
  subd_corner_patch_uv.clear();
}

CCL_NAMESPACE_END
