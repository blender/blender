/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_system.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_composite_util.hh"

namespace blender::nodes {

static void cmp_node_denoise_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Vector>(N_("Normal"))
      .default_value({0.0f, 0.0f, 0.0f})
      .min(-1.0f)
      .max(1.0f)
      .hide_value();
  b.add_input<decl::Color>(N_("Albedo")).default_value({1.0f, 1.0f, 1.0f, 1.0f}).hide_value();
  b.add_output<decl::Color>(N_("Image"));
}

}  // namespace blender::nodes

static void node_composit_init_denonise(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeDenoise *ndg = (NodeDenoise *)MEM_callocN(sizeof(NodeDenoise), "node denoise data");
  ndg->hdr = true;
  ndg->prefilter = CMP_NODE_DENOISE_PREFILTER_ACCURATE;
  node->storage = ndg;
}

static void node_composit_buts_denoise(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
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

void register_node_type_cmp_denoise()
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_DENOISE, "Denoise", NODE_CLASS_OP_FILTER, 0);
  ntype.declare = blender::nodes::cmp_node_denoise_declare;
  ntype.draw_buttons = node_composit_buts_denoise;
  node_type_init(&ntype, node_composit_init_denonise);
  node_type_storage(&ntype, "NodeDenoise", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
