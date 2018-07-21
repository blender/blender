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

/** \file DNA_world_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_WORLD_TYPES_H__
#define __DNA_WORLD_TYPES_H__

#include "DNA_defs.h"
#include "DNA_ID.h"

struct AnimData;
struct bNodeTree;
struct Ipo;
struct MTex;

#ifndef MAX_MTEX
#define MAX_MTEX	18
#endif


/**
 * World defines general modeling data such as a background fill,
 * gravity, color model etc. It mixes rendering data and modeling data. */
typedef struct World {
	ID id;
	struct AnimData *adt;	/* animation data (must be immediately after id for utilities to use it) */
	DrawDataList drawdata; /* runtime (must be immediately after id for utilities to use it). */

	char _pad0[4];
	short texact, mistype;

	float horr, horg, horb;

	/**
	 * Exposure= mult factor. unused now, but maybe back later. Kept in to be upward compat.
	 * New is exp/range control. linfac & logfac are constants... don't belong in
	 * file, but allocating 8 bytes for temp mem isn't useful either.
	 */
	float exposure, exp, range;
	float linfac, logfac;

	/**
	 * Some world modes
	 * bit 0: Do mist
	 */
	short mode;												// partially moved to scene->gamedata in 2.5
	short pad2[3];

	float misi, miststa, mistdist, misthi;

	/* ambient occlusion */
	float aodist, aoenergy;

	/* assorted settings  */
	short flag, pad3[3];

	struct Ipo *ipo  DNA_DEPRECATED;  /* old animation system, deprecated for 2.5 */
	short pr_texture, use_nodes, pad[2];

	/* previews */
	struct PreviewImage *preview;

	/* nodes */
	struct bNodeTree *nodetree;

	float mistend, pad1;        /* runtime : miststa + mistdist, used for drawing camera */
	ListBase gpumaterial;		/* runtime */
} World;

/* **************** WORLD ********************* */

/* mode */
#define WO_MIST	               1
//#define WO_STARS               2 /* deprecated */
/*#define WO_DOF                 4*/
//#define WO_ACTIVITY_CULLING	   8 /* deprecated */
//#define WO_ENV_LIGHT   		  16
//#define WO_DBVT_CULLING		  32 /* deprecated */
#define WO_AMB_OCC   		  64
//#define WO_INDIRECT_LIGHT	  128

enum {
	WO_MIST_QUADRATIC          = 0,
	WO_MIST_LINEAR             = 1,
	WO_MIST_INVERSE_QUADRATIC  = 2,
};

/* flag */
#define WO_DS_EXPAND	(1<<0)
	/* NOTE: this must have the same value as MA_DS_SHOW_TEXS,
	 * otherwise anim-editors will not read correctly
	 */
#define WO_DS_SHOW_TEXS	(1<<2)

#endif
