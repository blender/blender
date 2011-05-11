/**
 * $Id$
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

int		min_i				(int a, int b);

int		max_i				(int a, int b);
int		clamp_i				(int val, int min, int max); 

float	min_f				(float a, float b); 
float	max_f				(float a, float b); 
float	clamp_f				(float val, float min, float max); 

void	rect_copy			(int dst[2][2], int src[2][2]);
int		rect_contains_pt	(int rect[2][2], int pt[2]);
int		rect_width			(int rect[2][2]);
int		rect_height			(int rect[2][2]);

