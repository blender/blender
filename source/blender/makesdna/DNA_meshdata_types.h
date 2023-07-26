/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name Ordered Selection Storage
 * \{ */

/**
 * Optionally store the order of selected elements.
 * This won't always be set since only some selection operations have an order.
 *
 * Typically accessed from #Mesh.mselect
 */
typedef struct MSelect {
  /** Index in the vertex, edge or polygon array. */
  int index;
  /** #ME_VSEL, #ME_ESEL, #ME_FSEL. */
  int type;
} MSelect;

/** #MSelect.type */
enum {
  ME_VSEL = 0,
  ME_ESEL = 1,
  ME_FSEL = 2,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Loop Tessellation Runtime Data
 * \{ */

/**
 * #MLoopTri's are lightweight triangulation data,
 * for functionality that doesn't support ngons.
 * This is cache data created from (polygons, corner vert, and position arrays).
 * There is no attempt to maintain this data's validity over time,
 * any changes to the underlying mesh invalidate the #MLoopTri array,
 * which will need to be re-calculated.
 *
 * Users normally access this via #Mesh::looptris().
 * In rare cases its calculated directly, with #bke::mesh::looptris_calc.
 *
 * Typical usage includes:
 * - Viewport drawing.
 * - #BVHTree creation.
 * - Physics/collision detection.
 *
 * Storing loop indices (instead of vertex indices) allows us to
 * directly access UVs, vertex-colors as well as vertices.
 * The index of the source polygon is stored as well,
 * giving access to materials and polygon normals.
 *
 * \note This data is runtime only, never written to disk.
 *
 * Usage examples:
 * \code{.c}
 * // access vertex locations.
 * float *vtri_co[3] = {
 *     positions[corner_verts[lt->tri[0]]],
 *     positions[corner_verts[lt->tri[1]]],
 *     positions[corner_verts[lt->tri[2]]],
 * };
 *
 * // access UV coordinates (works for all loop data, vertex colors... etc).
 * float *uvtri_co[3] = {
 *     mloopuv[lt->tri[0]],
 *     mloopuv[lt->tri[1]],
 *     mloopuv[lt->tri[2]],
 * };
 * \endcode
 *
 * #MLoopTri's are allocated in an array, where each polygon's #MLoopTri's are stored contiguously,
 * the number of triangles for each polygon is guaranteed to be the corner count - 2,
 * even for degenerate geometry. See #ME_FACE_TRI_TOT macro.
 *
 * It's also possible to perform a reverse lookup (find all #MLoopTri's for any given face).
 *
 * \code{.c}
 * // loop over all looptri's for a given polygon: i
 * const IndexRange face = faces[i];
 * MLoopTri *lt = &looptri[poly_to_tri_count(i, face.start())];
 * int j, lt_tot = ME_FACE_TRI_TOT(face.size());
 *
 * for (j = 0; j < lt_tot; j++, lt++) {
 *     int vtri[3] = {
 *         corner_verts[lt->tri[0]],
 *         corner_verts[lt->tri[1]],
 *         corner_verts[lt->tri[2]],
 *     };
 *     printf("tri %u %u %u\n", vtri[0], vtri[1], vtri[2]);
 * };
 * \endcode
 *
 * It may also be useful to check whether or not two vertices of a triangle
 * form an edge in the underlying mesh.
 *
 * This can be done by checking the edge of the referenced corner,
 * the winding of the #MLoopTri and the corners's will always match,
 * however the order of vertices in the edge is undefined.
 *
 * \code{.c}
 * // print real edges from an MLoopTri: lt
 * int j, j_next;
 * for (j = 2, j_next = 0; j_next < 3; j = j_next++) {
 *     const int2 &edge = &medge[corner_edges[lt->tri[j]]];
 *     unsigned int tri_edge[2]  = {corner_verts[lt->tri[j]], corner_verts[lt->tri[j_next]]};
 *
 *     if (((edge[0] == tri_edge[0]) && (edge[1] == tri_edge[1])) ||
 *         ((edge[0] == tri_edge[1]) && (edge[1] == tri_edge[0])))
 *     {
 *         printf("real edge found %u %u\n", tri_edge[0], tri_edge[1]);
 *     }
 * }
 * \endcode
 *
 * See #BKE_mesh_looptri_get_real_edges for a utility that does this.
 *
 * \note A #MLoopTri may be in the middle of an ngon and not reference **any** edges.
 */
typedef struct MLoopTri {
  unsigned int tri[3];
} MLoopTri;
#
#
typedef struct MVertTri {
  unsigned int tri[3];
} MVertTri;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Custom Data (Generic)
 * \{ */

/** Custom Data Properties */
typedef struct MFloatProperty {
  float f;
} MFloatProperty;
typedef struct MIntProperty {
  int i;
} MIntProperty;
typedef struct MStringProperty {
  char s[255], s_len;
} MStringProperty;
typedef struct MBoolProperty {
  uint8_t b;
} MBoolProperty;
typedef struct MInt8Property {
  int8_t i;
} MInt8Property;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Custom Data (Vertex)
 * \{ */

/**
 * Vertex group index and weight for #MDeformVert.dw
 */
typedef struct MDeformWeight {
  /** The index for the vertex group, must *always* be unique when in an array. */
  unsigned int def_nr;
  /** Weight between 0.0 and 1.0. */
  float weight;
} MDeformWeight;

/**
 * Stores all of an element's vertex groups, and their weight values.
 */
typedef struct MDeformVert {
  /**
   * Array of weight indices and values.
   * - There must not be any duplicate #def_nr indices.
   * - Groups in the array are unordered.
   * - Indices outside the usable range of groups are ignored.
   */
  struct MDeformWeight *dw;
  /**
   * The length of the #dw array.
   * \note This is not necessarily the same length as the total number of vertex groups.
   * However, generally it isn't larger.
   */
  int totweight;
  /** Flag is only in use as a run-time tag at the moment. */
  int flag;
} MDeformVert;

typedef struct MVertSkin {
  /**
   * Radii of the skin, define how big the generated frames are.
   * Currently only the first two elements are used.
   */
  float radius[3];

  /** #eMVertSkinFlag */
  int flag;
} MVertSkin;

typedef enum eMVertSkinFlag {
  /**
   * Marks a vertex as the edge-graph root, used for calculating rotations for all connected
   * edges (recursively). Also used to choose a root when generating an armature.
   */
  MVERT_SKIN_ROOT = 1,

  /**
   * Marks a branch vertex (vertex with more than two connected edges), so that its neighbors
   * are directly hulled together, rather than the default of generating intermediate frames.
   */
  MVERT_SKIN_LOOSE = 2,
} eMVertSkinFlag;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Custom Data (Loop)
 * \{ */

/**
 * \note While alpha is not currently in the 3D Viewport,
 * this may eventually be added back, keep this value set to 255.
 */
typedef struct MLoopCol {
  unsigned char r, g, b, a;
} MLoopCol;

typedef struct MPropCol {
  float color[4];
} MPropCol;

/** Multi-Resolution loop data. */
typedef struct MDisps {
  /* Strange bug in SDNA: if disps pointer comes first, it fails to see totdisp */
  int totdisp;
  int level;
  float (*disps)[3];

  /**
   * Used for hiding parts of a multires mesh.
   * Essentially the multires equivalent of the mesh ".hide_vert" boolean attribute.
   *
   * \note This is a bitmap, keep in sync with type used in BLI_bitmap.h
   */
  unsigned int *hidden;
} MDisps;

/** Multi-Resolution grid loop data. */
typedef struct GridPaintMask {
  /**
   * The data array contains `grid_size * grid_size` elements.
   * Where `grid_size = (1 << (level - 1)) + 1`.
   */
  float *data;

  /** The maximum multires level associated with this grid. */
  unsigned int level;

  char _pad[4];
} GridPaintMask;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Custom Data (Original Space for Poly, Face)
 * \{ */

/**
 * Original space within a face (similar to UV coordinates),
 * however they are used to determine the original position in a face.
 *
 * Unlike UVs these are not user editable and always start out using a fixed 0-1 range.
 * Currently only used for particle placement.
 */
#
#
typedef struct OrigSpaceFace {
  float uv[4][2];
} OrigSpaceFace;

#
#
typedef struct OrigSpaceLoop {
  float uv[2];
} OrigSpaceLoop;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Custom Data (FreeStyle for Edge, Face)
 * \{ */

typedef struct FreestyleEdge {
  char flag;
} FreestyleEdge;

/** #FreestyleEdge.flag */
enum {
  FREESTYLE_EDGE_MARK = 1,
};

typedef struct FreestyleFace {
  char flag;
} FreestyleFace;

/** #FreestyleFace.flag */
enum {
  FREESTYLE_FACE_MARK = 1,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utility Macros
 * \{ */

/** Number of tri's that make up this polygon once tessellated. */
#define ME_FACE_TRI_TOT(size) (size - 2)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deprecated Structs
 * \{ */

#ifdef DNA_DEPRECATED_ALLOW

/**
 * Mesh Edges.
 *
 * Typically accessed with #Mesh.edges()
 */
typedef struct MEdge {
  /** Un-ordered vertex indices (cannot match). */
  unsigned int v1, v2;
  /** Deprecated edge crease, now located in `edge_crease`, except for file read and write. */
  char crease_legacy;
  /**
   * Deprecated bevel weight storage, now located in #CD_BWEIGHT, except for file read and write.
   */
  char bweight_legacy;
  short flag_legacy;
} MEdge;

/** #MEdge.flag */
enum {
  /** Deprecated selection status. Now stored in ".select_edge" attribute. */
  /*  SELECT = (1 << 0), */
  ME_SEAM = (1 << 2),
  /** Deprecated hide status. Now stored in ".hide_edge" attribute. */
  /*  ME_HIDE = (1 << 4), */
  /** Deprecated loose edge status. Now stored in #Mesh::loose_edges() runtime cache. */
  ME_LOOSEEDGE = (1 << 7),
  /** Deprecated sharp edge status. Now stored in "sharp_edge" attribute. */
  ME_SHARP = (1 << 9),
};

/**
 * Mesh Faces.
 * This only stores the polygon size & flags, the vertex & edge indices are stored in the "corner
 * edges" array.
 *
 * Typically accessed with #Mesh.faces().
 */
typedef struct MPoly {
  /** Offset into loop array and number of loops in the face. */
  int loopstart;
  /** Keep signed since we need to subtract when getting the previous loop. */
  int totloop;
  /** Deprecated material index. Now stored in the "material_index" attribute, but kept for IO. */
  short mat_nr_legacy;
  char flag_legacy, _pad;
} MPoly;

/** #MPoly.flag */
enum {
  /** Deprecated smooth shading status. Now stored reversed in "sharp_face" attribute. */
  ME_SMOOTH = (1 << 0),
  /** Deprecated selection status. Now stored in ".select_poly" attribute. */
  ME_FACE_SEL = (1 << 1),
  /** Deprecated hide status. Now stored in ".hide_poly" attribute. */
  /* ME_HIDE = (1 << 4), */
};

/**
 * UV coordinate for a polygon face & flag for selection & other options.
 * Deprecated, but kept to read old files. UV coordinates are now stored as #CD_PROP_FLOAT2 layers.
 */
typedef struct MLoopUV {
  float uv[2];
  int flag;
} MLoopUV;

/** #MLoopUV.flag */
enum {
  MLOOPUV_EDGESEL = (1 << 0),
  MLOOPUV_VERTSEL = (1 << 1),
  MLOOPUV_PINNED = (1 << 2),
};

/**
 * Deprecated mesh vertex data structure. Now stored with generic attributes.
 */
typedef struct MVert {
  float co_legacy[3];
  /**
   * Deprecated flag for storing hide status and selection, which are now stored in separate
   * generic attributes. Kept for file read and write.
   */
  char flag_legacy;
  /**
   * Deprecated bevel weight storage, now located in #CD_BWEIGHT, except for file read and write.
   */
  char bweight_legacy;
  char _pad[2];
} MVert;

/** #MVert.flag */
enum {
  /** Deprecated selection status. Now stored in ".select_vert" attribute. */
  /*  SELECT = (1 << 0), */
  /** Deprecated hide status. Now stored in ".hide_vert" attribute. */
  ME_HIDE = (1 << 4),
};

/**
 * Mesh Face Corners.
 * Deprecated storage for the vertex of a face corner and the following edge.
 * Replaced by the "corner_verts" and "corner_edges" arrays.
 */
typedef struct MLoop {
  /** Vertex index. */
  unsigned int v;
  /** Edge index into an #MEdge array. */
  unsigned int e;
} MLoop;

#endif

/**
 * Used in Blender pre 2.63, See #Mesh::corner_verts(), #Mesh::faces() for face data stored in the
 * blend file. Use for reading old files and in a handful of cases which should be removed
 * eventually.
 */
typedef struct MFace {
  unsigned int v1, v2, v3, v4;
  short mat_nr;
  /** We keep edcode, for conversion to edges draw flags in old files. */
  char edcode, flag;
} MFace;

/** #MFace.edcode */
enum {
  ME_V1V2 = (1 << 0),
  ME_V2V3 = (1 << 1),
  ME_V3V1 = (1 << 2),
  ME_V3V4 = ME_V3V1,
  ME_V4V1 = (1 << 3),
};

/** Tessellation uv face data. */
typedef struct MTFace {
  float uv[4][2];
} MTFace;

/**
 * Tessellation vertex color data.
 *
 * \note The red and blue are swapped for historical reasons.
 */
typedef struct MCol {
  unsigned char a, r, g, b;
} MCol;

#ifdef DNA_DEPRECATED_ALLOW

/** Old game engine recast navigation data, while unused 2.7x files may contain this. */
typedef struct MRecast {
  int i;
} MRecast;

#endif

/** \} */

#ifdef __cplusplus
}
#endif
