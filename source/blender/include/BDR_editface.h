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

#ifndef BDR_EDITFACE_H
#define BDR_EDITFACE_H

struct MTFace;
struct EditFace;
struct EditEdge;
struct Mesh;
struct MCol;

struct MTFace *get_active_mtface(struct EditFace **efa, struct MCol **mcol, int sloppy);
void calculate_uv_map(unsigned short mapmode);
void default_uv(float uv[][2], float size);
void make_tfaces(struct Mesh *me);
void reveal_tface(void);
void hide_tface(void);
void select_linked_tfaces(int mode);
void deselectall_tface(void);
void selectswap_tface(void);
void rotate_uv_tface(void);
void mirror_uv_tface(void);
int minmax_tface(float *min, float *max);
void face_select(void);
void face_borderselect(void);
void uv_autocalc_tface(void);
void set_texturepaint(void);
void get_same_uv(void);  
void seam_mark_clear_tface(short mode);
int edgetag_shortest_path(struct EditEdge *source, struct EditEdge *target);
void edgetag_context_set(struct EditEdge *eed, int val);
int edgetag_context_check(struct EditEdge *eed);
#endif /* BDR_EDITFACE_H */

