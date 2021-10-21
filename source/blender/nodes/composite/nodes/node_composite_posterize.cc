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
 * The Original Code is Copyright (C) 2021 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

/* **************** Posterize ******************** */

static bNodeSocketTemplate cmp_node_posterize_in[] = {
    {SOCK_RGBA, N_("Image"), 1.0f, 1.0f, 1.0f, 1.0f},
    {SOCK_FLOAT, N_("Steps"), 8.0f, 8.0f, 8.0f, 8.0f, 2.0f, 1024.0f, PROP_NONE},
    {-1, ""},
};
static bNodeSocketTemplate cmp_node_posterize_out[] = {
    {SOCK_RGBA, N_("Image")},
    {-1, ""},
};

void register_node_type_cmp_posterize(void)
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_POSTERIZE, "Posterize", NODE_CLASS_OP_COLOR, 0);
  node_type_socket_templates(&ntype, cmp_node_posterize_in, cmp_node_posterize_out);

  nodeRegisterType(&ntype);
}
