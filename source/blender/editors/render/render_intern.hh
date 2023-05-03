/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation */

/** \file
 * \ingroup edrend
 */

#pragma once

struct ScrArea;
struct bContext;
struct wmOperatorType;

/* render_shading.c */

void OBJECT_OT_material_slot_add(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_remove(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_assign(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_select(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_deselect(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_copy(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_move(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_remove_unused(struct wmOperatorType *ot);

void MATERIAL_OT_new(struct wmOperatorType *ot);
void TEXTURE_OT_new(struct wmOperatorType *ot);
void WORLD_OT_new(struct wmOperatorType *ot);

void MATERIAL_OT_copy(struct wmOperatorType *ot);
void MATERIAL_OT_paste(struct wmOperatorType *ot);

void SCENE_OT_view_layer_add(struct wmOperatorType *ot);
void SCENE_OT_view_layer_remove(struct wmOperatorType *ot);
void SCENE_OT_view_layer_add_aov(struct wmOperatorType *ot);
void SCENE_OT_view_layer_remove_aov(struct wmOperatorType *ot);
void SCENE_OT_view_layer_add_lightgroup(struct wmOperatorType *ot);
void SCENE_OT_view_layer_remove_lightgroup(struct wmOperatorType *ot);
void SCENE_OT_view_layer_add_used_lightgroups(struct wmOperatorType *ot);
void SCENE_OT_view_layer_remove_unused_lightgroups(struct wmOperatorType *ot);

void SCENE_OT_light_cache_bake(struct wmOperatorType *ot);
void SCENE_OT_light_cache_free(struct wmOperatorType *ot);

void OBJECT_OT_lightprobe_cache_bake(struct wmOperatorType *ot);
void OBJECT_OT_lightprobe_cache_free(struct wmOperatorType *ot);

void SCENE_OT_render_view_add(struct wmOperatorType *ot);
void SCENE_OT_render_view_remove(struct wmOperatorType *ot);

#ifdef WITH_FREESTYLE
void SCENE_OT_freestyle_module_add(struct wmOperatorType *ot);
void SCENE_OT_freestyle_module_remove(struct wmOperatorType *ot);
void SCENE_OT_freestyle_module_move(struct wmOperatorType *ot);
void SCENE_OT_freestyle_lineset_add(struct wmOperatorType *ot);
void SCENE_OT_freestyle_lineset_copy(struct wmOperatorType *ot);
void SCENE_OT_freestyle_lineset_paste(struct wmOperatorType *ot);
void SCENE_OT_freestyle_lineset_remove(struct wmOperatorType *ot);
void SCENE_OT_freestyle_lineset_move(struct wmOperatorType *ot);
void SCENE_OT_freestyle_linestyle_new(struct wmOperatorType *ot);
void SCENE_OT_freestyle_color_modifier_add(struct wmOperatorType *ot);
void SCENE_OT_freestyle_alpha_modifier_add(struct wmOperatorType *ot);
void SCENE_OT_freestyle_thickness_modifier_add(struct wmOperatorType *ot);
void SCENE_OT_freestyle_geometry_modifier_add(struct wmOperatorType *ot);
void SCENE_OT_freestyle_modifier_remove(struct wmOperatorType *ot);
void SCENE_OT_freestyle_modifier_move(struct wmOperatorType *ot);
void SCENE_OT_freestyle_modifier_copy(struct wmOperatorType *ot);
void SCENE_OT_freestyle_stroke_material_create(struct wmOperatorType *ot);
#endif

void TEXTURE_OT_slot_copy(struct wmOperatorType *ot);
void TEXTURE_OT_slot_paste(struct wmOperatorType *ot);
void TEXTURE_OT_slot_move(struct wmOperatorType *ot);

/* render_internal.c */

/**
 * Contextual render, using current scene, view3d?
 */
void RENDER_OT_render(struct wmOperatorType *ot);
void RENDER_OT_shutter_curve_preset(struct wmOperatorType *ot);

/* render_view.c */

/**
 * New window uses x,y to set position.
 */
struct ScrArea *render_view_open(struct bContext *C, int mx, int my, struct ReportList *reports);

void RENDER_OT_view_show(struct wmOperatorType *ot);
void RENDER_OT_view_cancel(struct wmOperatorType *ot);

/* render_opengl.c */

void RENDER_OT_opengl(struct wmOperatorType *ot);
