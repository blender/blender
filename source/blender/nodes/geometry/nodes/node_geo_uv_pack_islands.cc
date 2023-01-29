/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "GEO_uv_parametrizer.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_uv_pack_islands_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>(N_("UV")).hide_value().supports_field();
  b.add_input<decl::Bool>(N_("Selection"))
      .default_value(true)
      .hide_value()
      .supports_field()
      .description(N_("Faces to consider when packing islands"));
  b.add_input<decl::Float>(N_("Margin"))
      .default_value(0.001f)
      .min(0.0f)
      .max(1.0f)
      .description(N_("Space between islands"));
  b.add_input<decl::Bool>(N_("Rotate"))
      .default_value(true)
      .description(N_("Rotate islands for best fit"));
  b.add_output<decl::Vector>(N_("UV")).field_source_reference_all();
}

static VArray<float3> construct_uv_gvarray(const Mesh &mesh,
                                           const Field<bool> selection_field,
                                           const Field<float3> uv_field,
                                           const bool rotate,
                                           const float margin,
                                           const eAttrDomain domain)
{
  const Span<float3> positions = mesh.vert_positions();
  const Span<MPoly> polys = mesh.polys();
  const Span<MLoop> loops = mesh.loops();

  bke::MeshFieldContext face_context{mesh, ATTR_DOMAIN_FACE};
  FieldEvaluator face_evaluator{face_context, polys.size()};
  face_evaluator.add(selection_field);
  face_evaluator.evaluate();
  const IndexMask selection = face_evaluator.get_evaluated_as_mask(0);
  if (selection.is_empty()) {
    return {};
  }

  bke::MeshFieldContext corner_context{mesh, ATTR_DOMAIN_CORNER};
  FieldEvaluator evaluator{corner_context, mesh.totloop};
  Array<float3> uv(mesh.totloop);
  evaluator.add_with_destination(uv_field, uv.as_mutable_span());
  evaluator.evaluate();

  ParamHandle *handle = GEO_uv_parametrizer_construct_begin();
  for (const int mp_index : selection) {
    const MPoly &mp = polys[mp_index];
    Array<ParamKey, 16> mp_vkeys(mp.totloop);
    Array<bool, 16> mp_pin(mp.totloop);
    Array<bool, 16> mp_select(mp.totloop);
    Array<const float *, 16> mp_co(mp.totloop);
    Array<float *, 16> mp_uv(mp.totloop);
    for (const int i : IndexRange(mp.totloop)) {
      const MLoop &ml = loops[mp.loopstart + i];
      mp_vkeys[i] = ml.v;
      mp_co[i] = positions[ml.v];
      mp_uv[i] = uv[mp.loopstart + i];
      mp_pin[i] = false;
      mp_select[i] = false;
    }
    GEO_uv_parametrizer_face_add(handle,
                                 mp_index,
                                 mp.totloop,
                                 mp_vkeys.data(),
                                 mp_co.data(),
                                 mp_uv.data(),
                                 mp_pin.data(),
                                 mp_select.data());
  }
  GEO_uv_parametrizer_construct_end(handle, true, true, nullptr);

  GEO_uv_parametrizer_pack(handle, margin, rotate, true);
  GEO_uv_parametrizer_flush(handle);
  GEO_uv_parametrizer_delete(handle);

  return mesh.attributes().adapt_domain<float3>(
      VArray<float3>::ForContainer(std::move(uv)), ATTR_DOMAIN_CORNER, domain);
}

class PackIslandsFieldInput final : public bke::MeshFieldInput {
 private:
  const Field<bool> selection_field_;
  const Field<float3> uv_field_;
  const bool rotate_;
  const float margin_;

 public:
  PackIslandsFieldInput(const Field<bool> selection_field,
                        const Field<float3> uv_field,
                        const bool rotate,
                        const float margin)
      : bke::MeshFieldInput(CPPType::get<float3>(), "Pack UV Islands Field"),
        selection_field_(selection_field),
        uv_field_(uv_field),
        rotate_(rotate),
        margin_(margin)
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 const IndexMask /*mask*/) const final
  {
    return construct_uv_gvarray(mesh, selection_field_, uv_field_, rotate_, margin_, domain);
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    selection_field_.node().for_each_field_input_recursive(fn);
    uv_field_.node().for_each_field_input_recursive(fn);
  }

  std::optional<eAttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return ATTR_DOMAIN_CORNER;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  const Field<float3> uv_field = params.extract_input<Field<float3>>("UV");
  const bool rotate = params.extract_input<bool>("Rotate");
  const float margin = params.extract_input<float>("Margin");
  params.set_output("UV",
                    Field<float3>(std::make_shared<PackIslandsFieldInput>(
                        selection_field, uv_field, rotate, margin)));
}

}  // namespace blender::nodes::node_geo_uv_pack_islands_cc

void register_node_type_geo_uv_pack_islands()
{
  namespace file_ns = blender::nodes::node_geo_uv_pack_islands_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_UV_PACK_ISLANDS, "Pack UV Islands", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
