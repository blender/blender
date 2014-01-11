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
 *
 * Used for custom mesh data types (stored per vert/edge/loop/face)
 */

#ifndef __DNA_CUSTOMDATA_TYPES_H__
#define __DNA_CUSTOMDATA_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif

/** descriptor and storage for a custom data layer */
typedef struct CustomDataLayer {
	int type;       /* type of data in layer */
	int offset;     /* in editmode, offset of layer in block */
	int flag;       /* general purpose flag */
	int active;     /* number of the active layer of this type */
	int active_rnd; /* number of the layer to render*/
	int active_clone; /* number of the layer to render*/
	int active_mask; /* number of the layer to render*/
	int uid;        /* shape keyblock unique id reference*/
	char name[64];  /* layer name, MAX_CUSTOMDATA_LAYER_NAME */
	void *data;     /* layer data */
} CustomDataLayer;

#define MAX_CUSTOMDATA_LAYER_NAME 64

typedef struct CustomDataExternal {
	char filename[1024]; /* FILE_MAX */
} CustomDataExternal;

/** structure which stores custom element data associated with mesh elements
 * (vertices, edges or faces). The custom data is organized into a series of
 * layers, each with a data type (e.g. MTFace, MDeformVert, etc.). */
typedef struct CustomData {
	CustomDataLayer *layers;      /* CustomDataLayers, ordered by type */
	int typemap[40];              /* runtime only! - maps types to indices of first layer of that type,
	                               * MUST be >= CD_NUMTYPES, but we cant use a define here.
	                               * Correct size is ensured in CustomData_update_typemap assert() */
	int pad[1];
	int totlayer, maxlayer;       /* number of layers, size of layers array */
	int totsize;                  /* in editmode, total size of all data layers */
	void *pool;                   /* Bmesh: Memory pool for allocation of blocks */
	CustomDataExternal *external; /* external file storing customdata layers */
} CustomData;

/* CustomData.type */
enum {
	CD_MVERT            = 0,
	CD_MSTICKY          = 1,  /* DEPRECATED */
	CD_MDEFORMVERT      = 2,
	CD_MEDGE            = 3,
	CD_MFACE            = 4,
	CD_MTFACE           = 5,
	CD_MCOL             = 6,
	CD_ORIGINDEX        = 7,
	CD_NORMAL           = 8,
/*	CD_POLYINDEX        = 9, */
	CD_PROP_FLT         = 10,
	CD_PROP_INT         = 11,
	CD_PROP_STR         = 12,
	CD_ORIGSPACE        = 13,  /* for modifier stack face location mapping */
	CD_ORCO             = 14,
	CD_MTEXPOLY         = 15,
	CD_MLOOPUV          = 16,
	CD_MLOOPCOL         = 17,
	CD_TANGENT          = 18,
	CD_MDISPS           = 19,
	CD_PREVIEW_MCOL     = 20,  /* for displaying weightpaint colors */
	CD_ID_MCOL          = 21,
	CD_TEXTURE_MCOL     = 22,
	CD_CLOTH_ORCO       = 23,
	CD_RECAST           = 24,

/* BMESH ONLY START */
	CD_MPOLY            = 25,
	CD_MLOOP            = 26,
	CD_SHAPE_KEYINDEX   = 27,
	CD_SHAPEKEY         = 28,
	CD_BWEIGHT          = 29,
	CD_CREASE           = 30,
	CD_ORIGSPACE_MLOOP  = 31,
	CD_PREVIEW_MLOOPCOL = 32,
	CD_BM_ELEM_PYPTR    = 33,
/* BMESH ONLY END */

	CD_PAINT_MASK       = 34,
	CD_GRID_PAINT_MASK  = 35,
	CD_MVERT_SKIN       = 36,
	CD_FREESTYLE_EDGE   = 37,
	CD_FREESTYLE_FACE   = 38,
	CD_MLOOPTANGENT     = 39,
	CD_NUMTYPES         = 40,
};

/* Bits for CustomDataMask */
#define CD_MASK_MVERT		(1 << CD_MVERT)
#define CD_MASK_MSTICKY		(1 << CD_MSTICKY)  /* DEPRECATED */
#define CD_MASK_MDEFORMVERT	(1 << CD_MDEFORMVERT)
#define CD_MASK_MEDGE		(1 << CD_MEDGE)
#define CD_MASK_MFACE		(1 << CD_MFACE)
#define CD_MASK_MTFACE		(1 << CD_MTFACE)
#define CD_MASK_MCOL		(1 << CD_MCOL)
#define CD_MASK_ORIGINDEX	(1 << CD_ORIGINDEX)
#define CD_MASK_NORMAL		(1 << CD_NORMAL)
// #define CD_MASK_POLYINDEX	(1 << CD_POLYINDEX)
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
#define CD_MASK_PREVIEW_MCOL	(1 << CD_PREVIEW_MCOL)
#define CD_MASK_CLOTH_ORCO	(1 << CD_CLOTH_ORCO)
#define CD_MASK_RECAST		(1 << CD_RECAST)

/* BMESH ONLY START */
#define CD_MASK_MPOLY		(1 << CD_MPOLY)
#define CD_MASK_MLOOP		(1 << CD_MLOOP)
#define CD_MASK_SHAPE_KEYINDEX	(1 << CD_SHAPE_KEYINDEX)
#define CD_MASK_SHAPEKEY	(1 << CD_SHAPEKEY)
#define CD_MASK_BWEIGHT		(1 << CD_BWEIGHT)
#define CD_MASK_CREASE		(1 << CD_CREASE)
#define CD_MASK_ORIGSPACE_MLOOP	(1LL << CD_ORIGSPACE_MLOOP)
#define CD_MASK_PREVIEW_MLOOPCOL (1LL << CD_PREVIEW_MLOOPCOL)
#define CD_MASK_BM_ELEM_PYPTR (1LL << CD_BM_ELEM_PYPTR)
/* BMESH ONLY END */

#define CD_MASK_PAINT_MASK		(1LL << CD_PAINT_MASK)
#define CD_MASK_GRID_PAINT_MASK	(1LL << CD_GRID_PAINT_MASK)
#define CD_MASK_MVERT_SKIN		(1LL << CD_MVERT_SKIN)
#define CD_MASK_FREESTYLE_EDGE	(1LL << CD_FREESTYLE_EDGE)
#define CD_MASK_FREESTYLE_FACE	(1LL << CD_FREESTYLE_FACE)
#define CD_MASK_MLOOPTANGENT     (1LL << CD_MLOOPTANGENT)

/* CustomData.flag */
enum {
	/* Indicates layer should not be copied by CustomData_from_template or CustomData_copy_data */
	CD_FLAG_NOCOPY    = (1 << 0),
	/* Indicates layer should not be freed (for layers backed by external data) */
	CD_FLAG_NOFREE    = (1 << 1),
	/* Indicates the layer is only temporary, also implies no copy */
	CD_FLAG_TEMPORARY = ((1 << 2) | CD_FLAG_NOCOPY),
	/* Indicates the layer is stored in an external file */
	CD_FLAG_EXTERNAL  = (1 << 3),
	/* Indicates external data is read into memory */
	CD_FLAG_IN_MEMORY = (1 << 4),
};

/* Limits */
#define MAX_MTFACE  8
#define MAX_MCOL    8

#ifdef __cplusplus
}
#endif

#endif  /* __DNA_CUSTOMDATA_TYPES_H__ */
