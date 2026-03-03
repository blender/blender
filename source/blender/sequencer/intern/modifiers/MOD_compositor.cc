/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLT_translation.hh"

#include "COM_domain.hh"
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
#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_transform.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "RNA_access.hh"

#include "cache/compositor_cache.hh"
#include "compositor.hh"
#include "modifier.hh"
#include "render.hh"

namespace blender::seq {

class CompositorModifierContext : public CompositorContext {
 private:
  const SequencerCompositorModifierData *modifier_data_;

  ImBuf *image_buffer_;
  ImBuf *mask_buffer_;
  float3x3 xform_;

 public:
  CompositorModifierContext(compositor::StaticCacheManager &cache_manager,
                            const RenderData &render_data,
                            const SequencerCompositorModifierData *modifier_data,
                            ImBuf *image_buffer,
                            ImBuf *mask_buffer,
                            const Strip &strip)
      : CompositorContext(cache_manager, render_data, strip),
        modifier_data_(modifier_data),
        image_buffer_(image_buffer),
        mask_buffer_(mask_buffer),
        xform_(float3x3::identity())
  {
    if (mask_buffer) {
      /* Note: do not use passed transform matrix since compositor coordinate
       * space is not from the image corner, but rather centered on the image. */
      xform_ = math::invert(image_transform_matrix_get(render_data.scene, &strip));
    }
  }

  compositor::Domain get_compositing_domain() const override
  {
    return compositor::Domain(int2(image_buffer_->x, image_buffer_->y));
  }

  void write_viewer(compositor::Result &viewer_result) override
  {
    using namespace compositor;

    /* Realize the transforms if needed. */
    const InputDescriptor input_descriptor = {ResultType::Color,
                                              InputRealizationMode::OperationDomain};
    SimpleOperation *realization_operation = RealizeOnDomainOperation::construct_if_needed(
        *this, viewer_result, input_descriptor, viewer_result.domain());

    if (realization_operation) {
      Result realize_input = this->create_result(ResultType::Color, viewer_result.precision());
      realize_input.wrap_external(viewer_result);
      realization_operation->map_input_to_result(&realize_input);
      realization_operation->evaluate();

      Result &realized_viewer_result = realization_operation->get_result();
      this->write_output(realized_viewer_result, *image_buffer_);
      realized_viewer_result.release();
      viewer_was_written_ = true;
      delete realization_operation;
      return;
    }

    this->write_output(viewer_result, *image_buffer_);
    viewer_was_written_ = true;
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
    set_output_refcount(node_group, node_group_operation);

    /* Map the inputs to the operation. */
    Vector<std::unique_ptr<Result>> inputs;
    for (const bNodeTreeInterfaceSocket *input_socket : node_group.interface_inputs()) {
      Result *input_result = new Result(
          this->create_result(ResultType::Color, ResultPrecision::Full));
      if (input_socket == node_group.interface_inputs()[0]) {
        /* First socket is the image input. */
        create_result_from_input(*input_result, *image_buffer_);
      }
      else if (mask_buffer_ && input_socket == node_group.interface_inputs()[1]) {
        /* Second socket is the mask input. */
        create_result_from_input(*input_result, *mask_buffer_);
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
    this->write_outputs(node_group, node_group_operation, *this->image_buffer_);
  }
};

static void compositor_modifier_init_data(StripModifierData *strip_modifier_data)
{
  SequencerCompositorModifierData *modifier_data =
      reinterpret_cast<SequencerCompositorModifierData *>(strip_modifier_data);
  modifier_data->node_group = nullptr;
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

  CompositorCache &com_cache = context.render_data.scene->ed->runtime->ensure_compositor_cache();
  CompositorModifierContext com_mod_context(com_cache.get_cache_manager(),
                                            context.render_data,
                                            modifier_data,
                                            context.image,
                                            linear_mask,
                                            context.strip);

  const bool use_gpu = com_mod_context.use_gpu();
  if (use_gpu) {
    render_begin_gpu(context.render_data);
  }

  com_cache.recreate_if_needed(
      com_mod_context.use_gpu(), com_mod_context.get_precision(), context.render_data.gpu_context);
  com_mod_context.evaluate();
  com_mod_context.cache_manager().reset();
  if (use_gpu) {
    render_end_gpu(context.render_data);
  }

  context.result_translation += com_mod_context.get_result_translation();

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
