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

/* note; changing this also might affect the undo copy in editmesh.c */
typedef struct EditVert
{
	struct EditVert *next, *prev, *vn;
	float no[3];
	float co[3];
	float *ssco;  /* subsurfed coordinate, dont use for temporal storage! it points to DispListMesh */
	short xs, ys;
	unsigned char f, h, f1, f2;
	short fast;	/* only 0 or 1, for editmesh_fastmalloc */
	short	totweight;				/* __NLA */
	int hash;
	struct MDeformWeight *dw;	/* __NLA */
	int keyindex; /* original index #, for restoring  key information */
} EditVert;

struct EditEdge;

typedef struct HashEdge {
	struct EditEdge *eed;
	struct HashEdge *next;
} HashEdge;

/* note; changing this also might affect the undo copy in editmesh.c */
typedef struct EditEdge
{
	struct EditEdge *next, *prev;
	struct EditVert *v1, *v2, *vn;
	short f1, f2;	/* short, f1 is (ab)used in subdiv */
	unsigned char f, h, dir, seam;
	float crease;
	short fast; 		/* only 0 or 1, for editmesh_fastmalloc */
	short fgoni;		/* index for fgon, for search */
	HashEdge hash;
} EditEdge;

/* note; changing this also might affect the undo copy in editmesh.c */
typedef struct EditFace
{
	struct EditFace *next, *prev;
	struct EditVert *v1, *v2, *v3, *v4;
	struct EditEdge *e1, *e2, *e3, *e4;
	float n[3], cent[3];
	short xs, ys;		/* selection */
	struct TFace tf;	/* a copy of original tface. */
	unsigned char mat_nr, flag;
	unsigned char f, f1, h, puno;
	unsigned char fast;			/* only 0 or 1, for editmesh_fastmalloc */
	unsigned char fgonf;		/* flag for fgon options */
} EditFace;

typedef struct EditMesh
{
	ListBase verts, edges, faces;
	HashEdge *hashedgetab;
	
	/* this is for the editmesh_fastmalloc */
	EditVert *allverts, *curvert;
	EditEdge *alledges, *curedge;
	EditFace *allfaces, *curface;
	
} EditMesh;

#endif

