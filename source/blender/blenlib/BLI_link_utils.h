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
 */

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
