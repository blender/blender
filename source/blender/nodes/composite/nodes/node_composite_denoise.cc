/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __APPLE__
#  include "BLI_system.h"
#endif

#include "MEM_guardedalloc.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "DNA_node_types.h"

#include "COM_denoised_auxiliary_pass.hh"
#include "COM_derived_resources.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"
#include "COM_utilities_oidn.hh"

#include "node_composite_util.hh"

#ifdef WITH_OPENIMAGEDENOISE
#  include <OpenImageDenoise/oidn.hpp>
#endif

namespace blender::nodes::node_composite_denoise_cc {

static const EnumPropertyItem prefilter_items[] = {
    {CMP_NODE_DENOISE_PREFILTER_NONE,
     "NONE",
     0,
     N_("None"),
     N_("No prefiltering, use when guiding passes are noise-free")},
    {CMP_NODE_DENOISE_PREFILTER_FAST,
     "FAST",
     0,
     N_("Fast"),
     N_("Denoise image and guiding passes together. Improves quality when guiding passes are "
        "noisy using least amount of extra processing time.")},
    {CMP_NODE_DENOISE_PREFILTER_ACCURATE,
     "ACCURATE",
     0,
     N_("Accurate"),
     N_("Prefilter noisy guiding passes before denoising image. Improves quality when guiding "
        "passes are noisy using extra processing time.")},
    {0, nullptr, 0, nullptr, nullptr}};

static const EnumPropertyItem quality_items[] = {
    {CMP_NODE_DENOISE_QUALITY_SCENE,
     "FOLLOW_SCENE",
     0,
     N_("Follow Scene"),
     N_("Use the scene's denoising quality setting")},
    {CMP_NODE_DENOISE_QUALITY_HIGH, "HIGH", 0, "High", "High quality"},
    {CMP_NODE_DENOISE_QUALITY_BALANCED,
     "BALANCED",
     0,
     N_("Balanced"),
     N_("Balanced between performance and quality")},
    {CMP_NODE_DENOISE_QUALITY_FAST, "FAST", 0, "Fast", "High performance"},
    {0, nullptr, 0, nullptr, nullptr}};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Color>("Image"_ustr)
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .hide_value()
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image"_ustr)
      .structure_type(StructureType::Dynamic)
      .align_with_previous();

  b.add_input<decl::Color>("Albedo"_ustr)
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .hide_value()
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Vector>("Normal"_ustr)
      .default_value({0.0f, 0.0f, 0.0f})
      .min(-1.0f)
      .max(1.0f)
      .hide_value()
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Bool>("HDR"_ustr).default_value(true);
  b.add_input<decl::Menu>("Prefilter"_ustr)
      .default_value(CMP_NODE_DENOISE_PREFILTER_ACCURATE)
      .static_items(prefilter_items)
      .optional_label();
  b.add_input<decl::Menu>("Quality"_ustr)
      .default_value(CMP_NODE_DENOISE_QUALITY_SCENE)
      .static_items(quality_items)
      .optional_label();
}

static void node_init(bNodeTree * /*ntree*/, bNode *node)
{
  /* Unused, kept for forward compatibility. */
  NodeDenoise *ndg = MEM_new<NodeDenoise>(__func__);
  node->storage = ndg;
}

static bool is_oidn_supported()
{
#ifdef WITH_OPENIMAGEDENOISE
#  if defined(__APPLE__)
  /* Always supported through Accelerate framework BNNS. */
  return true;
#  elif defined(__aarch64__) || defined(_M_ARM64)
  /* OIDN 2.2 and up supports ARM64 on Windows and Linux. */
  return true;
#  else
  return BLI_cpu_support_sse42();
#  endif
#else
  return false;
#endif
}

static void node_draw_buttons(ui::Layout &layout, bContext * /*C*/, PointerRNA * /*ptr*/)
{
#ifndef WITH_OPENIMAGEDENOISE
  layout.label(RPT_("Disabled. Built without OpenImageDenoise"), ICON_ERROR);
#else
  if (!is_oidn_supported()) {
    layout.label(RPT_("Disabled. Platform not supported"), ICON_ERROR);
  }
#endif
}

using namespace blender::compositor;

/* A callback to cancel the filter operations by evaluating the context's is_canceled method. The
 * API specifies that true indicates the filter should continue, while false indicates it should
 * stop, so invert the condition. This callback can also be used to track progress using the given
 * n argument, but we currently don't make use of it. See OIDNProgressMonitorFunction in the API
 * for more information. */
[[maybe_unused]] static bool oidn_progress_monitor_function(void *user_ptr, double /*n*/)
{
  const Context *context = static_cast<const Context *>(user_ptr);
  return !context->is_canceled();
}

class DenoiseOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input_image = this->get_input("Image");
    Result &output_image = this->get_result("Image");

    if (!is_oidn_supported() || input_image.is_single_value()) {
      output_image.share_data(input_image);
      return;
    }

#ifdef WITH_OPENIMAGEDENOISE
    oidn::DeviceRef device = create_oidn_device(this->context());
    device.set("setAffinity", false);
    device.commit();

    /* For GPU, we download the input into a temporary result and mark it to be freed later, and
     * since it is temporary, we also perform denoising in-place. */
    Result denoise_input = this->context().create_result(input_image.type());
    Result *denoise_output = nullptr;
    if (this->context().use_gpu()) {
      Result input_image_cpu = input_image.download_to_cpu();
      denoise_input.share_data(input_image_cpu);
      input_image_cpu.release();

      denoise_output = &denoise_input;
    }
    else {
      denoise_input.share_data(input_image);

      output_image.allocate_texture(input_image.domain());
      denoise_output = &output_image;
    }

    oidn::BufferRef input_buffer = create_oidn_buffer(device, denoise_input);
    oidn::BufferRef output_buffer = create_oidn_buffer(device, *denoise_output);

    oidn::FilterRef filter = device.newFilter("RT");
    const int2 size = input_image.domain().data_size;
    const int pixel_stride = sizeof(float) * denoise_input.channels_count();
    filter.setImage("color", input_buffer, oidn::Format::Float3, size.x, size.y, 0, pixel_stride);
    filter.setImage(
        "output", output_buffer, oidn::Format::Float3, size.x, size.y, 0, pixel_stride);
    filter.set("hdr", use_hdr());
    filter.set("cleanAux", auxiliary_passes_are_clean());
    this->set_filter_quality(filter);
    filter.setProgressMonitorFunction(oidn_progress_monitor_function, &context());

    /* If the albedo input is not a single value input, set it to the albedo input of the filter,
     * denoising it if needed. */
    Result &input_albedo = this->get_input("Albedo");
    Result denoise_albedo = this->context().create_result(input_albedo.type());
    if (!input_albedo.is_single_value()) {
      if (this->should_denoise_auxiliary_passes()) {
        denoise_albedo.share_data(input_albedo.derived_resources()
                                      .denoised_auxiliary_passes
                                      .get(this->context(),
                                           input_albedo,
                                           DenoisedAuxiliaryPassType::Albedo,
                                           this->get_quality())
                                      .result);
      }
      else {
        if (this->context().use_gpu()) {
          Result input_albedo_cpu = input_albedo.download_to_cpu();
          denoise_albedo.share_data(input_albedo_cpu);
          input_albedo_cpu.release();
        }
        else {
          denoise_albedo.share_data(input_albedo);
        }
      }

      oidn::BufferRef albedo_buffer = create_oidn_buffer(device, denoise_albedo);
      const int pixel_stride = sizeof(float) * denoise_albedo.channels_count();
      filter.setImage(
          "albedo", albedo_buffer, oidn::Format::Float3, size.x, size.y, 0, pixel_stride);
    }

    /* If the albedo and normal inputs are not single value inputs, set the normal input to the
     * albedo input of the filter, denoising it if needed. Notice that we also consider the albedo
     * input because OIDN doesn't support denoising with only the normal auxiliary pass. */
    Result &input_normal = this->get_input("Normal");
    Result denoise_normal = this->context().create_result(input_normal.type());
    if (!input_albedo.is_single_value() && !input_normal.is_single_value()) {
      if (should_denoise_auxiliary_passes()) {
        denoise_normal.share_data(input_normal.derived_resources()
                                      .denoised_auxiliary_passes
                                      .get(this->context(),
                                           input_normal,
                                           DenoisedAuxiliaryPassType::Normal,
                                           this->get_quality())
                                      .result);
      }
      else {
        if (this->context().use_gpu()) {
          Result input_normal_cpu = input_normal.download_to_cpu();
          denoise_normal.share_data(input_normal_cpu);
          input_normal_cpu.release();
        }
        else {
          denoise_normal.share_data(input_normal);
        }
      }

      oidn::BufferRef normal_buffer = create_oidn_buffer(device, denoise_normal);
      const int pixel_stride = sizeof(float) * denoise_normal.channels_count();
      filter.setImage(
          "normal", normal_buffer, oidn::Format::Float3, size.x, size.y, 0, pixel_stride);
    }

    filter.commit();
    filter.execute();

    if (output_buffer.getStorage() != oidn::Storage::Host) {
      output_buffer.read(
          0, denoise_output->size_in_bytes(), denoise_output->cpu_data_for_write().data());
    }

    if (this->context().use_gpu()) {
      Result output_gpu = denoise_output->upload_to_gpu(true);
      output_image.share_data(output_gpu);
      output_gpu.release();
    }
    else {
      /* OIDN already wrote to the output directly, however, OIDN skips the alpha channel, so we
       * need to restore it. */
      parallel_for(size, [&](const int2 texel) {
        const float alpha = input_image.load_pixel<Color>(texel).a;
        output_image.store_pixel(
            texel, Color(float4(float4(output_image.load_pixel<Color>(texel)).xyz(), alpha)));
      });
    }

    denoise_input.release();
    denoise_albedo.release();
    denoise_normal.release();
#endif
  }

  /* If the pre-filter mode is set to CMP_NODE_DENOISE_PREFILTER_NONE, that it means the supplied
   * auxiliary passes are already noise-free, if it is set to CMP_NODE_DENOISE_PREFILTER_ACCURATE,
   * the auxiliary passes will be denoised before denoising the main image, so in both cases, the
   * auxiliary passes are considered clean. If it is set to CMP_NODE_DENOISE_PREFILTER_FAST on the
   * other hand, the auxiliary passes are assumed to be noisy and are thus not clean, and will be
   * denoised while denoising the main image. */
  bool auxiliary_passes_are_clean()
  {
    return get_prefilter_mode() != CMP_NODE_DENOISE_PREFILTER_FAST;
  }

  /* Returns whether the auxiliary passes should be denoised, see the auxiliary_passes_are_clean
   * method for more information. */
  bool should_denoise_auxiliary_passes()
  {
    return get_prefilter_mode() == CMP_NODE_DENOISE_PREFILTER_ACCURATE;
  }

#ifdef WITH_OPENIMAGEDENOISE
#  if OIDN_VERSION_MAJOR >= 2
  oidn::Quality get_quality()
  {
    const CMPNodeDenoiseQuality node_quality = this->get_quality_mode();

    if (node_quality == CMP_NODE_DENOISE_QUALITY_SCENE) {
      const eCompositorDenoiseQaulity scene_quality = context().get_denoise_quality();
      switch (scene_quality) {
#    if OIDN_VERSION >= 20300
        case SCE_COMPOSITOR_DENOISE_FAST:
          return oidn::Quality::Fast;
#    endif
        case SCE_COMPOSITOR_DENOISE_BALANCED:
          return oidn::Quality::Balanced;
        case SCE_COMPOSITOR_DENOISE_HIGH:
          return oidn::Quality::High;
      }

      return oidn::Quality::High;
    }

    switch (node_quality) {
#    if OIDN_VERSION >= 20300
      case CMP_NODE_DENOISE_QUALITY_FAST:
        return oidn::Quality::Fast;
#    endif
      case CMP_NODE_DENOISE_QUALITY_BALANCED:
        return oidn::Quality::Balanced;
      case CMP_NODE_DENOISE_QUALITY_HIGH:
      case CMP_NODE_DENOISE_QUALITY_SCENE:
        return oidn::Quality::High;
    }

    return oidn::Quality::High;
  }
#  endif /* OIDN_VERSION_MAJOR >= 2 */

  void set_filter_quality([[maybe_unused]] oidn::FilterRef &filter)
  {
#  if OIDN_VERSION_MAJOR >= 2
    oidn::Quality quality = this->get_quality();
    filter.set("quality", quality);
#  endif
  }
#endif /* WITH_OPENIMAGEDENOISE */

  bool use_hdr()
  {
    return this->get_input("HDR").get_single_value_default<bool>();
  }

  CMPNodeDenoisePrefilter get_prefilter_mode()
  {
    return CMPNodeDenoisePrefilter(
        this->get_input("Prefilter").get_single_value_default<MenuValue>().value);
  }

  CMPNodeDenoiseQuality get_quality_mode()
  {
    return CMPNodeDenoiseQuality(
        this->get_input("Quality").get_single_value_default<MenuValue>().value);
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new DenoiseOperation(context, node);
}

static void node_register()
{
  static bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeDenoise"_ustr, CMP_NODE_DENOISE);
  ntype.ui_name = "Denoise";
  ntype.ui_description = "Denoise renders from Cycles and other ray tracing renderers";
  ntype.enum_name_legacy = "DENOISE";
  ntype.nclass = NODE_CLASS_OP_FILTER;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_draw_buttons;
  ntype.initfunc = node_init;
  bke::node_type_storage(
      ntype, "NodeDenoise", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = get_compositor_operation;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_composite_denoise_cc
