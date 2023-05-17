/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2007 Blender Foundation */

/** \file
 * \ingroup nodes
 */

#include <cctype>
#include <climits>
#include <cstring>

#include "DNA_node_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_colortools.h"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"
#include "RNA_prototypes.h"

#include "MEM_guardedalloc.h"

#include "NOD_common.h"

#include "node_util.hh"

/* -------------------------------------------------------------------- */
/** \name Storage Data
 * \{ */

void node_free_curves(bNode *node)
{
  BKE_curvemapping_free(static_cast<CurveMapping *>(node->storage));
}

void node_free_standard_storage(bNode *node)
{
  if (node->storage) {
    MEM_freeN(node->storage);
  }
}

void node_copy_curves(bNodeTree * /*dest_ntree*/, bNode *dest_node, const bNode *src_node)
{
  dest_node->storage = BKE_curvemapping_copy(static_cast<CurveMapping *>(src_node->storage));
}

void node_copy_standard_storage(bNodeTree * /*dest_ntree*/,
                                bNode *dest_node,
                                const bNode *src_node)
{
  dest_node->storage = MEM_dupallocN(src_node->storage);
}

void *node_initexec_curves(bNodeExecContext * /*context*/, bNode *node, bNodeInstanceKey /*key*/)
{
  BKE_curvemapping_init(static_cast<CurveMapping *>(node->storage));
  return nullptr; /* unused return */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Updates
 * \{ */

void node_sock_label(bNodeSocket *sock, const char *name)
{
  STRNCPY(sock->label, name);
}

void node_sock_label_clear(bNodeSocket *sock)
{
  if (sock->label[0] != '\0') {
    sock->label[0] = '\0';
  }
}

void node_math_update(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *sock1 = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 0));
  bNodeSocket *sock2 = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 1));
  bNodeSocket *sock3 = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 2));
  blender::bke::nodeSetSocketAvailability(ntree,
                                          sock2,
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
  blender::bke::nodeSetSocketAvailability(ntree,
                                          sock3,
                                          ELEM(node->custom1,
                                               NODE_MATH_COMPARE,
                                               NODE_MATH_MULTIPLY_ADD,
                                               NODE_MATH_WRAP,
                                               NODE_MATH_SMOOTH_MIN,
                                               NODE_MATH_SMOOTH_MAX));

  node_sock_label_clear(sock1);
  node_sock_label_clear(sock2);
  node_sock_label_clear(sock3);

  switch (node->custom1) {
    case NODE_MATH_WRAP:
      node_sock_label(sock2, "Max");
      node_sock_label(sock3, "Min");
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

void node_blend_label(const bNodeTree * /*ntree*/,
                      const bNode *node,
                      char *label,
                      int label_maxncpy)
{
  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_ramp_blend_items, node->custom1, &name);
  if (!enum_label) {
    name = "Unknown";
  }
  BLI_strncpy_utf8(label, IFACE_(name), label_maxncpy);
}

void node_image_label(const bNodeTree * /*ntree*/,
                      const bNode *node,
                      char *label,
                      int label_maxncpy)
{
  /* If there is no loaded image, return an empty string,
   * and let blender::bke::nodeLabel() fill in the proper type translation. */
  BLI_strncpy(label, (node->id) ? node->id->name + 2 : "", label_maxncpy);
}

void node_math_label(const bNodeTree * /*ntree*/,
                     const bNode *node,
                     char *label,
                     int label_maxncpy)
{
  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_node_math_items, node->custom1, &name);
  if (!enum_label) {
    name = "Unknown";
  }
  BLI_strncpy_utf8(label, CTX_IFACE_(BLT_I18NCONTEXT_ID_NODETREE, name), label_maxncpy);
}

void node_vector_math_label(const bNodeTree * /*ntree*/,
                            const bNode *node,
                            char *label,
                            int label_maxncpy)
{
  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_node_vec_math_items, node->custom1, &name);
  if (!enum_label) {
    name = "Unknown";
  }
  BLI_strncpy_utf8(label, IFACE_(name), label_maxncpy);
}

void node_filter_label(const bNodeTree * /*ntree*/,
                       const bNode *node,
                       char *label,
                       int label_maxncpy)
{
  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_node_filter_items, node->custom1, &name);
  if (!enum_label) {
    name = "Unknown";
  }
  BLI_strncpy_utf8(label, IFACE_(name), label_maxncpy);
}

void node_combsep_color_label(const ListBase *sockets, NodeCombSepColorMode mode)
{
  bNodeSocket *sock1 = (bNodeSocket *)sockets->first;
  bNodeSocket *sock2 = sock1->next;
  bNodeSocket *sock3 = sock2->next;

  node_sock_label_clear(sock1);
  node_sock_label_clear(sock2);
  node_sock_label_clear(sock3);

  switch (mode) {
    case NODE_COMBSEP_COLOR_RGB:
      node_sock_label(sock1, "Red");
      node_sock_label(sock2, "Green");
      node_sock_label(sock3, "Blue");
      break;
    case NODE_COMBSEP_COLOR_HSL:
      node_sock_label(sock1, "Hue");
      node_sock_label(sock2, "Saturation");
      node_sock_label(sock3, "Lightness");
      break;
    case NODE_COMBSEP_COLOR_HSV:
      node_sock_label(sock1, "Hue");
      node_sock_label(sock2, "Saturation");
      node_sock_label(sock3, "Value");
      break;
    default: {
      BLI_assert_unreachable();
      break;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Link Insertion
 * \{ */

bool node_insert_link_default(bNodeTree * /*ntree*/,
                              bNode * /*node*/,
                              bNodeLink * /*inserted_link*/)
{
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Default value RNA access
 * \{ */

float node_socket_get_float(bNodeTree *ntree, bNode * /*node*/, bNodeSocket *sock)
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
  return RNA_float_get(&ptr, "default_value");
}

void node_socket_set_float(bNodeTree *ntree, bNode * /*node*/, bNodeSocket *sock, float value)
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
  RNA_float_set(&ptr, "default_value", value);
}

void node_socket_get_color(bNodeTree *ntree, bNode * /*node*/, bNodeSocket *sock, float *value)
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
  RNA_float_get_array(&ptr, "default_value", value);
}

void node_socket_set_color(bNodeTree *ntree,
                           bNode * /*node*/,
                           bNodeSocket *sock,
                           const float *value)
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
  RNA_float_set_array(&ptr, "default_value", value);
}

void node_socket_get_vector(bNodeTree *ntree, bNode * /*node*/, bNodeSocket *sock, float *value)
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
  RNA_float_get_array(&ptr, "default_value", value);
}

void node_socket_set_vector(bNodeTree *ntree,
                            bNode * /*node*/,
                            bNodeSocket *sock,
                            const float *value)
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
  RNA_float_set_array(&ptr, "default_value", value);
}

/** \} */
