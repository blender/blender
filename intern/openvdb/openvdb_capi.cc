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

#include "openvdb_capi.h"
#include "openvdb_level_set.h"
#include "openvdb_transform.h"
#include "openvdb_util.h"

int OpenVDB_getVersionHex()
{
  return openvdb::OPENVDB_LIBRARY_VERSION;
}

OpenVDBLevelSet *OpenVDBLevelSet_create(bool initGrid, OpenVDBTransform *xform)
{
  OpenVDBLevelSet *level_set = new OpenVDBLevelSet();
  if (initGrid) {
    openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create();
    grid->setGridClass(openvdb::GRID_LEVEL_SET);
    if (xform) {
      grid->setTransform(xform->get_transform());
    }
    level_set->set_grid(grid);
  }
  return level_set;
}

OpenVDBTransform *OpenVDBTransform_create()
{
  return new OpenVDBTransform();
}

void OpenVDBTransform_free(OpenVDBTransform *transform)
{
  delete transform;
}

void OpenVDBTransform_create_linear_transform(OpenVDBTransform *transform, double voxel_size)
{
  transform->create_linear_transform(voxel_size);
}

void OpenVDBLevelSet_free(OpenVDBLevelSet *level_set)
{
  delete level_set;
}

void OpenVDBLevelSet_mesh_to_level_set(struct OpenVDBLevelSet *level_set,
                                       const float *vertices,
                                       const unsigned int *faces,
                                       const unsigned int totvertices,
                                       const unsigned int totfaces,
                                       OpenVDBTransform *xform)
{
  level_set->mesh_to_level_set(vertices, faces, totvertices, totfaces, xform->get_transform());
}

void OpenVDBLevelSet_mesh_to_level_set_transform(struct OpenVDBLevelSet *level_set,
                                                 const float *vertices,
                                                 const unsigned int *faces,
                                                 const unsigned int totvertices,
                                                 const unsigned int totfaces,
                                                 OpenVDBTransform *transform)
{
  level_set->mesh_to_level_set(vertices, faces, totvertices, totfaces, transform->get_transform());
}

void OpenVDBLevelSet_volume_to_mesh(struct OpenVDBLevelSet *level_set,
                                    struct OpenVDBVolumeToMeshData *mesh,
                                    const double isovalue,
                                    const double adaptivity,
                                    const bool relax_disoriented_triangles)
{
  level_set->volume_to_mesh(mesh, isovalue, adaptivity, relax_disoriented_triangles);
}

void OpenVDBLevelSet_filter(struct OpenVDBLevelSet *level_set,
                            OpenVDBLevelSet_FilterType filter_type,
                            int width,
                            float distance,
                            OpenVDBLevelSet_FilterBias bias)
{
  level_set->filter(filter_type, width, distance, bias);
}

void OpenVDBLevelSet_CSG_operation(struct OpenVDBLevelSet *out,
                                   struct OpenVDBLevelSet *gridA,
                                   struct OpenVDBLevelSet *gridB,
                                   OpenVDBLevelSet_CSGOperation operation)
{
  openvdb::FloatGrid::Ptr grid = out->CSG_operation_apply(
      gridA->get_grid(), gridB->get_grid(), operation);
  out->set_grid(grid);
}

OpenVDBLevelSet *OpenVDBLevelSet_transform_and_resample(struct OpenVDBLevelSet *level_setA,
                                                        struct OpenVDBLevelSet *level_setB,
                                                        char sampler,
                                                        float isolevel)
{
  openvdb::FloatGrid::Ptr sourceGrid = level_setA->get_grid();
  openvdb::FloatGrid::Ptr targetGrid = level_setB->get_grid()->deepCopy();

  const openvdb::math::Transform &sourceXform = sourceGrid->transform(),
                                 &targetXform = targetGrid->transform();

  // Compute a source grid to target grid transform.
  // (For this example, we assume that both grids' transforms are linear,
  // so that they can be represented as 4 x 4 matrices.)
  openvdb::Mat4R xform = sourceXform.baseMap()->getAffineMap()->getMat4() *
                         targetXform.baseMap()->getAffineMap()->getMat4().inverse();

  // Create the transformer.
  openvdb::tools::GridTransformer transformer(xform);

  switch (sampler) {
    case OPENVDB_LEVELSET_GRIDSAMPLER_POINT:
      // Resample using nearest-neighbor interpolation.
      transformer.transformGrid<openvdb::tools::PointSampler, openvdb::FloatGrid>(*sourceGrid,
                                                                                  *targetGrid);
      // Prune the target tree for optimal sparsity.
      targetGrid->tree().prune();
      break;

    case OPENVDB_LEVELSET_GRIDSAMPLER_BOX:
      // Resample using trilinear interpolation.
      transformer.transformGrid<openvdb::tools::BoxSampler, openvdb::FloatGrid>(*sourceGrid,
                                                                                *targetGrid);
      // Prune the target tree for optimal sparsity.
      targetGrid->tree().prune();
      break;

    case OPENVDB_LEVELSET_GRIDSAMPLER_QUADRATIC:
      // Resample using triquadratic interpolation.
      transformer.transformGrid<openvdb::tools::QuadraticSampler, openvdb::FloatGrid>(*sourceGrid,
                                                                                      *targetGrid);
      // Prune the target tree for optimal sparsity.
      targetGrid->tree().prune();
      break;

    case OPENVDB_LEVELSET_GRIDSAMPLER_NONE:
      break;
  }

  targetGrid = openvdb::tools::levelSetRebuild(*targetGrid, isolevel, 1.0f);
  openvdb::tools::pruneLevelSet(targetGrid->tree());

  OpenVDBLevelSet *level_set = OpenVDBLevelSet_create(false, NULL);
  level_set->set_grid(targetGrid);

  return level_set;
}
