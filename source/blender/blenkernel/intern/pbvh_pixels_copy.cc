/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "BLI_array.hh"
#include "BLI_index_mask.hh"
#include "BLI_listbase.hh"
#include "BLI_math_geom_c.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "BKE_image_wrappers.hh"
#include "BKE_mesh.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_paint_bvh_pixels.hh"

#include "PRF_profile.hh"

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

static Vertex<CoordSpace::Tile> convert_coord_space(const Vertex<CoordSpace::UV> &uv_vertex,
                                                    const image::ImageTileWrapper image_tile,
                                                    const int2 tile_resolution)
{
  return Vertex<CoordSpace::Tile>{(uv_vertex.coordinate - float2(image_tile.get_tile_offset())) *
                                  float2(tile_resolution)};
}

static Edge<CoordSpace::Tile> convert_coord_space(const Edge<CoordSpace::UV> &uv_edge,
                                                  const image::ImageTileWrapper image_tile,
                                                  const int2 tile_resolution)
{
  return Edge<CoordSpace::Tile>{
      convert_coord_space(uv_edge.vertex_1, image_tile, tile_resolution),
      convert_coord_space(uv_edge.vertex_2, image_tile, tile_resolution),
  };
}

class NonManifoldTileEdges : public Vector<Edge<CoordSpace::Tile>> {};

class NonManifoldUVEdges : public Vector<Edge<CoordSpace::UV>> {
 public:
  NonManifoldUVEdges(const uv_islands::MeshData &mesh_data)
  {
    int num_non_manifold_edges = count_non_manifold_edges(mesh_data);
    reserve(num_non_manifold_edges);
    for (const int primitive_id : mesh_data.corner_tris.index_range()) {
      const int3 tri = mesh_data.corner_tris[primitive_id];
      const int3 real_edges = mesh::corner_tri_get_real_edges(
          mesh_data.mesh_edges, mesh_data.corner_verts, mesh_data.corner_edges, tri);
      for (int j = 0; j < 3; j++) {
        const int edge_id = real_edges[j];
        /* -1 means internal edge in face, which is always manifold. */
        if (edge_id == -1 || mesh_data.is_edge_manifold(edge_id)) {
          continue;
        }
        Edge<CoordSpace::UV> edge;
        edge.vertex_1.coordinate = mesh_data.uv_map[tri[j]];
        edge.vertex_2.coordinate = mesh_data.uv_map[tri[(j + 1) % 3]];
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
    for (const int primitive_id : mesh_data.corner_tris.index_range()) {
      const int3 real_edges = mesh::corner_tri_get_real_edges(mesh_data.mesh_edges,
                                                              mesh_data.corner_verts,
                                                              mesh_data.corner_edges,
                                                              mesh_data.corner_tris[primitive_id]);
      for (int j = 0; j < 3; j++) {
        const int edge_id = real_edges[j];
        /* -1 means internal edge in face, which is always manifold. */
        if (!(edge_id == -1 || mesh_data.is_edge_manifold(edge_id))) {
          result += 1;
        }
      }
    }
    return result;
  }
};

class PixelNodesTileData : public Vector<std::reference_wrapper<UDIMTilePixels>> {
 public:
  PixelNodesTileData(bke::pbvh::Tree &pbvh, const image::ImageTileWrapper &image_tile)
  {
    IndexMaskMemory memory;
    const IndexMask nodes = affected_nodes(pbvh, image_tile, memory);
    reserve(nodes.size());

    PixelData &pixel_data = *pbvh.pixels_;
    MutableSpan<PixelNode> pixel_nodes = pixel_data.nodes;

    nodes.foreach_index([&](const int i) { append(*pixel_nodes[i].find_tile_data(image_tile)); });
  }

 private:
  static bool should_add_node(PixelNode &node, const image::ImageTileWrapper &image_tile)
  {
    if (node.find_tile_data(image_tile) == nullptr) {
      return false;
    }
    return true;
  }

  static IndexMask affected_nodes(bke::pbvh::Tree &pbvh,
                                  const image::ImageTileWrapper &image_tile,
                                  IndexMaskMemory &memory)
  {
    IndexMask leaf_nodes = all_leaf_nodes(pbvh, memory);
    PixelData &pixel_data = *pbvh.pixels_;
    MutableSpan<PixelNode> pixel_nodes = pixel_data.nodes;

    return IndexMask::from_predicate(leaf_nodes, memory, [&](const int i) {
      return should_add_node(pixel_nodes[i], image_tile);
    });
  }
};

/**
 * Row contains intermediate data per pixel for a single image row. It is used during updating to
 * encode pixels.
 */

struct Rows {
  enum class PixelType : uint8_t {
    Undecided,
    /** This pixel is directly affected by a brush and doesn't need to be solved. */
    Brush,
    SelectedForCloserExamination,
    /** This pixel will be copied from another pixel to solve non-manifold edge bleeding. */
    CopyFromClosestEdge,
  };

  struct Pixel {
    float distance = std::numeric_limits<float>::max();
    /**
     * Index of the edge in the list of non-manifold edges.
     *
     * The edge is kept to calculate the mix factor between the two pixels that have chosen to
     * be mixed.
     */
    int edge_index = -1;
    PixelType type = PixelType::Undecided;
  };

  int2 resolution;
  int margin;
  Array<Pixel> pixels;

  Rows(int2 resolution, int margin)
      : resolution(resolution), margin(margin), pixels(resolution.x * resolution.y)
  {
  }

  int2 coordinate_of(const int64_t index) const
  {
    return int2(int(index % resolution.x), int(index / resolution.x));
  }

  /**
   * Mark pixels that are painted on by the brush. Those pixels don't need to be updated, but will
   * act as a source for other pixels.
   */
  void mark_pixels_effected_by_brush(const PixelNodesTileData &nodes_tile_pixels)
  {
    for (const UDIMTilePixels &tile_pixels : nodes_tile_pixels) {
      const int num_runs = tile_pixels.pixel_row_run_starts.size() - 1;
      threading::parallel_for(IndexRange(num_runs), 64, [&](const IndexRange range) {
        for (const int run_i : range) {
          int x = tile_pixels.pixel_row_run_start_coords[run_i].x;
          const int y = tile_pixels.pixel_row_run_start_coords[run_i].y;
          const int run_begin = tile_pixels.pixel_row_run_starts[run_i];
          const int run_end = tile_pixels.pixel_row_run_starts[run_i + 1];
          for (int k = run_begin; k < run_end; k++) {
            const int num_pixels = tile_pixels.pixel_rows[k].num_pixels;
            for (int px = 0; px < num_pixels; px++, x++) {
              int64_t index = int64_t(y) * resolution.x + x;
              pixels[index].type = PixelType::Brush;
              pixels[index].distance = 0.0f;
            }
          }
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
   * - The second source pixel must be the second closest pixel, or the first source
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
    float found_distance = std::numeric_limits<float>::max();
    for (int sy : IndexRange(search_bounds.ymin, BLI_rcti_size_y(&search_bounds) + 1)) {
      for (int sx : IndexRange(search_bounds.xmin, BLI_rcti_size_x(&search_bounds) + 1)) {
        int2 source(sx, sy);
        /* Skip first source as it should be the closest and already selected. */
        if (source == first_source) {
          continue;
        }
        int pixel_index = sy * resolution.x + sx;
        if (pixels[pixel_index].type != PixelType::Brush) {
          continue;
        }

        float new_distance = math::distance(float2(destination), float2(source));
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

  void find_copy_source(const int64_t index,
                        CopyPixelCommand &r_command,
                        const NonManifoldTileEdges &tile_edges)
  {
    Pixel &pixel = pixels[index];
    BLI_assert(pixel.type == PixelType::SelectedForCloserExamination);
    const int2 destination = coordinate_of(index);

    rcti bounds;
    BLI_rcti_init(&bounds, destination.x, destination.x, destination.y, destination.y);
    add_margin(bounds, margin);
    clamp(bounds, resolution);

    float found_distance = std::numeric_limits<float>::max();
    int2 found_source(0);

    for (int sy : IndexRange(bounds.ymin, BLI_rcti_size_y(&bounds))) {
      int pixel_index = sy * resolution.x;
      for (int sx : IndexRange(bounds.xmin, BLI_rcti_size_x(&bounds))) {
        Pixel &source = pixels[pixel_index + sx];
        if (source.type != PixelType::Brush) {
          continue;
        }
        float new_distance = math::distance(float2(sx, sy), float2(destination));
        if (new_distance < found_distance) {
          found_source = int2(sx, sy);
          found_distance = new_distance;
        }
      }
    }

    if (found_distance == std::numeric_limits<float>::max()) {
      return;
    }
    pixel.type = PixelType::CopyFromClosestEdge;
    pixel.distance = found_distance;
    r_command.destination = destination;
    r_command.source_1 = found_source;
    r_command.source_2 = find_second_source(destination, found_source);
    r_command.mix_factor = determine_mix_factor(
        destination, found_source, r_command.source_2, tile_edges[pixel.edge_index]);
  }

  void find_copy_source(const Span<int64_t> selected_pixels,
                        MutableSpan<CopyPixelCommand> r_commands,
                        const NonManifoldTileEdges &tile_edges)
  {
    threading::parallel_for(
        selected_pixels.index_range(), THREADING_GRAIN_SIZE, [&](IndexRange range) {
          for (const int64_t i : range) {
            find_copy_source(selected_pixels[i], r_commands[i], tile_edges);
          }
        });
  }

  Vector<int64_t> filter_pixels_for_closer_examination(const NonManifoldTileEdges &tile_edges)
  {
    Vector<int64_t> selected_pixels;
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
          float distance_to_edge = math::distance(closest_edge_point, point);
          if (distance_to_edge < margin && distance_to_edge < pixel.distance) {
            if (pixel.type != PixelType::SelectedForCloserExamination) {
              selected_pixels.append(index);
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

  void pack_into(const Span<int64_t> selected_pixels,
                 const Span<CopyPixelCommand> commands,
                 CopyPixelTile &copy_tile) const
  {
    std::optional<std::reference_wrapper<CopyPixelGroup>> last_group = std::nullopt;
    std::optional<CopyPixelCommand> last_command = std::nullopt;
    const int seam_tilex_x = (resolution.x + SEAM_TILE_SIZE - 1) >> SEAM_TILE_BITS;
    int last_seam_tile = -1;

    for (const int64_t i : selected_pixels.index_range()) {
      if (pixels[selected_pixels[i]].type == PixelType::CopyFromClosestEdge) {
        const CopyPixelCommand &command = commands[i];

        /* Split group when it cross into another seam tile, so we can cleanly
         * sort each group into a seam tile later. */
        const int seam_tile = CopyPixelTile::seam_tile_index(command.source_1, seam_tilex_x);
        if (!last_command.has_value() || !last_command->can_be_extended(command) ||
            seam_tile != last_seam_tile)
        {
          CopyPixelGroup new_group = {command.destination - int2(1, 0),
                                      command.source_1,
                                      copy_tile.command_deltas.size(),
                                      0};
          last_seam_tile = seam_tile;
          copy_tile.groups.append(new_group);
          last_group = copy_tile.groups.last();
          last_command = CopyPixelCommand(*last_group);
        }

        DeltaCopyPixelCommand delta_command = last_command->encode_delta(command);
        copy_tile.command_deltas.append(delta_command);
        last_group->get().num_deltas++;
        last_command = command;
      }
    }
  }
};

void CopyPixelTile::build_seam_tile_map(const int2 resolution)
{
  /* Sort the groups by the seam tile their source pixels fall in and
   * store the index range into the group array for each seam tile. */
  const int tiles_x = (resolution.x + SEAM_TILE_SIZE - 1) >> SEAM_TILE_BITS;
  std::ranges::stable_sort(groups, std::less<>{}, [tiles_x](const CopyPixelGroup &group) {
    return seam_tile_index(group.start_source_1, tiles_x);
  });

  seam_tile_to_groups.clear();
  int64_t start = 0;
  while (start < groups.size()) {
    const int tile = seam_tile_index(groups[start].start_source_1, tiles_x);
    int64_t end = start + 1;
    while (end < groups.size() && seam_tile_index(groups[end].start_source_1, tiles_x) == tile) {
      end++;
    }
    seam_tile_to_groups.add(tile, IndexRange(start, end - start));
    start = end;
  }
}

void copy_update(bke::pbvh::Tree &pbvh,
                 Image &image,
                 ImageUser &image_user,
                 const uv_islands::MeshData &mesh_data)
{
  PRF_scope(ProfileCategory::Editor);
  PixelData &pbvh_data = data_get(pbvh);
  pbvh_data.tiles_copy_pixels.clear();
  const NonManifoldUVEdges non_manifold_edges(mesh_data);
  if (non_manifold_edges.is_empty()) {
    /* Early exit: No non manifold edges detected. */
    return;
  }

  ImageUser tile_user = image_user;
  for (ImageTile &tile : image.tiles) {
    const image::ImageTileWrapper image_tile = image::ImageTileWrapper(&tile);
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
    rows.mark_pixels_effected_by_brush(nodes_tile_pixels);

    Vector<int64_t> selected_pixels = rows.filter_pixels_for_closer_examination(tile_edges);
    Array<CopyPixelCommand> selected_commands(selected_pixels.size());
    rows.find_copy_source(selected_pixels, selected_commands, tile_edges);
    rows.pack_into(selected_pixels, selected_commands, copy_tile);
    copy_tile.build_seam_tile_map(tile_resolution);

    // copy_tile.print_compression_rate();
    pbvh_data.tiles_copy_pixels.tiles.append(copy_tile);
  }
}

/* TODO: Allow passing `ImageData` here, but this requires pulling this entire class out of the
 * bke namespace. */
void copy_pixels(bke::pbvh::Tree &pbvh,
                 Map<image::TileNumber, ImBuf *> &buffers,
                 image::TileNumber tile_number,
                 const Span<uint8_t> seam_tiles_modified,
                 const FunctionRef<void(int x_start, int x_end, int y)> push_undo_tiles)
{
  PixelData &pbvh_data = data_get(pbvh);
  std::optional<std::reference_wrapper<CopyPixelTile>> pixel_tile =
      pbvh_data.tiles_copy_pixels.find_tile(tile_number);
  if (!pixel_tile.has_value()) {
    /* No pixels need to be copied. */
    return;
  }

  ImBuf *tile_buffer = buffers.lookup_default(tile_number, nullptr);
  if (tile_buffer == nullptr) {
    /* No tile buffer found to copy. */
    return;
  }

  CopyPixelTile &tile = pixel_tile->get();

  /* Apply the pixel copies for groups whose seam tile was modified. */
  for (const auto item : tile.seam_tile_to_groups.items()) {
    BLI_assert(item.key < seam_tiles_modified.size());
    if (!seam_tiles_modified[item.key]) {
      continue;
    }
    const IndexRange group_range = item.value;

    /* Push undo tiles affected by these groups before editing, just like painting. */
    for (const CopyPixelGroup &group : tile.groups.as_span().slice(group_range)) {
      push_undo_tiles(group.start_destination.x + 1,
                      group.start_destination.x + group.num_deltas,
                      group.start_destination.y);
    }

    tile.copy_pixels(*tile_buffer, group_range);
  }
}

}  // namespace blender::bke::pbvh::pixels
