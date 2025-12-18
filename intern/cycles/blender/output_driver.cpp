/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "blender/output_driver.h"

#include "BLI_listbase.h"
#include "IMB_imbuf_types.hh"
#include "RE_engine.h"

CCL_NAMESPACE_BEGIN

BlenderOutputDriver::BlenderOutputDriver(::RenderEngine &b_engine) : b_engine_(b_engine) {}

BlenderOutputDriver::~BlenderOutputDriver() = default;

bool BlenderOutputDriver::read_render_tile(const Tile &tile)
{
  /* Get render result. */
  ::RenderResult *b_rr = RE_engine_begin_result(&b_engine_,
                                                tile.offset.x,
                                                tile.offset.y,
                                                tile.size.x,
                                                tile.size.y,
                                                tile.layer.c_str(),
                                                tile.view.c_str());

  /* Can happen if the intersected rectangle gives 0 width or height. */
  if (b_rr == nullptr) {
    return false;
  }

  /* layer will be missing if it was disabled in the UI */
  if (BLI_listbase_is_empty(&b_rr->layers)) {
    return false;
  }

  ::RenderLayer *b_rlay = static_cast<::RenderLayer *>(b_rr->layers.first);

  /* Copy each pass.
   * TODO:copy only the required ones for better performance? */
  LISTBASE_FOREACH (::RenderPass *, b_pass, &b_rlay->passes) {
    if (b_pass->ibuf && b_pass->ibuf->float_buffer.data) {
      const float *rect = b_pass->ibuf->float_buffer.data;
      tile.set_pass_pixels(b_pass->name, b_pass->channels, rect);
    }
    else {
      blender::Array<float> rect(int64_t(b_pass->channels) * b_pass->rectx * b_pass->recty, 0.0f);
      tile.set_pass_pixels(b_pass->name, b_pass->channels, rect.data());
    }
  }

  RE_engine_end_result(&b_engine_, b_rr, false, false, false);

  return true;
}

bool BlenderOutputDriver::update_render_tile(const Tile &tile)
{
  /* Use final write for preview renders, otherwise render result wouldn't be updated
   * quickly on Blender side. For all other cases we use the display driver. */
  if (b_engine_.flag & RE_ENGINE_PREVIEW) {
    write_render_tile(tile);
    return true;
  }

  /* Don't highlight full-frame tile. */
  if (!(tile.size == tile.full_size)) {
    RE_engine_tile_highlight_clear_all(&b_engine_);
    RE_engine_tile_highlight_set(
        &b_engine_, tile.offset.x, tile.offset.y, tile.size.x, tile.size.y, true);
  }

  return false;
}

void BlenderOutputDriver::write_render_tile(const Tile &tile)
{
  RE_engine_tile_highlight_clear_all(&b_engine_);

  /* Get render result. */
  ::RenderResult *b_rr = RE_engine_begin_result(&b_engine_,
                                                tile.offset.x,
                                                tile.offset.y,
                                                tile.size.x,
                                                tile.size.y,
                                                tile.layer.c_str(),
                                                tile.view.c_str());

  /* Can happen if the intersected rectangle gives 0 width or height. */
  if (b_rr == nullptr) {
    return;
  }

  /* Layer will be missing if it was disabled in the UI. */
  if (BLI_listbase_is_empty(&b_rr->layers)) {
    return;
  }

  ::RenderLayer *b_rlay = static_cast<::RenderLayer *>(b_rr->layers.first);

  vector<float> pixels(static_cast<size_t>(tile.size.x) * tile.size.y * 4);

  /* Copy each pass. */
  LISTBASE_FOREACH (::RenderPass *, b_pass, &b_rlay->passes) {
    if (!tile.get_pass_pixels(b_pass->name, b_pass->channels, pixels.data())) {
      memset(pixels.data(), 0, pixels.size() * sizeof(float));
    }
    if (b_pass->ibuf && b_pass->ibuf->float_buffer.data) {
      float *rect = b_pass->ibuf->float_buffer.data;
      const size_t size_in_bytes = sizeof(float) * b_pass->rectx * b_pass->recty *
                                   b_pass->channels;
      memcpy(rect, pixels.data(), size_in_bytes);
    }
  }

  RE_engine_end_result(&b_engine_, b_rr, false, false, true);
}

CCL_NAMESPACE_END
