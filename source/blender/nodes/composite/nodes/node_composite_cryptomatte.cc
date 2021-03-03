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

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_assert.h"
#include "BLI_dynstr.h"
#include "BLI_hash_mm3.h"
#include "BLI_utildefines.h"
#include "node_composite_util.h"

extern "C" {
static CryptomatteEntry *cryptomatte_find(const NodeCryptomatte &n, float encoded_hash)
{
  LISTBASE_FOREACH (CryptomatteEntry *, entry, &n.entries) {
    if (entry->encoded_hash == encoded_hash) {
      return entry;
    }
  }
  return nullptr;
}

static void cryptomatte_add(NodeCryptomatte &n, float f)
{
  /* Check if entry already exist. */
  if (cryptomatte_find(n, f)) {
    return;
  }
  CryptomatteEntry *entry = static_cast<CryptomatteEntry *>(
      MEM_callocN(sizeof(CryptomatteEntry), __func__));
  entry->encoded_hash = f;
  BLI_addtail(&n.entries, entry);
}

static void cryptomatte_remove(NodeCryptomatte &n, float f)
{
  CryptomatteEntry *entry = cryptomatte_find(n, f);
  if (!entry) {
    return;
  }
  BLI_remlink(&n.entries, entry);
  MEM_freeN(entry);
}

static bNodeSocketTemplate outputs[] = {
    {SOCK_RGBA, N_("Image")},
    {SOCK_FLOAT, N_("Matte")},
    {SOCK_RGBA, N_("Pick")},
    {-1, ""},
};

void ntreeCompositCryptomatteSyncFromAdd(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  if (n->add[0] != 0.0f) {
    cryptomatte_add(*n, n->add[0]);
    zero_v3(n->add);
  }
}

void ntreeCompositCryptomatteSyncFromRemove(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  if (n->remove[0] != 0.0f) {
    cryptomatte_remove(*n, n->remove[0]);
    zero_v3(n->remove);
  }
}

bNodeSocket *ntreeCompositCryptomatteAddSocket(bNodeTree *ntree, bNode *node)
{
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  char sockname[32];
  n->num_inputs++;
  BLI_snprintf(sockname, sizeof(sockname), "Crypto %.2d", n->num_inputs - 1);
  bNodeSocket *sock = nodeAddStaticSocket(
      ntree, node, SOCK_IN, SOCK_RGBA, PROP_NONE, nullptr, sockname);
  return sock;
}

int ntreeCompositCryptomatteRemoveSocket(bNodeTree *ntree, bNode *node)
{
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  if (n->num_inputs < 2) {
    return 0;
  }
  bNodeSocket *sock = static_cast<bNodeSocket *>(node->inputs.last);
  nodeRemoveSocket(ntree, node, sock);
  n->num_inputs--;
  return 1;
}

static void init(bNodeTree *ntree, bNode *node)
{
  NodeCryptomatte *user = static_cast<NodeCryptomatte *>(
      MEM_callocN(sizeof(NodeCryptomatte), __func__));
  node->storage = user;

  nodeAddStaticSocket(ntree, node, SOCK_IN, SOCK_RGBA, PROP_NONE, "image", "Image");

  /* Add three inputs by default, as recommended by the Cryptomatte specification. */
  ntreeCompositCryptomatteAddSocket(ntree, node);
  ntreeCompositCryptomatteAddSocket(ntree, node);
  ntreeCompositCryptomatteAddSocket(ntree, node);
}

static void node_free_cryptomatte(bNode *node)
{
  NodeCryptomatte *nc = static_cast<NodeCryptomatte *>(node->storage);

  if (nc) {
    BLI_freelistN(&nc->entries);
    MEM_freeN(nc);
  }
}

static void node_copy_cryptomatte(bNodeTree *UNUSED(dest_ntree),
                                  bNode *dest_node,
                                  const bNode *src_node)
{
  NodeCryptomatte *src_nc = static_cast<NodeCryptomatte *>(src_node->storage);
  NodeCryptomatte *dest_nc = static_cast<NodeCryptomatte *>(MEM_dupallocN(src_nc));

  BLI_duplicatelist(&dest_nc->entries, &src_nc->entries);
  dest_node->storage = dest_nc;
}

void register_node_type_cmp_cryptomatte(void)
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_CRYPTOMATTE, "Cryptomatte", NODE_CLASS_CONVERTOR, 0);
  node_type_socket_templates(&ntype, nullptr, outputs);
  node_type_init(&ntype, init);
  node_type_storage(&ntype, "NodeCryptomatte", node_free_cryptomatte, node_copy_cryptomatte);
  nodeRegisterType(&ntype);
}
}
