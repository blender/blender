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

#ifndef BDR_EDITFACE_H
#define BDR_EDITFACE_H

struct TFace;
struct Mesh;

void set_lasttface(void);
void calculate_uv_map(unsigned short mapmode);
void default_uv(float uv[][2], float size);
void default_tface(struct TFace *tface);
void make_tfaces(struct Mesh *me);
void reveal_tface(void);
void hide_tface(void);
void select_linked_tfaces(void);
void deselectall_tface(void);
void selectswap_tface(void);
void rotate_uv_tface(void);
int face_pick(struct Mesh *me, short x, short y);
void face_select(void);
void face_borderselect(void);
float CalcNormUV(float *a, float *b, float *c);
void uv_autocalc_tface(void);
void set_faceselect(void);
void face_draw(void);
void get_same_uv(void);  

#endif /* BDR_EDITFACE_H */

