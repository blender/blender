/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/render/render_intern.h
 *  \ingroup edrend
 */


#ifndef __RENDER_INTERN_H__
#define __RENDER_INTERN_H__

struct wmOperatorType;
struct RenderResult;
struct Scene;

/* render_shading.c */
void OBJECT_OT_material_slot_add(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_remove(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_assign(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_select(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_deselect(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_copy(struct wmOperatorType *ot);

void MATERIAL_OT_new(struct wmOperatorType *ot);
void TEXTURE_OT_new(struct wmOperatorType *ot);
void WORLD_OT_new(struct wmOperatorType *ot);

void MATERIAL_OT_copy(struct wmOperatorType *ot);
void MATERIAL_OT_paste(struct wmOperatorType *ot);

void SCENE_OT_render_layer_add(struct wmOperatorType *ot);
void SCENE_OT_render_layer_remove(struct wmOperatorType *ot);


void TEXTURE_OT_slot_copy(struct wmOperatorType *ot);
void TEXTURE_OT_slot_paste(struct wmOperatorType *ot);
void TEXTURE_OT_slot_move(struct wmOperatorType *ot);
void TEXTURE_OT_envmap_save(struct wmOperatorType *ot);
void TEXTURE_OT_envmap_clear(struct wmOperatorType *ot);
void TEXTURE_OT_envmap_clear_all(struct wmOperatorType *ot);

/* render_internal.c */
void RENDER_OT_render(struct wmOperatorType *ot);

/* render_opengl.c uses this */
void image_buffer_rect_update(struct Scene *scene, struct RenderResult *rr, struct ImBuf *ibuf, volatile struct rcti *renrect);

/* render_view.c */
void render_view_open(struct bContext *C, int mx, int my);

void RENDER_OT_view_show(struct wmOperatorType *ot);
void RENDER_OT_view_cancel(struct wmOperatorType *ot);

/* render_opengl.c */
void RENDER_OT_opengl(struct wmOperatorType *ot);

#endif /* __RENDER_INTERN_H__ */

