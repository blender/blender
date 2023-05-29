/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_customdata_types.h"
#include "DNA_defs.h"
#include "DNA_meshdata_types.h"
#include "DNA_session_uuid_types.h"

/** Workaround to forward-declare C++ type in C header. */
#ifdef __cplusplus

#  include "BLI_bounds_types.hh"
#  include "BLI_math_vector_types.hh"
#  include "BLI_offset_indices.hh"

namespace blender {
template<typename T> class Span;
template<typename T> class MutableSpan;
namespace bke {
struct MeshRuntime;
class AttributeAccessor;
class MutableAttributeAccessor;
struct LooseVertCache;
struct LooseEdgeCache;
}  // namespace bke
}  // namespace blender
using MeshRuntimeHandle = blender::bke::MeshRuntime;
#else
typedef struct MeshRuntimeHandle MeshRuntimeHandle;
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct AnimData;
struct Ipo;
struct Key;
struct MCol;
struct MEdge;
struct MFace;
struct MLoopTri;
struct Material;

typedef struct Mesh {
  DNA_DEFINE_CXX_METHODS(Mesh)

  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  /** Old animation system, deprecated for 2.5. */
  struct Ipo *ipo DNA_DEPRECATED;
  struct Key *key;

  /**
   * An array of materials, with length #totcol. These can be overridden by material slots
   * on #Object. Indices in the "material_index" attribute control which material is used for every
   * face.
   */
  struct Material **mat;

  /** The number of vertices in the mesh, and the size of #vdata. */
  int totvert;
  /** The number of edges in the mesh, and the size of #edata. */
  int totedge;
  /** The number of polygons/faces in the mesh, and the size of #pdata. */
  int totpoly;
  /** The number of face corners in the mesh, and the size of #ldata. */
  int totloop;

  /**
   * Array owned by mesh. See #Mesh::polys() and #OffsetIndices.
   *
   * This array is shared based on the bke::MeshRuntime::poly_offsets_sharing_info.
   * Avoid accessing directly when possible.
   */
  int *poly_offset_indices;

  CustomData vdata, edata, pdata, ldata;

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
  float texspace_location[3];
  float texspace_size[3];
  char texspace_flag;

  /** Various flags used when editing the mesh. */
  char editflag;
  /** Mostly more flags used when editing or displaying the mesh. */
  uint16_t flag;

  /**
   * The angle for auto smooth in radians. `M_PI` (180 degrees) causes all edges to be smooth.
   */
  float smoothresh;

  /** Per-mesh settings for voxel remesh. */
  float remesh_voxel_size;
  float remesh_voxel_adaptivity;

  int face_sets_color_seed;
  /* Stores the initial Face Set to be rendered white. This way the overlay can be enabled by
   * default and Face Sets can be used without affecting the color of the mesh. */
  int face_sets_color_default;

  /** The color attribute currently selected in the list and edited by a user. */
  char *active_color_attribute;
  /** The color attribute used by default (i.e. for rendering) if no name is given explicitly. */
  char *default_color_attribute;

  /**
   * User-defined symmetry flag (#eMeshSymmetryType) that causes editing operations to maintain
   * symmetrical geometry. Supported by operations such as transform and weight-painting.
   */
  char symmetry;

  /** Choice between different remesh methods in the UI. */
  char remesh_mode;

  /** The length of the #mat array. */
  short totcol;

  /**
   * Deprecated flag for choosing whether to store specific custom data that was built into #Mesh
   * structs in edit mode. Replaced by separating that data to separate layers. Kept for forward
   * and backwards compatibility.
   */
  char cd_flag DNA_DEPRECATED;
  char subdiv DNA_DEPRECATED;
  char subdivr DNA_DEPRECATED;
  char subsurftype DNA_DEPRECATED;

  /** Deprecated pointer to mesh polygons, kept for forward compatibility. */
  struct MPoly *mpoly DNA_DEPRECATED;
  /** Deprecated pointer to face corners, kept for forward compatibility. */
  struct MLoop *mloop DNA_DEPRECATED;

  /** Deprecated array of mesh vertices, kept for reading old files, now stored in #CustomData. */
  struct MVert *mvert DNA_DEPRECATED;
  /** Deprecated array of mesh edges, kept for reading old files, now stored in #CustomData. */
  struct MEdge *medge DNA_DEPRECATED;
  /** Deprecated "Vertex group" data. Kept for reading old files, now stored in #CustomData. */
  struct MDeformVert *dvert DNA_DEPRECATED;
  /** Deprecated runtime data for tessellation face UVs and texture, kept for reading old files. */
  struct MTFace *mtface DNA_DEPRECATED;
  /** Deprecated, use mtface. */
  struct TFace *tface DNA_DEPRECATED;
  /** Deprecated array of colors for the tessellated faces, kept for reading old files. */
  struct MCol *mcol DNA_DEPRECATED;
  /** Deprecated face storage (quads & triangles only). Kept for reading old files. */
  struct MFace *mface DNA_DEPRECATED;

  /**
   * Deprecated storage of old faces (only triangles or quads).
   *
   * \note This would be marked deprecated, however the particles still use this at run-time
   * for placing particles on the mesh (something which should be eventually upgraded).
   */
  CustomData fdata;
  /* Deprecated size of #fdata. */
  int totface;

  char _pad1[4];

  /**
   * Data that isn't saved in files, including caches of derived data, temporary data to improve
   * the editing experience, etc. The struct is created when reading files and can be accessed
   * without null checks, with the exception of some temporary meshes which should allocate and
   * free the data if they are passed to functions that expect run-time data.
   */
  MeshRuntimeHandle *runtime;
#ifdef __cplusplus
  /**
   * Array of vertex positions. Edges and faces are defined by indices into this array.
   */
  blender::Span<blender::float3> vert_positions() const;
  /** Write access to vertex data. */
  blender::MutableSpan<blender::float3> vert_positions_for_write();
  /**
   * Array of edges, containing vertex indices, stored in the ".edge_verts" attributes. For simple
   * triangle or quad meshes, edges could be calculated from the polygon and "corner edge" arrays,
   * however, edges need to be stored explicitly to edge domain attributes and to support loose
   * edges that aren't connected to faces.
   */
  blender::Span<blender::int2> edges() const;
  /** Write access to edge data. */
  blender::MutableSpan<blender::int2> edges_for_write();
  /**
   * Face topology storage of the offset of each face's section of the face corners. The size of
   * each polygon is encoded using the next offset value. Can be used to slice the #corner_verts or
   * #corner_edges arrays to find the vertices or edges that make up each face.
   */
  blender::OffsetIndices<int> polys() const;
  /**
   * Index of the first corner of each polygon, and the size of the polygon encoded as the next
   * offset. The total number of corners is the final value, and the first value is always zero.
   * May be empty if there are no polygons.
   */
  blender::Span<int> poly_offsets() const;
  /** Write access to #poly_offsets data. */
  blender::MutableSpan<int> poly_offsets_for_write();

  /**
   * Array of vertices for every face corner,  stored in the ".corner_vert" integer attribute.
   * For example, the vertices in a face can be retrieved with the #slice method:
   * \code{.cc}
   * const Span<int> poly_verts = corner_verts.slice(poly.loopstart, poly.totloop);
   * \endcode
   * Such a span can often be passed as an argument in lieu of a polygon and the entire corner
   * verts array.
   */
  blender::Span<int> corner_verts() const;
  /** Write access to the #corner_verts data. */
  blender::MutableSpan<int> corner_verts_for_write();

  /**
   * Array of edges following every face corner traveling around each face, stored in the
   * ".corner_edge" attribute. The array sliced the same way as the #corner_verts data. The edge
   * previous to a corner must be accessed with the index of the previous face corner.
   */
  blender::Span<int> corner_edges() const;
  /** Write access to the #corner_edges data. */
  blender::MutableSpan<int> corner_edges_for_write();

  blender::bke::AttributeAccessor attributes() const;
  blender::bke::MutableAttributeAccessor attributes_for_write();

  /**
   * Vertex group data, encoded as an array of indices and weights for every vertex.
   * \warning: May be empty.
   */
  blender::Span<MDeformVert> deform_verts() const;
  /** Write access to vertex group data. */
  blender::MutableSpan<MDeformVert> deform_verts_for_write();

  /**
   * Cached triangulation of mesh faces, depending on the face topology and the vertex positions.
   */
  blender::Span<MLoopTri> looptris() const;

  /**
   * A map containing the face index that each cached triangle from #Mesh::looptris() came from.
   */
  blender::Span<int> looptri_polys() const;

  /** Set cached mesh bounds to a known-correct value to avoid their lazy calculation later on. */
  void bounds_set_eager(const blender::Bounds<blender::float3> &bounds);

  /**
   * Cached information about loose edges, calculated lazily when necessary.
   */
  const blender::bke::LooseEdgeCache &loose_edges() const;
  /**
   * Cached information about vertices that aren't used by any edges.
   */
  const blender::bke::LooseVertCache &loose_verts() const;
  /**
   * Cached information about vertices that aren't used by faces (but may be used by loose edges).
   */
  const blender::bke::LooseVertCache &verts_no_face() const;

  /**
   * Explicitly set the cached number of loose edges to zero. This can improve performance
   * later on, because finding loose edges lazily can be skipped entirely.
   *
   * \note To allow setting this status on meshes without changing them, this does not tag the
   * cache dirty. If the mesh was changed first, the relevant dirty tags should be called first.
   */
  void tag_loose_edges_none() const;
  /**
   * Set the number of vertices not connected to edges to zero. Similar to #tag_loose_edges_none().
   * There may still be vertices only used by loose edges though.
   *
   * \note If both #tag_loose_edges_none() and #tag_loose_verts_none() are called,
   * all vertices are used by faces, so #verts_no_faces() will be tagged empty as well.
   */
  void tag_loose_verts_none() const;

  /**
   * Normal direction of polygons, defined by positions and the winding direction of face corners.
   */
  blender::Span<blender::float3> poly_normals() const;
  /**
   * Normal direction of vertices, defined as the weighted average of face normals
   * surrounding each vertex and the normalized position for loose vertices.
   */
  blender::Span<blender::float3> vert_normals() const;
#endif
} Mesh;

/* deprecated by MTFace, only here for file reading */
#ifdef DNA_DEPRECATED_ALLOW
typedef struct TFace {
  DNA_DEFINE_CXX_METHODS(TFace)

  /** The faces image for the active UVLayer. */
  void *tpage;
  float uv[4][2];
  unsigned int col[4];
  char flag, transp;
  short mode, tile, unwrap;
} TFace;
#endif

/* **************** MESH ********************* */

/** #Mesh.texspace_flag */
enum {
  ME_TEXSPACE_FLAG_AUTO = 1 << 0,
  ME_TEXSPACE_FLAG_AUTO_EVALUATED = 1 << 1,
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
  ME_SCULPT_IGNORE_UVS = 1 << 6, /* cleared */
  ME_REMESH_REPROJECT_MATERIALS = 1 << 7,
  ME_REMESH_REPROJECT_VERTEX_COLORS = 1 << 8,
  ME_DS_EXPAND = 1 << 9,
  ME_SCULPT_DYNAMIC_TOPOLOGY = 1 << 10,
  ME_FLAG_UNUSED_5 = 1 << 11, /* cleared */
  ME_REMESH_REPROJECT_PAINT_MASK = 1 << 12,
  ME_REMESH_FIX_POLES = 1 << 13,
  ME_REMESH_REPROJECT_VOLUME = 1 << 14,
  ME_REMESH_REPROJECT_SCULPT_FACE_SETS = 1 << 15,
};

#ifdef DNA_DEPRECATED_ALLOW
/** #Mesh.cd_flag */
enum {
  ME_CDFLAG_VERT_BWEIGHT = 1 << 0,
  ME_CDFLAG_EDGE_BWEIGHT = 1 << 1,
  ME_CDFLAG_EDGE_CREASE = 1 << 2,
  ME_CDFLAG_VERT_CREASE = 1 << 3,
};
#endif

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
