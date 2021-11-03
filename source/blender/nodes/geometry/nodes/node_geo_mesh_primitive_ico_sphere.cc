/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "DNA_mesh_types.h"

#include "BKE_lib_id.h"
#include "BKE_material.h"
#include "BKE_mesh.h"

#include "bmesh.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_mesh_primitive_ico_sphere_declare(NodeDeclarationBuilder &b)
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
}

static Mesh *create_ico_sphere_mesh(const int subdivisions, const float radius)
{
  const float4x4 transform = float4x4::identity();

  const BMeshCreateParams bmcp = {true};
  const BMAllocTemplate allocsize = {0, 0, 0, 0};
  BMesh *bm = BM_mesh_create(&allocsize, &bmcp);
  BM_data_layer_add_named(bm, &bm->ldata, CD_MLOOPUV, nullptr);

  BMO_op_callf(bm,
               BMO_FLAG_DEFAULTS,
               "create_icosphere subdivisions=%i radius=%f matrix=%m4 calc_uvs=%b",
               subdivisions,
               std::abs(radius),
               transform.values,
               true);

  BMeshToMeshParams params{};
  params.calc_object_remap = false;
  Mesh *mesh = (Mesh *)BKE_id_new_nomain(ID_ME, nullptr);
  BKE_id_material_eval_ensure_default_slot(&mesh->id);
  BM_mesh_bm_to_me(nullptr, bm, mesh, &params);
  BM_mesh_free(bm);

  return mesh;
}

static void geo_node_mesh_primitive_ico_sphere_exec(GeoNodeExecParams params)
{
  const int subdivisions = std::min(params.extract_input<int>("Subdivisions"), 10);
  const float radius = params.extract_input<float>("Radius");

  Mesh *mesh = create_ico_sphere_mesh(subdivisions, radius);
  params.set_output("Mesh", GeometrySet::create_with_mesh(mesh));
}

}  // namespace blender::nodes

void register_node_type_geo_mesh_primitive_ico_sphere()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_MESH_PRIMITIVE_ICO_SPHERE, "Ico Sphere", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = blender::nodes::geo_node_mesh_primitive_ico_sphere_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_mesh_primitive_ico_sphere_exec;
  nodeRegisterType(&ntype);
}
