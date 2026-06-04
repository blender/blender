/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "bvh/bvh.h"

#include "device/device.h"

#include "scene/attribute.h"
#include "scene/camera.h"
#include "scene/geometry.h"
#include "scene/hair.h"
#include "scene/light.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/osl.h"
#include "scene/pointcloud.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_nodes.h"

#include "util/progress.h"

CCL_NAMESPACE_BEGIN

void GeometryManager::device_update_mesh(Device * /*unused*/,
                                         DeviceScene *dscene,
                                         Scene *scene,
                                         Progress &progress)
{
  /* Count. */
  size_t tri_size = 0;

  size_t curve_size = 0;
  size_t curve_segment_size = 0;

  size_t point_size = 0;

  for (Geometry *geom : scene->geometry) {
    if (geom->is_mesh() || geom->is_volume()) {
      Mesh *mesh = static_cast<Mesh *>(geom);

      tri_size += mesh->num_triangles();
    }
    else if (geom->is_hair()) {
      Hair *hair = static_cast<Hair *>(geom);

      curve_size += hair->num_curves();
      curve_segment_size += hair->num_segments();
    }
    else if (geom->is_pointcloud()) {
      PointCloud *pointcloud = static_cast<PointCloud *>(geom);
      point_size += pointcloud->num_points();
    }
  }

  /* Fill in all the arrays. */
  if (tri_size != 0) {
    progress.set_status("Updating Mesh", "Computing normals");

    uint *tri_shader = dscene->tri_shader.alloc(tri_size);
    packed_uint3 *tri_vindex = dscene->tri_vindex.alloc(tri_size);

    const bool copy_all_data = dscene->tri_shader.need_realloc() ||
                               dscene->tri_vindex.need_realloc();

    for (Geometry *geom : scene->geometry) {
      if (geom->is_mesh() || geom->is_volume()) {
        Mesh *mesh = static_cast<Mesh *>(geom);

        if (mesh->shader_is_modified() || mesh->smooth_is_modified() ||
            mesh->triangles_is_modified() || copy_all_data)
        {
          mesh->pack_shaders(scene, &tri_shader[mesh->prim_offset]);
        }

        if (mesh->triangles_is_modified() || copy_all_data) {
          mesh->pack_triangles(&tri_vindex[mesh->prim_offset]);
        }

        if (progress.get_cancel()) {
          return;
        }
      }
    }

    /* vertex coordinates */
    progress.set_status("Updating Mesh", "Copying Mesh to device");

    dscene->tri_shader.copy_to_device_if_modified();
    dscene->tri_vindex.copy_to_device_if_modified();
  }

  if (curve_segment_size != 0) {
    progress.set_status("Updating Mesh", "Copying Curves to device");

    KernelCurve *curves = dscene->curves.alloc(curve_size);
    KernelCurveSegment *curve_segments = dscene->curve_segments.alloc(curve_segment_size);

    const bool copy_all_data = dscene->curves.need_realloc() ||
                               dscene->curve_segments.need_realloc();

    for (Geometry *geom : scene->geometry) {
      if (geom->is_hair()) {
        Hair *hair = static_cast<Hair *>(geom);

        const bool curve_data_modified = hair->curve_shader_is_modified() ||
                                         hair->curve_first_key_is_modified();

        if (!curve_data_modified && !copy_all_data) {
          continue;
        }

        hair->pack_curves(
            scene, &curves[hair->prim_offset], &curve_segments[hair->curve_segment_offset]);
        if (progress.get_cancel()) {
          return;
        }
      }
    }

    dscene->curves.copy_to_device_if_modified();
    dscene->curve_segments.copy_to_device_if_modified();
  }

  if (point_size != 0) {
    progress.set_status("Updating Mesh", "Copying Point clouds to device");

    uint *points_shader = dscene->points_shader.alloc(point_size);

    for (Geometry *geom : scene->geometry) {
      if (geom->is_pointcloud()) {
        PointCloud *pointcloud = static_cast<PointCloud *>(geom);
        pointcloud->pack(scene, &points_shader[pointcloud->prim_offset]);
        if (progress.get_cancel()) {
          return;
        }
      }
    }

    dscene->points_shader.copy_to_device();
  }
}

CCL_NAMESPACE_END
