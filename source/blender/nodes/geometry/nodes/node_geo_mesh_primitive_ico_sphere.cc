/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "BKE_lib_id.h"
#include "BKE_material.h"
#include "BKE_mesh.h"

#include "bmesh.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_primitive_ico_sphere_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Radius"))
      .default_value(1.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description(N_("Distance from the generated points to the origin"));
  b.add_input<decl::Int>(N_("Subdivisions"))
      .default_value(1)
      .min(1)
      .max(7)
      .description(N_("Number of subdivisions on top of the basic icosahedron"));
  b.add_output<decl::Geometry>(N_("Mesh"));
  b.add_output<decl::Vector>(N_("UV Map")).field_on_all();
}

static Mesh *create_ico_sphere_mesh(const int subdivisions,
                                    const float radius,
                                    const AttributeIDRef &uv_map_id)
{
  const float4x4 transform = float4x4::identity();

  const bool create_uv_map = bool(uv_map_id);

  BMeshCreateParams bmesh_create_params{};
  bmesh_create_params.use_toolflags = true;
  const BMAllocTemplate allocsize = {0, 0, 0, 0};
  BMesh *bm = BM_mesh_create(&allocsize, &bmesh_create_params);
  BM_data_layer_add_named(bm, &bm->ldata, CD_PROP_FLOAT2, "UVMap");
  /* Make sure the associated boolean layers exists as well. Normally this would be done when
   * adding a UV layer via python or when copying from Mesh, but when we 'manually' create the UV
   * layer we need to make sure the boolean layers exist as well. */
  BM_uv_map_ensure_select_and_pin_attrs(bm);

  BMO_op_callf(bm,
               BMO_FLAG_DEFAULTS,
               "create_icosphere subdivisions=%i radius=%f matrix=%m4 calc_uvs=%b",
               subdivisions,
               std::abs(radius),
               transform.ptr(),
               create_uv_map);

  BMeshToMeshParams params{};
  params.calc_object_remap = false;
  Mesh *mesh = reinterpret_cast<Mesh *>(BKE_id_new_nomain(ID_ME, nullptr));
  BKE_id_material_eval_ensure_default_slot(&mesh->id);
  BM_mesh_bm_to_me(nullptr, bm, mesh, &params);
  BM_mesh_free(bm);

  /* The code above generates a "UVMap" attribute. The code below renames that attribute, we don't
   * have a simple utility for that yet though so there is some overhead right now. */
  MutableAttributeAccessor attributes = mesh->attributes_for_write();
  if (create_uv_map) {
    const VArraySpan<float2> orig_uv_map = attributes.lookup<float2>("UVMap");
    SpanAttributeWriter<float2> uv_map = attributes.lookup_or_add_for_write_only_span<float2>(
        uv_map_id, ATTR_DOMAIN_CORNER);
    uv_map.span.copy_from(orig_uv_map);
    uv_map.finish();
  }
  attributes.remove("UVMap");

  return mesh;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const int subdivisions = std::min(params.extract_input<int>("Subdivisions"), 10);
  const float radius = params.extract_input<float>("Radius");

  AutoAnonymousAttributeID uv_map_id = params.get_output_anonymous_attribute_id_if_needed(
      "UV Map");

  Mesh *mesh = create_ico_sphere_mesh(subdivisions, radius, uv_map_id.get());
  params.set_output("Mesh", GeometrySet::create_with_mesh(mesh));

  if (uv_map_id) {
    params.set_output("UV Map",
                      AnonymousAttributeFieldInput::Create<float3>(
                          std::move(uv_map_id), params.attribute_producer_name()));
  }
}

}  // namespace blender::nodes::node_geo_mesh_primitive_ico_sphere_cc

void register_node_type_geo_mesh_primitive_ico_sphere()
{
  namespace file_ns = blender::nodes::node_geo_mesh_primitive_ico_sphere_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_MESH_PRIMITIVE_ICO_SPHERE, "Ico Sphere", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
