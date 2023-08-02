/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edrend
 */

#pragma once

struct ScrArea;
struct bContext;
struct wmOperatorType;

/* `render_shading.cc` */

void OBJECT_OT_material_slot_add(wmOperatorType *ot);
void OBJECT_OT_material_slot_remove(wmOperatorType *ot);
void OBJECT_OT_material_slot_assign(wmOperatorType *ot);
void OBJECT_OT_material_slot_select(wmOperatorType *ot);
void OBJECT_OT_material_slot_deselect(wmOperatorType *ot);
void OBJECT_OT_material_slot_copy(wmOperatorType *ot);
void OBJECT_OT_material_slot_move(wmOperatorType *ot);
void OBJECT_OT_material_slot_remove_unused(wmOperatorType *ot);

void MATERIAL_OT_new(wmOperatorType *ot);
void TEXTURE_OT_new(wmOperatorType *ot);
void WORLD_OT_new(wmOperatorType *ot);

void MATERIAL_OT_copy(wmOperatorType *ot);
void MATERIAL_OT_paste(wmOperatorType *ot);

void SCENE_OT_view_layer_add(wmOperatorType *ot);
void SCENE_OT_view_layer_remove(wmOperatorType *ot);
void SCENE_OT_view_layer_add_aov(wmOperatorType *ot);
void SCENE_OT_view_layer_remove_aov(wmOperatorType *ot);
void SCENE_OT_view_layer_add_lightgroup(wmOperatorType *ot);
void SCENE_OT_view_layer_remove_lightgroup(wmOperatorType *ot);
void SCENE_OT_view_layer_add_used_lightgroups(wmOperatorType *ot);
void SCENE_OT_view_layer_remove_unused_lightgroups(wmOperatorType *ot);

void SCENE_OT_light_cache_bake(wmOperatorType *ot);
void SCENE_OT_light_cache_free(wmOperatorType *ot);

void OBJECT_OT_lightprobe_cache_bake(wmOperatorType *ot);
void OBJECT_OT_lightprobe_cache_free(wmOperatorType *ot);

void SCENE_OT_render_view_add(wmOperatorType *ot);
void SCENE_OT_render_view_remove(wmOperatorType *ot);

#ifdef WITH_FREESTYLE
void SCENE_OT_freestyle_module_add(wmOperatorType *ot);
void SCENE_OT_freestyle_module_remove(wmOperatorType *ot);
void SCENE_OT_freestyle_module_move(wmOperatorType *ot);
void SCENE_OT_freestyle_lineset_add(wmOperatorType *ot);
void SCENE_OT_freestyle_lineset_copy(wmOperatorType *ot);
void SCENE_OT_freestyle_lineset_paste(wmOperatorType *ot);
void SCENE_OT_freestyle_lineset_remove(wmOperatorType *ot);
void SCENE_OT_freestyle_lineset_move(wmOperatorType *ot);
void SCENE_OT_freestyle_linestyle_new(wmOperatorType *ot);
void SCENE_OT_freestyle_color_modifier_add(wmOperatorType *ot);
void SCENE_OT_freestyle_alpha_modifier_add(wmOperatorType *ot);
void SCENE_OT_freestyle_thickness_modifier_add(wmOperatorType *ot);
void SCENE_OT_freestyle_geometry_modifier_add(wmOperatorType *ot);
void SCENE_OT_freestyle_modifier_remove(wmOperatorType *ot);
void SCENE_OT_freestyle_modifier_move(wmOperatorType *ot);
void SCENE_OT_freestyle_modifier_copy(wmOperatorType *ot);
void SCENE_OT_freestyle_stroke_material_create(wmOperatorType *ot);
#endif

void TEXTURE_OT_slot_copy(wmOperatorType *ot);
void TEXTURE_OT_slot_paste(wmOperatorType *ot);
void TEXTURE_OT_slot_move(wmOperatorType *ot);

/* `render_internal.cc` */

/**
 * Contextual render, using current scene, view3d?
 */
void RENDER_OT_render(wmOperatorType *ot);
void RENDER_OT_shutter_curve_preset(wmOperatorType *ot);

/* `render_view.cc` */

/**
 * New window uses x,y to set position.
 */
ScrArea *render_view_open(bContext *C, int mx, int my, ReportList *reports);

void RENDER_OT_view_show(wmOperatorType *ot);
void RENDER_OT_view_cancel(wmOperatorType *ot);

/* `render_opengl.cc` */

void RENDER_OT_opengl(wmOperatorType *ot);
