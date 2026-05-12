/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <limits>
#include <string>

#include "BLI_assert.h"
#include "BLI_hash.hh"
#include "BLI_memory_utils.hh"
#include "BLI_path_utils.hh"
#include "BLI_rect.h"
#include "BLI_string.h"

#include "DNA_packedFile_types.h"
#include "DNA_vfont_types.h"

#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_vfont.hh"

#include "BLF_api.hh"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_string_image.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

/* --------------------------------------------------------------------
 * String Image Key.
 */

StringImageKey::StringImageKey(const std::string string,
                               const VFont *font,
                               const float size,
                               const HorizontalAlignment horizontal_alignment,
                               const VerticalAlignment vertical_alignment,
                               const std::optional<int> wrap_width)
    : string(string),
      font(font),
      size(size),
      horizontal_alignment(horizontal_alignment),
      vertical_alignment(vertical_alignment),
      wrap_width(wrap_width)
{
}

uint64_t StringImageKey::hash() const
{
  return get_default_hash(
      get_default_hash(string, font, size, horizontal_alignment, vertical_alignment),
      wrap_width.value_or(-1));
}

/* --------------------------------------------------------------------
 * String Image.
 */

/* Loads a BLF font from the given font ID and return its identifier. Unloading the font is the
 * responsibility of the caller. */
static int load_font(const VFont *font)
{
  if (!font || BKE_vfont_is_builtin(font)) {
    return BLF_load_default(true);
  }

  if (font->packedfile != nullptr) {
    char name[MAX_ID_FULL_NAME];
    BKE_id_full_name_get(name, &font->id, 0);
    return BLF_load_mem_unique(
        name, static_cast<const uchar *>(font->packedfile->data), font->packedfile->size);
  }

  char file_path[FILE_MAX];
  STRNCPY(file_path, font->filepath);
  BLI_path_abs(file_path, ID_BLEND_PATH_FROM_GLOBAL(&font->id));
  return BLF_load_unique(file_path);
}

/* Computes the horizontal position in pixels from which the line with the given width should be
 * drawn. The BLF module draws from the left most edge of the line. The start offset is an offset
 * that is intrinsic to the used font and thus should be canceled here to draw right from the edge
 * of the image. The total width is the total width of the image and is the maximum width of all
 * lines. */
static float compute_draw_horizontal_position(const int start_offset,
                                              const int line_width,
                                              const int total_width,
                                              const HorizontalAlignment alignment)
{
  switch (alignment) {
    case HorizontalAlignment::Left:
      return float(-start_offset);
    case HorizontalAlignment::Center:
      return -start_offset + (total_width - line_width) / 2.0f;
    case HorizontalAlignment::Right:
      return float(-start_offset + total_width - line_width);
  }

  BLI_assert_unreachable();
  return float(-start_offset);
}

/* Computes the vertical position in pixels from which the line with the given index should be
 * drawn. Assuming there are the given number of lines with each having the given height and
 * descender. The BLF module draws from the baseline of the line starting from the lower left
 * origin of the image, therefore, we need to subtract the negative descender such that the line is
 * fully drawn in the image. */
static float compute_draw_vertical_position(const int lines_count,
                                            const int line_index,
                                            const int line_height,
                                            const int descender)
{
  return (lines_count - 1 - line_index) * line_height - float(descender);
}

/* Computes the horizontal offset in pixels depending on the alignment. We also restore the start
 * offset that was canceled during drawing, see compute_draw_horizontal_position. */
static float compute_horizontal_offset(const int start_offset,
                                       const int total_width,
                                       const HorizontalAlignment alignment)
{
  switch (alignment) {
    case HorizontalAlignment::Left:
      return start_offset + total_width / 2.0f;
    case HorizontalAlignment::Center:
      return float(start_offset);
    case HorizontalAlignment::Right:
      return start_offset - total_width / 2.0f;
  }

  BLI_assert_unreachable();
  return float(start_offset);
}

/* Computes the vertical offset in pixels depending on the alignment. Assuming the image has the
 * given total height, the lines have the given height and negative descender. */
static float compute_vertical_offset(const int total_height,
                                     const int line_height,
                                     const int descender,
                                     const VerticalAlignment alignment)
{
  switch (alignment) {
    case VerticalAlignment::Top:
      return -total_height / 2.0f;
    case VerticalAlignment::TopBaseline:
      return -total_height / 2.0f + line_height + descender;
    case VerticalAlignment::Middle:
      return 0.0f;
    case VerticalAlignment::BottomBaseline:
      return total_height / 2.0f + descender;
    case VerticalAlignment::Bottom:
      return total_height / 2.0f;
  }

  BLI_assert_unreachable();
  return float(descender);
}

StringImage::StringImage(Context &context,
                         const std::string string,
                         const VFont *font,
                         const float size,
                         const HorizontalAlignment horizontal_alignment,
                         const VerticalAlignment vertical_alignment,
                         const std::optional<int> wrap_width)
    : result(context.create_result(ResultType::Float))
{
  if (string.empty() || !font || size <= 0.0f) {
    return;
  }

  const int font_identifier = load_font(font);
  if (font_identifier == -1) {
    return;
  }
  BLI_SCOPED_DEFER([&]() { BLF_unload_id(font_identifier); });

  BLF_size(font_identifier, size);
  BLF_enable(font_identifier, BLF_NO_FALLBACK);

  Vector<StringRef> lines = BLF_string_wrap(
      font_identifier, string, wrap_width.value_or(-1), BLFWrapMode::Typographical);

  /* Compute the width of all lines as well as their starting offset. The starting offset will be
   * zero in most fonts, but some special fonts might start before or after the zero point. */
  int total_width = 0;
  Array<int> line_widths(lines.size());
  int start_offset = std::numeric_limits<int>::max();
  for (const int64_t i : lines.index_range()) {
    rcti line_bounding_box;
    BLF_boundbox(font_identifier, lines[i].data(), lines[i].size(), &line_bounding_box);
    line_widths[i] = BLI_rcti_size_x(&line_bounding_box);
    total_width = math::max(total_width, line_widths[i]);
    start_offset = math::min(start_offset, line_bounding_box.xmin);
  }

  const int line_height = BLF_height_max(font_identifier);
  const int total_height = line_height * lines.size();

  /* Fill the background with alpha since the draws function does not initialize the background. */
  this->result.allocate_texture(int2(total_width, total_height), false, ResultStorageType::CPU);
  parallel_for(this->result.domain().data_size,
               [&](const int2 texel) { this->result.store_pixel(texel, 0.0f); });

  BLF_buffer_col(font_identifier, Color(1.0f, 1.0f, 1.0f, 1.0f));
  BLF_buffer(font_identifier,
             static_cast<float *>(this->result.cpu_data_for_write().data()),
             nullptr,
             total_width,
             total_height,
             1,
             nullptr);

  /* Draw each of lines in the appropriate position. */
  const int descender = BLF_descender(font_identifier);
  for (const int64_t i : lines.index_range()) {
    const float vertical_position = compute_draw_vertical_position(
        lines.size(), i, line_height, descender);
    const float horizontal_position = compute_draw_horizontal_position(
        start_offset, line_widths[i], total_width, horizontal_alignment);
    BLF_position(font_identifier, horizontal_position, vertical_position, 0.0f);
    BLF_draw_buffer(font_identifier, lines[i].data(), lines[i].size());
  }

  BLF_buffer(font_identifier, nullptr, nullptr, 0, 0, 1, nullptr);

  /* Move the image to account for the requested alignment. */
  const float horizontal_offset = compute_horizontal_offset(
      start_offset, total_width, horizontal_alignment);
  const float vertical_offset = compute_vertical_offset(
      total_height, line_height, descender, vertical_alignment);
  this->result.domain().transformation.location() = float2(horizontal_offset, vertical_offset);

  if (context.use_gpu()) {
    const Result gpu_result = this->result.upload_to_gpu(false);
    this->result.release();
    this->result = gpu_result;
  }
}

StringImage::~StringImage()
{
  this->result.release();
}

/* --------------------------------------------------------------------
 * String Image Container.
 */

void StringImageContainer::reset()
{
  /* First, delete all resources that are no longer needed. */
  map_.remove_if([](auto item) { return !item.value->needed; });

  /* Second, reset the needed status of the remaining resources to false to ready them to track
   * their needed status for the next evaluation. */
  for (auto &value : map_.values()) {
    value->needed = false;
  }
}

Result &StringImageContainer::get(Context &context,
                                  const std::string string,
                                  const VFont *font,
                                  const float size,
                                  const HorizontalAlignment horizontal_alignment,
                                  const VerticalAlignment vertical_alignment,
                                  const std::optional<int> wrap_width)
{
  const StringImageKey key(
      string, font, size, horizontal_alignment, vertical_alignment, wrap_width);

  auto &string_image = *map_.lookup_or_add_cb(key, [&]() {
    return std::make_unique<StringImage>(
        context, string, font, size, horizontal_alignment, vertical_alignment, wrap_width);
  });

  string_image.needed = true;
  return string_image.result;
}

}  // namespace blender::compositor
