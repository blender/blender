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

/** \file
 * \ingroup collada
 */

#ifndef __EXPORTSETTINGS_H__
#define __EXPORTSETTINGS_H__

#ifdef __cplusplus
#  include <vector>

extern "C" {
#endif

#include "BLI_linklist.h"
#include "BlenderContext.h"

typedef enum BC_export_mesh_type {
  BC_MESH_TYPE_VIEW,
  BC_MESH_TYPE_RENDER,
} BC_export_mesh_type;

typedef enum BC_export_transformation_type {
  BC_TRANSFORMATION_TYPE_MATRIX,
  BC_TRANSFORMATION_TYPE_TRANSROTLOC,
} BC_export_transformation_type;

typedef enum BC_export_animation_type {
  BC_ANIMATION_EXPORT_SAMPLES,
  BC_ANIMATION_EXPORT_KEYS,
} BC_export_animation_type;

typedef enum BC_ui_export_section {
  BC_UI_SECTION_MAIN,
  BC_UI_SECTION_GEOMETRY,
  BC_UI_SECTION_ARMATURE,
  BC_UI_SECTION_ANIMATION,
  BC_UI_SECTION_COLLADA,
} BC_ui_export_section;

typedef struct ExportSettings {
  bool apply_modifiers;
  BC_global_forward_axis global_forward;
  BC_global_up_axis global_up;
  bool apply_global_orientation;

  BC_export_mesh_type export_mesh_type;

  bool selected;
  bool include_children;
  bool include_armatures;
  bool include_shapekeys;
  bool deform_bones_only;
  bool include_animations;
  bool include_all_actions;
  int sampling_rate;
  bool keep_smooth_curves;
  bool keep_keyframes;
  bool keep_flat_curves;

  bool active_uv_only;
  BC_export_animation_type export_animation_type;
  bool use_texture_copies;

  bool triangulate;
  bool use_object_instantiation;
  bool use_blender_profile;
  bool sort_by_name;
  BC_export_transformation_type object_transformation_type;
  BC_export_transformation_type animation_transformation_type;

  bool open_sim;
  bool limit_precision;
  bool keep_bind_info;

  char *filepath;
  LinkNode *export_set;
} ExportSettings;

#ifdef __cplusplus
}

void bc_get_children(std::vector<Object *> &child_set, Object *ob, ViewLayer *view_layer);

class BCExportSettings {

 private:
  const ExportSettings &export_settings;
  BlenderContext &blender_context;
  const BCMatrix global_transform;

 public:
  BCExportSettings(ExportSettings *exportSettings, BlenderContext &blenderContext)
      : export_settings(*exportSettings),
        blender_context(blenderContext),
        global_transform(BCMatrix(exportSettings->global_forward, exportSettings->global_up))

  {
  }

  const BCMatrix &get_global_transform()
  {
    return global_transform;
  }

  bool get_apply_modifiers()
  {
    return export_settings.apply_modifiers;
  }

  BC_global_forward_axis get_global_forward()
  {
    return export_settings.global_forward;
  }

  BC_global_up_axis get_global_up()
  {
    return export_settings.global_up;
  }

  bool get_apply_global_orientation()
  {
    return export_settings.apply_global_orientation;
  }

  BC_export_mesh_type get_export_mesh_type()
  {
    return export_settings.export_mesh_type;
  }

  bool get_selected()
  {
    return export_settings.selected;
  }

  bool get_include_children()
  {
    return export_settings.include_children;
  }

  bool get_include_armatures()
  {
    return export_settings.include_armatures;
  }

  bool get_include_shapekeys()
  {
    return export_settings.include_shapekeys;
  }

  bool get_deform_bones_only()
  {
    return export_settings.deform_bones_only;
  }

  bool get_include_animations()
  {
    return export_settings.include_animations;
  }

  bool get_include_all_actions()
  {
    return export_settings.include_all_actions;
  }

  int get_sampling_rate()
  {
    return export_settings.sampling_rate;
  }

  bool get_keep_smooth_curves()
  {
    return export_settings.keep_smooth_curves;
  }

  bool get_keep_keyframes()
  {
    return export_settings.keep_keyframes;
  }

  bool get_keep_flat_curves()
  {
    return export_settings.keep_flat_curves;
  }

  bool get_active_uv_only()
  {
    return export_settings.active_uv_only;
  }

  BC_export_animation_type get_export_animation_type()
  {
    return export_settings.export_animation_type;
  }

  bool get_use_texture_copies()
  {
    return export_settings.use_texture_copies;
  }

  bool get_triangulate()
  {
    return export_settings.triangulate;
  }

  bool get_use_object_instantiation()
  {
    return export_settings.use_object_instantiation;
  }

  bool get_use_blender_profile()
  {
    return export_settings.use_blender_profile;
  }

  bool get_sort_by_name()
  {
    return export_settings.sort_by_name;
  }

  BC_export_transformation_type get_object_transformation_type()
  {
    return export_settings.object_transformation_type;
  }

  BC_export_transformation_type get_animation_transformation_type()
  {
    return export_settings.animation_transformation_type;
  }

  bool get_open_sim()
  {
    return export_settings.open_sim;
  }

  bool get_limit_precision()
  {
    return export_settings.limit_precision;
  }

  bool get_keep_bind_info()
  {
    return export_settings.keep_bind_info;
  }

  char *get_filepath()
  {
    return export_settings.filepath;
  }

  LinkNode *get_export_set()
  {
    return export_settings.export_set;
  }

  BlenderContext &get_blender_context()
  {
    return blender_context;
  }

  Scene *get_scene()
  {
    return blender_context.get_scene();
  }

  ViewLayer *get_view_layer()
  {
    return blender_context.get_view_layer();
  }

  bool is_export_root(Object *ob)
  {
    return bc_is_base_node(get_export_set(), ob, get_view_layer());
  }
};

#endif

#endif
