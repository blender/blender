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

#ifndef BIF_EDITVIEW_H
#define BIF_EDITVIEW_H

struct Base;
struct Object;
struct Camera;
struct View3D;

void	arrows_move_cursor(unsigned short event);
int		lasso_inside(short mcords[][2], short moves, short sx, short sy);
void	borderselect(void);
void	circle_select(void);
void	deselectall(void);
void	selectswap(void);
void	selectrandom(void);
void	selectall_type(short obtype);
void	selectall_layer(unsigned int layernum);
void	draw_sel_circle(short *mval, short *mvalo, float rad, float rado, int selecting);
void	fly(void);
int		gesture(void);
void	mouse_cursor(void);
void	mouse_select(void);
void	set_active_base(struct Base *base);
void	set_active_object(struct Object *ob);
void	set_render_border(void);
void	view3d_border_zoom(void);
void	view3d_edit_clipping(struct View3D *v3d);

#endif

