/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "../common/IO_orientation.hh"

#include "DEG_depsgraph.hh"

#include "DNA_modifier_types.h"

namespace blender {

struct bContext;
struct Mesh;
struct Object;
struct ReportList;
struct wmJobWorkerStatus;

namespace io::usd {

/**
 * Behavior when the name of an imported material
 * conflicts with an existing material.
 */
enum class MtlNameCollisionMode {
  MakeUnique = 0,
  ReferenceExisting = 1,
};

/* Enums specifying the USD material purpose,
 * corresponding to #pxr::UsdShadeTokens 'allPurpose',
 * 'preview', and 'render', respectively. */
enum class MtlPurpose { All = 0, Preview = 1, Full = 2 };

/**
 *  Behavior for importing of custom
 *  attributes / properties outside
 *  a prim's regular schema.
 */
enum class PropertyImportMode {
  None = 0,
  User = 1,
  All = 2,
};

/**
 *  Behavior when importing textures from a package
 * (e.g., USDZ archive) or from a URI path.
 */
enum class TexImportMode {
  None = 0,
  Pack,
  Copy,
};

/**
 * Behavior when the name of an imported texture
 * file conflicts with an existing file.
 */
enum class TexNameCollisionMode {
  UseExisting = 0,
  Overwrite = 1,
};

enum class SubdivExportMode {
  /** Subdivision scheme = None, export base mesh without subdivision. */
  Ignore = 0,
  /** Subdivision scheme = None, export subdivided mesh. */
  Tessellate = 1,
  /**
   * Apply the USD subdivision scheme that is the closest match to Blender.
   * Reverts to #SubdivExportMode::Tessellate if the subdivision method is not supported.
   */
  Match = 2,
};

enum class XformOpMode {
  TRS = 0,
  TOS = 1,
  MAT = 2,
};

enum class TextureDownscaleSize {
  Custom = -1,
  Keep = 0,
  Size256 = 256,
  Size512 = 512,
  Size1024 = 1024,
  Size2048 = 2048,
  Size4096 = 4096
};

/**
 *  Behavior when exporting textures.
 */
enum class TexExportMode {
  Keep = 0,
  Preserve,
  NewPath,
};

enum class SceneUnits {
  Custom = -1,
  Meters = 0,
  Kilometers = 1,
  Centimeters = 2,
  Millimeters = 3,
  Inches = 4,
  Feet = 5,
  Yards = 6,
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

  SubdivExportMode export_subdiv = SubdivExportMode::Match;
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
  XformOpMode xform_op_mode = XformOpMode::TRS;

  TextureDownscaleSize usdz_downscale_size = TextureDownscaleSize::Keep;
  int usdz_downscale_custom_size = 128;

  std::string root_prim_path = "";
  char collection[MAX_ID_NAME - 2] = "";
  char custom_properties_namespace[MAX_IDPROP_NAME] = "";

  std::string accessibility_label = "";
  std::string accessibility_description = "";

  SceneUnits convert_scene_units = SceneUnits::Meters;
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

  MtlPurpose mtl_purpose;
  MtlNameCollisionMode mtl_name_collision_mode;
  TexImportMode import_textures_mode;

  std::string prim_path_mask;
  char import_textures_dir[/*FILE_MAXDIR*/ 768];
  TexNameCollisionMode tex_name_collision_mode;
  PropertyImportMode property_import_mode;

  /**
   * Communication structure between the wmJob management code and the worker code. Currently used
   * to generate safely reports from the worker thread.
   */
  wmJobWorkerStatus *worker_status;
};

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

/* Similar to BLI_path_abs(), but also invokes the USD asset resolver
 * to determine the absolute path. This is necessary for resolving
 * paths with URIs that BLI_path_abs() would otherwise alter when
 * attempting to normalize the path. */
void USD_path_abs(char *path, const char *basepath, bool for_import);

double get_meters_per_unit(const USDExportParams &params);

};  // namespace io::usd

}  // namespace blender
