/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_paint_bvh_pixels.hh"

#include "DNA_image_types.h"
#include "DNA_object_types.h"

#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"

#include "BKE_image_wrappers.hh"
#include "BKE_paint.hh"

#include "pbvh_intern.hh"
#include "pbvh_pixels_copy.hh"
#include "pbvh_uv_islands.hh"

namespace blender {

namespace bke::pbvh::pixels {

/**
 * Calculate the delta of two neighbor UV coordinates in the given image buffer.
 */
static float2 calc_barycentric_delta(const float2 uvs[3],
                                     const float2 start_uv,
                                     const float2 end_uv)
{

  float3 start_barycentric;
  barycentric_weights_v2(uvs[0], uvs[1], uvs[2], start_uv, start_barycentric);
  float3 end_barycentric;
  barycentric_weights_v2(uvs[0], uvs[1], uvs[2], end_uv, end_barycentric);
  float3 barycentric = end_barycentric - start_barycentric;
  return float2(barycentric.x, barycentric.y);
}

static float2 calc_barycentric_delta_x(const ImBuf *image_buffer,
                                       const float2 uvs[3],
                                       const int x,
                                       const int y)
{
  const float2 start_uv(float(x) / image_buffer->x, float(y) / image_buffer->y);
  const float2 end_uv(float(x + 1) / image_buffer->x, float(y) / image_buffer->y);
  return calc_barycentric_delta(uvs, start_uv, end_uv);
}

/**
 * During debugging this check could be enabled.
 * It will write to each image pixel that is covered by the Tree.
 */
constexpr bool USE_WATERTIGHT_CHECK = false;

static void extract_barycentric_pixels(UDIMTilePixels &tile_data,
                                       const ImBuf *image_buffer,
                                       const uv_islands::UVIslandsMask &uv_mask,
                                       const int uv_island_index,
                                       const int uv_primitive_index,
                                       const float2 uvs[3],
                                       const float2 tile_offset,
                                       const int minx,
                                       const int miny,
                                       const int maxx,
                                       const int maxy)
{
  for (int y = miny; y < maxy; y++) {
    bool start_detected = false;
    PackedPixelRow pixel_row;
    pixel_row.uv_primitive_index = uv_primitive_index;
    pixel_row.num_pixels = 0;
    int x;

    for (x = minx; x < maxx; x++) {
      float2 uv((float(x) + 0.5f) / image_buffer->x, (float(y) + 0.5f) / image_buffer->y);
      float3 barycentric_weights;
      barycentric_weights_v2(uvs[0], uvs[1], uvs[2], uv, barycentric_weights);

      const bool is_inside = barycentric_inside_triangle_v2(barycentric_weights);
      const bool is_masked = uv_mask.is_masked(uv_island_index, uv + tile_offset);
      if (!start_detected && is_inside && is_masked) {
        start_detected = true;
        pixel_row.start_image_coordinate = ushort2(x, y);
        pixel_row.start_barycentric_coord = float2(barycentric_weights.x, barycentric_weights.y);
      }
      else if (start_detected && (!is_inside || !is_masked)) {
        break;
      }
    }

    if (!start_detected) {
      continue;
    }
    pixel_row.num_pixels = x - pixel_row.start_image_coordinate.x;
    tile_data.pixel_rows.append(pixel_row);
  }
}

/** Update the geometry primitives of the pbvh. */
static void update_geom_primitives(Tree &pbvh, const uv_islands::MeshData &mesh_data)
{
  PixelData &pbvh_data = data_get(pbvh);
  pbvh_data.vert_tris.reinitialize(mesh_data.corner_tris.size());
  bke::mesh::vert_tris_from_corner_tris(
      mesh_data.corner_verts, mesh_data.corner_tris, pbvh_data.vert_tris);
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

  UVPrimitiveLookup(const uint64_t geom_primitive_len, uv_islands::UVIslands &uv_islands)
  {
    lookup.append_n_times(Vector<Entry>(), geom_primitive_len);

    uint64_t uv_island_index = 0;
    for (uv_islands::UVIsland &uv_island : uv_islands.islands) {
      for (uv_islands::UVPrimitive &uv_primitive : uv_island.uv_primitives) {
        lookup[uv_primitive.primitive_i].append_as(Entry(&uv_primitive, uv_island_index));
      }
      uv_island_index++;
    }
  }
};

static void do_encode_pixels(const uv_islands::MeshData &mesh_data,
                             const uv_islands::UVIslandsMask &uv_masks,
                             const UVPrimitiveLookup &uv_prim_lookup,
                             Image &image,
                             ImageUser &image_user,
                             MeshNode &node,
                             PixelNode &pixel_node)
{
  BLI_assert(pixel_node.flags.rebuild ||
             (pixel_node.uv_primitives.tri_indices.is_empty() &&
              pixel_node.uv_primitives.delta_barycentric_coords.is_empty() &&
              pixel_node.tiles.is_empty()));
  /* Assuming a quad mesh, we'll have at least 2 * faces entries */
  pixel_node.uv_primitives.tri_indices.reserve(node.faces().size() * 2);
  pixel_node.uv_primitives.delta_barycentric_coords.reserve(node.faces().size() * 2);

  for (ImageTile &tile : image.tiles) {
    image::ImageTileWrapper image_tile(&tile);
    image_user.tile = image_tile.get_tile_number();
    ImBuf *image_buffer = BKE_image_acquire_ibuf(&image, &image_user, nullptr);
    if (image_buffer == nullptr) {
      continue;
    }

    UDIMTilePixels tile_data;
    tile_data.tile_number = image_tile.get_tile_number();
    float2 tile_offset = float2(image_tile.get_tile_offset());

    for (const int face : node.faces()) {
      for (const int tri : bke::mesh::face_triangles_range(mesh_data.faces, face)) {
        for (const UVPrimitiveLookup::Entry &entry : uv_prim_lookup.lookup[tri]) {
          uv_islands::UVBorder uv_border = entry.uv_primitive->extract_border();
          float2 uvs[3] = {
              entry.uv_primitive->get_uv_vertex(mesh_data, 0)->uv - tile_offset,
              entry.uv_primitive->get_uv_vertex(mesh_data, 1)->uv - tile_offset,
              entry.uv_primitive->get_uv_vertex(mesh_data, 2)->uv - tile_offset,
          };
          const float minv = clamp_f(min_fff(uvs[0].y, uvs[1].y, uvs[2].y), 0.0f, 1.0f);
          const int miny = floor(minv * image_buffer->y);
          const float maxv = clamp_f(max_fff(uvs[0].y, uvs[1].y, uvs[2].y), 0.0f, 1.0f);
          const int maxy = min_ii(ceil(maxv * image_buffer->y), image_buffer->y);
          const float minu = clamp_f(min_fff(uvs[0].x, uvs[1].x, uvs[2].x), 0.0f, 1.0f);
          const int minx = floor(minu * image_buffer->x);
          const float maxu = clamp_f(max_fff(uvs[0].x, uvs[1].x, uvs[2].x), 0.0f, 1.0f);
          const int maxx = min_ii(ceil(maxu * image_buffer->x), image_buffer->x);

          const int uv_prim_index = pixel_node.uv_primitives.tri_indices.size();
          pixel_node.uv_primitives.tri_indices.append(tri);
          pixel_node.uv_primitives.delta_barycentric_coords.append(
              calc_barycentric_delta_x(image_buffer, uvs, minx, miny));

          /* Extract the pixels. */
          extract_barycentric_pixels(tile_data,
                                     image_buffer,
                                     uv_masks,
                                     entry.uv_island_index,
                                     uv_prim_index,
                                     uvs,
                                     tile_offset,
                                     minx,
                                     miny,
                                     maxx,
                                     maxy);
        }
      }
    }
    BKE_image_release_ibuf(&image, image_buffer, nullptr);

    if (tile_data.pixel_rows.is_empty()) {
      continue;
    }

    BLI_assert(pixel_node.uv_primitives.delta_barycentric_coords.size() ==
               pixel_node.uv_primitives.tri_indices.size());

    pixel_node.tiles.append(tile_data);
  }
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
    IndexMaskMemory memory;
    IndexMask leaf_nodes = all_leaf_nodes(pbvh, memory);
    PixelData &pixel_data = *pbvh.pixels_;
    leaf_nodes.foreach_index([&](const int i) {
      PixelNode &pixel_node = pixel_data.nodes[i];
      UDIMTilePixels *tile_node_data = pixel_node.find_tile_data(image_tile);
      if (tile_node_data == nullptr) {
        return;
      }

      for (PackedPixelRow &pixel_row : tile_node_data->pixel_rows) {
        int pixel_offset = pixel_row.start_image_coordinate.y * image_buffer->x +
                           pixel_row.start_image_coordinate.x;
        for (int x = 0; x < pixel_row.num_pixels; x++) {
          if (image_buffer->float_buffer.data) {
            copy_v4_fl(&image_buffer->float_buffer.data[pixel_offset * 4], 1.0);
          }
          if (image_buffer->byte_buffer.data) {
            uint8_t *dest = &image_buffer->byte_buffer.data[pixel_offset * 4];
            dest[0] = dest[1] = dest[2] = dest[3] = 255;
          }
          pixel_offset += 1;
        }
      }
    });
    BKE_image_release_ibuf(&image, image_buffer, nullptr);
  }
  BKE_image_partial_update_mark_full_update(&image);
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
  uv_islands::UVIslands islands(mesh_data);

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
  uv_masks.add(mesh_data, islands);
  uv_masks.dilate(image.seam_margin);

  islands.extract_borders();
  islands.extend_borders(mesh_data, uv_masks);
  update_geom_primitives(pbvh, mesh_data);

  UVPrimitiveLookup uv_primitive_lookup(mesh_data.corner_tris.size(), islands);

  MutableSpan<MeshNode> nodes = pbvh.nodes<MeshNode>();
  MutableSpan<PixelNode> pixel_nodes = pbvh.pixels_->nodes;

  nodes_to_update.foreach_index([&](const int i) {
    do_encode_pixels(
        mesh_data, uv_masks, uv_primitive_lookup, image, image_user, nodes[i], pixel_nodes[i]);
  });
  if (USE_WATERTIGHT_CHECK) {
    apply_watertight_check(pbvh, image, image_user);
  }

  /* Add solution for non-manifold parts of the model. */
  copy_update(pbvh, image, image_user, mesh_data);

  /* Rebuild the undo regions. */
  nodes_to_update.foreach_index([&](const int i) { pixel_nodes[i].rebuild_undo_regions(); });

  /* Clear the UpdatePixels flag. */
  nodes_to_update.foreach_index([&](const int i) { pixel_nodes[i].flags.rebuild = false; });

  pbvh.pixels_->flags.dirty = false;

// #define DO_PRINT_STATISTICS
#ifdef DO_PRINT_STATISTICS
  /* Print some statistics about compression ratio. */
  {
    int compressed_data_len = 0;
    int num_pixels = 0;
    for (int n = 0; n < pbvh->totnode; n++) {
      Node *node = &pbvh->nodes[n];
      if ((node->flag & Node::Leaf) == 0) {
        continue;
      }
      NodeData *node_data = static_cast<NodeData *>(node->pixels.node_data);
      for (const UDIMTilePixels &tile_data : node_data->tiles) {
        compressed_data_len += tile_data.encoded_pixels.size() * sizeof(PackedPixelRow);
        for (const PackedPixelRow &encoded_pixels : tile_data.encoded_pixels) {
          num_pixels += encoded_pixels.num_pixels;
        }
      }
    }
    printf("Encoded %lld pixels in %lld bytes (%f bytes per pixel)\n",
           num_pixels,
           compressed_data_len,
           float(compressed_data_len) / num_pixels);
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
  if (pixel_node.flags.dirty) {
    for (UDIMTilePixels &tile : pixel_node.tiles) {
      std::optional<image::ImageTileWrapper> image_tile = find_image_tile(image, tile.tile_number);
      ImBuf *image_buffer = buffers.lookup_default(tile.tile_number, nullptr);
      if (image_buffer == nullptr || !image_tile) {
        continue;
      }

      pixel_node.mark_region(tile, image, *image_tile, *image_buffer);
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
