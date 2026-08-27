/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BLI_generic_key_string.hh"
#include "BLI_listbase.hh"
#include "BLI_memory_cache_file_load.hh"
#include "BLI_string.hh"

#include "BKE_report.hh"

#include "IO_spz.hh"

namespace blender::nodes::node_geo_import_spz {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>("Path"_ustr)
      .subtype(PROP_FILEPATH)
      .path_filter("*.spz")
      .optional_label()
      .description("Path to a SPZ file");

  b.add_output<decl::Geometry>("Points"_ustr);
}

class LoadSpzCache : public memory_cache::CachedValue {
 public:
  GeometrySet geometry;
  Vector<NodeWarning> warnings;

  void count_memory(MemoryCounter &counter) const override
  {
    this->geometry.count_memory(counter);
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_IO_SPZ
  const std::optional<std::string> path = params.ensure_absolute_path(
      params.extract_input<std::string>("Path"_ustr));
  if (!path) {
    params.set_default_remaining_outputs();
    return;
  }

  std::shared_ptr<const LoadSpzCache> cached_value = memory_cache::get_loaded<LoadSpzCache>(
      GenericStringKey{"import_spz_node"}, {StringRefNull(*path)}, [&]() {
        SPZImportParams import_params;
        import_params.filepath = *path;

        ReportList reports;
        BKE_reports_init(&reports, RPT_STORE);
        BLI_SCOPED_DEFER([&]() { BKE_reports_free(&reports); })
        import_params.reports = &reports;

        PointCloud *point_cloud = SPZ_import_point_cloud(&import_params);

        auto cached_value = std::make_unique<LoadSpzCache>();
        cached_value->geometry = GeometrySet::from_pointcloud(point_cloud);

        for (Report &report : (import_params.reports)->list) {
          cached_value->warnings.append_as(report);
        }

        return cached_value;
      });

  for (const NodeWarning &warning : cached_value->warnings) {
    params.error_message_add(warning.type, warning.message);
  }

  params.set_output("Points"_ustr, cached_value->geometry);

#else
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without SPZ I/O"));
  params.set_default_remaining_outputs();
#endif
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeImportSPZ"_ustr, GEO_NODE_IMPORT_SPZ);
  ntype.ui_name = "Import SPZ";
  ntype.ui_description =
      "Import a point cloud object that is rendered as gaussian splat from an SPZ file";
  ntype.enum_name_legacy = "IMPORT_SPZ";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_import_spz
