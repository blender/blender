/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GEO_uv_parametrizer.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_uv_unwrap_cc {

NODE_STORAGE_FUNCS(NodeGeometryUVUnwrap)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Bool>("Selection")
      .default_value(true)
      .hide_value()
      .supports_field()
      .description("Faces to participate in the unwrap operation");
  b.add_input<decl::Bool>("Seam").hide_value().supports_field().description(
      "Edges to mark where the mesh is \"cut\" for the purposes of unwrapping");
  b.add_input<decl::Float>("Margin").default_value(0.001f).min(0.0f).max(1.0f).description(
      "Space between islands");
  b.add_input<decl::Bool>("Fill Holes")
      .default_value(true)
      .description(
          "Virtually fill holes in mesh before unwrapping, to better avoid overlaps "
          "and preserve symmetry");
  b.add_output<decl::Vector>("UV").field_source_reference_all().description(
      "UV coordinates between 0 and 1 for each face corner in the selected faces");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "method", 0, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryUVUnwrap *data = MEM_cnew<NodeGeometryUVUnwrap>(__func__);
  data->method = GEO_NODE_UV_UNWRAP_METHOD_ANGLE_BASED;
  node->storage = data;
}

static VArray<float3> construct_uv_gvarray(const Mesh &mesh,
                                           const Field<bool> selection_field,
                                           const Field<bool> seam_field,
                                           const bool fill_holes,
                                           const float margin,
                                           const GeometryNodeUVUnwrapMethod method,
                                           const eAttrDomain domain)
{
  const Span<float3> positions = mesh.vert_positions();
  const Span<int2> edges = mesh.edges();
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();

  const bke::MeshFieldContext face_context{mesh, ATTR_DOMAIN_FACE};
  FieldEvaluator face_evaluator{face_context, faces.size()};
  face_evaluator.add(selection_field);
  face_evaluator.evaluate();
  const IndexMask selection = face_evaluator.get_evaluated_as_mask(0);
  if (selection.is_empty()) {
    return {};
  }

  const bke::MeshFieldContext edge_context{mesh, ATTR_DOMAIN_EDGE};
  FieldEvaluator edge_evaluator{edge_context, edges.size()};
  edge_evaluator.add(seam_field);
  edge_evaluator.evaluate();
  const IndexMask seam = edge_evaluator.get_evaluated_as_mask(0);

  Array<float3> uv(corner_verts.size(), float3(0));

  geometry::ParamHandle *handle = new geometry::ParamHandle();
  selection.foreach_index([&](const int face_index) {
    const IndexRange face = faces[face_index];
    Array<geometry::ParamKey, 16> mp_vkeys(face.size());
    Array<bool, 16> mp_pin(face.size());
    Array<bool, 16> mp_select(face.size());
    Array<const float *, 16> mp_co(face.size());
    Array<float *, 16> mp_uv(face.size());
    for (const int i : IndexRange(face.size())) {
      const int corner = face[i];
      const int vert = corner_verts[corner];
      mp_vkeys[i] = vert;
      mp_co[i] = positions[vert];
      mp_uv[i] = uv[corner];
      mp_pin[i] = false;
      mp_select[i] = false;
    }
    geometry::uv_parametrizer_face_add(handle,
                                       face_index,
                                       face.size(),
                                       mp_vkeys.data(),
                                       mp_co.data(),
                                       mp_uv.data(),
                                       mp_pin.data(),
                                       mp_select.data());
  });

  seam.foreach_index([&](const int i) {
    geometry::ParamKey vkeys[2]{uint(edges[i][0]), uint(edges[i][1])};
    geometry::uv_parametrizer_edge_set_seam(handle, vkeys);
  });

  /* TODO: once field input nodes are able to emit warnings (#94039), emit a
   * warning if we fail to solve an island. */
  geometry::uv_parametrizer_construct_end(handle, fill_holes, false, nullptr);

  geometry::uv_parametrizer_lscm_begin(
      handle, false, method == GEO_NODE_UV_UNWRAP_METHOD_ANGLE_BASED);
  geometry::uv_parametrizer_lscm_solve(handle, nullptr, nullptr);
  geometry::uv_parametrizer_lscm_end(handle);
  geometry::uv_parametrizer_average(handle, true, false, false);
  geometry::uv_parametrizer_pack(handle, margin, true, true);
  geometry::uv_parametrizer_flush(handle);
  delete (handle);

  return mesh.attributes().adapt_domain<float3>(
      VArray<float3>::ForContainer(std::move(uv)), ATTR_DOMAIN_CORNER, domain);
}

class UnwrapFieldInput final : public bke::MeshFieldInput {
 private:
  const Field<bool> selection_;
  const Field<bool> seam_;
  const bool fill_holes_;
  const float margin_;
  const GeometryNodeUVUnwrapMethod method_;

 public:
  UnwrapFieldInput(const Field<bool> selection,
                   const Field<bool> seam,
                   const bool fill_holes,
                   const float margin,
                   const GeometryNodeUVUnwrapMethod method)
      : bke::MeshFieldInput(CPPType::get<float3>(), "UV Unwrap Field"),
        selection_(selection),
        seam_(seam),
        fill_holes_(fill_holes),
        margin_(margin),
        method_(method)
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    return construct_uv_gvarray(mesh, selection_, seam_, fill_holes_, margin_, method_, domain);
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    selection_.node().for_each_field_input_recursive(fn);
    seam_.node().for_each_field_input_recursive(fn);
  }

  std::optional<eAttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return ATTR_DOMAIN_CORNER;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryUVUnwrap &storage = node_storage(params.node());
  const GeometryNodeUVUnwrapMethod method = (GeometryNodeUVUnwrapMethod)storage.method;
  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  const Field<bool> seam_field = params.extract_input<Field<bool>>("Seam");
  const bool fill_holes = params.extract_input<bool>("Fill Holes");
  const float margin = params.extract_input<float>("Margin");
  params.set_output("UV",
                    Field<float3>(std::make_shared<UnwrapFieldInput>(
                        selection_field, seam_field, fill_holes, margin, method)));
}

}  // namespace blender::nodes::node_geo_uv_unwrap_cc

void register_node_type_geo_uv_unwrap()
{
  namespace file_ns = blender::nodes::node_geo_uv_unwrap_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_UV_UNWRAP, "UV Unwrap", NODE_CLASS_CONVERTER);
  ntype.initfunc = file_ns::node_init;
  node_type_storage(
      &ntype, "NodeGeometryUVUnwrap", node_free_standard_storage, node_copy_standard_storage);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
