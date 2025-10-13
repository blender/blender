/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 *
 * Used for custom mesh data types (stored per vert/edge/loop/face)
 */

#pragma once

#include "DNA_defs.h"

#include "BLI_implicit_sharing.h"

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
  /** Shape key-block unique id reference. */
  int uid;
  /** Layer name. */
  char name[/*MAX_CUSTOMDATA_LAYER_NAME*/ 68];
  char _pad1[4];
  /** Layer data. */
  void *data;
  /**
   * Run-time data that allows sharing `data` with other entities (mostly custom data layers on
   * other geometries).
   */
  const ImplicitSharingInfoHandle *sharing_info;
} CustomDataLayer;

#define MAX_CUSTOMDATA_LAYER_NAME 68
#define MAX_CUSTOMDATA_LAYER_NAME_NO_PREFIX 64

typedef struct CustomDataExternal {
  char filepath[/*FILE_MAX*/ 1024];
} CustomDataExternal;

/**
 * #CustomData stores an arbitrary number of typed data "layers" for multiple elements.
 * The layers are typically geometry attributes, and the elements are typically geometry
 * elements like vertices, edges, or curves.
 *
 * Each layer has a type, often with certain semantics beyond the type of the raw data. However,
 * a subset of the layer types are exposed as attributes and accessed with a higher level API
 * built around #AttributeAccessor.
 *
 * For #BMesh, #CustomData is adapted to store the data from all layers in a single "block" which
 * is allocated for each element. Each layer's data is stored at a certain offset into every
 * block's data.
 */
typedef struct CustomData {
  /** Layers ordered by type. */
  CustomDataLayer *layers;
  /**
   * Runtime only map from types to indices of first layer of that type,
   * Correct size of #CD_NUMTYPES is ensured by CustomData_update_typemap.
   */
  int typemap[53];
  /** Number of layers, size of layers array. */
  int totlayer, maxlayer;
  /** In editmode, total size of all data layers. */
  int totsize;
  /** (BMesh Only): Memory pool for allocation of blocks. */
  struct BLI_mempool *pool;
  /** External file storing custom-data layers. */
  CustomDataExternal *external;
} CustomData;

/** #CustomDataLayer.type */
typedef enum eCustomDataType {
  /**
   * Used by GPU attributes in the cases when we don't know which layer
   * we are addressing in advance.
   */
  CD_AUTO_FROM_NAME = -1,

#ifdef DNA_DEPRECATED_ALLOW
  CD_MVERT = 0,
  CD_MSTICKY = 1,
#endif
  CD_MDEFORMVERT = 2, /* Array of #MDeformVert. */
#ifdef DNA_DEPRECATED_ALLOW
  CD_MEDGE = 3,
#endif
  CD_MFACE = 4,
  CD_MTFACE = 5,
  CD_MCOL = 6,
  CD_ORIGINDEX = 7,
  /**
   * Used as temporary storage for some areas that support interpolating custom normals.
   * Using a separate type from generic 3D vectors is a simple way of keeping values normalized.
   */
  CD_NORMAL = 8,
#ifdef DNA_DEPRECATED_ALLOW
  CD_FACEMAP = 9,
#endif
  CD_PROP_FLOAT = 10,
  CD_PROP_INT32 = 11,
  CD_PROP_STRING = 12,
  CD_ORIGSPACE = 13, /* for modifier stack face location mapping */
  CD_ORCO = 14,      /* undeformed vertex coordinates, normalized to 0..1 range */
#ifdef DNA_DEPRECATED_ALLOW
  CD_MTEXPOLY = 15,
  CD_MLOOPUV = 16,
#endif
  CD_PROP_BYTE_COLOR = 17,
  /**
   * Previously used for runtime corner tangent storage in mesh #CustomData. Currently only used
   * as an identifier to choose tangents in a few places.
   */
  CD_TANGENT = 18,
  CD_MDISPS = 19,
  CD_PROP_FLOAT4X4 = 20,
  /* CD_ID_MCOL = 21, */
  CD_PROP_INT16_2D = 22,
  CD_CLOTH_ORCO = 23,
/* CD_RECAST = 24, */ /* UNUSED */

#ifdef DNA_DEPRECATED_ALLOW
  CD_MPOLY = 25,
  CD_MLOOP = 26,
#endif
  CD_SHAPE_KEYINDEX = 27,
  CD_SHAPEKEY = 28,
#ifdef DNA_DEPRECATED_ALLOW
  CD_BWEIGHT = 29,
  CD_CREASE = 30,
#endif
  CD_ORIGSPACE_MLOOP = 31,
  /* CD_PREVIEW_MLOOPCOL = 32, */ /* UNUSED */
  CD_BM_ELEM_PYPTR = 33,

#ifdef DNA_DEPRECATED_ALLOW
  CD_PAINT_MASK = 34,
#endif
  CD_GRID_PAINT_MASK = 35,
  CD_MVERT_SKIN = 36,
#ifdef DNA_DEPRECATED_ALLOW
  CD_FREESTYLE_EDGE = 37,
  CD_FREESTYLE_FACE = 38,
#endif
  CD_MLOOPTANGENT = 39,
  CD_TESSLOOPNORMAL = 40,
#ifdef DNA_DEPRECATED_ALLOW
  CD_CUSTOMLOOPNORMAL = 41,
  CD_SCULPT_FACE_SETS = 42,
#endif

  /* CD_LOCATION = 43, */ /* UNUSED */
  /* CD_RADIUS = 44, */   /* UNUSED */
  CD_PROP_INT8 = 45,
  /* Two 32-bit signed integers. */
  CD_PROP_INT32_2D = 46,

  CD_PROP_COLOR = 47,
  CD_PROP_FLOAT3 = 48,
  CD_PROP_FLOAT2 = 49,
  CD_PROP_BOOL = 50,

  /* CD_HAIRLENGTH = 51, */ /* UNUSED */

  CD_PROP_QUATERNION = 52,

  CD_NUMTYPES = 53,
} eCustomDataType;

#ifdef __cplusplus
using eCustomDataMask = uint64_t;
#endif

/* Bits for #eCustomDataMask */

#define CD_MASK_MDEFORMVERT (1 << CD_MDEFORMVERT)
#define CD_MASK_MFACE (1 << CD_MFACE)
#define CD_MASK_MTFACE (1 << CD_MTFACE)
#define CD_MASK_MCOL (1 << CD_MCOL)
#define CD_MASK_ORIGINDEX (1 << CD_ORIGINDEX)
#define CD_MASK_NORMAL (1 << CD_NORMAL)
#define CD_MASK_PROP_FLOAT (1 << CD_PROP_FLOAT)
#define CD_MASK_PROP_INT32 (1 << CD_PROP_INT32)
#define CD_MASK_PROP_STRING (1 << CD_PROP_STRING)
#define CD_MASK_ORIGSPACE (1 << CD_ORIGSPACE)
#define CD_MASK_ORCO (1 << CD_ORCO)
#define CD_MASK_PROP_BYTE_COLOR (1 << CD_PROP_BYTE_COLOR)
#define CD_MASK_MDISPS (1 << CD_MDISPS)
#define CD_MASK_CLOTH_ORCO (1 << CD_CLOTH_ORCO)

#define CD_MASK_SHAPE_KEYINDEX (1 << CD_SHAPE_KEYINDEX)
#define CD_MASK_SHAPEKEY (1 << CD_SHAPEKEY)
#define CD_MASK_ORIGSPACE_MLOOP (1LL << CD_ORIGSPACE_MLOOP)
#define CD_MASK_BM_ELEM_PYPTR (1LL << CD_BM_ELEM_PYPTR)

#define CD_MASK_GRID_PAINT_MASK (1LL << CD_GRID_PAINT_MASK)
#define CD_MASK_MVERT_SKIN (1LL << CD_MVERT_SKIN)
#define CD_MASK_MLOOPTANGENT (1LL << CD_MLOOPTANGENT)
#define CD_MASK_TESSLOOPNORMAL (1LL << CD_TESSLOOPNORMAL)
#define CD_MASK_PROP_COLOR (1ULL << CD_PROP_COLOR)
#define CD_MASK_PROP_FLOAT3 (1ULL << CD_PROP_FLOAT3)
#define CD_MASK_PROP_FLOAT2 (1ULL << CD_PROP_FLOAT2)
#define CD_MASK_PROP_BOOL (1ULL << CD_PROP_BOOL)
#define CD_MASK_PROP_INT8 (1ULL << CD_PROP_INT8)
#define CD_MASK_PROP_INT16_2D (1ULL << CD_PROP_INT16_2D)
#define CD_MASK_PROP_INT32_2D (1ULL << CD_PROP_INT32_2D)
#define CD_MASK_PROP_QUATERNION (1ULL << CD_PROP_QUATERNION)
#define CD_MASK_PROP_FLOAT4X4 (1ULL << CD_PROP_FLOAT4X4)

/** Multi-resolution loop data. */
#define CD_MASK_MULTIRES_GRIDS (CD_MASK_MDISPS | CD_GRID_PAINT_MASK)

/** All data layers. */
#define CD_MASK_ALL (~0LL)

/** All generic attributes. */
#define CD_MASK_PROP_ALL \
  (CD_MASK_PROP_FLOAT | CD_MASK_PROP_FLOAT2 | CD_MASK_PROP_FLOAT3 | CD_MASK_PROP_INT32 | \
   CD_MASK_PROP_COLOR | CD_MASK_PROP_STRING | CD_MASK_PROP_BYTE_COLOR | CD_MASK_PROP_BOOL | \
   CD_MASK_PROP_INT8 | CD_MASK_PROP_INT16_2D | CD_MASK_PROP_INT32_2D | CD_MASK_PROP_QUATERNION | \
   CD_MASK_PROP_FLOAT4X4)

/** All color attributes */
#define CD_MASK_COLOR_ALL (CD_MASK_PROP_COLOR | CD_MASK_PROP_BYTE_COLOR)

typedef struct CustomData_MeshMasks {
  uint64_t vmask;
  uint64_t emask;
  uint64_t fmask;
  uint64_t pmask;
  uint64_t lmask;
} CustomData_MeshMasks;

/** #CustomData.flag */
enum {
  /**
   * Indicates layer should not be copied by #CustomData_from_template or #CustomData_copy_data.
   */
  CD_FLAG_NOCOPY = (1 << 0),
  CD_FLAG_UNUSED = (1 << 1),
  /** Indicates the layer is only temporary, also implies no copy */
  CD_FLAG_TEMPORARY = ((1 << 2) | CD_FLAG_NOCOPY),
  /** Indicates the layer is stored in an external file */
  CD_FLAG_EXTERNAL = (1 << 3),
  /** Indicates external data is read into memory */
  CD_FLAG_IN_MEMORY = (1 << 4),
#ifdef DNA_DEPRECATED_ALLOW
  CD_FLAG_COLOR_ACTIVE = (1 << 5),
  CD_FLAG_COLOR_RENDER = (1 << 6)
#endif
};

/** Limits. */
#define MAX_MTFACE 8
