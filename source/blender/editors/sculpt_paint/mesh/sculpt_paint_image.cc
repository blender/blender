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
#include "BLI_bounds.hh"
#include "BLI_colorspace.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_listbase.hh"
#include "BLI_math_color_blend.hh"
#include "BLI_math_color_c.hh"
#include "BLI_math_geom_c.hh"
#ifdef DEBUG_PIXEL_NODES
#  include "BLI_hash_c.hh"
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

#include <atomic>

namespace blender {

namespace ed::sculpt_paint::paint::image {

using namespace blender::bke::pbvh::pixels;
using namespace blender::bke::image;

ImageData::~ImageData()
{
  if (!image || !image_user) {
    return;
  }

  BLI_assert(buffers.size() <= image->tiles.count());
  for (ImBuf *buffer : buffers.values()) {
    if (buffer) {
      buffer->gpu.flag &= ~IMB_GPU_DISABLE_MIPMAP_UPDATE;
    }
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
  PRF_scope(ProfileCategory::Editor);
  for (const UDIMTilePixels &tile : pixel_node.tiles) {
    const ImBuf *buffer = image_data.buffers.lookup_or_add_cb(tile.tile_number, [&]() {
      ImageUser tile_user = *image_data.image_user;
      tile_user.tile = tile.tile_number;

      ImBuf *ibuf = BKE_image_acquire_ibuf(image_data.image, &tile_user, nullptr);
      if (ibuf) {
        ibuf->gpu.flag |= IMB_GPU_DISABLE_MIPMAP_UPDATE;
      }
      return ibuf;
    });

    if (buffer) {
      image_data.undo_tile_pushed.lookup_or_add_cb(tile.tile_number, [&]() {
        const int64_t tiles_x = (buffer->x + ED_IMAGE_UNDO_TILE_SIZE - 1) >>
                                ED_IMAGE_UNDO_TILE_BITS;
        const int64_t tiles_y = (buffer->y + ED_IMAGE_UNDO_TILE_SIZE - 1) >>
                                ED_IMAGE_UNDO_TILE_BITS;
        return Array<uint32_t>(tiles_x * tiles_y, 0);
      });
      image_data.seam_tile_modified.lookup_or_add_cb(tile.tile_number, [&]() {
        const int tiles_x = (buffer->x + SEAM_TILE_SIZE - 1) >> SEAM_TILE_BITS;
        const int tiles_y = (buffer->y + SEAM_TILE_SIZE - 1) >> SEAM_TILE_BITS;
        return Array<uint8_t>(int64_t(tiles_x) * tiles_y, 0);
      });
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

        /* Fast path for sRGB byte, to avoid overhead of calling into OpenColorIO. */
        if (!buffer->float_data() && buffer->byte_data() &&
            IMB_colormanagement_space_is_srgb(buffer_colorspace))
        {
          processor.is_srgb_byte = true;
          processor.is_noop = false;
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

static void calc_pixel_row_positions(const PackedPixelRowPosition &row_data,
                                     const MutableSpan<float3> positions,
                                     IndexRange range)
{
  PRF_scope(ProfileCategory::Editor);
  BLI_assert(range.size() == positions.size());

  const float3 delta = row_data.delta;
  const float3 start = row_data.start + delta * range.start();
  for (const int i : positions.index_range()) {
    positions[i] = start + delta * i;
  }
}

static BitVector<> init_uv_primitives_brush_test(SculptSession &ss,
                                                 const Span<int3> vert_tris,
                                                 const Span<int> tri_indices,
                                                 const Span<float3> positions)
{
  PRF_scope(ProfileCategory::Editor);
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

/** Cached settings for faster paint blending. */
struct PaintBlendSettings {
  PaintBlendSettings(const Paint &paint, const Brush &brush, const bool invert)
  {
    brush_color = float4(invert ? BKE_brush_secondary_color_get(&paint, &brush) :
                                  BKE_brush_color_get(&paint, &brush),
                         1.0f);
    brush_alpha = brush.alpha;
    blend_mode = IMB_BlendMode(brush.blend);
  }

  float4 brush_color;
  float brush_alpha;
  IMB_BlendMode blend_mode;
};

/** Blend one pixel with the brush. */
BLI_INLINE float4 paint_blend_pixel(const float4 &brush_color,
                                    const float brush_alpha,
                                    const bool is_mix,
                                    const IMB_BlendMode blend_mode,
                                    const float factor,
                                    const float4 color)
{
  float4 result;
  blend_color_mix_float(result, color, brush_color * factor);
  result *= brush_alpha;
  /* TODO: try making IMB_blend_color_float inline instead. */
  if (is_mix) {
    blend_color_mix_float(result, color, result);
  }
  else {
    IMB_blend_color_float(result, color, result, blend_mode);
  }
  return result;
}

#ifdef DEBUG_PIXEL_NODES
static float4 paint_debug_color(float4 scene, const int img_y)
{
  if ((img_y >> 3) & 1) {
    scene.x *= 0.5f;
    scene.y *= 0.5f;
    scene.z *= 0.5f;
  }
  return scene;
}
#endif

/**
 * Slower paint pixels blending with OpenColorIO conversion.
 */
template<typename PixelT>
BLI_NOINLINE static void paint_blend_pixels_color_managed(
    const PaintBlendSettings &settings,
    const Span<float> factors,
    const int start,
    const int size,
    const MutableSpan<PixelT> image_pixels,
    const int img_x,
    [[maybe_unused]] const int img_y,
    const TileColorspaceProcessor &processors,
    Vector<float4> &paint_pixels)
{
  constexpr bool is_float = std::is_same_v<typename PixelT::base_type, float>;
  PRF_scope(ProfileCategory::Editor);

  MutableSpan<float4> scene_linear_pixels;

  /* Convert from image colorspace to scene linear. */
  if constexpr (is_float) {
    scene_linear_pixels = image_pixels.slice(img_x, size);
  }
  else {
    paint_pixels.resize(size);
    for (int i = 0; i < size; i++) {
      rgba_uchar_to_float(paint_pixels[i], image_pixels[img_x + i]);
    }
    scene_linear_pixels = paint_pixels;
  }

  if (!processors.is_noop) {
    processors.buffer_to_linear_processor.apply(
        reinterpret_cast<float *>(scene_linear_pixels.data()), size, 1, 4, false);
  }

  /* Blend. */
  const bool is_mix = settings.blend_mode == IMB_BLEND_MIX;
  for (int i = 0; i < size; i++) {
    float4 scene = scene_linear_pixels[i];
#ifdef DEBUG_PIXEL_NODES
    scene = paint_debug_color(scene, img_y);
#endif
    scene_linear_pixels[i] = paint_blend_pixel(settings.brush_color,
                                               settings.brush_alpha,
                                               is_mix,
                                               settings.blend_mode,
                                               factors[start + i],
                                               scene);
  }

  /* Convert from scene linear to image colorspace. */
  if (!processors.is_noop) {
    processors.linear_to_buffer_processor.apply(
        reinterpret_cast<float *>(scene_linear_pixels.data()), size, 1, 4, false);
  }

  /* Write out byte, float was already modified in place. */
  if constexpr (!is_float) {
    for (int i = 0; i < size; i++) {
      rgba_float_to_uchar(image_pixels[img_x + i], scene_linear_pixels[i]);
    }
  }
}

/**
 * Perform paint pixel blending with computed factors.
 * Templated and specialized for common color spaces since this is a hotspot.
 */
template<typename PixelT>
static void paint_blend_pixels(const PaintBlendSettings &settings,
                               const Span<float> factors,
                               const int start,
                               const int size,
                               const MutableSpan<PixelT> image_pixels,
                               const int img_x,
                               const int img_y,
                               const TileColorspaceProcessor &processors,
                               Vector<float4> &paint_pixels)
{
  constexpr bool is_float = std::is_same_v<typename PixelT::base_type, float>;
  const bool fast_colorspace = is_float ? processors.is_noop : processors.is_srgb_byte;
  if (!fast_colorspace) {
    /* Slow path with OpenColorIO. */
    paint_blend_pixels_color_managed<PixelT>(
        settings, factors, start, size, image_pixels, img_x, img_y, processors, paint_pixels);
    return;
  }

  PRF_scope(ProfileCategory::Editor);

  /* Keep variables in registers during the loop. */
  const float4 brush_color = settings.brush_color;
  const float brush_alpha = settings.brush_alpha;
  const bool is_mix = settings.blend_mode == IMB_BLEND_MIX;
  const IMB_BlendMode blend_mode = settings.blend_mode;

  if constexpr (is_float) {
    /* Scene linear float. */
    for (int i = 0; i < size; i++) {
      float4 scene = image_pixels[img_x + i];
#ifdef DEBUG_PIXEL_NODES
      scene = paint_debug_color(scene, img_y);
#endif
      image_pixels[img_x + i] = paint_blend_pixel(
          brush_color, brush_alpha, is_mix, blend_mode, factors[start + i], scene);
    }
  }
  else {
    /* sRGB byte. */
    paint_pixels.resize(size);
    for (int i = 0; i < size; i++) {
      float4 scene;
      srgb_to_linearrgb_uchar4(scene, image_pixels[img_x + i]);
      IMB_colormanagement_rec709_to_scene_linear(scene, scene);
#ifdef DEBUG_PIXEL_NODES
      scene = paint_debug_color(scene, img_y);
#endif
      paint_pixels[i] = paint_blend_pixel(
          brush_color, brush_alpha, is_mix, blend_mode, factors[start + i], scene);
    }

    /* SIMD optimized write. */
    const float (*matrix)[3] = blender::colorspace::scene_linear_is_rec709 ?
                                   nullptr :
                                   reinterpret_cast<const float (*)[3]>(
                                       blender::colorspace::scene_linear_to_rec709.ptr());
    linearrgb_to_srgb_uchar4_n(reinterpret_cast<uchar(*)[4]>(&image_pixels[img_x]),
                               reinterpret_cast<const float (*)[4]>(paint_pixels.data()),
                               size,
                               matrix);
  }
}

struct PaintLocalData {
  Vector<float3> pixel_positions;
  Vector<float> distances;
  Vector<float> factors;

  Vector<float4> paint_blend_pixels;
};

static Bounds<int2> merge_bounds(const Bounds<int2> &a, const Bounds<int2> &b)
{
  return bounds::merge(a, b);
}

static Bounds<int2> negative_bounds()
{
  return {int2(std::numeric_limits<int>::max()), int2(std::numeric_limits<int>::lowest())};
}

/**
 * Save the pre-stroke pixels of an undo tile for a pixel row, just-in-time
 * before painting. Uses a mask to quickly skip already pushed tiles.
 */
static void push_undo_tiles(ImageData &image_data,
                            const image::TileNumber tile_number,
                            ImBuf &image_buffer,
                            MutableSpan<uint32_t> tile_pushed,
                            const int img_x_start,
                            const int img_x_end,
                            const int img_y)
{
  const int tile_y = img_y >> ED_IMAGE_UNDO_TILE_BITS;
  const int tile_x_start = img_x_start >> ED_IMAGE_UNDO_TILE_BITS;
  const int tile_x_end = img_x_end >> ED_IMAGE_UNDO_TILE_BITS;
  const int undo_tiles_x = (image_buffer.x + ED_IMAGE_UNDO_TILE_SIZE - 1) >>
                           ED_IMAGE_UNDO_TILE_BITS;

  for (int tile_x = tile_x_start; tile_x <= tile_x_end; tile_x++) {
    const int tile_index = tile_y * undo_tiles_x + tile_x;
    std::atomic_ref<uint32_t> pushed(tile_pushed[tile_index]);
    /* This atomic load is free on x86_64, and cheap on arm64. */
    if (pushed.load(std::memory_order_acquire)) {
      continue;
    }
    ImageUser tile_user = *image_data.image_user;
    tile_user.tile = tile_number;
    PaintTileMap *undo_tiles = ED_image_paint_tile_map_get();
    ED_image_paint_tile_push(
        undo_tiles, image_data.image, &image_buffer, &tile_user, tile_x, tile_y, nullptr, nullptr);
    pushed.store(1, std::memory_order_release);
  }
}

/**
 * Mark the seam tiles covering the given pixel row as modified. Dilated by one tile so a
 * seam whose second source sits one pixel across a tile boundary is still handled.
 */
static void mark_seam_tiles_modified(MutableSpan<uint8_t> mask,
                                     const int tiles_x,
                                     const int tiles_y,
                                     const int x_start,
                                     const int x_end,
                                     const int y)
{
  const int tile_y = y >> SEAM_TILE_BITS;
  const int tile_y_min = (tile_y > 0) ? tile_y - 1 : 0;
  const int tile_y_max = (tile_y + 1 < tiles_y) ? tile_y + 1 : tiles_y - 1;

  const int tile_x_start = x_start >> SEAM_TILE_BITS;
  const int tile_x_end = x_end >> SEAM_TILE_BITS;
  const int tile_x_min = (tile_x_start > 0) ? tile_x_start - 1 : 0;
  const int tile_x_max = (tile_x_end + 1 < tiles_x) ? tile_x_end + 1 : tiles_x - 1;

  for (int dty = tile_y_min; dty <= tile_y_max; dty++) {
    for (int dtx = tile_x_min; dtx <= tile_x_max; dtx++) {
      mask[int64_t(dty) * tiles_x + dtx] = 1;
    }
  }
}

static void do_paint_pixels(const Depsgraph &depsgraph,
                            Object &object,
                            const Paint &paint,
                            const Brush &brush,
                            ImageData &image_data,
                            bke::pbvh::Node & /*node*/,
                            PixelNode &pixel_node)
{
  PRF_scope(ProfileCategory::Editor);
  SculptSession &ss = *object.runtime->sculpt_session;
  const StrokeCache &cache = *ss.cache;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  PixelData &pbvh_data = bke::pbvh::pixels::data_get(pbvh);
  const Span<float3> positions = bke::pbvh::vert_positions_eval(depsgraph, object);

  BitVector<> brush_test = init_uv_primitives_brush_test(
      ss, pbvh_data.vert_tris, pixel_node.uv_primitives.tri_indices, positions);

  const PaintBlendSettings blend_settings(paint, brush, ss.cache->toggle_settings.invert);

#ifdef DEBUG_PIXEL_NODES
  float4 debug_color;
  uint hash = BLI_hash_int(POINTER_AS_UINT(&node));

  debug_color[0] = float(hash & 255) / 255.0f;
  debug_color[1] = float((hash >> 8) & 255) / 255.0f;
  debug_color[2] = float((hash >> 16) & 255) / 255.0f;
  debug_color[3] = 1.0f;
#endif

  bool pixels_updated = false;

  const float3 location = ss.cache ? ss.cache->location_symm : ss.cursor_location;
  const float radius = ss.cache ? ss.cache->radius : ss.cursor_radius;
  const Bounds<float3> brush_bounds(location - radius, location + radius);

  for (UDIMTilePixels &tile_data : pixel_node.tiles) {
    ImBuf *image_buffer = image_data.buffers.lookup_default(tile_data.tile_number, nullptr);
    if (image_buffer == nullptr) {
      continue;
    }
    IndexMaskMemory memory;

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

    const MutableSpan<uint32_t> undo_tile_pushed = image_data.undo_tile_pushed.lookup(
        tile_data.tile_number);

    const IndexMask valid_uv_rows = IndexMask::from_predicate(
        tile_data.pixel_rows.index_range(), memory, [&](const int i) {
          return brush_test[tile_data.pixel_rows[i].uv_primitive_index];
        });

    const IndexMask valid_rows = IndexMask::from_predicate(
        valid_uv_rows, memory, [&](const int i) {
          const float3 end = tile_data.pixel_row_positions[i].start +
                             tile_data.pixel_row_positions[i].delta *
                                 tile_data.pixel_rows[i].num_pixels;
          return brush_bounds.intersects_segment(tile_data.pixel_row_positions[i].start, end);
        });

    Array<bool> row_changed(valid_rows.min_array_size(), false);
    threading::EnumerableThreadSpecific<PaintLocalData> all_factor_tls;
    valid_rows.foreach_index(
        [&](const int row_i) {
          const PackedPixelRow pixel_row = tile_data.pixel_rows[row_i];
          const PackedPixelRowPosition &pixel_row_position = tile_data.pixel_row_positions[row_i];
          const int row_size = pixel_row.num_pixels;
          threading::parallel_for(IndexRange(row_size), 512, [&](const IndexRange range) {
            PaintLocalData &tls = all_factor_tls.local();
            tls.factors.resize(range.size());
            tls.factors.fill(1.0f);
            tls.pixel_positions.resize(range.size());
            calc_pixel_row_positions(pixel_row_position, tls.pixel_positions, range);

            MutableSpan<float> factors = tls.factors;

            tls.distances.resize(range.size());
            calc_brush_distances(
                ss, tls.pixel_positions, eBrushFalloffShape(brush.falloff_shape), tls.distances);
            filter_distances_with_radius(cache.radius, tls.distances, factors);
            apply_hardness_to_distances(cache, tls.distances);
            calc_brush_strength_factors(cache, brush, tls.distances, factors);
            calc_brush_texture_factors(ss, brush, tls.pixel_positions, factors);
            scale_factors(factors, cache.bstrength);

            if (std::ranges::all_of(factors, [](const float factor) { return factor == 0.0f; })) {
              return;
            }
            row_changed[row_i] = true;

            const int undo_img_x_start = int(pixel_row.start_image_coordinate.x) +
                                         int(range.start());
            const int undo_img_x_end = undo_img_x_start + int(range.size()) - 1;
            const int undo_img_y = int(pixel_row.start_image_coordinate.y);
            push_undo_tiles(image_data,
                            tile_data.tile_number,
                            *image_buffer,
                            undo_tile_pushed,
                            undo_img_x_start,
                            undo_img_x_end,
                            undo_img_y);

            const int img_y = int(pixel_row.start_image_coordinate.y);
            const int img_x = img_y * image_buffer->x + int(pixel_row.start_image_coordinate.x) +
                              int(range.start());

            /* Blend pixels with computed factors. */
            if (!float_buffer.is_empty()) {
              paint_blend_pixels<float4>(blend_settings,
                                         factors,
                                         0,
                                         int(range.size()),
                                         float_buffer,
                                         img_x,
                                         img_y,
                                         *processors,
                                         tls.paint_blend_pixels);
            }
            else {
              paint_blend_pixels<uchar4>(blend_settings,
                                         factors,
                                         0,
                                         int(range.size()),
                                         byte_buffer,
                                         img_x,
                                         img_y,
                                         *processors,
                                         tls.paint_blend_pixels);
            }
          });
        },
        exec_mode::grain_size(2));

    const IndexMask changed_rows = IndexMask::from_bools(valid_rows, row_changed, memory);

    const MutableSpan<uint8_t> seam_tile_modified = image_data.seam_tile_modified.lookup(
        tile_data.tile_number);
    const int seam_tiles_x = (image_buffer->x + SEAM_TILE_SIZE - 1) >> SEAM_TILE_BITS;
    const int seam_tiles_y = (image_buffer->y + SEAM_TILE_SIZE - 1) >> SEAM_TILE_BITS;

    const Bounds<int2> dirty_bounds = threading::parallel_reduce(
        changed_rows.index_range(),
        512,
        negative_bounds(),
        [&](const IndexRange range, const Bounds<int2> &init) {
          Bounds<int2> current = init;
          changed_rows.slice(range).foreach_index([&](const int row_i) {
            const PackedPixelRow pixel_row = tile_data.pixel_rows[row_i];

            mark_seam_tiles_modified(seam_tile_modified,
                                     seam_tiles_x,
                                     seam_tiles_y,
                                     int(pixel_row.start_image_coordinate.x),
                                     int(pixel_row.start_image_coordinate.x) +
                                         int(pixel_row.num_pixels) - 1,
                                     int(pixel_row.start_image_coordinate.y));

            const int2 start(pixel_row.start_image_coordinate.x,
                             pixel_row.start_image_coordinate.y);
            const int2 end = start + int2(pixel_row.num_pixels + 1, 0);

            current = bounds::merge(current, Bounds<int2>(start, end));
          });
          return current;
        },
        merge_bounds);
    if (!dirty_bounds.is_empty()) {
      tile_data.mark_dirty(dirty_bounds);
    }

    if (tile_data.flags.dirty) {
      IMB_mark_dirty(image_buffer);
    }
    pixels_updated |= tile_data.flags.dirty;
  }

  pixel_node.flags.dirty |= pixels_updated;
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
                                           ImageData &image_data,
                                           Span<TileNumber> tile_numbers_to_fix)
{
  PRF_scope(ProfileCategory::Editor);
  for (image::TileNumber tile_number : tile_numbers_to_fix) {
    ImBuf *image_buffer = image_data.buffers.lookup_default(tile_number, nullptr);
    if (image_buffer == nullptr) {
      continue;
    }
    const MutableSpan<uint32_t> undo_tile_pushed = image_data.undo_tile_pushed.lookup(tile_number);

    bke::pbvh::pixels::copy_pixels(
        pbvh,
        image_data.buffers,
        tile_number,
        image_data.seam_tile_modified.lookup(tile_number),
        [&](const int x_start, const int x_end, const int y) {
          push_undo_tiles(
              image_data, tile_number, *image_buffer, undo_tile_pushed, x_start, x_end, y);
        });
  }
}

static void fix_non_manifold_seam_bleeding(Object &ob,
                                           ImageData &image_data,
                                           MutableSpan<bke::pbvh::MeshNode> /*nodes*/,
                                           MutableSpan<PixelNode> pixel_nodes,
                                           const IndexMask &node_mask)
{
  Vector<image::TileNumber> dirty_tiles = collect_dirty_tiles(pixel_nodes, node_mask);
  fix_non_manifold_seam_bleeding(*bke::object::pbvh_get(ob), image_data, dirty_tiles);
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
  PRF_scope(ProfileCategory::Editor);
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

  for (Array<uint8_t> &modified : image_data.seam_tile_modified.values()) {
    modified.as_mutable_span().fill(0);
  }

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
