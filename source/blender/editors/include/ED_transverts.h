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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_transverts.h
 *  \ingroup editors
 */

#ifndef __ED_TRANSVERTS_H__
#define __ED_TRANSVERTS_H__

struct Object;

typedef struct TransVert {
	float *loc;
	float oldloc[3], maploc[3];
	float *val, oldval;
	int flag;
} TransVert;

typedef struct TransVertStore {
	struct TransVert *transverts;
	int transverts_tot;
} TransVertStore;

void ED_transverts_create_from_obedit(TransVertStore *tvs, struct Object *obedit, const int mode);
void ED_transverts_update_obedit(TransVertStore *tvs, struct Object *obedit);
void ED_transverts_free(TransVertStore *tvs);
bool ED_transverts_check_obedit(Object *obedit);

/* currently only used for bmesh index values */
enum {
	TM_INDEX_ON      =  1,  /* tag to make trans verts */
	TM_INDEX_OFF     =  0,  /* don't make verts */
	TM_INDEX_SKIP    = -1   /* dont make verts (when the index values point to trans-verts) */
};

/* mode flags: */
enum {
	TM_ALL_JOINTS      = 1, /* all joints (for bones only) */
	TM_SKIP_HANDLES    = 2  /* skip handles when control point is selected (for curves only) */
};


              /* SELECT == (1 << 0) */
#define TX_VERT_USE_MAPLOC (1 << 1)

#endif  /* __ED_TRANSVERTS_H__ */
