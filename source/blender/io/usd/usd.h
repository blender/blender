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

struct bContext;
struct Object;
struct CacheArchiveHandle;
struct CacheReader;
struct CacheFile;

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

static const USD_global_forward_axis USD_DEFAULT_FORWARD = USD_GLOBAL_FORWARD_MINUS_Z;
static const USD_global_up_axis USD_DEFAULT_UP = USD_GLOBAL_UP_Y;

typedef enum eUSDMtlNameCollisionMode {
  USD_MTL_NAME_COLLISION_MODIFY = 0,
  USD_MTL_NAME_COLLISION_SKIP = 1,
} eUSDMtlNameCollisionMode;

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
  char *default_prim_path;   // USD Stage Default Primitive Path
  char *root_prim_path;      // Root path to encapsulate blender stage under. e.g. /shot
  char *material_prim_path;  // Prim path to store all generated USDShade, shaders under e.g.
                             // /materials
  bool generate_preview_surface;
  bool convert_uv_to_st;
  bool convert_orientation;
  enum USD_global_forward_axis forward_axis;
  enum USD_global_up_axis up_axis;
  bool export_child_particles;
  bool export_as_overs;
  bool merge_transform_and_shape;
  bool export_custom_properties;
  bool export_identity_transforms;
  bool apply_subdiv;
  bool author_blender_name;
  bool vertex_data_as_face_varying;
  float frame_step;
  bool override_shutter;
  double shutter_open;
  double shutter_close;
  bool export_textures;
  bool relative_texture_paths;
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
  eUSDImportShadersMode import_shaders_mode;
  bool set_material_blend;
  float light_intensity_scale;
  bool apply_unit_conversion_scale;
  bool convert_light_from_nits;
  bool scale_light_radius;
  bool create_background_shader;
  eUSDMtlNameCollisionMode mtl_name_collision_mode;
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

bool USD_umm_module_loaded(void);

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
                           const float time,
                           const char **err_str,
                           int read_flag);

bool USD_mesh_topology_changed(struct CacheReader *reader,
                               struct Object *ob,
                               struct Mesh *existing_mesh,
                               const float time,
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
