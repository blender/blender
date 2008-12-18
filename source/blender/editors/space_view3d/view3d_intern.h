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
#ifndef ED_VIEW3D_INTERN_H
#define ED_VIEW3D_INTERN_H

/* internal exports only */

typedef struct ViewDepths {
	unsigned short w, h;
	float *depths;
	double depth_range[2];
	
	char damaged;
} ViewDepths;

/* drawing flags: */
#define DRAW_PICKING	1
#define DRAW_CONSTCOLOR	2
#define DRAW_SCENESET	4

/* project short */
#define IS_CLIPPED        12000

/* view3d_header.c */
void view3d_header_buttons(const bContext *C, ARegion *ar);

/* view3d_draw.c */
void drawview3dspace(const bContext *C, ARegion *ar, View3D *v3d);
int view3d_test_clipping(View3D *v3d, float *vec);
void circf(float x, float y, float rad);
void circ(float x, float y, float rad);
void view3d_update_depths(ARegion *ar, View3D *v3d);

/* view3d_view.c */
float *give_cursor(Scene *scene, View3D *v3d);
void initgrabz(View3D *v3d, float x, float y, float z);
void window_to_3d(ARegion *ar, View3D *v3d, float *vec, short mx, short my);
void view3d_project_float(ARegion *a, float *vec, float *adr, float mat[4][4]);
void project_short(ARegion *ar, View3D *v3d, float *vec, short *adr);
void project_int(ARegion *ar, View3D *v3d, float *vec, int *adr);
void project_int_noclip(ARegion *ar, View3D *v3d, float *vec, int *adr);
void project_short_noclip(ARegion *ar, View3D *v3d, float *vec, short *adr);
void project_float(ARegion *ar, View3D *v3d, float *vec, float *adr);
void project_float_noclip(ARegion *ar, View3D *v3d, float *vec, float *adr);
int get_view3d_viewplane(View3D *v3d, int winxi, int winyi, rctf *viewplane, float *clipsta, float *clipend, float *pixsize);


void setwinmatrixview3d(wmWindow *win, View3D *v3d, int winx, int winy, rctf *rect);	/* rect: for picking */
void setviewmatrixview3d(View3D *v3d);

#endif /* ED_VIEW3D_INTERN_H */

