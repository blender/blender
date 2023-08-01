/* SPDX-FileCopyrightText: 2009 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edrend
 */

#include <cstdlib>

#include "BLI_utildefines.h"

#include "ED_render.h"

#include "WM_api.h"

#include "render_intern.hh" /* own include */

/***************************** render ***********************************/

void ED_operatortypes_render()
{
  WM_operatortype_append(OBJECT_OT_material_slot_add);
  WM_operatortype_append(OBJECT_OT_material_slot_remove);
  WM_operatortype_append(OBJECT_OT_material_slot_assign);
  WM_operatortype_append(OBJECT_OT_material_slot_select);
  WM_operatortype_append(OBJECT_OT_material_slot_deselect);
  WM_operatortype_append(OBJECT_OT_material_slot_copy);
  WM_operatortype_append(OBJECT_OT_material_slot_move);
  WM_operatortype_append(OBJECT_OT_material_slot_remove_unused);

  WM_operatortype_append(OBJECT_OT_lightprobe_cache_bake);
  WM_operatortype_append(OBJECT_OT_lightprobe_cache_free);

  WM_operatortype_append(MATERIAL_OT_new);
  WM_operatortype_append(TEXTURE_OT_new);
  WM_operatortype_append(WORLD_OT_new);

  WM_operatortype_append(MATERIAL_OT_copy);
  WM_operatortype_append(MATERIAL_OT_paste);

  WM_operatortype_append(SCENE_OT_view_layer_add);
  WM_operatortype_append(SCENE_OT_view_layer_remove);
  WM_operatortype_append(SCENE_OT_view_layer_add_aov);
  WM_operatortype_append(SCENE_OT_view_layer_remove_aov);
  WM_operatortype_append(SCENE_OT_view_layer_add_lightgroup);
  WM_operatortype_append(SCENE_OT_view_layer_remove_lightgroup);
  WM_operatortype_append(SCENE_OT_view_layer_add_used_lightgroups);
  WM_operatortype_append(SCENE_OT_view_layer_remove_unused_lightgroups);

  WM_operatortype_append(SCENE_OT_render_view_add);
  WM_operatortype_append(SCENE_OT_render_view_remove);

  WM_operatortype_append(SCENE_OT_light_cache_bake);
  WM_operatortype_append(SCENE_OT_light_cache_free);

#ifdef WITH_FREESTYLE
  WM_operatortype_append(SCENE_OT_freestyle_module_add);
  WM_operatortype_append(SCENE_OT_freestyle_module_remove);
  WM_operatortype_append(SCENE_OT_freestyle_module_move);
  WM_operatortype_append(SCENE_OT_freestyle_lineset_add);
  WM_operatortype_append(SCENE_OT_freestyle_lineset_copy);
  WM_operatortype_append(SCENE_OT_freestyle_lineset_paste);
  WM_operatortype_append(SCENE_OT_freestyle_lineset_remove);
  WM_operatortype_append(SCENE_OT_freestyle_lineset_move);
  WM_operatortype_append(SCENE_OT_freestyle_linestyle_new);
  WM_operatortype_append(SCENE_OT_freestyle_color_modifier_add);
  WM_operatortype_append(SCENE_OT_freestyle_alpha_modifier_add);
  WM_operatortype_append(SCENE_OT_freestyle_thickness_modifier_add);
  WM_operatortype_append(SCENE_OT_freestyle_geometry_modifier_add);
  WM_operatortype_append(SCENE_OT_freestyle_modifier_remove);
  WM_operatortype_append(SCENE_OT_freestyle_modifier_move);
  WM_operatortype_append(SCENE_OT_freestyle_modifier_copy);
  WM_operatortype_append(SCENE_OT_freestyle_stroke_material_create);
#endif

  WM_operatortype_append(TEXTURE_OT_slot_copy);
  WM_operatortype_append(TEXTURE_OT_slot_paste);
  WM_operatortype_append(TEXTURE_OT_slot_move);

  /* `render_internal.cc` */
  WM_operatortype_append(RENDER_OT_view_show);
  WM_operatortype_append(RENDER_OT_render);
  WM_operatortype_append(RENDER_OT_view_cancel);
  WM_operatortype_append(RENDER_OT_shutter_curve_preset);

  /* `render_opengl.cc` */
  WM_operatortype_append(RENDER_OT_opengl);
}
