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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.h"

#include "BKE_context.h"
#include "BKE_library.h"

/* **************** Translate  ******************** */

static bNodeSocketTemplate cmp_node_stabilize2d_in[] = {
    {SOCK_RGBA, 1, N_("Image"), 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
    {-1, 0, ""},
};

static bNodeSocketTemplate cmp_node_stabilize2d_out[] = {
    {SOCK_RGBA, 0, N_("Image")},
    {-1, 0, ""},
};

static void init(const bContext *C, PointerRNA *ptr)
{
  bNode *node = ptr->data;
  Scene *scene = CTX_data_scene(C);

  node->id = (ID *)scene->clip;
  id_us_plus(node->id);

  /* default to bilinear, see node_sampler_type_items in rna_nodetree.c */
  node->custom1 = 1;
}

void register_node_type_cmp_stabilize2d(void)
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_STABILIZE2D, "Stabilize 2D", NODE_CLASS_DISTORT, 0);
  node_type_socket_templates(&ntype, cmp_node_stabilize2d_in, cmp_node_stabilize2d_out);
  ntype.initfunc_api = init;

  nodeRegisterType(&ntype);
}
