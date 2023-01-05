/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_separate_geometry_cc {

NODE_STORAGE_FUNCS(NodeGeometrySeparateGeometry)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::Bool>(N_("Selection"))
      .default_value(true)
      .hide_value()
      .field_on_all()
      .description(N_("The parts of the geometry that go into the first output"));
  b.add_output<decl::Geometry>(N_("Selection"))
      .propagate_all()
      .description(N_("The parts of the geometry in the selection"));
  b.add_output<decl::Geometry>(N_("Inverted"))
      .propagate_all()
      .description(N_("The parts of the geometry not in the selection"));
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "domain", 0, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometrySeparateGeometry *data = MEM_cnew<NodeGeometrySeparateGeometry>(__func__);
  data->domain = ATTR_DOMAIN_POINT;

  node->storage = data;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  const NodeGeometrySeparateGeometry &storage = node_storage(params.node());
  const eAttrDomain domain = eAttrDomain(storage.domain);

  auto separate_geometry_maybe_recursively =
      [&](GeometrySet &geometry_set,
          const Field<bool> &selection,
          const AnonymousAttributePropagationInfo &propagation_info) {
        bool is_error;
        if (domain == ATTR_DOMAIN_INSTANCE) {
          /* Only delete top level instances. */
          separate_geometry(geometry_set,
                            domain,
                            GEO_NODE_DELETE_GEOMETRY_MODE_ALL,
                            selection,
                            propagation_info,
                            is_error);
        }
        else {
          geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
            separate_geometry(geometry_set,
                              domain,
                              GEO_NODE_DELETE_GEOMETRY_MODE_ALL,
                              selection,
                              propagation_info,
                              is_error);
          });
        }
      };

  GeometrySet second_set(geometry_set);
  if (params.output_is_required("Selection")) {
    separate_geometry_maybe_recursively(
        geometry_set, selection_field, params.get_output_propagation_info("Selection"));
    params.set_output("Selection", std::move(geometry_set));
  }
  if (params.output_is_required("Inverted")) {
    separate_geometry_maybe_recursively(second_set,
                                        fn::invert_boolean_field(selection_field),
                                        params.get_output_propagation_info("Inverted"));
    params.set_output("Inverted", std::move(second_set));
  }
}

}  // namespace blender::nodes::node_geo_separate_geometry_cc

void register_node_type_geo_separate_geometry()
{
  namespace file_ns = blender::nodes::node_geo_separate_geometry_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SEPARATE_GEOMETRY, "Separate Geometry", NODE_CLASS_GEOMETRY);

  node_type_storage(&ntype,
                    "NodeGeometrySeparateGeometry",
                    node_free_standard_storage,
                    node_copy_standard_storage);

  ntype.initfunc = file_ns::node_init;

  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
