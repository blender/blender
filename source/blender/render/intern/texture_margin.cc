/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#include "BLI_assert.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_vector.hh"

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"

#include "IMB_imbuf.hh"
#include "IMB_interp.hh"

#include "MEM_guardedalloc.h"

#include "zbuf.h" /* For rasterizer (#ZSpan and associated functions). */

#include "RE_texture_margin.h"

#include <algorithm>
#include <cmath>

namespace blender::render::texturemargin {

/**
 * The map class contains both a pixel map which maps out face indices for all UV-polygons and
 * adjacency tables.
 */
class TextureMarginMap {
  static const int directions[8][2];
  static const int distances[8];

  /** Maps UV-edges to their corresponding UV-edge. */
  Vector<int> loop_adjacency_map_;
  /** Maps UV-edges to their corresponding face. */
  Array<int> loop_to_face_map_;

  int w_, h_;
  float uv_offset_[2];
  Vector<uint32_t> pixel_data_;
  ZSpan zspan_;
  uint32_t value_to_store_;
  bool write_mask_;
  char *mask_;

  OffsetIndices<int> faces_;
  Span<int> corner_edges_;
  Span<float2> uv_map_;
  int totedge_;

 public:
  TextureMarginMap(size_t w,
                   size_t h,
                   const float uv_offset[2],
                   const int totedge,
                   const OffsetIndices<int> faces,
                   const Span<int> corner_edges,
                   const Span<float2> uv_map)
      : w_(w),
        h_(h),
        faces_(faces),
        corner_edges_(corner_edges),
        uv_map_(uv_map),
        totedge_(totedge)
  {
    copy_v2_v2(uv_offset_, uv_offset);

    pixel_data_.resize(w_ * h_, 0xFFFFFFFF);

    zbuf_alloc_span(&zspan_, w_, h_);

    build_tables();
  }

  ~TextureMarginMap()
  {
    zbuf_free_span(&zspan_);
  }

  void set_pixel(int x, int y, uint32_t value)
  {
    BLI_assert(x < w_);
    BLI_assert(x >= 0);
    pixel_data_[y * w_ + x] = value;
  }

  uint32_t get_pixel(int x, int y) const
  {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) {
      return 0xFFFFFFFF;
    }

    return pixel_data_[y * w_ + x];
  }

  void rasterize_tri(float *v1, float *v2, float *v3, uint32_t value, char *mask, bool writemask)
  {
    /* NOTE: This is not thread safe, because the value to be written by the rasterizer is
     * a class member. If this is ever made multi-threaded each thread needs to get its own. */
    value_to_store_ = value;
    mask_ = mask;
    write_mask_ = writemask;
    zspan_scanconvert(
        &zspan_, this, &(v1[0]), &(v2[0]), &(v3[0]), TextureMarginMap::zscan_store_pixel);
  }

  static void zscan_store_pixel(
      void *map, int x, int y, [[maybe_unused]] float u, [[maybe_unused]] float v)
  {
    /* NOTE: Not thread safe, see comment above. */
    TextureMarginMap *m = static_cast<TextureMarginMap *>(map);
    if (m->mask_) {
      if (m->write_mask_) {
        /* if there is a mask and write_mask_ is true, write to the mask */
        m->mask_[y * m->w_ + x] = 1;
        m->set_pixel(x, y, m->value_to_store_);
      }
      else {
        /* if there is a mask and write_mask_ is false, read the mask
         * to decide if the map needs to be written
         */
        if (m->mask_[y * m->w_ + x] != 0) {
          m->set_pixel(x, y, m->value_to_store_);
        }
      }
    }
    else {
      m->set_pixel(x, y, m->value_to_store_);
    }
  }

/* The map contains 2 kinds of pixels: DijkstraPixels and face indices. The top bit determines
 * what kind it is. With the top bit set, it is a 'dijkstra' pixel. The bottom 4 bits encode the
 * direction of the shortest path and the remaining 27 bits are used to store the distance. If
 * the top bit is not set, the rest of the bits is used to store the face index.
 */
#define PackDijkstraPixel(dist, dir) (0x80000000 + ((dist) << 4) + (dir))
#define DijkstraPixelGetDistance(dp) (((dp) ^ 0x80000000) >> 4)
#define DijkstraPixelGetDirection(dp) ((dp) & 0xF)
#define IsDijkstraPixel(dp) ((dp) & 0x80000000)
#define DijkstraPixelIsUnset(dp) ((dp) == 0xFFFFFFFF)

  /**
   * Use dijkstra's algorithm to 'grow' a border around the polygons marked in the map.
   * For each pixel mark which direction is the shortest way to a face.
   */
  void grow_dijkstra(int margin)
  {
    class DijkstraActivePixel {
     public:
      DijkstraActivePixel(int dist, int _x, int _y) : distance(dist), x(_x), y(_y) {}
      int distance;
      int x, y;
    };
    auto cmp_dijkstrapixel_fun = [](DijkstraActivePixel const &a1, DijkstraActivePixel const &a2) {
      return a1.distance > a2.distance;
    };

    Vector<DijkstraActivePixel> active_pixels;
    for (int y = 0; y < h_; y++) {
      for (int x = 0; x < w_; x++) {
        if (DijkstraPixelIsUnset(get_pixel(x, y))) {
          for (int i = 0; i < 8; i++) {
            int xx = x - directions[i][0];
            int yy = y - directions[i][1];

            if (xx >= 0 && xx < w_ && yy >= 0 && yy < w_ && !IsDijkstraPixel(get_pixel(xx, yy))) {
              set_pixel(x, y, PackDijkstraPixel(distances[i], i));
              active_pixels.append(DijkstraActivePixel(distances[i], x, y));
              break;
            }
          }
        }
      }
    }

    /* Not strictly needed because at this point it already is a heap. */
#if 0
    std::make_heap(active_pixels.begin(), active_pixels.end(), cmp_dijkstrapixel_fun);
#endif

    while (active_pixels.size()) {
      std::pop_heap(active_pixels.begin(), active_pixels.end(), cmp_dijkstrapixel_fun);
      DijkstraActivePixel p = active_pixels.pop_last();

      int dist = p.distance;

      if (dist < 2 * (margin + 1)) {
        for (int i = 0; i < 8; i++) {
          int x = p.x + directions[i][0];
          int y = p.y + directions[i][1];
          if (x >= 0 && x < w_ && y >= 0 && y < h_) {
            uint32_t dp = get_pixel(x, y);
            if (IsDijkstraPixel(dp) && (DijkstraPixelGetDistance(dp) > dist + distances[i])) {
              BLI_assert(DijkstraPixelGetDirection(dp) != i);
              set_pixel(x, y, PackDijkstraPixel(dist + distances[i], i));
              active_pixels.append(DijkstraActivePixel(dist + distances[i], x, y));
              std::push_heap(active_pixels.begin(), active_pixels.end(), cmp_dijkstrapixel_fun);
            }
          }
        }
      }
    }
  }

  /**
   * Walk over the map and for margin pixels follow the direction stored in the bottom 3
   * bits back to the face.
   * Then look up the pixel from the next face.
   */
  void lookup_pixels(ImBuf *ibuf, char *mask, int maxPolygonSteps)
  {
    float4 *ibuf_ptr_fl = reinterpret_cast<float4 *>(ibuf->float_buffer.data);
    uchar4 *ibuf_ptr_ch = reinterpret_cast<uchar4 *>(ibuf->byte_buffer.data);
    size_t pixel_index = 0;
    for (int y = 0; y < h_; y++) {
      for (int x = 0; x < w_; x++) {
        uint32_t dp = pixel_data_[pixel_index];
        if (IsDijkstraPixel(dp) && !DijkstraPixelIsUnset(dp)) {
          int dist = DijkstraPixelGetDistance(dp);
          int direction = DijkstraPixelGetDirection(dp);

          int xx = x;
          int yy = y;

          /* Follow the dijkstra directions to find the face this margin pixels belongs to. */
          while (dist > 0) {
            xx -= directions[direction][0];
            yy -= directions[direction][1];
            dp = get_pixel(xx, yy);
            dist -= distances[direction];
            BLI_assert(!dist || (dist == DijkstraPixelGetDistance(dp)));
            direction = DijkstraPixelGetDirection(dp);
          }

          uint32_t face = get_pixel(xx, yy);

          BLI_assert(!IsDijkstraPixel(face));

          float destX, destY;

          int other_poly;
          bool found_pixel_in_polygon = false;
          if (lookup_pixel_polygon_neighborhood(x, y, &face, &destX, &destY, &other_poly)) {

            for (int i = 0; i < maxPolygonSteps; i++) {
              /* Force to pixel grid. */
              int nx = int(round(destX));
              int ny = int(round(destY));
              uint32_t polygon_from_map = get_pixel(nx, ny);
              if (other_poly == polygon_from_map) {
                found_pixel_in_polygon = true;
                break;
              }

              float dist_to_edge;
              /* Look up again, but starting from the face we were expected to land in. */
              if (!lookup_pixel(nx, ny, other_poly, &destX, &destY, &other_poly, &dist_to_edge)) {
                found_pixel_in_polygon = false;
                break;
              }
            }

            if (found_pixel_in_polygon) {
              if (ibuf_ptr_fl) {
                ibuf_ptr_fl[pixel_index] = imbuf::interpolate_bilinear_border_fl(
                    ibuf, destX, destY);
              }
              if (ibuf_ptr_ch) {
                ibuf_ptr_ch[pixel_index] = imbuf::interpolate_bilinear_border_byte(
                    ibuf, destX, destY);
              }
              /* Add our new pixels to the assigned pixel map. */
              mask[pixel_index] = 1;
            }
          }
        }
        else if (DijkstraPixelIsUnset(dp) || !IsDijkstraPixel(dp)) {
          /* These are not margin pixels, make sure the extend filter which is run after this step
           * leaves them alone.
           */
          mask[pixel_index] = 1;
        }
        pixel_index++;
      }
    }
  }

 private:
  float2 uv_to_xy(const float2 &uv_map) const
  {
    float2 ret;
    ret.x = (((uv_map[0] - uv_offset_[0]) * w_) - (0.5f + 0.001f));
    ret.y = (((uv_map[1] - uv_offset_[1]) * h_) - (0.5f + 0.001f));
    return ret;
  }

  void build_tables()
  {
    loop_to_face_map_ = blender::bke::mesh::build_corner_to_face_map(faces_);

    loop_adjacency_map_.resize(corner_edges_.size(), -1);

    Vector<int> tmpmap;
    tmpmap.resize(totedge_, -1);

    for (const int64_t i : corner_edges_.index_range()) {
      int edge = corner_edges_[i];
      if (tmpmap[edge] == -1) {
        loop_adjacency_map_[i] = -1;
        tmpmap[edge] = i;
      }
      else {
        BLI_assert(tmpmap[edge] >= 0);
        loop_adjacency_map_[i] = tmpmap[edge];
        loop_adjacency_map_[tmpmap[edge]] = i;
      }
    }
  }

  /**
   * Call lookup_pixel for the start_poly. If that fails, try the adjacent polygons as well.
   * Because the Dijkstra is not very exact in determining which face is the closest, the
   * face we need can be the one next to the one the Dijkstra map provides. To prevent missing
   * pixels also check the neighboring polygons.
   */
  bool lookup_pixel_polygon_neighborhood(
      float x, float y, uint32_t *r_start_poly, float *r_destx, float *r_desty, int *r_other_poly)
  {
    float found_dist;
    if (lookup_pixel(x, y, *r_start_poly, r_destx, r_desty, r_other_poly, &found_dist)) {
      return true;
    }

    int loopstart = faces_[*r_start_poly].start();
    int totloop = faces_[*r_start_poly].size();

    float destx, desty;
    int foundpoly;

    float mindist = -1.0f;

    /* Loop over all adjacent polygons and determine which edge is closest.
     * This could be optimized by only inspecting neighbors which are on the edge of an island.
     * But it seems fast enough for now and that would add a lot of complexity. */
    for (int i = 0; i < totloop; i++) {
      int otherloop = loop_adjacency_map_[i + loopstart];

      if (otherloop < 0) {
        continue;
      }

      uint32_t face = loop_to_face_map_[otherloop];

      if (lookup_pixel(x, y, face, &destx, &desty, &foundpoly, &found_dist)) {
        if (mindist < 0.0f || found_dist < mindist) {
          mindist = found_dist;
          *r_other_poly = foundpoly;
          *r_destx = destx;
          *r_desty = desty;
          *r_start_poly = face;
        }
      }
    }

    return mindist >= 0.0f;
  }

  /**
   * Find which edge of the src_poly is closest to x,y. Look up its adjacent UV-edge and face.
   * Then return the location of the equivalent pixel in the other face.
   * Returns true if a new pixel location was found, false if it wasn't, which can happen if the
   * margin pixel is on a corner, or the UV-edge doesn't have an adjacent face.
   */
  bool lookup_pixel(float x,
                    float y,
                    int src_poly,
                    float *r_destx,
                    float *r_desty,
                    int *r_other_poly,
                    float *r_dist_to_edge)
  {
    float2 point(x, y);

    *r_destx = *r_desty = 0;

    int found_edge = -1;
    float found_dist = -1;
    float found_t = 0;

    /* Find the closest edge on which the point x,y can be projected.
     */
    for (size_t i = 0; i < faces_[src_poly].size(); i++) {
      int l1 = faces_[src_poly].start() + i;
      int l2 = l1 + 1;
      if (l2 >= faces_[src_poly].start() + faces_[src_poly].size()) {
        l2 = faces_[src_poly].start();
      }
      /* edge points */
      float2 edgepoint1 = uv_to_xy(uv_map_[l1]);
      float2 edgepoint2 = uv_to_xy(uv_map_[l2]);
      /* Vector AB is the vector from the first edge point to the second edge point.
       * Vector AP is the vector from the first edge point to our point under investigation. */
      float2 ab = edgepoint2 - edgepoint1;
      float2 ap = point - edgepoint1;

      /* Project ap onto ab. */
      float dotv = math::dot(ab, ap);

      float ablensq = math::length_squared(ab);

      float t = dotv / ablensq;

      if (t >= 0.0 && t <= 1.0) {

        /* Find the point on the edge closest to P */
        float2 reflect_point = edgepoint1 + (t * ab);
        /* This is the vector to P, so 90 degrees out from the edge. */
        float2 reflect_vec = reflect_point - point;

        float reflectLen = sqrt(reflect_vec[0] * reflect_vec[0] + reflect_vec[1] * reflect_vec[1]);
        float cross = ab[0] * reflect_vec[1] - ab[1] * reflect_vec[0];
        /* Only if P is on the outside of the edge, which means the cross product is positive,
         * we consider this edge.
         */
        bool valid = (cross > 0.0);

        if (valid && (found_dist < 0 || reflectLen < found_dist)) {
          /* Stother_ab the info of the closest edge so far. */
          found_dist = reflectLen;
          found_t = t;
          found_edge = i + faces_[src_poly].start();
        }
      }
    }

    if (found_edge < 0) {
      return false;
    }

    *r_dist_to_edge = found_dist;

    /* Get the 'other' edge. I.E. the UV edge from the neighbor face. */
    int other_edge = loop_adjacency_map_[found_edge];

    if (other_edge < 0) {
      return false;
    }

    int dst_poly = loop_to_face_map_[other_edge];

    if (r_other_poly) {
      *r_other_poly = dst_poly;
    }

    int other_edge2 = other_edge + 1;
    if (other_edge2 >= faces_[dst_poly].start() + faces_[dst_poly].size()) {
      other_edge2 = faces_[dst_poly].start();
    }

    float2 other_edgepoint1 = uv_to_xy(uv_map_[other_edge]);
    float2 other_edgepoint2 = uv_to_xy(uv_map_[other_edge2]);

    /* Calculate the vector from the order edges last point to its first point. */
    float2 other_ab = other_edgepoint1 - other_edgepoint2;
    float2 other_reflect_point = other_edgepoint2 + (found_t * other_ab);
    float2 perpendicular_other_ab;
    perpendicular_other_ab.x = other_ab.y;
    perpendicular_other_ab.y = -other_ab.x;

    /* The new point is dound_dist distance from other_reflect_point at a 90 degree angle to
     * other_ab */
    float2 new_point = other_reflect_point + (found_dist / math::length(perpendicular_other_ab)) *
                                                 perpendicular_other_ab;

    *r_destx = new_point.x;
    *r_desty = new_point.y;

    return true;
  }
};  // class TextureMarginMap

const int TextureMarginMap::directions[8][2] = {
    {-1, 0}, {-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}};
const int TextureMarginMap::distances[8] = {2, 3, 2, 3, 2, 3, 2, 3};

static void generate_margin(ImBuf *ibuf,
                            char *mask,
                            const int margin,
                            const Span<float3> vert_positions,
                            const int edges_num,
                            const OffsetIndices<int> faces,
                            const Span<int> corner_edges,
                            const Span<int> corner_verts,
                            const Span<float2> uv_map,
                            const float uv_offset[2])
{
  Array<int3> corner_tris(poly_to_tri_count(faces.size(), corner_edges.size()));
  bke::mesh::corner_tris_calc(vert_positions, faces, corner_verts, corner_tris);

  Array<int> tri_faces(corner_tris.size());
  bke::mesh::corner_tris_calc_face_indices(faces, tri_faces);

  TextureMarginMap map(ibuf->x, ibuf->y, uv_offset, edges_num, faces, corner_edges, uv_map);

  bool draw_new_mask = false;
  /* Now the map contains 3 sorts of values: 0xFFFFFFFF for empty pixels, `0x80000000 + polyindex`
   * for margin pixels, just `polyindex` for face pixels. */
  if (mask) {
    mask = (char *)MEM_dupallocN(mask);
  }
  else {
    mask = MEM_calloc_arrayN<char>(size_t(ibuf->x) * size_t(ibuf->y), __func__);
    draw_new_mask = true;
  }

  for (const int i : corner_tris.index_range()) {
    const int3 tri = corner_tris[i];
    float vec[3][2];

    for (int a = 0; a < 3; a++) {
      const float *uv = uv_map[tri[a]];

      /* NOTE(@ideasman42): workaround for pixel aligned UVs which are common and can screw up
       * our intersection tests where a pixel gets in between 2 faces or the middle of a quad,
       * camera aligned quads also have this problem but they are less common.
       * Add a small offset to the UVs, fixes bug #18685. */
      vec[a][0] = (uv[0] - uv_offset[0]) * float(ibuf->x) - (0.5f + 0.001f);
      vec[a][1] = (uv[1] - uv_offset[1]) * float(ibuf->y) - (0.5f + 0.002f);
    }

    /* NOTE: we need the top bit for the dijkstra distance map. */
    BLI_assert(tri_faces[i] < 0x80000000);

    map.rasterize_tri(vec[0], vec[1], vec[2], tri_faces[i], mask, draw_new_mask);
  }

  char *tmpmask = (char *)MEM_dupallocN(mask);
  /* Extend (with averaging) by 2 pixels. Those will be overwritten, but it
   * helps linear interpolations on the edges of polygons. */
  IMB_filter_extend(ibuf, tmpmask, 2);
  MEM_freeN(tmpmask);

  map.grow_dijkstra(margin);

  /* Looking further than 3 polygons away leads to so much cumulative rounding
   * that it isn't worth it. So hard-code it to 3. */
  map.lookup_pixels(ibuf, mask, 3);

  /* Use the extend filter to fill in the missing pixels at the corners, not strictly correct, but
   * the visual difference seems very minimal. This also catches pixels we missed because of very
   * narrow polygons.
   */
  IMB_filter_extend(ibuf, mask, margin);

  MEM_freeN(mask);
}

}  // namespace blender::render::texturemargin

void RE_generate_texturemargin_adjacentfaces(ImBuf *ibuf,
                                             char *mask,
                                             const int margin,
                                             const Mesh *mesh,
                                             blender::StringRef uv_layer,
                                             const float uv_offset[2])
{
  using namespace blender;
  const StringRef name = uv_layer.is_empty() ? mesh->active_uv_map_name() : uv_layer;
  const blender::bke::AttributeAccessor attributes = mesh->attributes();
  const VArraySpan<float2> uv_map = *attributes.lookup<float2>(name, bke::AttrDomain::Corner);

  blender::render::texturemargin::generate_margin(ibuf,
                                                  mask,
                                                  margin,
                                                  mesh->vert_positions(),
                                                  mesh->edges_num,
                                                  mesh->faces(),
                                                  mesh->corner_edges(),
                                                  mesh->corner_verts(),
                                                  uv_map,
                                                  uv_offset);
}
