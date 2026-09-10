/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_pointcloud_types.h"

#include "BKE_pointcloud.hh"

#include "NOD_rna_define.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_enum_types.hh"

#include "GEO_foreach_geometry.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_points_type_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Geometry>("Points"_ustr)
      .supported_type(GeometryComponent::Type::PointCloud)
      .description("Point cloud to change the type of");
  b.add_output<decl::Geometry>("Points"_ustr).propagate_all_geometry().align_with_previous();
  b.add_input<decl::Menu>("Type"_ustr)
    .static_items(rna_enum_pointcloud_type_items)
    .optional_label();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const auto dst_type = params.get_input<PointCloudType>("Type"_ustr);
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Points"_ustr);
  geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry_set) {
    if (!geometry_set.has_pointcloud()) {
      return;
    }
    if (geometry_set.get_pointcloud()->type == dst_type) {
      return;
    }
    PointCloud *dst_pointcloud = geometry_set.get_pointcloud_for_write();
    dst_pointcloud->type = dst_type;
  });
  params.set_output("Points"_ustr, std::move(geometry_set));
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodePointsSetType"_ustr);
  ntype.ui_name = "Set Point Cloud Type";
  ntype.ui_description = "Change the type of point cloud";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_points_type_cc
