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

/** \file DNA_customdata_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_CUSTOMDATA_TYPES_H__
#define __DNA_CUSTOMDATA_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_defs.h" /* USE_BMESH_FORWARD_COMPAT */

/** descriptor and storage for a custom data layer */
typedef struct CustomDataLayer {
	int type;       /* type of data in layer */
	int offset;     /* in editmode, offset of layer in block */
	int flag;       /* general purpose flag */
	int active;     /* number of the active layer of this type */
	int active_rnd; /* number of the layer to render*/
	int active_clone; /* number of the layer to render*/
	int active_mask; /* number of the layer to render*/
	char pad[4];
	char name[64];  /* layer name, MAX_CUSTOMDATA_LAYER_AAME */
	void *data;     /* layer data */
} CustomDataLayer;

#define MAX_CUSTOMDATA_LAYER_NAME 64

typedef struct CustomDataExternal {
	char filename[1024]; /* FILE_MAX */
} CustomDataExternal;

/** structure which stores custom element data associated with mesh elements
 * (vertices, edges or faces). The custom data is organised into a series of
 * layers, each with a data type (e.g. MTFace, MDeformVert, etc.). */
typedef struct CustomData {
	CustomDataLayer *layers;      /* CustomDataLayers, ordered by type */
	int typemap[32];              /* runtime only! - maps types to indices of first layer of that type,
	                               * MUST be >= CD_NUMTYPES, but we cant use a define here.
	                               * Correct size is ensured in CustomData_update_typemap assert() */
	int totlayer, maxlayer;       /* number of layers, size of layers array */
	int totsize, pad;             /* in editmode, total size of all data layers */
	void *pool;                   /* Bmesh: Memory pool for allocation of blocks */
	CustomDataExternal *external; /* external file storing customdata layers */
} CustomData;

/* CustomData.type */
#define CD_MVERT		0
#define CD_MSTICKY		1
#define CD_MDEFORMVERT	2
#define CD_MEDGE		3
#define CD_MFACE		4
#define CD_MTFACE		5
#define CD_MCOL			6
#define CD_ORIGINDEX	7
#define CD_NORMAL		8
#define CD_POLYINDEX	9
#define CD_PROP_FLT		10
#define CD_PROP_INT		11
#define CD_PROP_STR		12
#define CD_ORIGSPACE	13 /* for modifier stack face location mapping */
#define CD_ORCO			14
#define CD_MTEXPOLY		15
#define CD_MLOOPUV		16
#define CD_MLOOPCOL		17
#define CD_TANGENT		18
#define CD_MDISPS		19
#define CD_WEIGHT_MCOL	20 /* for displaying weightpaint colors */
#define CD_ID_MCOL		21
#define CD_TEXTURE_MCOL	22
#define CD_CLOTH_ORCO	23
#define CD_RECAST		24

#ifdef USE_BMESH_FORWARD_COMPAT

/* BMESH ONLY START */
#define CD_MPOLY		25
#define CD_MLOOP		26
#define CD_SHAPE_KEYINDEX	27
#define CD_SHAPEKEY		28
#define CD_BWEIGHT		29
#define CD_CREASE		30
#define CD_WEIGHT_MLOOPCOL	31
/* BMESH ONLY END */

#define CD_NUMTYPES		32

#else

#define CD_NUMTYPES		25

#endif

/* Bits for CustomDataMask */
#define CD_MASK_MVERT		(1 << CD_MVERT)
#define CD_MASK_MSTICKY		(1 << CD_MSTICKY)
#define CD_MASK_MDEFORMVERT	(1 << CD_MDEFORMVERT)
#define CD_MASK_MEDGE		(1 << CD_MEDGE)
#define CD_MASK_MFACE		(1 << CD_MFACE)
#define CD_MASK_MTFACE		(1 << CD_MTFACE)
#define CD_MASK_MCOL		(1 << CD_MCOL)
#define CD_MASK_ORIGINDEX	(1 << CD_ORIGINDEX)
#define CD_MASK_NORMAL		(1 << CD_NORMAL)
#define CD_MASK_POLYINDEX	(1 << CD_POLYINDEX)
#define CD_MASK_PROP_FLT	(1 << CD_PROP_FLT)
#define CD_MASK_PROP_INT	(1 << CD_PROP_INT)
#define CD_MASK_PROP_STR	(1 << CD_PROP_STR)
#define CD_MASK_ORIGSPACE	(1 << CD_ORIGSPACE)
#define CD_MASK_ORCO		(1 << CD_ORCO)
#define CD_MASK_MTEXPOLY	(1 << CD_MTEXPOLY)
#define CD_MASK_MLOOPUV		(1 << CD_MLOOPUV)
#define CD_MASK_MLOOPCOL	(1 << CD_MLOOPCOL)
#define CD_MASK_TANGENT		(1 << CD_TANGENT)
#define CD_MASK_MDISPS		(1 << CD_MDISPS)
#define CD_MASK_WEIGHT_MCOL	(1 << CD_WEIGHT_MCOL)
#define CD_MASK_CLOTH_ORCO	(1 << CD_CLOTH_ORCO)
#define CD_MASK_RECAST		(1 << CD_RECAST)

#ifdef USE_BMESH_FORWARD_COMPAT

/* BMESH ONLY START */
#define CD_MASK_MPOLY		(1 << CD_MPOLY)
#define CD_MASK_MLOOP		(1 << CD_MLOOP)
#define CD_MASK_SHAPE_KEYINDEX	(1 << CD_SHAPE_KEYINDEX)
#define CD_MASK_SHAPEKEY	(1 << CD_SHAPEKEY)
#define CD_MASK_BWEIGHT		(1 << CD_BWEIGHT)
#define CD_MASK_CREASE		(1 << CD_CREASE)
#define CD_MASK_WEIGHT_MLOOPCOL (1 << CD_WEIGHT_MLOOPCOL)
/* BMESH ONLY END */

#endif

/* CustomData.flag */

/* indicates layer should not be copied by CustomData_from_template or
 * CustomData_copy_data */
#define CD_FLAG_NOCOPY    (1<<0)
/* indicates layer should not be freed (for layers backed by external data) */
#define CD_FLAG_NOFREE    (1<<1)
/* indicates the layer is only temporary, also implies no copy */
#define CD_FLAG_TEMPORARY ((1<<2)|CD_FLAG_NOCOPY)
/* indicates the layer is stored in an external file */
#define CD_FLAG_EXTERNAL  (1<<3)
/* indicates external data is read into memory */
#define CD_FLAG_IN_MEMORY (1<<4)

/* Limits */
#define MAX_MTFACE 8
#define MAX_MCOL   8

#ifdef __cplusplus
}
#endif

#endif
