/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_pbvh.h"
#include "BKE_pbvh_pixels.hh"

#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_task.h"

#include "BKE_image_wrappers.hh"

#include "bmesh.h"

#include "pbvh_intern.h"
#include "pbvh_uv_islands.hh"

namespace blender::bke::pbvh::pixels {

/**
 * During debugging this check could be enabled.
 * It will write to each image pixel that is covered by the PBVH.
 */
constexpr bool USE_WATERTIGHT_CHECK = false;

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

static void extract_barycentric_pixels(UDIMTilePixels &tile_data,
                                       const ImBuf *image_buffer,
                                       const uv_islands::UVIslandsMask &uv_mask,
                                       const int64_t uv_island_index,
                                       const int64_t uv_primitive_index,
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
static void update_geom_primitives(PBVH &pbvh, const uv_islands::MeshData &mesh_data)
{
  PBVHData &pbvh_data = BKE_pbvh_pixels_data_get(pbvh);
  pbvh_data.clear_data();
  for (const uv_islands::MeshPrimitive &mesh_primitive : mesh_data.primitives) {
    pbvh_data.geom_primitives.append(int3(mesh_primitive.vertices[0].vertex->v,
                                          mesh_primitive.vertices[1].vertex->v,
                                          mesh_primitive.vertices[2].vertex->v));
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

  UVPrimitiveLookup(const uint64_t geom_primitive_len, uv_islands::UVIslands &uv_islands)
  {
    lookup.append_n_times(Vector<Entry>(), geom_primitive_len);

    uint64_t uv_island_index = 0;
    for (uv_islands::UVIsland &uv_island : uv_islands.islands) {
      for (VectorList<uv_islands::UVPrimitive>::UsedVector &uv_primitives :
           uv_island.uv_primitives) {
        for (uv_islands::UVPrimitive &uv_primitive : uv_primitives) {
          lookup[uv_primitive.primitive->index].append_as(Entry(&uv_primitive, uv_island_index));
        }
      }
      uv_island_index++;
    }
  }
};

struct EncodePixelsUserData {
  Image *image;
  ImageUser *image_user;
  PBVH *pbvh;
  Vector<PBVHNode *> *nodes;
  const float2 *ldata_uv;
  const uv_islands::UVIslandsMask *uv_masks;
  /** Lookup to retrieve the UV primitives based on the primitive index. */
  const UVPrimitiveLookup *uv_primitive_lookup;
};

static void do_encode_pixels(void *__restrict userdata,
                             const int n,
                             const TaskParallelTLS *__restrict /*tls*/)
{
  EncodePixelsUserData *data = static_cast<EncodePixelsUserData *>(userdata);
  Image *image = data->image;
  ImageUser image_user = *data->image_user;
  PBVHNode *node = (*data->nodes)[n];
  NodeData *node_data = static_cast<NodeData *>(node->pixels.node_data);
  const uv_islands::UVIslandsMask &uv_masks = *data->uv_masks;

  LISTBASE_FOREACH (ImageTile *, tile, &data->image->tiles) {
    image::ImageTileWrapper image_tile(tile);
    image_user.tile = image_tile.get_tile_number();
    ImBuf *image_buffer = BKE_image_acquire_ibuf(image, &image_user, nullptr);
    if (image_buffer == nullptr) {
      continue;
    }

    UDIMTilePixels tile_data;
    tile_data.tile_number = image_tile.get_tile_number();
    float2 tile_offset = float2(image_tile.get_tile_offset());

    for (int pbvh_node_prim_index = 0; pbvh_node_prim_index < node->totprim;
         pbvh_node_prim_index++) {
      int64_t geom_prim_index = node->prim_indices[pbvh_node_prim_index];
      for (const UVPrimitiveLookup::Entry &entry :
           data->uv_primitive_lookup->lookup[geom_prim_index]) {
        uv_islands::UVBorder uv_border = entry.uv_primitive->extract_border();
        float2 uvs[3] = {
            entry.uv_primitive->get_uv_vertex(0)->uv - tile_offset,
            entry.uv_primitive->get_uv_vertex(1)->uv - tile_offset,
            entry.uv_primitive->get_uv_vertex(2)->uv - tile_offset,
        };
        const float minv = clamp_f(min_fff(uvs[0].y, uvs[1].y, uvs[2].y), 0.0f, 1.0f);
        const int miny = floor(minv * image_buffer->y);
        const float maxv = clamp_f(max_fff(uvs[0].y, uvs[1].y, uvs[2].y), 0.0f, 1.0f);
        const int maxy = min_ii(ceil(maxv * image_buffer->y), image_buffer->y);
        const float minu = clamp_f(min_fff(uvs[0].x, uvs[1].x, uvs[2].x), 0.0f, 1.0f);
        const int minx = floor(minu * image_buffer->x);
        const float maxu = clamp_f(max_fff(uvs[0].x, uvs[1].x, uvs[2].x), 0.0f, 1.0f);
        const int maxx = min_ii(ceil(maxu * image_buffer->x), image_buffer->x);

        /* TODO: Perform bounds check */
        int64_t uv_prim_index = node_data->uv_primitives.size();
        node_data->uv_primitives.append(geom_prim_index);
        UVPrimitivePaintInput &paint_input = node_data->uv_primitives.last();

        /* Calculate barycentric delta */
        paint_input.delta_barycentric_coord_u = calc_barycentric_delta_x(
            image_buffer, uvs, minx, miny);

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
    BKE_image_release_ibuf(image, image_buffer, nullptr);

    if (tile_data.pixel_rows.is_empty()) {
      continue;
    }

    node_data->tiles.append(tile_data);
  }
}

static bool should_pixels_be_updated(PBVHNode *node)
{
  if ((node->flag & PBVH_Leaf) == 0) {
    return false;
  }
  if ((node->flag & PBVH_RebuildPixels) != 0) {
    return true;
  }
  NodeData *node_data = static_cast<NodeData *>(node->pixels.node_data);
  if (node_data != nullptr) {
    return false;
  }
  return true;
}

static int64_t count_nodes_to_update(PBVH *pbvh)
{
  int64_t result = 0;
  for (int n = 0; n < pbvh->totnode; n++) {
    PBVHNode *node = &pbvh->nodes[n];
    if (should_pixels_be_updated(node)) {
      result++;
    }
  }
  return result;
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
static bool find_nodes_to_update(PBVH *pbvh, Vector<PBVHNode *> &r_nodes_to_update)
{
  int64_t nodes_to_update_len = count_nodes_to_update(pbvh);
  if (nodes_to_update_len == 0) {
    return false;
  }

  /* Init or reset PBVH pixel data when changes detected. */
  if (pbvh->pixels.data == nullptr) {
    PBVHData *pbvh_data = MEM_new<PBVHData>(__func__);
    pbvh->pixels.data = pbvh_data;
  }
  else {
    PBVHData *pbvh_data = static_cast<PBVHData *>(pbvh->pixels.data);
    pbvh_data->clear_data();
  }

  r_nodes_to_update.reserve(nodes_to_update_len);

  for (int n = 0; n < pbvh->totnode; n++) {
    PBVHNode *node = &pbvh->nodes[n];
    if (!should_pixels_be_updated(node)) {
      continue;
    }
    r_nodes_to_update.append(node);
    node->flag = static_cast<PBVHNodeFlags>(node->flag | PBVH_RebuildPixels);

    if (node->pixels.node_data == nullptr) {
      NodeData *node_data = MEM_new<NodeData>(__func__);
      node->pixels.node_data = node_data;
    }
    else {
      NodeData *node_data = static_cast<NodeData *>(node->pixels.node_data);
      node_data->clear_data();
    }
  }

  return true;
}

static void apply_watertight_check(PBVH *pbvh, Image *image, ImageUser *image_user)
{
  ImageUser watertight = *image_user;
  LISTBASE_FOREACH (ImageTile *, tile_data, &image->tiles) {
    image::ImageTileWrapper image_tile(tile_data);
    watertight.tile = image_tile.get_tile_number();
    ImBuf *image_buffer = BKE_image_acquire_ibuf(image, &watertight, nullptr);
    if (image_buffer == nullptr) {
      continue;
    }
    for (int n = 0; n < pbvh->totnode; n++) {
      PBVHNode *node = &pbvh->nodes[n];
      if ((node->flag & PBVH_Leaf) == 0) {
        continue;
      }
      NodeData *node_data = static_cast<NodeData *>(node->pixels.node_data);
      UDIMTilePixels *tile_node_data = node_data->find_tile_data(image_tile);
      if (tile_node_data == nullptr) {
        continue;
      }

      for (PackedPixelRow &pixel_row : tile_node_data->pixel_rows) {
        int pixel_offset = pixel_row.start_image_coordinate.y * image_buffer->x +
                           pixel_row.start_image_coordinate.x;
        for (int x = 0; x < pixel_row.num_pixels; x++) {
          if (image_buffer->rect_float) {
            copy_v4_fl(&image_buffer->rect_float[pixel_offset * 4], 1.0);
          }
          if (image_buffer->rect) {
            uint8_t *dest = static_cast<uint8_t *>(
                static_cast<void *>(&image_buffer->rect[pixel_offset]));
            copy_v4_uchar(dest, 255);
          }
          pixel_offset += 1;
        }
      }
    }
    BKE_image_release_ibuf(image, image_buffer, nullptr);
  }
  BKE_image_partial_update_mark_full_update(image);
}

static void update_pixels(PBVH *pbvh, Mesh *mesh, Image *image, ImageUser *image_user)
{
  Vector<PBVHNode *> nodes_to_update;

  if (!find_nodes_to_update(pbvh, nodes_to_update)) {
    return;
  }

  const float2 *ldata_uv = static_cast<const float2 *>(
      CustomData_get_layer(&mesh->ldata, CD_PROP_FLOAT2));
  if (ldata_uv == nullptr) {
    return;
  }

  uv_islands::MeshData mesh_data({pbvh->looptri, pbvh->totprim},
                                 {pbvh->mloop, mesh->totloop},
                                 pbvh->totvert,
                                 {ldata_uv, mesh->totloop});
  uv_islands::UVIslands islands(mesh_data);

  uv_islands::UVIslandsMask uv_masks;
  ImageUser tile_user = *image_user;
  LISTBASE_FOREACH (ImageTile *, tile_data, &image->tiles) {
    image::ImageTileWrapper image_tile(tile_data);
    tile_user.tile = image_tile.get_tile_number();
    ImBuf *tile_buffer = BKE_image_acquire_ibuf(image, &tile_user, nullptr);
    if (tile_buffer == nullptr) {
      continue;
    }
    uv_masks.add_tile(float2(image_tile.get_tile_x_offset(), image_tile.get_tile_y_offset()),
                      ushort2(tile_buffer->x, tile_buffer->y));
    BKE_image_release_ibuf(image, tile_buffer, nullptr);
  }
  uv_masks.add(islands);
  uv_masks.dilate(image->seam_margin);

  islands.extract_borders();
  islands.extend_borders(uv_masks);
  update_geom_primitives(*pbvh, mesh_data);

  UVPrimitiveLookup uv_primitive_lookup(mesh_data.looptris.size(), islands);

  EncodePixelsUserData user_data;
  user_data.pbvh = pbvh;
  user_data.image = image;
  user_data.image_user = image_user;
  user_data.ldata_uv = ldata_uv;
  user_data.nodes = &nodes_to_update;
  user_data.uv_primitive_lookup = &uv_primitive_lookup;
  user_data.uv_masks = &uv_masks;

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, nodes_to_update.size());
  BLI_task_parallel_range(0, nodes_to_update.size(), &user_data, do_encode_pixels, &settings);
  if (USE_WATERTIGHT_CHECK) {
    apply_watertight_check(pbvh, image, image_user);
  }

  /* Rebuild the undo regions. */
  for (PBVHNode *node : nodes_to_update) {
    NodeData *node_data = static_cast<NodeData *>(node->pixels.node_data);
    node_data->rebuild_undo_regions();
  }

  /* Clear the UpdatePixels flag. */
  for (PBVHNode *node : nodes_to_update) {
    node->flag = static_cast<PBVHNodeFlags>(node->flag & ~PBVH_RebuildPixels);
  }

//#define DO_PRINT_STATISTICS
#ifdef DO_PRINT_STATISTICS
  /* Print some statistics about compression ratio. */
  {
    int64_t compressed_data_len = 0;
    int64_t num_pixels = 0;
    for (int n = 0; n < pbvh->totnode; n++) {
      PBVHNode *node = &pbvh->nodes[n];
      if ((node->flag & PBVH_Leaf) == 0) {
        continue;
      }
      NodeData *node_data = static_cast<NodeData *>(node->pixels.node_data);
      compressed_data_len += node_data->triangles.mem_size();
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
}

NodeData &BKE_pbvh_pixels_node_data_get(PBVHNode &node)
{
  BLI_assert(node.pixels.node_data != nullptr);
  NodeData *node_data = static_cast<NodeData *>(node.pixels.node_data);
  return *node_data;
}

PBVHData &BKE_pbvh_pixels_data_get(PBVH &pbvh)
{
  BLI_assert(pbvh.pixels.data != nullptr);
  PBVHData *data = static_cast<PBVHData *>(pbvh.pixels.data);
  return *data;
}

void BKE_pbvh_pixels_mark_image_dirty(PBVHNode &node, Image &image, ImageUser &image_user)
{
  BLI_assert(node.pixels.node_data != nullptr);
  NodeData *node_data = static_cast<NodeData *>(node.pixels.node_data);
  if (node_data->flags.dirty) {
    ImageUser local_image_user = image_user;
    LISTBASE_FOREACH (ImageTile *, tile, &image.tiles) {
      image::ImageTileWrapper image_tile(tile);
      local_image_user.tile = image_tile.get_tile_number();
      ImBuf *image_buffer = BKE_image_acquire_ibuf(&image, &local_image_user, nullptr);
      if (image_buffer == nullptr) {
        continue;
      }

      node_data->mark_region(image, image_tile, *image_buffer);
      BKE_image_release_ibuf(&image, image_buffer, nullptr);
    }
    node_data->flags.dirty = false;
  }
}

}  // namespace blender::bke::pbvh::pixels

extern "C" {
using namespace blender::bke::pbvh::pixels;

void BKE_pbvh_build_pixels(PBVH *pbvh, Mesh *mesh, Image *image, ImageUser *image_user)
{
  update_pixels(pbvh, mesh, image, image_user);
}

void pbvh_node_pixels_free(PBVHNode *node)
{
  NodeData *node_data = static_cast<NodeData *>(node->pixels.node_data);
  MEM_delete(node_data);
  node->pixels.node_data = nullptr;
}

void pbvh_pixels_free(PBVH *pbvh)
{
  PBVHData *pbvh_data = static_cast<PBVHData *>(pbvh->pixels.data);
  MEM_delete(pbvh_data);
  pbvh->pixels.data = nullptr;
}
}
