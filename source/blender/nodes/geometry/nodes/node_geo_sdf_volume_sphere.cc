/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/LevelSetSphere.h>
#endif

#include "node_geometry_util.hh"

#include "BKE_geometry_set.hh"
#include "BKE_lib_id.h"
#include "BKE_volume.h"

#include "NOD_add_node_search.hh"
#include "NOD_socket_search_link.hh"

namespace blender::nodes::node_geo_sdf_volume_sphere_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Radius").default_value(1.0f).min(0.0f).subtype(PROP_DISTANCE);
  b.add_input<decl::Float>("Voxel Size").default_value(0.2f).min(0.01f).subtype(PROP_DISTANCE);
  b.add_input<decl::Float>("Half-Band Width")
      .description("Half the width of the narrow band in voxel units")
      .default_value(3.0f)
      .min(1.01f)
      .max(10.0f);
  b.add_output<decl::Geometry>("Volume").translation_context(BLT_I18NCONTEXT_ID_ID);
}

static void search_node_add_ops(GatherAddNodeSearchParams &params)
{
  if (U.experimental.use_new_volume_nodes) {
    blender::nodes::search_node_add_ops_for_basic_node(params);
  }
}

static void search_link_ops(GatherLinkSearchOpParams &params)
{
  if (U.experimental.use_new_volume_nodes) {
    blender::nodes::search_link_ops_for_basic_node(params);
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  float radius = params.extract_input<float>("Radius");
  float voxel_size = params.extract_input<float>("Voxel Size");
  float half_width = params.extract_input<float>("Half-Band Width");

  if (radius <= 0.0f) {
    params.error_message_add(NodeWarningType::Error, TIP_("Radius must be greater than 0"));
    params.set_default_remaining_outputs();
    return;
  }

  if (half_width <= 1.0f) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("Half-band width must be greater than 1"));
    params.set_default_remaining_outputs();
    return;
  }

  openvdb::FloatGrid::Ptr grid;

  try {
    grid = openvdb::tools::createLevelSetSphere<openvdb::FloatGrid>(
        radius, openvdb::Vec3f(0, 0, 0), voxel_size, half_width);
  }
  catch (openvdb::ArithmeticError &) {
    params.error_message_add(NodeWarningType::Error, TIP_("Voxel size is too small"));
    params.set_default_remaining_outputs();
    return;
  }
  Volume *volume = reinterpret_cast<Volume *>(BKE_id_new_nomain(ID_VO, nullptr));
  BKE_volume_grid_add_vdb(*volume, "distance", std::move(grid));

  GeometrySet r_geometry_set = GeometrySet::create_with_volume(volume);
  params.set_output("Volume", r_geometry_set);
#else
  params.set_default_remaining_outputs();
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without OpenVDB"));
#endif
}

}  // namespace blender::nodes::node_geo_sdf_volume_sphere_cc

void register_node_type_geo_sdf_volume_sphere()
{
  namespace file_ns = blender::nodes::node_geo_sdf_volume_sphere_cc;
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_SDF_VOLUME_SPHERE, "SDF Volume Sphere", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  blender::bke::node_type_size(&ntype, 180, 120, 300);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.gather_add_node_search_ops = file_ns::search_node_add_ops;
  ntype.gather_link_search_ops = file_ns::search_link_ops;
  nodeRegisterType(&ntype);
}
