#include "ply_import_mesh.hh"
#include "BKE_attribute.h"
#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BLI_math_vector.h"

namespace blender::io::ply {
Mesh *convert_ply_to_mesh(PlyData &data, Mesh *mesh)
{
  // Add vertices to the mesh
  mesh->totvert = (int)data.vertices.size(); // Explicit conversion from int64_t to int
  CustomData_add_layer(&mesh->vdata, CD_MVERT, CD_SET_DEFAULT, nullptr, mesh->totvert);
  MutableSpan<MVert> verts = mesh->verts_for_write();
  for (int i = 0; i < mesh->totvert; i++) {
    float3 vert = {data.vertices[i].x, data.vertices[i].y, data.vertices[i].z};
    copy_v3_v3(verts[i].co, vert);
  }

  // Add faces and edges to the mesh
  if (!data.faces.is_empty()) {
    mesh->totpoly = (int)data.faces.size(); // Explicit conversion from int64_t to int
    mesh->totloop = 0; // TODO: Make this more dynamic using data.edges()
    for (int i = 0; i < data.faces.size(); i++) {
    	mesh->totloop += data.faces[i].size();
	}
    CustomData_add_layer(&mesh->pdata, CD_MPOLY, CD_SET_DEFAULT, nullptr, mesh->totpoly);
    CustomData_add_layer(&mesh->ldata, CD_MLOOP, CD_SET_DEFAULT, nullptr, mesh->totloop);
    MutableSpan<MPoly> polys = mesh->polys_for_write();
    MutableSpan<MLoop> loops = mesh->loops_for_write();

    int offset = 0;
    for (int i = 0; i < mesh->totpoly; i++) {
      int size = (int)data.faces[i].size(); // Explicit conversion from int64_t to int
      polys[i].loopstart = offset;
      polys[i].totloop = size;

      for (int j = 0; j < size; j++) {
        loops[offset + j].v = data.faces[i][j];
      }
      offset += size;
    }
  }

  // Vertex colours
  if (!data.vertex_colors.is_empty()) {
    // Create a data layer for vertex colours and set them
    CustomDataLayer *color_layer = BKE_id_attribute_new(
        &mesh->id, "Col", CD_PROP_COLOR, ATTR_DOMAIN_POINT, nullptr);
    float4 *colors = (float4 *)color_layer->data;
    for (int i = 0; i < data.vertex_colors.size(); i++) {
      colors[i] = data.vertex_colors[i];
    }
  }

  // Calculate mesh from edges
  BKE_mesh_calc_edges(mesh, false, false);

  return mesh;
}
} // namespace blender::io::ply
