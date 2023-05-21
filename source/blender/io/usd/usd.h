/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation */

#pragma once

#include "DEG_depsgraph.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bContext;
struct CacheArchiveHandle;
struct CacheReader;
struct CacheFile;
struct Object;

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
  USD_TEXTURE_SIZE_256  = 256,
  USD_TEXTURE_SIZE_512  = 512,
  USD_TEXTURE_SIZE_1024 = 1024,
  USD_TEXTURE_SIZE_2048 = 2048,
  USD_TEXTURE_SIZE_4096 = 4096
} eUSDZTextureDownscaleSize;

static const USD_global_forward_axis USD_DEFAULT_FORWARD = USD_GLOBAL_FORWARD_MINUS_Z;
static const USD_global_up_axis USD_DEFAULT_UP = USD_GLOBAL_UP_Y;

/* Behavior when the name of an imported material
 * conflicts with an existing material. */
typedef enum eUSDMtlNameCollisionMode {
  USD_MTL_NAME_COLLISION_MAKE_UNIQUE = 0,
  USD_MTL_NAME_COLLISION_REFERENCE_EXISTING = 1,
} eUSDMtlNameCollisionMode;

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
  double frame_start;
  double frame_end;

  bool export_animation;
  bool export_hair;
  bool export_vertices;
  bool export_vertex_colors;
  bool export_vertex_groups;
  bool export_face_maps;
  bool export_uvmaps;
  bool export_normals;
  bool export_transforms;
  bool export_materials;
  bool export_meshes;
  bool export_lights;
  bool export_cameras;
  bool export_curves;
  bool export_particles;
  bool selected_objects_only;
  bool visible_objects_only;
  bool use_instancing;
  enum eEvaluationMode evaluation_mode;
  bool generate_preview_surface;
  bool convert_uv_to_st;
  bool convert_orientation;
  enum USD_global_forward_axis forward_axis;
  enum USD_global_up_axis up_axis;
  bool export_child_particles;
  bool export_as_overs;
  bool merge_transform_and_shape;
  bool export_custom_properties;
  bool add_properties_namespace;
  bool export_identity_transforms;
  bool apply_subdiv;
  bool author_blender_name;
  bool vertex_data_as_face_varying;
  float frame_step;
  bool override_shutter;
  double shutter_open;
  double shutter_close;
  bool export_textures;
  bool relative_paths;
  bool backward_compatible;
  float light_intensity_scale;
  bool generate_mdl;
  bool convert_to_cm;
  bool convert_light_to_nits;
  bool scale_light_radius;
  bool convert_world_material;
  bool generate_cycles_shaders;
  bool export_armatures;
  eUSDXformOpMode xform_op_mode;
  bool fix_skel_root;
  bool overwrite_textures;
  bool export_blendshapes;
  eUSDZTextureDownscaleSize usdz_downscale_size;
  int usdz_downscale_custom_size;
  bool usdz_is_arkit;
  bool export_blender_metadata;
  bool triangulate_meshes;
  int quad_method;
  int ngon_method;
  bool export_usd_kind;
  eUSDDefaultPrimKind default_prim_kind;
  char *default_prim_custom_kind;
  char root_prim_path[1024]; /* FILE_MAX */
  char default_prim_path[1024];  /* FILE_MAX */
  char material_prim_path[1024];  /* FILE_MAX */
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
  bool import_blendshapes;
  bool import_volumes;
  bool import_skeletons;
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
  eUSDImportShadersMode import_shaders_mode;
  bool set_material_blend;
  float light_intensity_scale;
  bool apply_unit_conversion_scale;
  bool convert_light_from_nits;
  bool scale_light_radius;
  bool create_background_shader;
  eUSDMtlNameCollisionMode mtl_name_collision_mode;
  eUSDAttrImportMode attr_import_mode;
  bool triangulate_meshes;
  bool import_defined_only;
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

bool USD_umm_module_loaded(void);

/* USD Import and Mesh Cache interface. */

/* Similar to BLI_path_abs(), but also invokes the USD asset resolver
 * to determine the absolute path. This is necessary for resolving
 * paths with URIs that BLI_path_abs() would otherwise alter when
 * attempting to normalize the path. */
void USD_path_abs(char *path, const char *basepath, bool for_import);

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
