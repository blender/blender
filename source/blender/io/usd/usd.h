/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation */

#pragma once

#include "DEG_depsgraph.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CacheArchiveHandle;
struct CacheReader;
struct Object;
struct bContext;

/* Behavior when the name of an imported material
 * conflicts with an existing material. */
typedef enum eUSDMtlNameCollisionMode {
  USD_MTL_NAME_COLLISION_MAKE_UNIQUE = 0,
  USD_MTL_NAME_COLLISION_REFERENCE_EXISTING = 1,
} eUSDMtlNameCollisionMode;

/* Behavior when importing textures from a package
 * (e.g., USDZ archive) or from a URI path. */
typedef enum eUSDTexImportMode {
  USD_TEX_IMPORT_NONE = 0,
  USD_TEX_IMPORT_PACK,
  USD_TEX_IMPORT_COPY,
} eUSDTexImportMode;

/* Behavior when the name of an imported texture
 * file conflicts with an existing file. */
typedef enum eUSDTexNameCollisionMode {
  USD_TEX_NAME_COLLISION_USE_EXISTING = 0,
  USD_TEX_NAME_COLLISION_OVERWRITE = 1,
} eUSDTexNameCollisionMode;

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
  bool generate_preview_surface;
  bool export_textures;
  bool overwrite_textures;
  bool relative_paths;
  char root_prim_path[1024]; /* FILE_MAX */
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
  bool import_shapes;
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
  eUSDMtlNameCollisionMode mtl_name_collision_mode;
  eUSDTexImportMode import_textures_mode;
  char import_textures_dir[768]; /* FILE_MAXDIR */
  eUSDTexNameCollisionMode tex_name_collision_mode;
  bool import_all_materials;
};

/* This struct is in place to store the mesh sequence parameters needed when reading a data from a
 * usd file for the mesh sequence cache.
 */
typedef struct USDMeshReadParams {
  double motion_sample_time; /* USD TimeCode in frames. */
  int read_flags; /* MOD_MESHSEQ_xxx value that is set from MeshSeqCacheModifierData.read_flag. */
} USDMeshReadParams;

USDMeshReadParams create_mesh_read_params(double motion_sample_time, int read_flags);

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
                                             const char *filepath,
                                             struct ListBase *object_paths);

void USD_free_handle(struct CacheArchiveHandle *handle);

void USD_get_transform(struct CacheReader *reader, float r_mat[4][4], float time, float scale);

/* Either modifies current_mesh in-place or constructs a new mesh. */
struct Mesh *USD_read_mesh(struct CacheReader *reader,
                           struct Object *ob,
                           struct Mesh *existing_mesh,
                           USDMeshReadParams params,
                           const char **err_str);

bool USD_mesh_topology_changed(struct CacheReader *reader,
                               const struct Object *ob,
                               const struct Mesh *existing_mesh,
                               double time,
                               const char **err_str);

struct CacheReader *CacheReader_open_usd_object(struct CacheArchiveHandle *handle,
                                                struct CacheReader *reader,
                                                struct Object *object,
                                                const char *object_path);

void USD_CacheReader_incref(struct CacheReader *reader);
void USD_CacheReader_free(struct CacheReader *reader);
void USD_ensure_plugin_path_registered(void);
#ifdef __cplusplus
}
#endif
