/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_bounds.hh"
#include "BLI_math_vector.hh"

#include "DNA_node_types.h"

#include "GPU_shader.hh"

#include "BKE_node_runtime.hh"

#include "COM_context.hh"
#include "COM_profiler.hh"
#include "COM_render_context.hh"
#include "COM_static_cache_manager.hh"

namespace blender::compositor {

Result Context::get_pass(const Scene * /*scene*/, int /*view_layer*/, const char * /*name*/)
{
  return this->create_result(compositor::ResultType::Color);
}

const RenderData &Context::get_render_data() const
{
  return this->get_scene().r;
}

StringRef Context::get_view_name() const
{
  return "";
}

ResultPrecision Context::get_precision() const
{
  return ResultPrecision::Full;
}

void Context::set_info_message(StringRef /*message*/) const {}

bool Context::treat_viewer_as_compositor_output() const
{
  return false;
}

void Context::populate_meta_data_for_pass(const Scene * /*scene*/,
                                          int /*view_layer_id*/,
                                          const char * /*pass_name*/,
                                          MetaData & /*meta_data*/) const
{
}

RenderContext *Context::render_context() const
{
  return nullptr;
}

Profiler *Context::profiler() const
{
  return nullptr;
}

void Context::evaluate_operation_post() const {}

bool Context::is_canceled() const
{
  if (!this->get_node_tree().runtime->test_break) {
    return false;
  }
  return this->get_node_tree().runtime->test_break(get_node_tree().runtime->tbh);
}

void Context::reset()
{
  cache_manager_.reset();
}

int2 Context::get_compositing_region_size() const
{
  return math::max(int2(1), this->get_compositing_region().size());
}

bool Context::is_valid_compositing_region() const
{
  return !this->get_compositing_region().is_empty();
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

eCompositorDenoiseQaulity Context::get_denoise_quality() const
{
  if (this->render_context()) {
    return static_cast<eCompositorDenoiseQaulity>(
        this->get_render_data().compositor_denoise_final_quality);
  }

  return static_cast<eCompositorDenoiseQaulity>(
      this->get_render_data().compositor_denoise_preview_quality);
}

gpu::Shader *Context::get_shader(const char *info_name, ResultPrecision precision)
{
  return cache_manager().cached_shaders.get(info_name, precision);
}

gpu::Shader *Context::get_shader(const char *info_name)
{
  return get_shader(info_name, get_precision());
}

Result Context::create_result(ResultType type, ResultPrecision precision)
{
  return Result(*this, type, precision);
}

Result Context::create_result(ResultType type)
{
  return create_result(type, get_precision());
}

StaticCacheManager &Context::cache_manager()
{
  return cache_manager_;
}

const Strip *Context::get_strip() const
{
  return nullptr;
}

}  // namespace blender::compositor
