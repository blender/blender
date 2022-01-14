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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

#pragma once

#include "DEG_depsgraph.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CacheArchiveHandle;
struct CacheFile;
struct CacheReader;
struct Object;
struct bContext;

struct USDExportParams {
  bool export_animation;
  bool export_hair;
  bool export_uvmaps;
  bool export_normals;
  bool export_materials;
  bool selected_objects_only;
  bool visible_objects_only;
  bool use_instancing;
  enum eEvaluationMode evaluation_mode;
};

struct USDImportParams {
  float scale;
  bool is_sequence;
  bool set_frame_range;
  int sequence_len;
  int offset;
  bool validate_meshes;
  char mesh_read_flag;
  bool import_cameras;
  bool import_curves;
  bool import_lights;
  bool import_materials;
  bool import_meshes;
  bool import_volumes;
  char *prim_path_mask;
  bool import_subdiv;
  bool import_instance_proxies;
  bool create_collection;
  bool import_guide;
  bool import_proxy;
  bool import_render;
  bool import_visible_only;
  bool use_instancing;
  bool import_usd_preview;
  bool set_material_blend;
  float light_intensity_scale;
};

/* The USD_export takes a as_background_job parameter, and returns a boolean.
 *
 * When as_background_job=true, returns false immediately after scheduling
 * a background job.
 *
 * When as_background_job=false, performs the export synchronously, and returns
 * true when the export was ok, and false if there were any errors.
 */

bool USD_export(struct bContext *C,
                const char *filepath,
                const struct USDExportParams *params,
                bool as_background_job);

bool USD_import(struct bContext *C,
                const char *filepath,
                const struct USDImportParams *params,
                bool as_background_job);

int USD_get_version(void);

/* USD Import and Mesh Cache interface. */

struct CacheArchiveHandle *USD_create_handle(struct Main *bmain,
                                             const char *filename,
                                             struct ListBase *object_paths);

void USD_free_handle(struct CacheArchiveHandle *handle);

void USD_get_transform(struct CacheReader *reader, float r_mat[4][4], float time, float scale);

/* Either modifies current_mesh in-place or constructs a new mesh. */
struct Mesh *USD_read_mesh(struct CacheReader *reader,
                           struct Object *ob,
                           struct Mesh *existing_mesh,
                           float time,
                           const char **err_str,
                           int read_flag);

bool USD_mesh_topology_changed(struct CacheReader *reader,
                               struct Object *ob,
                               struct Mesh *existing_mesh,
                               float time,
                               const char **err_str);

struct CacheReader *CacheReader_open_usd_object(struct CacheArchiveHandle *handle,
                                                struct CacheReader *reader,
                                                struct Object *object,
                                                const char *object_path);

void USD_CacheReader_incref(struct CacheReader *reader);
void USD_CacheReader_free(struct CacheReader *reader);

#ifdef __cplusplus
}
#endif
