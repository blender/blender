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

#ifndef __DNA_MESH_TYPES_H__
#define __DNA_MESH_TYPES_H__

#include "DNA_defs.h"
#include "DNA_ID.h"
#include "DNA_customdata_types.h"

struct AnimData;
struct Ipo;
struct Key;
struct LinkNode;
struct MCol;
struct MEdge;
struct MFace;
struct MLoop;
struct MLoopCol;
struct MLoopTri;
struct MLoopUV;
struct MPoly;
struct MTexPoly;
struct MVert;
struct Material;
struct Mesh;
struct Multires;
struct SubdivCCG;

#
#
typedef struct EditMeshData {
  /** when set, \a vertexNos, polyNos are lazy initialized */
  const float (*vertexCos)[3];

  /** lazy initialize (when \a vertexCos is set) */
  float const (*vertexNos)[3];
  float const (*polyNos)[3];
  /** also lazy init but dont depend on \a vertexCos */
  const float (*polyCos)[3];
} EditMeshData;

/**
 * \warning Typical access is done via
 * #BKE_mesh_runtime_looptri_ensure, #BKE_mesh_runtime_looptri_len.
 */
struct MLoopTri_Store {
  /* WARNING! swapping between array (ready-to-be-used data) and array_wip
   * (where data is actually computed)
   * shall always be protected by same lock as one used for looptris computing. */
  struct MLoopTri *array, *array_wip;
  int len;
  int len_alloc;
};

/* not saved in file! */
typedef struct Mesh_Runtime {
  struct EditMeshData *edit_data;
  void *batch_cache;

  struct SubdivCCG *subdiv_ccg;
  void *_pad1;
  int subdiv_ccg_tot_level;
  char _pad2[4];

  int64_t cd_dirty_vert;
  int64_t cd_dirty_edge;
  int64_t cd_dirty_loop;
  int64_t cd_dirty_poly;

  struct MLoopTri_Store looptris;

  /** 'BVHCache', for 'BKE_bvhutil.c' */
  struct LinkNode *bvh_cache;

  /** Non-manifold boundary data for Shrinkwrap Target Project. */
  struct ShrinkwrapBoundaryData *shrinkwrap_data;

  /** Set by modifier stack if only deformed from original. */
  char deformed_only;
  /**
   * Copied from edit-mesh (hint, draw with editmesh data).
   * In the future we may leave the mesh-data empty
   * since its not needed if we can use edit-mesh data. */
  char is_original;
  char _pad[6];
} Mesh_Runtime;

typedef struct Mesh {
  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  struct BoundBox *bb;

  /** Old animation system, deprecated for 2.5. */
  struct Ipo *ipo DNA_DEPRECATED;
  struct Key *key;
  struct Material **mat;
  struct MSelect *mselect;

  /* BMESH ONLY */
  /*new face structures*/
  struct MPoly *mpoly;
  struct MLoop *mloop;
  struct MLoopUV *mloopuv;
  struct MLoopCol *mloopcol;
  /* END BMESH ONLY */

  /* mface stores the tessellation (triangulation) of the mesh,
   * real faces are now stored in nface.*/
  /** Array of mesh object mode faces for tessellation. */
  struct MFace *mface;
  /** Store tessellation face UV's and texture here. */
  struct MTFace *mtface;
  /** Deprecated, use mtface. */
  struct TFace *tface DNA_DEPRECATED;
  /** Array of verts. */
  struct MVert *mvert;
  /** Array of edges. */
  struct MEdge *medge;
  /** Deformgroup vertices. */
  struct MDeformVert *dvert;

  /* array of colors for the tessellated faces, must be number of tessellated
   * faces * 4 in length */
  struct MCol *mcol;
  struct Mesh *texcomesh;

  /* When the object is available, the preferred access method is: BKE_editmesh_from_object(ob) */
  /** Not saved in file!. */
  struct BMEditMesh *edit_mesh;

  struct CustomData vdata, edata, fdata;

  /* BMESH ONLY */
  struct CustomData pdata, ldata;
  /* END BMESH ONLY */

  int totvert, totedge, totface, totselect;

  /* BMESH ONLY */
  int totpoly, totloop;
  /* END BMESH ONLY */

  /* the last selected vertex/edge/face are used for the active face however
   * this means the active face must always be selected, this is to keep track
   * of the last selected face and is similar to the old active face flag where
   * the face does not need to be selected, -1 is inactive */
  int act_face;

  /* texture space, copied as one block in editobject.c */
  float loc[3];
  float size[3];
  float rot[3];

  short texflag, flag;
  float smoothresh;

  /* customdata flag, for bevel-weight and crease, which are now optional */
  char cd_flag, _pad;

  char subdiv DNA_DEPRECATED, subdivr DNA_DEPRECATED;
  /** Only kept for backwards compat, not used anymore. */
  char subsurftype DNA_DEPRECATED;
  char editflag;

  short totcol;

  /** Deprecated multiresolution modeling data, only keep for loading old files. */
  struct Multires *mr DNA_DEPRECATED;
  void *_pad1;

  Mesh_Runtime runtime;
} Mesh;

/* deprecated by MTFace, only here for file reading */
#ifdef DNA_DEPRECATED
typedef struct TFace {
  /** The faces image for the active UVLayer. */
  void *tpage;
  float uv[4][2];
  unsigned int col[4];
  char flag, transp;
  short mode, tile, unwrap;
} TFace;
#endif

/* **************** MESH ********************* */

/* texflag */
enum {
  ME_AUTOSPACE = 1,
};

/* me->editflag */
enum {
  ME_EDIT_MIRROR_X = 1 << 0,
  ME_EDIT_MIRROR_Y = 1 << 1, /* unused so far */
  ME_EDIT_MIRROR_Z = 1 << 2, /* unused so far */

  ME_EDIT_PAINT_FACE_SEL = 1 << 3,
  ME_EDIT_MIRROR_TOPO = 1 << 4,
  ME_EDIT_PAINT_VERT_SEL = 1 << 5,
};

/* we cant have both flags enabled at once,
 * flags defined in DNA_scene_types.h */
#define ME_EDIT_PAINT_SEL_MODE(_me) \
  ((_me->editflag & ME_EDIT_PAINT_FACE_SEL) ? \
       SCE_SELECT_FACE : \
       (_me->editflag & ME_EDIT_PAINT_VERT_SEL) ? SCE_SELECT_VERTEX : 0)

/* me->flag */
enum {
  ME_FLAG_UNUSED_0 = 1 << 0,     /* cleared */
  ME_FLAG_UNUSED_1 = 1 << 1,     /* cleared */
  ME_FLAG_DEPRECATED_2 = 1 << 2, /* deprecated */
  ME_FLAG_UNUSED_3 = 1 << 3,     /* cleared */
  ME_FLAG_UNUSED_4 = 1 << 4,     /* cleared */
  ME_AUTOSMOOTH = 1 << 5,
  ME_FLAG_UNUSED_6 = 1 << 6, /* cleared */
  ME_FLAG_UNUSED_7 = 1 << 7, /* cleared */
  ME_FLAG_UNUSED_8 = 1 << 8, /* cleared */
  ME_DS_EXPAND = 1 << 9,
  ME_SCULPT_DYNAMIC_TOPOLOGY = 1 << 10,
};

/* me->cd_flag */
enum {
  ME_CDFLAG_VERT_BWEIGHT = 1 << 0,
  ME_CDFLAG_EDGE_BWEIGHT = 1 << 1,
  ME_CDFLAG_EDGE_CREASE = 1 << 2,
};

/* Subsurf Type */
enum {
  ME_CC_SUBSURF = 0,
  ME_SIMPLE_SUBSURF = 1,
};

#define MESH_MAX_VERTS 2000000000L

/* this is so we can save bmesh files that load in trunk, ignoring NGons
 * will eventually be removed */

/* enable this so meshes get tessfaces calculated by default */
/* #define USE_TESSFACE_DEFAULT */

#endif
