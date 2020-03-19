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
#include "openvdb_dense_convert.h"
#include "openvdb_level_set.h"
#include "openvdb_transform.h"
#include "openvdb_util.h"

int OpenVDB_getVersionHex()
{
  return openvdb::OPENVDB_LIBRARY_VERSION;
}

OpenVDBFloatGrid *OpenVDB_export_grid_fl(OpenVDBWriter *writer,
                                         const char *name,
                                         float *data,
                                         const int res[3],
                                         float matrix[4][4],
                                         const float clipping,
                                         OpenVDBFloatGrid *mask)
{
  Timer(__func__);

  using openvdb::FloatGrid;

  FloatGrid *mask_grid = reinterpret_cast<FloatGrid *>(mask);
  FloatGrid *grid = internal::OpenVDB_export_grid<FloatGrid>(
      writer, name, data, res, matrix, clipping, mask_grid);

  return reinterpret_cast<OpenVDBFloatGrid *>(grid);
}

OpenVDBIntGrid *OpenVDB_export_grid_ch(OpenVDBWriter *writer,
                                       const char *name,
                                       unsigned char *data,
                                       const int res[3],
                                       float matrix[4][4],
                                       const float clipping,
                                       OpenVDBFloatGrid *mask)
{
  Timer(__func__);

  using openvdb::FloatGrid;
  using openvdb::Int32Grid;

  FloatGrid *mask_grid = reinterpret_cast<FloatGrid *>(mask);
  Int32Grid *grid = internal::OpenVDB_export_grid<Int32Grid>(
      writer, name, data, res, matrix, clipping, mask_grid);

  return reinterpret_cast<OpenVDBIntGrid *>(grid);
}

OpenVDBVectorGrid *OpenVDB_export_grid_vec(struct OpenVDBWriter *writer,
                                           const char *name,
                                           const float *data_x,
                                           const float *data_y,
                                           const float *data_z,
                                           const int res[3],
                                           float matrix[4][4],
                                           short vec_type,
                                           const float clipping,
                                           const bool is_color,
                                           OpenVDBFloatGrid *mask)
{
  Timer(__func__);

  using openvdb::FloatGrid;
  using openvdb::GridBase;
  using openvdb::VecType;

  FloatGrid *mask_grid = reinterpret_cast<FloatGrid *>(mask);
  GridBase *grid = internal::OpenVDB_export_vector_grid(writer,
                                                        name,
                                                        data_x,
                                                        data_y,
                                                        data_z,
                                                        res,
                                                        matrix,
                                                        static_cast<VecType>(vec_type),
                                                        is_color,
                                                        clipping,
                                                        mask_grid);

  return reinterpret_cast<OpenVDBVectorGrid *>(grid);
}

void OpenVDB_import_grid_fl(OpenVDBReader *reader,
                            const char *name,
                            float **data,
                            const int res[3])
{
  Timer(__func__);

  internal::OpenVDB_import_grid<openvdb::FloatGrid>(reader, name, data, res);
}

void OpenVDB_import_grid_ch(OpenVDBReader *reader,
                            const char *name,
                            unsigned char **data,
                            const int res[3])
{
  internal::OpenVDB_import_grid<openvdb::Int32Grid>(reader, name, data, res);
}

void OpenVDB_import_grid_vec(struct OpenVDBReader *reader,
                             const char *name,
                             float **data_x,
                             float **data_y,
                             float **data_z,
                             const int res[3])
{
  Timer(__func__);

  internal::OpenVDB_import_grid_vector(reader, name, data_x, data_y, data_z, res);
}

OpenVDBWriter *OpenVDBWriter_create()
{
  return new OpenVDBWriter();
}

void OpenVDBWriter_free(OpenVDBWriter *writer)
{
  delete writer;
}

void OpenVDBWriter_set_flags(OpenVDBWriter *writer, const int flag, const bool half)
{
  int compression_flags = openvdb::io::COMPRESS_ACTIVE_MASK;

#ifdef WITH_OPENVDB_BLOSC
  if (flag == 0) {
    compression_flags |= openvdb::io::COMPRESS_BLOSC;
  }
  else
#endif
      if (flag == 1) {
    compression_flags |= openvdb::io::COMPRESS_ZIP;
  }
  else {
    compression_flags = openvdb::io::COMPRESS_NONE;
  }

  writer->setFlags(compression_flags, half);
}

void OpenVDBWriter_add_meta_fl(OpenVDBWriter *writer, const char *name, const float value)
{
  writer->insertFloatMeta(name, value);
}

void OpenVDBWriter_add_meta_int(OpenVDBWriter *writer, const char *name, const int value)
{
  writer->insertIntMeta(name, value);
}

void OpenVDBWriter_add_meta_v3(OpenVDBWriter *writer, const char *name, const float value[3])
{
  writer->insertVec3sMeta(name, value);
}

void OpenVDBWriter_add_meta_v3_int(OpenVDBWriter *writer, const char *name, const int value[3])
{
  writer->insertVec3IMeta(name, value);
}

void OpenVDBWriter_add_meta_mat4(OpenVDBWriter *writer, const char *name, float value[4][4])
{
  writer->insertMat4sMeta(name, value);
}

void OpenVDBWriter_write(OpenVDBWriter *writer, const char *filename)
{
  writer->write(filename);
}

OpenVDBReader *OpenVDBReader_create()
{
  return new OpenVDBReader();
}

void OpenVDBReader_free(OpenVDBReader *reader)
{
  delete reader;
}

void OpenVDBReader_open(OpenVDBReader *reader, const char *filename)
{
  reader->open(filename);
}

void OpenVDBReader_get_meta_fl(OpenVDBReader *reader, const char *name, float *value)
{
  reader->floatMeta(name, *value);
}

void OpenVDBReader_get_meta_int(OpenVDBReader *reader, const char *name, int *value)
{
  reader->intMeta(name, *value);
}

void OpenVDBReader_get_meta_v3(OpenVDBReader *reader, const char *name, float value[3])
{
  reader->vec3sMeta(name, value);
}

void OpenVDBReader_get_meta_v3_int(OpenVDBReader *reader, const char *name, int value[3])
{
  reader->vec3IMeta(name, value);
}

void OpenVDBReader_get_meta_mat4(OpenVDBReader *reader, const char *name, float value[4][4])
{
  reader->mat4sMeta(name, value);
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
