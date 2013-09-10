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
	int typemap[39];              /* runtime only! - maps types to indices of first layer of that type,
	                               * MUST be >= CD_NUMTYPES, but we cant use a define here.
	                               * Correct size is ensured in CustomData_update_typemap assert() */
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
	CD_NUMTYPES         = 39,
};

/* Bits for CustomDataMask */
enum {
	CD_MASK_MVERT            = (1LL << CD_MVERT),
	CD_MASK_MSTICKY          = (1LL << CD_MSTICKY),  /* DEPRECATED */
	CD_MASK_MDEFORMVERT      = (1LL << CD_MDEFORMVERT),
	CD_MASK_MEDGE            = (1LL << CD_MEDGE),
	CD_MASK_MFACE            = (1LL << CD_MFACE),
	CD_MASK_MTFACE           = (1LL << CD_MTFACE),
	CD_MASK_MCOL             = (1LL << CD_MCOL),
	CD_MASK_ORIGINDEX        = (1LL << CD_ORIGINDEX),
	CD_MASK_NORMAL           = (1LL << CD_NORMAL),
/*	CD_MASK_POLYINDEX        = (1LL << CD_POLYINDEX), */
	CD_MASK_PROP_FLT         = (1LL << CD_PROP_FLT),
	CD_MASK_PROP_INT         = (1LL << CD_PROP_INT),
	CD_MASK_PROP_STR         = (1LL << CD_PROP_STR),
	CD_MASK_ORIGSPACE        = (1LL << CD_ORIGSPACE),
	CD_MASK_ORCO             = (1LL << CD_ORCO),
	CD_MASK_MTEXPOLY         = (1LL << CD_MTEXPOLY),
	CD_MASK_MLOOPUV          = (1LL << CD_MLOOPUV),
	CD_MASK_MLOOPCOL         = (1LL << CD_MLOOPCOL),
	CD_MASK_TANGENT          = (1LL << CD_TANGENT),
	CD_MASK_MDISPS           = (1LL << CD_MDISPS),
	CD_MASK_PREVIEW_MCOL     = (1LL << CD_PREVIEW_MCOL),
	CD_MASK_CLOTH_ORCO       = (1LL << CD_CLOTH_ORCO),
	CD_MASK_RECAST           = (1LL << CD_RECAST),

/* BMESH ONLY START */
	CD_MASK_MPOLY            = (1LL << CD_MPOLY),
	CD_MASK_MLOOP            = (1LL << CD_MLOOP),
	CD_MASK_SHAPE_KEYINDEX   = (1LL << CD_SHAPE_KEYINDEX),
	CD_MASK_SHAPEKEY         = (1LL << CD_SHAPEKEY),
	CD_MASK_BWEIGHT          = (1LL << CD_BWEIGHT),
	CD_MASK_CREASE           = (1LL << CD_CREASE),
	CD_MASK_ORIGSPACE_MLOOP  = (1LL << CD_ORIGSPACE_MLOOP),
	CD_MASK_PREVIEW_MLOOPCOL = (1LL << CD_PREVIEW_MLOOPCOL),
	CD_MASK_BM_ELEM_PYPTR    = (1LL << CD_BM_ELEM_PYPTR),
/* BMESH ONLY END */

	CD_MASK_PAINT_MASK       = (1LL << CD_PAINT_MASK),
	CD_MASK_GRID_PAINT_MASK  = (1LL << CD_GRID_PAINT_MASK),
	CD_MASK_MVERT_SKIN       = (1LL << CD_MVERT_SKIN),
	CD_MASK_FREESTYLE_EDGE   = (1LL << CD_FREESTYLE_EDGE),
	CD_MASK_FREESTYLE_FACE   = (1LL << CD_FREESTYLE_FACE),
};

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
