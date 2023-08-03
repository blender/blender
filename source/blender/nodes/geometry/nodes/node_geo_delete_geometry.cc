/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "UI_interface.h"
#include "UI_resources.h"

#include "DNA_pointcloud_types.h"

#include "BKE_curves.hh"
#include "BKE_instances.hh"
#include "BKE_pointcloud.h"

#include "GEO_mesh_copy_selection.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_delete_geometry_cc {

/** \return std::nullopt if the geometry should remain unchanged. */
static std::optional<Curves *> separate_curves_selection(
    const Curves &src_curves_id,
    const Field<bool> &selection_field,
    const eAttrDomain domain,
    const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  const bke::CurvesGeometry &src_curves = src_curves_id.geometry.wrap();

  const int domain_size = src_curves.attributes().domain_size(domain);
  const bke::CurvesFieldContext context{src_curves, domain};
  fn::FieldEvaluator evaluator{context, domain_size};
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  if (selection.size() == domain_size) {
    return std::nullopt;
  }
  if (selection.is_empty()) {
    return nullptr;
  }

  Curves *dst_curves_id = nullptr;
  if (domain == ATTR_DOMAIN_POINT) {
    bke::CurvesGeometry dst_curves = bke::curves_copy_point_selection(
        src_curves, selection, propagation_info);
    dst_curves_id = bke::curves_new_nomain(std::move(dst_curves));
  }
  else if (domain == ATTR_DOMAIN_CURVE) {
    bke::CurvesGeometry dst_curves = bke::curves_copy_curve_selection(
        src_curves, selection, propagation_info);
    dst_curves_id = bke::curves_new_nomain(std::move(dst_curves));
  }

  bke::curves_copy_parameters(src_curves_id, *dst_curves_id);
  return dst_curves_id;
}

/** \return std::nullopt if the geometry should remain unchanged. */
static std::optional<PointCloud *> separate_point_cloud_selection(
    const PointCloud &src_pointcloud,
    const Field<bool> &selection_field,
    const AnonymousAttributePropagationInfo &propagation_info)
{
  const bke::PointCloudFieldContext context{src_pointcloud};
  fn::FieldEvaluator evaluator{context, src_pointcloud.totpoint};
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  if (selection.size() == src_pointcloud.totpoint) {
    return std::nullopt;
  }
  if (selection.is_empty()) {
    return nullptr;
  }

  PointCloud *pointcloud = BKE_pointcloud_new_nomain(selection.size());
  bke::gather_attributes(src_pointcloud.attributes(),
                         ATTR_DOMAIN_POINT,
                         propagation_info,
                         {},
                         selection,
                         pointcloud->attributes_for_write());
  return pointcloud;
}

static void delete_selected_instances(GeometrySet &geometry_set,
                                      const Field<bool> &selection_field,
                                      const AnonymousAttributePropagationInfo &propagation_info)
{
  bke::Instances &instances = *geometry_set.get_instances_for_write();
  bke::InstancesFieldContext field_context{instances};

  fn::FieldEvaluator evaluator{field_context, instances.instances_num()};
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  if (selection.is_empty()) {
    geometry_set.remove<InstancesComponent>();
    return;
  }

  instances.remove(selection, propagation_info);
}

static std::optional<Mesh *> separate_mesh_selection(
    const Mesh &mesh,
    const Field<bool> &selection,
    const eAttrDomain selection_domain,
    const GeometryNodeDeleteGeometryMode mode,
    const AnonymousAttributePropagationInfo &propagation_info)
{
  switch (mode) {
    case GEO_NODE_DELETE_GEOMETRY_MODE_ALL:
      return geometry::mesh_copy_selection(mesh, selection, selection_domain, propagation_info);
    case GEO_NODE_DELETE_GEOMETRY_MODE_EDGE_FACE:
      return geometry::mesh_copy_selection_keep_verts(
          mesh, selection, selection_domain, propagation_info);
    case GEO_NODE_DELETE_GEOMETRY_MODE_ONLY_FACE:
      return geometry::mesh_copy_selection_keep_edges(
          mesh, selection, selection_domain, propagation_info);
  }
  return nullptr;
}

}  // namespace blender::nodes::node_geo_delete_geometry_cc

namespace blender::nodes {

void separate_geometry(GeometrySet &geometry_set,
                       const eAttrDomain domain,
                       const GeometryNodeDeleteGeometryMode mode,
                       const Field<bool> &selection,
                       const AnonymousAttributePropagationInfo &propagation_info,
                       bool &r_is_error)
{
  namespace file_ns = blender::nodes::node_geo_delete_geometry_cc;

  bool some_valid_domain = false;
  if (const PointCloud *points = geometry_set.get_pointcloud()) {
    if (domain == ATTR_DOMAIN_POINT) {
      std::optional<PointCloud *> dst_points = file_ns::separate_point_cloud_selection(
          *points, selection, propagation_info);
      if (dst_points) {
        geometry_set.replace_pointcloud(*dst_points);
      }
      some_valid_domain = true;
    }
  }
  if (const Mesh *mesh = geometry_set.get_mesh()) {
    if (ELEM(domain, ATTR_DOMAIN_POINT, ATTR_DOMAIN_EDGE, ATTR_DOMAIN_FACE, ATTR_DOMAIN_CORNER)) {
      std::optional<Mesh *> dst_mesh = file_ns::separate_mesh_selection(
          *mesh, selection, domain, mode, propagation_info);
      if (dst_mesh) {
        geometry_set.replace_mesh(*dst_mesh);
      }
      some_valid_domain = true;
    }
  }
  if (const Curves *curves_id = geometry_set.get_curves()) {
    if (ELEM(domain, ATTR_DOMAIN_POINT, ATTR_DOMAIN_CURVE)) {
      std::optional<Curves *> dst_curves = file_ns::separate_curves_selection(
          *curves_id, selection, domain, propagation_info);
      if (dst_curves) {
        geometry_set.replace_curves(*dst_curves);
      }
      some_valid_domain = true;
    }
  }
  if (geometry_set.has_instances()) {
    if (domain == ATTR_DOMAIN_INSTANCE) {
      file_ns::delete_selected_instances(geometry_set, selection, propagation_info);
      some_valid_domain = true;
    }
  }
  r_is_error = !some_valid_domain && geometry_set.has_realized_data();
}

}  // namespace blender::nodes

namespace blender::nodes::node_geo_delete_geometry_cc {

NODE_STORAGE_FUNCS(NodeGeometryDeleteGeometry)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::Bool>("Selection")
      .default_value(true)
      .hide_value()
      .field_on_all()
      .description("The parts of the geometry to be deleted");
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  const bNode *node = static_cast<bNode *>(ptr->data);
  const NodeGeometryDeleteGeometry &storage = node_storage(*node);
  const eAttrDomain domain = eAttrDomain(storage.domain);

  uiItemR(layout, ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
  /* Only show the mode when it is relevant. */
  if (ELEM(domain, ATTR_DOMAIN_POINT, ATTR_DOMAIN_EDGE, ATTR_DOMAIN_FACE)) {
    uiItemR(layout, ptr, "mode", UI_ITEM_NONE, "", ICON_NONE);
  }
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryDeleteGeometry *data = MEM_cnew<NodeGeometryDeleteGeometry>(__func__);
  data->domain = ATTR_DOMAIN_POINT;
  data->mode = GEO_NODE_DELETE_GEOMETRY_MODE_ALL;

  node->storage = data;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  /* The node's input is a selection of elements that should be deleted, but the code is
   * implemented as a separation operation that copies the selected elements to a new geometry.
   * Invert the selection to avoid the need to keep track of both cases in the code. */
  const Field<bool> selection = fn::invert_boolean_field(
      params.extract_input<Field<bool>>("Selection"));

  const NodeGeometryDeleteGeometry &storage = node_storage(params.node());
  const eAttrDomain domain = eAttrDomain(storage.domain);
  const GeometryNodeDeleteGeometryMode mode = (GeometryNodeDeleteGeometryMode)storage.mode;

  const AnonymousAttributePropagationInfo &propagation_info = params.get_output_propagation_info(
      "Geometry");

  if (domain == ATTR_DOMAIN_INSTANCE) {
    bool is_error;
    separate_geometry(geometry_set, domain, mode, selection, propagation_info, is_error);
  }
  else {
    geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      bool is_error;
      /* Invert here because we want to keep the things not in the selection. */
      separate_geometry(geometry_set, domain, mode, selection, propagation_info, is_error);
    });
  }

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_delete_geometry_cc

void register_node_type_geo_delete_geometry()
{
  namespace file_ns = blender::nodes::node_geo_delete_geometry_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_DELETE_GEOMETRY, "Delete Geometry", NODE_CLASS_GEOMETRY);

  node_type_storage(&ntype,
                    "NodeGeometryDeleteGeometry",
                    node_free_standard_storage,
                    node_copy_standard_storage);

  ntype.initfunc = file_ns::node_init;

  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
