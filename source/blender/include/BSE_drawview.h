/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef BSE_DRAWVIEW_H
#define BSE_DRAWVIEW_H

struct Object;
struct BGpic;
struct rcti;
struct ScrArea;

void setalpha_bgpic(struct BGpic *bgpic);
void default_gl_light(void);
void init_gl_stuff(void);
void two_sided(int val);
void circf(float x, float y, float rad);
void circ(float x, float y, float rad);
void backdrawview3d(int test);

void do_viewbuts(unsigned short event);
void drawview3dspace(struct ScrArea *sa, void *spacedata);
void drawview3d_render(struct View3D *v3d);

int update_time(void);
void calc_viewborder(struct View3D *v3d, struct rcti *viewborder_r);
void view3d_set_1_to_1_viewborder(struct View3D *v3d);
void timestr(double time, char *str);
double speed_to_swaptime(int speed);
double key_to_swaptime(int key);

void sumo_callback(void *obp);
void init_anim_sumo(void);
void update_anim_sumo(void);
void end_anim_sumo(void);

void inner_play_anim_loop(int init, int mode);
int play_anim(int mode);

#endif /* BSE_DRAWVIEW_H */

