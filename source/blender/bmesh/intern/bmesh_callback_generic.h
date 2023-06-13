/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

#ifdef __cplusplus
extern "C" {
#endif

bool BM_elem_cb_check_hflag_enabled(BMElem *, void *user_data);
bool BM_elem_cb_check_hflag_disabled(BMElem *, void *user_data);
bool BM_elem_cb_check_hflag_ex(BMElem *, void *user_data);
bool BM_elem_cb_check_elem_not_equal(BMElem *ele, void *user_data);

#define BM_elem_cb_check_hflag_ex_simple(type, hflag_p, hflag_n) \
  (bool (*)(type, void *)) BM_elem_cb_check_hflag_ex, \
      POINTER_FROM_UINT(((hflag_p) | (hflag_n << 8)))

#define BM_elem_cb_check_hflag_enabled_simple(type, hflag_p) \
  (bool (*)(type, void *)) BM_elem_cb_check_hflag_enabled, POINTER_FROM_UINT((hflag_p))

#define BM_elem_cb_check_hflag_disabled_simple(type, hflag_n) \
  (bool (*)(type, void *)) BM_elem_cb_check_hflag_disabled, POINTER_FROM_UINT(hflag_n)

#ifdef __cplusplus
}
#endif
