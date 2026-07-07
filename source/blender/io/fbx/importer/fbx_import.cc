/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fbx
 */

#include "BKE_camera.h"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_light.h"
#include "BKE_object.hh"
#include "BKE_report.hh"

#include "BLI_fileops.h"
#include "BLI_math_rotation.h"
#include "BLI_task.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "DNA_camera_types.h"
#include "DNA_collection_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"

#include "IO_fbx.hh"

#include "fbx_import.hh"
#include "fbx_import_anim.hh"
#include "fbx_import_armature.hh"
#include "fbx_import_material.hh"
#include "fbx_import_mesh.hh"
#include "fbx_import_util.hh"

#include "CLG_log.h"

namespace blender {

static CLG_LogRef LOG = {"io.fbx"};

namespace io::fbx {

struct FbxImportContext {
  Main *bmain;
  const ufbx_scene &fbx;
  const FBXImportParams &params;
  std::string base_dir;
  FbxElementMapping mapping;

  FbxImportContext(Main *main, const ufbx_scene *fbx, const FBXImportParams &params)
      : bmain(main), fbx(*fbx), params(params)
  {
    char basedir[FILE_MAX];
    BLI_path_split_dir_part(params.filepath, basedir, sizeof(basedir));
    base_dir = basedir;

    ufbx_transform root_tr;
    root_tr.translation = ufbx_zero_vec3;
    root_tr.rotation = this->fbx.metadata.root_rotation;
    root_tr.scale.x = root_tr.scale.y = root_tr.scale.z = this->fbx.metadata.root_scale;
    this->mapping.global_conv_matrix = ufbx_transform_to_matrix(&root_tr);

#ifdef FBX_DEBUG_PRINT
    std::string debug_file_path = params.filepath;
    debug_file_path = debug_file_path.substr(0, debug_file_path.size() - 4) + "-dbg-b.txt";
    g_debug_file = BLI_fopen(debug_file_path.c_str(), "wb");
#endif
  }

  ~FbxImportContext()
  {
#ifdef FBX_DEBUG_PRINT
    if (g_debug_file) {
      fclose(g_debug_file);
    }
#endif
  }

  void import_globals(Scene *scene) const;
  void import_materials();
  void import_meshes();
  void import_cameras();
  void import_lights();
  void import_empties();
  void import_armatures();
  void import_animation(double fps);

  void setup_hierarchy();
};

void FbxImportContext::import_globals(Scene *scene) const
{
  /* Set scene frame-rate to that of FBX file. */
  double fps = this->fbx.settings.frames_per_second;
  scene->r.frs_sec = roundf(fps);
  scene->r.frs_sec_base = scene->r.frs_sec / fps;
}

void FbxImportContext::import_materials()
{
  for (const ufbx_material *fmat : this->fbx.materials) {
    Material *mat = nullptr;
    /* Check if a material with this name already exists in the main database */
    if (this->params.mtl_name_collision_mode == eFBXMtlNameCollisionMode::ReferenceExisting) {
      mat = (Material *)BKE_libblock_find_name(this->bmain, ID_MA, fmat->name.data);
    }

    if (mat == nullptr) {
      mat = io::fbx::import_material(this->bmain, this->base_dir, *fmat);
      if (this->params.use_custom_props) {
        read_custom_properties(fmat->props, mat->id, this->params.props_enum_as_string);
      }
    }
    this->mapping.mat_to_material.add(fmat, mat);
  }
}

void FbxImportContext::import_meshes()
{
  io::fbx::import_meshes(*this->bmain, this->fbx, this->mapping, this->params);
}

static bool should_import_camera(const ufbx_scene &fbx, const ufbx_camera *camera)
{
  BLI_assert(camera->instances.count > 0);
  const ufbx_node *node = camera->instances[0];
  /* Files produced by MotionBuilder have several cameras at the root,
   * which just map to "viewports" and should not get imported. */
  if (node->node_depth == 1 && node->children.count == 0 &&
      STREQ("MotionBuilder", fbx.metadata.original_application.name.data))
  {
    if (STREQ(node->name.data, camera->name.data)) {
      if (STREQ("Producer Perspective", node->name.data) ||
          STREQ("Producer Front", node->name.data) || STREQ("Producer Back", node->name.data) ||
          STREQ("Producer Right", node->name.data) || STREQ("Producer Left", node->name.data) ||
          STREQ("Producer Top", node->name.data) || STREQ("Producer Bottom", node->name.data))
      {
        return false;
      }
    }
  }
  return true;
}

void FbxImportContext::import_cameras()
{
  for (const ufbx_camera *fcam : this->fbx.cameras) {
    if (fcam->instances.count == 0) {
      continue; /* Ignore if not used by any objects. */
    }
    if (!should_import_camera(this->fbx, fcam)) {
      continue;
    }
    const ufbx_node *node = fcam->instances[0];

    Camera *bcam = BKE_camera_add(this->bmain, get_fbx_name(fcam->name, "Camera"));
    if (this->params.use_custom_props) {
      read_custom_properties(fcam->props, bcam->id, this->params.props_enum_as_string);
    }

    bcam->type = fcam->projection_mode == UFBX_PROJECTION_MODE_ORTHOGRAPHIC ? CAM_ORTHO :
                                                                              CAM_PERSP;
    bcam->dof.focus_distance = ufbx_find_real(&fcam->props, "FocusDistance", 10.0f) *
                               this->fbx.metadata.geometry_scale * this->fbx.metadata.root_scale;
    if (ufbx_find_bool(&fcam->props, "UseDepthOfField", false)) {
      bcam->dof.flag |= CAM_DOF_ENABLED;
    }
    bcam->lens = fcam->focal_length_mm;
    constexpr double m_to_in = 0.0393700787;
    bcam->sensor_x = fcam->film_size_inch.x / m_to_in;
    bcam->sensor_y = fcam->film_size_inch.y / m_to_in;

    /* Note: do not use `fcam->orthographic_extent` to match Python importer behavior, which was
     * not taking ortho units into account. */
    bcam->ortho_scale = ufbx_find_real(&fcam->props, "OrthoZoom", 1.0);

    bcam->shiftx = ufbx_find_real(&fcam->props, "FilmOffsetX", 0.0) / (m_to_in * bcam->sensor_x);
    bcam->shifty = ufbx_find_real(&fcam->props, "FilmOffsetY", 0.0) / (m_to_in * bcam->sensor_x);
    bcam->clip_start = fcam->near_plane * this->fbx.metadata.root_scale;
    bcam->clip_end = fcam->far_plane * this->fbx.metadata.root_scale;

    Object *obj = BKE_object_add_only_object(this->bmain, OB_CAMERA, get_fbx_name(node->name));
    obj->data = id_cast<ID *>(bcam);
    if (!node->visible) {
      obj->visibility_flag |= OB_HIDE_VIEWPORT;
    }
    if (this->params.use_custom_props) {
      read_custom_properties(node->props, obj->id, this->params.props_enum_as_string);
    }
    node_matrix_to_obj(node, obj, this->mapping);
    this->mapping.el_to_object.add(&node->element, obj);
    this->mapping.imported_objects.add(obj);
  }
}

void FbxImportContext::import_lights()
{
  for (const ufbx_light *flight : this->fbx.lights) {
    if (flight->instances.count == 0) {
      continue; /* Ignore if not used by any objects. */
    }
    const ufbx_node *node = flight->instances[0];

    Light *lamp = BKE_light_add(this->bmain, get_fbx_name(flight->name, "Light"));
    if (this->params.use_custom_props) {
      read_custom_properties(flight->props, lamp->id, this->params.props_enum_as_string);
    }
    switch (flight->type) {
      case UFBX_LIGHT_POINT:
        lamp->type = LA_LOCAL;
        break;
      case UFBX_LIGHT_DIRECTIONAL:
        lamp->type = LA_SUN;
        break;
      case UFBX_LIGHT_SPOT:
        lamp->type = LA_SPOT;
        lamp->spotsize = DEG2RAD(flight->outer_angle);
        lamp->spotblend = 1.0f - flight->inner_angle / flight->outer_angle;
        break;
      default:
        break;
    }

    lamp->r = flight->color.x;
    lamp->g = flight->color.y;
    lamp->b = flight->color.z;
    lamp->energy = flight->intensity;
    lamp->exposure = ufbx_find_real(&flight->props, "Exposure", 0.0);
    if (flight->cast_shadows) {
      lamp->mode |= LA_SHADOW;
    }
    //@TODO: if hasattr(lamp, "cycles"): lamp.cycles.cast_shadow = lamp.use_shadow

    Object *obj = BKE_object_add_only_object(this->bmain, OB_LAMP, get_fbx_name(node->name));
    obj->data = id_cast<ID *>(lamp);
    if (!node->visible) {
      obj->visibility_flag |= OB_HIDE_VIEWPORT;
    }

    if (this->params.use_custom_props) {
      read_custom_properties(node->props, obj->id, this->params.props_enum_as_string);
    }
    node_matrix_to_obj(node, obj, this->mapping);
    this->mapping.el_to_object.add(&node->element, obj);
    this->mapping.imported_objects.add(obj);
  }
}

void FbxImportContext::import_armatures()
{
  io::fbx::import_armatures(*this->bmain, this->fbx, this->mapping, this->params);
}

void FbxImportContext::import_empties()
{
  /* Create empties for fbx nodes. */
  for (const ufbx_node *node : this->fbx.nodes) {
    /* Ignore root, bones and nodes for which we have created objects already. */
    if (node->is_root || this->mapping.node_is_blender_bone.contains(node) ||
        this->mapping.el_to_object.contains(&node->element))
    {
      continue;
    }
    /* Ignore nodes at root for cameras (normally already imported, except for ignored cameras)
     * and camera switchers. */
    if (ELEM(node->attrib_type, UFBX_ELEMENT_CAMERA, UFBX_ELEMENT_CAMERA_SWITCHER) &&
        node->node_depth == 1 && node->children.count == 0)
    {
      continue;
    }
    Object *obj = BKE_object_add_only_object(this->bmain, OB_EMPTY, get_fbx_name(node->name));
    obj->data = nullptr;
    if (!node->visible) {
      obj->visibility_flag |= OB_HIDE_VIEWPORT;
    }
    if (this->params.use_custom_props) {
      read_custom_properties(node->props, obj->id, this->params.props_enum_as_string);
    }
    node_matrix_to_obj(node, obj, this->mapping);
    this->mapping.el_to_object.add(&node->element, obj);
    this->mapping.imported_objects.add(obj);
  }
}

void FbxImportContext::import_animation(double fps)
{
  if (this->params.use_anim) {
    io::fbx::import_animations(
        *this->bmain, this->fbx, this->mapping, fps, this->params.anim_offset);
  }
}

void FbxImportContext::setup_hierarchy()
{
  for (const auto &item : this->mapping.el_to_object.items()) {
    if (item.value->parent != nullptr) {
      continue; /* Parent is already set up (e.g. armature). */
    }
    const ufbx_node *node = ufbx_as_node(item.key);
    if (node == nullptr) {
      continue;
    }
    if (node->parent) {
      Object *obj_par = this->mapping.el_to_object.lookup_default(&node->parent->element, nullptr);
      if (!ELEM(obj_par, nullptr, item.value)) {
        item.value->parent = obj_par;
      }
    }
  }
}

static void fbx_task_run_fn(void * /* user */,
                            ufbx_thread_pool_context ctx,
                            uint32_t /* group */,
                            uint32_t start_index,
                            uint32_t count)
{
  threading::parallel_for_each(IndexRange(start_index, count), [&](const int64_t index) {
    ufbx_thread_pool_run_task(ctx, index);
  });
}

static void fbx_task_wait_fn(void * /* user */,
                             ufbx_thread_pool_context /* ctx */,
                             uint32_t /* group */,
                             uint32_t /* max_index */)
{
  /* Empty implementation; #fbx_task_run_fn already waits for the tasks.
   * This means that only one fbx "task group" is effectively scheduled at once. */
}

void importer_main(Main *bmain, Scene *scene, ViewLayer *view_layer, const FBXImportParams &params)
{
  FILE *file = BLI_fopen(params.filepath, "rb");
  if (!file) {
    CLOG_ERROR(&LOG, "Failed to open FBX file '%s'", params.filepath);
    BKE_reportf(params.reports, RPT_ERROR, "FBX Import: Cannot open file '%s'", params.filepath);
    return;
  }

  ufbx_load_opts opts = {};
  opts.filename.data = params.filepath;
  opts.filename.length = strlen(params.filepath);
  opts.evaluate_skinning = false;
  opts.evaluate_caches = false;
  opts.load_external_files = false;
  opts.clean_skin_weights = true;
  opts.use_blender_pbr_material = true;

  opts.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY;
  opts.pivot_handling = UFBX_PIVOT_HANDLING_ADJUST_TO_ROTATION_PIVOT;

  opts.space_conversion = UFBX_SPACE_CONVERSION_ADJUST_TRANSFORMS;
  opts.target_axes.right = UFBX_COORDINATE_AXIS_POSITIVE_X;
  opts.target_axes.up = UFBX_COORDINATE_AXIS_POSITIVE_Z;
  opts.target_axes.front = UFBX_COORDINATE_AXIS_NEGATIVE_Y;
  opts.target_unit_meters = 1.0f / params.global_scale;

  opts.target_camera_axes.right = UFBX_COORDINATE_AXIS_POSITIVE_X;
  opts.target_camera_axes.up = UFBX_COORDINATE_AXIS_POSITIVE_Y;
  opts.target_camera_axes.front = UFBX_COORDINATE_AXIS_POSITIVE_Z;
  opts.target_light_axes.right = UFBX_COORDINATE_AXIS_POSITIVE_X;
  opts.target_light_axes.up = UFBX_COORDINATE_AXIS_POSITIVE_Y;
  opts.target_light_axes.front = UFBX_COORDINATE_AXIS_POSITIVE_Z;

  /* Setup ufbx threading to go through our own task system. */
  opts.thread_opts.pool.run_fn = fbx_task_run_fn;
  opts.thread_opts.pool.wait_fn = fbx_task_wait_fn;

  ufbx_error fbx_error;
  ufbx_scene *fbx = ufbx_load_stdio(file, &opts, &fbx_error);
  fclose(file);

  if (!fbx) {
    CLOG_ERROR(&LOG,
               "Failed to import FBX file '%s': '%s'\n",
               params.filepath,
               fbx_error.description.data);
    BKE_reportf(params.reports,
                RPT_ERROR,
                "FBX Import: Cannot import file '%s': '%s'",
                params.filepath,
                fbx_error.description.data);
    return;
  }

  LayerCollection *lc = BKE_layer_collection_get_active_editable(view_layer);
  if (!ID_IS_EDITABLE(lc->collection)) {
    BKE_report(params.reports,
               RPT_WARNING,
               "Could not find an editable collection in current scene, imported data will not be "
               "instantiated");
  }
  //@TODO: do we need to sort objects by name? (faster to create within blender)

  FbxImportContext ctx(bmain, fbx, params);
  ctx.import_globals(scene);

#ifdef FBX_DEBUG_PRINT
  {
    fprintf(g_debug_file, "Initial NODE local matrices:\n");
    Vector<const ufbx_node *> nodes;
    for (const ufbx_node *node : ctx.fbx.nodes) {
      if (node->is_root) {
        continue;
      }
      nodes.append(node);
    }
    std::ranges::sort(nodes, [](const ufbx_node *a, const ufbx_node *b) {
      int ncmp = strcmp(a->name.data, b->name.data);
      if (ncmp != 0) {
        return ncmp < 0;
      }
      return a->attrib_type > b->attrib_type;
    });
    for (const ufbx_node *node : nodes) {
      ufbx_matrix mtx = ufbx_matrix_mul(node->node_depth < 2 ? &node->node_to_world :
                                                               &node->node_to_parent,
                                        &node->geometry_to_node);
      fprintf(g_debug_file, "init NODE %s self.matrix:\n", node->name.data);
      print_matrix(mtx);
    }
    fprintf(g_debug_file, "\n");
  }
#endif

  ctx.import_materials();
  ctx.import_armatures();
  ctx.import_meshes();
  ctx.import_cameras();
  ctx.import_lights();
  ctx.import_empties();
  ctx.import_animation(scene->frames_per_second());
  ctx.setup_hierarchy();

  ufbx_free_scene(fbx);

  /* Add objects to collection. */
  for (Object *obj : ctx.mapping.imported_objects) {
    BKE_collection_object_add(bmain, lc->collection, obj);
  }

  /* Select objects, sync layers etc. */
  BKE_view_layer_base_deselect_all(scene, view_layer);
  BKE_view_layer_synced_ensure(scene, view_layer);
  bool has_instantiated_object = false;
  bool has_uninstantiated_object = false;
  for (Object *obj : ctx.mapping.imported_objects) {
    Base *base = BKE_view_layer_base_find(view_layer, obj);
    if (!base) {
      /* Object not instantiated in current viewlayer. */
      has_uninstantiated_object = true;
      continue;
    }
    has_instantiated_object = true;
    BKE_view_layer_base_select_and_set_active(view_layer, base);

    int flags = ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION |
                ID_RECALC_BASE_FLAGS;
    DEG_id_tag_update_ex(bmain, &obj->id, flags);
  }

  if (has_instantiated_object && has_uninstantiated_object) {
    CLOG_ERROR(&LOG, "Some imported objects were not instantiated, while others were");
  }

  DEG_id_tag_update(&lc->collection->id, ID_RECALC_SYNC_TO_EVAL);

  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  DEG_relations_tag_update(bmain);
}

}  // namespace io::fbx
}  // namespace blender
