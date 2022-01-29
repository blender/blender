/*
 * Copyright 2021 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "blender/output_driver.h"

CCL_NAMESPACE_BEGIN

BlenderOutputDriver::BlenderOutputDriver(BL::RenderEngine &b_engine) : b_engine_(b_engine)
{
}

BlenderOutputDriver::~BlenderOutputDriver()
{
}

bool BlenderOutputDriver::read_render_tile(const Tile &tile)
{
  /* Get render result. */
  BL::RenderResult b_rr = b_engine_.begin_result(tile.offset.x,
                                                 tile.offset.y,
                                                 tile.size.x,
                                                 tile.size.y,
                                                 tile.layer.c_str(),
                                                 tile.view.c_str());

  /* Can happen if the intersected rectangle gives 0 width or height. */
  if (b_rr.ptr.data == NULL) {
    return false;
  }

  BL::RenderResult::layers_iterator b_single_rlay;
  b_rr.layers.begin(b_single_rlay);

  /* layer will be missing if it was disabled in the UI */
  if (b_single_rlay == b_rr.layers.end()) {
    return false;
  }

  BL::RenderLayer b_rlay = *b_single_rlay;

  /* Copy each pass.
   * TODO:copy only the required ones for better performance? */
  for (BL::RenderPass &b_pass : b_rlay.passes) {
    tile.set_pass_pixels(b_pass.name(), b_pass.channels(), (float *)b_pass.rect());
  }

  b_engine_.end_result(b_rr, false, false, false);

  return true;
}

bool BlenderOutputDriver::update_render_tile(const Tile &tile)
{
  /* Use final write for preview renders, otherwise render result wouldn't be updated
   * quickly on Blender side. For all other cases we use the display driver. */
  if (b_engine_.is_preview()) {
    write_render_tile(tile);
    return true;
  }

  /* Don't highlight full-frame tile. */
  if (!(tile.size == tile.full_size)) {
    b_engine_.tile_highlight_clear_all();
    b_engine_.tile_highlight_set(tile.offset.x, tile.offset.y, tile.size.x, tile.size.y, true);
  }

  return false;
}

void BlenderOutputDriver::write_render_tile(const Tile &tile)
{
  b_engine_.tile_highlight_clear_all();

  /* Get render result. */
  BL::RenderResult b_rr = b_engine_.begin_result(tile.offset.x,
                                                 tile.offset.y,
                                                 tile.size.x,
                                                 tile.size.y,
                                                 tile.layer.c_str(),
                                                 tile.view.c_str());

  /* Can happen if the intersected rectangle gives 0 width or height. */
  if (b_rr.ptr.data == NULL) {
    return;
  }

  BL::RenderResult::layers_iterator b_single_rlay;
  b_rr.layers.begin(b_single_rlay);

  /* Layer will be missing if it was disabled in the UI. */
  if (b_single_rlay == b_rr.layers.end()) {
    return;
  }

  BL::RenderLayer b_rlay = *b_single_rlay;

  vector<float> pixels(static_cast<size_t>(tile.size.x) * tile.size.y * 4);

  /* Copy each pass. */
  for (BL::RenderPass &b_pass : b_rlay.passes) {
    if (!tile.get_pass_pixels(b_pass.name(), b_pass.channels(), &pixels[0])) {
      memset(&pixels[0], 0, pixels.size() * sizeof(float));
    }

    b_pass.rect(&pixels[0]);
  }

  b_engine_.end_result(b_rr, false, false, true);
}

CCL_NAMESPACE_END
