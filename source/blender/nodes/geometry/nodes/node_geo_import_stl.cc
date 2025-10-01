/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BLI_generic_key_string.hh"
#include "BLI_listbase.h"
#include "BLI_memory_cache_file_load.hh"
#include "BLI_string.h"

#include "DNA_mesh_types.h"

#include "BKE_report.hh"

#include "IO_stl.hh"

namespace blender::nodes::node_geo_import_stl {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>("Path")
      .subtype(PROP_FILEPATH)
      .path_filter("*.stl")
      .optional_label()
      .description("Path to a STL file");

  b.add_output<decl::Geometry>("Mesh");
}

class LoadStlCache : public memory_cache::CachedValue {
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
#ifdef WITH_IO_STL
  const std::optional<std::string> path = params.ensure_absolute_path(
      params.extract_input<std::string>("Path"));
  if (!path) {
    params.set_default_remaining_outputs();
    return;
  }

  std::shared_ptr<const LoadStlCache> cached_value = memory_cache::get_loaded<LoadStlCache>(
      GenericStringKey{"import_stl_node"}, {StringRefNull(*path)}, [&]() {
        STLImportParams import_params;
        STRNCPY(import_params.filepath, path->c_str());

        import_params.forward_axis = IO_AXIS_NEGATIVE_Z;
        import_params.up_axis = IO_AXIS_Y;

        ReportList reports;
        BKE_reports_init(&reports, RPT_STORE);
        BLI_SCOPED_DEFER([&]() { BKE_reports_free(&reports); })
        import_params.reports = &reports;

        Mesh *mesh = STL_import_mesh(&import_params);

        auto cached_value = std::make_unique<LoadStlCache>();
        cached_value->geometry = GeometrySet::from_mesh(mesh);

        LISTBASE_FOREACH (Report *, report, &(import_params.reports)->list) {
          cached_value->warnings.append_as(*report);
        }

        return cached_value;
      });

  for (const geo_eval_log::NodeWarning &warning : cached_value->warnings) {
    params.error_message_add(warning.type, warning.message);
  }

  params.set_output("Mesh", cached_value->geometry);

#else
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without STL I/O"));
  params.set_default_remaining_outputs();
#endif
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeImportSTL", GEO_NODE_IMPORT_STL);
  ntype.ui_name = "Import STL";
  ntype.ui_description = "Import a mesh from an STL file";
  ntype.enum_name_legacy = "IMPORT_STL";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_import_stl
