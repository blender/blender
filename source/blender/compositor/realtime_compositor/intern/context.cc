/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector.hh"
#include "BLI_rect.h"

#include "DNA_node_types.h"
#include "DNA_vec_types.h"

#include "GPU_shader.hh"

#include "BKE_node_runtime.hh"

#include "COM_context.hh"
#include "COM_render_context.hh"
#include "COM_static_cache_manager.hh"
#include "COM_texture_pool.hh"

namespace blender::realtime_compositor {

Context::Context(TexturePool &texture_pool) : texture_pool_(texture_pool) {}

RenderContext *Context::render_context() const
{
  return nullptr;
}

bool Context::is_canceled() const
{
  if (!this->get_node_tree().runtime->test_break) {
    return false;
  }
  return this->get_node_tree().runtime->test_break(get_node_tree().runtime->tbh);
}

int2 Context::get_compositing_region_size() const
{
  const rcti compositing_region = get_compositing_region();
  const int x = BLI_rcti_size_x(&compositing_region);
  const int y = BLI_rcti_size_y(&compositing_region);
  return math::max(int2(1), int2(x, y));
}

bool Context::is_valid_compositing_region() const
{
  const rcti compositing_region = get_compositing_region();
  const int x = BLI_rcti_size_x(&compositing_region);
  const int y = BLI_rcti_size_y(&compositing_region);
  return x != 0 && y != 0;
}

float Context::get_render_percentage() const
{
  return get_render_data().size / 100.0f;
}

int Context::get_frame_number() const
{
  return get_render_data().cfra;
}

float Context::get_time() const
{
  const float frame_number = float(get_frame_number());
  const float frame_rate = float(get_render_data().frs_sec) /
                           float(get_render_data().frs_sec_base);
  return frame_number / frame_rate;
}

GPUShader *Context::get_shader(const char *info_name, ResultPrecision precision)
{
  return cache_manager().cached_shaders.get(info_name, precision);
}

GPUShader *Context::get_shader(const char *info_name)
{
  return get_shader(info_name, get_precision());
}

Result Context::create_result(ResultType type, ResultPrecision precision)
{
  return Result::Temporary(type, texture_pool_, precision);
}

Result Context::create_result(ResultType type)
{
  return create_result(type, get_precision());
}

Result Context::create_temporary_result(ResultType type, ResultPrecision precision)
{
  return Result::Temporary(type, texture_pool_, precision);
}

Result Context::create_temporary_result(ResultType type)
{
  return create_temporary_result(type, get_precision());
}

TexturePool &Context::texture_pool()
{
  return texture_pool_;
}

StaticCacheManager &Context::cache_manager()
{
  return cache_manager_;
}

}  // namespace blender::realtime_compositor
