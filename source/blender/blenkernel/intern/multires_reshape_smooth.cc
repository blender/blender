/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "multires_reshape.hh"

#include "DNA_mesh_types.h"

#include "BLI_function_ref.hh"
#include "BLI_math_matrix.hh"
#include "BLI_task.hh"
#include "BLI_utildefines.hh"

#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_multires.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_eval.hh"
#include "BKE_subdiv_mesh.hh"
#include "BKE_subdiv_mesh_layout.hh"

#include "opensubdiv_converter_capi.hh"
#ifdef WITH_OPENSUBDIV
#  include "opensubdiv_evaluator.hh"
#endif
#include "opensubdiv_evaluator_capi.hh"

#include "subdiv_converter.hh"

namespace blender {

#ifdef WITH_OPENSUBDIV

/* -------------------------------------------------------------------- */
/** \name Local Structs
 * \{ */

/* Surface refers to a simplified and lower-memory footprint representation of the limit surface.
 *
 * Used to store pre-calculated information which is expensive or impossible to evaluate when
 * traversing the final limit surface. */

struct SurfacePoint {
  float3 P = float3(0.0f);
  float3x3 tangent_matrix = float3x3::identity();
};

struct SurfaceGrid {
  Array<SurfacePoint> points = {};
};

/* Geometry elements which are used to simplify creation of topology refiner at the sculpt level.
 * Contains a limited subset of information needed to construct topology refiner. */

struct Vertex {
  /* All grid coordinates which the vertex corresponding to.
   * For a vertices which are created from inner points of grids there is always one coordinate. */
  Vector<GridCoord> grid_coords = {};
};

/* Storage of data which is linearly interpolated from the reshape level to the top level. */

struct LinearGridElement {
  float mask = 0.0f;
};

struct LinearGrid {
  /* Span pointing to section of `elements_storage` in `LinearGrids` */
  MutableSpan<LinearGridElement> elements = {};
};

struct LinearGrids {
  int num_grids = 0;
  int level = 0;

  /* Cached size for the grid, for faster lookup. */
  int grid_size;

  /* Indexed by grid index. */
  Array<LinearGrid> grids;

  /* Elements for all grids are allocated in a single array, for the allocation performance. */
  Array<LinearGridElement> elements_storage;
};

/* Context which holds all information needed during propagation and smoothing. */

struct MultiresReshapeSmoothContext : NonCopyable, NonMovable {
  const MultiresReshapeContext *reshape_context;

  /* Geometry at a reshape multires level. */
  struct {
    Array<Vertex> vertices;
    /* Sharpness of each vertex, including the infinite sharpness of vertices which are adjacent
     * to a loose edge. See #OpenSubdiv_Converter::vert_sharpness. */
    Array<float> vert_sharpness;

    /* Sparse storage of edges. Will only include edges which have non-zero sharpness. */
    Array<int2> edges;
    /* Sharpness of each of the edges above. */
    Array<float> edge_sharpness;

    /* Indexes into the geometry.vertices array */
    Array<int> corner_verts;
    /* The grid each corner was created for. All the corners of a face share the same grid. */
    Array<int> corner_grid_indices;

    /* Face topology of subdivision level. */
    Array<int> face_offsets;

    OffsetIndices<int> faces() const
    {
      return OffsetIndices<int>(face_offsets, offset_indices::NoSortCheck());
    }
  } geometry;

  /* Grids of data which is linearly interpolated between grid elements at the reshape level.
   * The data is actually stored as a delta, which is then to be added to the higher levels. */
  LinearGrids linear_delta_grids;

  /* Subdivision surface created for geometry at a reshape level. */
  bke::subdiv::Subdiv *reshape_subdiv = nullptr;

  /* Limit surface of the base mesh with original sculpt level details on it, subdivided up to the
   * top level.
   * Is used as a base point to calculate how much displacement has been made in the sculpt mode.
   *
   * NOTE: Referring to sculpt as it is the main user of this functionality and it is clear to
   * understand what it actually means in a concrete example. This is a generic code which is also
   * used by Subdivide operation, but the idea is exactly the same as propagation in the sculpt
   * mode. */
  Array<SurfaceGrid> base_surface_grids;

  /* Defines how displacement is interpolated on the higher levels (for example, whether
   * displacement is smoothed in Catmull-Clark mode or interpolated linearly preserving sharp edges
   * of the current sculpt level).
   *
   * NOTE: Uses same enumerator type as Subdivide operator, since the values are the same and
   * decoupling type just adds extra headache to convert one enumerator to another. */
  MultiresSubdivideModeType smoothing_type;

  MultiresReshapeSmoothContext(const MultiresReshapeContext *reshape_context,
                               const MultiresSubdivideModeType smoothing_type)
      : reshape_context(reshape_context),
        geometry(),
        linear_delta_grids(),
        smoothing_type(smoothing_type)
  {
  }
  ~MultiresReshapeSmoothContext();
};

MultiresReshapeSmoothContext::~MultiresReshapeSmoothContext()
{
  if (this->reshape_subdiv == nullptr) {
    return;
  }
  bke::subdiv::free(this->reshape_subdiv);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Linear grids manipulation
 * \{ */

static void linear_grids_allocate(LinearGrids *linear_grids, int num_grids, int level)
{
  const size_t grid_size = bke::subdiv::grid_size_from_level(level);
  const size_t grid_area = grid_size * grid_size;
  const size_t num_grid_elements = num_grids * grid_area;

  linear_grids->num_grids = num_grids;
  linear_grids->level = level;
  linear_grids->grid_size = grid_size;

  linear_grids->grids.reinitialize(num_grids);
  linear_grids->elements_storage.reinitialize(num_grid_elements);

  for (int i = 0; i < num_grids; ++i) {
    const size_t element_offset = grid_area * i;
    linear_grids->grids[i].elements = linear_grids->elements_storage.as_mutable_span().slice(
        element_offset, grid_area);
  }
}

static LinearGridElement *linear_grid_element_get(LinearGrids *linear_grids,
                                                  const GridCoord *grid_coord)
{
  BLI_assert(grid_coord->grid_index >= 0);
  BLI_assert(grid_coord->grid_index < linear_grids->num_grids);

  const int grid_size = linear_grids->grid_size;

  const int grid_x = lround(grid_coord->u * (grid_size - 1));
  const int grid_y = lround(grid_coord->v * (grid_size - 1));
  const int grid_element_index = grid_y * grid_size + grid_x;

  LinearGrid *grid = &linear_grids->grids[grid_coord->grid_index];
  return &grid->elements[grid_element_index];
}

static void linear_grid_element_init(LinearGridElement *linear_grid_element)
{
  linear_grid_element->mask = 0.0f;
}

/* result = a - b. */
static void linear_grid_element_sub(LinearGridElement *result,
                                    const LinearGridElement *a,
                                    const LinearGridElement *b)
{
  result->mask = a->mask - b->mask;
}

static void linear_grid_element_interpolate(LinearGridElement *result,
                                            const LinearGridElement elements[4],
                                            const float weights[4])
{
  result->mask = elements[0].mask * weights[0] + elements[1].mask * weights[1] +
                 elements[2].mask * weights[2] + elements[3].mask * weights[3];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Surface
 * \{ */

static void base_surface_grids_allocate(MultiresReshapeSmoothContext *reshape_smooth_context)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  const int num_grids = reshape_context->num_grids;
  const int grid_size = reshape_context->top.grid_size;
  const int grid_area = grid_size * grid_size;

  reshape_smooth_context->base_surface_grids.reinitialize(num_grids);

  for (const int i : reshape_smooth_context->base_surface_grids.index_range()) {
    reshape_smooth_context->base_surface_grids[i].points.reinitialize(grid_area);
  }
}

static SurfacePoint *base_surface_grids_read(MultiresReshapeSmoothContext *reshape_smooth_context,
                                             const GridCoord *grid_coord)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  const int grid_index = grid_coord->grid_index;
  const int grid_size = reshape_context->top.grid_size;
  const int grid_x = lround(grid_coord->u * (grid_size - 1));
  const int grid_y = lround(grid_coord->v * (grid_size - 1));
  const int grid_element_index = grid_y * grid_size + grid_x;

  SurfaceGrid *surface_grid = &reshape_smooth_context->base_surface_grids[grid_index];
  return &surface_grid->points[grid_element_index];
}

static void base_surface_grids_write(MultiresReshapeSmoothContext *reshape_smooth_context,
                                     const GridCoord *grid_coord,
                                     const float3 &P,
                                     const float3x3 &tangent_matrix)
{
  SurfacePoint *point = base_surface_grids_read(reshape_smooth_context, grid_coord);
  point->P = P;
  point->tangent_matrix = tangent_matrix;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Evaluation of subdivision surface at a reshape level
 * \{ */

/* Find grid index which given face was created for. */
static int get_face_grid_index(const MultiresReshapeSmoothContext *reshape_smooth_context,
                               const IndexRange face)
{
  const Span<int> corner_grid_indices = reshape_smooth_context->geometry.corner_grid_indices;
  const int grid_index = corner_grid_indices[face.start()];

#  ifndef NDEBUG
  for (const int corner_index : face) {
    BLI_assert(corner_grid_indices[corner_index] == grid_index);
  }
#  endif

  return grid_index;
}

static std::optional<GridCoord> vert_grid_coord_with_grid_index(const Vertex *vert,
                                                                const int grid_index)
{
  for (const int i : vert->grid_coords.index_range()) {
    if (vert->grid_coords[i].grid_index == grid_index) {
      return vert->grid_coords[i];
    }
  }
  return std::nullopt;
}

/* Get grid coordinates which correspond to corners of the given face.
 * All the grid coordinates will be from the same grid index. */
static std::array<std::optional<GridCoord>, 4> grid_coords_from_face_verts(
    MultiresReshapeSmoothContext *reshape_smooth_context, const IndexRange face)
{
  std::array<std::optional<GridCoord>, 4> result;
  BLI_assert(face.size() == 4);

  const int grid_index = get_face_grid_index(reshape_smooth_context, face);
  BLI_assert(grid_index != -1);

  for (const int i : face.index_range()) {
    const int corner_index = face[i];
    const int vert_index = reshape_smooth_context->geometry.corner_verts[corner_index];
    result[i] = vert_grid_coord_with_grid_index(
        &reshape_smooth_context->geometry.vertices[vert_index], grid_index);
    BLI_assert(result[i].has_value());
  }
  return result;
}

/**
 * C++20 has a built-in #lerp function, so use a different name here to avoid ambiguous calls for
 * now.
 */
static float lerp_f(float t, float a, float b)
{
  return (a + t * (b - a));
}

static GridCoord interpolate_grid_coord(Span<std::optional<GridCoord>> face_grid_coords,
                                        const float u,
                                        const float v)
{
  /*
   * v
   * ^
   * | (3) -------- (2)
   * |  |            |
   * |  |            |
   * |  |            |
   * |  |            |
   * | (0) -------- (1)
   * *--------------------------> u
   */
  GridCoord result;

  const float u01 = lerp_f(u, face_grid_coords[0]->u, face_grid_coords[1]->u);
  const float u32 = lerp_f(u, face_grid_coords[3]->u, face_grid_coords[2]->u);

  const float v03 = lerp_f(v, face_grid_coords[0]->v, face_grid_coords[3]->v);
  const float v12 = lerp_f(v, face_grid_coords[1]->v, face_grid_coords[2]->v);

  result.grid_index = face_grid_coords[0]->grid_index;
  result.u = lerp_f(v, u01, u32);
  result.v = lerp_f(u, v03, v12);

  return result;
}

static void foreach_toplevel_grid_coord(
    MultiresReshapeSmoothContext *reshape_smooth_context,
    FunctionRef<void(const PTexCoord *, const GridCoord *)> callback)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const int level_difference = (reshape_context->top.level - reshape_context->reshape.level);

  const int inner_grid_size = (1 << level_difference) + 1;
  const float inner_grid_size_1_inv = 1.0f / float(inner_grid_size - 1);

  const OffsetIndices<int> faces = reshape_smooth_context->geometry.faces();
  threading::parallel_for(faces.index_range(), 1, [&](const IndexRange range) {
    for (const int face_index : range) {
      const IndexRange face = faces[face_index];
      std::array<std::optional<GridCoord>, 4> face_grid_coords = grid_coords_from_face_verts(
          reshape_smooth_context, face);

      for (int y = 0; y < inner_grid_size; ++y) {
        const float ptex_v = float(y) * inner_grid_size_1_inv;
        for (int x = 0; x < inner_grid_size; ++x) {
          const float ptex_u = float(x) * inner_grid_size_1_inv;

          PTexCoord ptex_coord;
          ptex_coord.ptex_face_index = face_index;
          ptex_coord.u = ptex_u;
          ptex_coord.v = ptex_v;

          const GridCoord grid_coord = interpolate_grid_coord(
              Span(face_grid_coords), ptex_u, ptex_v);

          callback(&ptex_coord, &grid_coord);
        }
      }
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generation of a topology information for OpenSubdiv converter
 *
 * Calculates vertices, their coordinates in the original grids, and connections of them so then
 * it's easy to create OpenSubdiv's topology refiner.
 * \{ */

static int get_reshape_level_resolution(const MultiresReshapeContext *reshape_context)
{
  return (1 << reshape_context->reshape.level) + 1;
}

static bool is_crease_supported(const MultiresReshapeSmoothContext *reshape_smooth_context)
{
  return !ELEM(reshape_smooth_context->smoothing_type,
               MultiresSubdivideModeType::Linear,
               MultiresSubdivideModeType::Simple);
}

/* Get crease which will be used for communication to OpenSubdiv topology.
 * Note that simple subdivision treats all base edges as infinitely sharp. */
static float get_effective_crease(const MultiresReshapeSmoothContext *reshape_smooth_context,
                                  const int base_edge_index)
{
  if (!is_crease_supported(reshape_smooth_context)) {
    return 1.0f;
  }
  if (reshape_smooth_context->reshape_context->cd_edge_crease.is_empty()) {
    return 0.0f;
  }
  return reshape_smooth_context->reshape_context->cd_edge_crease[base_edge_index];
}

static float get_effective_crease_float(const MultiresReshapeSmoothContext *reshape_smooth_context,
                                        const float crease)
{
  if (!is_crease_supported(reshape_smooth_context)) {
    return 1.0f;
  }
  return crease;
}

static void geometry_allocate(MultiresReshapeSmoothContext *reshape_smooth_context,
                              const bke::subdiv::MeshLayout &layout)
{
  reshape_smooth_context->geometry.vertices.reinitialize(layout.verts_num);
  reshape_smooth_context->geometry.vert_sharpness = Array<float>(layout.verts_num, 0.0f);

  reshape_smooth_context->geometry.corner_verts.reinitialize(layout.corners_num);
  reshape_smooth_context->geometry.corner_grid_indices.reinitialize(layout.corners_num);

  reshape_smooth_context->geometry.face_offsets.reinitialize(layout.faces_num + 1);
  offset_indices::fill_constant_group_size(4, 0, reshape_smooth_context->geometry.face_offsets);
  reshape_smooth_context->geometry.face_offsets.last() = layout.corners_num;
}

static void add_grid_coord_to_vert(MultiresReshapeSmoothContext *reshape_smooth_context,
                                   const GridCoord *grid_coord,
                                   const int coarse_vert_index,
                                   const int subdiv_vert_index)
{
  BLI_assert(subdiv_vert_index < reshape_smooth_context->geometry.vertices.size());

  Vertex *vert = &reshape_smooth_context->geometry.vertices[subdiv_vert_index];

  vert->grid_coords.append(*grid_coord);

  if (coarse_vert_index == -1) {
    return;
  }

  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  if (reshape_context->cd_vert_crease.is_empty()) {
    return;
  }

  float crease = reshape_context->cd_vert_crease[coarse_vert_index];
  if (crease == 0.0f) {
    return;
  }

  crease = get_effective_crease_float(reshape_smooth_context, crease);
  reshape_smooth_context->geometry.vert_sharpness[subdiv_vert_index] =
      bke::subdiv::crease_to_sharpness(crease);
}

/* TODO(sergey): De-duplicate with similar function in multires_reshape_vertcos.cc */
static void add_ptex_coord_to_vert(MultiresReshapeSmoothContext *reshape_smooth_context,
                                   const bke::subdiv::PtexCoord &coord,
                                   const int coarse_vert_index,
                                   const int subdiv_vert_index)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  PTexCoord ptex_coord{};
  ptex_coord.ptex_face_index = coord.ptex_face_index;
  ptex_coord.u = coord.uv.x;
  ptex_coord.v = coord.uv.y;
  const GridCoord grid_coord = multires_reshape_ptex_coord_to_grid(reshape_context, &ptex_coord);
  const int face_index = multires_reshape_grid_to_face_index(reshape_context,
                                                             grid_coord.grid_index);

  const int num_corners = reshape_context->base_faces[face_index].size();
  const int start_grid_index = reshape_context->base_faces[face_index].start();
  const int corner = grid_coord.grid_index - start_grid_index;

  if (grid_coord.u == 0.0f && grid_coord.v == 0.0f) {
    for (int current_corner = 0; current_corner < num_corners; ++current_corner) {
      GridCoord corner_grid_coord = grid_coord;
      corner_grid_coord.grid_index = start_grid_index + current_corner;
      add_grid_coord_to_vert(
          reshape_smooth_context, &corner_grid_coord, coarse_vert_index, subdiv_vert_index);
    }
    return;
  }

  add_grid_coord_to_vert(
      reshape_smooth_context, &grid_coord, coarse_vert_index, subdiv_vert_index);

  if (grid_coord.u == 0.0f) {
    GridCoord prev_grid_coord;
    prev_grid_coord.grid_index = start_grid_index + ((corner + num_corners - 1) % num_corners);
    prev_grid_coord.u = grid_coord.v;
    prev_grid_coord.v = 0.0f;

    add_grid_coord_to_vert(
        reshape_smooth_context, &prev_grid_coord, coarse_vert_index, subdiv_vert_index);
  }

  if (grid_coord.v == 0.0f) {
    GridCoord next_grid_coord;
    next_grid_coord.grid_index = start_grid_index + ((corner + 1) % num_corners);
    next_grid_coord.u = 0.0f;
    next_grid_coord.v = grid_coord.u;

    add_grid_coord_to_vert(
        reshape_smooth_context, &next_grid_coord, coarse_vert_index, subdiv_vert_index);
  }
}

static void geometry_create_inner_verts(MultiresReshapeSmoothContext *reshape_smooth_context,
                                        const bke::subdiv::MeshLayout &layout)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const OffsetIndices<int> inner_verts_by_face = layout.inner_verts_by_face();
  const Span<int> face_ptex_offset = bke::subdiv::face_ptex_offset_get(reshape_context->subdiv);
  threading::parallel_for(
      reshape_context->base_faces.index_range(),
      layout.base_face_grain_size(),
      [&](const IndexRange range) {
        const IndexRange chunk_verts = inner_verts_by_face[range];
        Array<bke::subdiv::PtexCoord> ptex_coords(chunk_verts.size());
        bke::subdiv::fill_inner_vert_ptex_coords(
            layout, *reshape_context->base_mesh, face_ptex_offset, range, ptex_coords);
        for (const int i : ptex_coords.index_range()) {
          add_ptex_coord_to_vert(reshape_smooth_context,
                                 ptex_coords[i],
                                 -1,
                                 layout.inner_verts_start + chunk_verts[i]);
        }
      });
}

static void geometry_create_corner_grid_indices(
    MultiresReshapeSmoothContext *reshape_smooth_context, const bke::subdiv::MeshLayout &layout)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const OffsetIndices<int> base_faces = reshape_context->base_faces;
  const OffsetIndices<int> faces_by_base_face = layout.faces_by_base_face();
  const Span<int> face_ptex_offset = bke::subdiv::face_ptex_offset_get(reshape_context->subdiv);
  MutableSpan<int> corner_grid_indices = reshape_smooth_context->geometry.corner_grid_indices;
  threading::parallel_for(
      base_faces.index_range(), layout.base_face_grain_size(), [&](const IndexRange range) {
        const IndexRange chunk_faces = faces_by_base_face[range];
        Array<int> face_base_corners(chunk_faces.size());
        bke::subdiv::fill_corner_ptex_coords(
            layout, *reshape_context->base_mesh, face_ptex_offset, range, {}, face_base_corners);
        for (const int base_face : range) {
          /* The first grid index of a face is the index of its first corner. */
          const int first_grid_index = base_faces[base_face].start();
          for (const int face : faces_by_base_face[base_face]) {
            const int base_corner = face_base_corners[face - chunk_faces.start()];
            corner_grid_indices.slice(face * 4, 4).fill(first_grid_index + base_corner);
          }
        }
      });
}

static void geometry_tag_loose_edge_verts_sharp(
    MultiresReshapeSmoothContext *reshape_smooth_context)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const Span<int2> base_edges = reshape_context->base_edges;
  auto &geometry = reshape_smooth_context->geometry;
  /* NOTE: This runs after every other vertex is processed, so it is fine to override the
   * sharpness which may have been set from the vertex crease. The vertices inside of loose edges
   * are never used by faces, so only the base mesh vertices at the ends of the edges (which have
   * the same indices in the subdivided mesh) are relevant. */
  reshape_context->base_mesh->loose_edges().foreach_index([&](const int edge) {
    for (const int vert : {base_edges[edge][0], base_edges[edge][1]}) {
      if (!geometry.vertices[vert].grid_coords.is_empty()) {
        geometry.vert_sharpness[vert] = OPENSUBDIV_SHARPNESS_INFINITE;
      }
    }
  });
}

static void geometry_create_edges(MultiresReshapeSmoothContext *reshape_smooth_context,
                                  const bke::subdiv::MeshLayout &layout)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const Mesh &base_mesh = *reshape_context->base_mesh;
  auto &geometry = reshape_smooth_context->geometry;
  const int edges_per_base_edge = layout.resolution - 1;

  /* Loose edges are not communicated to OpenSubdiv. */
  IndexMaskMemory memory;
  const IndexMask non_loose_edges = base_mesh.loose_edges().complement(
      IndexRange(base_mesh.edges_num), memory);

  if (reshape_smooth_context->smoothing_type == MultiresSubdivideModeType::Linear) {
    /* All edges are infinitely sharp. */
    const int base_edge_edges_num = non_loose_edges.size() * edges_per_base_edge;
    const int inner_edges_num = layout.edges_num - layout.inner_edges_start;
    geometry.edges.reinitialize(base_edge_edges_num + inner_edges_num);
    bke::subdiv::fill_base_edge_edges(
        layout,
        reshape_context->base_edges,
        non_loose_edges,
        geometry.edges.as_mutable_span().take_front(base_edge_edges_num));
    bke::subdiv::fill_inner_edges(
        layout, base_mesh, geometry.edges.as_mutable_span().drop_front(base_edge_edges_num));
    geometry.edge_sharpness = Array<float>(geometry.edges.size(),
                                           bke::subdiv::crease_to_sharpness(1.0f));
    return;
  }

  /* Edges without crease and edges inside of base mesh faces have zero sharpness, so they are
   * ignored. */
  const IndexMask creased_edges = IndexMask::from_predicate(
      non_loose_edges, memory, [&](const int edge) {
        return get_effective_crease(reshape_smooth_context, edge) != 0.0f;
      });
  geometry.edges.reinitialize(creased_edges.size() * edges_per_base_edge);
  bke::subdiv::fill_base_edge_edges(
      layout, reshape_context->base_edges, creased_edges, geometry.edges);
  geometry.edge_sharpness.reinitialize(geometry.edges.size());
  creased_edges.foreach_index([&](const int edge, const int pos) {
    const float crease = get_effective_crease(reshape_smooth_context, edge);
    geometry.edge_sharpness.as_mutable_span()
        .slice(pos * edges_per_base_edge, edges_per_base_edge)
        .fill(bke::subdiv::crease_to_sharpness(crease));
  });
}

static void geometry_create(MultiresReshapeSmoothContext *reshape_smooth_context)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  const bke::subdiv::MeshLayout layout = bke::subdiv::build_mesh_layout(
      *reshape_context->base_mesh, get_reshape_level_resolution(reshape_context));
  geometry_allocate(reshape_smooth_context, layout);
  geometry_create_edges(reshape_smooth_context, layout);
  bke::subdiv::fill_corner_verts_and_edges(layout,
                                           *reshape_context->base_mesh,
                                           reshape_context->base_faces.index_range(),
                                           reshape_smooth_context->geometry.corner_verts,
                                           {});
  geometry_create_corner_grid_indices(reshape_smooth_context, layout);
  geometry_create_inner_verts(reshape_smooth_context, layout);
  multires_reshape_foreach_boundary_vert_ptex_coord(
      reshape_context,
      layout,
      [&](const bke::subdiv::PtexCoord &coord, const int base_vert, const int subdiv_vert) {
        add_ptex_coord_to_vert(reshape_smooth_context, coord, base_vert, subdiv_vert);
      });

  geometry_tag_loose_edge_verts_sharp(reshape_smooth_context);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generation of OpenSubdiv evaluator for topology created form reshape level
 * \{ */

static void converter_init(const MultiresReshapeSmoothContext *reshape_smooth_context,
                           OpenSubdiv_Converter *converter)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const bke::subdiv::Settings &settings = reshape_context->subdiv->settings;

  converter->scheme_type = OSD_SCHEME_CATMARK;
  converter->vtx_boundary_interpolation = OpenSubdiv_VtxBoundaryInterpolation(
      bke::subdiv::converter_vtx_boundary_interpolation_from_settings(&settings));
  converter->fvar_linear_interpolation = OpenSubdiv_FVarLinearInterpolation(
      bke::subdiv::converter_fvar_linear_from_settings(&settings));

  converter->verts_num = reshape_smooth_context->geometry.vertices.size();

  converter->edges = reshape_smooth_context->geometry.edges.as_span().cast<std::pair<int, int>>();
  converter->face_offsets = reshape_smooth_context->geometry.face_offsets.as_span();
  converter->corner_verts = reshape_smooth_context->geometry.corner_verts;
  converter->edge_sharpness = reshape_smooth_context->geometry.edge_sharpness;
  converter->vert_sharpness = reshape_smooth_context->geometry.vert_sharpness;

  converter->freeUserData = nullptr;

  converter->user_data = (void *)reshape_smooth_context;
}

/* Create subdiv descriptor created for topology at a reshape level. */
static void reshape_subdiv_create(MultiresReshapeSmoothContext *reshape_smooth_context)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const bke::subdiv::Settings *settings = &reshape_context->subdiv->settings;

  OpenSubdiv_Converter converter;
  converter_init(reshape_smooth_context, &converter);

  bke::subdiv::Subdiv *reshape_subdiv = bke::subdiv::new_from_converter(settings, &converter);

  OpenSubdiv_EvaluatorSettings evaluator_settings = {0};
  bke::subdiv::eval_begin(
      reshape_subdiv, bke::subdiv::SUBDIV_EVALUATOR_TYPE_CPU, nullptr, &evaluator_settings);

  reshape_smooth_context->reshape_subdiv = reshape_subdiv;

  bke::subdiv::converter_free(&converter);
}

/* Callback to provide coarse position for subdivision surface topology at a reshape level. */
using ReshapeSubdivCoarsePositionCb = void(
    const MultiresReshapeSmoothContext *reshape_smooth_context, const Vertex *vert, float3 &r_P);

/* Refine subdivision surface topology at a reshape level for new coarse vertices positions. */
static void reshape_subdiv_refine(const MultiresReshapeSmoothContext *reshape_smooth_context,
                                  ReshapeSubdivCoarsePositionCb coarse_position_cb)
{
  bke::subdiv::Subdiv *reshape_subdiv = reshape_smooth_context->reshape_subdiv;

  /* TODO(sergey): For non-trivial coarse_position_cb we should multi-thread this loop. */

  const int num_vertices = reshape_smooth_context->geometry.vertices.size();
  for (int i = 0; i < num_vertices; ++i) {
    const Vertex *vert = &reshape_smooth_context->geometry.vertices[i];
    float3 P;
    coarse_position_cb(reshape_smooth_context, vert, P);
    reshape_subdiv->evaluator->eval_output->setCoarsePositions(P, i, 1);
  }
  reshape_subdiv->evaluator->eval_output->refine();
}

BLI_INLINE const GridCoord *reshape_subdiv_refine_vert_grid_coord(const Vertex *vert)
{
  if (vert->grid_coords.is_empty()) {
    /* This is a loose vertex, the coordinate is not important. */
    /* TODO(sergey): Once loose elements are excluded from the topology at the reshape level this
     * should become an assert instead. */
    return nullptr;
  }
  /* NOTE: All grid coordinates will point to the same object position, so can be simple and use
   * first grid coordinate. */
  return vert->grid_coords.data();
}

/* Version of reshape_subdiv_refine() which uses coarse position from original grids. */
static void reshape_subdiv_refine_orig_P(
    const MultiresReshapeSmoothContext *reshape_smooth_context, const Vertex *vert, float3 &r_P)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const GridCoord *grid_coord = reshape_subdiv_refine_vert_grid_coord(vert);

  /* Check whether this is a loose vertex. */
  if (grid_coord == nullptr) {
    r_P = float3(0.0f);
    return;
  }

  float3 limit_P;
  float3x3 tangent_matrix;
  multires_reshape_evaluate_base_mesh_limit_at_grid(
      reshape_context, grid_coord, limit_P, tangent_matrix);

  const ReshapeConstGridElement orig_grid_element =
      multires_reshape_orig_grid_element_for_grid_coord(reshape_context, grid_coord);

  const float3 D = math::transform_direction(tangent_matrix, orig_grid_element.displacement);

  r_P = limit_P + D;
}
static void reshape_subdiv_refine_orig(const MultiresReshapeSmoothContext *reshape_smooth_context)
{
  reshape_subdiv_refine(reshape_smooth_context, reshape_subdiv_refine_orig_P);
}

/* Version of reshape_subdiv_refine() which uses coarse position from final grids. */
static void reshape_subdiv_refine_final_P(
    const MultiresReshapeSmoothContext *reshape_smooth_context, const Vertex *vert, float3 &r_P)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const GridCoord *grid_coord = reshape_subdiv_refine_vert_grid_coord(vert);

  /* Check whether this is a loose vertex. */
  if (grid_coord == nullptr) {
    r_P = float3(0.0f);
    return;
  }

  const ReshapeGridElement grid_element = multires_reshape_grid_element_for_grid_coord(
      reshape_context, grid_coord);

  /* NOTE: At this point in reshape/propagate pipeline grid displacement is actually storing object
   * vertices coordinates. */
  r_P = *grid_element.displacement;
}
static void reshape_subdiv_refine_final(const MultiresReshapeSmoothContext *reshape_smooth_context)
{
  reshape_subdiv_refine(reshape_smooth_context, reshape_subdiv_refine_final_P);
}

static void reshape_subdiv_evaluate_limit_at_grid(
    const MultiresReshapeSmoothContext *reshape_smooth_context,
    const PTexCoord *ptex_coord,
    const GridCoord *grid_coord,
    float3 &limit_P,
    float3x3 &r_tangent_matrix)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  float3 dPdu;
  float3 dPdv;
  bke::subdiv::eval_limit_point_and_derivatives(reshape_smooth_context->reshape_subdiv,
                                                ptex_coord->ptex_face_index,
                                                ptex_coord->u,
                                                ptex_coord->v,
                                                limit_P,
                                                dPdu,
                                                dPdv);

  const int face_index = multires_reshape_grid_to_face_index(reshape_context,
                                                             grid_coord->grid_index);
  const int corner = multires_reshape_grid_to_corner(reshape_context, grid_coord->grid_index);
  multires_reshape_tangent_matrix_for_corner(
      reshape_context, face_index, corner, dPdu, dPdv, r_tangent_matrix);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Linearly interpolated data
 * \{ */

static LinearGridElement linear_grid_element_orig_get(
    const MultiresReshapeSmoothContext *reshape_smooth_context, const GridCoord *grid_coord)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const ReshapeConstGridElement orig_grid_element =
      multires_reshape_orig_grid_element_for_grid_coord(reshape_context, grid_coord);

  LinearGridElement linear_grid_element;
  linear_grid_element_init(&linear_grid_element);

  linear_grid_element.mask = orig_grid_element.mask;

  return linear_grid_element;
}

static LinearGridElement linear_grid_element_final_get(
    const MultiresReshapeSmoothContext *reshape_smooth_context, const GridCoord *grid_coord)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const ReshapeGridElement final_grid_element = multires_reshape_grid_element_for_grid_coord(
      reshape_context, grid_coord);

  LinearGridElement linear_grid_element;
  linear_grid_element_init(&linear_grid_element);

  if (final_grid_element.mask != nullptr) {
    linear_grid_element.mask = *final_grid_element.mask;
  }

  return linear_grid_element;
}

/* Interpolate difference of the linear data.
 *
 * Will access final data and original data at the grid elements at the reshape level,
 * calculate difference between final and original, and linearly interpolate to get value at the
 * top level. */
static void linear_grid_element_delta_interpolate(
    const MultiresReshapeSmoothContext *reshape_smooth_context,
    const GridCoord *grid_coord,
    LinearGridElement *result)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  const int reshape_level = reshape_context->reshape.level;
  const int reshape_level_grid_size = bke::subdiv::grid_size_from_level(reshape_level);
  const int reshape_level_grid_size_1 = reshape_level_grid_size - 1;
  const float reshape_level_grid_size_1_inv = 1.0f / float(reshape_level_grid_size_1);

  const float x_f = grid_coord->u * reshape_level_grid_size_1;
  const float y_f = grid_coord->v * reshape_level_grid_size_1;

  const int x_i = x_f;
  const int y_i = y_f;
  const int x_n_i = (x_i == reshape_level_grid_size - 1) ? (x_i) : (x_i + 1);
  const int y_n_i = (y_i == reshape_level_grid_size - 1) ? (y_i) : (y_i + 1);

  const int corners_int_coords[4][2] = {{x_i, y_i}, {x_n_i, y_i}, {x_n_i, y_n_i}, {x_i, y_n_i}};

  LinearGridElement corner_elements[4];
  for (int i = 0; i < 4; ++i) {
    GridCoord corner_grid_coord;
    corner_grid_coord.grid_index = grid_coord->grid_index;
    corner_grid_coord.u = corners_int_coords[i][0] * reshape_level_grid_size_1_inv;
    corner_grid_coord.v = corners_int_coords[i][1] * reshape_level_grid_size_1_inv;

    const LinearGridElement orig_element = linear_grid_element_orig_get(reshape_smooth_context,
                                                                        &corner_grid_coord);
    const LinearGridElement final_element = linear_grid_element_final_get(reshape_smooth_context,
                                                                          &corner_grid_coord);
    linear_grid_element_sub(&corner_elements[i], &final_element, &orig_element);
  }

  const float u = x_f - x_i;
  const float v = y_f - y_i;
  const float weights[4] = {(1.0f - u) * (1.0f - v), u * (1.0f - v), u * v, (1.0f - u) * v};

  linear_grid_element_interpolate(result, corner_elements, weights);
}

static void evaluate_linear_delta_grids(MultiresReshapeSmoothContext *reshape_smooth_context)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const int num_grids = reshape_context->num_grids;
  const int top_level = reshape_context->top.level;

  linear_grids_allocate(&reshape_smooth_context->linear_delta_grids, num_grids, top_level);

  foreach_toplevel_grid_coord(reshape_smooth_context,
                              [&](const PTexCoord * /*ptex_coord*/, const GridCoord *grid_coord) {
                                LinearGridElement *linear_delta_element = linear_grid_element_get(
                                    &reshape_smooth_context->linear_delta_grids, grid_coord);

                                linear_grid_element_delta_interpolate(
                                    reshape_smooth_context, grid_coord, linear_delta_element);
                              });
}

static void propagate_linear_data_delta(MultiresReshapeSmoothContext *reshape_smooth_context,
                                        ReshapeGridElement *final_grid_element,
                                        const GridCoord *grid_coord)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  LinearGridElement *linear_delta_element = linear_grid_element_get(
      &reshape_smooth_context->linear_delta_grids, grid_coord);

  const ReshapeConstGridElement orig_grid_element =
      multires_reshape_orig_grid_element_for_grid_coord(reshape_context, grid_coord);

  if (final_grid_element->mask != nullptr) {
    *final_grid_element->mask = clamp_f(
        orig_grid_element.mask + linear_delta_element->mask, 0.0f, 1.0f);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Evaluation of base surface
 * \{ */

static void evaluate_base_surface_grids(MultiresReshapeSmoothContext *reshape_smooth_context)
{
  foreach_toplevel_grid_coord(
      reshape_smooth_context, [&](const PTexCoord *ptex_coord, const GridCoord *grid_coord) {
        float3 limit_P;
        float3x3 tangent_matrix;
        reshape_subdiv_evaluate_limit_at_grid(
            reshape_smooth_context, ptex_coord, grid_coord, limit_P, tangent_matrix);

        base_surface_grids_write(reshape_smooth_context, grid_coord, limit_P, tangent_matrix);
      });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Evaluation of new surface
 * \{ */

/* Evaluate final position of the original (pre-sculpt-edit) point position at a given grid
 * coordinate. */
static void evaluate_final_original_point(
    const MultiresReshapeSmoothContext *reshape_smooth_context,
    const GridCoord *grid_coord,
    float3 &r_orig_final_P)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  /* Element of an original MDISPS grid. */
  const ReshapeConstGridElement orig_grid_element =
      multires_reshape_orig_grid_element_for_grid_coord(reshape_context, grid_coord);

  /* Limit surface of the base mesh. */
  float3 base_mesh_limit_P;
  float3x3 base_mesh_tangent_matrix;
  multires_reshape_evaluate_base_mesh_limit_at_grid(
      reshape_context, grid_coord, base_mesh_limit_P, base_mesh_tangent_matrix);

  /* Convert original displacement from tangent space to object space. */
  const float3 orig_displacement = math::transform_direction(base_mesh_tangent_matrix,
                                                             orig_grid_element.displacement);

  /* Final point = limit surface + displacement. */
  r_orig_final_P = base_mesh_limit_P + orig_displacement;
}

static void evaluate_higher_grid_positions_with_details(
    MultiresReshapeSmoothContext *reshape_smooth_context)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  foreach_toplevel_grid_coord(
      reshape_smooth_context, [&](const PTexCoord *ptex_coord, const GridCoord *grid_coord) {
        /* Position of the original vertex at top level. */
        float3 orig_final_P;
        evaluate_final_original_point(reshape_smooth_context, grid_coord, orig_final_P);

        /* Original surface point on sculpt level (sculpt level before edits in sculpt mode). */
        const SurfacePoint *orig_sculpt_point = base_surface_grids_read(reshape_smooth_context,
                                                                        grid_coord);

        /* Difference between original top level and original sculpt level in object space. */
        const float3 original_detail_delta = orig_final_P - orig_sculpt_point->P;

        /* Difference between original top level and original sculpt level in tangent space of
         * original sculpt level. */
        const float3x3 original_sculpt_tangent_matrix_inv = math::invert(
            orig_sculpt_point->tangent_matrix);
        float3 original_detail_delta_tangent = math::transform_direction(
            original_sculpt_tangent_matrix_inv, original_detail_delta);

        /* Limit surface of smoothed (subdivided) edited sculpt level. */
        float3 smooth_limit_P;
        float3x3 smooth_tangent_matrix;
        reshape_subdiv_evaluate_limit_at_grid(
            reshape_smooth_context, ptex_coord, grid_coord, smooth_limit_P, smooth_tangent_matrix);

        /* Add original detail to the smoothed surface. */
        float3 smooth_delta = math::transform_direction(smooth_tangent_matrix,
                                                        original_detail_delta_tangent);

        /* Grid element of the result.
         *
         * NOTE: Displacement is storing object space coordinate. */
        ReshapeGridElement grid_element = multires_reshape_grid_element_for_grid_coord(
            reshape_context, grid_coord);

        *grid_element.displacement = smooth_limit_P + smooth_delta;

        /* Propagate non-coordinate data. */
        propagate_linear_data_delta(reshape_smooth_context, &grid_element, grid_coord);
      });
}

static void evaluate_higher_grid_positions(MultiresReshapeSmoothContext *reshape_smooth_context)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  foreach_toplevel_grid_coord(
      reshape_smooth_context, [&](const PTexCoord *ptex_coord, const GridCoord *grid_coord) {
        bke::subdiv::Subdiv *reshape_subdiv = reshape_smooth_context->reshape_subdiv;

        ReshapeGridElement grid_element = multires_reshape_grid_element_for_grid_coord(
            reshape_context, grid_coord);

        /* Surface. */
        const float3 P = bke::subdiv::eval_limit_point(
            reshape_subdiv, ptex_coord->ptex_face_index, ptex_coord->u, ptex_coord->v);

        *grid_element.displacement = P;

        /* Propagate non-coordinate data. */
        propagate_linear_data_delta(reshape_smooth_context, &grid_element, grid_coord);
      });
}

#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Entry point
 * \{ */

void multires_reshape_smooth_object_grids_with_details(
    const MultiresReshapeContext *reshape_context)
{
#ifdef WITH_OPENSUBDIV
  const int level_difference = (reshape_context->top.level - reshape_context->reshape.level);
  if (level_difference == 0) {
    /* Early output. */
    return;
  }

  const MultiresSubdivideModeType smoothing_type = reshape_context->subdiv->settings.is_simple ?
                                                       MultiresSubdivideModeType::Simple :
                                                       MultiresSubdivideModeType::CatmullClark;
  MultiresReshapeSmoothContext reshape_smooth_context(reshape_context, smoothing_type);
  geometry_create(&reshape_smooth_context);
  evaluate_linear_delta_grids(&reshape_smooth_context);

  reshape_subdiv_create(&reshape_smooth_context);

  base_surface_grids_allocate(&reshape_smooth_context);
  reshape_subdiv_refine_orig(&reshape_smooth_context);
  evaluate_base_surface_grids(&reshape_smooth_context);

  reshape_subdiv_refine_final(&reshape_smooth_context);
  evaluate_higher_grid_positions_with_details(&reshape_smooth_context);
#else
  UNUSED_VARS(reshape_context);
#endif
}

void multires_reshape_smooth_object_grids(const MultiresReshapeContext *reshape_context,
                                          const MultiresSubdivideModeType mode)
{
#ifdef WITH_OPENSUBDIV
  const int level_difference = (reshape_context->top.level - reshape_context->reshape.level);
  if (level_difference == 0) {
    /* Early output. */
    return;
  }

  MultiresReshapeSmoothContext reshape_smooth_context(reshape_context, mode);
  geometry_create(&reshape_smooth_context);
  evaluate_linear_delta_grids(&reshape_smooth_context);

  reshape_subdiv_create(&reshape_smooth_context);

  reshape_subdiv_refine_final(&reshape_smooth_context);
  evaluate_higher_grid_positions(&reshape_smooth_context);
#else
  UNUSED_VARS(reshape_context, mode);
#endif
}

/** \} */

}  // namespace blender
