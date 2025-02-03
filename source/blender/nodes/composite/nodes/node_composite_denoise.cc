/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#ifndef __APPLE__
#  include "BLI_system.h"
#endif

#include "MEM_guardedalloc.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "DNA_node_types.h"

#include "COM_denoised_auxiliary_pass.hh"
#include "COM_derived_resources.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

#ifdef WITH_OPENIMAGEDENOISE
#  include <OpenImageDenoise/oidn.hpp>
#endif

namespace blender::nodes::node_composite_denoise_cc {

NODE_STORAGE_FUNCS(NodeDenoise)

static void cmp_node_denoise_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Vector>("Normal")
      .default_value({0.0f, 0.0f, 0.0f})
      .min(-1.0f)
      .max(1.0f)
      .hide_value()
      .compositor_domain_priority(2);
  b.add_input<decl::Color>("Albedo")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .hide_value()
      .compositor_domain_priority(1);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_denonise(bNodeTree * /*ntree*/, bNode *node)
{
  NodeDenoise *ndg = MEM_cnew<NodeDenoise>(__func__);
  ndg->hdr = true;
  ndg->prefilter = CMP_NODE_DENOISE_PREFILTER_ACCURATE;
  ndg->quality = CMP_NODE_DENOISE_QUALITY_SCENE;
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

static void node_composit_buts_denoise(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
#ifndef WITH_OPENIMAGEDENOISE
  uiItemL(layout, RPT_("Disabled. Built without OpenImageDenoise"), ICON_ERROR);
#else
  if (!is_oidn_supported()) {
    uiItemL(layout, RPT_("Disabled. Platform not supported"), ICON_ERROR);
  }
#endif

  uiItemL(layout, IFACE_("Prefilter:"), ICON_NONE);
  uiItemR(layout, ptr, "prefilter", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  uiItemL(layout, IFACE_("Quality:"), ICON_NONE);
  uiItemR(layout, ptr, "quality", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  uiItemR(layout, ptr, "use_hdr", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
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
    Result &input_image = get_input("Image");
    Result &output_image = get_result("Image");

    if (!is_oidn_supported() || input_image.is_single_value()) {
      input_image.pass_through(output_image);
      return;
    }

    output_image.allocate_texture(input_image.domain());

#ifdef WITH_OPENIMAGEDENOISE
    oidn::DeviceRef device = oidn::newDevice(oidn::DeviceType::CPU);
    device.set("setAffinity", false);
    device.commit();

    const int width = input_image.domain().size.x;
    const int height = input_image.domain().size.y;
    const int pixel_stride = sizeof(float) * 4;
    const eGPUDataFormat data_format = GPU_DATA_FLOAT;

    Vector<float *> temporary_buffers_to_free;

    float *input_color = nullptr;
    float *output_color = nullptr;
    if (this->context().use_gpu()) {
      /* Download the input texture and set it as both the input and output of the filter to
       * denoise it in-place. Make sure to track the downloaded buffer to be later freed. */
      GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
      input_color = static_cast<float *>(GPU_texture_read(input_image, data_format, 0));
      output_color = input_color;
      temporary_buffers_to_free.append(input_color);
    }
    else {
      input_color = input_image.float_texture();
      output_color = output_image.float_texture();
    }
    oidn::FilterRef filter = device.newFilter("RT");
    filter.setImage("color", input_color, oidn::Format::Float3, width, height, 0, pixel_stride);
    filter.setImage("output", output_color, oidn::Format::Float3, width, height, 0, pixel_stride);
    filter.set("hdr", use_hdr());
    filter.set("cleanAux", auxiliary_passes_are_clean());
    this->set_filter_quality(filter);
    filter.setProgressMonitorFunction(oidn_progress_monitor_function, &context());

    /* If the albedo input is not a single value input, set it to the albedo input of the filter,
     * denoising it if needed. */
    Result &input_albedo = this->get_input("Albedo");
    if (!input_albedo.is_single_value()) {
      float *albedo = nullptr;
      if (this->should_denoise_auxiliary_passes()) {
        albedo = input_albedo.derived_resources()
                     .denoised_auxiliary_passes
                     .get(this->context(),
                          input_albedo,
                          DenoisedAuxiliaryPassType::Albedo,
                          this->get_quality())
                     .denoised_buffer;
      }
      else {
        if (this->context().use_gpu()) {
          albedo = static_cast<float *>(GPU_texture_read(input_albedo, data_format, 0));
          temporary_buffers_to_free.append(albedo);
        }
        else {
          albedo = input_albedo.float_texture();
        }
      }

      filter.setImage("albedo", albedo, oidn::Format::Float3, width, height, 0, pixel_stride);
    }

    /* If the albedo and normal inputs are not single value inputs, set the normal input to the
     * albedo input of the filter, denoising it if needed. Notice that we also consider the albedo
     * input because OIDN doesn't support denoising with only the normal auxiliary pass. */
    Result &input_normal = this->get_input("Normal");
    if (!input_albedo.is_single_value() && !input_normal.is_single_value()) {
      float *normal = nullptr;
      if (should_denoise_auxiliary_passes()) {
        normal = input_normal.derived_resources()
                     .denoised_auxiliary_passes
                     .get(this->context(),
                          input_normal,
                          DenoisedAuxiliaryPassType::Normal,
                          this->get_quality())
                     .denoised_buffer;
      }
      else {
        if (this->context().use_gpu()) {
          normal = static_cast<float *>(GPU_texture_read(input_normal, data_format, 0));
          temporary_buffers_to_free.append(normal);
        }
        else {
          normal = input_normal.float_texture();
        }
      }

      filter.setImage("normal", normal, oidn::Format::Float3, width, height, 0, pixel_stride);
    }

    filter.commit();
    filter.execute();

    if (this->context().use_gpu()) {
      GPU_texture_update(output_image, data_format, output_color);
    }
    else {
      /* OIDN already wrote to the output directly, however, OIDN skips the alpha channel, so we
       * need to restore it. */
      parallel_for(int2(width, height), [&](const int2 texel) {
        const float alpha = input_image.load_pixel<float4>(texel).w;
        output_image.store_pixel(texel,
                                 float4(output_image.load_pixel<float4>(texel).xyz(), alpha));
      });
    }

    for (float *buffer : temporary_buffers_to_free) {
      MEM_freeN(buffer);
    }
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

  bool use_hdr()
  {
    return node_storage(bnode()).hdr;
  }

  CMPNodeDenoisePrefilter get_prefilter_mode()
  {
    return static_cast<CMPNodeDenoisePrefilter>(node_storage(bnode()).prefilter);
  }

#ifdef WITH_OPENIMAGEDENOISE
#  if OIDN_VERSION_MAJOR >= 2
  oidn::Quality get_quality()
  {
    const CMPNodeDenoiseQuality node_quality = static_cast<CMPNodeDenoiseQuality>(
        node_storage(bnode()).quality);

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
        default:
          return oidn::Quality::High;
      }
    }

    switch (node_quality) {
#    if OIDN_VERSION >= 20300
      case CMP_NODE_DENOISE_QUALITY_FAST:
        return oidn::Quality::Fast;
#    endif
      case CMP_NODE_DENOISE_QUALITY_BALANCED:
        return oidn::Quality::Balanced;
      case CMP_NODE_DENOISE_QUALITY_HIGH:
      default:
        return oidn::Quality::High;
    }
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
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new DenoiseOperation(context, node);
}

}  // namespace blender::nodes::node_composite_denoise_cc

void register_node_type_cmp_denoise()
{
  namespace file_ns = blender::nodes::node_composite_denoise_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeDenoise", CMP_NODE_DENOISE);
  ntype.ui_name = "Denoise";
  ntype.ui_description = "Denoise renders from Cycles and other ray tracing renderers";
  ntype.enum_name_legacy = "DENOISE";
  ntype.nclass = NODE_CLASS_OP_FILTER;
  ntype.declare = file_ns::cmp_node_denoise_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_denoise;
  ntype.initfunc = file_ns::node_composit_init_denonise;
  blender::bke::node_type_storage(
      &ntype, "NodeDenoise", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}
