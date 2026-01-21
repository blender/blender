/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_listBase.h"

namespace blender {

struct CacheArchiveHandle;
struct CacheObjectPath;
struct CacheReader;
struct Object;
struct Main;
struct Mesh;

namespace bke {
struct GeometrySet;
}

namespace io::usd {

/**
 * This struct is in place to store the mesh sequence parameters needed when reading a data from a
 * USD file for the mesh sequence cache.
 */
struct USDMeshReadParams {
  double motion_sample_time; /* USD TimeCode in frames. */
  int read_flags; /* MOD_MESHSEQ_xxx value that is set from MeshSeqCacheModifierData.read_flag. */
};

USDMeshReadParams create_mesh_read_params(double motion_sample_time, int read_flags);

CacheArchiveHandle *USD_create_handle(Main *bmain,
                                      const char *filepath,
                                      ListBaseT<CacheObjectPath> *object_paths);

void USD_free_handle(CacheArchiveHandle *handle);

void USD_get_transform(CacheReader *reader, float r_mat[4][4], float time, float scale);

/** Either modifies current_mesh in-place or constructs a new mesh. */
void USD_read_geometry(CacheReader *reader,
                       const Object *ob,
                       bke::GeometrySet &geometry_set,
                       USDMeshReadParams params,
                       const char **r_err_str);

bool USD_mesh_topology_changed(CacheReader *reader,
                               const Object *ob,
                               const Mesh *existing_mesh,
                               double time,
                               const char **r_err_str);

CacheReader *CacheReader_open_usd_object(CacheArchiveHandle *handle,
                                         CacheReader *reader,
                                         Object *object,
                                         const char *object_path);

void USD_CacheReader_free(CacheReader *reader);

};  // namespace io::usd

}  // namespace blender
