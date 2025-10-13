/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_constants.h"

#include "DNA_mesh_types.h"

#include "BKE_lib_id.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"

#include "GEO_randomize.hh"

#include "bmesh.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_primitive_ico_sphere_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Radius")
      .default_value(1.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description("Distance from the generated points to the origin");
  b.add_input<decl::Int>("Subdivisions")
      .default_value(1)
      .min(1)
      .max(7)
      .description("Number of subdivisions on top of the basic icosahedron");
  b.add_output<decl::Geometry>("Mesh");
  b.add_output<decl::Vector>("UV Map").field_on_all();
}

static Bounds<float3> calculate_bounds_ico_sphere(const float radius, const int subdivisions)
{
  const float delta_phi = (2.0f * M_PI) / 5.0f;
  const float theta = std::cos(std::atan(0.5f));
  const float ro = radius * std::sin(delta_phi);

  float x_max = radius;
  float x_min = -radius;
  float y_max = radius;
  float y_min = -radius;

  if (subdivisions == 1) {
    x_max = radius * theta;
    x_min = -x_max;
    y_max = ro * theta;
    y_min = -y_max;
  }
  else if (subdivisions == 2) {
    x_max = ro;
    x_min = -x_max;
  }

  const float3 bounds_min(x_min, y_min, -radius);
  const float3 bounds_max(x_max, y_max, radius);

  return {bounds_min, bounds_max};
}

static Mesh *create_ico_sphere_mesh(const int subdivisions,
                                    const float radius,
                                    const std::optional<std::string> &uv_map_id)
{
  if (subdivisions >= 3) {
    /* Most nodes don't need this because they internally use multi-threading which triggers
     * lazy-threading without any extra code. */
    lazy_threading::send_hint();
  }

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
  BM_uv_map_attr_pin_ensure_for_all_layers(bm);

  BMO_op_callf(bm,
               BMO_FLAG_DEFAULTS,
               "create_icosphere subdivisions=%i radius=%f matrix=%m4 calc_uvs=%b",
               subdivisions,
               std::abs(radius),
               transform.ptr(),
               create_uv_map);

  BMeshToMeshParams params{};
  params.calc_object_remap = false;
  Mesh *mesh = BKE_id_new_nomain<Mesh>(nullptr);
  BKE_id_material_eval_ensure_default_slot(&mesh->id);
  BM_mesh_bm_to_me(nullptr, bm, mesh, &params);
  BM_mesh_free(bm);

  /* The code above generates a "UVMap" attribute. The code below renames that attribute, we don't
   * have a simple utility for that yet though so there is some overhead right now. */
  MutableAttributeAccessor attributes = mesh->attributes_for_write();
  if (create_uv_map) {
    const VArraySpan orig_uv_map = *attributes.lookup<float2>("UVMap");
    SpanAttributeWriter<float2> uv_map = attributes.lookup_or_add_for_write_only_span<float2>(
        *uv_map_id, AttrDomain::Corner);
    uv_map.span.copy_from(orig_uv_map);
    uv_map.finish();
  }
  attributes.remove("UVMap");

  geometry::debug_randomize_mesh_order(mesh);

  mesh->bounds_set_eager(calculate_bounds_ico_sphere(radius, subdivisions));

  return mesh;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const int subdivisions = std::min(params.extract_input<int>("Subdivisions"), 10);
  const float radius = params.extract_input<float>("Radius");

  std::optional<std::string> uv_map_id = params.get_output_anonymous_attribute_id_if_needed(
      "UV Map");

  Mesh *mesh = create_ico_sphere_mesh(subdivisions, radius, uv_map_id);
  params.set_output("Mesh", GeometrySet::from_mesh(mesh));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeMeshIcoSphere", GEO_NODE_MESH_PRIMITIVE_ICO_SPHERE);
  ntype.ui_name = "Ico Sphere";
  ntype.ui_description = "Generate a spherical mesh that consists of equally sized triangles";
  ntype.enum_name_legacy = "MESH_PRIMITIVE_ICO_SPHERE";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_primitive_ico_sphere_cc
