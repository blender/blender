/**
 * blenlib/BLI_editVert.h    mar 2001 Nzc
 *
 * Some editing types needed in the lib (unfortunately) for
 * scanfill.c
 *
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

#ifndef BLI_EDITVERT_H
#define BLI_EDITVERT_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct EditVert
{
	struct EditVert *next, *prev, *vn;
	float no[3];
	float co[3];
	short xs, ys;
	char f, h, f1, hash;
	int	totweight;				/* __NLA */
	struct MDeformWeight *dw;	/* __NLA */
} EditVert;

typedef struct EditEdge
{
	struct EditEdge *next, *prev;
	struct EditVert *v1, *v2, *vn;
	short f,h;
	short f1, dir;
} EditEdge;

typedef struct EditVlak
{
	struct EditVlak *next, *prev;
	struct EditVert *v1, *v2, *v3, *v4;
	struct EditEdge *e1, *e2, *e3, *e4;
	float n[3];
	float uv[4][2];
	unsigned int col[4];
	struct TFace *tface;	/* a pointer to original tface. */
	char mat_nr, flag;
	char f, f1;
} EditVlak;

#endif

