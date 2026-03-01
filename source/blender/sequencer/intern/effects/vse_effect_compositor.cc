/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BKE_node_runtime.hh"

#include "COM_domain.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_sequence_types.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"

#include "SEQ_sequencer.hh"

#include "cache/compositor_cache.hh"
#include "effects.hh"
#include "render.hh"
#include "vse_compositor.hh"

namespace blender::seq {

class CompositorEffectContext : public CompositorContext {
  bNodeTree *node_group_;

  ImBuf *input_1_;
  ImBuf *input_2_;
  ImBuf *output_;
  float factor_;

 public:
  CompositorEffectContext(compositor::StaticCacheManager &cache_manager,
                          const RenderData &render_data,
                          bNodeTree *node_tree,
                          ImBuf *input_1,
                          ImBuf *input_2,
                          ImBuf *output,
                          float factor,
                          const Strip &strip)
      : CompositorContext(cache_manager, render_data, strip),
        node_group_(node_tree),
        input_1_(input_1),
        input_2_(input_2),
        output_(output),
        factor_(factor)
  {
  }

  compositor::Domain get_compositing_domain() const override
  {
    return compositor::Domain(int2(this->output_->x, this->output_->y));
  }

  void write_viewer(compositor::Result &result) override
  {
    /* Within compositor effect, output and viewer output function the same. */
    this->write_output(result, *this->output_);
    viewer_was_written_ = true;
  }

  void evaluate()
  {
    using namespace compositor;
    const bNodeTree &node_group = *DEG_get_evaluated<bNodeTree>(render_data_.depsgraph,
                                                                node_group_);
    NodeGroupOperation node_group_operation(*this,
                                            node_group,
                                            this->needed_outputs(),
                                            nullptr,
                                            node_group.active_viewer_key,
                                            bke::NODE_INSTANCE_KEY_BASE);
    set_output_refcount(node_group, node_group_operation);

    /* Map the inputs to the operation. */
    Vector<std::unique_ptr<Result>> inputs;
    int float_counter = 0;
    int color_counter = 0;
    for (const bNodeTreeInterfaceSocket *input_socket : node_group.interface_inputs()) {
      const bke::bNodeSocketType *typeinfo = input_socket->socket_typeinfo();
      Result *input_result = nullptr;
      if (typeinfo && typeinfo->type == SOCK_FLOAT && float_counter == 0) {
        /* First float input is factor. */
        input_result = new Result(this->create_result(ResultType::Float, ResultPrecision::Full));
        input_result->allocate_single_value();
        input_result->set_single_value(this->factor_);
        float_counter++;
      }
      else if (color_counter == 0 && this->input_1_) {
        /* First input image. */
        input_result = new Result(this->create_result(ResultType::Color, ResultPrecision::Full));
        create_result_from_input(*input_result, *this->input_1_);
        color_counter++;
      }
      else if (color_counter == 1 && this->input_2_) {
        /* Second input image. */
        input_result = new Result(this->create_result(ResultType::Color, ResultPrecision::Full));
        create_result_from_input(*input_result, *this->input_2_);
        color_counter++;
      }
      else {
        /* Unsupported sockets. */
        input_result = new Result(this->create_result(ResultType::Color, ResultPrecision::Full));
        input_result->allocate_invalid();
      }

      node_group_operation.map_input_to_result(input_socket->identifier, input_result);
      inputs.append(std::unique_ptr<Result>(input_result));
    }

    node_group_operation.evaluate();
    this->write_outputs(node_group, node_group_operation, *this->output_);
  }
};

static ImBuf *make_linear_float_buffer(ImBuf *src)
{
  if (!src) {
    return nullptr;
  }

  /* Already have scene linear float pixels, return same buffer. */
  if (is_linear_float_buffer(src)) {
    return src;
  }

  ImBuf *dst = IMB_allocImBuf(
      src->x, src->y, src->planes, IB_float_data | IB_uninitialized_pixels);
  const char *to_colorspace = IMB_colormanagement_role_colorspace_name_get(
      COLOR_ROLE_SCENE_LINEAR);
  if (src->float_buffer.data == nullptr) {
    const char *from_colorspace = IMB_colormanagement_get_rect_colorspace(src);
    IMB_colormanagement_transform_byte_to_float(dst->float_buffer.data,
                                                src->byte_buffer.data,
                                                src->x,
                                                src->y,
                                                src->channels,
                                                from_colorspace,
                                                to_colorspace);
  }
  else {
    const char *from_colorspace = IMB_colormanagement_get_float_colorspace(src);
    //@TODO: src->dst transform would be faster instead of copy + transform in-place
    memcpy(dst->float_buffer.data,
           src->float_buffer.data,
           IMB_get_pixel_count(src) * src->channels * sizeof(float));
    IMB_colormanagement_transform_float(dst->float_buffer.data,
                                        dst->x,
                                        dst->y,
                                        dst->channels,
                                        from_colorspace,
                                        to_colorspace,
                                        true);
  }
  IMB_colormanagement_assign_float_colorspace(dst, to_colorspace);
  return dst;
}

static ImBuf *do_compositor_effect(const RenderData *context,
                                   SeqRenderState * /*state*/,
                                   Strip *strip,
                                   float /*timeline_frame*/,
                                   float fac,
                                   ImBuf *src1,
                                   ImBuf *src2)
{
  const int x = context->rectx;
  const int y = context->recty;
  ImBuf *out = IMB_allocImBuf(x, y, 32, IB_float_data | IB_uninitialized_pixels);
  IMB_colormanagement_assign_float_colorspace(
      out, IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_SCENE_LINEAR));

  CompositorEffectVars *data = static_cast<CompositorEffectVars *>(strip->effectdata);
  if (!data || !data->node_group) {
    IMB_rectfill(out, float4(0, 0, 0, 1));
  }
  else {
    ImBuf *linear_src1 = make_linear_float_buffer(src1);
    ImBuf *linear_src2 = make_linear_float_buffer(src2);

    CompositorCache &com_cache = context->scene->ed->runtime->ensure_compositor_cache();
    CompositorEffectContext com_context(com_cache.get_cache_manager(),
                                        *context,
                                        data->node_group,
                                        linear_src1,
                                        linear_src2,
                                        out,
                                        fac,
                                        *strip);

    const bool use_gpu = com_context.use_gpu();
    if (use_gpu) {
      render_begin_gpu(*context);
    }
    com_cache.recreate_if_needed(
        com_context.use_gpu(), com_context.get_precision(), context->gpu_context);
    com_context.evaluate();
    com_context.cache_manager().reset();
    if (use_gpu) {
      render_end_gpu(*context);
    }

    if (linear_src1 != src1) {
      IMB_freeImBuf(linear_src1);
    }
    if (linear_src2 != src2) {
      IMB_freeImBuf(linear_src2);
    }
    seq_imbuf_to_sequencer_space(context->scene, out, true);
  }
  return out;
}

static void init_compositor_effect(Strip *strip)
{
  CompositorEffectVars *data = MEM_new<CompositorEffectVars>(__func__);
  strip->effectdata = data;
}

static void free_compositor_effect(Strip *strip, const bool /*do_id_user*/)
{
  if (strip->effectdata) {
    CompositorEffectVars *data = static_cast<CompositorEffectVars *>(strip->effectdata);
    MEM_delete(data);
    strip->effectdata = nullptr;
  }
}

static StripEarlyOut early_out_compositor(const Strip *strip, float /*fac*/)
{
  /* No inputs: compositor generates the result. */
  if (strip->input1 == nullptr) {
    return StripEarlyOut::NoInput;
  }

  /* One or two inputs: do the effect. */
  return StripEarlyOut::DoEffect;
}

void compositor_effect_get_handle(EffectHandle &rval)
{
  rval.init = init_compositor_effect;
  rval.free = free_compositor_effect;
  rval.execute = do_compositor_effect;
  rval.early_out = early_out_compositor;
}

}  // namespace blender::seq
