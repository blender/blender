/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLT_translation.hh"

#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_node_group_operation.hh"
#include "COM_realize_on_domain_operation.hh"
#include "COM_result.hh"

#include "DNA_node_types.h"
#include "DNA_sequence_types.h"

#include "BKE_context.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "DEG_depsgraph_query.hh"

#include "IMB_colormanagement.hh"

#include "SEQ_modifier.hh"
#include "SEQ_modifiertypes.hh"
#include "SEQ_render.hh"
#include "SEQ_select.hh"
#include "SEQ_transform.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "RNA_access.hh"

#include "modifier.hh"
#include "render.hh"

namespace blender::seq {

class CompositorContext : public compositor::Context {
 private:
  const RenderData &render_data_;
  const SequencerCompositorModifierData *modifier_data_;

  ImBuf *image_buffer_;
  ImBuf *mask_buffer_;
  float3x3 xform_;
  float2 result_translation_ = float2(0, 0);
  const Strip *strip_;

  /* Identified if the output of the viewer was written. */
  bool viewer_was_written_ = false;

 public:
  CompositorContext(compositor::StaticCacheManager &cache_manager,
                    const RenderData &render_data,
                    const SequencerCompositorModifierData *modifier_data,
                    ImBuf *image_buffer,
                    ImBuf *mask_buffer,
                    const Strip &strip)
      : compositor::Context(cache_manager),
        render_data_(render_data),
        modifier_data_(modifier_data),
        image_buffer_(image_buffer),
        mask_buffer_(mask_buffer),
        xform_(float3x3::identity()),
        strip_(&strip)
  {
    if (mask_buffer) {
      /* Note: do not use passed transform matrix since compositor coordinate
       * space is not from the image corner, but rather centered on the image. */
      xform_ = math::invert(image_transform_matrix_get(render_data.scene, &strip));
    }
  }

  float2 get_result_translation() const
  {
    return result_translation_;
  }

  const Scene &get_scene() const override
  {
    return *render_data_.scene;
  }

  bool treat_viewer_as_group_output() const override
  {
    return true;
  }

  bool use_compositing_domain_for_input_output() const override
  {
    return false;
  }

  compositor::Domain get_compositing_domain() const override
  {
    return compositor::Domain(int2(image_buffer_->x, image_buffer_->y));
  }

  void write_output(const compositor::Result &result)
  {
    /* Do not write the output if the viewer output was already written. */
    if (viewer_was_written_) {
      return;
    }

    if (result.is_single_value()) {
      IMB_rectfill(image_buffer_, result.get_single_value<compositor::Color>());
      return;
    }

    result_translation_ = result.domain().transformation.location();
    const int2 size = result.domain().data_size;
    if (size != int2(image_buffer_->x, image_buffer_->y)) {
      /* Output size is different (e.g. image is blurred with expanded bounds);
       * need to allocate appropriately sized buffer. */
      IMB_free_all_data(image_buffer_);
      image_buffer_->x = size.x;
      image_buffer_->y = size.y;
      IMB_alloc_float_pixels(image_buffer_, 4, false);
    }
    std::memcpy(image_buffer_->float_buffer.data,
                result.cpu_data().data(),
                sizeof(float) * 4 * size.x * size.y);
  }

  void write_viewer(const compositor::Result &result) override
  {
    /* Within compositor modifier, output and viewer output function the same. */
    this->write_output(result);
    viewer_was_written_ = true;
  }

  const Strip *get_strip() const override
  {
    return strip_;
  }

  bool use_gpu() const override
  {
    return false;
  }

  compositor::NodeGroupOutputTypes needed_outputs() const
  {
    compositor::NodeGroupOutputTypes needed_outputs =
        compositor::NodeGroupOutputTypes::GroupOutputNode;
    if (!render_data_.render) {
      needed_outputs |= compositor::NodeGroupOutputTypes::ViewerNode;
    }
    return needed_outputs;
  }

  void evaluate()
  {
    using namespace compositor;
    const bNodeTree &node_group = *DEG_get_evaluated<bNodeTree>(render_data_.depsgraph,
                                                                modifier_data_->node_group);
    NodeGroupOperation node_group_operation(*this,
                                            node_group,
                                            this->needed_outputs(),
                                            nullptr,
                                            node_group.active_viewer_key,
                                            bke::NODE_INSTANCE_KEY_BASE);

    /* Set the reference count for the outputs, only the first color output is actually needed,
     * while the rest are ignored. */
    node_group.ensure_interface_cache();
    for (const bNodeTreeInterfaceSocket *output_socket : node_group.interface_outputs()) {
      const bool is_fisrt_output = output_socket == node_group.interface_outputs().first();
      Result &output_result = node_group_operation.get_result(output_socket->identifier);
      const bool is_color = output_result.type() == ResultType::Color;
      output_result.set_reference_count(is_fisrt_output && is_color ? 1 : 0);
    }

    /* Map the inputs to the operation. */
    Vector<std::unique_ptr<Result>> inputs;
    for (const bNodeTreeInterfaceSocket *input_socket : node_group.interface_inputs()) {
      Result *input_result = new Result(
          this->create_result(ResultType::Color, ResultPrecision::Full));
      if (input_socket == node_group.interface_inputs()[0]) {
        /* First socket is the image input. */
        input_result->wrap_external(image_buffer_->float_buffer.data,
                                    int2(image_buffer_->x, image_buffer_->y));
      }
      else if (mask_buffer_ && input_socket == node_group.interface_inputs()[1]) {
        /* Second socket is the mask input. */
        input_result->wrap_external(mask_buffer_->float_buffer.data,
                                    int2(mask_buffer_->x, mask_buffer_->y));
        input_result->set_transformation(xform_);
      }
      else {
        /* The rest of the sockets are not supported. */
        input_result->allocate_invalid();
      }

      node_group_operation.map_input_to_result(input_socket->identifier, input_result);
      inputs.append(std::unique_ptr<Result>(input_result));
    }

    node_group_operation.evaluate();

    /* Write the outputs of the operation. */
    for (const bNodeTreeInterfaceSocket *output_socket : node_group.interface_outputs()) {
      Result &output_result = node_group_operation.get_result(output_socket->identifier);
      if (!output_result.should_compute()) {
        continue;
      }

      /* Realize the output transforms if needed. */
      const InputDescriptor input_descriptor = {ResultType::Color,
                                                InputRealizationMode::OperationDomain};
      SimpleOperation *realization_operation = RealizeOnDomainOperation::construct_if_needed(
          *this, output_result, input_descriptor, output_result.domain());
      if (realization_operation) {
        realization_operation->map_input_to_result(&output_result);
        realization_operation->evaluate();
        Result &realized_output_result = realization_operation->get_result();
        this->write_output(realized_output_result);
        realized_output_result.release();
        delete realization_operation;
        continue;
      }

      this->write_output(output_result);
      output_result.release();
    }
  }
};

static void compositor_modifier_init_data(StripModifierData *strip_modifier_data)
{
  SequencerCompositorModifierData *modifier_data =
      reinterpret_cast<SequencerCompositorModifierData *>(strip_modifier_data);
  modifier_data->node_group = nullptr;
}

static bool is_linear_float_buffer(ImBuf *image_buffer)
{
  return image_buffer->float_buffer.data &&
         IMB_colormanagement_space_is_scene_linear(image_buffer->float_buffer.colorspace);
}

static bool ensure_linear_float_buffer(ImBuf *ibuf)
{
  if (!ibuf) {
    return false;
  }

  /* Already have scene linear float pixels, nothing to do. */
  if (is_linear_float_buffer(ibuf)) {
    return true;
  }

  if (ibuf->float_buffer.data == nullptr) {
    IMB_float_from_byte(ibuf);
  }
  else {
    const char *from_colorspace = IMB_colormanagement_get_float_colorspace(ibuf);
    const char *to_colorspace = IMB_colormanagement_role_colorspace_name_get(
        COLOR_ROLE_SCENE_LINEAR);
    IMB_colormanagement_transform_float(ibuf->float_buffer.data,
                                        ibuf->x,
                                        ibuf->y,
                                        ibuf->channels,
                                        from_colorspace,
                                        to_colorspace,
                                        true);
    IMB_colormanagement_assign_float_colorspace(ibuf, to_colorspace);
  }
  return false;
}

static void compositor_modifier_apply(ModifierApplyContext &context,
                                      StripModifierData *strip_modifier_data,
                                      ImBuf *mask)
{
  const SequencerCompositorModifierData *modifier_data =
      reinterpret_cast<SequencerCompositorModifierData *>(strip_modifier_data);
  if (!modifier_data->node_group) {
    return;
  }

  ImBuf *linear_mask = mask;
  if (mask && !is_linear_float_buffer(mask)) {
    linear_mask = IMB_dupImBuf(mask);
    ensure_linear_float_buffer(linear_mask);
  }

  const bool was_float_linear = ensure_linear_float_buffer(context.image);
  const bool was_byte = context.image->float_buffer.data == nullptr;

  /* TODO: Should be persistent across evaluations. */
  compositor::StaticCacheManager cache_manager;

  CompositorContext com_context(cache_manager,
                                context.render_data,
                                modifier_data,
                                context.image,
                                linear_mask,
                                context.strip);
  com_context.evaluate();
  com_context.cache_manager().reset();

  context.result_translation += com_context.get_result_translation();

  if (mask != linear_mask) {
    IMB_freeImBuf(linear_mask);
  }

  if (was_float_linear) {
    return;
  }

  if (was_byte) {
    IMB_byte_from_float(context.image);
    IMB_free_float_pixels(context.image);
  }
  else {
    seq_imbuf_to_sequencer_space(context.render_data.scene, context.image, true);
  }
}

static void compositor_modifier_panel_draw(const bContext *C, Panel *panel)
{
  ui::Layout &layout = *panel->layout;
  PointerRNA *ptr = ui::panel_custom_data_get(panel);

  layout.use_property_split_set(true);

  Scene *scene = CTX_data_sequencer_scene(C);
  Strip *strip = seq::select_active_get(scene);
  bool has_existing_group = false;
  if (strip != nullptr) {
    StripModifierData *smd = seq::modifier_get_active(strip);

    if (smd && smd->type == eSeqModifierType_Compositor) {
      SequencerCompositorModifierData *nmd = reinterpret_cast<SequencerCompositorModifierData *>(
          smd);
      if (nmd->node_group != nullptr) {
        template_id(&layout,
                    C,
                    ptr,
                    "node_group",
                    "NODE_OT_duplicate_compositing_modifier_node_group",
                    nullptr,
                    nullptr);
        has_existing_group = true;
      }
    }
  }

  if (!has_existing_group) {
    template_id(&layout,
                C,
                ptr,
                "node_group",
                "NODE_OT_new_compositor_sequencer_node_group",
                nullptr,
                nullptr);
  }

  if (ui::Layout *mask_input_layout = layout.panel_prop(
          C, ptr, "open_mask_input_panel", IFACE_("Mask Input")))
  {
    draw_mask_input_type_settings(C, *mask_input_layout, ptr);
  }
}

static void compositor_modifier_register(ARegionType *region_type)
{
  modifier_panel_register(
      region_type, eSeqModifierType_Compositor, compositor_modifier_panel_draw);
}

StripModifierTypeInfo seqModifierType_Compositor = {
    /*idname*/ "Compositor",
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Compositor"),
    /*struct_name*/ "SequencerCompositorModifierData",
    /*struct_size*/ sizeof(SequencerCompositorModifierData),
    /*init_data*/ compositor_modifier_init_data,
    /*free_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*apply*/ compositor_modifier_apply,
    /*panel_register*/ compositor_modifier_register,
};

};  // namespace blender::seq
