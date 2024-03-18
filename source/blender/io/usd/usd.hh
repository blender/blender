/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

#include "DEG_depsgraph.hh"

#include "RNA_types.hh"

struct bContext;
struct CacheArchiveHandle;
struct CacheReader;
struct ListBase;
struct Mesh;
struct Object;
struct ReportList;
struct wmJobWorkerStatus;

namespace blender::io::usd {

/**
 * Behavior when the name of an imported material
 * conflicts with an existing material.
 */
enum eUSDMtlNameCollisionMode {
  USD_MTL_NAME_COLLISION_MAKE_UNIQUE = 0,
  USD_MTL_NAME_COLLISION_REFERENCE_EXISTING = 1,
};

/**
 *  Behavior when importing textures from a package
 * (e.g., USDZ archive) or from a URI path.
 */
enum eUSDTexImportMode {
  USD_TEX_IMPORT_NONE = 0,
  USD_TEX_IMPORT_PACK,
  USD_TEX_IMPORT_COPY,
};

/**
 * Behavior when the name of an imported texture
 * file conflicts with an existing file.
 */
enum eUSDTexNameCollisionMode {
  USD_TEX_NAME_COLLISION_USE_EXISTING = 0,
  USD_TEX_NAME_COLLISION_OVERWRITE = 1,
};

enum eSubdivExportMode {
  /** Subdivision scheme = None, export base mesh without subdivision. */
  USD_SUBDIV_IGNORE = 0,
  /** Subdivision scheme = None, export subdivided mesh. */
  USD_SUBDIV_TESSELLATE = 1,
  /**
   * Apply the USD subdivision scheme that is the closest match to Blender.
   * Reverts to #USD_SUBDIV_TESSELLATE if the subdivision method is not supported.
   */
  USD_SUBDIV_BEST_MATCH = 2,
};

struct USDExportParams {
  bool export_animation = false;
  bool export_hair = true;
  bool export_uvmaps = true;
  bool export_normals = true;
  bool export_mesh_colors = true;
  bool export_materials = true;
  bool export_armatures = true;
  bool export_shapekeys = true;
  bool only_deform_bones = false;
  eSubdivExportMode export_subdiv = USD_SUBDIV_BEST_MATCH;
  bool selected_objects_only = false;
  bool visible_objects_only = true;
  bool use_instancing = false;
  enum eEvaluationMode evaluation_mode = DAG_EVAL_VIEWPORT;
  bool generate_preview_surface = true;
  bool export_textures = true;
  bool overwrite_textures = true;
  bool relative_paths = true;
  char root_prim_path[1024] = ""; /* FILE_MAX */

  /** Communication structure between the wmJob management code and the worker code. Currently used
   * to generate safely reports from the worker thread. */
  wmJobWorkerStatus *worker_status;
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
  bool import_skeletons;
  bool import_blendshapes;
  char *prim_path_mask;
  bool import_subdiv;
  bool support_scene_instancing;
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

  /**
   * Communication structure between the wmJob management code and the worker code. Currently used
   * to generate safely reports from the worker thread.
   */
  wmJobWorkerStatus *worker_status;
};

/**
 * This struct is in place to store the mesh sequence parameters needed when reading a data from a
 * USD file for the mesh sequence cache.
 */
struct USDMeshReadParams {
  double motion_sample_time; /* USD TimeCode in frames. */
  int read_flags; /* MOD_MESHSEQ_xxx value that is set from MeshSeqCacheModifierData.read_flag. */
};

USDMeshReadParams create_mesh_read_params(double motion_sample_time, int read_flags);

/**
 * The USD_export takes a `as_background_job` parameter, and returns a boolean.
 *
 * When `as_background_job=true`, returns false immediately after scheduling
 * a background job.
 *
 * When `as_background_job=false`, performs the export synchronously, and returns
 * true when the export was ok, and false if there were any errors.
 */
bool USD_export(bContext *C,
                const char *filepath,
                const USDExportParams *params,
                bool as_background_job,
                ReportList *reports);

bool USD_import(bContext *C,
                const char *filepath,
                const USDImportParams *params,
                bool as_background_job,
                ReportList *reports);

int USD_get_version(void);

/* USD Import and Mesh Cache interface. */

CacheArchiveHandle *USD_create_handle(Main *bmain, const char *filepath, ListBase *object_paths);

void USD_free_handle(CacheArchiveHandle *handle);

void USD_get_transform(CacheReader *reader, float r_mat[4][4], float time, float scale);

/** Either modifies current_mesh in-place or constructs a new mesh. */
Mesh *USD_read_mesh(CacheReader *reader,
                    Object *ob,
                    Mesh *existing_mesh,
                    USDMeshReadParams params,
                    const char **err_str);

bool USD_mesh_topology_changed(CacheReader *reader,
                               const Object *ob,
                               const Mesh *existing_mesh,
                               double time,
                               const char **err_str);

CacheReader *CacheReader_open_usd_object(CacheArchiveHandle *handle,
                                         CacheReader *reader,
                                         Object *object,
                                         const char *object_path);

void USD_CacheReader_incref(CacheReader *reader);
void USD_CacheReader_free(CacheReader *reader);

/** Data for registering USD IO hooks. */
struct USDHook {

  /* Identifier used for class name. */
  char idname[64];
  /* Identifier used as label. */
  char name[64];
  /* Short help/description. */
  char description[1024]; /* #RNA_DYN_DESCR_MAX */

  /* rna_ext.data points to the USDHook class PyObject. */
  ExtensionRNA rna_ext;
};

void USD_register_hook(std::unique_ptr<USDHook> hook);
/**
 * Remove the given entry from the list of registered hooks and
 * free the allocated memory for the hook instance.
 */
void USD_unregister_hook(USDHook *hook);
USDHook *USD_find_hook_name(const char idname[]);

};  // namespace blender::io::usd
