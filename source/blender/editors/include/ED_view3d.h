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
#ifndef ED_VIEW3D_H
#define ED_VIEW3D_H

/* ********* exports for space_view3d/ module ********** */
struct ARegion;
struct View3D;


float *give_cursor(Scene *scene, View3D *v3d);

void initgrabz(struct View3D *v3d, float x, float y, float z);
void window_to_3d(struct ARegion *ar, struct View3D *v3d, float *vec, short mx, short my);

/* Projection */

void project_short(struct ARegion *ar, struct View3D *v3d, float *vec, short *adr);
void project_short_noclip(struct ARegion *ar, struct View3D *v3d, float *vec, short *adr);

void project_int(struct ARegion *ar, struct View3D *v3d, float *vec, int *adr);
void project_int_noclip(struct ARegion *ar, struct View3D *v3d, float *vec, int *adr);

void project_float(struct ARegion *ar, struct View3D *v3d, float *vec, float *adr);
void project_float_noclip(struct ARegion *ar, struct View3D *v3d, float *vec, float *adr);

void viewline(struct ARegion *ar, struct View3D *v3d, short mval[2], float ray_start[3], float ray_end[3]);
void viewray(struct ARegion *ar, struct View3D *v3d, short mval[2], float ray_start[3], float ray_normal[3]);

#endif /* ED_VIEW3D_H */

