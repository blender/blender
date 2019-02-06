/*
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
 */

/** \file \ingroup DNA
 */

#ifndef __DNA_LATTICE_TYPES_H__
#define __DNA_LATTICE_TYPES_H__

#include "DNA_defs.h"
#include "DNA_ID.h"

struct AnimData;
struct BPoint;
struct Ipo;
struct Key;
struct MDeformVert;

typedef struct EditLatt {
	struct Lattice *latt;

	int shapenr;

	char pad[4];
} EditLatt;

typedef struct Lattice {
	ID id;
	struct AnimData *adt;

	short pntsu, pntsv, pntsw, flag;
	short opntsu, opntsv, opntsw, pad2;
	char typeu, typev, typew, pad3;
	/** Active element index, unset with LT_ACTBP_NONE. */
	int actbp;

	float fu, fv, fw, du, dv, dw;

	struct BPoint *def;

	/** Old animation system, deprecated for 2.5. */
	struct Ipo *ipo  DNA_DEPRECATED;
	struct Key *key;

	struct MDeformVert *dvert;
	/** Multiply the influence, MAX_VGROUP_NAME. */
	char vgroup[64];

	struct EditLatt *editlatt;
	void *batch_cache;
} Lattice;

/* ***************** LATTICE ********************* */

/* flag */
#define LT_GRID		1
#define LT_OUTSIDE	2

#define LT_DS_EXPAND	4

#define LT_ACTBP_NONE	-1

#endif
