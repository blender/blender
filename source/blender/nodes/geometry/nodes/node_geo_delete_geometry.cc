/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_rna_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "DNA_grease_pencil_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_mesh.hh"
#include "BKE_pointcloud.h"

#include "GEO_mesh_copy_selection.hh"

#include "RNA_enum_types.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_delete_geometry_cc {

/** \return std::nullopt if the geometry should remain unchanged. */
static std::optional<bke::CurvesGeometry> separate_curves_selection(
    const bke::CurvesGeometry &src_curves,
    const fn::FieldContext &field_context,
    const Field<bool> &selection_field,
    const eAttrDomain domain,
    const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  const int domain_size = src_curves.attributes().domain_size(domain);
  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  if (selection.size() == domain_size) {
    return std::nullopt;
  }
  if (selection.is_empty()) {
    return bke::CurvesGeometry();
  }

  if (domain == ATTR_DOMAIN_POINT) {
    return bke::curves_copy_point_selection(src_curves, selection, propagation_info);
  }
  else if (domain == ATTR_DOMAIN_CURVE) {
    return bke::curves_copy_curve_selection(src_curves, selection, propagation_info);
  }
  BLI_assert_unreachable();
  return std::nullopt;
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
    const Field<bool> &selection_field,
    const eAttrDomain selection_domain,
    const GeometryNodeDeleteGeometryMode mode,
    const AnonymousAttributePropagationInfo &propagation_info)
{
  const bke::AttributeAccessor attributes = mesh.attributes();
  const bke::MeshFieldContext context(mesh, selection_domain);
  fn::FieldEvaluator evaluator(context, attributes.domain_size(selection_domain));
  evaluator.add(selection_field);
  evaluator.evaluate();
  const VArray<bool> selection = evaluator.get_evaluated<bool>(0);

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

static std::optional<GreasePencil *> separate_grease_pencil_layer_selection(
    const GreasePencil &src_grease_pencil,
    const Field<bool> &selection_field,
    const AnonymousAttributePropagationInfo &propagation_info)
{
  const bke::AttributeAccessor attributes = src_grease_pencil.attributes();
  const bke::GeometryFieldContext context(src_grease_pencil);

  fn::FieldEvaluator evaluator(context, attributes.domain_size(ATTR_DOMAIN_LAYER));
  evaluator.set_selection(selection_field);
  evaluator.evaluate();

  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  if (selection.size() == attributes.domain_size(ATTR_DOMAIN_LAYER)) {
    return std::nullopt;
  }
  if (selection.is_empty()) {
    return nullptr;
  }

  GreasePencil *dst_grease_pencil = BKE_grease_pencil_new_nomain();
  BKE_grease_pencil_duplicate_drawing_array(&src_grease_pencil, dst_grease_pencil);
  selection.foreach_index([&](const int index) {
    const bke::greasepencil::Layer &src_layer = *src_grease_pencil.layers()[index];
    dst_grease_pencil->add_layer(src_layer);
  });
  dst_grease_pencil->remove_drawings_with_no_users();

  bke::gather_attributes(src_grease_pencil.attributes(),
                         ATTR_DOMAIN_LAYER,
                         propagation_info,
                         {},
                         selection,
                         dst_grease_pencil->attributes_for_write());

  return dst_grease_pencil;
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
  if (const Curves *src_curves_id = geometry_set.get_curves()) {
    if (ELEM(domain, ATTR_DOMAIN_POINT, ATTR_DOMAIN_CURVE)) {
      const bke::CurvesGeometry &src_curves = src_curves_id->geometry.wrap();
      const bke::CurvesFieldContext field_context{src_curves, domain};
      std::optional<bke::CurvesGeometry> dst_curves = file_ns::separate_curves_selection(
          src_curves, field_context, selection, domain, propagation_info);
      if (dst_curves) {
        if (dst_curves->points_num() == 0) {
          geometry_set.remove<CurveComponent>();
        }
        else {
          Curves *dst_curves_id = bke::curves_new_nomain(*dst_curves);
          bke::curves_copy_parameters(*src_curves_id, *dst_curves_id);
          geometry_set.replace_curves(dst_curves_id);
        }
      }
      some_valid_domain = true;
    }
  }
  if (geometry_set.get_grease_pencil()) {
    using namespace blender::bke::greasepencil;
    if (domain == ATTR_DOMAIN_LAYER) {
      const GreasePencil &grease_pencil = *geometry_set.get_grease_pencil();
      std::optional<GreasePencil *> dst_grease_pencil =
          file_ns::separate_grease_pencil_layer_selection(
              grease_pencil, selection, propagation_info);
      if (dst_grease_pencil) {
        geometry_set.replace_grease_pencil(*dst_grease_pencil);
      }
      some_valid_domain = true;
    }
    else if (ELEM(domain, ATTR_DOMAIN_POINT, ATTR_DOMAIN_CURVE)) {
      GreasePencil &grease_pencil = *geometry_set.get_grease_pencil_for_write();
      for (const int layer_index : grease_pencil.layers().index_range()) {
        Drawing *drawing = get_eval_grease_pencil_layer_drawing_for_write(grease_pencil,
                                                                          layer_index);
        if (drawing == nullptr) {
          continue;
        }
        const bke::CurvesGeometry &src_curves = drawing->strokes();
        const bke::GreasePencilLayerFieldContext field_context(
            grease_pencil, ATTR_DOMAIN_CURVE, layer_index);
        std::optional<bke::CurvesGeometry> dst_curves = file_ns::separate_curves_selection(
            src_curves, field_context, selection, domain, propagation_info);
        if (!dst_curves) {
          continue;
        }
        drawing->strokes_for_write() = std::move(*dst_curves);
        drawing->tag_topology_changed();
        some_valid_domain = true;
      }
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

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem mode_items[] = {
      {GEO_NODE_DELETE_GEOMETRY_MODE_ALL, "ALL", 0, "All", ""},
      {GEO_NODE_DELETE_GEOMETRY_MODE_EDGE_FACE, "EDGE_FACE", 0, "Only Edges & Faces", ""},
      {GEO_NODE_DELETE_GEOMETRY_MODE_ONLY_FACE, "ONLY_FACE", 0, "Only Faces", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "mode",
                    "Mode",
                    "Which parts of the mesh component to delete",
                    mode_items,
                    NOD_storage_enum_accessors(mode),
                    GEO_NODE_DELETE_GEOMETRY_MODE_ALL);

  RNA_def_node_enum(srna,
                    "domain",
                    "Domain",
                    "Which domain to delete in",
                    rna_enum_attribute_domain_without_corner_items,
                    NOD_storage_enum_accessors(domain),
                    ATTR_DOMAIN_POINT,
                    enums::domain_experimental_grease_pencil_version3_fn);
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_DELETE_GEOMETRY, "Delete Geometry", NODE_CLASS_GEOMETRY);

  node_type_storage(&ntype,
                    "NodeGeometryDeleteGeometry",
                    node_free_standard_storage,
                    node_copy_standard_storage);

  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_delete_geometry_cc
