/* SPDX-FileCopyrightText: 2007 by Janne Karhu. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edphys
 */

#pragma once

#define KEY_K \
  PTCacheEditKey *key; \
  int k
#define POINT_P \
  PTCacheEditPoint *point; \
  int p
#define LOOP_POINTS for (p = 0, point = edit->points; p < edit->totpoint; p++, point++)
#define LOOP_VISIBLE_POINTS \
  for (p = 0, point = edit->points; p < edit->totpoint; p++, point++) \
    if (!(point->flag & PEP_HIDE))
#define LOOP_SELECTED_POINTS \
  for (p = 0, point = edit->points; p < edit->totpoint; p++, point++) \
    if (point_is_selected(point))
#define LOOP_UNSELECTED_POINTS \
  for (p = 0, point = edit->points; p < edit->totpoint; p++, point++) \
    if (!point_is_selected(point))
#define LOOP_EDITED_POINTS \
  for (p = 0, point = edit->points; p < edit->totpoint; p++, point++) \
    if (point->flag & PEP_EDIT_RECALC)
#define LOOP_TAGGED_POINTS \
  for (p = 0, point = edit->points; p < edit->totpoint; p++, point++) \
    if (point->flag & PEP_TAG)
#define LOOP_KEYS for (k = 0, key = point->keys; k < point->totkey; k++, key++)
#define LOOP_VISIBLE_KEYS \
  for (k = 0, key = point->keys; k < point->totkey; k++, key++) \
    if (!(key->flag & PEK_HIDE))
#define LOOP_SELECTED_KEYS \
  for (k = 0, key = point->keys; k < point->totkey; k++, key++) \
    if ((key->flag & PEK_SELECT) && !(key->flag & PEK_HIDE))
#define LOOP_TAGGED_KEYS \
  for (k = 0, key = point->keys; k < point->totkey; k++, key++) \
    if (key->flag & PEK_TAG)

#define KEY_WCO ((key->flag & PEK_USE_WCO) ? key->world_co : key->co)
