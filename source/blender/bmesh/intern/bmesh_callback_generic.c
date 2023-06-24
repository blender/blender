/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * BM element callback functions.
 */

#include "BLI_utildefines.h"

#include "bmesh.h"

#include "intern/bmesh_callback_generic.h"

bool BM_elem_cb_check_hflag_ex(BMElem *ele, void *user_data)
{
  const uint hflag_pair = POINTER_AS_INT(user_data);
  const char hflag_p = (hflag_pair & 0xff);
  const char hflag_n = (hflag_pair >> 8);

  return ((BM_elem_flag_test(ele, hflag_p) != 0) && (BM_elem_flag_test(ele, hflag_n) == 0));
}

bool BM_elem_cb_check_hflag_enabled(BMElem *ele, void *user_data)
{
  const char hflag = POINTER_AS_INT(user_data);

  return (BM_elem_flag_test(ele, hflag) != 0);
}

bool BM_elem_cb_check_hflag_disabled(BMElem *ele, void *user_data)
{
  const char hflag = POINTER_AS_INT(user_data);

  return (BM_elem_flag_test(ele, hflag) == 0);
}

bool BM_elem_cb_check_elem_not_equal(BMElem *ele, void *user_data)
{
  return (ele != user_data);
}
