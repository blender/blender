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

#ifndef ED_UVEDIT_INTERN_H
#define ED_UVEDIT_INTERN_H

struct SpaceImage;
struct EditFace;
struct MTFace;
struct Scene;
struct Image;
struct Object;
struct wmOperatorType;

#define UV_SELECT_ALL       1
#define UV_SELECT_PINNED    2

/* id can be from 0 to 3 */
#define TF_PIN_MASK(id) (TF_PIN1 << id)
#define TF_SEL_MASK(id) (TF_SEL1 << id)

/* visibility and selection */
int uvedit_face_visible_nolocal(struct Scene *scene, struct EditFace *efa);
int uvedit_face_visible(struct Scene *scene, struct Image *ima, struct EditFace *efa, struct MTFace *tf);

int uvedit_face_selected(struct Scene *scene, struct EditFace *efa, struct MTFace *tf);
void uvedit_face_select(struct Scene *scene, struct EditFace *efa, struct MTFace *tf);
void uvedit_face_deselect(struct Scene *scene, struct EditFace *efa, struct MTFace *tf);

int uvedit_edge_selected(struct Scene *scene, struct EditFace *efa, struct MTFace *tf, int i);
void uvedit_edge_select(struct Scene *scene, struct EditFace *efa, struct MTFace *tf, int i);
void uvedit_edge_deselect(struct Scene *scene, struct EditFace *efa, struct MTFace *tf, int i);

int uvedit_uv_selected(struct Scene *scene, struct EditFace *efa, struct MTFace *tf, int i);
void uvedit_uv_select(struct Scene *scene, struct EditFace *efa, struct MTFace *tf, int i);
void uvedit_uv_deselect(struct Scene *scene, struct EditFace *efa, struct MTFace *tf, int i);

/* geometric utilities */
void uv_center(float uv[][2], float cent[2], int quad);
float uv_area(float uv[][2], int quad);
void uv_copy_aspect(float uv_orig[][2], float uv[][2], float aspx, float aspy);

/* operators */
void UV_OT_average_islands_scale(struct wmOperatorType *ot);
void UV_OT_cube_project(struct wmOperatorType *ot);
void UV_OT_cylinder_project(struct wmOperatorType *ot);
void UV_OT_from_view(struct wmOperatorType *ot);
void UV_OT_minimize_stretch(struct wmOperatorType *ot);
void UV_OT_pack_islands(struct wmOperatorType *ot);
void UV_OT_reset(struct wmOperatorType *ot);
void UV_OT_sphere_project(struct wmOperatorType *ot);
void UV_OT_unwrap(struct wmOperatorType *ot);

#endif /* ED_UVEDIT_INTERN_H */

