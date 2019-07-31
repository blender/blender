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

#include "node_composite_util.h"
#include "BLI_assert.h"
#include "BLI_dynstr.h"
#include "BLI_hash_mm3.h"
#include "BLI_utildefines.h"

/* this is taken from the cryptomatte specification 1.0 */

BLI_INLINE float hash_to_float(uint32_t hash)
{
  uint32_t mantissa = hash & ((1 << 23) - 1);
  uint32_t exponent = (hash >> 23) & ((1 << 8) - 1);
  exponent = MAX2(exponent, (uint32_t)1);
  exponent = MIN2(exponent, (uint32_t)254);
  exponent = exponent << 23;
  uint32_t sign = (hash >> 31);
  sign = sign << 31;
  uint32_t float_bits = sign | exponent | mantissa;
  float f;
  /* Bit casting relies on equal size for both types. */
  BLI_STATIC_ASSERT(sizeof(float) == sizeof(uint32_t), "float and uint32_t are not the same size")
  memcpy(&f, &float_bits, sizeof(float));
  return f;
}

static void cryptomatte_add(NodeCryptomatte *n, float f)
{
  /* Turn the number into a string. */
  char number[32];
  BLI_snprintf(number, sizeof(number), "<%.9g>", f);

  /* Search if we already have the number. */
  if (n->matte_id && strlen(n->matte_id) != 0) {
    size_t start = 0;
    const size_t end = strlen(n->matte_id);
    size_t token_len = 0;
    while (start < end) {
      /* Ignore leading whitespace. */
      while (start < end && n->matte_id[start] == ' ') {
        ++start;
      }

      /* Find the next separator. */
      char *token_end = strchr(n->matte_id + start, ',');
      if (token_end == NULL || token_end == n->matte_id + start) {
        token_end = n->matte_id + end;
      }
      /* Be aware that token_len still contains any trailing white space. */
      token_len = token_end - (n->matte_id + start);

      /* If this has a leading bracket,
       * assume a raw floating point number and look for the closing bracket. */
      if (n->matte_id[start] == '<') {
        if (strncmp(n->matte_id + start, number, strlen(number)) == 0) {
          /* This number is already there, so continue. */
          return;
        }
      }
      else {
        /* Remove trailing white space */
        size_t name_len = token_len;
        while (n->matte_id[start + name_len] == ' ' && name_len > 0) {
          name_len--;
        }
        /* Calculate the hash of the token and compare. */
        uint32_t hash = BLI_hash_mm3((const unsigned char *)(n->matte_id + start), name_len, 0);
        if (f == hash_to_float(hash)) {
          return;
        }
      }
      start += token_len + 1;
    }
  }

  DynStr *new_matte = BLI_dynstr_new();
  if (!new_matte) {
    return;
  }

  if (n->matte_id) {
    BLI_dynstr_append(new_matte, n->matte_id);
    MEM_freeN(n->matte_id);
  }

  if (BLI_dynstr_get_len(new_matte) > 0) {
    BLI_dynstr_append(new_matte, ",");
  }
  BLI_dynstr_append(new_matte, number);
  n->matte_id = BLI_dynstr_get_cstring(new_matte);
  BLI_dynstr_free(new_matte);
}

static void cryptomatte_remove(NodeCryptomatte *n, float f)
{
  if (n->matte_id == NULL || strlen(n->matte_id) == 0) {
    /* Empty string, nothing to remove. */
    return;
  }

  /* This will be the new string without the removed key. */
  DynStr *new_matte = BLI_dynstr_new();
  if (!new_matte) {
    return;
  }

  /* Turn the number into a string. */
  static char number[32];
  BLI_snprintf(number, sizeof(number), "<%.9g>", f);

  /* Search if we already have the number. */
  size_t start = 0;
  const size_t end = strlen(n->matte_id);
  size_t token_len = 0;
  bool is_first = true;
  while (start < end) {
    bool skip = false;
    /* Ignore leading whitespace or commas. */
    while (start < end && ((n->matte_id[start] == ' ') || (n->matte_id[start] == ','))) {
      ++start;
    }

    /* Find the next separator. */
    char *token_end = strchr(n->matte_id + start + 1, ',');
    if (token_end == NULL || token_end == n->matte_id + start) {
      token_end = n->matte_id + end;
    }
    /* Be aware that token_len still contains any trailing white space. */
    token_len = token_end - (n->matte_id + start);

    if (token_len == 1) {
      skip = true;
    }
    /* If this has a leading bracket,
     * assume a raw floating point number and look for the closing bracket. */
    else if (n->matte_id[start] == '<') {
      if (strncmp(n->matte_id + start, number, strlen(number)) == 0) {
        /* This number is already there, so skip it. */
        skip = true;
      }
    }
    else {
      /* Remove trailing white space */
      size_t name_len = token_len;
      while (n->matte_id[start + name_len] == ' ' && name_len > 0) {
        name_len--;
      }
      /* Calculate the hash of the token and compare. */
      uint32_t hash = BLI_hash_mm3((const unsigned char *)(n->matte_id + start), name_len, 0);
      if (f == hash_to_float(hash)) {
        skip = true;
      }
    }
    if (!skip) {
      if (is_first) {
        is_first = false;
      }
      else {
        BLI_dynstr_append(new_matte, ", ");
      }
      BLI_dynstr_nappend(new_matte, n->matte_id + start, token_len);
    }
    start += token_len + 1;
  }

  if (n->matte_id) {
    MEM_freeN(n->matte_id);
    n->matte_id = NULL;
  }
  if (BLI_dynstr_get_len(new_matte) > 0) {
    n->matte_id = BLI_dynstr_get_cstring(new_matte);
  }
  BLI_dynstr_free(new_matte);
}

static bNodeSocketTemplate outputs[] = {
    {SOCK_RGBA, 0, N_("Image")},
    {SOCK_FLOAT, 0, N_("Matte")},
    {SOCK_RGBA, 0, N_("Pick")},
    {-1, 0, ""},
};

void ntreeCompositCryptomatteSyncFromAdd(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeCryptomatte *n = node->storage;
  if (n->add[0] != 0.0f) {
    cryptomatte_add(n, n->add[0]);
    n->add[0] = 0.0f;
    n->add[1] = 0.0f;
    n->add[2] = 0.0f;
  }
}

void ntreeCompositCryptomatteSyncFromRemove(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeCryptomatte *n = node->storage;
  if (n->remove[0] != 0.0f) {
    cryptomatte_remove(n, n->remove[0]);
    n->remove[0] = 0.0f;
    n->remove[1] = 0.0f;
    n->remove[2] = 0.0f;
  }
}

bNodeSocket *ntreeCompositCryptomatteAddSocket(bNodeTree *ntree, bNode *node)
{
  NodeCryptomatte *n = node->storage;
  char sockname[32];
  n->num_inputs++;
  BLI_snprintf(sockname, sizeof(sockname), "Crypto %.2d", n->num_inputs - 1);
  bNodeSocket *sock = nodeAddStaticSocket(
      ntree, node, SOCK_IN, SOCK_RGBA, PROP_NONE, NULL, sockname);
  return sock;
}

int ntreeCompositCryptomatteRemoveSocket(bNodeTree *ntree, bNode *node)
{
  NodeCryptomatte *n = node->storage;
  if (n->num_inputs < 2) {
    return 0;
  }
  bNodeSocket *sock = node->inputs.last;
  nodeRemoveSocket(ntree, node, sock);
  n->num_inputs--;
  return 1;
}

static void init(bNodeTree *ntree, bNode *node)
{
  NodeCryptomatte *user = MEM_callocN(sizeof(NodeCryptomatte), "cryptomatte user");
  node->storage = user;

  nodeAddStaticSocket(ntree, node, SOCK_IN, SOCK_RGBA, PROP_NONE, "image", "Image");

  /* Add three inputs by default, as recommended by the Cryptomatte specification. */
  ntreeCompositCryptomatteAddSocket(ntree, node);
  ntreeCompositCryptomatteAddSocket(ntree, node);
  ntreeCompositCryptomatteAddSocket(ntree, node);
}

static void node_free_cryptomatte(bNode *node)
{
  NodeCryptomatte *nc = node->storage;

  if (nc) {
    if (nc->matte_id) {
      MEM_freeN(nc->matte_id);
    }

    MEM_freeN(nc);
  }
}

static void node_copy_cryptomatte(bNodeTree *UNUSED(dest_ntree),
                                  bNode *dest_node,
                                  const bNode *src_node)
{
  NodeCryptomatte *src_nc = src_node->storage;
  NodeCryptomatte *dest_nc = MEM_dupallocN(src_nc);

  if (src_nc->matte_id) {
    dest_nc->matte_id = MEM_dupallocN(src_nc->matte_id);
  }

  dest_node->storage = dest_nc;
}

void register_node_type_cmp_cryptomatte(void)
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_CRYPTOMATTE, "Cryptomatte", NODE_CLASS_CONVERTOR, 0);
  node_type_socket_templates(&ntype, NULL, outputs);
  node_type_init(&ntype, init);
  node_type_storage(&ntype, "NodeCryptomatte", node_free_cryptomatte, node_copy_cryptomatte);
  nodeRegisterType(&ntype);
}
