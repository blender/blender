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

static bNodeSocketTemplate cmp_node_movieclip_out[] = {
    {SOCK_RGBA, 0, N_("Image")},
    {SOCK_FLOAT, 0, N_("Alpha")},
    {SOCK_FLOAT, 1, N_("Offset X")},
    {SOCK_FLOAT, 1, N_("Offset Y")},
    {SOCK_FLOAT, 1, N_("Scale")},
    {SOCK_FLOAT, 1, N_("Angle")},
    {-1, 0, ""},
};

static void init(const bContext *C, PointerRNA *ptr)
{
  bNode *node = ptr->data;
  Scene *scene = CTX_data_scene(C);
  MovieClipUser *user = MEM_callocN(sizeof(MovieClipUser), "node movie clip user");

  node->id = (ID *)scene->clip;
  id_us_plus(node->id);
  node->storage = user;
  user->framenr = 1;
}

void register_node_type_cmp_movieclip(void)
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_MOVIECLIP, "Movie Clip", NODE_CLASS_INPUT, NODE_PREVIEW);
  node_type_socket_templates(&ntype, NULL, cmp_node_movieclip_out);
  ntype.initfunc_api = init;
  node_type_storage(
      &ntype, "MovieClipUser", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
