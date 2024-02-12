/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_pbvh_pixels.hh"

#include "DNA_image_types.h"
#include "DNA_object_types.h"

#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_task.h"
#include "BLI_time.h"

#include "BKE_global.hh"
#include "BKE_image_wrappers.hh"

#include "pbvh_intern.hh"
#include "pbvh_pixels_copy.hh"
#include "pbvh_uv_islands.hh"

namespace blender::bke::pbvh::pixels {

/**
 * Splitting of pixel nodes has been disabled as it was designed for C. When migrating to CPP
 * the splitting data structure will corrupt memory.
 *
 * TODO(jbakker): This should be fixed or replaced with a different solution. If we go into a
 * direction of compute shaders this might not be needed anymore.
 */
constexpr bool PBVH_PIXELS_SPLIT_NODES_ENABLED = false;

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

static int count_node_pixels(PBVHNode &node)
{
  if (!node.pixels.node_data) {
    return 0;
  }

  NodeData &data = node_data_get(node);

  int totpixel = 0;

  for (UDIMTilePixels &tile : data.tiles) {
    for (PackedPixelRow &row : tile.pixel_rows) {
      totpixel += row.num_pixels;
    }
  }

  return totpixel;
}

struct SplitQueueData {
  ThreadQueue *new_nodes;
  TaskPool *pool;

  PBVH *pbvh;
  Mesh *mesh;
  Image *image;
  ImageUser *image_user;
};

struct SplitNodePair {
  SplitNodePair *parent;
  PBVHNode node;
  int children_offset = 0;
  int depth = 0;
  int source_index = -1;
  bool is_old = false;
  SplitQueueData *tdata;

  SplitNodePair(SplitNodePair *node_parent = nullptr) : parent(node_parent)
  {
    memset(static_cast<void *>(&node), 0, sizeof(PBVHNode));
  }
};

static void split_thread_job(TaskPool *__restrict pool, void *taskdata);

static void split_pixel_node(
    PBVH *pbvh, SplitNodePair *split, Image *image, ImageUser *image_user, SplitQueueData *tdata)
{
  PBVHNode *node = &split->node;

  const Bounds<float3> cb = node->vb;

  if (count_node_pixels(*node) <= pbvh->pixel_leaf_limit || split->depth >= pbvh->depth_limit) {
    node_data_get(split->node).rebuild_undo_regions();
    return;
  }

  /* Find widest axis and its midpoint */
  const int axis = math::dominant_axis(cb.max - cb.min);
  const float mid = (cb.max[axis] + cb.min[axis]) * 0.5f;

  node->flag = (PBVHNodeFlags)(int(node->flag) & int(~PBVH_TexLeaf));

  SplitNodePair *split1 = MEM_new<SplitNodePair>("split_pixel_node split1", split);
  SplitNodePair *split2 = MEM_new<SplitNodePair>("split_pixel_node split1", split);

  split1->depth = split->depth + 1;
  split2->depth = split->depth + 1;

  PBVHNode *child1 = &split1->node;
  PBVHNode *child2 = &split2->node;

  child1->flag = PBVH_TexLeaf;
  child2->flag = PBVH_TexLeaf;

  child1->vb = cb;
  child1->vb.max[axis] = mid;

  child2->vb = cb;
  child2->vb.min[axis] = mid;

  NodeData &data = node_data_get(split->node);

  NodeData *data1 = MEM_new<NodeData>(__func__);
  NodeData *data2 = MEM_new<NodeData>(__func__);
  child1->pixels.node_data = static_cast<void *>(data1);
  child2->pixels.node_data = static_cast<void *>(data2);

  data1->uv_primitives = data.uv_primitives;
  data2->uv_primitives = data.uv_primitives;

  data1->tiles.resize(data.tiles.size());
  data2->tiles.resize(data.tiles.size());

  for (int i : IndexRange(data.tiles.size())) {
    UDIMTilePixels &tile = data.tiles[i];
    UDIMTilePixels &tile1 = data1->tiles[i];
    UDIMTilePixels &tile2 = data2->tiles[i];

    tile1.tile_number = tile2.tile_number = tile.tile_number;
    tile1.flags.dirty = tile2.flags.dirty = false;
  }

  ImageUser image_user2 = *image_user;

  for (int i : IndexRange(data.tiles.size())) {
    const UDIMTilePixels &tile = data.tiles[i];

    image_user2.tile = tile.tile_number;

    ImBuf *image_buffer = BKE_image_acquire_ibuf(image, &image_user2, nullptr);
    if (image_buffer == nullptr) {
      continue;
    }

    const Span<float3> positions = BKE_pbvh_get_vert_positions(pbvh);
    PBVHData &pbvh_data = data_get(*pbvh);

    for (const PackedPixelRow &row : tile.pixel_rows) {
      UDIMTilePixels *tile1 = &data1->tiles[i];
      UDIMTilePixels *tile2 = &data2->tiles[i];

      UVPrimitivePaintInput &uv_prim = data.uv_primitives.paint_input[row.uv_primitive_index];
      int3 tri = pbvh_data.geom_primitives.vert_indices[uv_prim.geometry_primitive_index];

      float verts[3][3];

      copy_v3_v3(verts[0], positions[tri[0]]);
      copy_v3_v3(verts[1], positions[tri[1]]);
      copy_v3_v3(verts[2], positions[tri[2]]);

      float2 delta = uv_prim.delta_barycentric_coord_u;
      float2 uv1 = row.start_barycentric_coord;
      float2 uv2 = row.start_barycentric_coord + delta * float(row.num_pixels);

      float co1[3];
      float co2[3];

      interp_barycentric_tri_v3(verts, uv1[0], uv1[1], co1);
      interp_barycentric_tri_v3(verts, uv2[0], uv2[1], co2);

      /* Are we spanning the midpoint? */
      if ((co1[axis] <= mid) != (co2[axis] <= mid)) {
        PackedPixelRow row1 = row;
        float t;

        if (mid < co1[axis]) {
          t = 1.0f - (mid - co2[axis]) / (co1[axis] - co2[axis]);

          std::swap(tile1, tile2);
        }
        else {
          t = (mid - co1[axis]) / (co2[axis] - co1[axis]);
        }

        int num_pixels = int(floorf(float(row.num_pixels) * t));

        if (num_pixels) {
          row1.num_pixels = num_pixels;
          tile1->pixel_rows.append(row1);
        }

        if (num_pixels != row.num_pixels) {
          PackedPixelRow row2 = row;

          row2.num_pixels = row.num_pixels - num_pixels;

          row2.start_barycentric_coord = row.start_barycentric_coord +
                                         uv_prim.delta_barycentric_coord_u * float(num_pixels);
          row2.start_image_coordinate = row.start_image_coordinate;
          row2.start_image_coordinate[0] += num_pixels;

          tile2->pixel_rows.append(row2);
        }
      }
      else if (co1[axis] <= mid && co2[axis] <= mid) {
        tile1->pixel_rows.append(row);
      }
      else {
        tile2->pixel_rows.append(row);
      }
    }

    BKE_image_release_ibuf(image, image_buffer, nullptr);
  }

  data.undo_regions.clear();

  if (node->flag & PBVH_Leaf) {
    data.clear_data();
  }
  else {
    node_pixels_free(node);
  }

  BLI_thread_queue_push(tdata->new_nodes, static_cast<void *>(split1));
  BLI_thread_queue_push(tdata->new_nodes, static_cast<void *>(split2));

  BLI_task_pool_push(tdata->pool, split_thread_job, static_cast<void *>(split1), false, nullptr);
  BLI_task_pool_push(tdata->pool, split_thread_job, static_cast<void *>(split2), false, nullptr);
}

static void split_flush_final_nodes(SplitQueueData *tdata)
{
  PBVH *pbvh = tdata->pbvh;
  Vector<SplitNodePair *> splits;

  while (!BLI_thread_queue_is_empty(tdata->new_nodes)) {
    SplitNodePair *newsplit = static_cast<SplitNodePair *>(BLI_thread_queue_pop(tdata->new_nodes));

    splits.append(newsplit);

    if (newsplit->is_old) {
      continue;
    }

    if (!newsplit->parent->children_offset) {
      newsplit->parent->children_offset = pbvh->nodes.size();

      pbvh->nodes.resize(pbvh->nodes.size() + 2);
      newsplit->source_index = newsplit->parent->children_offset;
    }
    else {
      newsplit->source_index = newsplit->parent->children_offset + 1;
    }
  }

  for (SplitNodePair *split : splits) {
    BLI_assert(split->source_index != -1);

    split->node.children_offset = split->children_offset;
    pbvh->nodes[split->source_index] = split->node;
  }

  for (SplitNodePair *split : splits) {
    MEM_delete<SplitNodePair>(split);
  }
}

static void split_thread_job(TaskPool *__restrict pool, void *taskdata)
{

  SplitQueueData *tdata = static_cast<SplitQueueData *>(BLI_task_pool_user_data(pool));
  SplitNodePair *split = static_cast<SplitNodePair *>(taskdata);

  split_pixel_node(tdata->pbvh, split, tdata->image, tdata->image_user, tdata);
}

static void split_pixel_nodes(PBVH *pbvh, Mesh *mesh, Image *image, ImageUser *image_user)
{
  if (G.debug_value == 891) {
    return;
  }

  if (!pbvh->depth_limit) {
    pbvh->depth_limit = 40; /* TODO: move into a constant */
  }

  if (!pbvh->pixel_leaf_limit) {
    pbvh->pixel_leaf_limit = 256 * 256; /* TODO: move into a constant */
  }

  SplitQueueData tdata;
  TaskPool *pool = BLI_task_pool_create_suspended(&tdata, TASK_PRIORITY_HIGH);

  tdata.pool = pool;
  tdata.pbvh = pbvh;
  tdata.mesh = mesh;
  tdata.image = image;
  tdata.image_user = image_user;

  tdata.new_nodes = BLI_thread_queue_init();

  /* Set up initial jobs before initializing threads. */
  for (const int i : pbvh->nodes.index_range()) {
    if (pbvh->nodes[i].flag & PBVH_TexLeaf) {
      SplitNodePair *split = MEM_new<SplitNodePair>("split_pixel_nodes split");

      split->source_index = i;
      split->is_old = true;
      split->node = pbvh->nodes[i];
      split->tdata = &tdata;

      BLI_task_pool_push(pool, split_thread_job, static_cast<void *>(split), false, nullptr);

      BLI_thread_queue_push(tdata.new_nodes, static_cast<void *>(split));
    }
  }

  BLI_task_pool_work_and_wait(pool);
  BLI_task_pool_free(pool);

  split_flush_final_nodes(&tdata);

  BLI_thread_queue_free(tdata.new_nodes);
}

/**
 * During debugging this check could be enabled.
 * It will write to each image pixel that is covered by the PBVH.
 */
constexpr bool USE_WATERTIGHT_CHECK = false;

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
  PBVHData &pbvh_data = data_get(pbvh);
  pbvh_data.clear_data();
  for (const int3 &tri : mesh_data.corner_tris) {
    pbvh_data.geom_primitives.append(int3(mesh_data.corner_verts[tri[0]],
                                          mesh_data.corner_verts[tri[1]],
                                          mesh_data.corner_verts[tri[2]]));
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
           uv_island.uv_primitives)
      {
        for (uv_islands::UVPrimitive &uv_primitive : uv_primitives) {
          lookup[uv_primitive.primitive_i].append_as(Entry(&uv_primitive, uv_island_index));
        }
      }
      uv_island_index++;
    }
  }
};

struct EncodePixelsUserData {
  const uv_islands::MeshData *mesh_data;
  Image *image;
  ImageUser *image_user;
  PBVH *pbvh;
  Vector<PBVHNode *> *nodes;
  const uv_islands::UVIslandsMask *uv_masks;
  /** Lookup to retrieve the UV primitives based on the primitive index. */
  const UVPrimitiveLookup *uv_primitive_lookup;
};

static void do_encode_pixels(EncodePixelsUserData *data, const int n)
{
  const uv_islands::MeshData &mesh_data = *data->mesh_data;
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

    for (const int geom_prim_index : node->prim_indices) {
      for (const UVPrimitiveLookup::Entry &entry :
           data->uv_primitive_lookup->lookup[geom_prim_index])
      {
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
  if ((node->flag & (PBVH_Leaf | PBVH_TexLeaf)) == 0) {
    return false;
  }
  if (node->children_offset != 0) {
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
  for (PBVHNode &node : pbvh->nodes) {
    if (should_pixels_be_updated(&node)) {
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

  for (PBVHNode &node : pbvh->nodes) {
    if (!should_pixels_be_updated(&node)) {
      continue;
    }
    r_nodes_to_update.append(&node);
    node.flag = static_cast<PBVHNodeFlags>(node.flag | PBVH_RebuildPixels);

    if (node.pixels.node_data == nullptr) {
      NodeData *node_data = MEM_new<NodeData>(__func__);
      node.pixels.node_data = node_data;
    }
    else {
      NodeData *node_data = static_cast<NodeData *>(node.pixels.node_data);
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
    for (PBVHNode &node : pbvh->nodes) {
      if ((node.flag & PBVH_Leaf) == 0) {
        continue;
      }
      NodeData *node_data = static_cast<NodeData *>(node.pixels.node_data);
      UDIMTilePixels *tile_node_data = node_data->find_tile_data(image_tile);
      if (tile_node_data == nullptr) {
        continue;
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

static bool update_pixels(PBVH *pbvh, Mesh *mesh, Image *image, ImageUser *image_user)
{
  Vector<PBVHNode *> nodes_to_update;

  if (!find_nodes_to_update(pbvh, nodes_to_update)) {
    return false;
  }

  const StringRef active_uv_name = CustomData_get_active_layer_name(&mesh->corner_data,
                                                                    CD_PROP_FLOAT2);
  if (active_uv_name.is_empty()) {
    return false;
  }

  const AttributeAccessor attributes = mesh->attributes();
  const VArraySpan uv_map = *attributes.lookup<float2>(active_uv_name, AttrDomain::Corner);

  uv_islands::MeshData mesh_data(
      pbvh->corner_tris, mesh->corner_verts(), uv_map, pbvh->vert_positions);
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
  uv_masks.add(mesh_data, islands);
  uv_masks.dilate(image->seam_margin);

  islands.extract_borders();
  islands.extend_borders(mesh_data, uv_masks);
  update_geom_primitives(*pbvh, mesh_data);

  UVPrimitiveLookup uv_primitive_lookup(mesh_data.corner_tris.size(), islands);

  EncodePixelsUserData user_data;
  user_data.mesh_data = &mesh_data;
  user_data.pbvh = pbvh;
  user_data.image = image;
  user_data.image_user = image_user;
  user_data.nodes = &nodes_to_update;
  user_data.uv_primitive_lookup = &uv_primitive_lookup;
  user_data.uv_masks = &uv_masks;

  threading::parallel_for(nodes_to_update.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_encode_pixels(&user_data, i);
    }
  });
  if (USE_WATERTIGHT_CHECK) {
    apply_watertight_check(pbvh, image, image_user);
  }

  /* Add solution for non-manifold parts of the model. */
  copy_update(*pbvh, *image, *image_user, mesh_data);

  /* Rebuild the undo regions. */
  for (PBVHNode *node : nodes_to_update) {
    NodeData *node_data = static_cast<NodeData *>(node->pixels.node_data);
    node_data->rebuild_undo_regions();
  }

  /* Clear the UpdatePixels flag. */
  for (PBVHNode *node : nodes_to_update) {
    node->flag = static_cast<PBVHNodeFlags>(node->flag & ~PBVH_RebuildPixels);
  }

  /* Add PBVH_TexLeaf flag */
  for (PBVHNode &node : pbvh->nodes) {
    if (node.flag & PBVH_Leaf) {
      node.flag = (PBVHNodeFlags)(int(node.flag) | int(PBVH_TexLeaf));
    }
  }

// #define DO_PRINT_STATISTICS
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

NodeData &node_data_get(PBVHNode &node)
{
  BLI_assert(node.pixels.node_data != nullptr);
  NodeData *node_data = static_cast<NodeData *>(node.pixels.node_data);
  return *node_data;
}

PBVHData &data_get(PBVH &pbvh)
{
  BLI_assert(pbvh.pixels.data != nullptr);
  PBVHData *data = static_cast<PBVHData *>(pbvh.pixels.data);
  return *data;
}

void mark_image_dirty(PBVHNode &node, Image &image, ImageUser &image_user)
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

void collect_dirty_tiles(PBVHNode &node, Vector<image::TileNumber> &r_dirty_tiles)
{
  NodeData *node_data = static_cast<NodeData *>(node.pixels.node_data);
  node_data->collect_dirty_tiles(r_dirty_tiles);
}

}  // namespace blender::bke::pbvh::pixels

namespace blender::bke::pbvh {

void build_pixels(PBVH *pbvh, Mesh *mesh, Image *image, ImageUser *image_user)
{
  if (pixels::update_pixels(pbvh, mesh, image, image_user) &&
      pixels::PBVH_PIXELS_SPLIT_NODES_ENABLED)
  {
    pixels::split_pixel_nodes(pbvh, mesh, image, image_user);
  }
}

void node_pixels_free(PBVHNode *node)
{
  pixels::NodeData *node_data = static_cast<pixels::NodeData *>(node->pixels.node_data);

  if (!node_data) {
    return;
  }

  MEM_delete(node_data);
  node->pixels.node_data = nullptr;
}

void pixels_free(PBVH *pbvh)
{
  pixels::PBVHData *pbvh_data = static_cast<pixels::PBVHData *>(pbvh->pixels.data);
  MEM_delete(pbvh_data);
  pbvh->pixels.data = nullptr;
}

}  // namespace blender::bke::pbvh
