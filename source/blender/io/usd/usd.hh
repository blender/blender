/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

#include "DEG_depsgraph.hh"

#include "DNA_modifier_types.h"
#include "RNA_types.hh"

struct bContext;
struct CacheArchiveHandle;
struct CacheReader;
struct CacheFile;
struct Object;
struct wmJobWorkerStatus;

namespace blender::bke {
struct GeometrySet;
}

namespace blender::io::usd {

typedef enum USD_global_forward_axis {
  USD_GLOBAL_FORWARD_X = 0,
  USD_GLOBAL_FORWARD_Y = 1,
  USD_GLOBAL_FORWARD_Z = 2,
  USD_GLOBAL_FORWARD_MINUS_X = 3,
  USD_GLOBAL_FORWARD_MINUS_Y = 4,
  USD_GLOBAL_FORWARD_MINUS_Z = 5
} USD_global_forward_axis;

typedef enum USD_global_up_axis {
  USD_GLOBAL_UP_X = 0,
  USD_GLOBAL_UP_Y = 1,
  USD_GLOBAL_UP_Z = 2,
  USD_GLOBAL_UP_MINUS_X = 3,
  USD_GLOBAL_UP_MINUS_Y = 4,
  USD_GLOBAL_UP_MINUS_Z = 5
} USD_global_up_axis;

typedef enum eUSDImportShadersMode {
  USD_IMPORT_SHADERS_NONE = 0,
  USD_IMPORT_USD_PREVIEW_SURFACE = 1,
  USD_IMPORT_MDL = 2,
} eUSDImportShadersMode;

typedef enum eUSDXformOpMode {
  USD_XFORM_OP_SRT = 0,
  USD_XFORM_OP_SOT = 1,
  USD_XFORM_OP_MAT = 2,
} eUSDXformOpMode;

typedef enum eUSDZTextureDownscaleSize {
  USD_TEXTURE_SIZE_CUSTOM = -1,
  USD_TEXTURE_SIZE_KEEP = 0,
  USD_TEXTURE_SIZE_256 = 256,
  USD_TEXTURE_SIZE_512 = 512,
  USD_TEXTURE_SIZE_1024 = 1024,
  USD_TEXTURE_SIZE_2048 = 2048,
  USD_TEXTURE_SIZE_4096 = 4096
} eUSDZTextureDownscaleSize;

static const USD_global_forward_axis USD_DEFAULT_FORWARD = USD_GLOBAL_FORWARD_MINUS_Z;
static const USD_global_up_axis USD_DEFAULT_UP = USD_GLOBAL_UP_Y;

/**
  * Behavior when the name of an imported material
  * conflicts with an existing material.
  */
typedef enum eUSDMtlNameCollisionMode {
  USD_MTL_NAME_COLLISION_MAKE_UNIQUE = 0,
  USD_MTL_NAME_COLLISION_REFERENCE_EXISTING = 1,
} eUSDMtlNameCollisionMode;

/*
 *  Behavior for importing of custom
 *  attributes / properties outside
 *  a prim's regular schema.
 */
typedef enum eUSDAttrImportMode {
  USD_ATTR_IMPORT_NONE = 0,
  USD_ATTR_IMPORT_USER = 1,
  USD_ATTR_IMPORT_ALL = 2,
} eUSDAttrImportMode;

typedef enum eUSDDefaultPrimKind {
  USD_KIND_NONE = 0,
  USD_KIND_COMPONENT,
  USD_KIND_GROUP,
  USD_KIND_ASSEMBLY,
  USD_KIND_CUSTOM
} eUSDDefaultPrimKind;

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
typedef enum eUSDTexNameCollisionMode {
  USD_TEX_NAME_COLLISION_USE_EXISTING = 0,
  USD_TEX_NAME_COLLISION_OVERWRITE = 1,
} eUSDTexNameCollisionMode;

typedef enum eSubdivExportMode {
  /** Subdivision scheme = None, export base mesh without subdivision. */
  USD_SUBDIV_IGNORE = 0,
  /** Subdivision scheme = None, export subdivided mesh. */
  USD_SUBDIV_TESSELLATE = 1,
  /**
    * Apply the USD subdivision scheme that is the closest match to Blender.
    * Reverts to #USD_SUBDIV_TESSELLATE if the subdivision method is not supported.
    */
    USD_SUBDIV_BEST_MATCH = 2,
} eSubdivExportMode;

struct USDExportParams {
  double frame_start = 0.0;
  double frame_end = 0.0;

  bool export_animation = false;
  bool export_hair = true;
  bool export_vertices = true;
  bool export_mesh_colors = true;
  bool export_vertex_groups = true;
  bool export_uvmaps = true;
  bool export_normals = true;
  bool export_mesh_attributes = true;
  bool export_transforms = true;
  bool export_materials = true;
  bool export_armatures = true;
  bool export_shapekeys = true;
  bool only_deform_bones = false;
  eSubdivExportMode export_subdiv = USD_SUBDIV_BEST_MATCH;
  bool export_meshes = true;
  bool export_lights = true;
  bool export_cameras = true;
  bool export_curves = true;
  bool export_particles = true;
  bool selected_objects_only = false;
  bool visible_objects_only = true;
  bool use_instancing = false;
  enum eEvaluationMode evaluation_mode = DAG_EVAL_VIEWPORT;
  bool generate_preview_surface = true;
  bool convert_uv_to_st = true;
  bool convert_orientation = false;
  enum USD_global_forward_axis forward_axis = USD_global_forward_axis::USD_GLOBAL_FORWARD_MINUS_Z;
  enum USD_global_up_axis up_axis = USD_global_up_axis::USD_GLOBAL_UP_Y;
  bool export_child_particles = false;
  bool export_as_overs = false;
  bool merge_transform_and_shape = false;
  bool add_properties_namespace = true;
  bool export_identity_transforms = true;
  bool vertex_data_as_face_varying = true;
  float frame_step = 1.0f;
  bool override_shutter = false;
  double shutter_open = 0.25;
  double shutter_close = 0.75;
  bool export_textures = true;
  bool relative_paths = true;
  bool use_original_paths = false;
  bool backward_compatible = true;
  float light_intensity_scale = 1.0f;
  bool generate_mdl = false;
  bool convert_to_cm = true;
  bool convert_light_to_nits = true;
  bool scale_light_radius = true;
  bool convert_world_material = true;
  bool generate_cycles_shaders = false;
  eUSDXformOpMode xform_op_mode = eUSDXformOpMode::USD_XFORM_OP_SRT;
  bool overwrite_textures = false;
  eUSDZTextureDownscaleSize usdz_downscale_size = eUSDZTextureDownscaleSize::USD_TEXTURE_SIZE_KEEP;
  int usdz_downscale_custom_size = 128;
  bool usdz_is_arkit = false;
  bool export_blender_metadata = true;
  bool triangulate_meshes = false;
  int quad_method = MOD_TRIANGULATE_QUAD_SHORTEDGE;
  int ngon_method = MOD_TRIANGULATE_NGON_BEAUTY;
  bool export_usd_kind = true;
  eUSDDefaultPrimKind default_prim_kind = eUSDDefaultPrimKind::USD_KIND_NONE;
  char default_prim_custom_kind[128] = "";
  bool export_custom_properties = true;
  bool author_blender_name = true;
  char root_prim_path[1024] = "/root";               /* FILE_MAX */
  char default_prim_path[1024] = "/root";            /* FILE_MAX */
  char material_prim_path[1024] = "/root/materials"; /* FILE_MAX */
  char collection[MAX_IDPROP_NAME] = "";

  /** Communication structure between the wmJob management code and the worker code. Currently used
    * to generate safely reports from the worker thread. */
  wmJobWorkerStatus* worker_status;
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
  char* prim_path_mask;
  bool import_subdiv;
  bool support_scene_instancing;
  bool create_collection;
  bool import_guide;
  bool import_proxy;
  bool import_render;
  bool import_visible_only;
  eUSDImportShadersMode import_shaders_mode;
  bool set_material_blend;
  float light_intensity_scale;
  bool apply_unit_conversion_scale;
  bool convert_light_from_nits;
  bool scale_light_radius;
  bool create_background_shader;
  eUSDMtlNameCollisionMode mtl_name_collision_mode;
  bool triangulate_meshes;
  bool import_defined_only;
  eUSDTexImportMode import_textures_mode;
  char import_textures_dir[768] = ""; /* FILE_MAXDIR */
  eUSDTexNameCollisionMode tex_name_collision_mode;
  bool import_all_materials;
  eUSDAttrImportMode attr_import_mode;

  /**
    * Communication structure between the wmJob management code and the worker code. Currently used
    * to generate safely reports from the worker thread.
    */
  wmJobWorkerStatus* worker_status;
};

/**
  * This struct is in place to store the mesh sequence parameters needed when reading a data from a
  * USD file for the mesh sequence cache.
  */
typedef struct USDMeshReadParams {
  double motion_sample_time; /* USD TimeCode in frames. */
  int read_flags; /* MOD_MESHSEQ_xxx value that is set from MeshSeqCacheModifierData.read_flag. */
} USDMeshReadParams;

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
bool USD_export(struct bContext* C,
  const char* filepath,
  const struct USDExportParams* params,
  bool as_background_job,
  ReportList* reports);

bool USD_import(struct bContext* C,
  const char* filepath,
  const struct USDImportParams* params,
  bool as_background_job,
  ReportList* reports);

int USD_get_version();

bool USD_umm_module_loaded(void);

/* USD Import and Mesh Cache interface. */

/* Similar to BLI_path_abs(), but also invokes the USD asset resolver
  * to determine the absolute path. This is necessary for resolving
  * paths with URIs that BLI_path_abs() would otherwise alter when
  * attempting to normalize the path. */
void USD_path_abs(char* path, const char* basepath, bool for_import);

struct CacheArchiveHandle* USD_create_handle(struct Main* bmain,
  const char* filepath,
  struct ListBase* object_paths);

void USD_free_handle(struct CacheArchiveHandle* handle);

void USD_get_transform(struct CacheReader* reader, float r_mat[4][4], float time, float scale);

/** Either modifies current_mesh in-place or constructs a new mesh. */
void USD_read_geometry(CacheReader *reader,
                       Object *ob,
                       blender::bke::GeometrySet &geometry_set,
                       USDMeshReadParams params,
                       const char **err_str);

bool USD_mesh_topology_changed(struct CacheReader* reader,
  const struct Object* ob,
  const struct Mesh* existing_mesh,
  double time,
  const char** err_str);

struct CacheReader* CacheReader_open_usd_object(struct CacheArchiveHandle* handle,
  struct CacheReader* reader,
  struct Object* object,
  const char* object_path);

void USD_CacheReader_incref(struct CacheReader* reader);
void USD_CacheReader_free(struct CacheReader* reader);

/** Data for registering USD IO hooks. */
struct USDHook {

  /* Identifier used for class name. */
  char idname[64];
  /* Identifier used as label. */
  char name[64];
  /* Short help/description. */
  char description[1024]; /* #RNA_DYN_DESCR_MAX */

  /* rna_ext.data points to the USDHook class PyObject. */
  struct ExtensionRNA rna_ext;
};

void USD_register_hook(std::unique_ptr<USDHook> hook);
/**
 * Remove the given entry from the list of registered hooks and
 * free the allocated memory for the hook instance.
 */
void USD_unregister_hook(USDHook* hook);
USDHook* USD_find_hook_name(const char idname[]);

};  // namespace blender::io::usd

