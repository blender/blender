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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_uvedit.h
 *  \ingroup editors
 */

#ifndef __ED_UVEDIT_H__
#define __ED_UVEDIT_H__

struct ARegionType;
struct BMEditMesh;
struct BMFace;
struct BMLoop;
struct Image;
struct ImageUser;
struct MTFace;
struct MTexPoly;
struct Main;
struct Object;
struct Scene;
struct SpaceImage;
struct bContext;
struct bNode;
struct wmKeyConfig;

/* uvedit_ops.c */
void ED_operatortypes_uvedit(void);
void ED_keymap_uvedit(struct wmKeyConfig *keyconf);

void ED_uvedit_assign_image(struct Main *bmain, struct Scene *scene, struct Object *obedit, struct Image *ima, struct Image *previma);
int  ED_uvedit_minmax(struct Scene *scene, struct Image *ima, struct Object *obedit, float *min, float *max);

int  ED_object_get_active_image(struct Object *ob, int mat_nr, struct Image **ima, struct ImageUser **iuser, struct bNode **node);
void ED_object_assign_active_image(struct Main *bmain, struct Object *ob, int mat_nr, struct Image *ima);

int ED_uvedit_test(struct Object *obedit);

/* visibility and selection */
int  uvedit_face_visible_test(struct Scene *scene, struct Image *ima, struct BMFace *efa, struct MTexPoly *tf);
int  uvedit_face_select_test(struct Scene *scene, struct BMEditMesh *em, struct BMFace *efa);
int  uvedit_edge_select_test(struct BMEditMesh *em, struct Scene *scene, struct BMLoop *l);
int  uvedit_uv_select_test(struct BMEditMesh *em, struct Scene *scene, struct BMLoop *l);

int  uvedit_face_select_enable(struct Scene *scene, struct BMEditMesh *em, struct BMFace *efa, const short do_history);
int  uvedit_face_select_disable(struct Scene *scene, struct BMEditMesh *em, struct BMFace *efa);
void uvedit_edge_select_enable(struct BMEditMesh *em, struct Scene *scene, struct BMLoop *l, const short do_history);
void uvedit_edge_select_disable(struct BMEditMesh *em, struct Scene *scene, struct BMLoop *l);
void uvedit_uv_select_enable(struct BMEditMesh *em, struct Scene *scene, struct BMLoop *l, const short do_history);
void uvedit_uv_select_disable(struct BMEditMesh *em, struct Scene *scene, struct BMLoop *l);

int ED_uvedit_nearest_uv(struct Scene *scene, struct Object *obedit, struct Image *ima, float co[2], float uv[2]);

/* uvedit_unwrap_ops.c */
void ED_uvedit_live_unwrap_begin(struct Scene *scene, struct Object *obedit);
void ED_uvedit_live_unwrap_re_solve(void);
void ED_uvedit_live_unwrap_end(short cancel);

void ED_uvedit_live_unwrap(struct Scene *scene, struct Object *obedit);

/* single call up unwrap using scene settings, used for edge tag unwrapping */
void ED_unwrap_lscm(struct Scene *scene, struct Object *obedit, const short sel);

/* uvedit_draw.c */
void draw_uvedit_main(struct SpaceImage *sima, struct ARegion *ar, struct Scene *scene, struct Object *obedit, struct Object *obact);

/* uvedit_buttons.c */
void ED_uvedit_buttons_register(struct ARegionType *art);

#endif /* __ED_UVEDIT_H__ */

