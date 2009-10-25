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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef ED_UVEDIT_H
#define ED_UVEDIT_H

struct bContext;
struct Scene;
struct Object;
struct MTFace;
struct EditFace;
struct Image;
struct wmKeyConfig;

/* uvedit_ops.c */
void ED_operatortypes_uvedit(void);
void ED_keymap_uvedit(struct wmKeyConfig *keyconf);

void ED_uvedit_assign_image(struct Scene *scene, struct Object *obedit, struct Image *ima, struct Image *previma);
void ED_uvedit_set_tile(struct bContext *C, struct Scene *scene, struct Object *obedit, struct Image *ima, int curtile, int dotile);
int ED_uvedit_minmax(struct Scene *scene, struct Image *ima, struct Object *obedit, float *min, float *max);

int ED_uvedit_test_silent(struct Object *obedit);
int ED_uvedit_test(struct Object *obedit);

int uvedit_face_visible(struct Scene *scene, struct Image *ima, struct EditFace *efa, struct MTFace *tf);
int uvedit_face_selected(struct Scene *scene, struct EditFace *efa, struct MTFace *tf);
int uvedit_edge_selected(struct Scene *scene, struct EditFace *efa, struct MTFace *tf, int i);
int uvedit_uv_selected(struct Scene *scene, struct EditFace *efa, struct MTFace *tf, int i);

int ED_uvedit_nearest_uv(struct Scene *scene, struct Object *obedit, struct Image *ima, float co[2], float uv[2]);

/* uvedit_unwrap.c */
void ED_uvedit_live_unwrap_begin(struct Scene *scene, struct Object *obedit);
void ED_uvedit_live_unwrap_re_solve(void);
void ED_uvedit_live_unwrap_end(short cancel);

#endif /* ED_UVEDIT_H */

