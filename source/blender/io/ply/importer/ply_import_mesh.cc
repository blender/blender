/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "BKE_attribute.h"
#include "BKE_customdata.h"
#include "BKE_mesh.h"

#include "BLI_math_vector.h"

#include "ply_import_mesh.hh"

namespace blender::io::ply {
Mesh *convert_ply_to_mesh(PlyData &data, Mesh *mesh)
{
  /* Add vertices to the mesh. */
  mesh->totvert = int(data.vertices.size()); /* Explicit conversion from int64_t to int. */
  CustomData_add_layer(&mesh->vdata, CD_MVERT, CD_SET_DEFAULT, nullptr, mesh->totvert);
  MutableSpan<MVert> verts = mesh->verts_for_write();
  for (int i = 0; i < mesh->totvert; i++) {
    float3 vert = {data.vertices[i].x, data.vertices[i].y, data.vertices[i].z};
    copy_v3_v3(verts[i].co, vert);
  }

  if (!data.edges.is_empty()) {
    mesh->totedge = int(data.edges.size());
    CustomData_add_layer(&mesh->edata, CD_MEDGE, CD_SET_DEFAULT, nullptr, mesh->totedge);
    MutableSpan<MEdge> edges = mesh->edges_for_write();
    for (int i = 0; i < mesh->totedge; i++) {
      edges[i].v1 = data.edges[i].first;
      edges[i].v2 = data.edges[i].second;
    }
  }

  /* Add faces and edges to the mesh. */
  if (!data.faces.is_empty()) {
    /* Specify amount of total faces. */
    mesh->totpoly = int(data.faces.size());
    mesh->totloop = 0;
    for (int i = 0; i < data.faces.size(); i++) {
      /* Add number of edges to the amount of edges. */
      mesh->totloop += data.faces[i].size();
    }
    CustomData_add_layer(&mesh->pdata, CD_MPOLY, CD_SET_DEFAULT, nullptr, mesh->totpoly);
    CustomData_add_layer(&mesh->ldata, CD_MLOOP, CD_SET_DEFAULT, nullptr, mesh->totloop);
    MutableSpan<MPoly> polys = mesh->polys_for_write();
    MutableSpan<MLoop> loops = mesh->loops_for_write();

    int offset = 0;
    /* Iterate over amount of faces. */
    for (int i = 0; i < mesh->totpoly; i++) {
      int size = int(data.faces[i].size());
      /* Set the index from where this face starts and specify the amount of edges it has. */
      polys[i].loopstart = offset;
      polys[i].totloop = size;

      for (int j = 0; j < size; j++) {
        /* Set the vertex index of the edge to the one in PlyData. */
        loops[offset + j].v = data.faces[i][j];
      }
      offset += size;
    }
  }

  /* Vertex colors */
  if (!data.vertex_colors.is_empty()) {
    /* Create a data layer for vertex colors and set them. */
    CustomDataLayer *color_layer = BKE_id_attribute_new(
        &mesh->id, "Col", CD_PROP_COLOR, ATTR_DOMAIN_POINT, nullptr);
    float4 *colors = (float4 *)color_layer->data;
    for (int i = 0; i < data.vertex_colors.size(); i++) {
      colors[i] = data.vertex_colors[i];
    }
  }

  /* Uvmap */
  if (!data.uvmap.is_empty()) {
    MLoopUV *Uv = static_cast<MLoopUV *>(
        CustomData_add_layer(&mesh->ldata, CD_MLOOPUV, CD_SET_DEFAULT, nullptr, mesh->totloop));
    int counter = 0;
    for (int i = 0; i < data.faces.size(); i++) {
      for (int j = 0; j < data.faces[i].size(); j++) {
        copy_v2_v2(Uv[counter].uv, data.uvmap[data.faces[i][j]]);
        counter++;
      }
    }
  }

  /* Calculate mesh from edges. */
  BKE_mesh_calc_edges(mesh, true, false);
  BKE_mesh_calc_edges_loose(mesh);

  /* Note: This is important to do after initializing the loops. */
  if (!data.vertex_normals.is_empty()) {
    float(*vertex_normals)[3] = static_cast<float(*)[3]>(
        MEM_malloc_arrayN(data.vertex_normals.size(), sizeof(float[3]), __func__));
    for (int i = 0; i < data.vertex_normals.size(); i++) {
      copy_v3_v3(vertex_normals[i], data.vertex_normals[i]);
    }
    BKE_mesh_set_custom_normals_from_verts(mesh, vertex_normals);
    MEM_freeN(vertex_normals);
  }

  return mesh;
}
}  // namespace blender::io::ply
