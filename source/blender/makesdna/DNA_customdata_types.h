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

/** \file
 * \ingroup DNA
 *
 * Used for custom mesh data types (stored per vert/edge/loop/face)
 */

#ifndef __DNA_CUSTOMDATA_TYPES_H__
#define __DNA_CUSTOMDATA_TYPES_H__

#include "DNA_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Descriptor and storage for a custom data layer. */
typedef struct CustomDataLayer {
  /** Type of data in layer. */
  int type;
  /** In editmode, offset of layer in block. */
  int offset;
  /** General purpose flag. */
  int flag;
  /** Number of the active layer of this type. */
  int active;
  /** Number of the layer to render. */
  int active_rnd;
  /** Number of the layer to render. */
  int active_clone;
  /** Number of the layer to render. */
  int active_mask;
  /** Shape keyblock unique id reference. */
  int uid;
  /** Layer name, MAX_CUSTOMDATA_LAYER_NAME. */
  char name[64];
  /** Layer data. */
  void *data;
} CustomDataLayer;

#define MAX_CUSTOMDATA_LAYER_NAME 64

typedef struct CustomDataExternal {
  /** FILE_MAX. */
  char filename[1024];
} CustomDataExternal;

/**
 * Structure which stores custom element data associated with mesh elements
 * (vertices, edges or faces). The custom data is organized into a series of
 * layers, each with a data type (e.g. MTFace, MDeformVert, etc.).
 */
typedef struct CustomData {
  /** CustomDataLayers, ordered by type. */
  CustomDataLayer *layers;
  /**
   * runtime only! - maps types to indices of first layer of that type,
   * MUST be >= CD_NUMTYPES, but we cant use a define here.
   * Correct size is ensured in CustomData_update_typemap assert().
   */
  int typemap[48];
  char _pad[4];
  /** Number of layers, size of layers array. */
  int totlayer, maxlayer;
  /** In editmode, total size of all data layers. */
  int totsize;
  /** (BMesh Only): Memory pool for allocation of blocks. */
  struct BLI_mempool *pool;
  /** External file storing customdata layers. */
  CustomDataExternal *external;
} CustomData;

/* CustomData.type */
typedef enum CustomDataType {
  /* Used by GLSL attributes in the cases when we need a delayed CD type
   * assignment (in the cases when we don't know in advance which layer
   * we are addressing).
   */
  CD_AUTO_FROM_NAME = -1,

  CD_MVERT = 0,
#ifdef DNA_DEPRECATED_ALLOW
  CD_MSTICKY = 1, /* DEPRECATED */
#endif
  CD_MDEFORMVERT = 2,
  CD_MEDGE = 3,
  CD_MFACE = 4,
  CD_MTFACE = 5,
  CD_MCOL = 6,
  CD_ORIGINDEX = 7,
  CD_NORMAL = 8,
  CD_FACEMAP = 9, /* exclusive face group, each face can only be part of one */
  CD_PROP_FLOAT = 10,
  CD_PROP_INT32 = 11,
  CD_PROP_STRING = 12,
  CD_ORIGSPACE = 13, /* for modifier stack face location mapping */
  CD_ORCO = 14,      /* undeformed vertex coordinates, normalized to 0..1 range */
#ifdef DNA_DEPRECATED_ALLOW
  CD_MTEXPOLY = 15, /* deprecated */
#endif
  CD_MLOOPUV = 16,
  CD_MLOOPCOL = 17,
  CD_TANGENT = 18,
  CD_MDISPS = 19,
  CD_PREVIEW_MCOL = 20,           /* for displaying weightpaint colors */
                                  /*  CD_ID_MCOL          = 21, */
  /* CD_TEXTURE_MLOOPCOL = 22, */ /* UNUSED */
  CD_CLOTH_ORCO = 23,
  CD_RECAST = 24,

  /* BMESH ONLY START */
  CD_MPOLY = 25,
  CD_MLOOP = 26,
  CD_SHAPE_KEYINDEX = 27,
  CD_SHAPEKEY = 28,
  CD_BWEIGHT = 29,
  CD_CREASE = 30,
  CD_ORIGSPACE_MLOOP = 31,
  CD_PREVIEW_MLOOPCOL = 32,
  CD_BM_ELEM_PYPTR = 33,
  /* BMESH ONLY END */

  CD_PAINT_MASK = 34,
  CD_GRID_PAINT_MASK = 35,
  CD_MVERT_SKIN = 36,
  CD_FREESTYLE_EDGE = 37,
  CD_FREESTYLE_FACE = 38,
  CD_MLOOPTANGENT = 39,
  CD_TESSLOOPNORMAL = 40,
  CD_CUSTOMLOOPNORMAL = 41,
  CD_SCULPT_FACE_SETS = 42,

  CD_LOCATION = 43,
  CD_HAIRCURVE = 45,
  CD_RADIUS = 44,
  CD_HAIRMAPPING = 46,

  CD_PROP_COLOR = 47,

  CD_NUMTYPES = 48,
} CustomDataType;

/* Bits for CustomDataMask */
#define CD_MASK_MVERT (1 << CD_MVERT)
// #define CD_MASK_MSTICKY      (1 << CD_MSTICKY)  /* DEPRECATED */
#define CD_MASK_MDEFORMVERT (1 << CD_MDEFORMVERT)
#define CD_MASK_MEDGE (1 << CD_MEDGE)
#define CD_MASK_MFACE (1 << CD_MFACE)
#define CD_MASK_MTFACE (1 << CD_MTFACE)
#define CD_MASK_MCOL (1 << CD_MCOL)
#define CD_MASK_ORIGINDEX (1 << CD_ORIGINDEX)
#define CD_MASK_NORMAL (1 << CD_NORMAL)
#define CD_MASK_FACEMAP (1 << CD_FACEMAP)
#define CD_MASK_PROP_FLOAT (1 << CD_PROP_FLOAT)
#define CD_MASK_PROP_INT32 (1 << CD_PROP_INT32)
#define CD_MASK_PROP_STRING (1 << CD_PROP_STRING)
#define CD_MASK_ORIGSPACE (1 << CD_ORIGSPACE)
#define CD_MASK_ORCO (1 << CD_ORCO)
// #define CD_MASK_MTEXPOLY (1 << CD_MTEXPOLY)  /* DEPRECATED */
#define CD_MASK_MLOOPUV (1 << CD_MLOOPUV)
#define CD_MASK_MLOOPCOL (1 << CD_MLOOPCOL)
#define CD_MASK_TANGENT (1 << CD_TANGENT)
#define CD_MASK_MDISPS (1 << CD_MDISPS)
#define CD_MASK_PREVIEW_MCOL (1 << CD_PREVIEW_MCOL)
#define CD_MASK_CLOTH_ORCO (1 << CD_CLOTH_ORCO)
#define CD_MASK_RECAST (1 << CD_RECAST)

/* BMESH ONLY START */
#define CD_MASK_MPOLY (1 << CD_MPOLY)
#define CD_MASK_MLOOP (1 << CD_MLOOP)
#define CD_MASK_SHAPE_KEYINDEX (1 << CD_SHAPE_KEYINDEX)
#define CD_MASK_SHAPEKEY (1 << CD_SHAPEKEY)
#define CD_MASK_BWEIGHT (1 << CD_BWEIGHT)
#define CD_MASK_CREASE (1 << CD_CREASE)
#define CD_MASK_ORIGSPACE_MLOOP (1LL << CD_ORIGSPACE_MLOOP)
#define CD_MASK_PREVIEW_MLOOPCOL (1LL << CD_PREVIEW_MLOOPCOL)
#define CD_MASK_BM_ELEM_PYPTR (1LL << CD_BM_ELEM_PYPTR)
/* BMESH ONLY END */

#define CD_MASK_PAINT_MASK (1LL << CD_PAINT_MASK)
#define CD_MASK_GRID_PAINT_MASK (1LL << CD_GRID_PAINT_MASK)
#define CD_MASK_MVERT_SKIN (1LL << CD_MVERT_SKIN)
#define CD_MASK_FREESTYLE_EDGE (1LL << CD_FREESTYLE_EDGE)
#define CD_MASK_FREESTYLE_FACE (1LL << CD_FREESTYLE_FACE)
#define CD_MASK_MLOOPTANGENT (1LL << CD_MLOOPTANGENT)
#define CD_MASK_TESSLOOPNORMAL (1LL << CD_TESSLOOPNORMAL)
#define CD_MASK_CUSTOMLOOPNORMAL (1LL << CD_CUSTOMLOOPNORMAL)
#define CD_MASK_SCULPT_FACE_SETS (1LL << CD_SCULPT_FACE_SETS)
#define CD_MASK_PROP_COLOR (1ULL << CD_PROP_COLOR)

/** Data types that may be defined for all mesh elements types. */
#define CD_MASK_GENERIC_DATA (CD_MASK_PROP_FLOAT | CD_MASK_PROP_INT32 | CD_MASK_PROP_STRING)

/** Multires loop data. */
#define CD_MASK_MULTIRES_GRIDS (CD_MASK_MDISPS | CD_GRID_PAINT_MASK)

/* All data layers. */
#define CD_MASK_ALL (~0LL)

typedef struct CustomData_MeshMasks {
  uint64_t vmask;
  uint64_t emask;
  uint64_t fmask;
  uint64_t pmask;
  uint64_t lmask;
} CustomData_MeshMasks;

/* CustomData.flag */
enum {
  /* Indicates layer should not be copied by CustomData_from_template or CustomData_copy_data */
  CD_FLAG_NOCOPY = (1 << 0),
  /* Indicates layer should not be freed (for layers backed by external data) */
  CD_FLAG_NOFREE = (1 << 1),
  /* Indicates the layer is only temporary, also implies no copy */
  CD_FLAG_TEMPORARY = ((1 << 2) | CD_FLAG_NOCOPY),
  /* Indicates the layer is stored in an external file */
  CD_FLAG_EXTERNAL = (1 << 3),
  /* Indicates external data is read into memory */
  CD_FLAG_IN_MEMORY = (1 << 4),
};

/* Limits */
#define MAX_MTFACE 8
#define MAX_MCOL 8

#define DYNTOPO_NODE_NONE -1

#define CD_TEMP_CHUNK_SIZE 128

#ifdef __cplusplus
}
#endif

#endif /* __DNA_CUSTOMDATA_TYPES_H__ */
