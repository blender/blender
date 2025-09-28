/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "BLI_generic_key_string.hh"
#include "BLI_listbase.h"
#include "BLI_memory_cache_file_load.hh"
#include "BLI_string.h"

#include "BKE_report.hh"

#include "IO_csv.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_import_csv {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>("Path")
      .subtype(PROP_FILEPATH)
      .path_filter("*.csv")
      .optional_label()
      .description("Path to a CSV file");
  b.add_input<decl::String>("Delimiter").default_value(",");

  b.add_output<decl::Geometry>("Point Cloud");
}

class LoadCsvCache : public memory_cache::CachedValue {
 public:
  GeometrySet geometry;
  Vector<geo_eval_log::NodeWarning> warnings;

  void count_memory(MemoryCounter &counter) const override
  {
    this->geometry.count_memory(counter);
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const std::optional<std::string> path = params.ensure_absolute_path(
      params.extract_input<std::string>("Path"));
  if (!path) {
    params.set_default_remaining_outputs();
    return;
  }
  const std::string delimiter = params.extract_input<std::string>("Delimiter");
  if (delimiter.size() != 1) {
    params.error_message_add(NodeWarningType::Error, TIP_("Delimiter must be a single character"));
    params.set_default_remaining_outputs();
    return;
  }
  if (ELEM(delimiter[0], '\n', '\r', '"', '\\')) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("Delimiter must not be \\n, \\r, \" or \\"));
    params.set_default_remaining_outputs();
    return;
  }

  /* Encode delimiter in key because it affects the result. */
  const std::string loader_key = fmt::format("import_csv_node_{}", delimiter[0]);
  std::shared_ptr<const LoadCsvCache> cached_value = memory_cache::get_loaded<LoadCsvCache>(
      GenericStringKey{loader_key}, {StringRefNull(*path)}, [&]() {
        blender::io::csv::CSVImportParams import_params{};
        import_params.delimiter = delimiter[0];
        STRNCPY(import_params.filepath, path->c_str());

        ReportList reports;
        BKE_reports_init(&reports, RPT_STORE);
        BLI_SCOPED_DEFER([&]() { BKE_reports_free(&reports); });
        import_params.reports = &reports;

        PointCloud *pointcloud = blender::io::csv::import_csv_as_pointcloud(import_params);

        auto cached_value = std::make_unique<LoadCsvCache>();
        cached_value->geometry = GeometrySet::from_pointcloud(pointcloud);

        LISTBASE_FOREACH (Report *, report, &(import_params.reports)->list) {
          cached_value->warnings.append_as(*report);
        }
        return cached_value;
      });

  for (const geo_eval_log::NodeWarning &warning : cached_value->warnings) {
    params.error_message_add(warning.type, warning.message);
  }

  params.set_output("Point Cloud", cached_value->geometry);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeImportCSV");
  ntype.ui_name = "Import CSV";
  ntype.ui_description = "Import geometry from an CSV file";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_import_csv
