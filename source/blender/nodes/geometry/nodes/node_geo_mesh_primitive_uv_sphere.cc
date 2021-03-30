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
#include "DNA_meshdata_types.h"

#include "BKE_lib_id.h"
#include "BKE_mesh.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "bmesh.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_mesh_primitive_uv_sphere_in[] = {
    {SOCK_INT, N_("Segments"), 32, 0.0f, 0.0f, 0.0f, 3, 1024},
    {SOCK_INT, N_("Rings"), 16, 0.0f, 0.0f, 0.0f, 3, 1024},
    {SOCK_FLOAT, N_("Radius"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, FLT_MAX, PROP_DISTANCE},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_mesh_primitive_uv_sphere_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

namespace blender::nodes {

static int sphere_vert_total(const int segments, const int rings)
{
  return segments * (rings - 1) + 2;
}

static int sphere_edge_total(const int segments, const int rings)
{
  return segments * (rings * 2 - 1);
}

static int sphere_corner_total(const int segments, const int rings)
{
  const int quad_corners = 4 * segments * (rings - 2);
  const int tri_corners = 3 * segments * 2;
  return quad_corners + tri_corners;
}

static int sphere_face_total(const int segments, const int rings)
{
  const int quads = segments * (rings - 2);
  const int triangles = segments * 2;
  return quads + triangles;
}

static Mesh *create_uv_sphere_mesh_bmesh(const float radius, const int segments, const int rings)
{
  const float4x4 transform = float4x4::identity();

  const BMeshCreateParams bmcp = {true};
  const BMAllocTemplate allocsize = {sphere_vert_total(segments, rings),
                                     sphere_edge_total(segments, rings),
                                     sphere_corner_total(segments, rings),
                                     sphere_face_total(segments, rings)};
  BMesh *bm = BM_mesh_create(&allocsize, &bmcp);

  BMO_op_callf(bm,
               BMO_FLAG_DEFAULTS,
               "create_uvsphere u_segments=%i v_segments=%i diameter=%f matrix=%m4 calc_uvs=%b",
               segments,
               rings,
               radius,
               transform.values,
               true);

  BMeshToMeshParams params{};
  params.calc_object_remap = false;
  Mesh *mesh = (Mesh *)BKE_id_new_nomain(ID_ME, nullptr);
  BM_mesh_bm_to_me(nullptr, bm, mesh, &params);
  BM_mesh_free(bm);

  return mesh;
}

static void geo_node_mesh_primitive_uv_sphere_exec(GeoNodeExecParams params)
{
  const int segments_num = params.extract_input<int>("Segments");
  const int rings_num = params.extract_input<int>("Rings");
  if (segments_num < 3 || rings_num < 3) {
    params.set_output("Geometry", GeometrySet());
    return;
  }

  const float radius = params.extract_input<float>("Radius");

  Mesh *mesh = create_uv_sphere_mesh_bmesh(radius, segments_num, rings_num);
  params.set_output("Geometry", GeometrySet::create_with_mesh(mesh));
}

}  // namespace blender::nodes

void register_node_type_geo_mesh_primitive_uv_sphere()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_MESH_PRIMITIVE_UV_SPHERE, "UV Sphere", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(
      &ntype, geo_node_mesh_primitive_uv_sphere_in, geo_node_mesh_primitive_uv_sphere_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_mesh_primitive_uv_sphere_exec;
  nodeRegisterType(&ntype);
}
