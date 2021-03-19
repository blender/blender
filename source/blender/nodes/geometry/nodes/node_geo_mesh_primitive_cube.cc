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
#include "BKE_mesh.h"

#include "bmesh.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_mesh_primitive_cube_in[] = {
    {SOCK_FLOAT, N_("Size"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX, PROP_DISTANCE},
    {SOCK_VECTOR, N_("Location"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX, PROP_TRANSLATION},
    {SOCK_VECTOR, N_("Rotation"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX, PROP_EULER},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_mesh_primitive_cube_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

namespace blender::nodes {

static Mesh *create_cube_mesh(const float3 location, const float3 rotation, const float size)
{
  float4x4 transform;
  loc_eul_size_to_mat4(transform.values, location, rotation, float3(1));

  const BMeshCreateParams bmcp = {true};
  const BMAllocTemplate allocsize = {8, 12, 24, 6};
  BMesh *bm = BM_mesh_create(&allocsize, &bmcp);

  BMO_op_callf(bm,
               BMO_FLAG_DEFAULTS,
               "create_cube matrix=%m4 size=%f calc_uvs=%b",
               transform.values,
               size,
               true);

  Mesh *mesh = (Mesh *)BKE_id_new_nomain(ID_ME, nullptr);
  BM_mesh_bm_to_me_for_eval(bm, mesh, nullptr);
  BM_mesh_free(bm);

  return mesh;
}

static void geo_node_mesh_primitive_cube_exec(GeoNodeExecParams params)
{
  const float size = params.extract_input<float>("Size");
  const float3 location = params.extract_input<float3>("Location");
  const float3 rotation = params.extract_input<float3>("Rotation");

  Mesh *mesh = create_cube_mesh(location, rotation, size);
  params.set_output("Geometry", GeometrySet::create_with_mesh(mesh));
}

}  // namespace blender::nodes

void register_node_type_geo_mesh_primitive_cube()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_PRIMITIVE_CUBE, "Cube", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(
      &ntype, geo_node_mesh_primitive_cube_in, geo_node_mesh_primitive_cube_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_mesh_primitive_cube_exec;
  nodeRegisterType(&ntype);
}
