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

#pragma once

#include "DNA_ID.h"
#include "DNA_customdata_types.h"
#include "DNA_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AnimData;
struct BVHCache;
struct Ipo;
struct Key;
struct MCol;
struct MEdge;
struct MFace;
struct MLoop;
struct MLoopCol;
struct MLoopTri;
struct MLoopUV;
struct MPoly;
struct MVert;
struct Material;
struct Mesh;
struct SubdivCCG;

#
#
typedef struct EditMeshData {
  /** when set, \a vertexNos, polyNos are lazy initialized */
  const float (*vertexCos)[3];

  /** lazy initialize (when \a vertexCos is set) */
  float const (*vertexNos)[3];
  float const (*polyNos)[3];
  /** also lazy init but don't depend on \a vertexCos */
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

/** Runtime data, not saved in files. */
typedef struct Mesh_Runtime {
  /* Evaluated mesh for objects which do not have effective modifiers.
   * This mesh is used as a result of modifier stack evaluation.
   * Since modifier stack evaluation is threaded on object level we need some synchronization. */
  struct Mesh *mesh_eval;
  void *eval_mutex;

  /** Needed to ensure some thread-safety during render data pre-processing. */
  void *render_mutex;

  /** Lazily initialized SoA data from the #edit_mesh field in #Mesh. */
  struct EditMeshData *edit_data;

  /**
   * Data used to efficiently draw the mesh in the viewport, especially useful when
   * the same mesh is used in many objects or instances. See `draw_cache_impl_mesh.c`.
   */
  void *batch_cache;

  /** Cache for derived triangulation of the mesh. */
  struct MLoopTri_Store looptris;

  /** Cache for BVH trees generated for the mesh. Defined in 'BKE_bvhutil.c' */
  struct BVHCache *bvh_cache;

  /** Cache of non-manifold boundary data for Shrinkwrap Target Project. */
  struct ShrinkwrapBoundaryData *shrinkwrap_data;

  /** Needed in case we need to lazily initialize the mesh. */
  CustomData_MeshMasks cd_mask_extra;

  struct SubdivCCG *subdiv_ccg;
  int subdiv_ccg_tot_level;

  /** Set by modifier stack if only deformed from original. */
  char deformed_only;
  /**
   * Copied from edit-mesh (hint, draw with edit-mesh data when true).
   *
   * Modifiers that edit the mesh data in-place must set this to false
   * (most #eModifierTypeType_NonGeometrical modifiers). Otherwise the edit-mesh
   * data will be used for drawing, missing changes from modifiers. See T79517.
   */
  char is_original;

  /** #eMeshWrapperType and others. */
  char wrapper_type;
  /**
   * A type mask from wrapper_type,
   * in case there are differences in finalizing logic between types.
   */
  char wrapper_type_finalize;

  /**
   * Used to mark when derived data needs to be recalculated for a certain layer.
   * Currently only normals.
   */

  int64_t cd_dirty_vert;
  int64_t cd_dirty_edge;
  int64_t cd_dirty_loop;
  int64_t cd_dirty_poly;

  /**
   * Settings for lazily evaluating the subdivision on the CPU if needed. These are
   * set in the modifier when GPU subdivision can be performed.
   */
  char subsurf_apply_render;
  char subsurf_use_optimal_display;
  char _pad[2];
  int subsurf_resolution;

} Mesh_Runtime;

typedef struct Mesh {
  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  /** Old animation system, deprecated for 2.5. */
  struct Ipo *ipo DNA_DEPRECATED;
  struct Key *key;

  /**
   * An array of materials, with length #totcol. These can be overridden by material slots
   * on #Object. Indices in #MPoly.mat_nr control which material is used for every face.
   */
  struct Material **mat;

  /**
   * Array of vertices. Edges and faces are defined by indices into this array.
   * \note This pointer is for convenient access to the #CD_MVERT layer in #vdata.
   */
  struct MVert *mvert;
  /**
   * Array of edges, containing vertex indices. For simple triangle or quad meshes, edges could be
   * calculated from the #MPoly and #MLoop arrays, however, edges need to be stored explicitly to
   * edge domain attributes and to support loose edges that aren't connected to faces.
   * \note This pointer is for convenient access to the #CD_MEDGE layer in #edata.
   */
  struct MEdge *medge;
  /**
   * Face topology storage of the size and offset of each face's section of the #mloop face corner
   * array. Also stores various flags and the `material_index` attribute.
   * \note This pointer is for convenient access to the #CD_MPOLY layer in #pdata.
   */
  struct MPoly *mpoly;
  /**
   * The vertex and edge index at each face corner.
   * \note This pointer is for convenient access to the #CD_MLOOP layer in #ldata.
   */
  struct MLoop *mloop;

  /** The number of vertices (#MVert) in the mesh, and the size of #vdata. */
  int totvert;
  /** The number of edges (#MEdge) in the mesh, and the size of #edata. */
  int totedge;
  /** The number of polygons/faces (#MPoly) in the mesh, and the size of #pdata. */
  int totpoly;
  /** The number of face corners (#MLoop) in the mesh, and the size of #ldata. */
  int totloop;

  CustomData vdata, edata, pdata, ldata;

  /** "Vertex group" vertices. */
  struct MDeformVert *dvert;
  /**
   * List of vertex group (#bDeformGroup) names and flags only. Actual weights are stored in dvert.
   * \note This pointer is for convenient access to the #CD_MDEFORMVERT layer in #vdata.
   */
  ListBase vertex_group_names;
  /** The active index in the #vertex_group_names list. */
  int vertex_group_active_index;

  /**
   * The index of the active attribute in the UI. The attribute list is a combination of the
   * generic type attributes from vertex, edge, face, and corner custom data.
   */
  int attributes_active_index;

  /**
   * 2D vector data used for UVs. "UV" data can also be stored as generic attributes in #ldata.
   * \note This pointer is for convenient access to the #CD_MLOOPUV layer in #ldata.
   */
  struct MLoopUV *mloopuv;
  /**
   * The active vertex corner color layer, if it exists. Also called "Vertex Color" in Blender's
   * UI, even though it is stored per face corner.
   * \note This pointer is for convenient access to the #CD_MLOOPCOL layer in #ldata.
   */
  struct MLoopCol *mloopcol;

  /**
   * Runtime storage of the edit mode mesh. If it exists, it generally has the most up-to-date
   * information about the mesh.
   * \note When the object is available, the preferred access method is #BKE_editmesh_from_object.
   */
  struct BMEditMesh *edit_mesh;

  /**
   * This array represents the selection order when the user manually picks elements in edit-mode,
   * some tools take advantage of this information. All elements in this array are expected to be
   * selected, see #BKE_mesh_mselect_validate which ensures this. For procedurally created meshes,
   * this is generally empty (selections are stored as boolean attributes in the corresponding
   * custom data).
   */
  struct MSelect *mselect;

  /** The length of the #mselect array. */
  int totselect;

  /**
   * In most cases the last selected element (see #mselect) represents the active element.
   * For faces we make an exception and store the active face separately so it can be active
   * even when no faces are selected. This is done to prevent flickering in the material properties
   * and UV Editor which base the content they display on the current material which is controlled
   * by the active face.
   *
   * \note This is mainly stored for use in edit-mode.
   */
  int act_face;

  /**
   * An optional mesh owned elsewhere (by #Main) that can be used to override
   * the texture space #loc and #size.
   * \note Vertex indices should be aligned for this to work usefully.
   */
  struct Mesh *texcomesh;

  /** Texture space location and size, used for procedural coordinates when rendering. */
  float loc[3];
  float size[3];
  char texflag;

  /** Various flags used when editing the mesh. */
  char editflag;
  /** Mostly more flags used when editing or displaying the mesh. */
  short flag;

  /**
   * The angle for auto smooth in radians. `M_PI` (180 degrees) causes all edges to be smooth.
   */
  float smoothresh;

  /**
   * Flag for choosing whether or not so store bevel weight and crease as custom data layers in the
   * edit mesh (they are always stored in #MVert and #MEdge currently). In the future, this data
   * may be stored as generic named attributes (see T89054 and T93602).
   */
  char cd_flag;

  /**
   * User-defined symmetry flag (#eMeshSymmetryType) that causes editing operations to maintain
   * symmetrical geometry. Supported by operations such as transform and weight-painting.
   */
  char symmetry;

  /** The length of the #mat array. */
  short totcol;

  /** Choice between different remesh methods in the UI. */
  char remesh_mode;

  char subdiv DNA_DEPRECATED;
  char subdivr DNA_DEPRECATED;
  char subsurftype DNA_DEPRECATED;

  /**
   * Deprecated. Store of runtime data for tessellation face UVs and texture.
   *
   * \note This would be marked deprecated, however the particles still use this at run-time
   * for placing particles on the mesh (something which should be eventually upgraded).
   */
  struct MTFace *mtface;
  /** Deprecated, use mtface. */
  struct TFace *tface DNA_DEPRECATED;

  /* Deprecated. Array of colors for the tessellated faces, must be number of tessellated
   * faces * 4 in length. This is stored in #fdata, and deprecated. */
  struct MCol *mcol;

  /**
   * Deprecated face storage (quads & triangles only);
   * faces are now pointed to by #Mesh.mpoly and #Mesh.mloop.
   *
   * \note This would be marked deprecated, however the particles still use this at run-time
   * for placing particles on the mesh (something which should be eventually upgraded).
   */
  struct MFace *mface;
  /* Deprecated storage of old faces (only triangles or quads). */
  CustomData fdata;
  /* Deprecated size of #fdata. */
  int totface;

  /** Per-mesh settings for voxel remesh. */
  float remesh_voxel_size;
  float remesh_voxel_adaptivity;

  int face_sets_color_seed;
  /* Stores the initial Face Set to be rendered white. This way the overlay can be enabled by
   * default and Face Sets can be used without affecting the color of the mesh. */
  int face_sets_color_default;

  char _pad1[4];

  void *_pad2;

  Mesh_Runtime runtime;
} Mesh;

/* deprecated by MTFace, only here for file reading */
#ifdef DNA_DEPRECATED_ALLOW
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

/** #Mesh_Runtime.wrapper_type */
typedef enum eMeshWrapperType {
  /** Use mesh data (#Mesh.mvert, #Mesh.medge, #Mesh.mloop, #Mesh.mpoly). */
  ME_WRAPPER_TYPE_MDATA = 0,
  /** Use edit-mesh data (#Mesh.edit_mesh, #Mesh_Runtime.edit_data). */
  ME_WRAPPER_TYPE_BMESH = 1,
  /** Use subdivision mesh data (#Mesh_Runtime.mesh_eval). */
  ME_WRAPPER_TYPE_SUBD = 2,
} eMeshWrapperType;

/** #Mesh.texflag */
enum {
  ME_AUTOSPACE = 1,
  ME_AUTOSPACE_EVALUATED = 2,
};

/** #Mesh.editflag */
enum {
  ME_EDIT_MIRROR_VERTEX_GROUPS = 1 << 0,
  ME_EDIT_MIRROR_Y = 1 << 1, /* unused so far */
  ME_EDIT_MIRROR_Z = 1 << 2, /* unused so far */

  ME_EDIT_PAINT_FACE_SEL = 1 << 3,
  ME_EDIT_MIRROR_TOPO = 1 << 4,
  ME_EDIT_PAINT_VERT_SEL = 1 << 5,
};

/* Helper macro to see if vertex group X mirror is on. */
#define ME_USING_MIRROR_X_VERTEX_GROUPS(_me) \
  (((_me)->editflag & ME_EDIT_MIRROR_VERTEX_GROUPS) && ((_me)->symmetry & ME_SYMMETRY_X))

/* We can't have both flags enabled at once,
 * flags defined in DNA_scene_types.h */
#define ME_EDIT_PAINT_SEL_MODE(_me) \
  (((_me)->editflag & ME_EDIT_PAINT_FACE_SEL) ? SCE_SELECT_FACE : \
   ((_me)->editflag & ME_EDIT_PAINT_VERT_SEL) ? SCE_SELECT_VERTEX : \
                                                0)

/** #Mesh.flag */
enum {
  ME_FLAG_UNUSED_0 = 1 << 0,     /* cleared */
  ME_FLAG_UNUSED_1 = 1 << 1,     /* cleared */
  ME_FLAG_DEPRECATED_2 = 1 << 2, /* deprecated */
  ME_FLAG_UNUSED_3 = 1 << 3,     /* cleared */
  ME_FLAG_UNUSED_4 = 1 << 4,     /* cleared */
  ME_AUTOSMOOTH = 1 << 5,
  ME_FLAG_UNUSED_6 = 1 << 6, /* cleared */
  ME_FLAG_UNUSED_7 = 1 << 7, /* cleared */
  ME_REMESH_REPROJECT_VERTEX_COLORS = 1 << 8,
  ME_DS_EXPAND = 1 << 9,
  ME_SCULPT_DYNAMIC_TOPOLOGY = 1 << 10,
  ME_FLAG_UNUSED_8 = 1 << 11, /* cleared */
  ME_REMESH_REPROJECT_PAINT_MASK = 1 << 12,
  ME_REMESH_FIX_POLES = 1 << 13,
  ME_REMESH_REPROJECT_VOLUME = 1 << 14,
  ME_REMESH_REPROJECT_SCULPT_FACE_SETS = 1 << 15,
};

/** #Mesh.cd_flag */
enum {
  ME_CDFLAG_VERT_BWEIGHT = 1 << 0,
  ME_CDFLAG_EDGE_BWEIGHT = 1 << 1,
  ME_CDFLAG_EDGE_CREASE = 1 << 2,
};

/** #Mesh.remesh_mode */
enum {
  REMESH_VOXEL = 0,
  REMESH_QUAD = 1,
};

/** #SubsurfModifierData.subdivType */
enum {
  ME_CC_SUBSURF = 0,
  ME_SIMPLE_SUBSURF = 1,
};

/** #Mesh.symmetry */
typedef enum eMeshSymmetryType {
  ME_SYMMETRY_X = 1 << 0,
  ME_SYMMETRY_Y = 1 << 1,
  ME_SYMMETRY_Z = 1 << 2,
} eMeshSymmetryType;

#define MESH_MAX_VERTS 2000000000L

#ifdef __cplusplus
}
#endif
