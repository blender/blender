/**
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BSE_DRAWVIEW_H
#define BSE_DRAWVIEW_H

struct Object;
struct BGpic;
struct rctf;
struct ScrArea;
struct ImBuf;

void circf(float x, float y, float rad);
void circ(float x, float y, float rad);

void do_viewbuts(unsigned short event);

/* View3DAfter->type */
#define V3D_XRAY	1
#define V3D_TRANSP	2
void add_view3d_after(struct View3D *v3d, struct Base *base, int type, int flag);

void backdrawview3d(int test);
void check_backbuf(void);
unsigned int sample_backbuf(int x, int y);
struct ImBuf *read_backbuf(short xmin, short ymin, short xmax, short ymax);
unsigned int sample_backbuf_rect(short mval[2], int size, unsigned int min, unsigned int max, int *dist, short strict, unsigned int (*indextest)(unsigned int index));

void drawview3dspace(struct ScrArea *sa, void *spacedata);
void drawview3d_render(struct View3D *v3d, float viewmat[][4], int winx, int winy, float winmat[][4], int shadow);
void draw_depth(struct ScrArea *sa, void *spacedata, int (*func)(void *) );
void view3d_update_depths(struct View3D *v3d);

int update_time(int cfra);
void calc_viewborder(struct View3D *v3d, struct rctf *viewborder_r);
void view3d_set_1_to_1_viewborder(struct View3D *v3d);

int view3d_test_clipping(struct View3D *v3d, float *vec);
void view3d_set_clipping(struct View3D *v3d);
void view3d_clr_clipping(void);

void sumo_callback(void *obp);
void init_anim_sumo(void);
void update_anim_sumo(void);
void end_anim_sumo(void);

void inner_play_anim_loop(int init, int mode);
int play_anim(int mode);

void make_axis_color(char *col, char *col2, char axis);
char *view3d_get_name(struct View3D *v3d);

/* SMOOTHVIEW */
void smooth_view(struct View3D *v3d, float *ofs, float *quat, float *dist, float *lens);
void smooth_view_to_camera(struct View3D *v3d);
void view_settings_from_ob(struct Object *ob, float *ofs, float *quat, float *dist, float *lens);
void object_view_settings(struct Object *ob, float *lens, float *clipsta, float *clipend);

#endif /* BSE_DRAWVIEW_H */

