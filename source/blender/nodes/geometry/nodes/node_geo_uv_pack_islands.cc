/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GEO_uv_pack.hh"
#include "GEO_uv_parametrizer.hh"

#include "DNA_mesh_types.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_uv_pack_islands_cc {

/** Local node enum that maps to eUVPackIsland_ShapeMethod in GEO_uv_pack.hh. */
enum class ShapeMethod : int16_t {
  Aabb = 0,
  Convex = 1,
  Concave = 2,
};

static const EnumPropertyItem shape_method_items[] = {
    {int(ShapeMethod::Aabb),
     "AABB",
     0,
     N_("Bounding Box"),
     N_("Use axis-aligned bounding boxes for packing (fastest, least space efficient)")},
    {int(ShapeMethod::Convex),
     "CONVEX",
     0,
     N_("Convex Hull"),
     N_("Use convex hull approximation of islands (good balance of speed and space efficiency)")},
    {int(ShapeMethod::Concave),
     "CONCAVE",
     0,
     N_("Exact Shape"),
     N_("Use exact geometry for most efficient packing (slowest)")},
    {0, nullptr, 0, nullptr, nullptr},
};

static eUVPackIsland_ShapeMethod convert_shape_method(const ShapeMethod method)
{
  switch (method) {
    case ShapeMethod::Aabb:
      return ED_UVPACK_SHAPE_AABB;
    case ShapeMethod::Convex:
      return ED_UVPACK_SHAPE_CONVEX;
    case ShapeMethod::Concave:
      return ED_UVPACK_SHAPE_CONCAVE;
  }
  BLI_assert_unreachable();
  return ED_UVPACK_SHAPE_AABB;
}

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Vector>("UV").hide_value().supports_field();
  b.add_output<decl::Vector>("UV").field_source_reference_all().align_with_previous();
  b.add_input<decl::Bool>("Selection")
      .default_value(true)
      .hide_value()
      .supports_field()
      .description("Faces to consider when packing islands");
  b.add_input<decl::Float>("Margin").default_value(0.001f).min(0.0f).max(1.0f).description(
      "Space between islands");
  b.add_input<decl::Bool>("Rotate").default_value(true).description("Rotate islands for best fit");
  b.add_input<decl::Menu>("Method")
      .static_items(shape_method_items)
      .default_value(ShapeMethod::Aabb)
      .optional_label()
      .description("Method used for packing UV islands");
  b.add_input<decl::Vector>("Bottom Left")
      .default_value({0.0f, 0.0f})
      .dimensions(2)
      .subtype(PROP_XYZ)
      .description("Bottom-left corner of packing bounds");
  b.add_input<decl::Vector>("Top Right")
      .default_value({1.0f, 1.0f})
      .dimensions(2)
      .subtype(PROP_XYZ)
      .description("Top-right corner of packing bounds");
}

static VArray<float3> construct_uv_gvarray(const Mesh &mesh,
                                           const Field<bool> selection_field,
                                           const Field<float3> uv_field,
                                           const bool rotate,
                                           const float margin,
                                           const eUVPackIsland_ShapeMethod shape_method,
                                           const float3 bottom,
                                           const float3 top,
                                           const AttrDomain domain)
{
  const Span<float3> positions = mesh.vert_positions();
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

  const bke::MeshFieldContext corner_context{mesh, AttrDomain::Corner};
  FieldEvaluator evaluator{corner_context, mesh.corners_num};
  Array<float3> uv(mesh.corners_num);
  evaluator.add_with_destination(uv_field, uv.as_mutable_span());
  evaluator.evaluate();

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
  geometry::uv_parametrizer_construct_end(handle, true, true, nullptr);

  blender::geometry::UVPackIsland_Params params;
  params.shape_method = shape_method;
  params.rotate_method = rotate ? ED_UVPACK_ROTATION_ANY : ED_UVPACK_ROTATION_NONE;
  params.margin = margin;
  if (top.x > bottom.x && top.y > bottom.y) {
    params.udim_base_offset[0] = bottom.x;
    params.udim_base_offset[1] = bottom.y;
    params.target_extent = top.y - bottom.y;
    params.target_aspect_y = (top.x - bottom.x) / (top.y - bottom.y);
  }
  geometry::uv_parametrizer_pack(handle, params);
  geometry::uv_parametrizer_flush(handle);
  delete (handle);

  return mesh.attributes().adapt_domain<float3>(
      VArray<float3>::from_container(std::move(uv)), AttrDomain::Corner, domain);
}

class PackIslandsFieldInput final : public bke::MeshFieldInput {
 private:
  const Field<bool> selection_field_;
  const Field<float3> uv_field_;
  const bool rotate_;
  const float margin_;
  const eUVPackIsland_ShapeMethod shape_method_;
  const float3 bottom_;
  const float3 top_;

 public:
  PackIslandsFieldInput(const Field<bool> selection_field,
                        const Field<float3> uv_field,
                        const bool rotate,
                        const float margin,
                        const eUVPackIsland_ShapeMethod shape_method,
                        const float3 bottom,
                        const float3 top)
      : bke::MeshFieldInput(CPPType::get<float3>(), "Pack UV Islands Field"),
        selection_field_(selection_field),
        uv_field_(uv_field),
        rotate_(rotate),
        margin_(margin),
        shape_method_(shape_method),
        bottom_(bottom),
        top_(top)
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    return construct_uv_gvarray(
        mesh, selection_field_, uv_field_, rotate_, margin_, shape_method_, bottom_, top_, domain);
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    selection_field_.node().for_each_field_input_recursive(fn);
    uv_field_.node().for_each_field_input_recursive(fn);
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return AttrDomain::Corner;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const ShapeMethod local_shape_method = params.get_input<ShapeMethod>("Method");
  const eUVPackIsland_ShapeMethod shape_method = convert_shape_method(local_shape_method);

  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  const Field<float3> uv_field = params.extract_input<Field<float3>>("UV");
  const bool rotate = params.extract_input<bool>("Rotate");
  const float margin = params.extract_input<float>("Margin");
  const float3 bottom = params.extract_input<float3>("Bottom Left");
  const float3 top = params.extract_input<float3>("Top Right");
  params.set_output("UV",
                    Field<float3>(std::make_shared<PackIslandsFieldInput>(
                        selection_field, uv_field, rotate, margin, shape_method, bottom, top)));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeUVPackIslands", GEO_NODE_UV_PACK_ISLANDS);
  ntype.ui_name = "Pack UV Islands";
  ntype.ui_description =
      "Scale islands of a UV map and move them so they fill the UV space as much as possible";
  ntype.enum_name_legacy = "UV_PACK_ISLANDS";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_uv_pack_islands_cc
