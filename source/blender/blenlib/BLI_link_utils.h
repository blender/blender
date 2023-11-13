/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 * \brief Single link-list utility macros. (header only api).
 *
 * Use this api when the structure defines its own `next` pointer
 * and a double linked list such as #ListBase isn't needed.
 */

#define BLI_LINKS_PREPEND(list, link) \
  { \
    CHECK_TYPE_PAIR(list, link); \
    (link)->next = list; \
    list = link; \
  } \
  (void)0

/* Use for append (single linked list, storing the last element). */
#define BLI_LINKS_APPEND(list, link) \
  { \
    (link)->next = NULL; \
    if ((list)->first) { \
      (list)->last->next = link; \
    } \
    else { \
      (list)->first = link; \
    } \
    (list)->last = link; \
  } \
  (void)0

/* Use for inserting after a certain element. */
#define BLI_LINKS_INSERT_AFTER(list, node, link) \
  { \
    if ((node)->next == NULL) { \
      (list)->last = link; \
    } \
    (link)->next = (node)->next; \
    (node)->next = link; \
  } \
  (void)0

#define BLI_LINKS_FREE(list) \
  { \
    while (list) { \
      void *next = (list)->next; \
      MEM_freeN(list); \
      list = next; \
    } \
  } \
  (void)0
