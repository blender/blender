/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

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
#include "BLI_kdtree_new.hh"
#include "BLI_listbase.hh"
#include "BLI_math_color_blend.hh"
#include "BLI_math_color_c.hh"
#include "BLI_math_geom_c.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector.hh"
#ifdef DEBUG_PIXEL_NODES
#  include "BLI_hash_c.hh"
#endif

#include "DNA_mesh_types.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"

#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_image_wrappers.hh"
#include "BKE_object_types.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_paint_bvh_pixels.hh"

#include "image_paint_intern.hh"
#include "mesh_brush_common.hh"
#include "sculpt_automask.hh"
#include "sculpt_intern.hh"

#include <atomic>

namespace blender {

namespace ed::sculpt_paint::image {

using namespace blender::bke::pbvh::pixels;
using namespace blender::bke::image;

ImageData::~ImageData()
{
  if (!image) {
    return;
  }

  int non_null_buffers = 0;
  for (ImBuf *buffer : image_buffers.values()) {
    if (buffer) {
      non_null_buffers++;
    }
    BKE_image_release_ibuf(image, buffer, nullptr);
  }
  BLI_assert(non_null_buffers <= image->tiles.count());
  UNUSED_VARS_NDEBUG(non_null_buffers);
  image_buffers.clear();
}
std::unique_ptr<ImageData> ImageData::init_active_image(Object &ob, ImagePaintSettings &settings)
{
  std::optional<CanvasImageData> canvas_image_data = BKE_paint_canvas_image_get(settings, ob);
  if (!canvas_image_data) {
    return nullptr;
  }

  std::unique_ptr<ImageData> image_data = std::make_unique<ImageData>();
  image_data->image = canvas_image_data->first;
  image_data->image_user = canvas_image_data->second;

  BLI_assert(image_data->image);

  return image_data;
}
std::unique_ptr<ImageData> ImageData::init_mask_image(ImagePaintSettings &settings)
{
  if (!settings.stencil) {
    return nullptr;
  }

  std::unique_ptr<ImageData> image_data = std::make_unique<ImageData>();
  image_data->image = settings.stencil;
  image_data->image_user = ImageUser{};
  image_data->is_data_colorspace = true;

  BLI_assert(image_data->image);

  return image_data;
}

static void fetch_image_buffers(ImageData &image_data,
                                ImageData *mask_data,
                                bke::pbvh::Node & /*node*/,
                                PixelNode &pixel_node)
{
  PRF_scope(ProfileCategory::Editor);
  for (const UDIMTilePixels &tile : pixel_node.tiles) {
    ImBuf *buffer = image_data.image_buffers.lookup_or_add_cb(tile.tile_number, [&]() {
      ImageUser tile_user = image_data.image_user_get();
      tile_user.tile = tile.tile_number;

      ImBuf *ibuf = BKE_image_acquire_ibuf(image_data.image, &tile_user, nullptr);
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
        return Array<bool>(int64_t(tiles_x) * tiles_y, 0);
      });
      image_data.processors.lookup_or_add_cb(tile.tile_number, [&]() {
        const ColorSpace &buffer_colorspace = buffer->float_data() ? buffer->float_colorspace() :
                                                                     buffer->byte_colorspace();

        TileColorspaceProcessor processor;
        if (image_data.is_data_colorspace) {
          return processor;
        }

        /* Fast path for sRGB byte, to avoid overhead of calling into OpenColorIO. */
        if (!buffer->float_data() && buffer->byte_data() &&
            IMB_colormanagement_space_is_srgb(&buffer_colorspace))
        {
          processor.is_srgb_byte = true;
          processor.is_noop = false;
          return processor;
        }

        ColormanageProcessor buffer_to_linear =
            ColormanageProcessor::colorspace_processor_to_scene_linear_new(buffer_colorspace);
        if (buffer_to_linear.is_noop()) {
          return processor;
        }

        processor.buffer_to_linear_processor = std::move(buffer_to_linear);
        processor.linear_to_buffer_processor =
            ColormanageProcessor::colorspace_processor_from_scene_linear_new(buffer_colorspace);
        processor.is_noop = false;

        return processor;
      });
      if (buffer->float_data()) {
        BLI_assert(ELEM(buffer->channels, 0, 4));
        image_data.data_buffers.add_overwrite(
            tile.tile_number,
            MutableSpan(reinterpret_cast<float4 *>(buffer->float_data_for_write()),
                        IMB_get_pixel_count(buffer)));
      }
      else {
        image_data.data_buffers.add_overwrite(
            tile.tile_number,
            MutableSpan(reinterpret_cast<uchar4 *>(buffer->byte_data_for_write()),
                        IMB_get_pixel_count(buffer)));
      }
    }

    if (mask_data) {
      ImBuf *mask_buffer = mask_data->image_buffers.lookup_or_add_cb(tile.tile_number, [&]() {
        ImageUser tile_user = mask_data->image_user_get();
        tile_user.tile = tile.tile_number;

        ImBuf *ibuf = BKE_image_acquire_ibuf(mask_data->image, &tile_user, nullptr);
        return ibuf;
      });
      if (mask_buffer) {
        if (mask_buffer->float_data()) {
          BLI_assert(ELEM(mask_buffer->channels, 0, 4));
          mask_data->mask_buffers.add_overwrite(
              tile.tile_number,
              Span(reinterpret_cast<const float4 *>(mask_buffer->float_data()),
                   IMB_get_pixel_count(mask_buffer)));
        }
        else {
          mask_data->mask_buffers.add_overwrite(
              tile.tile_number,
              Span(reinterpret_cast<const uchar4 *>(mask_buffer->byte_data()),
                   IMB_get_pixel_count(mask_buffer)));
        }
      }
    }
  }
}

static void calc_pixel_row_positions(const float3 P_start,
                                     const float3 P_delta,
                                     const MutableSpan<float3> positions,
                                     IndexRange range)
{
  PRF_scope(ProfileCategory::Editor);
  BLI_assert(range.size() == positions.size());

  const float3 start = P_start + P_delta * range.start();
  for (const int i : positions.index_range()) {
    positions[i] = start + P_delta * i;
  }
}

static BitVector<> init_uv_primitives_brush_test(const Bounds<float3> &brush_bounds,
                                                 const Span<int> corner_verts,
                                                 const Span<int3> corner_tris,
                                                 const Span<int> tri_indices,
                                                 const Span<float3> positions)
{
  PRF_scope(ProfileCategory::Editor);
  BitVector<> brush_test(tri_indices.size());
  for (const int i : tri_indices.index_range()) {
    const int3 &tri = corner_tris[tri_indices[i]];

    Bounds<float3> tri_bounds(positions[corner_verts[tri[0]]]);
    math::min_max(positions[corner_verts[tri[1]]], tri_bounds.min, tri_bounds.max);
    math::min_max(positions[corner_verts[tri[2]]], tri_bounds.min, tri_bounds.max);

    brush_test[i].set(
        isect_aabb_aabb_v3(brush_bounds.min, brush_bounds.max, tri_bounds.min, tri_bounds.max));
  }
  return brush_test;
}

static void apply_selection_filter(const Span<int> corner_tri_faces,
                                   const Span<int> tri_indices,
                                   const Span<bool> select_poly,
                                   BitVector<> &brush_test)
{
  BLI_assert(tri_indices.size() == brush_test.size());

  for (const int i : tri_indices.index_range()) {
    const bool current = brush_test[i];
    brush_test[i].set(current && select_poly[corner_tri_faces[tri_indices[i]]]);
  }
}

namespace {

/** Cached settings for faster paint blending. */
struct PaintBlendSettings {
  explicit PaintBlendSettings(const Brush &brush)
      : brush_alpha(brush.alpha), blend_mode(IMB_BlendMode(brush.blend))
  {
  }

  float brush_alpha;
  IMB_BlendMode blend_mode;
};

}  // namespace

/** Blend one pixel with the brush. */
BLI_INLINE float4 paint_blend_pixel(const float4 &brush_color,
                                    const float brush_alpha,
                                    const bool is_mix,
                                    const IMB_BlendMode blend_mode,
                                    const float factor,
                                    const float4 color)
{
  const float4 paint_color = brush_color * (factor * brush_alpha);
  /* Many blend modes don't write alpha to the result, so copy it. */
  float4 result = color;
  /* TODO: try making IMB_blend_color_float inline instead. */
  if (is_mix) {
    blend_color_mix_float(result, color, paint_color);
  }
  else {
    IMB_blend_color_float(result, color, paint_color, blend_mode);
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
template<typename PixelT, typename ColorsT>
BLI_NOINLINE static void paint_blend_pixels_color_managed(
    const PaintBlendSettings &settings,
    const ColorsT colors,
    const Span<float> factors,
    const MutableSpan<PixelT> image_pixels,
    [[maybe_unused]] const int img_y,
    const TileColorspaceProcessor &processors,
    Vector<float4> &paint_pixels)
{
  constexpr bool is_float = std::is_same_v<typename PixelT::base_type, float>;
  PRF_scope(ProfileCategory::Editor);

  const int size = factors.size();
  MutableSpan<float4> scene_linear_pixels;

  /* Convert from image colorspace to scene linear. */
  if constexpr (is_float) {
    scene_linear_pixels = image_pixels;
  }
  else {
    paint_pixels.resize(size);
    for (int i = 0; i < size; i++) {
      rgba_uchar_to_float(paint_pixels[i], image_pixels[i]);
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
    scene_linear_pixels[i] = paint_blend_pixel(
        colors[i], settings.brush_alpha, is_mix, settings.blend_mode, factors[i], scene);
  }

  /* Convert from scene linear to image colorspace. */
  if (!processors.is_noop) {
    processors.linear_to_buffer_processor.apply(
        reinterpret_cast<float *>(scene_linear_pixels.data()), size, 1, 4, false);
  }

  /* Write out byte, float was already modified in place. */
  if constexpr (!is_float) {
    for (int i = 0; i < size; i++) {
      rgba_float_to_uchar(image_pixels[i], scene_linear_pixels[i]);
    }
  }
}

inline static float4 scene_linear_pixel_get(
    const Map<image::TileNumber, TileColorspaceProcessor> &tile_processors,
    const Map<image::TileNumber, BufferType> &tile_buffers,
    const image::TileNumber tile_number,
    const int img_index)
{
  const BufferType type = tile_buffers.lookup(tile_number);
  const TileColorspaceProcessor &processor = *tile_processors.lookup_ptr(tile_number);

  if (std::holds_alternative<MutableSpan<float4>>(type)) {
    const Span<float4> image_pixels = std::get<MutableSpan<float4>>(type);
    if (processor.is_noop) {
      return image_pixels[img_index];
    }
    float4 scene = image_pixels[img_index];
    processor.buffer_to_linear_processor.apply_v4(scene);
    return scene;
  }
  if (std::holds_alternative<MutableSpan<uchar4>>(type)) {
    const Span<uchar4> image_pixels = std::get<MutableSpan<uchar4>>(type);
    if (processor.is_srgb_byte) {
      float4 scene;
      srgb_to_linearrgb_uchar4(scene, image_pixels[img_index]);
      IMB_colormanagement_rec709_to_scene_linear(scene, scene);
      return scene;
    }

    float4 scene;
    rgba_uchar_to_float(scene, image_pixels[img_index]);
    processor.buffer_to_linear_processor.apply_v4(scene);
    return scene;
  }

  BLI_assert_unreachable();
  return float4(0.0f);
}

namespace {

/** Paint color sources for #ColorsT in #paint_blend_pixels. */
struct SinglePaintColor {
  float4 color;

  const float4 &operator[](const int /*i*/) const
  {
    return color;
  }
};

struct ArrayPaintColor {
  Span<float4> colors;

  float4 operator[](const int i) const
  {
    /* TODO: Should this read the alpha of the source pixel or just use the color? */
    return float4(colors[i].xyz(), 1.0f);
  }
};

};  // namespace

/**
 * Perform paint pixel blending with computed factors.
 * Templated and specialized for common color spaces since this is a hot-spot.
 */
template<typename PixelT, typename ColorsT>
static void paint_blend_pixels(const PaintBlendSettings &settings,
                               const ColorsT colors,
                               const Span<float> factors,
                               const MutableSpan<PixelT> image_pixels,
                               const int img_y,
                               const TileColorspaceProcessor &processors,
                               Vector<float4> &paint_pixels)
{
  constexpr bool is_float = std::is_same_v<typename PixelT::base_type, float>;
  const bool fast_colorspace = is_float ? processors.is_noop : processors.is_srgb_byte;
  if (!fast_colorspace) {
    /* Slow path with OpenColorIO. */
    paint_blend_pixels_color_managed<PixelT>(
        settings, colors, factors, image_pixels, img_y, processors, paint_pixels);
    return;
  }

  PRF_scope(ProfileCategory::Editor);

  /* Keep variables in registers during the loop. */
  const int size = factors.size();
  const float brush_alpha = settings.brush_alpha;
  const bool is_mix = settings.blend_mode == IMB_BLEND_MIX;
  const IMB_BlendMode blend_mode = settings.blend_mode;

  if constexpr (is_float) {
    /* Scene linear float. */
    for (int i = 0; i < size; i++) {
      float4 scene = image_pixels[i];
#ifdef DEBUG_PIXEL_NODES
      scene = paint_debug_color(scene, img_y);
#endif
      image_pixels[i] = paint_blend_pixel(
          colors[i], brush_alpha, is_mix, blend_mode, factors[i], scene);
    }
  }
  else {
    /* sRGB byte. */
    paint_pixels.resize(size);
    for (int i = 0; i < size; i++) {
      float4 scene;
      srgb_to_linearrgb_uchar4(scene, image_pixels[i]);
      IMB_colormanagement_rec709_to_scene_linear(scene, scene);
#ifdef DEBUG_PIXEL_NODES
      scene = paint_debug_color(scene, img_y);
#endif
      paint_pixels[i] = paint_blend_pixel(
          colors[i], brush_alpha, is_mix, blend_mode, factors[i], scene);
    }

    /* SIMD optimized write. */
    const float (*matrix)[3] = blender::colorspace::scene_linear_is_rec709 ?
                                   nullptr :
                                   reinterpret_cast<const float (*)[3]>(
                                       blender::colorspace::scene_linear_to_rec709.ptr());
    linearrgb_to_srgb_uchar4_n(reinterpret_cast<uchar(*)[4]>(image_pixels.data()),
                               reinterpret_cast<const float (*)[4]>(paint_pixels.data()),
                               size,
                               matrix);
  }
}

template<typename ColorsT>
static void paint_blend_pixels(const PaintBlendSettings &settings,
                               const ColorsT colors,
                               const Span<float> factors,
                               const BufferType &buffer,
                               const int img_index,
                               const int img_y,
                               const TileColorspaceProcessor &processors,
                               Vector<float4> &paint_pixels)
{
  if (std::holds_alternative<MutableSpan<float4>>(buffer)) {
    paint_blend_pixels<float4>(
        settings,
        colors,
        factors,
        std::get<MutableSpan<float4>>(buffer).slice(img_index, factors.size()),
        img_y,
        processors,
        paint_pixels);
    return;
  }
  paint_blend_pixels<uchar4>(
      settings,
      colors,
      factors,
      std::get<MutableSpan<uchar4>>(buffer).slice(img_index, factors.size()),
      img_y,
      processors,
      paint_pixels);
}

namespace {

struct PaintLocalData {
  Vector<float3> pixel_positions;
  Vector<float> distances;
  Vector<float> factors;

  Vector<float4> paint_blend_pixels;

  /** Spans that were painted on by the thread. */
  struct ModifiedSpan {
    int x, y, size;
  };
  Vector<ModifiedSpan> modified_spans;
};

}  // namespace

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
  PRF_scope(ProfileCategory::Editor);
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
    ImageUser tile_user = image_data.image_user_get();
    tile_user.tile = tile_number;
    PaintTileMap *undo_tiles = ED_image_paint_tile_map_get();
    ED_image_paint_tile_push(
        undo_tiles, image_data.image, &image_buffer, &tile_user, tile_x, tile_y, nullptr, nullptr);
    pushed.store(1, std::memory_order_release);
  }
}

static IndexRange scratch_tiles_range(const int img_width,
                                      const int img_x_start,
                                      const int img_x_end,
                                      const int img_y)
{
  const int tile_y = img_y >> SCRATCH_TILE_BITS;
  const int tile_x_start = img_x_start >> SCRATCH_TILE_BITS;
  const int tile_x_end = img_x_end >> SCRATCH_TILE_BITS;
  const int scratch_tile_width = (img_width + SCRATCH_TILE_SIZE - 1) >> SCRATCH_TILE_BITS;

  const int tile_start = tile_y * scratch_tile_width + tile_x_start;
  const int tile_end = tile_y * scratch_tile_width + tile_x_end;

  return IndexRange::from_begin_end_inclusive(tile_start, tile_end);
}

/** Allocate the scratch tiles covering a pixel row if needed, and tag them as modified. */
static void activate_scratch_tiles(ImageData &image_data,
                                   ScratchTiles &scratch,
                                   const int img_width,
                                   const int img_x_start,
                                   const int img_x_end,
                                   const int img_y)
{
  PRF_scope(ProfileCategory::Editor);
  for (const int tile_index : scratch_tiles_range(img_width, img_x_start, img_x_end, img_y)) {
    /* Atomic because threads can share a scratch tile. */
    std::atomic_ref<bool>(scratch.modified[tile_index]).store(true, std::memory_order_relaxed);

    std::atomic_ref<uint32_t> allocated(scratch.allocated[tile_index]);
    /* This atomic load is free on x86_64, and cheap on arm64. */
    if (allocated.load(std::memory_order_acquire)) {
      continue;
    }

    {
      std::scoped_lock allocate_lock(image_data.scratch_mutex);

      if (allocated.load(std::memory_order_relaxed)) {
        continue;
      }

      scratch.colors[tile_index].reinitialize(SCRATCH_TILE_AREA);
      scratch.factors[tile_index].resize(SCRATCH_TILE_AREA, 0);
      scratch.positions[tile_index].reinitialize(SCRATCH_TILE_AREA);
      allocated.store(1, std::memory_order_release);
    }
  }
}

/**
 * Mark the seam tiles covering the given pixel row as modified. Dilated by one tile so a
 * seam whose second source sits one pixel across a tile boundary is still handled.
 */
static void mark_seam_tiles_modified(MutableSpan<bool> mask,
                                     const int tiles_x,
                                     const int tiles_y,
                                     const int x_start,
                                     const int x_end,
                                     const int y)
{
  PRF_scope(ProfileCategory::Editor);
  const int tile_y = y >> SEAM_TILE_BITS;
  const int tile_y_min = (tile_y > 0) ? tile_y - 1 : 0;
  const int tile_y_max = (tile_y + 1 < tiles_y) ? tile_y + 1 : tiles_y - 1;

  const int tile_x_start = x_start >> SEAM_TILE_BITS;
  const int tile_x_end = x_end >> SEAM_TILE_BITS;
  const int tile_x_min = (tile_x_start > 0) ? tile_x_start - 1 : 0;
  const int tile_x_max = (tile_x_end + 1 < tiles_x) ? tile_x_end + 1 : tiles_x - 1;

  for (int dty = tile_y_min; dty <= tile_y_max; dty++) {
    for (int dtx = tile_x_min; dtx <= tile_x_max; dtx++) {
      mask[int64_t(dty) * tiles_x + dtx] = true;
    }
  }
}

template<typename PixelT>
static void calc_stencil_mask_factors(const Span<PixelT> mask_buffer,
                                      const bool is_mask_inverted,
                                      const int2 mask_size,
                                      const int2 image_size,
                                      const MutableSpan<float> tri_factors,
                                      const int tri_row_size,
                                      const int tri_row_x,
                                      const int run_y)
{
  constexpr bool is_float = std::is_same_v<typename PixelT::base_type, float>;
  const float inv_w = 1.0f / image_size[0];
  const float inv_h = 1.0f / image_size[1];
  const float mask_scale_x = mask_size[0] * inv_w;
  const float mask_scale_y = mask_size[1] * inv_h;
  const int mask_run_y = std::clamp(int(run_y * mask_scale_y), 0, mask_size[1]);

  for (int i = 0; i < tri_row_size; i++) {
    if (tri_factors[i] == 0.0f) {
      continue;
    }
    int mask_tri_row_x = std::clamp(int((tri_row_x + i) * mask_scale_x), 0, mask_size[0]);
    int mask_pixel_index = (mask_run_y * mask_size[0]) + mask_tri_row_x;
    float mask_value;

    if constexpr (is_float) {
      const float4 rgba_f = mask_buffer[mask_pixel_index];
      mask_value = ((rgba_f[0] + rgba_f[1] + rgba_f[2]) * (1.0f / 3.0f)) * rgba_f[3];
    }
    else {
      const uchar4 rgba_ub = mask_buffer[mask_pixel_index];
      mask_value = ((rgba_ub[0] + rgba_ub[1] + rgba_ub[2]) * (1.0f / (255.0f * 3.0f))) *
                   (rgba_ub[3] * (1.0f / 255.0f));
    }
    float mask = is_mask_inverted ? mask_value : 1 - mask_value;
    tri_factors[i] *= mask;
  }
}

namespace {

/** The data of a paint step shared between brush types. */
struct PaintStepContext : NonCopyable, NonMovable {
  const Brush &brush;
  const SculptSession &ss;
  const StrokeCache &cache;
  Span<float3> positions_eval;
  Span<int> corner_verts;
  Span<int3> corner_tris;

  /** Only set when painting is restricted to selected faces. */
  Span<int> corner_tri_faces;
  VArraySpan<bool> select_poly;

  const ImageData *mask_data;
  bool is_mask_inverted;

  PaintStepContext(const Depsgraph &depsgraph,
                   const ImagePaintSettings &paint_settings,
                   const Brush &brush,
                   const Object &ob,
                   const ImageData *mask_data)
      : brush(brush),
        ss(*ob.runtime->sculpt_session),
        cache(*ss.cache),
        positions_eval(bke::pbvh::vert_positions_eval(depsgraph, ob)),
        mask_data(mask_data),
        is_mask_inverted((paint_settings.flag & IMAGEPAINT_PROJECT_LAYER_STENCIL_INV) != 0)
  {
    const Mesh &mesh = *id_cast<const Mesh *>(ob.data);
    corner_verts = mesh.corner_verts();
    corner_tris = mesh.corner_tris();
    if ((mesh.editflag & ME_EDIT_PAINT_FACE_SEL) != 0) {
      const bke::AttributeAccessor attributes = mesh.attributes();
      select_poly = *attributes.lookup<bool>(".select_poly", bke::AttrDomain::Face);
      corner_tri_faces = mesh.corner_tri_faces();
    }
  }
};

/** Painting UDIM tile context shared between all its runs. */
struct PaintTileData : NonCopyable, NonMovable {
  PaintTileData(const UDIMTilePixels &pixels,
                ImBuf &image_buffer,
                BufferType buffer,
                const TileColorspaceProcessor &processors)
      : pixels(pixels), image_buffer(image_buffer), buffer(buffer), processors(processors)
  {
  }

  const UDIMTilePixels &pixels;
  ImBuf &image_buffer;
  BufferType buffer;
  const TileColorspaceProcessor &processors;
};

/** Run of pixels affect by a brush. Note some factors can be zero. */
struct PixelRun {
  int x;
  int y;
  int size;
  Span<float> factors;
  Span<float3> positions;
};

}  // namespace

template<typename Fn>
static void foreach_pixel_tri_row(const UDIMTilePixels &tile_data,
                                  Span<float3x3> pixel_to_position_mats,
                                  const int run_x,
                                  const int run_y,
                                  const int run_begin,
                                  const int run_end,
                                  Fn &&fn)
{
  int tri_row_offset = 0;
  for (int k = run_begin; k < run_end; k++) {
    const PackedPixelRow &tri_row = tile_data.pixel_rows[k];
    const int tri_row_size = int(tri_row.num_pixels);

    /* Use pixel to position mapping to compute 3D position. */
    const int tri_row_x = run_x + tri_row_offset;
    const int uv_prim_index = tri_row.uv_primitive_index;
    const float3x3 &pixel_to_position = pixel_to_position_mats[uv_prim_index];
    const float3 P_delta = pixel_to_position[0];
    const float3 P_start = P_delta * tri_row_x + pixel_to_position[1] * run_y +
                           pixel_to_position[2];

    fn(uv_prim_index, tri_row_offset, tri_row_size, tri_row_x, P_start, P_delta);
    tri_row_offset += tri_row_size;
  }
}

/**
 * Call \a fn for each run of pixels affected by the brush to process pixels,
 * with parallelization. This computes factors, pushes undo tiles and tags
 * modified pixels.
 */
template<typename Fn>
static void foreach_run(const PaintStepContext &ctx,
                        ImageData &image_data,
                        PixelNode &pixel_node,
                        Fn &&fn)
{
  const Brush &brush = ctx.brush;
  const SculptSession &ss = ctx.ss;
  const StrokeCache &cache = ctx.cache;
  const ImageData *mask_data = ctx.mask_data;
  const bool is_mask_inverted = ctx.is_mask_inverted;

  const float3 location = cache.location_symm;
  const float radius = cache.radius;
  const Bounds<float3> brush_bounds(location - radius, location + radius);
  BitVector<> brush_test = init_uv_primitives_brush_test(brush_bounds,
                                                         ctx.corner_verts,
                                                         ctx.corner_tris,
                                                         pixel_node.uv_primitives.tri_indices,
                                                         ctx.positions_eval);

  if (!ctx.select_poly.is_empty()) {
    BLI_assert(!ctx.corner_tri_faces.is_empty());
    apply_selection_filter(
        ctx.corner_tri_faces, pixel_node.uv_primitives.tri_indices, ctx.select_poly, brush_test);
  }

  bool pixels_updated = false;

  for (UDIMTilePixels &tile_data : pixel_node.tiles) {
    ImBuf *image_buffer = image_data.image_buffers.lookup_default(tile_data.tile_number, nullptr);
    if (image_buffer == nullptr) {
      continue;
    }

    BufferType mutable_buffer = image_data.data_buffers.lookup_default(tile_data.tile_number,
                                                                       BufferType{});
    if (std::holds_alternative<std::monostate>(mutable_buffer)) {
      BLI_assert_unreachable();
      continue;
    }

    const ImBuf *mask_image_buffer = mask_data ? mask_data->image_buffers.lookup_default(
                                                     tile_data.tile_number, nullptr) :
                                                 nullptr;
    const MaskBufferType mask_buffer = mask_image_buffer ?
                                           mask_data->mask_buffers.lookup_default(
                                               tile_data.tile_number, MaskBufferType{}) :
                                           MaskBufferType{};

    const TileColorspaceProcessor *processors = image_data.processors.lookup_ptr(
        tile_data.tile_number);

    const MutableSpan<uint32_t> undo_tile_pushed = image_data.undo_tile_pushed.lookup(
        tile_data.tile_number);

    const PaintTileData tile(tile_data, *image_buffer, mutable_buffer, *processors);

    threading::EnumerableThreadSpecific<PaintLocalData> all_factor_tls;

    /* Parallelize over contiguous runs containing one or more per-triangle pixel rows. */
    const int num_runs = tile_data.pixel_row_run_starts.size() - 1;
    threading::parallel_for(IndexRange(num_runs), 32, [&](const IndexRange task_range) {
      PaintLocalData &tls = all_factor_tls.local();
      for (const int run_i : task_range) {
        const int run_begin = tile_data.pixel_row_run_starts[run_i];
        const int run_end = tile_data.pixel_row_run_starts[run_i + 1];

        /* Compute run position and size. */
        const int run_x = int(tile_data.pixel_row_run_start_coords[run_i].x);
        const int run_y = int(tile_data.pixel_row_run_start_coords[run_i].y);
        int run_size = 0;
        for (int k = run_begin; k < run_end; k++) {
          run_size += int(tile_data.pixel_rows[k].num_pixels);
        }

        /* Resize temporary data to match run size. */
        tls.factors.resize(run_size);
        tls.pixel_positions.resize(run_size);
        tls.distances.resize(run_size);
        const MutableSpan<float> factors = tls.factors;
        const MutableSpan<float3> positions = tls.pixel_positions;
        const MutableSpan<float> distances = tls.distances;

        /* Tracking for active subset of run with nonzero factors. */
        int run_active_begin = -1;
        int run_active_end = -1;
        int tri_skip_start = -1;

        /* For each triangle pixel row in the run. */
        foreach_pixel_tri_row(
            tile_data,
            pixel_node.uv_primitives.pixel_to_position,
            run_x,
            run_y,
            run_begin,
            run_end,
            [&](const int uv_prim_index,
                const int tri_row_offset,
                const int tri_row_size,
                const int tri_row_x,
                const float3 P_start,
                const float3 P_delta) {
              /* Quick bounds check. */
              if (brush_test[uv_prim_index] &&
                  brush_bounds.intersects_segment(P_start, P_start + P_delta * tri_row_size))
              {
                /* Compute brush factors per triangle. */
                const MutableSpan<float> tri_factors = factors.slice(tri_row_offset, tri_row_size);
                const MutableSpan<float3> tri_positions = positions.slice(tri_row_offset,
                                                                          tri_row_size);
                const MutableSpan<float> tri_distances = distances.slice(tri_row_offset,
                                                                         tri_row_size);

                tri_factors.fill(1.0f);
                calc_pixel_row_positions(
                    P_start, P_delta, tri_positions, IndexRange(tri_row_size));
                calc_brush_distances(
                    ss, tri_positions, eBrushFalloffShape(brush.falloff_shape), tri_distances);
                filter_distances_with_radius(cache.radius, tri_distances, tri_factors);
                apply_hardness_to_distances(cache, tri_distances);
                calc_brush_strength_factors(cache, brush, tri_distances, tri_factors);
                calc_brush_texture_factors(
                    PaintMode::Texture3D, ss, brush, tri_positions, tri_factors);
                scale_factors(tri_factors, cache.bstrength);
                if (std::holds_alternative<Span<float4>>(mask_buffer)) {
                  calc_stencil_mask_factors<float4>(
                      std::get<Span<float4>>(mask_buffer),
                      is_mask_inverted,
                      int2(mask_image_buffer->x, mask_image_buffer->y),
                      int2(image_buffer->x, image_buffer->y),
                      tri_factors,
                      tri_row_size,
                      tri_row_x,
                      run_y);
                }
                else if (std::holds_alternative<Span<uchar4>>(mask_buffer)) {
                  calc_stencil_mask_factors<uchar4>(
                      std::get<Span<uchar4>>(mask_buffer),
                      is_mask_inverted,
                      int2(mask_image_buffer->x, mask_image_buffer->y),
                      int2(image_buffer->x, image_buffer->y),
                      tri_factors,
                      tri_row_size,
                      tri_row_x,
                      run_y);
                }

                /* Track which subset of the run has non-zero factors. */
                int tri_active_begin = -1;
                int tri_active_end = -1;
                for (int i = 0; i < tri_row_size; i++) {
                  if (tri_factors[i] != 0.0f) {
                    if (tri_active_begin < 0) {
                      tri_active_begin = i;
                    }
                    tri_active_end = i;
                  }
                }

                if (tri_active_begin >= 0) {
                  if (run_active_begin < 0) {
                    run_active_begin = tri_row_offset + tri_active_begin;
                  }
                  else if (tri_skip_start >= 0) {
                    factors.slice(tri_skip_start, tri_row_offset - tri_skip_start).fill(0.0f);
                    tri_skip_start = -1;
                  }
                  run_active_end = tri_row_offset + tri_active_end;
                }
              }
              else {
                /* No intersection, skip this triangle row. */
                if (run_active_begin >= 0 && tri_skip_start < 0) {
                  tri_skip_start = tri_row_offset;
                }
              }
            });

        if (run_active_begin < 0) {
          continue;
        }

        /* Record modified spans. */
        const int run_active_size = run_active_end - run_active_begin + 1;
        const int run_active_x = run_x + run_active_begin;
        tls.modified_spans.append({.x = run_active_x, .y = run_y, .size = run_active_size});

        /* Push undo tiles before we edit the pixels. */
        push_undo_tiles(image_data,
                        tile_data.tile_number,
                        *image_buffer,
                        undo_tile_pushed,
                        run_active_x,
                        run_active_x + run_active_size - 1,
                        run_y);

        fn(tile,
           PixelRun{run_active_x,
                    run_y,
                    run_active_size,
                    factors.slice(run_active_begin, run_active_size),
                    positions.slice(run_active_begin, run_active_size)},
           tls);
      }
    });

    /* Mark the seam tiles modified and update dirty bounds. */
    const MutableSpan<bool> seam_tile_modified = image_data.seam_tile_modified.lookup(
        tile_data.tile_number);
    const int seam_tiles_x = (image_buffer->x + SEAM_TILE_SIZE - 1) >> SEAM_TILE_BITS;
    const int seam_tiles_y = (image_buffer->y + SEAM_TILE_SIZE - 1) >> SEAM_TILE_BITS;
    Bounds<int2> dirty_bounds = negative_bounds();
    for (PaintLocalData &tls : all_factor_tls) {
      for (const PaintLocalData::ModifiedSpan span : tls.modified_spans) {
        mark_seam_tiles_modified(seam_tile_modified,
                                 seam_tiles_x,
                                 seam_tiles_y,
                                 span.x,
                                 span.x + span.size - 1,
                                 span.y);
        const int2 start(span.x, span.y);
        const int2 end = start + int2(span.size, 1);
        dirty_bounds = bounds::merge(dirty_bounds, Bounds<int2>(start, end));
      }
    }
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

static void do_paint_pixels(const PaintStepContext &ctx,
                            const PaintBlendSettings &blend_settings,
                            const float4 &brush_color,
                            ImageData &image_data,
                            bke::pbvh::Node & /*node*/,
                            PixelNode &pixel_node)
{
  PRF_scope(ProfileCategory::Editor);

  foreach_run(ctx,
              image_data,
              pixel_node,
              [&](const PaintTileData &tile, const PixelRun &run, PaintLocalData &tls) {
                /* Blend pixels with computed factors. */
                paint_blend_pixels(blend_settings,
                                   SinglePaintColor{brush_color},
                                   run.factors,
                                   tile.buffer,
                                   run.y * tile.image_buffer.x + run.x,
                                   run.y,
                                   tile.processors,
                                   tls.paint_blend_pixels);
              });
}

/* -------------------------------------------------------------------- */
/** \name Smear brush helpers
 * \{ */

/** Ensure every image tile has scratch tiles, cleared from the previous brush step. */
static void prepare_scratch_tiles(ImageData &image_data)
{
  for (const auto item : image_data.image_buffers.items()) {
    const ImBuf *image_buffer = item.value;
    if (image_buffer == nullptr) {
      continue;
    }
    ScratchTiles &scratch = image_data.scratch_tiles.lookup_or_add_cb(item.key, [&]() {
      const int64_t tiles_x = (image_buffer->x + SCRATCH_TILE_SIZE - 1) >> SCRATCH_TILE_BITS;
      const int64_t tiles_y = (image_buffer->y + SCRATCH_TILE_SIZE - 1) >> SCRATCH_TILE_BITS;
      return ScratchTiles(tiles_x * tiles_y);
    });

    /* Only tiles modified by the previous step can have non-zero factors. */
    for (const int i : scratch.modified.index_range()) {
      if (scratch.modified[i]) {
        scratch.factors[i].fill(0.0f);
        scratch.modified[i] = false;
      }
    }
  }
}

static void calc_pixel_factors(const PaintStepContext &ctx,
                               ImageData &image_data,
                               bke::pbvh::Node & /*node*/,
                               PixelNode &pixel_node)
{
  PRF_scope(ProfileCategory::Editor);

#ifdef DEBUG_PIXEL_NODES
  float4 debug_color;
  uint hash = BLI_hash_int(POINTER_AS_UINT(&node));

  debug_color[0] = float(hash & 255) / 255.0f;
  debug_color[1] = float((hash >> 8) & 255) / 255.0f;
  debug_color[2] = float((hash >> 16) & 255) / 255.0f;
  debug_color[3] = 1.0f;
#endif

  foreach_run(
      ctx,
      image_data,
      pixel_node,
      [&](const PaintTileData &tile, const PixelRun &run, PaintLocalData & /*tls*/) {
        PRF_scope_with_name("calc_pixel_factors_run", ProfileCategory::Editor);
        const int img_x_start = run.x;
        const int img_x_end = run.x + run.size - 1;
        const int img_y = run.y;

        ScratchTiles &scratch = image_data.scratch_tiles.lookup(tile.pixels.tile_number);

        activate_scratch_tiles(
            image_data, scratch, tile.image_buffer.x, img_x_start, img_x_end, img_y);

        /* Write to the allocated tile buffer */
        const int tile_img_x_start = run.x % SCRATCH_TILE_SIZE;
        const int tile_img_y = run.y % SCRATCH_TILE_SIZE;

        int tile_x_offset = tile_img_x_start;
        int remaining_pixels = run.size;
        int run_offset = 0;

        int slice_size = math::min(remaining_pixels, SCRATCH_TILE_SIZE - tile_x_offset);
        for (const int scratch_tile_i :
             scratch_tiles_range(tile.image_buffer.x, img_x_start, img_x_end, img_y))
        {
          const int tile_start = tile_x_offset + tile_img_y * SCRATCH_TILE_SIZE;

          const Span<float> factor_src = run.factors.slice(run_offset, slice_size);
          const MutableSpan<float> factor_dst =
              scratch.factors[scratch_tile_i].as_mutable_span().slice(tile_start, slice_size);
          array_utils::copy(factor_src, factor_dst);

          const Span<float3> position_src = run.positions.slice(run_offset, slice_size);
          const MutableSpan<float3> position_dst =
              scratch.positions[scratch_tile_i].as_mutable_span().slice(tile_start, slice_size);
          array_utils::copy(position_src, position_dst);

          run_offset += slice_size;
          remaining_pixels -= slice_size;
          slice_size = math::min(remaining_pixels, SCRATCH_TILE_SIZE);
          tile_x_offset = 0;
        }
      });
}

static std::unique_ptr<SearchData> build_node_search_data(const ImageData &image_data,
                                                          const PixelNode &pixel_node)
{
  PRF_scope(ProfileCategory::Editor);
  struct RunRef {
    const UDIMTilePixels *tile_data;
    int tile_width;
    int run_i;
  };

  /* Pixels only covered by border extension triangles aren't sampled, since their colors are only
   * extrapolated from the original ones. Count the rest per run to fill them in parallel. */
  Vector<RunRef> run_refs;
  Vector<int> offset_data;
  for (const UDIMTilePixels &tile_data : pixel_node.tiles) {
    const ImBuf *image_buffer = image_data.image_buffers.lookup_default(tile_data.tile_number,
                                                                        nullptr);
    if (image_buffer == nullptr) {
      continue;
    }
    const int num_runs = tile_data.pixel_row_run_starts.size() - 1;
    for (const int run_i : IndexRange(num_runs)) {
      const int run_begin = tile_data.pixel_row_run_starts[run_i];
      const int run_end = tile_data.pixel_row_run_starts[run_i + 1];

      int run_size = 0;
      for (int k = run_begin; k < run_end; k++) {
        const PackedPixelRow &tri_row = tile_data.pixel_rows[k];
        if (pixel_node.uv_primitives.original_tris[tri_row.uv_primitive_index]) {
          run_size += int(tri_row.num_pixels);
        }
      }
      run_refs.append({&tile_data, image_buffer->x, run_i});
      offset_data.append(run_size);
    }
  }
  BLI_assert(run_refs.size() == offset_data.size());
  offset_data.append(0);
  const OffsetIndices offsets = offset_indices::accumulate_counts_to_offsets(offset_data);

  Array<float3, 0> positions(offsets.total_size());
  Array<PixelSource, 0> sources(offsets.total_size());
  threading::parallel_for(run_refs.index_range(), 256, [&](const IndexRange range) {
    for (const int run_ref_i : range) {
      const RunRef &run_ref = run_refs[run_ref_i];

      const UDIMTilePixels &tile_data = *run_ref.tile_data;
      const int run_i = run_ref.run_i;

      const int run_begin = tile_data.pixel_row_run_starts[run_i];
      const int run_end = tile_data.pixel_row_run_starts[run_i + 1];

      const int run_x = int(tile_data.pixel_row_run_start_coords[run_i].x);
      const int run_y = int(tile_data.pixel_row_run_start_coords[run_i].y);

      int dst = offsets[run_ref_i].start();
      foreach_pixel_tri_row(tile_data,
                            pixel_node.uv_primitives.pixel_to_position,
                            run_x,
                            run_y,
                            run_begin,
                            run_end,
                            [&](const int uv_prim_index,
                                const int /* tri_row_offset */,
                                const int tri_row_size,
                                const int tri_row_x,
                                const float3 P_start,
                                const float3 P_delta) {
                              if (pixel_node.uv_primitives.original_tris[uv_prim_index]) {
                                const int img_start = run_y * run_ref.tile_width + tri_row_x;
                                for (const int pixel_i : IndexRange(tri_row_size)) {
                                  positions[dst] = P_start + P_delta * pixel_i;
                                  sources[dst] = {tile_data.tile_number, img_start + pixel_i};
                                  dst++;
                                }
                              }
                            });
    }
  });
  return std::make_unique<SearchData>(std::move(positions), std::move(sources));
}

static IndexMask calculate_search_nodes(const ImageData &image_data,
                                        bke::pbvh::Tree &pbvh,
                                        MutableSpan<PixelNode> pixel_nodes,
                                        IndexMaskMemory &memory,
                                        const Bounds<float3> &source_bounds)
{
  const IndexMask search_nodes = bke::pbvh::search_nodes(
      pbvh, memory, [&](const bke::pbvh::Node &node) {
        return node.bounds().intersects(source_bounds);
      });
  const IndexMask nodes_to_build = IndexMask::from_predicate(
      search_nodes, memory, [&](const int i) { return !pixel_nodes[i].search_data; });
  nodes_to_build.foreach_index(
      [&](const int i) {
        pixel_nodes[i].search_data = build_node_search_data(image_data, pixel_nodes[i]);
      },
      exec_mode::grain_size(1));
  return search_nodes;
}

static ushort2 scratch_tile_index_to_xy(const int scratch_tile_index, const int img_width)
{
  const int scratch_tile_width = (img_width + SCRATCH_TILE_SIZE - 1) >> SCRATCH_TILE_BITS;
  return ushort2(scratch_tile_index % scratch_tile_width, scratch_tile_index / scratch_tile_width);
}

namespace {

/**
 * Search for the closest point that is strictly closer than a starting distance. The smear brush
 * starts with the distance to the pixel's own position, which it falls back to.
 *
 * TODO: Remove this code once we can pass in a starting distance to the generic query functions
 */
class NearestCloserQuery {
 private:
  int index_ = -1;
  float distance_sq_;

 public:
  static constexpr bool inclusive = false;

  explicit NearestCloserQuery(const float distance_sq) : distance_sq_(distance_sq) {}

  int index() const
  {
    return index_;
  }

  bool add_point(const float distance_sq, const int index)
  {
    distance_sq_ = distance_sq;
    index_ = index;
    return true;
  }

  float max_distance_sq() const
  {
    return distance_sq_;
  }
};

}  // namespace

static float distance_sq_to_bounds(const float3 &position, const Bounds<float3> &bounds)
{
  return math::distance_squared(position, math::clamp(position, bounds.min, bounds.max));
}

static void find_nearest_colors(ImageData &image_data,
                                float3 delta,
                                const Span<PixelNode> pixel_nodes,
                                const IndexMask &search_nodes,
                                const image::TileNumber tile_number)
{
  PRF_scope(ProfileCategory::Editor);

  ImBuf *image_buffer = image_data.image_buffers.lookup_default(tile_number, nullptr);
  if (image_buffer == nullptr) {
    return;
  }
  const int img_width = image_buffer->x;

  BufferType mutable_buffer = image_data.data_buffers.lookup_default(tile_number, BufferType{});
  if (std::holds_alternative<std::monostate>(mutable_buffer)) {
    BLI_assert_unreachable();
    return;
  }

  ScratchTiles &scratch = image_data.scratch_tiles.lookup(tile_number);

  const float delta_sq = math::length_squared(delta);
  const float delta_length = math::sqrt(delta_sq);

  IndexMaskMemory memory;
  const IndexMask modified_tiles = IndexMask::from_predicate(
      scratch.modified.index_range(), memory, [&](const int scratch_tile_i) {
        return scratch.modified[scratch_tile_i];
      });

  modified_tiles.foreach_index(
      [&](const int scratch_tile_i) {
        PRF_scope_with_name("find_nearest_color_tile", ProfileCategory::Editor);
        const ushort2 scratch_tile_pos = scratch_tile_index_to_xy(scratch_tile_i, image_buffer->x);
        const int2 img_start = int2(scratch_tile_pos) * SCRATCH_TILE_SIZE;

        MutableSpan<float4> scratch_tile_colors = scratch.colors[scratch_tile_i];
        Span<float> scratch_tile_factors = scratch.factors[scratch_tile_i];
        Span<float3> scratch_tile_positions = scratch.positions[scratch_tile_i];

        IndexMaskMemory tile_memory;
        const IndexMask pixels_to_fill = IndexMask::from_predicate(
            scratch_tile_factors.index_range(), tile_memory, [&](const int tile_pixel_i) {
              return scratch_tile_factors[tile_pixel_i] != 0.0f;
            });

        if (pixels_to_fill.is_empty()) {
          return;
        }

        /* Only the nodes close enough to this tile's sample positions can contain a pixel closer
         * than a sampling pixel's own position. */
        Bounds<float3> sample_bounds(scratch_tile_positions[pixels_to_fill.first()] + delta);
        pixels_to_fill.foreach_index([&](const int tile_pixel_i) {
          math::min_max(
              scratch_tile_positions[tile_pixel_i] + delta, sample_bounds.min, sample_bounds.max);
        });
        sample_bounds.pad(delta_length);

        Vector<const SearchData *, 16> nearby_data;
        search_nodes.foreach_index([&](const int node_i) {
          const SearchData &search_data = *pixel_nodes[node_i].search_data;
          if (!search_data.tree.is_empty() && search_data.bounds.intersects(sample_bounds)) {
            nearby_data.append(&search_data);
          }
        });

        pixels_to_fill.foreach_index(
            [&](const int tile_pixel_i) {
              const float3 sample_position = scratch_tile_positions[tile_pixel_i] + delta;

              /* Search the closest nodes first so the others are more likely to be skipped. */
              Vector<std::pair<float, const SearchData *>, 16> candidates;
              for (const SearchData *search_data : nearby_data) {
                const float distance_sq = distance_sq_to_bounds(sample_position,
                                                                search_data->bounds);
                if (distance_sq < delta_sq) {
                  candidates.append({distance_sq, search_data});
                }
              }
              std::ranges::sort(candidates,
                                [](const auto &a, const auto &b) { return a.first < b.first; });

              NearestCloserQuery query(delta_sq);
              const SearchData *nearest_data = nullptr;
              for (const auto &[bounds_distance_sq, search_data] : candidates) {
                const float distance_sq = query.max_distance_sq();
                if (bounds_distance_sq >= distance_sq) {
                  break;
                }
                search_data->tree.search(query, sample_position);
                if (query.max_distance_sq() < distance_sq) {
                  nearest_data = search_data;
                }
              }
              if (nearest_data) {
                const PixelSource &source = nearest_data->sources[query.index()];
                scratch_tile_colors[tile_pixel_i] = scene_linear_pixel_get(image_data.processors,
                                                                           image_data.data_buffers,
                                                                           source.tile_number,
                                                                           source.img_index);
                return;
              }

              /* Fallback to the current pixel if nothing else can be found */
              const int scratch_tile_x = tile_pixel_i % SCRATCH_TILE_SIZE;
              const int scratch_tile_y = tile_pixel_i / SCRATCH_TILE_SIZE;

              const int img_i = (img_start.y + scratch_tile_y) * img_width + img_start.x +
                                scratch_tile_x;

              scratch_tile_colors[tile_pixel_i] = scene_linear_pixel_get(
                  image_data.processors, image_data.data_buffers, tile_number, img_i);
            },
            exec_mode::grain_size(128));
      },
      exec_mode::grain_size(1));
}

template<typename Fn> static void foreach_non_zero_span(const Span<float> factors, Fn &&fn)
{
  int start = 0;
  while (start < factors.size()) {
    if (factors[start] == 0.0f) {
      start++;
      continue;
    }
    int end = start + 1;
    while (end < factors.size() && factors[end] != 0.0f) {
      end++;
    }
    fn(start, end - start);
    start = end;
  }
}

static void do_apply_tile(const PaintBlendSettings &blend_settings,
                          ImageData &image_data,
                          const image::TileNumber tile_number)
{
  PRF_scope(ProfileCategory::Editor);
  ImBuf *image_buffer = image_data.image_buffers.lookup_default(tile_number, nullptr);
  if (image_buffer == nullptr) {
    return;
  }
  const int img_width = image_buffer->x;

  BufferType mutable_buffer = image_data.data_buffers.lookup_default(tile_number, BufferType{});
  if (std::holds_alternative<std::monostate>(mutable_buffer)) {
    BLI_assert_unreachable();
    return;
  }

  const TileColorspaceProcessor *processors = image_data.processors.lookup_ptr(tile_number);

  const ScratchTiles &scratch = image_data.scratch_tiles.lookup(tile_number);

  IndexMaskMemory memory;
  const IndexMask modified_tiles = IndexMask::from_predicate(
      scratch.modified.index_range(), memory, [&](const int scratch_tile_i) {
        return scratch.modified[scratch_tile_i];
      });

  threading::EnumerableThreadSpecific<Vector<float4>> all_paint_pixels;
  modified_tiles.foreach_index(
      [&](const int scratch_tile_i) {
        PRF_scope_with_name("do_apply_tile_blend", ProfileCategory::Editor);
        Vector<float4> &paint_pixels = all_paint_pixels.local();

        const ushort2 scratch_tile_pos = scratch_tile_index_to_xy(scratch_tile_i, image_buffer->x);
        const int2 img_start = int2(scratch_tile_pos) * SCRATCH_TILE_SIZE;

        const Span<float4> scratch_tile_colors = scratch.colors[scratch_tile_i];
        const Span<float> scratch_tile_factors = scratch.factors[scratch_tile_i];

        for (const int scratch_tile_y : IndexRange(SCRATCH_TILE_SIZE)) {
          const int row_start = scratch_tile_y * SCRATCH_TILE_SIZE;
          const int img_y = img_start.y + scratch_tile_y;

          /* Pixels past the image edge always have a zero factor, so aren't part of any span. */
          foreach_non_zero_span(scratch_tile_factors.slice(row_start, SCRATCH_TILE_SIZE),
                                [&](const int scratch_tile_x, const int size) {
                                  const int span_start = row_start + scratch_tile_x;
                                  paint_blend_pixels(
                                      blend_settings,
                                      ArrayPaintColor{scratch_tile_colors.slice(span_start, size)},
                                      scratch_tile_factors.slice(span_start, size),
                                      mutable_buffer,
                                      img_y * img_width + img_start.x + scratch_tile_x,
                                      img_y,
                                      *processors,
                                      paint_pixels);
                                });
        }
      },
      exec_mode::grain_size(1));
}

/** \} */

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
    ImBuf *image_buffer = image_data.image_buffers.lookup_default(tile_number, nullptr);
    if (image_buffer == nullptr) {
      continue;
    }
    const MutableSpan<uint32_t> undo_tile_pushed = image_data.undo_tile_pushed.lookup(tile_number);
    const MutableSpan<bool> seam_tile_modified = image_data.seam_tile_modified.lookup(tile_number);

    bke::pbvh::pixels::copy_pixels(
        pbvh,
        image_data.image_buffers,
        tile_number,
        seam_tile_modified,
        [&](const int x_start, const int x_end, const int y) {
          push_undo_tiles(
              image_data, tile_number, *image_buffer, undo_tile_pushed, x_start, x_end, y);
        });

    seam_tile_modified.fill(0);
  }
}

/** \} */

static void finish_paint_step(Object &ob,
                              ImageData &image_data,
                              MutableSpan<bke::pbvh::MeshNode> nodes,
                              MutableSpan<PixelNode> pixel_nodes,
                              const IndexMask &node_mask)
{
  fix_non_manifold_seam_bleeding(
      *bke::object::pbvh_get(ob), image_data, collect_dirty_tiles(pixel_nodes, node_mask));

  node_mask.foreach_index([&](const int i) {
    bke::pbvh::pixels::mark_image_dirty(nodes[i], pixel_nodes[i], image_data.image_buffers);
  });
}

static void paint_constant_color(const Depsgraph &depsgraph,
                                 const ImagePaintSettings &paint_settings,
                                 const Brush &brush,
                                 Object &ob,
                                 ImageData &image_data,
                                 ImageData *mask_data,
                                 const float4 &brush_color,
                                 const IndexMask &node_mask)
{
  PRF_scope(ProfileCategory::Editor);

  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
  PixelData &pixel_data = *pbvh.pixels_;
  MutableSpan<PixelNode> pixel_nodes = pixel_data.nodes;

  const PaintBlendSettings blend_settings(brush);

  node_mask.foreach_index(
      [&](const int i) { fetch_image_buffers(image_data, mask_data, nodes[i], pixel_nodes[i]); });

  const PaintStepContext ctx(depsgraph, paint_settings, brush, ob, mask_data);

  node_mask.foreach_index(
      [&](const int i) {
        do_paint_pixels(ctx, blend_settings, brush_color, image_data, nodes[i], pixel_nodes[i]);
      },
      exec_mode::grain_size(1));

  finish_paint_step(ob, image_data, nodes, pixel_nodes, node_mask);
}

void do_3d_image_paint_brush(const Depsgraph &depsgraph,
                             const ImagePaintSettings &paint_settings,
                             const Brush &brush,
                             Object &ob,
                             ImageData &image_data,
                             ImageData *mask_data,
                             const IndexMask &node_mask)
{
  const bool is_inverted = ob.runtime->sculpt_session->cache->toggle_settings.invert;
  const float4 brush_color(is_inverted ?
                               BKE_brush_secondary_color_get(&paint_settings.paint, &brush) :
                               BKE_brush_color_get(&paint_settings.paint, &brush),
                           1.0f);
  paint_constant_color(
      depsgraph, paint_settings, brush, ob, image_data, mask_data, brush_color, node_mask);
}

void do_3d_image_mask_brush(const Depsgraph &depsgraph,
                            const ImagePaintSettings &paint_settings,
                            const Brush &brush,
                            Object &ob,
                            ImageData &mask_image,
                            const IndexMask &node_mask)
{
  const bool is_inverted = ob.runtime->sculpt_session->cache->toggle_settings.invert;
  const float4 brush_color(is_inverted ? float3(1.0f - brush.weight) : float3(brush.weight), 1.0f);
  paint_constant_color(
      depsgraph, paint_settings, brush, ob, mask_image, nullptr, brush_color, node_mask);
}

void do_3d_image_smear_brush(const Depsgraph &depsgraph,
                             const ImagePaintSettings &paint_settings,
                             const Brush &brush,
                             Object &ob,
                             ImageData &image_data,
                             ImageData *mask_data,
                             const IndexMask &node_mask)
{
  PRF_scope(ProfileCategory::Editor);

  const SculptSession &ss = *ob.runtime->sculpt_session;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
  PixelData &pixel_data = *pbvh.pixels_;

  if (stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
    /* We need a directional component to calculate the effect of this brush */
    return;
  }

  MutableSpan<PixelNode> pixel_nodes = pixel_data.nodes;

  /* We need to retrieve all image buffers as the nearest neighbor of a pixel may be in a different
   * tile.*/
  for (const int i : pixel_nodes.index_range()) {
    fetch_image_buffers(image_data, mask_data, nodes[i], pixel_nodes[i]);
  }
  prepare_scratch_tiles(image_data);

  for (Array<bool> &modified : image_data.seam_tile_modified.values()) {
    modified.as_mutable_span().fill(0);
  }

  const PaintStepContext ctx(depsgraph, paint_settings, brush, ob, mask_data);

  node_mask.foreach_index(
      [&](const int i) { calc_pixel_factors(ctx, image_data, nodes[i], pixel_nodes[i]); },
      exec_mode::grain_size(1));

  /* TODO: does this need to be projected in some way? Based on the normals of the specific vertex
   * maybe, or the view plane? */
  const float3 brush_delta = -1.0f * (brush.stroke_method == BRUSH_STROKE_ANCHORED ?
                                          ss.cache->grab_delta_symm :
                                          ss.cache->location_symm - ss.cache->last_location_symm);

  /* Every affected pixel is inside the brush bounds, and nothing further from its sample position
   * than its own position is used, so these bounds contain every pixel that can be sampled. */
  const float3 location = ss.cache->location_symm + brush_delta;
  const float radius = ss.cache->radius + math::length(brush_delta);
  const Bounds<float3> search_bounds(location - radius, location + radius);

  IndexMaskMemory memory;
  IndexMask search_nodes = calculate_search_nodes(
      image_data, pbvh, pixel_nodes, memory, search_bounds);
  BLI_assert(search_nodes.size() >= node_mask.size());

  const Vector<image::TileNumber> dirty_tiles = collect_dirty_tiles(pixel_nodes, node_mask);

  threading::parallel_for(dirty_tiles.index_range(), 1, [&](IndexRange range) {
    for (const int udim_tile_i : range) {
      find_nearest_colors(
          image_data, brush_delta, pixel_nodes, search_nodes, dirty_tiles[udim_tile_i]);
    }
  });

  const PaintBlendSettings blend_settings(brush);
  threading::parallel_for(dirty_tiles.index_range(), 1, [&](IndexRange range) {
    for (const int udim_tile_i : range) {
      do_apply_tile(blend_settings, image_data, dirty_tiles[udim_tile_i]);
    }
  });

  finish_paint_step(ob, image_data, nodes, pixel_nodes, node_mask);
}
}  // namespace ed::sculpt_paint::image
}  // namespace blender
