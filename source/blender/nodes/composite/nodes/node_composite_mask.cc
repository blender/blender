/* SPDX-FileCopyrightText: 2012 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_string_utf8.h"

#include "DNA_mask_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "COM_cached_mask.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** Mask  ******************** */

namespace blender::nodes::node_composite_mask_cc {

NODE_STORAGE_FUNCS(NodeMask)

static void cmp_node_mask_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Mask");
}

static void node_composit_init_mask(bNodeTree * /*ntree*/, bNode *node)
{
  NodeMask *data = MEM_cnew<NodeMask>(__func__);
  data->size_x = data->size_y = 256;
  node->storage = data;

  node->custom2 = 16;   /* samples */
  node->custom3 = 0.5f; /* shutter */
}

static void node_mask_label(const bNodeTree * /*ntree*/,
                            const bNode *node,
                            char *label,
                            int label_maxncpy)
{
  BLI_strncpy_utf8(label, node->id ? node->id->name + 2 : IFACE_("Mask"), label_maxncpy);
}

static void node_composit_buts_mask(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;

  uiTemplateID(layout,
               C,
               ptr,
               "mask",
               nullptr,
               nullptr,
               nullptr,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               nullptr);
  uiItemR(layout, ptr, "use_feather", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "size_source", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);

  if (node->custom1 & (CMP_NODE_MASK_FLAG_SIZE_FIXED | CMP_NODE_MASK_FLAG_SIZE_FIXED_SCENE)) {
    uiItemR(layout, ptr, "size_x", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    uiItemR(layout, ptr, "size_y", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }

  uiItemR(layout, ptr, "use_motion_blur", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  if (node->custom1 & CMP_NODE_MASK_FLAG_MOTION_BLUR) {
    uiItemR(layout, ptr, "motion_blur_samples", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    uiItemR(layout, ptr, "motion_blur_shutter", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }
}

using namespace blender::realtime_compositor;

class MaskOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &output_mask = get_result("Mask");
    if (!get_mask()) {
      output_mask.allocate_invalid();
      return;
    }

    const Domain domain = compute_domain();
    CachedMask &cached_mask = context().cache_manager().cached_masks.get(
        context(),
        get_mask(),
        domain.size,
        get_use_feather(),
        get_motion_blur_samples(),
        get_motion_blur_shutter());

    output_mask.allocate_texture(domain);
    GPU_texture_copy(output_mask.texture(), cached_mask.texture());
  }

  Domain compute_domain() override
  {
    return Domain(compute_size());
  }

  int2 compute_size()
  {
    if (get_flags() & CMP_NODE_MASK_FLAG_SIZE_FIXED) {
      return get_size();
    }

    if (get_flags() & CMP_NODE_MASK_FLAG_SIZE_FIXED_SCENE) {
      return get_size() * context().get_render_percentage();
    }

    return context().get_compositing_region_size();
  }

  int2 get_size()
  {
    return int2(node_storage(bnode()).size_x, node_storage(bnode()).size_y);
  }

  bool get_use_feather()
  {
    return !bool(get_flags() & CMP_NODE_MASK_FLAG_NO_FEATHER);
  }

  int get_motion_blur_samples()
  {
    return use_motion_blur() ? bnode().custom2 : 1;
  }

  float get_motion_blur_shutter()
  {
    return bnode().custom3;
  }

  bool use_motion_blur()
  {
    return get_flags() & CMP_NODE_MASK_FLAG_MOTION_BLUR;
  }

  CMPNodeMaskFlags get_flags()
  {
    return static_cast<CMPNodeMaskFlags>(bnode().custom1);
  }

  Mask *get_mask()
  {
    return reinterpret_cast<Mask *>(bnode().id);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new MaskOperation(context, node);
}

}  // namespace blender::nodes::node_composite_mask_cc

void register_node_type_cmp_mask()
{
  namespace file_ns = blender::nodes::node_composite_mask_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_MASK, "Mask", NODE_CLASS_INPUT);
  ntype.declare = file_ns::cmp_node_mask_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_mask;
  ntype.initfunc = file_ns::node_composit_init_mask;
  ntype.labelfunc = file_ns::node_mask_label;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  node_type_storage(&ntype, "NodeMask", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
