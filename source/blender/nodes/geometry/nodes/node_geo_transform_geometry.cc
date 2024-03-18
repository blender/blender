/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.hh"
#include "BLI_task.hh"

#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_volume_types.h"

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_mesh.hh"
#include "BKE_pointcloud.hh"
#include "BKE_volume.hh"

#include "DEG_depsgraph_query.hh"

#include "GEO_transform.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_transform_geometry_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::Vector>("Translation").subtype(PROP_TRANSLATION);
  b.add_input<decl::Rotation>("Rotation");
  b.add_input<decl::Vector>("Scale").default_value({1, 1, 1}).subtype(PROP_XYZ);
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

static bool use_translate(const math::Quaternion &rotation, const float3 scale)
{
  if (math::angle_of(rotation).radian() > 1e-7f) {
    return false;
  }
  if (compare_ff(scale.x, 1.0f, 1e-9f) != 1 || compare_ff(scale.y, 1.0f, 1e-9f) != 1 ||
      compare_ff(scale.z, 1.0f, 1e-9f) != 1)
  {
    return false;
  }
  return true;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  const float3 translation = params.extract_input<float3>("Translation");
  const math::Quaternion rotation = params.extract_input<math::Quaternion>("Rotation");
  const float3 scale = params.extract_input<float3>("Scale");

  /* Use only translation if rotation and scale don't apply. */
  if (use_translate(rotation, scale)) {
    geometry::translate_geometry(geometry_set, translation);
  }
  else {
    if (auto errors = geometry::transform_geometry(
            geometry_set, math::from_loc_rot_scale<float4x4>(translation, rotation, scale)))
    {
      if (errors->volume_too_small) {
        params.error_message_add(NodeWarningType::Warning,
                                 TIP_("Volume scale is lower than permitted by OpenVDB"));
      }
    }
  }

  params.set_output("Geometry", std::move(geometry_set));
}

static void register_node()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_TRANSFORM_GEOMETRY, "Transform Geometry", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(register_node)

}  // namespace blender::nodes::node_geo_transform_geometry_cc
