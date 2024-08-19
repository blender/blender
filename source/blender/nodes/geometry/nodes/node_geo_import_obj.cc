/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BLI_string.h"

#include "BKE_instances.hh"
#include "BKE_mesh.hh"
#include "BKE_report.hh"

#include "IO_wavefront_obj.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_import_obj {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>("Path")
      .subtype(PROP_FILEPATH)
      .hide_label()
      .description("Path to a OBJ file");

  b.add_output<decl::Geometry>("Instances");
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_IO_WAVEFRONT_OBJ
  const std::string path = params.extract_input<std::string>("Path");
  if (path.empty()) {
    params.set_default_remaining_outputs();
    return;
  }

  OBJImportParams import_params;
  STRNCPY(import_params.filepath, path.c_str());

  ReportList reports;
  BKE_reports_init(&reports, RPT_STORE);
  BLI_SCOPED_DEFER([&]() { BKE_reports_free(&reports); });
  import_params.reports = &reports;

  Vector<bke::GeometrySet> geometries;
  OBJ_import_geometries(&import_params, geometries);

  LISTBASE_FOREACH (Report *, report, &(import_params.reports)->list) {
    NodeWarningType type;
    switch (report->type) {
      case RPT_ERROR:
        type = NodeWarningType::Error;
        break;
      default:
        type = NodeWarningType::Info;
        break;
    }
    params.error_message_add(type, TIP_(report->message));
  }

  if (geometries.is_empty()) {
    params.set_default_remaining_outputs();
    return;
  }

  bke::Instances *instances = new bke::Instances();
  for (GeometrySet geometry : geometries) {
    const int handle = instances->add_reference(bke::InstanceReference{std::move(geometry)});
    instances->add_instance(handle, float4x4::identity());
  }

  params.set_output("Instances", GeometrySet::from_instances(instances));
#else
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without OBJ I/O"));
  params.set_default_remaining_outputs();
#endif
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_IMPORT_OBJ, "Import OBJ", NODE_CLASS_INPUT);

  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = search_link_ops_for_import_node;

  blender::bke::node_register_type(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_import_obj
