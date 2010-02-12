/**
 * endian.h
 *
 * This is external code.
 *
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
 *  */

#ifndef AVI_ENDIAN_H
#define AVI_ENDIAN_H

#include <stdio.h>
#include "AVI_avi.h"

#define AVI_RAW      0
#define AVI_CHUNK    1
#define AVI_LIST     2
#define AVI_MAINH    3
#define AVI_STREAMH  4
#define AVI_BITMAPH  5
#define AVI_INDEXE   6
#define AVI_MJPEGU   7

void awrite (AviMovie *movie, void *datain, int block, int size, FILE *fp, int type);

#endif

