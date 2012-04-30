/*
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

/**
 * \file IMB_filter.h
 * \ingroup imbuf
 * \brief Function declarations for filter.c
 */

#ifndef __IMB_FILTER_H__
#define __IMB_FILTER_H__

struct ImBuf;

void imb_filterx(struct ImBuf *ibuf);

void IMB_premultiply_rect(unsigned int *rect, char planes, int w, int h);
void IMB_premultiply_rect_float(float *rect_float, char planes, int w, int h);

void imb_onehalf_no_alloc(struct ImBuf *ibuf2, struct ImBuf *ibuf1);

#endif

