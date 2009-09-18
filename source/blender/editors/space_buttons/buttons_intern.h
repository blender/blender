/**
 * $Id:
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_BUTTONS_INTERN_H
#define ED_BUTTONS_INTERN_H

struct ARegion;
struct ARegionType;
struct bContext;
struct bContextDataResult;
struct SpaceButs;
struct uiLayout;
struct wmOperatorType;

/* buts->scaflag */		
#define BUTS_SENS_SEL		1
#define BUTS_SENS_ACT		2
#define BUTS_SENS_LINK		4
#define BUTS_CONT_SEL		8
#define BUTS_CONT_ACT		16
#define BUTS_CONT_LINK		32
#define BUTS_ACT_SEL		64
#define BUTS_ACT_ACT		128
#define BUTS_ACT_LINK		256
#define BUTS_SENS_STATE		512
#define BUTS_ACT_STATE		1024

#define BUTS_HEADERY		(HEADERY*1.2)
#define BUTS_UI_UNIT		(UI_UNIT_Y*1.2)

/* internal exports only */

/* buttons_header.c */
void buttons_header_buttons(const struct bContext *C, struct ARegion *ar);

/* buttons_context.c */
void buttons_context_compute(const struct bContext *C, struct SpaceButs *sbuts);
int buttons_context(const struct bContext *C, const char *member, struct bContextDataResult *result);
void buttons_context_draw(const struct bContext *C, struct uiLayout *layout);
void buttons_context_register(struct ARegionType *art);

/* buttons_ops.c */
void OBJECT_OT_group_add(struct wmOperatorType *ot);
void OBJECT_OT_group_remove(struct wmOperatorType *ot);

void OBJECT_OT_material_slot_add(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_remove(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_assign(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_select(struct wmOperatorType *ot);
void OBJECT_OT_material_slot_deselect(struct wmOperatorType *ot);

void MATERIAL_OT_new(struct wmOperatorType *ot);
void TEXTURE_OT_new(struct wmOperatorType *ot);
void WORLD_OT_new(struct wmOperatorType *ot);

void OBJECT_OT_particle_system_add(struct wmOperatorType *ot);
void OBJECT_OT_particle_system_remove(struct wmOperatorType *ot);

void PARTICLE_OT_new(struct wmOperatorType *ot);
void PARTICLE_OT_new_target(struct wmOperatorType *ot);
void PARTICLE_OT_remove_target(struct wmOperatorType *ot);
void PARTICLE_OT_target_move_up(struct wmOperatorType *ot);
void PARTICLE_OT_target_move_down(struct wmOperatorType *ot);
void PARTICLE_OT_connect_hair(struct wmOperatorType *ot);
void PARTICLE_OT_disconnect_hair(struct wmOperatorType *ot);

void SCENE_OT_render_layer_add(struct wmOperatorType *ot);
void SCENE_OT_render_layer_remove(struct wmOperatorType *ot);

void BUTTONS_OT_file_browse(struct wmOperatorType *ot);
void BUTTONS_OT_toolbox(struct wmOperatorType *ot);

#endif /* ED_BUTTONS_INTERN_H */

