/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLI_resource_scope.hh"

#include "BKE_compositor.hh"
#include "BKE_idprop.hh"
#include "BKE_node_runtime.hh"

#include "COM_domain.hh"
#include "COM_utilities.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_sequence_types.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"

#include "NOD_socket_usage_inference.hh"

#include "PRF_profile.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "SEQ_effects.hh"
#include "SEQ_sequencer.hh"

#include "cache/compositor_cache.hh"
#include "compositor.hh"
#include "effects.hh"

namespace blender::seq {

class CompositorEffectContext : public CompositorContext {
  bNodeTree *node_group_;

  ImBuf *input_1_;
  ImBuf *input_2_;
  ImBuf *output_;
  float factor_;

  /* The hash of the active compute context. */
  const ComputeContextHash active_compute_context_hash_;

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
        factor_(factor),
        active_compute_context_hash_(bke::compositor::compute_active_compute_context_hash(
            *render_data_.scene, *node_group_))
  {
  }

  const ComputeContextHash &get_active_compute_context_hash() const override
  {
    return active_compute_context_hash_;
  }

  compositor::Domain get_compositing_domain() const override
  {
    return compositor::Domain(int2(this->output_->x, this->output_->y));
  }

  void write_viewer(compositor::Result &viewer_result) override
  {
    write_viewer_impl(viewer_result, *this->output_);
  }

  void evaluate()
  {
    using namespace compositor;
    const bNodeTree &node_group = *DEG_get_evaluated<bNodeTree>(render_data_.depsgraph,
                                                                node_group_);
    const bke::DataBlockComputeContext compute_context(nullptr, this->get_scene().id);
    NodeGroupOperation node_group_operation(
        *this, node_group, this->needed_outputs(), compute_context);
    set_output_refcount(node_group, node_group_operation);

    node_group.ensure_topology_cache();
    PointerRNA strip_ptr = RNA_pointer_create_discrete(
        &render_data_.scene->id, RNA_CompositorStrip, const_cast<Strip *>(strip_));
    PointerRNA properties_ptr = RNA_pointer_get(&strip_ptr, "properties");
    PointerRNA inputs_ptr = RNA_pointer_get(&properties_ptr, "inputs");

    /* Map the inputs to the operation. */
    Vector<std::unique_ptr<Result>> inputs;
    int float_counter = 0;
    int color_counter = 0;
    for (const bNodeTreeInterfaceSocket *input_socket : node_group.interface_inputs()) {
      bke::bNodeSocketType *typeinfo = input_socket->socket_typeinfo();
      const eNodeSocketDatatype socket_type = typeinfo ? typeinfo->type : SOCK_CUSTOM;
      const bool valid_socket_type = typeinfo && node_group.typeinfo->valid_socket_type(
                                                     node_group.typeinfo, typeinfo);
      /* Fallback to ResultType::Float for invalid inputs. */
      const ResultType result_type = valid_socket_type ?
                                         compositor::get_node_interface_socket_result_type(
                                             *input_socket) :
                                         ResultType::Float;
      Result *input_result = new Result(this->create_result(result_type, ResultPrecision::Full));
      if (socket_type == SOCK_FLOAT && float_counter == 0) {
        /* First float input is the effect fader factor. */
        input_result->allocate_single_value();
        input_result->set_single_value(this->factor_);
        float_counter++;
      }
      else if (socket_type == SOCK_RGBA && color_counter == 0 && this->input_1_) {
        /* First color input is the first image. */
        create_result_from_input(*input_result, *this->input_1_);
        color_counter++;
      }
      else if (socket_type == SOCK_RGBA && color_counter == 1 && this->input_2_) {
        /* Second color input is the second image. */
        create_result_from_input(*input_result, *this->input_2_);
        color_counter++;
      }
      else if (valid_socket_type && inputs_ptr.data != nullptr) {
        /* Remaining inputs read their value from the exposed RNA property. */
        set_input_result_from_rna(inputs_ptr, *input_socket, socket_type, *input_result);
      }
      else {
        /* Unsupported sockets. */
        input_result->allocate_invalid();
      }

      node_group_operation.map_input_to_result(input_socket->identifier, input_result);
      inputs.append(std::unique_ptr<Result>(input_result));
    }

    node_group_operation.evaluate();
    this->write_outputs(node_group, node_group_operation, *this->output_);
  }
};

static SeqResult do_compositor_effect(const RenderData *context,
                                      SeqRenderState * /*state*/,
                                      Strip *strip,
                                      float /*timeline_frame*/,
                                      float fac,
                                      const SeqResult &src1,
                                      const SeqResult &src2)
{
  PRF_scope_with_name("SeqFxCompositor", ProfileCategory::Draw);
  const int x = context->rectx;
  const int y = context->recty;
  SeqResult out;
  out.image = IMB_allocImBuf(x, y, ImBufFlags::FloatData | ImBufFlags::UninitializedPixels);
  IMB_colormanagement_assign_float_colorspace(
      out.image, IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_SCENE_LINEAR));

  CompositorEffectVars *data = static_cast<CompositorEffectVars *>(strip->effectdata);
  if (!data || !data->node_group) {
    IMB_rectfill(out.image, float4(0, 0, 0, 1));
    out.image->color_mode = ImColorMode::RGB;
    out.is_opaque_before_transform = true;
  }
  else {
    CompositorCache &com_cache = context->scene->ed->runtime->ensure_compositor_cache();
    CompositorEffectContext com_context(com_cache.get_cache_manager(),
                                        *context,
                                        data->node_group,
                                        src1.image,
                                        src2.image,
                                        out.image,
                                        fac,
                                        *strip);

    if (com_context.use_gpu()) {
      com_context.set_gpu_supported(render_begin_gpu(*context));
    }
    com_cache.recreate_if_needed(
        com_context.use_gpu(), com_context.get_precision(), context->gpu_context);
    com_context.evaluate();
    com_context.cache_manager().reset();
    if (com_context.use_gpu()) {
      render_end_gpu(*context);
    }
    out.translation += com_context.get_result_translation();
    out.is_opaque_before_transform = !out.image->can_contain_alpha();
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
    if (data->system_properties != nullptr) {
      IDP_FreeProperty_ex(data->system_properties, false);
    }
    MEM_delete(data);
    strip->effectdata = nullptr;
  }
}

static void copy_compositor_effect(Strip *dst, const Strip *src, const int flag)
{
  CompositorEffectVars *dst_data = MEM_new<CompositorEffectVars>(__func__);
  const auto *src_data = static_cast<const CompositorEffectVars *>(src->effectdata);
  *dst_data = *src_data;
  dst_data->system_properties = nullptr;
  if (src_data->system_properties != nullptr) {
    dst_data->system_properties = IDP_CopyProperty_ex(src_data->system_properties, flag);
  }
  dst->effectdata = dst_data;
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

void compositor_effect_nodes_update_interface(Main &bmain, Scene &sequencer_scene, Strip &strip)
{
  if (strip.type != STRIP_TYPE_COMPOSITOR || strip.effectdata == nullptr) {
    return;
  }
  CompositorEffectVars *comp = static_cast<CompositorEffectVars *>(strip.effectdata);
  if (!comp->system_properties) {
    comp->system_properties =
        bke::idprop::create_group("SequencerCompositorEffectProperties").release();
  }
  PointerRNA properties_ptr = RNA_pointer_create_discrete(
      &sequencer_scene.id, RNA_SequencerCompositorEffectProperties, comp);
  RNA_ensure_and_sync_system_properties(bmain, properties_ptr, *comp->system_properties);

  DEG_id_tag_update(&sequencer_scene.id, ID_RECALC_SEQUENCER_STRIPS);
}

void compositor_effect_nodes_input_usages(const Scene &sequencer_scene,
                                          Strip &strip,
                                          Vector<bool> &r_used,
                                          Vector<bool> &r_visible)
{
  r_used.clear();
  r_visible.clear();
  if (strip.type != STRIP_TYPE_COMPOSITOR || strip.effectdata == nullptr) {
    return;
  }
  CompositorEffectVars *comp = static_cast<CompositorEffectVars *>(strip.effectdata);
  if (comp->node_group == nullptr || ID_MISSING(comp->node_group)) {
    return;
  }
  bNodeTree &tree = *comp->node_group;
  tree.ensure_interface_cache();
  const int inputs_num = tree.interface_inputs().size();
  if (inputs_num == 0) {
    return;
  }

  /* Get to the generated properties interface struct. */
  PointerRNA strip_ptr = RNA_pointer_create_discrete(
      &const_cast<Scene &>(sequencer_scene).id, RNA_CompositorStrip, &strip);
  PointerRNA properties_ptr = RNA_pointer_get(&strip_ptr, "properties");
  if (properties_ptr.data == nullptr) {
    return;
  }

  ResourceScope scope;
  Vector<nodes::InferenceValue> input_values = nodes::get_geometry_nodes_input_inference_values(
      tree, properties_ptr, scope);

  /* Effect fader and first N color inputs are supplied by the strips during evaluation, so
   * treat them as unknown. Otherwise an input with usage conditional on the fader (e.g. mixed
   * with the fader as a factor) might wrongly inferred as unused. */
  const int images_num = strip.effect_num_inputs_get();
  int float_counter = 0;
  int color_counter = 0;
  for (const int i : IndexRange(inputs_num)) {
    const bNodeTreeInterfaceSocket &socket = *tree.interface_inputs()[i];
    const bke::bNodeSocketType *typeinfo = socket.socket_typeinfo();
    const eNodeSocketDatatype socket_type = typeinfo ? typeinfo->type : SOCK_CUSTOM;
    if (socket_type == SOCK_FLOAT && float_counter == 0) {
      input_values[i] = nodes::InferenceValue::Unknown();
      float_counter++;
    }
    else if (socket_type == SOCK_RGBA && color_counter < images_num) {
      input_values[i] = nodes::InferenceValue::Unknown();
      color_counter++;
    }
  }

  Array<nodes::socket_usage_inference::SocketUsage> usages(inputs_num);
  nodes::socket_usage_inference::infer_group_interface_usage(tree, input_values, usages);
  r_used.resize(inputs_num);
  r_visible.resize(inputs_num);
  for (const int i : IndexRange(inputs_num)) {
    r_used[i] = usages[i].is_used;
    r_visible[i] = usages[i].is_visible;
  }
}

void compositor_effect_get_handle(EffectHandle &rval)
{
  rval.init = init_compositor_effect;
  rval.free = free_compositor_effect;
  rval.copy = copy_compositor_effect;
  rval.execute = do_compositor_effect;
  rval.early_out = early_out_compositor;
}

}  // namespace blender::seq
