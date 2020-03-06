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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.h"

/* **************** TEXTURE ******************** */
static bNodeSocketTemplate cmp_node_texture_in[] = {
    {SOCK_VECTOR, N_("Offset"), 0.0f, 0.0f, 0.0f, 0.0f, -2.0f, 2.0f, PROP_TRANSLATION},
    {SOCK_VECTOR, N_("Scale"), 1.0f, 1.0f, 1.0f, 1.0f, -10.0f, 10.0f, PROP_XYZ},
    {-1, ""},
};
static bNodeSocketTemplate cmp_node_texture_out[] = {
    {SOCK_FLOAT, N_("Value")},
    {SOCK_RGBA, N_("Color")},
    {-1, ""},
};

void register_node_type_cmp_texture(void)
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_TEXTURE, "Texture", NODE_CLASS_INPUT, NODE_PREVIEW);
  node_type_socket_templates(&ntype, cmp_node_texture_in, cmp_node_texture_out);

  nodeRegisterType(&ntype);
}
