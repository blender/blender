/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_camera.h"

#include "DEG_depsgraph_query.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_camera_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();

  b.add_output<decl::Matrix>("Projection Matrix").description("Camera projection matrix");
  b.add_output<decl::Float>("Focal Length").description("Perspective camera focal length");
  b.add_output<decl::Vector>("Sensor").description("Size of the camera sensor");
  b.add_output<decl::Vector>("Shift").description("Camera shift");
  b.add_output<decl::Float>("Clip Start").description("Camera near clipping distance");
  b.add_output<decl::Float>("Clip End").description("Camera far clipping distance");
  b.add_output<decl::Float>("Focus Distance")
      .description("Distance to the focus point for depth of field");
  b.add_output<decl::Bool>("Is Orthographic")
      .description("Whether the camera is using orthographic projection");
  b.add_output<decl::Float>("Orthographic Scale")
      .description("Orthographic camera scale (similar to zoom)");

  b.add_input<decl::Object>("Camera").optional_label();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const Scene *scene = DEG_get_evaluated_scene(params.depsgraph());
  if (!scene) {
    params.set_default_remaining_outputs();
    return;
  }

  const Object *camera_obj = params.extract_input<Object *>("Camera");

  if (!camera_obj || camera_obj->type != OB_CAMERA) {
    params.set_default_remaining_outputs();
    return;
  }

  Camera *camera = static_cast<Camera *>(camera_obj->data);
  if (!camera) {
    params.set_default_remaining_outputs();
    return;
  }

  CameraParams camera_params;
  BKE_camera_params_init(&camera_params);
  BKE_camera_params_from_object(&camera_params, camera_obj);
  BKE_camera_params_compute_viewplane(
      &camera_params, scene->r.xsch, scene->r.ysch, scene->r.xasp, scene->r.yasp);
  BKE_camera_params_compute_matrix(&camera_params);

  const float4x4 projection_matrix(camera_params.winmat);
  float focus_distance = BKE_camera_object_dof_distance(camera_obj);

  params.set_output("Projection Matrix", projection_matrix);
  params.set_output("Focal Length", camera_params.lens);
  params.set_output("Sensor", float3{camera_params.sensor_x, camera_params.sensor_y, 0.0f});
  params.set_output("Shift", float3{camera_params.shiftx, camera_params.shifty, 0.0f});
  params.set_output("Clip Start", camera_params.clip_start);
  params.set_output("Clip End", camera_params.clip_end);
  params.set_output("Focus Distance", focus_distance);
  params.set_output("Is Orthographic", camera_params.is_ortho);
  params.set_output("Orthographic Scale", camera_params.ortho_scale);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeCameraInfo");
  ntype.ui_name = "Camera Info";
  ntype.ui_description = "Retrieve information from a camera object";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_camera_info_cc
