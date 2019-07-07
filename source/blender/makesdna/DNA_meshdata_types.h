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
 */

#ifndef __DNA_MESHDATA_TYPES_H__
#define __DNA_MESHDATA_TYPES_H__

#include "DNA_customdata_types.h"
#include "DNA_listBase.h"

struct Image;

/*tessellation face, see MLoop/MPoly for the real face data*/
typedef struct MFace {
  unsigned int v1, v2, v3, v4;
  short mat_nr;
  /** We keep edcode, for conversion to edges draw flags in old files. */
  char edcode, flag;
} MFace;

typedef struct MEdge {
  unsigned int v1, v2;
  char crease, bweight;
  short flag;
} MEdge;

typedef struct MDeformWeight {
  int def_nr;
  float weight;
} MDeformWeight;

typedef struct MDeformVert {
  struct MDeformWeight *dw;
  int totweight;
  /** Flag only in use for weightpaint now. */
  int flag;
} MDeformVert;

typedef struct MVert {
  float co[3];
  short no[3];
  char flag, bweight;
} MVert;

/** Tessellation vertex color data.
 * at the moment alpha is abused for vertex painting and not used for transparency,
 * note that red and blue are swapped
 */
typedef struct MCol {
  unsigned char a, r, g, b;
} MCol;

/* new face structure, replaces MFace, which is now only used for storing tessellations.*/
typedef struct MPoly {
  /* offset into loop array and number of loops in the face */
  int loopstart;
  /** Keep signed since we need to subtract when getting the previous loop. */
  int totloop;
  short mat_nr;
  char flag, _pad;
} MPoly;

/* the e here is because we want to move away from relying on edge hashes.*/
typedef struct MLoop {
  /** Vertex index. */
  unsigned int v;
  /** Edge index. */
  unsigned int e;
} MLoop;

/**
 * #MLoopTri's are lightweight triangulation data,
 * for functionality that doesn't support ngons (#MPoly).
 * This is cache data created from (#MPoly, #MLoop & #MVert arrays).
 * There is no attempt to maintain this data's validity over time,
 * any changes to the underlying mesh invalidate the #MLoopTri array,
 * which will need to be re-calculated.
 *
 * Users normally access this via #BKE_mesh_runtime_looptri_ensure.
 * In rare cases its calculated directly, with #BKE_mesh_recalc_looptri.
 *
 * Typical usage includes:
 * - OpenGL drawing.
 * - #BVHTree creation.
 * - Physics/collision detection.
 *
 * Storing loop indices (instead of vertex indices) allows us to
 * directly access UV's, vertex-colors as well as vertices.
 * The index of the source polygon is stored as well,
 * giving access to materials and polygon normals.
 *
 * \note This data is runtime only, never written to disk.
 *
 * Usage examples:
 * \code{.c}
 * // access original material.
 * short mat_nr = mpoly[lt->poly].mat_nr;
 *
 * // access vertex locations.
 * float *vtri_co[3] = {
 *     mvert[mloop[lt->tri[0]].v].co,
 *     mvert[mloop[lt->tri[1]].v].co,
 *     mvert[mloop[lt->tri[2]].v].co,
 * };
 *
 * // access UV coordinates (works for all loop data, vertex colors... etc).
 * float *uvtri_co[3] = {
 *     mloopuv[lt->tri[0]].uv,
 *     mloopuv[lt->tri[1]].uv,
 *     mloopuv[lt->tri[2]].uv,
 * };
 * \endcode
 *
 * #MLoopTri's are allocated in an array, where each polygon's #MLoopTri's are stored contiguously,
 * the number of triangles for each polygon is guaranteed to be (#MPoly.totloop - 2),
 * even for degenerate geometry. See #ME_POLY_TRI_TOT macro.
 *
 * It's also possible to perform a reverse lookup (find all #MLoopTri's for any given #MPoly).
 *
 * \code{.c}
 * // loop over all looptri's for a given polygon: i
 * MPoly *mp = &mpoly[i];
 * MLoopTri *lt = &looptri[poly_to_tri_count(i, mp->loopstart)];
 * int j, lt_tot = ME_POLY_TRI_TOT(mp);
 *
 * for (j = 0; j < lt_tot; j++, lt++) {
 *     unsigned int vtri[3] = {
 *         mloop[lt->tri[0]].v,
 *         mloop[lt->tri[1]].v,
 *         mloop[lt->tri[2]].v,
 *     };
 *     printf("tri %u %u %u\n", vtri[0], vtri[1], vtri[2]);
 * };
 * \endcode
 *
 * It may also be useful to check whether or not two vertices of a triangle
 * form an edge in the underlying mesh.
 *
 * This can be done by checking the edge of the referenced loop (#MLoop.e),
 * the winding of the #MLoopTri and the #MLoop's will always match,
 * however the order of vertices in the edge is undefined.
 *
 * \code{.c}
 * // print real edges from an MLoopTri: lt
 * int j, j_next;
 * for (j = 2, j_next = 0; j_next < 3; j = j_next++) {
 *     MEdge *ed = &medge[mloop[lt->tri[j]].e];
 *     unsigned int tri_edge[2]  = {mloop[lt->tri[j]].v, mloop[lt->tri[j_next]].v};
 *
 *     if (((ed->v1 == tri_edge[0]) && (ed->v2 == tri_edge[1])) ||
 *         ((ed->v1 == tri_edge[1]) && (ed->v2 == tri_edge[0])))
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
  unsigned int poly;
} MLoopTri;
#
#
typedef struct MVertTri {
  unsigned int tri[3];
} MVertTri;

// typedef struct MTexPoly {
//  void *_pad;
//} MTexPoly;

typedef struct MLoopUV {
  float uv[2];
  int flag;
} MLoopUV;

/*mloopuv->flag*/
enum {
  MLOOPUV_EDGESEL = (1 << 0),
  MLOOPUV_VERTSEL = (1 << 1),
  MLOOPUV_PINNED = (1 << 2),
};

/**
 * at the moment alpha is abused for vertex painting,
 * otherwise it should _always_ be initialized to 255
 * Mostly its not used for transparency...
 * (except for blender-internal rendering, see [#34096]).
 *
 * \note red and blue are _not_ swapped, as they are with #MCol
 */
typedef struct MLoopCol {
  unsigned char r, g, b, a;
} MLoopCol;

#define MESH_MLOOPCOL_FROM_MCOL(_mloopcol, _mcol) \
  { \
    MLoopCol *mloopcol__tmp = _mloopcol; \
    const MCol *mcol__tmp = _mcol; \
    mloopcol__tmp->r = mcol__tmp->b; \
    mloopcol__tmp->g = mcol__tmp->g; \
    mloopcol__tmp->b = mcol__tmp->r; \
    mloopcol__tmp->a = mcol__tmp->a; \
  } \
  (void)0

#define MESH_MLOOPCOL_TO_MCOL(_mloopcol, _mcol) \
  { \
    const MLoopCol *mloopcol__tmp = _mloopcol; \
    MCol *mcol__tmp = _mcol; \
    mcol__tmp->b = mloopcol__tmp->r; \
    mcol__tmp->g = mloopcol__tmp->g; \
    mcol__tmp->r = mloopcol__tmp->b; \
    mcol__tmp->a = mloopcol__tmp->a; \
  } \
  (void)0

typedef struct MSelect {
  int index;
  /** ME_VSEL/ME_ESEL/ME_FSEL. */
  int type;
} MSelect;

/*tessellation uv face data*/
typedef struct MTFace {
  float uv[4][2];
} MTFace;

/*Custom Data Properties*/
typedef struct MFloatProperty {
  float f;
} MFloatProperty;
typedef struct MIntProperty {
  int i;
} MIntProperty;
typedef struct MStringProperty {
  char s[255], s_len;
} MStringProperty;

typedef struct OrigSpaceFace {
  float uv[4][2];
} OrigSpaceFace;

typedef struct OrigSpaceLoop {
  float uv[2];
} OrigSpaceLoop;

typedef struct MDisps {
  /* Strange bug in SDNA: if disps pointer comes first, it fails to see totdisp */
  int totdisp;
  int level;
  float (*disps)[3];

  /**
   * Used for hiding parts of a multires mesh.
   * Essentially the multires equivalent of MVert.flag's ME_HIDE bit.
   *
   * \note This is a bitmap, keep in sync with type used in BLI_bitmap.h
   */
  unsigned int *hidden;
} MDisps;

/** Multires structs kept for compatibility with old files. */
typedef struct MultiresCol {
  float a, r, g, b;
} MultiresCol;

typedef struct MultiresColFace {
  /* vertex colors */
  MultiresCol col[4];
} MultiresColFace;

typedef struct MultiresFace {
  unsigned int v[4];
  unsigned int mid;
  char flag, mat_nr, _pad[2];
} MultiresFace;

typedef struct MultiresEdge {
  unsigned int v[2];
  unsigned int mid;
} MultiresEdge;

typedef struct MultiresLevel {
  struct MultiresLevel *next, *prev;

  MultiresFace *faces;
  MultiresColFace *colfaces;
  MultiresEdge *edges;

  unsigned int totvert, totface, totedge;
  char _pad[4];

  /* Kept for compatibility with even older files */
  MVert *verts;
} MultiresLevel;

typedef struct Multires {
  ListBase levels;
  MVert *verts;

  unsigned char level_count, current, newlvl, edgelvl, pinlvl, renderlvl;
  unsigned char use_col, flag;

  /* Special level 1 data that cannot be modified from other levels */
  CustomData vdata;
  CustomData fdata;
  short *edge_flags;
  char *edge_creases;
} Multires;

/* End Multires */

typedef struct MRecast {
  int i;
} MRecast;

typedef struct GridPaintMask {
  /* The data array contains gridsize*gridsize elements */
  float *data;

  /* The maximum multires level associated with this grid */
  unsigned int level;

  char _pad[4];
} GridPaintMask;

typedef enum eMVertSkinFlag {
  /** Marks a vertex as the edge-graph root, used for calculating rotations for all connected
   * edges (recursively). Also used to choose a root when generating an armature.
   */
  MVERT_SKIN_ROOT = 1,

  /** Marks a branch vertex (vertex with more than two connected edges), so that it's neighbors
   * are directly hulled together, rather than the default of generating intermediate frames.
   */
  MVERT_SKIN_LOOSE = 2,
} eMVertSkinFlag;

typedef struct MVertSkin {
  /* Radii of the skin, define how big the generated frames are.
   * Currently only the first two elements are used. */
  float radius[3];

  /* eMVertSkinFlag */
  int flag;
} MVertSkin;

typedef struct FreestyleEdge {
  char flag;
  char _pad[3];
} FreestyleEdge;

/* FreestyleEdge->flag */
enum {
  FREESTYLE_EDGE_MARK = 1,
};

typedef struct FreestyleFace {
  char flag;
  char _pad[3];
} FreestyleFace;

/* FreestyleFace->flag */
enum {
  FREESTYLE_FACE_MARK = 1,
};

/* mvert->flag */
enum {
  /*  SELECT              = (1 << 0), */
  ME_VERT_TMP_TAG = (1 << 2),
  ME_HIDE = (1 << 4),
  ME_VERT_FACEDOT = (1 << 5),
  /*  ME_VERT_MERGED      = (1 << 6), */
  ME_VERT_PBVH_UPDATE = (1 << 7),
};

/* medge->flag */
enum {
  /*  SELECT              = (1 << 0), */
  ME_EDGEDRAW = (1 << 1),
  ME_SEAM = (1 << 2),
  /*  ME_HIDE             = (1 << 4), */
  ME_EDGERENDER = (1 << 5),
  ME_LOOSEEDGE = (1 << 7),
  ME_EDGE_TMP_TAG = (1 << 8),
  ME_SHARP = (1 << 9), /* only reason this flag remains a 'short' */
};

/* puno = vertexnormal (mface) */
enum {
  ME_PROJXY = (1 << 4),
  ME_PROJXZ = (1 << 5),
  ME_PROJYZ = (1 << 6),
};

/* edcode (mface) */
enum {
  ME_V1V2 = (1 << 0),
  ME_V2V3 = (1 << 1),
  ME_V3V1 = (1 << 2),
  ME_V3V4 = ME_V3V1,
  ME_V4V1 = (1 << 3),
};

/* flag (mface) */
enum {
  ME_SMOOTH = (1 << 0),
  ME_FACE_SEL = (1 << 1),
  /*  ME_HIDE     = (1 << 4), */
};

#define ME_POLY_LOOP_PREV(mloop, mp, i) \
  (&(mloop)[(mp)->loopstart + (((i) + (mp)->totloop - 1) % (mp)->totloop)])
#define ME_POLY_LOOP_NEXT(mloop, mp, i) (&(mloop)[(mp)->loopstart + (((i) + 1) % (mp)->totloop)])

/* number of tri's that make up this polygon once tessellated */
#define ME_POLY_TRI_TOT(mp) ((mp)->totloop - 2)

/**
 * Check out-of-bounds material, note that this is nearly always prevented,
 * yet its still possible in rare cases.
 * So usage such as array lookup needs to check.
 */
#define ME_MAT_NR_TEST(mat_nr, totmat) \
  (CHECK_TYPE_ANY(mat_nr, short, const short), \
   CHECK_TYPE_ANY(totmat, short, const short), \
   (LIKELY(mat_nr < totmat) ? mat_nr : 0))

/* mselect->type */
enum {
  ME_VSEL = 0,
  ME_ESEL = 1,
  ME_FSEL = 2,
};

#endif /* __DNA_MESHDATA_TYPES_H__ */
