/* SPDX-FileCopyrightText: 2012-2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 *
 * Baker from the Multires
 * =======================
 *
 * This file is an implementation of a special baking mode which bakes data (normals, displacement)
 * from the highest multi-resolution level to the current viewport subdivision level.
 *
 * The initial historical reasoning for having such baker was:
 * - Lower memory footprint than the regular baker.
 * - Performance (due to lower overhead compared to the regular baker at that time).
 * - Ease of use: no need to have explicit object to define cage.
 * Over the time some of these points became less relevant, but the ease of use is still there.
 *
 * The general idea of the algorithm is pretty simple:
 * - Rasterize UV of the mesh at the bake level.
 * - For every UV pixel that is rasterized, figure out attributes on the bake level mesh and the
 *   highest subdivision multi-resolution level (such as normal, position).
 * - Do the math (like convert normal to the tangent space),
 * - Write pixel to the image.
 *
 * SubdivCCG is used to access attributes at the highest multi-resolution subdivision level.
 *
 * The core rasterization logic works on triangles and those triangles are fed to the rasterizer in
 * a way that makes it easy to sample attributes in the SubdivCCG:
 * - Triangle knows which CCG index it corresponds to (triangle never covers multiple grids).
 * - It also knows UV coordinates of its vertices within that grid.
 *
 * The way triangles are calculated when baking to the base level is pretty straightforward:
 * - Triangles are actually calculated from a quad.
 * - Quad vertices align with the grid vertices.
 * This means that the top level loop iterates over face corners, calculates quad for the grids,
 * and passes it to the triangle rasterization.
 *
 * When baking to a non-0 subdivision level a special trick is used to know grid index and its UV
 * coordinates in the base mesh: for every loop in the bake-level mesh the algorithm calculates
 * this information using subdiv's foreach logic. This assumes that the bake level mesh is
 * calculated using the same foreach logic.
 *
 * Use low resolution mesh
 * -----------------------
 *
 * This is a special option for the displacement baker.
 *
 * When it is ON: displacement is calculated between the multi-resolution at the highest
 * subdivision level and the bake-level mesh.
 *
 * When it is OFF: displacement is calculated between the multi-resolution at the highest
 * subdivision level and a mesh which is created from the bake level mesh by subdividing it further
 * to reach the same subdivision level of the highest multi-resolution level. Additionally, the
 * texture UV, and UV tangents are used from this "special" mesh.
 *
 * Possible optimizations
 * ----------------------
 *
 * - Reuse mesh from the viewport as bake-level mesh.
 *
 *   It could be a bit challenging since mesh could be in sculpt mode, where it has own SubdivCCG
 *   and does not have UV map on the subdivided state. Additionally, it will make it harder to
 *   calculate tangent space as well.
 */

#include "RE_multires_bake.h"

#include <atomic>

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"

#include "BLI_array.hh"
#include "BLI_listbase.h"
#include "BLI_math_base.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"
#include "BLI_threads.h"

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_tangent.hh"
#include "BKE_multires.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_ccg.hh"
#include "BKE_subdiv_eval.hh"
#include "BKE_subdiv_foreach.hh"
#include "BKE_subdiv_mesh.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "DEG_depsgraph.hh"

#include "RE_texture_margin.h"

namespace blender::render {
namespace {

namespace subdiv = bke::subdiv;

/* -------------------------------------------------------------------- */
/** \name Math utilities that should actually be in the BLI
 * The only reason they are here is that there is currently no great place to put them to.
 * \{ */

template<class T> T interp_barycentric_triangle(const T data[3], const float2 &uv)
{
  return data[0] * uv.x + data[1] * uv.y + data[2] * (1.0f - uv.x - uv.y);
}

template<class T>
T interp_bilinear_quad(
    const float u, const float v, const T &p0, const T &p1, const T &p2, const T &p3)
{
  const float w0 = (1 - u) * (1 - v);
  const float w1 = u * (1 - v);
  const float w2 = u * v;
  const float w3 = (1 - u) * v;

  return p0 * w0 + p1 * w1 + p2 * w2 + p3 * w3;
}

float2 resolve_tri_uv(const float2 &st, const float2 &st0, const float2 &st1, const float2 &st2)
{
  float2 uv;
  resolve_tri_uv_v2(uv, st, st0, st1, st2);
  return uv;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implementation of data accessor from the subdiv
 * \{ */

template<class T> class Grid {
  Span<T> data_;
  int side_size_;

  T get_element(const int x, const int y) const
  {
    const int64_t index = int64_t(y) * side_size_ + x;
    return data_[index];
  }

 public:
  Grid(const Span<T> data, const int side_size) : data_(data), side_size_(side_size)
  {
    BLI_assert(data.size() == side_size_ * side_size_);
  }

  T sample(const float2 uv) const
  {
    const float2 xy = uv * (side_size_ - 1);

    const int x0 = int(xy.x);
    const int x1 = x0 >= (side_size_ - 1) ? (side_size_ - 1) : (x0 + 1);

    const int y0 = int(xy.y);
    const int y1 = y0 >= (side_size_ - 1) ? (side_size_ - 1) : (y0 + 1);

    const float u = xy.x - x0;
    const float v = xy.y - y0;

    return interp_bilinear_quad(
        u, v, get_element(x0, y0), get_element(x1, y0), get_element(x1, y1), get_element(x0, y1));
  }
};

template<class T>
Grid<T> get_subdiv_ccg_grid(const SubdivCCG &subdiv_ccg, const int grid_index, const Span<T> data)
{
  const int64_t offset = int64_t(grid_index) * subdiv_ccg.grid_area;
  return Grid(Span(data.data() + offset, subdiv_ccg.grid_area), subdiv_ccg.grid_size);
}

float3 sample_position_on_subdiv_ccg(const SubdivCCG &subdiv_ccg,
                                     const int grid_index,
                                     const float2 grid_uv)
{
  const Grid<float3> grid = get_subdiv_ccg_grid(
      subdiv_ccg, grid_index, subdiv_ccg.positions.as_span());
  return grid.sample(grid_uv);
}

float3 sample_normal_on_subdiv_ccg(const SubdivCCG &subdiv_ccg,
                                   const int grid_index,
                                   const float2 grid_uv)
{
  /* TODO(sergey): Support flat normals.
   * It seems that the baker always used smooth interpolation for CCG. */
  const Grid<float3> grid = get_subdiv_ccg_grid(
      subdiv_ccg, grid_index, subdiv_ccg.normals.as_span());
  return grid.sample(grid_uv);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Storage of mesh arrays, for quicker access without any lookup
 * \{ */

struct MeshArrays {
  Span<float3> vert_positions;
  Span<float3> vert_normals;

  Span<int> corner_verts;
  Span<int3> corner_tris;
  Span<float3> corner_normals;

  Span<int> tri_faces;

  OffsetIndices<int> faces;
  Span<float3> face_normals;
  VArraySpan<bool> sharp_faces;

  VArraySpan<float2> uv_map;

  VArraySpan<int> material_indices;

  MeshArrays() = default;

  explicit MeshArrays(const Mesh &mesh)
  {
    bke::AttributeAccessor attributes = mesh.attributes();

    const StringRef active_uv_map = mesh.active_uv_map_name();
    vert_positions = mesh.vert_positions();
    vert_normals = mesh.vert_normals();

    corner_verts = mesh.corner_verts();
    corner_tris = mesh.corner_tris();
    corner_normals = mesh.corner_normals();

    tri_faces = mesh.corner_tri_faces();

    faces = mesh.faces();
    face_normals = mesh.face_normals();
    sharp_faces = *attributes.lookup_or_default<bool>("sharp_face", bke::AttrDomain::Face, false);

    uv_map = *attributes.lookup<float2>(active_uv_map, bke::AttrDomain::Corner);

    material_indices = *attributes.lookup_or_default<int>(
        "material_index", bke::AttrDomain::Face, 0);
  }
};

/* Calculate UV map coordinates at the center of the face (grid coordinates (0, 0)). */
static float2 face_center_tex_uv_calc(const MeshArrays &mesh_arrays, const int face_index)
{
  const IndexRange &face = mesh_arrays.faces[face_index];
  float2 tex_uv_acc(0.0f, 0.0f);
  for (const int corner : face) {
    tex_uv_acc += mesh_arrays.uv_map[corner];
  }
  return tex_uv_acc / face.size();
}

/* Calculate smooth normal coordinates at the center of the face (grid coordinates (0, 0)).
 * NOTE: The returned value is not normalized to allow linear interpolation with other grid
 * elements. */
static float3 face_center_smooth_normal_calc(const MeshArrays &mesh_arrays, const int face_index)
{
  const IndexRange &face = mesh_arrays.faces[face_index];
  float3 normal_acc(0.0f, 0.0f, 0.0f);
  for (const int corner : face) {
    normal_acc += mesh_arrays.vert_normals[mesh_arrays.corner_verts[corner]];
  }
  /* NOTE: No normalization here: do it after interpolation at the baking point.
   *
   * This preserves linearity of operation. If normalization is done here interpolation will go
   * wrong. */
  return normal_acc / face.size();
}

/* Calculate tangent space for the given mesh state. */
Array<float4> calc_uv_tangents(const MeshArrays &mesh_arrays)
{
  Array<Array<float4>> tangent_data = bke::mesh::calc_uv_tangents(mesh_arrays.vert_positions,
                                                                  mesh_arrays.faces,
                                                                  mesh_arrays.corner_verts,
                                                                  mesh_arrays.corner_tris,
                                                                  mesh_arrays.tri_faces,
                                                                  mesh_arrays.sharp_faces,
                                                                  mesh_arrays.vert_normals,
                                                                  mesh_arrays.face_normals,
                                                                  mesh_arrays.corner_normals,
                                                                  {mesh_arrays.uv_map});

  return tangent_data[0];
}

/* Calculate tangent space at the center of the face (grid coordinates (0, 0)). */
static float4 face_center_uv_tangent_calc(const MeshArrays &mesh_arrays,
                                          const Span<float4> uv_tangents,
                                          const int face_index)
{
  const IndexRange &face = mesh_arrays.faces[face_index];
  float4 tex_uv_acc(0.0f, 0.0f, 0.0f, 0.0f);
  for (const int corner : face) {
    tex_uv_acc += uv_tangents[corner];
  }
  return tex_uv_acc / face.size();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common data types and utilities
 * \{ */

struct ExtraBuffers {
  Array<float> displacement_buffer;
  Array<char> mask_buffer;
};

struct RasterizeTile {
  ImBuf *ibuf = nullptr; /* Image buffer of the tile. */
  ExtraBuffers *extra_buffers = nullptr;
  float2 uv_offset; /* UV coordinate of the tile origin. */
};

struct RasterizeTriangle {
  /* UV coordinates with the CCG. All vertices belong to the same grid. */
  int grid_index;
  float2 grid_uvs[3];

  /* UV texture coordinates of the triangle vertices within the tile. */
  float2 tex_uvs[3];

  /* Positions and normals of the vertices, at the bake level. */
  float3 positions[3];
  float3 normals[3];

  /* Triangle is shaded flat: it has the same normal at every point of its surface.
   * Face normal is stored in all elements of the normals array. */
  bool is_flat;

  /* Optional tangents.
   * The uv_tangents might be uninitialized if has_uv_tangents=false. */
  bool has_uv_tangents;
  float4 uv_tangents[3];

  float3 get_position(const float2 &uv) const
  {
    return interp_barycentric_triangle(positions, uv);
  }

  float3 get_normal(const float2 &uv) const
  {
    if (is_flat) {
      return normals[0];
    }
    return math::normalize(interp_barycentric_triangle(normals, uv));
  }
};

struct RasterizeQuad {
  /* UV coordinates with the CCG. All vertices belong to the same grid. */
  int grid_index;
  float2 grid_uvs[4];

  /* UV texture coordinates of the triangle vertices within the tile. */
  float2 tex_uvs[4];

  /* Positions and normals of the vertices, at the bake level. */
  float3 positions[4];
  float3 normals[4];

  /* Quad is shaded flat: it has the same normal at every point of its surface.
   * Face normal is stored in all elements of the normals array. */
  bool is_flat;

  /* Optional tangents.
   * The uv_tangents might be uninitialized if has_uv_tangents=false. */
  bool has_uv_tangents;
  float4 uv_tangents[4];
};

struct RasterizeResult {
  float height_min = FLT_MAX;
  float height_max = -FLT_MAX;
};

struct BakedImBuf {
  Image *image;
  ImBuf *ibuf;
  ExtraBuffers extra_buffers;
  float2 uv_offset;
};

struct MultiresBakeResult {
  Vector<BakedImBuf> baked_ibufs;

  /* Bake-level mesh subdivided to the final multi-resolution level.
   * It is created by displacement baker that used "Use Low Resolution Mesh" OFF.
   *
   * This mesh is to be used to filter baked images. */
  const Mesh *highres_bake_mesh = nullptr;

  /* Minimum and maximum height during displacement baking. */
  float height_min = FLT_MAX;
  float height_max = -FLT_MAX;
};

class MultiresBaker {
 public:
  virtual ~MultiresBaker() = default;

  virtual float3 bake_pixel(const RasterizeTriangle &triangle,
                            const float2 &bary_uv,
                            const float2 &grid_uv,
                            RasterizeResult &result) const = 0;

  virtual void write_pixel(const RasterizeTile &tile,
                           const int2 &coord,
                           const float3 &value) const = 0;

 protected:
  void write_pixel_to_image_buffer(ImBuf &ibuf, const int2 &coord, const float3 &value) const
  {
    const int64_t pixel = int64_t(ibuf.x) * coord.y + coord.x;

    if (ibuf.float_buffer.data) {
      /* TODO(sergey): Properly tackle ibuf.channels. */
      BLI_assert(ibuf.channels == 4);
      float *rrgbf = ibuf.float_buffer.data + pixel * 4;
      rrgbf[0] = value[0];
      rrgbf[1] = value[1];
      rrgbf[2] = value[2];
      rrgbf[3] = 1.0f;
      ibuf.userflags |= IB_RECT_INVALID;
    }

    if (ibuf.byte_buffer.data) {
      uchar *rrgb = ibuf.byte_buffer.data + pixel * 4;
      unit_float_to_uchar_clamp_v3(rrgb, value);
      rrgb[3] = 255;
    }

    ibuf.userflags |= IB_DISPLAY_BUFFER_INVALID;
  }
};

static bool multiresbake_test_break(const MultiresBakeRender &bake)
{
  if (!bake.stop) {
    /* This means baker is executed outside from job system (for example, from Python API).
     * In this case there is no need to cancel, as it will be quite strange to cancel out
     * execution of a script. */
    return false;
  }
  return *bake.stop || G.is_break;
}

static float2 get_tile_uv(Image &image, ImageTile &tile)
{
  float uv_offset[2];
  BKE_image_get_tile_uv(&image, tile.tile_number, uv_offset);
  return uv_offset;
}

static bool need_tangent(const MultiresBakeRender &bake)
{
  return (bake.type == R_BAKE_NORMALS) || (bake.type == R_BAKE_VECTOR_DISPLACEMENT &&
                                           bake.displacement_space == R_BAKE_SPACE_TANGENT);
}

/* Get matrix which converts tangent space to object space in the (tangent, bitangent, normal)
 * convention. */
static float3x3 get_from_tangent_matrix_tbn(const RasterizeTriangle &triangle,
                                            const float2 &bary_uv)
{
  if (!triangle.has_uv_tangents) {
    return float3x3::identity();
  }

  const float u = bary_uv.x;
  const float v = bary_uv.y;
  const float w = 1 - u - v;

  const float3 &no0 = triangle.normals[0];
  const float3 &no1 = triangle.normals[1];
  const float3 &no2 = triangle.normals[2];

  const float4 &tang0 = triangle.uv_tangents[0];
  const float4 &tang1 = triangle.uv_tangents[1];
  const float4 &tang2 = triangle.uv_tangents[2];

  /* The sign is the same at all face vertices for any non-degenerate face.
   * Just in case we clamp the interpolated value though. */
  const float sign = (tang0.w * u + tang1.w * v + tang2.w * w) < 0 ? (-1.0f) : 1.0f;

  /* x - tangent
   * y - bitangent (B = sign * cross(N, T))
   * z - normal */
  float3x3 from_tangent;
  from_tangent.x = tang0.xyz() * u + tang1.xyz() * v + tang2.xyz() * w;
  from_tangent.z = no0.xyz() * u + no1.xyz() * v + no2.xyz() * w;
  from_tangent.y = sign * math::cross(from_tangent.z, from_tangent.x);

  return from_tangent;
}

/* Get matrix which converts object space to tangent space in the (tangent, bitangent, normal)
 * convention. */
static float3x3 get_to_tangent_matrix_tbn(const RasterizeTriangle &triangle, const float2 &bary_uv)
{
  const float3x3 from_tangent = get_from_tangent_matrix_tbn(triangle, bary_uv);
  return math::invert(from_tangent);
}

/* Get matrix which converts object space to tangent space in the (tangent, normal, bitangent)
 * convention. */
static float3x3 get_to_tangent_matrix_tnb(const RasterizeTriangle &triangle, const float2 &bary_uv)
{
  float3x3 from_tangent = get_from_tangent_matrix_tbn(triangle, bary_uv);
  std::swap(from_tangent.y, from_tangent.z);
  return math::invert(from_tangent);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Baking pipeline
 * \{ */

static void flush_pixel(const MultiresBaker &baker,
                        const RasterizeTile &tile,
                        const RasterizeTriangle &triangle,
                        const int x,
                        const int y,
                        RasterizeResult &result)
{
  const float2 st{(x + 0.5f) / tile.ibuf->x, (y + 0.5f) / tile.ibuf->y};

  const float2 bary_uv = resolve_tri_uv(
      st, triangle.tex_uvs[0], triangle.tex_uvs[1], triangle.tex_uvs[2]);
  const float2 grid_uv = interp_barycentric_triangle(triangle.grid_uvs, bary_uv);

  const float3 baked_pixel = baker.bake_pixel(triangle, bary_uv, grid_uv, result);
  baker.write_pixel(tile, int2(x, y), baked_pixel);
}

static void set_rast_triangle(const MultiresBaker &baker,
                              const RasterizeTile &tile,
                              const RasterizeTriangle &triangle,
                              const int x,
                              const int y,
                              RasterizeResult &result)
{
  const int w = tile.ibuf->x;
  const int h = tile.ibuf->y;

  if (x >= 0 && x < w && y >= 0 && y < h) {
    const int64_t pixel = int64_t(y) * w + x;
    if (tile.extra_buffers->mask_buffer[pixel] == FILTER_MASK_NULL) {
      tile.extra_buffers->mask_buffer[pixel] = FILTER_MASK_USED;
      flush_pixel(baker, tile, triangle, x, y, result);
    }
  }
}

static void rasterize_half(const MultiresBaker &baker,
                           const RasterizeTile &tile,
                           const RasterizeTriangle &triangle,
                           const float2 &s0,
                           const float2 &s1,
                           const float2 &l0,
                           const float2 &l1,
                           const int y0_in,
                           const int y1_in,
                           const bool is_mid_right,
                           RasterizeResult &result)
{
  const bool s_stable = fabsf(s1.y - s0.y) > FLT_EPSILON;
  const bool l_stable = fabsf(l1.y - l0.y) > FLT_EPSILON;
  const int w = tile.ibuf->x;
  const int h = tile.ibuf->y;

  if (y1_in <= 0 || y0_in >= h) {
    return;
  }

  const int y0 = y0_in < 0 ? 0 : y0_in;
  const int y1 = y1_in >= h ? h : y1_in;

  for (int y = y0; y < y1; y++) {
    /*-b(x-x0) + a(y-y0) = 0 */
    float x_l = s_stable ? (s0.x + (((s1.x - s0.x) * (y - s0.y)) / (s1.y - s0.y))) : s0.x;
    float x_r = l_stable ? (l0.x + (((l1.x - l0.x) * (y - l0.y)) / (l1.y - l0.y))) : l0.x;
    if (is_mid_right) {
      std::swap(x_l, x_r);
    }

    int iXl = int(ceilf(x_l));
    int iXr = int(ceilf(x_r));

    if (iXr > 0 && iXl < w) {
      iXl = iXl < 0 ? 0 : iXl;
      iXr = iXr >= w ? w : iXr;

      for (int x = iXl; x < iXr; x++) {
        set_rast_triangle(baker, tile, triangle, x, y, result);
      }
    }
  }
}

static void rasterize_triangle(const MultiresBaker &baker,
                               const RasterizeTile &tile,
                               const RasterizeTriangle &triangle,
                               RasterizeResult &result)
{
  const float2 ibuf_size(tile.ibuf->x, tile.ibuf->y);

  const float2 &st0_in = triangle.tex_uvs[0];
  const float2 &st1_in = triangle.tex_uvs[1];
  const float2 &st2_in = triangle.tex_uvs[2];

  float2 p_low = st0_in * ibuf_size - 0.5f;
  float2 p_mid = st1_in * ibuf_size - 0.5f;
  float2 p_high = st2_in * ibuf_size - 0.5f;

  /* Skip degenerates. */
  if ((p_low.x == p_mid.x && p_low.y == p_mid.y) || (p_low.x == p_mid.x && p_low.y == p_high.y) ||
      (p_mid.x == p_high.x && p_mid.y == p_high.y))
  {
    return;
  }

  /* Sort by T. */
  if (p_low.y > p_mid.y && p_low.y > p_high.y) {
    std::swap(p_high.x, p_low.x);
    std::swap(p_high.y, p_low.y);
  }
  else if (p_mid.y > p_high.y) {
    std::swap(p_high.x, p_mid.x);
    std::swap(p_high.y, p_mid.y);
  }

  if (p_low.y > p_mid.y) {
    std::swap(p_low.x, p_mid.x);
    std::swap(p_low.y, p_mid.y);
  }

  /* Check if mid-point is to the left or to the right of the lo-hi edge. */
  const bool is_mid_right = math::cross(p_mid - p_high, p_high - p_low) > 0;
  const int ylo = int(ceilf(p_low.y));
  const int yhi_beg = int(ceilf(p_mid.y));
  const int yhi = int(ceilf(p_high.y));

  rasterize_half(
      baker, tile, triangle, p_low, p_mid, p_low, p_high, ylo, yhi_beg, is_mid_right, result);
  rasterize_half(
      baker, tile, triangle, p_mid, p_high, p_low, p_high, yhi_beg, yhi, is_mid_right, result);
}

static void rasterize_quad(const MultiresBaker &baker,
                           const RasterizeTile &tile,
                           const RasterizeQuad &quad,
                           RasterizeResult &result)
{
  RasterizeTriangle triangle;
  triangle.grid_index = quad.grid_index;
  triangle.is_flat = quad.is_flat;
  triangle.has_uv_tangents = quad.has_uv_tangents;

  const int3 quad_split_data[2] = {{0, 1, 2}, {2, 3, 0}};
  for (const int3 &triangle_idx : Span(quad_split_data, 2)) {
    triangle.grid_uvs[0] = quad.grid_uvs[triangle_idx.x];
    triangle.grid_uvs[1] = quad.grid_uvs[triangle_idx.y];
    triangle.grid_uvs[2] = quad.grid_uvs[triangle_idx.z];

    triangle.tex_uvs[0] = quad.tex_uvs[triangle_idx.x];
    triangle.tex_uvs[1] = quad.tex_uvs[triangle_idx.y];
    triangle.tex_uvs[2] = quad.tex_uvs[triangle_idx.z];

    triangle.positions[0] = quad.positions[triangle_idx.x];
    triangle.positions[1] = quad.positions[triangle_idx.y];
    triangle.positions[2] = quad.positions[triangle_idx.z];

    triangle.normals[0] = quad.normals[triangle_idx.x];
    triangle.normals[1] = quad.normals[triangle_idx.y];
    triangle.normals[2] = quad.normals[triangle_idx.z];

    if (triangle.has_uv_tangents) {
      triangle.uv_tangents[0] = quad.uv_tangents[triangle_idx.x];
      triangle.uv_tangents[1] = quad.uv_tangents[triangle_idx.y];
      triangle.uv_tangents[2] = quad.uv_tangents[triangle_idx.z];
    }

    rasterize_triangle(baker, tile, triangle, result);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Displacement Baker
 * \{ */

class MultiresDisplacementBaker : public MultiresBaker {
  const SubdivCCG &high_subdiv_ccg_;

 public:
  MultiresDisplacementBaker(const SubdivCCG &subdiv_ccg,
                            const ImBuf &ibuf,
                            ExtraBuffers &extra_buffers)
      : high_subdiv_ccg_(subdiv_ccg)
  {
    extra_buffers.displacement_buffer.reinitialize(IMB_get_pixel_count(&ibuf));
    extra_buffers.displacement_buffer.fill(0);
  }

  float3 bake_pixel(const RasterizeTriangle &triangle,
                    const float2 &bary_uv,
                    const float2 &grid_uv,
                    RasterizeResult &result) const override
  {
    const float3 bake_level_position = triangle.get_position(bary_uv);
    const float3 bake_level_normal = triangle.get_normal(bary_uv);
    const float3 high_level_position = sample_position_on_subdiv_ccg(
        high_subdiv_ccg_, triangle.grid_index, grid_uv);

    const float length = math::dot(bake_level_normal, (high_level_position - bake_level_position));

    result.height_min = math::min(result.height_min, length);
    result.height_max = math::max(result.height_max, length);

    return {length, length, length};
  }

  void write_pixel(const RasterizeTile &tile,
                   const int2 &coord,
                   const float3 &value) const override
  {
    const ImBuf &ibuf = *tile.ibuf;

    const int64_t pixel = int64_t(ibuf.x) * coord.y + coord.x;
    tile.extra_buffers->displacement_buffer[pixel] = value.x;

    write_pixel_to_image_buffer(*tile.ibuf, coord, value);
  }
};

class MultiresVectorDisplacementBaker : public MultiresBaker {
  const SubdivCCG &high_subdiv_ccg_;
  eBakeSpace space_;

 public:
  MultiresVectorDisplacementBaker(const SubdivCCG &subdiv_ccg, const eBakeSpace space)
      : high_subdiv_ccg_(subdiv_ccg), space_(space)
  {
  }

  float3 bake_pixel(const RasterizeTriangle &triangle,
                    const float2 &bary_uv,
                    const float2 &grid_uv,
                    RasterizeResult & /*result*/) const override
  {
    const float3 bake_level_position = triangle.get_position(bary_uv);

    const float3 high_level_position = sample_position_on_subdiv_ccg(
        high_subdiv_ccg_, triangle.grid_index, grid_uv);

    const float3 displacement = high_level_position - bake_level_position;

    if (space_ == R_BAKE_SPACE_TANGENT) {
      const float3x3 to_tangent = get_to_tangent_matrix_tnb(triangle, bary_uv);
      return to_tangent * displacement;
    }

    return displacement;
  }

  void write_pixel(const RasterizeTile &tile,
                   const int2 &coord,
                   const float3 &value) const override
  {
    write_pixel_to_image_buffer(*tile.ibuf, coord, value);
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Normal Maps Baker
 * \{ */

class MultiresNormalsBaker : public MultiresBaker {
  const SubdivCCG &subdiv_ccg_;

 public:
  explicit MultiresNormalsBaker(const SubdivCCG &subdiv_ccg) : subdiv_ccg_(subdiv_ccg) {}

  float3 bake_pixel(const RasterizeTriangle &triangle,
                    const float2 &bary_uv,
                    const float2 &grid_uv,
                    RasterizeResult & /*result*/) const override
  {
    const float3x3 to_tangent = get_to_tangent_matrix_tbn(triangle, bary_uv);
    const float3 normal = sample_normal_on_subdiv_ccg(subdiv_ccg_, triangle.grid_index, grid_uv);
    return math::normalize(to_tangent * normal) * 0.5f + float3(0.5f, 0.5f, 0.5f);
  }

  void write_pixel(const RasterizeTile &tile,
                   const int2 &coord,
                   const float3 &value) const override
  {
    write_pixel_to_image_buffer(*tile.ibuf, coord, value);
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image initialization
 * \{ */

static void initialize_images(MultiresBakeRender &bake)
{
  bake.images.clear();

  for (Image *image : bake.ob_image) {
    if (image) {
      bake.images.add(image);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bake to base (non-subdivided) mesh
 * \{ */

static std::unique_ptr<MultiresBaker> create_baker(const MultiresBakeRender &bake,
                                                   const SubdivCCG &subdiv_ccg,
                                                   const ImBuf &ibuf,
                                                   ExtraBuffers &extra_buffers)
{
  switch (bake.type) {
    case R_BAKE_NORMALS:
      return std::make_unique<MultiresNormalsBaker>(subdiv_ccg);
    case R_BAKE_DISPLACEMENT:
      return std::make_unique<MultiresDisplacementBaker>(subdiv_ccg, ibuf, extra_buffers);
    case R_BAKE_VECTOR_DISPLACEMENT:
      return std::make_unique<MultiresVectorDisplacementBaker>(subdiv_ccg,
                                                               bake.displacement_space);
    case R_BAKE_AO:
      /* Not implemented, should not be used. */
      break;
  }
  BLI_assert_unreachable();
  return nullptr;
}

static void rasterize_base_face(const MultiresBaker &baker,
                                const RasterizeTile &tile,
                                const MeshArrays &mesh_arrays,
                                const Span<float4> uv_tangents,
                                const int face_index,
                                RasterizeResult &result)
{
  const IndexRange &face = mesh_arrays.faces[face_index];
  const Span<int> face_verts = mesh_arrays.corner_verts.slice(face);

  RasterizeQuad quad;

  /* - Grid coordinate (0, 0): face center.
   * - Grid axis U points from the face center to the middle of the edge connecting corner to
   *   next_corner.
   * - Grid axis V points from the face center to the middle of the edge connecting prev_corner to
   *   corner. */
  quad.grid_uvs[0] = float2(0.0f, 0.0f);
  quad.grid_uvs[1] = float2(1.0f, 0.0f);
  quad.grid_uvs[2] = float2(1.0f, 1.0f);
  quad.grid_uvs[3] = float2(0.0f, 1.0f);

  quad.tex_uvs[0] = face_center_tex_uv_calc(mesh_arrays, face_index) - tile.uv_offset;
  quad.positions[0] = bke::mesh::face_center_calc(mesh_arrays.vert_positions, face_verts);

  /* TODO(sergey): Support corner normals. */

  quad.is_flat = mesh_arrays.sharp_faces[face_index];
  if (quad.is_flat) {
    quad.normals[0] = mesh_arrays.face_normals[face_index];
  }
  else {
    quad.normals[0] = face_center_smooth_normal_calc(mesh_arrays, face_index);
  }

  quad.has_uv_tangents = !uv_tangents.is_empty();
  if (quad.has_uv_tangents) {
    quad.uv_tangents[0] = face_center_uv_tangent_calc(mesh_arrays, uv_tangents, face_index);
  }

  for (const int corner : face) {
    const int prev_corner = bke::mesh::face_corner_prev(face, corner);
    const int next_corner = bke::mesh::face_corner_next(face, corner);

    const float3 &position = mesh_arrays.vert_positions[mesh_arrays.corner_verts[corner]];
    const float3 &next_position =
        mesh_arrays.vert_positions[mesh_arrays.corner_verts[next_corner]];
    const float3 &prev_position =
        mesh_arrays.vert_positions[mesh_arrays.corner_verts[prev_corner]];

    quad.grid_index = corner;

    quad.tex_uvs[1] = (mesh_arrays.uv_map[corner] + mesh_arrays.uv_map[next_corner]) * 0.5f -
                      tile.uv_offset;
    quad.tex_uvs[2] = mesh_arrays.uv_map[corner] - tile.uv_offset;
    quad.tex_uvs[3] = (mesh_arrays.uv_map[prev_corner] + mesh_arrays.uv_map[corner]) * 0.5f -
                      tile.uv_offset;

    quad.positions[1] = (position + next_position) * 0.5f;
    quad.positions[2] = position;
    quad.positions[3] = (prev_position + position) * 0.5f;

    if (quad.is_flat) {
      quad.normals[1] = quad.normals[0];
      quad.normals[2] = quad.normals[0];
      quad.normals[3] = quad.normals[0];
    }
    else {
      const float3 &normal = mesh_arrays.vert_normals[mesh_arrays.corner_verts[corner]];
      const float3 &next_normal = mesh_arrays.vert_normals[mesh_arrays.corner_verts[next_corner]];
      const float3 &prev_normal = mesh_arrays.vert_normals[mesh_arrays.corner_verts[prev_corner]];

      /* NOTE: No normalization here: do it after interpolation at the baking point.
       *
       * This preserves linearity of operation. If normalization is done here interpolation will go
       * wrong. */
      quad.normals[1] = (normal + next_normal) * 0.5f;
      quad.normals[2] = normal;
      quad.normals[3] = (prev_normal + normal) * 0.5f;
    }

    if (quad.has_uv_tangents) {
      const float4 &tangent = uv_tangents[corner];
      const float4 &next_tangent = uv_tangents[next_corner];
      const float4 &prev_tangent = uv_tangents[prev_corner];

      quad.uv_tangents[1] = (tangent + next_tangent) * 0.5f;
      quad.uv_tangents[2] = tangent;
      quad.uv_tangents[3] = (prev_tangent + tangent) * 0.5f;
    }

    rasterize_quad(baker, tile, quad, result);
  }
}

static void bake_single_image_to_base_mesh(MultiresBakeRender &bake,
                                           const Mesh &bake_level_mesh,
                                           const SubdivCCG &subdiv_ccg,
                                           Image &image,
                                           ImageTile &image_tile,
                                           ImBuf &ibuf,
                                           ExtraBuffers &extra_buffers,
                                           MultiresBakeResult &result)
{
  std::unique_ptr<MultiresBaker> baker = create_baker(bake, subdiv_ccg, ibuf, extra_buffers);
  if (!baker) {
    return;
  }

  MeshArrays mesh_arrays(bake_level_mesh);
  Array<float4> uv_tangents;
  if (need_tangent(bake)) {
    uv_tangents = calc_uv_tangents(mesh_arrays);
  }

  RasterizeTile tile;
  tile.ibuf = &ibuf;
  tile.extra_buffers = &extra_buffers;
  tile.uv_offset = get_tile_uv(image, image_tile);

  SpinLock spin_lock;
  BLI_spin_init(&spin_lock);
  std::atomic<int> num_baked_faces = 0;
  threading::parallel_for(mesh_arrays.faces.index_range(), 1, [&](const IndexRange range) {
    for (const int64_t face_index : range) {
      if (multiresbake_test_break(bake)) {
        return;
      }

      /* Check whether the face is to be baked into the current image. */
      const int mat_nr = mesh_arrays.material_indices[face_index];
      const Image *face_image = mat_nr < bake.ob_image.size() ? bake.ob_image[mat_nr] : nullptr;
      if (face_image != &image) {
        continue;
      }

      RasterizeResult rasterize_result;
      rasterize_base_face(*baker, tile, mesh_arrays, uv_tangents, face_index, rasterize_result);

      ++num_baked_faces;
      BLI_spin_lock(&spin_lock);
      result.height_min = std::min(result.height_min, rasterize_result.height_min);
      result.height_max = std::max(result.height_max, rasterize_result.height_max);
      if (bake.do_update) {
        *bake.do_update = true;
      }
      if (bake.progress) {
        *bake.progress = (float(bake.num_baked_objects) +
                          float(num_baked_faces) / mesh_arrays.faces.size()) /
                         bake.num_total_objects;
      }
      BLI_spin_unlock(&spin_lock);
    }
  });
  BLI_spin_end(&spin_lock);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bake to subdivided mesh (base mesh with some subdivision levels)
 * \{ */

struct GridCoord {
  int grid_index;
  float2 uv;
};

static Array<GridCoord> get_subdivided_corner_grid_coords(subdiv::Subdiv &subdiv,
                                                          const Mesh &coarse_mesh,
                                                          const int level)
{
  struct SubdividedCornerGridCoordData {
    MeshArrays coarse_mesh_arrays;
    Array<GridCoord> corner_grid_coords;
  };

  subdiv::ToMeshSettings mesh_settings;
  mesh_settings.resolution = (1 << level) + 1;

  SubdividedCornerGridCoordData data;
  data.coarse_mesh_arrays = MeshArrays(coarse_mesh);

  subdiv::ForeachContext foreach_context;
  foreach_context.user_data = &data;

  foreach_context.topology_info = [](const subdiv::ForeachContext *context,
                                     const int /*num_vertices*/,
                                     const int /*num_edges*/,
                                     const int num_corners,
                                     const int /*num_faces*/,
                                     const int * /*subdiv_face_offset*/) -> bool {
    SubdividedCornerGridCoordData *data = static_cast<SubdividedCornerGridCoordData *>(
        context->user_data);
    data->corner_grid_coords.reinitialize(num_corners);
    return true;
  };

  foreach_context.loop = [](const subdiv::ForeachContext *context,
                            void * /*tls*/,
                            const int /*ptex_face_index*/,
                            const float u,
                            const float v,
                            const int /*coarse_corner_index*/,
                            const int coarse_face_index,
                            const int coarse_corner,
                            const int subdiv_corner_index,
                            const int /*subdiv_vert_index*/,
                            const int /*subdiv_edge_index*/) {
    SubdividedCornerGridCoordData *data = static_cast<SubdividedCornerGridCoordData *>(
        context->user_data);

    const float2 ptex_uv(u, v);
    const IndexRange coarse_face = data->coarse_mesh_arrays.faces[coarse_face_index];

    GridCoord &corner_grid_coord = data->corner_grid_coords[subdiv_corner_index];
    corner_grid_coord.grid_index = coarse_face.start() + coarse_corner;

    if (coarse_face.size() == 4) {
      corner_grid_coord.uv = subdiv::ptex_face_uv_to_grid_uv(
          subdiv::rotate_quad_to_corner(coarse_corner, ptex_uv));
    }
    else {
      corner_grid_coord.uv = subdiv::ptex_face_uv_to_grid_uv(ptex_uv);
    }
  };

  foreach_subdiv_geometry(&subdiv, &foreach_context, &mesh_settings, &coarse_mesh);

  return data.corner_grid_coords;
}

static void rasterize_subdivided_face(const MultiresBaker &baker,
                                      const RasterizeTile &tile,
                                      const MeshArrays &mesh_arrays,
                                      const Span<GridCoord> &corner_grid_coords,
                                      const Span<float4> uv_tangents,
                                      const int face_index,
                                      RasterizeResult &result)
{
  const IndexRange &face = mesh_arrays.faces[face_index];

  /* This code operates with mesh with at leats one subdivision level applied. Such mesh only has
   * quad faces as per how subdivision works. */
  BLI_assert(face.size() == 4);

  RasterizeQuad quad;

  /* TODO(sergey): Support corner normals. */

  quad.is_flat = mesh_arrays.sharp_faces[face_index];
  quad.has_uv_tangents = !uv_tangents.is_empty();
  quad.grid_index = corner_grid_coords[face.start()].grid_index;

  for (int i = 0; i < 4; ++i) {
    const int corner = face[i];
    const int vert = mesh_arrays.corner_verts[corner];

    BLI_assert(corner_grid_coords[corner].grid_index == quad.grid_index);
    quad.grid_uvs[i] = corner_grid_coords[corner].uv;

    quad.tex_uvs[i] = mesh_arrays.uv_map[corner] - tile.uv_offset;
    quad.positions[i] = mesh_arrays.vert_positions[vert];
    if (!quad.is_flat) {
      quad.normals[i] = mesh_arrays.vert_normals[vert];
    }

    if (quad.has_uv_tangents) {
      quad.uv_tangents[i] = uv_tangents[corner];
    }
  }

  if (quad.is_flat) {
    quad.normals[0] = mesh_arrays.face_normals[face_index];
    quad.normals[1] = quad.normals[0];
    quad.normals[2] = quad.normals[0];
    quad.normals[3] = quad.normals[0];
  }

  rasterize_quad(baker, tile, quad, result);
}

static void bake_single_image_to_subdivided_mesh(MultiresBakeRender &bake,
                                                 const Mesh &bake_level_mesh,
                                                 const SubdivCCG &subdiv_ccg,
                                                 Image &image,
                                                 ImageTile &image_tile,
                                                 ImBuf &ibuf,
                                                 ExtraBuffers &extra_buffers,
                                                 MultiresBakeResult &result)
{
  std::unique_ptr<MultiresBaker> baker = create_baker(bake, subdiv_ccg, ibuf, extra_buffers);
  if (!baker) {
    return;
  }

  MeshArrays mesh_arrays(bake_level_mesh);
  Array<float4> uv_tangents;
  if (need_tangent(bake)) {
    uv_tangents = calc_uv_tangents(mesh_arrays);
  }

  RasterizeTile tile;
  tile.ibuf = &ibuf;
  tile.extra_buffers = &extra_buffers;
  tile.uv_offset = get_tile_uv(image, image_tile);

  const Array<GridCoord> corner_grid_coords = get_subdivided_corner_grid_coords(
      *subdiv_ccg.subdiv, *bake.base_mesh, bake.multires_modifier->lvl);

  SpinLock spin_lock;
  BLI_spin_init(&spin_lock);
  std::atomic<int> num_baked_faces = 0;
  threading::parallel_for(mesh_arrays.faces.index_range(), 1, [&](const IndexRange range) {
    for (const int64_t face_index : range) {
      if (multiresbake_test_break(bake)) {
        return;
      }

      /* Check whether the face is to be baked into the current image. */
      const int mat_nr = mesh_arrays.material_indices[face_index];
      const Image *face_image = mat_nr < bake.ob_image.size() ? bake.ob_image[mat_nr] : nullptr;
      if (face_image != &image) {
        continue;
      }

      RasterizeResult rasterize_result;
      rasterize_subdivided_face(*baker,
                                tile,
                                mesh_arrays,
                                corner_grid_coords,
                                uv_tangents,
                                face_index,
                                rasterize_result);

      ++num_baked_faces;
      BLI_spin_lock(&spin_lock);
      result.height_min = std::min(result.height_min, rasterize_result.height_min);
      result.height_max = std::max(result.height_max, rasterize_result.height_max);
      if (bake.do_update) {
        *bake.do_update = true;
      }
      if (bake.progress) {
        *bake.progress = (float(bake.num_baked_objects) +
                          float(num_baked_faces) / mesh_arrays.faces.size()) /
                         bake.num_total_objects;
      }
      BLI_spin_unlock(&spin_lock);
    }
  });
  BLI_spin_end(&spin_lock);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name High resolution displacement baking
 * Used in cases of displacement baking with Low Resolution Mesh equals False.
 * \{ */

/* Subdivide bake_level_mesh to the level of `total level - viewport level`.
 * Essentially bring the bake_level_mesh to the same resolution level as the top multi-resolution
 * level. */
static const Mesh *create_highres_mesh(const Mesh &bake_level_mesh,
                                       const MultiresModifierData &multires_modifier)
{
  const int top_level = multires_modifier.totlvl;
  const int bake_level = multires_modifier.lvl;
  const int subdivide_level = top_level - bake_level;
  if (subdivide_level == 0) {
    return &bake_level_mesh;
  }

  subdiv::Settings subdiv_settings;
  BKE_multires_subdiv_settings_init(&subdiv_settings, &multires_modifier);
  subdiv::Subdiv *subdiv = subdiv::update_from_mesh(nullptr, &subdiv_settings, &bake_level_mesh);

  subdiv::ToMeshSettings mesh_settings;
  mesh_settings.resolution = (1 << subdivide_level) + 1;

  Mesh *result = subdiv::subdiv_to_mesh(subdiv, &mesh_settings, &bake_level_mesh);

  subdiv::free(subdiv);

  return result;
}

/* Get grid coordinates for every corner of the highres_bake_mesh. */
static Array<GridCoord> get_highres_mesh_loop_grid_coords(
    subdiv::Subdiv &subdiv,
    const MultiresModifierData &multires_modifier,
    const Mesh &base_mesh,
    const Mesh &bake_level_mesh,
    const Mesh &highres_bake_mesh)
{
  UNUSED_VARS_NDEBUG(highres_bake_mesh);

  if (multires_modifier.lvl == 0) {
    /* Simple case: baking from subdivided mesh highres_bake_mesh to the base mesh. */
    return get_subdivided_corner_grid_coords(
        subdiv, bake_level_mesh, multires_modifier.totlvl - multires_modifier.lvl);
  }

  /* More tricky case:
   * - The base_mesh is first subdivided to the viewport level (bake_level_mesh)
   * - The bake_level_mesh is then further subdivided (highres_bake_mesh).
   *
   * This case needs an extra level of indirection: map loops from the highres_bake_mesh to the
   * faces of the bake_level_mesh, and then interpolate the grid coordinates calculated for the
   * bake_level_mesh to get grid coordinates.
   *
   * The coarse mesh here is the same as bake_level_mesh, and the subdiv mesh is the same as
   * highres_bake_mesh.
   *
   * It is possible to optimize the memory usage here by utilizing an implicit knowledge about
   * how faces in the high-res mesh are created from the bake level mesh: Since the bake level
   * mesh has some amount of subdivisions in this branch all its faces are quads. So all the
   * faces in the high-res mesh are also quads, created in the grid pattern from the bake level
   * faces. */

  struct HighresCornerGridCoordData {
    MeshArrays bake_level_mesh_arrays;
    Array<GridCoord> bake_level_corner_grid_coords;

    Array<GridCoord> corner_grid_coords;
  };

  const int top_level = multires_modifier.totlvl;
  const int bake_level = multires_modifier.lvl;
  const int subdivide_level = top_level - bake_level;

  subdiv::ToMeshSettings mesh_settings;
  mesh_settings.resolution = (1 << subdivide_level) + 1;

  HighresCornerGridCoordData data;
  data.bake_level_mesh_arrays = MeshArrays(bake_level_mesh);
  data.bake_level_corner_grid_coords = get_subdivided_corner_grid_coords(
      subdiv, base_mesh, multires_modifier.lvl);

  subdiv::ForeachContext foreach_context;
  foreach_context.user_data = &data;

  foreach_context.topology_info = [](const subdiv::ForeachContext *context,
                                     const int /*num_vertices*/,
                                     const int /*num_edges*/,
                                     const int num_corners,
                                     const int /*num_faces*/,
                                     const int * /*subdiv_face_offset*/) -> bool {
    HighresCornerGridCoordData *data = static_cast<HighresCornerGridCoordData *>(
        context->user_data);
    data->corner_grid_coords.reinitialize(num_corners);
    return true;
  };

  foreach_context.loop = [](const subdiv::ForeachContext *context,
                            void * /*tls*/,
                            const int /*ptex_face_index*/,
                            const float u,
                            const float v,
                            const int /*bake_level_corner_index*/,
                            const int bake_level_face_index,
                            const int /*bake_level_corner*/,
                            const int highres_corner_index,
                            const int /*highres_vert_index*/,
                            const int /*highres_edge_index*/) {
    HighresCornerGridCoordData *data = static_cast<HighresCornerGridCoordData *>(
        context->user_data);

    const Span<GridCoord> bake_level_corner_grid_coords = data->bake_level_corner_grid_coords;

    const IndexRange bake_level_face = data->bake_level_mesh_arrays.faces[bake_level_face_index];
    BLI_assert(bake_level_face.size() == 4);

    const int bake_level_face_start = bake_level_face.start();

    GridCoord &corner_grid_coord = data->corner_grid_coords[highres_corner_index];
    corner_grid_coord.grid_index = bake_level_corner_grid_coords[bake_level_face_start].grid_index;
    corner_grid_coord.uv = interp_bilinear_quad(
        u,
        v,
        bake_level_corner_grid_coords[bake_level_face_start + 0].uv,
        bake_level_corner_grid_coords[bake_level_face_start + 1].uv,
        bake_level_corner_grid_coords[bake_level_face_start + 2].uv,
        bake_level_corner_grid_coords[bake_level_face_start + 3].uv);

    /* Loops of the bake level mesh are supposed to be in the same grid. */
    BLI_assert(corner_grid_coord.grid_index ==
               bake_level_corner_grid_coords[bake_level_face_start + 1].grid_index);
    BLI_assert(corner_grid_coord.grid_index ==
               bake_level_corner_grid_coords[bake_level_face_start + 2].grid_index);
    BLI_assert(corner_grid_coord.grid_index ==
               bake_level_corner_grid_coords[bake_level_face_start + 3].grid_index);
  };

  foreach_subdiv_geometry(&subdiv, &foreach_context, &mesh_settings, &bake_level_mesh);

  BLI_assert(data.corner_grid_coords.size() == highres_bake_mesh.corners_num);
  return data.corner_grid_coords;
}

static void bake_single_image_displacement(MultiresBakeRender &bake,
                                           const Mesh &bake_level_mesh,
                                           const SubdivCCG &subdiv_ccg,
                                           Image &image,
                                           ImageTile &image_tile,
                                           ImBuf &ibuf,
                                           ExtraBuffers &extra_buffers,
                                           MultiresBakeResult &result)
{
  std::unique_ptr<MultiresBaker> baker = create_baker(bake, subdiv_ccg, ibuf, extra_buffers);
  if (!baker) {
    return;
  }

  const Mesh *highres_bake_mesh = result.highres_bake_mesh;
  if (!highres_bake_mesh) {
    highres_bake_mesh = create_highres_mesh(bake_level_mesh, *bake.multires_modifier);
    result.highres_bake_mesh = highres_bake_mesh;
  }

  const Array<GridCoord> corner_grid_coords = get_highres_mesh_loop_grid_coords(
      *subdiv_ccg.subdiv,
      *bake.multires_modifier,
      *bake.base_mesh,
      bake_level_mesh,
      *highres_bake_mesh);

  MeshArrays mesh_arrays(*highres_bake_mesh);
  Array<float4> uv_tangents;
  if (need_tangent(bake)) {
    uv_tangents = calc_uv_tangents(mesh_arrays);
  }

  RasterizeTile tile;
  tile.ibuf = &ibuf;
  tile.extra_buffers = &extra_buffers;
  tile.uv_offset = get_tile_uv(image, image_tile);

  SpinLock spin_lock;
  BLI_spin_init(&spin_lock);
  std::atomic<int> num_baked_faces = 0;
  threading::parallel_for(mesh_arrays.faces.index_range(), 1, [&](const IndexRange range) {
    for (const int64_t face_index : range) {
      if (multiresbake_test_break(bake)) {
        return;
      }

      /* Check whether the face is to be baked into the current image. */
      const int mat_nr = mesh_arrays.material_indices[face_index];
      const Image *face_image = mat_nr < bake.ob_image.size() ? bake.ob_image[mat_nr] : nullptr;
      if (face_image != &image) {
        continue;
      }

      RasterizeResult rasterize_result;
      rasterize_subdivided_face(*baker,
                                tile,
                                mesh_arrays,
                                corner_grid_coords,
                                uv_tangents,
                                face_index,
                                rasterize_result);

      ++num_baked_faces;
      BLI_spin_lock(&spin_lock);
      result.height_min = std::min(result.height_min, rasterize_result.height_min);
      result.height_max = std::max(result.height_max, rasterize_result.height_max);
      if (bake.do_update) {
        *bake.do_update = true;
      }
      if (bake.progress) {
        *bake.progress = (float(bake.num_baked_objects) +
                          float(num_baked_faces) / mesh_arrays.faces.size()) /
                         bake.num_total_objects;
      }
      BLI_spin_unlock(&spin_lock);
    }
  });
  BLI_spin_end(&spin_lock);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image baking entry point
 * \{ */

static void bake_single_image(MultiresBakeRender &bake,
                              const Mesh &bake_level_mesh,
                              const SubdivCCG &subdiv_ccg,
                              Image &image,
                              ImageTile &image_tile,
                              ImBuf &ibuf,
                              ExtraBuffers &extra_buffers,
                              MultiresBakeResult &result)
{
  if (ELEM(bake.type, R_BAKE_DISPLACEMENT, R_BAKE_VECTOR_DISPLACEMENT) &&
      !bake.use_low_resolution_mesh &&
      bake.multires_modifier->lvl != bake.multires_modifier->totlvl)
  {
    bake_single_image_displacement(
        bake, bake_level_mesh, subdiv_ccg, image, image_tile, ibuf, extra_buffers, result);
    return;
  }

  if (bake.multires_modifier->lvl == 0) {
    bake_single_image_to_base_mesh(
        bake, bake_level_mesh, subdiv_ccg, image, image_tile, ibuf, extra_buffers, result);
    return;
  }

  bake_single_image_to_subdivided_mesh(
      bake, bake_level_mesh, subdiv_ccg, image, image_tile, ibuf, extra_buffers, result);
}

static void bake_images(MultiresBakeRender &bake,
                        const Mesh &bake_level_mesh,
                        const SubdivCCG &subdiv_ccg,
                        MultiresBakeResult &result)
{
  for (Image *image : bake.images) {
    LISTBASE_FOREACH (ImageTile *, image_tile, &image->tiles) {
      ImageUser iuser;
      BKE_imageuser_default(&iuser);
      iuser.tile = image_tile->tile_number;

      ImBuf *ibuf = BKE_image_acquire_ibuf(image, &iuser, nullptr);
      if (ibuf && ibuf->x > 0 && ibuf->y > 0) {
        result.baked_ibufs.append({});

        BakedImBuf &baked_ibuf = result.baked_ibufs.last();
        baked_ibuf.image = image;
        baked_ibuf.ibuf = ibuf;
        baked_ibuf.uv_offset = get_tile_uv(*image, *image_tile);

        ExtraBuffers &extra_buffers = baked_ibuf.extra_buffers;
        extra_buffers.mask_buffer.reinitialize(int64_t(ibuf->y) * ibuf->x);
        extra_buffers.mask_buffer.fill(FILTER_MASK_NULL);

        bake_single_image(
            bake, bake_level_mesh, subdiv_ccg, *image, *image_tile, *ibuf, extra_buffers, result);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image postprocessing
 * \{ */

static void bake_ibuf_normalize_displacement(ImBuf &ibuf,
                                             const MutableSpan<float> displacement,
                                             const Span<char> mask,
                                             const float displacement_min,
                                             const float displacement_max)
{
  const float *current_displacement = displacement.data();
  const char *current_mask = mask.data();
  const float max_distance = math::max(math::abs(displacement_min), math::abs(displacement_max));

  if (max_distance <= 1e-5f) {
    const float col[4] = {0.5f, 0.5f, 0.5f, 1.0f};
    IMB_rectfill(&ibuf, col);
    return;
  }

  /* TODO(sergey): Look into multi-threading this loop. */
  const size_t ibuf_pixel_count = IMB_get_pixel_count(&ibuf);
  for (size_t i = 0; i < ibuf_pixel_count; i++) {
    if (*current_mask == FILTER_MASK_USED) {
      const float normalized_displacement = (*current_displacement + max_distance) /
                                            (max_distance * 2);

      if (ibuf.float_buffer.data) {
        /* TODO(sergey): Properly tackle ibuf.channels. */
        BLI_assert(ibuf.channels == 4);
        float *fp = ibuf.float_buffer.data + int64_t(i) * 4;
        fp[0] = fp[1] = fp[2] = normalized_displacement;
        fp[3] = 1.0f;
      }

      if (ibuf.byte_buffer.data) {
        uchar *cp = ibuf.byte_buffer.data + int64_t(i) * 4;
        cp[0] = cp[1] = cp[2] = unit_float_to_uchar_clamp(normalized_displacement);
        cp[3] = 255;
      }
    }

    current_displacement++;
    current_mask++;
  }
}

static void bake_ibuf_filter(ImBuf &ibuf,
                             const MutableSpan<char> mask,
                             const Mesh &bake_level_mesh,
                             const int margin,
                             const eBakeMarginType margin_type,
                             const float2 uv_offset)
{
  /* NOTE: Must check before filtering. */
  const bool is_new_alpha = (ibuf.planes != R_IMF_PLANES_RGBA) && BKE_imbuf_alpha_test(&ibuf);

  if (margin) {
    switch (margin_type) {
      case R_BAKE_ADJACENT_FACES: {
        RE_generate_texturemargin_adjacentfaces(&ibuf,
                                                mask.data(),
                                                margin,
                                                &bake_level_mesh,
                                                bake_level_mesh.active_uv_map_name(),
                                                uv_offset);
        break;
      }
      default:
        /* Fall through. */
      case R_BAKE_EXTEND:
        IMB_filter_extend(&ibuf, mask.data(), margin);
        break;
    }
  }

  /* If the bake results in new alpha then change the image setting. */
  if (is_new_alpha) {
    ibuf.planes = R_IMF_PLANES_RGBA;
  }
  else {
    if (margin && ibuf.planes != R_IMF_PLANES_RGBA) {
      /* Clear alpha added by filtering. */
      IMB_rectfill_alpha(&ibuf, 1.0f);
    }
  }
}

static void finish_images(MultiresBakeRender &bake,
                          const Mesh &bake_level_mesh,
                          MultiresBakeResult &result)
{
  const bool use_displacement_buffer = bake.type == R_BAKE_DISPLACEMENT;

  for (BakedImBuf &baked_ibuf : result.baked_ibufs) {
    Image *image = baked_ibuf.image;
    ImBuf *ibuf = baked_ibuf.ibuf;

    if (use_displacement_buffer) {
      bake_ibuf_normalize_displacement(*ibuf,
                                       baked_ibuf.extra_buffers.displacement_buffer,
                                       baked_ibuf.extra_buffers.mask_buffer,
                                       result.height_min,
                                       result.height_max);
    }

    bake_ibuf_filter(*ibuf,
                     baked_ibuf.extra_buffers.mask_buffer,
                     bake_level_mesh,
                     bake.bake_margin,
                     bake.bake_margin_type,
                     baked_ibuf.uv_offset);

    ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
    BKE_image_mark_dirty(image, ibuf);

    if (ibuf->float_buffer.data) {
      ibuf->userflags |= IB_RECT_INVALID;
    }

    BKE_image_release_ibuf(image, ibuf, nullptr);
    DEG_id_tag_update(&image->id, 0);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Helpers to create mesh and CCG at requested levels
 * \{ */

static subdiv::Subdiv *create_subdiv(const Mesh &mesh,
                                     const MultiresModifierData &multires_modifier)
{
  subdiv::Settings subdiv_settings;
  BKE_multires_subdiv_settings_init(&subdiv_settings, &multires_modifier);
  subdiv::Subdiv *subdiv = subdiv::update_from_mesh(nullptr, &subdiv_settings, &mesh);
  subdiv::displacement_attach_from_multires(subdiv, &mesh, &multires_modifier);

  /* Initialization evaluation of the limit surface and the displacement. */
  if (!subdiv::eval_begin_from_mesh(subdiv, &mesh, subdiv::SUBDIV_EVALUATOR_TYPE_CPU)) {
    subdiv::free(subdiv);
    return nullptr;
  }
  subdiv::eval_init_displacement(subdiv);

  return subdiv;
}

static std::unique_ptr<SubdivCCG> create_subdiv_ccg(const Mesh &mesh,
                                                    const MultiresModifierData &multires_modifier)
{
  subdiv::Subdiv *subdiv = create_subdiv(mesh, multires_modifier);
  if (!subdiv) {
    return nullptr;
  }

  SubdivToCCGSettings settings;
  settings.resolution = (1 << multires_modifier.totlvl) + 1;
  settings.need_normal = true;
  settings.need_mask = false;

  return BKE_subdiv_to_ccg(*subdiv, settings, mesh);
}

static Mesh *create_bake_level_mesh(const Mesh &base_mesh,
                                    const MultiresModifierData &multires_modifier)
{
  subdiv::Subdiv *subdiv = create_subdiv(base_mesh, multires_modifier);
  if (!subdiv) {
    return nullptr;
  }

  subdiv::ToMeshSettings mesh_settings;
  mesh_settings.resolution = (1 << multires_modifier.lvl) + 1;
  subdiv::displacement_attach_from_multires(subdiv, &base_mesh, &multires_modifier);
  Mesh *result = subdiv::subdiv_to_mesh(subdiv, &mesh_settings, &base_mesh);
  subdiv::free(subdiv);
  return result;
}

/** \} */

}  // namespace
}  // namespace blender::render

void RE_multires_bake_images(MultiresBakeRender &bake)
{
  using namespace blender::render;

  std::unique_ptr<SubdivCCG> subdiv_ccg = create_subdiv_ccg(*bake.base_mesh,
                                                            *bake.multires_modifier);

  Mesh *bake_level_mesh = bake.base_mesh;
  if (bake.multires_modifier->lvl != 0) {
    bake_level_mesh = create_bake_level_mesh(*bake.base_mesh, *bake.multires_modifier);
  }

  MultiresBakeResult result;
  initialize_images(bake);
  bake_images(bake, *bake_level_mesh, *subdiv_ccg, result);

  const Mesh *filter_mesh = result.highres_bake_mesh ? result.highres_bake_mesh : bake_level_mesh;
  finish_images(bake, *filter_mesh, result);

  if (result.highres_bake_mesh && result.highres_bake_mesh != bake_level_mesh) {
    BKE_id_free(nullptr, const_cast<Mesh *>(result.highres_bake_mesh));
  }
  if (bake_level_mesh != bake.base_mesh) {
    BKE_id_free(nullptr, bake_level_mesh);
  }
}
