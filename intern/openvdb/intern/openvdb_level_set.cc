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
 *
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 */

#include "openvdb_level_set.h"
#include "MEM_guardedalloc.h"
#include "openvdb/tools/Composite.h"
#include "openvdb_capi.h"
#include "openvdb_util.h"

OpenVDBLevelSet::OpenVDBLevelSet()
{
  openvdb::initialize();
}

OpenVDBLevelSet::~OpenVDBLevelSet()
{
}

void OpenVDBLevelSet::mesh_to_level_set(const float *vertices,
                                        const unsigned int *faces,
                                        const unsigned int totvertices,
                                        const unsigned int totfaces,
                                        const openvdb::math::Transform::Ptr &xform)
{
  std::vector<openvdb::Vec3s> points(totvertices);
  std::vector<openvdb::Vec3I> triangles(totfaces);
  std::vector<openvdb::Vec4I> quads;

  for (unsigned int i = 0; i < totvertices; i++) {
    points[i] = openvdb::Vec3s(vertices[i * 3], vertices[i * 3 + 1], vertices[i * 3 + 2]);
  }

  for (unsigned int i = 0; i < totfaces; i++) {
    triangles[i] = openvdb::Vec3I(faces[i * 3], faces[i * 3 + 1], faces[i * 3 + 2]);
  }

  this->grid = openvdb::tools::meshToLevelSet<openvdb::FloatGrid>(
      *xform, points, triangles, quads, 1);
}

void OpenVDBLevelSet::volume_to_mesh(OpenVDBVolumeToMeshData *mesh,
                                     const double isovalue,
                                     const double adaptivity,
                                     const bool relax_disoriented_triangles)
{
  std::vector<openvdb::Vec3s> out_points;
  std::vector<openvdb::Vec4I> out_quads;
  std::vector<openvdb::Vec3I> out_tris;
  openvdb::tools::volumeToMesh<openvdb::FloatGrid>(*this->grid,
                                                   out_points,
                                                   out_tris,
                                                   out_quads,
                                                   isovalue,
                                                   adaptivity,
                                                   relax_disoriented_triangles);
  mesh->vertices = (float *)MEM_malloc_arrayN(
      out_points.size(), 3 * sizeof(float), "openvdb remesher out verts");
  mesh->quads = (unsigned int *)MEM_malloc_arrayN(
      out_quads.size(), 4 * sizeof(unsigned int), "openvdb remesh out quads");
  mesh->triangles = NULL;
  if (out_tris.size() > 0) {
    mesh->triangles = (unsigned int *)MEM_malloc_arrayN(
        out_tris.size(), 3 * sizeof(unsigned int), "openvdb remesh out tris");
  }

  mesh->totvertices = out_points.size();
  mesh->tottriangles = out_tris.size();
  mesh->totquads = out_quads.size();

  for (size_t i = 0; i < out_points.size(); i++) {
    mesh->vertices[i * 3] = out_points[i].x();
    mesh->vertices[i * 3 + 1] = out_points[i].y();
    mesh->vertices[i * 3 + 2] = out_points[i].z();
  }

  for (size_t i = 0; i < out_quads.size(); i++) {
    mesh->quads[i * 4] = out_quads[i].x();
    mesh->quads[i * 4 + 1] = out_quads[i].y();
    mesh->quads[i * 4 + 2] = out_quads[i].z();
    mesh->quads[i * 4 + 3] = out_quads[i].w();
  }

  for (size_t i = 0; i < out_tris.size(); i++) {
    mesh->triangles[i * 3] = out_tris[i].x();
    mesh->triangles[i * 3 + 1] = out_tris[i].y();
    mesh->triangles[i * 3 + 2] = out_tris[i].z();
  }
}

void OpenVDBLevelSet::filter(OpenVDBLevelSet_FilterType filter_type,
                             int width,
                             float distance,
                             OpenVDBLevelSet_FilterBias filter_bias)
{

  if (!this->grid) {
    return;
  }

  if (this->grid->getGridClass() != openvdb::GRID_LEVEL_SET) {
    return;
  }

  openvdb::tools::LevelSetFilter<openvdb::FloatGrid> filter(*this->grid);
  filter.setSpatialScheme((openvdb::math::BiasedGradientScheme)filter_bias);
  switch (filter_type) {
    case OPENVDB_LEVELSET_FILTER_GAUSSIAN:
      filter.gaussian(width);
      break;
    case OPENVDB_LEVELSET_FILTER_MEDIAN:
      filter.median(width);
      break;
    case OPENVDB_LEVELSET_FILTER_MEAN:
      filter.mean(width);
      break;
    case OPENVDB_LEVELSET_FILTER_MEAN_CURVATURE:
      filter.meanCurvature();
      break;
    case OPENVDB_LEVELSET_FILTER_LAPLACIAN:
      filter.laplacian();
      break;
    case OPENVDB_LEVELSET_FILTER_DILATE:
      filter.offset(distance);
      break;
    case OPENVDB_LEVELSET_FILTER_ERODE:
      filter.offset(distance);
      break;
    case OPENVDB_LEVELSET_FILTER_NONE:
      break;
  }
}
openvdb::FloatGrid::Ptr OpenVDBLevelSet::CSG_operation_apply(
    const openvdb::FloatGrid::Ptr &gridA,
    const openvdb::FloatGrid::Ptr &gridB,
    OpenVDBLevelSet_CSGOperation operation)
{
  switch (operation) {
    case OPENVDB_LEVELSET_CSG_UNION:
      openvdb::tools::csgUnion(*gridA, *gridB);
      break;
    case OPENVDB_LEVELSET_CSG_DIFFERENCE:
      openvdb::tools::csgDifference(*gridA, *gridB);
      break;
    case OPENVDB_LEVELSET_CSG_INTERSECTION:
      openvdb::tools::csgIntersection(*gridA, *gridB);
      break;
  }

  return gridA;
}

const openvdb::FloatGrid::Ptr &OpenVDBLevelSet::get_grid()
{
  return this->grid;
}

void OpenVDBLevelSet::set_grid(const openvdb::FloatGrid::Ptr &grid)
{
  this->grid = grid;
}
