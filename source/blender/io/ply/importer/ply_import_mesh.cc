#include "ply_import_mesh.hh"
#include "BKE_customdata.h"
#include "BLI_math_vector.h"

namespace blender::io::ply {
Mesh *convert_ply_to_mesh(PlyData &data, Mesh *mesh)
{
  // Add vertices to the mesh
  mesh->totvert = data.vertices.size();
  CustomData_add_layer(&mesh->vdata, CD_MVERT, CD_SET_DEFAULT, nullptr, mesh->totvert);
  MutableSpan<MVert> verts = mesh->verts_for_write();
  for (int i = 0; i < mesh->totvert; i++) {
    float vert[3] = {data.vertices[i].x, data.vertices[i].y, data.vertices[i].z};
    copy_v3_v3(verts[i].co, vert);
  }

  // Add faces and edges to the mesh
  mesh->totpoly = data.faces.size();
  mesh->totloop = data.edges.size();
  CustomData_add_layer(&mesh->pdata, CD_MPOLY, CD_SET_DEFAULT, nullptr, mesh->totpoly);
  CustomData_add_layer(&mesh->ldata, CD_MLOOP, CD_SET_DEFAULT, nullptr, mesh->totloop);
  MutableSpan<MPoly> polys = mesh->polys_for_write();
  MutableSpan<MLoop> loops = mesh->loops_for_write();

  return mesh;
}
} // namespace blender::io::ply
