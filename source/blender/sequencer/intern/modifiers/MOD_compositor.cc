/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLI_math_base.h"
#include "BLI_rect.h"

#include "BLT_translation.hh"

#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_evaluator.hh"

#include "DNA_sequence_types.h"

#include "DEG_depsgraph_query.hh"

#include "IMB_colormanagement.hh"

#include "SEQ_modifier.hh"
#include "SEQ_modifiertypes.hh"
#include "SEQ_render.hh"
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

 public:
  CompositorContext(const RenderData &render_data,
                    const SequencerCompositorModifierData *modifier_data,
                    ImBuf *image_buffer,
                    ImBuf *mask_buffer,
                    const Strip &strip)
      : compositor::Context(),
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

  const bNodeTree &get_node_tree() const override
  {
    return *DEG_get_evaluated<bNodeTree>(render_data_.depsgraph, modifier_data_->node_group);
  }

  compositor::OutputTypes needed_outputs() const override
  {
    compositor::OutputTypes needed_outputs = compositor::OutputTypes::Composite;
    if (!render_data_.for_render) {
      needed_outputs |= compositor::OutputTypes::Viewer;
    }
    return needed_outputs;
  }

  bool treat_viewer_as_compositor_output() const override
  {
    return true;
  }

  bool use_context_bounds_for_input_output() const override
  {
    return false;
  }

  Bounds<int2> get_compositing_region() const override
  {
    return Bounds<int2>(int2(0), int2(image_buffer_->x, image_buffer_->y));
  }

  compositor::Result get_output(compositor::Domain domain) override
  {
    result_translation_ = domain.transformation.location();
    compositor::Result result = this->create_result(compositor::ResultType::Color);
    if (domain.size.x != image_buffer_->x || domain.size.y != image_buffer_->y) {
      /* Output size is different (e.g. image is blurred with expanded bounds);
       * need to allocate appropriately sized buffer. */
      IMB_free_all_data(image_buffer_);
      image_buffer_->x = domain.size.x;
      image_buffer_->y = domain.size.y;
      IMB_alloc_float_pixels(image_buffer_, 4, false);
    }
    result.wrap_external(image_buffer_->float_buffer.data,
                         int2(image_buffer_->x, image_buffer_->y));
    return result;
  }

  compositor::Result get_viewer_output(compositor::Domain domain,
                                       bool /*is_data*/,
                                       compositor::ResultPrecision /*precision*/) override
  {
    /* Within compositor modifier, output and viewer output function the same. */
    return get_output(domain);
  }

  compositor::Result get_input(StringRef name) override
  {
    compositor::Result result = this->create_result(compositor::ResultType::Color);

    if (name == "Image") {
      result.wrap_external(image_buffer_->float_buffer.data,
                           int2(image_buffer_->x, image_buffer_->y));
    }
    else if (name == "Mask" && mask_buffer_) {
      result.wrap_external(mask_buffer_->float_buffer.data,
                           int2(mask_buffer_->x, mask_buffer_->y));
      result.set_transformation(xform_);
    }

    return result;
  }

  const Strip *get_strip() const override
  {
    return strip_;
  }

  bool use_gpu() const override
  {
    return false;
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

  CompositorContext com_context(
      context.render_data, modifier_data, context.image, linear_mask, context.strip);
  compositor::Evaluator evaluator(com_context);
  evaluator.evaluate();

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
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = UI_panel_custom_data_get(panel);

  layout->use_property_split_set(true);

  uiTemplateID(layout,
               C,
               ptr,
               "node_group",
               "NODE_OT_new_compositor_sequencer_node_group",
               nullptr,
               nullptr);

  if (uiLayout *mask_input_layout = layout->panel_prop(
          C, ptr, "open_mask_input_panel", IFACE_("Mask Input")))
  {
    draw_mask_input_type_settings(C, mask_input_layout, ptr);
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
