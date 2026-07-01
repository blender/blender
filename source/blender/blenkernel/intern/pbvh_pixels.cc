/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_paint_bvh_pixels.hh"

#include "DNA_image_types.h"
#include "DNA_object_types.h"

#include "BLI_listbase.hh"
#include "BLI_math_geom_c.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_c.hh"

#include "BKE_image_wrappers.hh"
#include "BKE_paint.hh"

#include "IMB_partial_update.hh"

#include "PRF_profile.hh"

#include "pbvh_intern.hh"
#include "pbvh_pixels_copy.hh"
#include "pbvh_pixels_rasterize.hh"
#include "pbvh_uv_islands.hh"

#include <algorithm>

namespace blender {

namespace bke::pbvh::pixels {

/**
 * Compute affine map from image pixel coordinate to object space position:
 */
static float3x3 calc_pixel_to_position_map(const uv_islands::MeshData &mesh_data,
                                           const int tri,
                                           const float2 uvs[3],
                                           const float inv_w,
                                           const float inv_h)
{
  const int3 &corner_tri = mesh_data.corner_tris[tri];
  const float3 v0 = mesh_data.vert_positions[mesh_data.corner_verts[corner_tri[0]]];
  const float3 v1 = mesh_data.vert_positions[mesh_data.corner_verts[corner_tri[1]]];
  const float3 v2 = mesh_data.vert_positions[mesh_data.corner_verts[corner_tri[2]]];

  /* Solve for coefficient so that:
   * P = pixel_to_position * (pixel_x, pixel_y, 1) */
  const float3 e0 = v0 - v2;
  const float3 e1 = v1 - v2;
  const float2 a = uvs[0] - uvs[2];
  const float2 b = uvs[1] - uvs[2];
  const float det = a.x * b.y - a.y * b.x;
  if (det == 0.0f) {
    /* Degenerate UV triangle. */
    return float3x3(float3(0.0f), float3(0.0f), v2);
  }

  const float3 dP_du = (b.y * e0 - a.y * e1) / det;
  const float3 dP_dv = (a.x * e1 - b.x * e0) / det;

  /* Per-pixel steps, and the position at pixel (0, 0), i.e. uv (0.5 / w, 0.5 / h). */
  const float3 delta_x = dP_du * inv_w;
  const float3 delta_y = dP_dv * inv_h;
  const float3 start_position = dP_du * (0.5f * inv_w - uvs[2].x) +
                                dP_dv * (0.5f * inv_h - uvs[2].y) + v2;

  return float3x3(delta_x, delta_y, start_position);
}

/**
 * During debugging this check could be enabled.
 * It will write to each image pixel that is covered by the Tree.
 */
constexpr bool USE_WATERTIGHT_CHECK = false;

/** A pixel row with its image coordinate. Used while building before it is packed into runs. */
struct BuildPixelRow {
  ushort2 start_image_coordinate;
  ushort num_pixels;
  ushort uv_primitive_index;
};

static void extract_barycentric_pixels(Vector<BuildPixelRow> &build_rows,
                                       const ImBuf *image_buffer,
                                       const uv_islands::UVIslandsMask::Tile &mask_tile,
                                       const int uv_island_index,
                                       const int uv_primitive_index,
                                       const float2 uvs[3],
                                       const int minx,
                                       const int miny,
                                       const int maxx,
                                       const int maxy)
{
  const float inv_w = 1.0f / image_buffer->x;
  const float inv_h = 1.0f / image_buffer->y;

  const int mask_resolution_x = mask_tile.mask_resolution.x;
  const int mask_resolution_y = mask_tile.mask_resolution.y;
  const float mask_scale_x = mask_resolution_x * inv_w;
  const float mask_scale_y = mask_resolution_y * inv_h;

  const float2 image_dimensions(image_buffer->x, image_buffer->y);
  const TriRasterizer rasterizer(
      uvs[0] * image_dimensions, uvs[1] * image_dimensions, uvs[2] * image_dimensions);

  for (int y = miny; y < maxy; y++) {
    bool start_detected = false;
    BuildPixelRow pixel_row;
    pixel_row.uv_primitive_index = uv_primitive_index;
    pixel_row.num_pixels = 0;
    int x;

    const float fy = float(y) + 0.5f;
    const int mask_y = std::clamp(int(fy * mask_scale_y), 0, mask_resolution_y - 1);

    for (x = minx; x < maxx; x++) {
      const float fx = float(x) + 0.5f;

      /* The mask UV is always in range, since loop pixels are inside the clamped bounding box. */
      const int mask_x = std::clamp(int(fx * mask_scale_x), 0, mask_resolution_x - 1);
      const bool is_masked = mask_tile.is_masked(uv_island_index, mask_x, mask_y);
      const bool is_inside = rasterizer.inside(x, y);

      if (!start_detected && is_inside && is_masked) {
        start_detected = true;
        pixel_row.start_image_coordinate = ushort2(x, y);
      }
      else if (start_detected && (!is_inside || !is_masked)) {
        break;
      }
    }

    if (!start_detected) {
      continue;
    }
    pixel_row.num_pixels = x - pixel_row.start_image_coordinate.x;
    build_rows.append(pixel_row);
  }
}

struct UVPrimitiveLookup {
  struct Entry {
    uv_islands::UVPrimitive *uv_primitive;
    uint64_t uv_island_index;

    Entry(uv_islands::UVPrimitive *uv_primitive, uint64_t uv_island_index)
        : uv_primitive(uv_primitive), uv_island_index(uv_island_index)
    {
    }
  };

  Vector<Vector<Entry>> lookup;

  UVPrimitiveLookup(const uint64_t geom_primitive_len,
                    MutableSpan<uv_islands::UVIsland> uv_islands)
  {
    lookup.append_n_times(Vector<Entry>(), geom_primitive_len);

    uint64_t uv_island_index = 0;
    for (uv_islands::UVIsland &uv_island : uv_islands) {
      for (uv_islands::UVPrimitive &uv_primitive : uv_island.uv_primitives) {
        lookup[uv_primitive.primitive_i].append_as(Entry(&uv_primitive, uv_island_index));
      }
      uv_island_index++;
    }
  }
};

static void build_pixel_row_runs(Vector<BuildPixelRow> &build_rows, UDIMTilePixels &tile_data)
{
  const int n = build_rows.size();

  /* Sort pixel rows by (y, x), to group contiguous pixel rows runs from adjacent triangles.
   * Stable sort for deterministic results with overlapping triangles. */
  std::ranges::stable_sort(build_rows, [](const BuildPixelRow &a, const BuildPixelRow &b) {
    if (a.start_image_coordinate.y != b.start_image_coordinate.y) {
      return a.start_image_coordinate.y < b.start_image_coordinate.y;
    }
    return a.start_image_coordinate.x < b.start_image_coordinate.x;
  });

  /* Build runs of packed rows from build rows. */
  tile_data.pixel_rows.reserve(n);
  tile_data.pixel_row_run_starts.clear();
  tile_data.pixel_row_run_start_coords.clear();
  for (const int i : build_rows.index_range()) {
    const BuildPixelRow &build_row = build_rows[i];
    if (i == 0) {
      tile_data.pixel_row_run_starts.append(0);
      tile_data.pixel_row_run_start_coords.append(build_row.start_image_coordinate);
    }
    else {
      const BuildPixelRow &prev = build_rows[i - 1];
      const bool contiguous = build_row.start_image_coordinate.y ==
                                  prev.start_image_coordinate.y &&
                              build_row.start_image_coordinate.x ==
                                  prev.start_image_coordinate.x + prev.num_pixels;
      if (!contiguous) {
        tile_data.pixel_row_run_starts.append(i);
        tile_data.pixel_row_run_start_coords.append(build_row.start_image_coordinate);
      }
    }
    tile_data.pixel_rows.append({build_row.num_pixels, build_row.uv_primitive_index});
  }
  tile_data.pixel_row_run_starts.append(n);
}

static void do_encode_pixels(const uv_islands::MeshData &mesh_data,
                             const Span<uv_islands::UVIsland> islands,
                             const uv_islands::UVIslandsMask &uv_masks,
                             const UVPrimitiveLookup &uv_prim_lookup,
                             Image &image,
                             ImageUser &image_user,
                             MeshNode &node,
                             PixelNode &pixel_node)
{
  PRF_scope(ProfileCategory::Editor);
  BLI_assert(pixel_node.flags.rebuild || (pixel_node.uv_primitives.tri_indices.is_empty() &&
                                          pixel_node.uv_primitives.pixel_to_position.is_empty() &&
                                          pixel_node.tiles.is_empty()));

  Vector<int> tri_indices;
  Vector<float3x3> pixel_to_position;

  /* Assuming a quad mesh, we'll have at least 2 * faces entries. */
  tri_indices.reserve(node.faces().size() * 2);
  pixel_to_position.reserve(node.faces().size() * 2);

  const Span<int3> corner_tris = mesh_data.corner_tris;
  const Span<float2> uv_map = mesh_data.uv_map;

  /* For multiple UDIM tiles, compute which ones this node overlaps with. */
  const bool multi_tile = BLI_listbase_count(&image.tiles) > 1;
  Vector<int2, 8> node_tiles;
  if (multi_tile) {
    int2 tiles_min(INT_MAX);
    int2 tiles_max(INT_MIN);
    for (ImageTile &tile : image.tiles) {
      math::min_max(image::ImageTileWrapper(&tile).get_tile_offset(), tiles_min, tiles_max);
    }

    for (const int face : node.faces()) {
      for (const int tri : bke::mesh::face_triangles_range(mesh_data.faces, face)) {
        if (uv_prim_lookup.lookup[tri].is_empty()) {
          continue;
        }
        float2 uv_min(FLT_MAX);
        float2 uv_max(-FLT_MAX);
        for (int i = 0; i < 3; i++) {
          const float2 uv = uv_map[corner_tris[tri][i]];
          uv_min = math::min(uv_min, uv);
          uv_max = math::max(uv_max, uv);
        }
        /* Bound by UDIM tiles to guard against very large UV coordinates. */
        const int2 tile_min = math::clamp(int2(math::floor(uv_min)), tiles_min, tiles_max);
        const int2 tile_max = math::clamp(int2(math::floor(uv_max)), tiles_min, tiles_max);
        for (int ty = tile_min.y; ty <= tile_max.y; ty++) {
          for (int tx = tile_min.x; tx <= tile_max.x; tx++) {
            node_tiles.append_non_duplicates(int2(tx, ty));
          }
        }
      }
    }
  }

  for (ImageTile &tile : image.tiles) {
    image::ImageTileWrapper image_tile(&tile);
    const int2 tile_offset_i = image_tile.get_tile_offset();
    const float2 tile_offset = float2(tile_offset_i);

    /* Skip UDIM tiles none of the node's primitives cover. */
    if (multi_tile && !node_tiles.contains(tile_offset_i)) {
      continue;
    }

    /* Skip UDIM tiles that have no mask. */
    const uv_islands::UVIslandsMask::Tile *mask_tile = uv_masks.find_tile(tile_offset +
                                                                          float2(0.5f, 0.5f));
    if (mask_tile == nullptr) {
      continue;
    }

    image_user.tile = image_tile.get_tile_number();
    ImBuf *image_buffer = BKE_image_acquire_ibuf(&image, &image_user, nullptr);
    if (image_buffer == nullptr) {
      continue;
    }
    const float inv_w = 1.0f / image_buffer->x;
    const float inv_h = 1.0f / image_buffer->y;

    UDIMTilePixels tile_data;
    tile_data.tile_number = image_tile.get_tile_number();
    Vector<BuildPixelRow> build_rows;

    for (const int face : node.faces()) {
      for (const int tri : bke::mesh::face_triangles_range(mesh_data.faces, face)) {
        for (const UVPrimitiveLookup::Entry &entry : uv_prim_lookup.lookup[tri]) {
          const uv_islands::UVIsland &island = islands[entry.uv_island_index];
          const uv_islands::UVPrimitive &uv_primitive = *entry.uv_primitive;
          const float2 uvs[3] = {
              island.uv_verts[uv_primitive.get_uv_vert(island, mesh_data, 0)].uv - tile_offset,
              island.uv_verts[uv_primitive.get_uv_vert(island, mesh_data, 1)].uv - tile_offset,
              island.uv_verts[uv_primitive.get_uv_vert(island, mesh_data, 2)].uv - tile_offset,
          };
          const float minv = clamp_f(std::min({uvs[0].y, uvs[1].y, uvs[2].y}), 0.0f, 1.0f);
          const int miny = floor(minv * image_buffer->y);
          const float maxv = clamp_f(std::max({uvs[0].y, uvs[1].y, uvs[2].y}), 0.0f, 1.0f);
          const int maxy = min_ii(ceil(maxv * image_buffer->y), image_buffer->y);
          const float minu = clamp_f(std::min({uvs[0].x, uvs[1].x, uvs[2].x}), 0.0f, 1.0f);
          const int minx = floor(minu * image_buffer->x);
          const float maxu = clamp_f(std::max({uvs[0].x, uvs[1].x, uvs[2].x}), 0.0f, 1.0f);
          const int maxx = min_ii(ceil(maxu * image_buffer->x), image_buffer->x);

          /* Skip primitives that don't overlap this tile. */
          if (minx >= maxx || miny >= maxy) {
            continue;
          }

          const int uv_prim_index = tri_indices.size();
          const int64_t build_rows_num = build_rows.size();
          extract_barycentric_pixels(build_rows,
                                     image_buffer,
                                     *mask_tile,
                                     entry.uv_island_index,
                                     uv_prim_index,
                                     uvs,
                                     minx,
                                     miny,
                                     maxx,
                                     maxy);

          /* Don't append primitive if no pixels where written to this tile. */
          if (build_rows.size() == build_rows_num) {
            continue;
          }
          tri_indices.append(tri);
          pixel_to_position.append(calc_pixel_to_position_map(mesh_data, tri, uvs, inv_w, inv_h));
        }
      }
    }
    BKE_image_release_ibuf(&image, image_buffer, nullptr);

    if (build_rows.is_empty()) {
      continue;
    }

    build_pixel_row_runs(build_rows, tile_data);

    BLI_assert(pixel_to_position.size() == tri_indices.size());

    pixel_node.tiles.append(tile_data);
  }

  /* Assign to Array, to avoid wasting reserved space. */
  pixel_node.uv_primitives.tri_indices = tri_indices.as_span();
  pixel_node.uv_primitives.pixel_to_position = pixel_to_position.as_span();
}

/**
 * Find the nodes that needs to be updated.
 *
 * The nodes that require updated are added to the r_nodes_to_update parameter.
 * Will fill in r_visited_polygons with polygons that are owned by nodes that do not require
 * updates.
 *
 * returns if there were any nodes found (true).
 */
static IndexMask find_nodes_to_update(Tree &pbvh, IndexMaskMemory &memory)
{
  MutableSpan<MeshNode> nodes = pbvh.nodes<MeshNode>();
  if (pbvh.pixels_ == nullptr) {
    pbvh.pixels_ = MEM_new<PixelData>(__func__);
    pbvh.pixels_->nodes.reinitialize(nodes.size());
  }

  PixelData &pixel_data = *pbvh.pixels_;
  MutableSpan<PixelNode> pixel_nodes = pixel_data.nodes;
  IndexMask leaf_nodes = all_leaf_nodes(pbvh, memory);
  IndexMask nodes_to_update = pbvh.pixels_->flags.dirty ?
                                  leaf_nodes :
                                  IndexMask::from_predicate(leaf_nodes, memory, [&](const int i) {
                                    return pixel_nodes[i].flags.rebuild;
                                  });

  if (nodes_to_update.is_empty()) {
    return nodes_to_update;
  }

  nodes_to_update.foreach_index([&](const int i) { pixel_nodes[i].clear_data(); });

  return nodes_to_update;
}

static void apply_watertight_check(Tree &pbvh, Image &image, ImageUser &image_user)
{
  ImageUser watertight = image_user;
  for (ImageTile &tile_data : image.tiles) {
    image::ImageTileWrapper image_tile(&tile_data);
    watertight.tile = image_tile.get_tile_number();
    ImBuf *image_buffer = BKE_image_acquire_ibuf(&image, &watertight, nullptr);
    if (image_buffer == nullptr) {
      continue;
    }

    float *data_float = image_buffer->float_data_for_write();
    uint8_t *data_byte = image_buffer->byte_data_for_write();

    IndexMaskMemory memory;
    IndexMask leaf_nodes = all_leaf_nodes(pbvh, memory);
    PixelData &pixel_data = *pbvh.pixels_;
    leaf_nodes.foreach_index([&](const int i) {
      PixelNode &pixel_node = pixel_data.nodes[i];
      UDIMTilePixels *tile_node_data = pixel_node.find_tile_data(image_tile);
      if (tile_node_data == nullptr) {
        return;
      }

      const int num_runs = tile_node_data->pixel_row_run_starts.size() - 1;
      for (int run_i = 0; run_i < num_runs; run_i++) {
        int run_x = tile_node_data->pixel_row_run_start_coords[run_i].x;
        const int run_y = tile_node_data->pixel_row_run_start_coords[run_i].y;
        const int run_begin = tile_node_data->pixel_row_run_starts[run_i];
        const int run_end = tile_node_data->pixel_row_run_starts[run_i + 1];
        for (int k = run_begin; k < run_end; k++) {
          const PackedPixelRow &pixel_row = tile_node_data->pixel_rows[k];
          int pixel_offset = run_y * image_buffer->x + run_x;
          for (int x = 0; x < pixel_row.num_pixels; x++) {
            if (data_float) {
              copy_v4_fl(&data_float[pixel_offset * 4], 1.0);
            }
            if (data_byte) {
              uint8_t *dest = &data_byte[pixel_offset * 4];
              dest[0] = dest[1] = dest[2] = dest[3] = 255;
            }
            pixel_offset += 1;
          }
          run_x += pixel_row.num_pixels;
        }
      }
    });
    IMB_partial_update_mark_full(image_buffer);
    IMB_mark_dirty(image_buffer);
    BKE_image_release_ibuf(&image, image_buffer, nullptr);
  }
}

static bool update_pixels(const Depsgraph &depsgraph,
                          const Object &object,
                          Tree &pbvh,
                          Image &image,
                          ImageUser &image_user)
{
  IndexMaskMemory memory;
  const IndexMask nodes_to_update = find_nodes_to_update(pbvh, memory);
  if (nodes_to_update.is_empty()) {
    return false;
  }

  const Mesh &mesh = *id_cast<const Mesh *>(object.data);
  const StringRef active_uv_name = mesh.active_uv_map_name();
  if (active_uv_name.is_empty()) {
    return false;
  }

  const AttributeAccessor attributes = mesh.attributes();
  const VArraySpan uv_map = *attributes.lookup<float2>(active_uv_name, AttrDomain::Corner);

  uv_islands::MeshData mesh_data(mesh.faces(),
                                 mesh.corner_tris(),
                                 mesh.corner_verts(),
                                 uv_map,
                                 bke::pbvh::vert_positions_eval(depsgraph, object));

  /* Group primitives by island. */
  Array<int> island_tri_offset_data;
  Array<int> island_tri_index_data;
  const GroupedSpan<int> tris_by_island = offset_indices::build_groups_from_indices(
      mesh_data.uv_island_ids,
      mesh_data.uv_island_len,
      island_tri_offset_data,
      island_tri_index_data);

  uv_islands::UVIslandsMask uv_masks;
  ImageUser tile_user = image_user;
  for (ImageTile &tile_data : image.tiles) {
    image::ImageTileWrapper image_tile(&tile_data);
    tile_user.tile = image_tile.get_tile_number();
    ImBuf *tile_buffer = BKE_image_acquire_ibuf(&image, &tile_user, nullptr);
    if (tile_buffer == nullptr) {
      continue;
    }
    uv_masks.add_tile(float2(image_tile.get_tile_x_offset(), image_tile.get_tile_y_offset()),
                      ushort2(tile_buffer->x, tile_buffer->y));
    BKE_image_release_ibuf(&image, tile_buffer, nullptr);
  }
  uv_masks.add(mesh_data, tris_by_island);
  uv_masks.dilate(image.seam_margin);

  Array<uv_islands::UVIsland> islands = uv_islands::build_uv_islands(
      mesh_data, tris_by_island, uv_masks);

  UVPrimitiveLookup uv_primitive_lookup(mesh_data.corner_tris.size(), islands);

  MutableSpan<MeshNode> nodes = pbvh.nodes<MeshNode>();
  MutableSpan<PixelNode> pixel_nodes = pbvh.pixels_->nodes;

  nodes_to_update.foreach_index(
      [&](const int i) {
        do_encode_pixels(mesh_data,
                         islands,
                         uv_masks,
                         uv_primitive_lookup,
                         image,
                         image_user,
                         nodes[i],
                         pixel_nodes[i]);
      },
      exec_mode::grain_size(1));
  if (USE_WATERTIGHT_CHECK) {
    apply_watertight_check(pbvh, image, image_user);
  }

  /* Add solution for non-manifold parts of the model. */
  copy_update(pbvh, image, image_user, mesh_data);

  /* Clear the UpdatePixels flag. */
  nodes_to_update.foreach_index([&](const int i) { pixel_nodes[i].flags.rebuild = false; });

  pbvh.pixels_->flags.dirty = false;

// #define DO_PRINT_STATISTICS
#ifdef DO_PRINT_STATISTICS
  /* Print statistics about the pixel row encoding size. */
  {
    int64_t rows_bytes = 0;
    int64_t num_pixels = 0;
    for (const PixelNode &pixel_node : pbvh.pixels_->nodes) {
      rows_bytes += int64_t(pixel_node.uv_primitives.pixel_to_position.size()) * sizeof(float3x3);
      for (const UDIMTilePixels &tile : pixel_node.tiles) {
        rows_bytes += int64_t(tile.pixel_rows.size()) * sizeof(PackedPixelRow);
        for (const PackedPixelRow &row : tile.pixel_rows) {
          num_pixels += row.num_pixels;
        }
      }
    }
    fmt::print("Encoded {} pixels in {} bytes ({} bytes per pixel)\n",
               num_pixels,
               rows_bytes,
               double(rows_bytes) / double(num_pixels));
  }
#endif

  return true;
}

PixelData &data_get(Tree &pbvh)
{
  BLI_assert(pbvh.pixels_ != nullptr);
  PixelData *data = pbvh.pixels_;
  return *data;
}

/* TODO: This is a awkward to have to re-iterate over the image tiles to find the matching tile.
 * Investigate storing the pointer on the `UDIMTilePixels` struct instead, or storing this as a
 * second map in `ImageData` */
static std::optional<image::ImageTileWrapper> find_image_tile(Image &image,
                                                              const image::TileNumber tile_number)
{
  for (ImageTile &image_tile : image.tiles) {
    image::ImageTileWrapper wrapper = image::ImageTileWrapper(&image_tile);
    if (wrapper.get_tile_number() == tile_number) {
      return std::make_optional(wrapper);
    }
  }
  /* Logically, we should be unable to reference a image_tile here without having first gotten it
   * from the image tile itself. */
  BLI_assert(0);
  return std::nullopt;
}

void mark_image_dirty(bke::pbvh::Node & /*node*/,
                      PixelNode &pixel_node,
                      Image &image,
                      Map<image::TileNumber, ImBuf *> &buffers)
{
  PRF_scope(ProfileCategory::Editor);
  if (pixel_node.flags.dirty) {
    for (UDIMTilePixels &tile : pixel_node.tiles) {
      std::optional<image::ImageTileWrapper> image_tile = find_image_tile(image, tile.tile_number);
      ImBuf *image_buffer = buffers.lookup_default(tile.tile_number, nullptr);
      if (image_buffer == nullptr || !image_tile) {
        continue;
      }

      pixel_node.mark_region(tile, *image_buffer);
    }
    pixel_node.flags.dirty = false;
  }
}

void collect_dirty_tiles(PixelNode &node, Vector<image::TileNumber> &r_dirty_tiles)
{
  node.collect_dirty_tiles(r_dirty_tiles);
}

}  // namespace bke::pbvh::pixels

namespace bke::pbvh {

void build_pixels(const Depsgraph &depsgraph, Object &object, Image &image, ImageUser &image_user)
{
  PRF_scope(ProfileCategory::Editor);
  Tree &pbvh = *object::pbvh_get(object);
  pixels::update_pixels(depsgraph, object, pbvh, image, image_user);
}

void pixels_free(Tree *pbvh)
{
  pixels::PixelData *pbvh_data = pbvh->pixels_;
  MEM_delete(pbvh_data);
  pbvh->pixels_ = nullptr;
}

}  // namespace bke::pbvh
}  // namespace blender
