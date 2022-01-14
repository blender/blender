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
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 */

#include "node_shader_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_shader_bsdf_hair_principled_cc {

/* Color, melanin and absorption coefficient default to approximately same brownish hair. */
static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Color")).default_value({0.017513f, 0.005763f, 0.002059f, 1.0f});
  b.add_input<decl::Float>(N_("Melanin"))
      .default_value(0.8f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Melanin Redness"))
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Color>(N_("Tint")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Vector>(N_("Absorption Coefficient"))
      .default_value({0.245531f, 0.52f, 1.365f})
      .min(0.0f)
      .max(1000.0f);
  b.add_input<decl::Float>(N_("Roughness"))
      .default_value(0.3f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Radial Roughness"))
      .default_value(0.3f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Coat"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("IOR")).default_value(1.55f).min(0.0f).max(1000.0f).subtype(
      PROP_FACTOR);
  b.add_input<decl::Float>(N_("Offset"))
      .default_value(2.0f * ((float)M_PI) / 180.0f)
      .min(-M_PI_2)
      .max(M_PI_2)
      .subtype(PROP_ANGLE);
  b.add_input<decl::Float>(N_("Random Color"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Random Roughness"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Random")).hide_value();
  b.add_output<decl::Shader>(N_("BSDF"));
}

static void node_shader_buts_principled_hair(uiLayout *layout,
                                             bContext *UNUSED(C),
                                             PointerRNA *ptr)
{
  uiItemR(layout, ptr, "parametrization", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

/* Initialize the custom Parametrization property to Color. */
static void node_shader_init_hair_principled(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom1 = SHD_PRINCIPLED_HAIR_REFLECTANCE;
}

/* Triggers (in)visibility of some sockets when changing Parametrization. */
static void node_shader_update_hair_principled(bNodeTree *ntree, bNode *node)
{
  int parametrization = node->custom1;

  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    if (STREQ(sock->name, "Color")) {
      nodeSetSocketAvailability(ntree, sock, parametrization == SHD_PRINCIPLED_HAIR_REFLECTANCE);
    }
    else if (STREQ(sock->name, "Melanin")) {
      nodeSetSocketAvailability(
          ntree, sock, parametrization == SHD_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION);
    }
    else if (STREQ(sock->name, "Melanin Redness")) {
      nodeSetSocketAvailability(
          ntree, sock, parametrization == SHD_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION);
    }
    else if (STREQ(sock->name, "Tint")) {
      nodeSetSocketAvailability(
          ntree, sock, parametrization == SHD_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION);
    }
    else if (STREQ(sock->name, "Absorption Coefficient")) {
      nodeSetSocketAvailability(
          ntree, sock, parametrization == SHD_PRINCIPLED_HAIR_DIRECT_ABSORPTION);
    }
    else if (STREQ(sock->name, "Random Color")) {
      nodeSetSocketAvailability(
          ntree, sock, parametrization == SHD_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION);
    }
  }
}

}  // namespace blender::nodes::node_shader_bsdf_hair_principled_cc

/* node type definition */
void register_node_type_sh_bsdf_hair_principled()
{
  namespace file_ns = blender::nodes::node_shader_bsdf_hair_principled_cc;

  static bNodeType ntype;

  sh_node_type_base(
      &ntype, SH_NODE_BSDF_HAIR_PRINCIPLED, "Principled Hair BSDF", NODE_CLASS_SHADER);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_principled_hair;
  node_type_size_preset(&ntype, NODE_SIZE_LARGE);
  node_type_init(&ntype, file_ns::node_shader_init_hair_principled);
  node_type_update(&ntype, file_ns::node_shader_update_hair_principled);

  nodeRegisterType(&ntype);
}
