/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "GEO_uv_pack.hh"
#include "GEO_uv_parametrizer.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_uv_unwrap_cc {

NODE_STORAGE_FUNCS(NodeGeometryUVUnwrap)

static EnumPropertyItem method_items[] = {
    {GEO_NODE_UV_UNWRAP_METHOD_ANGLE_BASED,
     "ANGLE_BASED",
     0,
     N_("Angle Based"),
     N_("This method gives a good 2D representation of a mesh")},
    {GEO_NODE_UV_UNWRAP_METHOD_CONFORMAL,
     "CONFORMAL",
     0,
     N_("Conformal"),
     N_("Uses LSCM (Least Squares Conformal Mapping). This usually gives a less accurate UV "
        "mapping than Angle Based, but works better for simpler objects")},
    {0, nullptr, 0, nullptr, nullptr},
};

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
  b.add_input<decl::Menu>("Method").static_items(method_items).optional_label();
  b.add_output<decl::Vector>("UV").field_source_reference_all().description(
      "UV coordinates between 0 and 1 for each face corner in the selected faces");
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  /* Still used for forward compatibility. */
  node->storage = MEM_callocN<NodeGeometryUVUnwrap>(__func__);
}

static VArray<float3> construct_uv_gvarray(const Mesh &mesh,
                                           const Field<bool> selection_field,
                                           const Field<bool> seam_field,
                                           const bool fill_holes,
                                           const float margin,
                                           const GeometryNodeUVUnwrapMethod method,
                                           const AttrDomain domain)
{
  const Span<float3> positions = mesh.vert_positions();
  const Span<int2> edges = mesh.edges();
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();

  const bke::MeshFieldContext face_context{mesh, AttrDomain::Face};
  FieldEvaluator face_evaluator{face_context, faces.size()};
  face_evaluator.add(selection_field);
  face_evaluator.evaluate();
  const IndexMask selection = face_evaluator.get_evaluated_as_mask(0);
  if (selection.is_empty()) {
    return {};
  }

  const bke::MeshFieldContext edge_context{mesh, AttrDomain::Edge};
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
                                       nullptr,
                                       mp_pin.data(),
                                       mp_select.data());
  });

  seam.foreach_index([&](const int i) {
    geometry::ParamKey vkeys[2]{uint(edges[i][0]), uint(edges[i][1])};
    geometry::uv_parametrizer_edge_set_seam(handle, vkeys);
  });

  blender::geometry::UVPackIsland_Params params;
  params.margin = margin;
  params.rotate_method = ED_UVPACK_ROTATION_ANY;

  /* TODO: once field input nodes are able to emit warnings (#94039), emit a
   * warning if we fail to solve an island. */
  geometry::uv_parametrizer_construct_end(handle, fill_holes, false, nullptr);

  geometry::uv_parametrizer_lscm_begin(
      handle, false, method == GEO_NODE_UV_UNWRAP_METHOD_ANGLE_BASED);
  geometry::uv_parametrizer_lscm_solve(handle, nullptr, nullptr);
  geometry::uv_parametrizer_lscm_end(handle);
  geometry::uv_parametrizer_average(handle, true, false, false);
  geometry::uv_parametrizer_pack(handle, params);
  geometry::uv_parametrizer_flush(handle);
  delete (handle);

  return mesh.attributes().adapt_domain<float3>(
      VArray<float3>::from_container(std::move(uv)), AttrDomain::Corner, domain);
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
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    return construct_uv_gvarray(mesh, selection_, seam_, fill_holes_, margin_, method_, domain);
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    selection_.node().for_each_field_input_recursive(fn);
    seam_.node().for_each_field_input_recursive(fn);
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return AttrDomain::Corner;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const auto method = params.get_input<GeometryNodeUVUnwrapMethod>("Method");
  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  const Field<bool> seam_field = params.extract_input<Field<bool>>("Seam");
  const bool fill_holes = params.extract_input<bool>("Fill Holes");
  const float margin = params.extract_input<float>("Margin");
  params.set_output("UV",
                    Field<float3>(std::make_shared<UnwrapFieldInput>(
                        selection_field, seam_field, fill_holes, margin, method)));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeUVUnwrap", GEO_NODE_UV_UNWRAP);
  ntype.ui_name = "UV Unwrap";
  ntype.ui_description = "Generate a UV map based on seam edges";
  ntype.enum_name_legacy = "UV_UNWRAP";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.initfunc = node_init;
  blender::bke::node_type_storage(
      ntype, "NodeGeometryUVUnwrap", node_free_standard_storage, node_copy_standard_storage);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_uv_unwrap_cc
