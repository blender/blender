/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. */

#include "BLI_array.hh"
#include "BLI_bit_vector.hh"
#include "BLI_math.h"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BKE_image_wrappers.hh"
#include "BKE_pbvh.h"
#include "BKE_pbvh_pixels.hh"

#include "pbvh_intern.hh"
#include "pbvh_pixels_copy.hh"
#include "pbvh_uv_islands.hh"

namespace blender::bke::pbvh::pixels {

const int THREADING_GRAIN_SIZE = 128;

/** Coordinate space of a coordinate. */
enum class CoordSpace {
  /**
   * Coordinate is in UV coordinate space. As in unmodified from mesh data.
   */
  UV,

  /**
   * Coordinate is in Tile coordinate space.
   *
   * With tile coordinate space each unit is a single pixel of the tile.
   * Range is [0..buffer width].
   */
  Tile,
};

template<CoordSpace Space> struct Vertex {
  float2 coordinate;
};

template<CoordSpace Space> struct Edge {
  Vertex<Space> vertex_1;
  Vertex<Space> vertex_2;
};

/** Calculate the bounds of the given edge. */
static rcti get_bounds(const Edge<CoordSpace::Tile> &tile_edge)
{
  rcti bounds;
  BLI_rcti_init_minmax(&bounds);
  BLI_rcti_do_minmax_v(&bounds, int2(tile_edge.vertex_1.coordinate));
  BLI_rcti_do_minmax_v(&bounds, int2(tile_edge.vertex_2.coordinate));
  return bounds;
}

/** Add a margin to the given bounds. */
static void add_margin(rcti &bounds, int margin)
{
  bounds.xmin -= margin;
  bounds.xmax += margin;
  bounds.ymin -= margin;
  bounds.ymax += margin;
}

/** Clamp bounds to be between 0,0 and the given resolution. */
static void clamp(rcti &bounds, int2 resolution)
{
  rcti clamping_bounds;
  BLI_rcti_init(&clamping_bounds, 0, resolution.x - 1, 0, resolution.y - 1);
  BLI_rcti_isect(&bounds, &clamping_bounds, &bounds);
}

static const Vertex<CoordSpace::Tile> convert_coord_space(const Vertex<CoordSpace::UV> &uv_vertex,
                                                          const image::ImageTileWrapper image_tile,
                                                          const int2 tile_resolution)
{
  return Vertex<CoordSpace::Tile>{(uv_vertex.coordinate - float2(image_tile.get_tile_offset())) *
                                  float2(tile_resolution)};
}

static const Edge<CoordSpace::Tile> convert_coord_space(const Edge<CoordSpace::UV> &uv_edge,
                                                        const image::ImageTileWrapper image_tile,
                                                        const int2 tile_resolution)
{
  return Edge<CoordSpace::Tile>{
      convert_coord_space(uv_edge.vertex_1, image_tile, tile_resolution),
      convert_coord_space(uv_edge.vertex_2, image_tile, tile_resolution),
  };
}

class NonManifoldTileEdges : public Vector<Edge<CoordSpace::Tile>> {
};

class NonManifoldUVEdges : public Vector<Edge<CoordSpace::UV>> {
 public:
  NonManifoldUVEdges(const uv_islands::MeshData &mesh_data)
  {
    int num_non_manifold_edges = count_non_manifold_edges(mesh_data);
    reserve(num_non_manifold_edges);
    for (const int primitive_id : mesh_data.looptris.index_range()) {
      for (const int edge_id : mesh_data.primitive_to_edge_map[primitive_id]) {
        if (is_manifold(mesh_data, edge_id)) {
          continue;
        }
        const MLoopTri &loop_tri = mesh_data.looptris[primitive_id];
        const uv_islands::MeshEdge &mesh_edge = mesh_data.edges[edge_id];
        Edge<CoordSpace::UV> edge;

        edge.vertex_1.coordinate = find_uv(mesh_data, loop_tri, mesh_edge.vert1);
        edge.vertex_2.coordinate = find_uv(mesh_data, loop_tri, mesh_edge.vert2);
        append(edge);
      }
    }
    BLI_assert_msg(size() == num_non_manifold_edges,
                   "Incorrect number of non manifold edges added. ");
  }

  NonManifoldTileEdges extract_tile_edges(const image::ImageTileWrapper image_tile,
                                          const int2 tile_resolution) const
  {
    NonManifoldTileEdges result;
    for (const Edge<CoordSpace::UV> &uv_edge : *this) {
      const Edge<CoordSpace::Tile> tile_edge = convert_coord_space(
          uv_edge, image_tile, tile_resolution);
      result.append(tile_edge);
    }
    return result;
  }

 private:
  static int64_t count_non_manifold_edges(const uv_islands::MeshData &mesh_data)
  {
    int64_t result = 0;
    for (const int primitive_id : mesh_data.looptris.index_range()) {
      for (const int edge_id : mesh_data.primitive_to_edge_map[primitive_id]) {
        if (is_manifold(mesh_data, edge_id)) {
          continue;
        }
        result += 1;
      }
    }
    return result;
  }

  static bool is_manifold(const uv_islands::MeshData &mesh_data, const int edge_id)
  {
    return mesh_data.edge_to_primitive_map[edge_id].size() == 2;
  }

  static float2 find_uv(const uv_islands::MeshData &mesh_data,
                        const MLoopTri &loop_tri,
                        int vertex_i)
  {
    for (int i = 0; i < 3; i++) {
      const int loop_i = loop_tri.tri[i];
      const int vert = mesh_data.corner_verts[loop_i];
      if (vert == vertex_i) {
        return mesh_data.uv_map[loop_i];
      }
    }
    BLI_assert_unreachable();
    return float2(0.0f);
  }
};

class PixelNodesTileData : public Vector<std::reference_wrapper<UDIMTilePixels>> {
 public:
  PixelNodesTileData(PBVH &pbvh, const image::ImageTileWrapper &image_tile)
  {
    reserve(count_nodes(pbvh, image_tile));

    for (PBVHNode &node : MutableSpan(pbvh.nodes, pbvh.totnode)) {
      if (should_add_node(node, image_tile)) {
        NodeData &node_data = *static_cast<NodeData *>(node.pixels.node_data);
        UDIMTilePixels &tile_pixels = *node_data.find_tile_data(image_tile);
        append(tile_pixels);
      }
    }
  }

 private:
  static bool should_add_node(PBVHNode &node, const image::ImageTileWrapper &image_tile)
  {
    if ((node.flag & PBVH_Leaf) == 0) {
      return false;
    }
    if (node.pixels.node_data == nullptr) {
      return false;
    }
    NodeData &node_data = *static_cast<NodeData *>(node.pixels.node_data);
    if (node_data.find_tile_data(image_tile) == nullptr) {
      return false;
    }
    return true;
  }

  static int64_t count_nodes(PBVH &pbvh, const image::ImageTileWrapper &image_tile)
  {
    int64_t result = 0;
    for (PBVHNode &node : MutableSpan(pbvh.nodes, pbvh.totnode)) {
      if (should_add_node(node, image_tile)) {
        result++;
      }
    }
    return result;
  }
};

/**
 * Row contains intermediate data per pixel for a single image row. It is used during updating to
 * encode pixels.
 */

struct Rows {
  enum class PixelType {
    Undecided,
    /** This pixel is directly affected by a brush and doesn't need to be solved. */
    Brush,
    SelectedForCloserExamination,
    /** This pixel will be copied from another pixel to solve non-manifold edge bleeding. */
    CopyFromClosestEdge,
  };

  struct Pixel {
    PixelType type;
    float distance;
    CopyPixelCommand copy_command;
    /**
     * Index of the edge in the list of non-manifold edges.
     *
     * The edge is kept to calculate the mix factor between the two pixels that have chosen to
     * be mixed.
     */
    int64_t edge_index;

    Pixel() = default;

    void init(int2 coordinate)
    {
      copy_command.destination = coordinate;
      copy_command.source_1 = coordinate;
      copy_command.source_2 = coordinate;
      copy_command.mix_factor = 0.0f;
      type = PixelType::Undecided;
      distance = std::numeric_limits<float>::max();
      edge_index = -1;
    }
  };

  int2 resolution;
  int margin;
  Array<Pixel> pixels;

  struct RowView {
    int row_number = 0;
    /** Not owning pointer into Row.pixels starts at the start of the row. */
    MutableSpan<Pixel> pixels;
    RowView() = delete;
    RowView(Rows &rows, int64_t row_number)
        : row_number(row_number),
          pixels(
              MutableSpan<Pixel>(&rows.pixels[row_number * rows.resolution.x], rows.resolution.x))
    {
    }
  };

  Rows(int2 resolution, int margin)
      : resolution(resolution), margin(margin), pixels(resolution.x * resolution.y)
  {
  }

  void init_pixels()
  {
    int64_t index = 0;
    for (int y : IndexRange(resolution.y)) {
      for (int64_t x : IndexRange(resolution.x)) {
        int2 position(x, y);
        pixels[index++].init(position);
      }
    }
  }

  /**
   * Mark pixels that are painted on by the brush. Those pixels don't need to be updated, but will
   * act as a source for other pixels.
   */
  void mark_pixels_effected_by_brush(const PixelNodesTileData &nodes_tile_pixels)
  {
    for (const UDIMTilePixels &tile_pixels : nodes_tile_pixels) {
      threading::parallel_for_each(
          tile_pixels.pixel_rows, [&](const PackedPixelRow &encoded_pixels) {
            for (int x = encoded_pixels.start_image_coordinate.x;
                 x < encoded_pixels.start_image_coordinate.x + encoded_pixels.num_pixels;
                 x++)
            {
              int64_t index = encoded_pixels.start_image_coordinate.y * resolution.x + x;
              pixels[index].type = PixelType::Brush;
              pixels[index].distance = 0.0f;
            }
          });
    }
  }

  /**
   * Look for a second source pixel that will be blended with the first source pixel to improve
   * the quality of the fix.
   *
   * - The second source pixel must be a neighbor pixel of the first source, or the same as the
   *   first source when no second pixel could be found.
   * - The second source pixel must be a pixel that is painted on by the brush.
   * - The second source pixel must be the second closest pixel , or the first source
   *   when no second pixel could be found.
   */
  int2 find_second_source(int2 destination, int2 first_source)
  {
    rcti search_bounds;
    BLI_rcti_init(&search_bounds,
                  max_ii(first_source.x - 1, 0),
                  min_ii(first_source.x + 1, resolution.x - 1),
                  max_ii(first_source.y - 1, 0),
                  min_ii(first_source.y + 1, resolution.y - 1));
    /* Initialize to the first source, so when no other source could be found it will use the
     * first_source. */
    int2 found_source = first_source;
    float found_distance = std::numeric_limits<float>().max();
    for (int sy : IndexRange(search_bounds.ymin, BLI_rcti_size_y(&search_bounds) + 1)) {
      for (int sx : IndexRange(search_bounds.xmin, BLI_rcti_size_x(&search_bounds) + 1)) {
        int2 source(sx, sy);
        /* Skip first source as it should be the closest and already selected. */
        if (source == first_source) {
          continue;
        }
        int pixel_index = sy * resolution.y + sx;
        if (pixels[pixel_index].type != PixelType::Brush) {
          continue;
        }

        float new_distance = blender::math::distance(float2(destination), float2(source));
        if (new_distance < found_distance) {
          found_distance = new_distance;
          found_source = source;
        }
      }
    }
    return found_source;
  }

  float determine_mix_factor(const int2 destination,
                             const int2 source_1,
                             const int2 source_2,
                             const Edge<CoordSpace::Tile> &edge)
  {
    /* Use stable result when both sources are the same. */
    if (source_1 == source_2) {
      return 0.0f;
    }

    float2 clamped_to_edge;
    float destination_lambda = closest_to_line_v2(
        clamped_to_edge, float2(destination), edge.vertex_1.coordinate, edge.vertex_2.coordinate);
    float source_1_lambda = closest_to_line_v2(
        clamped_to_edge, float2(source_1), edge.vertex_1.coordinate, edge.vertex_2.coordinate);
    float source_2_lambda = closest_to_line_v2(
        clamped_to_edge, float2(source_2), edge.vertex_1.coordinate, edge.vertex_2.coordinate);

    return clamp_f(
        (destination_lambda - source_1_lambda) / (source_2_lambda - source_1_lambda), 0.0f, 1.0f);
  }

  void find_copy_source(Pixel &pixel, const NonManifoldTileEdges &tile_edges)
  {
    BLI_assert(pixel.type == PixelType::SelectedForCloserExamination);

    rcti bounds;
    BLI_rcti_init(&bounds,
                  pixel.copy_command.destination.x,
                  pixel.copy_command.destination.x,
                  pixel.copy_command.destination.y,
                  pixel.copy_command.destination.y);
    add_margin(bounds, margin);
    clamp(bounds, resolution);

    float found_distance = std::numeric_limits<float>().max();
    int2 found_source(0);

    for (int sy : IndexRange(bounds.ymin, BLI_rcti_size_y(&bounds))) {
      int pixel_index = sy * resolution.x;
      for (int sx : IndexRange(bounds.xmin, BLI_rcti_size_x(&bounds))) {
        Pixel &source = pixels[pixel_index + sx];
        if (source.type != PixelType::Brush) {
          continue;
        }
        float new_distance = blender::math::distance(float2(sx, sy),
                                                     float2(pixel.copy_command.destination));
        if (new_distance < found_distance) {
          found_source = int2(sx, sy);
          found_distance = new_distance;
        }
      }
    }

    if (found_distance == std::numeric_limits<float>().max()) {
      return;
    }
    pixel.type = PixelType::CopyFromClosestEdge;
    pixel.distance = found_distance;
    pixel.copy_command.source_1 = found_source;
    pixel.copy_command.source_2 = find_second_source(pixel.copy_command.destination, found_source);
    pixel.copy_command.mix_factor = determine_mix_factor(pixel.copy_command.destination,
                                                         pixel.copy_command.source_1,
                                                         pixel.copy_command.source_2,
                                                         tile_edges[pixel.edge_index]);
  }

  void find_copy_source(Vector<std::reference_wrapper<Pixel>> &selected_pixels,
                        const NonManifoldTileEdges &tile_edges)
  {
    threading::parallel_for(
        IndexRange(selected_pixels.size()), THREADING_GRAIN_SIZE, [&](IndexRange range) {
          for (int selected_pixel_index : range) {
            Pixel &current_pixel = selected_pixels[selected_pixel_index];
            find_copy_source(current_pixel, tile_edges);
          }
        });
  }

  Vector<std::reference_wrapper<Pixel>> filter_pixels_for_closer_examination(
      const NonManifoldTileEdges &tile_edges)
  {
    Vector<std::reference_wrapper<Pixel>> selected_pixels;
    selected_pixels.reserve(10000);

    for (int tile_edge_index : tile_edges.index_range()) {
      const Edge<CoordSpace::Tile> &tile_edge = tile_edges[tile_edge_index];
      rcti edge_bounds = get_bounds(tile_edge);
      add_margin(edge_bounds, margin);
      clamp(edge_bounds, resolution);

      for (const int64_t sy : IndexRange(edge_bounds.ymin, BLI_rcti_size_y(&edge_bounds))) {
        for (const int64_t sx : IndexRange(edge_bounds.xmin, BLI_rcti_size_x(&edge_bounds))) {
          const int64_t index = sy * resolution.x + sx;
          Pixel &pixel = pixels[index];
          if (pixel.type == PixelType::Brush) {
            continue;
          }
          BLI_assert_msg(pixel.type != PixelType::CopyFromClosestEdge,
                         "PixelType::CopyFromClosestEdge isn't allowed to be set as it is set "
                         "when finding the pixels to copy.");

          const float2 point(sx, sy);
          float2 closest_edge_point;
          closest_to_line_segment_v2(closest_edge_point,
                                     point,
                                     tile_edge.vertex_1.coordinate,
                                     tile_edge.vertex_2.coordinate);
          float distance_to_edge = blender::math::distance(closest_edge_point, point);
          if (distance_to_edge < margin && distance_to_edge < pixel.distance) {
            if (pixel.type != PixelType::SelectedForCloserExamination) {
              selected_pixels.append(std::reference_wrapper<Pixel>(pixel));
            }
            pixel.type = PixelType::SelectedForCloserExamination;
            pixel.distance = distance_to_edge;
            pixel.edge_index = tile_edge_index;
          }
        }
      }
    }
    return selected_pixels;
  }

  void pack_into(const Vector<std::reference_wrapper<Pixel>> &selected_pixels,
                 CopyPixelTile &copy_tile) const
  {
    std::optional<std::reference_wrapper<CopyPixelGroup>> last_group = std::nullopt;
    std::optional<CopyPixelCommand> last_command = std::nullopt;

    for (const Pixel &elem : selected_pixels) {
      if (elem.type == PixelType::CopyFromClosestEdge) {
        if (!last_command.has_value() || !last_command->can_be_extended(elem.copy_command)) {
          CopyPixelGroup new_group = {elem.copy_command.destination - int2(1, 0),
                                      elem.copy_command.source_1,
                                      copy_tile.command_deltas.size(),
                                      0};
          copy_tile.groups.append(new_group);
          last_group = copy_tile.groups.last();
          last_command = CopyPixelCommand(*last_group);
        }

        DeltaCopyPixelCommand delta_command = last_command->encode_delta(elem.copy_command);
        copy_tile.command_deltas.append(delta_command);
        last_group->get().num_deltas++;
        last_command = elem.copy_command;
      }
    }
  }

};  // namespace blender::bke::pbvh::pixels

void BKE_pbvh_pixels_copy_update(PBVH &pbvh,
                                 Image &image,
                                 ImageUser &image_user,
                                 const uv_islands::MeshData &mesh_data)
{
  PBVHData &pbvh_data = BKE_pbvh_pixels_data_get(pbvh);
  pbvh_data.tiles_copy_pixels.clear();
  const NonManifoldUVEdges non_manifold_edges(mesh_data);
  if (non_manifold_edges.is_empty()) {
    /* Early exit: No non manifold edges detected. */
    return;
  }

  ImageUser tile_user = image_user;
  LISTBASE_FOREACH (ImageTile *, tile, &image.tiles) {
    const image::ImageTileWrapper image_tile = image::ImageTileWrapper(tile);
    tile_user.tile = image_tile.get_tile_number();

    ImBuf *tile_buffer = BKE_image_acquire_ibuf(&image, &tile_user, nullptr);
    if (tile_buffer == nullptr) {
      continue;
    }
    const PixelNodesTileData nodes_tile_pixels(pbvh, image_tile);

    int2 tile_resolution(tile_buffer->x, tile_buffer->y);
    BKE_image_release_ibuf(&image, tile_buffer, nullptr);

    NonManifoldTileEdges tile_edges = non_manifold_edges.extract_tile_edges(image_tile,
                                                                            tile_resolution);
    CopyPixelTile copy_tile(image_tile.get_tile_number());

    Rows rows(tile_resolution, image.seam_margin);
    rows.init_pixels();
    rows.mark_pixels_effected_by_brush(nodes_tile_pixels);

    Vector<std::reference_wrapper<Rows::Pixel>> selected_pixels =
        rows.filter_pixels_for_closer_examination(tile_edges);
    rows.find_copy_source(selected_pixels, tile_edges);
    rows.pack_into(selected_pixels, copy_tile);

    copy_tile.print_compression_rate();
    pbvh_data.tiles_copy_pixels.tiles.append(copy_tile);
  }
}

void BKE_pbvh_pixels_copy_pixels(PBVH &pbvh,
                                 Image &image,
                                 ImageUser &image_user,
                                 image::TileNumber tile_number)
{
  PBVHData &pbvh_data = BKE_pbvh_pixels_data_get(pbvh);
  std::optional<std::reference_wrapper<CopyPixelTile>> pixel_tile =
      pbvh_data.tiles_copy_pixels.find_tile(tile_number);
  if (!pixel_tile.has_value()) {
    /* No pixels need to be copied. */
    return;
  }

  ImageUser tile_user = image_user;
  tile_user.tile = tile_number;
  ImBuf *tile_buffer = BKE_image_acquire_ibuf(&image, &tile_user, nullptr);
  if (tile_buffer == nullptr) {
    /* No tile buffer found to copy. */
    return;
  }

  CopyPixelTile &tile = pixel_tile->get();
  threading::parallel_for(tile.groups.index_range(), THREADING_GRAIN_SIZE, [&](IndexRange range) {
    tile.copy_pixels(*tile_buffer, range);
  });

  BKE_image_release_ibuf(&image, tile_buffer, nullptr);
}

}  // namespace blender::bke::pbvh::pixels
