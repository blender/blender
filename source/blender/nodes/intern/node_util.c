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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup nodes
 */

#include <ctype.h>
#include <limits.h>
#include <string.h>

#include "DNA_node_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_colortools.h"
#include "BKE_node.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "MEM_guardedalloc.h"

#include "node_util.h"

/* -------------------------------------------------------------------- */
/** \name Storage Data
 * \{ */

void node_free_curves(bNode *node)
{
  BKE_curvemapping_free(node->storage);
}

void node_free_standard_storage(bNode *node)
{
  if (node->storage) {
    MEM_freeN(node->storage);
  }
}

void node_copy_curves(bNodeTree *UNUSED(dest_ntree), bNode *dest_node, const bNode *src_node)
{
  dest_node->storage = BKE_curvemapping_copy(src_node->storage);
}

void node_copy_standard_storage(bNodeTree *UNUSED(dest_ntree),
                                bNode *dest_node,
                                const bNode *src_node)
{
  dest_node->storage = MEM_dupallocN(src_node->storage);
}

void *node_initexec_curves(bNodeExecContext *UNUSED(context),
                           bNode *node,
                           bNodeInstanceKey UNUSED(key))
{
  BKE_curvemapping_init(node->storage);
  return NULL; /* unused return */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Updates
 * \{ */

void node_sock_label(bNodeSocket *sock, const char *name)
{
  BLI_strncpy(sock->label, name, MAX_NAME);
}

void node_math_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sock1 = BLI_findlink(&node->inputs, 0);
  bNodeSocket *sock2 = BLI_findlink(&node->inputs, 1);
  bNodeSocket *sock3 = BLI_findlink(&node->inputs, 2);
  nodeSetSocketAvailability(sock2,
                            !ELEM(node->custom1,
                                  NODE_MATH_SQRT,
                                  NODE_MATH_SIGN,
                                  NODE_MATH_CEIL,
                                  NODE_MATH_SINE,
                                  NODE_MATH_ROUND,
                                  NODE_MATH_FLOOR,
                                  NODE_MATH_COSINE,
                                  NODE_MATH_ARCSINE,
                                  NODE_MATH_TANGENT,
                                  NODE_MATH_ABSOLUTE,
                                  NODE_MATH_RADIANS,
                                  NODE_MATH_DEGREES,
                                  NODE_MATH_FRACTION,
                                  NODE_MATH_ARCCOSINE,
                                  NODE_MATH_ARCTANGENT) &&
                                !ELEM(node->custom1,
                                      NODE_MATH_INV_SQRT,
                                      NODE_MATH_TRUNC,
                                      NODE_MATH_EXPONENT,
                                      NODE_MATH_COSH,
                                      NODE_MATH_SINH,
                                      NODE_MATH_TANH));
  nodeSetSocketAvailability(sock3,
                            ELEM(node->custom1,
                                 NODE_MATH_COMPARE,
                                 NODE_MATH_MULTIPLY_ADD,
                                 NODE_MATH_WRAP,
                                 NODE_MATH_SMOOTH_MIN,
                                 NODE_MATH_SMOOTH_MAX));

  if (sock1->label[0] != '\0') {
    sock1->label[0] = '\0';
  }
  if (sock2->label[0] != '\0') {
    sock2->label[0] = '\0';
  }
  if (sock3->label[0] != '\0') {
    sock3->label[0] = '\0';
  }

  switch (node->custom1) {
    case NODE_MATH_WRAP:
      node_sock_label(sock2, "Min");
      node_sock_label(sock3, "Max");
      break;
    case NODE_MATH_MULTIPLY_ADD:
      node_sock_label(sock2, "Multiplier");
      node_sock_label(sock3, "Addend");
      break;
    case NODE_MATH_LESS_THAN:
    case NODE_MATH_GREATER_THAN:
      node_sock_label(sock2, "Threshold");
      break;
    case NODE_MATH_PINGPONG:
      node_sock_label(sock2, "Scale");
      break;
    case NODE_MATH_SNAP:
      node_sock_label(sock2, "Increment");
      break;
    case NODE_MATH_POWER:
      node_sock_label(sock1, "Base");
      node_sock_label(sock2, "Exponent");
      break;
    case NODE_MATH_LOGARITHM:
      node_sock_label(sock2, "Base");
      break;
    case NODE_MATH_DEGREES:
      node_sock_label(sock1, "Radians");
      break;
    case NODE_MATH_RADIANS:
      node_sock_label(sock1, "Degrees");
      break;
    case NODE_MATH_COMPARE:
      node_sock_label(sock3, "Epsilon");
      break;
    case NODE_MATH_SMOOTH_MAX:
    case NODE_MATH_SMOOTH_MIN:
      node_sock_label(sock3, "Distance");
      break;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Labels
 * \{ */

void node_blend_label(bNodeTree *UNUSED(ntree), bNode *node, char *label, int maxlen)
{
  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_ramp_blend_items, node->custom1, &name);
  if (!enum_label) {
    name = "Unknown";
  }
  BLI_strncpy(label, IFACE_(name), maxlen);
}

void node_image_label(bNodeTree *UNUSED(ntree), bNode *node, char *label, int maxlen)
{
  /* If there is no loaded image, return an empty string,
   * and let nodeLabel() fill in the proper type translation. */
  BLI_strncpy(label, (node->id) ? node->id->name + 2 : "", maxlen);
}

void node_math_label(bNodeTree *UNUSED(ntree), bNode *node, char *label, int maxlen)
{
  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_node_math_items, node->custom1, &name);
  if (!enum_label) {
    name = "Unknown";
  }
  BLI_strncpy(label, IFACE_(name), maxlen);
}

void node_vector_math_label(bNodeTree *UNUSED(ntree), bNode *node, char *label, int maxlen)
{
  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_node_vec_math_items, node->custom1, &name);
  if (!enum_label) {
    name = "Unknown";
  }
  BLI_strncpy(label, IFACE_(name), maxlen);
}

void node_filter_label(bNodeTree *UNUSED(ntree), bNode *node, char *label, int maxlen)
{
  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_node_filter_items, node->custom1, &name);
  if (!enum_label) {
    name = "Unknown";
  }
  BLI_strncpy(label, IFACE_(name), maxlen);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Link Insertion
 * \{ */

/* test if two sockets are interchangeable */
static bool node_link_socket_match(bNodeSocket *a, bNodeSocket *b)
{
  /* check if sockets are of the same type */
  if (a->typeinfo != b->typeinfo) {
    return false;
  }

  /* tests if alphabetic prefix matches
   * this allows for imperfect matches, such as numeric suffixes,
   * like Color1/Color2
   */
  int prefix_len = 0;
  char *ca = a->name, *cb = b->name;
  for (; *ca != '\0' && *cb != '\0'; ca++, cb++) {
    /* end of common prefix? */
    if (*ca != *cb) {
      /* prefix delimited by non-alphabetic char */
      if (isalpha(*ca) || isalpha(*cb)) {
        return false;
      }
      break;
    }
    prefix_len++;
  }
  return prefix_len > 0;
}

static int node_count_links(bNodeTree *ntree, bNodeSocket *sock)
{
  bNodeLink *link;
  int count = 0;
  for (link = ntree->links.first; link; link = link->next) {
    if (link->fromsock == sock) {
      count++;
    }
    if (link->tosock == sock) {
      count++;
    }
  }
  return count;
}

/* find an eligible socket for linking */
static bNodeSocket *node_find_linkable_socket(bNodeTree *ntree, bNode *node, bNodeSocket *cur)
{
  /* link swapping: try to find a free slot with a matching name */

  bNodeSocket *first = cur->in_out == SOCK_IN ? node->inputs.first : node->outputs.first;
  bNodeSocket *sock;

  sock = cur->next ? cur->next : first; /* wrap around the list end */
  while (sock != cur) {
    if (!nodeSocketIsHidden(sock) && node_link_socket_match(sock, cur)) {
      int link_count = node_count_links(ntree, sock);
      /* take +1 into account since we would add a new link */
      if (link_count + 1 <= nodeSocketLinkLimit(sock)) {
        return sock; /* found a valid free socket we can swap to */
      }
    }

    sock = sock->next ? sock->next : first; /* wrap around the list end */
  }
  return NULL;
}

void node_insert_link_default(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  bNodeSocket *sock = link->tosock;
  bNodeLink *tlink, *tlink_next;

  /* inputs can have one link only, outputs can have unlimited links */
  if (node != link->tonode) {
    return;
  }

  for (tlink = ntree->links.first; tlink; tlink = tlink_next) {
    bNodeSocket *new_sock;
    tlink_next = tlink->next;

    if (sock != tlink->tosock) {
      continue;
    }

    new_sock = node_find_linkable_socket(ntree, node, sock);
    if (new_sock && new_sock != sock) {
      /* redirect existing link */
      tlink->tosock = new_sock;
    }
    else if (!new_sock) {
      /* no possible replacement, remove tlink */
      nodeRemLink(ntree, tlink);
      tlink = NULL;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Links (mute and disconnect)
 * \{ */

/**
 * Common datatype priorities, works for compositor, shader and texture nodes alike
 * defines priority of datatype connection based on output type (to):
 * `<  0`: never connect these types.
 * `>= 0`: priority of connection (higher values chosen first).
 */
static int node_datatype_priority(eNodeSocketDatatype from, eNodeSocketDatatype to)
{
  switch (to) {
    case SOCK_RGBA:
      switch (from) {
        case SOCK_RGBA:
          return 4;
        case SOCK_FLOAT:
          return 3;
        case SOCK_INT:
          return 2;
        case SOCK_BOOLEAN:
          return 1;
        default:
          return -1;
      }
    case SOCK_VECTOR:
      switch (from) {
        case SOCK_VECTOR:
          return 4;
        case SOCK_FLOAT:
          return 3;
        case SOCK_INT:
          return 2;
        case SOCK_BOOLEAN:
          return 1;
        default:
          return -1;
      }
    case SOCK_FLOAT:
      switch (from) {
        case SOCK_FLOAT:
          return 5;
        case SOCK_INT:
          return 4;
        case SOCK_BOOLEAN:
          return 3;
        case SOCK_RGBA:
          return 2;
        case SOCK_VECTOR:
          return 1;
        default:
          return -1;
      }
    case SOCK_INT:
      switch (from) {
        case SOCK_INT:
          return 5;
        case SOCK_FLOAT:
          return 4;
        case SOCK_BOOLEAN:
          return 3;
        case SOCK_RGBA:
          return 2;
        case SOCK_VECTOR:
          return 1;
        default:
          return -1;
      }
    case SOCK_BOOLEAN:
      switch (from) {
        case SOCK_BOOLEAN:
          return 5;
        case SOCK_INT:
          return 4;
        case SOCK_FLOAT:
          return 3;
        case SOCK_RGBA:
          return 2;
        case SOCK_VECTOR:
          return 1;
        default:
          return -1;
      }
    case SOCK_SHADER:
      switch (from) {
        case SOCK_SHADER:
          return 1;
        default:
          return -1;
      }
    case SOCK_STRING:
      switch (from) {
        case SOCK_STRING:
          return 1;
        default:
          return -1;
      }
    case SOCK_OBJECT: {
      switch (from) {
        case SOCK_OBJECT:
          return 1;
        default:
          return -1;
      }
    }
    case SOCK_GEOMETRY: {
      switch (from) {
        case SOCK_GEOMETRY:
          return 1;
        default:
          return -1;
      }
    }
    default:
      return -1;
  }
}

/* select a suitable input socket for an output */
static bNodeSocket *select_internal_link_input(bNode *node, bNodeSocket *output)
{
  bNodeSocket *selected = NULL, *input;
  int i;
  int sel_priority = -1;
  bool sel_is_linked = false;

  for (input = node->inputs.first, i = 0; input; input = input->next, i++) {
    int priority = node_datatype_priority(input->type, output->type);
    bool is_linked = (input->link != NULL);
    bool preferred;

    if (nodeSocketIsHidden(input) || /* ignore hidden sockets */
        input->flag &
            SOCK_NO_INTERNAL_LINK || /* ignore if input is not allowed for internal connections */
        priority < 0 ||              /* ignore incompatible types */
        priority < sel_priority)     /* ignore if we already found a higher priority input */
    {
      continue;
    }

    /* determine if this input is preferred over the currently selected */
    preferred = (priority > sel_priority) ||   /* prefer higher datatype priority */
                (is_linked && !sel_is_linked); /* prefer linked over unlinked */

    if (preferred) {
      selected = input;
      sel_is_linked = is_linked;
      sel_priority = priority;
    }
  }

  return selected;
}

void node_update_internal_links_default(bNodeTree *ntree, bNode *node)
{
  bNodeLink *link;
  bNodeSocket *output, *input;

  /* sanity check */
  if (!ntree) {
    return;
  }

  /* use link pointer as a tag for handled sockets (for outputs is unused anyway) */
  for (output = node->outputs.first; output; output = output->next) {
    output->link = NULL;
  }

  for (link = ntree->links.first; link; link = link->next) {
    if (nodeLinkIsHidden(link)) {
      continue;
    }

    output = link->fromsock;
    if (link->fromnode != node || output->link) {
      continue;
    }
    if (nodeSocketIsHidden(output) || output->flag & SOCK_NO_INTERNAL_LINK) {
      continue;
    }
    output->link = link; /* not really used, just for tagging handled sockets */

    /* look for suitable input */
    input = select_internal_link_input(node, output);

    if (input) {
      bNodeLink *ilink = MEM_callocN(sizeof(bNodeLink), "internal node link");
      ilink->fromnode = node;
      ilink->fromsock = input;
      ilink->tonode = node;
      ilink->tosock = output;
      /* internal link is always valid */
      ilink->flag |= NODE_LINK_VALID;
      BLI_addtail(&node->internal_links, ilink);
    }
  }

  /* clean up */
  for (output = node->outputs.first; output; output = output->next) {
    output->link = NULL;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Default value RNA access
 * \{ */

float node_socket_get_float(bNodeTree *ntree, bNode *UNUSED(node), bNodeSocket *sock)
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
  return RNA_float_get(&ptr, "default_value");
}

void node_socket_set_float(bNodeTree *ntree, bNode *UNUSED(node), bNodeSocket *sock, float value)
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
  RNA_float_set(&ptr, "default_value", value);
}

void node_socket_get_color(bNodeTree *ntree, bNode *UNUSED(node), bNodeSocket *sock, float *value)
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
  RNA_float_get_array(&ptr, "default_value", value);
}

void node_socket_set_color(bNodeTree *ntree,
                           bNode *UNUSED(node),
                           bNodeSocket *sock,
                           const float *value)
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
  RNA_float_set_array(&ptr, "default_value", value);
}

void node_socket_get_vector(bNodeTree *ntree, bNode *UNUSED(node), bNodeSocket *sock, float *value)
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
  RNA_float_get_array(&ptr, "default_value", value);
}

void node_socket_set_vector(bNodeTree *ntree,
                            bNode *UNUSED(node),
                            bNodeSocket *sock,
                            const float *value)
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
  RNA_float_set_array(&ptr, "default_value", value);
}

/** \} */
