/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 * Some enums shared between mesh and grease pencil modifiers.
 */

#pragma once

/* Grease Pencil Modifiers */

typedef enum eArrayGpencil_Flag {
  GP_ARRAY_INVERT_LAYER = (1 << 2),
  GP_ARRAY_INVERT_PASS = (1 << 3),
  GP_ARRAY_INVERT_LAYERPASS = (1 << 5),
  GP_ARRAY_INVERT_MATERIAL = (1 << 6),
  GP_ARRAY_USE_OFFSET = (1 << 7),
  GP_ARRAY_USE_RELATIVE = (1 << 8),
  GP_ARRAY_USE_OB_OFFSET = (1 << 9),
  GP_ARRAY_UNIFORM_RANDOM_SCALE = (1 << 10),
} eArrayGpencil_Flag;

typedef enum eTextureGpencil_Fit {
  GP_TEX_FIT_STROKE = 0,
  GP_TEX_CONSTANT_LENGTH = 1,
} eTextureGpencil_Fit;

typedef enum eNoiseGpencil_Flag {
  GP_NOISE_USE_RANDOM = (1 << 0),
  GP_NOISE_MOD_LOCATION = (1 << 1),  /* Deprecated (only for versioning). */
  GP_NOISE_MOD_STRENGTH = (1 << 2),  /* Deprecated (only for versioning). */
  GP_NOISE_MOD_THICKNESS = (1 << 3), /* Deprecated (only for versioning). */
  GP_NOISE_FULL_STROKE = (1 << 4),
  GP_NOISE_CUSTOM_CURVE = (1 << 5),
  GP_NOISE_INVERT_LAYER = (1 << 6),
  GP_NOISE_INVERT_PASS = (1 << 7),
  GP_NOISE_INVERT_VGROUP = (1 << 8),
  GP_NOISE_MOD_UV = (1 << 9), /* Deprecated (only for versioning). */
  GP_NOISE_INVERT_LAYERPASS = (1 << 10),
  GP_NOISE_INVERT_MATERIAL = (1 << 11),
} eNoiseGpencil_Flag;

typedef enum eNoiseRandomGpencil_Mode {
  GP_NOISE_RANDOM_STEP = 0,
  GP_NOISE_RANDOM_KEYFRAME = 1,
} eNoiseRandomGpencil_Mode;

typedef enum eLengthGpencil_Flag {
  GP_LENGTH_INVERT_LAYER = (1 << 0),
  GP_LENGTH_INVERT_PASS = (1 << 1),
  GP_LENGTH_INVERT_LAYERPASS = (1 << 2),
  GP_LENGTH_INVERT_MATERIAL = (1 << 3),
  GP_LENGTH_USE_CURVATURE = (1 << 4),
  GP_LENGTH_INVERT_CURVATURE = (1 << 5),
  GP_LENGTH_USE_RANDOM = (1 << 6),
} eLengthGpencil_Flag;

typedef enum eLengthGpencil_Type {
  GP_LENGTH_RELATIVE = 0,
  GP_LENGTH_ABSOLUTE = 1,
} eLengthGpencil_Type;

/* Shrink-wrap Modifier */

/** #ShrinkwrapModifierData.shrinkType */
enum {
  MOD_SHRINKWRAP_NEAREST_SURFACE = 0,
  MOD_SHRINKWRAP_PROJECT = 1,
  MOD_SHRINKWRAP_NEAREST_VERTEX = 2,
  MOD_SHRINKWRAP_TARGET_PROJECT = 3,
};

/** #ShrinkwrapModifierData.shrinkMode */
enum {
  /** Move vertex to the surface of the target object (keepDist towards original position) */
  MOD_SHRINKWRAP_ON_SURFACE = 0,
  /** Move the vertex inside the target object; don't change if already inside */
  MOD_SHRINKWRAP_INSIDE = 1,
  /** Move the vertex outside the target object; don't change if already outside */
  MOD_SHRINKWRAP_OUTSIDE = 2,
  /** Move vertex to the surface of the target object, with keepDist towards the outside */
  MOD_SHRINKWRAP_OUTSIDE_SURFACE = 3,
  /** Move vertex to the surface of the target object, with keepDist along the normal */
  MOD_SHRINKWRAP_ABOVE_SURFACE = 4,
};

/** #ShrinkwrapModifierData.shrinkOpts */
enum {
  /** Allow shrink-wrap to move the vertex in the positive direction of axis. */
  MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR = (1 << 0),
  /** Allow shrink-wrap to move the vertex in the negative direction of axis. */
  MOD_SHRINKWRAP_PROJECT_ALLOW_NEG_DIR = (1 << 1),

  /** ignore vertex moves if a vertex ends projected on a front face of the target */
  MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE = (1 << 3),
  /** ignore vertex moves if a vertex ends projected on a back face of the target */
  MOD_SHRINKWRAP_CULL_TARGET_BACKFACE = (1 << 4),

#ifdef DNA_DEPRECATED_ALLOW
  /** distance is measure to the front face of the target */
  MOD_SHRINKWRAP_KEEP_ABOVE_SURFACE = (1 << 5),
#endif

  MOD_SHRINKWRAP_INVERT_VGROUP = (1 << 6),
  MOD_SHRINKWRAP_INVERT_CULL_TARGET = (1 << 7),
};

#define MOD_SHRINKWRAP_CULL_TARGET_MASK \
  (MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE | MOD_SHRINKWRAP_CULL_TARGET_BACKFACE)

/** #ShrinkwrapModifierData.projAxis */
enum {
  /** projection over normal is used if no axis is selected */
  MOD_SHRINKWRAP_PROJECT_OVER_NORMAL = 0,
  MOD_SHRINKWRAP_PROJECT_OVER_X_AXIS = (1 << 0),
  MOD_SHRINKWRAP_PROJECT_OVER_Y_AXIS = (1 << 1),
  MOD_SHRINKWRAP_PROJECT_OVER_Z_AXIS = (1 << 2),
};

/* TransferData modifier */

enum {
  DT_TYPE_MDEFORMVERT = 1 << 0,
  DT_TYPE_SKIN = 1 << 2,
  DT_TYPE_BWEIGHT_VERT = 1 << 3,

  DT_TYPE_SHARP_EDGE = 1 << 8,
  DT_TYPE_SEAM = 1 << 9,
  DT_TYPE_CREASE = 1 << 10,
  DT_TYPE_BWEIGHT_EDGE = 1 << 11,
  DT_TYPE_FREESTYLE_EDGE = 1 << 12,

  DT_TYPE_MPROPCOL_VERT = 1 << 16,
  DT_TYPE_LNOR = 1 << 17,

  DT_TYPE_UV = 1 << 24,
  DT_TYPE_SHARP_FACE = 1 << 25,
  DT_TYPE_FREESTYLE_FACE = 1 << 26,
  DT_TYPE_MLOOPCOL_VERT = 1 << 27,
  DT_TYPE_MPROPCOL_LOOP = 1 << 28,
  DT_TYPE_MLOOPCOL_LOOP = 1 << 29,
  DT_TYPE_VCOL_ALL = (1 << 16) | (1 << 27) | (1 << 28) | (1 << 29),
#define DT_TYPE_MAX 30

  DT_TYPE_VERT_ALL = DT_TYPE_MDEFORMVERT | DT_TYPE_SKIN | DT_TYPE_BWEIGHT_VERT |
                     DT_TYPE_MPROPCOL_VERT | DT_TYPE_MLOOPCOL_VERT,
  DT_TYPE_EDGE_ALL = DT_TYPE_SHARP_EDGE | DT_TYPE_SEAM | DT_TYPE_CREASE | DT_TYPE_BWEIGHT_EDGE |
                     DT_TYPE_FREESTYLE_EDGE,
  DT_TYPE_LOOP_ALL = DT_TYPE_LNOR | DT_TYPE_UV | DT_TYPE_MPROPCOL_LOOP | DT_TYPE_MLOOPCOL_LOOP,
  DT_TYPE_POLY_ALL = DT_TYPE_UV | DT_TYPE_SHARP_FACE | DT_TYPE_FREESTYLE_FACE,
};

#define DT_DATATYPE_IS_VERT(_dt) \
  ELEM(_dt, \
       DT_TYPE_MDEFORMVERT, \
       DT_TYPE_SKIN, \
       DT_TYPE_BWEIGHT_VERT, \
       DT_TYPE_MLOOPCOL_VERT, \
       DT_TYPE_MPROPCOL_VERT)
#define DT_DATATYPE_IS_EDGE(_dt) \
  ELEM(_dt, \
       DT_TYPE_CREASE, \
       DT_TYPE_SHARP_EDGE, \
       DT_TYPE_SEAM, \
       DT_TYPE_BWEIGHT_EDGE, \
       DT_TYPE_FREESTYLE_EDGE)
#define DT_DATATYPE_IS_LOOP(_dt) \
  ELEM(_dt, DT_TYPE_UV, DT_TYPE_LNOR, DT_TYPE_MLOOPCOL_LOOP, DT_TYPE_MPROPCOL_LOOP)
#define DT_DATATYPE_IS_FACE(_dt) ELEM(_dt, DT_TYPE_UV, DT_TYPE_SHARP_FACE, DT_TYPE_FREESTYLE_FACE)

#define DT_DATATYPE_IS_MULTILAYERS(_dt) \
  ELEM(_dt, \
       DT_TYPE_MDEFORMVERT, \
       DT_TYPE_MPROPCOL_VERT, \
       DT_TYPE_MLOOPCOL_VERT, \
       DT_TYPE_MPROPCOL_VERT | DT_TYPE_MLOOPCOL_VERT, \
       DT_TYPE_MPROPCOL_LOOP, \
       DT_TYPE_MLOOPCOL_LOOP, \
       DT_TYPE_MPROPCOL_LOOP | DT_TYPE_MLOOPCOL_LOOP, \
       DT_TYPE_UV)

enum {
  DT_MULTILAYER_INDEX_INVALID = -1,
  DT_MULTILAYER_INDEX_MDEFORMVERT = 0,
  DT_MULTILAYER_INDEX_VCOL_LOOP = 2,
  DT_MULTILAYER_INDEX_UV = 3,
  DT_MULTILAYER_INDEX_VCOL_VERT = 4,
  DT_MULTILAYER_INDEX_MAX = 5,
};

/* Below we keep positive values for real layers idx (generated dynamically). */

/* How to select data layers, for types supporting multi-layers.
 * Here too, some options are highly dependent on type of transferred data! */
enum {
  DT_LAYERS_ACTIVE_SRC = -1,
  DT_LAYERS_ALL_SRC = -2,
  /* Datatype-specific. */
  DT_LAYERS_VGROUP_SRC = 1 << 8,
  DT_LAYERS_VGROUP_SRC_BONE_SELECT = -(DT_LAYERS_VGROUP_SRC | 1),
  DT_LAYERS_VGROUP_SRC_BONE_DEFORM = -(DT_LAYERS_VGROUP_SRC | 2),
  /* Other types-related modes... */
};

/* How to map a source layer to a destination layer, for types supporting multi-layers.
 * NOTE: if no matching layer can be found, it will be created. */
enum {
  DT_LAYERS_ACTIVE_DST = -1, /* Only for DT_LAYERS_FROMSEL_ACTIVE. */
  DT_LAYERS_NAME_DST = -2,
  DT_LAYERS_INDEX_DST = -3,
#if 0 /* TODO */
  DT_LAYERS_CREATE_DST = -4, /* Never replace existing data in dst, always create new layers. */
#endif
};

/* TODO:
 * Add other 'from/to' mapping sources, like e.g. using a UVMap, etc.
 * https://blenderartists.org/t/619105
 *
 * We could also use similar topology mappings inside a same mesh
 * (cf. Campbell's 'select face islands from similar topology' WIP work).
 * Also, users will have to check, whether we can get rid of some modes here,
 * not sure all will be useful!
 */
enum {
  MREMAP_USE_VERT = 1 << 4,
  MREMAP_USE_EDGE = 1 << 5,
  MREMAP_USE_LOOP = 1 << 6,
  MREMAP_USE_POLY = 1 << 7,

  MREMAP_USE_NEAREST = 1 << 8,
  MREMAP_USE_NORPROJ = 1 << 9,
  MREMAP_USE_INTERP = 1 << 10,
  MREMAP_USE_NORMAL = 1 << 11,

  /* ***** Target's vertices ***** */
  MREMAP_MODE_VERT = 1 << 24,
  /* Nearest source vert. */
  MREMAP_MODE_VERT_NEAREST = MREMAP_MODE_VERT | MREMAP_USE_VERT | MREMAP_USE_NEAREST,

  /* Nearest vertex of nearest edge. */
  MREMAP_MODE_VERT_EDGE_NEAREST = MREMAP_MODE_VERT | MREMAP_USE_EDGE | MREMAP_USE_NEAREST,
  /* This one uses two verts of selected edge (weighted interpolation). */
  /* Nearest point on nearest edge. */
  MREMAP_MODE_VERT_EDGEINTERP_NEAREST = MREMAP_MODE_VERT | MREMAP_USE_EDGE | MREMAP_USE_NEAREST |
                                        MREMAP_USE_INTERP,

  /* Nearest vertex of nearest face. */
  MREMAP_MODE_VERT_FACE_NEAREST = MREMAP_MODE_VERT | MREMAP_USE_POLY | MREMAP_USE_NEAREST,
  /* Those two use all verts of selected face (weighted interpolation). */
  /* Nearest point on nearest face. */
  MREMAP_MODE_VERT_POLYINTERP_NEAREST = MREMAP_MODE_VERT | MREMAP_USE_POLY | MREMAP_USE_NEAREST |
                                        MREMAP_USE_INTERP,
  /* Point on nearest face hit by ray from target vertex's normal. */
  MREMAP_MODE_VERT_POLYINTERP_VNORPROJ = MREMAP_MODE_VERT | MREMAP_USE_POLY | MREMAP_USE_NORPROJ |
                                         MREMAP_USE_INTERP,

  /* ***** Target's edges ***** */
  MREMAP_MODE_EDGE = 1 << 25,

  /* Source edge which both vertices are nearest of destination ones. */
  MREMAP_MODE_EDGE_VERT_NEAREST = MREMAP_MODE_EDGE | MREMAP_USE_VERT | MREMAP_USE_NEAREST,

  /* Nearest source edge (using mid-point). */
  MREMAP_MODE_EDGE_NEAREST = MREMAP_MODE_EDGE | MREMAP_USE_EDGE | MREMAP_USE_NEAREST,

  /* Nearest edge of nearest face (using mid-point). */
  MREMAP_MODE_EDGE_POLY_NEAREST = MREMAP_MODE_EDGE | MREMAP_USE_POLY | MREMAP_USE_NEAREST,

  /* Cast a set of rays from along destination edge,
   * interpolating its vertices' normals, and use hit source edges. */
  MREMAP_MODE_EDGE_EDGEINTERP_VNORPROJ = MREMAP_MODE_EDGE | MREMAP_USE_VERT | MREMAP_USE_NORPROJ |
                                         MREMAP_USE_INTERP,

  /* ***** Target's loops ***** */
  /* NOTE: when islands are given to loop mapping func,
   * all loops from the same destination face will always be mapped
   * to loops of source faces within a same island, regardless of mapping mode. */
  MREMAP_MODE_LOOP = 1 << 26,

  /* Best normal-matching loop from nearest vert. */
  MREMAP_MODE_LOOP_NEAREST_LOOPNOR = MREMAP_MODE_LOOP | MREMAP_USE_LOOP | MREMAP_USE_VERT |
                                     MREMAP_USE_NEAREST | MREMAP_USE_NORMAL,
  /* Loop from best normal-matching face from nearest vert. */
  MREMAP_MODE_LOOP_NEAREST_POLYNOR = MREMAP_MODE_LOOP | MREMAP_USE_POLY | MREMAP_USE_VERT |
                                     MREMAP_USE_NEAREST | MREMAP_USE_NORMAL,

  /* Loop from nearest vertex of nearest face. */
  MREMAP_MODE_LOOP_POLY_NEAREST = MREMAP_MODE_LOOP | MREMAP_USE_POLY | MREMAP_USE_NEAREST,
  /* Those two use all verts of selected face (weighted interpolation). */
  /* Nearest point on nearest face. */
  MREMAP_MODE_LOOP_POLYINTERP_NEAREST = MREMAP_MODE_LOOP | MREMAP_USE_POLY | MREMAP_USE_NEAREST |
                                        MREMAP_USE_INTERP,
  /* Point on nearest face hit by ray from target loop's normal. */
  MREMAP_MODE_LOOP_POLYINTERP_LNORPROJ = MREMAP_MODE_LOOP | MREMAP_USE_POLY | MREMAP_USE_NORPROJ |
                                         MREMAP_USE_INTERP,

  /* ***** Target's faces ***** */
  MREMAP_MODE_POLY = 1 << 27,

  /* Nearest source face. */
  MREMAP_MODE_POLY_NEAREST = MREMAP_MODE_POLY | MREMAP_USE_POLY | MREMAP_USE_NEAREST,
  /* Source face from best normal-matching destination face. */
  MREMAP_MODE_POLY_NOR = MREMAP_MODE_POLY | MREMAP_USE_POLY | MREMAP_USE_NORMAL,

  /* Project destination face onto source mesh using its normal,
   * and use interpolation of all intersecting source faces. */
  MREMAP_MODE_POLY_POLYINTERP_PNORPROJ = MREMAP_MODE_POLY | MREMAP_USE_POLY | MREMAP_USE_NORPROJ |
                                         MREMAP_USE_INTERP,

  /* ***** Same topology, applies to all four elements types. ***** */
  MREMAP_MODE_TOPOLOGY = MREMAP_MODE_VERT | MREMAP_MODE_EDGE | MREMAP_MODE_LOOP | MREMAP_MODE_POLY,
};

/**
 * How to filter out some elements (to leave untouched).
 * Note those options are highly dependent on type of transferred data! */
enum {
  CDT_MIX_NOMIX = -1, /* Special case, only used because we abuse 'copy' CD callback. */
  CDT_MIX_TRANSFER = 0,
  CDT_MIX_REPLACE_ABOVE_THRESHOLD = 1,
  CDT_MIX_REPLACE_BELOW_THRESHOLD = 2,
  CDT_MIX_MIX = 16,
  CDT_MIX_ADD = 17,
  CDT_MIX_SUB = 18,
  CDT_MIX_MUL = 19,
  /* Etc. */
};
