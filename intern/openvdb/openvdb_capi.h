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

#ifndef __OPENVDB_CAPI_H__
#define __OPENVDB_CAPI_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Level Set Filters */
typedef enum OpenVDBLevelSet_FilterType {
  OPENVDB_LEVELSET_FILTER_NONE = 0,
  OPENVDB_LEVELSET_FILTER_GAUSSIAN = 1,
  OPENVDB_LEVELSET_FILTER_MEAN = 2,
  OPENVDB_LEVELSET_FILTER_MEDIAN = 3,
  OPENVDB_LEVELSET_FILTER_MEAN_CURVATURE = 4,
  OPENVDB_LEVELSET_FILTER_LAPLACIAN = 5,
  OPENVDB_LEVELSET_FILTER_DILATE = 6,
  OPENVDB_LEVELSET_FILTER_ERODE = 7,
} OpenVDBLevelSet_FilterType;

typedef enum OpenVDBLevelSet_FilterBias {
  OPENVDB_LEVELSET_FIRST_BIAS = 0,
  OPENVDB_LEVELSET_SECOND_BIAS,
  OPENVDB_LEVELSET_THIRD_BIAS,
  OPENVDB_LEVELSET_WENO5_BIAS,
  OPENVDB_LEVELSET_HJWENO5_BIAS,
} OpenVDBLevelSet_FilterBias;

/* Level Set CSG Operations */
typedef enum OpenVDBLevelSet_CSGOperation {
  OPENVDB_LEVELSET_CSG_UNION = 0,
  OPENVDB_LEVELSET_CSG_DIFFERENCE = 1,
  OPENVDB_LEVELSET_CSG_INTERSECTION = 2,
} OpenVDBLevelSet_CSGOperation;

typedef enum OpenVDBLevelSet_GridSampler {
  OPENVDB_LEVELSET_GRIDSAMPLER_NONE = 0,
  OPENVDB_LEVELSET_GRIDSAMPLER_POINT = 1,
  OPENVDB_LEVELSET_GRIDSAMPLER_BOX = 2,
  OPENVDB_LEVELSET_GRIDSAMPLER_QUADRATIC = 3,
} OpenVDBLevelSet_Gridsampler;

struct OpenVDBTransform;
struct OpenVDBLevelSet;

struct OpenVDBVolumeToMeshData {
  int tottriangles;
  int totquads;
  int totvertices;

  float *vertices;
  unsigned int *quads;
  unsigned int *triangles;
};

struct OpenVDBRemeshData {
  float *verts;
  unsigned int *faces;
  int totfaces;
  int totverts;

  float *out_verts;
  unsigned int *out_faces;
  unsigned int *out_tris;
  int out_totverts;
  int out_totfaces;
  int out_tottris;
  int filter_type;
  enum OpenVDBLevelSet_FilterType filter_bias;
  enum OpenVDBLevelSet_FilterBias filter_width; /* Parameter for gaussian, median, mean*/

  float voxel_size;
  float isovalue;
  float adaptivity;
  int relax_disoriented_triangles;
};

int OpenVDB_getVersionHex(void);

enum {
  VEC_INVARIANT = 0,
  VEC_COVARIANT = 1,
  VEC_COVARIANT_NORMALIZE = 2,
  VEC_CONTRAVARIANT_RELATIVE = 3,
  VEC_CONTRAVARIANT_ABSOLUTE = 4,
};

struct OpenVDBTransform *OpenVDBTransform_create(void);
void OpenVDBTransform_free(struct OpenVDBTransform *transform);
void OpenVDBTransform_create_linear_transform(struct OpenVDBTransform *transform,
                                              double voxel_size);

struct OpenVDBLevelSet *OpenVDBLevelSet_create(bool initGrid, struct OpenVDBTransform *xform);
void OpenVDBLevelSet_free(struct OpenVDBLevelSet *level_set);
void OpenVDBLevelSet_mesh_to_level_set(struct OpenVDBLevelSet *level_set,
                                       const float *vertices,
                                       const unsigned int *faces,
                                       const unsigned int totvertices,
                                       const unsigned int totfaces,
                                       struct OpenVDBTransform *xform);
void OpenVDBLevelSet_mesh_to_level_set_transform(struct OpenVDBLevelSet *level_set,
                                                 const float *vertices,
                                                 const unsigned int *faces,
                                                 const unsigned int totvertices,
                                                 const unsigned int totfaces,
                                                 struct OpenVDBTransform *transform);
void OpenVDBLevelSet_volume_to_mesh(struct OpenVDBLevelSet *level_set,
                                    struct OpenVDBVolumeToMeshData *mesh,
                                    const double isovalue,
                                    const double adaptivity,
                                    const bool relax_disoriented_triangles);
void OpenVDBLevelSet_filter(struct OpenVDBLevelSet *level_set,
                            OpenVDBLevelSet_FilterType filter_type,
                            int width,
                            float distance,
                            OpenVDBLevelSet_FilterBias bias);
void OpenVDBLevelSet_CSG_operation(struct OpenVDBLevelSet *out,
                                   struct OpenVDBLevelSet *gridA,
                                   struct OpenVDBLevelSet *gridB,
                                   OpenVDBLevelSet_CSGOperation operation);

struct OpenVDBLevelSet *OpenVDBLevelSet_transform_and_resample(struct OpenVDBLevelSet *level_setA,
                                                               struct OpenVDBLevelSet *level_setB,
                                                               char sampler,
                                                               float isolevel);

#ifdef __cplusplus
}
#endif

#endif /* __OPENVDB_CAPI_H__ */
