/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "multires_reshape.hh"

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"

#include "BLI_function_ref.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_multires.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_eval.hh"
#include "BKE_subdiv_foreach.hh"
#include "BKE_subdiv_mesh.hh"

#include "opensubdiv_converter_capi.hh"
#ifdef WITH_OPENSUBDIV
#  include "opensubdiv_evaluator.hh"
#endif
#include "opensubdiv_evaluator_capi.hh"

#include "atomic_ops.h"
#include "subdiv_converter.hh"

#ifdef WITH_OPENSUBDIV

/* -------------------------------------------------------------------- */
/** \name Local Structs
 * \{ */

/* Surface refers to a simplified and lower-memory footprint representation of the limit surface.
 *
 * Used to store pre-calculated information which is expensive or impossible to evaluate when
 * traversing the final limit surface. */

struct SurfacePoint {
  blender::float3 P = blender::float3(0.0f);
  blender::float3x3 tangent_matrix = blender::float3x3::identity();
};

struct SurfaceGrid {
  blender::Array<SurfacePoint> points = {};
};

/* Geometry elements which are used to simplify creation of topology refiner at the sculpt level.
 * Contains a limited subset of information needed to construct topology refiner. */

struct Vertex {
  /* All grid coordinates which the vertex corresponding to.
   * For a vertices which are created from inner points of grids there is always one coordinate. */
  blender::Vector<GridCoord> grid_coords = {};

  float sharpness = 0.0f;
  bool is_infinite_sharp = false;
};

struct Corner {
  /* Indexes into the geometry.vertices array */
  int vert_index = 0;
  int grid_index = 0;
};

struct Edge {
  int v1 = 0;
  int v2 = 0;

  float sharpness = 0.0f;
};

/* Storage of data which is linearly interpolated from the reshape level to the top level. */

struct LinearGridElement {
  float mask = 0.0f;
};

struct LinearGrid {
  /* Span pointing to section of `elements_storage` in `LinearGrids` */
  blender::MutableSpan<LinearGridElement> elements = {};
};

struct LinearGrids {
  int num_grids = 0;
  int level = 0;

  /* Cached size for the grid, for faster lookup. */
  int grid_size;

  /* Indexed by grid index. */
  blender::Array<LinearGrid> grids;

  /* Elements for all grids are allocated in a single array, for the allocation performance. */
  blender::Array<LinearGridElement> elements_storage;
};

/* Context which holds all information needed during propagation and smoothing. */

struct MultiresReshapeSmoothContext : blender::NonCopyable, blender::NonMovable {
  const MultiresReshapeContext *reshape_context;

  /* Geometry at a reshape multires level. */
  struct {
    blender::Array<Vertex> vertices;

    /* Maximum number of edges which might be stored in the edges array.
     * Is calculated based on the number of edges in the base mesh and the subdivision level. */
    int max_edges;

    /* Sparse storage of edges. Will only include edges which have non-zero sharpness.
     *
     * NOTE: Different type from others to be able to easier use atomic ops. */
    size_t num_edges;
    blender::Array<Edge> edges;

    blender::Array<Corner> corners;

    /* Face topology of subdivision level. */
    blender::Array<int> face_offsets;

    blender::OffsetIndices<int> faces() const
    {
      return blender::OffsetIndices<int>(face_offsets, blender::offset_indices::NoSortCheck());
    }
  } geometry;

  /* Grids of data which is linearly interpolated between grid elements at the reshape level.
   * The data is actually stored as a delta, which is then to be added to the higher levels. */
  LinearGrids linear_delta_grids;

  /* From #Mesh::loose_edges(). May be empty. */
  blender::BitSpan loose_base_edges = {};

  /* Subdivision surface created for geometry at a reshape level. */
  blender::bke::subdiv::Subdiv *reshape_subdiv = nullptr;

  /* Limit surface of the base mesh with original sculpt level details on it, subdivided up to the
   * top level.
   * Is used as a base point to calculate how much displacement has been made in the sculpt mode.
   *
   * NOTE: Referring to sculpt as it is the main user of this functionality and it is clear to
   * understand what it actually means in a concrete example. This is a generic code which is also
   * used by Subdivide operation, but the idea is exactly the same as propagation in the sculpt
   * mode. */
  blender::Array<SurfaceGrid> base_surface_grids;

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
  blender::bke::subdiv::free(this->reshape_subdiv);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Linear grids manipulation
 * \{ */

static void linear_grids_allocate(LinearGrids *linear_grids, int num_grids, int level)
{
  const size_t grid_size = blender::bke::subdiv::grid_size_from_level(level);
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
                                     const blender::float3 &P,
                                     const blender::float3x3 &tangent_matrix)
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
                               const blender::IndexRange face)
{
  const Corner *first_corner = &reshape_smooth_context->geometry.corners[face.start()];
  const int grid_index = first_corner->grid_index;

#  ifndef NDEBUG
  for (const int corner_index : face) {
    const Corner *corner = &reshape_smooth_context->geometry.corners[corner_index];
    BLI_assert(corner->grid_index == grid_index);
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
    MultiresReshapeSmoothContext *reshape_smooth_context, const blender::IndexRange face)
{
  std::array<std::optional<GridCoord>, 4> result;
  BLI_assert(face.size() == 4);

  const int grid_index = get_face_grid_index(reshape_smooth_context, face);
  BLI_assert(grid_index != -1);

  for (const int i : face.index_range()) {
    const int corner_index = face[i];
    Corner *corner = &reshape_smooth_context->geometry.corners[corner_index];
    result[i] = vert_grid_coord_with_grid_index(
        &reshape_smooth_context->geometry.vertices[corner->vert_index], grid_index);
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

static GridCoord interpolate_grid_coord(blender::Span<std::optional<GridCoord>> face_grid_coords,
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
    blender::FunctionRef<void(const PTexCoord *, const GridCoord *)> callback)
{
  using namespace blender;
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const int level_difference = (reshape_context->top.level - reshape_context->reshape.level);

  const int inner_grid_size = (1 << level_difference) + 1;
  const float inner_grid_size_1_inv = 1.0f / float(inner_grid_size - 1);

  const OffsetIndices<int> faces = reshape_smooth_context->geometry.faces();
  threading::parallel_for(faces.index_range(), 1, [&](const IndexRange range) {
    for (const int face_index : range) {
      const blender::IndexRange face = faces[face_index];
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
              blender::Span(face_grid_coords), ptex_u, ptex_v);

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

static bool foreach_topology_info(const blender::bke::subdiv::ForeachContext *foreach_context,
                                  const int num_vertices,
                                  const int num_edges,
                                  const int num_loops,
                                  const int num_faces,
                                  const int * /*subdiv_face_offset*/)
{
  MultiresReshapeSmoothContext *reshape_smooth_context =
      static_cast<MultiresReshapeSmoothContext *>(foreach_context->user_data);
  const int max_edges = reshape_smooth_context->smoothing_type ==
                                MultiresSubdivideModeType::Linear ?
                            num_edges :
                            reshape_smooth_context->geometry.max_edges;

  reshape_smooth_context->geometry.vertices.reinitialize(num_vertices);

  reshape_smooth_context->geometry.max_edges = max_edges;
  reshape_smooth_context->geometry.edges.reinitialize(max_edges);

  reshape_smooth_context->geometry.corners.reinitialize(num_loops);

  reshape_smooth_context->geometry.face_offsets.reinitialize(num_faces + 1);
  reshape_smooth_context->geometry.face_offsets.last() = num_loops;

  return true;
}

static void foreach_single_vert(const blender::bke::subdiv::ForeachContext *foreach_context,
                                const GridCoord *grid_coord,
                                const int coarse_vert_index,
                                const int subdiv_vert_index)
{
  MultiresReshapeSmoothContext *reshape_smooth_context =
      static_cast<MultiresReshapeSmoothContext *>(foreach_context->user_data);

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
  vert->sharpness = blender::bke::subdiv::crease_to_sharpness(crease);
}

/* TODO(sergey): De-duplicate with similar function in multires_reshape_vertcos.cc */
static void foreach_vert(const blender::bke::subdiv::ForeachContext *foreach_context,
                         const PTexCoord *ptex_coord,
                         const int coarse_vert_index,
                         const int subdiv_vert_index)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context =
      static_cast<const MultiresReshapeSmoothContext *>(foreach_context->user_data);
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  const GridCoord grid_coord = multires_reshape_ptex_coord_to_grid(reshape_context, ptex_coord);
  const int face_index = multires_reshape_grid_to_face_index(reshape_context,
                                                             grid_coord.grid_index);

  const int num_corners = reshape_context->base_faces[face_index].size();
  const int start_grid_index = reshape_context->face_start_grid_index[face_index];
  const int corner = grid_coord.grid_index - start_grid_index;

  if (grid_coord.u == 0.0f && grid_coord.v == 0.0f) {
    for (int current_corner = 0; current_corner < num_corners; ++current_corner) {
      GridCoord corner_grid_coord = grid_coord;
      corner_grid_coord.grid_index = start_grid_index + current_corner;
      foreach_single_vert(
          foreach_context, &corner_grid_coord, coarse_vert_index, subdiv_vert_index);
    }
    return;
  }

  foreach_single_vert(foreach_context, &grid_coord, coarse_vert_index, subdiv_vert_index);

  if (grid_coord.u == 0.0f) {
    GridCoord prev_grid_coord;
    prev_grid_coord.grid_index = start_grid_index + ((corner + num_corners - 1) % num_corners);
    prev_grid_coord.u = grid_coord.v;
    prev_grid_coord.v = 0.0f;

    foreach_single_vert(foreach_context, &prev_grid_coord, coarse_vert_index, subdiv_vert_index);
  }

  if (grid_coord.v == 0.0f) {
    GridCoord next_grid_coord;
    next_grid_coord.grid_index = start_grid_index + ((corner + 1) % num_corners);
    next_grid_coord.u = 0.0f;
    next_grid_coord.v = grid_coord.u;

    foreach_single_vert(foreach_context, &next_grid_coord, coarse_vert_index, subdiv_vert_index);
  }
}

static void foreach_vert_inner(const blender::bke::subdiv::ForeachContext *foreach_context,
                               void * /*tls*/,
                               const int ptex_face_index,
                               const float ptex_face_u,
                               const float ptex_face_v,
                               const int /*coarse_face_index*/,
                               const int /*coarse_corner*/,
                               const int subdiv_vert_index)
{
  PTexCoord ptex_coord{};
  ptex_coord.ptex_face_index = ptex_face_index;
  ptex_coord.u = ptex_face_u;
  ptex_coord.v = ptex_face_v;
  foreach_vert(foreach_context, &ptex_coord, -1, subdiv_vert_index);
}

static void foreach_vert_every_corner(const blender::bke::subdiv::ForeachContext *foreach_context,
                                      void * /*tls_v*/,
                                      const int ptex_face_index,
                                      const float ptex_face_u,
                                      const float ptex_face_v,
                                      const int coarse_vert_index,
                                      const int /*coarse_face_index*/,
                                      const int /*coarse_face_corner*/,
                                      const int subdiv_vert_index)
{
  PTexCoord ptex_coord{};
  ptex_coord.ptex_face_index = ptex_face_index;
  ptex_coord.u = ptex_face_u;
  ptex_coord.v = ptex_face_v;
  foreach_vert(foreach_context, &ptex_coord, coarse_vert_index, subdiv_vert_index);
}

static void foreach_vert_every_edge(const blender::bke::subdiv::ForeachContext *foreach_context,
                                    void * /*tls_v*/,
                                    const int ptex_face_index,
                                    const float ptex_face_u,
                                    const float ptex_face_v,
                                    const int /*coarse_edge_index*/,
                                    const int /*coarse_face_index*/,
                                    const int /*coarse_face_corner*/,
                                    const int subdiv_vert_index)
{
  PTexCoord ptex_coord{};
  ptex_coord.ptex_face_index = ptex_face_index;
  ptex_coord.u = ptex_face_u;
  ptex_coord.v = ptex_face_v;
  foreach_vert(foreach_context, &ptex_coord, -1, subdiv_vert_index);
}

static void foreach_loop(const blender::bke::subdiv::ForeachContext *foreach_context,
                         void * /*tls*/,
                         const int /*ptex_face_index*/,
                         const float /*ptex_face_u*/,
                         const float /*ptex_face_v*/,
                         const int /*coarse_loop_index*/,
                         const int coarse_face_index,
                         const int coarse_corner,
                         const int subdiv_loop_index,
                         const int subdiv_vert_index,
                         const int /*subdiv_edge_index*/)
{
  MultiresReshapeSmoothContext *reshape_smooth_context =
      static_cast<MultiresReshapeSmoothContext *>(foreach_context->user_data);
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  BLI_assert(subdiv_loop_index < reshape_smooth_context->geometry.corners.size());

  Corner *corner = &reshape_smooth_context->geometry.corners[subdiv_loop_index];
  corner->vert_index = subdiv_vert_index;

  const int first_grid_index = reshape_context->face_start_grid_index[coarse_face_index];
  corner->grid_index = first_grid_index + coarse_corner;
}

static void foreach_poly(const blender::bke::subdiv::ForeachContext *foreach_context,
                         void * /*tls*/,
                         const int /*coarse_face_index*/,
                         const int subdiv_face_index,
                         const int start_loop_index,
                         const int /*num_loops*/)
{
  MultiresReshapeSmoothContext *reshape_smooth_context =
      static_cast<MultiresReshapeSmoothContext *>(foreach_context->user_data);

  BLI_assert(subdiv_face_index < reshape_smooth_context->geometry.faces().size());

  reshape_smooth_context->geometry.face_offsets[subdiv_face_index] = start_loop_index;
}

static void foreach_vert_of_loose_edge(const blender::bke::subdiv::ForeachContext *foreach_context,
                                       void * /*tls*/,
                                       const int /*coarse_edge_index*/,
                                       const float /*u*/,
                                       const int vert_index)
{
  MultiresReshapeSmoothContext *reshape_smooth_context =
      static_cast<MultiresReshapeSmoothContext *>(foreach_context->user_data);
  Vertex *vert = &reshape_smooth_context->geometry.vertices[vert_index];

  vert->is_infinite_sharp = !vert->grid_coords.is_empty();
}

static void store_edge(MultiresReshapeSmoothContext *reshape_smooth_context,
                       const int subdiv_v1,
                       const int subdiv_v2,
                       const float crease)
{
  /* This is a bit overhead to use atomics in such a simple function called from many threads,
   * but this allows to save quite measurable amount of memory. */
  const int edge_index = atomic_fetch_and_add_z(&reshape_smooth_context->geometry.num_edges, 1);
  BLI_assert(edge_index < reshape_smooth_context->geometry.max_edges);

  Edge *edge = &reshape_smooth_context->geometry.edges[edge_index];
  edge->v1 = subdiv_v1;
  edge->v2 = subdiv_v2;
  edge->sharpness = blender::bke::subdiv::crease_to_sharpness(crease);
}

static void foreach_edge(const blender::bke::subdiv::ForeachContext *foreach_context,
                         void * /*tls*/,
                         const int coarse_edge_index,
                         const int /*subdiv_edge_index*/,
                         const bool is_loose,
                         const int subdiv_v1,
                         const int subdiv_v2)
{
  MultiresReshapeSmoothContext *reshape_smooth_context =
      static_cast<MultiresReshapeSmoothContext *>(foreach_context->user_data);

  if (reshape_smooth_context->smoothing_type == MultiresSubdivideModeType::Linear) {
    if (!is_loose) {
      store_edge(reshape_smooth_context, subdiv_v1, subdiv_v2, 1.0f);
    }
    return;
  }

  /* Ignore all inner face edges as they have sharpness of zero. */
  if (coarse_edge_index == ORIGINDEX_NONE) {
    return;
  }
  /* Ignore all loose edges as well, as they are not communicated to the OpenSubdiv. */
  if (!reshape_smooth_context->loose_base_edges.is_empty()) {
    if (reshape_smooth_context->loose_base_edges[coarse_edge_index]) {
      return;
    }
  }
  /* Edges without crease are to be ignored as well. */
  const float crease = get_effective_crease(reshape_smooth_context, coarse_edge_index);
  if (crease == 0.0f) {
    return;
  }
  store_edge(reshape_smooth_context, subdiv_v1, subdiv_v2, crease);
}

static void geometry_init_loose_information(MultiresReshapeSmoothContext *reshape_smooth_context)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const Mesh *base_mesh = reshape_context->base_mesh;

  const blender::bke::LooseEdgeCache &loose_edges = base_mesh->loose_edges();
  reshape_smooth_context->loose_base_edges = loose_edges.is_loose_bits;

  int num_used_edges = 0;
  for (const int edge : blender::IndexRange(base_mesh->edges_num)) {
    if (loose_edges.count > 0 && loose_edges.is_loose_bits[edge]) {
      continue;
    }
    const float crease = get_effective_crease(reshape_smooth_context, edge);
    if (crease == 0.0f) {
      continue;
    }
    num_used_edges++;
  }

  const int resolution = get_reshape_level_resolution(reshape_context);
  const int num_subdiv_vertices_per_base_edge = resolution - 2;
  reshape_smooth_context->geometry.max_edges = num_used_edges *
                                               (num_subdiv_vertices_per_base_edge + 1);
}

static void geometry_create(MultiresReshapeSmoothContext *reshape_smooth_context)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  blender::bke::subdiv::ForeachContext foreach_context{};
  foreach_context.topology_info = foreach_topology_info;
  foreach_context.vert_inner = foreach_vert_inner;
  foreach_context.vert_every_corner = foreach_vert_every_corner;
  foreach_context.vert_every_edge = foreach_vert_every_edge;
  foreach_context.loop = foreach_loop;
  foreach_context.poly = foreach_poly;
  foreach_context.vert_of_loose_edge = foreach_vert_of_loose_edge;
  foreach_context.edge = foreach_edge;
  foreach_context.user_data = reshape_smooth_context;

  geometry_init_loose_information(reshape_smooth_context);

  blender::bke::subdiv::ToMeshSettings mesh_settings;
  mesh_settings.resolution = get_reshape_level_resolution(reshape_context);
  mesh_settings.use_optimal_display = false;

  /* TODO(sergey): Tell the foreach() to ignore loose vertices. */
  blender::bke::subdiv::foreach_subdiv_geometry(
      reshape_context->subdiv, &foreach_context, &mesh_settings, reshape_context->base_mesh);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generation of OpenSubdiv evaluator for topology created form reshape level
 * \{ */

static OpenSubdiv_SchemeType get_scheme_type(const OpenSubdiv_Converter * /*converter*/)
{
  return OSD_SCHEME_CATMARK;
}

static OpenSubdiv_VtxBoundaryInterpolation get_vtx_boundary_interpolation(
    const OpenSubdiv_Converter *converter)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context =
      static_cast<const MultiresReshapeSmoothContext *>(converter->user_data);
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const blender::bke::subdiv::Settings *settings = &reshape_context->subdiv->settings;

  return OpenSubdiv_VtxBoundaryInterpolation(
      blender::bke::subdiv::converter_vtx_boundary_interpolation_from_settings(settings));
}

static OpenSubdiv_FVarLinearInterpolation get_fvar_linear_interpolation(
    const OpenSubdiv_Converter *converter)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context =
      static_cast<const MultiresReshapeSmoothContext *>(converter->user_data);
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const blender::bke::subdiv::Settings *settings = &reshape_context->subdiv->settings;

  return OpenSubdiv_FVarLinearInterpolation(
      blender::bke::subdiv::converter_fvar_linear_from_settings(settings));
}

static bool specifies_full_topology(const OpenSubdiv_Converter * /*converter*/)
{
  return false;
}

static int get_num_vertices(const OpenSubdiv_Converter *converter)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context =
      static_cast<const MultiresReshapeSmoothContext *>(converter->user_data);

  return reshape_smooth_context->geometry.vertices.size();
}

static void get_face_vertices(const OpenSubdiv_Converter *converter,
                              int face_index,
                              int *face_vertices)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context =
      static_cast<const MultiresReshapeSmoothContext *>(converter->user_data);
  BLI_assert(face_index < reshape_smooth_context->geometry.faces().size());

  const blender::IndexRange face = reshape_smooth_context->geometry.faces()[face_index];

  for (const int i : face.index_range()) {
    const int corner_index = face[i];
    const Corner *corner = &reshape_smooth_context->geometry.corners[corner_index];
    face_vertices[i] = corner->vert_index;
  }
}

static int get_num_edges(const OpenSubdiv_Converter *converter)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context =
      static_cast<const MultiresReshapeSmoothContext *>(converter->user_data);
  return reshape_smooth_context->geometry.num_edges;
}

static void get_edge_vertices(const OpenSubdiv_Converter *converter,
                              const int edge_index,
                              int edge_vertices[2])
{
  const MultiresReshapeSmoothContext *reshape_smooth_context =
      static_cast<const MultiresReshapeSmoothContext *>(converter->user_data);
  BLI_assert(edge_index < reshape_smooth_context->geometry.num_edges);

  const Edge *edge = &reshape_smooth_context->geometry.edges[edge_index];
  edge_vertices[0] = edge->v1;
  edge_vertices[1] = edge->v2;
}

static float get_edge_sharpness(const OpenSubdiv_Converter *converter, const int edge_index)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context =
      static_cast<const MultiresReshapeSmoothContext *>(converter->user_data);
  BLI_assert(edge_index < reshape_smooth_context->geometry.num_edges);

  const Edge *edge = &reshape_smooth_context->geometry.edges[edge_index];
  return edge->sharpness;
}

static float get_vert_sharpness(const OpenSubdiv_Converter *converter, const int vert_index)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context =
      static_cast<const MultiresReshapeSmoothContext *>(converter->user_data);
  BLI_assert(vert_index < reshape_smooth_context->geometry.vertices.size());

  const Vertex *vertex = &reshape_smooth_context->geometry.vertices[vert_index];
  return vertex->sharpness;
}

static bool is_infinite_sharp_vertex(const OpenSubdiv_Converter *converter, int vert_index)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context =
      static_cast<const MultiresReshapeSmoothContext *>(converter->user_data);

  BLI_assert(vert_index < reshape_smooth_context->geometry.vertices.size());

  const Vertex *vertex = &reshape_smooth_context->geometry.vertices[vert_index];
  return vertex->is_infinite_sharp;
}

static void converter_init(const MultiresReshapeSmoothContext *reshape_smooth_context,
                           OpenSubdiv_Converter *converter)
{
  converter->getSchemeType = get_scheme_type;
  converter->getVtxBoundaryInterpolation = get_vtx_boundary_interpolation;
  converter->getFVarLinearInterpolation = get_fvar_linear_interpolation;
  converter->specifiesFullTopology = specifies_full_topology;

  converter->faces = reshape_smooth_context->geometry.faces();

  converter->getNumEdges = get_num_edges;
  converter->getNumVertices = get_num_vertices;

  converter->getFaceVertices = get_face_vertices;
  converter->getFaceEdges = nullptr;

  converter->getEdgeVertices = get_edge_vertices;
  converter->getNumEdgeFaces = nullptr;
  converter->getEdgeFaces = nullptr;
  converter->getEdgeSharpness = get_edge_sharpness;

  converter->getNumVertexEdges = nullptr;
  converter->getVertexEdges = nullptr;
  converter->getNumVertexFaces = nullptr;
  converter->getVertexFaces = nullptr;
  converter->isInfiniteSharpVertex = is_infinite_sharp_vertex;
  converter->getVertexSharpness = get_vert_sharpness;

  converter->getNumUVLayers = nullptr;
  converter->precalcUVLayer = nullptr;
  converter->finishUVLayer = nullptr;
  converter->getNumUVCoordinates = nullptr;
  converter->getFaceCornerUVIndex = nullptr;

  converter->freeUserData = nullptr;

  converter->user_data = (void *)reshape_smooth_context;
}

/* Create subdiv descriptor created for topology at a reshape level. */
static void reshape_subdiv_create(MultiresReshapeSmoothContext *reshape_smooth_context)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const blender::bke::subdiv::Settings *settings = &reshape_context->subdiv->settings;

  OpenSubdiv_Converter converter;
  converter_init(reshape_smooth_context, &converter);

  blender::bke::subdiv::Subdiv *reshape_subdiv = blender::bke::subdiv::new_from_converter(
      settings, &converter);

  OpenSubdiv_EvaluatorSettings evaluator_settings = {0};
  blender::bke::subdiv::eval_begin(reshape_subdiv,
                                   blender::bke::subdiv::SUBDIV_EVALUATOR_TYPE_CPU,
                                   nullptr,
                                   &evaluator_settings);

  reshape_smooth_context->reshape_subdiv = reshape_subdiv;

  blender::bke::subdiv::converter_free(&converter);
}

/* Callback to provide coarse position for subdivision surface topology at a reshape level. */
using ReshapeSubdivCoarsePositionCb =
    void(const MultiresReshapeSmoothContext *reshape_smooth_context,
         const Vertex *vert,
         blender::float3 &r_P);

/* Refine subdivision surface topology at a reshape level for new coarse vertices positions. */
static void reshape_subdiv_refine(const MultiresReshapeSmoothContext *reshape_smooth_context,
                                  ReshapeSubdivCoarsePositionCb coarse_position_cb)
{
  blender::bke::subdiv::Subdiv *reshape_subdiv = reshape_smooth_context->reshape_subdiv;

  /* TODO(sergey): For non-trivial coarse_position_cb we should multi-thread this loop. */

  const int num_vertices = reshape_smooth_context->geometry.vertices.size();
  for (int i = 0; i < num_vertices; ++i) {
    const Vertex *vert = &reshape_smooth_context->geometry.vertices[i];
    blender::float3 P;
    coarse_position_cb(reshape_smooth_context, vert, P);
    reshape_subdiv->evaluator->eval_output->setCoarsePositions(P, i, 1);
  }
  reshape_subdiv->evaluator->eval_output->refine();
}

BLI_INLINE const GridCoord *reshape_subdiv_refine_vert_grid_coord(const Vertex *vert)
{
  if (vert->grid_coords.is_empty()) {
    /* This is a loose vertex, the coordinate is not important. */
    /* TODO(sergey): Once the subdiv_foreach() supports properly ignoring loose elements this
     * should become an assert instead. */
    return nullptr;
  }
  /* NOTE: All grid coordinates will point to the same object position, so can be simple and use
   * first grid coordinate. */
  return &vert->grid_coords[0];
}

/* Version of reshape_subdiv_refine() which uses coarse position from original grids. */
static void reshape_subdiv_refine_orig_P(
    const MultiresReshapeSmoothContext *reshape_smooth_context,
    const Vertex *vert,
    blender::float3 &r_P)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const GridCoord *grid_coord = reshape_subdiv_refine_vert_grid_coord(vert);

  /* Check whether this is a loose vertex. */
  if (grid_coord == nullptr) {
    r_P = blender::float3(0.0f);
    return;
  }

  blender::float3 limit_P;
  blender::float3x3 tangent_matrix;
  multires_reshape_evaluate_base_mesh_limit_at_grid(
      reshape_context, grid_coord, limit_P, tangent_matrix);

  const ReshapeConstGridElement orig_grid_element =
      multires_reshape_orig_grid_element_for_grid_coord(reshape_context, grid_coord);

  const blender::float3 D = blender::math::transform_direction(tangent_matrix,
                                                               orig_grid_element.displacement);

  r_P = limit_P + D;
}
static void reshape_subdiv_refine_orig(const MultiresReshapeSmoothContext *reshape_smooth_context)
{
  reshape_subdiv_refine(reshape_smooth_context, reshape_subdiv_refine_orig_P);
}

/* Version of reshape_subdiv_refine() which uses coarse position from final grids. */
static void reshape_subdiv_refine_final_P(
    const MultiresReshapeSmoothContext *reshape_smooth_context,
    const Vertex *vert,
    blender::float3 &r_P)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const GridCoord *grid_coord = reshape_subdiv_refine_vert_grid_coord(vert);

  /* Check whether this is a loose vertex. */
  if (grid_coord == nullptr) {
    r_P = blender::float3(0.0f);
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
    blender::float3 &limit_P,
    blender::float3x3 &r_tangent_matrix)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  blender::float3 dPdu;
  blender::float3 dPdv;
  blender::bke::subdiv::eval_limit_point_and_derivatives(reshape_smooth_context->reshape_subdiv,
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
  const int reshape_level_grid_size = blender::bke::subdiv::grid_size_from_level(reshape_level);
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
        blender::float3 limit_P;
        blender::float3x3 tangent_matrix;
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
    blender::float3 &r_orig_final_P)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  /* Element of an original MDISPS grid) */
  const ReshapeConstGridElement orig_grid_element =
      multires_reshape_orig_grid_element_for_grid_coord(reshape_context, grid_coord);

  /* Limit surface of the base mesh. */
  blender::float3 base_mesh_limit_P;
  blender::float3x3 base_mesh_tangent_matrix;
  multires_reshape_evaluate_base_mesh_limit_at_grid(
      reshape_context, grid_coord, base_mesh_limit_P, base_mesh_tangent_matrix);

  /* Convert original displacement from tangent space to object space. */
  const blender::float3 orig_displacement = blender::math::transform_direction(
      base_mesh_tangent_matrix, orig_grid_element.displacement);

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
        blender::float3 orig_final_P;
        evaluate_final_original_point(reshape_smooth_context, grid_coord, orig_final_P);

        /* Original surface point on sculpt level (sculpt level before edits in sculpt mode). */
        const SurfacePoint *orig_sculpt_point = base_surface_grids_read(reshape_smooth_context,
                                                                        grid_coord);

        /* Difference between original top level and original sculpt level in object space. */
        const blender::float3 original_detail_delta = orig_final_P - orig_sculpt_point->P;

        /* Difference between original top level and original sculpt level in tangent space of
         * original sculpt level. */
        const blender::float3x3 original_sculpt_tangent_matrix_inv = blender::math::invert(
            orig_sculpt_point->tangent_matrix);
        blender::float3 original_detail_delta_tangent = blender::math::transform_direction(
            original_sculpt_tangent_matrix_inv, original_detail_delta);

        /* Limit surface of smoothed (subdivided) edited sculpt level. */
        blender::float3 smooth_limit_P;
        blender::float3x3 smooth_tangent_matrix;
        reshape_subdiv_evaluate_limit_at_grid(
            reshape_smooth_context, ptex_coord, grid_coord, smooth_limit_P, smooth_tangent_matrix);

        /* Add original detail to the smoothed surface. */
        blender::float3 smooth_delta = blender::math::transform_direction(
            smooth_tangent_matrix, original_detail_delta_tangent);

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
        blender::bke::subdiv::Subdiv *reshape_subdiv = reshape_smooth_context->reshape_subdiv;

        ReshapeGridElement grid_element = multires_reshape_grid_element_for_grid_coord(
            reshape_context, grid_coord);

        /* Surface. */
        const blender::float3 P = blender::bke::subdiv::eval_limit_point(
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
