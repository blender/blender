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

#ifndef BDR_DRAWOBJECT_H
#define BDR_DRAWOBJECT_H

#ifdef __cplusplus
extern "C" { 
#endif


struct Object;
struct Nurb;
struct Lamp;
struct ListBase;
struct BoundBox;
struct Base;

void init_draw_rects(void);
void helpline(float *vec);
void drawaxes(float size);
void drawcamera(struct Object *ob);
void calc_lattverts_ext(void);
void calc_meshverts(void);
void calc_meshverts_ext(void);
void calc_meshverts_ext_f2(void);
void calc_nurbverts_ext(void);
void tekenvertices(short sel);
void tekenvertices_ext(int mode);
void drawcircball(float *cent, float rad, float tmat[][4]);
void get_local_bounds(struct Object *ob, float *centre, float *size);
void draw_object(struct Base *base);
void draw_object_ext(struct Base *base);

#ifdef __cplusplus
}
#endif

#endif  /*  BDR_DRAWOBJECT_H */

