/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "GEO_uv_parametrizer.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_uv_unwrap_cc {

NODE_STORAGE_FUNCS(NodeGeometryUVUnwrap)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Bool>(N_("Selection"))
      .default_value(true)
      .hide_value()
      .supports_field()
      .description(N_("Faces to participate in the unwrap operation"));
  b.add_input<decl::Bool>(N_("Seam"))
      .hide_value()
      .supports_field()
      .description(N_("Edges to mark where the mesh is \"cut\" for the purposes of unwrapping"));
  b.add_input<decl::Float>(N_("Margin"))
      .default_value(0.001f)
      .min(0.0f)
      .max(1.0f)
      .description(N_("Space between islands"));
  b.add_input<decl::Bool>(N_("Fill Holes"))
      .default_value(true)
      .description(N_("Virtually fill holes in mesh before unwrapping, to better avoid overlaps "
                      "and preserve symmetry"));
  b.add_output<decl::Vector>(N_("UV")).field_source().description(
      N_("UV coordinates between 0 and 1 for each face corner in the selected faces"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "method", 0, "", ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryUVUnwrap *data = MEM_cnew<NodeGeometryUVUnwrap>(__func__);
  data->method = GEO_NODE_UV_UNWRAP_METHOD_ANGLE_BASED;
  node->storage = data;
}

static VArray<float3> construct_uv_gvarray(const MeshComponent &component,
                                           const Field<bool> selection_field,
                                           const Field<bool> seam_field,
                                           const bool fill_holes,
                                           const float margin,
                                           const GeometryNodeUVUnwrapMethod method,
                                           const eAttrDomain domain)
{
  const Mesh *mesh = component.get_for_read();
  if (mesh == nullptr) {
    return {};
  }

  const int face_num = component.attribute_domain_size(ATTR_DOMAIN_FACE);
  GeometryComponentFieldContext face_context{component, ATTR_DOMAIN_FACE};
  FieldEvaluator face_evaluator{face_context, face_num};
  face_evaluator.add(selection_field);
  face_evaluator.evaluate();
  const IndexMask selection = face_evaluator.get_evaluated_as_mask(0);
  if (selection.is_empty()) {
    return {};
  }

  const int edge_num = component.attribute_domain_size(ATTR_DOMAIN_EDGE);
  GeometryComponentFieldContext edge_context{component, ATTR_DOMAIN_EDGE};
  FieldEvaluator edge_evaluator{edge_context, edge_num};
  edge_evaluator.add(seam_field);
  edge_evaluator.evaluate();
  const IndexMask seam = edge_evaluator.get_evaluated_as_mask(0);

  Array<float3> uv(mesh->totloop, float3(0));

  ParamHandle *handle = GEO_uv_parametrizer_construct_begin();
  for (const int mp_index : selection) {
    const MPoly &mp = mesh->mpoly[mp_index];
    Array<ParamKey, 16> mp_vkeys(mp.totloop);
    Array<bool, 16> mp_pin(mp.totloop);
    Array<bool, 16> mp_select(mp.totloop);
    Array<const float *, 16> mp_co(mp.totloop);
    Array<float *, 16> mp_uv(mp.totloop);
    for (const int i : IndexRange(mp.totloop)) {
      const MLoop &ml = mesh->mloop[mp.loopstart + i];
      mp_vkeys[i] = ml.v;
      mp_co[i] = mesh->mvert[ml.v].co;
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
  for (const int i : seam) {
    const MEdge &edge = mesh->medge[i];
    ParamKey vkeys[2]{edge.v1, edge.v2};
    GEO_uv_parametrizer_edge_set_seam(handle, vkeys);
  }
  /* TODO: once field input nodes are able to emit warnings (T94039), emit a
   * warning if we fail to solve an island. */
  GEO_uv_parametrizer_construct_end(handle, fill_holes, false, nullptr);

  GEO_uv_parametrizer_lscm_begin(handle, false, method == GEO_NODE_UV_UNWRAP_METHOD_ANGLE_BASED);
  GEO_uv_parametrizer_lscm_solve(handle, nullptr, nullptr);
  GEO_uv_parametrizer_lscm_end(handle);
  GEO_uv_parametrizer_average(handle, true, false, false);
  GEO_uv_parametrizer_pack(handle, margin, true, true);
  GEO_uv_parametrizer_flush(handle);
  GEO_uv_parametrizer_delete(handle);

  return component.attributes()->adapt_domain<float3>(
      VArray<float3>::ForContainer(std::move(uv)), ATTR_DOMAIN_CORNER, domain);
}

class UnwrapFieldInput final : public GeometryFieldInput {
 private:
  const Field<bool> selection;
  const Field<bool> seam;
  const bool fill_holes;
  const float margin;
  const GeometryNodeUVUnwrapMethod method;

 public:
  UnwrapFieldInput(const Field<bool> selection,
                   const Field<bool> seam,
                   const bool fill_holes,
                   const float margin,
                   const GeometryNodeUVUnwrapMethod method)
      : GeometryFieldInput(CPPType::get<float3>(), "UV Unwrap Field"),
        selection(selection),
        seam(seam),
        fill_holes(fill_holes),
        margin(margin),
        method(method)
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const eAttrDomain domain,
                                 IndexMask UNUSED(mask)) const final
  {
    if (component.type() == GEO_COMPONENT_TYPE_MESH) {
      const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
      return construct_uv_gvarray(
          mesh_component, selection, seam, fill_holes, margin, method, domain);
    }
    return {};
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
  node_type_init(&ntype, file_ns::node_init);
  node_type_storage(
      &ntype, "NodeGeometryUVUnwrap", node_free_standard_storage, node_copy_standard_storage);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
