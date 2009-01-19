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

struct Scene;
struct Object;
struct Image;
struct wmWindowManager;

/* uvedit_ops.c */
void ED_operatortypes_uvedit(void);
void ED_keymap_uvedit(struct wmWindowManager *wm);

void ED_uvedit_assign_image(struct Scene *scene, struct Object *obedit, struct Image *ima, struct Image *previma);
void ED_uvedit_set_tile(struct Scene *scene, struct Object *obedit, struct Image *ima, int curtile, int dotile);
int ED_uvedit_minmax(struct Scene *scene, struct Image *ima, struct Object *obedit, float *min, float *max);

#endif /* ED_UVEDIT_H */

