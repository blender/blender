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
struct MTexPoly;
struct Scene;
struct Image;
struct Object;
struct wmOperatorType;
struct BMEditMesh;
struct BMFace;
struct BMLoop;
struct BMEdge;
struct BMVert;

#define UV_SELECT_ALL       1
#define UV_SELECT_PINNED    2

/* id can be from 0 to 3 */
#define TF_PIN_MASK(id) (TF_PIN1 << id)
#define TF_SEL_MASK(id) (TF_SEL1 << id)

/* visibility and selection */
int uvedit_face_visible_nolocal(struct Scene *scene, struct BMFace *efa);

/*all the uvedit_xxxx_[de]selected functions are
   declared in ED_uvedit.h*/
int uvedit_face_select(struct Scene *scene, struct BMEditMesh *em, struct BMFace *efa);
int uvedit_face_deselect(struct Scene *scene, struct BMEditMesh *em, struct BMFace *efa);

void uvedit_edge_select(struct BMEditMesh *em, struct Scene *scene, struct BMLoop *l);
void uvedit_edge_deselect(struct BMEditMesh *em, struct Scene *scene, struct BMLoop *l);

void uvedit_uv_select(struct BMEditMesh *em, struct Scene *scene, struct BMLoop *l);
void uvedit_uv_deselect(struct BMEditMesh *em, struct Scene *scene, struct BMLoop *l);

/* geometric utilities */
void uv_center(float uv[][2], float cent[2], int quad);
float uv_area(float uv[][2], int quad);
void uv_copy_aspect(float uv_orig[][2], float uv[][2], float aspx, float aspy);

float poly_uv_area(float uv[][2], int len);
void poly_copy_aspect(float uv_orig[][2], float uv[][2], float aspx, float aspy, int len);
void poly_uv_center(struct BMEditMesh *em, struct BMFace *f, float cent[2]);

/* operators */
void UV_OT_average_islands_scale(struct wmOperatorType *ot);
void UV_OT_cube_project(struct wmOperatorType *ot);
void UV_OT_cylinder_project(struct wmOperatorType *ot);
void UV_OT_from_view(struct wmOperatorType *ot);
void UV_OT_mapping_menu(struct wmOperatorType *ot);
void UV_OT_minimize_stretch(struct wmOperatorType *ot);
void UV_OT_pack_islands(struct wmOperatorType *ot);
void UV_OT_reset(struct wmOperatorType *ot);
void UV_OT_sphere_project(struct wmOperatorType *ot);
void UV_OT_unwrap(struct wmOperatorType *ot);

#endif /* ED_UVEDIT_INTERN_H */

