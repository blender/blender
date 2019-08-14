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
 * The Original Code is Copyright (C) 2019 by Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <float.h>
#include <ctype.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_library.h"
#include "BKE_customdata.h"
#include "BKE_bvhutils.h"
#include "BKE_mesh_remesh_voxel.h" /* own include */

#ifdef WITH_OPENVDB
#  include "openvdb_capi.h"
#endif

#ifdef WITH_OPENVDB
struct OpenVDBLevelSet *BKE_mesh_remesh_voxel_ovdb_mesh_to_level_set_create(
    Mesh *mesh, struct OpenVDBTransform *transform)
{
  BKE_mesh_runtime_looptri_recalc(mesh);
  const MLoopTri *looptri = BKE_mesh_runtime_looptri_ensure(mesh);
  MVertTri *verttri = MEM_callocN(sizeof(*verttri) * BKE_mesh_runtime_looptri_len(mesh),
                                  "remesh_looptri");
  BKE_mesh_runtime_verttri_from_looptri(
      verttri, mesh->mloop, looptri, BKE_mesh_runtime_looptri_len(mesh));

  unsigned int totfaces = BKE_mesh_runtime_looptri_len(mesh);
  unsigned int totverts = mesh->totvert;
  float *verts = (float *)MEM_malloc_arrayN(totverts * 3, sizeof(float), "remesh_input_verts");
  unsigned int *faces = (unsigned int *)MEM_malloc_arrayN(
      totfaces * 3, sizeof(unsigned int), "remesh_intput_faces");

  for (unsigned int i = 0; i < totverts; i++) {
    MVert *mvert = &mesh->mvert[i];
    verts[i * 3] = mvert->co[0];
    verts[i * 3 + 1] = mvert->co[1];
    verts[i * 3 + 2] = mvert->co[2];
  }

  for (unsigned int i = 0; i < totfaces; i++) {
    MVertTri *vt = &verttri[i];
    faces[i * 3] = vt->tri[0];
    faces[i * 3 + 1] = vt->tri[1];
    faces[i * 3 + 2] = vt->tri[2];
  }

  struct OpenVDBLevelSet *level_set = OpenVDBLevelSet_create(false, NULL);
  OpenVDBLevelSet_mesh_to_level_set(level_set, verts, faces, totverts, totfaces, transform);

  MEM_freeN(verts);
  MEM_freeN(faces);
  MEM_freeN(verttri);

  return level_set;
}

Mesh *BKE_mesh_remesh_voxel_ovdb_volume_to_mesh_nomain(struct OpenVDBLevelSet *level_set,
                                                       double isovalue,
                                                       double adaptivity,
                                                       bool relax_disoriented_triangles)
{
#  ifdef WITH_OPENVDB
  struct OpenVDBVolumeToMeshData output_mesh;
  OpenVDBLevelSet_volume_to_mesh(
      level_set, &output_mesh, isovalue, adaptivity, relax_disoriented_triangles);
#  endif

  Mesh *mesh = BKE_mesh_new_nomain(output_mesh.totvertices,
                                   0,
                                   0,
                                   (output_mesh.totquads * 4) + (output_mesh.tottriangles * 3),
                                   output_mesh.totquads + output_mesh.tottriangles);

  for (int i = 0; i < output_mesh.totvertices; i++) {
    copy_v3_v3(mesh->mvert[i].co, &output_mesh.vertices[i * 3]);
  }

  MPoly *mp = mesh->mpoly;
  MLoop *ml = mesh->mloop;
  for (int i = 0; i < output_mesh.totquads; i++, mp++, ml += 4) {
    mp->loopstart = (int)(ml - mesh->mloop);
    mp->totloop = 4;

    ml[0].v = output_mesh.quads[i * 4 + 3];
    ml[1].v = output_mesh.quads[i * 4 + 2];
    ml[2].v = output_mesh.quads[i * 4 + 1];
    ml[3].v = output_mesh.quads[i * 4];
  }

  for (int i = 0; i < output_mesh.tottriangles; i++, mp++, ml += 3) {
    mp->loopstart = (int)(ml - mesh->mloop);
    mp->totloop = 3;

    ml[0].v = output_mesh.triangles[i * 3 + 2];
    ml[1].v = output_mesh.triangles[i * 3 + 1];
    ml[2].v = output_mesh.triangles[i * 3];
  }

  BKE_mesh_calc_edges(mesh, false, false);
  BKE_mesh_calc_normals(mesh);

  MEM_freeN(output_mesh.quads);
  MEM_freeN(output_mesh.vertices);

  if (output_mesh.tottriangles > 0) {
    MEM_freeN(output_mesh.triangles);
  }

  return mesh;
}
#endif

Mesh *BKE_mesh_remesh_voxel_to_mesh_nomain(Mesh *mesh, float voxel_size)
{
  Mesh *new_mesh = NULL;
#ifdef WITH_OPENVDB
  struct OpenVDBLevelSet *level_set;
  struct OpenVDBTransform *xform = OpenVDBTransform_create();
  OpenVDBTransform_create_linear_transform(xform, (double)voxel_size);
  level_set = BKE_mesh_remesh_voxel_ovdb_mesh_to_level_set_create(mesh, xform);
  new_mesh = BKE_mesh_remesh_voxel_ovdb_volume_to_mesh_nomain(level_set, 0.0, 0.0, false);
  OpenVDBLevelSet_free(level_set);
  OpenVDBTransform_free(xform);
#else
  UNUSED_VARS(mesh, voxel_size);
#endif
  return new_mesh;
}

void BKE_remesh_reproject_paint_mask(Mesh *target, Mesh *source)
{
  BVHTreeFromMesh bvhtree = {
      .nearest_callback = NULL,
  };
  BKE_bvhtree_from_mesh_get(&bvhtree, source, BVHTREE_FROM_VERTS, 2);
  MVert *target_verts = CustomData_get_layer(&target->vdata, CD_MVERT);

  float *target_mask;
  if (CustomData_has_layer(&target->vdata, CD_PAINT_MASK)) {
    target_mask = CustomData_get_layer(&target->vdata, CD_PAINT_MASK);
  }
  else {
    target_mask = CustomData_add_layer(
        &target->vdata, CD_PAINT_MASK, CD_CALLOC, NULL, target->totvert);
  }

  float *source_mask;
  if (CustomData_has_layer(&source->vdata, CD_PAINT_MASK)) {
    source_mask = CustomData_get_layer(&source->vdata, CD_PAINT_MASK);
  }
  else {
    source_mask = CustomData_add_layer(
        &source->vdata, CD_PAINT_MASK, CD_CALLOC, NULL, source->totvert);
  }

  for (int i = 0; i < target->totvert; i++) {
    float from_co[3];
    BVHTreeNearest nearest;
    nearest.index = -1;
    nearest.dist_sq = FLT_MAX;
    copy_v3_v3(from_co, target_verts[i].co);
    BLI_bvhtree_find_nearest(bvhtree.tree, from_co, &nearest, bvhtree.nearest_callback, &bvhtree);
    if (nearest.index != -1) {
      target_mask[i] = source_mask[nearest.index];
    }
  }
  free_bvhtree_from_mesh(&bvhtree);
}
