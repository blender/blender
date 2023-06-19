/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "bvh/bvh.h"
#include "bvh/bvh2.h"

#include "device/device.h"

#include "scene/attribute.h"
#include "scene/camera.h"
#include "scene/geometry.h"
#include "scene/hair.h"
#include "scene/light.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/pointcloud.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_nodes.h"
#include "scene/stats.h"
#include "scene/volume.h"

#include "subd/patch_table.h"
#include "subd/split.h"

#include "kernel/osl/globals.h"

#include "util/foreach.h"
#include "util/log.h"
#include "util/progress.h"
#include "util/task.h"

CCL_NAMESPACE_BEGIN

void Geometry::compute_bvh(Device *device,
                           DeviceScene *dscene,
                           SceneParams *params,
                           Progress *progress,
                           size_t n,
                           size_t total)
{
  if (progress->get_cancel())
    return;

  compute_bounds();

  const BVHLayout bvh_layout = BVHParams::best_bvh_layout(
      params->bvh_layout, device->get_bvh_layout_mask(dscene->data.kernel_features));
  if (need_build_bvh(bvh_layout)) {
    string msg = "Updating Geometry BVH ";
    if (name.empty())
      msg += string_printf("%u/%u", (uint)(n + 1), (uint)total);
    else
      msg += string_printf("%s %u/%u", name.c_str(), (uint)(n + 1), (uint)total);

    Object object;

    /* Ensure all visibility bits are set at the geometry level BVH. In
     * the object level BVH is where actual visibility is tested. */
    object.set_is_shadow_catcher(true);
    object.set_visibility(~0);

    object.set_geometry(this);

    vector<Geometry *> geometry;
    geometry.push_back(this);
    vector<Object *> objects;
    objects.push_back(&object);

    if (bvh && !need_update_rebuild) {
      progress->set_status(msg, "Refitting BVH");

      bvh->replace_geometry(geometry, objects);

      device->build_bvh(bvh, *progress, true);
    }
    else {
      progress->set_status(msg, "Building BVH");

      BVHParams bparams;
      bparams.use_spatial_split = params->use_bvh_spatial_split;
      bparams.use_compact_structure = params->use_bvh_compact_structure;
      bparams.bvh_layout = bvh_layout;
      bparams.use_unaligned_nodes = dscene->data.bvh.have_curves &&
                                    params->use_bvh_unaligned_nodes;
      bparams.num_motion_triangle_steps = params->num_bvh_time_steps;
      bparams.num_motion_curve_steps = params->num_bvh_time_steps;
      bparams.num_motion_point_steps = params->num_bvh_time_steps;
      bparams.bvh_type = params->bvh_type;
      bparams.curve_subdivisions = params->curve_subdivisions();

      delete bvh;
      bvh = BVH::create(bparams, geometry, objects, device);
      MEM_GUARDED_CALL(progress, device->build_bvh, bvh, *progress, false);
    }
  }

  need_update_rebuild = false;
  need_update_bvh_for_offset = false;
}

void GeometryManager::device_update_bvh(Device *device,
                                        DeviceScene *dscene,
                                        Scene *scene,
                                        Progress &progress)
{
  /* bvh build */
  progress.set_status("Updating Scene BVH", "Building");

  BVHParams bparams;
  bparams.top_level = true;
  bparams.bvh_layout = BVHParams::best_bvh_layout(
      scene->params.bvh_layout, device->get_bvh_layout_mask(dscene->data.kernel_features));
  bparams.use_spatial_split = scene->params.use_bvh_spatial_split;
  bparams.use_unaligned_nodes = dscene->data.bvh.have_curves &&
                                scene->params.use_bvh_unaligned_nodes;
  bparams.num_motion_triangle_steps = scene->params.num_bvh_time_steps;
  bparams.num_motion_curve_steps = scene->params.num_bvh_time_steps;
  bparams.num_motion_point_steps = scene->params.num_bvh_time_steps;
  bparams.bvh_type = scene->params.bvh_type;
  bparams.curve_subdivisions = scene->params.curve_subdivisions();

  VLOG_INFO << "Using " << bvh_layout_name(bparams.bvh_layout) << " layout.";

  const bool can_refit = scene->bvh != nullptr &&
                         (bparams.bvh_layout == BVHLayout::BVH_LAYOUT_OPTIX ||
                          bparams.bvh_layout == BVHLayout::BVH_LAYOUT_METAL);

  BVH *bvh = scene->bvh;
  if (!scene->bvh) {
    bvh = scene->bvh = BVH::create(bparams, scene->geometry, scene->objects, device);
  }

  device->build_bvh(bvh, progress, can_refit);

  if (progress.get_cancel()) {
    return;
  }

  const bool has_bvh2_layout = (bparams.bvh_layout == BVH_LAYOUT_BVH2);

  PackedBVH pack;
  if (has_bvh2_layout) {
    pack = std::move(static_cast<BVH2 *>(bvh)->pack);
  }
  else {
    pack.root_index = -1;
  }

  /* copy to device */
  progress.set_status("Updating Scene BVH", "Copying BVH to device");

  /* When using BVH2, we always have to copy/update the data as its layout is dependent on the
   * BVH's leaf nodes which may be different when the objects or vertices move. */

  if (pack.nodes.size()) {
    dscene->bvh_nodes.steal_data(pack.nodes);
    dscene->bvh_nodes.copy_to_device();
  }
  if (pack.leaf_nodes.size()) {
    dscene->bvh_leaf_nodes.steal_data(pack.leaf_nodes);
    dscene->bvh_leaf_nodes.copy_to_device();
  }
  if (pack.object_node.size()) {
    dscene->object_node.steal_data(pack.object_node);
    dscene->object_node.copy_to_device();
  }
  if (pack.prim_type.size()) {
    dscene->prim_type.steal_data(pack.prim_type);
    dscene->prim_type.copy_to_device();
  }
  if (pack.prim_visibility.size()) {
    dscene->prim_visibility.steal_data(pack.prim_visibility);
    dscene->prim_visibility.copy_to_device();
  }
  if (pack.prim_index.size()) {
    dscene->prim_index.steal_data(pack.prim_index);
    dscene->prim_index.copy_to_device();
  }
  if (pack.prim_object.size()) {
    dscene->prim_object.steal_data(pack.prim_object);
    dscene->prim_object.copy_to_device();
  }
  if (pack.prim_time.size()) {
    dscene->prim_time.steal_data(pack.prim_time);
    dscene->prim_time.copy_to_device();
  }

  dscene->data.bvh.root = pack.root_index;
  dscene->data.bvh.use_bvh_steps = (scene->params.num_bvh_time_steps != 0);
  dscene->data.bvh.curve_subdivisions = scene->params.curve_subdivisions();
  /* The scene handle is set in 'CPUDevice::const_copy_to' and 'OptiXDevice::const_copy_to' */
  dscene->data.device_bvh = 0;
}

CCL_NAMESPACE_END
