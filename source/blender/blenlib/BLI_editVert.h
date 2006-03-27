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

struct DerivedMesh;

/* note; changing this also might affect the undo copy in editmesh.c */
typedef struct EditVert
{
	struct EditVert *next, *prev;
	union {
		/* some lean storage for temporary usage 
		 * in editmesh routines
		 */
		struct EditVert *v;
		struct EditEdge *e;
		struct EditFace *f;
		float           *fp;
		void            *p;
		long             l;
	} tmp;
	float no[3]; /*vertex normal */
	float co[3]; /*vertex location */
	short xs, ys; /* used to store a screenspace 2d projection of the verts */
	
	/* f stores selection eg. if (eve->f & SELECT) {...
	h for hidden. if (!eve->h) {...
	f1 and f2 can be used for temp data, clear them first*/
	unsigned char f, h, f1, f2; 
	short fast;	/* only 0 or 1, for editmesh_fastmalloc, do not store temp data here! */
	short	totweight; /* __NLA total number of vertex weights for this vertex */
	int hash;
	struct MDeformWeight *dw;	/* __NLA a pointer to an array of defirm weights */
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
	struct EditVert *v1, *v2;
	union {
		/* some lean storage for temporary usage 
		 * in editmesh routines
		 */
		struct EditVert *v;
		struct EditEdge *e;
		struct EditFace *f;
		void            *p;
		long             l;
		float			fp;
	} tmp;
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
	union {
		/* some lean storage for temporary usage 
		 * in editmesh routines
		 */
		struct EditVert *v;
		struct EditEdge *e;
		struct EditFace *f;
		void            *p;
		long             l;
		float			fp;
	} tmp;
	float n[3], cent[3];
	struct TFace tf;	/* a copy of original tface. */
	unsigned char mat_nr, flag;
	unsigned char f, f1, h;
	unsigned char fast;			/* only 0 or 1, for editmesh_fastmalloc */
	unsigned char fgonf;		/* flag for fgon options */
} EditFace;


/*selection types*/
#define EDITVERT 0
#define EDITEDGE 1
#define EDITFACE 2

typedef struct EditSelection
{
	struct EditSelection *next, *prev;
	short type;
	void *data;
} EditSelection;


typedef struct EditMesh
{
	ListBase verts, edges, faces;
	ListBase selected; /*EditSelections. Used to store the order in which things are selected.*/
	HashEdge *hashedgetab;
	
	/* this is for the editmesh_fastmalloc */
	EditVert *allverts, *curvert;
	EditEdge *alledges, *curedge;
	EditFace *allfaces, *curface;
		/* DerivedMesh caches... note that derived cage can be equivalent
		 * to derived final, care should be taken on release.
		 */
	struct DerivedMesh *derivedCage, *derivedFinal;
} EditMesh;

#endif

