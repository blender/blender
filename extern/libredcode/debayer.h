/* ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2008 Peter Schlaile
 *
 * This file is part of libredcode.
 *
 * Libredcode is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Libredcode is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Libredcode; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****/

#ifndef __DEBAYER_H__
#define __DEBAYER_H__

void redcode_ycbcr2rgb_fullscale(
	int ** planes, int width, int height, float * out);
void redcode_ycbcr2rgb_halfscale(
	int ** planes, int width, int height, float * out);
void redcode_ycbcr2rgb_quarterscale(
	int ** planes, int width, int height, float * out);

#endif
