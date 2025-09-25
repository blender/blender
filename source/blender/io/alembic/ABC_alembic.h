/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup balembic
 */
#include <string>

#include "BLI_vector.hh"

#include "DEG_depsgraph.hh"

struct CacheArchiveHandle;
struct CacheFileLayer;
struct CacheReader;
struct ListBase;
struct Main;
struct Mesh;
struct Object;
struct Scene;
struct bContext;

int ABC_get_version();

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

  char collection[MAX_ID_NAME - 2] = "";
};

struct AlembicImportParams {
  /* Multiplier for the cached data scale. Mostly useful if the data is stored in a different unit
   * as what Blender expects (e.g. centimeters instead of meters). */
  float global_scale;

  blender::Vector<std::string> paths;

  /* Last frame number of consecutive files to expect if the cached animation is split in a
   * sequence. */
  int sequence_max_frame;
  /* Start frame of the sequence, offset from 0. */
  int sequence_min_frame;
  /* True if the cache is split in multiple files. */
  bool is_sequence;

  /* True if the importer should set the current scene's start and end frame based on the start and
   * end frames of the cached animation. */
  bool set_frame_range;
  /* True if imported meshes should be validated. Error messages are sent to the console. */
  bool validate_meshes;
  /* True if a cache reader should be added regardless of whether there is animated data in the
   * cached file. */
  bool always_add_cache_reader;
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
                const struct AlembicImportParams *params,
                bool as_background_job);

struct CacheArchiveHandle *ABC_create_handle(const struct Main *bmain,
                                             const char *filepath,
                                             const struct CacheFileLayer *layers,
                                             struct ListBase *object_paths);

void ABC_free_handle(struct CacheArchiveHandle *handle);

void ABC_get_transform(struct CacheReader *reader,
                       float r_mat_world[4][4],
                       double time,
                       float scale);

struct ABCReadParams {
  double time;
  int read_flags;
  const char *velocity_name;
  float velocity_scale;
};

#ifdef __cplusplus
namespace blender::bke {
struct GeometrySet;
}

/* Either modifies the existing geometry component, or create a new one. */
void ABC_read_geometry(CacheReader *reader,
                       Object *ob,
                       blender::bke::GeometrySet &geometry_set,
                       const ABCReadParams *params,
                       const char **r_err_str);
#endif

bool ABC_mesh_topology_changed(struct CacheReader *reader,
                               struct Object *ob,
                               const struct Mesh *existing_mesh,
                               double time,
                               const char **r_err_str);

void ABC_CacheReader_free(struct CacheReader *reader);

struct CacheReader *CacheReader_open_alembic_object(struct CacheArchiveHandle *handle,
                                                    struct CacheReader *reader,
                                                    struct Object *object,
                                                    const char *object_path,
                                                    bool is_sequence);
