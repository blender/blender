/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "GEO_resample_curves.hh"

#include "BKE_curves.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_resample_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurveResample)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Curve")).supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).supports_field().hide_value();
  b.add_input<decl::Int>(N_("Count")).default_value(10).min(1).max(100000).supports_field();
  b.add_input<decl::Float>(N_("Length"))
      .default_value(0.1f)
      .min(0.01f)
      .supports_field()
      .subtype(PROP_DISTANCE);
  b.add_output<decl::Geometry>(N_("Curve"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurveResample *data = MEM_cnew<NodeGeometryCurveResample>(__func__);

  data->mode = GEO_NODE_CURVE_RESAMPLE_COUNT;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryCurveResample &storage = node_storage(*node);
  const GeometryNodeCurveResampleMode mode = (GeometryNodeCurveResampleMode)storage.mode;

  bNodeSocket *count_socket = ((bNodeSocket *)node->inputs.first)->next->next;
  bNodeSocket *length_socket = count_socket->next;

  nodeSetSocketAvailability(ntree, count_socket, mode == GEO_NODE_CURVE_RESAMPLE_COUNT);
  nodeSetSocketAvailability(ntree, length_socket, mode == GEO_NODE_CURVE_RESAMPLE_LENGTH);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");

  const NodeGeometryCurveResample &storage = node_storage(params.node());
  const GeometryNodeCurveResampleMode mode = (GeometryNodeCurveResampleMode)storage.mode;

  const Field<bool> selection = params.extract_input<Field<bool>>("Selection");

  switch (mode) {
    case GEO_NODE_CURVE_RESAMPLE_COUNT: {
      Field<int> count = params.extract_input<Field<int>>("Count");
      geometry_set.modify_geometry_sets([&](GeometrySet &geometry) {
        if (const CurveComponent *component = geometry.get_component_for_read<CurveComponent>()) {
          if (!component->is_empty()) {
            geometry.replace_curves(geometry::resample_to_count(*component, selection, count));
          }
        }
      });
      break;
    }
    case GEO_NODE_CURVE_RESAMPLE_LENGTH: {
      Field<int> length = params.extract_input<Field<float>>("Length");
      geometry_set.modify_geometry_sets([&](GeometrySet &geometry) {
        if (const CurveComponent *component = geometry.get_component_for_read<CurveComponent>()) {
          if (!component->is_empty()) {
            geometry.replace_curves(geometry::resample_to_length(*component, selection, length));
          }
        }
      });
      break;
    }
    case GEO_NODE_CURVE_RESAMPLE_EVALUATED:
      geometry_set.modify_geometry_sets([&](GeometrySet &geometry) {
        if (const CurveComponent *component = geometry.get_component_for_read<CurveComponent>()) {
          if (!component->is_empty()) {
            geometry.replace_curves(geometry::resample_to_evaluated(*component, selection));
          }
        }
      });
      break;
  }

  params.set_output("Curve", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_curve_resample_cc

void register_node_type_geo_curve_resample()
{
  namespace file_ns = blender::nodes::node_geo_curve_resample_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_RESAMPLE_CURVE, "Resample Curve", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_layout;
  node_type_storage(
      &ntype, "NodeGeometryCurveResample", node_free_standard_storage, node_copy_standard_storage);
  node_type_init(&ntype, file_ns::node_init);
  node_type_update(&ntype, file_ns::node_update);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
