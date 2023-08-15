/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_system.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_state.h"
#include "GPU_texture.h"

#include "DNA_node_types.h"

#include "COM_node_operation.hh"

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
  node->storage = ndg;
}

static void node_composit_buts_denoise(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
#ifndef WITH_OPENIMAGEDENOISE
  uiItemL(layout, IFACE_("Disabled, built without OpenImageDenoise"), ICON_ERROR);
#else
  /* Always supported through Accelerate framework BNNS on macOS. */
#  ifndef __APPLE__
  if (!BLI_cpu_support_sse41()) {
    uiItemL(layout, IFACE_("Disabled, CPU with SSE4.1 is required"), ICON_ERROR);
  }
#  endif
#endif

  uiItemL(layout, IFACE_("Prefilter:"), ICON_NONE);
  uiItemR(layout, ptr, "prefilter", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "use_hdr", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

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

#ifdef WITH_OPENIMAGEDENOISE
    oidn::DeviceRef device = oidn::newDevice();
    device.commit();

    const int width = input_image.domain().size.x;
    const int height = input_image.domain().size.y;
    const int pixel_stride = sizeof(float) * 4;
    const eGPUDataFormat data_format = GPU_DATA_FLOAT;

    /* Download the input texture and set it as both the input and output of the filter to denoise
     * it in-place. */
    GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
    float *color = static_cast<float *>(GPU_texture_read(input_image.texture(), data_format, 0));
    oidn::FilterRef filter = device.newFilter("RT");
    filter.setImage("color", color, oidn::Format::Float3, width, height, 0, pixel_stride);
    filter.setImage("output", color, oidn::Format::Float3, width, height, 0, pixel_stride);
    filter.set("hdr", use_hdr());
    filter.set("cleanAux", auxiliary_passes_are_clean());

    /* If the albedo input is not a single value input, download the albedo texture, denoise it
     * in-place if denoising auxiliary passes is needed, and set it to the main filter. */
    float *albedo = nullptr;
    Result &input_albedo = get_input("Albedo");
    if (!input_albedo.is_single_value()) {
      albedo = static_cast<float *>(GPU_texture_read(input_albedo.texture(), data_format, 0));

      if (should_denoise_auxiliary_passes()) {
        oidn::FilterRef albedoFilter = device.newFilter("RT");
        albedoFilter.setImage(
            "albedo", albedo, oidn::Format::Float3, width, height, 0, pixel_stride);
        albedoFilter.setImage(
            "output", albedo, oidn::Format::Float3, width, height, 0, pixel_stride);
        albedoFilter.commit();
        albedoFilter.execute();
      }

      filter.setImage("albedo", albedo, oidn::Format::Float3, width, height, 0, pixel_stride);
    }

    /* If the albedo and normal inputs are not single value inputs, download the normal texture,
     * denoise it in-place if denoising auxiliary passes is needed, and set it to the main filter.
     * Notice that we also consider the albedo input because OIDN doesn't support denoising with
     * only the normal auxiliary pass. */
    float *normal = nullptr;
    Result &input_normal = get_input("Normal");
    if (albedo && !input_normal.is_single_value()) {
      normal = static_cast<float *>(GPU_texture_read(input_normal.texture(), data_format, 0));

      if (should_denoise_auxiliary_passes()) {
        oidn::FilterRef normalFilter = device.newFilter("RT");
        normalFilter.setImage(
            "normal", normal, oidn::Format::Float3, width, height, 0, pixel_stride);
        normalFilter.setImage(
            "output", normal, oidn::Format::Float3, width, height, 0, pixel_stride);
        normalFilter.commit();
        normalFilter.execute();
      }

      filter.setImage("normal", normal, oidn::Format::Float3, width, height, 0, pixel_stride);
    }

    filter.commit();
    filter.execute();

    output_image.allocate_texture(input_image.domain());
    GPU_texture_update(output_image.texture(), data_format, color);

    MEM_freeN(color);
    if (albedo) {
      MEM_freeN(albedo);
    }
    if (normal) {
      MEM_freeN(normal);
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

  /* OIDN can be disabled as a build option, so check WITH_OPENIMAGEDENOISE. Additionally, it is
   * only supported at runtime for CPUs that supports SSE4.1, except for MacOS where it is always
   * supported through the Accelerate framework BNNS on macOS. */
  bool is_oidn_supported()
  {
#ifndef WITH_OPENIMAGEDENOISE
    return false;
#else
#  ifdef __APPLE__
    return true;
#  else
    return BLI_cpu_support_sse41();
#  endif
#endif
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new DenoiseOperation(context, node);
}

}  // namespace blender::nodes::node_composite_denoise_cc

void register_node_type_cmp_denoise()
{
  namespace file_ns = blender::nodes::node_composite_denoise_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_DENOISE, "Denoise", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_denoise_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_denoise;
  ntype.initfunc = file_ns::node_composit_init_denonise;
  node_type_storage(&ntype, "NodeDenoise", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
