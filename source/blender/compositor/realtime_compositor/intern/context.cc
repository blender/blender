/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_rect.h"

#include "DNA_vec_types.h"

#include "COM_context.hh"
#include "COM_static_cache_manager.hh"
#include "COM_static_shader_manager.hh"
#include "COM_texture_pool.hh"

namespace blender::realtime_compositor {

Context::Context(TexturePool &texture_pool) : texture_pool_(texture_pool) {}

int2 Context::get_compositing_region_size() const
{
  const rcti compositing_region = get_compositing_region();
  return int2(BLI_rcti_size_x(&compositing_region), BLI_rcti_size_y(&compositing_region));
}

float Context::get_render_percentage() const
{
  return get_scene()->r.size / 100.0f;
}

int Context::get_frame_number() const
{
  return get_scene()->r.cfra;
}

float Context::get_time() const
{
  const float frame_number = float(get_frame_number());
  const float frame_rate = float(get_scene()->r.frs_sec) / float(get_scene()->r.frs_sec_base);
  return frame_number / frame_rate;
}

TexturePool &Context::texture_pool()
{
  return texture_pool_;
}

StaticShaderManager &Context::shader_manager()
{
  return shader_manager_;
}

StaticCacheManager &Context::cache_manager()
{
  return cache_manager_;
}

}  // namespace blender::realtime_compositor
