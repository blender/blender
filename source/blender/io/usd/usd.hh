/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

#include "../common/IO_orientation.hh"

#include "DEG_depsgraph.hh"

#include "DNA_modifier_types.h"
#include "RNA_types.hh"

struct bContext;
struct CacheArchiveHandle;
struct CacheReader;
struct ListBase;
struct Mesh;
struct Object;
struct ReportList;
struct wmJobWorkerStatus;

namespace blender::bke {
struct GeometrySet;
}

namespace blender::io::usd {

/**
 * Behavior when the name of an imported material
 * conflicts with an existing material.
 */
enum eUSDMtlNameCollisionMode {
  USD_MTL_NAME_COLLISION_MAKE_UNIQUE = 0,
  USD_MTL_NAME_COLLISION_REFERENCE_EXISTING = 1,
};

/* Enums specifying the USD material purpose,
 * corresponding to #pxr::UsdShadeTokens 'allPurpose',
 * 'preview', and 'render', respectively. */
enum eUSDMtlPurpose {
  USD_MTL_PURPOSE_ALL = 0,
  USD_MTL_PURPOSE_PREVIEW = 1,
  USD_MTL_PURPOSE_FULL = 2
};

/**
 *  Behavior for importing of custom
 *  attributes / properties outside
 *  a prim's regular schema.
 */
enum eUSDPropertyImportMode {
  USD_ATTR_IMPORT_NONE = 0,
  USD_ATTR_IMPORT_USER = 1,
  USD_ATTR_IMPORT_ALL = 2,
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

enum eUSDXformOpMode {
  USD_XFORM_OP_TRS = 0,
  USD_XFORM_OP_TOS = 1,
  USD_XFORM_OP_MAT = 2,
};

enum eUSDZTextureDownscaleSize {
  USD_TEXTURE_SIZE_CUSTOM = -1,
  USD_TEXTURE_SIZE_KEEP = 0,
  USD_TEXTURE_SIZE_256 = 256,
  USD_TEXTURE_SIZE_512 = 512,
  USD_TEXTURE_SIZE_1024 = 1024,
  USD_TEXTURE_SIZE_2048 = 2048,
  USD_TEXTURE_SIZE_4096 = 4096
};

/**
 *  Behavior when exporting textures.
 */
enum eUSDTexExportMode {
  USD_TEX_EXPORT_KEEP = 0,
  USD_TEX_EXPORT_PRESERVE,
  USD_TEX_EXPORT_NEW_PATH,
};

enum eUSDSceneUnits {
  USD_SCENE_UNITS_CUSTOM = -1,
  USD_SCENE_UNITS_METERS = 0,
  USD_SCENE_UNITS_KILOMETERS = 1,
  USD_SCENE_UNITS_CENTIMETERS = 2,
  USD_SCENE_UNITS_MILLIMETERS = 3,
  USD_SCENE_UNITS_INCHES = 4,
  USD_SCENE_UNITS_FEET = 5,
  USD_SCENE_UNITS_YARDS = 6,
};

struct USDExportParams {
  bool export_animation = false;
  bool selected_objects_only = false;

  bool export_meshes = true;
  bool export_lights = true;
  bool export_cameras = true;
  bool export_curves = true;
  bool export_points = true;
  bool export_volumes = true;
  bool export_hair = true;
  bool export_uvmaps = true;
  bool rename_uvmaps = true;
  bool export_normals = true;
  bool export_mesh_colors = true;
  bool export_materials = true;

  bool export_armatures = true;
  bool export_shapekeys = true;
  bool only_deform_bones = false;

  bool convert_world_material = true;
  bool merge_parent_xform = false;

  bool use_instancing = false;
  bool export_custom_properties = true;
  bool author_blender_name = true;
  bool allow_unicode = true;

  eSubdivExportMode export_subdiv = USD_SUBDIV_BEST_MATCH;
  enum eEvaluationMode evaluation_mode = DAG_EVAL_VIEWPORT;

  bool generate_preview_surface = true;
  bool generate_materialx_network = true;
  bool export_textures = false;
  bool overwrite_textures = true;
  bool relative_paths = true;
  bool use_original_paths = false;

  bool triangulate_meshes = false;
  int quad_method = MOD_TRIANGULATE_QUAD_SHORTEDGE;
  int ngon_method = MOD_TRIANGULATE_NGON_BEAUTY;

  bool convert_orientation = false;
  enum eIOAxis forward_axis = eIOAxis::IO_AXIS_NEGATIVE_Z;
  enum eIOAxis up_axis = eIOAxis::IO_AXIS_Y;
  eUSDXformOpMode xform_op_mode = eUSDXformOpMode::USD_XFORM_OP_TRS;

  eUSDZTextureDownscaleSize usdz_downscale_size = eUSDZTextureDownscaleSize::USD_TEXTURE_SIZE_KEEP;
  int usdz_downscale_custom_size = 128;

  std::string root_prim_path = "";
  char collection[MAX_ID_NAME - 2] = "";
  char custom_properties_namespace[MAX_IDPROP_NAME] = "";

  eUSDSceneUnits convert_scene_units = eUSDSceneUnits::USD_SCENE_UNITS_METERS;
  float custom_meters_per_unit = 1.0f;

  /** Communication structure between the wmJob management code and the worker code. Currently used
   * to generate safely reports from the worker thread. */
  wmJobWorkerStatus *worker_status = nullptr;
};

struct USDImportParams {
  float scale;
  float light_intensity_scale;
  bool apply_unit_conversion_scale;

  char mesh_read_flag;
  bool set_frame_range;
  bool is_sequence;
  int sequence_len;
  int offset;
  bool relative_path;

  bool import_defined_only;
  bool import_visible_only;

  bool import_cameras;
  bool import_curves;
  bool import_lights;
  bool import_materials;
  bool import_all_materials;
  bool import_meshes;
  bool import_points;
  bool import_subdivision;
  bool import_volumes;

  bool import_shapes;
  bool import_skeletons;
  bool import_blendshapes;

  bool create_collection;
  bool create_world_material;
  bool support_scene_instancing;

  bool import_guide;
  bool import_proxy;
  bool import_render;
  bool import_usd_preview;
  bool set_material_blend;

  bool validate_meshes;
  bool merge_parent_xform;

  eUSDMtlPurpose mtl_purpose;
  eUSDMtlNameCollisionMode mtl_name_collision_mode;
  eUSDTexImportMode import_textures_mode;

  std::string prim_path_mask;
  char import_textures_dir[/*FILE_MAXDIR*/ 768];
  eUSDTexNameCollisionMode tex_name_collision_mode;
  eUSDPropertyImportMode property_import_mode;

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
bool USD_export(const bContext *C,
                const char *filepath,
                const USDExportParams *params,
                bool as_background_job,
                ReportList *reports);

bool USD_import(const bContext *C,
                const char *filepath,
                const USDImportParams *params,
                bool as_background_job,
                ReportList *reports);

int USD_get_version();

/* USD Import and Mesh Cache interface. */

/* Similar to BLI_path_abs(), but also invokes the USD asset resolver
 * to determine the absolute path. This is necessary for resolving
 * paths with URIs that BLI_path_abs() would otherwise alter when
 * attempting to normalize the path. */
void USD_path_abs(char *path, const char *basepath, bool for_import);

CacheArchiveHandle *USD_create_handle(Main *bmain, const char *filepath, ListBase *object_paths);

void USD_free_handle(CacheArchiveHandle *handle);

void USD_get_transform(CacheReader *reader, float r_mat[4][4], float time, float scale);

/** Either modifies current_mesh in-place or constructs a new mesh. */
void USD_read_geometry(CacheReader *reader,
                       const Object *ob,
                       blender::bke::GeometrySet &geometry_set,
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

/** Data for registering USD IO hooks. */
struct USDHook {

  /* Identifier used for class name. */
  char idname[64];
  /* Identifier used as label. */
  char name[64];
  /* Short help/description. */
  char description[/*RNA_DYN_DESCR_MAX*/ 1024];

  /* rna_ext.data points to the USDHook class PyObject. */
  ExtensionRNA rna_ext;
};

void USD_register_hook(std::unique_ptr<USDHook> hook);
/**
 * Remove the given entry from the list of registered hooks and
 * free the allocated memory for the hook instance.
 */
void USD_unregister_hook(const USDHook *hook);
USDHook *USD_find_hook_name(const char idname[]);

double get_meters_per_unit(const USDExportParams &params);

};  // namespace blender::io::usd
