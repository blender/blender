/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_attribute_types.h"
#include "DNA_customdata_types.h"
#include "DNA_defs.h"
#include "DNA_session_uid_types.h"

/** Workaround to forward-declare C++ type in C header. */
#ifdef __cplusplus

#  include <optional>

#  include "BLI_math_vector_types.hh"
#  include "BLI_memory_counter_fwd.hh"
#  include "BLI_vector_set.hh"

namespace blender {
template<typename T> struct Bounds;
namespace offset_indices {
template<typename T> struct GroupedSpan;
template<typename T> class OffsetIndices;
}  // namespace offset_indices
using offset_indices::GroupedSpan;
using offset_indices::OffsetIndices;
template<typename T> class MutableSpan;
template<typename T> class Span;
namespace bke {
struct BVHTreeFromMesh;
struct MeshRuntime;
class AttributeAccessor;
class MutableAttributeAccessor;
struct LooseVertCache;
struct LooseEdgeCache;
enum class MeshNormalDomain : int8_t;
}  // namespace bke
}  // namespace blender
using MeshRuntimeHandle = blender::bke::MeshRuntime;
#else
typedef struct MeshRuntimeHandle MeshRuntimeHandle;
#endif

struct AnimData;
struct Key;
struct MCol;
struct MEdge;
struct MFace;
struct Material;

typedef struct Mesh {
#ifdef __cplusplus
  DNA_DEFINE_CXX_METHODS(Mesh)
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_ME;
#endif

  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  struct Key *key;

  /**
   * An array of materials, with length #totcol. These can be overridden by material slots
   * on #Object. Indices in the "material_index" attribute control which material is used for every
   * face.
   */
  struct Material **mat;

  /** The number of vertices in the mesh, and the size of #vert_data. */
  int verts_num;
  /** The number of edges in the mesh, and the size of #edge_data. */
  int edges_num;
  /** The number of faces in the mesh, and the size of #face_data. */
  int faces_num;
  /** The number of face corners in the mesh, and the size of #corner_data. */
  int corners_num;

  /**
   * Array owned by mesh. See #Mesh::faces() and #OffsetIndices.
   *
   * This array is shared based on the bke::MeshRuntime::face_offsets_sharing_info.
   * Avoid accessing directly when possible.
   */
  int *face_offset_indices;

  /**
   * Vertex, edge, face, and corner generic attributes. Currently unused at runtime, but used for
   * forward compatibility when reading files (see #122398).
   */
  struct AttributeStorage attribute_storage;

  CustomData vert_data;
  CustomData edge_data;
  CustomData face_data;
  CustomData corner_data;

  /**
   * List of vertex group (#bDeformGroup) names and flags only. Actual weights are stored in dvert.
   * \note This pointer is for convenient access to the #CD_MDEFORMVERT layer in #vert_data.
   */
  ListBase vertex_group_names;
  /** The active index in the #vertex_group_names list. */
  int vertex_group_active_index;

  /**
   * The index of the active attribute in the UI. The attribute list is a combination of the
   * generic type attributes from vertex, edge, face, and corner custom data.
   *
   * Set to -1 when none is active.
   */
  int attributes_active_index;

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

  float smoothresh_legacy DNA_DEPRECATED;

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
   * The UV map currently selected in the list and edited by a user.
   * Currently only used for file reading/writing (see #AttributeStorage).
   */
  char *active_uv_map_attribute;
  /**
   * The UV map used by default (i.e. for rendering) if no name is given explicitly.
   * Currently only used for file reading/writing (see #AttributeStorage).
   */
  char *default_uv_map_attribute;

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
  CustomData fdata_legacy;
  /* Deprecated size of #fdata. */
  int totface_legacy;

  char _pad1;
  int8_t radial_symmetry[3];

  /**
   * Data that isn't saved in files, including caches of derived data, temporary data to improve
   * the editing experience, etc. The struct is created when reading files and can be accessed
   * without null checks, with the exception of some temporary meshes which should allocate and
   * free the data if they are passed to functions that expect run-time data.
   */
  MeshRuntimeHandle *runtime;
#ifdef __cplusplus
  /**
   * Array of vertex positions. Edges and face corners are defined by indices into this array.
   */
  blender::Span<blender::float3> vert_positions() const;
  /** Write access to vertex data. */
  blender::MutableSpan<blender::float3> vert_positions_for_write();
  /**
   * Array of edges, containing vertex indices, stored in the ".edge_verts" attribute. For simple
   * triangle or quad meshes, edges could be calculated from the face and #corner_edge arrays.
   * However, edges need to be stored explicitly for edge domain attributes and to support loose
   * edges that aren't connected to faces.
   */
  blender::Span<blender::int2> edges() const;
  /** Write access to edge data. */
  blender::MutableSpan<blender::int2> edges_for_write();
  /**
   * Face topology information (using the same internal data as #face_offsets()). Each face is a
   * contiguous chunk of face corners represented as an #IndexRange. Each face can be used to slice
   * the #corner_verts or #corner_edges arrays to find the vertices or edges that each face uses.
   */
  blender::OffsetIndices<int> faces() const;
  /**
   * Return an array containing the first corner of each face. and the size of the face encoded as
   * the next offset. The total number of corners is the final value, and the first value is always
   * zero. May be empty if there are no faces.
   */
  blender::Span<int> face_offsets() const;
  /** Write access to #face_offsets data. */
  blender::MutableSpan<int> face_offsets_for_write();

  /**
   * Array of vertices for every face corner, stored in the ".corner_vert" integer attribute.
   * For example, the vertices in a face can be retrieved with the #slice method:
   * \code{.cc}
   * const Span<int> face_verts = corner_verts.slice(face);
   * \endcode
   * This span can often be passed as an argument in lieu of a face and the entire corner verts
   * array.
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
   * The names of all UV map attributes, in the order of the internal storage.
   * This is useful when UV maps are referenced by index.
   *
   * \warning Adding or removing attributes will invalidate the referenced memory.
   */
  blender::VectorSet<blender::StringRefNull> uv_map_names() const;

  /** The name of the active UV map attribute, if any. */
  blender::StringRefNull active_uv_map_name() const;
  /** The name of the default UV map (e.g. for rendering) attribute, if any. */
  blender::StringRefNull default_uv_map_name() const;

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
  blender::Span<blender::int3> corner_tris() const;

  /**
   * A map containing the face index that each cached triangle from #Mesh::corner_tris() came from.
   */
  blender::Span<int> corner_tri_faces() const;

  /**
   * Calculate the largest and smallest position values of vertices.
   */
  std::optional<blender::Bounds<blender::float3>> bounds_min_max() const;

  /** Set cached mesh bounds to a known-correct value to avoid their lazy calculation later on. */
  void bounds_set_eager(const blender::Bounds<blender::float3> &bounds);

  /** Get the largest material index used by the mesh or `nullopt` if it has no faces. */
  std::optional<int> material_index_max() const;

  /** Get all the material indices actually used by the mesh. */
  const blender::VectorSet<int> &material_indices_used() const;

  /**
   * Cached map containing the index of the face using each face corner.
   */
  blender::Span<int> corner_to_face_map() const;
  /**
   * Offsets per vertex used to slice arrays containing data for connected faces or face corners.
   */
  blender::OffsetIndices<int> vert_to_face_map_offsets() const;
  /**
   * Cached map from each vertex to the corners using it.
   */
  blender::GroupedSpan<int> vert_to_corner_map() const;
  /**
   * Cached map from each vertex to the faces using it.
   */
  blender::GroupedSpan<int> vert_to_face_map() const;

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
   * True if the mesh has no faces or edges "inside" of other faces. Those edges or faces would
   * reuse a subset of the vertices of a face. Knowing the mesh is "clean" or "good" can mean
   * algorithms can skip checking for duplicate edges and faces when they create new edges and
   * faces inside of faces.
   *
   * \note This is just a hint, so there still might be no overlapping geometry if it is false.
   */
  bool no_overlapping_topology() const;

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
  /** Set the #no_overlapping_topology() hint when the mesh is "clean." */
  void tag_overlapping_none();

  /**
   * Returns the least complex attribute domain needed to store normals encoding all relevant mesh
   * data. When all edges or faces are sharp, face normals are enough. When all are smooth, vertex
   * normals are enough. With a combination of sharp and smooth, normals may be "split",
   * requiring face corner storage.
   *
   * When possible, it's preferred to use face normals over vertex normals and vertex normals over
   * face corner normals, since there is a 2-4x performance cost increase for each more complex
   * domain.
   *
   * Optionally the consumer of the mesh can indicate that they support the sharp_face attribute
   * natively, to avoid using corner normals in some cases.
   */
  blender::bke::MeshNormalDomain normals_domain(const bool support_sharp_face = false) const;
  /**
   * Normal direction of faces, defined by positions and the winding direction of face corners.
   */
  blender::Span<blender::float3> face_normals() const;
  blender::Span<blender::float3> face_normals_true() const;
  /**
   * Normal direction of vertices, defined as the weighted average of face normals
   * surrounding each vertex and the normalized position for loose vertices.
   */
  blender::Span<blender::float3> vert_normals() const;
  blender::Span<blender::float3> vert_normals_true() const;
  /**
   * Normal direction at each face corner. Defined by a combination of face normals, vertex
   * normals, the `sharp_edge` and `sharp_face` attributes, and potentially by custom normals.
   *
   * \note Because of the large memory requirements of storing normals per face corner, prefer
   * using #face_normals() or #vert_normals() when possible (see #normals_domain()). For this
   * reason, the "true" face corner normals aren't cached, since they're just the same as the
   * corresponding face normals.
   */
  blender::Span<blender::float3> corner_normals() const;

  blender::bke::BVHTreeFromMesh bvh_verts() const;
  blender::bke::BVHTreeFromMesh bvh_edges() const;
  blender::bke::BVHTreeFromMesh bvh_legacy_faces() const;
  blender::bke::BVHTreeFromMesh bvh_corner_tris() const;
  blender::bke::BVHTreeFromMesh bvh_corner_tris_no_hidden() const;
  blender::bke::BVHTreeFromMesh bvh_loose_verts() const;
  blender::bke::BVHTreeFromMesh bvh_loose_edges() const;
  blender::bke::BVHTreeFromMesh bvh_loose_no_hidden_verts() const;
  blender::bke::BVHTreeFromMesh bvh_loose_no_hidden_edges() const;

  void count_memory(blender::MemoryCounter &memory) const;

  /** Call after changing vertex positions to tag lazily calculated caches for recomputation. */
  void tag_positions_changed();
  /** Call after moving every mesh vertex by the same translation. */
  void tag_positions_changed_uniformly();
  /** Like #tag_positions_changed but doesn't tag normals; they must be updated separately. */
  void tag_positions_changed_no_normals();
  /** Call when changing "sharp_face" or "sharp_edge" data. */
  void tag_sharpness_changed();
  /** Call when changing "custom_normal" data. */
  void tag_custom_normals_changed();
  /** Call when face vertex order has changed but positions and faces haven't changed. */
  void tag_face_winding_changed();
  /** Call when new edges and vertices have been created but vertices and faces haven't changed. */
  void tag_edges_split();
  /** Call for topology updates not described by other update tags. */
  void tag_topology_changed();
  /** Call when changing the ".hide_vert", ".hide_edge", or ".hide_poly" attributes. */
  void tag_visibility_changed();
  /** Call when changing the "material_index" attribute. */
  void tag_material_index_changed();
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
  /**
   * The UV selection is marked as synchronized.
   * See #BMesh::uv_select_sync_valid for details.
   */
  ME_FLAG_UV_SELECT_SYNC_VALID = 1 << 3,
  ME_FLAG_UNUSED_4 = 1 << 4,     /* cleared */
  ME_AUTOSMOOTH_LEGACY = 1 << 5, /* deprecated */
  ME_FLAG_UNUSED_6 = 1 << 6,     /* cleared */
  ME_FLAG_UNUSED_7 = 1 << 7,     /* cleared */
  ME_REMESH_REPROJECT_ATTRIBUTES = 1 << 8,
  ME_DS_EXPAND = 1 << 9,
  ME_SCULPT_DYNAMIC_TOPOLOGY = 1 << 10,
  /**
   * Used to tag that the mesh has no overlapping topology (see #Mesh::no_overlapping_topology()).
   * Theoretically this is runtime data that could always be recalculated, but since the intent is
   * to improve performance and it only takes one bit, it is stored in the mesh instead.
   */
  ME_NO_OVERLAPPING_TOPOLOGY = 1 << 11,
  ME_FLAG_UNUSED_8 = 1 << 12, /* deprecated */
  ME_REMESH_FIX_POLES = 1 << 13,
  ME_REMESH_REPROJECT_VOLUME = 1 << 14,
  ME_FLAG_UNUSED_9 = 1 << 15, /* deprecated */
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
typedef enum MeshSubdivType {
  ME_CC_SUBSURF = 0,
  ME_SIMPLE_SUBSURF = 1,
} MeshSubdivType;

/** #Mesh.symmetry */
typedef enum eMeshSymmetryType {
  ME_SYMMETRY_X = 1 << 0,
  ME_SYMMETRY_Y = 1 << 1,
  ME_SYMMETRY_Z = 1 << 2,
} eMeshSymmetryType;

#define MESH_MAX_VERTS 2000000000L
