/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Paint a color made from hash of node pointer. */
// #define DEBUG_PIXEL_NODES

#include "DNA_brush_types.h"
#include "DNA_image_types.h"
#include "DNA_object_types.h"
#include "DNA_userdef_types.h"

#include "ED_paint.hh"

#include "BLI_bit_vector.hh"
#include "BLI_listbase.h"
#include "BLI_math_color_blend.h"
#include "BLI_math_geom.h"
#ifdef DEBUG_PIXEL_NODES
#  include "BLI_hash.h"
#endif

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"

#include "BKE_brush.hh"
#include "BKE_image_wrappers.hh"
#include "BKE_object_types.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_paint_bvh_pixels.hh"

#include "mesh_brush_common.hh"
#include "sculpt_automask.hh"
#include "sculpt_intern.hh"

namespace blender {

namespace ed::sculpt_paint::paint::image {

using namespace blender::bke::pbvh::pixels;
using namespace blender::bke::image;

ImageData::~ImageData()
{
  if (!image || !image_user) {
    return;
  }

  BLI_assert(buffers.size() <= BLI_listbase_count(&image->tiles));
  for (ImBuf *buffer : buffers.values()) {
    BKE_image_release_ibuf(image, buffer, nullptr);
  }
  buffers.clear();
}
std::unique_ptr<ImageData> ImageData::init_active_image(Object &ob,
                                                        PaintModeSettings &paint_mode_settings)
{
  std::unique_ptr<ImageData> image_data = std::make_unique<ImageData>();
  if (!BKE_paint_canvas_image_get(
          &paint_mode_settings, &ob, &image_data->image, &image_data->image_user))
  {
    return nullptr;
  }

  BLI_assert(image_data->image);
  BLI_assert(image_data->image_user);

  return image_data;
}

static void fetch_image_buffers(ImageData &image_data,
                                bke::pbvh::Node & /*node*/,
                                PixelNode &pixel_node)
{
  for (const UDIMTilePixels &tile : pixel_node.tiles) {
    const ImBuf *buffer = image_data.buffers.lookup_or_add_cb(tile.tile_number, [&]() {
      ImageUser tile_user = *image_data.image_user;
      tile_user.tile = tile.tile_number;

      return BKE_image_acquire_ibuf(image_data.image, &tile_user, nullptr);
    });

    if (buffer) {
      image_data.processors.lookup_or_add_cb(tile.tile_number, [&]() {
        const StringRefNull buffer_colorspace_name =
            buffer->float_data() ? IMB_colormanagement_get_float_colorspace(buffer) :
                                   IMB_colormanagement_get_byte_colorspace(buffer);

        const ColorSpace *buffer_colorspace = IMB_colormanagement_space_get_named(
            buffer_colorspace_name);

        TileColorspaceProcessor processor;
        if (!buffer_colorspace) {
          return processor;
        }
        ColormanageProcessor buffer_to_linear =
            ColormanageProcessor::colorspace_processor_to_scene_linear_new(*buffer_colorspace);
        if (buffer_to_linear.is_noop()) {
          return processor;
        }

        processor.buffer_to_linear_processor = std::move(buffer_to_linear);
        processor.linear_to_buffer_processor =
            ColormanageProcessor::colorspace_processor_from_scene_linear_new(*buffer_colorspace);
        processor.is_noop = false;

        return processor;
      });
    }
  }
}

static float3 calc_pixel_position(const Span<float3> vert_positions,
                                  const Span<int3> vert_tris,
                                  const int tri_index,
                                  const float2 &barycentric_weight)
{
  const int3 &verts = vert_tris[tri_index];
  const float3 weights(barycentric_weight.x,
                       barycentric_weight.y,
                       1.0f - barycentric_weight.x - barycentric_weight.y);
  float3 result;
  interp_v3_v3v3v3(result,
                   vert_positions[verts[0]],
                   vert_positions[verts[1]],
                   vert_positions[verts[2]],
                   weights);
  return result;
}

static void calc_pixel_row_positions(const Span<float3> vert_positions,
                                     const Span<int3> vert_tris,
                                     const Span<int> tri_indices,
                                     const Span<float2> delta_barycentric_coords,
                                     const PackedPixelRow &pixel_row,
                                     const MutableSpan<float3> positions)
{
  const float3 start = calc_pixel_position(vert_positions,
                                           vert_tris,
                                           tri_indices[pixel_row.uv_primitive_index],
                                           pixel_row.start_barycentric_coord);
  const float3 next = calc_pixel_position(
      vert_positions,
      vert_tris,
      tri_indices[pixel_row.uv_primitive_index],
      pixel_row.start_barycentric_coord + delta_barycentric_coords[pixel_row.uv_primitive_index]);
  const float3 delta = next - start;
  for (const int i : IndexRange(pixel_row.num_pixels)) {
    positions[i] = start + delta * i;
  }
}

static BitVector<> init_uv_primitives_brush_test(SculptSession &ss,
                                                 const Span<int3> vert_tris,
                                                 const Span<int> tri_indices,
                                                 const Span<float3> positions)
{
  const float3 location = ss.cache ? ss.cache->location_symm : ss.cursor_location;
  const float radius = ss.cache ? ss.cache->radius : ss.cursor_radius;
  const Bounds<float3> brush_bounds(location - radius, location + radius);

  BitVector<> brush_test(tri_indices.size());
  for (const int i : tri_indices.index_range()) {
    const int3 verts = vert_tris[tri_indices[i]];

    Bounds<float3> tri_bounds(positions[verts[0]]);
    math::min_max(positions[verts[1]], tri_bounds.min, tri_bounds.max);
    math::min_max(positions[verts[2]], tri_bounds.min, tri_bounds.max);

    brush_test[i].set(
        isect_aabb_aabb_v3(brush_bounds.min, brush_bounds.max, tri_bounds.min, tri_bounds.max));
  }
  return brush_test;
}

/** Apply the per-pixel factor to the initial brush color. */
static void calc_brush_colors(MutableSpan<float4> buffer_colors,
                              Span<float> factors,
                              const float4 &brush_color)
{
  BLI_assert(buffer_colors.size() == factors.size());

  for (const int i : buffer_colors.index_range()) {
    buffer_colors[i] = brush_color * factors[i];
  }
}

static MutableSpan<float4> read_image_pixels(MutableSpan<float4> image_pixels,
                                             const TileColorspaceProcessor &processors,
                                             const PackedPixelRow &pixel_row,
                                             const int width)
{
  const int start_offset = int(pixel_row.start_image_coordinate.y) * width +
                           int(pixel_row.start_image_coordinate.x);
  MutableSpan<float4> scene_linear_pixels = image_pixels.slice(start_offset, pixel_row.num_pixels);

  if (processors.is_noop) {
    return scene_linear_pixels;
  }

  processors.buffer_to_linear_processor.apply(
      reinterpret_cast<float *>(scene_linear_pixels.data()), pixel_row.num_pixels, 1, 4, false);

  return scene_linear_pixels;
}

static MutableSpan<float4> read_image_pixels(Span<uchar4> image_pixels,
                                             const TileColorspaceProcessor &processors,
                                             const PackedPixelRow &pixel_row,
                                             const int width,
                                             Vector<float4> &storage)
{
  storage.resize(pixel_row.num_pixels);
  const int start_offset = int(pixel_row.start_image_coordinate.y) * width +
                           int(pixel_row.start_image_coordinate.x);

  for (int i = 0; i < pixel_row.num_pixels; i++) {
    rgba_uchar_to_float(storage[i], image_pixels[start_offset + i]);
  }

  if (processors.is_noop) {
    return storage;
  }

  processors.buffer_to_linear_processor.apply(
      reinterpret_cast<float *>(storage.data()), pixel_row.num_pixels, 1, 4, false);

  return storage;
}

static void write_image_pixels(MutableSpan<float4> scene_linear_pixels,
                               MutableSpan<uchar4> image_pixels,
                               const TileColorspaceProcessor &processors,
                               const PackedPixelRow &pixel_row,
                               const int width)
{
  if (!processors.is_noop) {
    processors.linear_to_buffer_processor.apply(
        reinterpret_cast<float *>(scene_linear_pixels.data()), pixel_row.num_pixels, 1, 4, false);
  }

  const int start_offset = int(pixel_row.start_image_coordinate.y) * width +
                           int(pixel_row.start_image_coordinate.x);

  for (int i = 0; i < pixel_row.num_pixels; i++) {
    rgba_float_to_uchar(image_pixels[start_offset + i], scene_linear_pixels[i]);
  }
}

static void write_image_pixels(MutableSpan<float4> scene_linear_pixels,
                               MutableSpan<float4> image_pixels,
                               const TileColorspaceProcessor &processors,
                               const PackedPixelRow &pixel_row,
                               const int width)
{
  if (!processors.is_noop) {
    processors.linear_to_buffer_processor.apply(
        reinterpret_cast<float *>(scene_linear_pixels.data()), pixel_row.num_pixels, 1, 4, false);
  }

  const int start_offset = int(pixel_row.start_image_coordinate.y) * width +
                           int(pixel_row.start_image_coordinate.x);

  std::copy_n(
      scene_linear_pixels.begin(), pixel_row.num_pixels, image_pixels.begin() + start_offset);
}

static void blend_colors(MutableSpan<float4> paint_pixels,
                         Span<float4> scene_linear_pixels,
                         const Brush &brush)
{
  BLI_assert(paint_pixels.size() == scene_linear_pixels.size());

  /* Mix the initial image color with the paint color. */
  for (const int i : paint_pixels.index_range()) {
    blend_color_mix_float(paint_pixels[i], scene_linear_pixels[i], paint_pixels[i]);
    paint_pixels[i] *= brush.alpha;
  }

  /* Apply the blended color to the original image with the brush alpha. */
  IMB_blend_color_float(
      paint_pixels, scene_linear_pixels, paint_pixels, IMB_BlendMode(brush.blend));
}

#ifdef DEBUG_PIXEL_NODES
static void apply_debug_color(MutableSpan<float4> paint_pixels, const PackedPixelRow &pixel_row)
{
  if ((pixel_row.start_image_coordinate.y >> 3) & 1) {
    for (const int i : paint_pixels.index_range()) {
      paint_pixels[i][0] *= 0.5f;
      paint_pixels[i][1] *= 0.5f;
      paint_pixels[i][2] *= 0.5f;
    }
  }
}
#endif

static void do_paint_pixels(const Depsgraph &depsgraph,
                            Object &object,
                            const Paint &paint,
                            const Brush &brush,
                            ImageData &image_data,
                            bke::pbvh::Node & /*node*/,
                            PixelNode &pixel_node)
{
  SculptSession &ss = *object.runtime->sculpt_session;
  const StrokeCache &cache = *ss.cache;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  PixelData &pbvh_data = bke::pbvh::pixels::data_get(pbvh);
  const Span<float3> positions = bke::pbvh::vert_positions_eval(depsgraph, object);

  BitVector<> brush_test = init_uv_primitives_brush_test(
      ss, pbvh_data.vert_tris, pixel_node.uv_primitives.tri_indices, positions);

  float4 brush_color = float4(ss.cache->toggle_settings.invert ?
                                  BKE_brush_secondary_color_get(&paint, &brush) :
                                  BKE_brush_color_get(&paint, &brush),
                              1.0f);

#ifdef DEBUG_PIXEL_NODES
  float4 debug_color;
  uint hash = BLI_hash_int(POINTER_AS_UINT(&node));

  debug_color[0] = float(hash & 255) / 255.0f;
  debug_color[1] = float((hash >> 8) & 255) / 255.0f;
  debug_color[2] = float((hash >> 16) & 255) / 255.0f;
  debug_color[3] = 1.0f;
#endif

  Vector<float4> byte_to_float_pixels;
  Vector<float4> paint_pixels;
  Vector<float3> pixel_positions;
  Vector<float> factors;
  Vector<float> distances;

  bool pixels_updated = false;
  for (UDIMTilePixels &tile_data : pixel_node.tiles) {
    ImBuf *image_buffer = image_data.buffers.lookup_default(tile_data.tile_number, nullptr);
    if (image_buffer == nullptr) {
      continue;
    }

    MutableSpan<float4> float_buffer;
    MutableSpan<uchar4> byte_buffer;

    if (image_buffer->float_data()) {
      BLI_assert(ELEM(image_buffer->channels, 0, 4));
      float_buffer = MutableSpan(reinterpret_cast<float4 *>(image_buffer->float_data_for_write()),
                                 image_buffer->x * image_buffer->y);
    }
    else {
      byte_buffer = MutableSpan(reinterpret_cast<uchar4 *>(image_buffer->byte_data_for_write()),
                                image_buffer->x * image_buffer->y);
    }

    const TileColorspaceProcessor *processors = image_data.processors.lookup_ptr(
        tile_data.tile_number);

    for (const PackedPixelRow &pixel_row : tile_data.pixel_rows) {
      if (!brush_test[pixel_row.uv_primitive_index]) {
        continue;
      }

      pixel_positions.resize(pixel_row.num_pixels);
      calc_pixel_row_positions(positions,
                               pbvh_data.vert_tris,
                               pixel_node.uv_primitives.tri_indices,
                               pixel_node.uv_primitives.delta_barycentric_coords,
                               pixel_row,
                               pixel_positions);

      factors.resize(pixel_positions.size());
      factors.fill(1.0f);

      distances.resize(pixel_positions.size());
      calc_brush_distances(
          ss, pixel_positions, eBrushFalloffShape(brush.falloff_shape), distances);
      filter_distances_with_radius(cache.radius, distances, factors);
      apply_hardness_to_distances(cache, distances);
      calc_brush_strength_factors(cache, brush, distances, factors);
      calc_brush_texture_factors(ss, brush, pixel_positions, factors);
      scale_factors(factors, cache.bstrength);

      const bool pixels_painted = std::ranges::any_of(
          factors, [](const float factor) { return factor != 0.0f; });

      if (!pixels_painted) {
        continue;
      }

      paint_pixels.resize(pixel_positions.size());
      calc_brush_colors(paint_pixels, factors, brush_color);

      MutableSpan<float4> scene_linear_pixels;
      if (!float_buffer.is_empty()) {
        scene_linear_pixels = read_image_pixels(
            float_buffer, *processors, pixel_row, image_buffer->x);
      }
      else {
        scene_linear_pixels = read_image_pixels(
            byte_buffer, *processors, pixel_row, image_buffer->x, byte_to_float_pixels);
      }

#ifdef DEBUG_PIXEL_NODES
      apply_debug_color(scene_linear_pixels, pixel_row);
#endif

      blend_colors(paint_pixels, scene_linear_pixels, brush);

      if (!float_buffer.is_empty()) {
        write_image_pixels(paint_pixels, float_buffer, *processors, pixel_row, image_buffer->x);
      }
      else {
        write_image_pixels(paint_pixels, byte_buffer, *processors, pixel_row, image_buffer->x);
      }

      tile_data.mark_dirty(pixel_row);
    }

    if (tile_data.flags.dirty) {
      BKE_image_mark_dirty(image_data.image, image_buffer);
    }
    pixels_updated |= tile_data.flags.dirty;
  }

  pixel_node.flags.dirty |= pixels_updated;
}

static void undo_region_tiles(
    ImBuf *ibuf, int x, int y, int w, int h, int *tx, int *ty, int *tw, int *th)
{
  int srcx = 0, srcy = 0;
  IMB_rectclip(ibuf, nullptr, &x, &y, &srcx, &srcy, &w, &h);
  *tw = ((x + w - 1) >> ED_IMAGE_UNDO_TILE_BITS);
  *th = ((y + h - 1) >> ED_IMAGE_UNDO_TILE_BITS);
  *tx = (x >> ED_IMAGE_UNDO_TILE_BITS);
  *ty = (y >> ED_IMAGE_UNDO_TILE_BITS);
}

static void push_undo(const PixelNode &node_data,
                      Image &image,
                      ImageUser &image_user,
                      const TileNumber tile_number,
                      ImBuf &image_buffer)
{
  for (const UDIMTileUndo &tile_undo : node_data.undo_regions) {
    if (tile_undo.tile_number != tile_number) {
      continue;
    }
    int tilex, tiley, tilew, tileh;
    PaintTileMap *undo_tiles = ED_image_paint_tile_map_get();
    undo_region_tiles(&image_buffer,
                      tile_undo.region.xmin,
                      tile_undo.region.ymin,
                      BLI_rcti_size_x(&tile_undo.region),
                      BLI_rcti_size_y(&tile_undo.region),
                      &tilex,
                      &tiley,
                      &tilew,
                      &tileh);
    for (int ty = tiley; ty <= tileh; ty++) {
      for (int tx = tilex; tx <= tilew; tx++) {
        ED_image_paint_tile_push(
            undo_tiles, &image, &image_buffer, &image_user, tx, ty, nullptr, nullptr, true, true);
      }
    }
  }
}

static void do_push_undo_tile(ImageData &image_data,
                              bke::pbvh::Node & /*node*/,
                              PixelNode &pixel_node)
{
  for (const UDIMTilePixels &tile : pixel_node.tiles) {
    ImBuf *buffer = image_data.buffers.lookup_default(tile.tile_number, nullptr);
    if (buffer == nullptr) {
      continue;
    }

    push_undo(pixel_node, *image_data.image, *image_data.image_user, tile.tile_number, *buffer);
  }
}

/* -------------------------------------------------------------------- */

/** \name Fix non-manifold edge bleeding.
 * \{ */

static Vector<image::TileNumber> collect_dirty_tiles(MutableSpan<PixelNode> nodes,
                                                     const IndexMask &node_mask)
{
  Vector<image::TileNumber> dirty_tiles;
  node_mask.foreach_index(
      [&](const int i) { bke::pbvh::pixels::collect_dirty_tiles(nodes[i], dirty_tiles); });
  return dirty_tiles;
}
static void fix_non_manifold_seam_bleeding(bke::pbvh::Tree &pbvh,
                                           Map<paint::image::TileNumber, ImBuf *> &buffers,
                                           Span<TileNumber> tile_numbers_to_fix)
{
  for (image::TileNumber tile_number : tile_numbers_to_fix) {
    bke::pbvh::pixels::copy_pixels(pbvh, buffers, tile_number);
  }
}

static void fix_non_manifold_seam_bleeding(Object &ob,
                                           ImageData &image_data,
                                           MutableSpan<bke::pbvh::MeshNode> /*nodes*/,
                                           MutableSpan<PixelNode> pixel_nodes,
                                           const IndexMask &node_mask)
{
  Vector<image::TileNumber> dirty_tiles = collect_dirty_tiles(pixel_nodes, node_mask);
  fix_non_manifold_seam_bleeding(*bke::object::pbvh_get(ob), image_data.buffers, dirty_tiles);
}

/** \} */

}  // namespace ed::sculpt_paint::paint::image

using namespace blender::ed::sculpt_paint::paint::image;

bool SCULPT_use_image_paint_brush(PaintModeSettings &settings, Object &ob)
{
  if (!USER_EXPERIMENTAL_TEST(&U, use_sculpt_texture_paint)) {
    return false;
  }
  if (ob.type != OB_MESH) {
    return false;
  }
  Image *image;
  ImageUser *image_user;
  return BKE_paint_canvas_image_get(&settings, &ob, &image, &image_user);
}

void SCULPT_do_paint_brush_image(const Depsgraph &depsgraph,
                                 const Sculpt &sd,
                                 Object &ob,
                                 const IndexMask &node_mask)
{
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);
  ed::sculpt_paint::StrokeCache &cache = *ob.runtime->sculpt_session->cache;

  if (!cache.image_data) {
    return;
  }

  ImageData &image_data = *cache.image_data;

  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
  PixelData &pixel_data = *pbvh.pixels_;
  MutableSpan<PixelNode> pixel_nodes = pixel_data.nodes;

  node_mask.foreach_index(
      [&](const int i) { fetch_image_buffers(image_data, nodes[i], pixel_nodes[i]); });
  node_mask.foreach_index(
      [&](const int i) { do_push_undo_tile(image_data, nodes[i], pixel_nodes[i]); },
      exec_mode::grain_size(1));
  node_mask.foreach_index(
      [&](const int i) {
        do_paint_pixels(depsgraph, ob, sd.paint, *brush, image_data, nodes[i], pixel_nodes[i]);
      },
      exec_mode::grain_size(1));

  fix_non_manifold_seam_bleeding(ob, image_data, nodes, pixel_nodes, node_mask);

  node_mask.foreach_index([&](const int i) {
    bke::pbvh::pixels::mark_image_dirty(
        nodes[i], pixel_nodes[i], *image_data.image, image_data.buffers);
  });
}

}  // namespace blender
