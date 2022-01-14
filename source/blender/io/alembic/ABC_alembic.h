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
 */
#pragma once

/** \file
 * \ingroup balembic
 */

#include "DEG_depsgraph.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CacheArchiveHandle;
struct CacheReader;
struct ListBase;
struct Main;
struct Mesh;
struct Object;
struct Scene;
struct bContext;

int ABC_get_version(void);

struct AlembicExportParams {
  double frame_start;
  double frame_end;

  unsigned int frame_samples_xform;
  unsigned int frame_samples_shape;

  double shutter_open;
  double shutter_close;

  bool selected_only;
  bool uvs;
  bool normals;
  bool vcolors;
  bool orcos;
  bool apply_subdiv;
  bool curves_as_mesh;
  bool flatten_hierarchy;
  bool visible_objects_only;
  bool face_sets;
  bool use_subdiv_schema;
  bool packuv;
  bool triangulate;
  bool export_hair;
  bool export_particles;
  bool export_custom_properties;
  bool use_instancing;
  enum eEvaluationMode evaluation_mode;

  /* See MOD_TRIANGULATE_NGON_xxx and MOD_TRIANGULATE_QUAD_xxx
   * in DNA_modifier_types.h */
  int quad_method;
  int ngon_method;

  float global_scale;
};

/* The ABC_export and ABC_import functions both take a as_background_job
 * parameter, and return a boolean.
 *
 * When as_background_job=true, returns false immediately after scheduling
 * a background job.
 *
 * When as_background_job=false, performs the export synchronously, and returns
 * true when the export was ok, and false if there were any errors.
 */

bool ABC_export(struct Scene *scene,
                struct bContext *C,
                const char *filepath,
                const struct AlembicExportParams *params,
                bool as_background_job);

bool ABC_import(struct bContext *C,
                const char *filepath,
                float scale,
                bool is_sequence,
                bool set_frame_range,
                int sequence_len,
                int offset,
                bool validate_meshes,
                bool always_add_cache_reader,
                bool as_background_job);

struct CacheArchiveHandle *ABC_create_handle(struct Main *bmain,
                                             const char *filename,
                                             struct ListBase *object_paths);

void ABC_free_handle(struct CacheArchiveHandle *handle);

void ABC_get_transform(struct CacheReader *reader,
                       float r_mat_world[4][4],
                       float time,
                       float scale);

/* Either modifies existing_mesh in-place or constructs a new mesh. */
struct Mesh *ABC_read_mesh(struct CacheReader *reader,
                           struct Object *ob,
                           struct Mesh *existing_mesh,
                           float time,
                           const char **err_str,
                           int read_flags,
                           const char *velocity_name,
                           float velocity_scale);

bool ABC_mesh_topology_changed(struct CacheReader *reader,
                               struct Object *ob,
                               struct Mesh *existing_mesh,
                               float time,
                               const char **err_str);

void ABC_CacheReader_incref(struct CacheReader *reader);
void ABC_CacheReader_free(struct CacheReader *reader);

struct CacheReader *CacheReader_open_alembic_object(struct CacheArchiveHandle *handle,
                                                    struct CacheReader *reader,
                                                    struct Object *object,
                                                    const char *object_path);

#ifdef __cplusplus
}
#endif
